#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

// File.h, like the pinned engine's other headers, expects Array to be declared.
#include "Utils/Array.h"
#include "Utils/File.h"

namespace QuestVr {

// Bounded read-only adapter used by LoadPortablePackageTables.
//
// The source must retain the pinned File exact-read contract (short reads
// throw), and remain immutable with exclusive cursor ownership for this
// adapter's lifetime. No other reader/writer may seek/read/resize that source;
// this is not a snapshot, concurrent-access wrapper, or mutation detector.
// Prefetch changes the underlying cursor, not this adapter's public cursor.
// Unexpected I/O error timing is not identical to an unbuffered read: a small
// caller span can fail because a larger prefetch touches an unreadable range.
// No retry/fallback conceals that error. Partial prefetch is never published;
// after any source read/seek error the cache is discarded and the cursor is
// recovered from source.tell(), or remains explicitly unknown if that fails.
// Predictable EOF reads bypass prefetch, retaining the source's partial-copy,
// cursor and exception behavior. All writes, including zero-sized writes, fail.
class PortableBufferedFile final : public File {
public:
    static constexpr std::size_t kDefaultBufferBytes = 64u * 1024u;
    static constexpr std::size_t kMaxBufferBytes = 1024u * 1024u;
    // Avoid the pinned Windows FileImpl's >DWORD-size loop and never stage a
    // large payload in another allocation. Large spans go directly to callers.
    static constexpr std::size_t kMaxSourceReadBytes = 1024u * 1024u;

    explicit PortableBufferedFile(std::shared_ptr<File> source,
        const std::size_t bufferBytes = kDefaultBufferBytes)
        : source_(std::move(source)), buffer_(CheckedCapacity(bufferBytes)) {
        if (!source_) throw std::invalid_argument("Buffered reader source is null");
        size_ = source_->size();
        if (size_ < 0) throw std::runtime_error("Buffered reader source size is negative");
        position_ = source_->tell();
        if (position_ > kMaxOffset) throw std::runtime_error("Buffered reader source cursor exceeds signed offsets");
        sourcePosition_ = position_;
    }
    PortableBufferedFile(const PortableBufferedFile&) = delete;
    PortableBufferedFile& operator=(const PortableBufferedFile&) = delete;
    PortableBufferedFile(PortableBufferedFile&&) = delete;
    PortableBufferedFile& operator=(PortableBufferedFile&&) = delete;

    std::int64_t size() override { return size_; }
    std::uint64_t tell() override {
        RequirePosition();
        return position_;
    }
    void write(const void*, std::size_t) override {
        throw std::runtime_error("Buffered reader does not permit writes");
    }
    void seek(const std::int64_t offset, const SeekPoint origin = SeekPoint::begin) override {
        std::uint64_t base{};
        switch (origin) {
        case SeekPoint::begin: break;
        case SeekPoint::current: RequirePosition(); base = position_; break;
        case SeekPoint::end: base = static_cast<std::uint64_t>(size_); break;
        default: throw std::invalid_argument("Buffered reader seek origin is invalid");
        }
        std::uint64_t target{};
        if (offset >= 0) {
            const auto delta = static_cast<std::uint64_t>(offset);
            if (delta > kMaxOffset - base) throw std::out_of_range("Buffered reader seek offset overflows");
            target = base + delta;
        } else {
            // This spelling also handles INT64_MIN without signed overflow.
            const auto delta = static_cast<std::uint64_t>(-(offset + 1)) + 1u;
            if (delta > base) throw std::out_of_range("Buffered reader seek offset is negative");
            target = base - delta;
        }
        // Forward seeks immediately: do not defer a real source seek failure to
        // a subsequent read, even when the requested bytes are already cached.
        sourcePositionKnown_ = false;
        try { source_->seek(static_cast<std::int64_t>(target), SeekPoint::begin); }
        catch (...) { RecoverSourceFailure(); throw; }
        position_ = target; positionKnown_ = true;
        sourcePosition_ = target; sourcePositionKnown_ = true;
    }
    void read(void* data, const std::size_t bytes) override {
        if (bytes == 0u) return; // Includes nullptr, EOF and an unknown cursor.
        if (!data) throw std::invalid_argument("Buffered reader destination is null");
        RequirePosition();
        static_assert(sizeof(std::size_t) <= sizeof(std::uint64_t));
        if (static_cast<std::uint64_t>(bytes) > kMaxOffset - position_)
            throw std::out_of_range("Buffered reader span exceeds signed offsets");
        auto* output = static_cast<std::uint8_t*>(data);
        auto remaining = bytes;
        const auto sourceSize = static_cast<std::uint64_t>(size_);
        // A request beyond the captured immutable EOF must reach the source,
        // even if its prefix is cached. This preserves real partial-copy errors.
        if (position_ >= sourceSize || static_cast<std::uint64_t>(remaining) > sourceSize - position_) {
            while (remaining != 0u) {
                const auto count = std::min(remaining, kMaxSourceReadBytes);
                ReadDirect(output, count);
                output += count; remaining -= count;
            }
            return;
        }
        while (remaining != 0u) {
            if (position_ >= bufferStart_ && position_ - bufferStart_ < bufferedBytes_) {
                const auto offset = static_cast<std::size_t>(position_ - bufferStart_);
                const auto count = std::min(remaining, bufferedBytes_ - offset);
                std::memcpy(output, buffer_.data() + offset, count);
                output += count; remaining -= count; position_ += count;
            } else if (remaining >= buffer_.size()) {
                const auto count = std::min(remaining, kMaxSourceReadBytes);
                ReadDirect(output, count);
                output += count; remaining -= count;
            } else {
                Fill();
            }
        }
    }

private:
    static constexpr std::uint64_t kMaxOffset = static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());
    std::shared_ptr<File> source_;
    std::vector<std::uint8_t> buffer_;
    std::int64_t size_{};
    std::uint64_t position_{}, sourcePosition_{}, bufferStart_{};
    std::size_t bufferedBytes_{};
    bool positionKnown_{true}, sourcePositionKnown_{true};

    static std::size_t CheckedCapacity(const std::size_t bytes) {
        if (bytes == 0u || bytes > kMaxBufferBytes)
            throw std::invalid_argument("Buffered reader cache capacity is outside its bound");
        return bytes;
    }
    void RequirePosition() const {
        if (!positionKnown_) throw std::runtime_error("Buffered reader cursor is unknown after source I/O failure; seek required");
    }
    void PositionSource() {
        if (sourcePositionKnown_ && sourcePosition_ == position_) return;
        sourcePositionKnown_ = false;
        try { source_->seek(static_cast<std::int64_t>(position_), SeekPoint::begin); }
        catch (...) { RecoverSourceFailure(); throw; }
        sourcePosition_ = position_; sourcePositionKnown_ = true;
    }
    void RecoverSourceFailure() noexcept {
        bufferedBytes_ = 0u; sourcePositionKnown_ = false; positionKnown_ = false;
        try {
            const auto actual = source_->tell();
            if (actual <= kMaxOffset) { position_ = actual; positionKnown_ = true; }
        } catch (...) {
            // Keep the original read exception. Absolute/end seek can recover;
            // neither tell nor relative seek may invent the missing cursor.
        }
    }
    void ReadDirect(void* output, const std::size_t bytes) {
        PositionSource();
        try { source_->read(output, bytes); }
        catch (...) { RecoverSourceFailure(); throw; }
        position_ += bytes; sourcePosition_ = position_; sourcePositionKnown_ = true;
    }
    void Fill() {
        bufferedBytes_ = 0u;
        PositionSource();
        const auto available = static_cast<std::uint64_t>(size_) - position_;
        const auto count = static_cast<std::size_t>(std::min<std::uint64_t>(buffer_.size(), available));
        try { source_->read(buffer_.data(), count); }
        catch (...) { RecoverSourceFailure(); throw; }
        bufferStart_ = position_; bufferedBytes_ = count;
        sourcePosition_ = position_ + count; sourcePositionKnown_ = true;
    }
};

} // namespace QuestVr
