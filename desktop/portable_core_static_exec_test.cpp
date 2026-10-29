#include "Precomp.h"
#include "GC/GC.h"
#include "portable_core_static_exec.h"
#include "portable_object_lookup.h"
#include "portable_unreal_runtime.h"
#include "portable_viewport_input.h"
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

// Actual Core.StaticExec/Input/config service only. No viewport FExec shortcut,
// Input.Init/ReadInput, begun player/world or live Quest physics is inferred.
namespace {
namespace fs=std::filesystem;
using Bytes=std::vector<std::uint8_t>;
using QuestVr::Vm::Kind;
constexpr std::uint64_t Client=5201u,Viewport=5202u,OtherViewport=5203u;
std::size_t checks{},refusals{};
void Require(bool value,const std::string& why) {++checks;if(!value)throw std::runtime_error(why);}

Bytes Read(const fs::path& path,std::size_t maximum=1u<<20u) {
    Bytes result;Require(QuestVr::ReadBoundedSaveFile(path.string(),maximum,result),"Cannot read bounded Core SET fixture: "+path.string());
    return result;
}
std::uint64_t Hash(const fs::path& path) {
    std::ifstream input(path,std::ios::binary);Require(bool(input),"Cannot fingerprint owned source: "+path.string());
    std::array<char,16384u> buffer{};std::uint64_t hash=14695981039346656037ull;std::uintmax_t bytes{};
    while(input) {
        input.read(buffer.data(),static_cast<std::streamsize>(buffer.size()));const auto count=input.gcount();
        bytes+=static_cast<std::uintmax_t>(count);Require(bytes<=512u*1024u*1024u,"Owned source exceeds fingerprint bound");
        for(std::streamsize i=0;i<count;++i) {hash^=static_cast<unsigned char>(buffer[static_cast<std::size_t>(i)]);hash*=1099511628211ull;}
    }
    Require(input.eof()&&!input.bad(),"Cannot finish owned source fingerprint");return hash;
}
struct Source {
    fs::path path;std::uintmax_t bytes{};fs::file_time_type modified;std::uint64_t hash{};
    explicit Source(const fs::path& supplied):path(fs::canonical(supplied)),bytes(fs::file_size(path)),modified(fs::last_write_time(path)),hash(Hash(path)) {}
    void Unchanged() const {
        Require(fs::file_size(path)==bytes&&fs::last_write_time(path)==modified&&Hash(path)==hash,
            "Core SET changed immutable owned source bytes/size/mtime: "+path.string());
    }
};
bool Within(const fs::path& child,const fs::path& parent) {
    auto a=child.begin();for(auto b=parent.begin();b!=parent.end();++a,++b) {
        if(a==child.end())return false;
        auto x=a->string(),y=b->string();
        for(auto& c:x)if(c>='A'&&c<='Z')c=static_cast<char>(c-'A'+'a');
        for(auto& c:y)if(c>='A'&&c<='Z')c=static_cast<char>(c-'A'+'a');
        if(x!=y)return false;
    }
    return true;
}
struct Temporary {
    fs::path parent,directory,application,config;
    explicit Temporary(const fs::path& original) {
        parent=fs::canonical(fs::temp_directory_path());
        Require(parent!=parent.root_path()&&!Within(parent,fs::canonical(original)),"Core SET temporary scope overlaps original installation/root");
        const auto stamp=std::chrono::steady_clock::now().time_since_epoch().count();
        for(unsigned suffix=0u;suffix<20u;++suffix) {
            const auto candidate=parent/("quest-core-static-exec-"+std::to_string(stamp)+'-'+std::to_string(suffix));
            if(fs::create_directory(candidate)) {directory=fs::canonical(candidate);break;}
        }
        Require(!directory.empty()&&directory.parent_path()==parent,"Cannot create isolated Core SET directory");
        application=directory/"ApplicationData";Require(fs::create_directory(application),"Cannot create explicit app-owned data directory");
        application=fs::canonical(application);config=application/"Config";
    }
    ~Temporary() {
        ShutdownPortableRuntime();
        // Only the exact created workspace and fixed known leaves are cleaned.
        // No recursive removal, wildcard deletion or unresolved target is used.
        const auto remove=[&](const fs::path& path) {
            std::error_code error;const auto state=fs::symlink_status(path,error);
            if(error||!fs::is_regular_file(state)||fs::is_symlink(state))return;
            if(path.parent_path()!=directory&&path.parent_path()!=config)return;
            fs::remove(path,error);
        };
        remove(directory/"Before.sav");remove(directory/"After.sav");
        remove(config/"User.ini");remove(config/"User.ini.stage");remove(config/"User.ini.lock");
        std::error_code error;
        if(!directory.empty()&&directory.parent_path()==parent) {
            if(config.parent_path()==application&&application.parent_path()==directory) {
                fs::remove(config,error);fs::remove(application,error);
            }
            fs::remove(directory,error);
        }
    }
    Bytes Checkpoint(const char* leaf) const {
        Require(std::string(leaf)=="Before.sav"||std::string(leaf)=="After.sav","Unexpected Core SET checkpoint leaf");
        const auto path=directory/leaf;Require(path.parent_path()==directory&&SavePortableRuntimeState(path.string()),"Cannot save Core SET whole-runtime checkpoint");
        return Read(path,QuestVr::kMaximumSaveRuntimeBytes);
    }
    std::vector<std::string> ConfigLeaves() const {
        Require(fs::is_directory(config)&&!fs::is_symlink(fs::symlink_status(config)),"Owned Config directory is absent or unsafe");
        std::vector<std::string> result;
        for(const auto& entry:fs::directory_iterator(config)) {
            Require(result.size()<16u&&entry.path().parent_path()==config&&!entry.is_symlink()&&entry.is_regular_file(),
                "Owned Config contains an unexpected unsafe/unbounded entry");
            result.push_back(entry.path().filename().string());
        }
        std::sort(result.begin(),result.end());return result;
    }
};

QuestVr::Vm::Limits Limits() {
    QuestVr::Vm::Limits limits;limits.nativeWork=QuestVr::NativeWork::MaximumLimit;
    // Diagnostic root allowance only, not a change to live/default limits.
    limits.writes=65536u;return limits;
}
const PortableInputDefaultSnapshot& Default(const PortableInputSchemaSnapshot& schema,const char* name) {
    const auto found=std::find_if(schema.defaults.begin(),schema.defaults.end(),[&](const auto& value){return value.classPath==name;});
    Require(found!=schema.defaults.end(),"Core SET schema lacks actual Input CDO");return *found;
}
void SameInput(const PortableViewportInputSnapshot& a,const PortableViewportInputSnapshot& b) {
    Require(a.prepared==b.prepared&&a.constructed==b.constructed&&a.initialized==b.initialized&&a.execInstalled==b.execInstalled&&
        a.extensionNamesPrepared==b.extensionNamesPrepared&&a.objectPath==b.objectPath&&a.classPath==b.classPath&&a.outerPath==b.outerPath&&
        a.ownerViewportPath==b.ownerViewportPath&&a.enumPath==b.enumPath&&a.objectIndex==b.objectIndex&&a.classIndex==b.classIndex&&
        a.outerIndex==b.outerIndex&&a.ownerViewportIndex==b.ownerViewportIndex&&a.enumIndex==b.enumIndex&&a.nativeViewportIndex==b.nativeViewportIndex&&
        a.objectFlags==b.objectFlags&&a.clientIdentity==b.clientIdentity&&a.viewportIdentity==b.viewportIdentity&&a.lookupGeneration==b.lookupGeneration&&
        a.uniqueCounter==b.uniqueCounter&&a.retainedBytes==b.retainedBytes&&a.configReloads==b.configReloads&&a.configPasses==b.configPasses&&
        a.configImports==b.configImports&&a.configFailures==b.configFailures&&a.configWarnings==b.configWarnings&&a.configWork==b.configWork&&
        a.aliases==b.aliases&&a.bindings==b.bindings&&a.extensionNames==b.extensionNames&&a.extensionNameIndices==b.extensionNameIndices,
        "Core SET rollback changed an Input payload/identity/bootstrap/lifetime snapshot");
}
void SameSchema(const PortableInputSchemaSnapshot& a,const PortableInputSchemaSnapshot& b) {
    Require(a.prepared==b.prepared&&a.defaultsPrepared==b.defaultsPrepared&&a.configApplied==b.configApplied&&a.configCacheDirty==b.configCacheDirty&&a.configSourcePath==b.configSourcePath&&
        a.configPasses==b.configPasses&&a.configImports==b.configImports&&a.configFailures==b.configFailures&&a.configWarnings==b.configWarnings&&
        a.configWork==b.configWork&&a.keyEnumPath==b.keyEnumPath&&a.keyEnumIndex==b.keyEnumIndex&&a.retainedBytes==b.retainedBytes&&
        a.strPropertyUniqueCounter==b.strPropertyUniqueCounter&&a.defaults.size()==b.defaults.size()&&a.fields.size()==b.fields.size()&&a.links.size()==b.links.size(),
        "Core SET rollback changed native schema/config-bootstrap ownership");
    for(std::size_t i=0u;i<a.defaults.size();++i) {
        const auto& x=a.defaults[i];const auto& y=b.defaults[i];
        Require(x.classPath==y.classPath&&x.superDefaultPath==y.superDefaultPath&&x.classIndex==y.classIndex&&x.defaultIndex==y.defaultIndex&&
            x.superDefaultIndex==y.superDefaultIndex&&x.nativeSize==y.nativeSize&&x.classFlags==y.classFlags&&x.hasInputValues==y.hasInputValues&&
            x.execInstalled==y.execInstalled&&x.viewportIndex==y.viewportIndex&&x.enumIndex==y.enumIndex&&x.aliases==y.aliases&&x.bindings==y.bindings,
            "Core SET rollback changed actual Input CDO identity or typed values");
    }
}
bool SameStorage(const PortableUserConfigStorageSnapshot& a,const PortableUserConfigStorageSnapshot& b) {
    return a.prepared==b.prepared&&a.owned==b.owned&&a.seedPath==b.seedPath&&a.targetPath==b.targetPath&&a.bytes==b.bytes&&
        a.retainedBytes==b.retainedBytes&&a.generation==b.generation&&a.publication==b.publication;
}
struct Fixture {
    const fs::path& root;const std::vector<PortablePackageTables>& packages;const PortablePackageTables& map;const Temporary& temporary;
    std::shared_ptr<QuestVr::NativeClientLifetime> lifetime;
    void Start() {
        Require(InitializePortableRuntime(packages).passed,"Cannot initialize Core SET owned source graph");
        Require(PreparePortableRuntimeNativeEngineClasses().prepared&&PreparePortableRuntimeNativeMetadataClasses().prepared&&
            PreparePortableRuntimeNativeConfigClasses().prepared&&PreparePortableRuntimeNativePackages().prepared&&
            PreparePortableRuntimeNativeInputClasses().prepared&&PreparePortableRuntimeObjectLookup().prepared&&
            PreparePortableRuntimeEngineCollision((root/"System"/"DeusEx.ini").string()).prepared&&PreparePortableRuntimeInputSchema().prepared,
            "Cannot prepare actual Core SET/Input schema prerequisites");
        const auto binding=BindPortableRuntimeUserConfigStorage(temporary.application.string(),(root/"System"/"User.ini").string());
        Require(binding.prepared&&fs::path(binding.targetPath)==temporary.config/"User.ini"&&binding.seedPath!=binding.targetPath,
            "Core SET writable authority overlaps seed or ignores explicit app scope");
        Require(PreparePortableRuntimeInputConfig((root/"System"/"User.ini").string()).configApplied,"Cannot bind configured native Input defaults");
        Require(LoadPortableRuntimeMap(map).passed,"Cannot load first Training for Core SET integration");
        lifetime=std::make_shared<QuestVr::NativeClientLifetime>();BindPortableRuntimeClient(lifetime,Client);
        for(const auto viewport:{Viewport,OtherViewport}) {
            RegisterPortableRuntimeViewport(Client,{viewport,1440u,1440u,3u,2u});
            const auto session=PreparePortableRuntimePlayerObjects(viewport);
            const auto input=PreparePortableRuntimeViewportInput(viewport,PortableInputLoggingProfile::Ordinary);
            Require(!session.bound&&input.classPath=="Extension.InputExt"&&input.prepared&&input.constructed&&!input.initialized&&
                input.objectFlags==0x4000u&&input.ownerViewportPath==session.viewportPath,"Core SET fixture fabricated Init/player binding or wrong Input authority");
        }
    }
    void Stop() {if(lifetime)lifetime->Revoke();ClearPortableRuntimeClient();lifetime.reset();ShutdownPortableRuntime();GC::Collect();}
    ~Fixture() {Stop();}
};
struct Baseline {
    const Temporary& temporary;Bytes save;PortableViewportInputSnapshot first,second;PortableInputSchemaSnapshot schema;
    PortableUserConfigStorageSnapshot storage;PortableObjectLookupState lookup;GCStats gc;std::uint64_t revision{};
    std::vector<std::string> leaves;std::optional<Bytes> target;std::optional<fs::file_time_type> targetTime;
    explicit Baseline(const Temporary& work):temporary(work),save(work.Checkpoint("Before.sav")),first(ReadPortableRuntimeViewportInput(Viewport)),
        second(ReadPortableRuntimeViewportInput(OtherViewport)),schema(ReadPortableRuntimeInputSchema()),storage(ReadPortableRuntimeUserConfigStorage()),
        lookup(ReadPortableRuntimeObjectLookup()),gc(GC::GetStats()),revision(GetPortableRuntimeWorldRevision()),leaves(work.ConfigLeaves()) {
        if(fs::exists(storage.targetPath)) {target=Read(storage.targetPath);targetTime=fs::last_write_time(storage.targetPath);}
    }
    void Unchanged() const {
        SameInput(first,ReadPortableRuntimeViewportInput(Viewport));SameInput(second,ReadPortableRuntimeViewportInput(OtherViewport));
        SameSchema(schema,ReadPortableRuntimeInputSchema());Require(SameStorage(storage,ReadPortableRuntimeUserConfigStorage()),"Core SET rollback changed config storage/cache generation");
        const auto current=ReadPortableRuntimeObjectLookup();GC::Collect();const auto actual=GC::GetStats();
        Require(current.prepared==lookup.prepared&&current.objects==lookup.objects&&current.generation==lookup.generation&&current.retainedBytes==lookup.retainedBytes&&
            actual.numObjects==gc.numObjects&&actual.memoryUsage==gc.memoryUsage&&GetPortableRuntimeWorldRevision()==revision&&
            temporary.Checkpoint("After.sav")==save,"Core SET rollback changed lookup/GC/world/save state");
        Require(temporary.ConfigLeaves()==leaves,"Rollback-only Core SET created a disk stage/lock/file");
        if(target)Require(Read(storage.targetPath)==*target&&fs::last_write_time(storage.targetPath)==*targetTime,"Core SET rollback changed committed config bytes/mtime");
        else Require(!fs::exists(storage.targetPath),"Rollback-only SET physically created User.ini");
    }
};
void Counts(const PortableCoreExecResult& result) {
    Require(result.handled&&result.flushRequested&&result.instanceImports==2u&&result.defaultImports==1u&&result.configExports==295u&&
        result.defaultReloads==2u,"Core SET omitted instance fanout/CDO import/all config exports/ancestor CDO reload");
}
PortableCoreExecResult Command(const std::string& command,PortableCoreExecMode mode=PortableCoreExecMode::RollbackOnly,
    const QuestVr::Vm::Limits& limits=Limits()) {
    return ExecutePortableRuntimeCoreStaticExec(command,mode,limits);
}
void RollbackControls(Temporary& temporary) {
    const Baseline before(temporary);
    const auto probed=Command("SET InputExt T Talk");Counts(probed);
    Require(!probed.execution.committed&&probed.execution.status==QuestVr::Vm::Status::Unsupported&&
        probed.execution.error=="Non-publishing Core static exec probe completed; rollback required"&&probed.cacheChanged&&
        probed.publication==PortableConfigPublication::None&&probed.output.empty()&&!probed.importFailures&&!probed.importWarnings,
        "Non-publishing SET probe failed before complete mutation/export/reload observations or published disk state");
    before.Unchanged();
    const auto repeat=Command("SET InputExt T Talk");Counts(repeat);
    Require(repeat.execution.writes==probed.execution.writes&&repeat.execution.error==probed.execution.error,"Rollback did not reproduce original SET work/writes");before.Unchanged();
    Require(probed.execution.writes>200u,"Core SET regression did not reach its actual complete reload stores");
    for(const auto prefix:{1u,5u,10u,200u,static_cast<unsigned>(probed.execution.writes-1u)}) {
        auto limits=Limits();limits.writes=prefix;const auto failed=Command("SET InputExt T Talk",PortableCoreExecMode::Commit,limits);
        Require(failed.execution.status==QuestVr::Vm::Status::Budget&&!failed.execution.committed&&failed.publication==PortableConfigPublication::None,
            "Core SET late write refusal swallowed budget failure or published partial config");++refusals;before.Unchanged();
    }
    {auto limits=Limits();limits.nativeWork=1u;const auto failed=Command("SET InputExt T Talk",PortableCoreExecMode::Commit,limits);
        Require(!failed.execution.passed()&&!failed.execution.committed&&failed.publication==PortableConfigPublication::None,
            "Core SET ignored shared native-work budget");++refusals;before.Unchanged();}
    std::cout<<"OWNED Core SET rollback writes="<<probed.execution.writes<<" instances="<<probed.instanceImports<<
        " exports="<<probed.configExports<<" CDOreloads="<<probed.defaultReloads<<"; no disk stage/lock\n";
}
void Warnings(Temporary& temporary) {
    const Baseline before(temporary);
    for(const auto& [command,handled,output]:std::vector<std::tuple<std::string,bool,std::string>>{
        {"NOTSET InputExt T Talk",false,""},{"SET NoSuchCoreSetClass T Talk",true,"Unrecognized class NoSuchCoreSetClass"},
        {"SET",true,"Unrecognized class "},{"SET InputExt NoSuchCoreSetProperty Talk",true,"Unrecognized property NoSuchCoreSetProperty"},
        {"SET InputExt",true,"Unrecognized property "},{"SET InputExt Aliases[3] Value",true,"Unrecognized property Aliases[3]"}}) {
        const auto result=Command(command,PortableCoreExecMode::Commit);
        Require(result.execution.passed()&&result.execution.committed&&result.handled==handled&&result.output==output&&!result.flushRequested&&
            !result.cacheChanged&&result.publication==PortableConfigPublication::None&&!result.instanceImports&&!result.defaultImports&&
            !result.defaultReloads&&!result.configExports,"Actual SET miss/decline changed handled/output or reached storage mutation");
        before.Unchanged();
    }
}
void CommitAndCold(Fixture& fixture,Temporary& temporary,const Source& seed) {
    const auto beforeSchema=ReadPortableRuntimeInputSchema();const auto beforeFirst=ReadPortableRuntimeViewportInput(Viewport);
    const auto beforeSecond=ReadPortableRuntimeViewportInput(OtherViewport);const auto save=temporary.Checkpoint("Before.sav");
    const auto revision=GetPortableRuntimeWorldRevision();const auto beforeStorage=ReadPortableRuntimeUserConfigStorage();
    const auto committed=Command("SET InputExt T Talk",PortableCoreExecMode::Commit);Counts(committed);
    Require(committed.execution.passed()&&committed.execution.committed&&committed.cacheChanged&&committed.publication!=PortableConfigPublication::None,
        "Actual Core SET did not persist its complete app-owned config publication");
    const auto first=ReadPortableRuntimeViewportInput(Viewport),second=ReadPortableRuntimeViewportInput(OtherViewport);
    Require(first.bindings[84u]=="Talk"&&second.bindings[84u]=="Talk"&&first.objectIndex==beforeFirst.objectIndex&&
        second.objectIndex==beforeSecond.objectIndex&&first.configReloads==beforeFirst.configReloads&&first.configPasses==beforeFirst.configPasses&&
        second.configReloads==beforeSecond.configReloads&&second.configPasses==beforeSecond.configPasses,
        "SET failed actual indexed Input fanout or corrupted constructor bootstrap evidence");
    const auto schema=ReadPortableRuntimeInputSchema();
    Require(Default(schema,"Extension.InputExt").bindings[84u]=="Talk"&&
        Default(schema,"Engine.Input").bindings[84u]==Default(beforeSchema,"Engine.Input").bindings[84u]&&schema.configPasses==beforeSchema.configPasses&&
        schema.configImports==beforeSchema.configImports&&schema.configWork==beforeSchema.configWork,
        "SET omitted target CDO/reload or overwrote base/bootstrap config state");
    const auto storage=ReadPortableRuntimeUserConfigStorage();
    Require(storage.prepared&&storage.owned&&storage.targetPath==beforeStorage.targetPath&&storage.generation>beforeStorage.generation&&
        storage.publication==committed.publication&&fs::is_regular_file(storage.targetPath)&&!fs::equivalent(storage.targetPath,seed.path),
        "Committed config was not a distinct owned target/generation");
    Require(!fs::exists(temporary.config/"User.ini.stage")&&GetPortableRuntimeWorldRevision()==revision&&temporary.Checkpoint("After.sav")==save,
        "Config SET contaminated map state or left a stage file");seed.Unchanged();
    const auto contents=Read(storage.targetPath);const auto modified=fs::last_write_time(storage.targetPath);
    const auto same=Command("SET InputExt T talk",PortableCoreExecMode::Commit);Counts(same);
    Require(same.execution.passed()&&same.execution.committed&&!same.cacheChanged&&same.publication==PortableConfigPublication::None&&
        Read(storage.targetPath)==contents&&fs::last_write_time(storage.targetPath)==modified&&
        ReadPortableRuntimeUserConfigStorage().generation==storage.generation,
        "Case-only SetString rewrote old cache spelling or physically flushed unchanged config");
    // Live imported values use the incoming spelling; CDO reload observes the
    // unchanged case-insensitive cache, so instances and CDO can differ here.
    Require(ReadPortableRuntimeViewportInput(Viewport).bindings[84u]=="talk"&&ReadPortableRuntimeViewportInput(OtherViewport).bindings[84u]=="talk"&&
        Default(ReadPortableRuntimeInputSchema(),"Extension.InputExt").bindings[84u]=="Talk",
        "SET did not preserve live-import versus cached-CDO case-only semantics");
    const auto originalAlias=ReadPortableRuntimeViewportInput(Viewport).aliases[0u];
    const auto aliases=Command("SET InputExt Aliases (Alias=Jump,Command=\"Core SET alias\")",PortableCoreExecMode::Commit);Counts(aliases);
    Require(aliases.execution.passed()&&aliases.execution.committed&&!aliases.importFailures&&!aliases.importWarnings,
        "Native Alias flags1/nested3 import failed");
    for(const auto viewport:{Viewport,OtherViewport}) {
        const auto input=ReadPortableRuntimeViewportInput(viewport);
        Require(input.aliases[0u].alias=="Jump"&&input.aliases[0u].command=="Core SET alias"&&
            input.aliases[1u]==beforeFirst.aliases[1u],"Bare Aliases did not mutate exactly slot0 through actual member stores");
    }
    Require(originalAlias!=ReadPortableRuntimeViewportInput(Viewport).aliases[0u],"Alias fixture made no genuine mutation");
    const auto malformed=Command("SET InputExt Aliases (Alias=Fire,Command=\"unterminated)",PortableCoreExecMode::Commit);Counts(malformed);
    Require(malformed.execution.passed()&&malformed.execution.committed&&malformed.importFailures==3u&&malformed.importWarnings==0u&&
        ReadPortableRuntimeViewportInput(Viewport).aliases[0u].alias=="Fire"&&
        ReadPortableRuntimeViewportInput(OtherViewport).aliases[0u].alias=="Fire"&&
        ReadPortableRuntimeViewportInput(Viewport).aliases[0u].command=="Core SET alias",
        "Malformed Alias import aborted ordinary prefix stores/save or invented a string result");
    const auto finalSchema=ReadPortableRuntimeInputSchema();const auto finalBytes=Read(storage.targetPath);seed.Unchanged();
    fixture.Stop();Require(!HasPortableRuntimeClient(),"Cold Core SET reopen retained prior viewport lease");fixture.Start();
    const auto reopened=ReadPortableRuntimeUserConfigStorage();
    Require(reopened.prepared&&reopened.owned&&Read(reopened.targetPath)==finalBytes,
        "Cold app-owned config binding ignored committed target bytes");
    const auto coldSchema=ReadPortableRuntimeInputSchema();
    Require(Default(coldSchema,"Extension.InputExt").bindings==Default(finalSchema,"Extension.InputExt").bindings&&
        Default(coldSchema,"Extension.InputExt").aliases==Default(finalSchema,"Extension.InputExt").aliases,
        "Cold original CDO reload lost persisted Config fields");
    for(const auto viewport:{Viewport,OtherViewport}) {
        const auto input=ReadPortableRuntimeViewportInput(viewport);
        Require(input.bindings==Default(coldSchema,"Extension.InputExt").bindings&&input.aliases==Default(coldSchema,"Extension.InputExt").aliases&&
            !input.initialized&&!ReadPortableRuntimePlayerSession(viewport).bound,"Cold committed settings failed actual Input construction or fabricated live player services");
    }
    seed.Unchanged();
}
} // namespace

int main(int argc,char** argv) {
    std::cout.setf(std::ios::unitbuf);
    try {
        fs::path root;if(argc==3&&std::string(argv[1])=="--game-root")root=argv[2];
        else if(argc==1) {if(const auto* value=std::getenv("DEUSEX_GAME_ROOT"))root=value;}
        else throw std::runtime_error("Usage: portable_core_static_exec_test [--game-root OWNED_GAME_ROOT]");
        if(root.empty()) {std::cout<<"SKIP owned Core.StaticExec integration: supply --game-root or DEUSEX_GAME_ROOT\n";return 77;}
        root=fs::canonical(root);ShutdownPortableRuntime();GC::Collect();const auto initial=GC::GetStats();Temporary temporary(root);
        std::vector<Source> sources;std::vector<PortablePackageTables> packages;
        sources.emplace_back(root/"System"/"DeusEx.ini");sources.emplace_back(root/"System"/"User.ini");const auto seed=sources.back();
        for(const auto* name:{"Core","Engine","DeusEx","Extension","ConSys","IpDrv","IpServer","UBrowser","UWindow","Editor","Fire", "MPCharacters","DeusExSounds","DeusExDeco","DeusExItems","DeusExCharacters"}) {
            sources.emplace_back(root/"System"/(std::string(name)+".u"));packages.push_back(LoadPortablePackageTables(sources.back().path.string()));
        }
        sources.emplace_back(root/"Maps"/"00_Training.dx");const auto map=LoadPortablePackageTables(sources.back().path.string());
        Fixture fixture{root,packages,map,temporary,{}};fixture.Start();
        Require(!ReadPortableRuntimeUserConfigStorage().owned&&!fs::exists(temporary.config/"User.ini"),"Initial Core SET seed binding physically wrote config");
        Warnings(temporary);RollbackControls(temporary);CommitAndCold(fixture,temporary,seed);fixture.Stop();
        GC::Collect();const auto final=GC::GetStats();Require(final.numObjects==initial.numObjects&&final.memoryUsage==initial.memoryUsage,
            "Core SET shutdown retained native config/default/Input/session GC ownership");
        for(const auto& source:sources)source.Unchanged();
        std::cout<<"PASS owned Core.StaticExec SET: "<<checks<<" checks / "<<refusals<<
            " refusals; real indexed two-Input fanout, target CDO, all295 exports, ancestor reload, app-owned cold persistence, "
            "malformed Alias prefix, case-only cache semantics and whole-root/save/GC/file rollback. No viewport FExec, Input.Init/ReadInput, "
            "begun player startup, live Quest crates or original DLL execution claim\n";return 0;
    } catch(const std::exception& error) {std::cerr<<"FAIL: "<<error.what()<<'\n';ShutdownPortableRuntime();return 1;}
}
