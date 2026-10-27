#include "quest_native_font_metrics.h"
#include "surreal_portable_package_tables.h"
#include "Utils/File.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cstring>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string_view>

namespace QuestVr {
namespace {

constexpr std::size_t kPathBytes=64u << 10u;
constexpr std::size_t kScratchBytes=8192u; // Fixed read cache, bounded path views and cursors.

std::size_t Add(const std::size_t left,const std::size_t right) {
    if(right>std::numeric_limits<std::size_t>::max()-left)
        throw std::runtime_error("Native Font metrics byte addition overflow");
    return left+right;
}
std::size_t Multiply(const std::size_t count,const std::size_t size) {
    if(size && count>std::numeric_limits<std::size_t>::max()/size)
        throw std::runtime_error("Native Font metrics byte multiplication overflow");
    return count*size;
}
bool EqualAscii(const std::string_view left,const std::string_view right) {
    if(left.size()!=right.size())return false;
    const auto fold=[](const unsigned char byte) {return byte>='A'&&byte<='Z'?byte+'a'-'A':byte;};
    for(std::size_t index=0u;index<left.size();++index)
        if(fold(static_cast<unsigned char>(left[index]))!=fold(static_cast<unsigned char>(right[index])))return false;
    return true;
}

struct Budget {
    const NativeFontMetricsLimits& limits;
    NativeFontMetrics& result;
    void Step(const std::size_t units=1u) {
        if(units>limits.work || result.usedWork>limits.work-units)
            throw std::runtime_error("Native Font metrics work budget exceeded");
        if(limits.chargeWork)limits.chargeWork(units);
        result.usedWork+=units;
    }
    void Check(const std::size_t nextAllocation=0u) {
        const auto retained=NativeFontMetricsRetainedBytes(result);
        if(retained>limits.retainedBytes || nextAllocation>limits.retainedBytes-retained)
            throw std::runtime_error("Native Font metrics retained-byte budget exceeded");
        const auto peak=Add(Add(retained,kScratchBytes),nextAllocation);
        if(peak>limits.workingBytes)throw std::runtime_error("Native Font metrics working-byte budget exceeded");
        if(limits.admitPeak)limits.admitPeak(peak);
        result.peakWorkingBytes=std::max(result.peakWorkingBytes,peak);
    }
};

class ReadCache {
public:
    explicit ReadCache(std::shared_ptr<File> source):source_(std::move(source)) {
        const auto size=source_->size();
        if(size<0)throw std::runtime_error("Native Font metrics source has negative file size");
        size_=static_cast<std::uint64_t>(size);
    }
    std::uint64_t Size() const {return size_;}
    void Read(const std::uint64_t offset,void* destination,std::size_t count) {
        if(offset>size_ || count>size_-offset)
            throw std::runtime_error("Native Font metrics read is outside immutable source");
        auto current=offset;auto* output=static_cast<std::uint8_t*>(destination);
        while(count) {
            if(current<start_ || current-start_>=buffered_) {
                const auto available=size_-current;
                const auto amount=static_cast<std::size_t>(std::min<std::uint64_t>(bytes_.size(),available));
                // The loader exclusively owns this cursor. No error fallback,
                // pixel staging or publication after a partial source read.
                buffered_=0u;source_->seek(static_cast<std::int64_t>(current));
                source_->read(bytes_.data(),amount);start_=current;buffered_=amount;
            }
            const auto position=static_cast<std::size_t>(current-start_);
            const auto amount=std::min(count,buffered_-position);
            std::memcpy(output,bytes_.data()+position,amount);
            output+=amount;current+=amount;count-=amount;
        }
    }
private:
    std::shared_ptr<File> source_;
    std::array<std::uint8_t,4096u> bytes_{};
    std::uint64_t size_{},start_{};
    std::size_t buffered_{};
};

class Cursor {
public:
    Cursor(ReadCache& source,const ExportTableEntry& entry,Budget& budget)
        :source_(source),budget_(budget) {
        budget_.Step();
        if(entry.ObjSize<=0 || entry.ObjOffset<0 ||
            static_cast<std::size_t>(entry.ObjSize)>budget_.limits.maxPayloadBytes ||
            static_cast<std::uint64_t>(entry.ObjOffset)>source_.Size() ||
            static_cast<std::uint64_t>(entry.ObjSize)>source_.Size()-static_cast<std::uint64_t>(entry.ObjOffset))
            throw std::runtime_error("Native Font/atlas/palette export payload is invalid or exceeds bound");
        start_=static_cast<std::uint64_t>(entry.ObjOffset);size_=static_cast<std::size_t>(entry.ObjSize);
    }
    std::uint8_t Byte() {
        budget_.Step();Require(1u);std::uint8_t result{};source_.Read(start_+position_,&result,1u);++position_;return result;
    }
    std::uint16_t Word() {
        const auto low=static_cast<std::uint16_t>(Byte());return static_cast<std::uint16_t>(low|(static_cast<std::uint16_t>(Byte())<<8u));
    }
    std::uint32_t Dword() {
        const auto low=static_cast<std::uint32_t>(Word());return low|(static_cast<std::uint32_t>(Word())<<16u);
    }
    std::int32_t Int() {return std::bit_cast<std::int32_t>(Dword());}
    std::int32_t Index(const std::size_t byteLimit=5u) {
        std::size_t consumed{};
        const auto read=[&] {
            if(consumed==byteLimit)throw std::runtime_error("Native Font metrics compact index exceeds its field span");
            ++consumed;return Byte();
        };
        auto value=read();const bool negative=(value&0x80u)!=0u;bool more=(value&0x40u)!=0u;
        std::uint64_t magnitude=value&0x3fu;unsigned shift=6u;
        while(more && shift<32u) {
            value=read();magnitude|=static_cast<std::uint64_t>(value&0x7fu)<<shift;
            more=(value&0x80u)!=0u;shift+=7u;
        }
        if(more || magnitude>static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max()))
            throw std::runtime_error("Native Font metrics compact index is invalid");
        const auto result=static_cast<std::int32_t>(magnitude);return negative?-result:result;
    }
    void Skip(const std::size_t bytes) {budget_.Step();Require(bytes);position_+=bytes;}
    void Position(const std::size_t position) {
        budget_.Step();if(position>size_)throw std::runtime_error("Native Font metrics cursor is outside export");position_=position;
    }
    void Require(const std::size_t bytes) const {
        if(bytes>size_-position_)throw std::runtime_error("Native Font metrics export is truncated");
    }
    std::size_t Position() const {return position_;}
    std::size_t Remaining() const {return size_-position_;}
private:
    ReadCache& source_;Budget& budget_;
    std::uint64_t start_{};std::size_t size_{},position_{};
};

void Reference(const PortablePackageTables& package,const std::int32_t reference) {
    if((reference>0 && static_cast<std::size_t>(reference)>package.exports.size()) ||
        (reference<0 && static_cast<std::uint64_t>(-static_cast<std::int64_t>(reference))>package.imports.size()))
        throw std::runtime_error("Native Font metrics reference is outside source tables");
}
const NameString& Name(const PortablePackageTables& package,const std::int32_t index) {
    if(index<0 || static_cast<std::size_t>(index)>=package.names.size())
        throw std::runtime_error("Native Font metrics name is outside source table");
    return package.names[static_cast<std::size_t>(index)].Name;
}
std::string_view SourceStem(const PortablePackageTables& package) {
    auto name=std::string_view(package.sourcePath);
    const auto slash=name.find_last_of("/\\");if(slash!=std::string_view::npos)name.remove_prefix(slash+1u);
    const auto dot=name.find_last_of('.');if(dot!=std::string_view::npos && dot!=0u)name=name.substr(0u,dot);
    if(name.empty() || name.size()>kPathBytes || name.find('\0')!=std::string_view::npos)
        throw std::runtime_error("Native Font metrics source stem is invalid");
    return name;
}
struct Path {
    std::array<std::string_view,32u> reverse{};
    std::size_t count{},bytes{};
};
Path ObjectPath(const PortablePackageTables& package,std::int32_t reference,Budget& budget) {
    Path result;std::array<std::int32_t,32u> visited{};
    while(reference) {
        budget.Step();Reference(package,reference);
        if(result.count==visited.size() || std::find(visited.begin(),visited.begin()+static_cast<std::ptrdiff_t>(result.count),reference)!=visited.begin()+static_cast<std::ptrdiff_t>(result.count))
            throw std::runtime_error("Native Font metrics object path is cyclic or too deep");
        visited[result.count]=reference;std::int32_t name{},outer{};
        if(reference>0) {const auto& entry=package.exports[static_cast<std::size_t>(reference-1)];name=entry.ObjName;outer=entry.ObjOuter;}
        else {const auto& entry=package.imports[static_cast<std::size_t>(-static_cast<std::int64_t>(reference)-1)];name=entry.ObjName;outer=entry.ObjOuter;}
        const auto& spelling=Name(package,name).ToString();
        if(spelling.empty() || spelling.find('\0')!=std::string::npos || spelling.size()>kPathBytes)
            throw std::runtime_error("Native Font metrics object path segment is invalid");
        result.bytes=Add(result.bytes,spelling.size()+(result.count?1u:0u));
        if(result.bytes>kPathBytes)throw std::runtime_error("Native Font metrics object path exceeds bound");
        result.reverse[result.count++]=spelling;reference=outer;
    }
    return result;
}
bool PathEquals(const Path& path,const std::string_view candidate) {
    if(path.bytes!=candidate.size())return false;
    std::size_t position{};
    for(std::size_t slot=path.count;slot>0u;--slot) {
        const auto segment=path.reverse[slot-1u];
        if(!EqualAscii(segment,candidate.substr(position,segment.size())))return false;
        position+=segment.size();if(slot>1u && candidate[position++]!='.')return false;
    }
    return true;
}
std::string CopyPath(const Path& path,Budget& budget) {
    budget.Check(Add(path.bytes,1u));std::string result(path.bytes,'\0');std::size_t position{};
    for(std::size_t slot=path.count;slot>0u;--slot) {
        const auto segment=path.reverse[slot-1u];
        std::copy(segment.begin(),segment.end(),result.begin()+static_cast<std::ptrdiff_t>(position));
        position+=segment.size();if(slot>1u)result[position++]='.';
    }
    return result;
}
bool ClassIs(const PortablePackageTables& package,const std::int32_t reference,
    const std::string_view packageName,const std::string_view className,Budget& budget) {
    if(!reference)return false;
    const auto path=ObjectPath(package,reference,budget);
    if(reference<0) {
        const auto& entry=package.imports[static_cast<std::size_t>(-static_cast<std::int64_t>(reference)-1)];
        if(!EqualAscii(Name(package,entry.ClassName).ToString(),"Class") ||
            !EqualAscii(Name(package,entry.ClassPackage).ToString(),"Core"))return false;
        return path.count==2u && EqualAscii(path.reverse[1u],packageName) && EqualAscii(path.reverse[0u],className);
    }
    const auto& entry=package.exports[static_cast<std::size_t>(reference-1)];
    return entry.ObjClass==0 && path.count==1u && EqualAscii(SourceStem(package),packageName) && EqualAscii(path.reverse[0u],className);
}
bool TextureClass(const PortablePackageTables& package,const std::int32_t reference,Budget& budget) {
    if(ClassIs(package,reference,"Engine","Texture",budget) || ClassIs(package,reference,"Engine","ScriptedTexture",budget))return true;
    for(const auto name:{"FractalTexture","FireTexture","WaterTexture","WaveTexture","WetTexture","IceTexture"})
        if(ClassIs(package,reference,"Fire",name,budget))return true;
    return false;
}

std::optional<std::int32_t> Properties(Cursor& cursor,const PortablePackageTables& package,
    const ExportTableEntry& entry,const bool capturePalette,Budget& budget) {
    if(AnyFlags(entry.ObjFlags,ObjectFlags::HasStack)) {
        const auto function=cursor.Index();const auto state=cursor.Index();Reference(package,function);Reference(package,state);
        static_cast<void>(cursor.Dword());static_cast<void>(cursor.Dword());static_cast<void>(cursor.Dword());
        if(function)static_cast<void>(cursor.Index());
    }
    std::optional<std::int32_t> palette;
    for(std::size_t count=0u;;++count) {
        budget.Step();const auto& name=Name(package,cursor.Index());
        if(name.IsNone())return palette;
        if(count>=budget.limits.maxProperties)throw std::runtime_error("Native Font metrics property count exceeds bound");
        const auto info=cursor.Byte();const auto type=info&0x0fu;
        if(type==10u)static_cast<void>(Name(package,cursor.Index()));
        std::size_t bytes{};
        switch((info&0x70u)>>4u) {
        case 0u:bytes=1u;break;case 1u:bytes=2u;break;case 2u:bytes=4u;break;
        case 3u:bytes=12u;break;case 4u:bytes=16u;break;
        case 5u:bytes=cursor.Byte();break;case 6u:bytes=cursor.Word();break;
        case 7u:bytes=cursor.Dword();break;
        }
        std::uint32_t arrayIndex{};
        if(type!=3u && (info&0x80u)) {
            auto first=cursor.Byte();
            if((first&0xc0u)==0xc0u) {
                const auto second=cursor.Byte(),third=cursor.Byte(),fourth=cursor.Byte();
                arrayIndex=(static_cast<std::uint32_t>(first&0x3fu)<<24u)|(static_cast<std::uint32_t>(second)<<16u)|(static_cast<std::uint32_t>(third)<<8u)|fourth;
            }
            else if(first&0x80u)arrayIndex=(static_cast<std::uint32_t>(first&0x7fu)<<8u)|cursor.Byte();
            else arrayIndex=first;
        }
        if(type==3u) {
            if(capturePalette && EqualAscii(name.ToString(),"Palette"))
                throw std::runtime_error("Native Font atlas Palette is not an ObjectProperty");
            continue;
        }
        cursor.Require(bytes);
        if(capturePalette && EqualAscii(name.ToString(),"Palette")) {
            if((type!=5u && type!=8u) || arrayIndex || bytes>5u)
                throw std::runtime_error("Native Font atlas Palette lacks its original scalar object reference");
            const auto begin=cursor.Position();const auto reference=cursor.Index(bytes);Reference(package,reference);
            if(cursor.Position()-begin!=bytes)
                throw std::runtime_error("Native Font atlas Palette reference has trailing or truncated bytes");
            palette=reference;
        } else cursor.Skip(bytes);
    }
}

std::uint32_t ValidatePalette(ReadCache& source,const PortablePackageTables& package,const std::int32_t reference,Budget& budget) {
    if(reference<=0)throw std::runtime_error("Native Font atlas Palette is not a local export");
    Reference(package,reference);const auto& entry=package.exports[static_cast<std::size_t>(reference-1)];
    if(!ClassIs(package,entry.ObjClass,"Engine","Palette",budget))
        throw std::runtime_error("Native Font atlas Palette reference is not Engine.Palette");
    Cursor cursor(source,entry,budget);static_cast<void>(Properties(cursor,package,entry,false,budget));
    const auto colors=cursor.Index();
    if(colors<=0 || colors>65'536 || static_cast<std::size_t>(colors)>cursor.Remaining()/4u)
        throw std::runtime_error("Native Font atlas Palette payload is invalid or truncated");
    cursor.Skip(static_cast<std::size_t>(colors)*4u); // Validate bytes; metrics never decode RGBA.
    return static_cast<std::uint32_t>(colors);
}
std::pair<std::uint32_t,std::uint32_t> Atlas(ReadCache& source,const PortablePackageTables& package,
    const std::size_t exportIndex,const std::string& path,Budget& budget) {
    const auto& entry=package.exports[exportIndex];
    if(!TextureClass(package,entry.ObjClass,budget))
        throw std::runtime_error("Native Font atlas reference is not an actual Texture class");
    std::size_t matches{};
    for(std::size_t candidate=0u;candidate<package.exports.size();++candidate) {
        budget.Step();const auto candidatePath=ObjectPath(package,static_cast<std::int32_t>(candidate+1u),budget);
        if(PathEquals(candidatePath,path) && TextureClass(package,package.exports[candidate].ObjClass,budget))++matches;
    }
    if(matches!=1u)throw std::runtime_error("Native Font atlas texture path is ambiguous");
    Cursor cursor(source,entry,budget);const auto palette=Properties(cursor,package,entry,true,budget);
    if(!palette)throw std::runtime_error("Native Font atlas lacks an actual Palette reference");
    const auto paletteColors=ValidatePalette(source,package,*palette,budget);
    const auto mips=cursor.Byte();
    if(!mips || mips>32u)throw std::runtime_error("Native Font atlas mip count is invalid");
    std::pair<std::uint32_t,std::uint32_t> result;
    for(std::uint8_t mip=0u;mip<mips;++mip) {
        budget.Step();if(package.version>=63u)static_cast<void>(cursor.Dword());
        const auto bytes=cursor.Index();
        if(bytes<0 || bytes>64*1024*1024 || static_cast<std::size_t>(bytes)>cursor.Remaining() || cursor.Remaining()-static_cast<std::size_t>(bytes)<10u)
            throw std::runtime_error("Native Font atlas mip data is outside export");
        const auto pixelStart=cursor.Position();cursor.Skip(static_cast<std::size_t>(bytes));
        const auto width=cursor.Dword(),height=cursor.Dword();
        static_cast<void>(cursor.Byte());static_cast<void>(cursor.Byte());
        if(!width || !height || width>8192u || height>8192u)
            throw std::runtime_error("Native Font atlas mip dimensions are invalid");
        if(!mip) {
            if(static_cast<std::uint64_t>(bytes)!=static_cast<std::uint64_t>(width)*height)
                throw std::runtime_error("Native Font atlas top indexed mip byte count differs from dimensions");
            // A byte necessarily fits a256+-color palette. For a smaller
            // palette, preserve the full decoder's index-domain check without
            // allocating pixels or RGBA: stream only the top indexed plane.
            if(paletteColors<256u) {
                auto pixels=cursor;pixels.Position(pixelStart);
                for(std::int32_t pixel=0;pixel<bytes;++pixel) {
                    budget.Step();if(pixels.Byte()>=paletteColors)
                        throw std::runtime_error("Native Font atlas palette index exceeds actual Palette count");
                }
            }
            result={width,height};
        }
    }
    return result;
}

} // namespace

std::size_t NativeFontMetricsRetainedBytes(const NativeFontMetrics& font) {
    auto bytes=Add(sizeof(font),Add(font.objectPath.capacity(),1u));
    bytes=Add(bytes,Multiply(font.pages.capacity(),sizeof(NativeFontMetricsPage)));
    for(const auto& page:font.pages) {
        bytes=Add(bytes,Add(page.texturePath.capacity(),1u));
        bytes=Add(bytes,Multiply(page.glyphs.capacity(),sizeof(NativeFontGlyphMetrics)));
    }
    return bytes;
}

NativeFontMetrics LoadNativeFontMetrics(const PortablePackageTables& package,const std::size_t exportIndex,
    const NativeFontMetricsLimits& limits) {
    NativeFontMetrics result;Budget budget{limits,result};budget.Check();budget.Step();
    if(package.version<=63u || package.version>=100u || exportIndex>=package.exports.size() ||
        package.exports.size()>static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()) ||
        package.sourcePath.empty() || package.sourcePath.size()>kPathBytes || package.sourcePath.find('\0')!=std::string::npos ||
        limits.maxPayloadBytes>16u*1024u*1024u || limits.maxPages>256u || limits.maxGlyphs>65'536u)
        throw std::runtime_error("Native Font metrics source/version/limits are outside authenticated bounds");
    const auto& entry=package.exports[exportIndex];
    if(!ClassIs(package,entry.ObjClass,"Engine","Font",budget))
        throw std::runtime_error("Native Font metrics export is not actual Engine.Font");
    result.objectPath=CopyPath(ObjectPath(package,static_cast<std::int32_t>(exportIndex+1u),budget),budget);budget.Check();
    ReadCache source(File::open_existing(package.sourcePath));Cursor cursor(source,entry,budget);
    static_cast<void>(Properties(cursor,package,entry,false,budget));
    const auto pageCount=cursor.Index();
    if(pageCount<=0 || static_cast<std::size_t>(pageCount)>limits.maxPages)
        throw std::runtime_error("Native Font metrics page count is invalid");
    budget.Check(Multiply(static_cast<std::size_t>(pageCount),sizeof(NativeFontMetricsPage)));
    result.pages.reserve(static_cast<std::size_t>(pageCount));budget.Check();
    std::size_t totalGlyphs{};std::uint64_t atlasPixels{};
    for(std::int32_t index=0;index<pageCount;++index) {
        budget.Step();const auto texture=cursor.Index();Reference(package,texture);
        if(texture<=0)throw std::runtime_error("Native Font metrics atlas must be a local Texture export");
        const auto glyphCount=cursor.Index();
        if(glyphCount<0 || static_cast<std::size_t>(glyphCount)>limits.maxGlyphs ||
            totalGlyphs>limits.maxGlyphs-static_cast<std::size_t>(glyphCount) ||
            static_cast<std::size_t>(glyphCount)>cursor.Remaining()/16u)
            throw std::runtime_error("Native Font metrics glyph count is invalid or truncated");
        result.pages.emplace_back();auto& page=result.pages.back();page.textureReference=texture;
        page.texturePath=CopyPath(ObjectPath(package,texture,budget),budget);budget.Check();
        const auto dimensions=Atlas(source,package,static_cast<std::size_t>(texture-1),page.texturePath,budget);
        page.atlasWidth=dimensions.first;page.atlasHeight=dimensions.second;
        const auto pixels=static_cast<std::uint64_t>(page.atlasWidth)*page.atlasHeight;
        if(pixels>limits.maxAtlasPixels || atlasPixels>limits.maxAtlasPixels-pixels)
            throw std::runtime_error("Native Font metrics atlas pixels exceed source-resource bound");
        atlasPixels+=pixels;
        budget.Check(Multiply(static_cast<std::size_t>(glyphCount),sizeof(NativeFontGlyphMetrics)));
        page.glyphs.reserve(static_cast<std::size_t>(glyphCount));budget.Check();
        for(std::int32_t glyph=0;glyph<glyphCount;++glyph) {
            budget.Step();NativeFontGlyphMetrics value{cursor.Int(),cursor.Int(),cursor.Int(),cursor.Int()};
            if(value.x<0 || value.y<0 || value.width<0 || value.height<0 ||
                static_cast<std::uint32_t>(value.x)>page.atlasWidth || static_cast<std::uint32_t>(value.y)>page.atlasHeight ||
                static_cast<std::uint32_t>(value.width)>page.atlasWidth-static_cast<std::uint32_t>(value.x) ||
                static_cast<std::uint32_t>(value.height)>page.atlasHeight-static_cast<std::uint32_t>(value.y))
                throw std::runtime_error("Native Font metrics glyph rectangle is outside atlas");
            page.glyphs.push_back(value);
        }
        totalGlyphs+=static_cast<std::size_t>(glyphCount);
    }
    result.charactersPerPage=cursor.Dword();
    if(!result.charactersPerPage || result.charactersPerPage>65'536u)
        throw std::runtime_error("Native Font metrics charactersPerPage is invalid");
    for(const auto& page:result.pages) {
        budget.Step();if(page.glyphs.size()>result.charactersPerPage)
            throw std::runtime_error("Native Font metrics page count exceeds charactersPerPage");
    }
    if(cursor.Remaining())throw std::runtime_error("Native Font metrics payload has trailing bytes");
    budget.Check();return result;
}

NativeFontGlyphMetrics LookupNativeFontGlyph(const NativeFontMetrics& font,const std::uint32_t codeUnit) {
    if(!font.charactersPerPage || font.charactersPerPage>65'536u)
        throw std::runtime_error("Native Font glyph lookup requires valid charactersPerPage");
    const auto code=codeUnit&0xffffu;const auto page=code/font.charactersPerPage;
    const auto character=code%font.charactersPerPage;
    if(page>=font.pages.size() || character>=font.pages[page].glyphs.size())return {};
    return font.pages[page].glyphs[character];
}

} // namespace QuestVr
