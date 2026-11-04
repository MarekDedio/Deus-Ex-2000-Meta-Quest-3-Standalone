#include "Precomp.h"
#include "GC/GC.h"
#include "portable_object_lookup.h"
#include "portable_unreal_runtime.h"
#include "portable_viewport_canvas.h"
#include "portable_viewport_console.h"
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
void SameClient(const PortableNativeClientSessionSnapshot& a,const PortableNativeClientSessionSnapshot& b) {
    Require(a.clientIdentity==b.clientIdentity&&a.lookupGeneration==b.lookupGeneration&&a.clientIndex==b.clientIndex&&
        a.engineIndex==b.engineIndex&&a.clientPath==b.clientPath&&a.clientClassPath==b.clientClassPath&&
        a.clientOuterPath==b.clientOuterPath&&a.enginePath==b.enginePath&&a.clientObjectFlags==b.clientObjectFlags&&
        a.initialized==b.initialized&&a.viewports.size()==b.viewports.size()&&a.registeredTargets.size()==b.registeredTargets.size(),
        "Native Client snapshot changed actual identity, Engine binding or array/target counts");
    for(std::size_t index=0u;index<a.viewports.size();++index)
        Require(a.viewports[index].identity==b.viewports[index].identity&&a.viewports[index].path==b.viewports[index].path,
            "Native Client snapshot changed actual viewport order/identity");
    for(std::size_t index=0u;index<a.registeredTargets.size();++index) {
        const auto& first=a.registeredTargets[index];const auto& second=b.registeredTargets[index];
        Require(first.identity==second.identity&&first.width==second.width&&first.height==second.height&&
            first.swapchainImages==second.swapchainImages&&first.eyeCount==second.eyeCount,
            "Native Client snapshot changed separately labelled registered target metadata");
    }
}
void EngineClient(const std::string& engine,const std::string& expected) {
    const auto value=ReadPortableRuntimeDeclaredScriptProperty(engine,"Engine.Engine.Client");
    Require(value.kind==QuestVr::Vm::Kind::Object&&value.text==expected,
        "Actual declared Engine.Client does not match the native Client backpointer/teardown");
}
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
    for(const auto* action:{"BUTTON bFire","PULSE bFire","TOGGLE bFire","AXIS aBaseX"})
        Refused([&]{(void)ExecutePortableRuntimeInputQuery(viewport,action);},"Original action branch fell through to aliases/providers");
    for(const auto& alias:input.aliases)if(alias.aliasCompareIndex) {
        bool reached{};try{static_cast<void>(ExecutePortableRuntimeInputQuery(viewport,alias.alias));}
        catch(const std::exception& error){reached=std::string(error.what()).find("Original Input alias requires its ExecInputCommands/")==0u;}
        Require(reached,"Actual configured alias did not reach its honest ExecInputCommands service boundary");break;
    }
    SameInput(input,ReadPortableRuntimeViewportInput(viewport));GC::Collect();SameGc(gc,"Input key queries leaked native objects");
    Require(SameLookup(lookup,ReadPortableRuntimeObjectLookup())&&GetPortableRuntimeWorldRevision()==revision&&
        temporary.Snapshot("QueriesAfter")==checkpoint&&!GetPortableRuntimeUnsavedScriptState(),"Input query mutated checkpoint/lookup/revision/script state");
}
void Dormant(){const auto value=ReadPortableActorDeclaredScriptProperty("00_Training.LevelInfo0","Engine.LevelInfo.bBegunPlay");
    Require(value.kind==QuestVr::Vm::Kind::Bool&&!value.boolean,"Input construction or query began the dormant first Training world");}
void ClientOnly(const fs::path& root,const std::vector<PortablePackageTables>& packages,Temporary& temporary) {
    constexpr std::uint64_t clientIdentity=8101u,firstIdentity=8102u,secondIdentity=8103u;
    Require(InitializePortableRuntime(packages).passed,"Cannot initialize fresh Client-only fixture");
    Require(PreparePortableRuntimeNativeEngineClasses().prepared&&PreparePortableRuntimeNativeMetadataClasses().prepared&&
        PreparePortableRuntimeNativeConfigClasses().prepared&&PreparePortableRuntimeNativePackages().prepared&&
        PreparePortableRuntimeNativeInputClasses().prepared&&PreparePortableRuntimeObjectLookup().prepared&&
        PreparePortableRuntimeEngineCollision((root/"System"/"DeusEx.ini").string()).prepared,
        "Cannot prepare genuine Engine prerequisites for Client-only construction");
    const auto engine=ReadPortableRuntimeEngineCollision();
    const auto engineOwner=ReadPortableRuntimeObjectOwnership(engine.enginePath);
    auto lifetime=std::make_shared<QuestVr::NativeClientLifetime>();BindPortableRuntimeClient(lifetime,clientIdentity);
    Refused([&]{(void)ReadPortableRuntimeClientSession();},"A lifetime token alone fabricated an actual Client UObject");
    Require(ReadPortableRuntimeClientView().empty(),"Fresh Client-only fixture already has registered stereo targets");
    const auto metaClass=ReadPortableRuntimeObjectOwnership("Core.Class").objectIndex;
    const auto enginePackage=ReadPortableRuntimeObjectOwnership("Engine").objectIndex;
    const auto nativeClass=[&](const char* name) {
        return FindPortableRuntimeObject({name,metaClass,enginePackage,false,true});
    };
    // An untyped ANY_PACKAGE lookup can return e.g. Console.Viewport, which
    // is a Property, not the Engine.Viewport UClass. Use actual Class/Outer.
    const auto clientClassBefore=nativeClass("Client");
    const auto viewportClassBefore=nativeClass("Viewport");
    const auto untypedViewport=FindPortableRuntimeObject({"Engine.Viewport",0u,0u,true,false});
    std::cout<<"CLIENT CLASS BASELINE untypedViewport="<<(untypedViewport?untypedViewport->objectPath:"None")
        <<" actualViewportClass="<<(viewportClassBefore?viewportClassBefore->objectPath:"None")<<'\n';
    const auto before=temporary.Snapshot("ClientOnlyBefore");const auto beforeGc=GC::GetStats();
    const auto beforeLookup=ReadPortableRuntimeObjectLookup();const auto worldCount=GetPortableRuntimeMapActors(true).size();
    const auto revision=GetPortableRuntimeWorldRevision();EngineClient(engine.enginePath,"");
    const auto client=PreparePortableRuntimeClientObjects();const auto ownership=ReadPortableRuntimeObjectOwnership(client.clientPath);
    Require(client.initialized&&client.clientIdentity==clientIdentity&&client.clientIndex==ownership.objectIndex&&
        client.clientIndex&&client.clientClassPath=="Engine.Client"&&client.clientOuterPath.empty()&&client.clientObjectFlags==0x4000&&
        client.enginePath==engine.enginePath&&client.engineIndex==engineOwner.objectIndex&&client.engineIndex&&
        client.lookupGeneration==beforeLookup.generation&&client.viewports.empty()&&client.registeredTargets.empty(),
        "Zero-target Client.Init did not establish an authentic actual Client/Engine pair and empty native viewport array");
    EngineClient(engine.enginePath,client.clientPath);
    const auto clientObjects=clientClassBefore?1u:2u;const auto clientGc=GC::GetStats();const auto clientLookup=ReadPortableRuntimeObjectLookup();
    // One additional GC allocation is the session root itself, not a UObject
    // or lookup record. Every other new object is precisely Client(+UClass).
    const auto viewportClassAfter=nativeClass("Viewport");
    Require(clientGc.numObjects==beforeGc.numObjects+clientObjects+1u&&clientLookup.objects==beforeLookup.objects+clientObjects&&
        clientLookup.generation==beforeLookup.generation&&bool(viewportClassAfter)==bool(viewportClassBefore)&&
        (!viewportClassBefore||viewportClassAfter->objectIndex==viewportClassBefore->objectIndex)&&
        GetPortableRuntimeMapActors(true).size()==worldCount&&GetPortableRuntimeWorldRevision()==revision&&
        temporary.Snapshot("ClientOnlyConstructed")==before&&!GetPortableRuntimeUnsavedScriptState(),
        "Client-only preparation created a Viewport/Class, Pawn, Input, Canvas or Console, or changed actor checkpoint state");
    for(unsigned repeat=0u;repeat<3u;++repeat) {
        SameClient(client,PreparePortableRuntimeClientObjects());SameClient(client,ReadPortableRuntimeClientSession());
        GC::Collect();SameGc(clientGc,"Idempotent zero-target Client preparation leaked native UObjects");
        Require(SameLookup(clientLookup,ReadPortableRuntimeObjectLookup()),"Idempotent Client preparation changed weak lookup");
    }
    RegisterPortableRuntimeViewport(clientIdentity,{firstIdentity,1440u,1440u,3u,2u});
    const auto oneTarget=ReadPortableRuntimeClientSession();
    Require(oneTarget.initialized&&oneTarget.clientIndex==client.clientIndex&&oneTarget.engineIndex==client.engineIndex&&
        oneTarget.viewports.empty()&&oneTarget.registeredTargets.size()==1u&&oneTarget.registeredTargets[0u].identity==firstIdentity,
        "Registering a stereo target implicitly appended an actual native Viewport");
    Refused([&]{(void)ReadPortableRuntimePlayerSession(firstIdentity);},"Registered metadata fabricated a native player session");
    Refused([&]{(void)ReadPortableRuntimeViewportInput(firstIdentity);},"Registered metadata fabricated an Input owner");
    Refused([&]{(void)ReadPortableRuntimeViewportCanvas(firstIdentity);},"Registered metadata fabricated a Canvas owner");
    Refused([&]{(void)ReadPortableRuntimeViewportConsole(firstIdentity);},"Registered metadata fabricated a Console owner");
    Refused([&]{(void)PreparePortableRuntimePlayerObjects(8999u);},"Unknown target partially published native Client/Viewport state");
    SameClient(oneTarget,ReadPortableRuntimeClientSession());GC::Collect();SameGc(clientGc,"Target-only reads/refusals leaked native objects");
    Require(SameLookup(clientLookup,ReadPortableRuntimeObjectLookup())&&temporary.Snapshot("ClientOnlyRegistered")==before,
        "Target metadata/refusals changed native lookup or actor checkpoint");
    const auto first=PreparePortableRuntimePlayerObjects(firstIdentity);const auto afterFirst=ReadPortableRuntimeClientSession();
    const auto viewportObjects=viewportClassBefore?1u:2u;
    Require(!first.bound&&first.playerPath.empty()&&first.clientPath==client.clientPath&&first.viewportOuterPath==client.clientPath&&
        afterFirst.initialized&&afterFirst.clientIndex==client.clientIndex&&afterFirst.viewports.size()==1u&&
        afterFirst.viewports[0u].identity==firstIdentity&&afterFirst.viewports[0u].path==first.viewportPath&&
        afterFirst.registeredTargets.size()==1u&&nativeClass("Viewport")&&
        GC::GetStats().numObjects==clientGc.numObjects+viewportObjects&&ReadPortableRuntimeObjectLookup().objects==clientLookup.objects+viewportObjects,
        "Explicit first NewViewport failed to append exactly its actual UClass/instance or manufactured a player");
    Empty(ReadPortableRuntimeViewportInput(firstIdentity));
    Require(!ReadPortableRuntimeViewportCanvas(firstIdentity).prepared&&!ReadPortableRuntimeViewportConsole(firstIdentity).prepared,
        "Explicit viewport preparation fabricated missing Canvas/Console construction/Init");
    const auto firstGc=GC::GetStats();const auto firstLookup=ReadPortableRuntimeObjectLookup();
    Require(PreparePortableRuntimePlayerObjects(firstIdentity).viewportPath==first.viewportPath,
        "Repeated explicit NewViewport replaced its actual object");
    SameClient(afterFirst,PreparePortableRuntimeClientObjects());GC::Collect();SameGc(firstGc,"Repeated Client/NewViewport allocated duplicate objects");
    Require(SameLookup(firstLookup,ReadPortableRuntimeObjectLookup()),"Repeated Client/NewViewport changed lookup publication");
    RegisterPortableRuntimeViewport(clientIdentity,{secondIdentity,1600u,1600u,3u,2u});
    const auto twoTargets=ReadPortableRuntimeClientSession();
    Require(twoTargets.viewports.size()==1u&&twoTargets.viewports[0u].path==first.viewportPath&&twoTargets.registeredTargets.size()==2u,
        "Second registered target mutated the actual native viewport array");
    const auto second=PreparePortableRuntimePlayerObjects(secondIdentity);const auto twoViewports=ReadPortableRuntimeClientSession();
    Require(!second.bound&&second.clientPath==client.clientPath&&second.viewportPath!=first.viewportPath&&
        twoViewports.viewports.size()==2u&&twoViewports.viewports[0u].identity==firstIdentity&&twoViewports.viewports[0u].path==first.viewportPath&&
        twoViewports.viewports[1u].identity==secondIdentity&&twoViewports.viewports[1u].path==second.viewportPath&&
        twoViewports.registeredTargets.size()==2u&&GC::GetStats().numObjects==firstGc.numObjects+1u&&
        ReadPortableRuntimeObjectLookup().objects==firstLookup.objects+1u,
        "Explicit second NewViewport changed Client identity/order or allocated more than one actual viewport");
    const auto twoGc=GC::GetStats();const auto twoLookup=ReadPortableRuntimeObjectLookup();
    SameClient(twoViewports,PreparePortableRuntimeClientObjects());
    Require(PreparePortableRuntimePlayerObjects(secondIdentity).viewportPath==second.viewportPath,
        "Repeated second NewViewport replaced its actual identity");
    Refused([&]{(void)ReadPortableRuntimePlayerSession(8999u);},"Unknown viewport read borrowed another actual viewport");
    Refused([&]{(void)PreparePortableRuntimePlayerObjects(8999u);},"Unknown viewport creation changed the actual array");
    SameClient(twoViewports,ReadPortableRuntimeClientSession());GC::Collect();SameGc(twoGc,"Two-viewport repeat/refusal leaked native objects");
    Require(SameLookup(twoLookup,ReadPortableRuntimeObjectLookup())&&GetPortableRuntimeWorldRevision()==revision&&
        temporary.Snapshot("ClientOnlyTwoViewports")==before,"Explicit native arrays changed lookup unexpectedly or contaminated checkpoint");
    lifetime->Revoke();
    Refused([&]{(void)ReadPortableRuntimeClientSession();},"Revoked Client retained native snapshot authority");
    Refused([&]{(void)PreparePortableRuntimeClientObjects();},"Revoked Client reinitialized the Engine backpointer");
    Refused([&]{(void)ReadPortableRuntimePlayerSession(firstIdentity);},"Revoked Client retained its actual viewport authority");
    Refused([&]{(void)PreparePortableRuntimePlayerObjects(secondIdentity);},"Revoked Client reconstructed an actual viewport");
    Require(SameLookup(twoLookup,ReadPortableRuntimeObjectLookup()),"Revocation refusal mutated native weak lookup");
    GC::Collect();SameGc(twoGc,"Revocation refusal prematurely collected or leaked actual native session objects");
    EngineClient(engine.enginePath,client.clientPath);ClearPortableRuntimeClient();GC::Collect();EngineClient(engine.enginePath,"");
    Refused([&]{(void)ReadPortableRuntimeClientSession();},"Cleared Client retained a native payload/snapshot");
    for(const auto& path:{client.clientPath,first.viewportPath,second.viewportPath}) {
        Require(!FindPortableRuntimeObject({path,0u,0u,true,false}),"Cleared Client/viewport survived weak lookup retirement");
        Refused([&]{(void)ReadPortableRuntimeObjectOwnership(path);},"Cleared Client/viewport retained canonical UObject authority");
    }
    Require(ReadPortableRuntimeObjectLookup().objects+3u==twoLookup.objects&&GC::GetStats().numObjects+3u==twoGc.numObjects&&
        !HasPortableRuntimeClient()&&ReadPortableRuntimeClientView().empty()&&temporary.Snapshot("ClientOnlyCleared")==before,
        "Teardown failed to retire actual Client/viewports, retained targets, or changed actor checkpoint");
    ShutdownPortableRuntime();GC::Collect();
}
void ActualArrayDistance(const fs::path& root,const std::vector<PortablePackageTables>& packages,
    const PortablePackageTables& map,Temporary& temporary) {
    constexpr std::uint64_t clientIdentity=8201u,firstIdentity=8202u,secondIdentity=8203u;
    Require(InitializePortableRuntime(packages).passed&&
        PreparePortableRuntimeNativeEngineClasses().prepared&&PreparePortableRuntimeNativeMetadataClasses().prepared&&
        PreparePortableRuntimeNativeConfigClasses().prepared&&PreparePortableRuntimeNativePackages().prepared&&
        PreparePortableRuntimeNativeInputClasses().prepared&&PreparePortableRuntimeObjectLookup().prepared&&
        PreparePortableRuntimeEngineCollision((root/"System"/"DeusEx.ini").string()).prepared&&
        LoadPortableRuntimeMap(map).passed,"Cannot prepare actual first Training Engine/Level for viewport-array distance controls");
    auto lifetime=std::make_shared<QuestVr::NativeClientLifetime>();BindPortableRuntimeClient(lifetime,clientIdentity);
    const auto client=PreparePortableRuntimeClientObjects();
    Require(client.initialized&&client.viewports.empty()&&client.registeredTargets.empty(),
        "Distance fixture did not start with actual empty initialized Client");
    const auto run=[&](bool succeeds,const char* stage,const char* expectedError="") {
        const auto before=temporary.Snapshot("ActualArrayDistanceBefore");
        const auto gc=GC::GetStats();const auto lookup=ReadPortableRuntimeObjectLookup();const auto revision=GetPortableRuntimeWorldRevision();
        const auto result=AdvancePortableLevelPlayerDistance();
        if(succeeds)Require(result.execution.passed()&&result.execution.committed&&result.pass.completed&&
            result.pass.noActor&&!result.pass.networkSkipped&&!result.pass.notPlayerPawn&&!result.pass.noPlayer&&
            result.pass.steps==2u&&!result.pass.visits&&!result.pass.nullSlots&&!result.pass.writes&&
            result.pass.playerPath.empty()&&!result.execution.writes,
            std::string("Actual unpossessed viewport did not take genuine no-Actor distance exit: ")+stage+": "+result.execution.error);
        else {
            Require(!result.execution.passed()&&!result.execution.committed&&!result.pass.completed&&!result.execution.writes&&
                (std::string(expectedError).empty()||result.execution.error.find(expectedError)!=std::string::npos),
                std::string("Missing actual distance dereference became success or selected a metadata-only viewport: ")+stage+": "+result.execution.error);
            ++refusals;
        }
        GC::Collect();SameGc(gc,std::string("Distance array control leaked/retired native objects: ")+stage);
        Require(SameLookup(lookup,ReadPortableRuntimeObjectLookup())&&GetPortableRuntimeWorldRevision()==revision&&
            temporary.Snapshot("ActualArrayDistanceAfter")==before&&!GetPortableRuntimeUnsavedScriptState(),
            std::string("Distance array control changed world/lookup/checkpoint: ")+stage);
        Dormant();
    };
    constexpr auto emptyError="Original distance prepass cannot dereference empty native Viewports[0]";
    run(false,"zero targets/zero actual viewports",emptyError);
    RegisterPortableRuntimeViewport(clientIdentity,{firstIdentity,1440u,1440u,3u,2u});
    RegisterPortableRuntimeViewport(clientIdentity,{secondIdentity,1600u,1600u,3u,2u});
    const auto descriptors=ReadPortableRuntimeClientSession();
    Require(descriptors.viewports.empty()&&descriptors.registeredTargets.size()==2u&&
        descriptors.registeredTargets[0u].identity==firstIdentity&&descriptors.registeredTargets[1u].identity==secondIdentity,
        "Descriptor A,B registration fabricated actual viewports or changed metadata order");
    // This must still be an actual empty-array dereference, NOT selection of
    // the raw first descriptor A followed by a missing native A error.
    run(false,"descriptors A,B/zero actual viewports",emptyError);
    const auto second=PreparePortableRuntimePlayerObjects(secondIdentity);
    const auto onlySecond=ReadPortableRuntimeClientSession();
    Require(!second.bound&&second.playerPath.empty()&&onlySecond.viewports.size()==1u&&
        onlySecond.viewports[0u].identity==secondIdentity&&onlySecond.viewports[0u].path==second.viewportPath&&
        onlySecond.registeredTargets.size()==2u&&onlySecond.registeredTargets[0u].identity==firstIdentity,
        "Explicit native B did not remain independent from the first registered target A");
    Refused([&]{(void)ReadPortableRuntimePlayerSession(firstIdentity);},"Metadata-only A borrowed actual B's native player session");
    run(true,"descriptors A,B/actual B only");SameClient(onlySecond,ReadPortableRuntimeClientSession());
    const auto first=PreparePortableRuntimePlayerObjects(firstIdentity);
    const auto reversed=ReadPortableRuntimeClientSession();
    Require(!first.bound&&reversed.clientIndex==client.clientIndex&&reversed.engineIndex==client.engineIndex&&
        reversed.viewports.size()==2u&&reversed.viewports[0u].identity==secondIdentity&&reversed.viewports[0u].path==second.viewportPath&&
        reversed.viewports[1u].identity==firstIdentity&&reversed.viewports[1u].path==first.viewportPath&&
        reversed.registeredTargets.size()==2u&&reversed.registeredTargets[0u].identity==firstIdentity&&
        reversed.registeredTargets[1u].identity==secondIdentity,
        "Actual viewport append order B,A was replaced by descriptor order A,B");
    run(true,"descriptors A,B/actual B,A");SameClient(reversed,ReadPortableRuntimeClientSession());
    lifetime->Revoke();run(false,"revoked exact Client lifetime");
    const auto beforeClearGc=GC::GetStats();const auto beforeClearLookup=ReadPortableRuntimeObjectLookup();
    ClearPortableRuntimeClient();EngineClient(client.enginePath,"");
    // Clear intentionally defers collection until a safe outer boundary. Do
    // not charge that legitimate retirement to the following refusal probe.
    GC::Collect();
    Require(GC::GetStats().numObjects+3u==beforeClearGc.numObjects&&
        ReadPortableRuntimeObjectLookup().objects+3u==beforeClearLookup.objects,
        "Distance teardown did not retire exactly Client plus two actual viewports");
    run(false,"cleared actual Engine.Client","Original distance prepass lacks a live actual Engine.Client");
    ShutdownPortableRuntime();GC::Collect();
}
void Owned(const fs::path& root,const std::vector<PortablePackageTables>& packages,const PortablePackageTables& map,Temporary& temporary) {
    constexpr std::uint64_t client=4101u,viewport=4102u,otherViewport=4103u;
    Require(InitializePortableRuntime(packages).passed,"Cannot initialize owned viewport Input fixture");
    auto lifetime=std::make_shared<QuestVr::NativeClientLifetime>();BindPortableRuntimeClient(lifetime,client);RegisterPortableRuntimeViewport(client,{viewport,1440u,1440u,3u,2u});
    const auto missingEngine=temporary.Snapshot("MissingEngineBefore");const auto missingEngineGc=GC::GetStats();
    const auto missingEngineLookup=ReadPortableRuntimeObjectLookup();
    Refused([&]{(void)PreparePortableRuntimeClientObjects();},"Client.Init fabricated an Engine from a live target registration");
    Refused([&]{(void)ReadPortableRuntimeClientSession();},"Target registration fabricated an actual constructed Client");
    GC::Collect();SameGc(missingEngineGc,"Missing-Engine Client.Init refusal leaked staged native objects");
    Require(SameLookup(missingEngineLookup,ReadPortableRuntimeObjectLookup())&&temporary.Snapshot("MissingEngineAfter")==missingEngine,
        "Missing-Engine Client.Init refusal changed lookup/checkpoint");
    Refused([&]{(void)PreparePortableRuntimeViewportInput(viewport,Ordinary);},"Input preparation implicitly manufactured a missing native viewport wrapper");
    Refused([&]{(void)ReadPortableRuntimeViewportInput(viewport);},"Input read silently manufactured a missing native viewport wrapper");
    const auto session=PreparePortableRuntimePlayerObjects(viewport);Empty(ReadPortableRuntimeViewportInput(viewport));
    Require(!session.bound&&session.playerPath.empty(),"Prerequisite viewport construction fabricated a possessed PlayerPawn");
    const auto constructorClient=ReadPortableRuntimeClientSession();
    Require(!constructorClient.initialized&&constructorClient.enginePath.empty()&&!constructorClient.engineIndex&&
        constructorClient.clientPath==session.clientPath&&constructorClient.clientIdentity==client&&constructorClient.viewports.size()==1u&&
        constructorClient.viewports[0u].identity==viewport&&constructorClient.viewports[0u].path==session.viewportPath&&
        constructorClient.registeredTargets.size()==1u,"Legacy constructor-only Client claimed an initialized Engine backpointer");
    const auto initial=temporary.Snapshot("MissingSchemaBefore");const auto initialGc=GC::GetStats();
    Refused([&]{(void)PreparePortableRuntimeClientObjects();},"Existing constructor-only Client bypassed missing actual Engine");
    SameClient(constructorClient,ReadPortableRuntimeClientSession());
    Refused([&]{(void)PreparePortableRuntimeViewportInput(viewport,Ordinary);},"Input preparation fabricated missing configured native schema");
    Refused([&]{(void)ExecutePortableRuntimeInputQuery(viewport,"KEYNAME 1");},"Key query silently constructed missing Input");
    Empty(ReadPortableRuntimeViewportInput(viewport));GC::Collect();SameGc(initialGc,"Missing-schema Input refusal leaked staged objects");
    Require(temporary.Snapshot("MissingSchemaAfter")==initial,"Missing Input prerequisite changed the whole checkpoint");
    // Late Engine construction is not Client.Init. It must preserve the
    // constructor-only session until the explicit reciprocal binding step.
    Require(PreparePortableRuntimeNativeEngineClasses().prepared&&PreparePortableRuntimeNativeMetadataClasses().prepared&&
        PreparePortableRuntimeNativeConfigClasses().prepared&&PreparePortableRuntimeNativePackages().prepared&&
        PreparePortableRuntimeObjectLookup().prepared&&PreparePortableRuntimeEngineCollision((root/"System"/"DeusEx.ini").string()).prepared,
        "Cannot prepare late actual Engine around the surviving constructor-only Client");
    const auto lateEngine=ReadPortableRuntimeEngineCollision();
    auto indexedConstructor=constructorClient;
    indexedConstructor.lookupGeneration=ReadPortableRuntimeObjectLookup().generation;
    SameClient(indexedConstructor,ReadPortableRuntimeClientSession());EngineClient(lateEngine.enginePath,"");
    const auto beforeInitLookup=ReadPortableRuntimeObjectLookup();const auto beforeInitGc=GC::GetStats();
    const auto beforeInitSave=temporary.Snapshot("LateEngineBeforeInit");
    const auto upgraded=PreparePortableRuntimeClientObjects();
    Require(upgraded.initialized&&upgraded.clientIdentity==client&&upgraded.clientPath==constructorClient.clientPath&&
        upgraded.clientIndex==constructorClient.clientIndex&&upgraded.enginePath==lateEngine.enginePath&&
        upgraded.engineIndex==lateEngine.engineIndex&&upgraded.viewports.size()==1u&&
        upgraded.viewports[0u].identity==viewport&&upgraded.viewports[0u].path==session.viewportPath,
        "Explicit late Client.Init replaced actual Client/Viewport identities or failed reciprocal binding");
    EngineClient(lateEngine.enginePath,upgraded.clientPath);GC::Collect();
    SameGc(beforeInitGc,"Late Client.Init allocated or lost native session objects");
    Require(SameLookup(beforeInitLookup,ReadPortableRuntimeObjectLookup())&&temporary.Snapshot("LateEngineAfterInit")==beforeInitSave,
        "Late Client.Init changed native lookup or actor checkpoint");
    ClearPortableRuntimeClient();GC::Collect();
    Require(PreparePortableRuntimeNativeEngineClasses().prepared&&PreparePortableRuntimeNativeMetadataClasses().prepared&&
        PreparePortableRuntimeNativeConfigClasses().prepared&&PreparePortableRuntimeNativePackages().prepared&&PreparePortableRuntimeNativeInputClasses().prepared&&
        PreparePortableRuntimeObjectLookup().prepared&&PreparePortableRuntimeEngineCollision((root/"System"/"DeusEx.ini").string()).prepared&&
        PreparePortableRuntimeInputSchema().prepared,"Cannot prepare genuine native Input schema dependencies");
    auto active=std::make_shared<QuestVr::NativeClientLifetime>();BindPortableRuntimeClient(active,4201u);
    const auto initializedClient=PreparePortableRuntimeClientObjects();
    Require(initializedClient.initialized&&initializedClient.clientIdentity==4201u&&initializedClient.viewports.empty()&&
        initializedClient.registeredTargets.empty(),"Input fixture Client.Init did not precede explicit actual viewport construction");
    EngineClient(initializedClient.enginePath,initializedClient.clientPath);
    RegisterPortableRuntimeViewport(4201u,{viewport,1440u,1440u,3u,2u});
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
    EngineClient(initializedClient.enginePath,"");
    Refused([&]{(void)ReadPortableRuntimeClientSession();},"Input teardown retained an actual Client payload");
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
    // A bound16-live-viewport array must be reusable across arbitrarily many
    // sequential render sessions, not exhausted by retired generations.
    const auto cycleGc=GC::GetStats();const auto cycleLookup=ReadPortableRuntimeObjectLookup();
    for(std::size_t cycle=0u;cycle<QuestVr::NativeClientRegistry::MaximumViewports+2u;++cycle){
        std::cout<<"SESSION REUSE cycle="<<cycle<<'\n';
        const auto cycleClient=6001u+cycle,cycleViewport=7001u+cycle;
        auto cycleLifetime=std::make_shared<QuestVr::NativeClientLifetime>();
        BindPortableRuntimeClient(cycleLifetime,cycleClient);
        const auto cycleNativeClient=PreparePortableRuntimeClientObjects();
        Require(cycleNativeClient.initialized&&cycleNativeClient.clientIdentity==cycleClient&&
            cycleNativeClient.viewports.empty()&&cycleNativeClient.registeredTargets.empty(),
            "Sequential Client.Init reused retired native array/target state or lacked actual Engine binding");
        EngineClient(cycleNativeClient.enginePath,cycleNativeClient.clientPath);
        const auto beforeStale=ReadPortableRuntimeObjectLookup();const auto beforeStaleGc=GC::GetStats();
        Refused([&]{(void)ReadPortableRuntimePlayerSession(viewport);},"New Client borrowed a retired generation's native viewport");
        Refused([&]{(void)PreparePortableRuntimePlayerObjects(viewport);},"New Client recreated a stale unregistered viewport generation");
        SameClient(cycleNativeClient,ReadPortableRuntimeClientSession());
        Require(SameLookup(beforeStale,ReadPortableRuntimeObjectLookup()),"Stale viewport refusal changed the new Client lookup");
        GC::Collect();SameGc(beforeStaleGc,"Stale viewport refusal changed the new Client's native allocations");
        RegisterPortableRuntimeViewport(cycleClient,{cycleViewport,1440u,1440u,3u,2u});
        const auto registeredClient=ReadPortableRuntimeClientSession();
        Require(registeredClient.viewports.empty()&&registeredClient.registeredTargets.size()==1u,
            "Sequential target registration fabricated an actual native viewport");
        const auto cycleSession=PreparePortableRuntimePlayerObjects(cycleViewport);
        Require(!cycleSession.bound&&cycleSession.viewportIdentity==cycleViewport&&cycleSession.clientIdentity==cycleClient,
            "Sequential session reused a retired native viewport/client identity");
        cycleLifetime->Revoke();ClearPortableRuntimeClient();GC::Collect();
        EngineClient(cycleNativeClient.enginePath,"");
        Refused([&]{(void)ReadPortableRuntimeClientSession();},"Sequential teardown retained native Client/backpointer payload");
        Require(!HasPortableRuntimeClient()&&ReadPortableRuntimeClientView().empty()&&
            !FindPortableRuntimeObject({cycleSession.viewportPath,0u,0u,true,false})&&
            !FindPortableRuntimeObject({cycleSession.clientPath,0u,0u,true,false}),
            "Sequential session retained a retired target or native object lookup");
        const auto cleared=ReadPortableRuntimeObjectLookup();
        Require(cleared.objects==cycleLookup.objects&&cleared.generation==cycleLookup.generation,
            "Sequential session changed persistent lookup census/generation");
        SameGc(cycleGc,"Sequential session retained native Client/Viewport allocations");
    }
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
        ClientOnly(root,packages,temporary);SameGc(baseline,"Fresh Client-only fixture retained roots after explicit shutdown");
        ActualArrayDistance(root,packages,map,temporary);SameGc(baseline,"Actual-array distance fixture retained roots after shutdown");
        Owned(root,packages,map,temporary);ShutdownPortableRuntime();GC::Collect();SameGc(baseline,"Input/session/native Class roots survived explicit runtime shutdown");
        for(const auto& source:sources)source.Unchanged();
        std::cout<<"PASS owned viewport Input integration: "<<checks<<" checks, "<<refusals
            <<" refusals; actual configured Transient InputExt objects/Enum/NoInit/default reload/bootstrap, exact key-query handling, cached config, "
              "zero-target authenticated Client.Init/Engine backpointer, separate ordered actual viewport/target arrays, distinct viewport ownership, "
              "detached snapshots, dormant first Training save/map/GC/revoke/rebind lifetime. NOT Input Init/Reset/ReadInput, "
              "complete viewport FExec fallback, begun startup or live VR weapon/physics acceptance.\n";return 0;
    }catch(const std::exception& error){std::cerr<<"FAIL: "<<error.what()<<'\n';ShutdownPortableRuntime();return 1;}
}
