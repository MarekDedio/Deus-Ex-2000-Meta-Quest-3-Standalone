#include "quest_ai_event_state.h"

#include <functional>
#include <iostream>

namespace {
using namespace QuestVr::Ai;
std::size_t checks = 0;
std::size_t rejections = 0;

void Require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
    ++checks;
}

void Reject(const std::function<void()>& operation, const std::string& message) {
    bool rejected = false;
    try { operation(); } catch (const std::runtime_error&) { rejected = true; }
    Require(rejected, message);
    ++rejections;
}

State Empty() {
    State state;
    state.ownerPath = "FixtureMap.LevelInfo0";
    state.levelPath = "FixtureMap.MyLevel";
    return state;
}

Id AddReceiver(State& state, const std::string& actor, const std::string& event,
               const std::string& callback = "AIEvent", const std::string& score = "") {
    std::size_t used = 0;
    return Register(state, actor, event, callback, score, {}, used);
}

Id AddSender(State& state, const std::string& actor, const std::string& event,
             std::uint8_t channel = 0, float value = 1, float radius = 800,
             bool pulse = false, bool pendingKill = false) {
    std::size_t used = 0;
    return Emit(state, actor, event, channel, value, radius, pulse, pendingKill, used);
}

void GuardsAndDefaults() {
    auto state = Empty();
    Validate(state);
    Require(state.processDepth == 0 && state.pendingDeleteCount == 0 &&
        state.historyCursor == 0 && state.receiverHead == 0, "Manager defaults changed");
    Require(RetainedBytes(state) >= sizeof(State), "Retained accounting omits manager");
    const auto before = state;
    std::size_t used = 0;
    Require(!Register(state, "", "Distress", "AIEvent", "", {}, used), "Null receiver allocated");
    Require(!Register(state, "FixtureMap.Pawn0", "nOnE", "AIEvent", "", {}, used), "None event registered");
    Require(!Emit(state, "", "Distress", 0, 1, 800, false, false, used), "Null sender allocated");
    Require(!Emit(state, "FixtureMap.Pawn0", "", 0, 1, 800, false, false, used), "None sender allocated");
    Require(!End(state, "FixtureMap.Pawn0", "Missing", 0, used), "Absent End claims a sender");
    Require(!ClearSender(state, "FixtureMap.Pawn0", "Missing", used), "Absent clear claims sender");
    Require(!ClearReceiver(state, "FixtureMap.Pawn0", "Missing", used), "Absent clear claims receiver");
    DestroyActor(state, "", used);
    Require(state == before, "Guard operation changed manager state");

    const Id receiver = AddReceiver(state, "FixtureMap.Pawn0", "Distress");
    Require(receiver == 1 && state.eventTypes.size() == 1 && state.receivers.size() == 1,
        "Receiver not allocated with first stable identity");
    const auto& node = state.receivers[0];
    Require(node.actor == "FixtureMap.Pawn0" && node.eventType == 1 && !node.deleted,
        "Receiver identity defaults changed");
    Require(node.flags == PerceptionFlags{} && !node.callbackPending && node.eventState == 0 &&
        !node.detected && node.previousScore == 0 && node.previousBestActor.empty() &&
        node.historyCursor == 0 && node.params == XAIParams{}, "Receiver detection defaults changed");
    Require(state.receiverHead == 1 && node.ringNext == 1 && node.ringPrev == 1,
        "First receiver does not form singleton ring");
    const Id sender = AddSender(state, "FixtureMap.Pawn1", "DISTRESS", 255);
    Require(sender == 1 && state.eventTypes.size() == 1 && state.eventTypes[0].name == "Distress",
        "Event identity/case/first spelling changed");
    Require(state.senders[0].current == Channels{} && state.senders[0].score == 0,
        "Unknown sensory type wrote new sender");
    for (const auto& slot : state.senders[0].history)
        Require(slot == Channels{}, "New sender history was not fully initialized");
    Validate(state);
}

void SenderChannels() {
    auto state = Empty();
    state.historyCursor = 7;
    const Id id = AddSender(state, "FixtureMap.Sender0", "Noise", 0, 0.25f, 999);
    Require(id == 1 && state.senders[0].current == Channels{0.25f, 0, 0, 0},
        "Visual used Radius or another sensory channel");
    AddSender(state, "FixtureMap.Sender0", "noise", 1, 2, 600);
    AddSender(state, "FixtureMap.Sender0", "noise", 2, 3, 1234);
    Require(state.senders[0].current == Channels{0.25f, 2, 600, 3}, "Sensory channel independence lost");
    Require(state.senders[0].history[7] == state.senders[0].current, "Current history slot not written");
    for (std::size_t index = 0; index < 16; ++index)
        if (index != 7) Require(state.senders[0].history[index] == Channels{}, "Wrong history slot written");
    const auto history = state.senders[0].history;
    AddSender(state, "FixtureMap.Sender0", "noise", 1, 0.5f, 300);
    Require(state.senders[0].current.volume == 0.5f && state.senders[0].current.radius == 300 &&
        state.senders[0].history == history, "Lower Start did not preserve history maxima");
    AddSender(state, "FixtureMap.Sender0", "noise", 1, 4, 900, true);
    Require(state.senders[0].current.volume == 0.5f && state.senders[0].current.radius == 300 &&
        state.senders[0].history[7].volume == 4 && state.senders[0].history[7].radius == 900,
        "Pulse incorrectly enabled/changed current emission");
    std::size_t used = 0;
    Require(End(state, "fixturemap.sender0", "NOISE", 1, used), "Case-folded End failed");
    Require(state.senders[0].current == Channels{0.25f, 0, 0, 3} &&
        state.senders[0].history[7] == Channels{0.25f, 4, 900, 3},
        "End erased history or other sensory channels");
    AddSender(state, "FixtureMap.Sender1", "Noise", 1, 5, 700);
    AddSender(state, "FixtureMap.Sender0", "Other", 2, 8);
    const auto others = state;
    Require(ClearSender(state, "FixtureMap.Sender0", "noise", used), "716 clear failed");
    Require(state.senders[0].current == Channels{} && state.senders[0].history == others.senders[0].history &&
        !state.senders[0].deleted && state.pendingDeleteCount == 0,
        "716 clear deleted sender or lost retained history");
    Require(state.senders[1] == others.senders[1] && state.senders[2] == others.senders[2],
        "716 clear modified another sender/name");
    const auto unknownBefore = state;
    Require(End(state, "FixtureMap.Sender0", "Noise", 254, used), "Existing unknown channel lookup lost");
    Require(state == unknownBefore, "Unknown End wrote channels");

    AddSender(state, "FixtureMap.Negative", "Negative", 1, -2, -800);
    const auto& negative = state.senders.back();
    Require(negative.current.volume == -2 && negative.current.radius == -800 &&
        negative.history[7].volume == 0 && negative.history[7].radius == 0,
        "Finite negative values were clamped/rejected or reduced maxima");
    AddSender(state, "FixtureMap.Pending", "Pending", 1, 9, 1000, false, true);
    Require(state.senders.back().current == Channels{} && state.senders.back().history[7] == Channels{},
        "Pending-kill Start was not zeroed before sender creation");
    AddSender(state, "FixtureMap.Pending", "Pending", 1, 2, 400);
    const auto pendingBefore = state.senders.back();
    AddSender(state, "FixtureMap.Pending", "Pending", 1, 99, 9999, true, true);
    Require(state.senders.back() == pendingBefore, "Pending-kill pulse unexpectedly cleared current");
    AddSender(state, "FixtureMap.Pending", "Pending", 1, 99, 9999, false, true);
    Require(state.senders.back().current == Channels{} &&
        state.senders.back().history == pendingBefore.history,
        "Pending-kill Start failed to zero current/preserve history");
    Validate(state);
}

void ReceiverRingAndLifecycle() {
    auto state = Empty();
    state.historyCursor = 13;
    AddReceiver(state, "FixtureMap.A", "Danger");
    AddReceiver(state, "FixtureMap.B", "Other");
    AddReceiver(state, "FixtureMap.C", "Danger");
    Require(state.receiverHead == 1 && state.receivers[0].ringNext == 2 &&
        state.receivers[1].ringNext == 3 && state.receivers[2].ringNext == 1 &&
        state.receivers[0].ringPrev == 3 && state.receivers[1].ringPrev == 1 &&
        state.receivers[2].ringPrev == 2, "Insertion is not before fixed ring head");
    Require(state.eventTypes[0].receiverIds == std::vector<Id>{1, 3} &&
        state.eventTypes[1].receiverIds == std::vector<Id>{2}, "Per-name append order changed");
    auto& first = state.receivers[0];
    first.callbackPending = true;
    first.eventState = 3;
    first.detected = true;
    first.previousScore = 17.25f;
    first.previousBestActor = "FixtureMap.Sender";
    first.historyCursor = 2;
    first.params = {"FixtureMap.Sender", 7.5f, 0.3f, 0.4f, 0.5f};
    const auto preserved = first;
    std::size_t used = 0;
    Require(Register(state, "fixturemap.a", "DANGER", "Replacement", "Score", {false, false, true, false}, used) == 1,
        "Existing registration replaced identity");
    auto expected = preserved;
    expected.callback = "Replacement";
    expected.scoreCallback = "Score";
    expected.flags = {false, false, true, false};
    Require(state.receivers[0] == expected, "Registration reset detection/history/callback data");
    Require(ClearReceiver(state, "FixtureMap.A", "Danger", used), "Clear711 failed");
    Require(!ClearReceiver(state, "FixtureMap.A", "Danger", used), "Deleted receiver returned by lookup");
    Require(state.pendingDeleteCount == 1 && state.receiverHead == 1 &&
        state.receivers[0].ringNext == 2, "Clear711 cleaned/decremented ring early");
    const auto deletedBefore = state.receivers[0];
    const Id replacement = AddReceiver(state, "FixtureMap.A", "Danger", "NewCallback");
    auto deletedAfterInsert = deletedBefore;
    deletedAfterInsert.ringPrev = 4;
    Require(replacement == 4 && state.receivers[0] == deletedAfterInsert &&
        state.eventTypes[0].receiverIds == std::vector<Id>{1, 3, 4},
        "Re-registration reused/overwrote deleted receiver");
    Require(state.receivers[2].ringNext == 4 && state.receivers[3].ringNext == 1 &&
        state.receivers[0].ringPrev == 4 && state.receiverHead == 1,
        "New receiver not inserted before retained deleted head");
    Require(state.receivers[3].historyCursor == 13 && !state.receivers[3].detected &&
        state.receivers[3].params == XAIParams{}, "Replacement did not get fresh detection defaults");
    AddSender(state, "FixtureMap.Sender", "Danger", 0, 2);
    AddSender(state, "FixtureMap.Sender", "Other", 1, 3, 400);
    AddReceiver(state, "FixtureMap.Sender", "Other");
    state.receivers[1].previousBestActor = "FixtureMap.Sender";
    state.receivers[1].params.bestActor = "FixtureMap.Sender";
    const auto ringBefore = state;
    DestroyActor(state, "fixturemap.sender", used);
    Require(state.senders[0].deleted && state.senders[1].deleted && state.receivers[4].deleted &&
        state.pendingDeleteCount == 4, "Destroy did not mark all owned live event nodes exactly once");
    Require(state.receivers[1].previousBestActor.empty() &&
        state.receivers[1].params.bestActor == "FixtureMap.Sender" &&
        state.receivers[0].previousBestActor == "FixtureMap.Sender",
        "Destroy previous-best clearing does not match live-only native field behavior");
    for (std::size_t index = 0; index < state.receivers.size(); ++index)
        Require(state.receivers[index].ringNext == ringBefore.receivers[index].ringNext &&
            state.receivers[index].ringPrev == ringBefore.receivers[index].ringPrev,
            "Destroy cleaned ring before deferred boundary");
    const auto destroyed = state;
    DestroyActor(state, "FixtureMap.Sender", used);
    Require(state == destroyed, "Repeated Destroy double-counted nodes");
    Require(!End(state, "FixtureMap.Sender", "Danger", 0, used), "End found deleted sender");
    Require(AddSender(state, "FixtureMap.Sender", "Danger", 2, 4) == 3 &&
        state.eventTypes[0].senderIds == std::vector<Id>{1, 3},
        "New sender reused tombstone instead of appending stable identity");
    Validate(state);

    auto aliased = Empty();
    AddReceiver(aliased, "FixtureMap.A", "Danger", "Callback", "Score");
    used = 0;
    ClearReceiver(aliased, "FixtureMap.A", "Danger", used);
    Register(aliased, aliased.receivers[0].actor, aliased.eventTypes[0].name,
        aliased.receivers[0].callback, aliased.receivers[0].scoreCallback, {}, used);
    Require(aliased.receivers[1].actor == "FixtureMap.A" && aliased.receivers[1].callback == "Callback" &&
        aliased.receivers[1].scoreCallback == "Score", "Aliased register input invalidated on vector growth");
    aliased.receivers[1].previousBestActor = "FixtureMap.A";
    DestroyActor(aliased, aliased.receivers[1].previousBestActor, used);
    Require(aliased.receivers[1].deleted && aliased.pendingDeleteCount == 2,
        "Aliased Destroy identity changed while clearing previous-best");
    Validate(aliased);
}

void RefusalsAndAccounting() {
    auto valid = Empty();
    AddReceiver(valid, "FixtureMap.A", "Danger");
    AddReceiver(valid, "FixtureMap.B", "Danger");
    AddSender(valid, "FixtureMap.Sender", "Danger", 1, 3, 400);
    auto bad = [&](const std::function<void(State&)>& mutate, const std::string& label) {
        auto state = valid;
        mutate(state);
        const auto before = state;
        Reject([&] { Validate(state); }, label);
        Require(state == before, "Validation mutated refused state: " + label);
    };
    bad([](State& s) { s.ownerPath.clear(); }, "Missing LevelInfo owner accepted");
    bad([](State& s) { s.levelPath.clear(); }, "Missing XLevel accepted");
    bad([](State& s) { s.ownerPath.assign(MaxStringBytes + 1, 'X'); }, "Oversized owner accepted");
    bad([](State& s) { s.eventTypes[0].name = std::string("Bad\0Name", 8); }, "NUL name accepted");
    bad([](State& s) { s.eventTypes[0].name = "None"; }, "None event entry accepted");
    bad([](State& s) { s.eventTypes.push_back(EventType{"DANGER", {}, {}}); }, "Case-duplicate event accepted");
    bad([](State& s) { s.eventTypes[0].senderIds[0] = 0; }, "Null sender list ID accepted");
    bad([](State& s) { s.eventTypes[0].receiverIds[0] = 3; }, "Out-of-range receiver ID accepted");
    bad([](State& s) { s.eventTypes[0].receiverIds.push_back(1); }, "Duplicate receiver list ID accepted");
    bad([](State& s) { s.eventTypes[0].senderIds.clear(); }, "Unlisted sender accepted");
    bad([](State& s) { s.eventTypes[0].receiverIds.pop_back(); }, "Unlisted receiver accepted");
    bad([](State& s) { s.senders[0].eventType = 2; }, "Sender type mismatch accepted");
    bad([](State& s) { s.receivers[0].eventType = 0; }, "Receiver type mismatch accepted");
    bad([](State& s) { s.receivers[1].actor = "FIXTUREMAP.A"; }, "Duplicate live receiver accepted");
    bad([](State& s) { s.senders[0].actor.clear(); }, "Null sender actor accepted");
    bad([](State& s) { s.receivers[0].callback.assign(MaxStringBytes + 1, 'C'); }, "Oversized callback accepted");
    bad([](State& s) { s.receivers[0].historyCursor = 16; }, "Invalid receiver cursor accepted");
    bad([](State& s) { s.historyCursor = 16; }, "Invalid manager cursor accepted");
    bad([](State& s) { s.receivers[0].eventState = 4; }, "Invalid callback state accepted");
    bad([](State& s) { s.receivers[0].ringNext = 0; }, "Null ring successor accepted");
    bad([](State& s) { s.receivers[0].ringPrev = 3; }, "Invalid ring predecessor accepted");
    bad([](State& s) { s.receiverHead = 0; }, "Missing ring head accepted");
    bad([](State& s) {
        s.receivers[0].ringNext = s.receivers[0].ringPrev = 1;
        s.receivers[1].ringNext = s.receivers[1].ringPrev = 2;
    }, "Disconnected reciprocal subrings accepted");
    bad([](State& s) { s.pendingDeleteCount = 1; }, "False deletion counter accepted");
    bad([](State& s) { s.receivers[0].deleted = true; }, "Uncounted deletion accepted");
    bad([](State& s) { s.senders[0].score = std::numeric_limits<float>::infinity(); }, "Infinite score accepted");
    bad([](State& s) { s.receivers[0].previousScore = std::numeric_limits<float>::infinity(); }, "Infinite previous score accepted");
    bad([](State& s) { s.receivers[0].params.volume = std::numeric_limits<float>::infinity(); }, "Infinite params accepted");
    for (std::size_t index = 0; index < 16; ++index)
        bad([index](State& s) { s.senders[0].history[index].radius = std::numeric_limits<float>::infinity(); },
            "Non-finite history slot accepted");
    bad([](State& s) { s.senders[0].actor.reserve(MaxStateBytes); }, "Oversized retained capacity accepted");
    {
        auto state = Empty();
        state.receiverHead = 1;
        Reject([&] { Validate(state); }, "Head without receivers accepted");
    }
    {
        auto state = valid;
        std::size_t used = MaxWork;
        Reject([&] { Emit(state, "FixtureMap.Sender", "Danger", 0, 8, 800, false, false, used); },
            "Exhausted shared native allowance accepted");
        Require(state == valid, "Allowance refusal changed emitter before work");
    }
    {
        auto state = valid;
        std::size_t used = 0;
        Reject([&] { Emit(state, "FixtureMap.Sender", "Danger", 0,
            std::numeric_limits<float>::quiet_NaN(), 800, false, false, used); }, "NaN emit accepted");
        Require(state == valid, "Scalar refusal mutated graph");
    }
    {
        auto state = Empty();
        state.eventTypes.push_back(EventType{"AtCapacity", {}, {}});
        for (std::size_t index = 0; index < MaxNodes; ++index) {
            Sender sender;
            sender.actor = "FixtureMap.Sender" + std::to_string(index);
            sender.eventType = 1;
            state.senders.push_back(std::move(sender));
            state.eventTypes[0].senderIds.push_back(static_cast<Id>(index + 1));
        }
        Validate(state);
        Require(AddSender(state, "FixtureMap.Sender0", "AtCapacity", 0, 2) == 1,
            "Existing sender update rejected at exact node capacity");
        const auto before = state;
        Reject([&] { AddSender(state, "FixtureMap.Extra", "AtCapacity"); }, "Extra sender exceeded node cap");
        Require(state == before, "Node cap refusal changed existing graph");
        Reject([&] { AddReceiver(state, "FixtureMap.Extra", "AtCapacity"); }, "Extra receiver exceeded shared cap");
    }
    {
        auto state = Empty();
        for (std::size_t index = 0; index < MaxEventTypes; ++index)
            state.eventTypes.push_back(EventType{"Type" + std::to_string(index), {}, {}});
        Validate(state);
        const auto before = state;
        Reject([&] { AddSender(state, "FixtureMap.Extra", "Overflow"); }, "Event type cap exceeded");
        Require(state == before, "Event type refusal changed graph");
    }
    auto snapshot = valid;
    std::size_t used = 0;
    Emit(valid, "FixtureMap.Sender", "Danger", 2, 8, 800, false, false, used);
    ClearReceiver(valid, "FixtureMap.A", "Danger", used);
    Require(!(valid == snapshot), "Native mutations did not change journal payload");
    valid = snapshot;
    Require(valid == snapshot, "Complete-state rollback did not restore native graph");
    Validate(valid);
}
} // namespace

int main() {
    try {
        GuardsAndDefaults();
        SenderChannels();
        ReceiverRingAndLifecycle();
        RefusalsAndAccounting();
        std::cout << "AI event mutation state: " << checks << " checks, " << rejections
                  << " explicit refusals passed. Processing remains unsupported.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "AI event mutation test failed: " << error.what() << '\n';
        return 1;
    }
}
