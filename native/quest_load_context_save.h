#pragma once

#include "quest_linker_load_context.h"
#include "quest_save_bundle.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <stdexcept>
#include <vector>

namespace QuestVr {

// Portable persistence envelope, NOT a recovered original Unreal save layout.
// Raw inspection mode is represented by NO envelope; its existing bytes are
// left alone by the caller. An explicit all-false profile is a real wrapped
// mask0, not permission to reinterpret a legacy/raw checkpoint.
// Wire: DXLC[4], version U32LE, profile mask U32LE, inner length U32LE,
// opaque inner bytes, CRC32 U32LE over ALL preceding prefix+inner bytes.
inline constexpr std::size_t LoadContextSavePrefixBytes=16u;
inline constexpr std::size_t LoadContextSaveChecksumBytes=4u;
inline constexpr std::size_t LoadContextSaveHeaderBytes=
    LoadContextSavePrefixBytes+LoadContextSaveChecksumBytes; // Total overhead.
inline constexpr std::uint32_t LoadContextSaveVersion=1u;
inline constexpr std::uint32_t LoadContextSaveAllowedMask=0x00070000u;
enum class LoadContextSaveStatus {NotWrapped,Wrapped};
struct DecodedLoadContextSave {
    LoadContextSaveStatus status{LoadContextSaveStatus::NotWrapped};
    std::optional<OriginalLinkerLoadProfile> profile;
    // Borrowed input span: caller must retain its backing byte buffer. Decode
    // never copies the opaque payload or allocates/materializes any UObject.
    std::span<const std::uint8_t> inner;
};

namespace LoadContextSaveDetail {
inline constexpr std::array<std::uint8_t,4u> Magic{'D','X','L','C'};
inline void Policy(std::size_t maximum) {
    // A caller may LOWER the existing runtime file policy, never raise it.
    if(maximum>kMaximumSaveRuntimeBytes)
        throw std::invalid_argument("Load-context save bound exceeds the existing runtime file policy");
}
inline std::uint32_t Word(std::span<const std::uint8_t> bytes,std::size_t offset) {
    if(offset>bytes.size()||bytes.size()-offset<4u)
        throw std::runtime_error("Load-context save word is truncated");
    std::uint32_t value{};
    for(unsigned i=0u;i<4u;++i)value|=std::uint32_t(bytes[offset+i])<<(i*8u);
    return value;
}
} // namespace LoadContextSaveDetail

inline std::size_t LoadContextSaveInnerLimit(std::size_t maxFileBytes=kMaximumSaveRuntimeBytes) {
    LoadContextSaveDetail::Policy(maxFileBytes);
    if(maxFileBytes<LoadContextSaveHeaderBytes)
        throw std::invalid_argument("Load-context save bound cannot hold its envelope overhead");
    return maxFileBytes-LoadContextSaveHeaderBytes;
}

inline std::vector<std::uint8_t> EncodeLoadContextSave(const OriginalLinkerLoadProfile& profile,
    std::span<const std::uint8_t> inner,std::size_t maxFileBytes=kMaximumSaveRuntimeBytes) {
    const auto maximum=LoadContextSaveInnerLimit(maxFileBytes);
    if(inner.size()>maximum||inner.size()>std::numeric_limits<std::uint32_t>::max())
        throw std::runtime_error("Load-context save inner bytes exceed the whole-file bound");
    std::vector<std::uint8_t> bytes(LoadContextSaveHeaderBytes+inner.size(),0u);
    std::copy(LoadContextSaveDetail::Magic.begin(),LoadContextSaveDetail::Magic.end(),bytes.begin());
    SaveBundleDetail::Put32(bytes,4u,LoadContextSaveVersion);
    SaveBundleDetail::Put32(bytes,8u,profile.Mask());
    SaveBundleDetail::Put32(bytes,12u,static_cast<std::uint32_t>(inner.size()));
    std::copy(inner.begin(),inner.end(),bytes.begin()+LoadContextSavePrefixBytes);
    const auto checksumOffset=bytes.size()-LoadContextSaveChecksumBytes;
    SaveBundleDetail::Put32(bytes,checksumOffset,SaveBundleDetail::Crc32(bytes.data(),checksumOffset));
    return bytes;
}

inline DecodedLoadContextSave DecodeLoadContextSave(std::span<const std::uint8_t> bytes,
    std::size_t maxFileBytes=kMaximumSaveRuntimeBytes) {
    LoadContextSaveDetail::Policy(maxFileBytes);
    if(bytes.size()>maxFileBytes)
        throw std::runtime_error("Load-context save exceeds the whole-file bound");
    const auto matched=std::min(bytes.size(),LoadContextSaveDetail::Magic.size());
    const bool matching=std::equal(bytes.begin(),bytes.begin()+matched,LoadContextSaveDetail::Magic.begin());
    if(!matching||bytes.empty())return {LoadContextSaveStatus::NotWrapped,std::nullopt,bytes};
    // A matching reserved magic PREFIX is a truncated wrapper, not a legacy
    // fallback. Different old magic (including DXQS) passes through unchanged.
    if(bytes.size()<LoadContextSaveHeaderBytes)
        throw std::runtime_error("Load-context save envelope is truncated");
    if(LoadContextSaveDetail::Word(bytes,4u)!=LoadContextSaveVersion)
        throw std::runtime_error("Load-context save version is unsupported");
    const auto mask=LoadContextSaveDetail::Word(bytes,8u);
    if(mask&~LoadContextSaveAllowedMask)
        throw std::runtime_error("Load-context save profile mask is noncanonical");
    const auto length=LoadContextSaveDetail::Word(bytes,12u);
    if(static_cast<std::size_t>(length)>LoadContextSaveInnerLimit(maxFileBytes)||
        static_cast<std::size_t>(length)!=bytes.size()-LoadContextSaveHeaderBytes)
        throw std::runtime_error("Load-context save inner length is invalid");
    const auto checksumOffset=bytes.size()-LoadContextSaveChecksumBytes;
    if(LoadContextSaveDetail::Word(bytes,checksumOffset)!=SaveBundleDetail::Crc32(bytes.data(),checksumOffset))
        throw std::runtime_error("Load-context save checksum does not bind its profile and payload");
    OriginalLinkerLoadProfile profile{(mask&0x00040000u)!=0u,(mask&0x00010000u)!=0u,(mask&0x00020000u)!=0u};
    // Profile equality/allowed migration, inner-save format, source identity
    // and cold map loading are the caller's PRE-publication responsibilities.
    return {LoadContextSaveStatus::Wrapped,profile,bytes.subspan(LoadContextSavePrefixBytes,length)};
}

} // namespace QuestVr
