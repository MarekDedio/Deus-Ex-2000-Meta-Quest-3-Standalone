#include "quest_quick_belt.h"

#include <climits>
#include <iostream>
#include <set>

namespace {
std::size_t checks{};
void Check(bool condition,const char* message) {
    ++checks;
    if (!condition) throw std::runtime_error(message);
}
template<class Action> void Refuses(Action action,const char* message) {
    bool refused{};
    try { action(); } catch (const std::runtime_error&) { refused = true; }
    Check(refused,message);
}
std::vector<QuestVr::QuickBeltItem> Inventory(std::size_t count) {
    std::vector<QuestVr::QuickBeltItem> owned{{QuestVr::kQuickBeltKeyRingIdentity,true,false}};
    for (std::size_t index = 0u; index < count; ++index)
        owned.push_back({"Training.Item"+std::to_string(index),false,true});
    owned.push_back({"Training.NanoKey0",false,false});
    owned.push_back({"Training.Ammo0",false,false});
    return owned;
}
void Validate(const QuestVr::QuickBeltModel& model,const std::vector<QuestVr::QuickBeltItem>& owned) {
    const auto& state = model.State();
    std::set<std::string> seen;
    for (std::size_t slot = 0u; slot < state.slots.size(); ++slot) {
        const auto& identity = state.slots[slot];
        if (identity.empty()) continue;
        const auto* item = QuestVr::QuickBeltDetail::Find(owned,identity);
        Check(item && (item->eligible || item->keyRing),"Every shortcut must remain owned and eligible");
        Check(item->keyRing == (slot == 0u),"Only actual supplied keyring can occupy zero");
        Check(seen.insert(QuestVr::QuickBeltDetail::Fold(identity)).second,"Shortcuts must not duplicate items");
    }
    if (!model.EquippedActorPath().empty()) {
        const auto* item = QuestVr::QuickBeltDetail::Find(owned,model.EquippedActorPath());
        Check(item && (item->eligible || item->keyRing),"Equipment cannot outlive ownership");
        Check(model.SlotFor(model.EquippedActorPath()).has_value(),"Equipped item has a usable quick shortcut");
    }
    QuestVr::QuickBeltModel restored;
    restored.Restore(QuestVr::DecodeQuickBeltState(QuestVr::EncodeQuickBeltState(state)),owned);
    Check(restored.State() == state,"Quick belt save must restore assignments and equipped identity exactly");
}
}

int main() {
    try {
        using namespace QuestVr;
        QuickBeltModel model;
        Check(!model.Cycle(1) && !model.SelectSlot(0u) && !model.Equip(kQuickBeltKeyRingIdentity),
            "Empty model must not invent equipment or ring ownership");
        Check(!model.Reconcile({}) && model.EquippedActorPath().empty(),"Empty inventory is truly empty");
        auto owned = Inventory(0u);
        Check(model.Reconcile(owned),"Supplied functional keyring becomes a shortcut");
        Check(model.State().slots[0] == kQuickBeltKeyRingIdentity &&
            model.EquippedActorPath() == kQuickBeltKeyRingIdentity,"Starter ring uses original special slot zero");
        Check(!model.Reconcile(owned),"Unchanged ownership does not dirty the HUD");
        Check(!model.SelectSlot(1u) && !model.SelectSlot(10u) && !model.Cycle(0),"Empty slots and zero movement do not equip");
        Check(model.Cycle(INT_MIN) && model.Cycle(INT_MAX) && model.EquippedActorPath() == kQuickBeltKeyRingIdentity,
            "Single ring cycling handles all signed step values");
        Check(!model.Equip("Training.NanoKey0") && !model.Equip("Training.Ammo0"),"Individual keys and ammunition remain outside belt");
        Validate(model,owned);

        owned = Inventory(3u);
        model.Reconcile(owned);
        Check(model.State().slots[1] == "Training.Item0" && model.State().slots[3] == "Training.Item2" &&
            model.State().slots[4].empty(),"Pickups fill ordinary shortcuts in ownership order");
        Check(model.EquippedActorPath() == kQuickBeltKeyRingIdentity,"Pickup does not silently equip a new item");
        Check(model.Cycle(1) && model.EquippedActorPath() == "Training.Item0", "Ring cycles to first occupied ordinary slot");
        Check(model.Cycle(1) && model.EquippedActorPath() == "Training.Item1", "Positive cycle follows bar order");
        Check(model.Cycle(2) && model.EquippedActorPath() == kQuickBeltKeyRingIdentity,"Cycle wraps through slot zero");
        Check(model.Cycle(-1) && model.EquippedActorPath() == "Training.Item2", "Reverse cycle skips empty positions");
        Check(model.Equip("TRAINING.ITEM0") && model.EquippedActorPath() == "Training.Item0" &&
            model.SlotFor("training.item0") == 1u,"Identity lookups use original case-insensitive UObject semantics");
        auto before = model.State();
        Check(!model.Equip("SomeOtherMap.Item0") && model.State() == before,"Unowned item cannot be equipped");
        owned.erase(owned.begin()+1);
        model.Reconcile(owned);
        Check(model.State().slots[1].empty() && model.State().slots[2] == "Training.Item1" &&
            model.State().slots[3] == "Training.Item2","Consumption preserves surviving shortcut positions");
        Check(model.EquippedActorPath() == kQuickBeltKeyRingIdentity,"Consumed selected item safely returns to keyring");
        owned.push_back({"Training.NewPickup",false,true});
        model.Reconcile(owned);
        Check(model.State().slots[1] == "Training.NewPickup","Next pickup fills a vacated shortcut");
        std::reverse(owned.begin(),owned.end());
        before = model.State();
        Check(!model.Reconcile(owned) && model.State() == before,"Reordering inventory does not scramble existing shortcuts");
        Validate(model,owned);

        owned = Inventory(12u);
        model.Reset();
        model.Reconcile(owned);
        Check(model.State().slots[9] == "Training.Item8" && !model.SlotFor("Training.Item11"),
            "Ordinary belt has nine positions without losing overflow inventory");
        Check(model.Equip("Training.Item11") && model.State().slots[9] == "Training.Item11" &&
            model.EquippedActorPath() == "Training.Item11","Equipping overflow from ring replaces shortcut nine");
        Check(model.SelectSlot(3u) && model.Equip("Training.Item10") && model.State().slots[3] == "Training.Item10" &&
            !model.SlotFor("Training.Item2"),"Equipping overflow replaces selected shortcut without consuming displaced item");
        Check(model.Equip("Training.Item2") && model.State().slots[3] == "Training.Item2", "Displaced owned equipment can be re-equipped");
        before = model.State();
        Check(!model.Reconcile(owned) && model.State() == before,"Reconciliation preserves chosen overflow assignments");
        Validate(model,owned);

        auto lowercaseOwned = owned;
        for (auto& item : lowercaseOwned) item.actorPath = QuickBeltDetail::Fold(item.actorPath);
        model.Reconcile(lowercaseOwned);
        Check(model.EquippedActorPath() == "training.item2" && model.State().slots[0] == "questvr.nanokeyring",
            "Runtime canonical casing is refreshed without changing selected ownership");
        Validate(model,lowercaseOwned);

        before = model.State();
        auto invalid = lowercaseOwned;
        invalid.push_back({"TRAINING.ITEM0",false,true});
        Refuses([&] { model.Reconcile(invalid); },"Case-insensitive duplicate ownership must fail");
        Check(model.State() == before,"Invalid reconciliation must not mutate assignments");
        invalid = lowercaseOwned;
        invalid.push_back({"Another.Ring",true,true});
        Refuses([&] { model.Reconcile(invalid); },"Two supplied rings must fail");
        Check(model.State() == before,"Invalid ring input must not mutate equipment");
        invalid = lowercaseOwned;
        invalid.push_back({std::string("bad\0identity",12u),false,true});
        Refuses([&] { model.Reconcile(invalid); },"Embedded identity zero must fail");
        invalid = lowercaseOwned;
        invalid.push_back({std::string(4097u,'x'),false,true});
        Refuses([&] { model.Reconcile(invalid); },"Oversized identity must fail");
        Refuses([&] { model.Reconcile(std::vector<QuickBeltItem>(1025u)); },"Oversized ownership chain must fail");

        auto badState = before;
        badState.slots[1] = "Training.NonOwned";
        Refuses([&] { model.Restore(badState,lowercaseOwned); },"Save cannot introduce unowned shortcuts");
        Check(model.State() == before,"Rejected restore must leave live model intact");
        badState = before;
        badState.slots[1] = "Training.Ammo0";
        Refuses([&] { model.Restore(badState,lowercaseOwned); },"Save cannot equip ammunition");
        badState = before;
        std::swap(badState.slots[0],badState.slots[1]);
        Refuses([&] { model.Restore(badState,lowercaseOwned); },"Ring and ordinary positions cannot be interchanged");
        badState = before;
        badState.slots[1] = badState.slots[2];
        Refuses([&] { model.Restore(badState,lowercaseOwned); },"Duplicate shortcut save must fail");
        badState = before;
        badState.equippedActorPath = "Training.NotOwned";
        Refuses([&] { model.Restore(badState,lowercaseOwned); },"Save cannot equip unowned actor");
        Check(model.State() == before,"All invalid restore cases preserve live model");

        const auto encoded = EncodeQuickBeltState(before);
        Check(DecodeQuickBeltState(encoded) == before,"Bounded binary checkpoint round trip");
        for (std::size_t length = 0u; length < encoded.size(); ++length) {
            const std::vector<std::uint8_t> truncated(encoded.begin(),encoded.begin()+static_cast<std::ptrdiff_t>(length));
            Refuses([&] { (void)DecodeQuickBeltState(truncated); },"Every truncated checkpoint must fail");
        }
        for (std::size_t index = 0u; index < 8u; ++index) {
            auto bad = encoded;
            bad[index] ^= 0xffu;
            Refuses([&] { (void)DecodeQuickBeltState(bad); },"Unknown checkpoint magic/version must fail");
        }
        auto bad = encoded;
        bad.push_back(0u);
        Refuses([&] { (void)DecodeQuickBeltState(bad); },"Trailing checkpoint bytes must fail");
        bad = encoded;
        for (std::size_t index = 8u; index < 12u; ++index) bad[index] = 255u;
        Refuses([&] { (void)DecodeQuickBeltState(bad); },"Checkpoint string integer overflow must fail");
        bad = encoded;
        bad[12] = 0u;
        Refuses([&] { (void)DecodeQuickBeltState(bad); },"Checkpoint embedded identity NUL must fail");

        // Adversarial long-running ownership/equipment changes. Validate both
        // ownership and a serialized restart after every action, not just shape.
        std::uint32_t random = 0x58414245u;
        const auto next = [&]() { random = random*1664525u+1013904223u; return random; };
        owned = Inventory(30u);
        model.Reset();
        model.Reconcile(owned);
        for (std::size_t iteration = 0u; iteration < 10000u; ++iteration) {
            switch (next()%5u) {
            case 0u: (void)model.Cycle(static_cast<int>(next()%31u)-15); break;
            case 1u: (void)model.Equip("Training.Item"+std::to_string(next()%40u)); break;
            case 2u: (void)model.SelectSlot(next()%12u); break;
            case 3u:
                if (owned.size() > 3u) owned.erase(owned.begin()+1+static_cast<std::ptrdiff_t>(next()%(owned.size()-3u)));
                model.Reconcile(owned);
                break;
            default: {
                const auto path = "Training.Item"+std::to_string(next()%40u);
                if (!QuickBeltDetail::Find(owned,path)) owned.push_back({path,false,true});
                model.Reconcile(owned);
                break;
            }
            }
            Validate(model,owned);
        }
        model.Reset();
        Check(model.State() == QuickBeltState{} && !model.Equip(kQuickBeltKeyRingIdentity),"Reset clears all prior session ownership");
        std::cout << "Quick belt checks: " << checks << "\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Quick belt failed after " << checks << " checks: " << error.what() << "\n";
        return 1;
    }
}
