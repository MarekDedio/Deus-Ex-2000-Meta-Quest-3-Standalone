#pragma once

#include "quest_unreal_string.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>

namespace QuestVr::ObjectNames {

inline constexpr std::size_t MaximumNameUnits = 62u;
inline constexpr std::size_t MaximumClassNameBytes = 8192u;
inline constexpr std::size_t MaximumCandidates = 1'000'000u;

// Retail GOTY Core.dll MakeUniqueObjectName: thunk1010263a -> body101575b0.
// 1015762e--10157650 strips trailing ASCII digits from the concrete UClass
// name. 10157655--1015766b consumes and increments its native +0x488 counter
// BEFORE checking the candidate; UClass.Serialize resets it to0 at101282d9.
// The suffix is signed decimal "%i" (UTF16 literal101af8d0). The appStrncpy
// count63-suffixLength and its terminator at count-1 (101233da) leave at most
// 62 UTF16 units. Splitting a surrogate pair is intentional, not scalar slicing.
// 101576bc--101576d2 checks StaticFindObject(nullptr,actualOuter,name,false):
// the callback must report ANY class with that name in the caller's Outer.
//
// A counter belongs to a concrete UClass, not an Outer or an instance. It is
// consumed for every candidate, including occupied names and callback throws.
// This helper does NOT roll it back on failure: the enclosing transaction must
// journal the counter along with allocations/indexes. Overflow and work limits
// are explicit portable refusals, never undefined signed arithmetic or wrapping.
template<typename Collision>
std::string MakeUnique(const std::string_view concreteClassName,
    std::int32_t& counter, Collision&& collision,
    const std::size_t maximumCandidates = MaximumCandidates) {
    if (concreteClassName.empty() || concreteClassName.size() > MaximumClassNameBytes ||
        concreteClassName.find('\0') != std::string_view::npos)
        throw std::runtime_error("Unique UObject class name is empty, contains NUL or exceeds its byte bound");
    if (maximumCandidates == 0u || maximumCandidates > MaximumCandidates)
        throw std::runtime_error("Unique UObject name candidate work bound is invalid");
    if (counter < 0)
        throw std::runtime_error("Unique UObject name counter is outside its supported nonnegative range");

    auto base = UnrealString::Decode(concreteClassName);
    while (!base.empty() && base.back() >= u'0' && base.back() <= u'9') base.pop_back();
    for (std::size_t attempt = 0u; attempt < maximumCandidates; ++attempt) {
        if (counter < 0 || counter == std::numeric_limits<std::int32_t>::max())
            throw std::runtime_error("Unique UObject name counter overflow");
        const auto suffixIndex = counter++;
        const auto suffix = std::to_string(suffixIndex);
        const auto prefixUnits = std::min(base.size(), MaximumNameUnits - suffix.size());
        auto candidate = UnrealString::Encode(std::u16string_view(base).substr(0u, prefixUnits));
        candidate += suffix;
        if (!collision(std::string_view(candidate))) return candidate;
    }
    throw std::runtime_error("Unique UObject name candidate work bound exceeded");
}

} // namespace QuestVr::ObjectNames
