#include "surreal_portable_package_tables.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using Bytes = std::vector<std::uint8_t>;
std::size_t rejectionControls{};

void Require(bool condition, const char* description) {
    if (!condition) throw std::runtime_error(description);
}
void UInt32(Bytes& bytes, std::uint32_t value) {
    for (unsigned shift = 0; shift < 32; shift += 8) bytes.push_back(static_cast<std::uint8_t>(value >> shift));
}
void Index(Bytes& bytes, std::int32_t value) {
    const bool negative = value < 0;
    std::uint32_t magnitude = static_cast<std::uint32_t>(negative ? -static_cast<std::int64_t>(value) : value);
    auto first = static_cast<std::uint8_t>((magnitude & 63u) | (negative ? 128u : 0u));
    magnitude >>= 6;
    if (magnitude) first |= 64u;
    bytes.push_back(first);
    while (magnitude) {
        auto next = static_cast<std::uint8_t>(magnitude & 127u);
        magnitude >>= 7;
        if (magnitude) next |= 128u;
        bytes.push_back(next);
    }
}
void ReplaceUInt32(Bytes& bytes, std::size_t offset, std::uint32_t value) {
    for (unsigned index = 0; index < 4; ++index) bytes.at(offset + index) = static_cast<std::uint8_t>(value >> (8u * index));
}

struct Fixture {
    PortablePackageTables package;
    Bytes font, atlas, palette;
    std::vector<std::size_t> glyphOffsets;

    explicit Fixture(const std::filesystem::path& path, std::uint32_t pages = 1) {
        package.sourcePath = path.string();
        package.version = 68;
        for (const auto* name : {"None", "Engine", "Font", "Texture", "Palette", "TestFont", "Atlas"})
            package.names.push_back({NameString(name), 0});
        // Engine, Engine.Font, Engine.Texture and Engine.Palette class imports.
        package.imports = {{0, 0, 0, 1}, {0, 0, -1, 2}, {0, 0, -1, 3}, {0, 0, -1, 4}};
        package.exports = {{-2, 0, 0, 5, ObjectFlags{}, 0, 0},
                           {-3, 0, 1, 6, ObjectFlags{}, 0, 0},
                           {-4, 0, 0, 4, ObjectFlags{}, 0, 0}};
        Index(font, 0); // None property sentinel.
        Index(font, static_cast<std::int32_t>(pages));
        for (std::uint32_t page = 0; page < pages; ++page) {
            Index(font, 2); // Local original atlas export, not name-based lookup.
            Index(font, static_cast<std::int32_t>(256u / pages));
            for (std::uint32_t code = page * (256u / pages); code < (page + 1u) * (256u / pages); ++code) {
                glyphOffsets.push_back(font.size());
                std::uint32_t width{};
                if (code == 32u) width = 3;
                if (code == 'A') width = 5;
                if (code == 'B') width = 6;
                if (code == 'X') width = 7;
                if (code == 200u) width = 8;
                UInt32(font, 0); UInt32(font, 0); UInt32(font, width); UInt32(font, width ? 10u : 0u);
            }
        }
        UInt32(font, 256u / pages);

        Index(atlas, 4); atlas.push_back(5); Index(atlas, 3); // Palette object property.
        Index(atlas, 0); atlas.push_back(1); // None, one stored mip.
        UInt32(atlas, 0); Index(atlas, 128); // v68 mip offset and pixel count.
        for (unsigned index = 0; index < 128; ++index) atlas.push_back(static_cast<std::uint8_t>(index % 2));
        UInt32(atlas, 8); UInt32(atlas, 16); atlas.push_back(3); atlas.push_back(4);
        Index(palette, 0); Index(palette, 2);
        UInt32(palette, 0xffffffffu); UInt32(palette, 0xffc08040u);
    }

    void Save() {
        std::ofstream stream(package.sourcePath, std::ios::binary | std::ios::trunc);
        Require(static_cast<bool>(stream), "Could not create synthetic font payload fixture");
        std::int32_t offset{};
        std::size_t index{};
        for (const auto* bytes : {&font, &atlas, &palette}) {
            package.exports.at(index).ObjOffset = offset;
            package.exports.at(index++).ObjSize = static_cast<std::int32_t>(bytes->size());
            stream.write(reinterpret_cast<const char*>(bytes->data()), static_cast<std::streamsize>(bytes->size()));
            offset += static_cast<std::int32_t>(bytes->size());
        }
        Require(static_cast<bool>(stream), "Could not write synthetic font payload fixture");
    }
};

void ExpectFailure(const std::function<void()>& action, const char* description) {
    bool failed{};
    try { action(); } catch (const std::runtime_error&) { failed = true; }
    Require(failed, description);
    ++rejectionControls;
}

void SyntheticTests() {
    // This generated payload is decoder validation, never original-font evidence.
    const auto id = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    const auto directory = std::filesystem::temp_directory_path() / ("deusex-font-decoder-test-" + std::to_string(id));
    Require(std::filesystem::create_directory(directory), "Could not create isolated font test directory");
    struct Cleanup {
        std::filesystem::path directory;
        ~Cleanup() { std::error_code ignored; std::filesystem::remove_all(directory, ignored); }
    } cleanup{directory};
    const auto file = directory / "font-payload.bin";
    Fixture valid(file); valid.Save();
    const auto font = DecodePortableBitmapFont(valid.package, "testfont");
    Require(font.pages.size() == 1 && font.glyphs.size() == 256 && font.charactersPerPage == 256 &&
            font.lineHeight == 10 && font.texturePaths.at(0) == "TestFont.Atlas",
            "Original payload field order/page metadata differs");
    Require(font.pages.at(0).width == 8 && font.pages.at(0).height == 16 &&
            font.pages.at(0).rgba.at(3) == 0 && font.pages.at(0).rgba.at(7) == 255,
            "Indexed original font alpha masking failed");
    Require(GetPortableBitmapGlyph(font, 'a')->width == 5 &&
            GetPortableBitmapGlyph(font, 'z')->width == 3 &&
            GetPortableBitmapGlyph(font, 1000000)->width == 3,
            "UFont lowercase/space fallback differs");
    const auto measured = MeasurePortableBitmapText(font, "aBX ");
    Require(measured.width == 21 && measured.height == 10, "UGC exact advance/height metrics differ");
    Require(MeasurePortableBitmapText(font, std::string(1, static_cast<char>(200))).width == 8,
            "Raw UE1 high-byte glyph index was sign-extended");
    Require(MeasurePortableBitmapText(font, "").width == 0 &&
            MeasurePortableBitmapText(font, "").height == 0, "Empty string metrics differ");
    PortableBitmapFont empty;
    Require(GetPortableBitmapGlyph(empty, 'a') == nullptr &&
            MeasurePortableBitmapText(empty, "aBX ").width == 0 &&
            MeasurePortableBitmapText(empty, "aBX ").height == 0,
            "Empty font lookup or measurement is not safe");

    Fixture multipage(file, 2); multipage.Save();
    const auto twoPages = DecodePortableBitmapFont(multipage.package, "TestFont");
    Require(twoPages.pages.size() == 2 && twoPages.glyphs.at(200).pageIndex == 1 &&
            GetPortableBitmapGlyph(twoPages, 200)->width == 8 && twoPages.charactersPerPage == 128,
            "Multi-page font glyph concatenation differs from UFont");

    const auto bad = [&](const std::function<void(Fixture&)>& modify, const char* description) {
        Fixture fixture(file); modify(fixture); fixture.Save();
        ExpectFailure([&] { DecodePortableBitmapFont(fixture.package, "TestFont"); }, description);
    };
    bad([](Fixture& fixture) { fixture.font.at(1) = 0; }, "Zero page count accepted");
    bad([](Fixture& fixture) { fixture.font.at(1) = 0x81; }, "Negative page count accepted");
    bad([](Fixture& fixture) {
        fixture.font.erase(fixture.font.begin() + 1);
        fixture.font.insert(fixture.font.begin() + 1, {0x41, 0x04}); // Compact 257.
    }, "Excessive page count accepted");
    bad([](Fixture& fixture) { fixture.font.at(2) = 0; }, "Null atlas reference accepted");
    bad([](Fixture& fixture) { fixture.font.at(2) = 0x81; }, "Imported atlas reference silently accepted");
    bad([](Fixture& fixture) { fixture.font.at(2) = 63; }, "Out-of-table atlas reference accepted");
    bad([](Fixture& fixture) {
        fixture.font.erase(fixture.font.begin() + 2);
        fixture.font.insert(fixture.font.begin() + 2, {0x42, 0x80, 0x80, 0x80, 0x20});
    }, "Overflowing compact atlas reference wrapped to a valid export");
    bad([](Fixture& fixture) { fixture.font.at(2) = 3; }, "Palette used as font atlas");
    bad([](Fixture& fixture) { fixture.font.at(3) = 0xff; fixture.font.at(4) = 0x7f; }, "Negative glyph count accepted");
    bad([](Fixture& fixture) {
        fixture.font.erase(fixture.font.begin() + 3, fixture.font.begin() + 5);
        fixture.font.insert(fixture.font.begin() + 3, {0x41, 0x80, 0x08}); // Compact 65537.
    }, "Excessive glyph count accepted");
    bad([](Fixture& fixture) {
        fixture.font.resize(fixture.font.size() - 16);
    }, "Truncated glyph array accepted");
    bad([](Fixture& fixture) { ReplaceUInt32(fixture.font, fixture.glyphOffsets.at('A'), 0xffffffffu); },
        "Negative glyph rectangle coordinate accepted");
    bad([](Fixture& fixture) { ReplaceUInt32(fixture.font, fixture.glyphOffsets.at('A') + 8, 9); },
        "Glyph width outside atlas accepted");
    bad([](Fixture& fixture) { ReplaceUInt32(fixture.font, fixture.glyphOffsets.at('A') + 4, 16); },
        "Glyph bottom outside atlas accepted");
    bad([](Fixture& fixture) { ReplaceUInt32(fixture.font, fixture.font.size() - 4, 0); },
        "Zero characters-per-page accepted");
    bad([](Fixture& fixture) { ReplaceUInt32(fixture.font, fixture.font.size() - 4, 128); },
        "Too-small characters-per-page accepted");
    bad([](Fixture& fixture) { ReplaceUInt32(fixture.font, fixture.font.size() - 4, 65537); },
        "Excessive characters-per-page accepted");
    bad([](Fixture& fixture) { fixture.font.pop_back(); }, "Truncated font metrics accepted");
    bad([](Fixture& fixture) { fixture.font.push_back(0); }, "Trailing font payload accepted");
    bad([](Fixture& fixture) { fixture.package.exports.at(2).ObjClass = -3; },
        "Non-Palette atlas palette reference accepted");
    bad([](Fixture& fixture) { fixture.atlas.at(2) = 0; }, "Null atlas palette reference accepted");
    bad([](Fixture& fixture) { fixture.atlas.at(2) = 0x81; }, "Imported atlas palette reference accepted");
    bad([](Fixture& fixture) { fixture.atlas.at(2) = 63; }, "Out-of-table atlas palette reference accepted");
    bad([](Fixture& fixture) { fixture.atlas.at(4) = 0; }, "Font atlas without mipmaps accepted");

    valid.Save();
    auto metadata = valid.package;
    metadata.version = 63;
    ExpectFailure([&] { DecodePortableBitmapFont(metadata, "TestFont"); }, "Legacy font payload guessed");
    metadata = valid.package;
    metadata.exports.at(0).ObjSize = 2;
    ExpectFailure([&] { DecodePortableBitmapFont(metadata, "TestFont"); }, "Truncated native font payload accepted");
    metadata = valid.package;
    metadata.exports.at(0).ObjOffset = 10000000;
    ExpectFailure([&] { DecodePortableBitmapFont(metadata, "TestFont"); }, "Out-of-file font payload accepted");
    metadata = valid.package;
    metadata.exports.at(0).ObjClass = -3;
    ExpectFailure([&] { LoadPortableBitmapFont(metadata, 0); }, "Wrong export class accepted");
    ExpectFailure([&] { LoadPortableBitmapFont(valid.package, 10); }, "Out-of-table font index accepted");

    // Extend only the generated fixture so the payload is in-file but exceeds
    // the memory guard. This checks rejection before property-reader copies.
    for (const std::size_t exportIndex : {0u, 1u, 2u}) {
        valid.Save();
        metadata = valid.package;
        metadata.exports.at(exportIndex).ObjSize = 16 * 1024 * 1024 + 1;
        std::filesystem::resize_file(file,
            static_cast<std::uintmax_t>(metadata.exports.at(exportIndex).ObjOffset) +
                static_cast<std::uintmax_t>(metadata.exports.at(exportIndex).ObjSize));
        ExpectFailure([&] { DecodePortableBitmapFont(metadata, "TestFont"); },
            "In-file oversized font/atlas/palette export accepted");
    }
    std::cout << "Synthetic bitmap font decoder, masks, fallback, multipage, raw byte metrics and "
              << rejectionControls << " rejection controls passed.\n";
}

void OriginalTests(const std::string& path) {
    const auto package = LoadPortablePackageTables(path);
    for (const auto* name : {"FontMenuHeaders", "FontMenuSmall"}) {
        const auto font = DecodePortableBitmapFont(package, name);
        const std::string expected = std::string(name) + (std::string(name) == "FontMenuHeaders" ? ".Texture16" : ".Texture19");
        Require(font.pages.size() == 1 && font.glyphs.size() == 256 && font.charactersPerPage == 256 &&
                font.lineHeight == 10 && font.texturePaths.at(0) == expected,
                "Original DeusExUI font metadata differs from inspected package");
        std::size_t transparent{}, visible{};
        for (std::size_t index = 3; index < font.pages.at(0).rgba.size(); index += 4) {
            if (font.pages.at(0).rgba[index] == 0) ++transparent; else ++visible;
        }
        Require(transparent > 0 && visible > 0, "Original font atlas is missing its masked glyph pixels");
        const auto metrics = MeasurePortableBitmapText(font, "Inventory Health Goals/Notes Logs");
        Require(metrics.width > 0 && metrics.height == 10, "Original font text metrics empty or wrong height");
        if (std::string(name) == "FontMenuHeaders") {
            const std::pair<const char*, std::uint64_t> captions[] = {
                {"Inventory", 53}, {"Health", 36}, {"Augs", 29}, {"Skills", 31},
                {"Goals/Notes", 69}, {"Conversations", 82}, {"Images", 39}, {"Logs", 28}};
            for (const auto& [caption, expectedWidth] : captions)
                Require(MeasurePortableBitmapText(font, caption).width == expectedWidth,
                        "Original Persona navigation caption advance differs");
        }
        std::cout << name << ": " << font.texturePaths.at(0) << "; "
                  << font.pages.at(0).width << 'x' << font.pages.at(0).height << "; 256 glyphs; height 10; "
                  << transparent << " masked pixels; sample advance " << metrics.width << ".\n";
    }
}
} // namespace

int main(int argc, char** argv) {
    try {
        SyntheticTests();
        if (argc == 3 && std::string(argv[1]) == "--package") OriginalTests(argv[2]);
        else if (argc != 1) throw std::runtime_error("Usage: portable_bitmap_font_test [--package DeusExUI.u]");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Bitmap font test failed: " << error.what() << '\n';
        return 1;
    }
}
