#pragma once

#include <cstdint>

namespace QuestVr {

// Original Core.dll1011DE86..1011DF00: one linker-owned context assembled
// from the explicit GIsEditor/GIsClient/GIsServer inputs. No auto-editor or
// standalone inference is made here; all-false deliberately means mask zero.
// These portable booleans are not the original ULinker layout or globals.
struct OriginalLinkerLoadProfile {
    bool editor{},client{},server{};

    constexpr std::uint32_t Mask() const noexcept {
        return (editor ? 0x00040000u : 0u) |
            (client ? 0x00010000u : 0u) |
            (server ? 0x00020000u : 0u);
    }
    bool operator==(const OriginalLinkerLoadProfile&) const = default;
};

enum class OriginalLinkerCreateExportAction {UseCached,Construct,Exclude};

// Original ULinkerLoad::CreateExport1012E26C..1012E283 checks the cached
// UObject FIRST. Only a fresh export tests source ObjectFlags against the
// linker's positive load-context mask. The upper NotFor* bits are not gates.
// This allocation-free decision grants no object identity, class/import
// resolution, source-reference rewrite, construction or runtime publication.
constexpr OriginalLinkerCreateExportAction OriginalLinkerCreateExportDecision(
    const OriginalLinkerLoadProfile& profile,const std::uint32_t sourceObjectFlags,
    const bool hasCachedObject) noexcept {
    if(hasCachedObject)return OriginalLinkerCreateExportAction::UseCached;
    return (sourceObjectFlags&profile.Mask())!=0u ?
        OriginalLinkerCreateExportAction::Construct : OriginalLinkerCreateExportAction::Exclude;
}

} // namespace QuestVr
