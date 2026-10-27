#include "quest_native_font_metrics.h"
#include "surreal_portable_package_tables.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using Bytes=std::vector<std::uint8_t>;
std::size_t checks{},refusals{};
void Require(const bool condition,const char* message) {++checks;if(!condition)throw std::runtime_error(message);}
void Refuses(const std::function<void()>& action,const char* message) {
    bool refused{};try {action();}catch(const std::exception&) {refused=true;}
    Require(refused,message);++refusals;
}
void Dword(Bytes& bytes,const std::uint32_t value) {
    for(unsigned shift=0u;shift<32u;shift+=8u)bytes.push_back(static_cast<std::uint8_t>(value>>shift));
}
void Index(Bytes& bytes,const std::int32_t value) {
    auto magnitude=static_cast<std::uint32_t>(value<0?-static_cast<std::int64_t>(value):value);
    auto first=static_cast<std::uint8_t>((magnitude&63u)|(value<0?128u:0u));magnitude>>=6u;
    if(magnitude)first|=64u;
    bytes.push_back(first);
    while(magnitude) {auto next=static_cast<std::uint8_t>(magnitude&127u);magnitude>>=7u;if(magnitude)next|=128u;bytes.push_back(next);}
}
void Replace(Bytes& bytes,const std::size_t offset,const std::uint32_t value) {
    for(unsigned byte=0u;byte<4u;++byte)bytes.at(offset+byte)=static_cast<std::uint8_t>(value>>(8u*byte));
}

struct Fixture {
    PortablePackageTables package;
    Bytes font,atlas,palette;
    std::vector<std::size_t> glyphOffsets;
    std::size_t cppOffset{},mipOffset{},pixelCountOffset{},widthOffset{},heightOffset{};
    explicit Fixture(const std::filesystem::path& path,const std::vector<std::uint32_t>& counts={256u},
        const std::uint32_t charactersPerPage=256u) {
        package.sourcePath=path.string();package.version=68u;
        for(const auto* name:{"None","Engine","Font","Texture","Palette","TestFont","Atlas","Core","Class","Package","Fixture","ClipRect"})
            package.names.push_back({NameString(name),0u});
        package.imports={{7,9,0,1},{7,8,-1,2},{7,8,-1,3},{7,8,-1,4}};
        package.exports={{-2,0,0,5,ObjectFlags{},0,0},{-3,0,1,6,ObjectFlags{},0,0},{-4,0,0,4,ObjectFlags{},0,0}};
        Index(font,0);Index(font,static_cast<std::int32_t>(counts.size()));
        for(std::size_t page=0u;page<counts.size();++page) {
            Index(font,2);Index(font,static_cast<std::int32_t>(counts[page]));
            for(std::uint32_t glyph=0u;glyph<counts[page];++glyph) {
                const auto code=static_cast<std::uint32_t>(page)*charactersPerPage+glyph;
                const auto width=code==32u?3u:code=='A'?5u:code=='X'?7u:code==200u?8u:code=='a'?0u:1u+(code%7u);
                const auto height=code==32u?8u:code==200u?12u:10u;
                glyphOffsets.push_back(font.size());Dword(font,0u);Dword(font,0u);Dword(font,width);Dword(font,height);
            }
        }
        cppOffset=font.size();Dword(font,charactersPerPage);
        Index(atlas,4);atlas.push_back(5u);Index(atlas,3);Index(atlas,0);
        mipOffset=atlas.size();atlas.push_back(1u);Dword(atlas,0u);
        pixelCountOffset=atlas.size();Index(atlas,128);
        for(std::size_t pixel=0u;pixel<128u;++pixel)atlas.push_back(static_cast<std::uint8_t>(pixel&1u));
        widthOffset=atlas.size();Dword(atlas,8u);heightOffset=atlas.size();Dword(atlas,16u);atlas.push_back(3u);atlas.push_back(4u);
        Index(palette,0);Index(palette,2);Dword(palette,0xffffffffu);Dword(palette,0xff010203u);
    }
    void Save() {
        std::ofstream output(package.sourcePath,std::ios::binary|std::ios::trunc);
        Require(static_cast<bool>(output),"Cannot create generated Font fixture");
        std::int32_t offset{};std::size_t entry{};
        for(const auto* bytes:{&font,&atlas,&palette}) {
            package.exports.at(entry).ObjOffset=offset;package.exports.at(entry++).ObjSize=static_cast<std::int32_t>(bytes->size());
            output.write(reinterpret_cast<const char*>(bytes->data()),static_cast<std::streamsize>(bytes->size()));
            offset+=static_cast<std::int32_t>(bytes->size());
        }
        Require(static_cast<bool>(output),"Cannot write generated Font fixture");
    }
};

std::size_t IndependentRetained(const QuestVr::NativeFontMetrics& font) {
    auto result=sizeof(font)+font.objectPath.capacity()+1u+font.pages.capacity()*sizeof(QuestVr::NativeFontMetricsPage);
    for(const auto& page:font.pages)result+=page.texturePath.capacity()+1u+page.glyphs.capacity()*sizeof(QuestVr::NativeFontGlyphMetrics);
    return result;
}
void LookupTests() {
    QuestVr::NativeFontMetrics font;font.charactersPerPage=4u;font.pages.resize(3u);
    font.pages[0u].glyphs={{1,2,3,4},{5,6,7,8}};
    font.pages[1u].glyphs={{9,10,11,12},{13,14,15,16},{17,18,19,20}};
    font.pages[2u].glyphs={};
    for(std::uint32_t code=0u;code<65'536u;++code) {
        const auto page=code/4u,slot=code%4u;
        const auto expected=page<font.pages.size() && slot<font.pages[page].glyphs.size()?font.pages[page].glyphs[slot]:QuestVr::NativeFontGlyphMetrics{};
        Require(QuestVr::LookupNativeFontGlyph(font,code)==expected,"UTF16 quotient/remainder lookup differs from independent sparse-page oracle");
        Require(QuestVr::LookupNativeFontGlyph(font,code|0xabcd0000u)==expected,"Glyph lookup did not mask high bits to one UTF16 unit");
    }
    Require(QuestVr::LookupNativeFontGlyph(font,4u)==font.pages[1u].glyphs[0u],"Sparse pages were concatenated instead of indexed by charactersPerPage");
    font.charactersPerPage=0u;Refuses([&] {static_cast<void>(QuestVr::LookupNativeFontGlyph(font,32u));},"Zero glyph divisor became fabricated fallback");
    font.charactersPerPage=65'537u;Refuses([&] {static_cast<void>(QuestVr::LookupNativeFontGlyph(font,32u));},"Out-of-range glyph divisor accepted");
}

void SyntheticTests() {
    LookupTests();
    const auto directory=std::filesystem::temp_directory_path()/("deusex-native-font-metrics-"+std::to_string(std::chrono::high_resolution_clock::now().time_since_epoch().count()));
    Require(std::filesystem::create_directory(directory),"Cannot create isolated metrics fixture directory");
    struct Cleanup {std::filesystem::path path;~Cleanup() {std::error_code ignored;std::filesystem::remove_all(path,ignored);}} cleanup{directory};
    const auto file=directory/"FontFixture.bin";Fixture valid(file);valid.Save();
    std::size_t charged{},peak{},admissions{};
    QuestVr::NativeFontMetricsLimits limits;
    limits.chargeWork=[&](const std::size_t units) {charged+=units;};
    limits.admitPeak=[&](const std::size_t bytes) {peak=std::max(peak,bytes);++admissions;};
    const auto font=QuestVr::LoadNativeFontMetrics(valid.package,0u,limits);
    Require(font.objectPath=="TestFont" && font.pages.size()==1u && font.pages[0u].texturePath=="TestFont.Atlas" &&
        font.pages[0u].textureReference==2 && font.pages[0u].atlasWidth==8u && font.pages[0u].atlasHeight==16u &&
        font.pages[0u].glyphs.size()==256u && font.charactersPerPage==256u,"Generated native metrics wire layout differs");
    Require(QuestVr::LookupNativeFontGlyph(font,'A').width==5 && QuestVr::LookupNativeFontGlyph(font,'a').width==0 &&
        QuestVr::LookupNativeFontGlyph(font,1000u)==QuestVr::NativeFontGlyphMetrics{},"Retail missing/lowercase glyph did not remain zero without fallback");
    Require(QuestVr::LookupNativeFontGlyph(font,32u).height==8 && QuestVr::LookupNativeFontGlyph(font,'X').height==10,
        "Native space-glyph height was replaced by X lineHeight");
    Require(charged==font.usedWork && peak==font.peakWorkingBytes && admissions>5u &&
        QuestVr::NativeFontMetricsRetainedBytes(font)==IndependentRetained(font),"Native metrics work/admission/capacity accounting differs");
    Fixture sparse(file,{2u,3u,0u},4u);sparse.Save();const auto split=QuestVr::LoadNativeFontMetrics(sparse.package,0u);
    Require(split.pages.size()==3u && split.pages[0u].glyphs.size()==2u && split.pages[1u].glyphs.size()==3u && split.pages[2u].glyphs.empty(),
        "Native loader padded or flattened actual sparse page counts");
    Require(QuestVr::LookupNativeFontGlyph(split,4u)==split.pages[1u].glyphs[0u] && QuestVr::LookupNativeFontGlyph(split,2u)==QuestVr::NativeFontGlyphMetrics{},
        "Loaded sparse page lookup differs");
    Fixture properties(file);Bytes prefix;
    Index(prefix,10);prefix.push_back(0x83u); // Bool true has no serialized value.
    Index(prefix,10);prefix.push_back(0xa2u);prefix.push_back(0xc0u);prefix.push_back(0u);prefix.push_back(0u);prefix.push_back(1u);Dword(prefix,123u);
    Index(prefix,10);prefix.push_back(0x4au);Index(prefix,11);for(unsigned word=0u;word<4u;++word)Dword(prefix,word);
    Index(prefix,10);prefix.push_back(0x5du);prefix.push_back(5u);for(const auto byte:{1u,2u,3u,4u,5u})prefix.push_back(static_cast<std::uint8_t>(byte));
    properties.font.insert(properties.font.begin(),prefix.begin(),prefix.end());properties.Save();
    Require(QuestVr::LoadNativeFontMetrics(properties.package,0u).pages[0u].glyphs==font.pages[0u].glyphs,
        "Native metrics property tag/Bool/array-index/Struct/variable-size skipping differs");
    Fixture stack(file);Bytes header;Index(header,0);Index(header,0);Dword(header,0xffffffffu);Dword(header,0x12345678u);Dword(header,0u);
    stack.font.insert(stack.font.begin(),header.begin(),header.end());stack.package.exports[0u].ObjFlags=ObjectFlags::HasStack;stack.Save();
    Require(QuestVr::LoadNativeFontMetrics(stack.package,0u).pages[0u].glyphs==font.pages[0u].glyphs,"HasStack prefix shifted native Font tail incorrectly");

    const auto bad=[&](const std::function<void(Fixture&)>& modify,const char* message) {
        Fixture fixture(file);modify(fixture);fixture.Save();Refuses([&] {static_cast<void>(QuestVr::LoadNativeFontMetrics(fixture.package,0u));},message);
    };
    bad([](Fixture& f) {f.font[1u]=0u;},"Zero page count accepted");
    bad([](Fixture& f) {f.font[1u]=0x81u;},"Negative page count accepted");
    bad([](Fixture& f) {f.font.erase(f.font.begin()+1);f.font.insert(f.font.begin()+1,{0x41u,0x04u});},"257 pages accepted");
    bad([](Fixture& f) {f.font[2u]=0u;},"None atlas accepted");
    bad([](Fixture& f) {f.font[2u]=0x81u;},"Imported atlas silently substituted");
    bad([](Fixture& f) {f.font[2u]=63u;},"Out-of-table atlas accepted");
    bad([](Fixture& f) {f.font[2u]=3u;},"Palette used as atlas Texture");
    bad([](Fixture& f) {f.font[3u]=0xffu;f.font[4u]=0x7fu;},"Negative glyph count accepted");
    bad([](Fixture& f) {f.font.erase(f.font.begin()+3,f.font.begin()+5);f.font.insert(f.font.begin()+3,{0x41u,0x80u,0x08u});},"65537 glyphs accepted");
    bad([](Fixture& f) {f.font.resize(f.font.size()-16u);},"Truncated glyph tail accepted");
    bad([](Fixture& f) {Replace(f.font,f.glyphOffsets['A'],0xffffffffu);},"Negative glyph rectangle accepted");
    bad([](Fixture& f) {Replace(f.font,f.glyphOffsets['A']+8u,9u);},"Glyph outside atlas width accepted");
    bad([](Fixture& f) {Replace(f.font,f.glyphOffsets['A']+4u,16u);},"Glyph outside atlas height accepted");
    bad([](Fixture& f) {Replace(f.font,f.cppOffset,0u);},"Zero charactersPerPage accepted");
    bad([](Fixture& f) {Replace(f.font,f.cppOffset,128u);},"Page larger than charactersPerPage accepted");
    bad([](Fixture& f) {Replace(f.font,f.cppOffset,65'537u);},"charactersPerPage above UTF16 space accepted");
    bad([](Fixture& f) {f.font.pop_back();},"Truncated final DWORD accepted");
    bad([](Fixture& f) {f.font.push_back(0u);},"Trailing Font data accepted");
    bad([](Fixture& f) {f.atlas[2u]=0u;},"None atlas Palette accepted");
    bad([](Fixture& f) {f.atlas[2u]=0x81u;},"Imported atlas Palette accepted");
    bad([](Fixture& f) {f.atlas[2u]=63u;},"Out-of-table atlas Palette accepted");
    bad([](Fixture& f) {f.package.exports[2u].ObjClass=-3;},"Texture used as Palette accepted");
    bad([](Fixture& f) {f.atlas[1u]=3u;},"Bool masquerading as missing Palette accepted");
    bad([](Fixture& f) {f.atlas[2u]=0x43u;},"Palette compact value read beyond its field span");
    bad([](Fixture& f) {f.atlas[f.mipOffset]=0u;},"Missing atlas mip accepted");
    bad([](Fixture& f) {f.atlas[f.mipOffset]=33u;},"Excessive atlas mips accepted");
    bad([](Fixture& f) {Replace(f.atlas,f.widthOffset,0u);},"Zero atlas width accepted");
    bad([](Fixture& f) {Replace(f.atlas,f.widthOffset,8193u);},"Oversized atlas width accepted");
    bad([](Fixture& f) {Replace(f.atlas,f.heightOffset,15u);},"Top indexed bytes/dimensions disagreement accepted");
    bad([](Fixture& f) {f.atlas.pop_back();},"Truncated atlas dimensions accepted");
    bad([](Fixture& f) {f.palette[1u]=0u;},"Empty Palette accepted");
    bad([](Fixture& f) {f.palette.pop_back();},"Truncated Palette accepted");
    bad([](Fixture& f) {f.atlas[f.pixelCountOffset+2u]=2u;},"Atlas pixel outside actual Palette count accepted");
    bad([](Fixture& f) {f.package.imports[1u].ClassName=3;},"Font class path with wrong import class provenance accepted");
    bad([](Fixture& f) {f.package.imports[1u].ObjOuter=-2;},"Cyclic class outer path accepted");
    bad([](Fixture& f) {f.package.exports[1u].ObjOuter=2;},"Cyclic atlas outer path accepted");
    bad([](Fixture& f) {f.package.names.push_back({NameString("Engine.Font"),0u});f.package.imports[1u].ObjName=12;f.package.imports[1u].ObjOuter=0;},"Flattened dotted import impersonated real Engine.Font chain");

    valid.Save();auto metadata=valid.package;metadata.version=63u;
    Refuses([&] {static_cast<void>(QuestVr::LoadNativeFontMetrics(metadata,0u));},"Legacy texture-derived Font guessed");
    metadata=valid.package;metadata.version=100u;Refuses([&] {static_cast<void>(QuestVr::LoadNativeFontMetrics(metadata,0u));},"Unknown Font package version accepted");
    metadata=valid.package;metadata.exports[0u].ObjClass=-3;Refuses([&] {static_cast<void>(QuestVr::LoadNativeFontMetrics(metadata,0u));},"Non-Font export accepted");
    metadata=valid.package;metadata.exports[0u].ObjOffset=10'000'000;Refuses([&] {static_cast<void>(QuestVr::LoadNativeFontMetrics(metadata,0u));},"Out-of-file Font offset accepted");
    metadata=valid.package;metadata.exports[0u].ObjSize=2;Refuses([&] {static_cast<void>(QuestVr::LoadNativeFontMetrics(metadata,0u));},"Truncated Font export span accepted");
    Refuses([&] {static_cast<void>(QuestVr::LoadNativeFontMetrics(valid.package,10u));},"Out-of-table export index accepted");
    metadata=valid.package;metadata.exports.push_back(metadata.exports[1u]);
    Refuses([&] {static_cast<void>(QuestVr::LoadNativeFontMetrics(metadata,0u));},"Ambiguous same-path Texture accepted");

    const auto limited=[&](const std::function<void(QuestVr::NativeFontMetricsLimits&)>& modify,const char* message) {
        auto limited=QuestVr::NativeFontMetricsLimits{};modify(limited);
        Refuses([&] {static_cast<void>(QuestVr::LoadNativeFontMetrics(valid.package,0u,limited));},message);
    };
    limited([](auto& l) {l.retainedBytes=0u;},"Zero retained budget accepted");
    limited([&](auto& l) {l.retainedBytes=QuestVr::NativeFontMetricsRetainedBytes(font)-1u;},"Late retained-byte cut ignored");
    limited([](auto& l) {l.workingBytes=0u;},"Zero working budget accepted");
    limited([](auto& l) {l.maxPayloadBytes=1u;},"Payload bound ignored");
    limited([](auto& l) {l.maxPages=0u;},"Page bound ignored");
    limited([](auto& l) {l.maxGlyphs=255u;},"Glyph bound ignored");
    limited([](auto& l) {l.maxProperties=0u;},"Property bound ignored");
    limited([](auto& l) {l.maxAtlasPixels=127u;},"Atlas pixel/resource bound ignored");
    for(const auto cut:{0u,1u,2u,4u,8u,16u,32u,64u,128u})
        limited([&](auto& l) {l.work=cut;},"Early shared-work cut ignored");
    for(const auto cut:{font.usedWork/2u,font.usedWork-1u})
        limited([&](auto& l) {l.work=cut;},"Late shared-work cut ignored");
    for(std::size_t cut=0u;cut<admissions;++cut) {
        auto next=QuestVr::NativeFontMetricsLimits{};std::size_t observed{};
        next.admitPeak=[&](const std::size_t) {if(observed++==cut)throw std::runtime_error("explicit outer native byte refusal");};
        Refuses([&] {static_cast<void>(QuestVr::LoadNativeFontMetrics(valid.package,0u,next));},"Outer admission callback failure was swallowed");
        Require(observed==cut+1u,"Loader continued admission after outer refusal");
    }
    for(const auto cut:{0u,1u,17u,127u}) {
        auto next=QuestVr::NativeFontMetricsLimits{};std::size_t observed{};
        next.chargeWork=[&](const std::size_t) {if(observed++==cut)throw std::runtime_error("explicit outer native work refusal");};
        Refuses([&] {static_cast<void>(QuestVr::LoadNativeFontMetrics(valid.package,0u,next));},"Outer work callback failure was swallowed");
        Require(observed==cut+1u,"Loader continued charged work after outer refusal");
    }
    auto exact=QuestVr::NativeFontMetricsLimits{};exact.work=font.usedWork;exact.workingBytes=font.peakWorkingBytes;
    const auto exactFont=QuestVr::LoadNativeFontMetrics(valid.package,0u,exact);
    Require(exactFont.usedWork==font.usedWork && exactFont.peakWorkingBytes==font.peakWorkingBytes,
        "Exact admitted work/working boundary changed successful operation");
    Fixture replacement(file);Replace(replacement.font,replacement.glyphOffsets['A']+8u,6u);replacement.Save();
    Require(QuestVr::LookupNativeFontGlyph(QuestVr::LoadNativeFontMetrics(replacement.package,0u),'A').width==6,
        "A new load reused stale same-filename font authority");
}

std::uint64_t Hash(const std::filesystem::path& path) {
    std::ifstream input(path,std::ios::binary);if(!input)throw std::runtime_error("Cannot fingerprint owned font source");
    std::array<char,16u*1024u> bytes{};std::uint64_t hash=14695981039346656037ull;
    while(input) {input.read(bytes.data(),bytes.size());for(std::streamsize i=0;i<input.gcount();++i) {hash^=static_cast<unsigned char>(bytes[static_cast<std::size_t>(i)]);hash*=1099511628211ull;}}
    if(!input.eof())throw std::runtime_error("Owned font fingerprint read failed");
    return hash;
}
void OriginalTests(const std::filesystem::path& root) {
    Require(std::filesystem::is_regular_file(root/"Maps"/"00_Training.dx"),"Owned first Training source is missing");
    const auto path=root/"System"/"DeusExUI.u";const auto size=std::filesystem::file_size(path);
    const auto modified=std::filesystem::last_write_time(path);const auto hash=Hash(path);
    const auto package=LoadPortablePackageTables(path.string());
    for(const auto* name:{"FontMenuHeaders","FontMenuSmall","FontMenuSmall_DS"}) {
        const auto entry=FindPortableExport(package,name);const auto metrics=QuestVr::LoadNativeFontMetrics(package,entry);
        const auto reference=LoadPortableBitmapFont(package,entry);
        Require(metrics.objectPath==reference.objectPath && metrics.charactersPerPage==reference.charactersPerPage &&
            metrics.pages.size()==reference.pages.size(),"Owned native Font metadata differs from independent full atlas decoder");
        std::size_t total{};
        for(std::size_t page=0u;page<metrics.pages.size();++page) {
            const auto& actual=metrics.pages[page];
            Require(actual.textureReference==reference.textureReferences[page] && actual.texturePath==reference.texturePaths[page] &&
                actual.atlasWidth==reference.pages[page].width && actual.atlasHeight==reference.pages[page].height,
                "Owned atlas reference/path/dimensions differ from original export");
            for(std::size_t glyph=0u;glyph<actual.glyphs.size();++glyph) {
                const auto& old=reference.glyphs.at(total+glyph);const auto& value=actual.glyphs[glyph];
                Require(old.pageIndex==page && old.x==static_cast<std::uint32_t>(value.x) && old.y==static_cast<std::uint32_t>(value.y) &&
                    old.width==static_cast<std::uint32_t>(value.width) && old.height==static_cast<std::uint32_t>(value.height),
                    "Owned raw glyph rectangle differs from independent serialization decoder");
                Require(QuestVr::LookupNativeFontGlyph(metrics,static_cast<std::uint32_t>(page)*metrics.charactersPerPage+static_cast<std::uint32_t>(glyph))==value,
                    "Owned exact Extension glyph index differs from authentic page layout");
            }
            total+=actual.glyphs.size();
        }
        Require(total==reference.glyphs.size() && metrics.peakWorkingBytes<64u*1024u &&
            QuestVr::NativeFontMetricsRetainedBytes(metrics)<64u*1024u,"Metrics provider retained pixel-sized storage or lost glyphs");
        std::cout<<"OWNED "<<name<<": pages="<<metrics.pages.size()<<" glyphs="<<total<<" cpp="<<metrics.charactersPerPage
            <<" retained="<<QuestVr::NativeFontMetricsRetainedBytes(metrics)<<" peak="<<metrics.peakWorkingBytes<<" work="<<metrics.usedWork<<'\n';
    }
    Require(std::filesystem::file_size(path)==size && std::filesystem::last_write_time(path)==modified && Hash(path)==hash,
        "Owned font source changed during metrics diagnostics");
    std::cout<<"UNCHANGED owned DeusExUI.u; only first Training source checked, no other map loaded.\n";
}
} // namespace

int main(int argc,char** argv) {
    try {
        bool ownedOnly{};std::filesystem::path root;
        for(int index=1;index<argc;++index) {
            const auto option=std::string(argv[index]);
            if(option=="--owned-only")ownedOnly=true;
            else if(option=="--game-root" && index+1<argc)root=argv[++index];
            else throw std::runtime_error("Usage: quest_native_font_metrics_test [--owned-only] [--game-root OWNED_GAME_ROOT]");
        }
        if(!ownedOnly)SyntheticTests();
        if(ownedOnly && root.empty()) {if(const auto* configured=std::getenv("DEUSEX_GAME_ROOT"))root=configured;}
        if(ownedOnly && root.empty()) {std::cout<<"SKIP owned native Font metrics: provide --game-root or DEUSEX_GAME_ROOT\n";return 77;}
        if(!root.empty())OriginalTests(root);
        std::cout<<"PASS native Font metrics: "<<checks<<" checks / "<<refusals<<" refusals; exact UTF16 sparse-page glyphs, bounded source resource parsing and capacity/work admission. NOT GC layout/startup/VR acceptance.\n";
        return 0;
    } catch(const std::exception& error) {std::cerr<<"FAIL native Font metrics: "<<error.what()<<'\n';return 1;}
}
