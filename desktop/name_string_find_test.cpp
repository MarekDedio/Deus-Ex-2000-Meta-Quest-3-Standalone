#include "Precomp.h"
#include "Package/NameString.h"

#include <array>
#include <iostream>
#include <stdexcept>
#include <string_view>

// Deliberately linked only with NameString.cpp. Other runtime translation units
// may intern names during static initialization, masking the empty-table case.
namespace {
std::size_t checks{};
void Require(bool value, const char* message)
{
    ++checks;
    if (!value) throw std::runtime_error(message);
}

void Found(std::string_view value, int expected)
{
    const auto actual = NameString::FindCompareIndex(value);
    Require(actual.has_value(), "Known compare identity was not found");
    Require(*actual == expected, "Wrong compare identity returned");
}

void Missing(std::string_view value)
{
    Require(!NameString::FindCompareIndex(value), "Unknown name gained a compare identity");
}

// Independent arithmetic oracle, not the production table/helper.
unsigned char FoldByte(unsigned char value)
{
    return value >= static_cast<unsigned char>('a') && value <= static_cast<unsigned char>('z')
        ? static_cast<unsigned char>(value - static_cast<unsigned char>('a') + static_cast<unsigned char>('A'))
        : value;
}

void EmptyTable()
{
    Found({}, 0);
    Found("", 0);
    for (unsigned mask = 0; mask != 16; ++mask)
    {
        std::string none = "None";
        for (unsigned i = 0; i != 4; ++i)
            none[i] = (mask & (1u << i)) ? "NONE"[i] : "none"[i];
        Found(none, 0);
    }
    Missing("UNINITIALIZED_UNKNOWN");
    Missing("uninitialized_unknown");
    Missing(std::string_view("None\0", 5));
    Missing(std::string_view("\0", 1));
    Missing(std::string(1, static_cast<char>(255)));
    const NameString first("FIND_FIRST_ANCHOR");
    Require(first.GetCompareIndex() == 1, "Read-only lookup initialized or populated the empty name table");
    Require(first.ToString() == "FIND_FIRST_ANCHOR", "Existing spelling behavior changed");
}

void NoNewNamesOrSpellings()
{
    const NameString before("FindKnownMiXeDAnchor");
    const int expectedNext = before.GetCompareIndex() + 2;
    for (unsigned repeat = 0; repeat != 64; ++repeat)
    {
        Found("FINDKNOWNMIXEDANCHOR", before.GetCompareIndex());
        Found("findknownmixedanchor", before.GetCompareIndex());
        Found("FindKnownMiXeDAnchor", before.GetCompareIndex());
        Missing("FIND_UNKNOWN_COMPARE");
        Missing("find_unknown_compare");
        Found("nOnE", 0);
        Found({}, 0);
    }
    const NameString after("FIND_AFTER_READ_ONLY_ANCHOR");
    Require(after.GetCompareIndex() == expectedNext,
        "Find added a name or case-variant spelling entry");

    // Verify the index-spacing observer itself: a real case-variant spelling
    // consumes one entry, while a fresh compare name consumes two.
    const NameString variant("FINDKNOWNMIXEDANCHOR");
    Require(variant.GetCompareIndex() == before.GetCompareIndex(), "GetIndex compare identity changed");
    Require(variant.ToString() == "FINDKNOWNMIXEDANCHOR", "GetIndex no longer preserves spelling");
    const NameString afterSpelling("FIND_AFTER_REAL_SPELLING_ANCHOR");
    Require(afterSpelling.GetCompareIndex() == after.GetCompareIndex() + 3,
        "Index-spacing observer did not detect a real new spelling");
    const NameString afterCompare("FIND_AFTER_REAL_COMPARE_ANCHOR");
    Require(afterCompare.GetCompareIndex() == afterSpelling.GetCompareIndex() + 2,
        "Index-spacing observer did not detect a fresh compare identity");
}

void ExhaustiveBytes()
{
    std::array<int, 256> actualIndices{};
    for (unsigned byte = 0; byte != 256; ++byte)
    {
        // The numeric suffix makes each iteration distinct even for a-z/A-Z.
        std::string value = "FindByte_";
        value.push_back(static_cast<char>(byte));
        value += '_' + std::to_string(byte);
        std::string folded;
        for (const unsigned char c : value)
            folded.push_back(static_cast<char>(FoldByte(c)));
        Missing(value);
        Missing(folded);
        const NameString stored(value);
        actualIndices[byte] = stored.GetCompareIndex();
        const int expectedNext = stored.GetCompareIndex() + 2;
        Found(value, stored.GetCompareIndex());
        Found(folded, stored.GetCompareIndex());
        std::string lower = value;
        for (char& c : lower)
            if (static_cast<unsigned char>(c) >= 0x41 && static_cast<unsigned char>(c) <= 0x5a)
                c = static_cast<char>(static_cast<unsigned char>(c) + 0x20);
        Found(lower, stored.GetCompareIndex());
        const NameString sentinel("FIND_BYTE_SENTINEL_" + std::to_string(byte));
        Require(sentinel.GetCompareIndex() == expectedNext,
            "Byte lookup created a name or case-variant spelling");
    }
    for (unsigned a = 0; a != 256; ++a)
        for (unsigned b = 0; b != 256; ++b)
            Require((actualIndices[a] == actualIndices[b]) == (a == b),
                "Distinct suffixed byte names acquired the same compare identity");

    // Same prefix/suffix for all 256 bytes checks fold equivalence directly,
    // including controls, embedded NUL, signed-char bytes and byte 255.
    std::array<int, 256> equivalenceIndices{};
    for (unsigned byte = 0; byte != 256; ++byte)
    {
        std::string value = "FIND_EQ_";
        value.push_back(static_cast<char>(byte));
        value += "_END";
        const NameString stored(value);
        equivalenceIndices[byte] = stored.GetCompareIndex();
        Found(value, stored.GetCompareIndex());
        std::string folded = "FIND_EQ_";
        folded.push_back(static_cast<char>(FoldByte(static_cast<unsigned char>(byte))));
        folded += "_END";
        Found(folded, stored.GetCompareIndex());
    }
    for (unsigned a = 0; a != 256; ++a)
        for (unsigned b = 0; b != 256; ++b)
            Require((equivalenceIndices[a] == equivalenceIndices[b]) ==
                (FoldByte(static_cast<unsigned char>(a)) == FoldByte(static_cast<unsigned char>(b))),
                "Find/GetIndex byte folding differs from the independent oracle");
}

void LengthAndNone()
{
    const std::string embedded("Find\0Name\xff", 10);
    Missing(embedded);
    const NameString interned(embedded);
    Found(embedded, interned.GetCompareIndex());
    Found(std::string_view("FIND\0NAME\xff", 10), interned.GetCompareIndex());
    Missing(std::string_view(embedded.data(), 4));
    Missing(std::string_view(embedded.data(), embedded.size() - 1));
    Missing(embedded + '\0');
    const std::string noneNul("None\0", 5);
    Missing(noneNul);
    const NameString internedNoneNul(noneNul);
    Require(!internedNoneNul.IsNone(), "GetIndex treated a length-bearing NUL name as None");
    Found(noneNul, internedNoneNul.GetCompareIndex());
    Found("None", 0);
    Found("NONE", 0);
    Found("none", 0);
    Found({}, 0);
    const char noTerminator[] = {'f','i','n','d','_','s','l','i','c','e','!'};
    const std::string_view slice(noTerminator, 10);
    Missing(slice);
    const NameString sliceName{std::string(slice)};
    Found(slice, sliceName.GetCompareIndex());
    Missing(std::string_view(noTerminator, sizeof(noTerminator)));
}
}

int main()
{
    try
    {
        EmptyTable();
        NoNewNamesOrSpellings();
        ExhaustiveBytes();
        LengthAndNone();
        std::cout << "NameString non-interning find: " << checks << " checks passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "NameString non-interning find failed after " << checks << " checks: " << error.what() << '\n';
        return 1;
    }
}
