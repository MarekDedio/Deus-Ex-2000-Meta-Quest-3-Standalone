#include "Precomp.h"
#include "GC/GC.h"
#include "portable_input_reset.h"
#include "portable_object_lookup.h"
#include "portable_unreal_runtime.h"
#include "portable_viewport_input.h"
#include "quest_native_config.h"
#include "quest_save_bundle.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

// Actual owned InputExt class/default/cache reset. Native544 pointer gates and
// complete startup/controller/gameplay are separate, explicitly unclaimed tests.
namespace {
namespace fs=std::filesystem;
using Bytes=std::vector<std::uint8_t>;
constexpr std::uint64_t Client=6201u,First=6202u,Second=6203u;
std::size_t checks{},refusals{};
void Require(bool value,const std::string& why) {++checks;if(!value)throw std::runtime_error(why);}
template<class Function> void Refuses(Function&& operation,const char* why) {
    bool refused{};try {operation();}catch(const std::exception&) {refused=true;}
    Require(refused,why);++refusals;
}
Bytes Read(const fs::path& path,std::size_t maximum=1u<<20u) {
    Bytes bytes;Require(QuestVr::ReadBoundedSaveFile(path.string(),maximum,bytes),"Cannot read bounded reset fixture: "+path.string());return bytes;
}
std::uint64_t Hash(const fs::path& path) {
    std::ifstream input(path,std::ios::binary);Require(bool(input),"Cannot fingerprint owned reset source");
    std::array<char,16384u> buffer{};std::uint64_t value=14695981039346656037ull;std::size_t bytes{};
    while(input) {input.read(buffer.data(),static_cast<std::streamsize>(buffer.size()));const auto count=input.gcount();
        bytes+=static_cast<std::size_t>(count);Require(bytes<=512u*1024u*1024u,"Owned reset source exceeds fingerprint bound");
        for(std::streamsize i=0;i<count;++i){value^=static_cast<unsigned char>(buffer[static_cast<std::size_t>(i)]);value*=1099511628211ull;}}
    Require(input.eof()&&!input.bad(),"Cannot finish reset source fingerprint");return value;
}
struct Source {
    fs::path path;std::uintmax_t bytes{};fs::file_time_type modified;std::uint64_t hash{};
    explicit Source(const fs::path& supplied):path(fs::canonical(supplied)),bytes(fs::file_size(path)),modified(fs::last_write_time(path)),hash(Hash(path)){}
    void Unchanged() const {Require(fs::file_size(path)==bytes&&fs::last_write_time(path)==modified&&Hash(path)==hash,
        "Reset changed immutable owned source bytes/size/mtime: "+path.string());}
};
bool Within(const fs::path& child,const fs::path& parent) {
    auto a=child.begin();for(auto b=parent.begin();b!=parent.end();++a,++b){if(a==child.end())return false;
        auto x=a->string(),y=b->string();for(auto& c:x)if(c>='A'&&c<='Z')c=static_cast<char>(c-'A'+'a');
        for(auto& c:y)if(c>='A'&&c<='Z')c=static_cast<char>(c-'A'+'a');
        if(x!=y)return false;
    }
    return true;
}
struct Temporary {
    fs::path parent,directory,application,config;
    explicit Temporary(const fs::path& original) {
        parent=fs::canonical(fs::temp_directory_path());Require(parent!=parent.root_path()&&!Within(parent,fs::canonical(original)),"Unsafe reset temporary parent");
        const auto stamp=std::chrono::steady_clock::now().time_since_epoch().count();
        for(unsigned suffix=0u;suffix<20u;++suffix){const auto path=parent/("quest-input-reset-"+std::to_string(stamp)+'-'+std::to_string(suffix));
            if(fs::create_directory(path)){directory=fs::canonical(path);break;}}
        Require(!directory.empty()&&directory.parent_path()==parent,"Cannot create isolated reset workspace");
        application=directory/"ApplicationData";Require(fs::create_directory(application),"Cannot create reset app-data root");config=application/"Config";
    }
    ~Temporary() {
        ShutdownPortableRuntime();
        const auto remove=[&](const fs::path& path){std::error_code error;const auto state=fs::symlink_status(path,error);
            if(!error&&fs::is_regular_file(state)&&!fs::is_symlink(state)&&(path.parent_path()==directory||path.parent_path()==config))fs::remove(path,error);};
        for(const auto* leaf:{"Before.sav","After.sav","DefUserCustom.ini","DefUserOther.ini"})remove(directory/leaf);
        for(const auto* leaf:{"User.ini","User.ini.stage","User.ini.lock"})remove(config/leaf);
        std::error_code error;if(!directory.empty()&&directory.parent_path()==parent){fs::remove(config,error);fs::remove(application,error);fs::remove(directory,error);}
    }
    Bytes Checkpoint(const char* leaf) const {const auto path=directory/leaf;
        Require((std::string(leaf)=="Before.sav"||std::string(leaf)=="After.sav")&&SavePortableRuntimeState(path.string()),"Cannot capture whole reset checkpoint");
        return Read(path,QuestVr::kMaximumSaveRuntimeBytes);}
    std::vector<std::string> Leaves() const {
        std::vector<std::string> result;for(const auto& entry:fs::directory_iterator(config)){
            Require(result.size()<16u&&entry.is_regular_file()&&!entry.is_symlink(),"Unsafe/unbounded owned reset Config entry");result.push_back(entry.path().filename().string());}
        std::sort(result.begin(),result.end());return result;
    }
};
QuestVr::Vm::Limits Limits(){QuestVr::Vm::Limits value;value.nativeWork=QuestVr::NativeWork::MaximumLimit;value.writes=65536u;return value;}
const PortableInputDefaultSnapshot& Default(const PortableInputSchemaSnapshot& schema,const char* name){
    const auto found=std::find_if(schema.defaults.begin(),schema.defaults.end(),[&](const auto& value){return value.classPath==name;});
    Require(found!=schema.defaults.end(),"Reset schema lacks actual selected/base CDO");return *found;
}
bool SameDefault(const PortableInputDefaultSnapshot& a,const PortableInputDefaultSnapshot& b) {
    return std::tie(a.classPath,a.superDefaultPath,a.friendlyName,a.classIndex,a.defaultIndex,a.superDefaultIndex,a.nativeSize,a.classFlags,
        a.propertyDimension,a.propertyElementSize,a.propertyFlags,a.propertyOffset,a.structPropertiesSize,a.hasInputValues,a.execInstalled,a.viewportIndex,a.enumIndex,a.aliases,a.bindings)==
        std::tie(b.classPath,b.superDefaultPath,b.friendlyName,b.classIndex,b.defaultIndex,b.superDefaultIndex,b.nativeSize,b.classFlags,
        b.propertyDimension,b.propertyElementSize,b.propertyFlags,b.propertyOffset,b.structPropertiesSize,b.hasInputValues,b.execInstalled,b.viewportIndex,b.enumIndex,b.aliases,b.bindings);
}
void SameInput(const PortableViewportInputSnapshot& a,const PortableViewportInputSnapshot& b,bool values=true,bool capacities=true) {
    Require(std::tie(a.prepared,a.constructed,a.initialized,a.execInstalled,a.extensionNamesPrepared,a.objectPath,a.classPath,a.outerPath,a.ownerViewportPath,a.enumPath,
        a.objectIndex,a.classIndex,a.outerIndex,a.ownerViewportIndex,a.enumIndex,a.nativeViewportIndex,a.objectFlags,a.clientIdentity,a.viewportIdentity,a.lookupGeneration,
        a.uniqueCounter,a.configReloads,a.configPasses,a.configImports,a.configFailures,a.configWarnings,a.configWork,a.extensionNames,a.extensionNameIndices)==
        std::tie(b.prepared,b.constructed,b.initialized,b.execInstalled,b.extensionNamesPrepared,b.objectPath,b.classPath,b.outerPath,b.ownerViewportPath,b.enumPath,
        b.objectIndex,b.classIndex,b.outerIndex,b.ownerViewportIndex,b.enumIndex,b.nativeViewportIndex,b.objectFlags,b.clientIdentity,b.viewportIdentity,b.lookupGeneration,
        b.uniqueCounter,b.configReloads,b.configPasses,b.configImports,b.configFailures,b.configWarnings,b.configWork,b.extensionNames,b.extensionNameIndices),
        "Reset changed Input identity/constructor counters/native ownership");
    if(values)Require((!capacities||a.retainedBytes==b.retainedBytes)&&a.bindings==b.bindings&&a.aliases==b.aliases,"Reset changed Input payloads or required allocation beforeimages");
}
bool SameStorage(const PortableUserConfigStorageSnapshot& a,const PortableUserConfigStorageSnapshot& b){
    return std::tie(a.prepared,a.owned,a.seedPath,a.targetPath,a.bytes,a.retainedBytes,a.generation,a.publication)==
        std::tie(b.prepared,b.owned,b.seedPath,b.targetPath,b.bytes,b.retainedBytes,b.generation,b.publication);
}
struct Fixture {
    const fs::path& root;const std::vector<PortablePackageTables>& packages;const PortablePackageTables& map;const Temporary& temporary;
    std::shared_ptr<QuestVr::NativeClientLifetime> lifetime;
    void Start(){Require(InitializePortableRuntime(packages).passed,"Cannot initialize owned reset runtime");
        Require(PreparePortableRuntimeNativeEngineClasses().prepared&&PreparePortableRuntimeNativeMetadataClasses().prepared&&
            PreparePortableRuntimeNativeConfigClasses().prepared&&PreparePortableRuntimeNativePackages().prepared&&
            PreparePortableRuntimeNativeInputClasses().prepared&&PreparePortableRuntimeObjectLookup().prepared&&
            PreparePortableRuntimeEngineCollision((root/"System"/"DeusEx.ini").string()).prepared&&PreparePortableRuntimeInputSchema().prepared,
            "Cannot prepare actual reset class/default/schema prerequisites");
        Require(BindPortableRuntimeUserConfigStorage(temporary.application.string(),(root/"System"/"User.ini").string()).prepared&&
            PreparePortableRuntimeInputConfig((root/"System"/"User.ini").string()).configApplied&&LoadPortableRuntimeMap(map).passed,
            "Cannot bind app-owned reset User cache/first Training map");
        lifetime=std::make_shared<QuestVr::NativeClientLifetime>();BindPortableRuntimeClient(lifetime,Client);
        for(const auto viewport:{First,Second}){RegisterPortableRuntimeViewport(Client,{viewport,1440u,1440u,3u,2u});
            const auto session=PreparePortableRuntimePlayerObjects(viewport);const auto input=PreparePortableRuntimeViewportInput(viewport,PortableInputLoggingProfile::Ordinary);
            Require(!session.bound&&input.classPath=="Extension.InputExt"&&input.constructed&&!input.initialized&&input.objectFlags==0x4000u,
                "Reset fixture fabricated Input.Init/player binding or wrong configured class");}}
    void Stop(){if(lifetime)lifetime->Revoke();ClearPortableRuntimeClient();lifetime.reset();ShutdownPortableRuntime();GC::Collect();}
    ~Fixture(){Stop();}
};
struct Baseline {
    const Temporary& temporary;Bytes save;PortableInputSchemaSnapshot schema;std::array<PortableViewportInputSnapshot,2u> inputs;
    PortableUserConfigStorageSnapshot storage;PortableObjectLookupState lookup;GCStats gc;std::uint64_t revision{};
    std::vector<std::string> leaves;std::optional<Bytes> file;std::optional<fs::file_time_type> modified;
    explicit Baseline(const Temporary& work):temporary(work),save(work.Checkpoint("Before.sav")),schema(ReadPortableRuntimeInputSchema()),
        inputs{ReadPortableRuntimeViewportInput(First),ReadPortableRuntimeViewportInput(Second)},storage(ReadPortableRuntimeUserConfigStorage()),
        lookup(ReadPortableRuntimeObjectLookup()),gc(GC::GetStats()),revision(GetPortableRuntimeWorldRevision()),leaves(work.Leaves()){
        if(fs::exists(storage.targetPath)){file=Read(storage.targetPath);modified=fs::last_write_time(storage.targetPath);}}
    void Unchanged(bool allowCapacityChange=false) const {
        const auto currentGc=GC::GetStats();Require(currentGc.numObjects==gc.numObjects&&currentGc.memoryUsage==gc.memoryUsage,"Reset refusal changed immediate GC ownership");
        SameInput(inputs[0u],ReadPortableRuntimeViewportInput(First),true,!allowCapacityChange);
        SameInput(inputs[1u],ReadPortableRuntimeViewportInput(Second),true,!allowCapacityChange);
        const auto current=ReadPortableRuntimeInputSchema();Require(std::tie(current.prepared,current.defaultsPrepared,current.configApplied,current.configCacheDirty,
            current.configSourcePath,current.configPasses,current.configImports,current.configFailures,current.configWarnings,current.configWork,current.keyEnumPath,
            current.keyEnumIndex,current.strPropertyUniqueCounter)==std::tie(schema.prepared,schema.defaultsPrepared,schema.configApplied,schema.configCacheDirty,
            schema.configSourcePath,schema.configPasses,schema.configImports,schema.configFailures,schema.configWarnings,schema.configWork,schema.keyEnumPath,
            schema.keyEnumIndex,schema.strPropertyUniqueCounter)&&current.defaults.size()==schema.defaults.size()&&
            (allowCapacityChange||current.retainedBytes==schema.retainedBytes),"Reset changed schema/config cache or required allocation beforeimages");
        for(std::size_t i=0u;i<schema.defaults.size();++i)Require(SameDefault(current.defaults[i],schema.defaults[i]),"Reset rollback changed actual CDO/index/value");
        const auto actual=ReadPortableRuntimeObjectLookup();Require(std::tie(actual.objects,actual.retainedBytes,actual.generation,actual.prepared)==
            std::tie(lookup.objects,lookup.retainedBytes,lookup.generation,lookup.prepared)&&SameStorage(storage,ReadPortableRuntimeUserConfigStorage())&&
            GetPortableRuntimeWorldRevision()==revision&&temporary.Checkpoint("After.sav")==save,"Reset rollback changed lookup/filegeneration/world/checkpoint");
        Require(temporary.Leaves()==leaves,"Reset without Flush created a stage/lock/target");
        if(file)Require(Read(storage.targetPath)==*file&&fs::last_write_time(storage.targetPath)==*modified,"Reset changed committed User bytes/mtime");
        else Require(!fs::exists(storage.targetPath),"Cache-only reset created User.ini");
    }
};
PortableInputResetResult Reset(PortableCoreExecMode mode=PortableCoreExecMode::RollbackOnly,const QuestVr::Vm::Limits& limits=Limits(),
    const PortableInputResetCommands& commands={}){return ExecutePortableRuntimeInputResetConfig("Extension.InputExt",mode,limits,commands);}
void Counts(const PortableInputResetResult& result,bool section,std::size_t overlays){
    Require(result.completed&&result.classPath=="Extension.InputExt"&&result.sectionPresent==section&&result.overlayCalls==overlays&&
        result.defaultReloads==1u&&result.instanceReloads==2u&&result.configPasses==6u&&!result.importFailures&&!result.importWarnings,
        "Actual selected-class ResetConfig omitted source overlay/InputExt-only CDO/two live reloads");}
void Set(const std::string& command){const auto result=ExecutePortableRuntimeCoreStaticExec(command,PortableCoreExecMode::Commit,Limits());
    Require(result.execution.passed()&&result.execution.committed&&result.handled&&result.flushRequested,"Cannot establish genuine prior SET/Flush");}
void Rollbacks(const Temporary& temporary,bool section,std::size_t overlays){
    const Baseline before(temporary);const auto result=Reset();Counts(result,section,overlays);
    Require(!result.execution.passed()&&!result.execution.committed&&result.execution.status==QuestVr::Vm::Status::Unsupported,
        "Rollback reset unexpectedly committed or refused before complete observations");before.Unchanged();
    const auto repeat=Reset();Counts(repeat,section,overlays);Require(repeat.execution.writes==result.execution.writes&&repeat.execution.error==result.execution.error,
        "Repeated whole-root ResetConfig rollback changed observations");before.Unchanged();
    Require(result.execution.writes>100u&&result.execution.nativeWork&&result.forwardNativeWork>100u&&
        result.execution.nativeWork->used>=result.forwardNativeWork,"Reset controls did not reach actual reload writes/work");
    for(const auto writes:{std::size_t{1u},std::size_t{100u},result.execution.writes-1u}){auto limits=Limits();limits.writes=writes;const auto cut=Reset(PortableCoreExecMode::Commit,limits);
        Require(cut.execution.status==QuestVr::Vm::Status::Budget&&!cut.execution.committed,"Late ResetConfig write refusal partially committed");++refusals;before.Unchanged();}
    for(const auto work:{std::size_t{1u},result.forwardNativeWork-1u}){auto limits=Limits();limits.nativeWork=work;const auto cut=Reset(PortableCoreExecMode::Commit,limits);
        Require(!cut.execution.passed()&&!cut.execution.committed&&cut.execution.nativeWork&&cut.execution.nativeWork->exhausted,
            "ResetConfig ignored shared native-work cutoff");++refusals;before.Unchanged();}
}
void Original(const fs::path& root,const std::vector<PortablePackageTables>& packages,const PortablePackageTables& map){
    Temporary work(root);Fixture fixture{root,packages,map,work,{}};fixture.Start();
    {const Baseline before(work);const auto missing=Reset(PortableCoreExecMode::Commit);Require(!missing.execution.passed()&&!missing.execution.committed,
        "Unprepared reset source was treated as an empty DefUser cache");++refusals;before.Unchanged();}
    Refuses([&]{static_cast<void>(PreparePortableRuntimeInputResetConfigSource((work.directory/"Missing.ini").string()));},"Missing reset source file was fabricated");
    Require(!ReadPortableRuntimeInputResetConfigSource().prepared,"Failed reset source preparation published authority");
    const auto path=root/"System"/"DefUser.ini";const auto prepared=PreparePortableRuntimeInputResetConfigSource(path.string());
    const auto again=PreparePortableRuntimeInputResetConfigSource(path.string());
    Require(prepared.prepared&&prepared.sourcePath==fs::absolute(path).lexically_normal().string()&&prepared.sourceBytes==fs::file_size(path)&&
        std::tie(prepared.sourcePath,prepared.sourceBytes,prepared.sections,prepared.entries,prepared.retainedBytes)==
        std::tie(again.sourcePath,again.sourceBytes,again.sections,again.entries,again.retainedBytes),"Pinned DefUser source preparation is not idempotent");
    Refuses([&]{static_cast<void>(PreparePortableRuntimeInputResetConfigSource((root/"System"/"User.ini").string()));},"Pinned DefUser source rebound to User alias");
    const auto document=QuestVr::NativeConfig::ReadOrderedIni(path);Require(document&&!document->StringArray("Extension.InputExt","F1"),"Owned DefUser fixture unexpectedly has InputExt section");
    for(const auto* command:{"SET InputExt T Talk","SET InputExt F5 ResetKeepFive","SET InputExt F6 ResetKeepSix","SET InputExt F7 ResetKeepSeven","SET InputExt F8 ResetKeepEight"})Set(command);
    Rollbacks(work,false,0u);const Baseline before(work);const auto result=Reset(PortableCoreExecMode::Commit);Counts(result,false,0u);
    Require(result.execution.passed()&&result.execution.committed&&!result.cacheChanged&&!result.cacheDirty&&!ReadPortableRuntimeInputSchema().configCacheDirty,
        "Absent InputExt reset section invented cache changes/Flush");
    // A successful COW/default FString reload preserves logical values, not
    // necessarily the old standard-library buffer capacities. Rollback calls
    // above still require exact retained-byte beforeimages by default.
    before.Unchanged(true);fixture.Stop();
}
void Modified(const fs::path& root,const std::vector<PortablePackageTables>& packages,const PortablePackageTables& map){
    Temporary work(root);const auto path=work.directory/"DefUserCustom.ini",other=work.directory/"DefUserOther.ini";
    auto bytes=Read(root/"System"/"DefUser.ini");const std::string extra="\r\n[Extension.InputExt]\r\nF1=First\r\nf1=ResetCache\r\nF1=resetcache\r\nF3=ResetTail=Value\r\n";
    bytes.insert(bytes.end(),extra.begin(),extra.end());Require(QuestVr::WriteDurableSaveFile(path.string(),bytes)&&QuestVr::WriteDurableSaveFile(other.string(),bytes),"Cannot create isolated modified DefUser seed");
    const Source source(path),alternate(other);Fixture fixture{root,packages,map,work,{}};fixture.Start();
    Require(PreparePortableRuntimeInputResetConfigSource(path.string()).prepared,"Cannot pin modified DefUser source");
    Refuses([&]{static_cast<void>(PreparePortableRuntimeInputResetConfigSource(other.string()));},"Same-byte different reset source replaced pinned identity");
    Rollbacks(work,true,4u);
    const auto base=Default(ReadPortableRuntimeInputSchema(),"Engine.Input");
    const std::array<PortableViewportInputSnapshot,2u> identities{ReadPortableRuntimeViewportInput(First),ReadPortableRuntimeViewportInput(Second)};
    const auto storage=ReadPortableRuntimeUserConfigStorage();const auto leaves=work.Leaves();const auto save=work.Checkpoint("Before.sav");
    const auto result=Reset(PortableCoreExecMode::Commit);Counts(result,true,4u);
    Require(result.execution.passed()&&result.execution.committed&&result.cacheChanged&&result.cacheDirty&&ReadPortableRuntimeInputSchema().configCacheDirty&&
        SameStorage(storage,ReadPortableRuntimeUserConfigStorage())&&work.Leaves()==leaves&&!fs::exists(storage.targetPath)&&work.Checkpoint("After.sav")==save,
        "Cache-only reset dirtied disk/publication/map state or lost pending cache dirtiness");
    const auto values=[&](bool dirty){const auto schema=ReadPortableRuntimeInputSchema();Require(schema.configCacheDirty==dirty&&SameDefault(Default(schema,"Engine.Input"),base)&&
            Default(schema,"Extension.InputExt").bindings[112u]=="ResetCache"&&Default(schema,"Extension.InputExt").bindings[114u]=="ResetTail=Value",
            "Reset failed newest-target/source-order duplicate/case/first-equals semantics or changed base CDO");
        for(std::size_t i=0u;i<identities.size();++i){const auto input=ReadPortableRuntimeViewportInput(i?Second:First);SameInput(identities[i],input,false);
            Require(input.bindings[112u]=="ResetCache"&&input.bindings[114u]=="ResetTail=Value","Actual instance reload did not read current reset cache");}};
    values(true);
    PortableInputResetCommands beforeCommand;beforeCommand.before="SET InputExt F1 BeforeFlush";
    const auto pending=Reset(PortableCoreExecMode::Commit,Limits(),beforeCommand);Counts(pending,true,4u);
    Require(pending.execution.passed()&&pending.execution.committed&&pending.beforeCommand.handled&&pending.beforeCommand.flushRequested&&pending.cacheDirty,
        "Same-root SET-before-reset failed original command/reset ordering");values(true);
    const auto disk=QuestVr::NativeConfig::ReadOrderedIni(storage.targetPath);Require(disk&&disk->Scalar("Extension.InputExt","F1")==std::optional<std::string_view>{"BeforeFlush"},
        "Commit implicitly flushed later reset cache over the earlier explicit Flush snapshot");
    PortableInputResetCommands afterCommand;afterCommand.after="SET InputExt F2 AfterFlush";
    const auto flushed=Reset(PortableCoreExecMode::Commit,Limits(),afterCommand);Counts(flushed,true,4u);
    Require(flushed.execution.passed()&&flushed.execution.committed&&flushed.afterCommand.handled&&flushed.afterCommand.flushRequested&&!flushed.cacheDirty,
        "SET after reset did not explicitly Flush dirty cache");values(false);
    const auto final=QuestVr::NativeConfig::ReadOrderedIni(storage.targetPath);Require(final&&
        final->Scalar("Extension.InputExt","F1")==std::optional<std::string_view>{"ResetCache"}&&
        final->Scalar("Extension.InputExt","F2")==std::optional<std::string_view>{"AfterFlush"}&&
        final->Scalar("Extension.InputExt","F3")==std::optional<std::string_view>{"ResetTail=Value"},"Explicit later Flush omitted dirty reset keys or new SET key");
    source.Unchanged();alternate.Unchanged();fixture.Stop();
}
}
int main(int argc,char** argv){std::cout.setf(std::ios::unitbuf);try{
    fs::path root;if(argc==3&&std::string(argv[1])=="--game-root")root=argv[2];else if(argc==1){if(const auto* value=std::getenv("DEUSEX_GAME_ROOT"))root=value;}
    else throw std::runtime_error("Usage: portable_input_reset_test [--game-root OWNED_GAME_ROOT]");
    if(root.empty()){std::cout<<"SKIP owned Input ResetConfig: supply --game-root or DEUSEX_GAME_ROOT\n";return 77;}
    root=fs::canonical(root);ShutdownPortableRuntime();GC::Collect();const auto initial=GC::GetStats();
    Refuses([&]{static_cast<void>(PreparePortableRuntimeInputResetConfigSource((root/"System"/"DefUser.ini").string()));},"Reset source prepared without runtime/schema phase");
    std::vector<Source> sources;std::vector<PortablePackageTables> packages;
    for(const auto* ini:{"DeusEx.ini","User.ini","DefUser.ini"})sources.emplace_back(root/"System"/ini);
    for(const auto* name:{"Core","Engine","DeusEx","Extension","ConSys","IpDrv","IpServer","UBrowser","UWindow","Editor","Fire","MPCharacters","DeusExSounds","DeusExDeco","DeusExItems","DeusExCharacters"}){
        sources.emplace_back(root/"System"/(std::string(name)+".u"));packages.push_back(LoadPortablePackageTables(sources.back().path.string()));}
    sources.emplace_back(root/"Maps"/"00_Training.dx");const auto map=LoadPortablePackageTables(sources.back().path.string());
    Original(root,packages,map);Modified(root,packages,map);GC::Collect();const auto final=GC::GetStats();
    Require(final.numObjects==initial.numObjects&&final.memoryUsage==initial.memoryUsage,"Reset shutdown retained native source/default/Input/client GC ownership");
    for(const auto& source:sources)source.Unchanged();
    std::cout<<"PASS owned InputExt ResetConfig: "<<checks<<" checks / "<<refusals<<" refusals; actual CDO-first/two-instance reload, five prior SET bindings, "
        "pinned separate DefUser source, duplicate/case cache overlay, pending Flush ordering and whole-root/file/GC rollback. NOT native544 gate, Input.Init/ReadInput, startup or live Quest gameplay\n";return 0;
}catch(const std::exception& error){std::cerr<<"FAIL: "<<error.what()<<'\n';ShutdownPortableRuntime();return 1;}}
