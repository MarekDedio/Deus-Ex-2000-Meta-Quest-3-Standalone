#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace QuestVr {

// Retains little-endian v1-v4 compatibility; v5 stores map-local feet/heading
// rather than a translation tied to a previous OpenXR tracking origin.
// Parsing is bounded and transactional: failure never changes the caller's UI.
struct QuestSaveMetadata {
    std::array<float, 4> pose{};
    std::string mapName;
    std::unordered_map<std::string, std::size_t> dialogueOffsets;
    std::vector<std::string> personaLogs;
    bool mapLocalPose{};
};

inline constexpr std::size_t kQuestSaveMetadataLimit = 64u * 1024u;

namespace SaveMetadataDetail {
inline bool TextValid(const std::string& value, std::size_t limit) {
    return !value.empty() && value.size() <= limit &&
        value.find('\0') == std::string::npos;
}
inline void Put32(std::vector<std::uint8_t>& out, std::uint32_t value) {
    for (unsigned shift = 0u; shift != 32u; shift += 8u)
        out.push_back(static_cast<std::uint8_t>(value >> shift));
}
inline void PutString(std::vector<std::uint8_t>& out, const std::string& value) {
    Put32(out, static_cast<std::uint32_t>(value.size()));
    out.insert(out.end(), value.begin(), value.end());
}
struct Reader {
    const std::vector<std::uint8_t>& bytes;
    std::size_t offset{};
    bool Get32(std::uint32_t& value) {
        if (offset > bytes.size() || bytes.size() - offset < 4u) return false;
        value = 0u;
        for (unsigned shift = 0u; shift != 32u; shift += 8u)
            value |= static_cast<std::uint32_t>(bytes[offset++]) << shift;
        return true;
    }
    bool GetString(std::string& value, std::size_t limit) {
        std::uint32_t length{};
        if (!Get32(length) || length == 0u || length > limit ||
            length > bytes.size() - offset) return false;
        value.assign(reinterpret_cast<const char*>(bytes.data() + offset), length);
        offset += length;
        return TextValid(value, limit);
    }
};
} // namespace SaveMetadataDetail

inline bool EncodeQuestSaveMetadata(const QuestSaveMetadata& value,
                                    std::vector<std::uint8_t>& output) noexcept {
    try {
        using namespace SaveMetadataDetail;
        if (!TextValid(value.mapName, 255u) || value.dialogueOffsets.size() > 4096u ||
            value.personaLogs.size() > 12u) return false;
        for (const float coordinate : value.pose)
            if (!std::isfinite(coordinate)) return false;
        std::size_t required = 36u + value.mapName.size();
        for (const auto& entry : value.dialogueOffsets) {
            if (!TextValid(entry.first, 1024u)) return false;
            required += 12u + entry.first.size();
        }
        for (const auto& entry : value.personaLogs) {
            if (!TextValid(entry, 256u)) return false;
            required += 4u + entry.size();
        }
        if (required > kQuestSaveMetadataLimit) return false;
        std::vector<std::uint8_t> bytes;
        bytes.reserve(required);
        Put32(bytes, 0x4d515844u);
        Put32(bytes, value.mapLocalPose ? 5u : 4u);
        for (const float coordinate : value.pose) {
            std::uint32_t bits{};
            static_assert(sizeof(bits) == sizeof(coordinate));
            std::memcpy(&bits, &coordinate, sizeof(bits));
            Put32(bytes, bits);
        }
        PutString(bytes, value.mapName);
        // Stable ordering makes snapshots reproducible despite unordered_map.
        std::vector<std::pair<std::string, std::size_t>> offsets(
            value.dialogueOffsets.begin(), value.dialogueOffsets.end());
        std::sort(offsets.begin(), offsets.end());
        Put32(bytes, static_cast<std::uint32_t>(offsets.size()));
        for (const auto& entry : offsets) {
            PutString(bytes, entry.first);
            const auto cursor = static_cast<std::uint64_t>(entry.second);
            Put32(bytes, static_cast<std::uint32_t>(cursor));
            Put32(bytes, static_cast<std::uint32_t>(cursor >> 32u));
        }
        Put32(bytes, static_cast<std::uint32_t>(value.personaLogs.size()));
        for (const auto& entry : value.personaLogs) PutString(bytes, entry);
        output = std::move(bytes);
        return true;
    } catch (...) {
        return false;
    }
}

inline bool DecodeQuestSaveMetadata(const std::vector<std::uint8_t>& bytes,
                                    const std::string& legacyV1Map,
                                    QuestSaveMetadata& output) noexcept {
    try {
        using namespace SaveMetadataDetail;
        if (bytes.size() > kQuestSaveMetadataLimit) return false;
        Reader reader{bytes};
        std::uint32_t magic{}, version{};
        if (!reader.Get32(magic) || magic != 0x4d515844u || !reader.Get32(version) ||
            version < 1u || version > 5u) return false;
        QuestSaveMetadata decoded;
        decoded.mapLocalPose = version >= 5u;
        for (float& coordinate : decoded.pose) {
            std::uint32_t bits{};
            if (!reader.Get32(bits)) return false;
            std::memcpy(&coordinate, &bits, sizeof(bits));
            if (!std::isfinite(coordinate)) return false;
        }
        if (version >= 2u) {
            if (!reader.GetString(decoded.mapName, 255u)) return false;
        } else {
            if (!TextValid(legacyV1Map, 255u)) return false;
            decoded.mapName = legacyV1Map;
        }
        if (version >= 3u) {
            std::uint32_t count{};
            if (!reader.Get32(count) || count > 4096u) return false;
            for (std::uint32_t index = 0u; index < count; ++index) {
                std::string path;
                std::uint32_t low{}, high{};
                if (!reader.GetString(path, 1024u) || !reader.Get32(low) ||
                    !reader.Get32(high)) return false;
                const std::uint64_t cursor = low | (static_cast<std::uint64_t>(high) << 32u);
                if (cursor > std::numeric_limits<std::size_t>::max() ||
                    !decoded.dialogueOffsets.emplace(std::move(path),
                        static_cast<std::size_t>(cursor)).second) return false;
            }
        }
        if (version >= 4u) {
            std::uint32_t count{};
            if (!reader.Get32(count) || count > 12u) return false;
            for (std::uint32_t index = 0u; index < count; ++index) {
                std::string entry;
                if (!reader.GetString(entry, 256u)) return false;
                decoded.personaLogs.push_back(std::move(entry));
            }
        }
        if (reader.offset != bytes.size()) return false;
        output = std::move(decoded);
        return true;
    } catch (...) {
        return false;
    }
}

} // namespace QuestVr
