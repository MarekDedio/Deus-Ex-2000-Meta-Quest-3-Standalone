#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

struct PortablePackageTables;

namespace QuestVr {

struct NativeFontGlyphMetrics {
    std::int32_t x{}, y{}, width{}, height{};
    bool operator==(const NativeFontGlyphMetrics&) const = default;
};

struct NativeFontMetricsPage {
    std::int32_t textureReference{};
    std::string texturePath; // Package-relative identity, never a native pointer.
    std::uint32_t atlasWidth{}, atlasHeight{};
    std::vector<NativeFontGlyphMetrics> glyphs; // Actual per-page count, not padded.
};

struct NativeFontMetrics {
    std::string objectPath; // Package-relative, as in PortableBitmapFont.
    std::uint32_t charactersPerPage{};
    std::vector<NativeFontMetricsPage> pages;
    std::size_t usedWork{}, peakWorkingBytes{};
};

struct NativeFontMetricsLimits {
    std::size_t retainedBytes{4u << 20u};
    std::size_t workingBytes{8u << 20u};
    std::size_t work{4u << 20u};
    std::size_t maxPayloadBytes{16u << 20u};
    std::size_t maxPages{256u};
    std::size_t maxGlyphs{65'536u};
    std::size_t maxProperties{65'536u};
    std::uint64_t maxAtlasPixels{16u << 20u};
    // Called BEFORE charged work / allocations, in the caller's transaction.
    // admitPeak is the complete overlapping loader result+scratch+next allocation,
    // not cumulative allocation history. Exceptions are never swallowed.
    std::function<void(std::size_t)> chargeWork;
    std::function<void(std::size_t)> admitPeak;
};

// The caller owns immutable, authenticated source tables/files for this call and
// any cache lifetime. This loader does not make a mutable file into an immutable
// snapshot, run original code, or establish live UObject/cache authority.
// Font, atlas and palette data are read-only. No pixel/color arrays are decoded.
NativeFontMetrics LoadNativeFontMetrics(const PortablePackageTables& package,
    std::size_t exportIndex, const NativeFontMetricsLimits& limits = {});

// Owned capacities, including the fixed result object and path/string capacity.
// The host must retain this charge while a metrics cache entry remains live.
std::size_t NativeFontMetricsRetainedBytes(const NativeFontMetrics& font);

// Retail Extension glyph lookup: mask to UTF16, quotient/remainder by the real
// charactersPerPage, then validate actual page and character counts. Missing is
// zero; there is no lowercase/space fallback. Invalid zero divisor is refused.
NativeFontGlyphMetrics LookupNativeFontGlyph(const NativeFontMetrics& font,
    std::uint32_t codeUnit);

} // namespace QuestVr
