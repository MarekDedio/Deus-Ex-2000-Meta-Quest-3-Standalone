#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace QuestVr {

// The caller validates the Array property tag and its IntProperty inner schema.
// Pinned SurrealEngine 677ee14c5b83486e6634687953779aafb7973ad6:
// UArrayProperty::LoadValue reads a CompactIndex count, followed by each inner
// UIntProperty::LoadStructMemberValue's signed LE32 value. No per-item tags.
// Unlike a save/checkpoint codec, authored CompactIndex is sign/magnitude and
// accepts nonminimal encodings, including signed zero. Invalid/unsafe streams
// are rejected before allocating the result; the input must be exactly one tag
// payload, not a larger stream containing subsequent properties.
inline std::vector<std::int32_t> DecodeAuthoredIntArray(
    const std::vector<std::uint8_t>& bytes, std::size_t elementCap=262144u) {
    const auto fail=[](const char* message) -> void {
        throw std::runtime_error(std::string("Authored IntArray: ")+message);
    };
    std::size_t position{};
    const auto readByte=[&]() -> std::uint8_t {
        if (position==bytes.size()) fail("truncated CompactIndex count");
        return bytes[position++];
    };
    const auto first=readByte();
    const bool negative=(first&0x80u)!=0u;
    bool more=(first&0x40u)!=0u;
    std::uint64_t magnitude=first&0x3fu;
    for (unsigned shift=6u; more; shift+=7u) {
        if (shift>27u) fail("overlong CompactIndex count");
        const auto next=readByte();
        magnitude|=static_cast<std::uint64_t>(next&0x7fu)<<shift;
        more=(next&0x80u)!=0u;
    }
    // Use a wider accumulator instead of the original loader's signed shifts:
    // malformed high bits must not overflow or alias a small accepted count.
    const std::uint64_t signedCap=negative ? 0x80000000ull : 0x7fffffffull;
    if (magnitude>signedCap) fail("CompactIndex count overflow");
    if (negative && magnitude!=0u) fail("negative array count");
    if (magnitude>elementCap || magnitude>std::numeric_limits<std::size_t>::max()/4u)
        fail("element or retained-byte budget exceeded");
    const auto count=static_cast<std::size_t>(magnitude);
    const auto remaining=bytes.size()-position;
    // Division/remainder avoids unchecked count*4 and rejects both truncation
    // and trailing bytes. All count, size, and budget checks precede allocation.
    if (remaining%4u!=0u || remaining/4u!=count)
        fail("payload size does not match array count");
    std::vector<std::int32_t> result;
    if (count>result.max_size()) fail("result allocation exceeds vector capacity");
    result.reserve(count);
    for (std::size_t index=0u; index<count; ++index) {
        std::uint32_t bits{};
        for (unsigned byte=0u; byte<4u; ++byte)
            bits|=static_cast<std::uint32_t>(bytes[position++])<<(byte*8u);
        std::int32_t value{};
        std::memcpy(&value,&bits,sizeof(value));
        result.push_back(value);
    }
    return result;
}

} // namespace QuestVr
