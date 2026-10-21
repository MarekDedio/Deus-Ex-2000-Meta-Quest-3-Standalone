#include "Precomp.h"
#include "GC/GC.h"
#include "portable_unreal_runtime.h"
#include "quest_save_bundle.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

// Owned-original indexed metadata-class binding only. No commercial binary
// execution, native default construction, Engine/Cylinder or Actor startup.
namespace {
namespace fs=std::filesystem;
using Bytes=std::vector<std::uint8_t>;
using Kind=QuestVr::Vm::Kind;
using Value=QuestVr::Vm::Value;
std::size_t checks{},refusals{};
void Require(bool value,const std::string& why) {++checks;if(!value)throw std::runtime_error(why);}
std::string Fold(std::string value) {
    for(auto& c:value)if(c>='A'&&c<='Z')c=static_cast<char>(c-'A'+'a');
    return value;
}
bool Within(const fs::path& child,const fs::path& parent) {
    auto a=child.begin(),b=parent.begin();
    for(;b!=parent.end();++a,++b)if(a==child.end()||Fold(a->string())!=Fold(b->string()))return false;
    return true;
}
template<class F> void Refused(F&& operation,const std::string& why) {
    bool refused{};try {operation();}catch(const std::exception&) {refused=true;}
    Require(refused,why);++refusals;
}
template<class F> void Unavailable(F&& operation,const std::string& path) {
    try {operation();}
    catch(const std::exception& error) {
        Require(error.what()=="Portable actor VM object is unavailable: "+path,
            "Refused metadata phase published a native UObject identity");++refusals;return;
    }
    throw std::runtime_error("Native identity should remain unavailable: "+path);
}
std::uint64_t SourceHash(const fs::path& path) {
    std::ifstream file(path,std::ios::binary);Require(bool(file),"Cannot read owned metadata source: "+path.string());
    std::array<char,16384u> buffer{};std::uint64_t hash=14695981039346656037ull;std::uintmax_t total{};
    while(file) {
        file.read(buffer.data(),static_cast<std::streamsize>(buffer.size()));const auto count=file.gcount();
        total+=static_cast<std::uintmax_t>(count);Require(total<=512u*1024u*1024u,"Metadata source fingerprint exceeds bound");
        for(std::streamsize i=0;i<count;++i) {hash^=static_cast<unsigned char>(buffer[static_cast<std::size_t>(i)]);hash*=1099511628211ull;}
    }
    Require(file.eof(),"Cannot finish metadata source fingerprint");return hash;
}
struct Source {
    fs::path path;std::uintmax_t size{};fs::file_time_type modified;std::uint64_t hash{};
    explicit Source(const fs::path& value):path(fs::canonical(value)),size(fs::file_size(path)),
        modified(fs::last_write_time(path)),hash(SourceHash(path)) {}
    void Unchanged() const {
        Require(fs::file_size(path)==size&&fs::last_write_time(path)==modified&&SourceHash(path)==hash,
            "Owned source bytes/size/mtime changed: "+path.string());
    }
};
struct Temporary {
    fs::path parent,directory;std::vector<fs::path> files;
    explicit Temporary(const fs::path& original) {
        parent=fs::canonical(fs::temp_directory_path());
        Require(parent!=parent.root_path()&&!Within(parent,fs::canonical(original)),"Temporary metadata fixture overlaps original/root");
        const auto stamp=std::chrono::steady_clock::now().time_since_epoch().count();
        for(unsigned i=0u;i<20u;++i) {
            const auto candidate=parent/("quest-native-metadata-classes-"+std::to_string(stamp)+'-'+std::to_string(i));
            if(fs::create_directory(candidate)) {directory=fs::canonical(candidate);break;}
        }
        Require(!directory.empty()&&directory.parent_path()==parent,"Cannot create isolated metadata fixture");
        Require(fs::create_directory(directory/"System"),"Cannot create metadata fixture System directory");
    }
    ~Temporary() {
        ShutdownPortableRuntime();std::error_code ignored;
        for(const auto& file:files)
            if(file.parent_path()==directory||file.parent_path()==directory/"System")fs::remove(file,ignored);
        if(!directory.empty()&&directory.parent_path()==parent) {
            fs::remove(directory/"System",ignored);fs::remove(directory,ignored);
        }
    }
    fs::path Track(const fs::path& path) {
        Require(path.parent_path()==directory||path.parent_path()==directory/"System","Invalid isolated metadata file path");
        if(std::find(files.begin(),files.end(),path)==files.end())files.push_back(path);
        return path;
    }
    void Ini() {
        const auto path=Track(directory/"System"/"DeusEx.ini");std::ofstream file(path,std::ios::binary|std::ios::trunc);
        const std::string text="[Engine.Engine]\nGameEngine=DeusEx.DeusExGameEngine\n";
        Require(bool(file),"Cannot create isolated metadata INI");file.write(text.data(),static_cast<std::streamsize>(text.size()));file.close();
        Require(bool(file),"Cannot finish isolated metadata INI");
    }
    Bytes Snapshot(const std::string& leaf) {
        Require(!leaf.empty()&&leaf.find_first_of("/\\:")==std::string::npos,"Invalid metadata checkpoint leaf");
        const auto path=Track(directory/(leaf+".sav"));Require(SavePortableRuntimeState(path.string()),"Cannot save whole metadata fixture checkpoint");
        Bytes bytes;Require(QuestVr::ReadBoundedSaveFile(path.string(),QuestVr::kMaximumSaveRuntimeBytes,bytes),"Cannot read metadata fixture checkpoint");
        return bytes;
    }
};

// Independent literal evidence from ORIGINAL-NATIVE-METADATA-REGISTRATION.md
// and ORIGINAL-STRUCT-PROPERTY-REGISTRATION.md,
// not a copy of a production registration table or a path-hash pseudo-index.
constexpr std::array<const char*,12u> Paths{
    "Core.Field","Core.Struct","Core.State","Core.Class","Core.Property","Core.ObjectProperty",
    "Core.ClassProperty","Core.IntProperty","Core.BoolProperty","Core.StrProperty","Core.ArrayProperty","Core.StructProperty"};
constexpr std::array<const char*,12u> Bases{
    "Core.Object","Core.Field","Core.Struct","Core.State","Core.Field","Core.Property",
    "Core.ObjectProperty","Core.Property","Core.Property","Core.Property","Core.Property","Core.Property"};
constexpr std::array<const char*,12u> Withins{
    "Core.Object","Core.Object","Core.Object","Core.Package","Core.Field","Core.Field",
    "Core.Field","Core.Field","Core.Field","Core.Field","Core.Field","Core.Field"};
constexpr std::array<std::uint32_t,12u> Sizes{0x34u,0x6cu,0x484u,0x4f4u,0x5cu,0x64u,0x68u,0x5cu,0x60u,0x5cu,0x60u,0x64u};
constexpr std::array<std::uint32_t,12u> Flags{0x13u,0x12u,0x12u,0x12u,0x13u,0x12u,0x12u,0x12u,0x12u,0x12u,0x12u,0x12u};
void SnapshotIs(const PortableNativeMetadataClassesSnapshot& value,bool prepared,std::size_t serialized=0u) {
    Require(value.prepared==prepared&&value.classes.size()==(prepared?Paths.size():0u),"Metadata preparation state/count differs from twelve-class contract");
    Require(value.boundSerializedClasses==(prepared?serialized:0u),"Metadata binding count differs from actual loaded serialized UClasses");
    if(!prepared)return;
    std::set<std::uint32_t> indices;
    for(std::size_t i=0u;i<Paths.size();++i) {
        const auto& row=value.classes[i];
        Require(row.classPath==Paths[i]&&row.baseClassPath==Bases[i]&&row.withinClassPath==Withins[i]&&
            row.metaClassPath=="Core.Class"&&row.nativeSize==Sizes[i]&&row.registrationClassFlags==Flags[i]&&
            row.registrationObjectFlags==0x04084004u,"Metadata native registration literal/relationship differs from original audit");
        Require(row.objectIndex!=0u&&indices.insert(row.objectIndex).second,"Metadata UClasses do not hold distinct actual object-index leases");
        Require(row.baseClassIndex!=0u&&row.withinClassIndex!=0u&&row.metaClassIndex==value.classes[3u].objectIndex,
            "Metadata class relationship has a null/stale/unshared actual index");
    }
    const auto object=value.classes[0u].baseClassIndex,package=value.classes[3u].withinClassIndex;
    Require(object!=package&&indices.count(object)==0u&&indices.count(package)==0u,
        "Original Object/Package dependencies alias metadata class allocation slots");
    constexpr std::array<int,12u> baseSlots{-1,0,1,2,0,4,5,4,4,4,4,4};
    for(std::size_t i=0u;i<Paths.size();++i) {
        const auto base=baseSlots[i]<0?object:value.classes[static_cast<std::size_t>(baseSlots[i])].objectIndex;
        const auto within=i<3u?object:i==3u?package:value.classes[0u].objectIndex;
        Require(value.classes[i].baseClassIndex==base&&value.classes[i].withinClassIndex==within,
            "Metadata Super/Within uses matching names but not actual shared indexed identities");
    }
    Require(value.classes[3u].metaClassIndex==value.classes[3u].objectIndex,"Core.Class is not its own actual indexed metaclass");
}
void SameSnapshot(const PortableNativeMetadataClassesSnapshot& expected,const PortableNativeMetadataClassesSnapshot& actual) {
    SnapshotIs(actual,expected.prepared,expected.boundSerializedClasses);
    Require(actual.classes.size()==expected.classes.size(),"Detached metadata snapshot size changed");
    for(std::size_t i=0u;i<expected.classes.size();++i) {
        const auto& a=expected.classes[i];const auto& b=actual.classes[i];
        Require(a.objectIndex==b.objectIndex&&a.baseClassIndex==b.baseClassIndex&&a.withinClassIndex==b.withinClassIndex&&
            a.metaClassIndex==b.metaClassIndex,"Repeated metadata phase replaced actual UObject allocation/index relationships");
    }
}
void SameGc(const GCStats& expected,const std::string& why) {
    const auto actual=GC::GetStats();Require(actual.numObjects==expected.numObjects&&actual.memoryUsage==expected.memoryUsage,why);
}
std::vector<std::string> WorldPaths() {
    std::vector<std::string> result;
    for(const auto& actor:GetPortableRuntimeMapActors(true))result.push_back(actor.objectPath+'|'+actor.classPath);
    return result;
}
void Dormant() {
    const auto begun=ReadPortableActorDeclaredScriptProperty("00_Training.LevelInfo0","Engine.LevelInfo.bBegunPlay");
    Require(begun.kind==Kind::Bool&&!begun.boolean,"Metadata registration began the original world");
}
std::int32_t ExportClass(const PortablePackageTables& table,const std::string& path) {
    const auto stem=fs::path(table.sourcePath).stem().string();
    for(std::size_t i=0u;i<table.exports.size();++i)
        if(table.exports[i].ObjClass==0&&Fold(stem+'.'+GetPortableObjectPath(table,static_cast<std::int32_t>(i+1u)))==Fold(path))
            return static_cast<std::int32_t>(i+1u);
    throw std::runtime_error("Actual owned UClass export is absent: "+path);
}
std::size_t SerializedClasses(const std::vector<PortablePackageTables>& packages) {
    std::size_t count{};
    for(const auto& package:packages)for(const auto& entry:package.exports)if(entry.ObjClass==0)++count;
    return count;
}
void Cast(const std::string& declaring,std::int32_t target,const std::string& path,bool meta,const std::string& expected) {
    const auto operand=Value::Text(Kind::Object,path);const auto result=CastPortableRuntimeObject(declaring,target,operand,meta);
    Require(result.kind==Kind::Object&&result.text==expected&&result.fields.empty(),"Metadata class cast returned an incorrect real identity/kind");
    Require(operand.text==path,"Metadata class cast changed its input");
}
void Headers(const PortablePackageTables& core,const PortablePackageTables& engine) {
    const auto object=ExportClass(core,"Core.Object"),engineClass=ExportClass(engine,"Engine.Engine"),actor=ExportClass(engine,"Engine.Actor");
    for(const auto* path:Paths) {
        Cast("Core.Object",object,Fold(path),true,path);
        Cast("Core.Object",object,path,false,path);
        Cast("Engine.Engine",engineClass,path,false,"");
        Cast("Engine.Actor",actor,path,true,"");
        Refused([&] {(void)ReadPortableClassDefault(path,"Name");},"Metadata class acquired invented default storage");
        Refused([&] {(void)ReadPortableRuntimeAuthoredStateDescriptor(path);},"Metadata class acquired invented serialized state metadata");
        Refused([&] {(void)ReadPortableRuntimeDispatchClass(path);},"Metadata class acquired invented authored dispatch metadata");
        Refused([&] {(void)ReadPortableRuntimeActorToWorld(path);},"Metadata UClass was treated as a native Actor");
        const auto alias=std::string("@ClassDefault:")+path;
        Refused([&] {(void)CastPortableRuntimeObject("Core.Object",object,Value::Text(Kind::Object,alias),false);},
            "Metadata registration manufactured a class-default UObject alias");
    }
}
void SameAuthoredState(const PortableStateDescriptor& a,const PortableStateDescriptor& b) {
    Require(a.objectPath==b.objectPath&&a.baseField==b.baseField&&a.nextField==b.nextField&&a.scriptText==b.scriptText&&
        a.children==b.children&&a.friendlyName==b.friendlyName&&a.line==b.line&&a.textPos==b.textPos&&a.logicalSize==b.logicalSize&&
        a.rawBytes==b.rawBytes&&a.bytecode==b.bytecode&&a.probeMask==b.probeMask&&a.ignoreMask==b.ignoreMask&&
        a.labelTableOffset==b.labelTableOffset&&a.stateFlags==b.stateFlags,"Metadata binding changed actual serialized UClass state/Children/Next");
}
void Original(const std::vector<PortablePackageTables>& packages,const PortablePackageTables& map,Temporary& temporary) {
    Require(InitializePortableRuntime(packages).passed,"Cannot initialize actual original metadata fixture");
    const auto serialized=SerializedClasses(packages);Require(serialized>1000u,"Owned fixture lacks the expected substantial original UClass population");
    SnapshotIs(ReadPortableRuntimeNativeMetadataClasses(),false);
    const auto noPhaseGc=GC::GetStats();const auto noPhaseCheckpoint=temporary.Snapshot("NoPhaseBefore");
    Refused([&] {(void)PreparePortableRuntimeNativeMetadataClasses();},"Metadata phase silently prepared native Engine classes");
    Require(!ReadPortableRuntimeNativeEngineClasses().prepared,"Metadata refusal auto-published the native Engine phase");
    SnapshotIs(ReadPortableRuntimeNativeMetadataClasses(),false);GC::Collect();SameGc(noPhaseGc,"Missing prior phase leaked metadata graph");
    Require(temporary.Snapshot("NoPhaseAfter")==noPhaseCheckpoint,"Missing prior phase changed checkpoint");
    for(const auto* path:Paths)Unavailable([&] {(void)ReadPortableRuntimeAuthoredStateDescriptor(path);},path);
    Require(PreparePortableRuntimeNativeEngineClasses().prepared,"Cannot prepare required existing native Engine class phase");
    const auto scriptGc=GC::GetStats();Require(LoadPortableRuntimeMap(map).passed,"Cannot load dormant original Training fixture");
    Dormant();const auto before=temporary.Snapshot("OwnedBefore");const auto world=WorldPaths();const auto revision=GetPortableRuntimeWorldRevision();
    const auto gc=GC::GetStats();const auto model=ReadPortableRuntimeNativeModel();
    const auto cache=ReadPortableClassDefault("Engine.Engine","CacheSizeMegs"),sound=ReadPortableClassDefault("Engine.Engine","UseSound");
    const std::array<const char*,3u> originalPaths{"Core.Object","Core.Subsystem","Engine.Engine"};
    std::vector<PortableStateDescriptor> states;
    for(const auto* path:originalPaths)states.push_back(ReadPortableRuntimeAuthoredStateDescriptor(path));
    const auto prepared=PreparePortableRuntimeNativeMetadataClasses();SnapshotIs(prepared,true,serialized);
    const auto preparedGc=GC::GetStats();Require(preparedGc.numObjects==gc.numObjects+13u&&preparedGc.memoryUsage>gc.memoryUsage,
        "Metadata phase did not allocate exactly twelve UClasses plus one owning domain (no defaults/instances)");
    Headers(packages[0u],packages[1u]);
    for(unsigned repeat=0u;repeat<4u;++repeat) {
        SameSnapshot(prepared,PreparePortableRuntimeNativeMetadataClasses());SameSnapshot(prepared,ReadPortableRuntimeNativeMetadataClasses());
        GC::Collect();SameGc(preparedGc,"Repeated metadata preparation/read/GC changed native UObject graph");
    }
    auto detached=ReadPortableRuntimeNativeMetadataClasses();detached.classes[0u].classPath="Changed.Detached";
    detached.classes[3u].metaClassIndex=0u;detached.boundSerializedClasses=0u;detached.prepared=false;
    SameSnapshot(prepared,ReadPortableRuntimeNativeMetadataClasses());
    for(std::size_t i=0u;i<states.size();++i)SameAuthoredState(states[i],ReadPortableRuntimeAuthoredStateDescriptor(originalPaths[i]));
    Require(QuestVr::Vm::Equal(cache,ReadPortableClassDefault("Engine.Engine","CacheSizeMegs"))&&
        QuestVr::Vm::Equal(sound,ReadPortableClassDefault("Engine.Engine","UseSound")),"Metadata binding changed original Engine default scalars");
    Require(ReadPortableRuntimeNativeEngineClasses().classes.size()==5u,"Metadata phase changed the closed five-class Engine registration domain");
    Require(temporary.Snapshot("OwnedAfter")==before&&GetPortableRuntimeWorldRevision()==revision&&WorldPaths()==world&&
        ReadPortableRuntimeLevelTime()==0.0&&!GetPortableRuntimeUnsavedScriptState(),"Metadata preparation changed complete dormant checkpoint/revision/actors/clock/dirty state");
    const auto afterModel=ReadPortableRuntimeNativeModel();
    Require(model.available==afterModel.available&&model.geometryCached==afterModel.geometryCached&&model.mapGeneration==afterModel.mapGeneration&&
        model.levelPath==afterModel.levelPath&&model.modelPath==afterModel.modelPath&&model.cachedNodes==afterModel.cachedNodes,
        "Metadata phase changed native Model identity/cache");Dormant();
    const auto warmRevision=GetPortableRuntimeWorldRevision();
    Require(LoadPortableRuntimeState((temporary.directory/"OwnedBefore.sav").string()),"Cannot warm-restore dormant metadata checkpoint");
    Require(GetPortableRuntimeWorldRevision()==warmRevision+1u,"Explicit warm replacement did not advance revision exactly once");
    SameSnapshot(prepared,ReadPortableRuntimeNativeMetadataClasses());GC::Collect();SameGc(preparedGc,"Warm checkpoint replacement lost/duplicated metadata objects");
    Require(temporary.Snapshot("OwnedWarm")==before&&WorldPaths()==world&&ReadPortableRuntimeLevelTime()==0.0,
        "Warm checkpoint replacement contaminated dormant world");Dormant();
    Require(UnloadPortableRuntimeMap()>0u&&WorldPaths().empty(),"Cannot unload metadata Training fixture");GC::Collect();
    SameSnapshot(prepared,PreparePortableRuntimeNativeMetadataClasses());
    Require(GC::GetStats().numObjects==scriptGc.numObjects+13u,"Map unload did not retain exactly the static metadata registration domain");
    Require(LoadPortableRuntimeMap(map).passed,"Cannot reload Training with existing indexed metadata classes");
    const auto reloadGc=GC::GetStats();SameSnapshot(prepared,ReadPortableRuntimeNativeMetadataClasses());
    SameSnapshot(prepared,PreparePortableRuntimeNativeMetadataClasses());GC::Collect();SameGc(reloadGc,"Map reload lost/duplicated static metadata roots");
    Require(temporary.Snapshot("OwnedReload")==before&&WorldPaths()==world&&ReadPortableRuntimeLevelTime()==0.0,
        "Reloaded map changed actor/default state after metadata binding");Dormant();
    Require(ReadPortableRuntimeNativeModel().mapGeneration!=model.mapGeneration,"Reload reused retired native Level generation");
    // A future map's actual ObjClass0 identity must bind even though the map
    // loader intentionally does not manufacture a script ClassDescriptor.
    // This is an in-memory table addition, not a change to original map bytes.
    auto lateMap=map;ExportTableEntry lateClass{};
    lateClass.ObjName=static_cast<std::int32_t>(lateMap.names.size());
    lateMap.names.push_back({NameString("MetadataLateMapClass"),0u});lateMap.exports.push_back(lateClass);
    Require(LoadPortableRuntimeMap(lateMap).passed,"Cannot load owned Training plus explicit tables-only Class identity");
    SnapshotIs(ReadPortableRuntimeNativeMetadataClasses(),true,serialized+1u);
    SnapshotIs(PreparePortableRuntimeNativeMetadataClasses(),true,serialized+1u);
    Refused([&] {(void)ReadPortableRuntimeAuthoredStateDescriptor("00_Training.MetadataLateMapClass");},
        "Actual map Class identity invented a script descriptor");
    Require(UnloadPortableRuntimeMap()>0u,"Cannot retire late map metadata fixture");
    SameSnapshot(prepared,ReadPortableRuntimeNativeMetadataClasses());
    ShutdownPortableRuntime();SnapshotIs(ReadPortableRuntimeNativeMetadataClasses(),false);
}
void ClientClassPhases(const std::vector<PortablePackageTables>& packages,Temporary& temporary) {
    const auto serialized=SerializedClasses(packages);
    for(const bool before:{true,false}) {
        Require(InitializePortableRuntime(packages).passed,"Cannot initialize native-session metadata fixture");
        Require(PreparePortableRuntimeNativeEngineClasses().prepared,"Cannot prepare session fixture Engine classes");
        auto lifetime=std::make_shared<QuestVr::NativeClientLifetime>();
        const std::uint64_t client=before?701u:801u,viewport=client+1u;
        const auto create=[&] {
            BindPortableRuntimeClient(lifetime,client);RegisterPortableRuntimeViewport(client,{viewport,1440u,1440u,3u,2u});
            const auto session=PreparePortableRuntimePlayerObjects(viewport);
            Require(session.clientIdentity==client&&session.viewportIdentity==viewport&&session.viewportClassPath=="Engine.Viewport"&&
                !session.bound&&session.playerPath.empty(),"Native metadata fixture session fabricated possession or lost its actual viewport");
        };
        if(before)create();
        const auto checkpoint=temporary.Snapshot(before?"SessionBeforeMetadata":"MetadataBeforeSession");
        const auto revision=GetPortableRuntimeWorldRevision();
        const auto prepared=PreparePortableRuntimeNativeMetadataClasses();SnapshotIs(prepared,true,serialized);
        if(!before)create();
        // Read validates the actual metaclass of every existing native session
        // class, including those created after the metadata phase publication.
        SameSnapshot(prepared,ReadPortableRuntimeNativeMetadataClasses());
        const auto session=ReadPortableRuntimePlayerSession(viewport);
        Require(session.clientIdentity==client&&session.viewportIdentity==viewport&&!session.bound,
            "Metadata binding replaced the actual existing native player session");
        const auto gc=GC::GetStats();
        SameSnapshot(prepared,PreparePortableRuntimeNativeMetadataClasses());
        Require(PreparePortableRuntimePlayerObjects(viewport).viewportPath==session.viewportPath,
            "Repeated native player preparation replaced viewport identity");GC::Collect();
        SameGc(gc,"Repeated session/metadata preparation duplicated native classes or instances");
        lifetime->Revoke();ClearPortableRuntimeClient();GC::Collect();
        Require(!HasPortableRuntimeClient(),"Session fixture did not clear revoked native client");
        SameSnapshot(prepared,ReadPortableRuntimeNativeMetadataClasses());
        Require(temporary.Snapshot(before?"SessionAfterMetadata":"MetadataAfterSession")==checkpoint&&
            GetPortableRuntimeWorldRevision()==revision&&WorldPaths().empty()&&!GetPortableRuntimeUnsavedScriptState(),
            "Session metadata binding/revocation changed saved actor/default state or revision");
        ShutdownPortableRuntime();SnapshotIs(ReadPortableRuntimeNativeMetadataClasses(),false);
    }
}
void RefusalFixtures(const fs::path& root,const std::vector<PortablePackageTables>& packages,Temporary& temporary) {
    const auto copyPath=temporary.Track(temporary.directory/"System"/"Core.u");
    Require(fs::copy_file(root/"System"/"Core.u",copyPath),"Cannot create isolated unchanged Core metadata copy");
    const Source copy(copyPath);auto core=packages[0u];core.sourcePath=copyPath.string();temporary.Ini();
    Require(InitializePortableRuntime(std::vector<PortablePackageTables>{core}).passed,"Cannot initialize missing Engine dependency fixture");
    const auto missingGc=GC::GetStats();const auto missingBefore=temporary.Snapshot("MissingBefore");const auto missingRevision=GetPortableRuntimeWorldRevision();
    Refused([&] {(void)PreparePortableRuntimeNativeMetadataClasses();},"Missing actual Engine/Package dependency was fabricated");
    Refused([&] {(void)PreparePortableRuntimeNativeEngineClasses();},"Missing actual serialized Engine base was fabricated");
    SnapshotIs(ReadPortableRuntimeNativeMetadataClasses(),false);GC::Collect();SameGc(missingGc,"Missing-dependency refusal leaked metadata registrations");
    Require(temporary.Snapshot("MissingAfter")==missingBefore&&GetPortableRuntimeWorldRevision()==missingRevision&&WorldPaths().empty(),
        "Missing-dependency refusal changed checkpoint/revision/map state");
    for(const auto* path:Paths)Unavailable([&] {(void)ReadPortableRuntimeAuthoredStateDescriptor(path);},path);
    // Add a tables-only ordinary Object under the LAST native metadata name.
    // Its payload is never read; all original package bytes remain untouched.
    ExportTableEntry extra{};extra.ObjName=static_cast<std::int32_t>(core.names.size());
    core.names.push_back({NameString("StructProperty"),0u});
    extra.ObjClass=ExportClass(core,"Core.Object");core.exports.push_back(extra);
    Require(InitializePortableRuntime(std::vector<PortablePackageTables>{core,packages[1u]}).passed,"Cannot initialize late metadata-name collision fixture");
    Require(PreparePortableRuntimeNativeEngineClasses().prepared,"Late metadata collision prevents unrelated prerequisite Engine class phase");
    const auto collisionGc=GC::GetStats();const auto collisionBefore=temporary.Snapshot("CollisionBefore");const auto revision=GetPortableRuntimeWorldRevision();
    Refused([&] {(void)PreparePortableRuntimeNativeMetadataClasses();},"Native metadata phase overwrote a real late same-name UObject");
    SnapshotIs(ReadPortableRuntimeNativeMetadataClasses(),false);GC::Collect();SameGc(collisionGc,"Late metadata staging refusal leaked UClasses/domain/indices");
    Require(ReadPortableRuntimeNativeEngineClasses().prepared&&ReadPortableRuntimeNativeEngineClasses().classes.size()==5u,
        "Failed metadata phase removed/changed its preexisting Engine registration dependency");
    for(std::size_t i=0u;i+1u<Paths.size();++i)Unavailable([&] {(void)ReadPortableRuntimeAuthoredStateDescriptor(Paths[i]);},Paths[i]);
    Refused([&] {(void)ReadPortableRuntimeAuthoredStateDescriptor("Core.StructProperty");},"Existing ordinary collision object acquired authored UClass metadata");
    Require(temporary.Snapshot("CollisionAfter")==collisionBefore&&GetPortableRuntimeWorldRevision()==revision&&WorldPaths().empty(),
        "Late metadata-name refusal published partial gameplay/index/checkpoint state");
    copy.Unchanged();ShutdownPortableRuntime();
}
} // namespace
int main(int argc,char** argv) {
    std::cout.setf(std::ios::unitbuf);
    try {
        fs::path root;
        if(argc==3&&std::string(argv[1])=="--game-root")root=argv[2];
        else if(argc==1) {const auto* configured=std::getenv("DEUSEX_GAME_ROOT");if(configured&&*configured)root=configured;}
        else throw std::runtime_error("Usage: portable_native_metadata_classes_test [--game-root OWNED_GAME_ROOT] (or DEUSEX_GAME_ROOT)");
        if(root.empty()) {std::cout<<"SKIP owned native metadata-class integration: use --game-root or DEUSEX_GAME_ROOT.\n";return 77;}
        root=fs::canonical(root);ShutdownPortableRuntime();GC::Collect();const auto baseline=GC::GetStats();Temporary temporary(root);
        std::vector<Source> sources;std::vector<PortablePackageTables> packages;
        sources.emplace_back(root/"System"/"DeusEx.ini");
        for(const char* name:{"Core","Engine","DeusEx","Extension","ConSys","IpDrv","IpServer","UBrowser","UWindow",
            "Editor","Fire","MPCharacters","DeusExSounds","DeusExDeco","DeusExItems","DeusExCharacters"}) {
            sources.emplace_back(root/"System"/(std::string(name)+".u"));packages.push_back(LoadPortablePackageTables(sources.back().path.string()));
        }
        sources.emplace_back(root/"Maps"/"00_Training.dx");const auto map=LoadPortablePackageTables(sources.back().path.string());
        Original(packages,map,temporary);ClientClassPhases(packages,temporary);RefusalFixtures(root,packages,temporary);
        ShutdownPortableRuntime();GC::Collect();SameGc(baseline,"Metadata roots survived explicit runtime shutdown");
        SnapshotIs(ReadPortableRuntimeNativeMetadataClasses(),false);
        Refused([&] {(void)PreparePortableRuntimeNativeMetadataClasses();},"Metadata phase succeeded without initialized runtime");
        GC::Collect();SameGc(baseline,"No-runtime metadata preparation leaked native objects");
        for(const auto& source:sources)source.Unchanged();
        std::cout<<"PASS owned native metadata-class integration: "<<checks<<" checks, "<<refusals
            <<" refusals; exact twelve registration records/+13 GC graph, real distinct object-index/Super/Within/self-metaclass relationships, "
              "serialized-UClass binding count including later map Class without descriptor, native session classes before/after preparation, "
              "dormant whole checkpoints/default/state/model preservation, warm restore/map/GC/shutdown lifetime, "
              "missing-phase/dependency and late-name atomic refusal. Not defaults, Engine/Cylinder instances, startup, physics or VR rendering.\n";
        return 0;
    }catch(const std::exception& error) {std::cerr<<"FAIL: "<<error.what()<<'\n';ShutdownPortableRuntime();return 1;}
}
