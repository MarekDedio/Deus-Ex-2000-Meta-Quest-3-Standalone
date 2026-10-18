#include "Precomp.h"
#include "GC/GC.h"
#include "portable_engine_collision.h"
#include "portable_object_lookup.h"
#include "portable_unreal_runtime.h"
#include "quest_actor_line_trace.h"
#include "quest_collision_hash_math.h"
#include "quest_save_bundle.h"
#include "quest_script_state.h"
#include "quest_runtime_script_packages.h"

#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

// Actual Engine/default/field/Transient/Primitive ownership in a dormant first
// Training map. Original files are read as data, never executed or modified.
// Cylinder arithmetic below is an ADAPTER oracle: independent kernel tests own
// the numerical evidence. This is not begun-world, Move, gravity or loot proof.
namespace {
namespace fs=std::filesystem;
using Bytes=std::vector<std::uint8_t>;
using Kind=QuestVr::Vm::Kind;
using Array=std::array<float,3u>;
std::size_t checks{},refusals{},fallbacks{},collidableFallbacks{},queries{};

void Require(bool value,const std::string& message) {
    ++checks;if(!value)throw std::runtime_error(message);
}
template<class F>void Refused(F&& operation,const std::string& message) {
    bool refused{};try {operation();}catch(const std::exception&) {refused=true;}
    Require(refused,message);++refusals;
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
std::uint64_t Hash(const fs::path& path) {
    std::ifstream file(path,std::ios::binary);Require(bool(file),"Cannot fingerprint owned source: "+path.string());
    std::array<char,16384u> buffer{};std::uint64_t hash=14695981039346656037ull;std::uintmax_t total{};
    while(file) {
        file.read(buffer.data(),static_cast<std::streamsize>(buffer.size()));const auto count=file.gcount();
        total+=static_cast<std::uintmax_t>(count);
        Require(total<=512u*1024u*1024u,"Owned collision-engine fingerprint exceeds bound");
        for(std::streamsize i=0;i<count;++i) {hash^=static_cast<unsigned char>(buffer[static_cast<std::size_t>(i)]);hash*=1099511628211ull;}
    }
    Require(file.eof(),"Cannot finish owned fingerprint");return hash;
}
struct Source {
    fs::path path;std::uintmax_t size{};fs::file_time_type modified;std::uint64_t hash{};
    explicit Source(const fs::path& source):path(fs::canonical(source)),size(fs::file_size(path)),
        modified(fs::last_write_time(path)),hash(Hash(path)) {}
    void Unchanged() const {
        Require(fs::file_size(path)==size&&fs::last_write_time(path)==modified&&Hash(path)==hash,
            "Owned source bytes, size or timestamp changed: "+path.string());
    }
};
struct Temporary {
    fs::path parent,directory;std::vector<fs::path> files;
    explicit Temporary(const fs::path& original) {
        parent=fs::canonical(fs::temp_directory_path());
        Require(parent!=parent.root_path()&&!Within(parent,fs::canonical(original)),
            "Temporary collision-engine fixture overlaps owned installation/root");
        const auto stamp=std::chrono::steady_clock::now().time_since_epoch().count();
        for(unsigned i=0u;i<20u;++i) {
            const auto candidate=parent/("quest-engine-collision-"+std::to_string(stamp)+'-'+std::to_string(i));
            if(fs::create_directory(candidate)) {directory=fs::canonical(candidate);break;}
        }
        Require(!directory.empty()&&directory.parent_path()==parent,"Cannot create isolated collision-engine directory");
    }
    ~Temporary() {
        ShutdownPortableRuntime();std::error_code ignored;
        for(const auto& path:files)if(path.parent_path()==directory)fs::remove(path,ignored);
        if(!directory.empty()&&directory.parent_path()==parent)fs::remove(directory,ignored);
    }
    fs::path Path(const std::string& leaf) {
        Require(!leaf.empty()&&leaf.find_first_of("/\\:")==std::string::npos,"Invalid isolated checkpoint leaf");
        const auto path=directory/(leaf+".sav");
        if(std::find(files.begin(),files.end(),path)==files.end())files.push_back(path);
        return path;
    }
    Bytes Snapshot(const std::string& leaf) {
        const auto path=Path(leaf);Require(SavePortableRuntimeState(path.string()),"Cannot save complete collision-engine checkpoint");
        Bytes bytes;Require(QuestVr::ReadBoundedSaveFile(path.string(),QuestVr::kMaximumSaveRuntimeBytes,bytes),
            "Cannot read complete collision-engine checkpoint");return bytes;
    }
};
void SameGc(const GCStats& expected,const std::string& message) {
    const auto actual=GC::GetStats();Require(actual.numObjects==expected.numObjects&&actual.memoryUsage==expected.memoryUsage,message);
}
QuestVr::Vm::Value Declared(const std::string& path,const char* field,Kind kind) {
    const auto value=ReadPortableActorDeclaredScriptProperty(path,field);
    Require(value.kind==kind,"Owned declared field has wrong type: "+path+' '+field);return value;
}
std::vector<std::string> WorldPaths() {
    std::vector<std::string> result;
    for(const auto& actor:GetPortableRuntimeMapActors(true))result.push_back(actor.objectPath+'|'+actor.classPath);
    return result;
}
void SameFloat(float actual,float expected,const std::string& message) {
    Require(std::bit_cast<std::uint32_t>(actual)==std::bit_cast<std::uint32_t>(expected),message);
}
void SameVector(const Array& actual,const Array& expected,const std::string& message) {
    for(std::size_t i=0u;i<3u;++i)SameFloat(actual[i],expected[i],message+" component "+std::to_string(i));
}
PortableModelVec3 Vector(const Array& value) {return {value[0u],value[1u],value[2u]};}
Array ArrayOf(const PortableModelVec3& value) {return {value.x,value.y,value.z};}

const PortableNativeDefaultSnapshot& Default(const PortableEngineCollisionSnapshot& snapshot,const std::string_view path) {
    const auto found=std::find_if(snapshot.defaults.begin(),snapshot.defaults.end(),[&](const auto& row) {return row.classPath==path;});
    Require(found!=snapshot.defaults.end(),"Prepared native default missing: "+std::string(path));return *found;
}
const PortableNativeEngineFieldSnapshot& Field(const PortableEngineCollisionSnapshot& snapshot,const std::string_view path) {
    const auto found=std::find_if(snapshot.fields.begin(),snapshot.fields.end(),[&](const auto& row) {return row.objectPath==path;});
    Require(found!=snapshot.fields.end(),"Real native field missing: "+std::string(path));return *found;
}
void SameDefaults(const PortableNativeDefaultSnapshot& a,const PortableNativeDefaultSnapshot& b) {
    Require(a.classPath==b.classPath&&a.classIndex==b.classIndex&&a.defaultIndex==b.defaultIndex&&
        a.superClassIndex==b.superClassIndex&&a.nativeSize==b.nativeSize&&a.classFlags==b.classFlags&&
        a.configuredClasses==b.configuredClasses&&a.cacheSizeMegs==b.cacheSizeMegs&&a.useSound==b.useSound&&
        a.urlValid==b.urlValid&&a.serverActors==b.serverActors&&a.serverPackages==b.serverPackages,
        "Prepared default identity/payload/config changed: "+a.classPath);
    SameFloat(a.currentTickRate,b.currentTickRate,"Prepared native default CurrentTickRate changed");
}
void SameFields(const PortableNativeEngineFieldSnapshot& a,const PortableNativeEngineFieldSnapshot& b) {
    Require(a.objectPath==b.objectPath&&a.classPath==b.classPath&&a.outerPath==b.outerPath&&a.nextPath==b.nextPath&&
        a.innerPath==b.innerPath&&a.propertyNextPath==b.propertyNextPath&&a.configNextPath==b.configNextPath&&
        a.constructorNextPath==b.constructorNextPath&&a.category==b.category&&a.objectIndex==b.objectIndex&&
        a.classIndex==b.classIndex&&a.outerIndex==b.outerIndex&&a.objectFlags==b.objectFlags&&
        a.arrayDimension==b.arrayDimension&&a.elementSize==b.elementSize&&a.propertyFlags==b.propertyFlags&&
        a.dataOffset==b.dataOffset,"Actual native field header/link/layout changed: "+a.objectPath);
}
void SameSession(const PortableEngineCollisionSnapshot& a,const PortableEngineCollisionSnapshot& b,bool bound=true) {
    Require(a.prepared&&b.prepared&&a.enginePath==b.enginePath&&a.engineClassPath==b.engineClassPath&&
        a.transientPath==b.transientPath&&a.cylinderPath==b.cylinderPath&&a.engineIndex==b.engineIndex&&
        a.engineClassIndex==b.engineClassIndex&&a.transientIndex==b.transientIndex&&a.cylinderIndex==b.cylinderIndex&&
        a.cylinderClassIndex==b.cylinderClassIndex&&a.engineFlags==b.engineFlags&&a.transientFlags==b.transientFlags&&
        a.cylinderFlags==b.cylinderFlags&&a.engineExecInstalled==b.engineExecInstalled&&a.instanceUrlValid==b.instanceUrlValid&&
        a.cylinderBoundsValid==b.cylinderBoundsValid&&a.engineUniqueCounter==b.engineUniqueCounter&&
        a.primitiveUniqueCounter==b.primitiveUniqueCounter&&a.instanceCacheSizeMegs==b.instanceCacheSizeMegs&&
        a.instanceUseSound==b.instanceUseSound&&a.instanceConfiguredClasses==b.instanceConfiguredClasses&&
        a.instanceServerActors==b.instanceServerActors&&a.instanceServerPackages==b.instanceServerPackages&&
        a.defaults.size()==b.defaults.size()&&a.fields.size()==b.fields.size(),"Engine collision session was replaced or mutated");
    if(bound)Require(a.boundLevelPath==b.boundLevelPath&&a.levelIndex==b.levelIndex,"Engine current-Level binding changed");
    for(const auto& row:a.defaults)SameDefaults(row,Default(b,row.classPath));
    for(const auto& row:a.fields)SameFields(row,Field(b,row.objectPath));
}
void Lookup(const std::string& path,std::uint32_t index,std::uint32_t cls,std::uint32_t outer) {
    const auto owned=ReadPortableRuntimeObjectOwnership(path);
    Require(owned.objectPath==path&&owned.objectIndex==index&&owned.classIndex==cls&&owned.outerIndex==outer,
        "Actual native allocation disagrees with its ownership snapshot: "+path);
    const auto found=FindPortableRuntimeObject({path,cls,0u,true,true});
    Require(found&&found->objectPath==path&&found->objectIndex==index&&found->classIndex==cls&&found->outerIndex==outer&&
        found->generation==ReadPortableRuntimeObjectLookup().generation,"Actual native allocation missing from object lookup: "+path);
}
struct SourceFields {
    std::array<std::string,3u> heads;
    std::vector<std::pair<std::string,std::pair<std::uint32_t,std::uint32_t>>> identities;
};
SourceFields OriginalFieldLists(const std::vector<PortablePackageTables>& packages) {
    SourceFields result;
    // Actual original Class.Children/Next order, not a sorted path inventory.
    // These are the audited three loaded superclass segments of GameEngine.
    for(const auto* path:{"Engine.Engine","Core.Subsystem","Core.Object"}) {
        const std::string clsPath=path;const auto dot=clsPath.find('.');
        const auto found=std::find_if(packages.begin(),packages.end(),[&](const auto& table) {
            return fs::path(table.sourcePath).stem().string()==clsPath.substr(0u,dot);
        });
        Require(found!=packages.end(),"Original native field-list source was not explicitly loaded");
        const auto index=FindPortableExport(*found,clsPath.substr(dot+1u));
        const auto cls=LoadPortableClassDescriptor(*found,index);
        auto child=cls.state.children;std::set<std::int32_t> visited;
        while(child) {
            Require(child>0&&static_cast<std::size_t>(child)<=found->exports.size()&&visited.size()<65536u&&
                visited.insert(child).second,"Original native field chain is invalid/cyclic/over-budget");
            const auto slot=static_cast<std::size_t>(child-1);const auto& entry=found->exports[slot];
            Require(entry.ObjOuter==static_cast<std::int32_t>(index+1u),"Original native field lost its actual declaring Outer");
            const auto type=GetPortableObjectPath(*found,entry.ObjClass);
            const auto fieldPath=fs::path(found->sourcePath).stem().string()+'.'+GetPortableObjectPath(*found,child);
            const auto actual=ReadPortableRuntimeObjectOwnership(fieldPath);
            result.identities.push_back({fieldPath,{actual.objectIndex,actual.outerIndex}});
            if(type.size()>=8u&&type.substr(type.size()-8u)=="Property") {
                const auto property=LoadPortablePropertyDescriptor(*found,slot);
                if(result.heads[0u].empty())result.heads[0u]=fieldPath;
                if(result.heads[1u].empty()&&(property.flags&0x4000u))result.heads[1u]=fieldPath;
                if(result.heads[2u].empty()&&(property.flags&0x400000u))result.heads[2u]=fieldPath;
                child=property.nextField;
            }else if(type=="Core.Function")child=LoadPortableFunctionScript(*found,slot).nextField;
            else if(type=="Core.State")child=LoadPortableStateDescriptor(*found,slot).nextField;
            else child=LoadPortableFieldLinks(*found,slot).nextField;
        }
    }
    Require(!result.heads[0u].empty()&&!result.heads[1u].empty()&&!result.identities.empty(),
        "Original inherited native property/config lists are unexpectedly empty");return result;
}
void DefaultAndFieldAuthority(const PortableEngineCollisionSnapshot& snapshot,
    const std::array<std::uint32_t,3u>& oldDefaults,const SourceFields& original) {
    const std::array<std::pair<const char*,std::uint32_t>,11u> expected{{
        {"Core.Object",0x28u},{"Core.Subsystem",0x2cu},{"Core.Field",0x34u},{"Core.Property",0x5cu},
        {"Core.StrProperty",0x5cu},{"Core.ArrayProperty",0x60u},{"Engine.Primitive",0x54u},
        {"Engine.Engine",0x68u},{"Engine.GameEngine",0xd0u},{"Extension.GameEngineExt",0xd0u},
        {"DeusEx.DeusExGameEngine",0xd0u}}};
    Require(snapshot.defaults.size()==expected.size(),"Prepared native default set is not the closed eleven-class chain");
    std::set<std::uint32_t> classes,defaults;
    for(const auto& [path,size]:expected) {
        const auto& row=Default(snapshot,path);const auto cls=ReadPortableRuntimeObjectOwnership(path);
        const auto cdo=ReadPortableRuntimeObjectOwnership("@ClassDefault:"+std::string(path));
        Require(row.classIndex==cls.objectIndex&&row.classIndex&&row.defaultIndex&&row.defaultIndex!=row.classIndex&&
            classes.insert(row.classIndex).second&&defaults.insert(row.defaultIndex).second&&row.nativeSize==size&&
            cdo.objectIndex==row.defaultIndex&&cdo.classIndex==row.classIndex&&!cdo.outerIndex&&!row.urlValid,
            "Prepared native default lacks distinct actual Class/CDO/size/zero-URL authority: "+std::string(path));
        SameFloat(row.currentTickRate,0.0f,"Native default CurrentTickRate is not original Float0");
        if(std::string(path)=="Core.Object")Require(!row.superClassIndex,"Object native default invented a superclass");
        else Require(row.superClassIndex!=0u,"Native default lost actual superclass: "+std::string(path));
        // Class-owned blocks have object identity, but are not named/hash candidates.
        Require(!FindPortableRuntimeObject({"@ClassDefault:"+std::string(path),0u,0u,true,false}),
            "Class-owned default leaked into named object lookup");
    }
    for(const auto& [path,super]:std::array<std::pair<const char*,const char*>,10u>{{
        {"Core.Subsystem","Core.Object"},{"Core.Field","Core.Object"},{"Core.Property","Core.Field"},
        {"Core.StrProperty","Core.Property"},{"Core.ArrayProperty","Core.Property"},{"Engine.Primitive","Core.Object"},
        {"Engine.Engine","Core.Subsystem"},{"Engine.GameEngine","Engine.Engine"},
        {"Extension.GameEngineExt","Engine.GameEngine"},{"DeusEx.DeusExGameEngine","Extension.GameEngineExt"}}})
        Require(Default(snapshot,path).superClassIndex==Default(snapshot,super).classIndex,
            "Prepared native default lost its actual direct superclass identity");
    for(const auto& [path,flags]:std::array<std::pair<const char*,std::uint32_t>,11u>{{
        {"Core.Object",0x112u},{"Core.Subsystem",0x112u},{"Core.Field",0x13u},{"Core.Property",0x13u},
        {"Core.StrProperty",0x12u},{"Core.ArrayProperty",0x12u},{"Engine.Primitive",0x12u},
        {"Engine.Engine",0x11eu},{"Engine.GameEngine",0x1eu},{"Extension.GameEngineExt",0x1eu},
        {"DeusEx.DeusExGameEngine",0x1eu}}})
        Require(Default(snapshot,path).classFlags==flags,
            "Native defaults replaced actual loaded flags with native-constructor arguments");
    Require(Default(snapshot,"Core.Object").defaultIndex==oldDefaults[0u]&&
        Default(snapshot,"Core.Subsystem").defaultIndex==oldDefaults[1u]&&
        Default(snapshot,"Engine.Engine").defaultIndex==oldDefaults[2u],"Existing source CDO identity was replaced");
    const auto& base=Default(snapshot,"Engine.Engine");
    Require(base.cacheSizeMegs==2&&base.useSound,"Engine native defaults lost original authored Cache2/UseSoundTrue");
    const auto& concrete=Default(snapshot,"DeusEx.DeusExGameEngine");
    const std::array<std::string,5u> configured{{"","","Engine.Console","",""}};
    const std::vector<std::string> serverActors{
        "IpDrv.UdpBeacon","IpServer.UdpServerQuery",
        "IpServer.UdpServerUplink MasterServerAddress=master0.gamespy.com MasterServerPort=27900",
        "IpServer.UdpServerUplink MasterServerAddress=master.epicgames.com MasterServerPort=27900",
        "IpServer.UdpServerUplink MasterServerAddress=master.fragaholic.com MasterServerPort=27900"};
    Require(concrete.cacheSizeMegs==4&&concrete.useSound&&concrete.configuredClasses==configured&&
        concrete.serverActors==serverActors&&concrete.serverPackages.empty(),"Concrete native defaults do not match authentic ordered INI actions");
    for(const auto* path:{"Engine.Engine","Engine.GameEngine","Extension.GameEngineExt"}) {
        const auto& row=Default(snapshot,path);
        Require(row.cacheSizeMegs==2&&row.useSound&&row.configuredClasses==configured&&
            row.serverActors.empty()&&row.serverPackages.empty(),"Descendant config leaked backwards into copied superclass defaults");
    }
    Require(snapshot.fields.size()==4u,"GameEngine arrays/inners are not four distinct actual fields");
    std::set<std::uint32_t> fieldIds;
    const auto& actors=Field(snapshot,"Engine.GameEngine.ServerActors");
    const auto& packages=Field(snapshot,"Engine.GameEngine.ServerPackages");
    const auto game=ReadPortableRuntimeObjectOwnership("Engine.GameEngine");
    const auto array=ReadPortableRuntimeObjectOwnership("Core.ArrayProperty");
    const auto str=ReadPortableRuntimeObjectOwnership("Core.StrProperty");
    for(const auto* row:{&actors,&packages}) {
        Require(row->classPath=="Core.ArrayProperty"&&row->outerPath=="Engine.GameEngine"&&row->classIndex==array.objectIndex&&
            row->outerIndex==game.objectIndex&&row->objectFlags==4u&&row->arrayDimension==1u&&row->elementSize==12u&&
            row->propertyFlags==0x404000u&&row->category=="Settings"&&row->innerPath==row->objectPath+".StrProperty0",
            "Native array lacks actual final field/class/Outer/layout authority");
        Require(row->dataOffset==(row==&actors?0xb8u:0xc4u),"Native Link0 destroyed the declared Cpp array offset");
        const auto& inner=Field(snapshot,row->innerPath);
        Require(inner.classPath=="Core.StrProperty"&&inner.outerPath==row->objectPath&&inner.classIndex==str.objectIndex&&
            inner.outerIndex==row->objectIndex&&inner.objectFlags==4u&&inner.arrayDimension==1u&&inner.elementSize==12u&&
            inner.propertyFlags==0x400000u&&inner.dataOffset==0u&&inner.category=="None"&&inner.nextPath.empty()&&
            inner.innerPath.empty()&&inner.propertyNextPath.empty()&&inner.configNextPath.empty()&&inner.constructorNextPath.empty(),
            "Native string inner was shared, appended to Class.Children, or has wrong final linking state");
    }
    Require(packages.nextPath==actors.objectPath&&actors.nextPath.empty()&&packages.propertyNextPath==actors.objectPath&&
        packages.configNextPath==actors.objectPath&&packages.constructorNextPath==actors.objectPath,
        "GameEngine native Children/property/config/constructor prefix order differs");
    Require(actors.propertyNextPath==original.heads[0u]&&actors.configNextPath==original.heads[1u]&&
        actors.constructorNextPath==original.heads[2u],"Native GameEngine array lists did not retain exact inherited source-list tails");
    for(const auto& [path,ids]:original.identities) {
        const auto actual=ReadPortableRuntimeObjectOwnership(path);
        Require(actual.objectIndex==ids.first&&actual.outerIndex==ids.second,
            "Equivalent-final native preparation replaced/reparented an original source field: "+path);
    }
    Require(Field(snapshot,actors.innerPath).objectIndex!=Field(snapshot,packages.innerPath).objectIndex,
        "Distinct array Outers share one StrProperty0 inner");
    for(const auto& row:snapshot.fields) {
        Require(row.objectIndex&&fieldIds.insert(row.objectIndex).second,"Native fields have duplicate/null allocation identity");
        Lookup(row.objectPath,row.objectIndex,row.classIndex,row.outerIndex);
    }
    Require(snapshot.engineClassPath=="DeusEx.DeusExGameEngine"&&snapshot.engineClassIndex==concrete.classIndex&&
        snapshot.engineFlags==0u&&snapshot.transientFlags==0u&&snapshot.cylinderFlags==0u&&snapshot.engineExecInstalled&&
        snapshot.instanceUrlValid&&!snapshot.cylinderBoundsValid&&snapshot.engineUniqueCounter>0&&snapshot.primitiveUniqueCounter>0&&
        snapshot.instanceCacheSizeMegs==concrete.cacheSizeMegs&&snapshot.instanceUseSound==concrete.useSound&&
        snapshot.instanceConfiguredClasses==concrete.configuredClasses&&snapshot.instanceServerActors==concrete.serverActors&&
        snapshot.instanceServerPackages==concrete.serverPackages,"Actual Engine instance did not copy defaults before its fresh constructor");
    Require(snapshot.transientPath=="Transient"&&snapshot.cylinderClassIndex==Default(snapshot,"Engine.Primitive").classIndex&&
        snapshot.engineIndex&&snapshot.transientIndex&&snapshot.cylinderIndex&&snapshot.engineIndex!=snapshot.cylinderIndex&&
        snapshot.engineIndex!=snapshot.transientIndex&&snapshot.transientIndex!=snapshot.cylinderIndex,
        "Actual Engine/Transient/Cylinder identities are absent or aliased");
    Lookup(snapshot.enginePath,snapshot.engineIndex,snapshot.engineClassIndex,snapshot.transientIndex);
    Lookup(snapshot.cylinderPath,snapshot.cylinderIndex,snapshot.cylinderClassIndex,snapshot.transientIndex);
    const auto package=ReadPortableRuntimeObjectOwnership("Core.Package");
    Lookup(snapshot.transientPath,snapshot.transientIndex,package.objectIndex,0u);
}
bool SamePrimitive(const PortableActorPrimitiveSnapshot& a,const PortableActorPrimitiveSnapshot& b) {
    return a.actorPath==b.actorPath&&a.primitivePath==b.primitivePath&&a.nativeClassPath==b.nativeClassPath&&
        a.lineImplementation==b.lineImplementation&&a.boundsImplementation==b.boundsImplementation;
}
void CylinderLine(const PortableActorPrimitiveSnapshot& selected,const QuestVr::OriginalActorCylinder& owner,
    const Array& end,const Array& start,const Array& extent,std::uint32_t flags=0u) {
    const auto actual=ProbePortableRuntimeActorPrimitiveLine(selected.actorPath,end,start,extent,flags);
    QuestVr::OriginalActorLineHit expected{};
    const bool clear=QuestVr::CheckOriginalActorCylinder(expected,owner,Vector(end),Vector(start),Vector(extent));
    Require(SamePrimitive(actual.primitive,selected)&&actual.clear==clear&&actual.hitOwner==(expected.actor==owner.identity)&&
        actual.hitPrimitiveIsNone==(expected.primitive==0u)&&actual.item==expected.item,"Raw Cylinder result/owner/primitive packet differs");
    SameFloat(actual.time,expected.time,"Raw Cylinder Time differs");
    SameVector(actual.location,ArrayOf(expected.location),"Raw Cylinder Location differs");
    SameVector(actual.normal,ArrayOf(expected.normal),"Raw Cylinder seeded/partial Normal differs");++queries;
}
void CylinderActors(const PortableEngineCollisionSnapshot& snapshot) {
    std::string sample;
    for(const auto& actor:GetPortableRuntimeMapActors(true)) {
        if(!Declared(actor.objectPath,"Engine.Actor.Brush",Kind::Object).text.empty()||
            !Declared(actor.objectPath,"Engine.Actor.Mesh",Kind::Object).text.empty())continue;
        const auto selected=ReadPortableRuntimeActorPrimitive(actor.objectPath);
        Require(selected.actorPath==actor.objectPath&&selected.primitivePath==snapshot.cylinderPath&&selected.nativeClassPath=="Engine.Primitive"&&
            selected.lineImplementation=="UPrimitive::LineCheck"&&selected.boundsImplementation=="UPrimitive::GetCollisionBoundingBox",
            "Null Brush/Mesh did not select the actual shared Engine-owned Primitive");
        ++fallbacks;
        const bool collides=Declared(actor.objectPath,"Engine.Actor.bCollideActors",Kind::Bool).boolean;
        if(collides)++collidableFallbacks;
        const auto location=Declared(actor.objectPath,"Engine.Actor.Location",Kind::Vector).vector;
        const float radius=Declared(actor.objectPath,"Engine.Actor.CollisionRadius",Kind::Float).floating;
        const float height=Declared(actor.objectPath,"Engine.Actor.CollisionHeight",Kind::Float).floating;
        const auto actual=ReadPortableRuntimeActorPrimitiveBounds(actor.objectPath);
        const auto expected=QuestVr::OriginalPrimitiveCollisionBounds(Vector(location),radius,height);
        Require(SamePrimitive(actual.primitive,selected),"Cylinder bounds changed selected object identity");
        SameVector(actual.minimum,ArrayOf(expected.minimum),"Cylinder bounds minimum differs");
        SameVector(actual.maximum,ArrayOf(expected.maximum),"Cylinder bounds maximum differs");
        if(sample.empty()&&collides&&radius>0.0f&&height>0.0f)sample=actor.objectPath;
    }
    Require(fallbacks==227u&&collidableFallbacks==31u&&!sample.empty(),"Untouched Training fallback census changed");
    const auto selected=ReadPortableRuntimeActorPrimitive(sample);
    const auto location=Declared(sample,"Engine.Actor.Location",Kind::Vector).vector;
    const float radius=Declared(sample,"Engine.Actor.CollisionRadius",Kind::Float).floating;
    const float height=Declared(sample,"Engine.Actor.CollisionHeight",Kind::Float).floating;
    const QuestVr::OriginalActorCylinder owner{1u,Vector(location),radius,height};
    const float reach=radius+128.0f;
    const Array start{location[0u]+reach,location[1u],location[2u]};
    const Array end{location[0u]-reach,location[1u],location[2u]};
    const Array away{start[0u]+128.0f,start[1u],start[2u]};
    CylinderLine(selected,owner,end,start,{});
    CylinderLine(selected,owner,end,start,{-0.0f,0.0f,-0.0f},0xffffffffu);
    CylinderLine(selected,owner,end,start,{2.0f,3.0f,4.0f},0x20u);
    CylinderLine(selected,owner,away,start,{});
    const float nan=std::bit_cast<float>(std::uint32_t{0x7fc12345u});
    Refused([&] {(void)ProbePortableRuntimeActorPrimitiveLine(sample,end,start,{nan,nan,nan});},
        "Raw Primitive incorrectly used Mesh's all-NaN extent-to-zero wrapper");
}
void PrepareDependencies() {
    Require(PreparePortableRuntimeNativeEngineClasses().prepared,"Cannot prepare native Engine classes");
    Require(PreparePortableRuntimeNativeMetadataClasses().prepared,"Cannot prepare native Core metadata classes");
    Require(PreparePortableRuntimeNativeConfigClasses().prepared,"Cannot prepare genuine config constraints");
    Require(PreparePortableRuntimeNativePackages().prepared,"Cannot prepare actual Packages");
    Require(PreparePortableRuntimeObjectLookup().prepared,"Cannot prepare actual object lookup");
}
void LateTransientRollback(Temporary& temporary,const std::string& ini,const std::array<std::uint32_t,3u>& oldDefaults) {
    PortablePackageTables occupied;
    occupied.sourcePath=(temporary.directory/"Transient.dx").string();occupied.version=68u;
    occupied.names.push_back({NameString("None"),0u});
    const auto loaded=LoadPortableRuntimeMap(occupied);
    Require(!loaded.passed&&!loaded.exports&&!loaded.actors,
        "Empty synthetic linker fixture claimed a playable map or created actors");
    const auto transient=ReadPortableRuntimeObjectOwnership("Transient");
    Require(transient.objectIndex&&transient.classPath=="Core.Package"&&!transient.outerIndex,
        "Synthetic source did not occupy an actual top-level Transient Package");
    const auto before=temporary.Snapshot("OccupiedTransientBefore");const auto gc=GC::GetStats();
    const auto revision=GetPortableRuntimeWorldRevision();const auto lookup=ReadPortableRuntimeObjectLookup();
    const auto packages=ReadPortableRuntimeNativePackages();
    const auto cache=ReadPortableClassDefault("Engine.Engine","CacheSizeMegs");
    for(unsigned repeat=0u;repeat<2u;++repeat) {
        bool refused{};
        try {static_cast<void>(PreparePortableRuntimeEngineCollision(ini));}
        catch(const std::exception& error) {
            refused=true;
            Require(std::string(error.what())=="Native collision Engine Transient package name is already occupied",
                "Late collision test stopped before its actual Transient allocation boundary: "+std::string(error.what()));
        }
        Require(refused,"Occupied actual Transient Package did not refuse late native collision publication");++refusals;
        GC::Collect();SameGc(gc,"Late Transient refusal leaked staged native fields/defaults/classes");
        Require(!ReadPortableRuntimeEngineCollision().prepared&&GetPortableRuntimeWorldRevision()==revision&&
            GetPortableRuntimeMapActors(true).empty()&&temporary.Snapshot("OccupiedTransientAfter")==before&&
            QuestVr::Vm::Equal(cache,ReadPortableClassDefault("Engine.Engine","CacheSizeMegs")),
            "Late Transient refusal changed actual world/save/default state");
        const auto after=ReadPortableRuntimeObjectLookup();
        Require(after.objects==lookup.objects&&after.retainedBytes==lookup.retainedBytes&&after.generation==lookup.generation,
            "Late Transient refusal changed actual lookup membership, accounting or generation");
        const auto packageAfter=ReadPortableRuntimeNativePackages();
        Require(packageAfter.packages.size()==packages.packages.size()&&packageAfter.defaultObjectIndex==packages.defaultObjectIndex,
            "Late Transient refusal changed actual Package registry/default authority");
        for(std::size_t i=0u;i<packages.packages.size();++i) {
            const auto& a=packages.packages[i];const auto& b=packageAfter.packages[i];
            Require(a.objectPath==b.objectPath&&a.objectIndex==b.objectIndex&&a.classIndex==b.classIndex&&
                a.objectFlags==b.objectFlags&&a.bindingAttempted==b.bindingAttempted,
                "Late Transient refusal replaced an existing actual Package");
        }
        for(const auto* path:{"Core.FloatProperty","Core.NameProperty","Engine.GameEngine.ServerActors",
            "Engine.GameEngine.ServerActors.StrProperty0","Engine.GameEngine.ServerPackages",
            "Engine.GameEngine.ServerPackages.StrProperty0","Transient.DeusExGameEngine0","Transient.Primitive0"}) {
            Refused([&] {(void)ReadPortableRuntimeObjectOwnership(path);},"Late refused allocation retained canonical alias: "+std::string(path));
            Require(!FindPortableRuntimeObject({path,0u,0u,true,false}),"Late refused allocation remained in actual object lookup");
        }
        const std::array<std::uint32_t,3u> defaults{{
            ReadPortableRuntimeObjectOwnership("@ClassDefault:Core.Object").objectIndex,
            ReadPortableRuntimeObjectOwnership("@ClassDefault:Core.Subsystem").objectIndex,
            ReadPortableRuntimeObjectOwnership("@ClassDefault:Engine.Engine").objectIndex}};
        Require(defaults==oldDefaults,"Late native allocation refusal replaced existing original class-owned defaults");
        Refused([&] {(void)ReadPortableRuntimeObjectOwnership("@ClassDefault:Engine.GameEngine");},
            "Late refusal attached a staged native GameEngine CDO to its actual UClass");
    }
    Require(UnloadPortableRuntimeMap()==0u,"Empty synthetic Transient source unload invented retired exports");GC::Collect();
    Refused([&] {(void)ReadPortableRuntimeObjectOwnership("Transient");},
        "Empty synthetic Transient source retained a revoked linker root");
}
std::uint32_t Word(const Bytes& bytes,std::size_t offset) {
    Require(offset<=bytes.size()&&bytes.size()-offset>=4u,"Immutable-default fixture word is truncated");
    std::uint32_t value{};for(unsigned i=0u;i<4u;++i)value|=std::uint32_t(bytes[offset+i])<<(i*8u);return value;
}
void PutWord(Bytes& bytes,std::size_t offset,std::uint32_t value) {
    Require(offset<=bytes.size()&&bytes.size()-offset>=4u,"Immutable-default fixture word escaped payload");
    for(unsigned i=0u;i<4u;++i)bytes[offset+i]=static_cast<std::uint8_t>(value>>(i*8u));
}
Bytes ReplaceDormantScriptBlob(const Bytes& checkpoint,const Bytes& blob) {
    // Fresh untouched Training in this fixture has precisely the unchanged v3
    // gameplay prefix. Walk it independently before upgrading to the exact v6
    // class-default envelope; never search for arbitrary codec magic bytes.
    Require(Word(checkpoint,0u)==0x53515844u&&Word(checkpoint,4u)==3u,
        "Immutable-default injection requires this fixture's fresh dormant v3 checkpoint");
    std::size_t cursor=8u;
    const auto skip=[&](std::size_t count) {
        Require(cursor<=checkpoint.size()&&count<=checkpoint.size()-cursor,"Immutable-default fixture prefix is truncated");cursor+=count;
    };
    const auto word=[&] {const auto value=Word(checkpoint,cursor);cursor+=4u;return value;};
    const auto string=[&] {const auto length=word();Require(length<=1'048'576u,"Immutable-default fixture string exceeds bound");skip(length);};
    const auto list=[&] {const auto count=word();Require(count<=100'000u,"Immutable-default fixture list exceeds bound");for(std::uint32_t i=0u;i<count;++i)string();};
    list();list();list();skip(4u);
    const auto damaged=word();Require(damaged<=100'000u,"Immutable-default damaged list exceeds bound");
    for(std::uint32_t i=0u;i<damaged;++i) {string();skip(4u);}
    skip(8u);list();list();list();list();Require(cursor==checkpoint.size(),"Fresh v3 fixture has an unexpected trailing envelope");
    Require(!blob.empty()&&blob.size()<=std::numeric_limits<std::uint32_t>::max(),"Injected default codec is empty/oversized");
    Bytes result=checkpoint;PutWord(result,4u,6u);
    const auto length=static_cast<std::uint32_t>(blob.size());
    for(unsigned i=0u;i<4u;++i)result.push_back(static_cast<std::uint8_t>(length>>(i*8u)));
    result.insert(result.end(),blob.begin(),blob.end());return result;
}
fs::path ImmutableDefaultFixture(Temporary& temporary,const Bytes& before) {
    QuestVr::ScriptSavedState state;state.mapName="00_Training";
    state.classDefaults.push_back({"Engine.Engine",{{"Engine.Engine.CacheSizeMegs","CacheSizeMegs",0u,QuestVr::Vm::Value::Integer(999)}}});
    const auto blob=QuestVr::EncodeScriptSavedState(state);
    Require(blob.size()>6u&&blob[6u]==QuestVr::ScriptStateDetail::ClassDefaultsVersion,
        "Injected immutable-default record did not produce actual class-default codec version");
    const auto path=temporary.Path("ImmutableEngineDefaultInjection");
    Require(QuestVr::WriteDurableSaveFile(path.string(),ReplaceDormantScriptBlob(before,blob)),
        "Cannot write isolated immutable-default injection fixture");return path;
}
void ActualInstanceCasts(const PortableEngineCollisionSnapshot& snapshot,const PortablePackageTables& engine) {
    const auto console=LoadPortableClassDescriptor(engine,FindPortableExport(engine,"Console"));
    Require(console.classWithinReference.has_value()&&*console.classWithinReference!=0,
        "Actual serialized Console lacks its original Core.Object Within reference");
    const auto objectReference=*console.classWithinReference;
    const auto objectPath=GetPortableObjectPath(engine,objectReference);
    Require((objectReference>0?"Engine."+objectPath:objectPath)=="Core.Object",
        "Actual Console Within reference does not target original Core.Object");
    const auto primitive=LoadPortablePropertyDescriptor(engine,FindPortableExport(engine,"Engine.Cylinder"));
    Require(primitive.type=="ObjectProperty"&&GetPortableObjectPath(engine,primitive.referencedType)=="Engine.Primitive",
        "Actual Engine.Cylinder property lacks its source Primitive class reference");
    const auto cast=[&](const std::string& path,std::int32_t reference,const std::string& expected) {
        const auto operand=QuestVr::Vm::Value::Text(Kind::Object,path);
        const auto result=CastPortableRuntimeObject("Engine.Console",reference,operand,false);
        Require(result.kind==Kind::Object&&result.text==expected&&result.fields.empty()&&operand.text==path,
            "Constructed native collision instance cast lost its actual Class identity: "+path);
    };
    for(const auto* path:{&snapshot.enginePath,&snapshot.cylinderPath,&snapshot.transientPath})cast(*path,objectReference,*path);
    for(const auto& field:snapshot.fields)cast(field.objectPath,objectReference,field.objectPath);
    cast(snapshot.cylinderPath,primitive.referencedType,snapshot.cylinderPath);
    cast(snapshot.enginePath,primitive.referencedType,{});
    for(const auto& field:snapshot.fields)cast(field.objectPath,primitive.referencedType,{});
}
void Original(const fs::path& root,bool questStartup) {
    Temporary temporary(root);std::vector<Source> sources;std::vector<PortablePackageTables> packages;
    std::vector<const char*> packageNames{
        "Core","Engine","DeusEx","Extension","ConSys","IpDrv","IpServer","UBrowser","UWindow",
        "Editor","Fire","MPCharacters","DeusExSounds","DeusExDeco","DeusExItems","DeusExCharacters"};
    if(questStartup)packageNames.assign(QuestVr::RuntimeScriptPackages.begin(),QuestVr::RuntimeScriptPackages.end());
    sources.reserve(packageNames.size()+2u);packages.reserve(packageNames.size());
    for(const char* name:packageNames) {
        sources.emplace_back(root/"System"/(std::string(name)+".u"));
        packages.push_back(LoadPortablePackageTables(sources.back().path.string()));
    }
    sources.emplace_back(root/"System"/"DeusEx.ini");const auto ini=sources.back().path.string();
    sources.emplace_back(root/"Maps"/"00_Training.dx");const auto map=LoadPortablePackageTables(sources.back().path.string());
    Require(!ReadPortableRuntimeEngineCollision().prepared,"Uninitialized runtime exposes an Engine collision session");
    Refused([&] {(void)PreparePortableRuntimeEngineCollision(ini);},"Engine collision phase accepted an uninitialized runtime");
    Require(InitializePortableRuntime(packages).passed,"Cannot initialize authentic shared Training scripts");
    const std::array<std::uint32_t,3u> oldDefaults{{
        ReadPortableRuntimeObjectOwnership("@ClassDefault:Core.Object").objectIndex,
        ReadPortableRuntimeObjectOwnership("@ClassDefault:Core.Subsystem").objectIndex,
        ReadPortableRuntimeObjectOwnership("@ClassDefault:Engine.Engine").objectIndex}};
    Require(oldDefaults[0u]&&oldDefaults[1u]&&oldDefaults[2u],"Original source CDO identities are absent");
    const auto beforeDependencies=temporary.Snapshot("BeforeDependencies");
    const auto noCapability=[&] {
        const auto gc=GC::GetStats();const auto revision=GetPortableRuntimeWorldRevision();
        Refused([&] {(void)PreparePortableRuntimeEngineCollision(ini);},"Engine collision accepted incomplete explicit prerequisites");
        GC::Collect();SameGc(gc,"Prerequisite refusal leaked native session/default/field objects");
        Require(!ReadPortableRuntimeEngineCollision().prepared&&GetPortableRuntimeWorldRevision()==revision&&
            temporary.Snapshot("PrerequisiteRefusal")==beforeDependencies,"Prerequisite refusal published partial collision/default/world state");
    };
    if(questStartup)std::cout<<"QUEST-SCOPE native Engine classes\n";
    noCapability();Require(PreparePortableRuntimeNativeEngineClasses().prepared,"Cannot prepare Engine prerequisite");
    if(questStartup)std::cout<<"QUEST-SCOPE Core metadata\n";
    noCapability();Require(PreparePortableRuntimeNativeMetadataClasses().prepared,"Cannot prepare Core metadata prerequisite");
    if(questStartup)std::cout<<"QUEST-SCOPE config constraints\n";
    noCapability();Require(PreparePortableRuntimeNativeConfigClasses().prepared,"Cannot prepare config prerequisite");
    if(questStartup)std::cout<<"QUEST-SCOPE Packages\n";
    noCapability();Require(PreparePortableRuntimeNativePackages().prepared,"Cannot prepare Package prerequisite");
    if(questStartup)std::cout<<"QUEST-SCOPE actual object lookup\n";
    noCapability();Require(PreparePortableRuntimeObjectLookup().prepared,"Cannot prepare actual lookup prerequisite");
    if(questStartup) {
        const auto actualLookup=ReadPortableRuntimeObjectLookup();
        std::cout<<"QUEST-SCOPE actual object lookup ready: "<<actualLookup.objects
            <<" objects / "<<actualLookup.retainedBytes<<" retained bytes\n";
    }
    if(questStartup)std::cout<<"QUEST-SCOPE late Transient rollback\n";
    LateTransientRollback(temporary,ini,oldDefaults);
    if(questStartup)std::cout<<"QUEST-SCOPE real first Training load\n";
    Require(LoadPortableRuntimeMap(map).passed,"Cannot load dormant first Training map");
    const auto world=WorldPaths();const auto revision=GetPortableRuntimeWorldRevision();
    const auto before=temporary.Snapshot("BeforeEngine");const auto gc=GC::GetStats();
    const auto immutableInjection=ImmutableDefaultFixture(temporary,before);
    Require(ValidatePortableRuntimeState(immutableInjection.string(),"00_Training")&&
        temporary.Snapshot("BeforeImmutableAuthority")==before&&GetPortableRuntimeWorldRevision()==revision,
        "Injected Engine-default record is not a structurally/schema-valid pre-native-preparation save");
    const auto model=ReadPortableRuntimeNativeModel();Require(model.available,"Dormant Training lacks real native Level/Model ownership");
    const auto sourceFields=OriginalFieldLists(packages);
    Require(!Declared("00_Training.LevelInfo0","Engine.LevelInfo.bBegunPlay",Kind::Bool).boolean,
        "Owned fixture unexpectedly started Training before native collision preparation");
    const auto lookupBefore=ReadPortableRuntimeObjectLookup();
    Refused([&] {(void)PreparePortableRuntimeEngineCollision(temporary.Path("MissingConfig").string());},
        "Native collision preparation accepted a missing explicitly supplied config file");
    GC::Collect();SameGc(gc,"Missing-config refusal leaked prepared defaults, fields, Engine or Primitive objects");
    Require(!ReadPortableRuntimeEngineCollision().prepared&&ReadPortableRuntimeObjectLookup().objects==lookupBefore.objects&&
        ReadPortableRuntimeObjectLookup().generation==lookupBefore.generation&&GetPortableRuntimeWorldRevision()==revision&&
        temporary.Snapshot("MissingConfigRollback")==before&&WorldPaths()==world,
        "Missing-config refusal published partial default/field/lookup/world state");
    if(questStartup)std::cout<<"QUEST-SCOPE Engine defaults and collision instance\n";
    const auto prepared=PreparePortableRuntimeEngineCollision(ini);
    if(questStartup)std::cout<<"QUEST-SCOPE Engine preparation ready\n";
    Require(prepared.prepared&&prepared.boundLevelPath==model.levelPath&&prepared.levelIndex==
        ReadPortableRuntimeObjectOwnership(model.levelPath).objectIndex,"Collision phase did not bind its actual currently loaded native Level");
    Require(GC::GetStats().numObjects>gc.numObjects&&GC::GetStats().memoryUsage>gc.memoryUsage,
        "Collision phase did not allocate genuine prepared fields/defaults/Engine/Transient/Primitive identities");
    DefaultAndFieldAuthority(prepared,oldDefaults,sourceFields);
    if(questStartup)std::cout<<"QUEST-SCOPE default/field authority ready\n";
    const auto engineSource=std::find_if(packages.begin(),packages.end(),[](const auto& source) {
        return fs::path(source.sourcePath).stem().string()=="Engine";
    });
    Require(engineSource!=packages.end(),"Original Engine source missing from explicit shared-package set");
    ActualInstanceCasts(prepared,*engineSource);
    if(questStartup)std::cout<<"QUEST-SCOPE native instance casts ready\n";
    const auto preparedGc=GC::GetStats();const auto lookup=ReadPortableRuntimeObjectLookup();
    Require(!ValidatePortableRuntimeState(immutableInjection.string(),"00_Training")&&
        !LoadPortableRuntimeState(immutableInjection.string()),
        "Save ingestion bypassed immutable native Engine default authority");++refusals;
    GC::Collect();SameGc(preparedGc,"Rejected immutable Engine-default save leaked native/default graph");
    SameSession(prepared,ReadPortableRuntimeEngineCollision());
    Require(temporary.Snapshot("ImmutableDefaultAfter")==before&&WorldPaths()==world&&
        GetPortableRuntimeWorldRevision()==revision&&!GetPortableRuntimeUnsavedScriptState(),
        "Rejected immutable Engine-default save retained a hidden classDefaultValues overlay");
    if(questStartup)std::cout<<"QUEST-SCOPE immutable-default save rejection ready\n";
    for(unsigned i=0u;i<3u;++i) {
        SameSession(prepared,ReadPortableRuntimeEngineCollision());
        SameSession(prepared,PreparePortableRuntimeEngineCollision(ini));GC::Collect();
        SameGc(preparedGc,"Idempotent native collision preparation replaced or leaked its owning graph");
    }
    auto detached=ReadPortableRuntimeEngineCollision();detached.defaults.front().cacheSizeMegs=999;
    detached.instanceServerActors.clear();detached.fields.front().outerIndex=0u;detached.engineIndex=0u;
    SameSession(prepared,ReadPortableRuntimeEngineCollision());
    CylinderActors(prepared);GC::Collect();SameGc(preparedGc,"Read-only Cylinder probes allocated native GC objects");
    if(questStartup)std::cout<<"QUEST-SCOPE Training Cylinder providers ready\n";
    Require(WorldPaths()==world&&GetPortableRuntimeWorldRevision()==revision&&temporary.Snapshot("AfterEngine")==before&&
        !GetPortableRuntimeUnsavedScriptState()&&!Declared("00_Training.LevelInfo0","Engine.LevelInfo.bBegunPlay",Kind::Bool).boolean&&
        ReadPortableRuntimeLevelTime()==0.0,"Collision preparation/probes changed dormant actors, script/save state, revision or time");
    Require(ReadPortableRuntimeObjectLookup().objects==lookup.objects&&ReadPortableRuntimeObjectLookup().generation==lookup.generation,
        "Cylinder probes mutated actual object lookup membership/generation");
    Require(LoadPortableRuntimeState(temporary.Path("BeforeEngine").string()),"Cannot warm-restore dormant Training collision fixture");
    SameSession(prepared,ReadPortableRuntimeEngineCollision());GC::Collect();SameGc(preparedGc,"Warm save restore replaced native Engine/default/field ownership");
    Require(temporary.Snapshot("WarmEngine")==before&&WorldPaths()==world,"Warm save restore changed dormant first Training state");
    ClearPortableRuntimeClient();SameSession(prepared,ReadPortableRuntimeEngineCollision());
    Require(UnloadPortableRuntimeMap()==map.exports.size(),"Cannot unload only Training map");GC::Collect();
    const auto unbound=ReadPortableRuntimeEngineCollision();SameSession(prepared,unbound,false);
    Require(unbound.boundLevelPath.empty()&&!unbound.levelIndex&&GetPortableRuntimeMapActors(true).empty(),
        "Native Engine retained its retired current Level or invented actors after unload");
    Refused([&] {(void)ReadPortableRuntimeObjectOwnership(model.levelPath);},"Retired Training native Level alias survived map GC");
    const auto unboundGc=GC::GetStats();
    Require(LoadPortableRuntimeMap(map).passed,"Cannot reload first Training map after actual Engine binding retirement");
    const auto rebound=ReadPortableRuntimeEngineCollision();SameSession(prepared,rebound,false);
    Require(rebound.boundLevelPath==ReadPortableRuntimeNativeModel().levelPath&&rebound.levelIndex&&
        WorldPaths()==world&&temporary.Snapshot("ReloadEngine")==before,"Reloaded Training lacks its real Engine binding or changed authored state");
    Require(UnloadPortableRuntimeMap()==map.exports.size(),"Cannot retire reloaded Training map");GC::Collect();
    SameGc(unboundGc,"Repeated Training loader/Engine lifecycle leaked native Level or map graph");
    const auto oldGeneration=ReadPortableRuntimeObjectLookup().generation;
    ShutdownPortableRuntime();Require(!ReadPortableRuntimeEngineCollision().prepared,"Shutdown retained the native collision session");
    Require(InitializePortableRuntime(packages).passed,"Cannot reinitialize genuine scripts for cold restore");PrepareDependencies();
    const auto cold=PreparePortableRuntimeEngineCollision(ini);
    Require(cold.prepared&&cold.boundLevelPath.empty()&&!cold.levelIndex&&ReadPortableRuntimeObjectLookup().generation!=oldGeneration,
        "Cold collision session reused a previous lookup generation or invented a map binding");
    // Mirror the production CPU bootstrap order with the exact shared source
    // set: collision Engine, real registered Client/Viewport, then first map.
    // These are metadata leases, never fabricated GL/OpenXR handles or a Pawn.
    std::shared_ptr<QuestVr::NativeClientLifetime> clientLifetime;
    if(questStartup) {
        std::cout<<"QUEST-SCOPE Engine-before-map client/viewport bootstrap\n";
        clientLifetime=std::make_shared<QuestVr::NativeClientLifetime>();
        BindPortableRuntimeClient(clientLifetime,1901u);
        RegisterPortableRuntimeViewport(1901u,{1902u,1024u,1152u,3u,2u});
        const auto session=PreparePortableRuntimePlayerObjects(1902u);
        Require(!session.bound&&!session.clientPath.empty()&&!session.viewportPath.empty(),
            "Production-order Client/Viewport preparation forced player possession or lost its genuine objects");
        SameSession(cold,ReadPortableRuntimeEngineCollision());
    }
    Require(LoadPortableRuntimeMap(map).passed&&LoadPortableRuntimeState(temporary.Path("BeforeEngine").string()),
        "Cannot cold-restore first Training with explicit native Engine prerequisites");
    const auto coldBound=ReadPortableRuntimeEngineCollision();SameSession(cold,coldBound,false);
    Require(coldBound.levelIndex&&coldBound.boundLevelPath==ReadPortableRuntimeNativeModel().levelPath&&
        WorldPaths()==world&&temporary.Snapshot("ColdEngine")==before&&
        !Declared("00_Training.LevelInfo0","Engine.LevelInfo.bBegunPlay",Kind::Bool).boolean,
        "Cold save reconstruction lost native Level.Engine or forced begun world state");
    if(questStartup) {
        Require(VerifyPortableRuntimeInteraction(),"Production-order Training pickup self-check regressed");
        Require(VerifyPortableRuntimeDamage(),"Production-order Training pawn damage self-check regressed");
        const auto stomp=ExecutePortableFunction("ScriptedPawn.WillTakeStompDamage");
        const auto shield=ExecutePortableFunction("ScriptedPawn.ShieldDamage");
        const auto cancel=ExecutePortableFunction("MenuUIChoice.CancelSetting");
        Require(stomp.type==PortableVmValueType::Boolean&&stomp.boolean&&
            shield.type==PortableVmValueType::Float&&shield.floating==1.0f&&cancel.type==PortableVmValueType::Nothing,
            "Production-order Training original function checks regressed");
        Require(temporary.Snapshot("QuestBootstrapAfterChecks")==before&&WorldPaths()==world&&
            !Declared("00_Training.LevelInfo0","Engine.LevelInfo.bBegunPlay",Kind::Bool).boolean,
            "Production CPU bootstrap self-checks changed dormant Training or forced original startup");
        clientLifetime->Revoke();ClearPortableRuntimeClient();
        SameSession(coldBound,ReadPortableRuntimeEngineCollision());
    }
    for(const auto& source:sources)source.Unchanged();
    ShutdownPortableRuntime();Require(!ReadPortableRuntimeEngineCollision().prepared,"Final shutdown retained native Engine collision capability");
    std::cout<<"PASS dormant first Training Engine collision integration: "<<packages.size()<<" shared script packages"
        <<(questStartup?" (exact Quest startup set)":" (focused dependency fixture)")<<", "
        <<fallbacks<<" actual shared Cylinder selections, "
        <<collidableFallbacks<<" collidable fallbacks, "<<queries<<" caller-packet adapter probes; actual defaults/fields/Outers/lookup, "
        "authentic ordered config, warm/cold save and loader/GC lifetime. Not begun world, Move, falling, live crates or headset evidence.\n";
}
} // namespace

int main(int argc,char** argv) {
    std::cout.setf(std::ios::unitbuf);
    try {
        fs::path root;bool questStartup{};
        const auto* configured=std::getenv("DEUSEX_GAME_ROOT");if(configured&&*configured)root=configured;
        for(int i=1;i<argc;++i) {
            const std::string_view option=argv[i];
            if(option=="--quest-startup"&&!questStartup)questStartup=true;
            else if(option=="--game-root"&&i+1<argc)root=argv[++i];
            else throw std::runtime_error("Usage: portable_engine_collision_test [--quest-startup] [--game-root OWNED_GAME_ROOT] (or DEUSEX_GAME_ROOT)");
        }
        if(root.empty()) {std::cout<<"SKIP owned Engine collision integration: use --game-root or DEUSEX_GAME_ROOT.\n";return 77;}
        ShutdownPortableRuntime();GC::Collect();const auto gc=GC::GetStats();Original(fs::canonical(root),questStartup);
        ShutdownPortableRuntime();GC::Collect();SameGc(gc,"Engine collision test leaked rooted native UObjects after final shutdown");
        std::cout<<"PASS native Engine collision integration "<<checks<<" checks, "<<refusals<<" explicit refusals\n";return 0;
    }catch(const std::exception& error) {std::cerr<<"FAIL: "<<error.what()<<'\n';ShutdownPortableRuntime();return 1;}
}
