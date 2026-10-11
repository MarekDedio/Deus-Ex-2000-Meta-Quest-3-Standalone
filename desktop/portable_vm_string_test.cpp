#include "Precomp.h"
#include "surreal_portable_package_tables.h"
#include "quest_unreal_string.h"

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

// Pure generated tagged payloads; no owned packages, VM startup or UI changes.
namespace {
using Bytes = std::vector<std::uint8_t>;
std::size_t checks{}, rejections{};
void Check(const bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
    ++checks;
}
void Index(Bytes& out, const std::int32_t value) {
    auto magnitude = static_cast<std::uint64_t>(value < 0 ? -static_cast<std::int64_t>(value) : value);
    auto byte = static_cast<std::uint8_t>((magnitude & 63u) | (value < 0 ? 128u : 0u)); magnitude >>= 6u;
    if (magnitude) byte |= 64u;
    out.push_back(byte);
    while (magnitude) {
        byte = static_cast<std::uint8_t>(magnitude & 127u); magnitude >>= 7u;
        if (magnitude) byte |= 128u;
        out.push_back(byte);
    }
}
PortableTaggedProperty Property(Bytes bytes) {
    PortableTaggedProperty property; property.type = 13u; property.value = std::move(bytes);
    property.size = static_cast<std::uint32_t>(property.value.size()); return property;
}
PortableTaggedProperty Ansi(const std::string& bytes) {
    Check(bytes.size() < static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()), "ANSI fixture overflow");
    Bytes payload; Index(payload, static_cast<std::int32_t>(bytes.size() + 1u));
    for (const unsigned char byte : bytes) payload.push_back(byte);
    payload.push_back(0u); return Property(std::move(payload));
}
PortableTaggedProperty Unicode(const std::u16string& units) {
    Check(units.size() < static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()), "Unicode fixture overflow");
    Bytes payload; Index(payload, -static_cast<std::int32_t>(units.size() + 1u));
    for (const auto unit : units) {
        payload.push_back(static_cast<std::uint8_t>(unit)); payload.push_back(static_cast<std::uint8_t>(unit >> 8u));
    }
    payload.push_back(0u); payload.push_back(0u); return Property(std::move(payload));
}
void Exact(const PortableTaggedProperty& property, const std::u16string& units, const std::string& message) {
    const auto before = property.value; const auto encoded = DecodePortableVmStringProperty(property);
    Check(QuestVr::UnrealString::Decode(encoded) == units, message);
    Check(encoded == QuestVr::UnrealString::Encode(units), "VM FString is not canonical UTF8/WTF8 transport");
    Check(property.value == before, "VM FString decoder changed its tagged input");
}
void Refused(const PortableTaggedProperty& property, const std::string& message,
    const std::size_t maximum = 32u * 1024u * 1024u) {
    const auto before = property.value; bool rejected{};
    try { static_cast<void>(DecodePortableVmStringProperty(property, maximum)); }
    catch (const std::exception&) { rejected = true; }
    Check(rejected, message); Check(property.value == before, "Refused VM FString changed its input"); ++rejections;
}
void Tests() {
    Exact(Property({0u}), {}, "Canonical zero-count FString did not decode empty");
    Exact(Property({0x80u}), {}, "Original compact negative-zero count lost its zero value");
    Exact(Ansi(""), {}, "Terminated ANSI empty FString failed");
    Exact(Unicode(u""), {}, "Terminated UTF16 empty FString failed");
    Exact(Ansi("None"), u"None", "Literal FString None was treated as Name None");
    Exact(Ansi("Game version: 1.112fm"), u"Game version: 1.112fm", "ASCII property ingress changed original bytes");
    Exact(Unicode(u"Za\u017c\u00f3\u0142\u0107 \u03bb\u0416\u4e2d"), u"Za\u017c\u00f3\u0142\u0107 \u03bb\u0416\u4e2d",
        "Original Unicode FString fell back to presentation ASCII");
    const std::u16string pair{static_cast<char16_t>(0xd83du), static_cast<char16_t>(0xde42u)};
    Exact(Unicode(pair), pair, "Supplementary UTF16 pair lost its two original code units");
    Check(DecodePortableVmStringProperty(Unicode(pair)) == std::string("\xf0\x9f\x99\x82"),
        "Supplementary pair did not use canonical four-byte transport");
    for (const auto& units : std::vector<std::u16string>{
        {static_cast<char16_t>(0xd800u)}, {static_cast<char16_t>(0xdfffu)},
        {u'A', static_cast<char16_t>(0xd83du)}, {static_cast<char16_t>(0xde42u), u'B'},
        {static_cast<char16_t>(0xd800u), u'X', static_cast<char16_t>(0xdc00u)}, {u'A', u'\0', u'B'}})
        Exact(Unicode(units), units, "VM ingress rejected/normalized retained original UTF16 storage");
    Exact(Ansi(std::string("A\0B", 3u)), std::u16string{u'A', u'\0', u'B'}, "ANSI embedded-NUL storage was truncated");
    // Original ANSI serialization zero-extends every byte; it does not interpret
    // the original bytes as UTF8. All 256 values independently round-trip.
    for (unsigned byte = 0u; byte < 256u; ++byte)
        Exact(Ansi(std::string(1u, static_cast<char>(byte))),
            std::u16string(1u, static_cast<char16_t>(byte)), "ANSI byte ingress did not zero-extend its original unit");
    // Every individual UTF16 unit, including lone surrogates, is retained.
    for (std::uint32_t unit = 0u; unit <= 0xffffu; ++unit)
        Exact(Unicode(std::u16string(1u, static_cast<char16_t>(unit))),
            std::u16string(1u, static_cast<char16_t>(unit)), "Original UTF16 unit did not round-trip through VM transport");

    Refused(Property({}), "Type13 with no compact count was accepted as empty");
    Refused(Property({0u, 0u}), "Zero-count FString accepted trailing bytes");
    Refused(Property({0x80u, 1u}), "Negative-zero FString accepted trailing bytes");
    auto ansiTrailing = Ansi("abc"); ansiTrailing.value.push_back(0x55u); ++ansiTrailing.size;
    Refused(ansiTrailing, "Terminated ANSI FString accepted trailing payload");
    auto wideTrailing = Unicode(u"abc"); wideTrailing.value.push_back(0u); ++wideTrailing.size;
    Refused(wideTrailing, "Terminated Unicode FString accepted trailing payload");
    Refused(Property({1u, 'x'}), "ANSI FString missing its final zero terminator was accepted");
    Refused(Property({0x81u, 1u, 0u}), "UTF16 FString missing its final zero terminator was accepted");
    Refused(Property({0x40u}), "Truncated compact length was accepted");
    Refused(Property({0x7fu, 0xffu, 0xffu, 0xffu, 0xffu, 1u}), "Overflowing/overlong compact length was accepted");
    Bytes minimum; Index(minimum, std::numeric_limits<std::int32_t>::min());
    Refused(Property(minimum), "Unrepresentable negative FString length was accepted");
    for (const auto length : {std::numeric_limits<std::int32_t>::max(), -std::numeric_limits<std::int32_t>::max()}) {
        Bytes payload; Index(payload, length); Refused(Property(payload), "Huge compact count without data was accepted");
    }
    for (const auto& property : {Ansi("abc"), Unicode(u"A\u017c\u03bb"), Ansi(std::string(64u, 'x')), Unicode(std::u16string(64u, u'X'))})
        for (std::size_t size = 0u; size < property.value.size(); ++size)
            Refused(Property(Bytes(property.value.begin(), property.value.begin() + static_cast<std::ptrdiff_t>(size))),
                "A truncated ANSI/Unicode payload prefix was accepted");

    Check(DecodePortableVmStringProperty(Property({0u}), 0u).empty(), "Zero-byte output budget rejected a zero-count string");
    Check(DecodePortableVmStringProperty(Ansi(""), 0u).empty(), "Zero-byte output budget rejected terminated empty ANSI");
    Check(DecodePortableVmStringProperty(Unicode(u""), 0u).empty(), "Zero-byte output budget rejected terminated empty UTF16");
    for (const auto& property : {Ansi("A"), Ansi(std::string(1u, static_cast<char>(0xffu))),
        Unicode(u"\u4e2d"), Unicode(std::u16string(1u, static_cast<char16_t>(0xd800u))), Unicode(pair),
        Unicode(std::u16string{u'A', u'\0', u'B'}), Unicode(std::u16string{static_cast<char16_t>(0xd800u), u'X', static_cast<char16_t>(0xdc00u)})}) {
        const auto expected = DecodePortableVmStringProperty(property);
        Check(!expected.empty() && DecodePortableVmStringProperty(property, expected.size()) == expected,
            "Exact transport-byte budget did not admit its actual encoded output");
        Refused(property, "Transport-byte budget counted source bytes or scalar count instead of actual encoded output", expected.size() - 1u);
        Refused(property, "Zero-byte transport budget admitted a nonempty FString", 0u);
    }
    const auto highAnsi = Ansi(std::string(4096u, static_cast<char>(0xffu)));
    Check(DecodePortableVmStringProperty(highAnsi, 8192u).size() == 8192u,
        "ANSI boundary output did not charge its expanded transport length");
    Refused(highAnsi, "ANSI expansion bypassed a lower transport-byte bound", 8191u);
}
} // namespace
int main() {
    try { Tests(); std::cout << "VM FString ingress: " << checks << " checks, " << rejections
        << " malformed/budget refusals; ANSI zero-extension, exact UTF16/WTF8 units, strict payload boundaries, transport-byte bounds\n"; return 0; }
    catch (const std::exception& error) { std::cerr << "VM FString ingress failed: " << error.what() << '\n'; return 1; }
}
