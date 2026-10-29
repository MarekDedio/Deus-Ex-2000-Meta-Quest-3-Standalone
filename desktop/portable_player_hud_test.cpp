#include "Precomp.h"
#include "GC/GC.h"
#include "portable_engine_collision.h"
#include "portable_object_lookup.h"
#include "portable_unreal_runtime.h"
#include "portable_root_window_activation_probe.h"
#include "portable_model_geometry.h"
#include "quest_script_state.h"
#include "quest_save_bundle.h"
#include "quest_native_window_clip.h"
#include "quest_native_gc_context.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <tuple>

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
    Require(hud.swimTimer==Declared(player,"DeusEx.DeusExPlayer.swimTimer",Kind::Float).floating &&
        hud.swimDuration==Declared(player,"DeusEx.DeusExPlayer.swimDuration",Kind::Float).floating,
        "HUD oxygen reader replaced actual player swim timer/duration");
    const auto region=Declared(player,"Engine.Pawn.HeadRegion",Kind::Struct);
    const auto zone=region.fields.at("zone");
    Require(zone.kind==Kind::Object,"HUD oxygen head zone lacks actual Object storage");
    if(zone.text.empty()) Require(!hud.headInWater,"HUD oxygen inferred a dry zone from missing state");
    else Require(hud.headInWater && *hud.headInWater==Declared(zone.text,"Engine.ZoneInfo.bWaterZone",Kind::Bool).boolean,
        "HUD oxygen did not use the actual declared head-region water flag");
}
void VerifyToggleAppearance(const std::array<Value,3u>& appearance,bool pressed) {
    const auto channel=pressed?21:11;
    Require(appearance[1u].kind==Kind::Object && appearance[1u].text.empty(),
        "Original Toggle appearance invented a texture in the explicit None-texture control");
    for(const auto index:{0u,2u}) {
        const auto& color=appearance[index];
        Require(color.kind==Kind::Struct && color.fields.size()==4u,
            "Original Toggle appearance lost its typed text/tile Color");
        for(const auto& [name,expected]:{std::pair{"r",channel},std::pair{"g",channel+1},
            std::pair{"b",channel+2},std::pair{"a",0}}) {
            const auto found=color.fields.find(name);
            Require(found!=color.fields.end() && found->second.kind==Kind::Byte &&
                found->second.integer==expected,
                "Original Toggle appearance did not select the actual normal/pressed Info record");
        }
    }
}
bool SameToggleAppearance(const std::array<Value,3u>& left,const std::array<Value,3u>& right) {
    return std::equal(left.begin(),left.end(),right.begin(),QuestVr::Vm::Equal);
}
void VerifyNativeWindowCoalescing(const std::vector<std::uint8_t>& baseline,const std::string& player,
    const std::filesystem::path& directory,std::size_t expectedWrites,std::size_t expectedObjects) {
    // This is an authenticated, temporary CDO checkpoint fixture followed by
    // the SAME original nonpublishing constructor probe. It neither replaces
    // InitWindow bytecode nor establishes Login, possession or a live HUD.
    const std::string rootClass="DeusEx.DeusExRootWindow";
    Require(ReadPortableClassDefault(rootClass,"keyDownMap",7u).kind==Kind::Byte &&
        ReadPortableClassDefault(rootClass,"clientObject").kind==Kind::Object &&
        ReadPortableClassDefault(rootClass,"MouseX").kind==Kind::Float &&
        ReadPortableClassDefault(rootClass,"MouseY").kind==Kind::Float,
        "Native coalescing fixture does not match the actual owned root field kinds");
    auto icon=ReadPortableClassDefault("DeusEx.DamageHUDDisplay","iconInfo",0u);
    Require(icon.kind==Kind::Struct && icon.fields.count("damagetype") &&
        icon.fields.at("damagetype").kind==Kind::Struct &&
        icon.fields.at("damagetype").fields.count("0") &&
        icon.fields.at("damagetype").fields.at("0").kind==Kind::Name,
        "Native coalescing fixture lacks its real nested damage-type Name schema");
    icon.fields.at("damagetype").fields.at("0")=Value::Text(Kind::Name,"sHoCkEd");
    const auto offset=ScriptOffset(baseline);
    auto graph=QuestVr::DecodeScriptSavedState({baseline.begin()+static_cast<std::ptrdiff_t>(offset+4u),baseline.end()});
    Require(graph.classDefaults.empty(),"Native coalescing fixture would replace existing CDO changes");
    graph.classDefaults.push_back({rootClass,{
        {"Extension.RootWindow.keyDownMap","keyDownMap",7u,Value::Byte(37u)},
        {"Extension.Window.clientObject","clientObject",0u,Value::Text(Kind::Object,player)},
        {"Extension.RootWindow.MouseX","MouseX",0u,Value::Float(-0.0f)},
        {"Extension.RootWindow.MouseY","MouseY",0u,Value::Float(0.0f)}}});
    graph.classDefaults.push_back({"DeusEx.DamageHUDDisplay",{
        {"DeusEx.DamageHUDDisplay.iconInfo","iconInfo",0u,std::move(icon)}}});
    const auto blob=QuestVr::EncodeScriptSavedState(graph);
    Require(blob.size()<=16u*1024u*1024u,"Native coalescing fixture exceeds its script bound");
    std::vector<std::uint8_t> fixture(baseline.begin(),baseline.begin()+static_cast<std::ptrdiff_t>(offset+4u));
    for(unsigned i=0u;i<4u;++i)fixture[offset+i]=static_cast<std::uint8_t>(blob.size()>>(i*8u));
    fixture.insert(fixture.end(),blob.begin(),blob.end());
    const auto fixturePath=directory/"test-only-window-default-clearing.sav";
    const auto inspectPath=directory/"window-default-clearing-inspect.sav";
    const auto restorePath=directory/"window-default-clearing-restore.sav";
    const auto initialStats=GC::GetStats();
    Require(QuestVr::WriteDurableSaveFile(fixturePath.string(),fixture) &&
        LoadPortableRuntimeState(fixturePath.string()) && SavePortableRuntimeState(inspectPath.string()),
        "Cannot load authenticated temporary native-window CDO controls");
    const auto before=Bytes(inspectPath);
    const auto stats=GC::GetStats();const auto revision=GetPortableRuntimeWorldRevision();
    const auto summary=ReadPortableRuntimeNativeWindowSummary();
    const auto probe=ProbePortableRootWindowInitialization(player);
    Require(probe.execution.status==QuestVr::Vm::Status::Unsupported && !probe.execution.committed &&
        probe.execution.error=="Non-publishing native root initialization probe completed; rollback required" &&
        probe.inspectionError.empty() && probe.execution.writes==expectedWrites && probe.objects.size()==expectedObjects,
        "Prepatched native-window constructor changed the logical write sequence or failed: "+
        probe.execution.error+"; "+probe.inspectionError);
    const auto rootView=std::find_if(probe.objects.begin(),probe.objects.end(),[&](const auto& view){return view.path==probe.pawnRootPath;});
    Require(rootView!=probe.objects.end(),"Prepatched constructor did not capture its actual native root");
    const auto field=[&](const PortableNativeWindowObjectSnapshot& view,const char* key,std::uint32_t index=0u)
        ->const PortableNativeWindowFieldSnapshot& {
        const auto found=std::find_if(view.fields.begin(),view.fields.end(),[&](const auto& entry){
            return entry.declaration==key && entry.arrayIndex==index;
        });
        Require(found!=view.fields.end(),std::string("Prepatched root omitted declared logical slot: ")+key);return *found;
    };
    for(const auto* key:{"Extension.ModalWindow.acceleratorTable","Extension.RootWindow.keyDownMap"})
        Require(std::count_if(rootView->fields.begin(),rootView->fields.end(),[&](const auto& entry){return entry.declaration==key;})==255,
            "Coalescing removed logical root slots instead of redundant physical overrides");
    for(std::uint32_t index=0u;index<255u;++index) {
        const auto& key=field(*rootView,"Extension.RootWindow.keyDownMap",index);
        const auto& accelerator=field(*rootView,"Extension.ModalWindow.acceleratorTable",index);
        Require(key.value.kind==Kind::Byte && key.value.integer==0 && accelerator.value.kind==Kind::Object &&
            accelerator.value.text.empty() && accelerator.propertyRoots.empty(),
            "Constructor did not clear all510 logical key/accelerator slots, including nonzero frozen defaults");
    }
    const auto& client=field(*rootView,"Extension.Window.clientObject");
    Require(client.value.kind==Kind::Object && client.value.text.empty() && client.propertyRoots.empty(),
        "Object-None coalescing retained a nonnull frozen value or stale owning GC edge");
    for(const auto* key:{"Extension.RootWindow.MouseX","Extension.RootWindow.MouseY"}) {
        const auto& value=field(*rootView,key).value;
        Require(value.kind==Kind::Float && value.floating==0.0f && !std::signbit(value.floating),
            "Native constructor coalesced negative zero with its required positive-zero store");
    }
    const auto damage=std::find_if(probe.objects.begin(),probe.objects.end(),[](const auto& view){
        return view.classPath=="DeusEx.DamageHUDDisplay";
    });
    Require(damage!=probe.objects.end(),"Prepatched constructor lost its actual damage display");
    // Structs are deliberately OUTSIDE the scalar fast path. This guards the
    // original nested Name assignment and spelling, not scalar Name eligibility.
    Require(field(*damage,"DeusEx.DamageHUDDisplay.iconInfo",0u).value.fields.at("damagetype").fields.at("0").text=="Shocked" &&
        ReadPortableClassDefault("DeusEx.DamageHUDDisplay","iconInfo",0u).fields.at("damagetype").fields.at("0").text=="sHoCkEd" &&
        ReadPortableClassDefault(rootClass,"keyDownMap",7u).integer==37 &&
        ReadPortableClassDefault(rootClass,"clientObject").text==player &&
        std::signbit(ReadPortableClassDefault(rootClass,"MouseX").floating),
        "Constructor clearing or nested Name writes changed their frozen CDO source");
    GC::Collect();const auto after=ReadPortableRuntimeNativeWindowSummary();
    Require(after.objects==summary.objects && after.roots==summary.roots &&
        after.configuredRootNameCounter==summary.configuredRootNameCounter &&
        after.graphicsContextNameCounter==summary.graphicsContextNameCounter &&
        Declared(player,"Extension.PlayerPawnExt.RootWindow",Kind::Object).text.empty() &&
        GetPortableRuntimeWorldRevision()==revision && GC::GetStats().numObjects==stats.numObjects &&
        GC::GetStats().memoryUsage==stats.memoryUsage && SavePortableRuntimeState(inspectPath.string()) && Bytes(inspectPath)==before,
        "Coalesced constructor probe retained native identity, GC authority, CDO or checkpoint changes");
    Require(QuestVr::WriteDurableSaveFile(restorePath.string(),baseline) && LoadPortableRuntimeState(restorePath.string()) &&
        SavePortableRuntimeState(inspectPath.string()) && Bytes(inspectPath)==baseline,
        "Temporary native-window CDO fixture did not restore the exact owned baseline");
    GC::Collect();
    Require(GC::GetStats().numObjects==initialStats.numObjects && GC::GetStats().memoryUsage==initialStats.memoryUsage,
        "Temporary native-window CDO fixture retained GC objects after baseline restore");
}
}

int main(int argc,char** argv) {
    std::filesystem::path directory;
    bool ownedDirectory{};
    try {
        if(argc==1) {std::cout<<"SKIP: supply owned Deus Ex installation for original player/keyring execution\n";return 77;}
        if(argc!=2) throw std::runtime_error("Usage: portable_player_hud_test GAME_ROOT");
        const auto root=std::filesystem::path(argv[1]);
        const auto prepareServices=[&] {
            Require(PreparePortableRuntimeNativeEngineClasses().prepared &&
                PreparePortableRuntimeNativeMetadataClasses().prepared &&
                PreparePortableRuntimeNativeConfigClasses().prepared &&
                PreparePortableRuntimeNativePackages().prepared &&
                PreparePortableRuntimeObjectLookup().prepared &&
                PreparePortableRuntimeEngineCollision((root/"System"/"DeusEx.ini").string()).prepared,
                "Original player controls lack genuine native lookup/Engine collision services");
        };
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
        prepareServices();
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
        std::cout<<"ORIGINAL root pending graph: "<<pending.execution.function<<':'<<pending.execution.offset<<
            " opcode="<<unsigned(pending.execution.opcode)<<" objects="<<pending.objects.size()<<
            " writes="<<pending.execution.writes<<" "<<pending.execution.error<<"; inspection="<<pending.inspectionError<<std::endl;
        Require(pending.execution.status==QuestVr::Vm::Status::Unsupported && !pending.execution.committed && pending.inspectionError.empty() &&
            pending.execution.function.empty() && pending.execution.offset==0u && pending.execution.opcode==0u &&
            pending.execution.error=="Non-publishing native root initialization probe completed; rollback required" &&
            pending.execution.writes==16025u && pending.objects.size()==198u && pending.execution.callStack.empty() &&
            pending.rootedWindows==std::vector<std::string>{pending.pawnRootPath},
            "Original root prefix probe did not capture the actual pending native graph: "+pending.inspectionError);
        const auto object=[&](const std::string& path)->const PortableNativeWindowObjectSnapshot& {
            const auto found=std::find_if(pending.objects.begin(),pending.objects.end(),[&](const auto& view) {return view.path==path;});
            Require(found!=pending.objects.end(),"Pending root prefix has no native object: "+path);return *found;
        };
        const auto field=[&](const PortableNativeWindowObjectSnapshot& view,const std::string& key,
            const std::uint32_t index=0u)->const PortableNativeWindowFieldSnapshot& {
            const auto found=std::find_if(view.fields.begin(),view.fields.end(),[&](const auto& entry) {
                return entry.declaration==key && entry.arrayIndex==index;
            });
            Require(found!=view.fields.end(),"Pending root prefix has no declared field: "+key);return *found;
        };
        const auto edge=[&](const PortableNativeWindowObjectSnapshot& view,const std::string& key,const std::string& target) {
            const auto& entry=field(view,key);
            Require(entry.value.kind==Kind::Object && entry.value.text==target &&
                entry.propertyRoots==(target.empty()?std::vector<std::string>{}:std::vector<std::string>{target}),
                "Native window reflected/GC edge differs from the original callback: "+view.path+'.'+key+
                " actual="+entry.value.text+" expected="+target);
        };
        const auto& rootView=object(pending.pawnRootPath);
        Require(rootView.classPath=="DeusEx.DeusExRootWindow" && rootView.outerPath==player,
            "Native root did not use the original class and actual Pawn Outer");
        edge(rootView,"Extension.RootWindow.parentPawn",player);
        edge(rootView,"Extension.Window.normalFont","Extension.TechMedium");
        edge(rootView,"Extension.Window.boldFont","Extension.TechMedium");
        edge(rootView,"Extension.Window.winParent",{});
        edge(rootView,"Extension.Window.defaultCursor","DeusExUI.UserInterface.DeusExCursor1");
        edge(rootView,"Extension.Window.defaultCursorShadow","DeusExUI.UserInterface.DeusExCursor1_Shadow");
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
        Require(rootView.rowMajorWindows.empty() && rootView.colMajorWindows.empty() && rootView.tabOrderDirty &&
            rootView.tabGroupWindows.size()==2u &&
            std::count(rootView.tabGroupWindows.begin(),rootView.tabGroupWindows.end(),rootView.path)==1 &&
            field(rootView,"Extension.TabGroupWindow.bSizeParentToChildren").value.boolean &&
            !field(rootView,"Extension.TabGroupWindow.bSizeChildrenToParent").value.boolean &&
            field(rootView,"Extension.ModalWindow.focusMode").value.integer==2 &&
            field(rootView,"Extension.ModalWindow.bDirtyAccelerators").value.boolean,
            "Root native TabGroup/Modal initialization omitted its actual self table/flags/focus mode");
        for(std::size_t index=0u;index<rootView.tabGroupWindows.size();++index) {
            const auto& group=object(rootView.tabGroupWindows[index]);
            Require(field(group,"Extension.TabGroupWindow.tabGroupIndex").value.integer==static_cast<std::int32_t>(index),
                "Native Modal append/resort lost its actual tab group index");
        }
        edge(rootView,"Extension.ModalWindow.preferredFocus",{});
        for(std::uint32_t slot=0u;slot<255u;++slot) {
            const auto& accelerator=field(rootView,"Extension.ModalWindow.acceleratorTable",slot);
            const auto& key=field(rootView,"Extension.RootWindow.keyDownMap",slot);
            Require(accelerator.value.kind==Kind::Object && accelerator.value.text.empty() && accelerator.propertyRoots.empty() &&
                key.value.kind==Kind::Byte && key.value.integer==0,"Root did not clear its real255 accelerator/key slots");
        }
        const auto& display=object(field(rootView,"DeusEx.DeusExRootWindow.actorDisplay").value.text);
        Require(display.classPath=="DeusEx.ActorDisplayWindow" && display.outerPath==rootView.path &&
            display.rowMajorWindows.empty() && display.colMajorWindows.empty() && display.tabGroupWindows.empty(),
            "NewChild fabricated ActorDisplayWindow class/Outer/native subclass storage");
        edge(rootView,"DeusEx.DeusExRootWindow.actorDisplay",display.path);
        const auto hudFound=std::find_if(pending.objects.begin(),pending.objects.end(),[](const auto& view) {return view.classPath=="DeusEx.DeusExHUD";});
        const auto ammoFound=std::find_if(pending.objects.begin(),pending.objects.end(),[](const auto& view) {return view.classPath=="DeusEx.HUDAmmoDisplay";});
        const auto hitFound=std::find_if(pending.objects.begin(),pending.objects.end(),[](const auto& view) {return view.classPath=="DeusEx.HUDHitDisplay";});
        const auto crossFound=std::find_if(pending.objects.begin(),pending.objects.end(),[](const auto& view) {return view.classPath=="DeusEx.Crosshair";});
        const auto beltFound=std::find_if(pending.objects.begin(),pending.objects.end(),[](const auto& view) {return view.classPath=="DeusEx.HUDObjectBelt";});
        Require(hudFound!=pending.objects.end() && ammoFound!=pending.objects.end() && hitFound!=pending.objects.end() &&
            crossFound!=pending.objects.end() && beltFound!=pending.objects.end(),
            "Original root did not create its actual HUD/ammo/health/crosshair/belt children");
        const auto& hudView=*hudFound;const auto& ammoView=*ammoFound;
        const auto& hitView=*hitFound;const auto& crossView=*crossFound;const auto& beltView=*beltFound;
        const auto classView=[&](const char* name)->const PortableNativeWindowObjectSnapshot& {
            const auto found=std::find_if(pending.objects.begin(),pending.objects.end(),[&](const auto& view) {
                return view.classPath==name;
            });
            Require(found!=pending.objects.end(),"Original later HUD construction omitted "+std::string(name));return *found;
        };
        const auto& activeItems=classView("DeusEx.HUDActiveItemsDisplay");
        const auto& damage=classView("DeusEx.DamageHUDDisplay");
        const auto& compass=classView("DeusEx.HUDCompassDisplay");
        const auto& multiSkills=classView("DeusEx.HUDMultiSkills");
        const auto& information=classView("DeusEx.HUDInformationDisplay");
        const auto& logDisplay=classView("DeusEx.HUDLogDisplay");
        const auto& frobDisplay=classView("DeusEx.FrobDisplayWindow");
        const auto& augmentationDisplay=classView("DeusEx.AugmentationDisplayWindow");
        const auto& missionStart=classView("DeusEx.HUDMissionStartTextDisplay");
        const auto& barkDisplay=classView("DeusEx.HUDBarkDisplay");
        const auto& receivedDisplay=classView("DeusEx.HUDReceivedDisplay");
        const auto& textLog=classView("Extension.TextLogWindow");
        const auto& scope=classView("DeusEx.DeusExScopeView");
        edge(rootView,"DeusEx.DeusExRootWindow.scopeView",scope.path);
        edge(scope,"DeusEx.DeusExScopeView.Player",player);
        edge(scope,"Extension.Window.parentOwner",rootView.path);edge(scope,"Extension.Window.winParent",rootView.path);
        Require(scope.outerPath==rootView.path&&!field(scope,"Extension.Window.bIsVisible").value.boolean&&
            field(scope,"Extension.Window.bTickEnabled").value.boolean&&field(scope,"Extension.Window.lockCount").value.integer==0,
            "Original hidden scope did not complete its Player/Tick/lock initialization");
        for(const auto* aligned:{&hudView,&scope})for(const auto* name:{"winHAlign","winVAlign"})
            Require(field(*aligned,"Extension.Window."+std::string(name)).value.integer==3,
                "Completed root InitWindow did not request both original fill alignments");
        const auto& scopeColor=field(scope,"DeusEx.DeusExScopeView.colLines").value;
        Require(scopeColor.kind==Kind::Struct&&scopeColor.fields.size()==4u,"Original scope lost its complete Color result storage");
        for(const auto& [name,channel]:scopeColor.fields) {
            static_cast<void>(name);Require(channel.kind==Kind::Byte&&channel.integer==0,
                "Unbound original scope theme Context did not clear its complete Color result");
        }
        Require(std::count_if(pending.objects.begin(),pending.objects.end(),[](const auto& view) {
                return view.classPath=="Extension.TextLogWindow";
            })==1 && textLog.outerPath==logDisplay.path,
            "Original HUD log did not construct its one actual TextLog child with the correct Outer");
        edge(textLog,"Extension.Window.parentOwner",logDisplay.path);
        edge(textLog,"Extension.Window.winParent",logDisplay.path);
        edge(logDisplay,"DeusEx.HUDLogDisplay.winLog",textLog.path);
        edge(logDisplay,"Extension.Window.firstChild",field(logDisplay,"DeusEx.HUDLogDisplay.winIcon").value.text);
        edge(logDisplay,"Extension.Window.lastChild",textLog.path);
        edge(textLog,"Extension.Window.prevSibling",field(logDisplay,"DeusEx.HUDLogDisplay.winIcon").value.text);
        edge(textLog,"Extension.Window.nextSibling",{});
        Require(textLog.textLogLinesInitialized && textLog.textLogLines.empty() && textLog.textLogLineCapacity==0u,
            "Original TextLog constructor/Init did not retain constructed-empty, freed native Lines storage");
        for(const auto& view:pending.objects) if(view.classPath!="Extension.TextLogWindow")
            Require(!view.textLogLinesInitialized && view.textLogLines.empty() && view.textLogLineCapacity==0u,
                "Native Lines storage was fabricated for a non-TextLog object");
        for(const auto* name:{"HAlign","VAlign"}) {
            const auto& value=field(textLog,"Extension.TextWindow."+std::string(name)).value;
            Require(value.kind==Kind::Byte && value.integer==0,
                "Original TextLog Init did not replace the inherited alignment with raw byte zero");
        }
        const auto& logTimeout=field(textLog,"Extension.TextLogWindow.textTimeout").value;
        const auto& logTooTall=field(textLog,"Extension.TextLogWindow.bTooTall").value;
        const auto& logPaused=field(textLog,"Extension.TextLogWindow.bPaused").value;
        const auto pausedDefault=ReadPortableClassDefault("Extension.TextLogWindow","bPaused");
        const auto playerLogTimeout=Declared(player,"DeusEx.DeusExPlayer.logTimeout",Kind::Float);
        std::cout<<"ORIGINAL pending TextLog scalars: paused="<<logPaused.boolean<<" timeout="<<logTimeout.floating<<
            " playerTimeout="<<playerLogTimeout.floating<<std::endl;
        Require(Declared(level,"Engine.LevelInfo.NetMode",Kind::Byte).integer==0 &&
            logTimeout.kind==Kind::Float && QuestVr::Vm::Equal(logTimeout,playerLogTimeout) &&
            logTooTall.kind==Kind::Bool && !logTooTall.boolean && logPaused.kind==Kind::Bool &&
            pausedDefault.kind==Kind::Bool && QuestVr::Vm::Equal(logPaused,pausedDefault),
            "Original standalone SetLogTimeout lost the actual player's value or changed untouched TextLog flags");
        const auto& textLogGC=object(field(textLog,"Extension.Window.wGc").value.text);
        const auto& textLogGeneralGC=object(field(textLog,"Extension.Window.gGc").value.text);
        Require(textLogGC.path!=textLogGeneralGC.path && textLogGC.classPath=="Extension.GC" &&
            textLogGeneralGC.classPath=="Extension.GC" && textLogGC.outerPath==textLog.path &&
            textLogGeneralGC.outerPath==textLog.path,
            "Native TextLog did not own two actual distinct inherited graphics contexts");
        edge(textLog,"Extension.Window.wGc",textLogGC.path);
        edge(textLog,"Extension.Window.gGc",textLogGeneralGC.path);
        std::cout<<"ORIGINAL TextLog pending child: actual parent/GC graph, constructed-empty native Lines, "
            "zero alignments, real standalone player timeout and retained CDO pause value; rollback-only prefix\n";
        for(const auto& view:pending.objects) {
            if(view.classPath=="Extension.GC") {
                const auto& owner=object(view.outerPath);
                if(owner.classPath=="Extension.GC") {
                    std::cout<<"PENDING saved GC "<<view.path<<" outer="<<owner.path<<
                        " free="<<field(view,"Extension.GC.bFree").value.boolean<<
                        " count="<<field(view,"Extension.GC.gcCount").value.integer<<
                        " ownerCount="<<field(owner,"Extension.GC.gcCount").value.integer<<
                        " ownerStack="<<field(owner,"Extension.GC.gcStack").value.text<<
                        " ownerFree="<<field(owner,"Extension.GC.gcFree").value.text<<std::endl;
                }
                continue;
            }
            std::cout<<"PENDING layout "<<view.classPath<<" "<<view.path;
            for(const auto* name:{"X","Y","Width","Height"})
                std::cout<<" "<<name<<"="<<field(view,"Extension.Window."+std::string(name)).value.floating;
            for(const auto* name:{"bIsVisible","bConfigured","bNeedsReconfigure"})
                std::cout<<" "<<name<<"="<<field(view,"Extension.Window."+std::string(name)).value.boolean;
            std::cout<<" lock="<<field(view,"Extension.Window.lockCount").value.integer;
            if(view.classPath=="DeusEx.HUDActiveItemsDisplay" || view.classPath=="DeusEx.HUDActiveAugsBorder" ||
                view.classPath=="DeusEx.HUDActiveItemsBorder" || view.classPath=="Extension.TileWindow" ||
                view.classPath=="DeusEx.HUDActiveAug") {
                for(const auto* name:{"lastSpecifiedWidth","lastSpecifiedHeight","lastQueryWidth","lastQueryHeight",
                    "holdX","holdY","holdWidth","holdHeight"})
                    std::cout<<" "<<name<<"="<<field(view,"Extension.Window."+std::string(name)).value.floating;
                for(const auto* name:{"bNeedsQuery","bLastWidthSpecified","bLastHeightSpecified"})
                    std::cout<<" "<<name<<"="<<field(view,"Extension.Window."+std::string(name)).value.boolean;
                std::cout<<" tileInitialized="<<view.tileRowsInitialized<<" tileRows="<<view.tileRows.size();
            }
            std::cout<<'\n';
        }
        Require(!field(crossView,"Extension.Window.bNeedsQuery").value.boolean &&
            !field(crossView,"Extension.Window.bNeedsReconfigure").value.boolean &&
            field(crossView,"Extension.Window.bConfigured").value.boolean &&
            !field(crossView,"Extension.Window.bLastWidthSpecified").value.boolean &&
            !field(crossView,"Extension.Window.bLastHeightSpecified").value.boolean,
            "Original crosshair preferred-size/configure path has incorrect query/configuration flags");
        for(const auto* name:{"lastSpecifiedWidth","lastSpecifiedHeight","lastQueryWidth","lastQueryHeight"})
            Require(field(crossView,"Extension.Window."+std::string(name)).value.floating==-1.0f,
                "Original preferred-size query cached texture fallback instead of the pre-fallback result");
        Require(field(crossView,"Extension.Window.Width").value.floating==16.0f &&
            field(crossView,"Extension.Window.Height").value.floating==16.0f &&
            field(crossView,"Extension.Window.X").value.floating==-2.5f &&
            field(crossView,"Extension.Window.Y").value.floating==-2.5f &&
            field(crossView,"Extension.Window.hardcodedWidth").value.floating==-1.0f &&
            field(crossView,"Extension.Window.hardcodedHeight").value.floating==-1.0f,
            "Original ConfigureChild did not apply the actual crosshair background dimensions and script position");
        const auto& crossClip=field(crossView,"Extension.Window.winClipRect").value;
        Require(crossClip.fields.at("originx").floating==-2.5f && crossClip.fields.at("originy").floating==-2.5f &&
            crossClip.fields.at("clipx").floating==2.5f && crossClip.fields.at("clipy").floating==2.5f &&
            crossClip.fields.at("clipwidth").floating==10.0f && crossClip.fields.at("clipheight").floating==10.0f,
            "Original crosshair ClipTree did not intersect the hidden parent's current clip");
        const auto& crossGC=object(field(crossView,"Extension.Window.wGc").value.text);
        Require(QuestVr::Vm::Equal(field(crossGC,"Extension.GC.gcClipRect").value,crossClip),
            "Original ClipTree did not copy all six channels to the drawing GC");
        // These exact dimensions were captured at the earlier TextLog
        // constructor refusal and remain checked at the later size boundary.
        // Retain independent literal negative intersections as well as using
        // the separately x87-audited clip helper for the native propagation.
        const QuestVr::NativeWindowClip hudClip{0,0,0,0,10,10};
        const auto activeClip=QuestVr::ComputeNativeWindowClip(&hudClip,-100,0,110,-59);
        const auto augClip=QuestVr::ComputeNativeWindowClip(&activeClip,0,0,10,10);
        const auto itemClip=QuestVr::ComputeNativeWindowClip(&activeClip,14,6,48,13);
        const auto itemTileClip=QuestVr::ComputeNativeWindowClip(&itemClip,0,0,10,10);
        const auto damageClip=QuestVr::ComputeNativeWindowClip(&hudClip,0,44,66,246);
        const auto compassClip=QuestVr::ComputeNativeWindowClip(&hudClip,0,4,73,40);
        const auto channels=[](const QuestVr::NativeWindowClip& clip) {
            return std::array<float,6u>{clip.originX,clip.originY,clip.clipX,clip.clipY,clip.clipWidth,clip.clipHeight};
        };
        Require(channels(activeClip)==std::array<float,6u>{-100,0,100,0,10,-59} &&
            channels(augClip)==std::array<float,6u>{-100,0,100,0,-90,-59} &&
            channels(itemClip)==std::array<float,6u>{-86,6,86,0,-38,-65} &&
            channels(itemTileClip)==std::array<float,6u>{-86,6,86,0,-76,-65} &&
            channels(damageClip)==std::array<float,6u>{0,44,0,0,10,-34} &&
            channels(compassClip)==std::array<float,6u>{0,4,0,0,10,6},
            "Later HUD negative clip derivation differs from independent literal intersections");
        const auto& receivedTileForQuery=object(field(receivedDisplay,"Extension.Window.firstChild").value.text);
        const auto& receivedTextForQuery=object(field(receivedTileForQuery,"Extension.Window.firstChild").value.text);
        std::size_t completedTextQueries{};
        for(const auto& view:pending.objects) if(view.classPath=="Extension.GC" && view.path!=crossGC.path) {
            const auto& owner=object(view.outerPath);
            if(owner.classPath=="Extension.GC") {
                const auto& queryWindow=object(owner.outerPath);
                edge(queryWindow,"Extension.Window.gGc",owner.path);
                const bool free=field(view,"Extension.GC.bFree").value.boolean;
                Require(free&&field(view,"Extension.GC.gcCount").value.integer==1&&
                    field(owner,"Extension.GC.gcCount").value.integer==1,
                    "Text preferred-size query lost its actual saved/current context counts");
                edge(view,"Extension.GC.gcOwner",owner.path);edge(view,"Extension.GC.gcStack",{});edge(view,"Extension.GC.gcFree",{});
                {
                    Require(owner.path==textLogGeneralGC.path||queryWindow.path==receivedTextForQuery.path||queryWindow.path==missionStart.path,
                        "Unexpected completed native text-query context in original HUD prefix");
                    edge(owner,"Extension.GC.gcFree",view.path);edge(owner,"Extension.GC.gcStack",{});
                    for(const auto* name:QuestVr::NativeGcContext::ContextFields)
                        Require(QuestVr::Vm::Equal(field(view,"Extension.GC."+std::string(name)).value,
                            field(owner,"Extension.GC."+std::string(name)).value),"Released GC lost its saved original context");
                    ++completedTextQueries;
                }
                continue;
            }
            const bool drawing=field(owner,"Extension.Window.wGc").value.text==view.path;
            const bool beltDrawing=drawing && (owner.path==beltView.path || owner.path.rfind(beltView.path+'.',0u)==0u);
            const bool activeDrawing=drawing && (owner.path==activeItems.path || owner.path.rfind(activeItems.path+'.',0u)==0u);
            const bool damageDrawing=drawing && owner.path==damage.path;
            const bool compassDrawing=drawing && (owner.path==compass.path || owner.path.rfind(compass.path+'.',0u)==0u);
            auto expectedClip=QuestVr::NativeWindowClip{};
            if(activeDrawing) {
                if(owner.path==activeItems.path)expectedClip=activeClip;
                else if(owner.classPath=="DeusEx.HUDActiveItemsBorder")expectedClip=itemClip;
                else if(owner.classPath=="Extension.TileWindow" &&
                    object(field(owner,"Extension.Window.winParent").value.text).classPath=="DeusEx.HUDActiveItemsBorder")
                    expectedClip=itemTileClip;
                else expectedClip=augClip;
            } else if(damageDrawing)expectedClip=damageClip;
            else if(compassDrawing)expectedClip=compassClip;
            // The now-assigned belt is configured by the next HUD callback:
            // 10-541,10-69 =>(-531,-59). ClipTree preserves negative clipped
            // extents for the still-unconfigured 10x10 descendants, not zero.
            for(const auto& [name,value]:field(view,"Extension.GC.gcClipRect").value.fields) {
                float expected{};
                if(beltDrawing) {
                    if(name=="originx") expected=-531.0f;
                    else if(name=="originy") expected=-59.0f;
                    else if(name=="clipx") expected=531.0f;
                    else if(name=="clipy") expected=59.0f;
                    else if(name=="clipwidth") expected=owner.path==beltView.path?10.0f:-521.0f;
                    else if(name=="clipheight") expected=owner.path==beltView.path?10.0f:-49.0f;
                    else throw std::runtime_error("Unexpected native GC clip channel");
                } else {
                    const std::array<const char*,6u> names{"originx","originy","clipx","clipy","clipwidth","clipheight"};
                    const auto found=std::find(names.begin(),names.end(),name);
                    Require(found!=names.end(),"Unexpected later HUD GC clip channel");
                    expected=channels(expectedClip)[static_cast<std::size_t>(found-names.begin())];
                }
                Require(value.kind==Kind::Float && value.floating==expected &&
                    (expected!=0.0f || !std::signbit(value.floating)),
                    "Original callback clipping changed a drawing/general GC: "+view.path+'.'+name+
                    " actual="+std::to_string(value.floating)+" expected="+std::to_string(expected));
            }
            if(beltDrawing || activeDrawing || damageDrawing || compassDrawing)
                Require(QuestVr::Vm::Equal(field(view,"Extension.GC.gcClipRect").value,
                    field(owner,"Extension.Window.winClipRect").value),"Original ClipTree window/drawing GC channels disagree");
        }
        Require(completedTextQueries==3u,
            "Completed HUD queries did not release all three original saved GC contexts");
        const auto& radioView=object(field(beltView,"Extension.Window.firstChild").value.text);
        const auto& slotsView=object(field(beltView,"DeusEx.HUDObjectBelt.winSlots").value.text);
        Require(radioView.classPath=="Extension.RadioBoxWindow" && radioView.outerPath==beltView.path &&
            slotsView.classPath=="Extension.TileWindow" && slotsView.outerPath==radioView.path,
            "Original belt did not construct its actual Radio/Tile hierarchy");
        edge(beltView,"Extension.Window.lastChild",radioView.path);
        edge(radioView,"Extension.Window.winParent",beltView.path);edge(radioView,"Extension.Window.parentOwner",beltView.path);
        edge(radioView,"Extension.Window.prevSibling",{});edge(radioView,"Extension.Window.nextSibling",{});
        edge(radioView,"Extension.Window.firstChild",slotsView.path);edge(radioView,"Extension.Window.lastChild",slotsView.path);
        edge(slotsView,"Extension.Window.winParent",radioView.path);edge(slotsView,"Extension.Window.parentOwner",radioView.path);
        edge(slotsView,"Extension.Window.prevSibling",{});edge(slotsView,"Extension.Window.nextSibling",{});
        Require(std::count(rootView.tabGroupWindows.begin(),rootView.tabGroupWindows.end(),radioView.path)==1 &&
            radioView.tabOrderDirty && radioView.rowMajorWindows.empty() && radioView.colMajorWindows.empty() &&
            radioView.tabGroupWindows.empty() && radioView.toggleButtons.size()==10u && !radioView.emptyTileRows &&
            slotsView.emptyTileRows && slotsView.tileRowsInitialized && slotsView.tileRows.empty() &&
            !slotsView.tabOrderDirty && slotsView.rowMajorWindows.empty() &&
            slotsView.colMajorWindows.empty() && slotsView.tabGroupWindows.empty() && slotsView.toggleButtons.empty(),
            "Native belt constructors fabricated pointer arrays, registered Tile as TabGroup, or lost Toggle entries");
        std::vector<std::string> objectSlots;
        for(std::uint32_t index=0u;index<10u;++index) {
            const auto& slot=object(field(beltView,"DeusEx.HUDObjectBelt.objects",index).value.text);
            Require(slot.classPath=="DeusEx.HUDObjectSlot" && slot.outerPath==slotsView.path &&
                std::find(objectSlots.begin(),objectSlots.end(),slot.path)==objectSlots.end() &&
                radioView.toggleButtons[index]==slot.path,
                "Original belt slot identity, actual Outer or Radio creation-order list differs");
            const auto& ownedSlot=field(beltView,"DeusEx.HUDObjectBelt.objects",index);
            Require(ownedSlot.propertyRoots==std::vector<std::string>{slot.path},"Belt fixed-array slot lost its actual GC edge");
            objectSlots.push_back(slot.path);
            edge(slot,"Extension.Window.winParent",slotsView.path);edge(slot,"Extension.Window.parentOwner",slotsView.path);
            edge(slot,"Extension.Window.firstChild",{});edge(slot,"Extension.Window.lastChild",{});
            edge(slot,"DeusEx.HUDObjectSlot.Player",player);edge(slot,"DeusEx.HUDObjectSlot.Item",{});
            Require(field(slot,"DeusEx.HUDObjectSlot.objectNum").value.integer==static_cast<std::int32_t>(index) &&
                field(slot,"DeusEx.HUDObjectSlot.itemTextPosY").value.integer==32 &&
                field(slot,"DeusEx.HUDObjectSlot.slotFillHeight").value.integer==37 &&
                field(slot,"DeusEx.HUDObjectSlot.slotIconY").value.integer==3,
                "Original HUD slot numbering/text geometry did not run its actual script");
            Require(field(slot,"Extension.Window.bIsVisible").value.boolean &&
                !field(slot,"Extension.Window.bIsSelectable").value.boolean &&
                field(slot,"Extension.Window.lockCount").value.integer==0 &&
                field(slot,"Extension.Window.rowMajorIndex").value.integer==-1 &&
                field(slot,"Extension.Window.colMajorIndex").value.integer==-1 &&
                slot.rowMajorWindows.empty() && slot.colMajorWindows.empty() && slot.tabGroupWindows.empty() &&
                slot.toggleButtons.empty() && !slot.emptyTileRows,
                "Original slot did not finish native constructor selection insertion/removal and unlock");
            Require(field(slot,"Extension.Window.Width").value.floating==10.0f && field(slot,"Extension.Window.Height").value.floating==10.0f &&
                field(slot,"Extension.Window.hardcodedWidth").value.floating==(index==0u?44.0f:51.0f) &&
                field(slot,"Extension.Window.hardcodedHeight").value.floating==54.0f,
                "Original first-slot SetWidth or all-slot requested/current size separation differs");
            for(const auto* font:{"normalFont","boldFont"}) {
                edge(slot,"Extension.Window."+std::string(font),"DeusExUI.FontTiny");
                edge(object(field(slot,"Extension.Window.wGc").value.text),"Extension.GC."+std::string(font),"DeusExUI.FontTiny");
                edge(object(field(slot,"Extension.Window.gGc").value.text),"Extension.GC."+std::string(font),{});
            }
            for(const auto* name:{"HAlign","VAlign"})
                Require(field(slot,"Extension.TextWindow."+std::string(name)).value.integer==1,
                    "Native Text constructor omitted its original centered alignment");
            for(const auto* name:{"hMargin","vMargin"})
                Require(field(slot,"Extension.TextWindow."+std::string(name)).value.floating==3.0f,
                    "Native Text constructor omitted its original margin");
            Require(field(slot,"Extension.TextWindow.bWordWrap").value.boolean &&
                field(slot,"Extension.TextWindow.bTextIsAccelerator").value.boolean &&
                field(slot,"Extension.TextWindow.MinWidth").value.floating==0.0f &&
                field(slot,"Extension.TextWindow.minLines").value.integer==-1 &&
                field(slot,"Extension.TextWindow.MaxLines").value.integer==-1 &&
                field(slot,"Extension.TextWindow.Text").value.kind==Kind::String &&
                field(slot,"Extension.TextWindow.Text").value.text.empty() &&
                field(slot,"Extension.Window.acceleratorKey").value.integer==0,
                "Native Text constructor/accelerator used invented string, line or key defaults");
            for(const auto* name:{"bButtonPressed","bMousePressed","bAutoRepeat"})
                Require(!field(slot,"Extension.ButtonWindow."+std::string(name)).value.boolean,
                    "Native Button constructor did not clear its original pressed/repeat flags");
            Require(QuestVr::Vm::Equal(field(slot,"Extension.ButtonWindow.bEnableRightMouseClick").value,
                ReadPortableClassDefault("DeusEx.HUDObjectSlot","bEnableRightMouseClick")),
                "Native Button constructor overwrote its preserved right-click CDO flag");
            for(const auto& [name,value]:{std::pair{"activateDelay",0.3f},std::pair{"initialDelay",0.5f},std::pair{"repeatRate",0.1f}})
                Require(field(slot,"Extension.ButtonWindow."+std::string(name)).value.floating==value,
                    "Native Button constructor omitted its original timing");
            for(const auto* name:{"curTexture","pressSound","clickSound"})
                edge(slot,"Extension.ButtonWindow."+std::string(name),{});
            for(const auto* name:{"enableSound","disableSound"})
                edge(slot,"Extension.ToggleWindow."+std::string(name),{});
            Require(field(slot,"Extension.ButtonWindow.activateTimer").value.integer==0 &&
                field(slot,"Extension.ButtonWindow.repeatTime").value.floating==0.0f &&
                field(slot,"Extension.ButtonWindow.lastInputKey").value.integer==0,
                "Native Button constructor did not reset its actual timing/key state");
            const auto rgb=[&](const Value& color,unsigned r,unsigned g,unsigned b,unsigned a) {
                Require(color.kind==Kind::Struct && color.fields.size()==4u,"Native slot Color lost its authored shape");
                for(const auto& [name,value]:{std::pair{"r",r},std::pair{"g",g},std::pair{"b",b},std::pair{"a",a}})
                    Require(color.fields.at(name).kind==Kind::Byte && color.fields.at(name).integer==static_cast<std::int32_t>(value),
                        "Native slot RGB or explicitly portable A0 storage policy differs");
            };
            for(const auto* name:{"curTileColor","curTextColor"})
                rgb(field(slot,"Extension.ButtonWindow."+std::string(name)).value,255u,255u,255u,0u);
            for(std::uint32_t record=0u;record<6u;++record) {
                const auto& info=field(slot,"Extension.ButtonWindow.Info",record);
                Require(info.value.kind==Kind::Struct && info.value.fields.size()==3u && info.propertyRoots.empty() &&
                    info.value.fields.at("tx").kind==Kind::Object && info.value.fields.at("tx").text.empty(),
                    "Native Button did not clear all six actual display records/texture edges");
                rgb(info.value.fields.at("tlcolor"),255u,255u,255u,0u);
                rgb(info.value.fields.at("txcolor"),0u,255u,0u,0u);
            }
            for(const auto* name:{"colBackground","colObjectNum"})
                rgb(field(slot,"DeusEx.HUDObjectSlot."+std::string(name)).value,0u,0u,0u,0u);
            const auto originalSelected=ReadPortableClassDefault("DeusEx.HUDObjectSlot","colSelected");
            rgb(field(slot,"DeusEx.HUDObjectSlot.colSelected").value,0u,0u,0u,
                static_cast<unsigned>(originalSelected.fields.at("a").integer));
            const bool translucent=Declared(player,"DeusEx.DeusExPlayer.bHUDBackgroundTranslucent",Kind::Bool).boolean;
            Require(field(slot,"DeusEx.HUDObjectSlot.backgroundDrawStyle").value.integer==(translucent?3:2) &&
                QuestVr::Vm::Equal(field(slot,"DeusEx.HUDObjectSlot.fillMode").value,ReadPortableClassDefault("DeusEx.HUDObjectSlot","fillMode")) &&
                QuestVr::Vm::Equal(field(slot,"DeusEx.HUDObjectSlot.bAllowDragging").value,ReadPortableClassDefault("DeusEx.HUDObjectSlot","bAllowDragging")),
                "Original slot theme/settings script changed actual translucency, fill or dragging state");
        }
        Require(QuestVr::Vm::Equal(field(beltView,"DeusEx.HUDObjectBelt.KeyRingSlot").value,
            ReadPortableClassDefault("DeusEx.HUDObjectBelt","KeyRingSlot")) &&
            Declared(player,"DeusEx.DeusExPlayer.KeyRing",Kind::Object).text.empty(),
            "Dormant belt prefix fabricated starter-ring ownership or changed original keyring slot");
        const std::array<std::size_t,10u> siblingOrder{0u,9u,8u,7u,6u,5u,4u,3u,2u,1u};
        edge(slotsView,"Extension.Window.firstChild",objectSlots[siblingOrder.front()]);
        edge(slotsView,"Extension.Window.lastChild",objectSlots[siblingOrder.back()]);
        for(std::size_t index=0u;index<siblingOrder.size();++index) {
            const auto& slot=object(objectSlots[siblingOrder[index]]);
            edge(slot,"Extension.Window.prevSibling",index==0u?std::string{}:objectSlots[siblingOrder[index-1u]]);
            edge(slot,"Extension.Window.nextSibling",index+1u==siblingOrder.size()?std::string{}:objectSlots[siblingOrder[index+1u]]);
        }
        Require(field(radioView,"Extension.Window.windowType").value.integer==1 &&
            field(slotsView,"Extension.Window.windowType").value.integer==0 &&
            !field(radioView,"Extension.RadioBoxWindow.bOneCheck").value.boolean &&
            field(radioView,"Extension.RadioBoxWindow.currentSelection").value.text.empty() &&
            field(radioView,"Extension.TabGroupWindow.bSizeParentToChildren").value.boolean &&
            !field(radioView,"Extension.TabGroupWindow.bSizeChildrenToParent").value.boolean &&
            field(radioView,"Extension.TabGroupWindow.firstAbsX").value.floating==0.0f &&
            field(radioView,"Extension.TabGroupWindow.firstAbsY").value.floating==0.0f,
            "Native Radio Init/registration or original later bOneCheck=false assignment differs");
        Require(field(beltView,"Extension.Window.X").value.floating==-531.0f &&
            field(beltView,"Extension.Window.Y").value.floating==-59.0f &&
            field(beltView,"Extension.Window.Width").value.floating==541.0f &&
            field(beltView,"Extension.Window.Height").value.floating==69.0f &&
            field(beltView,"Extension.Window.bConfigured").value.boolean &&
            !field(beltView,"Extension.Window.bNeedsReconfigure").value.boolean &&
            field(radioView,"Extension.Window.bConfigured").value.boolean &&
            !field(slotsView,"Extension.Window.bConfigured").value.boolean,
            "Original later HUD callback did not configure the assigned belt while retaining its unconfigured Tile child");
        for(const auto* view:{&radioView,&slotsView}) {
            Require(field(*view,"Extension.Window.bIsVisible").value.boolean &&
                field(*view,"Extension.Window.lockCount").value.integer==0 &&
                field(*view,"Extension.Window.Width").value.floating==10.0f && field(*view,"Extension.Window.Height").value.floating==10.0f,
                "Native Radio/Tile hidden-ancestor Show published layout or retained its Init lock");
            for(const auto* font:{"normalFont","boldFont"}) {
                edge(*view,"Extension.Window."+std::string(font),"Extension.TechMedium");
                edge(object(field(*view,"Extension.Window.wGc").value.text),"Extension.GC."+std::string(font),"Extension.TechMedium");
                edge(object(field(*view,"Extension.Window.gGc").value.text),"Extension.GC."+std::string(font),{});
            }
        }
        for(const auto& [name,value]:{std::pair{"hMargin0",10.0f},std::pair{"vMargin0",6.0f},
            std::pair{"hardcodedWidth",504.0f},std::pair{"hardcodedHeight",54.0f}})
            Require(field(radioView,"Extension.Window."+std::string(name)).value.floating==value,
                "Original belt Radio SetSize/SetPos did not preserve requested dimensions");
        for(const auto& [name,value]:{std::pair{"hMargin",0.0f},std::pair{"vMargin",0.0f},
            std::pair{"minorSpacing",0.0f},std::pair{"majorSpacing",1.0f}})
            Require(field(slotsView,"Extension.TileWindow."+std::string(name)).value.floating==value,
                "Original Tile constructor/default spacing or margin setters differ");
        for(const auto& [name,value]:{std::pair{"orientation",0},std::pair{"hDirection",1},
            std::pair{"vDirection",1},std::pair{"hChildAlign",3},std::pair{"vChildAlign",3}})
            Require(field(slotsView,"Extension.TileWindow."+std::string(name)).value.integer==value,
                "Original Tile SetOrder7 or constructor alignments differ");
        Require(field(slotsView,"Extension.TileWindow.bWrap").value.boolean && field(slotsView,"Extension.TileWindow.bFillParent").value.boolean &&
            !field(slotsView,"Extension.TileWindow.bEqualWidth").value.boolean && field(slotsView,"Extension.TileWindow.bEqualHeight").value.boolean,
            "Original Tile setters changed unrelated constructor flags");
        edge(rootView,"Extension.Window.firstChild",scope.path);edge(rootView,"Extension.Window.lastChild",hudView.path);
        edge(scope,"Extension.Window.prevSibling",{});edge(scope,"Extension.Window.nextSibling",display.path);
        // The original Tile callbacks have now returned. Damage, compass,
        // multi-skills and information children completed construction. The
        // Hidden HUDLogDisplay/TextLog sizing now returns through genuine
        // GetGC/Copy/Pop. All HUD controls, including received-item text, now
        // finish. Original UpdateSettings and scope construction now return;
        // Lower prepends the hidden scope before the actor/HUD siblings.
        edge(rootView,"DeusEx.DeusExRootWindow.hud",hudView.path);
        edge(hudView,"DeusEx.DeusExHUD.Ammo",ammoView.path);edge(hudView,"DeusEx.DeusExHUD.hit",hitView.path);
        edge(hudView,"DeusEx.DeusExHUD.Cross",crossView.path);edge(hudView,"DeusEx.DeusExHUD.belt",beltView.path);
        edge(display,"Extension.Window.parentOwner",rootView.path);edge(display,"Extension.Window.winParent",rootView.path);
        edge(display,"Extension.Window.prevSibling",scope.path);edge(display,"Extension.Window.nextSibling",hudView.path);
        edge(hudView,"Extension.Window.parentOwner",rootView.path);edge(hudView,"Extension.Window.winParent",rootView.path);
        edge(hudView,"Extension.Window.prevSibling",display.path);edge(hudView,"Extension.Window.nextSibling",{});
        edge(hudView,"DeusEx.DeusExHUD.activeItems",activeItems.path);
        edge(hudView,"DeusEx.DeusExHUD.damageDisplay",damage.path);
        edge(hudView,"DeusEx.DeusExHUD.compass",compass.path);
        edge(hudView,"DeusEx.DeusExHUD.hms",multiSkills.path);
        edge(hudView,"DeusEx.DeusExHUD.Info",information.path);
        edge(hudView,"DeusEx.DeusExHUD.msgLog",logDisplay.path);
        edge(hudView,"DeusEx.DeusExHUD.frobDisplay",frobDisplay.path);
        edge(hudView,"DeusEx.DeusExHUD.augDisplay",augmentationDisplay.path);
        edge(hudView,"DeusEx.DeusExHUD.startDisplay",missionStart.path);
        edge(hudView,"DeusEx.DeusExHUD.barkDisplay",barkDisplay.path);
        edge(hudView,"DeusEx.DeusExHUD.receivedItems",receivedDisplay.path);
        edge(damage,"DeusEx.DamageHUDDisplay.Player",player);
        // Original DamageHUDDisplay.InitWindow PC29..848 assigns each of the
        // seven sIconInfo records, including every authored DamageType[3]
        // Name slot. Unassigned names retain typed None; the nested Icon is a
        // real owning property edge, not merely a presentation texture string.
        const std::array<std::array<const char*,3u>,7u> damageTypes{{
            {{"Shocked","None","None"}},{{"EMP","None","None"}},{{"Burned","Flamed","exploded"}},
            {{"PoisonGas","TearGas","None"}},{{"Drowned","HalonGas","None"}},
            {{"Radiation","None","None"}},{{"shot","Sabot","None"}}
        }};
        const std::array<const char*,7u> damageIcons{{"DamageIconElectricity","DamageIconEMP","DamageIconFire",
            "DamageIconGas","DamageIconOxygen","DamageIconRadiation",""}};
        const std::array<std::array<int,3u>,7u> damageColors{{
            {{0,196,255}},{{0,196,255}},{{255,96,0}},{{0,196,0}},{{0,128,255}},{{255,255,0}},{{255,0,0}}
        }};
        Require(std::count_if(damage.fields.begin(),damage.fields.end(),[](const auto& entry) {
            return entry.declaration=="DeusEx.DamageHUDDisplay.iconInfo";
        })==7,"Original damage display snapshot did not retain exactly seven authored iconInfo elements");
        for(std::uint32_t slot=0u;slot<7u;++slot) {
            const auto& captured=field(damage,"DeusEx.DamageHUDDisplay.iconInfo",slot);const auto& value=captured.value;
            Require(value.kind==Kind::Struct && value.fields.size()==7u && value.text.empty() && value.intArray.empty(),
                "Original iconInfo element lacks its canonical seven-field Struct: "+std::to_string(slot));
            const auto& names=value.fields.at("damagetype");
            Require(names.kind==Kind::Struct && names.fields.size()==3u && names.text.empty() && names.intArray.empty(),
                "Original iconInfo DamageType[3] is not its canonical fixed-member envelope: "+std::to_string(slot));
            for(std::size_t index=0u;index<3u;++index) {
                const auto& name=names.fields.at(std::to_string(index));
                Require(name.kind==Kind::Name && name.fields.empty() && name.intArray.empty() &&
                    name.text==damageTypes[slot][index],
                    "Original iconInfo lost a typed assigned/default DamageType slot: "+std::to_string(slot)+'.'+std::to_string(index));
            }
            const std::string texture=*damageIcons[slot]?"DeusExUI.Icons."+std::string(damageIcons[slot]):std::string{};
            const auto& icon=value.fields.at("icon");
            Require(icon.kind==Kind::Object && icon.fields.empty() && icon.intArray.empty() && icon.text==texture &&
                captured.propertyRoots==(texture.empty()?std::vector<std::string>{}:std::vector<std::string>{texture}),
                "Original iconInfo nested Icon value/actual owning GC edge differs: "+std::to_string(slot));
            const auto& color=value.fields.at("color");
            Require(color.kind==Kind::Struct && color.fields.size()==4u && color.text.empty() && color.intArray.empty(),
                "Original iconInfo Color lacks canonical RGBA storage: "+std::to_string(slot));
            for(const auto& [channel,expected]:{
                std::pair{"r",damageColors[slot][0]},std::pair{"g",damageColors[slot][1]},
                std::pair{"b",damageColors[slot][2]},std::pair{"a",0}}) {
                const auto& component=color.fields.at(channel);
                Require(component.kind==Kind::Byte && component.integer==expected && component.fields.empty() &&
                    component.text.empty() && component.intArray.empty(),
                    "Original iconInfo RGB writes/unchanged alpha0 differ: "+std::to_string(slot)+'.'+channel);
            }
            for(const auto* flag:{"bactive","bhitcenter"}) {
                const auto& unchanged=value.fields.at(flag);
                Require(unchanged.kind==Kind::Bool && !unchanged.boolean && unchanged.text.empty() &&
                    unchanged.fields.empty() && unchanged.intArray.empty(),"Original iconInfo fabricated an active/hit flag");
            }
            for(const auto* counter:{"inittime","hitdir"}) {
                const auto& unchanged=value.fields.at(counter);
                Require(unchanged.kind==Kind::Float && unchanged.floating==0.0f && !std::signbit(unchanged.floating) &&
                    unchanged.text.empty() && unchanged.fields.empty() && unchanged.intArray.empty(),
                    "Original iconInfo fabricated a timer/direction or lost positive-zero default");
            }
        }
        std::cout<<"ORIGINAL DamageHUDDisplay: all7 iconInfo records, all21 typed DamageType slots, six nested texture GC edges and colors verified\n";
        edge(hudView,"Extension.Window.firstChild",augmentationDisplay.path);edge(hudView,"Extension.Window.lastChild",receivedDisplay.path);
        // Original AugmentationDisplayWindow.InitWindow calls Lower1412;
        // traversal order is not construction order after that real prepend.
        edge(augmentationDisplay,"Extension.Window.winParent",hudView.path);
        edge(augmentationDisplay,"Extension.Window.parentOwner",hudView.path);
        edge(augmentationDisplay,"Extension.Window.prevSibling",{});
        edge(augmentationDisplay,"Extension.Window.nextSibling",ammoView.path);
        Require(field(augmentationDisplay,"Extension.Window.bTickEnabled").value.boolean&&
            field(augmentationDisplay,"Extension.Window.bIsVisible").value.boolean&&
            field(augmentationDisplay,"Extension.Window.lockCount").value.integer==0,
            "Original augmentation display did not finish Tick/Lower/visibility initialization");
        edge(activeItems,"Extension.Window.winParent",hudView.path);edge(activeItems,"Extension.Window.parentOwner",hudView.path);
        edge(activeItems,"Extension.Window.prevSibling",beltView.path);edge(activeItems,"Extension.Window.nextSibling",damage.path);
        edge(damage,"Extension.Window.winParent",hudView.path);edge(damage,"Extension.Window.parentOwner",hudView.path);
        edge(damage,"Extension.Window.prevSibling",activeItems.path);edge(damage,"Extension.Window.nextSibling",compass.path);
        const std::array<const PortableNativeWindowObjectSnapshot*,9u> laterChildren{
            &damage,&compass,&multiSkills,&information,&logDisplay,&frobDisplay,&missionStart,&barkDisplay,&receivedDisplay};
        for(std::size_t index=1u;index<laterChildren.size();++index) {
            const auto& view=*laterChildren[index];
            Require(view.outerPath==hudView.path,"Original later HUD child lost its actual Outer");
            edge(view,"Extension.Window.winParent",hudView.path);edge(view,"Extension.Window.parentOwner",hudView.path);
            edge(view,"Extension.Window.prevSibling",laterChildren[index-1u]->path);
            edge(view,"Extension.Window.nextSibling",index+1u==laterChildren.size()?std::string{}:laterChildren[index+1u]->path);
            const bool expectedVisible=index==1u?Declared(player,"DeusEx.DeusExPlayer.bCompassVisible",Kind::Bool).boolean:
                index<3u||index==5u;
            Require(field(view,"Extension.Window.lockCount").value.integer==0 &&
                field(view,"Extension.Window.bIsVisible").value.boolean==expectedVisible,
                "Original later HUD NewChild visibility/lock prefix differs");
        }
        edge(frobDisplay,"Extension.Window.firstChild",{});edge(frobDisplay,"Extension.Window.lastChild",{});
        const auto& frobGC=object(field(frobDisplay,"Extension.Window.wGc").value.text);
        const auto& frobGeneralGC=object(field(frobDisplay,"Extension.Window.gGc").value.text);
        Require(frobGC.path!=frobGeneralGC.path && frobGC.classPath=="Extension.GC" &&
            frobGeneralGC.classPath=="Extension.GC" && frobGC.outerPath==frobDisplay.path &&
            frobGeneralGC.outerPath==frobDisplay.path,
            "In-flight original Frob child did not own its two distinct native graphics contexts");
        edge(frobDisplay,"Extension.Window.wGc",frobGC.path);
        edge(frobDisplay,"Extension.Window.gGc",frobGeneralGC.path);
        edge(ammoView,"Extension.Window.parentOwner",hudView.path);edge(ammoView,"Extension.Window.winParent",hudView.path);
        edge(ammoView,"Extension.Window.prevSibling",augmentationDisplay.path);edge(ammoView,"Extension.Window.nextSibling",hitView.path);
        edge(ammoView,"DeusEx.HUDBaseWindow.Player",player);
        edge(ammoView,"DeusEx.HUDAmmoDisplay.Player",player);
        edge(hitView,"DeusEx.HUDBaseWindow.Player",player);edge(hitView,"DeusEx.HUDHitDisplay.Player",player);
        edge(hitView,"Extension.Window.parentOwner",hudView.path);edge(hitView,"Extension.Window.winParent",hudView.path);
        edge(hitView,"Extension.Window.prevSibling",ammoView.path);edge(hitView,"Extension.Window.nextSibling",crossView.path);
        edge(crossView,"Extension.Window.parentOwner",hudView.path);edge(crossView,"Extension.Window.winParent",hudView.path);
        edge(crossView,"Extension.Window.prevSibling",hitView.path);edge(crossView,"Extension.Window.nextSibling",beltView.path);
        edge(beltView,"Extension.Window.parentOwner",hudView.path);edge(beltView,"Extension.Window.winParent",hudView.path);
        edge(beltView,"Extension.Window.prevSibling",crossView.path);edge(beltView,"Extension.Window.nextSibling",activeItems.path);
        edge(beltView,"Extension.Window.firstChild",radioView.path);edge(beltView,"Extension.Window.lastChild",radioView.path);
        const auto& shadow=object(field(missionStart,"DeusEx.HUDMissionStartTextDisplay.winTextShadow").value.text);
        const auto& foreground=object(field(missionStart,"DeusEx.HUDMissionStartTextDisplay.winText").value.text);
        edge(missionStart,"Extension.Window.firstChild",shadow.path);edge(missionStart,"Extension.Window.lastChild",foreground.path);
        Require(shadow.path!=foreground.path&&shadow.classPath=="Extension.TextWindow"&&foreground.classPath=="Extension.TextWindow"&&
            shadow.outerPath==missionStart.path&&foreground.outerPath==missionStart.path,
            "Original mission-start initialization lost its actual shadow/foreground TextWindows");
        edge(shadow,"Extension.Window.prevSibling",{});edge(shadow,"Extension.Window.nextSibling",foreground.path);
        edge(foreground,"Extension.Window.prevSibling",shadow.path);edge(foreground,"Extension.Window.nextSibling",{});
        const auto missionFont=ReadPortableClassDefault("DeusEx.HUDMissionStartTextDisplay","fontText");
        Require(missionFont.kind==Kind::Object&&!missionFont.text.empty(),"Original mission-start CDO font missing");
        const auto& copiedMissionFont=field(missionStart,"DeusEx.HUDMissionStartTextDisplay.fontText");
        Require(QuestVr::Vm::Equal(copiedMissionFont.value,missionFont)&&copiedMissionFont.propertyRoots.empty(),
            "Mission-start font lost its unchanged copied CDO value or fabricated an instance native write");
        for(const auto* view:{&shadow,&foreground})for(const auto* name:{"normalFont","boldFont"})
            edge(*view,"Extension.Window."+std::string(name),missionFont.text);
        for(const auto* name:{"hMargin","vMargin"}) {
            Require(field(shadow,"Extension.TextWindow."+std::string(name)).value.floating==0.0f&&
                field(foreground,"Extension.TextWindow."+std::string(name)).value.floating==0.0f,
                "Completed mission-start text lost its shadow/foreground zero margins");
        }
        const auto& foregroundGC=object(field(foreground,"Extension.Window.wGc").value.text);
        Require(field(foreground,"Extension.Window.bTextTranslucent").value.boolean&&
            field(foregroundGC,"Extension.GC.bTextTranslucent").value.boolean&&
            field(foregroundGC,"Extension.GC.textPolyFlags").value.integer==0x804,
            "Original mission-start1483 did not update the actual Window/drawing-context text flags");
        for(const auto* view:{&barkDisplay,&receivedDisplay}) {
            const auto& tile=object(field(*view,"Extension.Window.firstChild").value.text);
            Require(tile.classPath=="Extension.TileWindow"&&tile.outerPath==view->path,
                "Original later display lost its native Tile child");
            edge(*view,"Extension.Window.lastChild",tile.path);
            edge(tile,"Extension.Window.winParent",view->path);edge(tile,"Extension.Window.parentOwner",view->path);
            if(view==&barkDisplay)edge(tile,"Extension.Window.firstChild",{});
            else {
                const auto& label=object(field(tile,"Extension.Window.firstChild").value.text);
                const auto receivedLabel=ReadPortableClassDefault("DeusEx.HUDReceivedDisplay","TextReceivedLabel");
                Require(receivedLabel.kind==Kind::String&&!receivedLabel.text.empty()&&
                    label.classPath=="Extension.TextWindow"&&label.outerPath==tile.path&&
                    field(label,"Extension.TextWindow.Text").value.kind==Kind::String&&
                    QuestVr::Vm::Equal(field(label,"Extension.TextWindow.Text").value,receivedLabel),
                    "Completed received-item label lost the original localized string passed to1550");
                edge(tile,"Extension.Window.lastChild",label.path);
                edge(label,"Extension.Window.winParent",tile.path);edge(label,"Extension.Window.parentOwner",tile.path);
            }
        }
        // Original HUDAmmoDisplay/HUDHitDisplay.SetVisibility change their
        // script bVisible only; unlike Crosshair/active-items/damage they do
        // not call Window.Show. Their local Window visibility stays true.
        Require(hudView.outerPath==rootView.path && ammoView.outerPath==hudView.path &&
            hitView.outerPath==hudView.path && crossView.outerPath==hudView.path && beltView.outerPath==hudView.path &&
            field(hudView,"Extension.Window.bIsVisible").value.boolean &&
            field(ammoView,"Extension.Window.bIsVisible").value.boolean && field(hitView,"Extension.Window.bIsVisible").value.boolean &&
            field(crossView,"Extension.Window.bIsVisible").value.boolean==Declared(player,"DeusEx.DeusExPlayer.bCrosshairVisible",Kind::Bool).boolean &&
            !Declared(player,"DeusEx.DeusExPlayer.bObjectBeltVisible",Kind::Bool).boolean&&
            !field(beltView,"Extension.Window.bIsVisible").value.boolean &&
            !field(hudView,"Extension.Window.bIsSensitive").value.boolean && field(ammoView,"Extension.Window.bIsSensitive").value.boolean &&
            field(hudView,"Extension.Window.lockCount").value.integer==0 && field(ammoView,"Extension.Window.lockCount").value.integer==0 &&
            field(hitView,"Extension.Window.lockCount").value.integer==0 && field(crossView,"Extension.Window.lockCount").value.integer==0 &&
            field(beltView,"Extension.Window.lockCount").value.integer==0 && field(activeItems,"Extension.Window.lockCount").value.integer==0 &&
            field(damage,"Extension.Window.lockCount").value.integer==0 &&
            field(activeItems,"Extension.Window.bIsVisible").value.boolean==Declared(player,"DeusEx.DeusExPlayer.bAugDisplayVisible",Kind::Bool).boolean &&
            field(damage,"Extension.Window.bIsVisible").value.boolean==Declared(player,"DeusEx.DeusExPlayer.bHitDisplayVisible",Kind::Bool).boolean,
            "Completed original HUD below the hidden root lost its visibility, sensitivity or child lock state");
        edge(activeItems,"DeusEx.HUDBaseWindow.Player",player);
        const auto& augsBorder=object(field(activeItems,"DeusEx.HUDActiveItemsDisplay.winAugsContainer").value.text);
        const auto& itemsBorder=object(field(activeItems,"DeusEx.HUDActiveItemsDisplay.winItemsContainer").value.text);
        Require(augsBorder.classPath=="DeusEx.HUDActiveAugsBorder" && itemsBorder.classPath=="DeusEx.HUDActiveItemsBorder" &&
            augsBorder.outerPath==activeItems.path && itemsBorder.outerPath==activeItems.path,
            "Original active-item display did not retain its distinct augmentation/item containers");
        edge(activeItems,"DeusEx.HUDActiveItemsDisplay.winAugsContainer",augsBorder.path);
        edge(activeItems,"DeusEx.HUDActiveItemsDisplay.winItemsContainer",itemsBorder.path);
        edge(activeItems,"Extension.Window.firstChild",augsBorder.path);edge(activeItems,"Extension.Window.lastChild",itemsBorder.path);
        edge(augsBorder,"Extension.Window.prevSibling",{});edge(augsBorder,"Extension.Window.nextSibling",itemsBorder.path);
        edge(itemsBorder,"Extension.Window.prevSibling",augsBorder.path);edge(itemsBorder,"Extension.Window.nextSibling",{});
        for(const auto* border:{&augsBorder,&itemsBorder}) {
            edge(*border,"Extension.Window.winParent",activeItems.path);edge(*border,"Extension.Window.parentOwner",activeItems.path);
            edge(*border,"DeusEx.HUDBaseWindow.Player",player);
            Require(field(*border,"Extension.Window.bIsVisible").value.boolean &&
                field(*border,"Extension.Window.lockCount").value.integer==0 &&
                field(*border,"DeusEx.HUDActiveItemsBorderBase.iconCount").value.integer==0,
                "Original active-item/augmentation NewChild Show did not finish, retained a lock or fabricated an active icon");
            for(const auto* name:{"texBorderTop","texBorderCenter","texBorderBottom"}) {
                const auto expected=ReadPortableClassDefault(border->classPath,name);
                Require(expected.kind==Kind::Object && !expected.text.empty(),"Original active-border texture metadata is absent");
                const auto& texture=field(*border,"DeusEx.HUDActiveItemsBorderBase."+std::string(name));
                // Untouched class-default references are owned by the loaded
                // package graph, not a fabricated native property-write root.
                Require(QuestVr::Vm::Equal(texture.value,expected) && texture.propertyRoots.empty(),
                    "Original active-border texture changed its authored CDO value or fabricated a property write");
            }
            for(const auto* name:{"borderTopMargin","borderBottomMargin","borderWidth","topHeight","topOffset",
                "bottomHeight","bottomOffset","tilePosX","tilePosY"})
                Require(QuestVr::Vm::Equal(field(*border,"DeusEx.HUDActiveItemsBorderBase."+std::string(name)).value,
                    ReadPortableClassDefault(border->classPath,name)),"Original active-border authored geometry changed");
            const auto& tile=object(field(*border,"DeusEx.HUDActiveItemsBorderBase.winIcons").value.text);
            Require(tile.classPath=="Extension.TileWindow" && tile.outerPath==border->path && tile.emptyTileRows &&
                tile.tileRowsInitialized && tile.tileRows.empty(),
                "Original active-border icon child is not its owned TileWindow");
            edge(*border,"DeusEx.HUDActiveItemsBorderBase.winIcons",tile.path);
            edge(*border,"Extension.Window.firstChild",tile.path);edge(*border,"Extension.Window.lastChild",tile.path);
            edge(tile,"Extension.Window.winParent",border->path);edge(tile,"Extension.Window.parentOwner",border->path);
            edge(tile,"Extension.Window.prevSibling",{});edge(tile,"Extension.Window.nextSibling",{});
            Require(field(tile,"Extension.Window.lockCount").value.integer==0 &&
                field(tile,"Extension.Window.bIsVisible").value.boolean &&
                field(tile,"Extension.TileWindow.hMargin").value.floating==0.0f &&
                field(tile,"Extension.TileWindow.vMargin").value.floating==0.0f &&
                field(tile,"Extension.TileWindow.minorSpacing").value.floating==2.0f,
                "Original active-border tile did not complete its margin/spacing initialization");
            Require(field(tile,"Extension.Window.hMargin0").value.floating==
                static_cast<float>(field(*border,"DeusEx.HUDActiveItemsBorderBase.tilePosX").value.integer) &&
                field(tile,"Extension.Window.vMargin0").value.floating==
                static_cast<float>(field(*border,"DeusEx.HUDActiveItemsBorderBase.tilePosY").value.integer),
                "Original active-border tile did not use its authored icon offset");
        }
        const auto& augTile=object(field(augsBorder,"DeusEx.HUDActiveItemsBorderBase.winIcons").value.text);
        const auto& itemTile=object(field(itemsBorder,"DeusEx.HUDActiveItemsBorderBase.winIcons").value.text);
        // Both original border callbacks and their actual native Tile sizing
        // returned, including the augmentation Tile's ten hidden children.
        // The supplied negative height remains specified, not a cached sum.
        Require(!field(activeItems,"Extension.Window.bNeedsQuery").value.boolean &&
            !field(activeItems,"Extension.Window.bLastWidthSpecified").value.boolean &&
            field(activeItems,"Extension.Window.bLastHeightSpecified").value.boolean &&
            field(activeItems,"Extension.Window.lastSpecifiedWidth").value.floating==-1.0f &&
            field(activeItems,"Extension.Window.lastSpecifiedHeight").value.floating==
                field(hudView,"Extension.Window.Height").value.floating-field(beltView,"Extension.Window.Height").value.floating,
            "Original1463 did not cache its actual height constraint before the selected active-item callback");
        Require(field(activeItems,"Extension.Window.lastQueryWidth").value.floating==110.0f &&
            field(activeItems,"Extension.Window.lastQueryHeight").value.floating==-59.0f &&
            field(activeItems,"Extension.Window.X").value.floating==-100.0f &&
            field(activeItems,"Extension.Window.Y").value.floating==0.0f &&
            field(activeItems,"Extension.Window.Width").value.floating==110.0f &&
            field(activeItems,"Extension.Window.Height").value.floating==-59.0f &&
            field(activeItems,"Extension.Window.bConfigured").value.boolean,
            "Original active-item callback did not complete its actual post-query/ConfigureChild result");
        for(const auto& [view,width,height]:{
            std::tuple{&augsBorder,62.0f,22.0f},std::tuple{&itemsBorder,48.0f,13.0f},
            std::tuple{&augTile,0.0f,0.0f},std::tuple{&itemTile,0.0f,0.0f}}) {
            Require(!field(*view,"Extension.Window.bNeedsQuery").value.boolean &&
                !field(*view,"Extension.Window.bLastWidthSpecified").value.boolean &&
                !field(*view,"Extension.Window.bLastHeightSpecified").value.boolean &&
                field(*view,"Extension.Window.lastSpecifiedWidth").value.floating==-1.0f &&
                field(*view,"Extension.Window.lastSpecifiedHeight").value.floating==-1.0f &&
                field(*view,"Extension.Window.lastQueryWidth").value.floating==width &&
                field(*view,"Extension.Window.lastQueryHeight").value.floating==height,
                "Original preferred-size Tile/border callback lost its completed pre/post cache values: "+view->path);
        }
        Require(field(itemsBorder,"Extension.Window.X").value.floating==14.0f &&
            field(itemsBorder,"Extension.Window.Y").value.floating==6.0f &&
            field(itemsBorder,"Extension.Window.Width").value.floating==48.0f &&
            field(itemsBorder,"Extension.Window.Height").value.floating==13.0f &&
            field(itemsBorder,"Extension.Window.bConfigured").value.boolean &&
            field(itemTile,"Extension.Window.bConfigured").value.boolean,
            "Original later item border did not complete its actual configuration callback");
        edge(itemTile,"Extension.Window.firstChild",{});edge(itemTile,"Extension.Window.lastChild",{});
        Require(field(augsBorder,"DeusEx.HUDActiveAugsBorder.FirstKeyNum").value.integer==3 &&
            field(augsBorder,"DeusEx.HUDActiveAugsBorder.LastKeyNum").value.integer==12,
            "Original active augmentation hotkey bounds are not F3..F12");
        std::array<bool,10u> seenAugKeys{};
        std::vector<std::string> augPaths;
        for(const auto& aug:pending.objects) if(aug.classPath=="DeusEx.HUDActiveAug") {
            const auto key=field(aug,"DeusEx.HUDActiveAug.HotKeyNum").value.integer;
            Require(key>=3 && key<=12 && !seenAugKeys[static_cast<std::size_t>(key-3)],
                "Original augmentation icons lost a unique F3..F12 hotkey");
            seenAugKeys[static_cast<std::size_t>(key-3)]=true;augPaths.push_back(aug.path);
            Require(aug.outerPath==augTile.path &&
                field(aug,"DeusEx.HUDActiveAug.hotKeyString").value.text=="F"+std::to_string(key),
                "Original IntToString/Concat did not produce the actual augmentation hotkey label");
            edge(aug,"Extension.Window.winParent",augTile.path);edge(aug,"Extension.Window.parentOwner",augTile.path);
            edge(aug,"DeusEx.HUDBaseWindow.Player",player);edge(aug,"DeusEx.HUDActiveItemBase.Icon",{});
            const auto& background=field(aug,"DeusEx.HUDActiveItemBase.texBackground");
            Require(QuestVr::Vm::Equal(background.value,ReadPortableClassDefault(aug.classPath,"texBackground")) &&
                background.propertyRoots.empty(),"Original augmentation background changed its CDO reference or fabricated a write");
            Require(!field(aug,"Extension.Window.bIsVisible").value.boolean &&
                field(aug,"Extension.Window.lockCount").value.integer==0 &&
                field(aug,"DeusEx.HUDActiveItemBase.IconWidth").value.integer==34 &&
                field(aug,"DeusEx.HUDActiveItemBase.IconHeight").value.integer==34 &&
                field(aug,"Extension.Window.hardcodedWidth").value.floating==34.0f &&
                field(aug,"Extension.Window.hardcodedHeight").value.floating==34.0f &&
                field(aug,"Extension.Window.Width").value.floating==10.0f &&
                field(aug,"Extension.Window.Height").value.floating==10.0f,
                "Original augmentation icon did not finish hidden 34x34 requested-size initialization");
            for(const auto* name:{"holdWidth","holdHeight","holdX","holdY"}) {
                const auto& held=field(aug,"Extension.Window."+std::string(name)).value;
                Require(held.kind==Kind::Float && held.floating==0.0f && !std::signbit(held.floating),
                    "Original native Tile hidden-child pass lost a held-geometry +0 store: "+aug.path+'.'+name);
            }
            Require(field(aug,"Extension.Window.bNeedsQuery").value.boolean &&
                field(aug,"Extension.Window.bLastWidthSpecified").value.boolean &&
                field(aug,"Extension.Window.bLastHeightSpecified").value.boolean,
                "Native Tile incorrectly queried a hidden augmentation child");
            for(const auto* name:{"lastSpecifiedWidth","lastSpecifiedHeight","lastQueryWidth","lastQueryHeight"})
                Require(field(aug,"Extension.Window."+std::string(name)).value.floating==0.0f,
                    "Native Tile changed a hidden augmentation query-cache field");
            for(const auto* name:{"colBlack","colAugActive","colAugInactive"})
                Require(QuestVr::Vm::Equal(field(aug,"DeusEx.HUDActiveAug."+std::string(name)).value,
                    ReadPortableClassDefault(aug.classPath,name)),"Original augmentation icon authored colors changed");
        }
        Require(augPaths.size()==10u && std::all_of(seenAugKeys.begin(),seenAugKeys.end(),[](bool seen) {return seen;}),
            "Original augmentation setup did not create all ten hidden hotkey icons");
        std::vector<std::string> linkedAugPaths;
        auto nextAug=field(augTile,"Extension.Window.firstChild").value.text;
        std::string previousAug;
        while(!nextAug.empty()) {
            Require(linkedAugPaths.size()<10u && std::find(augPaths.begin(),augPaths.end(),nextAug)!=augPaths.end() &&
                std::find(linkedAugPaths.begin(),linkedAugPaths.end(),nextAug)==linkedAugPaths.end(),
                "Original augmentation tile child chain is cyclic, extra or incomplete");
            const auto& aug=object(nextAug);edge(aug,"Extension.Window.prevSibling",previousAug);
            linkedAugPaths.push_back(nextAug);previousAug=nextAug;
            nextAug=field(aug,"Extension.Window.nextSibling").value.text;
        }
        Require(linkedAugPaths.size()==10u,"Original augmentation tile child chain omitted an icon");
        edge(augTile,"Extension.Window.lastChild",previousAug);
        edge(rootView,"Extension.RootWindow.FocusWindow",{});edge(rootView,"Extension.RootWindow.grabbedWindow",{});
        Require(field(display,"Extension.Window.winHAlign").value.integer==3 && field(display,"Extension.Window.winVAlign").value.integer==3,
            "Original SetWindowAlignments did not apply ActorDisplay's Full/Full alignment");
        for(const auto* name:{"hMargin0","vMargin0","hMargin1","vMargin1"}) {
            const auto& margin=field(display,std::string("Extension.Window.")+name).value;
            Require(margin.kind==Kind::Float && margin.floating==0.0f,"Original omitted alignment margins changed");
        }
        edge(display,"Extension.Window.normalFont","Extension.TechMedium");edge(display,"Extension.Window.boldFont","Extension.TechMedium");
        Require(field(display,"Extension.Window.bIsVisible").value.boolean &&
            field(display,"Extension.Window.bNeedsQuery").value.boolean && field(display,"Extension.Window.bNeedsReconfigure").value.boolean &&
            field(display,"Extension.Window.lockCount").value.integer==0 && field(rootView,"Extension.RootWindow.initCount").value.integer==0 &&
            field(rootView,"Extension.Window.bTickEnabled").value.boolean && !field(display,"Extension.Window.bTickEnabled").value.boolean,
            "NewChild hidden-ancestor Show unexpectedly ran init-count/layout or retained its lock");
        const auto& displayGC=object(field(display,"Extension.Window.wGc").value.text);
        const auto& displayGeneralGC=object(field(display,"Extension.Window.gGc").value.text);
        Require(displayGC.path!=displayGeneralGC.path && displayGC.outerPath==display.path && displayGeneralGC.outerPath==display.path,
            "Native child did not allocate two distinct GC UObjects under itself");
        edge(display,"Extension.Window.wGc",displayGC.path);edge(display,"Extension.Window.gGc",displayGeneralGC.path);
        for(const auto* name:{"normalFont","boldFont"}) {
            edge(displayGC,std::string("Extension.GC.")+name,"Extension.TechMedium");
            edge(displayGeneralGC,std::string("Extension.GC.")+name,{});
        }
        for(const auto* view:{&hudView,&ammoView,&hitView,&crossView,&beltView}) {
            edge(*view,"Extension.Window.normalFont","Extension.TechMedium");edge(*view,"Extension.Window.boldFont","Extension.TechMedium");
            const auto& ownGC=object(field(*view,"Extension.Window.wGc").value.text);
            const auto& ownGeneralGC=object(field(*view,"Extension.Window.gGc").value.text);
            Require(ownGC.path!=ownGeneralGC.path && ownGC.outerPath==view->path && ownGeneralGC.outerPath==view->path,
                "Original HUD child lost either native owned GC");
            for(const auto* name:{"normalFont","boldFont"}) {
                edge(ownGC,std::string("Extension.GC.")+name,"Extension.TechMedium");
                edge(ownGeneralGC,std::string("Extension.GC.")+name,{});
            }
        }
        Require(Declared(player,"DeusEx.DeusExPlayer.ThemeManager",Kind::Object).text.empty(),
            "Original theme-null Color handling invented a ThemeManager");
        for(const auto* view:{&ammoView,&hitView})
            for(const auto* name:{"colBackground","colBorder","colText","colHeaderText"}) {
                const auto& color=field(*view,"DeusEx.HUDBaseWindow."+std::string(name)).value;
                Require(color.kind==Kind::Struct && color.fields.size()==4u,"Original Color result lost its native four-channel shape");
                for(const auto* channel:{"r","g","b","a"})
                    Require(color.fields.at(channel).kind==Kind::Byte && color.fields.at(channel).integer==0,
                        "Original null Context did not zero exactly four canonical Color bytes");
            }
        const auto geometry=[&](const PortableNativeWindowObjectSnapshot& view,
            float x,float y,float width,float height) {
            for(const auto& [name,expected]:{std::pair{"hMargin0",x},std::pair{"vMargin0",y},
                std::pair{"hardcodedWidth",width},std::pair{"hardcodedHeight",height},
                std::pair{"hMargin1",0.0f},std::pair{"vMargin1",0.0f},std::pair{"Width",10.0f},std::pair{"Height",10.0f}}) {
                const auto& value=field(view,"Extension.Window."+std::string(name)).value;
                Require(value.kind==Kind::Float && value.floating==expected,
                    "Original health configuration changed far margins/current geometry or requested dimensions: "+view.path+'.'+name);
            }
            Require(field(view,"Extension.Window.winHAlign").value.integer==0 &&
                field(view,"Extension.Window.winVAlign").value.integer==0,
                "Original health configuration did not set absolute alignments");
        };
        const auto colorPlane=[&](const PortableNativeWindowObjectSnapshot& view,
            const std::string& base,const Value& expected) {
            const auto& color=field(view,base+".tileColor").value;
            const auto& plane=field(view,base+".tilePlane").value;
            Require(expected.kind==Kind::Struct && expected.fields.size()==4u &&
                QuestVr::Vm::Equal(color,expected) && plane.kind==Kind::Struct && plane.fields.size()==4u,
                "Original SetTileColor did not retain the exact canonical RGBA/Plane shape: "+view.path);
            for(const auto& [axis,channel]:{std::pair{"x","r"},std::pair{"y","g"},std::pair{"z","b"}}) {
                const auto& value=plane.fields.at(axis);
                Require(expected.fields.at(channel).kind==Kind::Byte && value.kind==Kind::Float &&
                    value.floating==static_cast<float>(static_cast<double>(expected.fields.at(channel).integer)*(1.0/255.0)),
                    "Original SetTileColor did not normalize via the original binary64 reciprocal: "+view.path);
            }
            Require(plane.fields.at("w").kind==Kind::Float && plane.fields.at("w").floating==0.0f &&
                !std::signbit(plane.fields.at("w").floating),"Original SetTileColor Plane.W is not positive zero");
        };
        Value black;black.kind=Kind::Struct;
        black.fields={{"r",Value::Byte(0)},{"g",Value::Byte(0)},{"b",Value::Byte(0)},{"a",Value::Byte(0)}};
        Value white;white.kind=Kind::Struct;
        white.fields={{"r",Value::Byte(255)},{"g",Value::Byte(255)},{"b",Value::Byte(255)},{"a",Value::Byte(0)}};
        const auto healthWindow=[&](const PortableNativeWindowObjectSnapshot& view,const std::string& texture,
            float x,float y,float width,float height,const Value& color) {
            Require(view.classPath=="Extension.Window" && view.outerPath==hitView.path &&
                field(view,"Extension.Window.bIsVisible").value.boolean && field(view,"Extension.Window.lockCount").value.integer==0 &&
                view.rowMajorWindows.empty() && view.colMajorWindows.empty() && view.tabGroupWindows.empty(),
                "Original health Window did not complete its inherited visibility callback or retained foreign native tables");
            edge(view,"Extension.Window.winParent",hitView.path);edge(view,"Extension.Window.parentOwner",hitView.path);
            edge(view,"Extension.Window.Background","DeusExUI.UserInterface.HUDHitDisplay_"+texture);
            geometry(view,x,y,width,height);
            Require(field(view,"Extension.Window.backgroundStyle").value.integer==3,
                "Original health background style was not applied");
            const auto& ownGC=object(field(view,"Extension.Window.wGc").value.text);
            const auto& ownGeneralGC=object(field(view,"Extension.Window.gGc").value.text);
            Require(ownGC.path!=ownGeneralGC.path && ownGC.outerPath==view.path && ownGeneralGC.outerPath==view.path &&
                field(ownGC,"Extension.GC.Style").value.integer==3 && field(ownGC,"Extension.GC.PolyFlags").value.integer==0x904 &&
                field(ownGC,"Extension.GC.bDrawEnabled").value.boolean && field(ownGC,"Extension.GC.bTranslucent").value.boolean &&
                !field(ownGC,"Extension.GC.bMasked").value.boolean && !field(ownGC,"Extension.GC.bModulated").value.boolean &&
                !field(ownGC,"Extension.GC.bSmoothed").value.boolean && field(ownGC,"Extension.GC.textPolyFlags").value.integer==0x802 &&
                field(ownGeneralGC,"Extension.GC.Style").value.integer==1 && field(ownGeneralGC,"Extension.GC.PolyFlags").value.integer==0x900,
                "Original background-style setter lost its GC modes/flags or touched general GC/text/smoothing state");
            edge(view,"Extension.Window.wGc",ownGC.path);edge(view,"Extension.Window.gGc",ownGeneralGC.path);
            colorPlane(view,"Extension.Window",color);colorPlane(ownGC,"Extension.GC",color);
            colorPlane(ownGeneralGC,"Extension.GC",white);
        };
        std::vector<std::string> healthChildren;
        // Body is created after all six parts and Lower moves only sibling
        // order: it retains its actual owner, margins, texture and armor color.
        const auto& bodyView=object(field(hitView,"Extension.Window.firstChild").value.text);
        const auto armor=ReadPortableClassDefault("DeusEx.HUDHitDisplay","colArmor");
        healthWindow(bodyView,"Body",24.0f,15.0f,34.0f,68.0f,armor);
        healthChildren.push_back(bodyView.path);
        for(const auto& [part,texture,x,y,width,height]:{
            std::tuple{"Head","Head",39.0f,17.0f,4.0f,7.0f},
            std::tuple{"torso","Torso",36.0f,25.0f,10.0f,23.0f},
            std::tuple{"armLeft","ArmLeft",46.0f,27.0f,10.0f,23.0f},
            std::tuple{"armRight","ArmRight",26.0f,27.0f,10.0f,23.0f},
            std::tuple{"legLeft","LegLeft",41.0f,44.0f,8.0f,36.0f},
            std::tuple{"legRight","LegRight",33.0f,44.0f,8.0f,36.0f}}) {
            const auto& value=field(hitView,"DeusEx.HUDHitDisplay."+std::string(part)).value;
            Require(value.kind==Kind::Struct && value.fields.size()==7u &&
                value.fields.at("partwindow").kind==Kind::Object && !value.fields.at("partwindow").text.empty(),
                "Original CreateBodyPart did not assign its actual seven-field structure: "+std::string(part));
            for(const auto* name:{"displayedhealth","lasthealth","healhealth"})
                Require(value.fields.at(name).kind==Kind::Int && value.fields.at(name).integer==0,
                    "Original CreateBodyPart did not initialize its actual health counter: "+std::string(part));
            for(const auto* name:{"damagecounter","healcounter","refreshcounter"})
                Require(value.fields.at(name).kind==Kind::Float && value.fields.at(name).floating==0.0f,
                    "Original CreateBodyPart did not initialize its actual flash counter: "+std::string(part));
            const auto& partView=object(value.fields.at("partwindow").text);
            healthWindow(partView,texture,x,y,width,height,black);
            Require(std::find(healthChildren.begin(),healthChildren.end(),partView.path)==healthChildren.end(),
                "Original health parts alias the same Window");
            healthChildren.push_back(partView.path);
        }
        for(const auto& [name,x]:{std::pair{"winEnergy",15.0f},std::pair{"winBreath",61.0f}}) {
            const auto& bar=object(field(hitView,"DeusEx.HUDHitDisplay."+std::string(name)).value.text);
            Require(bar.classPath=="DeusEx.ProgressBarWindow" && bar.outerPath==hitView.path &&
                field(bar,"Extension.Window.bIsVisible").value.boolean && field(bar,"Extension.Window.lockCount").value.integer==0,
                "Original CreateProgressBar did not complete its actual child initialization");
            edge(bar,"Extension.Window.winParent",hitView.path);edge(bar,"Extension.Window.parentOwner",hitView.path);
            geometry(bar,x,20.0f,5.0f,55.0f);
            for(const auto& [property,expected]:{std::pair{"lowValue",0.0f},std::pair{"highValue",100.0f},std::pair{"CurrentValue",0.0f}}) {
                const auto& value=field(bar,"DeusEx.ProgressBarWindow."+std::string(property)).value;
                Require(value.kind==Kind::Float && value.floating==expected,
                    "Original progress bar clamp did not preserve its initialized range/current value");
            }
            Require(field(bar,"DeusEx.ProgressBarWindow.barSize").value.kind==Kind::Int &&
                field(bar,"DeusEx.ProgressBarWindow.barSize").value.integer==0 &&
                field(bar,"DeusEx.ProgressBarWindow.bVertical").value.boolean &&
                field(bar,"DeusEx.ProgressBarWindow.bUseScaledColor").value.boolean &&
                QuestVr::Vm::Equal(field(bar,"DeusEx.ProgressBarWindow.scaleColorModifier").value,
                    ReadPortableClassDefault("DeusEx.ProgressBarWindow","scaleColorModifier")) &&
                QuestVr::Vm::Equal(field(bar,"DeusEx.ProgressBarWindow.colForeground").value,black),
                "Original UpdateBars/SetVertical/UseScaledColor did not initialize the actual empty health bar");
            Require(std::find(healthChildren.begin(),healthChildren.end(),bar.path)==healthChildren.end(),
                "Original energy/breath bars alias another health Window");
            healthChildren.push_back(bar.path);
        }
        Require(healthChildren.size()==9u,"Original health panel did not retain exactly Body, six limbs and two bars");
        edge(hitView,"Extension.Window.firstChild",healthChildren.front());edge(hitView,"Extension.Window.lastChild",healthChildren.back());
        for(std::size_t index=0u;index<healthChildren.size();++index) {
            const auto& child=object(healthChildren[index]);
            edge(child,"Extension.Window.prevSibling",index==0u?std::string{}:healthChildren[index-1u]);
            edge(child,"Extension.Window.nextSibling",index+1u==healthChildren.size()?std::string{}:healthChildren[index+1u]);
        }
        for(const auto& [view,width,height,configured]:{
            std::tuple{&ammoView,95.0f,77.0f,false},std::tuple{&hitView,84.0f,106.0f,false},
            std::tuple{&beltView,541.0f,69.0f,true}})
            Require(field(*view,"Extension.Window.hardcodedWidth").value.floating==width &&
                field(*view,"Extension.Window.hardcodedHeight").value.floating==height &&
                field(*view,"Extension.Window.Width").value.floating==(configured?width:10.0f) &&
                field(*view,"Extension.Window.Height").value.floating==(configured?height:10.0f),
                "Original HUD requested/current geometry differs from the actual deferred or completed layout callback");
        for(const auto* view:{&rootView,&display}) {
            for(const auto* name:{"bDrawRawBackground","bSpecialText","bLastWidthSpecified","bLastHeightSpecified","bIsSensitive"})
                Require(field(*view,std::string("Extension.Window.")+name).value.boolean,"Native Window constructor missed a true original flag");
            for(const auto* name:{"bStretchBackground","bSmoothBackground","bConfigured","bIsSelectable","bBeingDestroyed"})
                Require(!field(*view,std::string("Extension.Window.")+name).value.boolean,"Native Window constructor missed a false original flag");
            for(const auto* name:{"rowMajorIndex","colMajorIndex"})
                Require(field(*view,std::string("Extension.Window.")+name).value.integer==-1,"Native Window list index is not -1");
            const auto& clip=field(*view,"Extension.Window.winClipRect").value;
            Require(clip.kind==Kind::Struct && clip.fields.size()==6u && clip.fields.at("originx").floating==0 &&
                clip.fields.at("originy").floating==0 && clip.fields.at("clipx").floating==0 && clip.fields.at("clipy").floating==0 &&
                clip.fields.at("clipwidth").floating==10 && clip.fields.at("clipheight").floating==10,
                "Native Window constructor did not establish its actual initial six-Float clip");
            for(const auto* name:{"bIsInitialized","bTextTranslucent","Background","SoundVolume","acceleratorKey","maxClicks","freeTimer",
                "holdX","holdY","holdWidth","holdHeight"}) {
                const auto original=ReadPortableClassDefault(view->classPath,name);
                Require(QuestVr::Vm::Equal(field(*view,std::string("Extension.Window.")+name).value,original),
                    "Native Window constructor overwrote an original preserved CDO field: "+std::string(name));
            }
        }
        const auto& rawColor=field(rootView,"Extension.RootWindow.rawColor").value;
        Require(rawColor.kind==Kind::Struct && rawColor.fields.size()==4u && rawColor.fields.at("r").integer==0 &&
            rawColor.fields.at("g").integer==0 && rawColor.fields.at("b").integer==0 && rawColor.fields.at("a").integer==0,
            "Native Root background color is not retail black RGB/defined portable alpha0");
        Require(field(display,"DeusEx.ActorDisplayWindow.maxPoints").value.integer==8,
            "Original ActorDisplayWindow.InitWindow did not execute its own maximum-point assignment");
        for(std::uint32_t slot=0u;slot<16u;++slot) {
            const auto& sine=field(display,"DeusEx.ActorDisplayWindow.sinTable",slot).value;
            const float angle=(2.0f*3.14159265358979323846f)*(static_cast<float>(slot)/8.0f);
            Require(sine.kind==Kind::Float && std::abs(sine.floating-static_cast<float>(std::sin(static_cast<double>(angle))))<0.000001f,
                "Original ActorDisplayWindow.InitWindow did not calculate all16 sine entries");
        }
        Require(ReadPortableRuntimeNativeWindowSummary().objects==0u &&
            Declared(player,"Extension.PlayerPawnExt.RootWindow",Kind::Object).text.empty() &&
            GC::GetStats().numObjects==windowStats.numObjects && GC::GetStats().memoryUsage==windowStats.memoryUsage &&
            SavePortableRuntimeState(inspectPath.string()) && Bytes(inspectPath)==baseline,
            "Original root prefix probe published its pending root/GC state");
        // A separate genuine base-Root/TextLog graph exercises the native
        // ownership and setter contracts without resuming the failed HUD root.
        // Explicit fixture lines are not AddLog, Tick, Draw or usable gameplay.
        const auto textLogStats=GC::GetStats();
        const auto textLogRollback=[&] {
            const auto summary=ReadPortableRuntimeNativeWindowSummary();
            Require(summary.objects==0u && summary.roots==0u &&
                summary.configuredRootNameCounter==windowBaseline.configuredRootNameCounter &&
                summary.graphicsContextNameCounter==windowBaseline.graphicsContextNameCounter &&
                Declared(player,"Extension.PlayerPawnExt.RootWindow",Kind::Object).text.empty() &&
                GetPortableRuntimeWorldRevision()==windowRevision && GetPortableRuntimeMapActors(true).size()==count &&
                GC::GetStats().numObjects==textLogStats.numObjects && GC::GetStats().memoryUsage==textLogStats.memoryUsage &&
                SavePortableRuntimeState(inspectPath.string()) && Bytes(inspectPath)==baseline,
                "TextLog rejection retained native strings/arrays, roots, names, GC, player/map state or save bytes");
        };
        const auto verifyTextLog=[&](const PortableTextLogProbeResult& result) {
            Require(!result.rootPath.empty() && !result.logPath.empty() && result.rootPath!=result.logPath &&
                result.constructorStorageObserved && result.constructorLinesInitialized &&
                result.constructorLineCount==0u && result.constructorLineCapacity==0u,
                "TextLog native Lines were not observed constructed-empty before inherited Init callbacks");
            const std::vector<std::string> stages{"constructed","fixture-populated","reinitialized","aligned",
                "equal-alignment","margins","equal-margins","nan-margins","negative-lines","clamped-lines",
                "equal-lines","no-result","color-initial","color-changed","equal-color","color-none"};
            Require(result.stages.size()==stages.size(),"TextLog diagnostic omitted an actual lifecycle/setter observation");
            for(std::size_t index=0u;index<stages.size();++index) {
                const auto& stage=result.stages[index];
                Require(stage.name==stages[index] && stage.linesInitialized,
                    "TextLog native lifecycle/setter order or constructed-state differs");
                if(index) Require(stage.writes>=result.stages[index-1u].writes,
                    "TextLog native callback/setter reset the caller write ledger");
                if(index!=1u) Require(stage.lines.empty() && stage.lineCapacity==0u,
                    "TextLog Init suffix retained a fixture FString or native array capacity");
                if(index>=2u) Require(stage.paused && !stage.tooTall && stage.timeout==5.0f,
                    "TextLog Init/setter lost preserved pause state, too-tall clearing or timeout");
            }
            const auto& constructed=result.stages[0u];const auto& populated=result.stages[1u];
            const auto& reinitialized=result.stages[2u];const auto& aligned=result.stages[3u];
            const auto& equalAlignment=result.stages[4u];const auto& margins=result.stages[5u];
            const auto& equalMargins=result.stages[6u];const auto& nanMargins=result.stages[7u];
            const auto& negativeLines=result.stages[8u];const auto& clampedLines=result.stages[9u];
            const auto& equalLines=result.stages[10u];const auto& noResult=result.stages[11u];
            Require(constructed.hAlign==0u && constructed.vAlign==0u && constructed.timeout==5.0f &&
                !constructed.tooTall && constructed.paused==pausedDefault.boolean &&
                constructed.hMargin==3.0f && constructed.vMargin==3.0f &&
                constructed.minLines==-1 && constructed.maxLines==-1,
                "TextLog constructor/Init replaced actual inherited defaults or copied CDO pause state");
            Require(populated.paused && populated.tooTall && populated.lines.size()==1u && populated.lineCapacity>=37u,
                "TextLog explicit ownership fixture did not retain its populated flags/native capacity");
            const auto& line=populated.lines.front();
            Require(line.expiry==0.0f && std::signbit(line.expiry) && line.text==std::string("owned\0log",9u) &&
                line.color==std::array<std::uint8_t,4u>{7u,11u,19u,0u},
                "Native TextLog detached line lost raw expiry, owning embedded-NUL text or all four color bytes");
            Require(reinitialized.hAlign==0u && reinitialized.vAlign==0u &&
                reinitialized.writes>populated.writes,
                "TextLog genuine re-Init did not run its scalar/freeing suffix in the same root");
            Require(!reinitialized.needsQuery && !reinitialized.needsReconfigure &&
                !aligned.needsQuery && !aligned.needsReconfigure &&
                !equalAlignment.needsQuery && !equalAlignment.needsReconfigure,
                "Native1556 fabricated a layout request after the explicit clean-flag fixture");
            Require(aligned.hAlign==255u && aligned.vAlign==254u && aligned.writes==reinitialized.writes+2u &&
                equalAlignment.hAlign==255u && equalAlignment.vAlign==254u &&
                equalAlignment.writes==aligned.writes+2u,
                "Native1556 clamped full-range bytes or fabricated an equality no-op");
            Require(margins.hMargin==1.25f && margins.vMargin==-2.5f && margins.writes==equalAlignment.writes+4u &&
                margins.needsQuery && margins.needsReconfigure &&
                equalMargins.hMargin==margins.hMargin && equalMargins.vMargin==margins.vMargin &&
                equalMargins.writes==margins.writes && equalMargins.needsQuery && equalMargins.needsReconfigure &&
                nanMargins.hMargin==margins.hMargin && nanMargins.vMargin==margins.vMargin &&
                nanMargins.writes==margins.writes && nanMargins.needsQuery && nanMargins.needsReconfigure,
                "Native1557 lost raw margins or changed storage/callback state on equal/unordered x87 comparisons");
            Require(negativeLines.minLines==0 && negativeLines.maxLines==0 && negativeLines.writes==nanMargins.writes+4u &&
                negativeLines.needsQuery && negativeLines.needsReconfigure &&
                clampedLines.minLines==3 && clampedLines.maxLines==3 && clampedLines.writes==negativeLines.writes+4u &&
                clampedLines.needsQuery && clampedLines.needsReconfigure &&
                equalLines.minLines==3 && equalLines.maxLines==3 && equalLines.writes==clampedLines.writes &&
                equalLines.needsQuery && equalLines.needsReconfigure,
                "Native1558 lost signed nonnegative/min<=max clamping or effective-value equality no-op");
            Require(noResult.hAlign==0u && noResult.vAlign==0u && noResult.writes==equalLines.writes+2u,
                "Native1556 no-result operands did not preserve both original zero-byte seeds");
            const auto& colorInitial=result.stages[12u];const auto& colorChanged=result.stages[13u];
            const auto& equalColor=result.stages[14u];const auto& colorNone=result.stages[15u];
            Require(result.colorCasesVerified==256u && colorInitial.writes==noResult.writes,
                "Native1476 diagnostic omitted256 channel cases or changed storage before its first color call");
            Require(result.scalarCasesVerified==20u,
                "Native1572/1573 diagnostic omitted raw Float, duplicate-store, preserved flag or no-result cases");
            const auto verifyColor=[&](const Value& color,const Value& plane,
                const std::array<std::uint8_t,4u>& bytes) {
                Require(color.kind==Kind::Struct && color.fields.size()==4u &&
                    plane.kind==Kind::Struct && plane.fields.size()==4u,
                    "Native1476 lost the canonical four-Byte Color/four-Float Plane shape");
                const std::array<const char*,4u> channels{"r","g","b","a"};
                for(std::size_t index=0u;index<channels.size();++index) {
                    const auto& channel=color.fields.at(channels[index]);
                    Require(channel.kind==Kind::Byte && channel.integer==bytes[index],
                        "Native1476 changed a raw Color byte, including alpha");
                }
                const std::array<const char*,3u> axes{"x","y","z"};
                for(std::size_t index=0u;index<axes.size();++index) {
                    const auto& component=plane.fields.at(axes[index]);
                    Require(component.kind==Kind::Float &&
                        component.floating==static_cast<float>(static_cast<double>(bytes[index])*(1.0/255.0)),
                        "Native1476 did not use the original binary64 reciprocal before Float rounding");
                }
                const auto& w=plane.fields.at("w");
                Require(w.kind==Kind::Float && w.floating==0.0f && !std::signbit(w.floating),
                    "Native1476 plane W lost its positive-zero storage");
            };
            for(const auto* stage:{&colorChanged,&equalColor}) {
                verifyColor(stage->textColor,stage->textPlane,{17u,101u,239u,7u});
                verifyColor(stage->windowGCTextColor,stage->windowGCTextPlane,{17u,101u,239u,7u});
                Require(QuestVr::Vm::Equal(stage->textColor,stage->windowGCTextColor) &&
                    QuestVr::Vm::Equal(stage->textPlane,stage->windowGCTextPlane),
                    "Native1476 did not update the actual own drawing GC coherently");
            }
            Require(colorChanged.writes==colorInitial.writes+4u && equalColor.writes==colorChanged.writes+4u,
                "Native1476 omitted original Color/Plane stores or fabricated an equal-color shortcut");
            verifyColor(colorNone.textColor,colorNone.textPlane,{255u,0u,128u,255u});
            Require(result.colorNoneFixtureWrites==1u &&
                colorNone.writes==equalColor.writes+result.colorNoneFixtureWrites+2u &&
                QuestVr::Vm::Equal(colorNone.windowGCTextColor,equalColor.windowGCTextColor) &&
                QuestVr::Vm::Equal(colorNone.windowGCTextPlane,equalColor.windowGCTextPlane),
                "Native1476 wGc=None branch mutated the detached GC or skipped either actual Window store");
            for(const auto* stage:{&colorInitial,&colorChanged,&equalColor,&colorNone}) {
                Require(QuestVr::Vm::Equal(stage->generalGCTextColor,colorInitial.generalGCTextColor) &&
                    QuestVr::Vm::Equal(stage->generalGCTextPlane,colorInitial.generalGCTextPlane),
                    "Native1476 incorrectly propagated text color into the independent general GC");
                Require(stage->needsQuery==noResult.needsQuery && stage->needsReconfigure==noResult.needsReconfigure &&
                    stage->hAlign==noResult.hAlign && stage->vAlign==noResult.vAlign &&
                    stage->hMargin==noResult.hMargin && stage->vMargin==noResult.vMargin &&
                    stage->minLines==noResult.minLines && stage->maxLines==noResult.maxLines,
                    "Native1476 fabricated layout requests or changed unrelated text sizing/alignment storage");
            }
        };
        const auto runTextLog=[&](const PortableTextLogProbeMode mode) {
            auto result=ProbePortableNativeTextLogWindow(player,mode);
            Require(!result.execution.passed() && !result.execution.committed &&
                result.execution.error=="Native TextLog diagnostic complete; rollback required" &&
                result.execution.nativeWork && result.execution.nativeWork->used>0u &&
                !result.execution.nativeWork->exhausted,
                "TextLog diagnostic failed before its exact final rollback: "+result.execution.function+':'+
                std::to_string(result.execution.offset)+" "+result.execution.error);
            verifyTextLog(result);textLogRollback();return result;
        };
        const auto textLogCold=runTextLog(PortableTextLogProbeMode::RoundTrip);
        const auto textLogWarm=runTextLog(PortableTextLogProbeMode::RoundTrip);
        Require(textLogCold.rootPath==textLogWarm.rootPath && textLogCold.logPath==textLogWarm.logPath &&
            textLogCold.execution.writes==textLogWarm.execution.writes,
            "TextLog rollback retained native root/child names or changed the repeated setter write sequence");
        for(const auto& [mode,error]:{
            std::pair{PortableTextLogProbeMode::PrivateRead,
                "Native TextLog Lines is owned native storage, not a script-visible DynamicArray"},
            std::pair{PortableTextLogProbeMode::PrivateWrite,
                "Native TextLog Lines mutation requires its typed journaled storage service"}}) {
            const auto invalid=ProbePortableNativeTextLogWindow(player,mode);
            Require(!invalid.execution.passed() && !invalid.execution.committed && invalid.execution.error==error,
                "TextLog exposed its private native Lines through a fabricated script DynamicArray: "+invalid.execution.error);
            textLogRollback();
        }
        for(const auto& [mode,error]:{
            std::pair{PortableTextLogProbeMode::WrongReceiver,
                "TextWindow setter requires its actual constructed native superclass"},
            std::pair{PortableTextLogProbeMode::WrongArgument,
                "TextWindow setter argument changed its original scalar type"},
            std::pair{PortableTextLogProbeMode::MissingArgument,
                "Missing required VM parameter newVAlign"},
            std::pair{PortableTextLogProbeMode::WrongColor,
                "VM CoreColor4 destination has malformed Byte channels"},
            std::pair{PortableTextLogProbeMode::NoColorResult,
                "VM uninitialized native argument requires a complete result write"}}) {
            const auto invalid=ProbePortableNativeTextLogWindow(player,mode);
            Require(!invalid.execution.passed() && !invalid.execution.committed && invalid.execution.error==error,
                "Window/text setter ingress did not refuse at its exact receiver/type/arity/result guard: "+invalid.execution.error);
            textLogRollback();
        }
        const auto textLogLate=ProbePortableNativeTextLogWindow(player,PortableTextLogProbeMode::LateFailure);
        Require(!textLogLate.execution.passed() && !textLogLate.execution.committed &&
            textLogLate.execution.error=="Native TextLog diagnostic forced late failure",
            "TextLog late failure did not occur after the authentic ownership/setter sequence");
        verifyTextLog(textLogLate);textLogRollback();
        for(const auto prefix:std::vector<std::size_t>{0u,1u,textLogWarm.stages[1u].writes+1u,
            textLogWarm.stages[2u].writes+1u,textLogWarm.stages[4u].writes-1u,
            textLogWarm.stages[13u].writes-1u,textLogWarm.stages[14u].writes-1u,textLogWarm.execution.writes-1u}) {
            QuestVr::Vm::Limits bounded;bounded.writes=prefix;
            const auto partial=ProbePortableNativeTextLogWindow(player,PortableTextLogProbeMode::RoundTrip,bounded);
            Require(partial.execution.status==QuestVr::Vm::Status::Budget && !partial.execution.committed,
                "TextLog constructor/freeing/setter reset the caller write budget: "+std::to_string(prefix)+" "+
                partial.execution.error);
            textLogRollback();
        }
        for(const auto prefix:{std::size_t{0u},textLogWarm.execution.nativeWork->used/2u,
            textLogWarm.execution.nativeWork->used-1u}) {
            QuestVr::Vm::Limits bounded;bounded.nativeWork=prefix;
            const auto partial=ProbePortableNativeTextLogWindow(player,PortableTextLogProbeMode::RoundTrip,bounded);
            Require(!partial.execution.passed() && !partial.execution.committed && partial.execution.nativeWork &&
                partial.execution.nativeWork->exhausted,
                "TextLog lifecycle/setters reset or ignored the shared native-work budget: "+std::to_string(prefix));
            textLogRollback();
        }
        QuestVr::Vm::Limits noTextLogStorage;noTextLogStorage.retainedBytes=0u;
        const auto textLogStorageRefused=ProbePortableNativeTextLogWindow(player,
            PortableTextLogProbeMode::RoundTrip,noTextLogStorage);
        Require(textLogStorageRefused.execution.status==QuestVr::Vm::Status::Budget &&
            !textLogStorageRefused.execution.committed,"TextLog constructor ignored the caller retained-byte budget");
        textLogRollback();
        std::cout<<"ORIGINAL TextLog native controls: constructor-before-callback storage, owned populated Lines freeing, "
            "pause preservation, raw byte/margin/line setters, all256 native1476 colors/planes/own-GC stores, "
            "private-array refusal and whole-root rollback\n";
        // These controls construct a separate actual native Root/Radio/Toggle
        // graph in one rollback-only VM transaction. They NEVER resume the
        // poisoned HUD layout callback above, patch owned scripts, or establish
        // successful original HUD startup. Distinct explicit test-only Info
        // colors make the real native appearance choice observable.
        const auto toggleStats=GC::GetStats();
        const auto toggleRollback=[&] {
            const auto summary=ReadPortableRuntimeNativeWindowSummary();
            Require(summary.objects==0u && summary.roots==0u &&
                summary.configuredRootNameCounter==windowBaseline.configuredRootNameCounter &&
                summary.graphicsContextNameCounter==windowBaseline.graphicsContextNameCounter &&
                Declared(player,"Extension.PlayerPawnExt.RootWindow",Kind::Object).text.empty() &&
                GetPortableRuntimeWorldRevision()==windowRevision && GetPortableRuntimeMapActors(true).size()==count &&
                GC::GetStats().numObjects==toggleStats.numObjects && GC::GetStats().memoryUsage==toggleStats.memoryUsage &&
                SavePortableRuntimeState(inspectPath.string()) && Bytes(inspectPath)==baseline,
                "Toggle callback/budget refusal retained roots, lists, names, GC, Actor/player state or checkpoint bytes");
        };
        const auto toggleRun=[&](PortableToggleProbeMode mode) {
            auto result=ProbePortableNativeWindowToggle(player,mode);
            Require(!result.execution.passed() && !result.execution.committed &&
                result.execution.error=="Native toggle diagnostic complete; rollback required",
                "Original Toggle diagnostic failed before its genuine final rollback: "+
                result.execution.function+':'+std::to_string(result.execution.offset)+" "+result.execution.error);
            Require(!result.rootPath.empty() && !result.radioPath.empty() && !result.firstPath.empty() &&
                !result.secondPath.empty() && result.rootPath!=result.radioPath && result.firstPath!=result.secondPath &&
                result.registeredToggles==std::vector<std::string>{result.firstPath,result.secondPath},
                "Original Toggle control lacks actual distinct windows and native Radio creation-order registration");
            Require(result.execution.nativeWork && result.execution.nativeWork->used>0u &&
                !result.execution.nativeWork->exhausted,"Original Toggle calls lost the caller's native-work ledger");
            Require(result.forwardNativeWork>0u && result.forwardNativeWork<=result.execution.nativeWork->used,
                "Original Toggle diagnostic lost its pre-rollback forward-work observation");
            toggleRollback();return result;
        };
        const auto toggleNames=[&](const PortableToggleProbeResult& result,
            const std::vector<std::string>& expected) {
            Require(result.stages.size()==expected.size(),"Original Toggle control omitted actual call observations");
            for(std::size_t index=0u;index<expected.size();++index) {
                Require(result.stages[index].name==expected[index],"Original Toggle callback order differs");
                if(index) Require(result.stages[index].writes>=result.stages[index-1u].writes,
                    "Original Toggle nested call reset the actual shared VM write ledger");
            }
        };
        const auto toggleRoundTrip=[&](const PortableToggleProbeResult& result,bool oneCheck) {
            toggleNames(result,{"initial","equal-false","first-on","equal-true","second-on","second-off"});
            const auto& initial=result.stages[0u];const auto& equalFalse=result.stages[1u];
            const auto& first=result.stages[2u];const auto& equalTrue=result.stages[3u];
            const auto& replacement=result.stages[4u];const auto& last=result.stages[5u];
            Require(!initial.firstPressed && !initial.secondPressed && initial.selection.empty(),
                "Native Radio/Toggle constructor fabricated an initial selection");
            Require(!equalFalse.firstPressed && !equalFalse.secondPressed && equalFalse.selection.empty() &&
                equalFalse.writes==initial.writes && SameToggleAppearance(equalFalse.firstAppearance,initial.firstAppearance) &&
                SameToggleAppearance(equalFalse.secondAppearance,initial.secondAppearance),
                "Equal-false SetToggle performed a write, appearance refresh or Radio selection");
            Require(first.firstPressed && !first.secondPressed && first.selection==result.firstPath &&
                first.writes>equalFalse.writes,"Native Toggle true did not select its actual registered source");
            Require(equalTrue.firstPressed && !equalTrue.secondPressed && equalTrue.selection==result.firstPath &&
                equalTrue.writes==first.writes && SameToggleAppearance(equalTrue.firstAppearance,first.firstAppearance) &&
                SameToggleAppearance(equalTrue.secondAppearance,first.secondAppearance),
                "Equal-true SetToggle performed a write or repeated its notification");
            Require(!replacement.firstPressed && replacement.secondPressed && replacement.selection==result.secondPath &&
                replacement.writes>equalTrue.writes,
                "Native Radio replacement did not reentrantly reset the former Toggle in the caller root");
            Require(!last.firstPressed && last.secondPressed==oneCheck &&
                last.selection==(oneCheck?result.secondPath:std::string{}) && last.writes>replacement.writes,
                "Original Radio false branch did not preserve one-check or clear optional selection");
            for(const auto& stage:result.stages) {
                Require(stage.oneCheck==oneCheck,"SetToggle changed native Radio one-check policy");
                VerifyToggleAppearance(stage.firstAppearance,stage.firstPressed);
                VerifyToggleAppearance(stage.secondAppearance,stage.secondPressed);
            }
        };
        const auto toggleCold=toggleRun(PortableToggleProbeMode::RoundTrip);
        toggleRoundTrip(toggleCold,false);
        const auto toggleWarm=toggleRun(PortableToggleProbeMode::RoundTrip);
        toggleRoundTrip(toggleWarm,false);
        const auto printToggleWork=[&](const char* label,const PortableToggleProbeResult& result) {
            std::cout<<"ORIGINAL Toggle native work "<<label<<": forward="<<result.forwardNativeWork<<
                " total="<<(result.execution.nativeWork?std::to_string(result.execution.nativeWork->used):"absent")<<
                " limit="<<(result.execution.nativeWork?std::to_string(result.execution.nativeWork->limit):"absent")<<
                " exhausted="<<(result.execution.nativeWork?(result.execution.nativeWork->exhausted?"true":"false"):"absent")<<
                " status="<<static_cast<unsigned>(result.execution.status)<<" writes="<<result.execution.writes<<
                " stages="<<result.stages.size()<<" error="<<result.execution.error<<std::endl;
        };
        printToggleWork("cold",toggleCold);printToggleWork("warm",toggleWarm);
        Require(toggleCold.rootPath==toggleWarm.rootPath && toggleCold.radioPath==toggleWarm.radioPath &&
            toggleCold.firstPath==toggleWarm.firstPath && toggleCold.secondPath==toggleWarm.secondPath &&
            toggleCold.registeredToggles==toggleWarm.registeredToggles,
            "Rolled-back Toggle diagnostic retained a root/child name or native registration");
        toggleRoundTrip(toggleRun(PortableToggleProbeMode::OneCheck),true);
        const auto emptyToggle=toggleRun(PortableToggleProbeMode::EmptySlot);
        toggleNames(emptyToggle,{"initial","equal-false","rejected-empty"});
        Require(emptyToggle.stages[0u].writes==emptyToggle.stages[1u].writes &&
            emptyToggle.stages[2u].writes==emptyToggle.stages[1u].writes+8u,
            "Actual HUDObjectSlot empty rejection did not share both four-store native SetToggle calls");
        for(const auto& stage:emptyToggle.stages) {
            Require(!stage.firstPressed && !stage.secondPressed && stage.selection.empty(),
                "Original HUDObjectSlot.ToggleChanged failed to reject an empty selected slot");
            VerifyToggleAppearance(stage.firstAppearance,false);
            VerifyToggleAppearance(stage.secondAppearance,false);
        }
        const auto noResultToggle=toggleRun(PortableToggleProbeMode::NoResult);
        toggleNames(noResultToggle,{"initial","first-on","no-result"});
        Require(noResultToggle.stages[1u].firstPressed &&
            noResultToggle.stages[1u].selection==noResultToggle.firstPath &&
            !noResultToggle.stages[2u].firstPressed && !noResultToggle.stages[2u].secondPressed &&
            noResultToggle.stages[2u].selection.empty() &&
            noResultToggle.stages[2u].writes>noResultToggle.stages[1u].writes,
            "Original native1611 thunk did not use its initialized-false seed for a no-result argument");
        VerifyToggleAppearance(noResultToggle.stages[2u].firstAppearance,false);
        for(const auto mode:{PortableToggleProbeMode::WrongReceiver,PortableToggleProbeMode::WrongArgument}) {
            const auto invalid=ProbePortableNativeWindowToggle(player,mode);
            Require(!invalid.execution.passed() && !invalid.execution.committed &&
                !invalid.execution.error.empty() &&
                invalid.execution.error!="Native toggle diagnostic complete; rollback required",
                "Native1611 accepted a non-Toggle receiver or non-Bool argument");
            toggleRollback();
        }
        // The last two prefixes fail AFTER successful earlier toggles. Their
        // whole root must still undo the old-selection reset, appearance,
        // selection pointer, native list and newly constructed identities.
        for(const auto prefix:std::vector<std::size_t>{0u,1u,toggleWarm.stages[2u].writes+1u,
            toggleWarm.stages[4u].writes+1u,toggleWarm.execution.writes-1u}) {
            QuestVr::Vm::Limits bounded;bounded.writes=prefix;
            const auto partial=ProbePortableNativeWindowToggle(player,PortableToggleProbeMode::RoundTrip,bounded);
            Require(partial.execution.status==QuestVr::Vm::Status::Budget && !partial.execution.committed,
                "Native Toggle/Radio reset the caller write budget: "+std::to_string(prefix)+" "+partial.execution.error);
            toggleRollback();
        }
        const auto toggleFresh=toggleRun(PortableToggleProbeMode::RoundTrip);
        toggleRoundTrip(toggleFresh,false);
        printToggleWork("fresh",toggleFresh);
        // Result.nativeWork also includes undo Reference::write calls. Cut
        // genuine forward execution, not the diagnostic's cleanup high water.
        for(const auto prefix:{std::size_t{0u},toggleFresh.forwardNativeWork/2u,
            toggleFresh.forwardNativeWork-1u}) {
            QuestVr::Vm::Limits bounded;bounded.nativeWork=prefix;
            const auto partial=ProbePortableNativeWindowToggle(player,PortableToggleProbeMode::RoundTrip,bounded);
            printToggleWork(("prefix-"+std::to_string(prefix)).c_str(),partial);
            Require(!partial.execution.passed() && !partial.execution.committed && partial.execution.nativeWork &&
                partial.execution.nativeWork->exhausted,
                "Native Toggle notifications reset/ignored shared constructor/callback work: prefix="+std::to_string(prefix)+
                " status="+std::to_string(static_cast<unsigned>(partial.execution.status))+
                " used="+(partial.execution.nativeWork?std::to_string(partial.execution.nativeWork->used):"absent")+
                " limit="+(partial.execution.nativeWork?std::to_string(partial.execution.nativeWork->limit):"absent")+
                " exhausted="+(partial.execution.nativeWork?(partial.execution.nativeWork->exhausted?"true":"false"):"absent")+
                " forward="+std::to_string(partial.forwardNativeWork)+" fullForward="+std::to_string(toggleFresh.forwardNativeWork)+
                " writes="+std::to_string(partial.execution.writes)+" stages="+std::to_string(partial.stages.size())+
                " at "+partial.execution.function+':'+std::to_string(partial.execution.offset)+" "+partial.execution.error);
            toggleRollback();
        }
        std::cout<<"ORIGINAL native1611 controls: equality no-op, typed appearance, registered Radio replacement/one-check, "
            "reentrant empty HUD slot, initialized-false no-result, malformed ingress and late whole-root rollback\n";
        const auto activation=ProbePortableRootWindowActivation(player);
        const auto& rootAttempt=activation.execution;
        std::cout<<"ORIGINAL unbound root initialization boundary: "<<rootAttempt.function<<':'<<rootAttempt.offset<<
            " opcode="<<unsigned(rootAttempt.opcode)<<" writes="<<rootAttempt.writes<<" "<<rootAttempt.error<<std::endl;
        for(const auto& frame:rootAttempt.callStack)std::cout<<"ORIGINAL root boundary active frame: "<<frame<<'\n';
        Require(!rootAttempt.passed() && !rootAttempt.committed && rootAttempt.function==pending.execution.function &&
            rootAttempt.offset==pending.execution.offset && rootAttempt.opcode==pending.execution.opcode && rootAttempt.writes==pending.execution.writes &&
            rootAttempt.error==pending.execution.error && rootAttempt.callStack==pending.execution.callStack,
            "Native1052 did not complete original script InitWindow AND native activation before diagnostic rollback");
        Require(activation.inspectionError.empty() && activation.pawnRootPath==pending.pawnRootPath &&
            activation.rootedWindows==pending.rootedWindows && activation.objects.size()==198u,
            "Native activation did not retain its actual complete rooted graph for inspection");
        const auto activatedObject=[&](const std::string& path)->const PortableNativeWindowObjectSnapshot& {
            const auto found=std::find_if(activation.objects.begin(),activation.objects.end(),
                [&](const auto& view){return view.path==path;});
            Require(found!=activation.objects.end(),"Activated root references an uncaptured Window");return *found;
        };
        const auto activatedField=[&](const PortableNativeWindowObjectSnapshot& view,const char* name)->const Value& {
            const auto found=std::find_if(view.fields.begin(),view.fields.end(),
                [&](const auto& entry){return entry.declaration==name && entry.arrayIndex==0u;});
            Require(found!=view.fields.end(),"Activated Window lacks its original captured field");return found->value;
        };
        std::size_t visibleUninitialized{},windowCount{};
        for(const auto& view:activation.objects) {
            const auto flag=std::find_if(view.fields.begin(),view.fields.end(),[](const auto& entry) {
                return entry.declaration=="Extension.Window.bIsVisible";
            });
            if(flag==view.fields.end()) continue; // Actual GC objects are not Windows.
            ++windowCount;
            const auto& initialized=activatedField(view,"Extension.Window.bIsInitialized");
            Require(initialized.kind==Kind::Bool && !initialized.boolean,
                "Activation fabricated WindowReady or an initialized-bit store before PreRender");
            bool visible=true;const auto* current=&view;std::size_t depth{};
            while(current) {
                Require(++depth<=activation.objects.size(),"Activated visibility ancestry cycles");
                const auto& local=activatedField(*current,"Extension.Window.bIsVisible");
                Require(local.kind==Kind::Bool,"Activated visibility is not typed Bool storage");
                if(!local.boolean) {visible=false;break;}
                const auto& parent=activatedField(*current,"Extension.Window.winParent");
                Require(parent.kind==Kind::Object,"Activated parent is not typed Object storage");
                current=parent.text.empty()?nullptr:&activatedObject(parent.text);
            }
            if(visible) ++visibleUninitialized;
            const auto& lock=activatedField(view,"Extension.Window.lockCount");
            Require(lock.kind==Kind::Int && lock.integer==0,"Activation retained a native configuration lock");
        }
        const auto& activatedRoot=activatedObject(activation.pawnRootPath);
        Require(windowCount==65u && visibleUninitialized>0u &&
            activatedField(activatedRoot,"Extension.Window.bIsVisible").boolean &&
            !activatedField(activatedRoot,"Extension.Window.bNeedsReconfigure").boolean &&
            activatedField(activatedRoot,"Extension.RootWindow.initCount").kind==Kind::Int &&
            activatedField(activatedRoot,"Extension.RootWindow.initCount").integer==static_cast<std::int32_t>(visibleUninitialized) &&
            activatedField(activatedRoot,"Extension.ModalWindow.bDirtyAccelerators").boolean,
            "Activated root visibility/layout/init count/modal dirtiness differs from its actual visible tree");
        std::cout<<"ORIGINAL native activation: windows="<<windowCount<<" visible-uninitialized="<<visibleUninitialized<<
            " initCount="<<activatedField(activatedRoot,"Extension.RootWindow.initCount").integer<<
            "; every initialized bit false until actual PreRender; every configuration lock released\n";
        const auto windowsAfter=ReadPortableRuntimeNativeWindowSummary();
        Require(windowsAfter.objects==0u && windowsAfter.roots==0u &&
            windowsAfter.configuredRootNameCounter==windowBaseline.configuredRootNameCounter &&
            windowsAfter.graphicsContextNameCounter==windowBaseline.graphicsContextNameCounter &&
            Declared(player,"Extension.PlayerPawnExt.RootWindow",Kind::Object).text.empty() &&
            GC::GetStats().numObjects==windowStats.numObjects && GC::GetStats().memoryUsage==windowStats.memoryUsage &&
            GetPortableRuntimeWorldRevision()==windowRevision && GetPortableRuntimeMapActors(true).size()==count &&
            SavePortableRuntimeState(inspectPath.string()) && Bytes(inspectPath)==baseline,
            "Root callback failure retained native roots/GC objects/names/player edges or map/save changes");
        const auto repeatedRoot=ProbePortableRootWindowActivation(player).execution;
        Require(repeatedRoot.status==rootAttempt.status && repeatedRoot.function==rootAttempt.function &&
            repeatedRoot.offset==rootAttempt.offset && repeatedRoot.opcode==rootAttempt.opcode &&
            repeatedRoot.error==rootAttempt.error && repeatedRoot.callStack==rootAttempt.callStack,
            "Original root callback did not reproduce its exact first remaining gap after rollback");
        Require(pending.execution.writes>650u,"Expanded root prefix did not reach its original later write sequence");
        for(const auto writeLimit:std::vector<std::size_t>{1u,5u,25u,100u,400u,650u,pending.execution.writes-1u}) {
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
        VerifyNativeWindowCoalescing(baseline,player,directory,pending.execution.writes,pending.objects.size());
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
        Require(InitializePortableRuntime(scripts).passed,"Cold original player runtime initialization failed");
        prepareServices();
        Require(LoadPortableRuntimeMap(map).passed &&
            LoadPortableRuntimeState(ringPath.string()),"Cold original ring graph restore failed");
        Require(!HasPortableRuntimeClient(),"Cold actor checkpoint fabricated a native client/session");
        VerifyRing(player,ring);
        Require(SavePortableRuntimeState(inspectPath.string()) && Bytes(inspectPath)==ringBytes,
            "Cold original ring graph did not restore byte-identically");
        ShutdownPortableRuntime();
        Require(InitializePortableRuntime(scripts).passed,"Compass map-replacement runtime failed");
        // Only first Training is in scope. A genuine unload/reload invalidates
        // the native map generation without sweeping another campaign map.
        Require(LoadPortableRuntimeMap(map).passed,"Compass Training reload failed");
        const auto compassGeneration=ReadPortableRuntimeNativeModel().mapGeneration;
        Require(compassGeneration!=0u && UnloadPortableRuntimeMap()!=0u && LoadPortableRuntimeMap(map).passed &&
            ReadPortableRuntimeNativeModel().mapGeneration!=compassGeneration,
            "Compass Training reload retained the retired native map generation");
        const auto replacementNorth=ReadPortableRuntimeMapNorth();
        Require(replacementNorth.found && replacementNorth.actorPath==north.actorPath &&
            replacementNorth.trueNorth==north.trueNorth &&
            Declared(replacementNorth.actorPath,"DeusEx.DeusExLevelInfo.TrueNorth",Kind::Int).integer==replacementNorth.trueNorth,
            "Compass lost the actual first Training identity/north after native map replacement");
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
