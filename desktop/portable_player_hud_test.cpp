#include "Precomp.h"
#include "GC/GC.h"
#include "portable_unreal_runtime.h"
#include "portable_model_geometry.h"
#include "quest_script_state.h"
#include "quest_save_bundle.h"
#include <algorithm>
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
        std::cout<<"ORIGINAL root pending graph: "<<pending.execution.function<<':'<<pending.execution.offset<<
            " objects="<<pending.objects.size()<<" writes="<<pending.execution.writes<<" "<<pending.execution.error<<"; inspection="<<pending.inspectionError<<std::endl;
        Require(!pending.execution.passed() && !pending.execution.committed && pending.inspectionError.empty() &&
            pending.execution.function=="DeusEx.HUDObjectBelt.CreateSlots" && pending.execution.offset==11u &&
            pending.execution.error=="NewChild native subclass Init is not implemented: extension.radioboxwindow" &&
            pending.objects.size()==48u &&
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
        Require(rootView.rowMajorWindows.empty() && rootView.colMajorWindows.empty() && rootView.tabOrderDirty &&
            rootView.tabGroupWindows==std::vector<std::string>{rootView.path} &&
            field(rootView,"Extension.TabGroupWindow.tabGroupIndex").value.integer==0 &&
            field(rootView,"Extension.TabGroupWindow.bSizeParentToChildren").value.boolean &&
            !field(rootView,"Extension.TabGroupWindow.bSizeChildrenToParent").value.boolean &&
            field(rootView,"Extension.ModalWindow.focusMode").value.integer==2 &&
            field(rootView,"Extension.ModalWindow.bDirtyAccelerators").value.boolean,
            "Root native TabGroup/Modal initialization omitted its actual self table/flags/focus mode");
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
        edge(rootView,"Extension.Window.firstChild",display.path);edge(rootView,"Extension.Window.lastChild",hudView.path);
        // Ammo, all health children and Cross completed their original Init,
        // visibility callbacks and unlocks. The belt's first native subclass
        // constructor is pending, so neither HUD.belt nor Root.hud is assigned.
        edge(rootView,"DeusEx.DeusExRootWindow.hud",{});
        edge(hudView,"DeusEx.DeusExHUD.Ammo",ammoView.path);edge(hudView,"DeusEx.DeusExHUD.hit",hitView.path);
        edge(hudView,"DeusEx.DeusExHUD.Cross",crossView.path);edge(hudView,"DeusEx.DeusExHUD.belt",{});
        edge(display,"Extension.Window.parentOwner",rootView.path);edge(display,"Extension.Window.winParent",rootView.path);
        edge(display,"Extension.Window.prevSibling",{});edge(display,"Extension.Window.nextSibling",hudView.path);
        edge(hudView,"Extension.Window.parentOwner",rootView.path);edge(hudView,"Extension.Window.winParent",rootView.path);
        edge(hudView,"Extension.Window.prevSibling",display.path);edge(hudView,"Extension.Window.nextSibling",{});
        edge(hudView,"Extension.Window.firstChild",ammoView.path);edge(hudView,"Extension.Window.lastChild",beltView.path);
        edge(ammoView,"Extension.Window.parentOwner",hudView.path);edge(ammoView,"Extension.Window.winParent",hudView.path);
        edge(ammoView,"Extension.Window.prevSibling",{});edge(ammoView,"Extension.Window.nextSibling",hitView.path);
        edge(ammoView,"DeusEx.HUDBaseWindow.Player",player);
        edge(ammoView,"DeusEx.HUDAmmoDisplay.Player",player);
        edge(hitView,"DeusEx.HUDBaseWindow.Player",player);edge(hitView,"DeusEx.HUDHitDisplay.Player",player);
        edge(hitView,"Extension.Window.parentOwner",hudView.path);edge(hitView,"Extension.Window.winParent",hudView.path);
        edge(hitView,"Extension.Window.prevSibling",ammoView.path);edge(hitView,"Extension.Window.nextSibling",crossView.path);
        edge(crossView,"Extension.Window.parentOwner",hudView.path);edge(crossView,"Extension.Window.winParent",hudView.path);
        edge(crossView,"Extension.Window.prevSibling",hitView.path);edge(crossView,"Extension.Window.nextSibling",beltView.path);
        edge(beltView,"Extension.Window.parentOwner",hudView.path);edge(beltView,"Extension.Window.winParent",hudView.path);
        edge(beltView,"Extension.Window.prevSibling",crossView.path);edge(beltView,"Extension.Window.nextSibling",{});
        edge(beltView,"Extension.Window.firstChild",{});edge(beltView,"Extension.Window.lastChild",{});
        Require(hudView.outerPath==rootView.path && ammoView.outerPath==hudView.path &&
            hitView.outerPath==hudView.path && crossView.outerPath==hudView.path && beltView.outerPath==hudView.path &&
            !field(hudView,"Extension.Window.bIsVisible").value.boolean &&
            field(ammoView,"Extension.Window.bIsVisible").value.boolean && field(hitView,"Extension.Window.bIsVisible").value.boolean &&
            field(crossView,"Extension.Window.bIsVisible").value.boolean && !field(beltView,"Extension.Window.bIsVisible").value.boolean &&
            !field(hudView,"Extension.Window.bIsSensitive").value.boolean && field(ammoView,"Extension.Window.bIsSensitive").value.boolean &&
            field(hudView,"Extension.Window.lockCount").value.integer==1 && field(ammoView,"Extension.Window.lockCount").value.integer==0 &&
            field(hitView,"Extension.Window.lockCount").value.integer==0 && field(crossView,"Extension.Window.lockCount").value.integer==0 &&
            field(beltView,"Extension.Window.lockCount").value.integer==1,
            "Original hidden HUD sensitivity or in-flight child lock state was skipped/fabricated");
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
        for(const auto& [view,width,height]:{
            std::tuple{&ammoView,95.0f,77.0f},std::tuple{&hitView,84.0f,106.0f},std::tuple{&beltView,541.0f,69.0f}})
            Require(field(*view,"Extension.Window.hardcodedWidth").value.floating==width &&
                field(*view,"Extension.Window.hardcodedHeight").value.floating==height &&
                field(*view,"Extension.Window.Width").value.floating==10.0f && field(*view,"Extension.Window.Height").value.floating==10.0f,
                "Original ammo/health SetSize did not defer current geometry while storing requested dimensions");
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
        const auto rootAttempt=ExecutePortableActorFunction(player,"Extension.PlayerPawnExt.InitRootWindow");
        std::cout<<"ORIGINAL unbound root initialization boundary: "<<rootAttempt.function<<':'<<rootAttempt.offset<<
            " opcode="<<unsigned(rootAttempt.opcode)<<" "<<rootAttempt.error<<std::endl;
        Require(!rootAttempt.passed() && !rootAttempt.committed && rootAttempt.function=="DeusEx.HUDObjectBelt.CreateSlots" &&
            rootAttempt.offset==11u && rootAttempt.error=="NewChild native subclass Init is not implemented: extension.radioboxwindow" &&
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
