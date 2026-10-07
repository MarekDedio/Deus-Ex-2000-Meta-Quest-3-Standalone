#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace QuestVr {
// DXQS sidecar records align with raw DXQM chunks, before native GPU splitting.
// This contains derived surface/zone indices only, never original game bytes.
struct WorldSurfaceRecord { std::int32_t surface{}, zone{}; };
struct WorldSurfaceChunk {
    std::int32_t materialSlot{};
    std::vector<WorldSurfaceRecord> records;
};
static_assert(sizeof(WorldSurfaceRecord) == 8u, "DXQS record layout changed");
inline constexpr std::uint32_t kWorldSurfaceStreamMagic = 0x53515844u; // DXQS, little-endian.
inline constexpr std::uint32_t kWorldSurfaceStreamVersion = 1u;
inline constexpr std::size_t kMaximumWorldSurfaceChunks = 128u;
inline constexpr std::size_t kMaximumWorldSurfaceChunkVertices = 60'000u;
inline constexpr std::size_t kMaximumWorldSurfaceVertices = 8'000'000u;

namespace WorldSurfaceDetail {
inline std::filesystem::path Utf8Path(const std::string& path) {
#if defined(__cpp_char8_t)
    return std::filesystem::path(reinterpret_cast<const char8_t*>(path.c_str()));
#else
    return std::filesystem::u8path(path);
#endif
}
inline void ValidateChunk(const WorldSurfaceChunk& chunk) {
    if ((chunk.materialSlot != 0 && chunk.materialSlot != -1) || chunk.records.empty() ||
        chunk.records.size() > kMaximumWorldSurfaceChunkVertices || chunk.records.size()%3u != 0u)
        throw std::runtime_error("DXQS invalid material slot or chunk vertex count");
    for (const auto& record : chunk.records)
        if (record.surface < 0 || record.zone < 0 || record.zone > 63)
            throw std::runtime_error("DXQS surface or zone index is invalid");
    for (std::size_t i = 0; i < chunk.records.size(); i += 3u)
        if (chunk.records[i].surface != chunk.records[i+1u].surface ||
            chunk.records[i].surface != chunk.records[i+2u].surface ||
            chunk.records[i].zone != chunk.records[i+1u].zone || chunk.records[i].zone != chunk.records[i+2u].zone)
            throw std::runtime_error("DXQS triangle mixes surface or zone indices");
}
inline void Write32(std::ofstream& output, const std::uint32_t value) {
    char bytes[4];
    for (unsigned i = 0; i < 4; ++i) bytes[i] = static_cast<char>(value >> (i*8u));
    if (!output.write(bytes, 4)) throw std::runtime_error("DXQS write failed");
}
inline std::uint32_t Read32(std::ifstream& input) {
    unsigned char bytes[4]{};
    if (!input.read(reinterpret_cast<char*>(bytes), 4)) throw std::runtime_error("DXQS header or record is truncated");
    std::uint32_t value{};
    for (unsigned i = 0; i < 4; ++i) value |= static_cast<std::uint32_t>(bytes[i]) << (i*8u);
    return value;
}
} // namespace WorldSurfaceDetail

inline void WriteWorldSurfaceStream(const std::string& path, const std::vector<WorldSurfaceChunk>& chunks) {
    if (path.empty() || path.find('\0') != std::string::npos || chunks.empty() || chunks.size() > kMaximumWorldSurfaceChunks)
        throw std::runtime_error("DXQS output path or chunk count is invalid");
    std::size_t total{};
    // Validate all caller data before opening/truncating the exact destination.
    for (const auto& chunk : chunks) {
        WorldSurfaceDetail::ValidateChunk(chunk);
        if (chunk.records.size() > kMaximumWorldSurfaceVertices-total)
            throw std::runtime_error("DXQS total vertex budget exceeded");
        total += chunk.records.size();
    }
    std::ofstream output(WorldSurfaceDetail::Utf8Path(path), std::ios::binary | std::ios::trunc);
    if (!output) throw std::runtime_error("Could not open DXQS output file");
    WorldSurfaceDetail::Write32(output, kWorldSurfaceStreamMagic);
    WorldSurfaceDetail::Write32(output, kWorldSurfaceStreamVersion);
    WorldSurfaceDetail::Write32(output, static_cast<std::uint32_t>(chunks.size()));
    for (const auto& chunk : chunks) {
        WorldSurfaceDetail::Write32(output, static_cast<std::uint32_t>(chunk.materialSlot));
        WorldSurfaceDetail::Write32(output, static_cast<std::uint32_t>(chunk.records.size()));
        for (const auto& record : chunk.records) {
            WorldSurfaceDetail::Write32(output, static_cast<std::uint32_t>(record.surface));
            WorldSurfaceDetail::Write32(output, static_cast<std::uint32_t>(record.zone));
        }
    }
    output.flush();
    if (!output) throw std::runtime_error("DXQS output flush failed");
    output.close();
    if (!output) throw std::runtime_error("DXQS output close failed");
}

inline std::vector<WorldSurfaceChunk> ReadWorldSurfaceStream(const std::string& path) {
    if (path.empty() || path.find('\0') != std::string::npos) throw std::runtime_error("DXQS input path is invalid");
    std::ifstream input(WorldSurfaceDetail::Utf8Path(path), std::ios::binary | std::ios::ate);
    if (!input) throw std::runtime_error("Could not open DXQS input file");
    const auto end = input.tellg();
    constexpr std::uint64_t maximumBytes = 12u + kMaximumWorldSurfaceChunks*8u + kMaximumWorldSurfaceVertices*8u;
    if (end < 12 || static_cast<std::uint64_t>(end) > maximumBytes)
        throw std::runtime_error("DXQS input size is truncated or oversized");
    input.seekg(0);
    if (WorldSurfaceDetail::Read32(input) != kWorldSurfaceStreamMagic ||
        WorldSurfaceDetail::Read32(input) != kWorldSurfaceStreamVersion)
        throw std::runtime_error("Expected DXQS version 1 stream");
    const auto count = WorldSurfaceDetail::Read32(input);
    if (count == 0u || count > kMaximumWorldSurfaceChunks) throw std::runtime_error("DXQS chunk count is invalid");
    std::vector<WorldSurfaceChunk> chunks;
    chunks.reserve(count);
    std::size_t total{};
    for (std::uint32_t i = 0; i < count; ++i) {
        const auto material = WorldSurfaceDetail::Read32(input), vertices = WorldSurfaceDetail::Read32(input);
        if ((material != 0u && material != 0xffffffffu) || vertices == 0u ||
            vertices > kMaximumWorldSurfaceChunkVertices || vertices%3u != 0u ||
            vertices > kMaximumWorldSurfaceVertices-total)
            throw std::runtime_error("DXQS material/chunk vertex count exceeds valid bounds");
        const auto offset = input.tellg();
        if (offset < 0 || static_cast<std::uint64_t>(offset) + static_cast<std::uint64_t>(vertices)*8u >
                static_cast<std::uint64_t>(end))
            throw std::runtime_error("DXQS records extend beyond the exact input file");
        WorldSurfaceChunk chunk;
        chunk.materialSlot = material == 0u ? 0 : -1;
        chunk.records.reserve(vertices);
        for (std::uint32_t j = 0; j < vertices; ++j) {
            const auto surface = WorldSurfaceDetail::Read32(input), zone = WorldSurfaceDetail::Read32(input);
            if (surface > static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max()) || zone > 63u)
                throw std::runtime_error("DXQS record has an invalid signed surface or zone index");
            chunk.records.push_back({static_cast<std::int32_t>(surface),static_cast<std::int32_t>(zone)});
        }
        WorldSurfaceDetail::ValidateChunk(chunk);
        total += vertices;
        chunks.push_back(std::move(chunk));
    }
    if (input.peek() != std::char_traits<char>::eof() || input.bad())
        throw std::runtime_error("DXQS stream has trailing bytes or an I/O error");
    return chunks;
}
} // namespace QuestVr
