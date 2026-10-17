#include "Precomp.h"
#include "portable_model_geometry.h"
#include "surreal_portable_package_tables.h"

#include <array>
#include <bit>
#include <cfenv>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <iterator>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

// Commercial-data-free serialized-layout/data-admission tests. No query
// capability, native UObject authority, original executable or renderer.
namespace {
using Bytes=std::vector<std::uint8_t>;
std::size_t checks{},refusals{};
void Require(bool value,const char* why) {++checks;if(!value)throw std::runtime_error(why);}
void U16(Bytes& b,std::uint16_t n) {b.push_back(static_cast<std::uint8_t>(n));b.push_back(static_cast<std::uint8_t>(n>>8u));}
void U32(Bytes& b,std::uint32_t n) {U16(b,static_cast<std::uint16_t>(n));U16(b,static_cast<std::uint16_t>(n>>16u));}
void U64(Bytes& b,std::uint64_t n) {U32(b,static_cast<std::uint32_t>(n));U32(b,static_cast<std::uint32_t>(n>>32u));}
void Float(Bytes& b,float n) {U32(b,std::bit_cast<std::uint32_t>(n));}
void Index(Bytes& b,std::int32_t n) {
    auto magnitude=static_cast<std::uint32_t>(n<0?-static_cast<std::int64_t>(n):n);
    auto first=static_cast<std::uint8_t>((magnitude&63u)|(n<0?128u:0u));magnitude>>=6u;if(magnitude)first|=64u;b.push_back(first);
    while(magnitude) {auto next=static_cast<std::uint8_t>(magnitude&127u);magnitude>>=7u;if(magnitude)next|=128u;b.push_back(next);}
}
void Replace32(Bytes& b,std::size_t offset,std::uint32_t n) {for(unsigned i=0u;i<4u;++i)b.at(offset+i)=static_cast<std::uint8_t>(n>>(8u*i));}
void ReplaceIndex(Bytes& b,std::size_t offset,std::int32_t n) {
    Bytes value;Index(value,n);b.erase(b.begin()+static_cast<std::ptrdiff_t>(offset));b.insert(b.begin()+static_cast<std::ptrdiff_t>(offset),value.begin(),value.end());
}
template<class F> void Refused(F&& operation,const char* why) {
    bool refused{};try {operation();} catch(const std::runtime_error&) {refused=true;}
    Require(refused,why);++refusals;
}
template<class F> void BudgetRefused(F&& operation) {
    bool refused{};try {operation();} catch(const std::runtime_error& error) {
        refused=std::string(error.what()).find("working-byte budget")!=std::string::npos;
    }
    Require(refused,"Expected explicit working-byte refusal before decode/allocation/file access");++refusals;
}
struct Body {
    Bytes bytes;std::map<std::string,std::size_t> offsets;
    void Mark(const std::string& name) {offsets.emplace(name,bytes.size());}
    void Scalar(const std::string& name,float value) {Mark(name);Float(bytes,value);}
    void Bounds(const std::string& prefix) {
        Scalar(prefix+".minX",0);Scalar(prefix+".minY",0);Scalar(prefix+".minZ",0);
        Scalar(prefix+".maxX",64);Scalar(prefix+".maxY",64);Scalar(prefix+".maxZ",64);Mark(prefix+".valid");bytes.push_back(1u);
    }
    explicit Body(bool empty=false) {
        Bounds("box");Scalar("sphere.x",0);Scalar("sphere.y",0);Scalar("sphere.z",0);Scalar("sphere.radius",64);
        Mark("vectors");Index(bytes,empty?0:3);
        if(!empty)for(unsigned i=0u;i<3u;++i)for(unsigned j=0u;j<3u;++j)Scalar("vector"+std::to_string(i)+'.'+std::to_string(j),i==j?1.0f:0.0f);
        Mark("points");Index(bytes,empty?0:3);
        if(!empty)for(unsigned i=0u;i<3u;++i)for(unsigned j=0u;j<3u;++j)Scalar("point"+std::to_string(i)+'.'+std::to_string(j),i==j?64.0f:0.0f);
        Mark("nodes");Index(bytes,empty?0:1);
        if(!empty) {
            Scalar("plane.x",0);Scalar("plane.y",0);Scalar("plane.z",1);Scalar("plane.w",0);U64(bytes,0x123456789abcdef0ull);bytes.push_back(5u);
            Mark("node.pool");Index(bytes,0);Mark("node.surface");Index(bytes,0);Mark("node.back");Index(bytes,-1);
            Index(bytes,-1);Index(bytes,-1);Index(bytes,-1);Mark("node.render");Index(bytes,0);Index(bytes,0);Index(bytes,0);bytes.push_back(3u);
            Mark("node.leaf");U32(bytes,0);U32(bytes,0xffffffffu);
        }
        Mark("surfaces");Index(bytes,empty?0:1);
        if(!empty) {
            Mark("surface.material");Index(bytes,-1);U32(bytes,0x00400002u);Mark("surface.base");Index(bytes,0);Index(bytes,0);Index(bytes,1);Index(bytes,2);
            Mark("surface.lightMap");Index(bytes,0);Index(bytes,-1);U16(bytes,0xfff9u);U16(bytes,11u);Mark("surface.actor");Index(bytes,5);
        }
        Mark("vertices");Index(bytes,empty?0:3);
        if(!empty)for(unsigned i=0u;i<3u;++i) {if(i==0u)Mark("vertex.point");Index(bytes,static_cast<std::int32_t>(i));Index(bytes,-1);}
        Mark("sharedSides");U32(bytes,0);Mark("zones");U32(bytes,empty?0:1);
        if(!empty) {Mark("zone.actor");Index(bytes,4);U64(bytes,0x8000000000000001ull);U64(bytes,0x4000000000000002ull);}
        Mark("polys");Index(bytes,0);Mark("lightmaps");Index(bytes,empty?0:1);
        if(!empty) {
            Mark("lm.offset");U32(bytes,0xffffffffu);Scalar("lm.panX",1.25f);Scalar("lm.panY",-2.5f);Scalar("lm.panZ",3.75f);
            Mark("lm.uClamp");Index(bytes,1);Mark("lm.vClamp");Index(bytes,1);Scalar("lm.uScale",1);Scalar("lm.vScale",1);Mark("lm.lightActors");U32(bytes,0xffffffffu);
        }
        Mark("lightBits");Index(bytes,empty?0:3);if(!empty)bytes.insert(bytes.end(),{0x12u,0x80u,0xffu});
        Mark("renderBounds");Index(bytes,empty?0:1);if(!empty)Bounds("render");
        Mark("hulls");Index(bytes,empty?0:1);if(!empty)U32(bytes,0xffffffffu);
        Mark("leaves");Index(bytes,empty?0:1);if(!empty) {Index(bytes,0);Index(bytes,-1);Index(bytes,-1);U64(bytes,0x8000000000000000ull);}
        Mark("lights");Index(bytes,empty?0:3);if(!empty) {Mark("light.first");Index(bytes,5);Index(bytes,5);Mark("light.terminator");Index(bytes,0);}
        Mark("rootOutside");U32(bytes,1);Mark("linked");U32(bytes,0);
    }
};
PortableModelGeometry Collision(const Bytes& b,std::size_t budget=256u*1024u*1024u) {return DecodePortableCollisionModel68Body(b,3u,5u,68u,0u,budget);}
PortableModelGeometry Render(const Bytes& b) {return DecodePortableModel68Body(b,3u,5u);}
std::uint32_t Bits(float value) {return std::bit_cast<std::uint32_t>(value);}
std::size_t ArrayBytes(const PortableModelGeometry& m) {
    return m.vectors.capacity()*sizeof(PortableModelVec3)+m.points.capacity()*sizeof(PortableModelVec3)+m.nodes.capacity()*sizeof(PortableModelNode)+
        m.surfaces.capacity()*sizeof(PortableModelSurface)+m.vertices.capacity()*sizeof(PortableModelVertex)+m.zones.capacity()*sizeof(PortableModelZone)+
        m.lightMaps.capacity()*sizeof(PortableModelLightMapIndex)+m.lightBits.capacity()+m.renderBounds.capacity()*sizeof(PortableModelBounds)+
        m.leafHulls.capacity()*sizeof(std::int32_t)+m.leaves.capacity()*sizeof(PortableModelLeaf)+m.lights.capacity()*sizeof(std::int32_t);
}
float Component(const PortableModelVec3& v,unsigned i) {return i==0u?v.x:i==1u?v.y:v.z;}
float Field(const PortableModelGeometry& m,const std::string& name) {
    if(name=="box.minX")return m.bounds.minimum.x;
    if(name=="box.minY")return m.bounds.minimum.y;
    if(name=="box.minZ")return m.bounds.minimum.z;
    if(name=="box.maxX")return m.bounds.maximum.x;
    if(name=="box.maxY")return m.bounds.maximum.y;
    if(name=="box.maxZ")return m.bounds.maximum.z;
    if(name=="sphere.x")return m.sphere.x;
    if(name=="sphere.y")return m.sphere.y;
    if(name=="sphere.z")return m.sphere.z;
    if(name=="sphere.radius")return m.sphere.radius;
    if(name.rfind("vector",0u)==0u)return Component(m.vectors.at(static_cast<std::size_t>(name[6]-'0')),static_cast<unsigned>(name[8]-'0'));
    if(name.rfind("point",0u)==0u)return Component(m.points.at(static_cast<std::size_t>(name[5]-'0')),static_cast<unsigned>(name[7]-'0'));
    if(name=="plane.x")return m.nodes[0u].planeX;
    if(name=="plane.y")return m.nodes[0u].planeY;
    if(name=="plane.z")return m.nodes[0u].planeZ;
    if(name=="plane.w")return m.nodes[0u].planeW;
    if(name=="lm.panX")return m.lightMaps[0u].panX;
    if(name=="lm.panY")return m.lightMaps[0u].panY;
    if(name=="lm.panZ")return m.lightMaps[0u].panZ;
    if(name=="lm.uScale")return m.lightMaps[0u].uScale;
    if(name=="lm.vScale")return m.lightMaps[0u].vScale;
    if(name=="render.minX")return m.renderBounds[0u].minimum.x;
    if(name=="render.minY")return m.renderBounds[0u].minimum.y;
    if(name=="render.minZ")return m.renderBounds[0u].minimum.z;
    if(name=="render.maxX")return m.renderBounds[0u].maximum.x;
    if(name=="render.maxY")return m.renderBounds[0u].maximum.y;
    if(name=="render.maxZ")return m.renderBounds[0u].maximum.z;
    throw std::runtime_error("Unknown generated Float field selector");
}
void RawValues() {
    const Body fixture;const auto model=Collision(fixture.bytes);const auto rendered=Render(fixture.bytes);
    Require(model.bodyBytesConsumed==fixture.bytes.size()&&rendered.bodyBytesConsumed==fixture.bytes.size(),"Native body was not consumed exactly");
    Require(model.bounds.valid&&model.bounds.rawValid==1u&&rendered.bounds.rawValid==1u&&model.renderBounds[0u].rawValid==1u,
        "Ordinary raw box validity byte changed strict rendering behavior");
    Require(model.vectors.size()==3u&&model.points.size()==3u&&model.nodes.size()==1u&&model.surfaces.size()==1u&&model.vertices.size()==3u&&
        model.zones.size()==1u&&model.lightMaps.size()==1u&&model.lightBits==Bytes({0x12u,0x80u,0xffu})&&model.renderBounds.size()==1u&&
        model.leafHulls==std::vector<std::int32_t>{-1}&&model.leaves.size()==1u&&model.lights==std::vector<std::int32_t>({5,5,0}),
        "Collision decoder lost serialized array ordering/content");
    const std::array<std::uint32_t,7u> special{0x7fc12345u,0xffc54321u,0x7f800001u,0xff800001u,0x7f800000u,0xff800000u,0x80000000u};
    for(const auto& [name,offset]:fixture.offsets) {
        const bool floating=name.find("box.")==0u||name.find("sphere.")==0u||(name.find("vector")==0u&&name!="vectors")||
            (name.find("point")==0u&&name!="points")||name.find("plane.")==0u||name.find("lm.pan")==0u||name=="lm.uScale"||name=="lm.vScale"||name.find("render.")==0u;
        if(!floating||name.ends_with(".valid"))continue;
        for(const auto bits:special) {
            auto bytes=fixture.bytes;Replace32(bytes,offset,bits);const auto before=bytes;std::feclearexcept(FE_ALL_EXCEPT);
            const auto actual=Collision(bytes);
            Require(std::fetestexcept(FE_INVALID)==0,"Collision decode compared inactive signaling NaN");
            Require(Bits(Field(actual,name))==bits&&bytes==before,"Collision decode changed raw Float bits/source bytes");
            if((bits&0x7fffffffu)>0x7f7fffffu)Refused([&] {(void)Render(bytes);},"Strict render decoder accepted nonfinite Float");
        }
    }
    for(unsigned valid=0u;valid<=255u;++valid) {
        auto bytes=fixture.bytes;bytes[fixture.offsets.at("box.valid")]=static_cast<std::uint8_t>(valid);
        bytes[fixture.offsets.at("render.valid")]=static_cast<std::uint8_t>(255u-valid);const auto actual=Collision(bytes);
        Require(actual.bounds.rawValid==valid&&actual.bounds.valid==(valid!=0u)&&actual.renderBounds[0u].rawValid==255u-valid&&
            actual.renderBounds[0u].valid==(valid!=255u),"Collision decode lost arbitrary serialized box Valid byte");
        if(valid>1u||255u-valid>1u)Refused([&] {(void)Render(bytes);},"Strict render decoder accepted arbitrary box Valid byte");
    }
    auto reversed=fixture.bytes;Replace32(reversed,fixture.offsets.at("box.minX"),Bits(100.0f));Replace32(reversed,fixture.offsets.at("render.minY"),Bits(200.0f));
    const auto backwards=Collision(reversed);Require(backwards.bounds.minimum.x==100&&backwards.bounds.maximum.x==64&&
        backwards.renderBounds[0u].minimum.y==200&&backwards.renderBounds[0u].maximum.y==64,"Collision decode reordered reversed box endpoints");
    Refused([&] {(void)Render(reversed);},"Strict render decoder accepted reversed valid endpoints");
    // Numeric/render relationships are not serialized-array allocation bounds.
    // These remain exact values until the actual consuming query activates them.
    for(const auto& [name,value]:std::vector<std::pair<std::string,std::uint32_t>>{{"lm.offset",0x80000000u},{"lm.lightActors",0x7fffffffu},
        {"sharedSides",0xffffffffu},{"node.leaf",0x7fffffffu}}) {
        auto bytes=fixture.bytes;Replace32(bytes,fixture.offsets.at(name),value);const auto actual=Collision(bytes);
        Require(actual.bodyBytesConsumed==bytes.size(),"Collision decode refused inactive render numeric/index data");
        Refused([&] {(void)Render(bytes);},"Strict render decoder no longer validates numeric/index data");
    }
    for(const auto& [name,value]:std::vector<std::pair<std::string,std::int32_t>>{{"lm.uClamp",0},{"lm.vClamp",-1},{"node.render",7},
        {"node.pool",7},{"node.surface",7},{"node.back",7},{"surface.base",7},{"surface.lightMap",7},{"vertex.point",7},{"light.terminator",5}}) {
        auto bytes=fixture.bytes;ReplaceIndex(bytes,fixture.offsets.at(name),value);(void)Collision(bytes);
        if(name!="light.terminator")Refused([&] {(void)Render(bytes);},"Strict render index/dimension policy was weakened");
    }
    const Body empty(true);auto inactive=empty.bytes;Replace32(inactive,0u,0x7f800001u);Replace32(inactive,25u,0xffc01234u);inactive[24u]=255u;
    const auto zero=Collision(inactive,0u);Require(zero.nodes.empty()&&zero.bounds.rawValid==255u&&Bits(zero.bounds.minimum.x)==0x7f800001u&&
        Bits(zero.sphere.x)==0xffc01234u&&ArrayBytes(zero)==0u,"Empty structural collision body activated inactive box/sphere or allocated arrays");
    auto rootFlags=empty.bytes;Replace32(rootFlags,empty.offsets.at("rootOutside"),0x80000000u);Replace32(rootFlags,empty.offsets.at("linked"),0x7fffffffu);
    const auto flags=Collision(rootFlags);Require(flags.rootOutside==std::numeric_limits<std::int32_t>::min()&&flags.linked==std::numeric_limits<std::int32_t>::max(),
        "Collision decode normalized native tail integer flags");
}
void StructuralRefusals() {
    const Body fixture;
    for(std::size_t length=0u;length<fixture.bytes.size();++length) {
        const Bytes truncated(fixture.bytes.begin(),fixture.bytes.begin()+static_cast<std::ptrdiff_t>(length));
        Refused([&] {(void)Collision(truncated);},"Truncated collision model body was accepted");
    }
    auto tail=fixture.bytes;tail.push_back(0u);Refused([&] {(void)Collision(tail);},"Collision model trailing tail byte accepted");
    Refused([&] {(void)DecodePortableCollisionModel68Body(fixture.bytes,3u,5u,67u,0u);},"Collision decoder accepted unsupported package version");
    Refused([&] {(void)DecodePortableCollisionModel68Body(fixture.bytes,3u,5u,68u,1u);},"Collision decoder accepted unsupported licensee layout");
    for(const char* name:{"vectors","points","nodes","surfaces","vertices","lightmaps","lightBits","renderBounds","hulls","leaves","lights"}) {
        for(const auto count:{-1,2'000'001}) {auto bad=fixture.bytes;ReplaceIndex(bad,fixture.offsets.at(name),count);Refused([&] {(void)Collision(bad);},"Invalid/truncated array count accepted");}
    }
    for(const auto zones:{0xffffffffu,65u}) {auto bad=fixture.bytes;Replace32(bad,fixture.offsets.at("zones"),zones);Refused([&] {(void)Collision(bad);},"Invalid fixed zone count accepted");}
    for(const char* name:{"surface.material","surface.actor","zone.actor","polys","light.first"})
        for(const auto value:{-4,6}) {auto bad=fixture.bytes;ReplaceIndex(bad,fixture.offsets.at(name),value);Refused([&] {(void)Collision(bad);},"Out-of-table actual object reference accepted");}
    for(const Bytes& malformed:std::vector<Bytes>{{0x40u,0x80u,0x80u,0x80u,0x10u},{0x40u,0x80u,0x80u,0x80u,0x80u},{0xffu,0xffu,0xffu,0xffu,0x7fu}}) {
        auto bad=fixture.bytes;const auto at=fixture.offsets.at("vectors");bad.erase(bad.begin()+static_cast<std::ptrdiff_t>(at));
        bad.insert(bad.begin()+static_cast<std::ptrdiff_t>(at),malformed.begin(),malformed.end());Refused([&] {(void)Collision(bad);},"Overflowing compact array count accepted");
    }
    const auto decoded=Collision(fixture.bytes);const auto arrayBytes=ArrayBytes(decoded);Require(arrayBytes>0u,"Populated decoder budget fixture has no arrays");
    BudgetRefused([&] {(void)Collision(fixture.bytes,0u);});BudgetRefused([&] {(void)Collision(fixture.bytes,arrayBytes-1u);});
    Require(ArrayBytes(Collision(fixture.bytes,arrayBytes))==arrayBytes,"Exact decoded array-capacity budget did not pass");
    Require(ArrayBytes(Collision(fixture.bytes,std::numeric_limits<std::size_t>::max()))==arrayBytes,"Oversized requested limit changed decoded geometry");
}
struct FileFixture {
    std::filesystem::path parent,directory,path;PortablePackageTables package;Bytes bytes;std::filesystem::file_time_type modified;
    explicit FileFixture(const Bytes& body) {
        parent=std::filesystem::canonical(std::filesystem::temp_directory_path());const auto stamp=std::chrono::steady_clock::now().time_since_epoch().count();
        for(unsigned i=0u;i<20u;++i) {const auto candidate=parent/("deusex-collision-model-decode-"+std::to_string(stamp)+'-'+std::to_string(i));
            if(std::filesystem::create_directory(candidate)) {directory=std::filesystem::canonical(candidate);break;}}
        Require(!directory.empty()&&directory.parent_path()==parent,"Collision Model file fixture escaped isolated temporary parent");
        path=directory/"generated-model.bin";package.sourcePath=path.string();package.version=68u;package.licenseeMode=0u;
        for(const char* name:{"None","Engine","Model","Mesh","TestModel","Flag"})package.names.push_back({NameString(name),0u});
        package.imports={{0,0,0,1},{0,0,-1,2},{0,0,-1,3}};package.exports.resize(5u);
        Index(bytes,5);bytes.push_back(0x83u);Index(bytes,0);bytes.insert(bytes.end(),body.begin(),body.end());
        package.exports[0u]={-2,0,0,4,ObjectFlags{},static_cast<std::int32_t>(bytes.size()),0};
        std::ofstream file(path,std::ios::binary|std::ios::trunc);Require(bool(file),"Cannot create generated collision Model file");
        file.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));file.close();Require(bool(file),"Cannot finish generated collision Model file");
        modified=std::filesystem::last_write_time(path);
    }
    ~FileFixture() {std::error_code ignored;if(path.parent_path()==directory)std::filesystem::remove(path,ignored);
        if(!directory.empty()&&directory.parent_path()==parent)std::filesystem::remove(directory,ignored);}
    void Unchanged() const {
        std::ifstream file(path,std::ios::binary);Require(bool(file),"Generated collision file disappeared");const Bytes actual{std::istreambuf_iterator<char>(file),std::istreambuf_iterator<char>()};
        Require(actual==bytes&&std::filesystem::last_write_time(path)==modified,"Collision/render decoder modified source bytes/mtime");
    }
};
void FileLoading() {
    const Body fixture;auto raw=fixture.bytes;Replace32(raw,25u,0x7f800001u);raw[24u]=255u;FileFixture file(raw);
    const auto loaded=LoadPortableCollisionModel68(file.package,0u);
    Require(loaded.objectPath=="TestModel"&&loaded.exportIndex==0u&&loaded.bodyBytesConsumed==raw.size()&&
        Bits(loaded.sphere.x)==0x7f800001u&&loaded.bounds.rawValid==255u,"Collision file loader lost native property boundary/identity/raw bits");
    Refused([&] {(void)LoadPortableModel68(file.package,0u);},"Existing render file loader accepted collision-only nonfinite/raw Valid data");
    const auto arrays=ArrayBytes(loaded);BudgetRefused([&] {(void)LoadPortableCollisionModel68(file.package,0u,512u*1024u*1024u,arrays-1u);});
    Require(ArrayBytes(LoadPortableCollisionModel68(file.package,0u,512u*1024u*1024u,arrays))==arrays,"Explicit file decoded-array budget differs");
    for(const auto propertyBudget:{0u,1u})BudgetRefused([&] {(void)LoadPortableCollisionModel68(file.package,0u,propertyBudget);});
    auto bad=file.package;bad.sourcePath=(file.directory/"not-created.bin").string();bad.exports[0u].ObjSize=64*1024*1024;
    BudgetRefused([&] {(void)LoadPortableCollisionModel68(bad,0u,1024u);});
    for(unsigned variant=0u;variant<8u;++variant) {
        bad=file.package;
        if(variant==0u)bad.version=69u;else if(variant==1u)bad.licenseeMode=1u;
        else if(variant==2u)bad.exports[0u].ObjClass=-3;else if(variant==3u)bad.exports[0u].ObjSize=0;
        else if(variant==4u)bad.exports[0u].ObjOffset=-1;else if(variant==5u)bad.exports[0u].ObjOffset=static_cast<std::int32_t>(file.bytes.size());
        else if(variant==6u)bad.exports[0u].ObjSize=std::numeric_limits<std::int32_t>::max();else bad.exports[0u].ObjSize=2;
        Refused([&] {(void)LoadPortableCollisionModel68(bad,0u);},"Malformed collision file layout/class/payload/property boundary accepted");
    }
    Refused([&] {(void)LoadPortableCollisionModel68(file.package,5u);},"Out-of-table collision file export accepted");file.Unchanged();
    FileFixture strict(fixture.bytes);Require(LoadPortableModel68(strict.package,0u).bodyBytesConsumed==fixture.bytes.size(),"Strict render file decoder changed valid behavior");strict.Unchanged();
}
} // namespace
int main() {
    try {RawValues();StructuralRefusals();FileLoading();std::cout<<"PASS collision-only Model v68 decoder "<<checks<<" checks, "<<refusals
        <<" refusals; exact raw Float/Valid preservation, independent structural/working budgets, whole-tail/reference admission and unchanged strict render policy. "
          "No native query, lifecycle, rendering or headset evidence.\n";return 0;}
    catch(const std::exception& error) {std::cerr<<"Collision-only Model decoder failed: "<<error.what()<<'\n';return 1;}
}
