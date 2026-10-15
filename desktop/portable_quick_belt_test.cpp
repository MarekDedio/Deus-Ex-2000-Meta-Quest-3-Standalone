#include "Precomp.h"
#include "portable_unreal_runtime.h"
#include "quest_save_bundle.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <tuple>

namespace {
using Bytes=std::vector<std::uint8_t>;
using Kind=QuestVr::Vm::Kind;
std::size_t checks{};
void Require(bool condition,const std::string& message) {
    if (!condition) throw std::runtime_error(message);
    ++checks;
}
struct Temporary {
    std::filesystem::path parent,directory;
    explicit Temporary(const std::filesystem::path& root) {
        parent=std::filesystem::canonical(std::filesystem::temp_directory_path());
        const auto original=std::filesystem::canonical(root);
        Require(parent.string().rfind(original.string(),0u)!=0u,
            "Quickbar checkpoint parent overlaps the original installation");
        const auto stamp=std::chrono::steady_clock::now().time_since_epoch().count();
        for (unsigned attempt=0u;attempt<20u;++attempt) {
            const auto candidate=parent/("deusex-quick-belt-"+std::to_string(stamp)+'-'+std::to_string(attempt));
            if (std::filesystem::create_directory(candidate)) {
                directory=std::filesystem::canonical(candidate);break;
            }
        }
        Require(!directory.empty() && directory.parent_path()==parent,
            "Cannot create isolated quickbar checkpoint directory");
    }
    ~Temporary() {
        ShutdownPortableRuntime();
        std::error_code error;
        const auto actual=std::filesystem::weakly_canonical(directory,error);
        if (!error && !directory.empty() && actual==directory && actual.parent_path()==parent &&
            actual.filename().string().rfind("deusex-quick-belt-",0u)==0u)
            std::filesystem::remove_all(actual,error);
    }
    std::string Path(const std::string& name) const { return (directory/(name+".sav")).string(); }
};
Bytes Snapshot(const Temporary& temporary,const std::string& name) {
    const auto path=temporary.Path(name);
    Require(SavePortableRuntimeState(path),"Cannot save quickbar checkpoint: "+name);
    Bytes bytes;
    Require(QuestVr::ReadBoundedSaveFile(path,QuestVr::kMaximumSaveRuntimeBytes,bytes),
        "Cannot read quickbar checkpoint: "+name);
    return bytes;
}
std::uint32_t Word(const Bytes& bytes,std::size_t offset) {
    Require(offset<=bytes.size() && bytes.size()-offset>=4u,"Generated quickbar checkpoint word truncated");
    std::uint32_t value{};
    for (unsigned i=0u;i<4u;++i) value|=std::uint32_t(bytes[offset+i])<<(i*8u);
    return value;
}
void ReplaceWord(Bytes& bytes,std::size_t offset,std::uint32_t value) {
    Require(offset<=bytes.size() && bytes.size()-offset>=4u,"Generated quickbar mutation escaped its checkpoint");
    for (unsigned i=0u;i<4u;++i) bytes[offset+i]=static_cast<std::uint8_t>(value>>(i*8u));
}
bool Same(const PortableQuickBeltSnapshot& left,const PortableQuickBeltSnapshot& right) {
    if (std::tie(left.available,left.equippedActorPath,left.collectedKeys)!=
        std::tie(right.available,right.equippedActorPath,right.collectedKeys)) return false;
    for (std::size_t position=0u;position<left.slots.size();++position) {
        if (left.slots[position].has_value()!=right.slots[position].has_value()) return false;
        if (!left.slots[position]) continue;
        const auto& a=*left.slots[position];const auto& b=*right.slots[position];
        if (std::tie(a.actorPath,a.classPath,a.iconPath,a.description,a.keyRing,a.selected)!=
            std::tie(b.actorPath,b.classPath,b.iconPath,b.description,b.keyRing,b.selected)) return false;
    }
    return true;
}
void VerifySelection(const PortableQuickBeltSnapshot& snapshot,const std::string& equipped) {
    Require(snapshot.available && snapshot.equippedActorPath==equipped &&
        GetPortableRuntimeEquippedItem()==equipped,"Quickbar and gameplay equipment identity disagree");
    std::size_t selected{};
    for (std::size_t position=0u;position<snapshot.slots.size();++position) {
        const auto& item=snapshot.slots[position];
        if (!item) continue;
        Require(item->keyRing==(position==0u),"Quickbar keyring escaped special slot0");
        Require(item->selected==(item->actorPath==equipped),"Quickbar selection does not track equipped identity");
        if (item->selected) ++selected;
    }
    Require(selected==1u,"Quickbar does not have exactly one equipped slot");
    Require(PortableRuntimeKeyRingEquipped()==(snapshot.slots[0u] &&
        snapshot.slots[0u]->actorPath==equipped),"Keyring equipped query disagrees with the selected slot");
}
std::string Find(const std::vector<PortableActorSnapshot>& actors,const std::string& classPath,bool prefix=false) {
    const auto found=std::find_if(actors.begin(),actors.end(),[&](const auto& actor) {
        return actor.inventory && (prefix?actor.classPath.rfind(classPath,0u)==0u:actor.classPath==classPath);
    });
    Require(found!=actors.end(),"Original Training has no required item: "+classPath);
    return found->objectPath;
}
void Pickup(const std::string& path) {
    const auto before=GetPortableRuntimeInventoryCount();
    const auto picked=InteractPortableRuntimeActor(path);
    Require(picked.handled && picked.worldChanged && picked.action=="pickup" &&
        GetPortableRuntimeInventoryCount()==before+1u,"Cannot collect original Training item: "+path);
    const auto inventory=GetPortableRuntimeInventoryItems();
    Require(std::find(inventory.begin(),inventory.end(),path)!=inventory.end(),
        "Picked item lost its actual map identity: "+path);
}
void Ordinary(const PortableQuickBeltSnapshot& snapshot,std::size_t position,const std::string& actor,
    const std::string& classPath) {
    Require(position>0u && position<snapshot.slots.size() && snapshot.slots[position] &&
        snapshot.slots[position]->actorPath==actor && snapshot.slots[position]->classPath==classPath &&
        !snapshot.slots[position]->keyRing && !snapshot.slots[position]->iconPath.empty() &&
        !snapshot.slots[position]->description.empty(),"Quickbar lost actual item metadata or stable position");
}
void RejectCheckpoint(const Temporary& temporary,const std::string& name,const Bytes& malformed,
    const Bytes& expected,const PortableQuickBeltSnapshot& quick) {
    const auto inventory=GetPortableRuntimeInventoryItems();
    const auto health=GetPortableRuntimePlayerHealth();
    const auto revision=GetPortableRuntimeWorldRevision();
    if (malformed.empty()) {
        // Production durable saves deliberately refuse an empty file. This
        // isolated generated truncation fixture must nevertheless exercise it.
        std::ofstream file(temporary.Path(name),std::ios::binary|std::ios::trunc);
        Require(static_cast<bool>(file),"Cannot create generated empty quickbar checkpoint");
        file.close();
        Require(static_cast<bool>(file),"Cannot close generated empty quickbar checkpoint");
    } else Require(QuestVr::WriteDurableSaveFile(temporary.Path(name),malformed),
        "Cannot stage generated invalid quickbar checkpoint");
    Require(!ValidatePortableRuntimeState(temporary.Path(name),"00_Training") &&
        !LoadPortableRuntimeState(temporary.Path(name)),"Invalid quickbar checkpoint accepted: "+name);
    Require(Same(ReadPortableRuntimeQuickBelt(),quick) && inventory==GetPortableRuntimeInventoryItems() &&
        health==GetPortableRuntimePlayerHealth() && revision==GetPortableRuntimeWorldRevision() &&
        Snapshot(temporary,name+"-after")==expected,
        "Invalid quickbar checkpoint changed equipment, inventory, health or complete state: "+name);
}
} // namespace

int main(int argc,char** argv) {
    try {
        if (argc==1) {
            std::cout << "SKIP: supply owned Deus Ex installation for actual quickbar/keyring integration\n";
            return 77;
        }
        if (argc!=2) throw std::runtime_error("Usage: portable_quick_belt_test GAME_ROOT");
        const std::filesystem::path root=argv[1];
        Temporary temporary(root);
        std::vector<PortablePackageTables> scripts;
        for (const auto* name:{"ConSys","Core","DeusEx","DeusExCharacters","DeusExConAudioAIBarks",
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
        const auto map=LoadPortablePackageTables((root/"Maps"/"00_Training.dx").string());
        const auto prepare=[&] {
            Require(InitializePortableRuntime(scripts).passed,"Cannot initialize actual owned quickbar packages");
            Require(LoadPortableRuntimeMap(map).passed,"Cannot load actual Training quickbar map");
        };
        prepare();
        Require(GetPortableRuntimeInventoryItems().empty(),"Fresh Training unexpectedly owns picked-up items");
        // Save before first quickbar read retains the existing pre-equipment
        // format, so legacy/script-only users do not receive a new envelope.
        const auto legacy=Snapshot(temporary,"legacy-before-quickbar");
        Require(Word(legacy,0u)==0x53515844u && Word(legacy,4u)<15u,
            "Uninitialized quickbar changed the existing runtime checkpoint format");
        const auto initial=ReadPortableRuntimeQuickBelt();
        Require(initial.available && initial.slots[0u] && initial.slots[0u]->keyRing &&
            initial.slots[0u]->actorPath=="QuestVR.NanoKeyRing" &&
            initial.slots[0u]->classPath=="DeusEx.NanoKeyRing" && initial.collectedKeys==0u,
            "Fresh Training lacks its functional starter keyring in slot0");
        const auto ring=initial.slots[0u]->actorPath;
        const auto ringIcon=ReadPortableClassDefault("DeusEx.NanoKeyRing","Icon");
        const auto ringDescription=ReadPortableClassDefault("DeusEx.NanoKeyRing","BeltDescription");
        Require(ringIcon.kind==Kind::Object && !ringIcon.text.empty() &&
            initial.slots[0u]->iconPath==ringIcon.text && ringDescription.kind==Kind::String &&
            !ringDescription.text.empty() && initial.slots[0u]->description==ringDescription.text,
            "Starter keyring icon/description did not come from the original class defaults");
        for (std::size_t position=1u;position<initial.slots.size();++position)
            Require(!initial.slots[position],"Fresh Training quickbar ordinary slots are not empty");
        VerifySelection(initial,ring);
        Require(Same(initial,ReadPortableRuntimeQuickBelt()),"Repeated quickbar read changed starter state");
        const auto actors=GetPortableRuntimeMapActors(true);
        const auto crowbar=Find(actors,"DeusEx.WeaponCrowbar");
        const auto soda=Find(actors,"DeusEx.Candybar");
        const auto knife=Find(actors,"DeusEx.WeaponCombatKnife");
        const auto key=Find(actors,"DeusEx.NanoKey");
        const auto ammo=Find(actors,"DeusEx.Ammo",true);
        Require(!EquipPortableRuntimeInventoryItem(crowbar),"Quickbar equipped an uncollected world item");
        Pickup(crowbar);Pickup(soda);Pickup(knife);
        auto owned=ReadPortableRuntimeQuickBelt();
        Ordinary(owned,1u,crowbar,"DeusEx.WeaponCrowbar");
        Ordinary(owned,2u,soda,"DeusEx.Candybar");
        Ordinary(owned,3u,knife,"DeusEx.WeaponCombatKnife");
        Require(EquipPortableRuntimeInventoryItem(crowbar),"Cannot equip actual collected crowbar");
        VerifySelection(ReadPortableRuntimeQuickBelt(),crowbar);
        Require(CyclePortableRuntimeQuickBelt(1),"Quickbar forward cycle failed");
        VerifySelection(ReadPortableRuntimeQuickBelt(),soda);
        Require(CyclePortableRuntimeQuickBelt(1),"Quickbar second forward cycle failed");
        VerifySelection(ReadPortableRuntimeQuickBelt(),knife);
        Require(CyclePortableRuntimeQuickBelt(1),"Quickbar cycle did not wrap through starter keyring");
        VerifySelection(ReadPortableRuntimeQuickBelt(),ring);
        Require(CyclePortableRuntimeQuickBelt(-1),"Quickbar backward cycle failed");
        VerifySelection(ReadPortableRuntimeQuickBelt(),knife);
        const auto selected=ReadPortableRuntimeQuickBelt();
        Require(!CyclePortableRuntimeQuickBelt(0) && !EquipPortableRuntimeInventoryItem("00_Training.NotAnItem") &&
            !EquipPortableRuntimeInventoryItem("DeusEx.WeaponCrowbar") &&
            !EquipPortableRuntimeInventoryItem(""),"Quickbar accepted an invalid equip/cycle request");
        Require(Same(selected,ReadPortableRuntimeQuickBelt()),"Rejected equipment request changed the selected item");
        const auto locked=InteractPortableRuntimeActor("00_Training.DeusExMover47");
        Require(locked.handled && locked.action=="mover_locked","Training key door opened without its actual key");
        Pickup(key);Pickup(ammo);
        const auto withKeys=ReadPortableRuntimeQuickBelt();
        Require(withKeys.collectedKeys==1u,"Picked NanoKey did not enter the functional starter keyring");
        Ordinary(withKeys,1u,crowbar,"DeusEx.WeaponCrowbar");
        Ordinary(withKeys,2u,soda,"DeusEx.Candybar");
        Ordinary(withKeys,3u,knife,"DeusEx.WeaponCombatKnife");
        for (std::size_t position=1u;position<withKeys.slots.size();++position)
            Require(!withKeys.slots[position] || (withKeys.slots[position]->actorPath!=key &&
                withKeys.slots[position]->actorPath!=ammo),"Quickbar put loose keys or ammunition in an ordinary slot");
        Require(!EquipPortableRuntimeInventoryItem(key) && !EquipPortableRuntimeInventoryItem(ammo),
            "Loose NanoKey/ammunition can be equipped as a hand item");
        Require(!InteractPortableRuntimeActor(key).handled && ReadPortableRuntimeQuickBelt().collectedKeys==1u,
            "Repeated inactive-key pickup duplicated the keyring entry");
        Require(EquipPortableRuntimeInventoryItem(ring) && PortableRuntimeKeyRingEquipped(),
            "Collected-key ring cannot be equipped for door use");
        const auto unlocked=InteractPortableRuntimeActor("00_Training.DeusExMover47");
        Require(unlocked.handled && unlocked.worldChanged && unlocked.action=="mover_open",
            "Functional ring did not unlock the Training door using its collected Train key");
        Require(EquipPortableRuntimeInventoryItem(soda),"Cannot select actual soda before saving");
        const auto savedHealth=DamagePortableRuntimePlayer(17.0f);
        const auto savedQuick=ReadPortableRuntimeQuickBelt();
        const auto savedInventory=GetPortableRuntimeInventoryItems();
        const auto saved=Snapshot(temporary,"equipment-v15");
        Require(Word(saved,0u)==0x53515844u && Word(saved,4u)==15u,
            "Initialized playable equipment was not saved in the new v15 envelope");
        const auto quickLength=Word(saved,8u);
        Require(quickLength>0u && quickLength<=saved.size()-12u,"v15 quickbar blob length is invalid");
        const auto innerLengthOffset=12u+static_cast<std::size_t>(quickLength);
        const auto innerLength=Word(saved,innerLengthOffset);
        Require(innerLength>=8u && innerLength==saved.size()-innerLengthOffset-4u &&
            Word(saved,innerLengthOffset+4u)==0x53515844u && Word(saved,innerLengthOffset+8u)<15u,
            "v15 did not retain a complete original runtime payload inside the equipment wrapper");
        Require(ValidatePortableRuntimeState(temporary.Path("equipment-v15"),"00_Training"),
            "Valid equipment wrapper failed map-bound validation");
        Require(ConsumePortableRuntimeInventoryItem(soda),"Cannot consume the selected actual soda");
        const auto consumed=ReadPortableRuntimeQuickBelt();
        Require(!consumed.slots[2u] && consumed.equippedActorPath!=soda,
            "Consumed selected item retained its quick slot or equipped identity");
        VerifySelection(consumed,consumed.equippedActorPath);
        Ordinary(consumed,1u,crowbar,"DeusEx.WeaponCrowbar");
        Ordinary(consumed,3u,knife,"DeusEx.WeaponCombatKnife");
        Require(EquipPortableRuntimeInventoryItem(ring),"Functional starter keyring cannot be selected");
        VerifySelection(ReadPortableRuntimeQuickBelt(),ring);
        Require(LoadPortableRuntimeState(temporary.Path("equipment-v15")),"Warm equipment restore failed");
        Require(Same(savedQuick,ReadPortableRuntimeQuickBelt()) && savedInventory==GetPortableRuntimeInventoryItems() &&
            savedHealth==GetPortableRuntimePlayerHealth() && Snapshot(temporary,"warm-restored")==saved,
            "Warm equipment restore lost stable slots, selection, keys or original gameplay state");
        ShutdownPortableRuntime();prepare();
        Require(LoadPortableRuntimeState(temporary.Path("equipment-v15")),"Cold equipment restore failed");
        Require(Same(savedQuick,ReadPortableRuntimeQuickBelt()) && savedInventory==GetPortableRuntimeInventoryItems() &&
            savedHealth==GetPortableRuntimePlayerHealth() && Snapshot(temporary,"cold-restored")==saved,
            "Cold equipment restore lost stable slots, selection, keys or original gameplay state");
        VerifySelection(ReadPortableRuntimeQuickBelt(),soda);

        std::vector<std::size_t> cuts{0u,1u,4u,7u,8u,11u,12u,
            innerLengthOffset-1u,innerLengthOffset,innerLengthOffset+3u,saved.size()-1u};
        std::sort(cuts.begin(),cuts.end());cuts.erase(std::unique(cuts.begin(),cuts.end()),cuts.end());
        for (const auto cut:cuts) {
            Require(cut<saved.size(),"Generated v15 truncation is not a truncation");
            RejectCheckpoint(temporary,"truncated-"+std::to_string(cut),
                Bytes(saved.begin(),saved.begin()+static_cast<std::ptrdiff_t>(cut)),saved,savedQuick);
        }
        for (const auto& [name,offset,value]:std::vector<std::tuple<std::string,std::size_t,std::uint32_t>>{
            {"bad-magic",0u,0u},{"future-version",4u,16u},{"empty-quick-blob",8u,0u},
            {"huge-quick-blob",8u,std::numeric_limits<std::uint32_t>::max()},
            {"empty-inner",innerLengthOffset,0u},
            {"huge-inner",innerLengthOffset,std::numeric_limits<std::uint32_t>::max()},
            {"bad-inner-magic",innerLengthOffset+4u,0u}}) {
            auto malformed=saved;ReplaceWord(malformed,offset,value);
            RejectCheckpoint(temporary,name,malformed,saved,savedQuick);
        }
        auto trailing=saved;trailing.push_back(0u);
        RejectCheckpoint(temporary,"trailing-wrapper-data",trailing,saved,savedQuick);
        Require(LoadPortableRuntimeState(temporary.Path("legacy-before-quickbar")),
            "Existing pre-equipment runtime save no longer loads");
        Require(GetPortableRuntimeInventoryItems().empty() && GetPortableRuntimePlayerHealth()==100.0f,
            "Legacy load retained equipment-era inventory or aggregate damage");
        const auto legacyRing=ReadPortableRuntimeQuickBelt();
        Require(legacyRing.collectedKeys==0u && legacyRing.slots[0u] && legacyRing.slots[0u]->actorPath==ring,
            "Legacy load did not recreate an empty functional starter ring");
        for (std::size_t position=1u;position<legacyRing.slots.size();++position)
            Require(!legacyRing.slots[position],"Legacy load retained an equipment-era quickbar slot");
        VerifySelection(legacyRing,ring);
        std::cout << "PASS playable quickbar integration: " << checks
            << " checks; original starter-ring metadata, actual Training pickups, stable slots/equipment cycling, collected keys, consumption, warm/cold v15 saves, legacy restore and atomic malformed-wrapper refusal\n";
        return 0;
    } catch (const std::exception& error) {
        ShutdownPortableRuntime();
        std::cerr << "FAIL playable quickbar integration: " << error.what() << '\n';
        return 1;
    }
}
