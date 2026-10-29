#pragma once

#include "quest_portable_vm.h"

#include <string_view>

namespace QuestVr::NativeWindowStore {

// Eligibility only, NOT equality or native write authority. The caller must
// already have the actual native Window/property, validated/coerced incoming
// storage and a lower-case qualified property key. It must still compare the
// COMPLETE current/incoming Value with its storage-identity comparator before
// eliding an ordinary store. Inactive payloads, float/NaN bits, signed zero and
// Name spelling must not be discarded through script equality or this filter.
// UObject headers and owning/nontrivial native reference/composite stores are
// deliberately excluded; their lifecycle/GC/override work is never skipped.
inline bool IsPlainStoreCandidate(std::string_view loweredPropertyKey,const Vm::Value& incoming) noexcept {
    if(loweredPropertyKey.empty()||loweredPropertyKey=="core.object"||loweredPropertyKey.starts_with("core.object."))return false;
    switch(incoming.kind) {
    case Vm::Kind::Byte:
    case Vm::Kind::Int:
    case Vm::Kind::Bool:
    case Vm::Kind::Float:
    case Vm::Kind::String:
    case Vm::Kind::Name:return true;
    case Vm::Kind::Object:return incoming.text.empty(); // Actual None spelling, not the literal "None".
    default:return false;
    }
}

} // namespace QuestVr::NativeWindowStore
