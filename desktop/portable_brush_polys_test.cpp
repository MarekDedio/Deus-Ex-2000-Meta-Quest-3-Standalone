#include "Precomp.h"
#include "portable_brush_polys.h"
#include "surreal_portable_package_tables.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>

namespace {
using Bytes=std::vector<std::uint8_t>;namespace fs=std::filesystem;
std::size_t checks{},refusals{};
void Require(bool good,const char* why){++checks;if(!good)throw std::runtime_error(why);}
template<class F>void Refuse(F&& action){bool failed{};try{action();}catch(const std::exception&){failed=true;}Require(failed,"Invalid native Polys body was accepted");++refusals;}
void Word(Bytes& out,std::uint32_t value){for(unsigned i=0;i<4u;++i)out.push_back(static_cast<std::uint8_t>(value>>(8u*i)));}
void Index(Bytes& out,std::int32_t value){
    const auto magnitude=value<0?static_cast<std::uint32_t>(-static_cast<std::int64_t>(value)):static_cast<std::uint32_t>(value);
    auto remaining=magnitude>>6u;out.push_back(static_cast<std::uint8_t>((value<0?0x80u:0u)|(remaining?0x40u:0u)|(magnitude&0x3fu)));
    while(remaining){const auto next=remaining>>7u;out.push_back(static_cast<std::uint8_t>((remaining&0x7fu)|(next?0x80u:0u)));remaining=next;}
}
void Vector(Bytes& out,PortableModelVec3 value){for(const auto v:{value.x,value.y,value.z})Word(out,std::bit_cast<std::uint32_t>(v));}
Bytes Encode(const PortableBrushPolys& source){
    Bytes out;Word(out,static_cast<std::uint32_t>(source.polygons.size()));Word(out,static_cast<std::uint32_t>(source.serializedCapacity));
    for(const auto& poly:source.polygons){
        Index(out,poly.vertexCount);Vector(out,poly.base);Vector(out,poly.normal);Vector(out,poly.textureU);Vector(out,poly.textureV);
        for(std::int32_t i=0;i<poly.vertexCount;++i)Vector(out,poly.vertices[static_cast<std::size_t>(i)]);
        Word(out,poly.polyFlags);Index(out,poly.actorReference);Index(out,poly.textureReference);Index(out,poly.itemNameReference);
        Index(out,poly.link);Index(out,poly.brushPoly);
        for(const auto pan:{poly.panU,poly.panV}){const auto bits=std::bit_cast<std::uint16_t>(pan);out.push_back(static_cast<std::uint8_t>(bits));out.push_back(static_cast<std::uint8_t>(bits>>8u));}
    }
    return out;
}
PortableBrushPolys Example(std::int32_t count){
    PortableBrushPolys source;source.serializedCapacity=7;source.polygons.resize(1u);auto& p=source.polygons.front();
    p.vertexCount=count;p.base={-0.0f,17.25f,-99.0f};p.normal={0,0,1};p.textureU={2,0,0};p.textureV={0,-3,0};
    for(std::int32_t i=0;i<count;++i)p.vertices[static_cast<std::size_t>(i)]={static_cast<float>(i),-static_cast<float>(i),0.5f};
    p.polyFlags=0xfedcba99u;p.actorReference=-123;p.textureReference=197;p.itemNameReference=2;p.link=-1;
    p.brushPoly=std::numeric_limits<std::int32_t>::min();p.panU=std::numeric_limits<std::int16_t>::min();p.panV=32767;return source;
}
void Generated(){
    for(std::int32_t corners=0;corners<=16;++corners){const auto source=Example(corners);const auto bytes=Encode(source);
        const auto parsed=DecodePortableBrushPolys68Body(bytes,123u,197u,3u);
        Require(Encode(parsed)==bytes&&parsed.serializedCapacity==7&&parsed.bytesConsumed==bytes.size(),"Native FPoly round trip lost serialized fields/float bits");
        Require(parsed.polygons[0].polyFlags==0xfedcba99u&&parsed.polygons[0].link==-1&&parsed.polygons[0].brushPoly==std::numeric_limits<std::int32_t>::min(),
            "Native FPoly codec interpreted source flags/links as render materials");
        for(std::size_t length=0;length<bytes.size();++length)Refuse([&]{DecodePortableBrushPolys68Body(std::span(bytes).first(length),123u,197u,3u);});
        auto trailing=bytes;trailing.push_back(0u);Refuse([&]{DecodePortableBrushPolys68Body(trailing,123u,197u,3u);});
        Refuse([&]{DecodePortableBrushPolys68Body(bytes,122u,197u,3u);});
        Refuse([&]{DecodePortableBrushPolys68Body(bytes,123u,196u,3u);});
        Refuse([&]{DecodePortableBrushPolys68Body(bytes,123u,197u,2u);});
        Refuse([&]{DecodePortableBrushPolys68Body(bytes,123u,197u,3u,bytes.size());});
    }
    PortableBrushPolys empty;empty.serializedCapacity=std::numeric_limits<std::int32_t>::max();const auto zero=Encode(empty);
    Require(DecodePortableBrushPolys68Body(zero,0,0,0).polygons.empty(),"Serialized capacity was treated as live polygon count/allocation");
    auto base=Encode(Example(3));
    for(const auto value:{std::uint32_t{0xffffffffu},std::uint32_t{100001u},std::uint32_t{100000u}}){auto bad=base;for(unsigned i=0;i<4u;++i)bad[i]=static_cast<std::uint8_t>(value>>(8u*i));Refuse([&]{DecodePortableBrushPolys68Body(bad,123,197,3);});}
    for(const auto value:{std::uint8_t{17u},std::uint8_t{0x81u}}){auto bad=base;bad[8u]=value;Refuse([&]{DecodePortableBrushPolys68Body(bad,123,197,3);});}
    for(const auto bits:{0x7f800000u,0xff800000u,0x7fc00000u}){auto bad=base;for(unsigned i=0;i<4u;++i)bad[9u+i]=static_cast<std::uint8_t>(bits>>(8u*i));Refuse([&]{DecodePortableBrushPolys68Body(bad,123,197,3);});}
    Refuse([&]{DecodePortableBrushPolys68Body(base,123,197,3,0u);});
    Refuse([&]{DecodePortableBrushPolys68Body(base,123,197,3,(32u<<20u)+1u);});
}
std::uint64_t Hash(const fs::path& path){std::ifstream in(path,std::ios::binary);Require(bool(in),"Cannot fingerprint owned Training map");
    std::uint64_t hash=14695981039346656037ull;std::array<char,65536u> block{};while(in){in.read(block.data(),block.size());for(std::streamsize i=0;i<in.gcount();++i){hash^=static_cast<unsigned char>(block[static_cast<std::size_t>(i)]);hash*=1099511628211ull;}}
    Require(in.eof()&&!in.bad(),"Cannot finish Training fingerprint");return hash;}
void Owned(const fs::path& root){
    const auto path=root/"Maps"/"00_Training.dx";const auto size=fs::file_size(path);const auto hash=Hash(path);const auto stamp=fs::last_write_time(path);
    const auto map=LoadPortablePackageTables(path.string());std::size_t objects{},polygons{},vertices{},smallest=map.exports.size();
    const auto classNameBefore=NameString::FindCompareIndex("Engine.Polys");
    for(std::size_t i=0;i<map.exports.size();++i){
        if(GetPortableObjectPath(map,map.exports[i].ObjClass)!="Engine.Polys")continue;
        const auto parsed=LoadPortableBrushPolys68(map,i);const auto& entry=map.exports[i];const auto prefix=LoadPortableExportProperties(map,i).bytesConsumed;
        Bytes original(static_cast<std::size_t>(entry.ObjSize)-prefix);std::ifstream input(path,std::ios::binary);input.seekg(entry.ObjOffset+prefix);
        Require(bool(input.read(reinterpret_cast<char*>(original.data()),static_cast<std::streamsize>(original.size()))),"Cannot read original Polys body");
        Require(Encode(parsed)==original&&parsed.bytesConsumed==original.size()&&parsed.sourcePath==map.sourcePath&&parsed.exportIndex==i,
            "Owned native Polys byte/identity round trip differs");
        ++objects;polygons+=parsed.polygons.size();for(const auto& poly:parsed.polygons)vertices+=static_cast<std::size_t>(poly.vertexCount);
        if(smallest==map.exports.size()||entry.ObjSize<map.exports[smallest].ObjSize)smallest=i;
        Refuse([&]{LoadPortableBrushPolys68(map,i,1u);});
    }
    Require(objects&&polygons&&vertices,"Training did not exercise real source FPolys");
    Require(NameString::FindCompareIndex("Engine.Polys")==classNameBefore,
        "Loading real Polys exports interned a synthesized qualified class identity");
    auto wrong=map;wrong.exports[0].ObjClass=0;Refuse([&]{LoadPortableBrushPolys68(wrong,0u);});
    wrong=map;wrong.version=69u;Refuse([&]{LoadPortableBrushPolys68(wrong,0u);});
    // Detached table mutations exercise resolver/phase budgets without writing
    // the owned file. Identity scratch must be admitted before property reads.
    wrong=map;
    const auto nameIndex=static_cast<std::size_t>(wrong.exports[smallest].ObjName);
    wrong.names[nameIndex].Name=NameString(std::string(60000u,'P'));
    const auto longIdentity=LoadPortableBrushPolys68(wrong,smallest,1u<<20u);
    Require(longIdentity.objectPath.size()>60000u&&longIdentity.polygons.size()==LoadPortableBrushPolys68(map,smallest).polygons.size(),
        "Long qualified Polys identity lost source data");
    Refuse([&]{LoadPortableBrushPolys68(wrong,smallest,65536u);});
    wrong.sourcePath=std::string(65536u,'x');Refuse([&]{LoadPortableBrushPolys68(wrong,smallest);});
    wrong=map;wrong.sourcePath.clear();
    for(std::size_t i=0;i<24000u;++i)wrong.sourcePath+="/p";
    wrong.sourcePath+="/00_Training.dx";
    bool pathBudget{};
    try{static_cast<void>(LoadPortableBrushPolys68(wrong,smallest,1u<<20u));}
    catch(const std::exception& error){pathBudget=std::string(error.what()).find("source/identity scratch exceeds byte bound")!=std::string::npos;}
    Require(pathBudget,"Many-component native path reached filesystem allocation before byte admission");++refusals;
    wrong=map;wrong.exports[smallest].ObjOuter=static_cast<std::int32_t>(smallest+1u);
    Refuse([&]{LoadPortableBrushPolys68(wrong,smallest);});
    const auto classRef=map.exports[smallest].ObjClass;
    Require(classRef<0,"Owned Training Polys does not use its native class import");
    const auto classIndex=static_cast<std::size_t>(-static_cast<std::int64_t>(classRef)-1);
    wrong=map;wrong.imports[classIndex].ClassPackage=wrong.imports[classIndex].ObjName;
    Refuse([&]{LoadPortableBrushPolys68(wrong,smallest);});
    wrong=map;wrong.imports[classIndex].ClassName=wrong.imports[classIndex].ObjName;
    Refuse([&]{LoadPortableBrushPolys68(wrong,smallest);});
    const auto rootIndex=static_cast<std::size_t>(-static_cast<std::int64_t>(map.imports[classIndex].ObjOuter)-1);
    wrong=map;wrong.imports[rootIndex].ClassPackage=wrong.imports[classIndex].ObjName;
    Refuse([&]{LoadPortableBrushPolys68(wrong,smallest);});
    wrong=map;wrong.imports[rootIndex].ClassName=wrong.imports[classIndex].ObjName;
    Refuse([&]{LoadPortableBrushPolys68(wrong,smallest);});
    wrong=map;wrong.exports[smallest].ObjClass=static_cast<std::int32_t>(smallest+1u);
    Refuse([&]{LoadPortableBrushPolys68(wrong,smallest);});
    Require(!NameString::FindCompareIndex("Engine.Polys.BudgetProbeDoNotIntern"),"Synthetic identity already interned");
    wrong=map;wrong.names[static_cast<std::size_t>(wrong.imports[classIndex].ObjName)].Name=NameString("Polys.BudgetProbeDoNotIntern");
    Refuse([&]{LoadPortableBrushPolys68(wrong,smallest);});
    Require(!NameString::FindCompareIndex("Engine.Polys.BudgetProbeDoNotIntern"),"Class comparison interned synthesized identities");
    Require(size==fs::file_size(path)&&stamp==fs::last_write_time(path)&&hash==Hash(path),"Native Polys loader changed original Training bytes/stamp");
    std::cout<<"OWNED Training Polys exports="<<objects<<" polygons="<<polygons<<" vertices="<<vertices<<"; exact byte round trips, original unchanged\n";
}
}
int main(int argc,char** argv){try{
    if(argc==1)Generated();
    else if((argc==2||argc==3)&&std::string(argv[1])=="--owned"){
        const char* root=argc==3?argv[2]:std::getenv("DEUSEX_GAME_ROOT");if(!root){std::cout<<"SKIP owned Polys: supply --owned GAME_ROOT\n";return 77;}Owned(root);
    }else throw std::runtime_error("Usage: portable_brush_polys_test [--owned GAME_ROOT]");
    std::cout<<"PASS native brush Polys codec checks="<<checks<<" refusals="<<refusals<<"; serialized fields only, NOT tracker construction/BSP insertion/live doors/startup\n";return 0;
}catch(const std::exception& error){std::cerr<<"FAIL native Polys: "<<error.what()<<'\n';return 1;}}
