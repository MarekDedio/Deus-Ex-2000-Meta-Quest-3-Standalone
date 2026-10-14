#include "quest_engine_config.h"

#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
std::size_t checks{};
void Require(const bool value, const char* what) {
    ++checks;
    if (!value) throw std::runtime_error(what);
}
void Root(const std::string_view text, const std::string_view expected) {
    const auto result = QuestVr::EngineConfig::ParseRootClass(text);
    Require(result && *result == expected, "INI Root selection or preserved spelling is wrong");
}
template<class Function> void Refused(Function&& function, const char* what) {
    ++checks;
    try { function(); }
    catch (const std::runtime_error&) { return; }
    throw std::runtime_error(what);
}
void Invalid(const std::string_view text) {
    Refused([&] { (void)QuestVr::EngineConfig::ParseRootClass(text); }, "Malformed INI was accepted");
}
struct TemporaryDirectory {
    std::filesystem::path path;
    TemporaryDirectory() {
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        const auto temporaryRoot = std::filesystem::temp_directory_path();
        for (unsigned suffix = 0u; suffix < 100u; ++suffix) {
            const auto candidate = temporaryRoot /
                ("quest-engine-config-" + std::to_string(stamp) + '-' + std::to_string(suffix));
            std::error_code error;
            if (std::filesystem::create_directory(candidate, error)) { path = candidate; return; }
            if (error) throw std::runtime_error("Could not create isolated INI test directory");
        }
        throw std::runtime_error("Could not select isolated INI test directory");
    }
    ~TemporaryDirectory() {
        if (!path.empty()) { std::error_code error; std::filesystem::remove_all(path, error); }
    }
};
void Write(const std::filesystem::path& path, const std::string_view text) {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) throw std::runtime_error("Could not create generated INI fixture");
    file.write(text.data(), static_cast<std::streamsize>(text.size()));
    file.close();
    if (!file) throw std::runtime_error("Could not finish generated INI fixture");
}
} // namespace

int main() {
    try {
        using namespace QuestVr::EngineConfig;
        constexpr std::string_view original = "[Engine.Engine]\r\nRoot=DeusEx.DeusExRootWindow\r\n";
        Root(original, "DeusEx.DeusExRootWindow");
        Root(" \t[ENGINE.engine] \t\r\n \tRoOt \t= \tCustom_Package.MyRoot_2 \t", "Custom_Package.MyRoot_2");
        Root("[Engine.Engine]\rRoot=My.Package.CustomRoot\r", "My.Package.CustomRoot");
        Root("[Engine.Engine]\nRoot=_Package._Root0", "_Package._Root0");
        Root("; Root=Ignore.Bad\n// a full-line comment\n[Engine.Engine]\n;Root=Ignore.Bad\n //Root=Ignore.Bad\nRoot=DeusEx.DeusExRootWindow", "DeusEx.DeusExRootWindow");
        Root("Root=Wrong.Unsectioned\n[Other]\nRoot=Wrong.Other\n[Engine.Engine]\nRoot=Right.Root\n[Other]\nRoot=Wrong.Last", "Right.Root");
        Root("[Engine.Engine]\nRoot=First.Root\nROOT=first.ROOT\n[engine.ENGINE]\nRoot=FIRST.root", "First.Root");
        Root("[Engine.Engine]\nAudioDevice=Galaxy.Audio\n[Other]\nRoot=Wrong.Root\n[engine.engine]\nRoot=DeusEx.DeusExRootWindow", "DeusEx.DeusExRootWindow");
        Root("[Engine.Engine]\nOther=\xE9\nRoot=Good.Root", "Good.Root");
        Root("[Engine.Engine]\nRoot[0]=Ignored.Root\n+Root=Ignored.Root\nRoot=Actual.Root", "Actual.Root");
        Require(!ParseRootClass(std::string_view{}), "Empty INI invented a root class");
        Require(!ParseRootClass("[Other]\nRoot=Wrong.Root"), "Unrelated section selected a root class");
        Require(!ParseRootClass("[Engine.Engine]\nRootWindow=Wrong.Root\nRoot"), "Absent Root key invented a class");
        Require(!ParseRootClass("[ Engine.Engine ]\nRoot=Wrong.Root"), "Bracket-internal whitespace was silently normalized");
        Require(!ParseRootClass("[Engine.Engine]\n=Ignored\n/\n# no recognized comment syntax"), "Ignored non-key lines invented a class");
        const std::vector<std::uint8_t> raw(original.begin(), original.end());
        Require(ParseRootClass(raw) == ParseRootClass(original), "Byte-vector INI parse differs from text parse");
        Require(!ParseRootClass(std::vector<std::uint8_t>{}), "Empty byte-vector INI invented a class");

        for (const std::string_view invalid : {"", " ", "RootOnly", ".Root", "Package.", "Package..Root",
            "1Package.Root", "Package.1Root", "Package/Root", "Package\\Root", "Package:Root", "Package.Root;comment",
            "Package.Root //comment", "\"Package.Root\"", "Class'Package.Root'", "Package.Root=Other", "Package.Ro ot",
            "Package.Root\tMore", "Package.R\xC3\xB3ot"})
            Invalid(std::string("[Engine.Engine]\nRoot=") + std::string(invalid));
        Invalid("[Engine.Engine]\nRoot=First.Root\nRoot=Second.Root");
        Invalid("[Engine.Engine]\nRoot=First.Root\n[Other]\nRoot=Other.Root\n[engine.engine]\nRoot=Second.Root");
        Invalid("[Engine.Engine]\nRoot=\nRoot=Later.Root");
        Invalid("[Engine.Engine]\nRoot=Valid.Root\nRoot=\n");
        for (const std::string_view invalid : {"[Engine.Engine", "[Engine.Engine] ;comment", "[Engine.Engine]]",
            "[[Engine.Engine]", "[]", "[Eng\xC3\xADne.Engine]\nRoot=Valid.Root", "[Engine.Engine]\nR\xC3\xB3ot=Valid.Root"})
            Invalid(invalid);
        for (const unsigned char byte : {0u, 1u, 11u, 12u, 31u, 127u}) {
            std::string invalid(original); invalid.push_back(static_cast<char>(byte)); Invalid(invalid);
        }
        Invalid(std::string("\xEF\xBB\xBF") + std::string(original)); // Unsupported encoding is not silently rewritten.
        Invalid(std::string("[Engine.Engine]\nRoot=Package.") + std::string(kMaxRootClassBytes, 'R'));
        Root(std::string("[Engine.Engine]\nRoot=Package.") + std::string(kMaxRootClassBytes - 8u, 'R'),
            std::string("Package.") + std::string(kMaxRootClassBytes - 8u, 'R'));
        Invalid('[' + std::string(kMaxNameBytes + 1u, 'S') + "]\nRoot=Good.Root");
        Invalid("[Engine.Engine]\n" + std::string(kMaxNameBytes + 1u, 'K') + "=Ignored");
        Invalid(';'+std::string(kMaxLineBytes, 'C'));
        Invalid(std::string(kMaxFileBytes + 1u, '\n'));
        Require(!ParseRootClass(std::string(kMaxFileBytes, '\n')), "Bounded newline-only INI did not parse");
        Require(!ParseRootClass(';' + std::string(kMaxLineBytes - 1u, 'C')), "Maximum-length comment was refused");

        // Generated filesystem fixtures; never modify the original installation.
        TemporaryDirectory fixture;
        Require(!ReadRootClass(fixture.path), "Missing System/DeusEx.ini invented a root class");
        Require(!ReadRootClassFile(fixture.path / "missing.ini"), "Missing explicit INI invented a class");
        std::filesystem::create_directory(fixture.path / "System");
        const auto ini = fixture.path / "System" / "DeusEx.ini";
        Write(ini, original);
        Require(ReadRootClass(fixture.path) == std::optional<std::string>("DeusEx.DeusExRootWindow"), "Owned-root reader selected the wrong path/class");
        Require(ReadRootClassFile(ini) == ReadRootClass(fixture.path), "Explicit and owned-root file readers differ");
        Write(fixture.path / "DeusEx.ini", "[Engine.Engine]\nRoot=Wrong.Location");
        Require(ReadRootClass(fixture.path) == std::optional<std::string>("DeusEx.DeusExRootWindow"), "Reader fell back to a different INI location");
        Write(ini, "[Other]\nRoot=Wrong.Section");
        Require(!ReadRootClass(fixture.path), "File with missing key invented a class");
        Write(ini, "[Engine.Engine]\nRoot=Bad/Path");
        Refused([&] { (void)ReadRootClass(fixture.path); }, "Invalid file Root was accepted");
        Write(ini, std::string(kMaxFileBytes + 1u, '\n'));
        Refused([&] { (void)ReadRootClass(fixture.path); }, "Oversized INI file was accepted");
        Refused([&] { (void)ReadRootClassFile(fixture.path / "System"); }, "Directory was treated as a missing INI");
        Write(ini, {});
        Require(!ReadRootClass(fixture.path), "Empty INI file invented a class");

        std::cout << "PASS original engine Root config: " << checks
            << " checks; owned path, bounded ASCII identity, comments/CRLF, absent settings, same-identity duplicates and explicit ambiguous/malformed refusals\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
