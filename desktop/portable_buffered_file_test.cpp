#include "portable_buffered_file.h"
#include "Package/PackageStream.h"

#include <array>
#include <functional>
#include <iostream>
#include <numeric>
#include <string>

namespace {
using QuestVr::PortableBufferedFile;
std::size_t checks{}, rejections{};
struct FaultToken { int identity{}; };
void Require(const bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
    ++checks;
}
void Reject(const std::function<void()>& action, const std::string& message) {
    bool refused{};
    try { action(); } catch (const std::exception&) { refused = true; }
    Require(refused, message); ++rejections;
}
void FaultReject(const std::function<void()>& action, const int identity, const std::string& message) {
    bool refused{};
    try { action(); } catch (const FaultToken& fault) { refused = fault.identity == identity; }
    Require(refused, message); ++rejections;
}
std::uint8_t Pattern(const std::uint64_t offset) { return static_cast<std::uint8_t>((offset * 17u + 29u) % 251u); }

// Exact-read fake matches the pinned Windows/std::stdio partial-copy + throw
// contract. It never opens or mutates a filesystem path.
class FakeFile final : public File {
public:
    explicit FakeFile(const std::uint64_t length) : length_(length), reportedSize(static_cast<std::int64_t>(length)) {}
    explicit FakeFile(std::vector<std::uint8_t> bytes)
        : length_(bytes.size()), bytes_(std::move(bytes)), reportedSize(static_cast<std::int64_t>(length_)) {}
    std::int64_t size() override {
        ++sizeCalls;
        if (failSize) { if (nonStandardSize) throw FaultToken{40}; throw std::runtime_error("synthetic size error"); }
        return reportedSize;
    }
    std::uint64_t tell() override {
        ++tellCalls;
        if (failTell) { if (nonStandardTell) throw FaultToken{42}; throw std::runtime_error("synthetic tell error"); }
        return overrideTell ? tellOverride : cursor;
    }
    void seek(const std::int64_t offset, const SeekPoint origin = SeekPoint::begin) override {
        ++seekCalls;
        if (offset < 0 || origin != SeekPoint::begin) throw std::runtime_error("fake expects absolute source seeks");
        if (failSeek) {
            if (moveBeforeSeekFailure) cursor = static_cast<std::uint64_t>(offset);
            if (nonStandardSeek) throw FaultToken{43};
            throw std::runtime_error("synthetic seek error");
        }
        cursor = static_cast<std::uint64_t>(offset);
    }
    void write(const void*, std::size_t) override { ++writeCalls; throw std::runtime_error("synthetic read-only source"); }
    void read(void* output, const std::size_t count) override {
        ++readCalls; maximumRead = std::max(maximumRead, count);
        auto* destination = static_cast<std::uint8_t*>(output);
        const auto available = cursor < length_ ? length_ - cursor : 0u;
        const auto permitted = std::min<std::uint64_t>(available,
            failOnReadCall == 0u || failOnReadCall == readCalls ? failReadAfter : std::numeric_limits<std::uint64_t>::max());
        const auto copied = static_cast<std::size_t>(std::min<std::uint64_t>(count, permitted));
        for (std::size_t index = 0u; index < copied; ++index)
            destination[index] = bytes_.empty() ? Pattern(cursor + index) : bytes_.at(static_cast<std::size_t>(cursor + index));
        cursor += copied; bytesRead += copied;
        if (copied != count) {
            if (nonStandardRead) throw FaultToken{41};
            throw std::runtime_error("synthetic short read");
        }
    }
    std::uint64_t length_{};
    std::vector<std::uint8_t> bytes_;
    std::int64_t reportedSize{};
    std::uint64_t cursor{}, failReadAfter{std::numeric_limits<std::uint64_t>::max()};
    std::uint64_t tellOverride{};
    std::size_t readCalls{}, seekCalls{}, tellCalls{}, sizeCalls{}, writeCalls{}, maximumRead{}, bytesRead{}, failOnReadCall{};
    bool failSize{}, failTell{}, failSeek{}, moveBeforeSeekFailure{}, overrideTell{};
    bool nonStandardSize{}, nonStandardRead{}, nonStandardTell{}, nonStandardSeek{};
};

void VerifyBytes(const std::vector<std::uint8_t>& bytes, const std::uint64_t start) {
    for (std::size_t index = 0u; index < bytes.size(); ++index)
        Require(bytes[index] == Pattern(start + index), "Generated source byte changed");
}
void Basics() {
    auto source = std::make_shared<FakeFile>(1000u);
    PortableBufferedFile reader(source, 7u);
    Require(reader.size() == 1000 && source->sizeCalls == 1u, "Size was not captured once");
    for (const auto count : {1u, 2u, 3u, 6u, 7u, 8u, 23u, 2u, 4u, 8u}) {
        const auto start = reader.tell(); std::vector<std::uint8_t> bytes(count);
        reader.read(bytes.data(), bytes.size()); VerifyBytes(bytes, start);
        Require(reader.tell() == start + count, "Sequential read cursor changed");
    }
    reader.seek(3); Require(reader.tell() == 3u, "Absolute seek changed");
    reader.seek(6, SeekPoint::current); Require(reader.tell() == 9u, "Relative forward seek changed");
    reader.seek(-2, SeekPoint::current); Require(reader.tell() == 7u, "Relative backward seek changed");
    reader.seek(-5, SeekPoint::end); Require(reader.tell() == 995u, "End-relative seek changed");
    std::vector<std::uint8_t> tail(5u); reader.read(tail.data(), tail.size()); VerifyBytes(tail, 995u);
    Require(reader.tell() == 1000u, "Exact EOF read lost cursor");
    const auto reads = source->readCalls, seeks = source->seekCalls, tells = source->tellCalls;
    reader.read(nullptr, 0u); reader.read(tail.data(), 0u);
    Require(reader.tell() == 1000u && source->readCalls == reads && source->seekCalls == seeks && source->tellCalls == tells,
        "Zero read performed underlying I/O");
    Reject([&] { reader.write(tail.data(), 1u); }, "Writer was forwarded");
    Reject([&] { reader.write(nullptr, 0u); }, "Zero writer was permitted");
    Require(source->writeCalls == 0u && reader.tell() == 1000u, "Rejected writer mutated source/cursor");
    reader.seek(1500); Require(reader.tell() == 1500u, "Valid seek beyond EOF was refused");
    Reject([&] { reader.read(tail.data(), 1u); }, "Beyond EOF nonzero read fabricated bytes");
    Require(reader.tell() == 1500u, "Zero-byte EOF failure changed cursor");
    reader.seek(-1500, SeekPoint::current); Require(reader.tell() == 0u, "Beyond EOF seek recovery failed");
    Reject([&] { reader.read(nullptr, 1u); }, "Null destination was accepted");
}
void EofParity() {
    for (const auto capacity : {1u, 3u, 7u, 64u}) {
        for (const auto start : {0u, 1u, 7u, 12u, 13u, 14u, 64u}) {
            for (const auto requested : {1u, 2u, 7u, 64u}) {
                auto raw = std::make_shared<FakeFile>(13u), source = std::make_shared<FakeFile>(13u);
                PortableBufferedFile reader(source, capacity);
                std::uint8_t warm{}; reader.read(&warm, 1u); // Different physical/logical cursor before seek.
                reader.seek(start); raw->seek(start);
                std::vector<std::uint8_t> actual(requested, 0xcdu), expected(requested, 0xcdu);
                bool failed{}, rawFailed{};
                try { reader.read(actual.data(), requested); } catch (const std::runtime_error&) { failed = true; }
                try { raw->read(expected.data(), requested); } catch (const std::runtime_error&) { rawFailed = true; }
                Require(failed == rawFailed && actual == expected && reader.tell() == raw->tell(),
                    "EOF/partial-copy/error cursor differs from exact source");
            }
        }
    }
    auto empty = std::make_shared<FakeFile>(0u); PortableBufferedFile reader(empty, 1u);
    reader.read(nullptr, 0u); std::uint8_t byte = 44u;
    Reject([&] { reader.read(&byte, 1u); }, "Empty source fabricated byte");
    Require(byte == 44u && reader.tell() == 0u, "Empty source failure mutated destination/cursor");
}
void SeekChecks() {
    auto source = std::make_shared<FakeFile>(100u); PortableBufferedFile reader(source, 16u);
    reader.seek(1); std::uint8_t byte{}; reader.read(&byte, 1u);
    const auto before = reader.tell(), seeks = source->seekCalls;
    for (const auto& action : std::vector<std::function<void()>>{
        [&] { reader.seek(-1); }, [&] { reader.seek(-3, SeekPoint::current); },
        [&] { reader.seek(-101, SeekPoint::end); },
        [&] { reader.seek(std::numeric_limits<std::int64_t>::min(), SeekPoint::current); },
        [&] { reader.seek(std::numeric_limits<std::int64_t>::max(), SeekPoint::current); },
        [&] { reader.seek(std::numeric_limits<std::int64_t>::max(), SeekPoint::end); },
        [&] { reader.seek(0, static_cast<SeekPoint>(-1)); }}) {
        Reject(action, "Invalid seek was accepted");
        Require(reader.tell() == before && source->seekCalls == seeks, "Invalid seek mutated state/source");
    }
    reader.seek(1); const auto reads = source->readCalls;
    reader.read(&byte, 1u);
    Require(byte == Pattern(1u) && source->readCalls == reads, "Seek within cache reread or corrupted bytes");
    source->failSeek = true;
    Reject([&] { reader.seek(3); }, "Seek I/O failure was deferred or suppressed");
    Require(reader.tell() == source->cursor, "Failed source seek did not recover actual source cursor");
    source->failSeek = false; reader.seek(1);
    reader.read(&byte, 1u);
    Require(byte == Pattern(1u) && source->readCalls == reads + 1u, "Failed source seek retained cached bytes");

    const auto maximum = std::numeric_limits<std::int64_t>::max();
    auto huge = std::make_shared<FakeFile>(static_cast<std::uint64_t>(maximum));
    PortableBufferedFile bounded(huge, 16u); bounded.seek(maximum - 1);
    bounded.read(&byte, 1u); Require(bounded.tell() == static_cast<std::uint64_t>(maximum), "Highest valid cursor failed");
    const auto hugeReads = huge->readCalls, hugeSeeks = huge->seekCalls;
    Reject([&] { bounded.read(&byte, 1u); }, "Read span offset overflow was accepted");
    Reject([&] { bounded.seek(1, SeekPoint::current); }, "Relative seek overflow was accepted");
    Reject([&] { bounded.seek(std::numeric_limits<std::int64_t>::min(), SeekPoint::end); }, "INT64_MIN negative target accepted");
    Require(huge->readCalls == hugeReads && huge->seekCalls == hugeSeeks && bounded.tell() == static_cast<std::uint64_t>(maximum),
        "Overflow rejection touched source/cursor");
}
void ConstructionAndErrors() {
    Reject([] { PortableBufferedFile reader(nullptr); }, "Null source accepted");
    auto source = std::make_shared<FakeFile>(100u);
    Reject([&] { PortableBufferedFile reader(source, 0u); }, "Zero cache accepted");
    Reject([&] { PortableBufferedFile reader(source, PortableBufferedFile::kMaxBufferBytes + 1u); }, "Oversized cache accepted");
    source->reportedSize = -1;
    Reject([&] { PortableBufferedFile reader(source); }, "Negative source size accepted");
    source->reportedSize = 100; source->cursor = std::numeric_limits<std::uint64_t>::max();
    Reject([&] { PortableBufferedFile reader(source); }, "Unsigned source cursor accepted");
    source->cursor = 7u;
    PortableBufferedFile positioned(source, 16u); std::uint8_t byte{};
    positioned.read(&byte, 1u); Require(byte == Pattern(7u) && positioned.tell() == 8u, "Initial source position was discarded");

    auto failing = std::make_shared<FakeFile>(100u); failing->failReadAfter = 3u;
    auto exact = std::make_shared<FakeFile>(100u); exact->failReadAfter = 3u;
    std::uint8_t exactByte{}; exact->read(&exactByte, 1u);
    Require(exactByte == Pattern(0u) && exact->tell() == 1u, "Paired direct one-byte fault fixture did not succeed");
    PortableBufferedFile reader(failing, 16u); byte = 77u;
    Reject([&] { reader.read(&byte, 1u); }, "Prefetch short read was suppressed");
    Require(byte == 77u && reader.tell() == 3u, "Partial prefetch was published or actual failure cursor invented");
    std::cout << "PREFETCH ERROR CONTRACT: exact one-byte request succeeds; 16-byte prefetch fails after three bytes; "
        "caller byte unchanged, source cursor recovered, no fallback hides error\n";
    failing->failReadAfter = std::numeric_limits<std::uint64_t>::max(); reader.seek(0);
    reader.read(&byte, 1u); Require(byte == Pattern(0u) && failing->readCalls == 2u, "Failed cache was reused after explicit recovery");
    reader.seek(80); failing->failReadAfter = 2u; failing->failTell = true; byte = 55u;
    Reject([&] { reader.read(&byte, 1u); }, "Read+tell error suppressed");
    Reject([&] { static_cast<void>(reader.tell()); }, "Unknown cursor fabricated by tell");
    Reject([&] { reader.seek(1, SeekPoint::current); }, "Unknown relative cursor accepted");
    reader.read(nullptr, 0u); Require(reader.size() == 100, "Zero read or immutable size needs unknown cursor");
    failing->failTell = false; failing->failReadAfter = std::numeric_limits<std::uint64_t>::max();
    reader.seek(-1, SeekPoint::end); reader.read(&byte, 1u);
    Require(byte == Pattern(99u) && reader.tell() == 100u, "End-relative recovery failed");

    auto direct = std::make_shared<FakeFile>(100u); direct->failReadAfter = 3u;
    PortableBufferedFile bypass(direct, 8u); std::vector<std::uint8_t> output(20u, 99u);
    Reject([&] { bypass.read(output.data(), output.size()); }, "Large-read short failure suppressed");
    Require(output[0] == Pattern(0) && output[2] == Pattern(2) && output[3] == 99u && bypass.tell() == 3u,
        "Direct partial-copy/error behavior changed");
}
void LargeReads() {
    auto source = std::make_shared<FakeFile>(1000u); PortableBufferedFile reader(source, 16u);
    std::vector<std::uint8_t> bytes(320u); reader.read(bytes.data(), bytes.size()); VerifyBytes(bytes, 0u);
    Require(source->readCalls == 1u && source->maximumRead == 320u && source->bytesRead == 320u,
        "Large read was staged through cache");
    auto mixed = std::make_shared<FakeFile>(1000u); PortableBufferedFile cached(mixed, 16u);
    std::uint8_t warm{}; cached.read(&warm, 1u); bytes.resize(200u); cached.read(bytes.data(), bytes.size()); VerifyBytes(bytes, 1u);
    Require(mixed->readCalls == 2u && mixed->bytesRead == 201u, "Cached-prefix/direct-tail read duplicated or missed bytes");
    cached.seek(1); const auto reads = mixed->readCalls; cached.read(&warm, 1u);
    Require(warm == Pattern(1u) && mixed->readCalls == reads, "Large bypass corrupted retained immutable cache");
    const auto count = PortableBufferedFile::kMaxSourceReadBytes + 23u;
    auto large = std::make_shared<FakeFile>(count); PortableBufferedFile chunks(large, 64u);
    bytes.resize(count); chunks.read(bytes.data(), bytes.size()); VerifyBytes(bytes, 0u);
    Require(large->readCalls == 2u && large->maximumRead == PortableBufferedFile::kMaxSourceReadBytes && chunks.tell() == count,
        "Large source read was unbounded or chunk cursor diverged");
    auto eofSource = std::make_shared<FakeFile>(PortableBufferedFile::kMaxSourceReadBytes + 3u);
    auto raw = std::make_shared<FakeFile>(PortableBufferedFile::kMaxSourceReadBytes + 3u);
    PortableBufferedFile eof(eofSource, 64u);
    std::vector<std::uint8_t> actual(PortableBufferedFile::kMaxSourceReadBytes + 10u, 99u), expected(actual);
    Reject([&] { eof.read(actual.data(), actual.size()); }, "Chunked large EOF read suppressed failure");
    Reject([&] { raw->read(expected.data(), expected.size()); }, "Raw large EOF fixture did not fail");
    Require(actual == expected && eof.tell() == raw->tell() && eofSource->readCalls == 2u &&
        eofSource->maximumRead == PortableBufferedFile::kMaxSourceReadBytes,
        "Chunked large EOF changed source partial-copy/error cursor");
    auto maximumCache = std::make_shared<FakeFile>(3u); PortableBufferedFile maximum(maximumCache, PortableBufferedFile::kMaxBufferBytes);
    maximum.read(&warm, 1u);
    Require(maximumCache->maximumRead == 3u && warm == Pattern(0u), "Maximum cache read beyond immutable EOF");
}
void ArbitrarySourceErrors() {
    auto construction = std::make_shared<FakeFile>(100u);
    construction->failSize = true; construction->nonStandardSize = true;
    FaultReject([&] { PortableBufferedFile reader(construction, 16u); }, 40, "Constructor changed non-standard size exception");
    construction->failSize = false; construction->failTell = true; construction->nonStandardTell = true;
    FaultReject([&] { PortableBufferedFile reader(construction, 16u); }, 42, "Constructor changed non-standard tell exception");

    auto moved = std::make_shared<FakeFile>(100u); PortableBufferedFile seekable(moved, 16u);
    std::uint8_t byte{}; seekable.read(&byte, 1u);
    moved->failSeek = true; moved->moveBeforeSeekFailure = true; moved->nonStandardSeek = true;
    FaultReject([&] { seekable.seek(30); }, 43, "Move-then-throw seek exception changed");
    Require(seekable.tell() == 30u && moved->cursor == 30u, "Move-then-throw source cursor was not recovered");
    moved->failSeek = false; const auto reads = moved->readCalls; seekable.read(&byte, 1u);
    Require(byte == Pattern(30u) && moved->readCalls == reads + 1u, "Move-then-throw seek served old cached bytes");
    moved->failSeek = true; moved->failTell = true; moved->nonStandardTell = true;
    FaultReject([&] { seekable.seek(70); }, 43, "Recovery tell exception replaced original seek exception");
    Reject([&] { static_cast<void>(seekable.tell()); }, "Failed seek+tell fabricated position");
    Reject([&] { seekable.seek(-1, SeekPoint::current); }, "Failed seek+tell permitted unknown relative cursor");
    seekable.read(nullptr, 0u);
    moved->failSeek = false; moved->failTell = false; seekable.seek(0); seekable.read(&byte, 1u);
    Require(byte == Pattern(0u), "Absolute seek did not recover arbitrary seek error");

    auto positioned = std::make_shared<FakeFile>(100u); PortableBufferedFile internalSeek(positioned, 16u);
    internalSeek.read(&byte, 1u);
    positioned->failSeek = true; positioned->moveBeforeSeekFailure = true; positioned->nonStandardSeek = true;
    std::vector<std::uint8_t> untouched(101u, 99u);
    FaultReject([&] { internalSeek.read(untouched.data(), untouched.size()); }, 43,
        "Read positioning seek exception changed or was suppressed");
    Require(internalSeek.tell() == 1u && positioned->readCalls == 1u &&
        std::all_of(untouched.begin(), untouched.end(), [](const auto value) { return value == 99u; }),
        "Failed source positioning seek read destination or retained fictitious cursor");
    positioned->failSeek = false; internalSeek.read(&byte, 1u);
    Require(byte == Pattern(1u) && positioned->readCalls == 2u, "Failed internal seek retained cached bytes");

    auto direct = std::make_shared<FakeFile>(100u); direct->nonStandardRead = true; direct->failReadAfter = 3u;
    PortableBufferedFile failedRead(direct, 8u); std::vector<std::uint8_t> partial(20u, 99u);
    FaultReject([&] { failedRead.read(partial.data(), partial.size()); }, 41, "Non-standard read exception changed");
    Require(failedRead.tell() == 3u && partial[2] == Pattern(2u) && partial[3] == 99u,
        "Non-standard direct read lost exact partial copy/cursor");
    direct->failTell = true; direct->nonStandardTell = true;
    FaultReject([&] { failedRead.read(partial.data(), partial.size()); }, 41,
        "Non-standard recovery tell replaced original read exception");
    Reject([&] { static_cast<void>(failedRead.tell()); }, "Non-standard read+tell failure invented position");

    auto invalidTell = std::make_shared<FakeFile>(100u); PortableBufferedFile invalid(invalidTell, 16u);
    invalidTell->overrideTell = true; invalidTell->tellOverride = std::numeric_limits<std::uint64_t>::max();
    invalidTell->failReadAfter = 0u;
    Reject([&] { invalid.read(&byte, 1u); }, "Read failure with oversized tell did not throw");
    Reject([&] { static_cast<void>(invalid.tell()); }, "Read recovery accepted unsigned overflow cursor");
    invalidTell->overrideTell = false; invalidTell->failReadAfter = std::numeric_limits<std::uint64_t>::max();
    invalid.seek(2); invalid.read(&byte, 1u);
    Require(byte == Pattern(2u), "Invalid source tell prevented explicit seek recovery");

    const auto firstChunk = PortableBufferedFile::kMaxSourceReadBytes;
    auto later = std::make_shared<FakeFile>(firstChunk + 20u); PortableBufferedFile chunked(later, 16u);
    later->failOnReadCall = 2u; later->failReadAfter = 3u; later->nonStandardRead = true;
    partial.assign(firstChunk + 20u, 99u);
    FaultReject([&] { chunked.read(partial.data(), partial.size()); }, 41, "Later direct chunk failure was suppressed");
    Require(chunked.tell() == firstChunk + 3u && later->readCalls == 2u && later->maximumRead == firstChunk &&
        partial[0] == Pattern(0u) && partial[firstChunk - 1u] == Pattern(firstChunk - 1u) &&
        partial[firstChunk + 2u] == Pattern(firstChunk + 2u) &&
        std::all_of(partial.begin() + static_cast<std::ptrdiff_t>(firstChunk + 3u), partial.end(),
            [](const auto value) { return value == 99u; }), "Later chunk error changed partial destination/cursor");
}
void AppendIndex(std::vector<std::uint8_t>& bytes, const std::int32_t value) {
    auto magnitude = static_cast<std::uint32_t>(value < 0 ? -static_cast<std::int64_t>(value) : value);
    auto first = static_cast<std::uint8_t>(magnitude & 0x3fu); magnitude >>= 6u;
    if (value < 0) first |= 0x80u;
    if (magnitude != 0u) first |= 0x40u;
    bytes.push_back(first);
    while (magnitude != 0u) {
        auto next = static_cast<std::uint8_t>(magnitude & 0x7fu); magnitude >>= 7u;
        if (magnitude != 0u) next |= 0x80u;
        bytes.push_back(next);
    }
}
void ActualPackageStream() {
    for (const auto capacity : {1u, 3u, 7u, 64u}) {
        auto source = std::make_shared<FakeFile>(1000u), raw = std::make_shared<FakeFile>(1000u);
        auto reader = std::make_shared<PortableBufferedFile>(source, capacity);
        PackageStream actual(nullptr, reader), expected(nullptr, raw);
        for (unsigned iteration = 0u; iteration < 20u; ++iteration) {
            Require(actual.ReadInt8() == expected.ReadInt8(), "PackageStream int8 differs");
            Require(actual.ReadInt16() == expected.ReadInt16(), "PackageStream int16 differs");
            Require(actual.ReadInt32() == expected.ReadInt32(), "PackageStream int32 differs");
            Require(actual.ReadInt64() == expected.ReadInt64(), "PackageStream int64 differs");
            const auto first = actual.ReadFloat(), second = expected.ReadFloat();
            Require(std::memcmp(&first, &second, sizeof(first)) == 0, "PackageStream float bits differ");
            Require(actual.Tell() == expected.Tell(), "PackageStream scalar cursor differs");
        }
        actual.Seek(11); raw->seek(11); actual.Skip(7); raw->seek(18);
        Require(actual.ReadUInt32() == expected.ReadUInt32() && actual.Tell() == expected.Tell(), "PackageStream Seek/Skip differs");
        const std::vector<std::int32_t> values{0, 1, -1, 63, -63, 64, -64, 8191, -8191, 8192,
            -8192, 1048575, -1048575, std::numeric_limits<std::int32_t>::max(), -std::numeric_limits<std::int32_t>::max()};
        std::vector<std::uint8_t> encoded;
        for (unsigned repeat = 0u; repeat < 20u; ++repeat) for (const auto value : values) AppendIndex(encoded, value);
        auto indexes = std::make_shared<FakeFile>(encoded); auto indexed = std::make_shared<PortableBufferedFile>(indexes, capacity);
        PackageStream stream(nullptr, indexed);
        for (unsigned repeat = 0u; repeat < 20u; ++repeat) for (const auto value : values)
            Require(stream.ReadIndex() == value, "Actual PackageStream compact index changed");
        Require(stream.Tell() == encoded.size(), "Compact index stream consumed extra bytes");
    }
    for (const auto& truncated : std::vector<std::vector<std::uint8_t>>{{0x40u}, {0xc0u}, {0x40u, 0x80u}, {0x40u, 0x80u, 0x80u}}) {
        auto source = std::make_shared<FakeFile>(truncated); auto reader = std::make_shared<PortableBufferedFile>(source, 3u);
        PackageStream stream(nullptr, reader);
        Reject([&] { static_cast<void>(stream.ReadIndex()); }, "Truncated compact index returned a fabricated value");
        Require(stream.Tell() == truncated.size(), "Truncated compact index error cursor differs");
    }
}
void SyntheticReduction() {
    constexpr std::size_t bytes = 1024u * 1024u;
    auto raw = std::make_shared<FakeFile>(bytes), source = std::make_shared<FakeFile>(bytes);
    auto reader = std::make_shared<PortableBufferedFile>(source);
    PackageStream unbuffered(nullptr, raw), buffered(nullptr, reader);
    std::uint64_t checksum{};
    for (std::size_t offset = 0u; offset < bytes; ++offset) {
        const auto first = unbuffered.ReadUInt8(), second = buffered.ReadUInt8();
        Require(first == second && second == Pattern(offset), "Synthetic scalar stream byte mismatch"); checksum += second;
    }
    Require(raw->readCalls == bytes && source->readCalls == bytes / PortableBufferedFile::kDefaultBufferBytes &&
        raw->bytesRead == bytes && source->bytesRead == bytes && reader->tell() == bytes,
        "Synthetic buffering call reduction or aggregate byte count changed");
    std::cout << "SYNTHETIC 1MiB one-byte scalar stream: unbuffered reads=" << raw->readCalls <<
        " buffered reads=" << source->readCalls << " cache=65536 bytes checksum=" << checksum <<
        "; call-count reduction only, not wall-clock/device performance\n";
}
} // namespace

int main() {
    try {
        Basics(); EofParity(); SeekChecks(); ConstructionAndErrors(); LargeReads(); ArbitrarySourceErrors(); ActualPackageStream(); SyntheticReduction();
        std::cout << "PASS portable buffered read-only controls: checks=" << checks << " rejections=" << rejections <<
            "; fake sources only; this test is not original data/device proof\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL portable buffered read-only controls: " << error.what() << '\n';
        return 1;
    }
}
