#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace QuestVr {

// The currently playable native port aggregates the collected NanoKey actors
// in its persistent inventory. This component identity denotes that functional
// ring; it is not a claim that the unfinished original PlayerPawn owns a UObject.
inline constexpr const char* kQuickBeltKeyRingIdentity = "QuestVR.NanoKeyRing";

struct QuickBeltItem {
    std::string actorPath;
    bool keyRing{};
    bool eligible{true};
};

struct QuickBeltState {
    // Original logical positions: 1..9, followed visually by special slot 0.
    std::array<std::string,10> slots;
    std::string equippedActorPath;
    bool operator==(const QuickBeltState&) const = default;
};

namespace QuickBeltDetail {
inline std::string Fold(const std::string& identity) {
    std::string result = identity;
    for (auto& character : result)
        if (character >= 'A' && character <= 'Z') character = static_cast<char>(character-'A'+'a');
    return result;
}
inline bool ValidIdentity(const std::string& identity, bool empty = false) {
    return (empty || !identity.empty()) && identity.size() <= 4096u &&
        identity.find('\0') == std::string::npos;
}
inline void ValidateStateShape(const QuickBeltState& state) {
    std::vector<std::string> seen;
    for (const auto& identity : state.slots) {
        if (!ValidIdentity(identity,true)) throw std::runtime_error("Quick belt save identity exceeds bounds");
        if (identity.empty()) continue;
        const auto folded = Fold(identity);
        if (std::find(seen.begin(),seen.end(),folded) != seen.end())
            throw std::runtime_error("Quick belt save has duplicate assignments");
        seen.push_back(folded);
    }
    if (!ValidIdentity(state.equippedActorPath,true))
        throw std::runtime_error("Quick belt equipped identity exceeds bounds");
}
inline std::vector<QuickBeltItem> ValidateOwned(const std::vector<QuickBeltItem>& owned) {
    if (owned.size() > 1024u) throw std::runtime_error("Quick belt owned inventory exceeds bounds");
    std::vector<std::string> seen;
    bool ring{};
    for (const auto& item : owned) {
        if (!ValidIdentity(item.actorPath)) throw std::runtime_error("Quick belt owned identity is invalid");
        const auto folded = Fold(item.actorPath);
        if (std::find(seen.begin(),seen.end(),folded) != seen.end())
            throw std::runtime_error("Quick belt owned identity is ambiguous");
        seen.push_back(folded);
        if (item.keyRing && ring) throw std::runtime_error("Quick belt has more than one owned keyring");
        ring = ring || item.keyRing;
    }
    return owned;
}
inline const QuickBeltItem* Find(const std::vector<QuickBeltItem>& owned,const std::string& identity) {
    if (identity.empty()) return nullptr;
    const auto folded = Fold(identity);
    const auto found = std::find_if(owned.begin(),owned.end(),[&](const auto& item) {
        return Fold(item.actorPath) == folded;
    });
    return found == owned.end() ? nullptr : &*found;
}
inline bool Usable(const QuickBeltItem* item) { return item && (item->keyRing || item->eligible); }
}

// Equipment selection belongs to this model, not the Persona browsing cursor.
// It never creates or consumes inventory or keys, and never guesses classes from
// names. The runtime supplies real ownership and eligibility for each item.
class QuickBeltModel {
public:
    bool Reconcile(const std::vector<QuickBeltItem>& owned) {
        auto checked = QuickBeltDetail::ValidateOwned(owned);
        auto next = state_;
        ReconcileState(next,checked);
        const bool changed = next != state_;
        state_ = std::move(next);
        owned_ = std::move(checked);
        return changed;
    }

    void Restore(const QuickBeltState& state,const std::vector<QuickBeltItem>& owned) {
        QuickBeltDetail::ValidateStateShape(state);
        auto checked = QuickBeltDetail::ValidateOwned(owned);
        auto next = state;
        for (std::size_t slot = 0u; slot < next.slots.size(); ++slot) {
            if (next.slots[slot].empty()) continue;
            const auto* item = QuickBeltDetail::Find(checked,next.slots[slot]);
            if (!QuickBeltDetail::Usable(item) || item->keyRing != (slot == 0u))
                throw std::runtime_error("Quick belt save references an unowned or ineligible slot item");
            next.slots[slot] = item->actorPath;
        }
        if (!next.equippedActorPath.empty()) {
            const auto* item = QuickBeltDetail::Find(checked,next.equippedActorPath);
            if (!QuickBeltDetail::Usable(item))
                throw std::runtime_error("Quick belt save equips an unowned or ineligible item");
            next.equippedActorPath = item->actorPath;
        }
        ReconcileState(next,checked);
        state_ = std::move(next);
        owned_ = std::move(checked);
    }

    bool Equip(const std::string& actorPath) {
        const auto* item = QuickBeltDetail::Find(owned_,actorPath);
        if (!QuickBeltDetail::Usable(item)) return false;
        auto next = state_;
        EnsureAssigned(next,*item);
        next.equippedActorPath = item->actorPath;
        state_ = std::move(next);
        return true;
    }

    bool SelectSlot(std::size_t slot) {
        if (slot >= state_.slots.size() || state_.slots[slot].empty()) return false;
        return Equip(state_.slots[slot]);
    }

    bool Cycle(int steps) {
        if (steps == 0) return false;
        std::vector<std::size_t> occupied;
        for (std::size_t ordinal = 0u; ordinal < 10u; ++ordinal) {
            const auto slot = (ordinal+1u)%10u;
            if (!state_.slots[slot].empty()) occupied.push_back(slot);
        }
        if (occupied.empty()) return false;
        const auto selected = SlotFor(state_.equippedActorPath);
        const auto found = std::find(occupied.begin(),occupied.end(),selected.value_or(10u));
        const auto count = static_cast<std::int64_t>(occupied.size());
        std::int64_t ordinal = found == occupied.end()
            ? (steps > 0 ? -1 : 0) : static_cast<std::int64_t>(found-occupied.begin());
        ordinal = ((ordinal+static_cast<std::int64_t>(steps))%count+count)%count;
        return SelectSlot(occupied[static_cast<std::size_t>(ordinal)]);
    }

    std::optional<std::size_t> SlotFor(const std::string& actorPath) const {
        if (actorPath.empty()) return std::nullopt;
        const auto folded = QuickBeltDetail::Fold(actorPath);
        for (std::size_t slot = 0u; slot < state_.slots.size(); ++slot)
            if (!state_.slots[slot].empty() && QuickBeltDetail::Fold(state_.slots[slot]) == folded) return slot;
        return std::nullopt;
    }

    const QuickBeltState& State() const { return state_; }
    const std::string& EquippedActorPath() const { return state_.equippedActorPath; }
    void Reset() { state_ = {}; owned_.clear(); }

private:
    QuickBeltState state_;
    std::vector<QuickBeltItem> owned_;

    static void EnsureAssigned(QuickBeltState& state,const QuickBeltItem& item) {
        if (item.keyRing) { state.slots[0] = item.actorPath; return; }
        if (std::find(state.slots.begin()+1,state.slots.end(),item.actorPath) != state.slots.end()) return;
        const auto empty = std::find(state.slots.begin()+1,state.slots.end(),std::string{});
        if (empty != state.slots.end()) { *empty = item.actorPath; return; }
        // Equipping an overflow item replaces the selected ordinary shortcut,
        // or slot9 if the ring is selected. Displaced items remain owned.
        const auto selected = std::find(state.slots.begin()+1,state.slots.end(),state.equippedActorPath);
        if (selected != state.slots.end()) *selected = item.actorPath;
        else state.slots[9] = item.actorPath;
    }

    static void ReconcileState(QuickBeltState& state,const std::vector<QuickBeltItem>& owned) {
        state.slots[0].clear();
        for (const auto& item : owned) if (item.keyRing) state.slots[0] = item.actorPath;
        for (std::size_t slot = 1u; slot < state.slots.size(); ++slot) {
            const auto* item = QuickBeltDetail::Find(owned,state.slots[slot]);
            if (!item || item->keyRing || !item->eligible) state.slots[slot].clear();
            else state.slots[slot] = item->actorPath;
        }
        for (const auto& item : owned) {
            if (item.keyRing || !item.eligible ||
                std::find(state.slots.begin()+1,state.slots.end(),item.actorPath) != state.slots.end()) continue;
            const auto empty = std::find(state.slots.begin()+1,state.slots.end(),std::string{});
            if (empty != state.slots.end()) *empty = item.actorPath;
        }
        if (const auto* item = QuickBeltDetail::Find(owned,state.equippedActorPath);
            QuickBeltDetail::Usable(item)) {
            state.equippedActorPath = item->actorPath;
            EnsureAssigned(state,*item);
        } else {
            state.equippedActorPath = state.slots[0];
            if (state.equippedActorPath.empty())
                for (std::size_t slot = 1u; slot < state.slots.size(); ++slot)
                    if (!state.slots[slot].empty()) { state.equippedActorPath = state.slots[slot]; break; }
        }
    }
};

inline std::vector<std::uint8_t> EncodeQuickBeltState(const QuickBeltState& state) {
    QuickBeltDetail::ValidateStateShape(state);
    std::vector<std::uint8_t> bytes{'Q','V','R','B','E','L','T',1u};
    const auto append = [&](const std::string& value) {
        const auto length = static_cast<std::uint32_t>(value.size());
        for (unsigned shift = 0u; shift < 32u; shift += 8u)
            bytes.push_back(static_cast<std::uint8_t>((length>>shift)&255u));
        bytes.insert(bytes.end(),value.begin(),value.end());
    };
    for (const auto& slot : state.slots) append(slot);
    append(state.equippedActorPath);
    return bytes;
}

inline QuickBeltState DecodeQuickBeltState(const std::vector<std::uint8_t>& bytes) {
    constexpr std::array<std::uint8_t,8> magic{'Q','V','R','B','E','L','T',1u};
    if (bytes.size() < magic.size()+44u || bytes.size() > magic.size()+44u+11u*4096u ||
        !std::equal(magic.begin(),magic.end(),bytes.begin()))
        throw std::runtime_error("Quick belt save header or size is invalid");
    std::size_t position = magic.size();
    const auto read = [&]() {
        if (position+4u > bytes.size()) throw std::runtime_error("Quick belt save is truncated");
        std::uint32_t size{};
        for (unsigned shift = 0u; shift < 32u; shift += 8u)
            size |= static_cast<std::uint32_t>(bytes[position++])<<shift;
        if (size > 4096u || size > bytes.size()-position)
            throw std::runtime_error("Quick belt save string is truncated or oversized");
        std::string value(bytes.begin()+static_cast<std::ptrdiff_t>(position),
            bytes.begin()+static_cast<std::ptrdiff_t>(position+size));
        position += size;
        return value;
    };
    QuickBeltState state;
    for (auto& slot : state.slots) slot = read();
    state.equippedActorPath = read();
    if (position != bytes.size()) throw std::runtime_error("Quick belt save has trailing data");
    QuickBeltDetail::ValidateStateShape(state);
    return state;
}

} // namespace QuestVr
