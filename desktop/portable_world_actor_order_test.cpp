#include "Precomp.h"
#include "GC/GC.h"
#include "portable_unreal_runtime.h"
#include "portable_engine_collision.h"
#include "portable_model_geometry.h"
#include "portable_object_lookup.h"
#include "portable_retail_collision.h"
#include "quest_runtime_script_packages.h"
#include "quest_save_bundle.h"
#include "quest_script_state.h"

#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

// ONLY owned first Training. The production native partition is observed in a
// rollback-only root; explicit portable checkpoints are restored separately.
// This does not run original binaries, startup, Tick, loot or crate physics.
namespace {
namespace fs=std::filesystem;
using Bytes=std::vector<std::uint8_t>;
using Order=QuestVr::ScriptSavedNativeActorOrder;
using Slot=QuestVr::ScriptSavedNativeActorSlot;
using SlotKind=QuestVr::ScriptSavedNativeActorSlotKind;
std::size_t checks{},refusals{};

void Require(bool value,const std::string& message) {
    ++checks;if(!value)throw std::runtime_error(message);
}
std::string Fold(std::string text) {
    for(auto& c:text)if(c>='A'&&c<='Z')c=static_cast<char>(c-'A'+'a');
    return text;
}
bool Within(const fs::path& child,const fs::path& parent) {
    auto a=child.begin();
    for(auto b=parent.begin();b!=parent.end();++a,++b)
        if(a==child.end()||Fold(a->string())!=Fold(b->string()))return false;
    return true;
}
std::uint64_t Fingerprint(const fs::path& path) {
    std::ifstream input(path,std::ios::binary);
    Require(bool(input),"Cannot fingerprint owned source: "+path.string());
    std::array<char,16384u> buffer{};std::uint64_t hash=14695981039346656037ull;
    std::uintmax_t total{};
    while(input) {
        input.read(buffer.data(),static_cast<std::streamsize>(buffer.size()));
        const auto count=input.gcount();total+=static_cast<std::uintmax_t>(count);
        Require(total<=512u*1024u*1024u,"Owned source fingerprint exceeds bound");
        for(std::streamsize i=0;i<count;++i) {
            hash^=static_cast<unsigned char>(buffer[static_cast<std::size_t>(i)]);
            hash*=1099511628211ull;
        }
    }
    Require(input.eof(),"Owned source fingerprint did not finish");return hash;
}
struct Source {
    fs::path path;std::uintmax_t size{};fs::file_time_type time;std::uint64_t hash{};
    explicit Source(const fs::path& file):path(fs::canonical(file)),size(fs::file_size(path)),
        time(fs::last_write_time(path)),hash(Fingerprint(path)){}
    void Unchanged()const {
        Require(fs::file_size(path)==size&&fs::last_write_time(path)==time&&Fingerprint(path)==hash,
            "Original bytes/size/timestamp changed: "+path.string());
    }
};
struct Temporary {
    fs::path parent,directory;std::vector<fs::path> files;
    explicit Temporary(const fs::path& original) {
        parent=fs::canonical(fs::temp_directory_path());
        Require(parent!=parent.root_path()&&!Within(parent,original),
            "Temporary checkpoints overlap original installation/root");
        const auto stamp=std::chrono::steady_clock::now().time_since_epoch().count();
        for(unsigned i=0;i<20u;++i) {
            const auto candidate=parent/("quest-world-actor-order-"+std::to_string(stamp)+'-'+std::to_string(i));
            if(fs::create_directory(candidate)){directory=fs::canonical(candidate);break;}
        }
        Require(!directory.empty()&&directory.parent_path()==parent,"Cannot create isolated checkpoint directory");
    }
    ~Temporary() {
        ShutdownPortableRuntime();std::error_code ignored;
        for(const auto& path:files)if(path.parent_path()==directory)fs::remove(path,ignored);
        if(!directory.empty()&&directory.parent_path()==parent)fs::remove(directory,ignored);
    }
    fs::path Path(const std::string& leaf) {
        Require(!leaf.empty()&&leaf.find_first_of("/\\:")==std::string::npos,"Invalid checkpoint leaf");
        const auto path=directory/(leaf+".sav");
        if(std::find(files.begin(),files.end(),path)==files.end())files.push_back(path);
        return path;
    }
    Bytes Read(const std::string& leaf) {
        Bytes result;
        Require(QuestVr::ReadBoundedSaveFile(Path(leaf).string(),QuestVr::kMaximumSaveRuntimeBytes,result),
            "Cannot read bounded checkpoint: "+leaf);return result;
    }
    Bytes Save(const std::string& leaf) {
        Require(SavePortableRuntimeState(Path(leaf).string()),"Cannot capture full runtime checkpoint: "+leaf);
        return Read(leaf);
    }
    void Write(const std::string& leaf,const Bytes& bytes) {
        Require(!bytes.empty()&&bytes.size()<=QuestVr::kMaximumSaveRuntimeBytes,"Invalid generated checkpoint size");
        std::ofstream output(Path(leaf),std::ios::binary|std::ios::trunc);
        Require(bool(output),"Cannot create isolated generated checkpoint");
        output.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));
        output.close();Require(bool(output),"Cannot finish isolated generated checkpoint");
    }
};
void U32(Bytes& bytes,std::uint32_t value) {
    for(unsigned i=0;i<4u;++i)bytes.push_back(static_cast<std::uint8_t>(value>>(8u*i)));
}
void Replace32(Bytes& bytes,std::size_t offset,std::uint32_t value) {
    Require(offset<=bytes.size()&&bytes.size()-offset>=4u,"Generated word is outside checkpoint");
    for(unsigned i=0;i<4u;++i)bytes[offset+i]=static_cast<std::uint8_t>(value>>(8u*i));
}
std::uint32_t Read32(const Bytes& bytes,std::size_t& cursor) {
    Require(cursor<=bytes.size()&&bytes.size()-cursor>=4u,"Independent checkpoint parser truncated");
    std::uint32_t value{};
    for(unsigned i=0;i<4u;++i)value|=static_cast<std::uint32_t>(bytes[cursor++])<<(8u*i);
    return value;
}
std::uint32_t Version(const Bytes& bytes) {
    std::size_t cursor{};Require(Read32(bytes,cursor)==0x53515844u,"Independent checkpoint magic mismatch");
    return Read32(bytes,cursor);
}
std::size_t PrefixSize(const Bytes& bytes) {
    const auto version=Version(bytes);
    Require(version==3u||version==17u,"Expected simple original v3 or actor-order v17 checkpoint");
    std::size_t cursor=8u;
    const auto string=[&] {
        const auto length=Read32(bytes,cursor);
        Require(cursor<=bytes.size()&&length<=bytes.size()-cursor,"Independent checkpoint string overflow");
        cursor+=length;
    };
    const auto strings=[&] {
        const auto count=Read32(bytes,cursor);
        Require(count<100'000u&&count<=(bytes.size()-cursor)/4u,"Independent checkpoint list overflow");
        for(std::uint32_t i=0;i<count;++i)string();
    };
    strings();strings();strings();
    static_cast<void>(Read32(bytes,cursor));const auto damaged=Read32(bytes,cursor);
    Require(damaged<100'000u&&damaged<=(bytes.size()-cursor)/8u,"Independent damaged list overflow");
    for(std::uint32_t i=0;i<damaged;++i){string();static_cast<void>(Read32(bytes,cursor));}
    static_cast<void>(Read32(bytes,cursor));static_cast<void>(Read32(bytes,cursor));
    for(unsigned i=0;i<4u;++i)strings();
    return cursor;
}
Bytes EnvelopeBlob(const Bytes& pristine,const Bytes& blob) {
    Require(Version(pristine)==3u&&PrefixSize(pristine)==pristine.size(),"Envelope requires a pristine complete v3 prefix");
    Require(blob.size()<=std::numeric_limits<std::uint32_t>::max(),"Generated script blob exceeds word size");
    auto result=pristine;Replace32(result,4u,17u);U32(result,static_cast<std::uint32_t>(blob.size()));
    result.insert(result.end(),blob.begin(),blob.end());return result;
}
Bytes Envelope(const Bytes& pristine,const QuestVr::ScriptSavedState& script) {
    return EnvelopeBlob(pristine,QuestVr::EncodeScriptSavedState(script));
}
QuestVr::ScriptSavedState Decode(const Bytes& bytes) {
    Require(Version(bytes)==17u,"Native Actor ordering was not captured in runtime v17");
    auto cursor=PrefixSize(bytes);const auto size=Read32(bytes,cursor);
    Require(size==bytes.size()-cursor,"Independent v17 script trailer size mismatch");
    return QuestVr::DecodeScriptSavedState(Bytes(bytes.begin()+static_cast<std::ptrdiff_t>(cursor),bytes.end()));
}
std::string ActorPath(const PortablePackageTables& map,std::int32_t reference) {
    Require(reference>0&&static_cast<std::size_t>(reference)<=map.exports.size(),"Original Actor reference is not map-local");
    return "00_Training."+GetPortableObjectPath(map,reference);
}
struct ActorIdentity {
    std::string path,cls,outer;std::uint32_t index{},classIndex{},outerIndex{};bool outerZero{};
    bool operator==(const ActorIdentity&)const=default;
};
std::vector<ActorIdentity> Identities(const PortablePackageTables& map,const std::vector<std::int32_t>& refs) {
    std::vector<ActorIdentity> result;
    for(const auto ref:refs)if(ref) {
        const auto owner=ReadPortableRuntimeObjectOwnership(ActorPath(map,ref));
        result.push_back({owner.objectPath,owner.classPath,owner.outerPath,owner.objectIndex,owner.classIndex,
            owner.outerIndex,owner.serializedOuterZero});
    }
    std::sort(result.begin(),result.end(),[](const auto& a,const auto& b){return a.path<b.path;});return result;
}
PortableWorldActorOrderSnapshot Expected(const PortablePackageTables& map,
    const std::vector<std::int32_t>& refs,const std::optional<std::uint32_t> split={}) {
    PortableWorldActorOrderSnapshot result;result.available=true;result.firstDynamic=split;
    for(std::size_t i=0;i<map.exports.size();++i)if(Fold(GetPortableObjectPath(map,map.exports[i].ObjClass))=="engine.level") {
        Require(result.levelPath.empty(),"Original map contains multiple Levels");
        result.levelPath="00_Training."+GetPortableObjectPath(map,static_cast<std::int32_t>(i+1u));
    }
    Require(!result.levelPath.empty(),"Original map contains no Level");
    for(const auto ref:refs) {
        if(!ref){result.actorIndices.push_back(0u);result.staticFlags.push_back(0u);continue;}
        const auto path=ActorPath(map,ref);const auto owner=ReadPortableRuntimeObjectOwnership(path);
        Require(owner.objectPath==path&&owner.objectIndex!=0u,"Original Actor has no live UObject identity");
        // Exact declaring metadata also remains readable for collected
        // inactive items and retained saved-worldRemoved ownership controls.
        const auto flag=ReadPortableRuntimeDeclaredScriptProperty(path,"Engine.Actor.bStatic");
        Require(flag.kind==QuestVr::Vm::Kind::Bool,"Original declaring bStatic is not Bool");
        result.actorIndices.push_back(owner.objectIndex);result.staticFlags.push_back(flag.boolean?1u:0u);
    }
    return result;
}
struct Partition {std::vector<std::int32_t> refs;std::uint32_t split{};};
Partition StableOracle(const std::vector<std::int32_t>& refs,const std::vector<std::uint8_t>& flags) {
    Require(refs.size()>=2u&&refs.size()==flags.size(),"Independent partition lacks reserved native slots");
    Partition result;result.refs={refs[0],refs[1]};
    // A separate stable selection, not the production coordinator or its output.
    for(std::size_t i=2u;i<refs.size();++i)if(refs[i]&&flags[i])result.refs.push_back(refs[i]);
    result.split=static_cast<std::uint32_t>(result.refs.size());
    for(std::size_t i=2u;i<refs.size();++i)if(refs[i]&&!flags[i])result.refs.push_back(refs[i]);
    return result;
}
Order SavedOrder(const std::string& level,const std::vector<std::int32_t>& refs,
    const std::optional<std::uint32_t> split={}) {
    Order result;result.levelPath=level;result.firstDynamic=split;
    for(const auto ref:refs) {
        Slot slot;if(ref){Require(ref>0,"Negative reference cannot become a local export identity");
            slot.kind=SlotKind::AuthoredExport;slot.authoredExport=static_cast<std::uint32_t>(ref-1);}
        result.slots.push_back(std::move(slot));
    }
    return result;
}
void SameOrder(const Order& actual,const Order& expected) {
    Require(actual.levelPath==expected.levelPath&&actual.firstDynamic==expected.firstDynamic&&
        actual.slots.size()==expected.slots.size(),"Saved native Actor Level/split/count changed");
    for(std::size_t i=0;i<actual.slots.size();++i)Require(actual.slots[i].kind==expected.slots[i].kind&&
        actual.slots[i].authoredExport==expected.slots[i].authoredExport&&
        actual.slots[i].birthPath==expected.slots[i].birthPath,"Saved native Actor slot identity/order changed");
}
void SameHash(const PortableRetailCollisionSnapshot& a,const PortableRetailCollisionSnapshot& b) {
    Require(a.initialized==b.initialized&&a.enabled==b.enabled&&a.initializedHere==b.initializedHere&&
        a.levelPath==b.levelPath&&a.mapGeneration==b.mapGeneration&&a.hashIdentity==b.hashIdentity&&
        a.tag==b.tag&&a.seed==b.seed&&a.retainedBytes==b.retainedBytes&&a.hashes==b.hashes&&
        a.allocatedLinks==b.allocatedLinks&&a.activeLinks==b.activeLinks&&
        a.initializationDraws==b.initializationDraws&&a.lifecycleOperations==b.lifecycleOperations&&
        a.actors.size()==b.actors.size(),"Native Actor ordering changed native collision/hash ownership");
    for(std::size_t i=0;i<a.actors.size();++i) {
        const auto& x=a.actors[i];const auto& y=b.actors[i];
        Require(x.path==y.path&&x.objectIndex==y.objectIndex&&x.tag==y.tag&&x.location==y.location&&
            x.colLocation==y.colLocation&&x.collideActors==y.collideActors&&x.pendingDelete==y.pendingDelete&&
            x.declaredMirrorsMatch==y.declaredMirrorsMatch,"Native hash Actor identity/mirrors changed");
    }
}
struct Observation {
    Bytes save;PortableWorldActorOrderSnapshot order;std::vector<ActorIdentity> identities;
    PortableObjectLookupState lookup;PortableRetailCollisionSnapshot hash;
    std::uint64_t revision{},clockBits{};decltype(GC::GetStats()) gc;
    Observation(Temporary& temporary,const PortablePackageTables& map,const std::vector<std::int32_t>& refs)
        :save(temporary.Save("Witness")),order(ReadPortableWorldActorOrder()),identities(Identities(map,refs)),
        lookup(ReadPortableRuntimeObjectLookup()),hash(ReadPortableRuntimeRetailCollision()),
        revision(GetPortableRuntimeWorldRevision()),clockBits(std::bit_cast<std::uint64_t>(ReadPortableRuntimeLevelTime())) {
        GC::Collect();gc=GC::GetStats();
    }
    void Unchanged(Temporary& temporary,const PortablePackageTables& map,const std::vector<std::int32_t>& refs)const {
        const auto immediate=GC::GetStats();
        Require(immediate.numObjects==gc.numObjects&&immediate.memoryUsage==gc.memoryUsage,
            "Native partition/refused restore retained GC objects at its public boundary");
        Require(ReadPortableWorldActorOrder()==order,"Native Actor array/split/cache rollback failed");
        Require(Identities(map,refs)==identities,"Ordering changed UObject/Class/Outer identity rather than membership");
        Require(GetPortableRuntimeWorldRevision()==revision&&
            std::bit_cast<std::uint64_t>(ReadPortableRuntimeLevelTime())==clockBits,"Revision/native clock changed");
        const auto actual=ReadPortableRuntimeObjectLookup();
        Require(actual.prepared==lookup.prepared&&actual.objects==lookup.objects&&actual.generation==lookup.generation&&
            actual.retainedBytes==lookup.retainedBytes,"UObject lookup ownership changed during Actor ordering");
        SameHash(ReadPortableRuntimeRetailCollision(),hash);
        Require(temporary.Save("AfterWitness")==save,"Full portable checkpoint/animation state rollback failed");
        GC::Collect();const auto after=GC::GetStats();
        Require(after.numObjects==gc.numObjects&&after.memoryUsage==gc.memoryUsage,"Native order rollback leaked GC storage");
    }
};
void Open(const fs::path& root,const std::vector<PortablePackageTables>& packages,const PortablePackageTables& map) {
    ShutdownPortableRuntime();
    Require(InitializePortableRuntime(packages,1u).passed,"Cannot initialize owned original scripts");
    Require(PreparePortableRuntimeNativeEngineClasses().prepared&&PreparePortableRuntimeNativeMetadataClasses().prepared&&
        PreparePortableRuntimeNativeConfigClasses().prepared&&PreparePortableRuntimeNativePackages().prepared&&
        PreparePortableRuntimeObjectLookup().prepared&&
        PreparePortableRuntimeEngineCollision((root/"System"/"DeusEx.ini").string()).prepared,
        "Cannot prepare actual native Engine/Config/lookup/Model prerequisites");
    Require(LoadPortableRuntimeMap(map).passed,"Cannot load ONLY original first Training");
}
void Load(Temporary& temporary,const std::string& leaf) {
    Require(ValidatePortableRuntimeState(temporary.Path(leaf).string(),"00_Training"),"Valid actor-order checkpoint refused validation: "+leaf);
    Require(LoadPortableRuntimeState(temporary.Path(leaf).string()),"Valid actor-order checkpoint refused restore: "+leaf);
}
// Codec12's final section is independently located by its explicit wire widths,
// then verified byte-for-byte before corrupting it. No production parser oracle.
struct OrderOffsets {std::size_t split{};std::vector<std::size_t> tags,payloads;};
OrderOffsets LocateOrder(const Bytes& blob,const Order& order) {
    std::size_t size=4u+order.levelPath.size()+1u+(order.firstDynamic?4u:0u)+4u;
    for(const auto& slot:order.slots)size+=1u+(slot.kind==SlotKind::AuthoredExport?4u:
        slot.kind==SlotKind::Birth?4u+slot.birthPath.size():0u);
    Require(blob.size()>=size,"Independent actor-order section exceeds blob");
    std::size_t cursor=blob.size()-size;const auto text=Read32(blob,cursor);
    Require(text==order.levelPath.size()&&cursor+text<=blob.size()&&
        std::string(blob.begin()+static_cast<std::ptrdiff_t>(cursor),blob.begin()+static_cast<std::ptrdiff_t>(cursor+text))==order.levelPath,
        "Independent actor-order section identity mismatch");cursor+=text;
    Require(blob.at(cursor++)==(order.firstDynamic?1u:0u),"Independent split discriminator mismatch");
    OrderOffsets offsets;offsets.split=cursor;
    if(order.firstDynamic)Require(Read32(blob,cursor)==*order.firstDynamic,"Independent split word mismatch");
    Require(Read32(blob,cursor)==order.slots.size(),"Independent slot-count mismatch");
    for(const auto& slot:order.slots) {
        offsets.tags.push_back(cursor);Require(blob.at(cursor++)==static_cast<std::uint8_t>(slot.kind),"Independent slot tag mismatch");
        offsets.payloads.push_back(cursor);
        if(slot.kind==SlotKind::AuthoredExport)Require(Read32(blob,cursor)==slot.authoredExport,"Independent export identity mismatch");
        else if(slot.kind==SlotKind::Birth){const auto count=Read32(blob,cursor);Require(count==slot.birthPath.size(),"Independent birth length mismatch");cursor+=count;}
    }
    Require(cursor==blob.size(),"Independent native Actor order has an unexpected following section");return offsets;
}

struct MotionLayout {
    std::uint32_t innerVersion{};std::size_t recordsEnd{};std::vector<std::string> owners;
};
MotionLayout InspectMotion(const Bytes& bytes) {
    Require(Version(bytes)==14u,"Actual mover checkpoint did not use format14");
    std::size_t cursor=8u;MotionLayout result;result.innerVersion=Read32(bytes,cursor);
    Require(result.innerVersion==3u||result.innerVersion==17u,"Mover control requires original v3 or native-order v17 inner payload");
    Bytes normalized;U32(normalized,0x53515844u);U32(normalized,result.innerVersion);
    normalized.insert(normalized.end(),bytes.begin()+12,bytes.end());
    cursor=PrefixSize(normalized)+4u;
    const auto count=Read32(bytes,cursor);Require(count==2u,"Actual first-door checkpoint must retain both authored panel motions");
    for(std::uint32_t i=0u;i<count;++i) {
        const auto size=Read32(bytes,cursor);
        Require(size>0u&&size<=4096u&&cursor<=bytes.size()&&size<=bytes.size()-cursor,"Mover owner identity is truncated");
        result.owners.emplace_back(bytes.begin()+static_cast<std::ptrdiff_t>(cursor),
            bytes.begin()+static_cast<std::ptrdiff_t>(cursor+size));cursor+=size;
        // Two xyz/rotator poses, target/return/phase, elapsed/stay Float bits.
        Require(cursor<=bytes.size()&&68u<=bytes.size()-cursor,"Mover state record is truncated");cursor+=68u;
    }
    result.recordsEnd=cursor;
    Require(result.owners==std::vector<std::string>{"00_Training.DeusExMover16","00_Training.DeusExMover17"},
        "Public interaction did not retain the exact original first-door pair");
    if(result.innerVersion==3u)Require(cursor==bytes.size(),"Pure mover/v3 checkpoint has unexpected trailing state");
    else {
        const auto size=Read32(bytes,cursor);Require(size==bytes.size()-cursor,"Mover/v17 trailer size differs");
        const auto saved=QuestVr::DecodeScriptSavedState(Bytes(bytes.begin()+static_cast<std::ptrdiff_t>(cursor),bytes.end()));
        Require(saved.nativeActorOrder.has_value(),"Actual mover/v17 payload lacks native Actor ownership");
    }
    return result;
}
Bytes MotionWithScript(const Bytes& actual,const QuestVr::ScriptSavedState& script) {
    const auto layout=InspectMotion(actual);Bytes result(actual.begin(),actual.begin()+static_cast<std::ptrdiff_t>(layout.recordsEnd));
    Replace32(result,8u,17u);const auto blob=QuestVr::EncodeScriptSavedState(script);
    U32(result,static_cast<std::uint32_t>(blob.size()));result.insert(result.end(),blob.begin(),blob.end());return result;
}
struct DoorPose {
    std::string path;std::array<std::uint32_t,3u> location{};std::array<std::int32_t,3u> rotation{};
    bool moving{},closing{},activated{};
    bool operator==(const DoorPose&)const=default;
};
std::vector<DoorPose> DoorPoses() {
    std::vector<DoorPose> result;
    for(const auto& actor:GetPortableRuntimeMoverActors())if(actor.objectPath=="00_Training.DeusExMover16"||
        actor.objectPath=="00_Training.DeusExMover17")result.push_back({actor.objectPath,
            {std::bit_cast<std::uint32_t>(actor.x),std::bit_cast<std::uint32_t>(actor.y),std::bit_cast<std::uint32_t>(actor.z)},
            {actor.pitch,actor.yaw,actor.roll},actor.moverMoving,actor.moverClosing,actor.activated});
    Require(result.size()==2u,"Actual first-door pair is absent from native mover view");return result;
}
QuestVr::ScriptSavedState RemovedActor(const PortablePackageTables& map,const std::vector<std::int32_t>& refs,
    const std::string& level,const std::string& path) {
    const auto identity=ReadPortableRuntimeObjectOwnership(path);auto omitted=refs;std::size_t found{};
    for(auto& reference:omitted)if(reference&&ActorPath(map,reference)==path){reference=0;++found;}
    Require(found==1u,"Removed ownership control must omit exactly one authored native Actor");
    QuestVr::ScriptSavedState saved;saved.mapName="00_Training";saved.nativeActorOrder=SavedOrder(level,omitted);
    QuestVr::ScriptSavedObject object;object.path=path;object.classPath=identity.classPath;
    object.lifecycle=QuestVr::ScriptSavedActorLifecycle{};object.lifecycle->worldRemoved=true;
    saved.objects.push_back(std::move(object));return saved;
}
void MoverOwnership(Temporary& temporary,const fs::path& root,const std::vector<PortablePackageTables>& packages,
    const PortablePackageTables& map,const std::vector<std::int32_t>& original,const Bytes& pristine,
    const Partition& partition) {
    Load(temporary,"Legacy");const auto baseline=Expected(map,original);const auto closed=DoorPoses();
    const auto animate=[&] {
        const auto opened=InteractPortableRuntimeActor("00_Training.DeusExMover16");
        Require(opened.handled&&opened.worldChanged&&opened.action=="mover_open"&&AdvancePortableRuntimeMovers(0.2f),
            "Cannot obtain genuine first-door motion through the public interaction layer");
        const auto pose=DoorPoses();Require(pose!=closed&&pose[0].moving&&pose[1].moving,
            "Public mover control did not advance actual authored first-door geometry");return pose;
    };
    const auto legacyPose=animate();const auto legacyMotion=temporary.Save("Mover14Legacy");
    Require(InspectMotion(legacyMotion).innerVersion==3u,"Original motion control unexpectedly owns script state");
    Load(temporary,"CanonicalPartition");const auto ordered=Expected(map,partition.refs,partition.split);
    const auto orderedPose=animate();const auto orderedMotion=temporary.Save("Mover14Order");
    Require(InspectMotion(orderedMotion).innerVersion==17u,"Explicit native-order motion was not captured as14+17");
    auto removed=RemovedActor(map,original,baseline.levelPath,"00_Training.DeusExMover16");
    temporary.Write("RemovedMover",Envelope(pristine,removed));
    const auto removedIndex=ReadPortableRuntimeObjectOwnership("00_Training.DeusExMover16").objectIndex;
    auto omitted=baseline;
    const auto slot=std::find(omitted.actorIndices.begin(),omitted.actorIndices.end(),removedIndex);
    Require(slot!=omitted.actorIndices.end(),"Original first-door panel lacks native membership");
    const auto position=static_cast<std::size_t>(slot-omitted.actorIndices.begin());
    const auto establishRemoved=[&] {
        // Native UObject indices are session leases, not save identities.
        // Rebuild this witness after a cold Open rather than reusing indices.
        auto currentOmitted=Expected(map,original);
        currentOmitted.actorIndices[position]=0u;currentOmitted.staticFlags[position]=0u;
        Load(temporary,"RemovedMover");
        Require(ReadPortableWorldActorOrder()==currentOmitted&&GetPortableRuntimeMoverActors(true).empty(),
            "No-motion v17 control did not remove its saved mover/clear stale motion");
    };
    for(const bool explicitOrder:{false,true}) {
        establishRemoved();const Observation witness(temporary,map,original);
        const auto leaf=explicitOrder?"Mover14Order":"Mover14Legacy";
        Require(ValidatePortableRuntimeState(temporary.Path(leaf).string(),"00_Training"),
            "Same-map mover preflight incorrectly depends on CURRENT removed ownership");
        witness.Unchanged(temporary,map,original);Load(temporary,leaf);
        Require(ReadPortableWorldActorOrder()==(explicitOrder?ordered:baseline)&&
            DoorPoses()==(explicitOrder?orderedPose:legacyPose)&&temporary.Save("MoverRestored")==
                (explicitOrder?orderedMotion:legacyMotion),
            "Earlier mover checkpoint did not restore exact saved motion/order over a currently removed mover");
    }
    establishRemoved();const Observation refusalBefore(temporary,map,original);
    const auto rejected=[&](const Bytes& bytes,const char* why) {
        temporary.Write("InvalidMoverOwnership",bytes);
        Require(!ValidatePortableRuntimeState(temporary.Path("InvalidMoverOwnership").string(),"00_Training")&&
            !LoadPortableRuntimeState(temporary.Path("InvalidMoverOwnership").string()),why);
        refusalBefore.Unchanged(temporary,map,original);
        Require(temporary.Read("InvalidMoverOwnership")==bytes&&temporary.Read("Mover14Order")==orderedMotion&&
            temporary.Read("Mover14Legacy")==legacyMotion,"Refused saved mover motion overwrote existing checkpoint files");++refusals;
    };
    rejected(MotionWithScript(orderedMotion,removed),"Saved omitted+removed mover retained forbidden motion");
    auto retainedRemoved=removed;retainedRemoved.nativeActorOrder=SavedOrder(baseline.levelPath,partition.refs,partition.split);
    rejected(MotionWithScript(orderedMotion,retainedRemoved),"Saved removed mover retained motion despite its native slot membership");
    Open(root,packages,map);establishRemoved();Load(temporary,"Mover14Order");
    Require(ReadPortableWorldActorOrder()==Expected(map,partition.refs,partition.split)&&DoorPoses()==orderedPose&&
        temporary.Save("MoverColdRestored")==orderedMotion,"Cold removed-mover replacement lost exact14+17 motion/order");
    Load(temporary,"Legacy");Require(temporary.Save("MoverFinalLegacy")==pristine,"Mover ownership regression did not reset legacy state");
    std::cout<<"PASS ONLY first Training mover checkpoint ownership: genuine first-door14+3/14+17 motion, removed-current warm/cold restoration, saved-removed motion refusals\n";
}
bool SameQuick(const PortableQuickBeltSnapshot& a,const PortableQuickBeltSnapshot& b) {
    if(a.available!=b.available||a.equippedActorPath!=b.equippedActorPath||a.collectedKeys!=b.collectedKeys)return false;
    for(std::size_t i=0u;i<a.slots.size();++i) {
        if(a.slots[i].has_value()!=b.slots[i].has_value())return false;
        if(!a.slots[i])continue;
        const auto& x=*a.slots[i];const auto& y=*b.slots[i];
        if(x.actorPath!=y.actorPath||x.classPath!=y.classPath||x.iconPath!=y.iconPath||x.description!=y.description||
            x.keyRing!=y.keyRing||x.selected!=y.selected)return false;
    }
    return true;
}
void QuickBeltOwnership(Temporary& temporary,const fs::path& root,const std::vector<PortablePackageTables>& packages,
    const PortablePackageTables& map,const std::vector<std::int32_t>& original,const Bytes& pristine) {
    Load(temporary,"Legacy");const auto baseline=Expected(map,original);
    const auto actors=GetPortableRuntimeMapActors();
    const auto chosen=std::find_if(actors.begin(),actors.end(),[](const auto& actor) {
        return actor.inventory&&actor.classPath=="DeusEx.WeaponCrowbar";
    });
    Require(chosen!=actors.end(),"Actual first Training has no usable crowbar equipment control");
    const auto item=chosen->objectPath;const auto displayable=ReadPortableActorDeclaredScriptProperty(item,"Engine.Inventory.bDisplayableInv");
    Require(displayable.kind==QuestVr::Vm::Kind::Bool&&displayable.boolean,
        "Actual collected crowbar lacks source-authenticated displayable-inventory eligibility");
    const auto picked=InteractPortableRuntimeActor(item);
    Require(picked.handled&&picked.worldChanged&&picked.action=="pickup"&&EquipPortableRuntimeInventoryItem(item),
        "Cannot obtain genuine owned/selected original crowbar quickbelt checkpoint");
    const auto expected=ReadPortableRuntimeQuickBelt();const auto inventory=GetPortableRuntimeInventoryItems();
    Require(expected.available&&expected.equippedActorPath==item&&GetPortableRuntimeEquippedItem()==item&&
        inventory==std::vector<std::string>{item},"Genuine quickbelt control lost ownership/selection");
    const auto saved=temporary.Save("OwnedQuickBelt");Require(Version(saved)==15u,"Actual selected equipment did not produce quickbelt format15");
    auto removed=RemovedActor(map,original,baseline.levelPath,item);
    temporary.Write("RemovedQuickItem",Envelope(pristine,removed));
    const auto itemIndex=ReadPortableRuntimeObjectOwnership(item).objectIndex;auto omitted=baseline;
    const auto slot=std::find(omitted.actorIndices.begin(),omitted.actorIndices.end(),itemIndex);
    Require(slot!=omitted.actorIndices.end(),"Selected original inventory item lacks native membership");
    const auto position=static_cast<std::size_t>(slot-omitted.actorIndices.begin());
    const auto establishRemoved=[&] {
        auto currentOmitted=Expected(map,original);
        currentOmitted.actorIndices[position]=0u;currentOmitted.staticFlags[position]=0u;
        Load(temporary,"RemovedQuickItem");
        Require(ReadPortableWorldActorOrder()==currentOmitted&&GetPortableRuntimeInventoryItems().empty(),
            "Explicit removed-item control retained live owned inventory membership");
    };
    const auto restore=[&] {
        const Observation witness(temporary,map,original);
        Require(ValidatePortableRuntimeState(temporary.Path("OwnedQuickBelt").string(),"00_Training"),
            "Quickbelt preflight incorrectly depends on the current item's removed/overlay view");
        witness.Unchanged(temporary,map,original);Load(temporary,"OwnedQuickBelt");
        Require(ReadPortableWorldActorOrder()==Expected(map,original)&&SameQuick(ReadPortableRuntimeQuickBelt(),expected)&&
            GetPortableRuntimeInventoryItems()==inventory&&GetPortableRuntimeEquippedItem()==item&&
            ReadPortableRuntimeDeclaredScriptProperty(item,"Engine.Inventory.bDisplayableInv").boolean&&
            temporary.Save("QuickBeltRestored")==saved,
            "Saved/prepared quickbelt restore lost original slots, selected item, inventory or exact checkpoint");
    };
    establishRemoved();restore();establishRemoved();restore();
    // A typed saved Bool overlay creates a genuinely different CURRENT view.
    // Restoring the earlier public checkpoint must use its immutable authored
    // eligibility (the earlier snapshot has no bDisplayableInv override).
    QuestVr::ScriptSavedState hidden;hidden.mapName="00_Training";
    hidden.nativeActorOrder=SavedOrder(baseline.levelPath,original);
    QuestVr::ScriptSavedObject overlay;overlay.path=item;overlay.classPath=chosen->classPath;
    overlay.properties.push_back({"Engine.Inventory.bDisplayableInv","bDisplayableInv",0u,QuestVr::Vm::Value::Bool(false)});
    hidden.objects.push_back(std::move(overlay));temporary.Write("HiddenQuickItem",Envelope(pristine,hidden));
    Load(temporary,"HiddenQuickItem");
    Require(!ReadPortableActorDeclaredScriptProperty(item,"Engine.Inventory.bDisplayableInv").boolean,
        "Current eligibility overlay control failed to establish a different real Bool view");restore();
    Open(root,packages,map);establishRemoved();restore();
    Require(temporary.Read("OwnedQuickBelt")==saved&&temporary.Read("RemovedQuickItem")==Envelope(pristine,removed),
        "Warm/cold quickbelt restore altered existing generated checkpoints");
    Load(temporary,"Legacy");Require(temporary.Save("QuickBeltFinalLegacy")==pristine,
        "Quickbelt ownership regression did not reset native equipment/legacy baseline");
    std::cout<<"PASS ONLY first Training saved quickbelt ownership: actual collected/selected crowbar, removed-current warm/cold restore and opposing current displayable Bool overlay\n";
}

void Run(const fs::path& root) {
    Temporary temporary(root);std::vector<PortablePackageTables> packages;std::vector<Source> sources;
    for(const auto* name:QuestVr::RuntimeScriptPackages) {
        const auto file=root/"System"/(std::string(name)+".u");sources.emplace_back(file);
        packages.push_back(LoadPortablePackageTables(file.string()));
    }
    sources.emplace_back(root/"Maps"/"00_Training.dx");sources.emplace_back(root/"System"/"DeusEx.ini");
    const auto map=LoadPortablePackageTables((root/"Maps"/"00_Training.dx").string());
    const auto original=ReadPortableLevel68ActorOrder(map);Require(original.size()>=2u,"First Training lacks reserved Actor slots");
    Open(root,packages,map);const auto authored=Expected(map,original);
    Require(ReadPortableWorldActorOrder()==authored,"Loaded native Actor order does not match original serialized membership");
    const auto pristine=temporary.Save("Legacy");
    Require(Version(pristine)==3u&&PrefixSize(pristine)==pristine.size(),"Pristine seed1 first Training is not a complete legacy v3 checkpoint");
    const auto partition=StableOracle(original,authored.staticFlags);
    Require(partition.refs!=original&&partition.split>=2u&&partition.split<partition.refs.size(),
        "Actual first-Training case does not distinguish stable static/dynamic native ordering");
    const auto expected=Expected(map,partition.refs,partition.split);
    const auto waiting=ExecutePortableActorFunction("00_Training.Doctor1","PlayWaiting");
    Require(waiting.passed()&&waiting.committed,"Cannot seed a real unrelated NPC animation clock");
    const Observation before(temporary,map,original);
    const auto completed=[&](const PortableWorldActorPartitionProbeResult& result) {
        Require(result.execution.passed()&&!result.execution.committed,
            "Actual native partition did not complete rollback-only: "+result.execution.error);
        Require(result.before==authored&&result.after==expected,"Actual partition disagrees with independent stable original-source oracle");
        before.Unchanged(temporary,map,original);
    };
    const auto full=ProbePortableWorldActorPartition();completed(full);
    const auto warm=ProbePortableWorldActorPartition();completed(warm);
    Require(warm.execution.nativeWork&&warm.execution.nativeWork->used>3u,"Partition did not use its caller's native-work ledger");
    const auto used=warm.execution.nativeWork->used;
    for(const auto prefix:{std::size_t{0u},used/2u,used-1u}) {
        QuestVr::Vm::Limits limits;limits.nativeWork=prefix;
        const auto result=ProbePortableWorldActorPartition(limits);
        Require(!result.execution.passed()&&!result.execution.committed&&result.execution.nativeWork&&
            result.execution.nativeWork->exhausted,"Native partition ignored shared native-work prefix");
        before.Unchanged(temporary,map,original);++refusals;
    }

    // Reproduce the full-order/sparse-deletion undo composition bug with a
    // genuine native DestroyActor, not a synthetic slot clear or Destroyed.
    // The deleted crate MUST move to an index whose original occupant differs:
    // an erroneous post-partition sparse before-image would overwrite that
    // exact old occupant after the full-order rollback swap.
    std::string crate;std::uint32_t crateIndex{};std::size_t oldSlot{},newSlot{};
    for(const auto* candidate:{"00_Training.CrateBreakableMedGeneral1","00_Training.CrateBreakableMedGeneral2"}) {
        const auto identity=ReadPortableRuntimeObjectOwnership(candidate).objectIndex;
        const auto old=std::find(authored.actorIndices.begin(),authored.actorIndices.end(),identity);
        const auto next=std::find(expected.actorIndices.begin(),expected.actorIndices.end(),identity);
        if(identity==0u||old==authored.actorIndices.end()||next==expected.actorIndices.end())continue;
        const auto oldIndex=static_cast<std::size_t>(old-authored.actorIndices.begin());
        const auto newIndex=static_cast<std::size_t>(next-expected.actorIndices.begin());
        if(oldIndex==newIndex||newIndex>=authored.actorIndices.size()||authored.actorIndices[newIndex]==identity)continue;
        crate=candidate;crateIndex=identity;oldSlot=oldIndex;newSlot=newIndex;break;
    }
    Require(!crate.empty()&&oldSlot!=newSlot&&newSlot>=partition.split&&
        authored.actorIndices[newSlot]!=crateIndex,"Actual first Training lacks a moved destructible-crate rollback witness");
    const std::vector<std::string> destroyPaths{crate};
    const auto linkerBefore=ReadPortableRuntimeObjectLinker(crate);
    const auto deferredBefore=ReadPortableRuntimeDeferredOwnership();
    Require(linkerBefore.bound&&linkerBefore.attached&&linkerBefore.sourceExport&&
        linkerBefore.objectIndex==crateIndex&&deferredBefore.available,
        "Moved crate lacks original live linker/deferred native ownership");
    auto deletedOrder=expected;deletedOrder.actorIndices[newSlot]=0u;deletedOrder.staticFlags[newSlot]=0u;
    auto deletedDeferred=deferredBefore;deletedDeferred.firstDeletedPath=crate;
    const auto deletedLink=std::find_if(deletedDeferred.actorLinks.begin(),deletedDeferred.actorLinks.end(),
        [&](const auto& item){return item.first==crate;});
    Require(deletedLink!=deletedDeferred.actorLinks.end(),"Moved crate lacks bound original Actor.Deleted storage");
    deletedLink->second=deferredBefore.firstDeletedPath;
    const auto destructionRollback=[&] {
        before.Unchanged(temporary,map,original);
        Require(ReadPortableRuntimeObjectLinker(crate)==linkerBefore&&
            ReadPortableRuntimeDeferredOwnership()==deferredBefore,
            "Partition followed by destruction did not restore exact linker/deferred ownership");
        const auto actual=ReadPortableWorldActorOrder();
        Require(actual.actorIndices.at(oldSlot)==crateIndex&&
            actual.actorIndices.at(newSlot)==authored.actorIndices[newSlot],
            "Sparse post-partition deletion undo corrupted the full original Actor order");
    };
    const auto destroyed=[&](const PortableWorldActorPartitionDestroyProbeResult& result) {
        Require(result.execution.passed()&&!result.execution.committed&&result.destruction.completed&&
            result.destruction.returned==std::vector<std::int32_t>{1},
            "Real same-root partition then native DestroyActor refused: "+result.execution.error);
        Require(result.before==authored&&result.partitioned==expected&&result.afterDestroy==deletedOrder,
            "Native destruction did not clear only the reordered crate slot while preserving count/split/other identities");
        Require(result.destruction.before==deferredBefore&&result.destruction.after==deletedDeferred,
            "Native destruction did not retain the original deferred LIFO head/Deleted edge");
        Require(result.linkersBefore==std::vector<PortableObjectLinkerSnapshot>{linkerBefore}&&result.linkersAfter.size()==1u,
            "Same-root partition/destruction lost exact live linker observations");
        const auto& detached=result.linkersAfter.front();
        Require(detached.bound&&!detached.attached&&detached.sourceExport&&detached.linkerIdentity==0u&&
            detached.sourceLinkerIdentity==linkerBefore.sourceLinkerIdentity&&detached.exportObjectPath.empty()&&
            detached.sourcePath==linkerBefore.sourcePath&&detached.sourceExportIndex==linkerBefore.sourceExportIndex&&
            detached.objectIndex==linkerBefore.objectIndex&&detached.outerIndex==linkerBefore.outerIndex,
            "Real ConditionalDestroy did not detach only the authored source export while preserving UObject/Outer provenance");
        Require(linkerBefore.outerIndex<=static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max())&&
            detached.nativeIndex==static_cast<std::int32_t>(linkerBefore.outerIndex)&&
            // The diagnostic's genuine InitExecution adds HasStack before
            // ConditionalDestroy adds Destroyed/DebugDestroy; no other bits.
            detached.flags==(linkerBefore.flags|0x02000000u|0x80800000u),
            "Actual InitExecution/ConditionalDestroy header bits or native Outer index differ");
        destructionRollback();
    };
    const auto partitionDestroyed=ProbePortableWorldActorPartitionThenDestroy(destroyPaths);destroyed(partitionDestroyed);
    const auto partitionDestroyedWarm=ProbePortableWorldActorPartitionThenDestroy(destroyPaths);destroyed(partitionDestroyedWarm);
    Require(partitionDestroyedWarm.execution.writes>0u&&partitionDestroyedWarm.execution.writes<128u,
        "Actual partition/deletion VM write-prefix sweep exceeds bounded case count");
    for(std::size_t prefix=0u;prefix<partitionDestroyedWarm.execution.writes;++prefix) {
        QuestVr::Vm::Limits limits;limits.writes=prefix;
        const auto result=ProbePortableWorldActorPartitionThenDestroy(destroyPaths,limits);
        Require(!result.execution.passed()&&!result.execution.committed,"A refused same-root partition/deletion write prefix was published");
        destructionRollback();++refusals;
    }
    Require(partitionDestroyedWarm.execution.nativeWork&&partitionDestroyedWarm.execution.nativeWork->used>3u,
        "Same-root partition/deletion did not share the original caller native-work ledger");
    const auto destructionWork=partitionDestroyedWarm.execution.nativeWork->used;
    for(const auto prefix:{std::size_t{0u},destructionWork/2u,destructionWork-1u}) {
        QuestVr::Vm::Limits limits;limits.nativeWork=prefix;
        const auto result=ProbePortableWorldActorPartitionThenDestroy(destroyPaths,limits);
        Require(!result.execution.passed()&&!result.execution.committed&&result.execution.nativeWork&&
            result.execution.nativeWork->exhausted,"Same-root partition/deletion ignored native-work refusal prefix");
        destructionRollback();++refusals;
    }
    std::cout<<"NATIVE PARTITION/DESTROY ROLLBACK actor="<<crate<<" originalSlot="<<oldSlot
        <<" reorderedSlot="<<newSlot<<" originalReorderedSlotOccupant="<<authored.actorIndices[newSlot]
        <<" writes="<<partitionDestroyedWarm.execution.writes<<" sharedNativeWork="<<destructionWork
        <<"; genuine native deletion/queue/linker edges, NOT crate combat/loot/gravity acceptance\n";

    // Restore, do not merge the unrelated animation overlay into manual saves.
    Load(temporary,"Legacy");Require(ReadPortableWorldActorOrder()==authored&&temporary.Save("Reset")==pristine,
        "Legacy v3 did not reinstate authored membership/split/animation baseline");
    QuestVr::ScriptSavedState saved;saved.mapName="00_Training";
    saved.nativeActorOrder=SavedOrder(authored.levelPath,partition.refs,partition.split);
    temporary.Write("Partition",Envelope(pristine,saved));Load(temporary,"Partition");
    Require(ReadPortableWorldActorOrder()==Expected(map,partition.refs,partition.split),"Public restore did not install explicit original native partition");
    const auto canonical=temporary.Save("CanonicalPartition");const auto decoded=Decode(canonical);
    Require(decoded.nativeActorOrder.has_value(),"v17 lost its native Actor order section");SameOrder(*decoded.nativeActorOrder,*saved.nativeActorOrder);
    Open(root,packages,map);Require(ReadPortableWorldActorOrder()==Expected(map,original),"Cold loader inherited another world's partition");
    Load(temporary,"CanonicalPartition");
    Require(ReadPortableWorldActorOrder()==Expected(map,partition.refs,partition.split)&&temporary.Save("ColdPartition")==canonical,
        "Cold native partition restore is not exact/deterministic");
    Load(temporary,"Legacy");Require(ReadPortableWorldActorOrder()==Expected(map,original)&&temporary.Save("LegacyReset")==pristine,
        "Legacy save merged the live compacted Actor prefix instead of original holes/unbound split");

    // Preserve all actors, but explicitly permute nonreserved identities and
    // add native null holes. No worldRemoved lifecycle or fake actor flags.
    auto interleaved=partition.refs;std::vector<std::size_t> nonnull;
    for(std::size_t i=2u;i<interleaved.size();++i)if(interleaved[i])nonnull.push_back(i);
    Require(nonnull.size()>2u,"Actual map lacks distinct nonreserved actors");
    std::swap(interleaved[nonnull.front()],interleaved[nonnull.back()]);
    interleaved.insert(interleaved.begin()+2,0);interleaved.push_back(0);
    saved.nativeActorOrder=SavedOrder(authored.levelPath,interleaved);
    temporary.Write("Interleaved",Envelope(pristine,saved));Load(temporary,"Interleaved");
    Require(ReadPortableWorldActorOrder()==Expected(map,interleaved),"Explicit unbound split/permutation/null holes were normalized away");
    const auto interleavedSave=temporary.Save("CanonicalInterleaved");
    const auto interleavedDecoded=Decode(interleavedSave);
    Require(interleavedDecoded.nativeActorOrder.has_value(),"Nonbaseline slots were not saved without firstDynamic");
    SameOrder(*interleavedDecoded.nativeActorOrder,*saved.nativeActorOrder);
    Open(root,packages,map);Load(temporary,"CanonicalInterleaved");
    Require(ReadPortableWorldActorOrder()==Expected(map,interleaved)&&temporary.Save("ColdInterleaved")==interleavedSave,
        "Cold save restore did not preserve unbound split/exact Actor identities/null holes");
    const Observation invalidBefore(temporary,map,original);
    const auto rejected=[&](const std::string& label,const Bytes& bytes) {
        temporary.Write("Invalid",bytes);
        Require(!ValidatePortableRuntimeState(temporary.Path("Invalid").string(),"00_Training")&&
            !LoadPortableRuntimeState(temporary.Path("Invalid").string()),"Malformed native Actor checkpoint validated/loaded: "+label);
        invalidBefore.Unchanged(temporary,map,original);
        Require(temporary.Read("Invalid")==bytes&&temporary.Read("CanonicalInterleaved")==interleavedSave&&
            temporary.Read("CanonicalPartition")==canonical&&temporary.Read("Legacy")==pristine,
            "Refused restore modified an existing checkpoint/input file: "+label);++refusals;
    };
    auto bad=saved;bad.nativeActorOrder->levelPath="00_Training.Doctor1";rejected("wrong actual Level",Envelope(pristine,bad));
    std::size_t member=2u;while(member<saved.nativeActorOrder->slots.size()&&saved.nativeActorOrder->slots[member].kind!=SlotKind::AuthoredExport)++member;
    Require(member<saved.nativeActorOrder->slots.size(),"Cannot locate authored corruption control");
    bad=saved;bad.nativeActorOrder->slots[member].authoredExport=static_cast<std::uint32_t>(map.exports.size());
    rejected("out-of-map export identity",Envelope(pristine,bad));
    bad=saved;bad.nativeActorOrder->slots[member].authoredExport=static_cast<std::uint32_t>(FindPortableRootModel68Export(map));
    rejected("local non-Actor Model export",Envelope(pristine,bad));
    bad=saved;bad.nativeActorOrder->slots[member]={};
    rejected("omitted live Actor without worldRemoved lifecycle",Envelope(pristine,bad));
    bad=saved;std::swap(bad.nativeActorOrder->slots[0],bad.nativeActorOrder->slots[member]);
    rejected("reserved slot0 identity replaced",Envelope(pristine,bad));
    auto splitSaved=saved;splitSaved.nativeActorOrder->firstDynamic=2u;
    const auto blob=QuestVr::EncodeScriptSavedState(splitSaved);const auto offsets=LocateOrder(blob,*splitSaved.nativeActorOrder);
    auto corrupted=blob;Replace32(corrupted,offsets.split,1u);rejected("split before reserved slots",EnvelopeBlob(pristine,corrupted));
    corrupted=blob;Replace32(corrupted,offsets.split,static_cast<std::uint32_t>(interleaved.size()+1u));
    rejected("split past array",EnvelopeBlob(pristine,corrupted));
    std::size_t second=member+1u;
    while(second<splitSaved.nativeActorOrder->slots.size()&&splitSaved.nativeActorOrder->slots[second].kind!=SlotKind::AuthoredExport)++second;
    Require(second<splitSaved.nativeActorOrder->slots.size(),"Cannot locate duplicate-export corruption control");
    corrupted=blob;Replace32(corrupted,offsets.payloads[second],splitSaved.nativeActorOrder->slots[member].authoredExport);
    rejected("duplicate authored export",EnvelopeBlob(pristine,corrupted));
    corrupted=blob;corrupted[offsets.tags[member]]=255u;rejected("unknown native slot tag",EnvelopeBlob(pristine,corrupted));
    corrupted=blob;corrupted.pop_back();rejected("truncated final native slot",EnvelopeBlob(pristine,corrupted));
    corrupted=EnvelopeBlob(pristine,blob);Replace32(corrupted,4u,16u);
    rejected("codec12 inside legacy level-clock envelope",corrupted);
    Load(temporary,"Legacy");Require(ReadPortableWorldActorOrder()==Expected(map,original)&&temporary.Save("FinalLegacy")==pristine,
        "Legacy reset retained malformed/interleaved live ordering");
    MoverOwnership(temporary,root,packages,map,original,pristine,partition);
    QuickBeltOwnership(temporary,root,packages,map,original,pristine);
    ShutdownPortableRuntime();for(const auto& source:sources)source.Unchanged();
    std::cout<<"PASS ONLY first Training original native partition: "<<original.size()<<" authored slots -> "
        <<partition.refs.size()<<" retained, split "<<partition.split<<", "<<used<<" shared native work; rollback, v17 cold/order/hole restore, legacy v3 reset and malformed-save refusal.\n"
        <<"UNCHANGED "<<sources.size()<<" owned source files; NOT complete startup/Tick/crate loot/gravity.\n";
}
}
int main(int argc,char** argv) {
    std::cout.setf(std::ios::unitbuf);
    try {
        Require(argc<=2,"Usage: portable_world_actor_order_test [OWNED_GAME_ROOT]");
        const auto* env=std::getenv("DEUSEX_GAME_ROOT");
        const fs::path root=argc==2?fs::path(argv[1]):env?fs::path(env):fs::path{};
        if(root.empty()){std::cout<<"SKIP owned first-Training native Actor ordering\n";return 77;}
        ShutdownPortableRuntime();GC::Collect();const auto before=GC::GetStats();
        Run(fs::canonical(root));ShutdownPortableRuntime();GC::Collect();const auto after=GC::GetStats();
        Require(before.numObjects==after.numObjects&&before.memoryUsage==after.memoryUsage,"Cold native Actor order lifecycle leaked GC storage");
        std::cout<<"PASS native Actor order: "<<checks<<" checks / "<<refusals<<" refusal controls\n";return 0;
    } catch(const std::exception& error) {
        ShutdownPortableRuntime();std::cerr<<"FAIL native Actor order: "<<error.what()<<'\n';return 1;
    }
}
