#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace QuestVr::EngineConfig {

// This is a bounded reader for the owned System/DeusEx.ini Root setting, not
// a general UE1 config backend. Bounds are port resource limits, not claims
// about the original engine's maximum valid INI or FName lengths.
inline constexpr std::size_t kMaxFileBytes = 1024u * 1024u;
inline constexpr std::size_t kMaxLineBytes = 16u * 1024u;
inline constexpr std::size_t kMaxNameBytes = 1024u;
inline constexpr std::size_t kMaxRootClassBytes = 1024u;

namespace Detail {
inline unsigned char FoldAscii(const unsigned char value) {
    return value >= 'A' && value <= 'Z' ? static_cast<unsigned char>(value + ('a' - 'A')) : value;
}

inline bool SameAscii(const std::string_view a, const std::string_view b) {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0u; i < a.size(); ++i)
        if (FoldAscii(static_cast<unsigned char>(a[i])) !=
            FoldAscii(static_cast<unsigned char>(b[i]))) return false;
    return true;
}

inline std::string_view Trim(const std::string_view text) {
    const auto first = text.find_first_not_of(" \t");
    if (first == std::string_view::npos) return {};
    return text.substr(first, text.find_last_not_of(" \t") - first + 1u);
}

inline void Name(const std::string_view name, const char* kind) {
    if (name.empty() || name.size() > kMaxNameBytes)
        throw std::runtime_error(std::string("Original engine INI has an empty or oversized ") + kind);
    for (const unsigned char byte : name)
        if (byte < 32u || byte >= 127u)
            throw std::runtime_error(std::string("Original engine INI has a non-ASCII ") + kind);
}

inline bool IdentifierStart(const unsigned char byte) {
    return (byte >= 'A' && byte <= 'Z') || (byte >= 'a' && byte <= 'z') || byte == '_';
}

inline void RootClass(const std::string_view path) {
    if (path.empty() || path.size() > kMaxRootClassBytes)
        throw std::runtime_error("Original engine INI Root class is empty or oversized");
    bool first = true;
    std::size_t separators = 0u;
    for (const unsigned char byte : path) {
        if (byte == '.') {
            if (first) throw std::runtime_error("Original engine INI Root class has an empty path component");
            first = true;
            ++separators;
        } else {
            if (!IdentifierStart(byte) && (first || byte < '0' || byte > '9'))
                throw std::runtime_error("Original engine INI Root class is not a qualified ASCII identifier path");
            first = false;
        }
    }
    if (first || separators == 0u)
        throw std::runtime_error("Original engine INI Root class must have package and class components");
}
} // namespace Detail

// Supported whitespace, CR/LF handling, full-line ';'/'//' comments and
// case-insensitive sections/keys follow pinned Package/IniFile.cpp. In that
// reference duplicate sections merge and GetValue(index=0) selects the first
// duplicate key. Retail duplicate precedence has not been established: this
// reader therefore accepts only same-identity Root duplicates and explicitly
// refuses conflicts. It does not unwrap quotes, strip inline comments, expand
// variables, follow includes or substitute a default root class.
inline std::optional<std::string> ParseRootClass(const std::string_view bytes) {
    if (bytes.size() > kMaxFileBytes)
        throw std::runtime_error("Original engine INI exceeds its byte limit");
    if(bytes.starts_with("\xef\xbb\xbf"))
        throw std::runtime_error("Original engine INI BOM encoding is not established");
    for (const unsigned char byte : bytes)
        if (byte == 0u || (byte < 32u && byte != '\t' && byte != '\r' && byte != '\n') || byte == 127u)
            throw std::runtime_error("Original engine INI contains a NUL or unsupported control byte");

    std::optional<std::string> rootClass;
    bool engineSection = false;
    std::size_t cursor = 0u;
    while (cursor < bytes.size()) {
        const auto end = bytes.find_first_of("\r\n", cursor);
        const auto count = (end == std::string_view::npos ? bytes.size() : end) - cursor;
        if (count > kMaxLineBytes)
            throw std::runtime_error("Original engine INI line exceeds its byte limit");
        const auto line = Detail::Trim(bytes.substr(cursor, count));
        cursor += count;
        while (cursor < bytes.size() && (bytes[cursor] == '\r' || bytes[cursor] == '\n')) ++cursor;
        if (line.empty() || line.front() == ';' || line.starts_with("//")) continue;

        if (line.front() == '[') {
            if (line.size() < 3u || line.back() != ']' ||
                line.find(']', 1u) != line.size() - 1u || line.find('[', 1u) != std::string_view::npos)
                throw std::runtime_error("Original engine INI has a malformed section header");
            const auto section = line.substr(1u, line.size() - 2u);
            Detail::Name(section, "section name");
            // Like the reference parser, spaces inside the brackets are part
            // of the name; [ Engine.Engine ] is not [Engine.Engine].
            engineSection = Detail::SameAscii(section, "Engine.Engine");
            continue;
        }

        const auto equals = line.find('=');
        if (equals == std::string_view::npos) continue; // Reference ignores non-key lines.
        const auto key = Detail::Trim(line.substr(0u, equals));
        if (key.empty()) continue; // Reference ignores an empty key.
        Detail::Name(key, "key name");
        if (!engineSection || !Detail::SameAscii(key, "Root")) continue;
        const auto value = Detail::Trim(line.substr(equals + 1u));
        Detail::RootClass(value);
        if (rootClass && !Detail::SameAscii(*rootClass, value))
            throw std::runtime_error("Conflicting original engine INI Root values; retail duplicate precedence is unproven");
        if (!rootClass) rootClass = std::string(value); // Preserve the owned setting's spelling.
    }
    return rootClass;
}

inline std::optional<std::string> ParseRootClass(const std::vector<std::uint8_t>& bytes) {
    if (bytes.empty()) return ParseRootClass(std::string_view{});
    return ParseRootClass(std::string_view(reinterpret_cast<const char*>(bytes.data()), bytes.size()));
}

// Missing is distinct from unreadable/invalid: a deployment with no INI may
// defer possession, whereas an existing malformed configuration must not be
// silently replaced with a hardcoded class. Never writes the owned INI.
inline std::optional<std::string> ReadRootClassFile(const std::filesystem::path& iniPath) {
    std::error_code error;
    const auto status = std::filesystem::status(iniPath, error);
    if (error == std::errc::no_such_file_or_directory ||
        (!error && status.type() == std::filesystem::file_type::not_found)) return std::nullopt;
    if (error) throw std::runtime_error("Could not inspect original engine INI");
    if (!std::filesystem::is_regular_file(status))
        throw std::runtime_error("Original engine INI path is not a regular file");
    const auto size = std::filesystem::file_size(iniPath, error);
    if (error) throw std::runtime_error("Could not measure original engine INI");
    if (size > kMaxFileBytes) throw std::runtime_error("Original engine INI exceeds its byte limit");
    std::ifstream file(iniPath, std::ios::binary);
    if (!file) throw std::runtime_error("Could not open original engine INI");
    std::string bytes(static_cast<std::size_t>(size), '\0');
    if (!bytes.empty()) {
        file.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        if (file.gcount() != static_cast<std::streamsize>(bytes.size()) || !file)
            throw std::runtime_error("Original engine INI changed or could not be read completely");
    }
    if (file.peek() != std::char_traits<char>::eof())
        throw std::runtime_error("Original engine INI grew while it was being read");
    if (file.bad()) throw std::runtime_error("Could not finish reading original engine INI");
    return ParseRootClass(std::string_view(bytes));
}

inline std::optional<std::string> ReadRootClass(const std::filesystem::path& originalGameRoot) {
    return ReadRootClassFile(originalGameRoot / "System" / "DeusEx.ini");
}

} // namespace QuestVr::EngineConfig
