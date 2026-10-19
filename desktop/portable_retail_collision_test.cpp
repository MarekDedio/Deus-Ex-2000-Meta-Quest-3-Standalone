#include "Precomp.h"
#include "GC/GC.h"
#include "portable_engine_collision.h"
#include "portable_object_lookup.h"
#include "portable_retail_collision.h"
#include "portable_actor_point_check.h"
#include "quest_actor_point_check.h"
#include "quest_collision_hash_math.h"
#include "quest_model_line_hit.h"
#include "quest_model_point_line_hit.h"
#include "quest_model_point_overlap.h"
#include "quest_brush_coords.h"
#include "quest_original_sine_table.h"
#include "quest_multi_line_trace.h"
#include "quest_runtime_script_packages.h"
#include "quest_save_bundle.h"
#include "quest_script_state.h"
#include "quest_authored_struct_value.h"

#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <set>
#include <stdexcept>
#include <string_view>
#include <vector>

// First Training ONLY, with the production Quest source-package set. Original
// DLL/EXE code is never loaded/executed. Diagnostics always roll back: this is
// real-object hash/provider/transaction evidence, not playable crate acceptance.
namespace {
namespace fs=std::filesystem;
using Bytes=std::vector<std::uint8_t>;
using Array=std::array<float,3u>;
using Kind=QuestVr::Vm::Kind;
using Command=PortableRetailCollisionCommand;
using Operation=PortableRetailCollisionOperation;
constexpr std::uint32_t InitialSeed=0x13579bdfu;
std::size_t checks{},refusals{},collidable{},providerCalls{};

void Require(bool value,const std::string& message) {
    ++checks;if(!value)throw std::runtime_error(message);
}
std::string Fold(std::string value) {
    for(auto& c:value)if(c>='A'&&c<='Z')c=static_cast<char>(c-'A'+'a');
    return value;
}
bool Within(const fs::path& child,const fs::path& parent) {
    auto a=child.begin();
    for(auto b=parent.begin();b!=parent.end();++a,++b)
        if(a==child.end()||Fold(a->string())!=Fold(b->string()))return false;
    return true;
}
std::uint64_t Fingerprint(const fs::path& path) {
    std::ifstream file(path,std::ios::binary);Require(bool(file),"Cannot fingerprint owned source: "+path.string());
    std::array<char,16384u> buffer{};std::uint64_t hash=14695981039346656037ull;std::uintmax_t total{};
    while(file) {
        file.read(buffer.data(),static_cast<std::streamsize>(buffer.size()));const auto count=file.gcount();
        total+=static_cast<std::uintmax_t>(count);
        Require(total<=512u*1024u*1024u,"Owned source fingerprint exceeded its bound");
        for(std::streamsize i=0;i<count;++i) {hash^=static_cast<unsigned char>(buffer[static_cast<std::size_t>(i)]);hash*=1099511628211ull;}
    }
    Require(file.eof(),"Owned source fingerprint did not finish");return hash;
}
struct Source {
    fs::path path;std::uintmax_t size;fs::file_time_type modified;std::uint64_t fingerprint;
    explicit Source(const fs::path& value):path(fs::canonical(value)),size(fs::file_size(path)),
        modified(fs::last_write_time(path)),fingerprint(Fingerprint(path)) {}
    void Unchanged() const {
        Require(fs::file_size(path)==size&&fs::last_write_time(path)==modified&&Fingerprint(path)==fingerprint,
            "Owned source bytes, size or modification time changed: "+path.string());
    }
};
struct Temporary {
    fs::path parent,directory;std::vector<fs::path> files;
    explicit Temporary(const fs::path& source) {
        parent=fs::canonical(fs::temp_directory_path());
        Require(parent!=parent.root_path()&&!Within(parent,source),"Temporary checkpoint directory overlaps original installation/root");
        const auto stamp=std::chrono::steady_clock::now().time_since_epoch().count();
        for(unsigned i=0u;i<20u;++i) {
            const auto path=parent/("quest-retail-collision-"+std::to_string(stamp)+'-'+std::to_string(i));
            if(fs::create_directory(path)) {directory=fs::canonical(path);break;}
        }
        Require(!directory.empty()&&directory.parent_path()==parent,"Cannot create isolated checkpoint directory");
    }
    ~Temporary() {
        ShutdownPortableRuntime();std::error_code ignored;
        for(const auto& file:files)if(file.parent_path()==directory)fs::remove(file,ignored);
        if(!directory.empty()&&directory.parent_path()==parent)fs::remove(directory,ignored);
    }
    Bytes Save(const std::string& leaf) {
        Require(!leaf.empty()&&leaf.find_first_of("/\\:")==std::string::npos,"Invalid isolated checkpoint leaf");
        const auto path=directory/(leaf+".sav");
        if(std::find(files.begin(),files.end(),path)==files.end())files.push_back(path);
        Require(SavePortableRuntimeState(path.string()),"Cannot capture complete runtime checkpoint");
        Bytes bytes;Require(QuestVr::ReadBoundedSaveFile(path.string(),QuestVr::kMaximumSaveRuntimeBytes,bytes),
            "Cannot read complete runtime checkpoint");return bytes;
    }
    fs::path Write(const std::string& leaf,const Bytes& bytes) {
        Require(!leaf.empty()&&leaf.find_first_of("/\\:")==std::string::npos,"Invalid isolated fixture leaf");
        const auto path=directory/(leaf+".sav");
        if(std::find(files.begin(),files.end(),path)==files.end())files.push_back(path);
        Require(QuestVr::WriteDurableSaveFile(path.string(),bytes),"Cannot write isolated collision checkpoint fixture");return path;
    }
};
std::uint32_t Word(const Bytes& bytes,std::size_t offset) {
    Require(offset<=bytes.size()&&bytes.size()-offset>=4u,"Collision fixture word is truncated");
    std::uint32_t result{};for(unsigned i=0u;i<4u;++i)result|=std::uint32_t(bytes[offset+i])<<(8u*i);return result;
}
struct Envelope {
    Bytes prefix;QuestVr::ScriptSavedState state;
    explicit Envelope(const Bytes& bytes) {
        const auto version=Word(bytes,4u);
        Require(Word(bytes,0u)==0x53515844u&&version>=10u&&version<=13u,
            "Collision fixture requires a seed-aware plain script checkpoint");
        std::size_t cursor=8u;
        const auto skip=[&](std::size_t count) {
            Require(cursor<=bytes.size()&&count<=bytes.size()-cursor,"Collision fixture prefix is truncated");cursor+=count;
        };
        const auto word=[&] {const auto value=Word(bytes,cursor);cursor+=4u;return value;};
        const auto string=[&] {const auto size=word();Require(size<=1'048'576u,"Collision fixture string exceeds bound");skip(size);};
        const auto list=[&] {const auto count=word();Require(count<=100'000u,"Collision fixture list exceeds bound");for(std::uint32_t i=0u;i<count;++i)string();};
        list();list();list();skip(4u);const auto damaged=word();Require(damaged<=100'000u,"Collision fixture damaged list exceeds bound");
        for(std::uint32_t i=0u;i<damaged;++i) {string();skip(4u);}
        skip(8u);list();list();list();list();prefix.assign(bytes.begin(),bytes.begin()+cursor);
        const auto count=word();Require(count==bytes.size()-cursor,"Collision fixture script envelope length differs");
        state=QuestVr::DecodeScriptSavedState(Bytes(bytes.begin()+cursor,bytes.end()));
        Require(state.mapName=="00_Training"&&state.randomSeed.has_value(),"Collision fixture lost first Training/shared seed authority");
    }
    Bytes Wrap(const QuestVr::ScriptSavedState& replacement) const {
        const auto blob=QuestVr::EncodeScriptSavedState(replacement);
        Require(blob.size()<=QuestVr::kMaximumSaveRuntimeBytes,"Collision injected script exceeds bound");
        Bytes result=prefix;for(unsigned i=0u;i<4u;++i)result.push_back(static_cast<std::uint8_t>(blob.size()>>(8u*i)));
        result.insert(result.end(),blob.begin(),blob.end());return result;
    }
};
void SameGc(const GCStats& before,const std::string& message) {
    const auto after=GC::GetStats();Require(before.numObjects==after.numObjects&&before.memoryUsage==after.memoryUsage,message);
}
void SameFloat(float a,float b,const std::string& message) {
    Require(std::bit_cast<std::uint32_t>(a)==std::bit_cast<std::uint32_t>(b),message);
}
void SameVector(const Array& a,const Array& b,const std::string& message) {
    for(std::size_t i=0u;i<a.size();++i)SameFloat(a[i],b[i],message+" component "+std::to_string(i));
}
PortableModelVec3 Vector(const Array& value) {return {value[0u],value[1u],value[2u]};}
QuestVr::Vm::Value Declared(const std::string& path,const char* field,Kind kind) {
    const auto value=ReadPortableActorDeclaredScriptProperty(path,field);
    Require(value.kind==kind,"Original declared field has wrong kind: "+path+' '+field);return value;
}
struct Actor {
    std::string path;std::uint32_t index{},tag{};Array location{},colLocation{};
    bool collide{},deleted{};
};
std::vector<Actor> Census(const PortablePackageTables& map) {
    const auto snapshots=GetPortableRuntimeMapActors(true);std::set<std::string> world;
    for(const auto& actor:snapshots)Require(world.insert(actor.objectPath).second,"Duplicate actual map Actor");
    std::vector<Actor> result;
    for(const auto reference:ReadPortableLevel68ActorOrder(map)) {
        if(!reference)continue;
        Require(reference>0&&static_cast<std::size_t>(reference)<=map.exports.size(),"Source native Level slot has an invalid export");
        const auto path=std::string("00_Training.")+GetPortableObjectPath(map,reference);
        Require(world.erase(path)==1u,"Actual native Level slot does not match a current Training Actor: "+path);
        const auto owned=ReadPortableRuntimeObjectOwnership(path);
        Actor actor;actor.path=path;actor.index=owned.objectIndex;
        actor.location=Declared(path,"Engine.Actor.Location",Kind::Vector).vector;
        actor.colLocation=Declared(path,"Engine.Actor.ColLocation",Kind::Vector).vector;
        actor.tag=std::bit_cast<std::uint32_t>(Declared(path,"Engine.Actor.CollisionTag",Kind::Int).integer);
        actor.collide=Declared(path,"Engine.Actor.bCollideActors",Kind::Bool).boolean;
        actor.deleted=Declared(path,"Engine.Actor.bDeleteMe",Kind::Bool).boolean;
        Require(actor.index!=0u,"Source actor lacks its actual UObject index");
        if(actor.collide&&!actor.deleted)++collidable;
        result.push_back(std::move(actor));
    }
    Require(world.empty()&&!result.empty()&&collidable,"Source/actual first Training actor census is incomplete");return result;
}
void SameActors(const PortableRetailCollisionSnapshot& state,const std::vector<Actor>& source,
    const std::vector<std::uint32_t>& tags) {
    Require(state.actors.size()==source.size()&&tags.size()==source.size(),"Retail hash actor census differs from native Level slots");
    for(std::size_t i=0u;i<source.size();++i) {
        const auto& row=state.actors[i];const auto& original=source[i];
        Require(row.path==original.path&&row.objectIndex==original.index&&row.collideActors==original.collide&&
            row.pendingDelete==original.deleted&&row.tag==tags[i]&&row.declaredMirrorsMatch,
            "Retail hash actor identity/flags/tag/declared mirror differs: "+original.path);
        SameVector(row.location,original.location,"Retail hash changed actor Location");
        SameVector(row.colLocation,original.collide&&!original.deleted?original.location:original.colLocation,
            "Retail Add skipped/misplaced live Location→ColLocation copy: "+original.path);
    }
}
struct Oracle {
    struct Link {std::size_t actor;std::uint32_t cell;};
    std::array<std::array<std::uint32_t,16384u>,3u> permutations{};
    std::vector<std::vector<Link>> buckets=std::vector<std::vector<Link>>(16384u);
    std::vector<std::uint32_t> tags;
    std::vector<QuestVr::OriginalCollisionHashRange> ranges;
    std::uint32_t seed{InitialSeed},tag{};
    std::size_t links{};
    explicit Oracle(const std::vector<Actor>& actors) {
        for(auto& axis:permutations)for(std::size_t i=0u;i<axis.size();++i)axis[i]=static_cast<std::uint32_t>(i);
        // Independent uint64 recurrence, not production ScriptRandom/initializer.
        for(std::size_t i=0u;i<16384u;++i)for(auto& axis:permutations) {
            seed=static_cast<std::uint32_t>((std::uint64_t{seed}*214013u+2531011u)&0xffffffffull);
            std::swap(axis[i],axis[((seed>>16u)&32767u)%16384u]);
        }
        tags.reserve(actors.size());ranges.resize(actors.size());
        for(std::size_t i=0u;i<actors.size();++i) {
            const auto& actor=actors[i];tags.push_back(actor.tag);if(!actor.collide||actor.deleted)continue;
            const auto bounds=ReadPortableRuntimeActorPrimitiveBounds(actor.path);
            Require(bounds.primitive.actorPath==actor.path&&!bounds.primitive.primitivePath.empty(),"Actual registration lacks selected primitive ownership");
            ranges[i]=QuestVr::OriginalCollisionHashBoxRange({Vector(bounds.minimum),Vector(bounds.maximum)});
            Add(i);
        }
    }
    void Add(std::size_t actor) {
        Cells(ranges.at(actor),[&](std::uint32_t bucket,std::uint32_t cell) {
            Require(links<1'048'576u,"Training oracle registration exceeded bounded link count");
            buckets[bucket].push_back({actor,cell});++links;
        });
    }
    void Remove(std::size_t actor) {
        // Original Remove visits CURRENT bounds and removes every actor match
        // in each selected bucket, without an encoded-cell predicate.
        Cells(ranges.at(actor),[&](std::uint32_t bucket,std::uint32_t) {
            auto& entries=buckets[bucket];const auto before=entries.size();
            entries.erase(std::remove_if(entries.begin(),entries.end(),[&](const auto& link){return link.actor==actor;}),entries.end());
            links-=before-entries.size();
        });
    }
    template<class F>void Cells(const QuestVr::OriginalCollisionHashRange& range,F&& run) {
        const auto count=QuestVr::CheckedOriginalCollisionHashCellCount(range,262144u);if(!count)return;
        for(auto x=range.minimum[0u];x<=range.maximum[0u];++x)
            for(auto y=range.minimum[1u];y<=range.maximum[1u];++y)
                for(auto z=range.minimum[2u];z<=range.maximum[2u];++z) {
                    const auto bucket=permutations[0u][static_cast<std::size_t>(x)]^
                        permutations[1u][static_cast<std::size_t>(y)]^permutations[2u][static_cast<std::size_t>(z)];
                    const auto cell=((static_cast<std::uint32_t>(z)*256u)+static_cast<std::uint32_t>(y))*256u+
                        static_cast<std::uint32_t>(x);
                    run(bucket,cell);
                }
    }
    struct ResizedCylinder {std::size_t actor;float radius,height;};
    static PortableActorPrimitiveLineSnapshot CenterLine(const Command& command,const Actor& actor,float radius,float height) {
        // Independent restricted numerical oracle. On this centerline the
        // actual cylinder quadratic reduces to an exact integer/dyadic root;
        // only the original final stored Enter minus Double.001 and Float
        // offset/add stores remain. This is NOT a general cylinder substitute.
        Require(command.extent[0u]==0.0f&&command.extent[1u]==0.0f&&command.extent[2u]>=0.0f&&
            command.start[1u]==actor.location[1u]&&command.end[1u]==actor.location[1u]&&
            command.start[2u]==command.end[2u]&&
            double(actor.location[0u])-double(command.start[0u])==256.0&&
            double(command.end[0u])-double(actor.location[0u])==256.0&&
            radius>0.0f&&radius<256.0f&&radius==std::floor(radius)&&height>0.0f,
            "Resized-cylinder oracle is outside its independently reduced centerline domain");
        const float lower=static_cast<float>(double(actor.location[2u])-double(height)-double(command.extent[2u]));
        const float upper=static_cast<float>(double(actor.location[2u])+double(height)+double(command.extent[2u]));
        if(command.start[2u]<lower||command.start[2u]>upper) {
            PortableActorPrimitiveLineSnapshot result;result.clear=true;return result;
        }
        const float enter=static_cast<float>((256.0-double(radius))/512.0);
        const double time=double(enter)-std::bit_cast<double>(std::uint64_t{0x3f50624dd2f1a9fcull});
        const float offset=static_cast<float>(512.0*time);
        PortableActorPrimitiveLineSnapshot result;result.clear=false;result.hitOwner=true;result.hitPrimitiveIsNone=true;
        result.time=static_cast<float>(time);result.item=-1;result.normal={-1.0f,0.0f,0.0f};
        result.location={static_cast<float>(double(command.start[0u])+double(offset)),actor.location[1u],command.start[2u]};
        return result;
    }
    std::pair<std::vector<PortableRetailCollisionHit>,std::size_t> Query(const Command& command,const std::vector<Actor>& actors,
        const std::optional<ResizedCylinder>& resized={}) {
        ++tag;std::vector<PortableRetailCollisionHit> hits;std::size_t head{};
        const auto range=QuestVr::OriginalCollisionHashSweptRange(Vector(command.end),Vector(command.start),Vector(command.extent));
        Cells(range,[&](std::uint32_t bucket,std::uint32_t cell) {
            // Vector append models prepend links by REVERSE traversal, not the
            // production chunk/free linked-list representation.
            for(auto link=buckets[bucket].rbegin();link!=buckets[bucket].rend();++link) {
                if(tags[link->actor]==tag||link->cell!=cell)continue;
                tags[link->actor]=tag;const auto& actor=actors[link->actor];++providerCalls;
                const auto reply=resized&&resized->actor==link->actor?
                    CenterLine(command,actor,resized->radius,resized->height):
                    ProbePortableRuntimeRetailCollisionPrimitiveLine(actor.path,command.end,command.start,command.extent,command.flags);
                if(reply.clear)continue;
                Require(reply.hitOwner,"Blocked actual primitive diagnostic did not identify its supplied owner");
                hits.push_back({actor.path,reply.hitPrimitiveIsNone?std::string{}:reply.primitive.primitivePath,
                    head,reply.location,reply.normal,reply.time,reply.item});head=hits.size();
            }
        });return {std::move(hits),head};
    }
};
void SameHits(const std::vector<PortableRetailCollisionHit>& actual,std::size_t actualHead,
    const std::pair<std::vector<PortableRetailCollisionHit>,std::size_t>& expected) {
    Require(actualHead==expected.second&&actual.size()==expected.first.size(),"Actual hash result count/head differs from source registration oracle");
    for(std::size_t i=0u;i<actual.size();++i) {
        const auto& a=actual[i];const auto& b=expected.first[i];
        Require(a.actorPath==b.actorPath&&a.primitivePath==b.primitivePath&&a.next==b.next&&a.item==b.item,
            "Actual hash encounter/result-link/owner/primitive packet differs");
        SameVector(a.location,b.location,"Actual hash hit Location differs");
        SameVector(a.normal,b.normal,"Actual hash hit Normal differs");SameFloat(a.time,b.time,"Actual hash hit Time differs");
    }
}
QuestVr::OriginalCollisionHashRange CylinderRange(const Actor& actor,float radius,float height) {
    Require(radius>=0.0f&&height>=0.0f&&radius==std::floor(radius)&&height==std::floor(height)&&
        radius<256.0f&&height<256.0f,"Setter registration oracle requires bounded exact integer dimensions");
    // Chosen integer dimensions have exact +1 padding; independent Double
    // endpoint arithmetic rounds once to Float, including the retained-Y path.
    QuestVr::OriginalCollisionHashBox box;
    box.minimum={static_cast<float>(double(actor.location[0u])-double(radius)-1.0),
        static_cast<float>(double(actor.location[1u])-double(radius)-1.0),
        static_cast<float>(double(actor.location[2u])-double(height)-1.0)};
    box.maximum={static_cast<float>(double(actor.location[0u])+double(radius)+1.0),
        static_cast<float>(double(actor.location[1u])+double(radius)+1.0),
        static_cast<float>(double(actor.location[2u])+double(height)+1.0)};
    return QuestVr::OriginalCollisionHashBoxRange(box);
}
Command SetterOrderQuery(const std::vector<Actor>& actors,std::size_t target) {
    std::array<std::size_t,2u> pair{};std::size_t count{};
    for(std::size_t i=0u;i<actors.size();++i)
        if(actors[i].collide&&!actors[i].deleted&&(actors[i].path=="00_Training.CrateBreakableMedGeneral1"||
            actors[i].path=="00_Training.CrateBreakableMedGeneral2")) {
            Require(count<pair.size(),"Original crate encounter-order fixture has duplicate native slots");pair[count++]=i;
        }
    Require(count==2u&&pair[0u]==target,"Setter encounter-order test requires both source crates and the earlier native slot");
    std::array<PortableActorPrimitiveBoundsSnapshot,2u> bounds;
    std::array<float,2u> radius{},height{};Array center{};
    for(std::size_t p=0u;p<pair.size();++p) {
        const auto& actor=actors[pair[p]];bounds[p]=ReadPortableRuntimeActorPrimitiveBounds(actor.path);
        radius[p]=Declared(actor.path,"Engine.Actor.CollisionRadius",Kind::Float).floating;
        height[p]=Declared(actor.path,"Engine.Actor.CollisionHeight",Kind::Float).floating;
        for(std::size_t axis=0u;axis<center.size();++axis)center[axis]+=actor.location[axis]*0.5f;
        std::cout<<std::setprecision(9)<<"SETTER ORDER SOURCE "<<actor.path<<" Level-census-index="<<pair[p]
            <<" location=("<<actor.location[0u]<<','<<actor.location[1u]<<','<<actor.location[2u]<<") bounds=("
            <<bounds[p].minimum[0u]<<','<<bounds[p].minimum[1u]<<','<<bounds[p].minimum[2u]<<")..("
            <<bounds[p].maximum[0u]<<','<<bounds[p].maximum[1u]<<','<<bounds[p].maximum[2u]<<")\n";
    }
    const auto a=QuestVr::OriginalCollisionHashBoxRange({Vector(bounds[0u].minimum),Vector(bounds[0u].maximum)});
    const auto b=QuestVr::OriginalCollisionHashBoxRange({Vector(bounds[1u].minimum),Vector(bounds[1u].maximum)});
    std::array<std::int32_t,3u> common{};
    for(std::size_t axis=0u;axis<common.size();++axis) {
        common[axis]=std::max(a.minimum[axis],b.minimum[axis]);
        Require(common[axis]<=std::min(a.maximum[axis],b.maximum[axis]),"Original crate pair has no shared registration cell");
    }
    // A long ray discovers/tag-deduplicates actors in earlier neighboring cells
    // before reaching their shared bucket. Select a genuinely blocked query
    // entirely inside ONE common source cell, then prove re-prepend changes its
    // actual-provider encounter list before using it as an order assertion.
    std::size_t attempts{};
    const auto original=std::make_unique<Oracle>(actors);
    for(const std::size_t axis:{0u,1u})for(const float padding:{4.0f,16.0f,32.0f}) {
        ++attempts;Command command{Operation::LineCheck};command.start=command.end=center;
        const float span=std::max(radius[0u],radius[1u])+padding;
        command.start[axis]-=span;command.end[axis]+=span;
        command.extent[2u]=std::max(0.0f,std::abs(actors[pair[0u]].location[2u]-actors[pair[1u]].location[2u])*0.5f-
            std::min(height[0u],height[1u])+1.0f);
        const auto range=QuestVr::OriginalCollisionHashSweptRange(Vector(command.end),Vector(command.start),Vector(command.extent));
        if(range.minimum!=common||range.maximum!=common)continue;
        auto query=std::make_unique<Oracle>(*original);const auto before=query->Query(command,actors);
        const auto contains=[&](const auto& hits,std::size_t index) {
            return std::any_of(hits.first.begin(),hits.first.end(),[&](const auto& hit){return hit.actorPath==actors[index].path;});
        };
        if(!contains(before,pair[0u])||!contains(before,pair[1u]))continue;
        query->Remove(target);query->Add(target);const auto after=query->Query(command,actors);
        std::vector<std::string> first,second;
        for(const auto& hit:before.first)first.push_back(hit.actorPath);
        for(const auto& hit:after.first)second.push_back(hit.actorPath);
        if(first==second)continue;
        std::cout<<"SETTER ORDER QUERY attempts="<<attempts<<" commonCell=("<<common[0u]<<','<<common[1u]<<','<<common[2u]
            <<") start=("<<command.start[0u]<<','<<command.start[1u]<<','<<command.start[2u]<<") end=("
            <<command.end[0u]<<','<<command.end[1u]<<','<<command.end[2u]<<") extent=("<<command.extent[0u]<<','
            <<command.extent[1u]<<','<<command.extent[2u]<<")\nSETTER ORDER ORIGINAL ENCOUNTER";
        for(const auto& path:first)std::cout<<" -> "<<path;
        std::cout<<"\nSETTER ORDER REPREPENDED ENCOUNTER";
        for(const auto& path:second)std::cout<<" -> "<<path;
        std::cout<<'\n';return command;
    }
    Require(attempts==6u,"Setter encounter query search escaped its six-candidate bound");
    throw std::runtime_error("No bounded actual shared-cell crate query proves setter remove/prepend order");
}
void SetterCoverage(const std::vector<Actor>& actors,const std::function<void()>& rollback) {
    const auto selected=std::find_if(actors.begin(),actors.end(),[](const auto& actor) {
        return actor.collide&&!actor.deleted&&(actor.path=="00_Training.CrateBreakableMedGeneral1"||
            actor.path=="00_Training.CrateBreakableMedGeneral2");
    });
    Require(selected!=actors.end(),"Original first Training crate pair is unavailable for setter tests");
    const auto target=static_cast<std::size_t>(selected-actors.begin());const auto primitive=ReadPortableRuntimeActorPrimitive(selected->path);
    Require((primitive.nativeClassPath=="Engine.Mesh"||primitive.nativeClassPath=="Engine.LodMesh"||
        primitive.nativeClassPath=="Engine.Primitive")&&
        Declared(selected->path,"Engine.Actor.Brush",Kind::Object).text.empty(),
        "Setter fixture would incorrectly use cylinder math for a selected Model");
    const float originalRadius=Declared(selected->path,"Engine.Actor.CollisionRadius",Kind::Float).floating;
    const float originalHeight=Declared(selected->path,"Engine.Actor.CollisionHeight",Kind::Float).floating;
    const bool originalBlockActors=Declared(selected->path,"Engine.Actor.bBlockActors",Kind::Bool).boolean;
    const bool originalBlockPlayers=Declared(selected->path,"Engine.Actor.bBlockPlayers",Kind::Bool).boolean;
    const bool originalWorld=Declared(selected->path,"Engine.Actor.bCollideWorld",Kind::Bool).boolean;
    Command line{Operation::LineCheck};line.start=line.end=selected->location;
    line.start[0u]-=256.0f;line.end[0u]+=256.0f;line.extent[2u]=128.0f;
    Command elevated=line;elevated.extent[2u]=0.0f;elevated.start[2u]+=56.0f;elevated.end[2u]=elevated.start[2u];
    const auto order=SetterOrderQuery(actors,target);
    const auto setter=[&](Operation operation,std::vector<QuestVr::Vm::Value> values) {
        Command command{operation};command.actorPath=selected->path;command.arguments=std::move(values);return command;
    };
    using Value=QuestVr::Vm::Value;
    struct Frame {
        bool collide,blocks,players;float radius,height;std::size_t links;std::uint32_t tag;
        std::vector<std::uint32_t> tags;
    };
    const auto sequence=[&](const std::vector<Command>& commands,bool requireOrderChange) {
        Require(commands.size()<=16u,"Setter sequence exceeds production command bound");
        Oracle expected(actors);std::vector<Frame> frames;std::vector<PortableRetailCollisionMutation> mutations;
        std::vector<std::pair<std::vector<PortableRetailCollisionHit>,std::size_t>> hits;
        bool collide=true,blocks=originalBlockActors,players=originalBlockPlayers;float radius=originalRadius,height=originalHeight;
        std::vector<Actor> model=actors;model[target].colLocation=model[target].location;
        Require(expected.ranges[target].minimum==CylinderRange(*selected,radius,height).minimum&&
            expected.ranges[target].maximum==CylinderRange(*selected,radius,height).maximum,
            "Independent source-dimension registration differs from actual selected primitive bounds");
        for(const auto& command:commands) {
            if(command.operation==Operation::SetCollision||command.operation==Operation::SetCollisionSize) {
                if(collide)expected.Remove(target);
                if(command.operation==Operation::SetCollision) {
                    const auto flag=[&](std::size_t slot,bool before) {
                        return slot<command.arguments.size()&&command.arguments[slot].kind!=Kind::Nothing?
                            command.arguments[slot].boolean:before;
                    };
                    collide=flag(0u,collide);blocks=flag(1u,blocks);players=flag(2u,players);
                }else {
                    radius=QuestVr::Vm::ToFloat(command.arguments[0u]);height=QuestVr::Vm::ToFloat(command.arguments[1u]);
                    expected.ranges[target]=CylinderRange(*selected,radius,height);
                }
                if(collide)expected.Add(target);
                mutations.push_back({selected->path,command.operation==Operation::SetCollision?Value{}:Value::Bool(true),
                    radius,height,collide,blocks,players,originalWorld});
            }else if(command.operation==Operation::LineCheck) {
                const bool reduced=command.start[0u]==line.start[0u]&&command.end[0u]==line.end[0u]&&
                    command.start[1u]==line.start[1u]&&command.end[1u]==line.end[1u];
                Require(reduced||(radius==originalRadius&&height==originalHeight),
                    "Actual-provider order query cannot substitute original dimensions for a resized target");
                hits.push_back(expected.Query(command,actors,reduced?
                    std::optional<Oracle::ResizedCylinder>{{target,radius,height}}:std::nullopt));
            }
            frames.push_back({collide,blocks,players,radius,height,expected.links,expected.tag,expected.tags});
        }
        if(requireOrderChange) {
            Require(hits.size()>=2u&&hits[0u].first.size()>=2u,"Setter encounter-order fixture lacks genuine shared-cell hits");
            std::vector<std::string> before,after;
            for(const auto& hit:hits[0u].first)before.push_back(hit.actorPath);
            for(const auto& hit:hits[1u].first)after.push_back(hit.actorPath);
            Require(before!=after,"No-op setter fixture cannot observe original remove/prepend encounter-order change");
        }
        const auto result=ProbePortableRuntimeRetailCollision(commands);
        Require(result.available&&result.execution.passed()&&!result.execution.committed&&result.states.size()==frames.size()&&
            result.mutations.size()==mutations.size()&&result.queries.size()==hits.size()&&result.queryHeads.size()==hits.size(),
            "Original setter sequence failed: "+result.execution.error);
        for(std::size_t i=0u;i<frames.size();++i) {
            const auto& frame=frames[i];const auto& state=result.states[i];model[target].collide=frame.collide;
            Require(state.initialized&&state.enabled&&state.hashIdentity==result.states.front().hashIdentity&&
                state.seed==expected.seed&&state.tag==frame.tag&&state.activeLinks==frame.links,
                "Setter changed native hash lease/RNG/query tag or lost original registration count");
            SameActors(state,model,frame.tags);
        }
        for(std::size_t i=0u;i<mutations.size();++i) {
            const auto& actual=result.mutations[i];const auto& wanted=mutations[i];
            Require(actual.actorPath==wanted.actorPath&&QuestVr::Vm::Equal(actual.returned,wanted.returned)&&
                actual.collideActors==wanted.collideActors&&actual.blockActors==wanted.blockActors&&
                actual.blockPlayers==wanted.blockPlayers&&actual.collideWorld==originalWorld,
                "Setter returned wrong type/value or changed unrelated original collision flags");
            SameFloat(actual.radius,wanted.radius,"Setter did not write actual qualified CollisionRadius");
            SameFloat(actual.height,wanted.height,"Setter did not write actual qualified CollisionHeight");
        }
        for(std::size_t i=0u;i<hits.size();++i)SameHits(result.queries[i],result.queryHeads[i],hits[i]);
        rollback();
    };
    sequence({Command{Operation::Enable},order,setter(Operation::SetCollision,{Value::Bool(true)}),order,
        setter(Operation::SetCollision,{Value::Bool(false)}),line,setter(Operation::SetCollision,{Value::Bool(true)}),line,
        setter(Operation::SetCollision,{Value{},Value::Bool(!originalBlockActors),Value::Bool(!originalBlockPlayers)}),line,
        setter(Operation::SetCollision,{}),line,
        setter(Operation::SetCollision,{Value::Bool(true),Value::Bool(originalBlockActors),Value::Bool(originalBlockPlayers)}),line},true);
    sequence({Command{Operation::Enable},order,
        setter(Operation::SetCollisionSize,{Value::Float(originalRadius),Value::Float(originalHeight)}),order,
        setter(Operation::SetCollisionSize,{Value::Float(64.0f),Value::Float(32.0f)}),elevated,
        setter(Operation::SetCollisionSize,{Value::Float(64.0f),Value::Float(32.0f)}),line,
        setter(Operation::SetCollision,{Value::Bool(false)}),
        setter(Operation::SetCollisionSize,{Value::Float(16.0f),Value::Float(80.0f)}),line,
        setter(Operation::SetCollision,{Value::Bool(true)}),elevated,
        setter(Operation::SetCollisionSize,{Value::Float(originalRadius),Value::Float(originalHeight)}),line},true);
    std::vector<Command> invalid;
    for(const auto operation:{Operation::SetCollision,Operation::SetCollisionSize}) {
        invalid.push_back(setter(operation,operation==Operation::SetCollision?std::vector<Value>{Value::Bool(true)}:
            std::vector<Value>{Value::Float(16.0f),Value::Float(16.0f)}));
        auto empty=invalid.back();empty.actorPath.clear();invalid.push_back(empty);
        auto missing=invalid.front();missing.operation=operation;missing.actorPath="00_Training.NoSuchActor";invalid.push_back(missing);
        auto foreign=invalid.back();foreign.actorPath="Engine.Actor";invalid.push_back(foreign);
    }
    invalid.push_back(setter(Operation::SetCollision,{Value::Integer(1)}));
    invalid.push_back(setter(Operation::SetCollision,{Value::Bool(true),Value::Bool(true),Value::Bool(true),Value::Bool(true)}));
    invalid.push_back(setter(Operation::SetCollisionSize,{}));
    invalid.push_back(setter(Operation::SetCollisionSize,{Value::Float(16.0f)}));
    invalid.push_back(setter(Operation::SetCollisionSize,{Value::Float(16.0f),Value::Float(16.0f),Value::Float(16.0f)}));
    invalid.push_back(setter(Operation::SetCollisionSize,{Value::Text(Kind::String,"16"),Value::Float(16.0f)}));
    invalid.push_back(setter(Operation::SetCollisionSize,{Value::Float(16.0f),Value::Float(std::numeric_limits<float>::quiet_NaN())}));
    invalid.push_back(setter(Operation::SetCollisionSize,{Value::Float(std::numeric_limits<float>::infinity()),Value::Float(16.0f)}));
    invalid.push_back(setter(Operation::SetCollisionSize,{Value::Float(-1.0f),Value::Float(16.0f)}));
    const auto reject=[&](const std::vector<Command>& commands,const std::string& name) {
        const auto result=ProbePortableRuntimeRetailCollision(commands);
        Require(!result.available&&!result.execution.passed()&&!result.execution.committed&&!result.execution.error.empty(),
            "Original collision setter accepted unsupported input: "+name);++refusals;rollback();
    };
    reject({invalid[0u]},"SetCollision without native domain");reject({invalid[4u]},"SetCollisionSize without native domain");
    for(std::size_t i=1u;i<invalid.size();++i)if(i!=4u)
        reject({Command{Operation::Enable},invalid[i]},"type/count/actor case "+std::to_string(i));
    auto badSize=setter(Operation::SetCollisionSize,{Value::Float(16.0f),Value::Float(std::numeric_limits<float>::quiet_NaN())});
    reject({Command{Operation::Enable},setter(Operation::SetCollision,{Value::Bool(false)}),
        setter(Operation::SetCollisionSize,{Value::Float(64.0f),Value::Float(32.0f)}),
        setter(Operation::SetCollision,{Value::Bool(true)}),line,badSize},"late bad Float after successful flags/size/query");
    std::cout<<"FIRST TRAINING setters: actual "<<selected->path<<" selected "<<primitive.nativeClassPath
        <<"; original262/283 typed returns, unchanged-setter prepend ordering, flag toggles, resized bounds/hits, late rollback/refusals\n";
}
#include "portable_retail_multi_line_cases.inl"
#include "portable_actor_point_cases.inl"
#include "portable_model_point_cases.inl"
#include "portable_retail_encroachment_cases.inl"
#include "portable_retail_actor_blocking_cases.inl"
#include "portable_retail_actor_touch_cases.inl"
#include "portable_model_point_region_cases.inl"
#include "portable_actor_zone_cases.inl"
#include "portable_retail_actor_move_cases.inl"
#include "portable_actor_move_clock_cases.inl"
void Run(const fs::path& root,bool movementOnly) {
    Temporary temporary(root);std::vector<Source> sources;std::vector<PortablePackageTables> scripts;
    sources.reserve(QuestVr::RuntimeScriptPackages.size()+2u);scripts.reserve(QuestVr::RuntimeScriptPackages.size());
    for(const auto* name:QuestVr::RuntimeScriptPackages) {
        sources.emplace_back(root/"System"/(std::string(name)+".u"));scripts.push_back(LoadPortablePackageTables(sources.back().path.string()));
    }
    sources.emplace_back(root/"System"/"DeusEx.ini");const auto ini=sources.back().path.string();
    sources.emplace_back(root/"Maps"/"00_Training.dx");const auto map=LoadPortablePackageTables(sources.back().path.string());
    Require(scripts.size()==38u,"Test did not use the exact shared production source list");
    Require(InitializePortableRuntime(scripts,InitialSeed).passed,"Cannot initialize original Quest script-source set");
    Require(PreparePortableRuntimeNativeEngineClasses().prepared&&PreparePortableRuntimeNativeMetadataClasses().prepared&&
        PreparePortableRuntimeNativeConfigClasses().prepared&&PreparePortableRuntimeNativePackages().prepared&&
        PreparePortableRuntimeObjectLookup().prepared&&PreparePortableRuntimeEngineCollision(ini).prepared,
        "Cannot prepare actual Engine/metadata/config/Package/lookup/Cylinder prerequisites");
    auto lifetime=std::make_shared<QuestVr::NativeClientLifetime>();BindPortableRuntimeClient(lifetime,2801u);
    RegisterPortableRuntimeViewport(2801u,{2802u,1024u,1152u,3u,2u});
    const auto playerObjects=PreparePortableRuntimePlayerObjects(2802u);
    Require(!playerObjects.bound,"Collision test forced player possession");
    Require(LoadPortableRuntimeMap(map).passed,"Cannot load actual first Training map");
    const auto native=ReadPortableRuntimeNativeModel();const auto engine=ReadPortableRuntimeEngineCollision();
    Require(native.available&&native.mapGeneration&&engine.boundLevelPath==native.levelPath,"First Training lacks its actual Level/Model/Engine authority");
    const auto actors=Census(map);Oracle oracle(actors);
    const auto before=temporary.Save("BeforeRetailHash");const auto revision=GetPortableRuntimeWorldRevision();
    const auto gc=GC::GetStats();const auto lookup=ReadPortableRuntimeObjectLookup();
    const auto rollback=[&] {
        GC::Collect();SameGc(gc,"Retail collision diagnostic leaked GC/actor/birth state");
        const auto outside=ReadPortableRuntimeRetailCollision();
        Require(!outside.initialized&&!outside.enabled&&!outside.hashIdentity&&outside.actors.empty()&&outside.seed==InitialSeed&&
            !outside.initializationDraws&&!outside.lifecycleOperations,
            "Rollback retained process tables/hash/actor mirrors/shared RNG draws");
        const auto current=ReadPortableRuntimeObjectLookup();
        Require(current.generation==lookup.generation&&current.objects==lookup.objects&&
            GetPortableRuntimeWorldRevision()==revision&&!GetPortableRuntimeUnsavedScriptState()&&
            temporary.Save("AfterRetailHash")==before&&GetPortableRuntimeMapActors(true).size()==actors.size()&&
            !Declared("00_Training.LevelInfo0","Engine.LevelInfo.bBegunPlay",Kind::Bool).boolean&&
            Declared("00_Training.LevelInfo0","Engine.LevelInfo.Game",Kind::Object).text.empty()&&
            ReadPortableRuntimeLevelTime()==0.0&&HasPortableRuntimeClient()&&ReadPortableRuntimeClientView().size()==1u,
            "Retail collision diagnostic published dormant world/save/lookup/client state");
    };
    rollback();
    if(movementOnly){
        ActorMoveCoverage(map,actors,rollback);
        AnimatedActorMoveRollback(temporary);
        CommittedAnimatedActorMove(temporary);
        for(const auto& source:sources)source.Unchanged();
        lifetime->Revoke();ClearPortableRuntimeClient();
        std::cout<<"PASS focused actual first Training native movement/clock/publication controls; not startup or playable physics.\n";
        return;
    }
    ModelPointRegionCoverage(map,actors,rollback);
    ActorZoneCases(map,actors,rollback);
    ActorMoveCoverage(map,actors,rollback);
    for(const auto& commands:std::vector<std::vector<Command>>{{},std::vector<Command>(17u),
        {Command{static_cast<Operation>(999)}},{Command{Operation::Enable},Command{static_cast<Operation>(999)}}}) {
        const auto rejected=ProbePortableRuntimeRetailCollision(commands);
        Require(!rejected.available&&!rejected.execution.passed()&&!rejected.execution.committed&&!rejected.execution.error.empty(),
            "Malformed collision command sequence succeeded");++refusals;rollback();
    }
    const auto noop=ProbePortableRuntimeRetailCollision({Command{Operation::Disable}});
    Require(noop.available&&noop.execution.passed()&&!noop.execution.committed&&noop.states.size()==1u&&
        !noop.states.front().initialized&&!noop.states.front().enabled&&noop.states.front().seed==InitialSeed,
        "Disable with absent Level.Hash initialized tables or changed the RNG");rollback();
    const auto target=std::find_if(actors.begin(),actors.end(),[](const auto& actor) {
        return actor.collide&&!actor.deleted&&actor.path=="00_Training.CrateBreakableMedGeneral1";
    });
    Require(target!=actors.end(),"First Training lacks its actual collidable source crate for the provider query");
    const auto& targetPath=target->path;const auto& targetLocation=target->location;
    Command miss{Operation::LineCheck};miss.start={100000.0f,100000.0f,100000.0f};miss.end=miss.start;
    Command hit{Operation::LineCheck};hit.start=targetLocation;hit.end=targetLocation;
    hit.start[0u]-=96.0f;hit.end[0u]+=96.0f;
    const auto expectedMiss=oracle.Query(miss,actors);const auto missTags=oracle.tags;
    const auto expectedHit=oracle.Query(hit,actors);const auto hitTags=oracle.tags;
    Require(expectedMiss.first.empty()&&!expectedHit.first.empty()&&std::any_of(expectedHit.first.begin(),expectedHit.first.end(),
        [&](const auto& packet){return packet.actorPath==targetPath;}),"Actual primitive oracle lacks a positive crate and empty-query control");
    const std::vector<Command> commands{{Operation::Disable},{Operation::Enable},{Operation::Enable},
        {Operation::Disable},{Operation::Enable},miss,hit,{Operation::Disable}};
    const auto result=ProbePortableRuntimeRetailCollision(commands);
    Require(result.available&&result.execution.passed()&&!result.execution.committed&&result.states.size()==commands.size()&&
        result.queries.size()==2u&&result.queryHeads.size()==2u,"Complete rollback-only collision sequence failed: "+result.execution.error);
    const auto& first=result.states[1u];const auto& same=result.states[2u];const auto& disabled=result.states[3u];const auto& renewed=result.states[4u];
    std::cout<<"FIRST TRAINING native hash retained="<<first.retainedBytes<<" actors="<<first.actors.size()
        <<" links="<<first.activeLinks<<" sourceSeed="<<InitialSeed<<" initializedSeed="<<first.seed
        <<" initializationDraws="<<first.initializationDraws<<" lifecycleOperations="<<first.lifecycleOperations<<'\n';
    Require(result.states[0u].initializationDraws==0u&&result.states[0u].lifecycleOperations==0u&&
        first.initializationDraws==49152u&&first.lifecycleOperations>actors.size()&&first.lifecycleOperations<49152u,
        "Fixed table initialization was skipped, redrawn, or mischarged as actor traversal work");
    for(std::size_t state=2u;state<result.states.size();++state)
        Require(result.states[state].initializationDraws==49152u&&
            result.states[state].lifecycleOperations>=result.states[state-1u].lifecycleOperations,
            "Nested enable/disable/query reset the shared initialization or actor work ledger");
    Require(same.lifecycleOperations==first.lifecycleOperations,
        "Repeated no-op enable charged or replayed native registration");
    Require(!result.states[0u].initialized&&first.initialized&&first.initializedHere&&first.enabled&&first.hashIdentity&&
        first.levelPath==native.levelPath&&first.mapGeneration==native.mapGeneration&&first.seed==oracle.seed&&first.tag==0u&&
        first.hashes==1u&&first.activeLinks==oracle.links&&first.allocatedLinks>=first.activeLinks&&first.allocatedLinks%1024u==0u&&first.retainedBytes,
        "First enable lost actual Level/hash identity, source registration count or exact49152 shared CRT draws");
    Require(same.hashIdentity==first.hashIdentity&&same.seed==first.seed&&same.activeLinks==first.activeLinks&&
        same.allocatedLinks==first.allocatedLinks&&same.retainedBytes==first.retainedBytes,
        "Repeated Enable redrew tables, reallocated the hash or duplicated registrations");
    Require(disabled.initialized&&!disabled.enabled&&!disabled.hashIdentity&&!disabled.hashes&&!disabled.activeLinks&&
        !disabled.allocatedLinks&&disabled.seed==first.seed,"Disable did not destroy only the Level-owned hash");
    Require(renewed.enabled&&renewed.hashIdentity&&renewed.hashIdentity!=first.hashIdentity&&renewed.seed==first.seed&&
        renewed.activeLinks==first.activeLinks,"Re-enable reused a retired hash lease or redrew global tables");
    std::vector<std::uint32_t> sourceTags;for(const auto& actor:actors)sourceTags.push_back(actor.tag);
    for(const auto index:{1u,2u,3u,4u})SameActors(result.states[index],actors,sourceTags);
    Require(result.states[5u].tag==1u&&result.states[6u].tag==2u&&result.states[7u].tag==2u&&
        result.states[7u].initialized&&!result.states[7u].enabled,"Queries/disable did not retain process-global tag history within the root");
    SameActors(result.states[5u],actors,missTags);SameActors(result.states[6u],actors,hitTags);SameActors(result.states[7u],actors,hitTags);
    SameHits(result.queries[0u],result.queryHeads[0u],expectedMiss);SameHits(result.queries[1u],result.queryHeads[1u],expectedHit);rollback();
    SetterCoverage(actors,rollback);
    MultiLineCoverage(map,actors,rollback);
    ActorPointCoverage(actors,rollback);
    ModelPointCoverage(map,actors,rollback);
    EncroachmentCoverage(map,actors,rollback);
    ActorBlockingCases(actors,rollback);
    ActorTouchCases(actors,rollback);
    auto bad=hit;bad.extent[1u]=std::numeric_limits<float>::quiet_NaN();
    for(unsigned repeat=0u;repeat<2u;++repeat) {
        const auto failure=ProbePortableRuntimeRetailCollision({Command{Operation::Enable},hit,bad});
        Require(!failure.execution.passed()&&!failure.execution.committed&&!failure.available&&!failure.execution.error.empty(),
            "Unsupported query extent retained a successful hash prefix");++refusals;rollback();
    }
    const auto noHash=ProbePortableRuntimeRetailCollision({hit});
    Require(!noHash.execution.passed()&&!noHash.execution.committed&&!noHash.available&&!noHash.execution.error.empty(),
        "LineCheck without an actual enabled native Level.Hash succeeded");++refusals;rollback();
    PortableWorldStartupProbeOptions options;options.defaultGameClass="DeusEx.DeusExGameInfo";
    QuestVr::Vm::Limits limits;limits.instructions=4'000'000u;limits.nodes=1'048'576u;
    limits.writes=1'048'576u;limits.retainedBytes=128u<<20u;
    const auto startup=ProbePortableRuntimeRetailCollisionStartup(options,limits);
    const auto& world=startup.world;
    std::cout<<"FIRST TRAINING RETAIL STARTUP phase="<<PortableWorldStartupPhaseName(world.phase)<<" slot="<<world.actorSlot
        <<" actor="<<world.actorPath<<" function="<<world.execution.function<<':'<<world.execution.offset
        <<" opcode="<<static_cast<unsigned>(world.execution.opcode)<<" error="<<world.execution.error<<'\n';
    Require(startup.collision.initialized&&startup.collision.enabled&&startup.collision.hashIdentity==first.hashIdentity&&
        startup.collision.seed==first.seed&&startup.collision.levelPath==native.levelPath&&
        startup.collision.actors.size()==actors.size()&&!world.execution.passed()&&!world.execution.committed&&
        !world.execution.error.empty(),"Enhanced startup did not capture the actual hash prefix before its next unsupported native");
    Require(world.phase==PortableWorldStartupPhase::BeginPlay&&world.actorSlot==126u&&
        world.actorPath=="00_Training.DeusExMover16"&&world.execution.function=="Engine.Mover.BeginPlay"&&
        world.execution.offset==197u&&world.execution.opcode==97u&&
        world.execution.error=="Unsupported runtime native 316",
        "First Training startup boundary changed: inspect real evidence before updating this pin");
    for(const auto& actual:startup.collision.actors){
        const auto prior=std::find_if(actors.begin(),actors.end(),[&](const auto& a){return a.path==actual.path;});
        if(prior!=actors.end()&&(prior->location!=actual.location||prior->colLocation!=actual.colLocation||prior->tag!=actual.tag))
            std::cout<<"STARTUP MOVE PREFIX "<<actual.path<<" location=("<<actual.location[0u]<<','<<actual.location[1u]<<','<<actual.location[2u]
                <<") col=("<<actual.colLocation[0u]<<','<<actual.colLocation[1u]<<','<<actual.colLocation[2u]<<") tag="<<actual.tag<<'\n';
    }
    SameActors(startup.collision,actors,sourceTags);rollback();
    const auto again=ProbePortableRuntimeRetailCollisionStartup(options,limits);
    Require(again.world.phase==world.phase&&again.world.actorSlot==world.actorSlot&&again.world.actorPath==world.actorPath&&
        again.world.gameInfoPath==world.gameInfoPath&&again.world.execution.function==world.execution.function&&
        again.world.execution.offset==world.execution.offset&&again.world.execution.opcode==world.execution.opcode&&
        again.world.execution.error==world.execution.error&&again.world.execution.callStack==world.execution.callStack&&
        again.world.visited==world.visited&&again.collision.hashIdentity==startup.collision.hashIdentity&&
        again.collision.seed==startup.collision.seed,"Enhanced startup did not reproduce its exact rolled-back native boundary");rollback();
    auto shortBudget=limits;shortBudget.instructions=2u;
    const auto bounded=ProbePortableRuntimeRetailCollisionStartup(options,shortBudget);
    Require(!bounded.world.execution.passed()&&!bounded.world.execution.committed&&
        bounded.world.execution.status==QuestVr::Vm::Status::Budget&&!bounded.world.execution.error.empty(),
        "Enhanced startup ignored a restrictive shared VM instruction budget");++refusals;rollback();
    // Persistent CollisionTag history is not a generic saved property. Each
    // malformed variant differs from an independently accepted real schema.
    const auto checkpoint=temporary.Write("DormantBeforeInjection",before);Envelope envelope(before);
    const auto rejectSave=[&](const std::string& name,const Envelope& container,const QuestVr::ScriptSavedState& state) {
        const auto path=temporary.Write(name,container.Wrap(state));const auto unchanged=Fingerprint(path);
        const auto save=temporary.Save("BeforeRejectedInjection");const auto worldRevision=GetPortableRuntimeWorldRevision();
        Require(!ValidatePortableRuntimeState(path.string(),"00_Training")&&!LoadPortableRuntimeState(path.string()),
            "Persistent native CollisionTag overlay was admitted: "+name);++refusals;
        Require(temporary.Save("AfterRejectedInjection")==save&&GetPortableRuntimeWorldRevision()==worldRevision&&
            Fingerprint(path)==unchanged,"Rejected CollisionTag fixture changed source/runtime/save state");
    };
    QuestVr::ScriptSavedProperty tag{"Engine.Actor.CollisionTag","CollisionTag",0u,QuestVr::Vm::Value::Integer(12345)};
    auto actorState=envelope.state;
    const auto snapshots=GetPortableRuntimeMapActors(true);
    const auto crate=std::find_if(snapshots.begin(),snapshots.end(),[&](const auto& actor){return actor.objectPath==targetPath;});
    Require(crate!=snapshots.end(),"Cannot authenticate real crate class for persistence fixture");
    QuestVr::ScriptSavedObject patch;patch.path=targetPath;patch.classPath=crate->classPath;
    patch.properties.push_back({"Engine.Actor.Location","Location",0u,QuestVr::Vm::Value::Vector(targetLocation)});
    actorState.objects.push_back(patch);
    Require(ValidatePortableRuntimeState(temporary.Write("ValidActorOverlay",envelope.Wrap(actorState)).string(),"00_Training"),
        "Real declared actor overlay positive control was rejected");
    actorState.objects.back().properties={tag};rejectSave("TransientActorTag",envelope,actorState);rollback();
    auto defaultState=envelope.state;
    defaultState.classDefaults.push_back({crate->classPath,{{"Engine.Actor.CollisionRadius","CollisionRadius",0u,
        ReadPortableClassDefault(crate->classPath,"CollisionRadius")}}});
    Require(ValidatePortableRuntimeState(temporary.Write("ValidDefaultOverlay",envelope.Wrap(defaultState)).string(),"00_Training"),
        "Real inherited default overlay positive control was rejected");
    defaultState.classDefaults.back().properties={tag};rejectSave("TransientDefaultTag",envelope,defaultState);rollback();
    const auto birth=ExecutePortableActorFunction("00_Training.LevelInfo0","Engine.Actor.Spawn",
        {{QuestVr::Vm::Value::Text(Kind::Object,"DeusEx.DeusExGameInfo"),{}}});
    Require(birth.passed()&&birth.committed&&birth.value.kind==Kind::Object&&!birth.value.text.empty(),
        "Dormant original GameInfo native birth positive control failed");
    const auto born=temporary.Save("ValidBornWorld");Envelope bornEnvelope(born);auto bornState=bornEnvelope.state;
    const auto manifest=std::find_if(bornState.births.begin(),bornState.births.end(),[&](const auto& row){return row.path==birth.value.text;});
    Require(manifest!=bornState.births.end()&&ValidatePortableRuntimeState((temporary.directory/"ValidBornWorld.sav").string(),"00_Training"),
        "Authentic frozen-birth manifest positive control failed");
    manifest->frozenDefaults.push_back(tag);rejectSave("TransientFrozenTag",bornEnvelope,bornState);
    Require(LoadPortableRuntimeState(checkpoint.string()),"Cannot restore prebirth first Training checkpoint");
    GC::Collect();Require(temporary.Save("RestoredAfterBirth")==before&&GetPortableRuntimeMapActors(true).size()==actors.size(),
        "Frozen-birth injection control changed dormant first Training state");
    AnimatedActorTouchRollback(temporary,targetPath);
    AnimatedActorZoneRollback(temporary);
    AnimatedActorMoveRollback(temporary);
    CommittedAnimatedActorMove(temporary);
    for(const auto& source:sources)source.Unchanged();
    lifetime->Revoke();ClearPortableRuntimeClient();
    std::cout<<"PASS first Training only:38 exact Quest source packages, "<<actors.size()<<" actual native-slot actors, "
        <<collidable<<" collidable nondeleted actors, "<<oracle.links<<" registration links, "<<providerCalls
        <<" independent-order actual primitive calls; full snapshot/save/revision/GC/RNG rollback. "
        "NOT complete world startup, automatic gravity, live crate destruction/loot or headset evidence.\n";
}
} // namespace

int main(int argc,char** argv) {
    std::cout.setf(std::ios::unitbuf);
    try {
        fs::path root;bool movementOnly{};if(const auto* configured=std::getenv("DEUSEX_GAME_ROOT");configured&&*configured)root=configured;
        for(int i=1;i<argc;++i) {
            const std::string_view option=argv[i];
            if((option=="--owned-source-root"||option=="--game-root")&&i+1<argc)root=argv[++i];
            else if(option=="--movement-only")movementOnly=true;
            else throw std::runtime_error("Usage: portable_retail_collision_test [--owned-source-root OWNED_GAME_ROOT] [--movement-only] (or DEUSEX_GAME_ROOT)");
        }
        if(root.empty()) {std::cout<<"SKIP owned first Training retail collision: use --owned-source-root or DEUSEX_GAME_ROOT.\n";return 77;}
        ShutdownPortableRuntime();GC::Collect();const auto gc=GC::GetStats();
        const auto absent=ProbePortableRuntimeRetailCollision({Command{Operation::Enable}});
        const auto absentStartup=ProbePortableRuntimeRetailCollisionStartup({});
        Require(!absent.available&&!absent.execution.passed()&&!absent.execution.committed&&
            !absentStartup.world.execution.passed()&&!absentStartup.collision.initialized&&
            !ReadPortableRuntimeRetailCollision().initialized,"No-runtime collision diagnostic did not refuse safely");++refusals;
        Run(fs::canonical(root),movementOnly);
        ShutdownPortableRuntime();GC::Collect();SameGc(gc,"Retail collision test leaked rooted native UObjects after shutdown");
        std::cout<<"PASS retail collision integration "<<checks<<" checks / "<<refusals<<" explicit refusals\n";return 0;
    }catch(const std::exception& error) {std::cerr<<"FAIL: "<<error.what()<<'\n';ShutdownPortableRuntime();return 1;}
}
