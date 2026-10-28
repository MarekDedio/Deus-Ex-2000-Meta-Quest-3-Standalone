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

// Actual indexed constraint-class registration only. No commercial binary is
// executed, and no configured subclass/default/Engine/Actor is constructed.
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
            "Refused config-class phase published a native UObject identity");++refusals;return;
    }
    throw std::runtime_error("Native identity should remain unavailable: "+path);
}
std::uint64_t SourceHash(const fs::path& path) {
    std::ifstream file(path,std::ios::binary);Require(bool(file),"Cannot read owned config-class source: "+path.string());
    std::array<char,16384u> buffer{};std::uint64_t hash=14695981039346656037ull;std::uintmax_t total{};
    while(file) {
        file.read(buffer.data(),static_cast<std::streamsize>(buffer.size()));const auto count=file.gcount();
        total+=static_cast<std::uintmax_t>(count);Require(total<=512u*1024u*1024u,"Config-class source fingerprint exceeds bound");
        for(std::streamsize i=0;i<count;++i) {hash^=static_cast<unsigned char>(buffer[static_cast<std::size_t>(i)]);hash*=1099511628211ull;}
    }
    Require(file.eof(),"Cannot finish config-class source fingerprint");return hash;
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
        Require(parent!=parent.root_path()&&!Within(parent,fs::canonical(original)),"Temporary config-class fixture overlaps original/root");
        const auto stamp=std::chrono::steady_clock::now().time_since_epoch().count();
        for(unsigned i=0u;i<20u;++i) {
            const auto candidate=parent/("quest-native-config-classes-"+std::to_string(stamp)+'-'+std::to_string(i));
            if(fs::create_directory(candidate)) {directory=fs::canonical(candidate);break;}
        }
        Require(!directory.empty()&&directory.parent_path()==parent,"Cannot create isolated config-class fixture");
        Require(fs::create_directory(directory/"System"),"Cannot create config-class fixture System directory");
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
        Require(path.parent_path()==directory||path.parent_path()==directory/"System","Invalid isolated config-class file path");
        if(std::find(files.begin(),files.end(),path)==files.end())files.push_back(path);
        return path;
    }
    void Ini() {
        const auto path=Track(directory/"System"/"DeusEx.ini");std::ofstream file(path,std::ios::binary|std::ios::trunc);
        const std::string text="[Engine.Engine]\nGameEngine=DeusEx.DeusExGameEngine\n";
        Require(bool(file),"Cannot create isolated config-class INI");file.write(text.data(),static_cast<std::streamsize>(text.size()));file.close();
        Require(bool(file),"Cannot finish isolated config-class INI");
    }
    Bytes Snapshot(const std::string& leaf) {
        Require(!leaf.empty()&&leaf.find_first_of("/\\:")==std::string::npos,"Invalid config-class checkpoint leaf");
        const auto path=Track(directory/(leaf+".sav"));Require(SavePortableRuntimeState(path.string()),"Cannot save whole config-class checkpoint");
        Bytes bytes;Require(QuestVr::ReadBoundedSaveFile(path.string(),QuestVr::kMaximumSaveRuntimeBytes,bytes),"Cannot read config-class checkpoint");
        return bytes;
    }
};

// Independent literals from the owned native audit, not the production table.
constexpr std::array<const char*,4u> Paths{"Core.Language","Engine.AudioSubsystem","Engine.RenderDevice","Engine.NetDriver"};
constexpr std::array<const char*,4u> Bases{"Core.Object","Core.Subsystem","Core.Subsystem","Core.Subsystem"};
constexpr std::array<std::uint32_t,4u> Sizes{0x2cu,0x2cu,0xbcu,0x90u};
constexpr std::array<std::uint32_t,4u> Flags{0x1bu,0x17u,0x17u,0x1fu};
void SnapshotIs(const PortableNativeConfigClassesSnapshot& value,bool prepared,
    const PortableNativeMetadataClassesSnapshot* metadata=nullptr) {
    Require(value.prepared==prepared&&value.classes.size()==(prepared?4u:0u),"Config-class state/count differs from closed four-class contract");
    if(!prepared) {
        Require(value.consoleClassPath.empty()&&value.consoleObjectIndex==0u&&value.consoleMetaClassIndex==0u&&
            value.consoleWithinClassIndex==0u&&value.consoleClassFlags==0u&&value.consoleObjectFlags==0u,
            "Unprepared config-class snapshot exposed a partial Console binding");return;
    }
    Require(metadata&&metadata->prepared&&metadata->classes.size()==14u,"Config-class fixture lacks its explicit fourteen-class dependency");
    const auto objectIndex=metadata->classes[0u].baseClassIndex,metaIndex=metadata->classes[3u].objectIndex;
    Require(value.consoleClassPath=="Engine.Console"&&value.consoleObjectIndex!=0u&&value.consoleMetaClassIndex==metaIndex&&
        value.consoleWithinClassIndex==objectIndex&&value.consoleClassFlags==0x13au&&value.consoleObjectFlags==0x040f0004u,
        "Config-class phase did not reuse the original serialized Console/header/actual Class and Within identities");
    std::set<std::uint32_t> indices;
    for(const auto& cls:metadata->classes)indices.insert(cls.objectIndex);
    Require(indices.insert(value.consoleObjectIndex).second&&value.consoleObjectIndex!=objectIndex,
        "Serialized Console aliases a native metadata or Core.Object allocation");
    std::uint32_t subsystemIndex{};
    for(std::size_t i=0u;i<Paths.size();++i) {
        const auto& row=value.classes[i];
        Require(row.classPath==Paths[i]&&row.baseClassPath==Bases[i]&&row.withinClassPath=="Core.Object"&&
            row.metaClassPath=="Core.Class"&&row.nativeSize==Sizes[i]&&row.registrationClassFlags==Flags[i]&&
            row.registrationObjectFlags==0x04084004u,"Config-class native literal/relationship differs from original audit");
        Require(row.objectIndex!=0u&&row.objectIndex!=objectIndex&&indices.insert(row.objectIndex).second,
            "Config classes do not hold distinct actual UObject index leases");
        Require(row.withinClassIndex==objectIndex&&row.metaClassIndex==metaIndex&&row.baseClassIndex!=0u,
            "Config-class Within/metaclass/base uses a null or unshared actual index");
        if(i==0u)Require(row.baseClassIndex==objectIndex,"Language does not derive from actual shared Core.Object");
        else {
            if(!subsystemIndex)subsystemIndex=row.baseClassIndex;
            Require(row.baseClassIndex==subsystemIndex&&subsystemIndex!=objectIndex&&subsystemIndex!=metaIndex,
                "Subsystem constraints do not share actual Core.Subsystem distinct from Object/Class");
        }
    }
    Require(indices.count(subsystemIndex)==0u,"A native constraint allocation aliases its original Core.Subsystem dependency");
}
void SameSnapshot(const PortableNativeConfigClassesSnapshot& expected,const PortableNativeConfigClassesSnapshot& actual,
    const PortableNativeMetadataClassesSnapshot& metadata) {
    SnapshotIs(actual,expected.prepared,&metadata);
    Require(actual.consoleObjectIndex==expected.consoleObjectIndex&&actual.consoleMetaClassIndex==expected.consoleMetaClassIndex&&
        actual.consoleWithinClassIndex==expected.consoleWithinClassIndex,"Repeated config-class phase replaced the actual serialized Console identity");
    for(std::size_t i=0u;i<expected.classes.size();++i) {
        const auto& a=expected.classes[i];const auto& b=actual.classes[i];
        Require(a.objectIndex==b.objectIndex&&a.baseClassIndex==b.baseClassIndex&&a.withinClassIndex==b.withinClassIndex&&a.metaClassIndex==b.metaClassIndex,
            "Repeated config-class phase replaced actual native UObject index relationships");
    }
}
void SameMetadata(const PortableNativeMetadataClassesSnapshot& expected) {
    const auto actual=ReadPortableRuntimeNativeMetadataClasses();
    Require(actual.prepared&&actual.classes.size()==14u&&actual.boundSerializedClasses==expected.boundSerializedClasses,
        "Config-class registration changed the closed fourteen-class metadata count");
    for(std::size_t i=0u;i<expected.classes.size();++i) {
        const auto& a=expected.classes[i];const auto& b=actual.classes[i];
        Require(a.classPath==b.classPath&&a.objectIndex==b.objectIndex&&a.baseClassIndex==b.baseClassIndex&&
            a.withinClassIndex==b.withinClassIndex&&a.metaClassIndex==b.metaClassIndex,
            "Config-class registration replaced a preexisting Core metadata identity");
    }
    Require(ReadPortableRuntimeNativeEngineClasses().classes.size()==5u,"Config-class phase changed the closed five-class Engine domain");
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
    Require(begun.kind==Kind::Bool&&!begun.boolean,"Config-class registration began the original world");
}
std::int32_t ExportClass(const PortablePackageTables& table,const std::string& path) {
    const auto stem=fs::path(table.sourcePath).stem().string();
    for(std::size_t i=0u;i<table.exports.size();++i)
        if(table.exports[i].ObjClass==0&&Fold(stem+'.'+GetPortableObjectPath(table,static_cast<std::int32_t>(i+1u)))==Fold(path))
            return static_cast<std::int32_t>(i+1u);
    throw std::runtime_error("Actual owned UClass export is absent: "+path);
}
void Cast(const std::string& declaring,std::int32_t target,const std::string& path,bool meta,const std::string& expected) {
    const auto operand=Value::Text(Kind::Object,path);const auto result=CastPortableRuntimeObject(declaring,target,operand,meta);
    Require(result.kind==Kind::Object&&result.text==expected&&result.fields.empty(),"Config-class cast returned an incorrect identity/kind");
    Require(operand.text==path,"Config-class cast changed its input");
}
void Headers(const PortablePackageTables& core,const PortablePackageTables& engine) {
    const auto object=ExportClass(core,"Core.Object"),subsystem=ExportClass(core,"Core.Subsystem"),actor=ExportClass(engine,"Engine.Actor");
    for(std::size_t i=0u;i<Paths.size();++i) {
        const auto* path=Paths[i];
        Cast("Core.Object",object,Fold(path),true,path);Cast("Core.Object",object,path,false,path);
        Cast("Core.Subsystem",subsystem,path,true,i?path:"");
        Cast("Core.Subsystem",subsystem,path,false,"");Cast("Engine.Actor",actor,path,true,"");
        Refused([&] {(void)ReadPortableClassDefault(path,"Name");},"Config class acquired invented default storage");
        Refused([&] {(void)ReadPortableRuntimeAuthoredStateDescriptor(path);},"Config class acquired invented serialized state metadata");
        Refused([&] {(void)ReadPortableRuntimeDispatchClass(path);},"Config class acquired invented authored dispatch metadata");
        Refused([&] {(void)ReadPortableRuntimeActorToWorld(path);},"Config UClass was treated as a native Actor");
        const auto alias=std::string("@ClassDefault:")+path;
        Refused([&] {(void)CastPortableRuntimeObject("Core.Object",object,Value::Text(Kind::Object,alias),false);},
            "Config-class phase manufactured a default UObject alias");
    }
    Cast("Core.Object",object,"engine.console",true,"Engine.Console");
    Cast("Core.Object",object,"Engine.Console",false,"Engine.Console");
}
void SameAuthoredState(const PortableStateDescriptor& a,const PortableStateDescriptor& b) {
    Require(a.objectPath==b.objectPath&&a.baseField==b.baseField&&a.nextField==b.nextField&&a.scriptText==b.scriptText&&
        a.children==b.children&&a.friendlyName==b.friendlyName&&a.line==b.line&&a.textPos==b.textPos&&a.logicalSize==b.logicalSize&&
        a.rawBytes==b.rawBytes&&a.bytecode==b.bytecode&&a.probeMask==b.probeMask&&a.ignoreMask==b.ignoreMask&&
        a.labelTableOffset==b.labelTableOffset&&a.stateFlags==b.stateFlags,"Config-class registration changed serialized UClass state/Children/Next");
}
void SameConsoleDescriptor(const PortableClassDescriptor& a,const PortableClassDescriptor& b) {
    SameAuthoredState(a.state,b.state);
    Require(a.objectPath==b.objectPath&&a.classFlags==b.classFlags&&a.classWithinReference==b.classWithinReference&&
        a.configName==b.configName&&a.stateBytecode==b.stateBytecode&&a.dependencyCount==b.dependencyCount&&
        a.packageImportCount==b.packageImportCount&&a.defaults.size()==b.defaults.size(),
        "Console serialized flags/Within/config/default descriptor changed");
    for(std::size_t i=0u;i<a.defaults.size();++i) {
        const auto& x=a.defaults[i];const auto& y=b.defaults[i];
        Require(x.name==y.name&&x.type==y.type&&x.structName==y.structName&&x.arrayIndex==y.arrayIndex&&
            x.size==y.size&&x.valueOffset==y.valueOffset&&x.boolValue==y.boolValue&&x.value==y.value,
            "Console original serialized default tags changed");
    }
}
struct ConsoleDefault {std::string name;std::uint32_t slot{};Value value;};
std::vector<ConsoleDefault> ConsoleDefaults(const PortableClassDescriptor& descriptor) {
    std::vector<ConsoleDefault> result;
    for(const auto& tag:descriptor.defaults) {
        if(tag.type!=2u&&tag.type!=3u&&tag.type!=4u&&tag.type!=6u&&tag.type!=13u)continue;
        const auto name=tag.name.ToString();
        result.push_back({name,tag.arrayIndex,ReadPortableClassDefault("Engine.Console",name,tag.arrayIndex)});
    }
    return result;
}
void SameConsoleDefaults(const std::vector<ConsoleDefault>& expected) {
    for(const auto& field:expected)
        Require(QuestVr::Vm::Equal(field.value,ReadPortableClassDefault("Engine.Console",field.name,field.slot)),
            "Config-class registration replaced or changed an existing Console scalar default");
}
void Original(const std::vector<PortablePackageTables>& packages,const PortablePackageTables& map,Temporary& temporary) {
    Require(InitializePortableRuntime(packages).passed,"Cannot initialize actual original config-class fixture");
    SnapshotIs(ReadPortableRuntimeNativeConfigClasses(),false);
    for(unsigned phase=0u;phase<2u;++phase) {
        if(phase)Require(PreparePortableRuntimeNativeEngineClasses().prepared,"Cannot prepare existing Engine phase");
        const auto gc=GC::GetStats();const auto before=temporary.Snapshot(phase?"OnlyEngineBefore":"NoPhaseBefore");
        const auto revision=GetPortableRuntimeWorldRevision();
        Refused([&] {(void)PreparePortableRuntimeNativeConfigClasses();},"Config-class phase silently prepared its metadata prerequisite");
        SnapshotIs(ReadPortableRuntimeNativeConfigClasses(),false);
        Require(!ReadPortableRuntimeNativeMetadataClasses().prepared,"Missing-prerequisite refusal auto-published metadata classes");
        Require(ReadPortableRuntimeNativeEngineClasses().prepared==bool(phase),"Missing-prerequisite refusal changed existing Engine phase");
        GC::Collect();SameGc(gc,"Missing-prerequisite config phase leaked UClasses/domain/index leases");
        Require(temporary.Snapshot(phase?"OnlyEngineAfter":"NoPhaseAfter")==before&&GetPortableRuntimeWorldRevision()==revision,
            "Missing-prerequisite config refusal changed the whole checkpoint/revision");
        for(const auto* path:Paths)Unavailable([&] {(void)ReadPortableRuntimeAuthoredStateDescriptor(path);},path);
    }
    const auto metadata=PreparePortableRuntimeNativeMetadataClasses();Require(metadata.prepared,"Cannot prepare existing metadata phase");
    const auto scriptGc=GC::GetStats();
    Require(LoadPortableRuntimeMap(map).passed,"Cannot load dormant original Training fixture");Dormant();
    const auto before=temporary.Snapshot("OwnedBefore");const auto worldBefore=WorldPaths();const auto revision=GetPortableRuntimeWorldRevision();
    const auto gc=GC::GetStats();const auto model=ReadPortableRuntimeNativeModel();
    const auto cache=ReadPortableClassDefault("Engine.Engine","CacheSizeMegs"),sound=ReadPortableClassDefault("Engine.Engine","UseSound");
    const std::array<const char*,4u> authoredPaths{"Core.Object","Core.Subsystem","Engine.Engine","Engine.Console"};
    std::vector<PortableStateDescriptor> authored;
    for(const auto* path:authoredPaths)authored.push_back(ReadPortableRuntimeAuthoredStateDescriptor(path));
    const auto& engine=packages[1u];const auto console=ExportClass(engine,"Engine.Console");
    const auto consoleDescriptor=LoadPortableClassDescriptor(engine,308u);
    Require(console==309&&engine.exports[308u].ObjClass==0&&static_cast<std::uint32_t>(engine.exports[308u].ObjFlags)==0x040f0004u&&
        consoleDescriptor.classFlags==0x13au,"Owned Console is not the audited original serialized Class/header");
    Require(consoleDescriptor.classWithinReference&&*consoleDescriptor.classWithinReference!=0&&
        GetPortableObjectPath(engine,*consoleDescriptor.classWithinReference)=="Core.Object"&&
        consoleDescriptor.configName&&*consoleDescriptor.configName=="System",
        "Console descriptor did not retain its actual original Within reference and System config FName");
    const auto consoleDefaults=ConsoleDefaults(consoleDescriptor);
    const auto prepared=PreparePortableRuntimeNativeConfigClasses();SnapshotIs(prepared,true,&metadata);
    const auto preparedGc=GC::GetStats();
    Require(preparedGc.numObjects==gc.numObjects+5u&&preparedGc.memoryUsage>gc.memoryUsage,
        "Config phase did not allocate exactly four native UClasses plus one domain, with no Console replacement/defaults/instances");
    Headers(packages[0u],packages[1u]);SameMetadata(metadata);
    for(unsigned repeat=0u;repeat<4u;++repeat) {
        SameSnapshot(prepared,PreparePortableRuntimeNativeConfigClasses(),metadata);
        SameSnapshot(prepared,ReadPortableRuntimeNativeConfigClasses(),metadata);
        GC::Collect();SameGc(preparedGc,"Repeated config-class prepare/read/GC changed the native graph");
    }
    auto detached=ReadPortableRuntimeNativeConfigClasses();detached.classes[0u].classPath="Changed.Detached";
    detached.consoleClassPath="Changed.Console";detached.consoleObjectIndex=0u;detached.consoleWithinClassIndex=0u;detached.prepared=false;
    SameSnapshot(prepared,ReadPortableRuntimeNativeConfigClasses(),metadata);
    for(std::size_t i=0u;i<authored.size();++i)SameAuthoredState(authored[i],ReadPortableRuntimeAuthoredStateDescriptor(authoredPaths[i]));
    SameConsoleDescriptor(consoleDescriptor,LoadPortableClassDescriptor(engine,308u));SameConsoleDefaults(consoleDefaults);
    Require(QuestVr::Vm::Equal(cache,ReadPortableClassDefault("Engine.Engine","CacheSizeMegs"))&&
        QuestVr::Vm::Equal(sound,ReadPortableClassDefault("Engine.Engine","UseSound")),"Config-class phase changed original Engine scalar defaults");
    Require(temporary.Snapshot("OwnedAfter")==before&&GetPortableRuntimeWorldRevision()==revision&&WorldPaths()==worldBefore&&
        ReadPortableRuntimeLevelTime()==0.0&&!GetPortableRuntimeUnsavedScriptState(),"Config-class phase changed dormant checkpoint/revision/actors/clock/dirty state");
    const auto afterModel=ReadPortableRuntimeNativeModel();
    Require(model.available==afterModel.available&&model.geometryCached==afterModel.geometryCached&&model.mapGeneration==afterModel.mapGeneration&&
        model.levelPath==afterModel.levelPath&&model.modelPath==afterModel.modelPath&&model.cachedNodes==afterModel.cachedNodes,
        "Config-class phase changed native Model identity/cache");Dormant();
    const auto warmRevision=GetPortableRuntimeWorldRevision();
    Require(LoadPortableRuntimeState((temporary.directory/"OwnedBefore.sav").string()),"Cannot warm-restore dormant config-class checkpoint");
    Require(GetPortableRuntimeWorldRevision()==warmRevision+1u,"Explicit warm replacement did not advance revision exactly once");
    SameSnapshot(prepared,ReadPortableRuntimeNativeConfigClasses(),metadata);SameMetadata(metadata);
    SameConsoleDefaults(consoleDefaults);
    GC::Collect();SameGc(preparedGc,"Warm checkpoint replacement lost/duplicated config classes or Console");
    Require(temporary.Snapshot("OwnedWarm")==before&&WorldPaths()==worldBefore&&ReadPortableRuntimeLevelTime()==0.0,
        "Warm checkpoint replacement contaminated the dormant world");Dormant();
    Require(UnloadPortableRuntimeMap()>0u&&WorldPaths().empty(),"Cannot unload config-class Training fixture");GC::Collect();
    SameSnapshot(prepared,PreparePortableRuntimeNativeConfigClasses(),metadata);SameMetadata(metadata);
    Require(GC::GetStats().numObjects==scriptGc.numObjects+5u,"Map unload did not retain exactly four classes plus their domain");
    Require(LoadPortableRuntimeMap(map).passed,"Cannot reload Training with indexed config classes");
    const auto reloadedGc=GC::GetStats();SameSnapshot(prepared,ReadPortableRuntimeNativeConfigClasses(),metadata);SameMetadata(metadata);
    SameConsoleDescriptor(consoleDescriptor,LoadPortableClassDescriptor(engine,308u));SameConsoleDefaults(consoleDefaults);
    SameSnapshot(prepared,PreparePortableRuntimeNativeConfigClasses(),metadata);GC::Collect();
    SameGc(reloadedGc,"Map reload lost/duplicated static config-class roots or Console");
    Require(temporary.Snapshot("OwnedReload")==before&&WorldPaths()==worldBefore&&ReadPortableRuntimeLevelTime()==0.0,
        "Reloaded map changed saved actor/default state after config registration");Dormant();
    Require(ReadPortableRuntimeNativeModel().mapGeneration!=model.mapGeneration,"Reload reused a retired native Level generation");
    ShutdownPortableRuntime();SnapshotIs(ReadPortableRuntimeNativeConfigClasses(),false);
}
void RefusalFixtures(const fs::path& root,const std::vector<PortablePackageTables>& packages,Temporary& temporary) {
    const auto copyPath=temporary.Track(temporary.directory/"System"/"Core.u");
    Require(fs::copy_file(root/"System"/"Core.u",copyPath),"Cannot create isolated unchanged Core copy");
    const Source copy(copyPath);auto core=packages[0u];core.sourcePath=copyPath.string();temporary.Ini();
    Require(InitializePortableRuntime(std::vector<PortablePackageTables>{core}).passed,"Cannot initialize missing Engine dependency fixture");
    const auto missingGc=GC::GetStats();const auto missing=temporary.Snapshot("MissingBefore");
    Refused([&] {(void)PreparePortableRuntimeNativeConfigClasses();},"Missing real Engine/metadata dependencies were fabricated");
    SnapshotIs(ReadPortableRuntimeNativeConfigClasses(),false);GC::Collect();SameGc(missingGc,"Missing dependency leaked config classes");
    Require(temporary.Snapshot("MissingAfter")==missing,"Missing dependency changed checkpoint");
    for(const auto* path:Paths)Unavailable([&] {(void)ReadPortableRuntimeAuthoredStateDescriptor(path);},path);
    // Tables-only ordinary Object at the LAST native registration name. No
    // original payload/source bytes are altered and no false Class is supplied.
    auto engine=packages[1u];ExportTableEntry extra{};extra.ObjName=static_cast<std::int32_t>(engine.names.size());
    engine.names.push_back({NameString("NetDriver"),0u});extra.ObjClass=ExportClass(engine,"Engine.Engine");engine.exports.push_back(extra);
    Require(InitializePortableRuntime(std::vector<PortablePackageTables>{core,engine}).passed,"Cannot initialize late config-class-name collision fixture");
    Require(PreparePortableRuntimeNativeEngineClasses().prepared,"Late config-name collision changed Engine prerequisite");
    const auto metadata=PreparePortableRuntimeNativeMetadataClasses();Require(metadata.prepared,"Cannot prepare collision fixture metadata");
    const auto before=temporary.Snapshot("CollisionBefore");const auto gc=GC::GetStats();const auto revision=GetPortableRuntimeWorldRevision();
    const auto console=ReadPortableRuntimeAuthoredStateDescriptor("Engine.Console");
    Refused([&] {(void)PreparePortableRuntimeNativeConfigClasses();},"Native config phase overwrote a late same-name ordinary UObject");
    SnapshotIs(ReadPortableRuntimeNativeConfigClasses(),false);SameMetadata(metadata);
    GC::Collect();SameGc(gc,"Late config-name collision leaked staged prefix/domain/object indices");
    for(std::size_t i=0u;i+1u<Paths.size();++i)Unavailable([&] {(void)ReadPortableRuntimeAuthoredStateDescriptor(Paths[i]);},Paths[i]);
    Refused([&] {(void)ReadPortableRuntimeAuthoredStateDescriptor("Engine.NetDriver");},"Existing ordinary collision object acquired UClass metadata");
    // This tables-only sentinel intentionally has no on-disk export slot. Its
    // ordinary-object cast must keep refusing source provenance; removing it
    // or replacing it with the pinned native Class profile would return a
    // value instead and fail this control.
    Refused([&] {(void)CastPortableRuntimeObject("Engine.Engine",ExportClass(engine,"Engine.Engine"),
        Value::Text(Kind::Object,"Engine.NetDriver"),false);},
        "Tables-only collision object was removed or acquired native Class/source authority");
    SameAuthoredState(console,ReadPortableRuntimeAuthoredStateDescriptor("Engine.Console"));
    Require(temporary.Snapshot("CollisionAfter")==before&&GetPortableRuntimeWorldRevision()==revision&&WorldPaths().empty(),
        "Late-name refusal changed checkpoint/revision/map state or partially published Console binding");
    // A renamed real serialized Console stays a real Class, but is not the
    // required Engine.Console identity. Its original on-disk bytes are intact.
    engine=packages[1u];const auto consoleSlot=static_cast<std::size_t>(ExportClass(engine,"Engine.Console")-1);
    engine.exports[consoleSlot].ObjName=static_cast<std::int32_t>(engine.names.size());
    engine.names.push_back({NameString("MissingConsoleFixture"),0u});
    Require(InitializePortableRuntime(std::vector<PortablePackageTables>{core,engine}).passed,"Cannot initialize missing Console identity fixture");
    Require(PreparePortableRuntimeNativeEngineClasses().prepared,"Cannot prepare missing Console Engine prerequisite");
    const auto noConsoleMetadata=PreparePortableRuntimeNativeMetadataClasses();Require(noConsoleMetadata.prepared,"Cannot prepare missing Console metadata prerequisite");
    const auto noConsole=temporary.Snapshot("NoConsoleBefore");const auto noConsoleGc=GC::GetStats();
    Refused([&] {(void)PreparePortableRuntimeNativeConfigClasses();},"Config phase manufactured a replacement serialized Console");
    SnapshotIs(ReadPortableRuntimeNativeConfigClasses(),false);SameMetadata(noConsoleMetadata);
    GC::Collect();SameGc(noConsoleGc,"Missing Console refusal leaked native classes or replacement Console");
    Require(temporary.Snapshot("NoConsoleAfter")==noConsole&&WorldPaths().empty(),"Missing Console refusal changed checkpoint/map state");
    Unavailable([&] {(void)ReadPortableRuntimeAuthoredStateDescriptor("Engine.Console");},"Engine.Console");
    for(const auto* path:Paths)Unavailable([&] {(void)ReadPortableRuntimeAuthoredStateDescriptor(path);},path);
    copy.Unchanged();ShutdownPortableRuntime();
}
} // namespace
int main(int argc,char** argv) {
    std::cout.setf(std::ios::unitbuf);
    try {
        fs::path root;
        if(argc==3&&std::string(argv[1])=="--game-root")root=argv[2];
        else if(argc==1) {const auto* configured=std::getenv("DEUSEX_GAME_ROOT");if(configured&&*configured)root=configured;}
        else throw std::runtime_error("Usage: portable_native_config_classes_test [--game-root OWNED_GAME_ROOT] (or DEUSEX_GAME_ROOT)");
        if(root.empty()) {std::cout<<"SKIP owned native config-class integration: use --game-root or DEUSEX_GAME_ROOT.\n";return 77;}
        root=fs::canonical(root);ShutdownPortableRuntime();GC::Collect();const auto baseline=GC::GetStats();Temporary temporary(root);
        std::vector<Source> sources;std::vector<PortablePackageTables> packages;
        sources.emplace_back(root/"System"/"DeusEx.ini");
        for(const char* name:{"Core","Engine","DeusEx","Extension","ConSys","IpDrv","IpServer","UBrowser","UWindow",
            "Editor","Fire","MPCharacters","DeusExSounds","DeusExDeco","DeusExItems","DeusExCharacters"}) {
            sources.emplace_back(root/"System"/(std::string(name)+".u"));packages.push_back(LoadPortablePackageTables(sources.back().path.string()));
        }
        sources.emplace_back(root/"Maps"/"00_Training.dx");const auto map=LoadPortablePackageTables(sources.back().path.string());
        Original(packages,map,temporary);RefusalFixtures(root,packages,temporary);
        ShutdownPortableRuntime();GC::Collect();SameGc(baseline,"Config-class roots/Console binding survived explicit runtime shutdown");
        SnapshotIs(ReadPortableRuntimeNativeConfigClasses(),false);
        Refused([&] {(void)PreparePortableRuntimeNativeConfigClasses();},"Config phase succeeded without initialized runtime");
        GC::Collect();SameGc(baseline,"No-runtime config-class preparation leaked native objects");
        for(const auto& source:sources)source.Unchanged();
        std::cout<<"PASS owned native config-class integration: "<<checks<<" checks, "<<refusals
            <<" refusals; exact four registration records/+5 GC graph, real Super/Within/metaclass indices, original serialized Console reuse, "
              "unchanged five/fourteen prerequisite domains, dormant checkpoints/default/state/model preservation, warm restore/map/GC/shutdown lifetime, "
              "missing-phase/dependency/Console and late-NetDriver-name atomic refusals. Not StaticFindObject/package Outers, configured subclasses, "
              "native defaults, Engine/Cylinder instances, startup, physics or VR rendering.\n";
        return 0;
    }catch(const std::exception& error) {std::cerr<<"FAIL: "<<error.what()<<'\n';ShutdownPortableRuntime();return 1;}
}
