#include "quest_engine_config.h"

#include <chrono>
#include <iostream>
#include <iterator>
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
void GameEngine(const std::string_view text, const std::string_view expected) {
    const auto result = QuestVr::EngineConfig::ParseGameEngineClass(text);
    Require(result && *result == expected, "INI GameEngine selection or preserved spelling is wrong");
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
void InvalidGameEngine(const std::string_view text) {
    Refused([&] { (void)QuestVr::EngineConfig::ParseGameEngineClass(text); },
        "Malformed GameEngine INI was accepted");
}
template<class Function> void ErrorMessage(Function&& function, const std::string_view expected) {
    ++checks;
    try { function(); }
    catch (const std::runtime_error& error) {
        if (error.what() == expected) return;
        throw std::runtime_error("INI refusal changed its exact setting-specific diagnostic");
    }
    throw std::runtime_error("INI requiring a setting-specific diagnostic was accepted");
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
std::string Read(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) throw std::runtime_error("Could not read generated INI fixture");
    std::string result{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    if (file.bad()) throw std::runtime_error("Could not finish reading generated INI fixture");
    return result;
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

        // The configured native engine is separate from the UI root. There is
        // no default, semantic fallback, key alias or cross-setting validation.
        constexpr std::string_view engineOriginal =
            "[Engine.Engine]\r\nGameEngine=DeusEx.DeusExGameEngine\r\nRoot=DeusEx.DeusExRootWindow\r\n";
        GameEngine(engineOriginal, "DeusEx.DeusExGameEngine");
        Root(engineOriginal, "DeusEx.DeusExRootWindow");
        GameEngine("\t[ENGINE.engine] \t\r\n \tGaMeEnGiNe\t = \tCustom_Package.MyGameEngine_2\t",
            "Custom_Package.MyGameEngine_2");
        GameEngine("[Engine.Engine]\rGameEngine=My.Package.ConcreteEngine\r", "My.Package.ConcreteEngine");
        GameEngine("[Engine.Engine]\nGameEngine=_Package._GameEngine0", "_Package._GameEngine0");
        GameEngine(";GameEngine=Ignore.Bad\n//GameEngine=Ignore.Bad\n[Engine.Engine]\n"
            " ;GameEngine=Ignore.Bad\n //GameEngine=Ignore.Bad\nGameEngine=Right.GameEngine", "Right.GameEngine");
        GameEngine("GameEngine=Wrong.Unsectioned\n[Other]\nGameEngine=Wrong.Other\n"
            "[Engine.Engine]\nGameEngine=Right.Engine\n[Other]\nGameEngine=Wrong.Last", "Right.Engine");
        GameEngine("[Engine.Engine]\nGameEngine=First.GameEngine\nGAMEENGINE=first.GAMEENGINE\n"
            "[Other]\nGameEngine=Ignored.Other\n[engine.ENGINE]\nGameEngine=FIRST.gameengine", "First.GameEngine");
        GameEngine("[Engine.Engine]\nGameEngine[0]=Ignored.Array\n+GameEngine=Ignored.Add\n"
            "GameEngineClass=Ignored.Suffix\nGameEngine=Actual.Engine", "Actual.Engine");
        GameEngine("[Engine.Engine]\nRoot=\nRoot=Bad/Path\nGameEngine=Actual.Engine", "Actual.Engine");
        GameEngine("[Engine.Engine]\nRoot=First.Root\nRoot=Second.Root\nGameEngine=Actual.Engine", "Actual.Engine");
        Root("[Engine.Engine]\nGameEngine=\nGameEngine=Bad/Path\nRoot=Actual.Root", "Actual.Root");
        Root("[Engine.Engine]\nGameEngine=First.Engine\nGameEngine=Second.Engine\nRoot=Actual.Root", "Actual.Root");
        GameEngine("[Engine.Engine]\nRoot=\xE9\nOther=\xE9\nGameEngine=Good.Engine", "Good.Engine");
        Root("[Engine.Engine]\nGameEngine=\xE9\nRoot=Good.Root", "Good.Root");
        Require(!ParseGameEngineClass(std::string_view{}), "Empty INI invented a GameEngine class");
        Require(!ParseGameEngineClass(original), "Root was used as a GameEngine fallback");
        Require(!ParseRootClass("[Engine.Engine]\nGameEngine=Actual.Engine"),
            "GameEngine was used as a Root fallback");
        Require(!ParseGameEngineClass("[Other]\nGameEngine=Wrong.Engine"), "Wrong section selected GameEngine");
        Require(!ParseGameEngineClass("[ Engine.Engine ]\nGameEngine=Wrong.Engine"),
            "GameEngine normalized bracket-internal spaces");
        Require(!ParseGameEngineClass("[Engine.Engine]\nGameEngineClass=Wrong.Engine\n"
            "GameEngine[0]=Wrong.Engine\n+GameEngine=Wrong.Engine\nGameEngine"), "GameEngine accepted an alias/non-key line");
        Require(!ParseGameEngineClass("[Engine.Engine]\n=Ignored\n/\n# no recognized comment syntax"),
            "Ignored lines invented GameEngine");
        const std::vector<std::uint8_t> engineRaw(engineOriginal.begin(), engineOriginal.end());
        Require(ParseGameEngineClass(engineRaw) == ParseGameEngineClass(engineOriginal),
            "Byte-vector GameEngine parse differs from text parse");
        Require(ParseRootClass(engineRaw) == ParseRootClass(engineOriginal), "Mixed byte-vector Root parse differs");
        Require(!ParseGameEngineClass(std::vector<std::uint8_t>{}), "Empty byte vector invented GameEngine");

        for (const std::string_view invalid : {"", " ", "EngineOnly", ".Engine", "Package.", "Package..Engine",
            "1Package.Engine", "Package.1Engine", "Package/Engine", "Package\\Engine", "Package:Engine", "Package.Engine;comment",
            "Package.Engine //comment", "\"Package.Engine\"", "Class'Package.Engine'", "Package.Engine=Other", "Package.En gine",
            "Package.Engine\tMore", "Package.Eng\xC3\xADne"})
            InvalidGameEngine(std::string("[Engine.Engine]\nGameEngine=") + std::string(invalid));
        InvalidGameEngine("[Engine.Engine]\nGameEngine=First.Engine\nGameEngine=Second.Engine");
        InvalidGameEngine("[Engine.Engine]\nGameEngine=First.Engine\n[Other]\nGameEngine=Ignored.Engine\n"
            "[engine.engine]\nGameEngine=Second.Engine");
        InvalidGameEngine("[Engine.Engine]\nGameEngine=\nGameEngine=Later.Engine");
        InvalidGameEngine("[Engine.Engine]\nGameEngine=Valid.Engine\nGameEngine=\n");
        for (const std::string_view invalid : {"[Engine.Engine", "[Engine.Engine] ;comment", "[Engine.Engine]]",
            "[[Engine.Engine]", "[]", "[Eng\xC3\xADne.Engine]\nGameEngine=Valid.Engine",
            "[Engine.Engine]\nGameEng\xC3\xADne=Valid.Engine"})
            InvalidGameEngine(invalid);
        for (unsigned byte = 0u; byte <= 255u; ++byte) {
            if ((byte < 32u && byte != '\t' && byte != '\r' && byte != '\n') || byte == 127u) {
                std::string invalid(engineOriginal); invalid.push_back(static_cast<char>(byte));
                InvalidGameEngine(invalid);
            }
            if (byte >= 128u) {
                const auto invalid = std::string("[Engine.Engine]\nGameEngine=Package.Eng") + static_cast<char>(byte);
                InvalidGameEngine(invalid);
                Root(invalid + "\nRoot=Good.Root", "Good.Root");
            }
        }
        InvalidGameEngine(std::string("\xEF\xBB\xBF") + std::string(engineOriginal));
        const auto longestEngine = std::string("Package.") + std::string(kMaxGameEngineClassBytes - 8u, 'E');
        GameEngine("[Engine.Engine]\nGameEngine=" + longestEngine, longestEngine);
        InvalidGameEngine("[Engine.Engine]\nGameEngine=" + longestEngine + 'E');
        InvalidGameEngine('[' + std::string(kMaxNameBytes + 1u, 'S') + "]\nGameEngine=Good.Engine");
        InvalidGameEngine("[Engine.Engine]\n" + std::string(kMaxNameBytes + 1u, 'K') + "=Ignored");
        GameEngine('[' + std::string(kMaxNameBytes, 'S') + "]\n" + std::string(engineOriginal), "DeusEx.DeusExGameEngine");
        GameEngine(std::string(kMaxNameBytes, 'K') + "=Ignored\n" + std::string(engineOriginal), "DeusEx.DeusExGameEngine");
        InvalidGameEngine(';' + std::string(kMaxLineBytes, 'C'));
        InvalidGameEngine(std::string(kMaxFileBytes + 1u, '\n'));
        Require(!ParseGameEngineClass(std::string(kMaxFileBytes, '\n')), "Maximum-size absent GameEngine INI was refused");
        Require(!ParseGameEngineClass(';' + std::string(kMaxLineBytes - 1u, 'C')), "Maximum-length GameEngine comment was refused");
        std::string maximumEngineFile(engineOriginal);
        maximumEngineFile.resize(kMaxFileBytes, '\n');
        GameEngine(maximumEngineFile, "DeusEx.DeusExGameEngine");

        // Root's pre-existing public diagnostic text is preserved exactly.
        // GameEngine uses its own setting label for the corresponding refusal.
        for (const std::string_view key : {"Root", "GameEngine"}) {
            const auto invoke = [&](const std::string_view value) {
                const auto bytes = std::string("[Engine.Engine]\n") + std::string(key) + '=' + std::string(value);
                if (key == "Root") (void)ParseRootClass(bytes);
                else (void)ParseGameEngineClass(bytes);
            };
            const auto prefix = std::string("Original engine INI ") + std::string(key) + " class ";
            ErrorMessage([&] { invoke(""); }, prefix + "is empty or oversized");
            ErrorMessage([&] { invoke(".Engine"); }, prefix + "has an empty path component");
            ErrorMessage([&] { invoke("Package/Engine"); }, prefix + "is not a qualified ASCII identifier path");
            ErrorMessage([&] { invoke("Package."); }, prefix + "must have package and class components");
            ErrorMessage([&] {
                const auto duplicate = std::string("First.Engine\n") + std::string(key) + "=Second.Engine";
                invoke(duplicate);
            }, "Conflicting original engine INI " + std::string(key) + " values; retail duplicate precedence is unproven");
        }

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

        Require(!ReadGameEngineClassFile(fixture.path / "missing.ini"), "Missing explicit INI invented GameEngine");
        Require(!ReadGameEngineClass(fixture.path / "missing-root"), "Missing owned directory invented GameEngine");
        Require(!ReadGameEngineClass(fixture.path), "Empty owned INI invented GameEngine");
        Write(ini, engineOriginal);
        const auto originalStamp = std::filesystem::last_write_time(ini);
        Require(ReadGameEngineClass(fixture.path) == std::optional<std::string>("DeusEx.DeusExGameEngine"),
            "Owned-root GameEngine reader selected the wrong path/class");
        Require(ReadGameEngineClassFile(ini) == ReadGameEngineClass(fixture.path),
            "Explicit and owned-root GameEngine readers differ");
        Require(ReadRootClass(fixture.path) == std::optional<std::string>("DeusEx.DeusExRootWindow"),
            "GameEngine file support changed Root selection");
        Write(fixture.path / "DeusEx.ini", "[Engine.Engine]\nGameEngine=Wrong.Location");
        Require(ReadGameEngineClass(fixture.path) == std::optional<std::string>("DeusEx.DeusExGameEngine"),
            "GameEngine reader fell back to another INI location");
        Require(Read(ini) == engineOriginal, "Config readers changed owned INI bytes");
        Require(std::filesystem::last_write_time(ini) == originalStamp, "Config readers changed owned INI mtime");
        Write(ini, "[Other]\nGameEngine=Wrong.Section\n[Engine.Engine]\nRoot=Actual.Root");
        Require(!ReadGameEngineClass(fixture.path), "File with missing GameEngine invented a class");
        Write(ini, "[Engine.Engine]\nRoot=Bad/Path\nGameEngine=Actual.Engine");
        Require(ReadGameEngineClass(fixture.path) == std::optional<std::string>("Actual.Engine"),
            "File reader validated an unrelated Root value");
        Write(ini, "[Engine.Engine]\nRoot=Actual.Root\nGameEngine=Bad/Path");
        Refused([&] { (void)ReadGameEngineClass(fixture.path); }, "Invalid file GameEngine was accepted");
        Require(ReadRootClass(fixture.path) == std::optional<std::string>("Actual.Root"),
            "Root file reader validated unrelated GameEngine value");
        Write(ini, "[Engine.Engine]\nGameEngine=First.Engine\nGameEngine=first.ENGINE");
        Require(ReadGameEngineClass(fixture.path) == std::optional<std::string>("First.Engine"),
            "File reader failed same-identity GameEngine duplicates");
        Write(ini, "[Engine.Engine]\nGameEngine=First.Engine\nGameEngine=Second.Engine");
        Refused([&] { (void)ReadGameEngineClass(fixture.path); }, "Conflicting file GameEngine was accepted");
        Write(ini, maximumEngineFile);
        Require(ReadGameEngineClass(fixture.path) == std::optional<std::string>("DeusEx.DeusExGameEngine"),
            "Maximum-size valid GameEngine file was refused");
        Write(ini, std::string(kMaxFileBytes + 1u, '\n'));
        Refused([&] { (void)ReadGameEngineClass(fixture.path); }, "Oversized GameEngine INI file was accepted");
        Refused([&] { (void)ReadGameEngineClassFile(fixture.path / "System"); }, "Directory was accepted as GameEngine INI");
        Write(ini, std::string("[Engine.Engine]\nGameEngine=Good.Engine\n") + '\0');
        Refused([&] { (void)ReadGameEngineClass(fixture.path); }, "NUL-containing GameEngine INI was accepted");
        Write(ini, {});
        Require(!ReadGameEngineClassFile(ini), "Empty explicit INI invented GameEngine");

        std::cout << "PASS original engine Root and GameEngine config: " << checks
            << " checks; independent owned settings, preserved Root diagnostics, bounded ASCII identity, comments/CRLF, absent settings, same-identity duplicates, read-only filesystem checks and explicit ambiguous/malformed refusals\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
