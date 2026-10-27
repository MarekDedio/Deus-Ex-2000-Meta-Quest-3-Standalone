#include "Precomp.h"
#include "GC/GC.h"
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
#include <vector>

// Genuine owned native constructor/key-query service, not a fake PlayerPawn,
// native Init/Reset/ReadInput, successful startup or live controller gameplay.
// Owned packages/config are read only; no commercial executable is invoked.
namespace {
namespace fs=std::filesystem;
using Bytes=std::vector<std::uint8_t>;
constexpr auto Ordinary=PortableInputLoggingProfile::Ordinary;
std::size_t checks{},refusals{};
void Require(bool value,const std::string& why) {++checks;if(!value)throw std::runtime_error(why);}
template<class F>void Refused(F&& operation,const std::string& why) {
    bool refused{};try {operation();}catch(const std::exception&) {refused=true;}Require(refused,why);++refusals;
}
std::string Fold(std::string value) {
    for(auto& c:value)if(c>='A'&&c<='Z')c=static_cast<char>(c-'A'+'a');
    return value;
}
bool Within(const fs::path& child,const fs::path& parent) {
    auto a=child.begin();for(auto b=parent.begin();b!=parent.end();++a,++b)
        if(a==child.end()||Fold(a->string())!=Fold(b->string()))return false;
    return true;
}
std::uint64_t Hash(const fs::path& path) {
    std::ifstream file(path,std::ios::binary);Require(bool(file),"Cannot fingerprint Input source: "+path.string());
    std::array<char,16384u> buffer{};std::uint64_t hash=14695981039346656037ull;std::uintmax_t count{};
    while(file) {file.read(buffer.data(),static_cast<std::streamsize>(buffer.size()));const auto size=file.gcount();
        count+=static_cast<std::uintmax_t>(size);Require(count<=512u*1024u*1024u,"Input source fingerprint exceeds bound");
        for(std::streamsize i=0;i<size;++i){hash^=static_cast<unsigned char>(buffer[static_cast<std::size_t>(i)]);hash*=1099511628211ull;}}
    Require(file.eof()&&!file.bad(),"Cannot finish Input source fingerprint");return hash;
}
struct Source {
    fs::path path;std::uintmax_t size{};fs::file_time_type modified;std::uint64_t hash{};
    explicit Source(const fs::path& value):path(fs::canonical(value)),size(fs::file_size(path)),modified(fs::last_write_time(path)),hash(Hash(path)) {}
    void Unchanged() const {Require(fs::file_size(path)==size&&fs::last_write_time(path)==modified&&Hash(path)==hash,
        "Owned Input source bytes/size/mtime changed: "+path.string());}
};
struct Temporary {
    fs::path parent,directory;std::vector<fs::path> files;
    explicit Temporary(const fs::path& root) {
        parent=fs::canonical(fs::temp_directory_path());Require(parent!=parent.root_path()&&!Within(parent,fs::canonical(root)),"Temporary Input fixture overlaps original/root");
        const auto stamp=std::chrono::steady_clock::now().time_since_epoch().count();
        for(unsigned i=0;i<20u;++i){const auto candidate=parent/("quest-viewport-input-"+std::to_string(stamp)+'-'+std::to_string(i));
            if(fs::create_directory(candidate)){directory=fs::canonical(candidate);break;}}
        Require(!directory.empty()&&directory.parent_path()==parent,"Cannot create isolated viewport Input directory");
    }
    ~Temporary(){ShutdownPortableRuntime();std::error_code error;for(const auto& file:files)if(file.parent_path()==directory)fs::remove(file,error);
        if(!directory.empty()&&directory.parent_path()==parent)fs::remove(directory,error);}
    fs::path Track(const std::string& leaf) {
        Require(!leaf.empty()&&leaf.find_first_of("/\\:")==std::string::npos,"Invalid isolated Input leaf");const auto path=directory/leaf;
        Require(path.parent_path()==directory,"Input file escaped isolated directory");if(std::find(files.begin(),files.end(),path)==files.end())files.push_back(path);return path;
    }
    fs::path Write(const std::string& leaf,const std::string& text) {
        const auto path=Track(leaf);std::ofstream file(path,std::ios::binary|std::ios::trunc);Require(bool(file),"Cannot write isolated Input fixture");
        file.write(text.data(),static_cast<std::streamsize>(text.size()));file.close();Require(bool(file),"Cannot finish isolated Input fixture");return path;
    }
    void Remove(const fs::path& path){Require(path.parent_path()==directory&&std::find(files.begin(),files.end(),path)!=files.end()&&fs::remove(path),"Cannot remove tracked isolated Input config");}
    Bytes Snapshot(const std::string& leaf) {
        const auto path=Track(leaf+".sav");Require(SavePortableRuntimeState(path.string()),"Cannot save whole viewport Input checkpoint");Bytes result;
        Require(QuestVr::ReadBoundedSaveFile(path.string(),QuestVr::kMaximumSaveRuntimeBytes,result),"Cannot read viewport Input checkpoint");return result;
    }
};
void SameGc(const GCStats& expected,const std::string& why) {const auto actual=GC::GetStats();Require(actual.numObjects==expected.numObjects&&actual.memoryUsage==expected.memoryUsage,why);}
bool SameLookup(const PortableObjectLookupState& a,const PortableObjectLookupState& b){return a.prepared==b.prepared&&a.objects==b.objects&&a.generation==b.generation&&a.retainedBytes==b.retainedBytes;}
const PortableInputDefaultSnapshot& Default(const PortableInputSchemaSnapshot& schema,const std::string& path) {
    const auto found=std::find_if(schema.defaults.begin(),schema.defaults.end(),[&](const auto& row){return row.classPath==path;});
    Require(found!=schema.defaults.end(),"Missing actual configured Input default: "+path);return *found;
}
void Empty(const PortableViewportInputSnapshot& value) {
    Require(!value.prepared&&!value.constructed&&!value.initialized&&!value.execInstalled&&!value.extensionNamesPrepared&&value.objectPath.empty()&&
        value.classPath.empty()&&value.outerPath.empty()&&value.ownerViewportPath.empty()&&value.enumPath.empty()&&!value.objectIndex&&!value.classIndex&&
        !value.outerIndex&&!value.ownerViewportIndex&&!value.enumIndex&&!value.nativeViewportIndex&&!value.objectFlags&&!value.retainedBytes,
        "Absent viewport Input exposed a partial native instance/Init capability");
}
void SameInput(const PortableViewportInputSnapshot& a,const PortableViewportInputSnapshot& b,bool counter=true) {
    Require(a.prepared==b.prepared&&a.constructed==b.constructed&&a.initialized==b.initialized&&a.execInstalled==b.execInstalled&&
        a.extensionNamesPrepared==b.extensionNamesPrepared&&a.objectPath==b.objectPath&&a.classPath==b.classPath&&a.outerPath==b.outerPath&&
        a.ownerViewportPath==b.ownerViewportPath&&a.enumPath==b.enumPath&&a.objectIndex==b.objectIndex&&a.classIndex==b.classIndex&&a.outerIndex==b.outerIndex&&
        a.ownerViewportIndex==b.ownerViewportIndex&&a.enumIndex==b.enumIndex&&a.nativeViewportIndex==b.nativeViewportIndex&&a.objectFlags==b.objectFlags&&
        a.clientIdentity==b.clientIdentity&&a.viewportIdentity==b.viewportIdentity&&a.lookupGeneration==b.lookupGeneration&&(!counter||a.uniqueCounter==b.uniqueCounter)&&
        a.retainedBytes==b.retainedBytes&&a.configReloads==b.configReloads&&a.configPasses==b.configPasses&&a.configImports==b.configImports&&
        a.configFailures==b.configFailures&&a.configWarnings==b.configWarnings&&a.configWork==b.configWork&&a.aliases==b.aliases&&a.bindings==b.bindings&&
        a.extensionNames==b.extensionNames&&a.extensionNameIndices==b.extensionNameIndices,"Viewport Input snapshot changed an actual constructor identity/value/lifetime");
}
void Constructed(const PortableViewportInputSnapshot& value,const PortablePlayerSessionSnapshot& session,const PortableInputSchemaSnapshot& schema) {
    const auto& expected=Default(schema,value.classPath);const auto cls=ReadPortableRuntimeObjectOwnership(value.classPath);
    const auto transient=ReadPortableRuntimeObjectOwnership("Transient"),owner=ReadPortableRuntimeObjectOwnership(session.viewportPath);
    const auto actual=ReadPortableRuntimeObjectOwnership(value.objectPath);
    Require(value.prepared&&value.constructed&&!value.initialized&&value.execInstalled&&!value.nativeViewportIndex&&value.objectFlags==0x4000u&&
        value.objectIndex&&value.objectIndex!=cls.objectIndex&&value.classIndex==cls.objectIndex&&value.outerPath=="Transient"&&value.outerIndex==transient.objectIndex&&
        value.ownerViewportPath==session.viewportPath&&value.ownerViewportIndex==owner.objectIndex&&value.clientIdentity==session.clientIdentity&&
        value.viewportIdentity==session.viewportIdentity&&value.enumPath=="Engine.Actor.EInputKey"&&value.enumIndex==schema.keyEnumIndex&&
        value.lookupGeneration==ReadPortableRuntimeObjectLookup().generation&&value.retainedBytes>0u,
        "Input constructor did not preserve actual transient Class/Enum/owner identities, flags or constructor-versus-Init distinction");
    Require(actual.objectIndex==value.objectIndex&&actual.classIndex==value.classIndex&&actual.outerIndex==value.outerIndex&&actual.outerPath==value.outerPath&&
        actual.objectPath==value.objectPath&&!actual.serializedOuterZero,"Input is not a genuine indexed native UObject with Transient Outer");
    Require(value.aliases==expected.aliases&&value.bindings==expected.bindings,"NoInit constructor erased or aliased configured Alias/binding defaults");
    const auto found=FindPortableRuntimeObject({value.objectPath,value.classIndex,0u,true,true});
    Require(found&&found->objectIndex==value.objectIndex&&found->outerIndex==transient.objectIndex&&found->generation==value.lookupGeneration,
        "Constructed Input was not published into the actual weak lookup");
    if(value.classPath=="Extension.InputExt") {
        Require(value.configReloads==2u&&value.configPasses==4u&&value.configImports>0u&&value.configWork>0u&&!value.configFailures&&!value.configWarnings&&value.extensionNamesPrepared,
            "InputExt omitted default-then-instance parent-first config reload or Extension name bootstrap");
        for(std::size_t i=0u;i<value.extensionNames.size();++i) {
            const auto name=NameString::FindCompareIndex(value.extensionNames[i]);
            Require(!value.extensionNames[i].empty()&&name&&*name==value.extensionNameIndices[i]&&*name!=0,"Extension bootstrap did not retain actual named identities");
        }
        Require(value.extensionNames.front()=="InitWindow"&&value.extensionNames.back()=="ComputerFadeOutCompleted","Extension bootstrap order differs from owned audit");
    }else Require(value.classPath=="Engine.Input"&&!value.configReloads&&!value.configPasses,"Base Input ran fabricated InputExt constructor reloads");
    Require(!ReadPortableRuntimePlayerSession(session.viewportIdentity).bound,"Constructor-only Input preparation fabricated a PlayerPawn possession");
}
void Query(std::uint64_t viewport,const std::string& command,bool handled,const std::string& output) {
    const auto result=ExecutePortableRuntimeInputQuery(viewport,command);
    Require(result.handled==handled&&result.output==output&&result.work>0u,"Actual Input provider query changed handled/output/cursor/log semantics: "+command);
}
void Queries(std::uint64_t viewport,const PortableViewportInputSnapshot& input,Temporary& temporary) {
    const auto checkpoint=temporary.Snapshot("QueriesBefore");const auto lookup=ReadPortableRuntimeObjectLookup();const auto gc=GC::GetStats();const auto revision=GetPortableRuntimeWorldRevision();
    const auto enumeration=ReadPortableRuntimeEnum("Engine.Actor.EInputKey");
    Require(enumeration.names.size()==255u,"Input query fixture lost actual complete source Enum");
    for(std::size_t key=0u;key<enumeration.names.size();++key) {
        const auto spelling=enumeration.names[key].ToString();Require(spelling.size()>=3u,"Owned key is shorter than original native suffix offset");
        const auto suffix=spelling.substr(3u);Query(viewport,"KEYNAME "+std::to_string(key),true,suffix);
        // Original FindKeyName prepends IK_; owned key239 is UK_Unknown10F.
        Query(viewport,"KEYBINDING "+suffix,true,key==239u?std::string{}:input.bindings[key]);
    }
    Query(viewport," \tkeyname 1",true,"LeftMouse");Query(viewport,"KEYNAME -1",true,"");Query(viewport,"KEYNAME 255",true,"");
    Query(viewport,"KEYNAME nonsense",true,"None");Query(viewport,"KEYNAME1",false,"");Query(viewport,"GETPING",false,"");
    Query(viewport,std::string(10000u,'x'),false,""); // String growth is metered, not just logical units.
    Query(viewport,std::string("KEYNAME 1\0ignored",17u),true,"LeftMouse");
    const std::string unknown="QuestInputUnknownKey_NoIntern_20261027";
    Require(!NameString::FindCompareIndex("IK_"+unknown),"Unknown query fixture is already interned");Query(viewport,"KEYBINDING "+unknown,true,"");
    Require(!NameString::FindCompareIndex("IK_"+unknown),"Read-only KEYBINDING lookup interned an unknown Name");
    Refused([&]{(void)ExecutePortableRuntimeInputQuery(viewport,"KEYNAME 2147483648");},"Signed-overflow KEYNAME silently fabricated an index");
    Refused([&]{(void)ExecutePortableRuntimeInputQuery(viewport,"KEYBINDING "+std::string(253u,'x'));},"FindKeyName native format buffer bound was ignored");
    Refused([&]{(void)ExecutePortableRuntimeInputQuery(viewport,std::string(16385u,'x'));},"Input command unit admission was ignored");
    SameInput(input,ReadPortableRuntimeViewportInput(viewport));GC::Collect();SameGc(gc,"Input key queries leaked native objects");
    Require(SameLookup(lookup,ReadPortableRuntimeObjectLookup())&&GetPortableRuntimeWorldRevision()==revision&&
        temporary.Snapshot("QueriesAfter")==checkpoint&&!GetPortableRuntimeUnsavedScriptState(),"Input query mutated checkpoint/lookup/revision/script state");
}
void Dormant(){const auto value=ReadPortableActorDeclaredScriptProperty("00_Training.LevelInfo0","Engine.LevelInfo.bBegunPlay");
    Require(value.kind==QuestVr::Vm::Kind::Bool&&!value.boolean,"Input construction or query began the dormant first Training world");}
void Owned(const fs::path& root,const std::vector<PortablePackageTables>& packages,const PortablePackageTables& map,Temporary& temporary) {
    constexpr std::uint64_t client=4101u,viewport=4102u,otherViewport=4103u;
    Require(InitializePortableRuntime(packages).passed,"Cannot initialize owned viewport Input fixture");
    auto lifetime=std::make_shared<QuestVr::NativeClientLifetime>();BindPortableRuntimeClient(lifetime,client);RegisterPortableRuntimeViewport(client,{viewport,1440u,1440u,3u,2u});
    Refused([&]{(void)PreparePortableRuntimeViewportInput(viewport,Ordinary);},"Input preparation implicitly manufactured a missing native viewport wrapper");
    Refused([&]{(void)ReadPortableRuntimeViewportInput(viewport);},"Input read silently manufactured a missing native viewport wrapper");
    const auto session=PreparePortableRuntimePlayerObjects(viewport);Empty(ReadPortableRuntimeViewportInput(viewport));
    Require(!session.bound&&session.playerPath.empty(),"Prerequisite viewport construction fabricated a possessed PlayerPawn");
    const auto initial=temporary.Snapshot("MissingSchemaBefore");const auto initialGc=GC::GetStats();
    Refused([&]{(void)PreparePortableRuntimeViewportInput(viewport,Ordinary);},"Input preparation fabricated missing configured native schema");
    Refused([&]{(void)ExecutePortableRuntimeInputQuery(viewport,"KEYNAME 1");},"Key query silently constructed missing Input");
    Empty(ReadPortableRuntimeViewportInput(viewport));GC::Collect();SameGc(initialGc,"Missing-schema Input refusal leaked staged objects");
    Require(temporary.Snapshot("MissingSchemaAfter")==initial,"Missing Input prerequisite changed the whole checkpoint");
    ClearPortableRuntimeClient();GC::Collect();
    Require(PreparePortableRuntimeNativeEngineClasses().prepared&&PreparePortableRuntimeNativeMetadataClasses().prepared&&
        PreparePortableRuntimeNativeConfigClasses().prepared&&PreparePortableRuntimeNativePackages().prepared&&PreparePortableRuntimeNativeInputClasses().prepared&&
        PreparePortableRuntimeObjectLookup().prepared&&PreparePortableRuntimeEngineCollision((root/"System"/"DeusEx.ini").string()).prepared&&
        PreparePortableRuntimeInputSchema().prepared,"Cannot prepare genuine native Input schema dependencies");
    auto active=std::make_shared<QuestVr::NativeClientLifetime>();BindPortableRuntimeClient(active,4201u);RegisterPortableRuntimeViewport(4201u,{viewport,1440u,1440u,3u,2u});
    const auto preparedSession=PreparePortableRuntimePlayerObjects(viewport);const auto noConfig=ReadPortableRuntimeObjectLookup();const auto noConfigGc=GC::GetStats();
    Refused([&]{(void)PreparePortableRuntimeViewportInput(viewport,Ordinary);},"Input construction implicitly configured User.ini");
    Empty(ReadPortableRuntimeViewportInput(viewport));Require(SameLookup(noConfig,ReadPortableRuntimeObjectLookup()),"Missing config published an Input lookup alias");
    GC::Collect();SameGc(noConfigGc,"Missing-config refusal leaked native Input objects");
    const auto userSource=root/"System"/"User.ini";std::ifstream file(userSource,std::ios::binary);Require(bool(file),"Cannot read owned User.ini as fixture data");
    const auto size=fs::file_size(userSource);Require(size<=1u<<20u,"Owned User.ini exceeds source profile");std::string text(static_cast<std::size_t>(size),'\0');
    if(!text.empty())file.read(text.data(),static_cast<std::streamsize>(text.size()));
    Require(bool(file),"Cannot finish owned User.ini fixture read");file.close();
    const auto user=temporary.Write("User.ini",text);const auto schema=PreparePortableRuntimeInputConfig(user.string());
    Require(schema.configApplied,"Actual native Input config did not commit");
    temporary.Write("User.ini","[Engine.Input]\nLeftMouse=changed-after-committed-binding\nAliases[0]=(Command=\"Changed\",Alias=Changed)\n");temporary.Remove(user);
    Require(LoadPortableRuntimeMap(map).passed,"Cannot load first Training before Input construction");Dormant();
    const auto baseline=temporary.Snapshot("BeforeInput");const auto worldRevision=GetPortableRuntimeWorldRevision();
    const auto lookup=ReadPortableRuntimeObjectLookup();const auto gc=GC::GetStats();
    Refused([&]{(void)PreparePortableRuntimeViewportInput(999999u,Ordinary);},"Unknown viewport fabricated an Input/session");
    Refused([&]{(void)PreparePortableRuntimeViewportInput(viewport,static_cast<PortableInputLoggingProfile>(255u));},"Unaudited logging profile was silently accepted");
    const auto input=PreparePortableRuntimeViewportInput(viewport,Ordinary);Constructed(input,preparedSession,schema);
    Require(input.classPath=="Extension.InputExt"&&input.aliases[0u].alias=="Fire"&&input.aliases[0u].command=="Button bFire | Fire"&&
        input.bindings[1u]=="ParseLeftClick|Fire"&&input.bindings[87u]=="MoveForward","InputExt constructor did not use committed original config after supplied file deletion");
    const auto inputGc=GC::GetStats();const auto withInput=ReadPortableRuntimeObjectLookup();
    Require(inputGc.numObjects==gc.numObjects+1u&&withInput.objects==lookup.objects+1u&&withInput.generation==lookup.generation,
        "One viewport Input constructor did not allocate exactly one genuine UObject/lookup record");
    for(unsigned repeat=0u;repeat<3u;++repeat){SameInput(input,PreparePortableRuntimeViewportInput(viewport,Ordinary));SameInput(input,ReadPortableRuntimeViewportInput(viewport));}
    auto detached=input;detached.aliases[0u].command="Detached";detached.bindings[1u]="Detached";detached.extensionNameIndices[0u]=0;
    SameInput(input,ReadPortableRuntimeViewportInput(viewport));Queries(viewport,input,temporary);
    Require(temporary.Snapshot("AfterInput")==baseline&&GetPortableRuntimeWorldRevision()==worldRevision,
        "Input constructor changed actor checkpoint or world revision");Dormant();
    Require(std::search(baseline.begin(),baseline.end(),input.objectPath.begin(),input.objectPath.end())==baseline.end(),"Transient Input identity was serialized into dormant checkpoint");
    RegisterPortableRuntimeViewport(4201u,{otherViewport,1440u,1440u,3u,2u});const auto otherSession=PreparePortableRuntimePlayerObjects(otherViewport);
    Empty(ReadPortableRuntimeViewportInput(otherViewport));const auto beforeOther=GC::GetStats();const auto other=PreparePortableRuntimeViewportInput(otherViewport,Ordinary);
    Constructed(other,otherSession,schema);Require(other.objectIndex!=input.objectIndex&&other.objectPath!=input.objectPath&&other.uniqueCounter>input.uniqueCounter&&
        other.ownerViewportIndex!=input.ownerViewportIndex&&GC::GetStats().numObjects==beforeOther.numObjects+1u,
        "Two viewports shared an Input instance, owner or concrete-class unique-name counter");
    SameInput(input,ReadPortableRuntimeViewportInput(viewport),false);const auto both=ReadPortableRuntimeObjectLookup();const auto bothGc=GC::GetStats();
    Require(LoadPortableRuntimeState((temporary.directory/"BeforeInput.sav").string()),"Cannot warm-restore dormant Training with native Inputs");
    SameInput(input,ReadPortableRuntimeViewportInput(viewport),false);SameInput(other,ReadPortableRuntimeViewportInput(otherViewport));GC::Collect();SameGc(bothGc,"Warm restore lost native Input/session roots");
    Require(temporary.Snapshot("WarmInputs")==baseline,"Warm restore serialized/overwrote native Input service state");Dormant();
    Require(UnloadPortableRuntimeMap()>0u,"Cannot unload first Training with viewport Inputs");GC::Collect();
    SameInput(input,ReadPortableRuntimeViewportInput(viewport),false);SameInput(other,ReadPortableRuntimeViewportInput(otherViewport));Query(viewport,"KEYNAME 1",true,"LeftMouse");
    Require(LoadPortableRuntimeMap(map).passed,"Cannot reload first Training with constructor-only Input service");
    SameInput(input,ReadPortableRuntimeViewportInput(viewport),false);SameInput(other,ReadPortableRuntimeViewportInput(otherViewport));
    Require(temporary.Snapshot("ReloadInputs")==baseline,"Input map lifetime contaminated dormant Training state");Dormant();
    active->Revoke();Refused([&]{(void)ReadPortableRuntimeViewportInput(viewport);},"Revoked renderer retained Input snapshot authority");
    Refused([&]{(void)ExecutePortableRuntimeInputQuery(viewport,"KEYNAME 1");},"Revoked Input query masqueraded as handled empty");
    Refused([&]{(void)PreparePortableRuntimeViewportInput(viewport,Ordinary);},"Revoked renderer reconstructed a native Input");
    ClearPortableRuntimeClient();GC::Collect();
    Require(!HasPortableRuntimeClient(),"Input teardown retained a live native renderer client");
    for(const auto& path:{input.objectPath,other.objectPath,preparedSession.viewportPath,otherSession.viewportPath,preparedSession.clientPath}) {
        Refused([&]{(void)ReadPortableRuntimeObjectOwnership(path);},"Cleared Input/session identity retained canonical lookup authority");
        Require(!FindPortableRuntimeObject({path,0u,0u,true,false}),"Cleared Input/session identity survived weak lookup/GC retirement");
    }
    Require(ReadPortableRuntimeObjectLookup().objects+5u==both.objects,"Clear did not retire exactly Client/two Viewports/two Inputs");
    auto replacement=std::make_shared<QuestVr::NativeClientLifetime>();BindPortableRuntimeClient(replacement,4301u);RegisterPortableRuntimeViewport(4301u,{4302u,1440u,1440u,3u,2u});
    const auto replacementSession=PreparePortableRuntimePlayerObjects(4302u);const auto next=PreparePortableRuntimeViewportInput(4302u,Ordinary);
    Constructed(next,replacementSession,schema);Require(next.objectPath!=input.objectPath&&next.objectPath!=other.objectPath&&next.uniqueCounter>other.uniqueCounter,
        "Fresh native session reused retired concrete Input name/counter or preserved old owner");Query(4302u,"KEYBINDING LeftMouse",true,"ParseLeftClick|Fire");
    replacement->Revoke();ClearPortableRuntimeClient();GC::Collect();
    Require(temporary.Snapshot("ClearedInputs")==baseline,"Input/session revoke/rebind changed actor checkpoint");Dormant();
    ShutdownPortableRuntime();
}
} // namespace
int main(int argc,char** argv) {
    std::cout.setf(std::ios::unitbuf);
    try {
        fs::path root;if(argc==3&&std::string(argv[1])=="--game-root")root=argv[2];
        else if(argc==1){const auto* value=std::getenv("DEUSEX_GAME_ROOT");if(value&&*value)root=value;}
        else throw std::runtime_error("Usage: portable_viewport_input_test [--game-root OWNED_GAME_ROOT] (or DEUSEX_GAME_ROOT)");
        if(root.empty()){std::cout<<"SKIP owned viewport Input constructor/query integration: use --game-root or DEUSEX_GAME_ROOT.\n";return 77;}
        root=fs::canonical(root);ShutdownPortableRuntime();GC::Collect();const auto baseline=GC::GetStats();Temporary temporary(root);
        Refused([&]{(void)PreparePortableRuntimeViewportInput(1u,Ordinary);},"Input constructor accepted an uninitialized runtime");
        Refused([&]{(void)ExecutePortableRuntimeInputQuery(1u,"KEYNAME 1");},"Input query accepted an uninitialized runtime");
        std::vector<Source> sources;std::vector<PortablePackageTables> packages;
        sources.emplace_back(root/"System"/"DeusEx.ini");sources.emplace_back(root/"System"/"User.ini");
        for(const char* name:{"Core","Engine","DeusEx","Extension","ConSys","IpDrv","IpServer","UBrowser","UWindow","Editor","Fire","MPCharacters","DeusExSounds","DeusExDeco","DeusExItems","DeusExCharacters"}) {
            sources.emplace_back(root/"System"/(std::string(name)+".u"));packages.push_back(LoadPortablePackageTables(sources.back().path.string()));
        }
        sources.emplace_back(root/"Maps"/"00_Training.dx");const auto map=LoadPortablePackageTables(sources.back().path.string());
        Owned(root,packages,map,temporary);ShutdownPortableRuntime();GC::Collect();SameGc(baseline,"Input/session/native Class roots survived explicit runtime shutdown");
        for(const auto& source:sources)source.Unchanged();
        std::cout<<"PASS owned viewport Input integration: "<<checks<<" checks, "<<refusals
            <<" refusals; actual configured Transient InputExt objects/Enum/NoInit/default reload/bootstrap, exact key-query handling, cached config, "
              "distinct viewport ownership, detached snapshots, dormant first Training save/map/GC/revoke/rebind lifetime. NOT Input Init/Reset/ReadInput, "
              "complete viewport FExec fallback, begun startup or live VR weapon/physics acceptance.\n";return 0;
    }catch(const std::exception& error){std::cerr<<"FAIL: "<<error.what()<<'\n';ShutdownPortableRuntime();return 1;}
}
