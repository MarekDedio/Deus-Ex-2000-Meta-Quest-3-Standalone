#include "Precomp.h"
#include "GC/GC.h"
#include "portable_unreal_runtime.h"
#include "portable_viewport_canvas.h"
#include "portable_viewport_console.h"
#include "portable_viewport_input.h"
#include "portable_object_lookup.h"
#include "quest_save_bundle.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string_view>

namespace {
using QuestVr::Vm::Kind;
using QuestVr::Vm::Value;
std::size_t checks{},refusals{};
void Require(bool value,const std::string& why) {++checks;if(!value)throw std::runtime_error(why);}

struct FieldSpec {const char* name;std::uint32_t dimension;};
// Actual Console Children/Next inventory, excluding the native vtblOut address.
constexpr std::array<FieldSpec,56u> fields{{
    {"Viewport",1u},{"HistoryTop",1u},{"HistoryBot",1u},{"HistoryCur",1u},{"TypedStr",1u},{"History",16u},
    {"Scrollback",1u},{"numLines",1u},{"TopLine",1u},{"TextLines",1u},{"MsgTime",1u},{"MsgTickTime",1u},
    {"MsgText",64u},{"MsgType",64u},{"MsgPlayer",64u},{"MsgTick",64u},{"BorderSize",1u},{"ConsoleLines",1u},
    {"BorderLines",1u},{"BorderPixels",1u},{"ConsolePos",1u},{"ConsoleDest",1u},{"FrameX",1u},{"FrameY",1u},
    {"ConBackground",1u},{"Border",1u},{"bNoStuff",1u},{"bTyping",1u},{"bNoDrawWorld",1u},{"bTimeDemo",1u},
    {"bStartTimeDemo",1u},{"bRestartTimeDemo",1u},{"bSaveTimeDemoToFile",1u},{"StartTime",1u},{"ExtraTime",1u},
    {"LastFrameTime",1u},{"LastSecondStartTime",1u},{"FrameCount",1u},{"LastSecondFrameCount",1u},{"MinFPS",1u},
    {"MaxFPS",1u},{"LastSecFPS",1u},{"TimeDemoFont",1u},{"LoadingMessage",1u},{"SavingMessage",1u},
    {"ConnectingMessage",1u},{"PausedMessage",1u},{"PrecachingMessage",1u},{"FrameRateText",1u},{"AvgText",1u},
    {"LastSecText",1u},{"MinText",1u},{"MaxText",1u},{"fpsText",1u},{"SecondsText",1u},{"FramesText",1u}
}};
const Value& Field(const PortableViewportConsoleSnapshot& console,const char* name,std::uint32_t index=0u) {
    const auto key="Engine.Console."+std::string(name);
    const auto found=std::find_if(console.fields.begin(),console.fields.end(),[&](const auto& field) {
        return field.declaration==key&&field.index==index;
    });
    Require(found!=console.fields.end(),"Console omitted indexed field "+key+'['+std::to_string(index)+']');
    return found->value;
}
std::vector<std::uint8_t> Save(const std::filesystem::path& path) {
    Require(SavePortableRuntimeState(path.string()),"Cannot checkpoint Console fixture");
    std::vector<std::uint8_t> bytes;
    Require(QuestVr::ReadBoundedSaveFile(path.string(),QuestVr::kMaximumSaveRuntimeBytes,bytes),"Cannot read Console checkpoint");
    return bytes;
}
void Same(const PortableViewportConsoleSnapshot& a,const PortableViewportConsoleSnapshot& b) {
    Require(a.prepared==b.prepared&&a.initialized==b.initialized&&a.outputDeviceBound==b.outputDeviceBound&&
        a.hasStack==b.hasStack&&a.frameOverride==b.frameOverride&&a.codePresent==b.codePresent&&
        a.objectPath==b.objectPath&&a.classPath==b.classPath&&a.outerPath==b.outerPath&&
        a.ownerViewportPath==b.ownerViewportPath&&a.viewportPath==b.viewportPath&&a.nodePath==b.nodePath&&a.stateNodePath==b.stateNodePath&&
        a.objectIndex==b.objectIndex&&a.classIndex==b.classIndex&&a.outerIndex==b.outerIndex&&
        a.ownerViewportIndex==b.ownerViewportIndex&&a.viewportIndex==b.viewportIndex&&a.nodeIndex==b.nodeIndex&&a.stateNodeIndex==b.stateNodeIndex&&
        a.objectFlags==b.objectFlags&&a.uniqueCounter==b.uniqueCounter&&a.probeMask==b.probeMask&&a.stateLocalsRevision==b.stateLocalsRevision&&
        a.inventoryCapacity==b.inventoryCapacity&&a.inventoryBytes==b.inventoryBytes&&a.propertyRootBytes==b.propertyRootBytes&&
        a.frozenDefaultOverrides==b.frozenDefaultOverrides&&a.fields.size()==b.fields.size(),
        "Read-only/rolled-back Console changed identity, frame, defaults or retained accounting");
    for(std::size_t i=0u;i<a.fields.size();++i)
        Require(a.fields[i].declaration==b.fields[i].declaration&&a.fields[i].index==b.fields[i].index&&
            QuestVr::Vm::Equal(a.fields[i].value,b.fields[i].value),"Read-only/rolled-back Console changed an indexed field");
}
void Original(const PortableViewportConsoleSnapshot& console,std::uint64_t viewport) {
    const auto owner=ReadPortableRuntimePlayerSession(viewport);
    const auto ownerObject=ReadPortableRuntimeObjectOwnership(owner.viewportPath);
    std::cout<<"CONSOLE HEADER path="<<console.objectPath<<" class="<<console.classPath<<" outer="<<console.outerPath
        <<" owner="<<console.ownerViewportPath<<" viewport="<<console.viewportPath<<" flags="<<std::hex<<console.objectFlags<<std::dec
        <<" object/class/outer/owner/viewport="<<console.objectIndex<<'/'<<console.classIndex<<'/'<<console.outerIndex<<'/'
        <<console.ownerViewportIndex<<'/'<<console.viewportIndex<<" expectedViewport="<<ownerObject.objectIndex<<" fields="<<console.fields.size()<<'\n';
    Require(console.prepared&&console.initialized&&console.outputDeviceBound&&console.classPath=="Engine.Console"&&
        console.outerPath=="Transient"&&console.ownerViewportPath==owner.viewportPath&&console.viewportPath==owner.viewportPath&&
        console.objectFlags==0x02004000u&&console.objectIndex&&console.classIndex&&console.outerIndex&&
        console.ownerViewportIndex==ownerObject.objectIndex&&console.viewportIndex==ownerObject.objectIndex&&console.fields.size()==323u,
        "Console identity/Transient Outer/flags/owning and reflected Viewport graph differs");
    Require(console.hasStack&&console.frameOverride&&!console.codePresent&&console.nodePath==console.classPath&&
        console.stateNodePath==console.classPath&&console.nodeIndex==console.classIndex&&console.stateNodeIndex==console.classIndex&&
        console.probeMask==std::numeric_limits<std::uint64_t>::max()&&console.stateLocalsRevision==1u,
        "Console InitExecution omitted original class Node/StateNode, null Code, all-one probes or actual frame revision");
    std::size_t slots{};
    for(const auto& spec:fields)for(std::uint32_t index=0u;index<spec.dimension;++index) {
        ++slots;const auto& actual=Field(console,spec.name,index);
        Value expected;
        if(std::string_view(spec.name)=="Viewport")expected=Value::Text(Kind::Object,owner.viewportPath);
        else if(std::string_view(spec.name)=="TopLine")expected=Value::Integer(63);
        else if(std::string_view(spec.name)=="BorderSize")expected=Value::Integer(1);
        else expected=ReadPortableClassDefault("Engine.Console",spec.name,index);
        Require(QuestVr::Vm::Equal(actual,expected),"Console constructor/Init changed copied indexed CDO field "+
            std::string(spec.name)+'['+std::to_string(index)+']');
    }
    Require(slots==323u,"Console test inventory does not cover all non-address slots");
    Require(Field(console,"Viewport").kind==Kind::Object&&Field(console,"TopLine").kind==Kind::Int&&
        Field(console,"BorderSize").kind==Kind::Int&&Field(console,"bTyping").kind==Kind::Bool&&!Field(console,"bTyping").boolean,
        "Console Init stored a guessed Boolean BorderSize or started Typing");
    const auto input=ReadPortableRuntimeViewportInput(viewport);
    Require(!owner.bound&&input.constructed&&!input.initialized&&input.execInstalled&&!input.nativeViewportIndex,
        "Console initialization fabricated possession/Input.Init or lost the constructed Input.Exec provider");
}
void Actual(const PortableViewportConsoleSnapshot& console) {
    const auto actual=ReadPortableRuntimeObjectOwnership(console.objectPath);
    Require(actual.objectPath==console.objectPath&&actual.classPath==console.classPath&&actual.outerPath==console.outerPath&&
        actual.objectIndex==console.objectIndex&&actual.classIndex==console.classIndex&&actual.outerIndex==console.outerIndex&&!actual.serializedOuterZero,
        "Console snapshot is not backed by its genuine indexed UObject header");
    const auto found=FindPortableRuntimeObject({console.objectPath,console.classIndex,0u,true,true});
    Require(found&&found->objectIndex==console.objectIndex&&found->classIndex==console.classIndex&&found->outerIndex==console.outerIndex,
        "Console did not publish into the actual weak object lookup");
    const auto leaf=console.objectPath.substr(console.objectPath.find_last_of('.')+1u);
    const auto scoped=FindPortableRuntimeObject({leaf,console.classIndex,console.outerIndex,false,true});
    Require(scoped&&scoped->objectIndex==console.objectIndex,"Console leaf-name/actual-Outer lookup disagrees with qualified lookup");
    for(const auto& field:console.fields)
        Require(QuestVr::Vm::Equal(field.value,ReadPortableRuntimeDeclaredScriptProperty(console.objectPath,field.declaration,field.index)),
            "Console snapshot differs from its actual reflected instance storage");
    Require(ReadPortableRuntimeDeclaredScriptProperty(console.objectPath,"Core.Object.Class").text==console.classPath&&
        ReadPortableRuntimeDeclaredScriptProperty(console.objectPath,"Core.Object.Outer").text==console.outerPath&&
        ReadPortableRuntimeDeclaredScriptProperty(console.objectPath,"Core.Object.Name").text==console.objectPath.substr(console.objectPath.find_last_of('.')+1u)&&
        ReadPortableRuntimeDeclaredScriptProperty(console.objectPath,"Core.Object.ObjectFlags").integer==0x02004000,
        "Console constructor or InitExecution changed only detached header/flag snapshots");
}
void RefusedCleanly(std::uint64_t viewport,const std::string& ini,const std::filesystem::path& checkpoint,
    const QuestVr::Vm::Limits& limits={}) {
    GC::Collect();const auto before=GC::GetStats();const auto lookup=ReadPortableRuntimeObjectLookup();
    const auto revision=GetPortableRuntimeWorldRevision();const auto saved=Save(checkpoint);
    const auto console=ReadPortableRuntimeViewportConsole(viewport);
    const auto failed=PreparePortableRuntimeViewportConsole(viewport,ini,true,limits);
    Require(!failed.execution.passed()&&!failed.execution.committed&&!failed.execution.error.empty(),
        "Console ignored a missing constructor prerequisite or shared execution budget");++refusals;
    GC::Collect();const auto after=GC::GetStats();const auto current=ReadPortableRuntimeObjectLookup();
    Same(console,ReadPortableRuntimeViewportConsole(viewport));
    Require(after.numObjects==before.numObjects&&after.memoryUsage==before.memoryUsage&&
        current.objects==lookup.objects&&current.generation==lookup.generation&&current.retainedBytes==lookup.retainedBytes&&
        GetPortableRuntimeWorldRevision()==revision&&Save(checkpoint)==saved,
        "Console refusal retained native objects/index entries/counters/save/revision state");
}
}

int main(int argc,char** argv) {
    std::filesystem::path temporary;std::shared_ptr<QuestVr::NativeClientLifetime> lifetime;
    try {
        std::cout.setf(std::ios::unitbuf);
        std::filesystem::path root;
        if(argc==3&&std::string(argv[1])=="--game-root")root=argv[2];
        else if(argc==1) {if(const auto* value=std::getenv("DEUSEX_GAME_ROOT"))root=value;}
        else throw std::runtime_error("Usage: portable_viewport_console_test [--game-root OWNED_GAME_ROOT]");
        if(root.empty()){std::cout<<"SKIP owned Console: supply --game-root\n";return 77;}
        ShutdownPortableRuntime();GC::Collect();const auto initial=GC::GetStats();
        std::vector<PortablePackageTables> sources;
        for(const auto* name:{"Core","Engine","DeusEx","Extension","ConSys","IpDrv","IpServer","UBrowser","UWindow","Editor","Fire",
            "MPCharacters","DeusExSounds","DeusExDeco","DeusExItems","DeusExCharacters"})
            sources.push_back(LoadPortablePackageTables((root/"System"/(std::string(name)+".u")).string()));
        Require(InitializePortableRuntime(sources).passed,"Cannot initialize owned Console graph");
        const auto ini=(root/"System"/"DeusEx.ini").string();
        Require(PreparePortableRuntimeNativeEngineClasses().prepared&&PreparePortableRuntimeNativeMetadataClasses().prepared&&
            PreparePortableRuntimeNativeConfigClasses().prepared&&PreparePortableRuntimeNativePackages().prepared&&
            PreparePortableRuntimeNativeInputClasses().prepared&&PreparePortableRuntimeObjectLookup().prepared&&
            PreparePortableRuntimeEngineCollision(ini).prepared&&PreparePortableRuntimeInputSchema().prepared&&
            PreparePortableRuntimeInputConfig((root/"System"/"User.ini").string()).prepared,"Cannot prepare actual native Console dependencies");
        const auto map=LoadPortablePackageTables((root/"Maps"/"00_Training.dx").string());
        Require(LoadPortableRuntimeMap(map).passed,"Cannot load first Training");
        lifetime=std::make_shared<QuestVr::NativeClientLifetime>();BindPortableRuntimeClient(lifetime,991u);
        for(const auto viewport:{992u,993u,994u}) {
            RegisterPortableRuntimeViewport(991u,{viewport,1440u,1440u,3u,2u});
            static_cast<void>(PreparePortableRuntimePlayerObjects(viewport));
        }
        temporary=std::filesystem::temp_directory_path()/("quest-console-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        Require(std::filesystem::create_directory(temporary),"Cannot create Console diagnostic directory");
        const auto checkpoint=temporary/"checkpoint.sav";
        RefusedCleanly(992u,ini,checkpoint); // Neither Canvas nor Input may be fabricated.
        Require(PreparePortableRuntimeViewportCanvas(992u,ini).execution.passed(),"Cannot prepare Console's prior Canvas");
        RefusedCleanly(992u,ini,checkpoint); // Canvas alone does not authorize Console.
        Require(PreparePortableRuntimeViewportInput(992u,PortableInputLoggingProfile::Ordinary).constructed,"Cannot construct Console's prior Input");
        Require(PreparePortableRuntimeViewportCanvas(993u,ini).execution.passed()&&
            PreparePortableRuntimeViewportInput(993u,PortableInputLoggingProfile::Ordinary).constructed,"Cannot prepare second Console owner");
        Require(PreparePortableRuntimeViewportInput(994u,PortableInputLoggingProfile::Ordinary).constructed,"Cannot prepare missing-Canvas control");
        RefusedCleanly(994u,ini,checkpoint); // An Input-only viewport still lacks original Canvas order.
        RefusedCleanly(992u,std::string{},checkpoint);
        RefusedCleanly(992u,(temporary/"missing.ini").string(),checkpoint);

        GC::Collect();const auto before=GC::GetStats();const auto lookup=ReadPortableRuntimeObjectLookup();
        const auto revision=GetPortableRuntimeWorldRevision();const auto inventory=ReadPortableRuntimeViewportConsole(992u);
        const auto baseline=Save(checkpoint);
        const auto unchanged=[&] {
            GC::Collect();const auto gc=GC::GetStats();const auto current=ReadPortableRuntimeObjectLookup();
            Same(inventory,ReadPortableRuntimeViewportConsole(992u));
            Require(gc.numObjects==before.numObjects&&gc.memoryUsage==before.memoryUsage&&current.objects==lookup.objects&&
                current.generation==lookup.generation&&current.retainedBytes==lookup.retainedBytes&&
                GetPortableRuntimeWorldRevision()==revision&&Save(checkpoint)==baseline,
                "Console probe retained native frame/objects/index/save/revision state");
        };
        const auto probe=PreparePortableRuntimeViewportConsole(992u,ini,false);
        Require(!probe.execution.passed()&&!probe.execution.committed&&probe.execution.error==
            "Non-publishing Console initialization probe completed; rollback required",
            "Console failed before complete original initialization: "+probe.execution.error);
        Original(probe.console,992u);unchanged();
        for(std::size_t writes=0u;writes<10u;++writes) {
            QuestVr::Vm::Limits limits;limits.writes=writes;RefusedCleanly(992u,ini,checkpoint,limits);unchanged();
        }
        for(const auto work:{0u,1u,8u}) {
            QuestVr::Vm::Limits limits;limits.nativeWork=work;RefusedCleanly(992u,ini,checkpoint,limits);unchanged();
        }
        for(const auto bytes:{0u,1u}) {
            QuestVr::Vm::Limits limits;limits.retainedBytes=bytes;RefusedCleanly(992u,ini,checkpoint,limits);unchanged();
        }
        const auto ready=PreparePortableRuntimeViewportConsole(992u,ini);
        Require(ready.execution.passed()&&ready.execution.committed&&ready.execution.writes==10u,
            "Console constructor/InitExecution failed: "+ready.execution.error);
        Original(ready.console,992u);Actual(ready.console);
        Require(ready.console.inventoryBytes>=ready.console.propertyRootBytes&&ready.console.propertyRootBytes>0u&&ready.console.inventoryCapacity>0u,
            "Console accounting omitted reflected GC roots/native inventory allocation");
        Require(ready.console.frozenDefaultOverrides==0u,
            "Unmodified owned Console CDO redundantly copied immutable tagged/typed-zero defaults");
        Require(ready.console.objectPath==probe.console.objectPath&&ready.console.uniqueCounter==probe.console.uniqueCounter,
            "Console rollback changed its genuine unique-name counter");
        Same(ready.console,ReadPortableRuntimeViewportConsole(992u));
        const auto again=PreparePortableRuntimeViewportConsole(992u,ini);
        Require(again.execution.passed()&&again.execution.committed&&again.execution.writes==0u,"Idempotent Console initialization ran additional stores");
        Same(ready.console,again.console);
        const auto commandCheckpoint=Save(checkpoint);GC::Collect();const auto commandGc=GC::GetStats();
        const auto commandLookup=ReadPortableRuntimeObjectLookup();
        const auto unknownName=NameString::FindCompareIndex("QuestAbsentConsolePrefix20261028");
        for(const auto* command:{"", " \t", "SET InputExt T Talk", "KeyEvent", "QuestAbsentConsolePrefix20261028"}) {
            const auto queried=ExecutePortableRuntimeConsoleExecPrefix(992u,command);
            Require(queried.execution.passed()&&queried.execution.committed&&!queried.handled&&!queried.execution.writes,
                "Actual Console miss/non-Exec did not decline cleanly: "+queried.execution.error);
        }
        Require(NameString::FindCompareIndex("QuestAbsentConsolePrefix20261028")==unknownName,
            "Actual Console command lookup interned a missing name");
        for(const auto* command:{"TimeDemo", "Talk", "TeamTalk"}) {
            const auto queried=ExecutePortableRuntimeConsoleExecPrefix(992u,command);
            Require(!queried.execution.passed()&&!queried.execution.committed&&!queried.handled&&
                queried.execution.error==std::string("Original ScriptConsoleExec command parameter/call service is not implemented: Engine.Console.")+command,
                "Actual Console Exec match became a fabricated decline/success: "+queried.execution.error);++refusals;
        }
        for(const auto* actor:{"00_Training.LevelInfo0","00_Training.Doctor1","00_Training.WeaponCrowbar1"}) {
            for(const auto* command:{"SET InputExt T Talk","QuestAbsentActorConsolePrefix20261028","Tick"}) {
                const auto queried=ExecutePortableRuntimeActorConsoleExecPrefix(actor,command);
                Require(queried.execution.passed()&&queried.execution.committed&&!queried.handled&&!queried.execution.writes,
                    "Actual Actor ScriptConsoleExec miss/non-Exec did not decline cleanly: "+queried.execution.error);
            }
        }
        const auto classReceiver=ExecutePortableRuntimeActorConsoleExecPrefix("Engine.PlayerPawn","SET InputExt T Talk");
        Require(!classReceiver.execution.passed()&&!classReceiver.execution.committed,"ScriptConsoleExec acquired an invented Actor instance from a UClass");++refusals;
        Require(!NameString::FindCompareIndex("QuestAbsentActorConsolePrefix20261028"),"Actor command provider interned an unknown name");
        for(const auto work:{0u,1u,4u}) {
            QuestVr::Vm::Limits limits;limits.nativeWork=work;
            const auto queried=ExecutePortableRuntimeConsoleExecPrefix(992u,"SET InputExt T Talk",limits);
            Require(!queried.execution.passed()&&!queried.execution.committed,"Console prefix ignored the shared native work limit");++refusals;
        }
        for(const auto bytes:{0u,1u}) {
            QuestVr::Vm::Limits limits;limits.retainedBytes=bytes;
            const auto queried=ExecutePortableRuntimeConsoleExecPrefix(992u,"SET InputExt T Talk",limits);
            Require(!queried.execution.passed()&&!queried.execution.committed,"Console prefix ignored retained admission");++refusals;
        }
        GC::Collect();const auto afterCommands=GC::GetStats();const auto afterCommandLookup=ReadPortableRuntimeObjectLookup();
        Same(ready.console,ReadPortableRuntimeViewportConsole(992u));
        Require(Save(checkpoint)==commandCheckpoint&&GetPortableRuntimeWorldRevision()==revision&&
            commandGc.numObjects==afterCommands.numObjects&&commandGc.memoryUsage==afterCommands.memoryUsage&&
            commandLookup.objects==afterCommandLookup.objects&&commandLookup.generation==afterCommandLookup.generation&&
            commandLookup.retainedBytes==afterCommandLookup.retainedBytes,
            "Console command prefixes changed whole save/frame/GC/lookup/revision state");
        for(const auto* raw:{"Engine.Console.vtblOut","Core.Object.ObjectInternal"}) {
            bool refused{};try{static_cast<void>(ReadPortableRuntimeDeclaredScriptProperty(ready.console.objectPath,raw));}
            catch(const std::exception&){refused=true;}
            Require(refused,"Console exposed an unauthenticated native pointer as a script value");++refusals;
        }
        GC::Collect();Same(ready.console,ReadPortableRuntimeViewportConsole(992u));Actual(ReadPortableRuntimeViewportConsole(992u));
        const auto other=PreparePortableRuntimeViewportConsole(993u,ini);
        Require(other.execution.passed()&&other.execution.committed&&other.console.objectIndex!=ready.console.objectIndex&&
            other.console.objectPath!=ready.console.objectPath,"Distinct viewport reused Console ownership");
        Original(other.console,993u);Actual(other.console);
        RefusedCleanly(994u,ini,checkpoint); // Preserve a nonempty inventory's capacity and bytes on refusal too.
        Require(Save(checkpoint)==baseline&&GetPortableRuntimeWorldRevision()==revision,"Console changed map/checkpoint/world state");
        Require(LoadPortableRuntimeMap(map).passed,"Map reload failed with genuine Console ownership");
        const auto reloadRevision=GetPortableRuntimeWorldRevision();
        GC::Collect();const auto retained=ReadPortableRuntimeViewportConsole(992u);
        Require(retained.objectIndex==ready.console.objectIndex&&retained.objectPath==ready.console.objectPath&&
            retained.stateLocalsRevision==ready.console.stateLocalsRevision&&retained.inventoryCapacity==other.console.inventoryCapacity&&
            retained.inventoryBytes==other.console.inventoryBytes,"Map reload discarded/reinitialized viewport Console or its inventory");
        Original(retained,992u);Actual(retained);
        Require(Save(checkpoint)==baseline&&GetPortableRuntimeWorldRevision()==reloadRevision,"Map-retained Console leaked into world saves/revision");
        lifetime->Revoke();ClearPortableRuntimeClient();lifetime.reset();GC::Collect();
        bool revoked{};try{static_cast<void>(ReadPortableRuntimeViewportConsole(992u));}catch(const std::exception&){revoked=true;}
        Require(revoked,"Revoked Console exposed a live viewport service");++refusals;
        const auto revokedCommand=ExecutePortableRuntimeConsoleExecPrefix(992u,"SET InputExt T Talk");
        Require(!revokedCommand.execution.passed()&&!revokedCommand.execution.committed,"Revoked Console command provider remained callable");++refusals;
        for(const auto& path:{ready.console.objectPath,other.console.objectPath}) {
            Require(!FindPortableRuntimeObject({path,0u,0u,true,false}),"Retired Console survived weak object lookup");
            bool retired{};try{static_cast<void>(ReadPortableRuntimeObjectOwnership(path));}catch(const std::exception&){retired=true;}
            Require(retired,"Retired Console retained canonical UObject ownership");++refusals;
        }
        ShutdownPortableRuntime();GC::Collect();const auto final=GC::GetStats();
        Require(final.numObjects==initial.numObjects&&final.memoryUsage==initial.memoryUsage,"Console lifecycle retained GC objects or native frame storage");
        std::filesystem::remove(checkpoint);std::filesystem::remove(temporary);temporary.clear();
        std::cout<<"PASS original Console constructor/InitExecution: "<<checks<<" checks / "<<refusals
            <<" refusals; 323 copied indexed fields, genuine header/Viewport/frame/probes, constructor order, rollback/name/index/save/GC/accounting, "
            "distinct viewports, actual ScriptConsoleExec lookup/miss/non-Exec/match-refusal, map retention and client teardown. "
            "Not matched parameter execution, Typing, OpenWindow, Input.Init, full FExec or VR gameplay.\n";
        return 0;
    }catch(const std::exception& error) {
        if(lifetime)lifetime->Revoke();
        ClearPortableRuntimeClient();ShutdownPortableRuntime();GC::Collect();
        std::cerr<<error.what()<<'\n';return 1;
    }
}
