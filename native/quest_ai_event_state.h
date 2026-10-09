#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

namespace QuestVr::Ai {

// Portable identities, not the original PE32 layout. Zero is the null ID; the
// remaining IDs are stable one-based positions and are never reused by mutation.
using Id = std::uint32_t;
inline constexpr std::size_t MaxNodes = 8192;
inline constexpr std::size_t MaxEventTypes = 1024;
inline constexpr std::size_t MaxStringBytes = 8192;
inline constexpr std::size_t MaxStateBytes = 16 * 1024 * 1024;
inline constexpr std::size_t MaxWork = 65536;

struct Channels {
    float visibility = 0;
    float volume = 0;
    float radius = 0;
    float smell = 0;
    bool operator==(const Channels&) const = default;
};

struct PerceptionFlags {
    bool checkVisibility = true;
    bool checkDirection = true;
    bool checkCylinder = false;
    bool checkLineOfSight = true;
    bool operator==(const PerceptionFlags&) const = default;
};

struct XAIParams {
    std::string bestActor;
    float score = 0;
    float visibility = 0;
    float volume = 0;
    float smell = 0;
    bool operator==(const XAIParams&) const = default;
};

struct EventType {
    // First registered spelling is retained. Lists include pending deletions.
    std::string name;
    std::vector<Id> senderIds;
    std::vector<Id> receiverIds;
    bool operator==(const EventType&) const = default;
};

struct Sender {
    std::string actor;
    Id eventType = 0;
    bool deleted = false;
    float score = 0;
    std::array<Channels, 16> history{};
    Channels current;
    bool operator==(const Sender&) const = default;
};

struct Receiver {
    std::string actor;
    Id eventType = 0;
    bool deleted = false;
    std::string callback;
    std::string scoreCallback;
    PerceptionFlags flags;
    bool callbackPending = false;
    std::uint8_t eventState = 0;
    bool detected = false;
    float previousScore = 0;
    std::string previousBestActor;
    std::uint8_t historyCursor = 0;
    XAIParams params;
    Id ringNext = 0;
    Id ringPrev = 0;
    bool operator==(const Receiver&) const = default;
};

struct State {
    std::string ownerPath;
    std::string levelPath;
    std::uint32_t processDepth = 0;
    std::uint32_t pendingDeleteCount = 0;
    std::uint8_t historyCursor = 0;
    Id receiverHead = 0;
    std::vector<EventType> eventTypes;
    std::vector<Sender> senders;
    std::vector<Receiver> receivers;
    bool operator==(const State&) const = default;
};

// The owner journals the complete State with the enclosing VM transaction.
// These routines do not deliver callbacks, advance history, or clean tombstones.
// Work is charged to the caller's shared native/lifecycle allowance.
inline Id Register(State&, const std::string& actor, const std::string& eventName,
                   const std::string& callback, const std::string& scoreCallback,
                   PerceptionFlags, std::size_t& used);
inline bool ClearReceiver(State&, const std::string& actor,
                          const std::string& eventName, std::size_t& used);
inline Id Emit(State&, const std::string& actor, const std::string& eventName,
               std::uint8_t channel, float value, float radius, bool pulse,
               bool pendingKill, std::size_t& used);
inline bool End(State&, const std::string& actor, const std::string& eventName,
                std::uint8_t channel, std::size_t& used);
inline bool ClearSender(State&, const std::string& actor,
                        const std::string& eventName, std::size_t& used);
inline void DestroyActor(State&, const std::string& actor, std::size_t& used);
inline std::size_t RetainedBytes(const State&);
inline void Validate(const State&, std::size_t& used);
inline void Validate(const State& state) {
    std::size_t used = 0;
    Validate(state, used);
}

namespace Detail {

[[noreturn]] inline void Fail(const char* message) {
    throw std::runtime_error(std::string("AI event state: ") + message);
}

inline void Step(std::size_t& used, std::size_t count = 1) {
    if (used > MaxWork || count > MaxWork - used) Fail("work limit exceeded");
    used += count;
}

inline void Text(std::string_view text, bool required = false) {
    if (text.size() > MaxStringBytes || text.find('\0') != std::string_view::npos ||
        (required && text.empty())) Fail("invalid identity or name");
}

inline char Fold(char character) {
    return character >= 'A' && character <= 'Z'
        ? static_cast<char>(character + ('a' - 'A')) : character;
}

inline bool Equal(std::string_view first, std::string_view second) {
    if (first.size() != second.size()) return false;
    for (std::size_t index = 0; index < first.size(); ++index)
        if (Fold(first[index]) != Fold(second[index])) return false;
    return true;
}

inline std::string Folded(std::string text) {
    for (char& character : text) character = Fold(character);
    return text;
}

inline bool None(std::string_view name) {
    return name.empty() || Equal(name, "None");
}

inline void AddBytes(std::size_t& bytes, std::size_t count,
                     std::size_t elementSize = 1) {
    if (bytes > MaxStateBytes || count > (MaxStateBytes - bytes) / elementSize)
        Fail("retained byte limit exceeded");
    bytes += count * elementSize;
}

inline void Scalar(float value) {
    if (!std::isfinite(value)) Fail("non-finite scalar");
}

inline void Sample(const Channels& sample) {
    Scalar(sample.visibility);
    Scalar(sample.volume);
    Scalar(sample.radius);
    Scalar(sample.smell);
}

inline void Envelope(const State& state, std::size_t& used) {
    if (state.eventTypes.size() > MaxEventTypes ||
        state.senders.size() > MaxNodes ||
        state.receivers.size() > MaxNodes - state.senders.size())
        Fail("node limit exceeded");
    Step(used, 1 + state.eventTypes.size() + state.senders.size() + state.receivers.size());
    (void)RetainedBytes(state);
    if (state.historyCursor >= 16) Fail("invalid history cursor");
}

inline Id FindType(const State& state, std::string_view name, std::size_t& used) {
    for (std::size_t index = 0; index < state.eventTypes.size(); ++index) {
        Step(used);
        if (Equal(state.eventTypes[index].name, name)) return static_cast<Id>(index + 1);
    }
    return 0;
}

inline Id Type(State& state, const std::string& name, std::size_t& used) {
    if (const Id id = FindType(state, name, used)) return id;
    if (state.eventTypes.size() >= MaxEventTypes) Fail("event type limit exceeded");
    state.eventTypes.push_back(EventType{name, {}, {}});
    return static_cast<Id>(state.eventTypes.size());
}

inline Sender& SenderAt(State& state, Id id) {
    if (!id || id > state.senders.size()) Fail("invalid sender ID");
    return state.senders[id - 1];
}

inline Receiver& ReceiverAt(State& state, Id id) {
    if (!id || id > state.receivers.size()) Fail("invalid receiver ID");
    return state.receivers[id - 1];
}

inline Id FindSender(State& state, Id type, std::string_view actor, std::size_t& used) {
    for (Id id : state.eventTypes[type - 1].senderIds) {
        Step(used);
        const auto& node = SenderAt(state, id);
        if (!node.deleted && Equal(node.actor, actor)) return id;
    }
    return 0;
}

inline Id FindReceiver(State& state, Id type, std::string_view actor, std::size_t& used) {
    for (Id id : state.eventTypes[type - 1].receiverIds) {
        Step(used);
        const auto& node = ReceiverAt(state, id);
        if (!node.deleted && Equal(node.actor, actor)) return id;
    }
    return 0;
}

inline void Mark(State& state, bool& deleted) {
    if (!deleted) {
        if (state.pendingDeleteCount == std::numeric_limits<std::uint32_t>::max())
            Fail("deletion counter overflow");
        ++state.pendingDeleteCount;
        deleted = true;
    }
}

inline void WriteChannel(State& state, Sender& sender, std::uint8_t channel,
                         float value, float radius, bool pulse) {
    auto write = [pulse](float& history, float& current, float next) {
        if (history < next) history = next;
        if (!pulse) current = next;
    };
    auto& history = sender.history[state.historyCursor];
    switch (channel) {
    case 0: write(history.visibility, sender.current.visibility, value); break;
    case 1:
        write(history.volume, sender.current.volume, value);
        write(history.radius, sender.current.radius, radius);
        break;
    case 2: write(history.smell, sender.current.smell, value); break;
    default: break; // The original wrapper masks to a byte, not the enum range.
    }
}

} // namespace Detail

inline std::size_t RetainedBytes(const State& state) {
    std::size_t bytes = sizeof(State);
    Detail::AddBytes(bytes, state.ownerPath.capacity() + 1);
    Detail::AddBytes(bytes, state.levelPath.capacity() + 1);
    Detail::AddBytes(bytes, state.eventTypes.capacity(), sizeof(EventType));
    Detail::AddBytes(bytes, state.senders.capacity(), sizeof(Sender));
    Detail::AddBytes(bytes, state.receivers.capacity(), sizeof(Receiver));
    for (const auto& event : state.eventTypes) {
        Detail::AddBytes(bytes, event.name.capacity() + 1);
        Detail::AddBytes(bytes, event.senderIds.capacity(), sizeof(Id));
        Detail::AddBytes(bytes, event.receiverIds.capacity(), sizeof(Id));
    }
    for (const auto& sender : state.senders)
        Detail::AddBytes(bytes, sender.actor.capacity() + 1);
    for (const auto& receiver : state.receivers) {
        Detail::AddBytes(bytes, receiver.actor.capacity() + 1);
        Detail::AddBytes(bytes, receiver.callback.capacity() + 1);
        Detail::AddBytes(bytes, receiver.scoreCallback.capacity() + 1);
        Detail::AddBytes(bytes, receiver.previousBestActor.capacity() + 1);
        Detail::AddBytes(bytes, receiver.params.bestActor.capacity() + 1);
    }
    return bytes;
}

inline Id Register(State& state, const std::string& actor, const std::string& eventName,
                   const std::string& callback, const std::string& scoreCallback,
                   PerceptionFlags flags, std::size_t& used) {
    Detail::Text(actor);
    Detail::Text(eventName);
    Detail::Text(callback);
    Detail::Text(scoreCallback);
    Detail::Envelope(state, used);
    if (actor.empty() || Detail::None(eventName)) return 0;
    // Inputs may refer to strings in State; appending nodes can relocate them.
    const std::string actorIdentity = actor;
    const std::string eventIdentity = eventName;
    const std::string callbackName = callback;
    const std::string scoreName = scoreCallback;
    Id type = Detail::FindType(state, eventIdentity, used);
    Id id = type ? Detail::FindReceiver(state, type, actorIdentity, used) : 0;
    if (!id) {
        if (state.senders.size() + state.receivers.size() >= MaxNodes)
            Detail::Fail("node limit exceeded");
        if (!type) type = Detail::Type(state, eventIdentity, used);
        Receiver receiver;
        receiver.actor = actorIdentity;
        receiver.eventType = type;
        receiver.historyCursor = state.historyCursor;
        id = static_cast<Id>(state.receivers.size() + 1);
        if (state.receiverHead) {
            auto& head = Detail::ReceiverAt(state, state.receiverHead);
            receiver.ringNext = state.receiverHead;
            receiver.ringPrev = head.ringPrev;
            Detail::ReceiverAt(state, head.ringPrev).ringNext = id;
            head.ringPrev = id;
        } else {
            receiver.ringNext = id;
            receiver.ringPrev = id;
            state.receiverHead = id;
        }
        state.receivers.push_back(std::move(receiver));
        state.eventTypes[type - 1].receiverIds.push_back(id);
    }
    auto& receiver = Detail::ReceiverAt(state, id);
    receiver.callback = callbackName;
    receiver.scoreCallback = scoreName;
    receiver.flags = flags;
    (void)RetainedBytes(state);
    return id;
}

inline bool ClearReceiver(State& state, const std::string& actor,
                          const std::string& eventName, std::size_t& used) {
    Detail::Text(actor);
    Detail::Text(eventName);
    Detail::Envelope(state, used);
    if (actor.empty() || Detail::None(eventName)) return false;
    const Id type = Detail::FindType(state, eventName, used);
    const Id id = type ? Detail::FindReceiver(state, type, actor, used) : 0;
    if (!id) return false;
    Detail::Mark(state, Detail::ReceiverAt(state, id).deleted);
    return true;
}

inline Id Emit(State& state, const std::string& actor, const std::string& eventName,
               std::uint8_t channel, float value, float radius, bool pulse,
               bool pendingKill, std::size_t& used) {
    Detail::Text(actor);
    Detail::Text(eventName);
    Detail::Scalar(value);
    Detail::Scalar(radius);
    Detail::Envelope(state, used);
    if (actor.empty() || Detail::None(eventName)) return 0;
    const std::string actorIdentity = actor;
    const std::string eventIdentity = eventName;
    Id type = Detail::FindType(state, eventIdentity, used);
    Id id = type ? Detail::FindSender(state, type, actorIdentity, used) : 0;
    if (!id) {
        if (state.senders.size() + state.receivers.size() >= MaxNodes)
            Detail::Fail("node limit exceeded");
        if (!type) type = Detail::Type(state, eventIdentity, used);
        Sender sender;
        sender.actor = actorIdentity;
        sender.eventType = type;
        state.senders.push_back(std::move(sender));
        id = static_cast<Id>(state.senders.size());
        state.eventTypes[type - 1].senderIds.push_back(id);
    }
    if (pendingKill) { value = 0; radius = 0; }
    Detail::WriteChannel(state, Detail::SenderAt(state, id), channel, value, radius, pulse);
    (void)RetainedBytes(state);
    return id;
}

inline bool End(State& state, const std::string& actor, const std::string& eventName,
                std::uint8_t channel, std::size_t& used) {
    Detail::Text(actor);
    Detail::Text(eventName);
    Detail::Envelope(state, used);
    if (actor.empty() || Detail::None(eventName)) return false;
    const Id type = Detail::FindType(state, eventName, used);
    const Id id = type ? Detail::FindSender(state, type, actor, used) : 0;
    if (!id) return false;
    Detail::WriteChannel(state, Detail::SenderAt(state, id), channel, 0, 0, false);
    return true;
}

inline bool ClearSender(State& state, const std::string& actor,
                        const std::string& eventName, std::size_t& used) {
    Detail::Text(actor);
    Detail::Text(eventName);
    Detail::Envelope(state, used);
    if (actor.empty() || Detail::None(eventName)) return false;
    const Id type = Detail::FindType(state, eventName, used);
    const Id id = type ? Detail::FindSender(state, type, actor, used) : 0;
    if (!id) return false;
    auto& sender = Detail::SenderAt(state, id);
    Detail::WriteChannel(state, sender, 1, 0, 0, false);
    Detail::WriteChannel(state, sender, 0, 0, 0, false);
    Detail::WriteChannel(state, sender, 2, 0, 0, false);
    return true;
}

inline void DestroyActor(State& state, const std::string& actor, std::size_t& used) {
    Detail::Text(actor);
    Detail::Envelope(state, used);
    if (actor.empty()) return;
    const std::string actorIdentity = actor;
    for (auto& sender : state.senders) {
        Detail::Step(used);
        if (!sender.deleted && Detail::Equal(sender.actor, actorIdentity))
            Detail::Mark(state, sender.deleted);
    }
    for (auto& receiver : state.receivers) {
        Detail::Step(used);
        if (receiver.deleted) continue;
        if (Detail::Equal(receiver.previousBestActor, actorIdentity))
            receiver.previousBestActor.clear();
        if (Detail::Equal(receiver.actor, actorIdentity)) Detail::Mark(state, receiver.deleted);
    }
}

inline void Validate(const State& state, std::size_t& used) {
    Detail::Text(state.ownerPath, true);
    Detail::Text(state.levelPath, true);
    Detail::Envelope(state, used);
    std::vector<bool> seenSenders(state.senders.size());
    std::vector<bool> seenReceivers(state.receivers.size());
    std::unordered_set<std::string> names;
    for (std::size_t typeIndex = 0; typeIndex < state.eventTypes.size(); ++typeIndex) {
        const auto& type = state.eventTypes[typeIndex];
        Detail::Text(type.name, true);
        if (Detail::None(type.name) || !names.insert(Detail::Folded(type.name)).second)
            Detail::Fail("duplicate or None event type");
        if (type.senderIds.size() > state.senders.size() ||
            type.receiverIds.size() > state.receivers.size())
            Detail::Fail("invalid event list size");
        std::unordered_set<std::string> liveSenders;
        for (Id id : type.senderIds) {
            Detail::Step(used);
            if (!id || id > state.senders.size() || seenSenders[id - 1])
                Detail::Fail("duplicate or invalid sender list ID");
            seenSenders[id - 1] = true;
            const auto& sender = state.senders[id - 1];
            if (sender.eventType != typeIndex + 1)
                Detail::Fail("sender event type mismatch");
            if (!sender.deleted && !liveSenders.insert(Detail::Folded(sender.actor)).second)
                Detail::Fail("duplicate live sender identity");
        }
        std::unordered_set<std::string> liveReceivers;
        for (Id id : type.receiverIds) {
            Detail::Step(used);
            if (!id || id > state.receivers.size() || seenReceivers[id - 1])
                Detail::Fail("duplicate or invalid receiver list ID");
            seenReceivers[id - 1] = true;
            const auto& receiver = state.receivers[id - 1];
            if (receiver.eventType != typeIndex + 1)
                Detail::Fail("receiver event type mismatch");
            if (!receiver.deleted && !liveReceivers.insert(Detail::Folded(receiver.actor)).second)
                Detail::Fail("duplicate live receiver identity");
        }
    }
    std::uint32_t deleted = 0;
    for (std::size_t index = 0; index < state.senders.size(); ++index) {
        Detail::Step(used);
        const auto& sender = state.senders[index];
        Detail::Text(sender.actor, true);
        if (!seenSenders[index]) Detail::Fail("sender omitted from event list");
        Detail::Scalar(sender.score);
        Detail::Sample(sender.current);
        for (const auto& sample : sender.history) Detail::Sample(sample);
        if (sender.deleted) ++deleted;
    }
    for (std::size_t index = 0; index < state.receivers.size(); ++index) {
        Detail::Step(used);
        const auto& receiver = state.receivers[index];
        Detail::Text(receiver.actor, true);
        Detail::Text(receiver.callback);
        Detail::Text(receiver.scoreCallback);
        Detail::Text(receiver.previousBestActor);
        Detail::Text(receiver.params.bestActor);
        if (!seenReceivers[index]) Detail::Fail("receiver omitted from event list");
        if (receiver.eventState > 3 || receiver.historyCursor >= 16)
            Detail::Fail("invalid receiver detection or history state");
        Detail::Scalar(receiver.previousScore);
        Detail::Scalar(receiver.params.score);
        Detail::Scalar(receiver.params.visibility);
        Detail::Scalar(receiver.params.volume);
        Detail::Scalar(receiver.params.smell);
        if (!receiver.ringNext || receiver.ringNext > state.receivers.size() ||
            !receiver.ringPrev || receiver.ringPrev > state.receivers.size() ||
            state.receivers[receiver.ringNext - 1].ringPrev != index + 1 ||
            state.receivers[receiver.ringPrev - 1].ringNext != index + 1)
            Detail::Fail("invalid receiver ring links");
        if (receiver.deleted) ++deleted;
    }
    if (deleted != state.pendingDeleteCount) Detail::Fail("pending deletion count mismatch");
    if (state.receivers.empty()) {
        if (state.receiverHead) Detail::Fail("head without receivers");
        return;
    }
    if (!state.receiverHead || state.receiverHead > state.receivers.size())
        Detail::Fail("invalid receiver head");
    Id cursor = state.receiverHead;
    std::vector<bool> seenRing(state.receivers.size());
    for (std::size_t count = 0; count < state.receivers.size(); ++count) {
        Detail::Step(used);
        if (seenRing[cursor - 1]) Detail::Fail("disconnected receiver ring");
        seenRing[cursor - 1] = true;
        cursor = state.receivers[cursor - 1].ringNext;
    }
    if (cursor != state.receiverHead) Detail::Fail("open receiver ring");
}

} // namespace QuestVr::Ai
