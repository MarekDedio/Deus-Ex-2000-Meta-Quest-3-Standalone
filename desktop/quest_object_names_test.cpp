#include "quest_object_names.h"

#include <functional>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace {
using QuestVr::ObjectNames::MakeUnique;
using QuestVr::UnrealString::Decode;
using QuestVr::UnrealString::Encode;
std::size_t checks{}, rejections{};

void Require(const bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
    ++checks;
}
void Reject(const std::function<void()>& operation, const std::string& message) {
    bool rejected{};
    try { operation(); } catch (const std::runtime_error&) { rejected = true; }
    Require(rejected, message);
    ++rejections;
}

void ClassNamesAndDigits() {
    std::int32_t counter{};
    const auto vacant = [](std::string_view) { return false; };
    Require(MakeUnique("DeusExRootWindow", counter, vacant) == "DeusExRootWindow0" && counter == 1,
        "First concrete class counter did not start at0");
    Require(MakeUnique("DeusExRootWindow", counter, vacant) == "DeusExRootWindow1" && counter == 2,
        "Same class reused its first free suffix");
    counter = 7;
    Require(MakeUnique("Class123", counter, vacant) == "Class7" && counter == 8,
        "Trailing ASCII digits were not removed");
    Require(MakeUnique("Cl4ss12", counter, vacant) == "Cl4ss8" && counter == 9,
        "Non-trailing digit was removed");
    Require(MakeUnique("123", counter, vacant) == "9" && counter == 10,
        "All-digit class name did not retain its empty stripped prefix");
    const auto nonAsciiDigit = Encode(u"Class\u0661");
    Require(MakeUnique(nonAsciiDigit, counter, vacant) == nonAsciiDigit + "10" && counter == 11,
        "Non-ASCII digit was treated as an ASCII suffix");
}

void OuterCollisionsAndConcreteClasses() {
    std::map<std::string, std::set<std::string>> objects;
    std::map<std::string, std::int32_t> counters;
    // A different object's CLASS is deliberately not part of a name lookup.
    objects["PawnA"].insert("Window0");
    objects["PawnA"].insert("Window2");
    std::vector<std::string> observed;
    const auto create = [&](const std::string& cls, const std::string& outer) {
        const auto name = MakeUnique(cls, counters[cls], [&](const std::string_view candidate) {
            observed.push_back(outer + '.' + std::string(candidate));
            return objects[outer].count(std::string(candidate)) != 0u;
        });
        objects[outer].insert(name);
        return name;
    };
    Require(create("Window", "PawnA") == "Window1" && counters["Window"] == 2,
        "Any-class same-Outer collision did not consume the first candidate");
    Require(observed == std::vector<std::string>{"PawnA.Window0", "PawnA.Window1"},
        "Candidate collision checks changed retail order");
    observed.clear();
    Require(create("Window", "PawnB") == "Window2" && counters["Window"] == 3,
        "Different Outer reset the concrete class counter or collided across Outers");
    Require(observed == std::vector<std::string>{"PawnB.Window2"},
        "Foreign Outer collision was consulted");
    Require(create("OtherWindow", "PawnA") == "OtherWindow0" && counters["OtherWindow"] == 1 &&
        counters["Window"] == 3, "Different concrete class shared another class's counter");
    Require(create("Window", "PawnA") == "Window3" && counters["Window"] == 4,
        "Concrete class counter did not retain progress across Outers");
}

void Utf16Truncation() {
    const auto vacant = [](std::string_view) { return false; };
    for (const std::int32_t suffixIndex : {0, 9, 10, 99, 100, 999, 1000, 999999999, 2147483646}) {
        const auto suffix = std::to_string(suffixIndex);
        for (const std::size_t size : {1u, 60u, 61u, 62u, 63u, 100u}) {
            std::int32_t counter = suffixIndex;
            const auto generated = MakeUnique(std::string(size, 'A'), counter, vacant);
            const auto prefix = std::min(size, 62u - suffix.size());
            Require(generated == std::string(prefix, 'A') + suffix && Decode(generated).size() <= 62u &&
                counter == suffixIndex + 1, "ASCII class name truncation/suffix progression changed");
        }
    }
    std::u16string unicodeClass(80u, u'\u732b');
    std::int32_t counter = 123;
    auto actual = MakeUnique(Encode(unicodeClass), counter, vacant);
    Require(Decode(actual) == std::u16string(59u, u'\u732b') + u"123" && counter == 124,
        "Unicode class name was truncated by UTF8 bytes instead of UTF16 units");

    // First candidate cuts a surrogate pair in half; the original code-unit
    // operation must retain its high half as WTF8 instead of rounding to a scalar.
    unicodeClass = std::u16string(60u, u'A') + u"\U0001f642Tail99";
    counter = 0;
    actual = MakeUnique(Encode(unicodeClass), counter, vacant);
    auto expected = std::u16string(60u, u'A');
    expected.push_back(static_cast<char16_t>(0xd83du));
    expected += u"0";
    Require(Decode(actual) == expected && actual == Encode(expected),
        "Surrogate-splitting original UTF16 truncation was normalized away");

    unicodeClass = std::u16string(60u, u'A');
    unicodeClass.push_back(static_cast<char16_t>(0xdc00u));
    unicodeClass += u"99";
    counter = 0;
    Require(Decode(MakeUnique(Encode(unicodeClass), counter, vacant)) ==
        unicodeClass.substr(0u, 61u) + u"0", "Isolated UTF16 surrogate was not preserved");
}

void WorkFailureAndOverflow() {
    std::int32_t counter = 17;
    std::vector<std::string> observed;
    Reject([&] {
        static_cast<void>(MakeUnique("Thing", counter, [&](std::string_view name) {
            observed.emplace_back(name); return true;
        }, 3u));
    }, "Occupied candidate work cap did not refuse");
    Require(counter == 20 && observed == std::vector<std::string>{"Thing17", "Thing18", "Thing19"},
        "Work-cap failure silently rolled back/reused consumed naming counters");
    Reject([&] {
        static_cast<void>(MakeUnique("Thing", counter, [](std::string_view) -> bool {
            throw std::runtime_error("Fixture collision callback failed");
        }));
    }, "Collision callback failure did not propagate");
    Require(counter == 21, "Callback failure silently rolled back its consumed candidate");

    counter = std::numeric_limits<std::int32_t>::max() - 1;
    Require(MakeUnique("Thing", counter, [](std::string_view) { return false; }) == "Thing2147483646" &&
        counter == std::numeric_limits<std::int32_t>::max(), "Last safe naming counter was not consumed");
    std::size_t calls{};
    Reject([&] {
        static_cast<void>(MakeUnique("Thing", counter, [&](std::string_view) { ++calls; return false; }));
    }, "Signed naming counter overflow did not refuse");
    Require(counter == std::numeric_limits<std::int32_t>::max() && calls == 0u,
        "Overflow modified the counter or called the collision checker");

    counter = std::numeric_limits<std::int32_t>::max() - 1;
    calls = 0u;
    Reject([&] {
        static_cast<void>(MakeUnique("Thing", counter, [&](std::string_view) { ++calls; return true; }));
    }, "Collision into naming counter overflow did not refuse");
    Require(counter == std::numeric_limits<std::int32_t>::max() && calls == 1u,
        "Overflow after collision erased the already consumed candidate");
}

void InputBounds() {
    std::int32_t counter = 4;
    std::size_t calls{};
    const auto vacant = [&](std::string_view) { ++calls; return false; };
    for (const auto& invalid : {std::string{}, std::string("Cl\0ass", 6u), std::string("\xc0\x80", 2u),
            std::string("\xf0\x9f", 2u), std::string("\x80", 1u),
            std::string(QuestVr::ObjectNames::MaximumClassNameBytes + 1u, 'A')}) {
        Reject([&] { static_cast<void>(MakeUnique(invalid, counter, vacant)); },
            "Malformed/overbound class name was accepted");
        Require(counter == 4 && calls == 0u, "Invalid class name consumed a naming counter");
    }
    for (const auto cap : {std::size_t{0u}, QuestVr::ObjectNames::MaximumCandidates + 1u}) {
        Reject([&] { static_cast<void>(MakeUnique("Thing", counter, vacant, cap)); },
            "Invalid candidate cap was accepted");
        Require(counter == 4 && calls == 0u, "Invalid candidate cap consumed a naming counter");
    }
    counter = -1;
    Reject([&] { static_cast<void>(MakeUnique("Thing", counter, vacant)); },
        "Negative unsupported naming counter was accepted");
    Require(counter == -1 && calls == 0u, "Negative naming counter was altered");
    counter = 0;
    const auto maximumInput = std::string(QuestVr::ObjectNames::MaximumClassNameBytes, 'A');
    Require(MakeUnique(maximumInput, counter, vacant) == std::string(61u, 'A') + "0" && counter == 1,
        "Exact class-name byte bound was incorrectly rejected");
}
} // namespace

int main() {
    try {
        ClassNamesAndDigits();
        OuterCollisionsAndConcreteClasses();
        Utf16Truncation();
        WorkFailureAndOverflow();
        InputBounds();
        std::cout << "Retail UObject unique names: " << checks << " checks, " << rejections << " refusals passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Retail UObject unique names failed: " << error.what() << '\n';
        return 1;
    }
}
