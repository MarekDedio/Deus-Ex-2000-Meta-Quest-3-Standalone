#include "quest_world_surface_stream.h"

#include <chrono>
#include <functional>
#include <iostream>

namespace {
using Bytes = std::vector<std::uint8_t>;
using Chunk = QuestVr::WorldSurfaceChunk;
void Require(bool condition, const char* description) {
    if (!condition) throw std::runtime_error(description);
}
std::size_t rejections{};
void Reject(const std::function<void()>& action, const char* description) {
    bool rejected{};
    try { action(); } catch (const std::runtime_error&) { rejected = true; }
    Require(rejected,description);
    ++rejections;
}
struct Fixture {
    std::filesystem::path directory, file;
    Fixture() {
        const auto parent = std::filesystem::canonical(std::filesystem::temp_directory_path());
        const auto stamp = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        for (unsigned i = 0; i < 20; ++i) {
            const auto candidate = parent / ("deusex-world-surface-test-"+std::to_string(stamp)+'-'+std::to_string(i));
            if (std::filesystem::create_directory(candidate)) {
                directory = std::filesystem::canonical(candidate);
                Require(directory.parent_path() == parent, "DXQS fixture escaped temporary parent");
                file = directory / "generated-surface.dxqs";
                return;
            }
        }
        throw std::runtime_error("Could not create DXQS fixture directory");
    }
    ~Fixture() {
        std::error_code ignored;
        std::filesystem::remove(file,ignored);
        std::filesystem::remove(directory,ignored);
    }
    void WriteRaw(const Bytes& bytes) const {
        std::ofstream output(file,std::ios::binary|std::ios::trunc);
        output.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());
        Require(static_cast<bool>(output),"Could not write generated malformed stream");
    }
    Bytes ReadRaw() const {
        std::ifstream input(file,std::ios::binary|std::ios::ate);
        Require(static_cast<bool>(input),"Could not read generated stream");
        const auto size = input.tellg();
        Require(size >= 0 && size <= 64*1024*1024,"Generated fixture exceeds test budget");
        Bytes bytes(static_cast<std::size_t>(size));
        input.seekg(0); input.read(reinterpret_cast<char*>(bytes.data()),size);
        Require(static_cast<bool>(input),"Generated fixture read failed");
        return bytes;
    }
};
void Put32(Bytes& bytes,std::size_t offset,std::uint32_t value) {
    for (unsigned i=0;i<4;++i) bytes.at(offset+i)=static_cast<std::uint8_t>(value>>(i*8u));
}
bool Same(const std::vector<Chunk>& a,const std::vector<Chunk>& b) {
    if (a.size()!=b.size()) return false;
    for (std::size_t i=0;i<a.size();++i) {
        if (a[i].materialSlot!=b[i].materialSlot || a[i].records.size()!=b[i].records.size()) return false;
        for (std::size_t j=0;j<a[i].records.size();++j)
            if (a[i].records[j].surface!=b[i].records[j].surface || a[i].records[j].zone!=b[i].records[j].zone) return false;
    }
    return true;
}
void Tests() {
    Fixture fixture;
    const auto path=fixture.file.string();
    const std::vector<Chunk> source{{0,{{12,0},{12,0},{12,0},{34,63},{34,63},{34,63}}},
        {-1,{{std::numeric_limits<std::int32_t>::max(),7},{std::numeric_limits<std::int32_t>::max(),7},
             {std::numeric_limits<std::int32_t>::max(),7}}}};
    QuestVr::WriteWorldSurfaceStream(path,source);
    Require(Same(QuestVr::ReadWorldSurfaceStream(path),source),"DXQS raw chunk/record roundtrip differs");
    const auto valid=fixture.ReadRaw();
    Require(valid.size()==12u+8u*2u+8u*9u && valid[0]=='D' && valid[1]=='X' && valid[2]=='Q' && valid[3]=='S' &&
            valid[4]==1u && valid[8]==2u && valid[16]==6u && valid[20]==12u && valid[48]==63u,
            "DXQS little-endian header/count/record offsets differ");
    const auto malformed=[&](const Bytes& bytes,const char* message) {
        fixture.WriteRaw(bytes);
        Reject([&]{(void)QuestVr::ReadWorldSurfaceStream(path);},message);
    };
    for (std::size_t i=0;i<valid.size();++i)
        malformed(Bytes(valid.begin(),valid.begin()+i),"Truncated DXQS accepted");
    auto bad=valid; bad.push_back(0); malformed(bad,"Trailing DXQS bytes accepted");
    for (const auto& pair : {std::pair<std::size_t,std::uint32_t>{0u,0u},{4u,2u},{8u,0u},{8u,129u},
            {8u,0xffffffffu},{12u,1u},{16u,0u},{16u,2u},{16u,60003u},{16u,0xffffffffu},
            {20u,0xffffffffu},{24u,64u},{24u,0xffffffffu},{28u,13u},{32u,1u}}) {
        bad=valid; Put32(bad,pair.first,pair.second);
        malformed(bad,"Malformed DXQS header/count/surface/zone/triangle accepted");
    }
    fixture.WriteRaw(valid);
    const auto invalidWrite=[&](std::vector<Chunk> chunks,const char* message) {
        Reject([&]{QuestVr::WriteWorldSurfaceStream(path,chunks);},message);
        Require(fixture.ReadRaw()==valid,"Invalid caller data truncated the existing stream before validation");
    };
    invalidWrite({},"Zero writer chunks accepted");
    invalidWrite(std::vector<Chunk>(129u,source[0]),"Excessive writer chunk count accepted");
    auto invalid=source; invalid[0].materialSlot=1; invalidWrite(invalid,"Unsupported writer material slot accepted");
    invalid=source; invalid[0].records.clear(); invalidWrite(invalid,"Empty writer chunk accepted");
    invalid=source; invalid[0].records.pop_back(); invalidWrite(invalid,"Nontriangle writer vertex count accepted");
    invalid=source; invalid[0].records.resize(60003u,{1,0}); invalidWrite(invalid,"Oversized writer vertex count accepted");
    invalid=source; invalid[0].records[0].surface=-1; invalidWrite(invalid,"Negative writer surface accepted");
    invalid=source; invalid[0].records[0].zone=-1; invalidWrite(invalid,"Negative writer zone accepted");
    invalid=source; invalid[0].records[0].zone=64; invalidWrite(invalid,"Oversized writer zone accepted");
    invalid=source; invalid[0].records[1].surface=13; invalidWrite(invalid,"Mixed writer triangle surface accepted");
    invalid=source; invalid[0].records[1].zone=1; invalidWrite(invalid,"Mixed writer triangle zone accepted");
    Reject([&]{QuestVr::WriteWorldSurfaceStream("",source);},"Empty writer path accepted");
    Reject([&]{(void)QuestVr::ReadWorldSurfaceStream("");},"Empty reader path accepted");
    Reject([&]{QuestVr::WriteWorldSurfaceStream(fixture.directory.string(),source);},"Directory writer path accepted");
    Reject([&]{(void)QuestVr::ReadWorldSurfaceStream((fixture.directory/"missing.dxqs").string());},"Missing reader file accepted");
    // Maximum legal raw chunk count and vertex count; still bounded to 7.68M
    // records by the combined limits, below the explicit 8M total budget.
    std::vector<Chunk> maximum(128u,{0,std::vector<QuestVr::WorldSurfaceRecord>(3u,{0,0})});
    maximum[0].records.assign(60000u,{123,63});
    QuestVr::WriteWorldSurfaceStream(path,maximum);
    Require(Same(QuestVr::ReadWorldSurfaceStream(path),maximum),"Maximum legal chunk/vertex bounds rejected");
    std::cout << "DXQS LE/raw chunk and surface-zone roundtrip, legal bounds, "<<rejections
              <<" malformed/I/O controls and prewrite validation preservation passed.\n";
}
} // namespace
int main() {
    try { Tests(); return 0; }
    catch(const std::exception& error) { std::cerr<<"World surface stream test failed: "<<error.what()<<'\n'; return 1; }
}
