#include "Precomp.h"
#include "GC/GC.h"
#include "portable_unreal_runtime.h"
#include "quest_engine_config.h"
#include "quest_save_bundle.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

// Owned-original native UClass registration only. Never executes an original
// DLL, starts Actors, instantiates an Engine/Cylinder or tests VR rendering.
namespace {
namespace fs=std::filesystem;
using Bytes=std::vector<std::uint8_t>;
using Kind=QuestVr::Vm::Kind;
using Value=QuestVr::Vm::Value;
std::size_t checks{},refusals{};
void Require(bool value,const std::string& why) {++checks;if(!value) throw std::runtime_error(why);}
std::string Fold(std::string text) {
    for(auto& c:text) if(c>='A'&&c<='Z') c=static_cast<char>(c-'A'+'a');
    return text;
}
bool Within(const fs::path& child,const fs::path& parent) {
    auto a=child.begin(),b=parent.begin();
    for(;b!=parent.end();++a,++b) if(a==child.end()||Fold(a->string())!=Fold(b->string())) return false;
    return true;
}
template<class F> void Refused(F&& operation,const std::string& why) {
    bool refused{};try {operation();} catch(const std::exception&) {refused=true;}
    Require(refused,why);++refusals;
}
template<class F> void ErrorIs(F&& operation,const std::string& expected) {
    try {operation();}
    catch(const std::exception& error) {Require(error.what()==expected,"Native registration refusal has wrong index/publication diagnostic");++refusals;return;}
    throw std::runtime_error("Native registration should have refused: "+expected);
}
std::uint64_t SourceHash(const fs::path& path) {
    std::ifstream file(path,std::ios::binary);Require(bool(file),"Cannot read owned source: "+path.string());
    std::uint64_t hash=14695981039346656037ull;std::array<char,16384u> buffer{};std::uintmax_t total{};
    while(file) {
        file.read(buffer.data(),static_cast<std::streamsize>(buffer.size()));const auto count=file.gcount();
        total+=static_cast<std::uintmax_t>(count);Require(total<=512u*1024u*1024u,"Owned source fingerprint exceeds bound");
        for(std::streamsize i=0;i<count;++i) {hash^=static_cast<unsigned char>(buffer[static_cast<std::size_t>(i)]);hash*=1099511628211ull;}
    }
    Require(file.eof(),"Owned source fingerprint read failed");return hash;
}
struct Source {
    fs::path path;std::uintmax_t size{};fs::file_time_type modified;std::uint64_t hash{};
    explicit Source(const fs::path& value):path(fs::canonical(value)),size(fs::file_size(path)),
        modified(fs::last_write_time(path)),hash(SourceHash(path)) {}
    void Unchanged() const {
        Require(fs::file_size(path)==size&&fs::last_write_time(path)==modified&&SourceHash(path)==hash,
            "Original source bytes/size/mtime changed: "+path.string());
    }
};
struct Temporary {
    fs::path parent,directory;std::vector<fs::path> files;
    explicit Temporary(const fs::path& original) {
        parent=fs::canonical(fs::temp_directory_path());
        Require(parent!=parent.root_path()&&!Within(parent,fs::canonical(original)),"Temporary root overlaps original installation/root");
        const auto stamp=std::chrono::steady_clock::now().time_since_epoch().count();
        for(unsigned i=0u;i<20u;++i) {
            const auto candidate=parent/("quest-native-engine-classes-"+std::to_string(stamp)+'-'+std::to_string(i));
            if(fs::create_directory(candidate)) {directory=fs::canonical(candidate);break;}
        }
        Require(!directory.empty()&&directory.parent_path()==parent,"Cannot create isolated native-engine fixture");
        Require(fs::create_directory(directory/"System"),"Cannot create isolated System directory");
    }
    ~Temporary() {
        ShutdownPortableRuntime();std::error_code ignored;
        for(const auto& file:files)
            if(file.parent_path()==directory||file.parent_path()==directory/"System") fs::remove(file,ignored);
        if(!directory.empty()&&directory.parent_path()==parent) {
            fs::remove(directory/"System",ignored);fs::remove(directory,ignored);
        }
    }
    fs::path Track(const fs::path& path) {
        Require(path.parent_path()==directory||path.parent_path()==directory/"System","Invalid isolated fixture file path");
        if(std::find(files.begin(),files.end(),path)==files.end()) files.push_back(path);
        return path;
    }
    void Ini(const std::string& text) {
        const auto path=Track(directory/"System"/"DeusEx.ini");std::ofstream file(path,std::ios::binary|std::ios::trunc);
        Require(bool(file),"Cannot create generated engine INI");file.write(text.data(),static_cast<std::streamsize>(text.size()));file.close();
        Require(bool(file),"Cannot finish generated engine INI");
    }
    Bytes Snapshot(const std::string& leaf) {
        Require(!leaf.empty()&&leaf.find_first_of("/\\:")==std::string::npos,"Invalid isolated checkpoint leaf");
        const auto path=Track(directory/(leaf+".sav"));Require(SavePortableRuntimeState(path.string()),"Cannot save complete native-engine checkpoint");
        Bytes bytes;Require(QuestVr::ReadBoundedSaveFile(path.string(),QuestVr::kMaximumSaveRuntimeBytes,bytes),"Cannot read native-engine checkpoint");
        return bytes;
    }
};
// Independent literals from the original native UClass registration audit,
// deliberately not copied out of the production NativeEngineClasses array.
// Registration baseline flags are not the loaded serialized ClassFlags, full
// post-registration/CDO state, or an Engine instance's allocation flags.
constexpr std::array<const char*,5u> Paths{
    "Core.Package","Engine.Primitive","Engine.GameEngine","Extension.GameEngineExt","DeusEx.DeusExGameEngine"};
constexpr std::array<const char*,5u> Bases{
    "Core.Object","Core.Object","Engine.Engine","Engine.GameEngine","Extension.GameEngineExt"};
constexpr std::array<std::uint32_t,5u> Sizes{0x34u,0x54u,0xd0u,0xd0u,0xd0u};
constexpr std::array<std::uint32_t,5u> Flags{0x12u,0x12u,0x1eu,0x1eu,0x1eu};
void SnapshotIs(const PortableNativeEngineClassesSnapshot& snapshot,const std::optional<std::string>& configured,bool prepared) {
    Require(snapshot.configuredGameEngineClass==configured,"Configured GameEngine spelling/availability changed");
    Require(snapshot.prepared==prepared&&snapshot.classes.size()==(prepared?5u:0u),"Native class preparation state/count mismatch");
    if(!prepared) return;
    for(std::size_t i=0u;i<Paths.size();++i) {
        const auto& row=snapshot.classes[i];
        Require(row.classPath==Paths[i]&&row.baseClassPath==Bases[i]&&row.withinClassPath=="Core.Object"&&
            row.nativeSize==Sizes[i]&&row.registrationClassFlags==Flags[i]&&row.registrationObjectFlags==0x04084004u,
            "Native UClass registration differs from independently audited original header");
    }
}
void SameGc(const GCStats& expected,const std::string& why) {
    const auto actual=GC::GetStats();Require(actual.numObjects==expected.numObjects&&actual.memoryUsage==expected.memoryUsage,why);
}
void Dormant() {
    const auto value=ReadPortableActorDeclaredScriptProperty("00_Training.LevelInfo0","Engine.LevelInfo.bBegunPlay");
    Require(value.kind==Kind::Bool&&!value.boolean,"Native class registration started the original world");
}
std::vector<std::string> WorldPaths() {
    std::vector<std::string> result;
    for(const auto& actor:GetPortableRuntimeMapActors(true)) result.push_back(actor.objectPath+'|'+actor.classPath);
    return result;
}
void SameModel(const PortableNativeModelSnapshot& a,const PortableNativeModelSnapshot& b) {
    Require(a.available==b.available&&a.geometryCached==b.geometryCached&&a.mapGeneration==b.mapGeneration&&
        a.levelPath==b.levelPath&&a.modelPath==b.modelPath&&a.modelExportIndex==b.modelExportIndex&&a.cachedNodes==b.cachedNodes,
        "Native class registration changed native Model binding/cache");
}
std::int32_t ExportClass(const PortablePackageTables& table,const std::string& path) {
    const auto stem=fs::path(table.sourcePath).stem().string();
    for(std::size_t i=0u;i<table.exports.size();++i)
        if(table.exports[i].ObjClass==0&&Fold(stem+'.'+GetPortableObjectPath(table,static_cast<std::int32_t>(i+1u)))==Fold(path))
            return static_cast<std::int32_t>(i+1u);
    throw std::runtime_error("Required actual owned UClass export absent: "+path);
}
std::int32_t ClassImport(const PortablePackageTables& table,const std::string& path) {
    for(std::size_t i=0u;i<table.imports.size();++i) {
        const auto ref=-static_cast<std::int32_t>(i+1u);
        if(Fold(GetPortableObjectPath(table,ref))==Fold(path)&&
            table.names.at(static_cast<std::size_t>(table.imports[i].ClassName)).Name=="Class") return ref;
    }
    throw std::runtime_error("Required actual owned Class import absent: "+path);
}
void SerializedBaseAuthorities(const PortablePackageTables& core,const PortablePackageTables& engine) {
    const std::array<const PortablePackageTables*,3u> tables{&core,&core,&engine};
    constexpr std::array<const char*,3u> paths{"Core.Object","Core.Subsystem","Engine.Engine"};
    constexpr std::array<const char*,3u> directBases{"","Core.Object","Core.Subsystem"};
    constexpr std::array<std::uint32_t,3u> serializedFlags{0x112u,0x112u,0x11eu};
    for(std::size_t i=0u;i<tables.size();++i) {
        const auto& table=*tables[i];const auto reference=ExportClass(table,paths[i]);
        const auto index=static_cast<std::size_t>(reference-1);const auto& entry=table.exports[index];
        const auto descriptor=LoadPortableClassDescriptor(table,index);
        std::string base;
        if(entry.ObjBase!=0) {
            base=GetPortableObjectPath(table,entry.ObjBase);
            if(entry.ObjBase>0) base=fs::path(table.sourcePath).stem().string()+'.'+base;
        }
        Require(base==directBases[i],"Actual serialized native base class has the wrong direct superclass");
        Require((static_cast<std::uint32_t>(entry.ObjFlags)&0x04000000u)!=0u,
            "Actual serialized engine base class lost RF_Native authority");
        Require(descriptor.classFlags==serializedFlags[i],"Actual original serialized ClassFlags differ from audited owned metadata");
    }
    // Owned Engine.u stores 0x11e, without CLASS_Abstract. The original native
    // constructor's 0xd|0x12 registration baseline is not this serialized value
    // and must not be substituted as a schema-admission abstract-bit test.
    Require((serializedFlags[2u]&1u)==0u,"Owned Engine.Engine serialized flags were conflated with native constructor baseline");
}
void Cast(const std::string& declaring,std::int32_t target,const std::string& path,bool meta,const std::string& expected) {
    const auto operand=Value::Text(Kind::Object,path);const auto result=CastPortableRuntimeObject(declaring,target,operand,meta);
    Require(result.kind==Kind::Object&&result.text==expected&&result.fields.empty(),"Native class cast returned incorrect identity/kind");
    Require(operand.text==path,"Native class cast mutated its input value");
}
void Headers(const PortablePackageTables& core,const PortablePackageTables& engine) {
    const auto object=ExportClass(core,"Core.Object"),subsystem=ExportClass(core,"Core.Subsystem");
    const auto engineClass=ExportClass(engine,"Engine.Engine"),actor=ExportClass(engine,"Engine.Actor");
    for(std::size_t i=0u;i<Paths.size();++i) {
        Cast("Core.Object",object,Fold(Paths[i]),true,Paths[i]);
        Cast("Core.Object",object,Paths[i],false,Paths[i]); // Actual operand is a UClass UObject.
        Cast("Core.Object",subsystem,Paths[i],true,i>=2u?Paths[i]:"");
        Cast("Engine.Engine",engineClass,Paths[i],true,i>=2u?Paths[i]:"");
        Cast("Engine.Engine",engineClass,Paths[i],false,""); // Never an Engine instance.
        Cast("Engine.Actor",actor,Paths[i],true,"");
        Refused([&] {(void)ReadPortableClassDefault(Paths[i],"Name");},"Native-only UClass invented a serialized CDO");
        Refused([&] {(void)ReadPortableRuntimeAuthoredStateDescriptor(Paths[i]);},"Native-only UClass invented a serialized descriptor");
        Refused([&] {(void)ReadPortableRuntimeDispatchClass(Paths[i]);},"Native-only UClass invented authored dispatch metadata");
        Refused([&] {(void)ReadPortableRuntimeActorToWorld(Paths[i]);},"Native UClass was treated as a map Actor instance");
        Refused([&] {(void)CastPortableRuntimeObject("Core.Object",object,Value::Text(Kind::Object,std::string("@ClassDefault:")+Paths[i]),false);},
            "Native-only UClass fabricated a CDO alias");
    }
    for(const char* path:{"Engine.UnknownGameEngine","DeusEx.UnknownGameEngine","Engine.Primitive0","Transient.Primitive0"})
        Refused([&] {(void)CastPortableRuntimeObject("Core.Object",object,Value::Text(Kind::Object,path),true);},
            "Unknown native class or nonexistent Primitive instance was accepted");
}
void Original(const fs::path& root,Temporary& temporary,const std::vector<PortablePackageTables>& packages,
    const PortablePackageTables& map) {
    SerializedBaseAuthorities(packages[0u],packages[1u]);
    const auto config=QuestVr::EngineConfig::ReadGameEngineClass(root);
    Require(config&&Fold(*config)=="deusex.deusexgameengine","Owned config is not the expected actual DeusEx concrete engine");
    Require(InitializePortableRuntime(packages).passed,"Cannot initialize original native-class fixture");
    SnapshotIs(ReadPortableRuntimeNativeEngineClasses(),config,false);
    const auto scriptGc=GC::GetStats();Require(LoadPortableRuntimeMap(map).passed,"Cannot load dormant original Training fixture");
    Dormant();const auto before=temporary.Snapshot("OwnedBefore");const auto revision=GetPortableRuntimeWorldRevision();
    const auto gc=GC::GetStats();const auto model=ReadPortableRuntimeNativeModel();const auto world=WorldPaths();
    Require(!world.empty()&&ReadPortableRuntimeLevelTime()==0.0,"Original Training fixture has no dormant actors/zero clock");
    SnapshotIs(PreparePortableRuntimeNativeEngineClasses(),config,true);
    const auto preparedGc=GC::GetStats();
    Require(preparedGc.numObjects==gc.numObjects+6u&&preparedGc.memoryUsage>gc.memoryUsage,
        "Preparation did not allocate exactly five UClasses plus one owning domain (no instances/CDOs)");
    Headers(packages[0u],packages[1u]);
    for(unsigned repeat=0u;repeat<4u;++repeat) {
        SnapshotIs(PreparePortableRuntimeNativeEngineClasses(),config,true);SnapshotIs(ReadPortableRuntimeNativeEngineClasses(),config,true);
        GC::Collect();SameGc(preparedGc,"Repeated native class preparation/read/GC changed UObject graph");
    }
    auto detached=ReadPortableRuntimeNativeEngineClasses();detached.classes[0u].classPath="changed detached row";
    detached.configuredGameEngineClass="Changed.Detached";detached.prepared=false;
    SnapshotIs(ReadPortableRuntimeNativeEngineClasses(),config,true);
    auto lifetime=std::make_shared<QuestVr::NativeClientLifetime>();BindPortableRuntimeClient(lifetime,911u);
    Require(HasPortableRuntimeClient(),"Fixture client bind failed");lifetime->Revoke();ClearPortableRuntimeClient();
    Require(!HasPortableRuntimeClient(),"Fixture client clear failed");GC::Collect();
    SnapshotIs(ReadPortableRuntimeNativeEngineClasses(),config,true);SameGc(preparedGc,"Client revocation/clear removed static native classes");
    Require(temporary.Snapshot("OwnedAfter")==before&&GetPortableRuntimeWorldRevision()==revision&&WorldPaths()==world&&
        ReadPortableRuntimeLevelTime()==0.0&&!GetPortableRuntimeUnsavedScriptState(),
        "Class registration/header inspection changed whole actor checkpoint/revision/slots/clock/dirty state");
    SameModel(model,ReadPortableRuntimeNativeModel());Dormant();
    // Explicit checkpoint replacement is a mutation, unlike the pure native
    // registration/read phase above. Its revision advance remains observable.
    const auto warmRevision=GetPortableRuntimeWorldRevision();
    Require(LoadPortableRuntimeState((temporary.directory/"OwnedBefore.sav").string()),
        "Cannot warm-load dormant Training checkpoint with native classes prepared");
    Require(GetPortableRuntimeWorldRevision()==warmRevision+1u,
        "Explicit warm checkpoint load did not advance world revision exactly once");
    SnapshotIs(ReadPortableRuntimeNativeEngineClasses(),config,true);
    SnapshotIs(PreparePortableRuntimeNativeEngineClasses(),config,true);GC::Collect();
    SameGc(preparedGc,"Warm checkpoint replacement lost or duplicated static native class/domain roots");
    SameModel(model,ReadPortableRuntimeNativeModel());Dormant();
    Require(temporary.Snapshot("OwnedWarm")==before&&WorldPaths()==world&&
        ReadPortableRuntimeLevelTime()==0.0&&!GetPortableRuntimeUnsavedScriptState(),
        "Warm checkpoint replacement changed dormant Training checkpoint/slots/clock/dirty state");
    Require(UnloadPortableRuntimeMap()>0u&&WorldPaths().empty(),"Cannot unload dormant Training fixture");GC::Collect();
    SnapshotIs(PreparePortableRuntimeNativeEngineClasses(),config,true);
    Require(GC::GetStats().numObjects==scriptGc.numObjects+6u,"Map unload did not retain exactly the session native class graph");
    Require(LoadPortableRuntimeMap(map).passed,"Cannot reload dormant Training fixture");
    const auto reloadGc=GC::GetStats();SnapshotIs(PreparePortableRuntimeNativeEngineClasses(),config,true);GC::Collect();
    SameGc(reloadGc,"Map reload duplicated/lost native class roots");Dormant();
    Require(temporary.Snapshot("OwnedReload")==before&&WorldPaths()==world&&ReadPortableRuntimeLevelTime()==0.0,
        "Native classes contaminated a clean original map reload/checkpoint");
    Require(ReadPortableRuntimeNativeModel().mapGeneration!=model.mapGeneration,"Reload reused the retired native Level generation");
    ShutdownPortableRuntime();SnapshotIs(ReadPortableRuntimeNativeEngineClasses(),std::nullopt,false);
}
void NotPublished(const std::string& path) {
    ErrorIs([&] {(void)ReadPortableRuntimeAuthoredStateDescriptor(path);},"Portable actor VM object is unavailable: "+path);
}
void AlternateConfigurations(const fs::path& root,Temporary& temporary,const std::vector<PortablePackageTables>& packages) {
    const auto copiedCore=temporary.Track(temporary.directory/"System"/"Core.u");
    Require(fs::copy_file(root/"System"/"Core.u",copiedCore),"Cannot create isolated read-only Core metadata copy");
    const Source copy(copiedCore);
    auto core=packages[0u];core.sourcePath=copiedCore.string();const auto& engine=packages[1u];
    const std::vector<PortablePackageTables> minimal{core,engine};
    const auto initialize=[&](const std::optional<std::string>& expected) {
        Require(InitializePortableRuntime(minimal).passed,"Cannot initialize isolated original Core/Engine metadata");
        SnapshotIs(ReadPortableRuntimeNativeEngineClasses(),expected,false);
    };
    // No INI, absent key and unsupported-but-well-formed values must remain
    // unavailable, never become a hardcoded abstract/concrete Engine instance.
    initialize(std::nullopt);Refused([&] {(void)PreparePortableRuntimeNativeEngineClasses();},"Missing INI invented native engine authority");
    for(const auto& configured:std::vector<std::optional<std::string>>{std::nullopt,"Engine.Engine","Engine.Primitive",
        "Core.Package","Core.Object","Unknown.ConcreteEngine"}) {
        temporary.Ini("[Engine.Engine]\nRoot=DeusEx.DeusExRootWindow\n"+
            (configured?"GameEngine="+*configured+'\n':std::string()));initialize(configured);
        const auto before=temporary.Snapshot("RefusalBefore");const auto gc=GC::GetStats();const auto revision=GetPortableRuntimeWorldRevision();
        Refused([&] {(void)PreparePortableRuntimeNativeEngineClasses();},"Absent/unsupported engine setting admitted native registration");
        SnapshotIs(ReadPortableRuntimeNativeEngineClasses(),configured,false);GC::Collect();SameGc(gc,"Configuration refusal leaked native registrations");
        Require(temporary.Snapshot("RefusalAfter")==before&&GetPortableRuntimeWorldRevision()==revision&&WorldPaths().empty(),
            "Configuration refusal changed checkpoint/revision/map state");NotPublished("Core.Package");NotPublished("Engine.GameEngine");
    }
    for(const std::string configured:{"Engine.GameEngine","Extension.GameEngineExt","dEuSeX.DeusExGameEngine"}) {
        temporary.Ini("[Engine.Engine]\nGameEngine="+configured+'\n');initialize(configured);
        const auto before=temporary.Snapshot("ConcreteBefore");const auto gc=GC::GetStats();const auto revision=GetPortableRuntimeWorldRevision();
        SnapshotIs(PreparePortableRuntimeNativeEngineClasses(),configured,true);
        Require(GC::GetStats().numObjects==gc.numObjects+6u,"Configured engine variant changed registration allocation count");
        Headers(core,engine);const auto prepared=GC::GetStats();GC::Collect();
        SnapshotIs(PreparePortableRuntimeNativeEngineClasses(),configured,true);SameGc(prepared,"Concrete engine variant preparation was not idempotent");
        Require(temporary.Snapshot("ConcreteAfter")==before&&GetPortableRuntimeWorldRevision()==revision&&WorldPaths().empty(),
            "Concrete registration changed no-map checkpoint/revision/state");
    }
    ShutdownPortableRuntime();GC::Collect();const auto emptyGc=GC::GetStats();
    for(const std::string malformed:{"[Engine.Engine]\nGameEngine=", "[Engine.Engine]\nGameEngine=Bad/Path",
        "[Engine.Engine]\nGameEngine=First.Engine\nGameEngine=Second.Engine"}) {
        temporary.Ini(malformed);Refused([&] {(void)InitializePortableRuntime(minimal);},"Malformed GameEngine config initialized a partial runtime");
        SnapshotIs(ReadPortableRuntimeNativeEngineClasses(),std::nullopt,false);GC::Collect();SameGc(emptyGc,"Malformed INI leaked initialized objects");
    }
    temporary.Ini("[Engine.Engine]\nGameEngine=DeusEx.DeusExGameEngine\n");
    Require(InitializePortableRuntime(std::vector<PortablePackageTables>{core}).passed,"Cannot initialize original Core-only refusal fixture");
    const auto missingGc=GC::GetStats();const auto missingBefore=temporary.Snapshot("MissingBaseBefore");
    Refused([&] {(void)PreparePortableRuntimeNativeEngineClasses();},"Missing actual Engine base metadata was fabricated");
    SnapshotIs(ReadPortableRuntimeNativeEngineClasses(),std::string("DeusEx.DeusExGameEngine"),false);
    GC::Collect();SameGc(missingGc,"Missing base metadata refusal leaked native classes");
    Require(temporary.Snapshot("MissingBaseAfter")==missingBefore,"Missing base metadata refusal changed checkpoint");
    // A tables-only ordinary Engine.Primitive identity collides after the first
    // Core.Package allocation. Do not modify the original Engine.u bytes.
    auto collision=engine;ExportTableEntry extra{};
    const auto name=std::find_if(collision.names.begin(),collision.names.end(),[](const auto& entry) {return entry.Name=="Primitive";});
    Require(name!=collision.names.end(),"Actual Engine name table lacks Primitive collision identity");
    extra.ObjName=static_cast<std::int32_t>(std::distance(collision.names.begin(),name));extra.ObjClass=ClassImport(collision,"Core.Object");
    collision.exports.push_back(extra);
    Require(InitializePortableRuntime(std::vector<PortablePackageTables>{core,collision}).passed,"Cannot initialize tables-only late-collision fixture");
    const auto collisionGc=GC::GetStats();const auto collisionBefore=temporary.Snapshot("CollisionBefore");
    const auto revision=GetPortableRuntimeWorldRevision();NotPublished("Core.Package");NotPublished("Engine.GameEngine");
    Refused([&] {(void)PreparePortableRuntimeNativeEngineClasses();},"Late native class identity collision was accepted");
    SnapshotIs(ReadPortableRuntimeNativeEngineClasses(),std::string("DeusEx.DeusExGameEngine"),false);
    GC::Collect();SameGc(collisionGc,"Late native class staging rollback leaked domain/UClass allocation");
    NotPublished("Core.Package");NotPublished("Engine.GameEngine");
    Require(temporary.Snapshot("CollisionAfter")==collisionBefore&&GetPortableRuntimeWorldRevision()==revision&&WorldPaths().empty(),
        "Late native class staging failure published gameplay/index state");
    copy.Unchanged();ShutdownPortableRuntime();
}
} // namespace
int main(int argc,char** argv) {
    std::cout.setf(std::ios::unitbuf);
    try {
        fs::path root;
        if(argc==3&&std::string(argv[1])=="--game-root") root=argv[2];
        else if(argc==1) {const auto* configured=std::getenv("DEUSEX_GAME_ROOT");if(configured&&*configured) root=configured;}
        else throw std::runtime_error("Usage: portable_native_engine_classes_test [--game-root OWNED_GAME_ROOT] (or DEUSEX_GAME_ROOT)");
        if(root.empty()) {std::cout<<"SKIP owned native-engine UClass integration: use --game-root or DEUSEX_GAME_ROOT.\n";return 77;}
        root=fs::canonical(root);ShutdownPortableRuntime();GC::Collect();const auto baseline=GC::GetStats();Temporary temporary(root);
        std::vector<Source> sources;std::vector<PortablePackageTables> packages;
        sources.emplace_back(root/"System"/"DeusEx.ini");
        for(const char* name:{"Core","Engine","DeusEx","Extension","ConSys","IpDrv","IpServer","UBrowser","UWindow",
            "Editor","Fire","MPCharacters","DeusExSounds","DeusExDeco","DeusExItems","DeusExCharacters"}) {
            sources.emplace_back(root/"System"/(std::string(name)+".u"));packages.push_back(LoadPortablePackageTables(sources.back().path.string()));
        }
        sources.emplace_back(root/"Maps"/"00_Training.dx");const auto map=LoadPortablePackageTables(sources.back().path.string());
        Original(root,temporary,packages,map);AlternateConfigurations(root,temporary,packages);
        ShutdownPortableRuntime();GC::Collect();SameGc(baseline,"Native engine UClass roots survived explicit runtime shutdown");
        SnapshotIs(ReadPortableRuntimeNativeEngineClasses(),std::nullopt,false);
        Refused([&] {(void)PreparePortableRuntimeNativeEngineClasses();},"Native registration succeeded without initialized runtime");
        SameGc(baseline,"No-runtime preparation leaked native objects");
        for(const auto& source:sources) source.Unchanged();
        std::cout<<"PASS owned native engine UClass integration: "<<checks<<" checks, "<<refusals
            <<" refusals; exact five registration records/+6 GC graph, configured concrete variants, dormant Training checkpoint/clock/slots, "
              "warm restore/client/map/GC lifetime, unsupported/default/schema/index rollback and read-only original sources. "
              "Not Engine/Cylinder instance construction, startup, movement, rendering or headset evidence.\n";
        return 0;
    } catch(const std::exception& error) {std::cerr<<"FAIL: "<<error.what()<<'\n';ShutdownPortableRuntime();return 1;}
}
