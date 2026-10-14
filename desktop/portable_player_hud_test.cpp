#include "Precomp.h"
#include "GC/GC.h"
#include "portable_unreal_runtime.h"
#include "portable_model_geometry.h"
#include "quest_script_state.h"
#include "quest_save_bundle.h"
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
using QuestVr::Vm::Evaluation;
using QuestVr::Vm::Kind;
using QuestVr::Vm::Value;
std::size_t checks{};
void Require(bool condition,const std::string& message) {
    if(!condition) throw std::runtime_error(message);
    ++checks;
}
std::vector<std::uint8_t> Bytes(const std::filesystem::path& path) {
    const auto size=std::filesystem::file_size(path);
    Require(size<=64u*1024u*1024u,"Player checkpoint exceeds read bound");
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
    std::ifstream file(path,std::ios::binary);
    Require(static_cast<bool>(file),"Cannot read player checkpoint");
    file.read(reinterpret_cast<char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));
    Require(static_cast<bool>(file),"Player checkpoint truncated");
    return bytes;
}
QuestVr::Vm::Result Call(const std::string& actor,const std::string& function,
    const std::vector<Evaluation>& arguments={}) {
    auto result=ExecutePortableActorFunction(actor,function,arguments);
    Require(result.passed() && result.committed,function+" failed: "+result.error+
        " at "+result.function+':'+std::to_string(result.offset));
    return result;
}
Value Declared(const std::string& actor,const std::string& field,Kind kind) {
    auto value=ReadPortableActorDeclaredScriptProperty(actor,field);
    Require(value.kind==kind,"Player field has wrong declared type: "+field);
    return value;
}
std::size_t ScriptOffset(const std::vector<std::uint8_t>& bytes) {
    std::size_t cursor=8u;
    const auto word=[&]() {
        Require(cursor<=bytes.size() && bytes.size()-cursor>=4u,"Player generated checkpoint word truncated");
        std::uint32_t value{};for(unsigned i=0u;i<4u;++i) value|=std::uint32_t(bytes[cursor+i])<<(i*8u);
        cursor+=4u;return value;
    };
    const auto skip=[&](std::size_t count) {
        Require(cursor<=bytes.size() && count<=bytes.size()-cursor,"Player generated checkpoint prefix truncated");cursor+=count;
    };
    const auto string=[&]() {const auto size=word();Require(size<=1048576u,"Player generated string exceeds bound");skip(size);};
    const auto list=[&]() {const auto count=word();Require(count<=100000u,"Player generated list exceeds bound");for(std::uint32_t i=0u;i<count;++i) string();};
    list();list();list();skip(4u);
    const auto damaged=word();Require(damaged<=100000u,"Player generated damaged list exceeds bound");
    for(std::uint32_t i=0u;i<damaged;++i) {string();skip(4u);}
    skip(8u);list();list();list();list();
    const auto offset=cursor;
    const auto size=word();
    Require(size>0u && cursor<=bytes.size() && size==bytes.size()-cursor,"Player script blob does not cover exact save tail");
    return offset;
}
std::vector<std::uint8_t> InjectTestBegunWorld(const std::vector<std::uint8_t>& bytes,
    const std::string& level,const std::string& classPath) {
    const auto offset=ScriptOffset(bytes);
    auto graph=QuestVr::DecodeScriptSavedState({bytes.begin()+static_cast<std::ptrdiff_t>(offset+4u),bytes.end()});
    QuestVr::ScriptSavedObject info{level,classPath,{}, {}, {}, {}};
    info.properties.push_back({"Engine.LevelInfo.bBegunPlay","bBegunPlay",0u,Value::Bool(true)});
    graph.objects.push_back(std::move(info));
    const auto blob=QuestVr::EncodeScriptSavedState(graph);
    Require(blob.size()<=16u*1024u*1024u,"Player generated script blob exceeds independent bound");
    std::vector<std::uint8_t> result(bytes.begin(),bytes.begin()+static_cast<std::ptrdiff_t>(offset+4u));
    for(unsigned i=0u;i<4u;++i) result[offset+i]=static_cast<std::uint8_t>(blob.size()>>(i*8u));
    result.insert(result.end(),blob.begin(),blob.end());return result;
}
void VerifyRing(const std::string& player,const std::string& ring) {
    Require(Declared(player,"DeusEx.DeusExPlayer.KeyRing",Kind::Object).text==ring,
        "Original player lost actual KeyRing identity");
    Require(Declared(player,"Engine.Actor.Inventory",Kind::Object).text==ring,
        "GiveTo/AddInventory did not retain the actual player Inventory head");
    Require(Declared(ring,"Engine.Actor.Owner",Kind::Object).text==player &&
        Declared(ring,"Engine.Actor.Base",Kind::Object).text==player &&
        Declared(ring,"Engine.Actor.Instigator",Kind::Object).text==player,
        "Starting ring is not actually owned, based and given to its player");
    Require(Declared(ring,"Engine.Actor.Inventory",Kind::Object).text.empty(),
        "Fresh ring has a fabricated Inventory successor");
    Require(Declared(ring,"Engine.Actor.InitialState",Kind::Name).text=="Idle2" &&
        Call(ring,"GetStateName").value.text=="Idle2",
        "Original ring did not enter its real Idle2 state");
    Require(Declared(ring,"Engine.Actor.bHidden",Kind::Bool).boolean &&
        !Declared(ring,"Engine.Inventory.bDisplayableInv",Kind::Bool).boolean &&
        Declared(ring,"Engine.Actor.bCarriedItem",Kind::Bool).boolean,
        "Original ring hidden/inventory/carried flags were replaced by icon state");
    Require(Declared(ring,"Engine.Actor.Physics",Kind::Byte).integer==0,
        "Original BecomeItem did not stop ring physics");
    Require(Declared(player,"Engine.PlayerPawn.Player",Kind::Object).text.empty(),
        "Isolated keyring control fabricated native possession");
    const auto hud=ReadPortablePlayerHudSnapshot(player);
    Require(hud.available && !hud.bound && hud.playerPath==player && hud.keyRingPath==ring &&
        hud.inventory.size()==1u && hud.inventory.front().actorPath==ring &&
        hud.inventory.front().classPath=="DeusEx.NanoKeyRing" &&
        std::all_of(hud.limbHealth.begin(),hud.limbHealth.end(),[](auto health) {return health==100;}),
        "Original player HUD snapshot replaced actual limb or owned inventory state");
}
}

int main(int argc,char** argv) {
    std::filesystem::path directory;
    bool ownedDirectory{};
    try {
        if(argc==1) {std::cout<<"SKIP: supply owned Deus Ex installation for original player/keyring execution\n";return 77;}
        if(argc!=2) throw std::runtime_error("Usage: portable_player_hud_test GAME_ROOT");
        const auto root=std::filesystem::path(argv[1]);
        std::vector<PortablePackageTables> scripts;
        // Match the production runtime's owned package graph. A deliberately
        // missing mesh package must not masquerade as a possession/keyring gap.
        for(const auto* name:{"ConSys","Core","DeusEx","DeusExCharacters","DeusExConAudioAIBarks",
            "DeusExConAudioEndGame","DeusExConAudioHK_Shared","DeusExConAudioIntro",
            "DeusExConAudioMission00","DeusExConAudioMission01","DeusExConAudioMission02",
            "DeusExConAudioMission03","DeusExConAudioMission04","DeusExConAudioMission05",
            "DeusExConAudioMission08","DeusExConAudioMission09","DeusExConAudioMission10",
            "DeusExConAudioMission11","DeusExConAudioMission12","DeusExConAudioMission14",
            "DeusExConAudioMission15","DeusExConAudioNYShared","DeusExConText",
            "DeusExConversations","DeusExDeco","DeusExItems","DeusExSounds","DeusExText",
            "DeusExUI","Editor","Engine","Extension","Fire","IpDrv","IpServer",
            "MPCharacters","UBrowser","UWindow"})
            scripts.push_back(LoadPortablePackageTables((root/"System"/(std::string(name)+".u")).string()));
        Require(InitializePortableRuntime(scripts).passed,"Original player runtime initialization failed");
        const auto map=LoadPortablePackageTables((root/"Maps"/"00_Training.dx").string());
        Require(LoadPortableRuntimeMap(map).passed,"Original player Training map failed");
        const auto north=ReadPortableRuntimeMapNorth();
        Require(north.found && north.trueNorth==0 &&
            Declared(north.actorPath,"DeusEx.DeusExLevelInfo.TrueNorth",Kind::Int).integer==0,
            "Training compass did not read the actual first Level-slot TrueNorth");
        const auto order=ReadPortableLevel68ActorOrder(map);
        Require(!order.empty() && order.front()>0,"Original player map has no slot-zero LevelInfo");
        const auto level=std::filesystem::path(map.sourcePath).stem().string()+'.'+GetPortableObjectPath(map,order.front());
        Require(!Declared(level,"Engine.LevelInfo.bBegunPlay",Kind::Bool).boolean,
            "Isolated player control unexpectedly started the world");
        auto actors=GetPortableRuntimeMapActors(true);
        const auto start=std::find_if(actors.begin(),actors.end(),[](const auto& actor) {
            return actor.classPath=="Engine.PlayerStart";
        });
        Require(start!=actors.end() && start->hasLocation,"Original Training PlayerStart not indexed");
        // Explicit native birth in a dormant world is an isolated script control,
        // NOT original Login/Possess or accepted startup. No callback is skipped
        // by this test: the original begun-play ProcessEvent gate is still clear.
        const auto spawned=Call(level,"Engine.Actor.Spawn",{
            {Value::Text(Kind::Object,"DeusEx.JCDentonMale"),{}},
            {Value::Text(Kind::Object,{}),{}},{Value::Text(Kind::Name,{}),{}},
            {Value::Vector({start->x,start->y,start->z}),{}},
            {Value::Rotator({start->pitch,start->yaw,start->roll}),{}}});
        Require(spawned.value.kind==Kind::Object && !spawned.value.text.empty(),
            "Original native player birth returned None");
        const auto player=spawned.value.text;
        Require(Declared(player,"DeusEx.DeusExPlayer.KeyRing",Kind::Object).text.empty(),
            "Player native birth fabricated a keyring without original bytecode");
        const auto stamp=std::chrono::steady_clock::now().time_since_epoch().count();
        directory=std::filesystem::temp_directory_path()/("deusex-original-player-"+std::to_string(stamp));
        ownedDirectory=std::filesystem::create_directory(directory);
        Require(ownedDirectory,"Cannot create unique player checkpoint directory");
        const auto baselinePath=directory/"baseline.sav",inspectPath=directory/"inspect.sav",ringPath=directory/"ring.sav";
        Require(SavePortableRuntimeState(baselinePath.string()),"Cannot checkpoint dormant original player");
        const auto baseline=Bytes(baselinePath);
        const auto count=GetPortableRuntimeMapActors(true).size();
        const auto windowRevision=GetPortableRuntimeWorldRevision();
        const auto windowBaseline=ReadPortableRuntimeNativeWindowSummary();
        Require(windowBaseline.configuredRootClass==std::optional<std::string>("DeusEx.DeusExRootWindow") &&
            windowBaseline.objects==0u && windowBaseline.roots==0u,"Original Root INI/native window baseline is wrong");
        const auto windowStats=GC::GetStats();
        const auto pending=ProbePortableRootWindowInitialization(player);
        Require(!pending.execution.passed() && !pending.execution.committed && pending.inspectionError.empty() &&
            pending.execution.function=="DeusEx.DeusExRootWindow.InitWindow" && pending.execution.offset==24u &&
            pending.execution.error.find("1410")!=std::string::npos && pending.objects.size()==3u &&
            pending.rootedWindows==std::vector<std::string>{pending.pawnRootPath},
            "Original root prefix probe did not capture the actual pending native graph: "+pending.inspectionError);
        const auto object=[&](const std::string& path)->const PortableNativeWindowObjectSnapshot& {
            const auto found=std::find_if(pending.objects.begin(),pending.objects.end(),[&](const auto& view) {return view.path==path;});
            Require(found!=pending.objects.end(),"Pending root prefix has no native object: "+path);return *found;
        };
        const auto field=[&](const PortableNativeWindowObjectSnapshot& view,const std::string& key)->const PortableNativeWindowFieldSnapshot& {
            const auto found=std::find_if(view.fields.begin(),view.fields.end(),[&](const auto& entry) {return entry.declaration==key;});
            Require(found!=view.fields.end(),"Pending root prefix has no declared field: "+key);return *found;
        };
        const auto edge=[&](const PortableNativeWindowObjectSnapshot& view,const std::string& key,const std::string& target) {
            const auto& entry=field(view,key);
            Require(entry.value.kind==Kind::Object && entry.value.text==target &&
                entry.propertyRoots==(target.empty()?std::vector<std::string>{}:std::vector<std::string>{target}),
                "Native window reflected/GC edge differs from the original callback: "+key);
        };
        const auto& rootView=object(pending.pawnRootPath);
        Require(rootView.classPath=="DeusEx.DeusExRootWindow" && rootView.outerPath==player,
            "Native root did not use the original class and actual Pawn Outer");
        edge(rootView,"Extension.RootWindow.parentPawn",player);
        edge(rootView,"Extension.Window.normalFont","Extension.TechMedium");
        edge(rootView,"Extension.Window.boldFont","Extension.TechMedium");
        edge(rootView,"Extension.Window.winParent",{});
        edge(rootView,"Extension.Window.defaultCursor","Extension.defaultCursor");
        edge(rootView,"Extension.Window.defaultCursorShadow",{});
        const auto& windowGC=object(field(rootView,"Extension.Window.wGc").value.text);
        const auto& generalGC=object(field(rootView,"Extension.Window.gGc").value.text);
        Require(windowGC.path!=generalGC.path && windowGC.classPath=="Extension.GC" && generalGC.classPath=="Extension.GC" &&
            windowGC.outerPath==rootView.path && generalGC.outerPath==rootView.path,"Native root has no two distinct owned GC instances");
        edge(rootView,"Extension.Window.wGc",windowGC.path);edge(rootView,"Extension.Window.gGc",generalGC.path);
        for(const auto* name:{"normalFont","boldFont"}) {
            edge(windowGC,std::string("Extension.GC.")+name,"Extension.TechMedium");
            edge(generalGC,std::string("Extension.GC.")+name,{});
        }
        for(const auto* name:{"defaultHotX","defaultHotY"}) {
            const auto& value=field(rootView,std::string("Extension.Window.")+name).value;
            Require(value.kind==Kind::Float && value.floating==-1.0f,"Original omitted cursor hotspot did not retain native -1");
        }
        const auto& cursorColor=field(rootView,"Extension.Window.defaultCursorColor").value;
        Require(cursorColor.kind==Kind::Struct && cursorColor.fields.size()==4u &&
            cursorColor.fields.at("r").integer==255 && cursorColor.fields.at("g").integer==255 &&
            cursorColor.fields.at("b").integer==255 && cursorColor.fields.at("a").integer==0,
            "Original cursor RGB/default storage policy is incorrect");
        for(const auto& [name,texture]:{
            std::pair{"defaultEditCursor","DefaultTextCursor"},std::pair{"DefaultMoveCursor","DefaultMoveCursor"},
            std::pair{"defaultHorizontalMoveCursor","DefaultHMoveCursor"},std::pair{"defaultVerticalMoveCursor","DefaultVMoveCursor"},
            std::pair{"defaultTopLeftMoveCursor","DefaultTLMoveCursor"},std::pair{"defaultTopRightMoveCursor","DefaultTRMoveCursor"}})
            edge(rootView,std::string("Extension.RootWindow.")+name,std::string("Extension.")+texture);
        for(const auto* name:{"bIsVisible","bNeedsQuery","bNeedsReconfigure"}) {
            const auto& value=field(rootView,std::string("Extension.Window.")+name).value;
            Require(value.kind==Kind::Bool && value.boolean!=(std::string(name)=="bIsVisible"),
                "Hidden native root reconfiguration branch changed visibility or failed to mark Self");
        }
        Require(ReadPortableRuntimeNativeWindowSummary().objects==0u &&
            Declared(player,"Extension.PlayerPawnExt.RootWindow",Kind::Object).text.empty() &&
            GC::GetStats().numObjects==windowStats.numObjects && GC::GetStats().memoryUsage==windowStats.memoryUsage &&
            SavePortableRuntimeState(inspectPath.string()) && Bytes(inspectPath)==baseline,
            "Original root prefix probe published its pending root/GC state");
        const auto rootAttempt=ExecutePortableActorFunction(player,"Extension.PlayerPawnExt.InitRootWindow");
        std::cout<<"ORIGINAL unbound root initialization boundary: "<<rootAttempt.function<<':'<<rootAttempt.offset<<
            " opcode="<<unsigned(rootAttempt.opcode)<<" "<<rootAttempt.error<<std::endl;
        Require(!rootAttempt.passed() && !rootAttempt.committed && rootAttempt.error.find("1410")!=std::string::npos &&
            std::find(rootAttempt.callStack.begin(),rootAttempt.callStack.end(),"DeusEx.DeusExRootWindow.InitWindow")!=rootAttempt.callStack.end(),
            "Native1052 did not construct/dispatch original root without a fabricated client prerequisite");
        const auto windowsAfter=ReadPortableRuntimeNativeWindowSummary();
        Require(windowsAfter.objects==0u && windowsAfter.roots==0u &&
            windowsAfter.configuredRootNameCounter==windowBaseline.configuredRootNameCounter &&
            windowsAfter.graphicsContextNameCounter==windowBaseline.graphicsContextNameCounter &&
            Declared(player,"Extension.PlayerPawnExt.RootWindow",Kind::Object).text.empty() &&
            GC::GetStats().numObjects==windowStats.numObjects && GC::GetStats().memoryUsage==windowStats.memoryUsage &&
            GetPortableRuntimeWorldRevision()==windowRevision && GetPortableRuntimeMapActors(true).size()==count &&
            SavePortableRuntimeState(inspectPath.string()) && Bytes(inspectPath)==baseline,
            "Root callback failure retained native roots/GC objects/names/player edges or map/save changes");
        const auto repeatedRoot=ExecutePortableActorFunction(player,"Extension.PlayerPawnExt.InitRootWindow");
        Require(repeatedRoot.status==rootAttempt.status && repeatedRoot.function==rootAttempt.function &&
            repeatedRoot.offset==rootAttempt.offset && repeatedRoot.opcode==rootAttempt.opcode &&
            repeatedRoot.error==rootAttempt.error && repeatedRoot.callStack==rootAttempt.callStack,
            "Original root callback did not reproduce its exact first remaining gap after rollback");
        for(const auto writeLimit:{1u,5u,25u,100u}) {
            QuestVr::Vm::Limits windowLimits;windowLimits.writes=writeLimit;
            const auto bounded=ExecutePortableActorFunction(player,"Extension.PlayerPawnExt.InitRootWindow",{},windowLimits);
            Require(bounded.status==QuestVr::Vm::Status::Budget && !bounded.committed,
                "Root native constructor ignored the shared write budget: "+std::to_string(writeLimit)+" "+bounded.error);
            const auto boundedWindows=ReadPortableRuntimeNativeWindowSummary();
            Require(boundedWindows.objects==0u && boundedWindows.roots==0u &&
                boundedWindows.configuredRootNameCounter==windowBaseline.configuredRootNameCounter &&
                boundedWindows.graphicsContextNameCounter==windowBaseline.graphicsContextNameCounter &&
                Declared(player,"Extension.PlayerPawnExt.RootWindow",Kind::Object).text.empty() &&
                GC::GetStats().numObjects==windowStats.numObjects && GC::GetStats().memoryUsage==windowStats.memoryUsage &&
                GetPortableRuntimeWorldRevision()==windowRevision && SavePortableRuntimeState(inspectPath.string()) &&
                Bytes(inspectPath)==baseline,"Budgeted root construction retained reflected/window/GC state");
        }
        auto clientLifetime=std::make_shared<QuestVr::NativeClientLifetime>();
        BindPortableRuntimeClient(clientLifetime,17u);
        RegisterPortableRuntimeViewport(17u,{91u,1440u,1440u,3u,2u});
        const auto session=PreparePortableRuntimePlayerObjects(91u);
        Require(!session.bound && session.playerPath.empty() && session.viewportClassPath=="Engine.Viewport" &&
            session.clientIdentity==17u && session.viewportIdentity==91u &&
            !session.clientPath.empty() && session.clientPath!=session.viewportPath &&
            session.viewportOuterPath==session.clientPath && session.clientObjectFlags==0x4000 &&
            session.viewportObjectFlags==0x4000 &&
            !ReadPortableRuntimePlayerHud(91u).available && GetPortableRuntimeMapActors(true).size()==count,
            "Actual native session UObject preparation fabricated a player, inventory or HUD state");
        const auto sessionStats=GC::GetStats();
        Require(PreparePortableRuntimePlayerObjects(91u).viewportPath==session.viewportPath &&
            GC::GetStats().numObjects==sessionStats.numObjects && SavePortableRuntimeState(inspectPath.string()) &&
            Bytes(inspectPath)==baseline,"Native player/session preparation changed map saves or duplicated UObjects");
        bool unknownViewportRefused{};
        try {static_cast<void>(PreparePortableRuntimePlayerObjects(999u));}
        catch(const std::exception&) {unknownViewportRefused=true;}
        Require(unknownViewportRefused && GC::GetStats().numObjects==sessionStats.numObjects &&
            ReadPortableRuntimePlayerSession(91u).viewportPath==session.viewportPath,
            "Native session preparation published partial objects for an unregistered viewport");
        RegisterPortableRuntimeViewport(17u,{94u,1600u,1600u,3u,2u});
        const auto secondViewport=PreparePortableRuntimePlayerObjects(94u);
        const auto retainedClient=ReadPortableRuntimePlayerSession(91u);
        Require(secondViewport.clientPath==session.clientPath && secondViewport.viewportPath!=session.viewportPath &&
            retainedClient.clientPath==session.clientPath && retainedClient.clientObjectFlags==session.clientObjectFlags &&
            retainedClient.viewportPath==session.viewportPath && retainedClient.viewportOuterPath==session.viewportOuterPath &&
            secondViewport.viewport.identity==94u && retainedClient.viewport.identity==91u &&
            GC::GetStats().numObjects==sessionStats.numObjects+1u && SavePortableRuntimeState(inspectPath.string()) &&
            Bytes(inspectPath)==baseline,"Adding a second native viewport replaced its retained Client identity or touched map state");
        const auto completeSessionStats=GC::GetStats();
        GC::Collect();
        Require(ReadPortableRuntimePlayerSession(91u).viewportPath==session.viewportPath &&
            GC::GetStats().numObjects==completeSessionStats.numObjects,"Native player/session UObjects lost their GC roots");
        const auto dormantBinding=SetPortableRuntimePlayer(91u,player);
        Require(!dormantBinding.passed() && !dormantBinding.committed &&
            dormantBinding.error.find("accepted begun world")!=std::string::npos &&
            !ReadPortableRuntimePlayerSession(91u).bound &&
            Declared(player,"Engine.PlayerPawn.Player",Kind::Object).text.empty() &&
            SavePortableRuntimeState(inspectPath.string()) && Bytes(inspectPath)==baseline,
            "SetPlayer bypassed actual world startup/Possess or retained an unsupported native pairing");
        const auto info=std::find_if(actors.begin(),actors.end(),[&](const auto& actor) {return actor.objectPath==level;});
        Require(info!=actors.end(),"Generated possession control lacks actual LevelInfo class identity");
        // Explicit TEST-ONLY reflected begun flag. This is not a completed
        // LoadMap transaction, Login, gameplay startup or a live Quest mutation.
        const auto begunPath=directory/"test-only-begun-for-possession.sav";
        Require(QuestVr::WriteDurableSaveFile(begunPath.string(),InjectTestBegunWorld(baseline,level,info->classPath)) &&
            LoadPortableRuntimeState(begunPath.string()) && SavePortableRuntimeState(inspectPath.string()),
            "Cannot load the explicit generated isolated possession control");
        const auto begun=Bytes(inspectPath);
        const auto begunRevision=GetPortableRuntimeWorldRevision();
        const auto begunStats=GC::GetStats();
        const auto possession=SetPortableRuntimePlayer(91u,player);
        Require(!possession.passed() && !possession.committed && !possession.function.empty() &&
            !possession.callStack.empty() && possession.callStack.front()=="DeusEx.DeusExPlayer.Possess" &&
            SavePortableRuntimeState(inspectPath.string()) && Bytes(inspectPath)==begun &&
            GetPortableRuntimeWorldRevision()==begunRevision && !ReadPortableRuntimePlayerSession(91u).bound &&
            Declared(player,"Engine.PlayerPawn.Player",Kind::Object).text.empty() &&
            GC::GetStats().numObjects==begunStats.numObjects,
            "Original isolated Possess failure did not roll back native edges, scripts, checkpoint and revision");
        std::cout<<"ORIGINAL isolated SetPlayer/Possess first gap: "<<possession.function<<':'<<possession.offset<<
            " opcode="<<unsigned(possession.opcode)<<" "<<possession.error<<"; native pairing and full checkpoint rolled back\n";
        Require(LoadPortableRuntimeState(baselinePath.string()) && SavePortableRuntimeState(inspectPath.string()) &&
            Bytes(inspectPath)==baseline && !Declared(level,"Engine.LevelInfo.bBegunPlay",Kind::Bool).boolean,
            "Generated possession control leaked its test-only begun flag or native edges");
        const auto stats=GC::GetStats();
        const auto failureRevision=GetPortableRuntimeWorldRevision();
        QuestVr::Vm::Limits limits;limits.writes=1u;
        const auto refused=ExecutePortableActorFunction(player,"CreateKeyRing",{},limits);
        Require(refused.status==QuestVr::Vm::Status::Budget && !refused.committed,
            "Original keyring forced-write budget did not refuse: "+refused.function+':'+
            std::to_string(refused.offset)+" status="+std::to_string(static_cast<unsigned>(refused.status))+" "+refused.error);
        Require(refused.status==QuestVr::Vm::Status::Budget && !refused.committed &&
            SavePortableRuntimeState(inspectPath.string()) && Bytes(inspectPath)==baseline &&
            GetPortableRuntimeMapActors(true).size()==count && GetPortableRuntimeWorldRevision()==failureRevision &&
            GC::GetStats().numObjects==stats.numObjects,
            "Original keyring failed birth retained state, identity, allocation or revision: actors="+
            std::to_string(GetPortableRuntimeMapActors(true).size())+'/'+std::to_string(count)+" revision="+
            std::to_string(GetPortableRuntimeWorldRevision())+'/'+std::to_string(failureRevision)+" GC="+
            std::to_string(GC::GetStats().numObjects)+'/'+std::to_string(stats.numObjects));
        const auto created=Call(player,"CreateKeyRing");
        Require(created.function=="DeusEx.DeusExPlayer.CreateKeyRing" &&
            created.offset==76u && GetPortableRuntimeMapActors(true).size()==count+1u,
            "Original CreateKeyRing did not return through actual bytecode/birth");
        const auto ring=Declared(player,"DeusEx.DeusExPlayer.KeyRing",Kind::Object).text;
        Require(!ring.empty(),"Original CreateKeyRing has no actual ring");
        VerifyRing(player,ring);
        Require(SavePortableRuntimeState(ringPath.string()),"Cannot checkpoint actual owned ring graph");
        const auto ringBytes=Bytes(ringPath);
        const auto ringRevision=GetPortableRuntimeWorldRevision();
        Call(player,"CreateKeyRing");
        Require(GetPortableRuntimeMapActors(true).size()==count+1u &&
            GetPortableRuntimeWorldRevision()==ringRevision && SavePortableRuntimeState(inspectPath.string()) &&
            Bytes(inspectPath)==ringBytes,"Repeated original CreateKeyRing duplicated or changed its ring");
        GC::Collect();VerifyRing(player,ring);
        ShutdownPortableRuntime();
        Require(InitializePortableRuntime(scripts).passed && LoadPortableRuntimeMap(map).passed &&
            LoadPortableRuntimeState(ringPath.string()),"Cold original ring graph restore failed");
        Require(!HasPortableRuntimeClient(),"Cold actor checkpoint fabricated a native client/session");
        VerifyRing(player,ring);
        Require(SavePortableRuntimeState(inspectPath.string()) && Bytes(inspectPath)==ringBytes,
            "Cold original ring graph did not restore byte-identically");
        ShutdownPortableRuntime();
        Require(InitializePortableRuntime(scripts).passed,"Compass map-replacement runtime failed");
        const auto replacement=LoadPortablePackageTables((root/"Maps"/"01_NYC_UNATCOIsland.dx").string());
        Require(LoadPortableRuntimeMap(replacement).passed,"Compass replacement map failed");
        const auto replacementNorth=ReadPortableRuntimeMapNorth();
        Require(replacementNorth.found && replacementNorth.actorPath.rfind("01_NYC_UNATCOIsland.",0u)==0u &&
            Declared(replacementNorth.actorPath,"DeusEx.DeusExLevelInfo.TrueNorth",Kind::Int).integer==replacementNorth.trueNorth,
            "Compass retained Training identity/north after actual map replacement");
        Require(UnloadPortableRuntimeMap()!=0u,"Compass replacement map did not unload");
        bool noMapRefused{};
        try {static_cast<void>(ReadPortableRuntimeMapNorth());} catch(const std::exception&) {noMapRefused=true;}
        Require(noMapRefused,"Compass inspection accepted no loaded native world");
        auto replacementLifetime=std::make_shared<QuestVr::NativeClientLifetime>();
        BindPortableRuntimeClient(replacementLifetime,23u);
        RegisterPortableRuntimeViewport(23u,{92u,1280u,1280u,3u,2u});
        const auto replacementSession=PreparePortableRuntimePlayerObjects(92u);
        Require(!replacementSession.bound && !replacementSession.viewportPath.empty(),
            "Session UObject preparation incorrectly requires a fabricated map actor");
        replacementLifetime->Revoke();bool revokedRefused{};
        try {static_cast<void>(ReadPortableRuntimePlayerSession(92u));} catch(const std::exception&) {revokedRefused=true;}
        Require(revokedRefused && !HasPortableRuntimeClient(),"Native viewport/player query ignored revoked renderer lease");
        ClearPortableRuntimeClient();
        GC::Collect();
        const auto classesOnly=GC::GetStats();
        auto nextLifetime=std::make_shared<QuestVr::NativeClientLifetime>();
        BindPortableRuntimeClient(nextLifetime,24u);
        RegisterPortableRuntimeViewport(24u,{93u,1280u,1280u,3u,2u});
        const auto nextSession=PreparePortableRuntimePlayerObjects(93u);
        Require(nextSession.viewportClassPath=="Engine.Viewport" && nextSession.viewportPath!=replacementSession.viewportPath &&
            nextSession.clientPath!=replacementSession.clientPath && !nextSession.bound &&
            GC::GetStats().numObjects==classesOnly.numObjects+2u,
            "Session restart duplicated native UClasses or reused retired UPlayer instances");
        ClearPortableRuntimeClient();GC::Collect();
        Require(GC::GetStats().numObjects==classesOnly.numObjects,
            "Session clearing discarded engine-owned native UClasses or retained retired UPlayers");
        ShutdownPortableRuntime();
        std::filesystem::remove_all(directory);directory.clear();ownedDirectory=false;
        std::cout<<"PASS "<<checks<<" original-player controls: map TrueNorth, actual JCDenton native birth, "
            "compiled CreateKeyRing/GiveTo/AddInventory/Idle2/base, idempotence, birth-budget rollback, GC and cold graph save; "
            "real session UViewport identities, GC/idempotence/no-map/lease lifetime and no fabricated native pairing. "
            "Dormant-world isolated control only: Login, possession and UI bootstrap remain unproven.\n";
        return 0;
    } catch(const std::exception& error) {
        ShutdownPortableRuntime();
        if(ownedDirectory && !directory.empty()) std::filesystem::remove_all(directory);
        std::cerr<<error.what()<<'\n';return 1;
    }
}
