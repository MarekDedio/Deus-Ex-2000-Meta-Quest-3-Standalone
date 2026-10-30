#include "Precomp.h"
#include "portable_brush_polys.h"
#include "surreal_portable_package_tables.h"
#include <bit>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string_view>

namespace {
[[noreturn]] void Fail(const char* why){throw std::runtime_error(std::string("Native brush Polys: ")+why);}
void Limit(std::size_t limit){if(!limit||limit>(32u<<20u))Fail("unsupported byte bound");}
bool Equal(std::string_view left,std::string_view right){
    if(left.size()!=right.size())return false;
    const auto fold=[](unsigned char c){return c>='a'&&c<='z'?static_cast<unsigned char>(c-32u):c;};
    for(std::size_t i=0;i<left.size();++i)if(fold(static_cast<unsigned char>(left[i]))!=fold(static_cast<unsigned char>(right[i])))return false;
    return true;
}
bool Name(const PortablePackageTables& package,std::int32_t index,std::string_view expected){
    return index>=0&&static_cast<std::size_t>(index)<package.names.size()&&
        Equal(package.names[static_cast<std::size_t>(index)].Name.ToString(),expected);
}
const ImportTableEntry& Import(const PortablePackageTables& package,std::int32_t ref){
    if(ref>=0||static_cast<std::uint64_t>(-static_cast<std::int64_t>(ref))>package.imports.size())Fail("class provenance requires an actual import");
    return package.imports[static_cast<std::size_t>(-static_cast<std::int64_t>(ref)-1)];
}
void ClassSource(const PortablePackageTables& package,std::int32_t ref){
    if(ref<0){
        const auto& cls=Import(package,ref);const auto& root=Import(package,cls.ObjOuter);
        if(!Name(package,cls.ClassPackage,"Core")||!Name(package,cls.ClassName,"Class")||!Name(package,cls.ObjName,"Polys")||
            root.ObjOuter||!Name(package,root.ClassPackage,"Core")||!Name(package,root.ClassName,"Package")||!Name(package,root.ObjName,"Engine"))
            Fail("Polys class import lacks actual Core.Class/Engine provenance");
    }else if(!ref||static_cast<std::size_t>(ref)>package.exports.size()||package.exports[static_cast<std::size_t>(ref-1)].ObjClass!=0)
        Fail("Polys class reference does not name an actual local Class export");
}
struct Reader {
    std::span<const std::uint8_t> bytes;std::size_t position{};
    void Need(std::size_t count)const{if(position>bytes.size()||count>bytes.size()-position)Fail("truncated serialized body");}
    std::uint8_t Byte(){Need(1u);return bytes[position++];}
    std::uint16_t Half(){const auto low=Byte();return static_cast<std::uint16_t>(low|(std::uint16_t(Byte())<<8u));}
    std::uint32_t Word(){std::uint32_t value{};for(unsigned i=0u;i<4u;++i)value|=std::uint32_t(Byte())<<(8u*i);return value;}
    std::int32_t Int(){return std::bit_cast<std::int32_t>(Word());}
    std::int32_t Index(){
        const auto first=Byte();std::uint32_t value=first&0x3fu;bool more=(first&0x40u)!=0u;unsigned shift=6u;
        for(unsigned slot=1u;more;++slot){
            if(slot>4u)Fail("compact index has excessive continuation");
            const auto next=Byte();
            if(slot==4u&&(next&0xe0u))Fail("compact index exceeds32 bits");
            value|=std::uint32_t(next&(slot==4u?0x1fu:0x7fu))<<shift;
            more=(next&0x80u)!=0u;shift+=7u;
        }
        if(first&0x80u){if(value>0x80000000u)Fail("negative compact index overflow");
            return value==0x80000000u?std::numeric_limits<std::int32_t>::min():-static_cast<std::int32_t>(value);}
        if(value>0x7fffffffu)Fail("positive compact index overflow");
        return static_cast<std::int32_t>(value);
    }
    float Float(){const auto value=std::bit_cast<float>(Word());if(!std::isfinite(value))Fail("nonfinite polygon vector");return value;}
    PortableModelVec3 Vector(){return {Float(),Float(),Float()};}
};
void Reference(std::int32_t ref,std::size_t imports,std::size_t exports){
    if((ref>0&&static_cast<std::size_t>(ref)>exports)||(ref<0&&static_cast<std::uint64_t>(-static_cast<std::int64_t>(ref))>imports))
        Fail("polygon object reference outside actual tables");
}
}
PortableBrushPolys DecodePortableBrushPolys68Body(std::span<const std::uint8_t> body,
    std::size_t importCount,std::size_t exportCount,std::size_t nameCount,std::size_t byteLimit){
    Limit(byteLimit);if(body.size()>byteLimit)Fail("body exceeds working-byte bound");
    Reader reader{body};const auto count=reader.Int(),capacity=reader.Int();
    if(count<0||count>100000||capacity<count||static_cast<std::size_t>(count)>(body.size()-reader.position)/62u)
        Fail("invalid or impossible polygon count/capacity");
    const auto base=body.size()+sizeof(PortableBrushPolys);
    if(base>byteLimit||static_cast<std::size_t>(count)>(byteLimit-base)/sizeof(PortableBrushPoly))
        Fail("polygon storage exceeds admitted working bytes");
    PortableBrushPolys result;result.serializedCapacity=capacity;result.polygons.resize(static_cast<std::size_t>(count));
    for(auto& poly:result.polygons){
        poly.vertexCount=reader.Index();if(poly.vertexCount<0||poly.vertexCount>16)Fail("polygon exceeds native FPoly vertex capacity");
        poly.base=reader.Vector();poly.normal=reader.Vector();poly.textureU=reader.Vector();poly.textureV=reader.Vector();
        for(std::int32_t vertex=0;vertex<poly.vertexCount;++vertex)poly.vertices[static_cast<std::size_t>(vertex)]=reader.Vector();
        poly.polyFlags=reader.Word();poly.actorReference=reader.Index();poly.textureReference=reader.Index();
        Reference(poly.actorReference,importCount,exportCount);Reference(poly.textureReference,importCount,exportCount);
        poly.itemNameReference=reader.Index();
        if(poly.itemNameReference<0||static_cast<std::size_t>(poly.itemNameReference)>=nameCount)Fail("polygon item FName outside source name table");
        poly.link=reader.Index();poly.brushPoly=reader.Index();
        poly.panU=std::bit_cast<std::int16_t>(reader.Half());poly.panV=std::bit_cast<std::int16_t>(reader.Half());
    }
    if(reader.position!=body.size())Fail("unexpected trailing serialized bytes");
    result.bytesConsumed=reader.position;return result;
}
PortableBrushPolys LoadPortableBrushPolys68(const PortablePackageTables& package,std::size_t index,std::size_t byteLimit){
    Limit(byteLimit);
    if(package.version!=68u||package.licenseeMode||index>=package.exports.size())Fail("unsupported source profile/export");
    // Charge identity/path scratch BEFORE either resolver or property loading.
    // Resolver concatenation can retain an old64KiB identity while allocating
    // its replacement (and a package-stem prefix); a single64KiB reserve is not
    // sufficient. Native filesystem paths also retain parsed component records
    // (including allocation growth), not just a narrow/wide character copy.
    // Charge128 bytes per source byte for that pinned-platform path scratch,
    // plus qualified identity/ancestry and stream handles/buffer.
    if(package.sourcePath.size()>=65536u)Fail("source path exceeds identity bound");
    const auto sourceBytes=package.sourcePath.size()+1u;
    const auto reserve=128u*sourceBytes+4u*65536u+16384u;
    if(reserve>=byteLimit)Fail("source/identity scratch exceeds byte bound");
    const auto& entry=package.exports[index];
    if(entry.ObjOffset<0||entry.ObjSize<=0||static_cast<std::size_t>(entry.ObjSize)>byteLimit-reserve)
        Fail("not an actual bounded Engine.Polys export");
    ClassSource(package,entry.ObjClass);
    {
        const auto actual=ResolvePortableValueObjectReference(package,entry.ObjClass,[](const std::string& path,const std::string& type){
            return Equal(path,"Engine.Polys")&&Equal(type,"Class");
        });
        if(!Equal(actual,"Engine.Polys"))Fail("not an actual Engine.Polys export");
    }
    auto identity=ResolvePortableValueObjectReference(package,static_cast<std::int32_t>(index+1u));
    const std::filesystem::path path(package.sourcePath);const auto size=std::filesystem::file_size(path);
    const auto stamp=std::filesystem::last_write_time(path);
    if(static_cast<std::uint64_t>(entry.ObjOffset)+static_cast<std::uint64_t>(entry.ObjSize)>size)Fail("export lies outside original file");
    const auto properties=LoadPortableExportProperties(package,index,byteLimit-reserve).bytesConsumed;
    if(properties>static_cast<std::size_t>(entry.ObjSize))Fail("property stream exceeds export");
    // Property records/raw bytes have been released before the second read.
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(entry.ObjSize));
    std::ifstream input(path,std::ios::binary);if(!input)Fail("cannot read original Polys source");input.seekg(entry.ObjOffset);
    if(!input.read(reinterpret_cast<char*>(bytes.data()),static_cast<std::streamsize>(bytes.size())))Fail("truncated original Polys export");
    // Include the raw tagged-property prefix retained by bytes while decoding.
    const auto decoderLimit=byteLimit-reserve-properties;
    auto result=DecodePortableBrushPolys68Body(std::span<const std::uint8_t>(bytes).subspan(properties),
        package.imports.size(),package.exports.size(),package.names.size(),decoderLimit);
    result.sourcePath=package.sourcePath;result.objectPath=std::move(identity);result.exportIndex=index;
    if(std::filesystem::file_size(path)!=size||std::filesystem::last_write_time(path)!=stamp)Fail("original source changed while reading");
    return result;
}
