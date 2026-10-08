#include "Precomp.h"
#include "surreal_portable_package_tables.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using Bytes = std::vector<std::uint8_t>;
std::size_t checks{}, rejections{};
void Require(const bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
    ++checks;
}
void U32(Bytes& bytes, const std::uint32_t value) {
    for (unsigned shift = 0u; shift < 32u; shift += 8u)
        bytes.push_back(static_cast<std::uint8_t>(value >> shift));
}
void U64(Bytes& bytes, const std::uint64_t value) {
    U32(bytes, static_cast<std::uint32_t>(value));
    U32(bytes, static_cast<std::uint32_t>(value >> 32u));
}
void Index(Bytes& bytes, const std::int32_t value) {
    auto magnitude = static_cast<std::uint32_t>(value < 0 ? -static_cast<std::int64_t>(value) : value);
    auto first = static_cast<std::uint8_t>((value < 0 ? 0x80u : 0u) | (magnitude & 63u));
    magnitude >>= 6u;
    if (magnitude != 0u) first |= 64u;
    bytes.push_back(first);
    while (magnitude != 0u) {
        auto next = static_cast<std::uint8_t>(magnitude & 127u);
        magnitude >>= 7u;
        if (magnitude != 0u) next |= 128u;
        bytes.push_back(next);
    }
}
Bytes Properties() {
    Bytes result; Index(result, 1); result.push_back(0x22u); U32(result, 0x89abcdefu);
    Index(result, 2); result.push_back(0x83u); Index(result, 0);
    return result;
}
Bytes StackBytes(const std::int32_t function, const std::int32_t state,
    const std::uint64_t mask, const std::uint32_t latent, const std::int32_t offset) {
    Bytes result; Index(result, function); Index(result, state); U64(result, mask); U32(result, latent);
    if (function != 0) Index(result, offset);
    return result;
}
struct Fixture {
    PortablePackageTables package;
    std::filesystem::path directory, path;
    Fixture() {
        const auto parent = std::filesystem::canonical(std::filesystem::temp_directory_path());
        const auto stamp = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        for (unsigned i = 0u; i < 20u; ++i) {
            const auto candidate = parent / ("deusex-object-stack-test-" + std::to_string(stamp) + '-' + std::to_string(i));
            if (!std::filesystem::create_directory(candidate)) continue;
            directory = std::filesystem::canonical(candidate);
            Require(directory.parent_path() == parent &&
                directory.filename().string().rfind("deusex-object-stack-test-", 0u) == 0u,
                "Generated stack fixture escaped temporary parent");
            break;
        }
        Require(!directory.empty(), "Could not create isolated object stack fixture");
        path = directory / "GeneratedObjectPayload.bin";
        package.sourcePath = path.string(); package.version = 68u;
        package.names = {{NameString("None"), 0}, {NameString("Counter"), 0},
            {NameString("bEnabled"), 0}, {NameString("Actor"), 0}};
        package.imports.resize(1024u);
        package.exports.resize(1024u);
        package.exports[0] = {-1, 0, 0, 3, static_cast<ObjectFlags>(0), 0, 37};
    }
    ~Fixture() {
        std::error_code ignored;
        if (!directory.empty()) std::filesystem::remove_all(directory, ignored);
    }
    PortablePropertyStream Read(const Bytes& payload, const bool hasStack) {
        {
            std::ofstream file(path, std::ios::binary | std::ios::trunc);
            Require(static_cast<bool>(file), "Could not create generated stack fixture");
            const std::array<char, 37> prefix{};
            file.write(prefix.data(), static_cast<std::streamsize>(prefix.size()));
            if (!payload.empty()) file.write(reinterpret_cast<const char*>(payload.data()),
                static_cast<std::streamsize>(payload.size()));
            Require(static_cast<bool>(file), "Could not write generated stack fixture");
        }
        package.exports[0].ObjSize = static_cast<std::int32_t>(payload.size());
        package.exports[0].ObjFlags = hasStack ? ObjectFlags::HasStack : static_cast<ObjectFlags>(0);
        return LoadPortableExportProperties(package, 0u);
    }
    void Reject(const Bytes& payload, const char* message) {
        bool rejected{};
        try { (void)Read(payload, true); } catch (const std::exception&) { rejected = true; }
        Require(rejected, message); ++rejections;
    }
};
void Synthetic() {
    Fixture fixture;
    const auto properties = Properties();
    const auto ordinary = fixture.Read(properties, false);
    Require(!ordinary.stack && ordinary.properties.size() == 2u && ordinary.bytesConsumed == properties.size(),
        "Ordinary properties gained invented stack state");
    Require(ordinary.properties[0].name == "Counter" && ordinary.properties[0].type == 2u &&
        ordinary.properties[0].value == Bytes({0xefu, 0xcdu, 0xabu, 0x89u}) &&
        ordinary.properties[1].name == "bEnabled" && ordinary.properties[1].boolValue,
        "Ordinary property values changed during stack retention");
    for (const auto& expected : std::vector<PortableObjectStack>{
        {-130, 511, 0x0123456789abcdefull, 0xfedcba98u, 2'000'000'000},
        {511, -130, std::numeric_limits<std::uint64_t>::max(), 101u, -1},
        {0, -129, 0x8000000000000001ull, 0u, {}},
        {-1, 0, 0u, std::numeric_limits<std::uint32_t>::max(), 0}}) {
        const auto header = StackBytes(expected.functionReference, expected.stateReference,
            expected.probeMask, expected.latentAction, expected.logicalOffset.value_or(0));
        auto payload = header; payload.insert(payload.end(), properties.begin(), properties.end());
        const auto result = fixture.Read(payload, true);
        Require(result.stack.has_value(), "Serialized HasStack was not retained");
        const auto& actual = *result.stack;
        Require(actual.functionReference == expected.functionReference && actual.stateReference == expected.stateReference &&
            actual.probeMask == expected.probeMask && actual.latentAction == expected.latentAction &&
            actual.logicalOffset == expected.logicalOffset, "Stack field bits/optional offset changed");
        Require(result.properties.size() == ordinary.properties.size() && result.bytesConsumed == payload.size(),
            "Retaining stack changed property traversal/consumed length");
        for (std::size_t i = 0u; i < ordinary.properties.size(); ++i) {
            const auto& a = result.properties[i]; const auto& b = ordinary.properties[i];
            Require(a.name == b.name && a.type == b.type && a.arrayIndex == b.arrayIndex &&
                a.boolValue == b.boolValue && a.value == b.value && a.valueOffset == b.valueOffset + header.size(),
                "Stack prefix corrupted ordinary tagged properties");
        }
        // Missing bytes in every header field must be rejected, not treated as
        // an empty/no-state stack. No property terminator is supplied here.
        for (std::size_t length = 0u; length < header.size(); ++length)
            fixture.Reject(Bytes(header.begin(), header.begin() + static_cast<std::ptrdiff_t>(length)),
                "Truncated serialized stack accepted");
    }
    auto invalidFunction = StackBytes(-1025, 0, 0u, 0u, 0); invalidFunction.push_back(0u);
    fixture.Reject(invalidFunction, "Out-of-range function reference accepted");
    auto invalidState = StackBytes(0, 1025, 0u, 0u, 0); invalidState.push_back(0u);
    fixture.Reject(invalidState, "Out-of-range state reference accepted");
    fixture.Reject({0xffu, 0xffu, 0xffu, 0xffu, 0xffu}, "Overflowing compact stack reference accepted");
    std::cout << "PASS exact object stack retention " << checks << " checks, " << rejections << " rejections\n";
}
std::string Qualified(const PortablePackageTables& package, const std::int32_t reference) {
    if (reference == 0) return "None";
    const auto path = GetPortableObjectPath(package, reference);
    return reference > 0 ? std::filesystem::path(package.sourcePath).stem().string() + '.' + path : path;
}
void AuditOriginal(const std::filesystem::path& root) {
    std::vector<std::filesystem::path> maps;
    for (const auto& entry : std::filesystem::directory_iterator(root / "Maps"))
        if (entry.is_regular_file() && entry.path().extension() == ".dx") maps.push_back(entry.path());
    std::sort(maps.begin(), maps.end());
    Require(!maps.empty() && maps.size() <= 256u, "Original map table count exceeds audit bound");
    std::size_t total{}, realStates{}, classes{}, nonzeroLatents{};
    std::cout << "map,exports,HasStack,functionRef,stateRef,nonzeroLatent,offsetMinusOne,classBacked,stateBacked\n";
    for (const auto& map : maps) {
        Require(std::filesystem::file_size(map) <= 256u * 1024u * 1024u, "Map file exceeds readonly audit bound");
        const auto package = LoadPortablePackageTables(map.string());
        Require(package.exports.size() <= 100'000u && package.imports.size() <= 100'000u,
            "Original map metadata exceeds readonly audit bound");
        std::size_t stacks{}, functions{}, states{}, latent{}, minusOne{}, classBacked{}, stateBacked{};
        for (std::size_t i = 0u; i < package.exports.size(); ++i) {
            const auto& entry = package.exports[i];
            if (!AnyFlags(entry.ObjFlags, ObjectFlags::HasStack)) continue;
            Require(entry.ObjSize > 0 && entry.ObjSize <= 16 * 1024 * 1024, "Stack export exceeds readonly payload bound");
            const auto result = LoadPortableExportProperties(package, i);
            Require(result.stack.has_value(), "Original HasStack header absent from retained stream");
            const auto& stack = *result.stack;
            bool actualState{};
            ++stacks; functions += stack.functionReference != 0; states += stack.stateReference != 0;
            latent += stack.latentAction != 0u; minusOne += stack.logicalOffset == std::optional<std::int32_t>(-1);
            if (stack.stateReference < 0) {
                const auto& imported = package.imports[static_cast<std::size_t>(-stack.stateReference - 1)];
                const auto meta = package.names[static_cast<std::size_t>(imported.ClassName)].Name.ToString();
                classBacked += meta == "Class"; stateBacked += meta == "State";
                actualState = meta == "State";
            } else if (stack.stateReference > 0) {
                const auto& referenced = package.exports[static_cast<std::size_t>(stack.stateReference - 1)];
                classBacked += referenced.ObjClass == 0;
                const auto meta = GetPortableObjectPath(package, referenced.ObjClass);
                actualState = meta == "Core.State" || meta == "State";
                stateBacked += actualState;
            }
            const auto name = package.names[static_cast<std::size_t>(entry.ObjName)].Name.ToString();
            if ((map.stem() == "00_Training" && (name == "Doctor1" || name == "RepairBot0")) ||
                (map.stem() == "00_Intro" && name == "Pigeon0") || (actualState && realStates == 0u)) {
                std::cout << "EXAMPLE " << map.stem().string() << '.' << name << " function=" <<
                    Qualified(package, stack.functionReference) << " state=" << Qualified(package, stack.stateReference) <<
                    " mask=" << stack.probeMask << " latent=" << stack.latentAction << " offset=" <<
                    (stack.logicalOffset ? std::to_string(*stack.logicalOffset) : "absent") << '\n';
            }
        }
        total += stacks; classes += classBacked; realStates += stateBacked; nonzeroLatents += latent;
        std::cout << map.stem().string() << ',' << package.exports.size() << ',' << stacks << ',' << functions << ',' <<
            states << ',' << latent << ',' << minusOne << ',' << classBacked << ',' << stateBacked << '\n';
    }
    std::cout << "AUDIT maps=" << maps.size() << " stacks=" << total << " classBacked=" << classes <<
        " stateBacked=" << realStates << " nonzeroLatent=" << nonzeroLatents << '\n';
}
} // namespace

int main(int argc, char** argv) {
    try {
        Synthetic();
        if (argc == 3 && std::string(argv[1]) == "--audit-original") AuditOriginal(argv[2]);
        else Require(argc == 1, "Usage: portable_object_stack_test [--audit-original GAME_ROOT]");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
