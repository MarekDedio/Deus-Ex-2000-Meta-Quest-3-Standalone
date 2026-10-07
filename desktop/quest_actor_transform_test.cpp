#include "Precomp.h"
#include "quest_actor_transform.h"
#include "surreal_portable_package_tables.h"
#include "Math/coords.h" // Actual pinned engine rotation/matrix oracle.

#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <limits>
#include <map>
#include <random>

namespace {
using Vec = QuestVr::ActorVec3;
using Bytes = std::vector<std::uint8_t>;
std::size_t checks{}, rejections{};
void Require(const bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
    ++checks;
}
void Near(const Vec& value, const Vec& expected, const char* message, const float epsilon=0.00005f) {
    Require(QuestVr::IsFiniteActorVector(value) && std::fabs(value.x-expected.x)<=epsilon &&
        std::fabs(value.y-expected.y)<=epsilon && std::fabs(value.z-expected.z)<=epsilon, message);
}
Vec Convert(const vec3& v) { return {v.x, v.y, v.z}; }
Vec QuestPoint(const vec3& v, const Vec& origin) {
    return {(v.y-origin.y)/52.5f, (v.z-origin.z)/52.5f+1.0f, -(v.x-origin.x)/52.5f};
}
Vec Cross(const Vec& a, const Vec& b) {
    return {a.y*b.z-a.z*b.y, a.z*b.x-a.x*b.z, a.x*b.y-a.y*b.x};
}
Vec Subtract(const Vec& a, const Vec& b) { return {a.x-b.x, a.y-b.y, a.z-b.z}; }
float Dot(const Vec& a, const Vec& b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
void Rejected(const std::function<void()>& action, const char* message) {
    bool rejected{}; try { action(); } catch (const std::runtime_error&) { rejected=true; }
    Require(rejected, message); ++rejections;
}
void RotationAndPlacement() {
    std::mt19937 random(0x2000u);
    std::uniform_int_distribution<std::int32_t> angles(-2000000, 2000000);
    std::uniform_real_distribution<float> coordinates(-100.0f, 100.0f);
    for (std::size_t sample=0u; sample<3000u; ++sample) {
        const auto pitch=angles(random), yaw=angles(random), roll=angles(random);
        const mat4 pinned=Coords::Rotation(Rotator(pitch, yaw, roll)).ToMatrix();
        const auto rotation=QuestVr::UnrealActorRotation(pitch, yaw, roll);
        for (const Vec axis : {Vec{1,0,0}, Vec{0,1,0}, Vec{0,0,1}, Vec{3,-7,2}}) {
            const auto p=(pinned*vec4(axis.x,axis.y,axis.z,0.0f)).xyz();
            Near(rotation.Transform(axis), Convert(p), "Rotation differs from actual pinned Coords");
        }
        const Vec location{coordinates(random),coordinates(random),coordinates(random)};
        const Vec pivot{coordinates(random),coordinates(random),coordinates(random)};
        const Vec origin{coordinates(random),coordinates(random),coordinates(random)};
        const Vec scale{0.5f,2.0f,-3.0f};
        const Vec point{coordinates(random),coordinates(random),coordinates(random)};
        const float drawScale=1.3f;
        const auto transform=QuestVr::BuildActorToQuest(location,pivot,pitch,yaw,roll,drawScale,scale,origin);
        const mat4 matrix=mat4::translate(location.x+pivot.x,location.y+pivot.y,location.z+pivot.z)*
            pinned*mat4::scale(drawScale*scale.x,drawScale*scale.y,drawScale*scale.z);
        Near(transform.TransformPoint(point), QuestPoint((matrix*vec4(point.x,point.y,point.z,1.0f)).xyz(),origin),
            "Mesh actor PrePivot/rotation/nonuniform scale placement differs");
        const auto brush=QuestVr::BuildBrushToQuest(location,pivot,pitch,yaw,roll,scale,origin);
        const mat4 brushMatrix=mat4::translate(location.x,location.y,location.z)*pinned*
            mat4::scale(scale.x,scale.y,scale.z)*mat4::translate(-pivot.x,-pivot.y,-pivot.z);
        Near(brush.TransformPoint(point), QuestPoint((brushMatrix*vec4(point.x,point.y,point.z,1.0f)).xyz(),origin),
            "Mover subtractive local PrePivot/MainScale differs");
    }
    // Positive UE pitch tilts +X upwards. Positive roll tilts +Y downwards.
    const auto pitch=QuestVr::BuildActorToQuest({}, {},16384,0,0,1,{1,1,1},{});
    Near(pitch.TransformPoint({52.5f,0,0}),{0,2,0},"Positive pitch still inverted");
    const auto roll=QuestVr::BuildActorToQuest({}, {},0,0,16384,1,{1,1,1},{});
    Near(roll.TransformPoint({0,52.5f,0}),{0,0,0},"Positive roll still inverted");
    const auto yaw=QuestVr::BuildActorToQuest({}, {},0,16384,0,1,{1,1,1},{});
    Near(yaw.TransformPoint({52.5f,0,0}),{1,1,0},"Authored yaw sign changed");
}
void NormalsAndMirrors() {
    const Vec v0{0,0,0}, v1{3,0,0}, v2{0,4,2};
    const Vec sourceNormal=QuestVr::NormalizeActorVector(Cross(Subtract(v1,v0),Subtract(v2,v0)));
    for (const Vec scale : {Vec{1,1,1}, Vec{2,3,4}, Vec{-2,3,4}, Vec{2,-3,-4}}) {
        const auto transform=QuestVr::BuildActorToQuest({20,30,40},{1,2,3},8192,35000,-6000,1.5f,scale,{});
        const Vec a=transform.TransformPoint(v0), b=transform.TransformPoint(v1), c=transform.TransformPoint(v2);
        const Vec n=transform.TransformNormal(sourceNormal);
        Require(std::fabs(Dot(n,Subtract(b,a)))<0.00001f && std::fabs(Dot(n,Subtract(c,a)))<0.00001f,
            "Inverse-transpose normal is not perpendicular after nonuniform actor scale");
        const Vec outward=QuestVr::NormalizeActorVector(transform.mirrored
            ? Cross(Subtract(c,a),Subtract(b,a)) : Cross(Subtract(b,a),Subtract(c,a)));
        Near(n,outward,"Reflection winding does not preserve source outward normal");
        Require(transform.mirrored == (scale.x*scale.y*scale.z>0.0f),
            "UE-to-Quest reflection determinant ignored actor mirror");
    }
    Rejected([] { QuestVr::BuildActorToQuest({}, {},0,0,0,0,{1,1,1},{}); },"Singular scale accepted");
    Rejected([] { QuestVr::BuildBrushToQuest({}, {},0,0,0,{1,0,1},{}); },"Singular MainScale accepted");
    Rejected([] { QuestVr::BuildActorToQuest({std::numeric_limits<float>::infinity(),0,0},{},0,0,0,1,{1,1,1},{}); },
        "Non-finite actor placement accepted");
}
void U16(Bytes& b, std::uint16_t value) { b.push_back(static_cast<std::uint8_t>(value)); b.push_back(static_cast<std::uint8_t>(value>>8u)); }
void U32(Bytes& b, std::uint32_t value) { for (unsigned shift=0;shift<32u;shift+=8u) b.push_back(static_cast<std::uint8_t>(value>>shift)); }
void Float(Bytes& b,float value) { std::uint32_t bits{}; std::memcpy(&bits,&value,4); U32(b,bits); }
void Vector(Bytes& b,float x,float y,float z) { Float(b,x);Float(b,y);Float(b,z); }
void Index(Bytes& b,std::int32_t value) {
    auto magnitude=static_cast<std::uint32_t>(value<0 ? -static_cast<std::int64_t>(value) : value);
    auto first=static_cast<std::uint8_t>((magnitude&0x3fu)|(value<0 ? 0x80u : 0u)); magnitude>>=6u;
    if (magnitude) first|=0x40u;
    b.push_back(first);
    while (magnitude) { auto byte=static_cast<std::uint8_t>(magnitude&0x7fu); magnitude>>=7u; if(magnitude) byte|=0x80u; b.push_back(byte); }
}
void Replace(Bytes& b,const std::size_t offset,const std::uint32_t value) {
    for (unsigned i=0u;i<4u;++i) b.at(offset+i)=static_cast<std::uint8_t>(value>>(i*8u));
}
struct Fixture {
    PortablePackageTables package;
    Bytes bytes;
    std::map<std::string,std::size_t> offsets;
    bool lod{};
    void Mark(const char* key) { offsets[key]=bytes.size(); }
    void LazyEnd(const char* key) { Replace(bytes,offsets[key],static_cast<std::uint32_t>(bytes.size()+37u)); }
    Fixture(const std::filesystem::path& path,const bool useLod=false,const bool remap=false) : lod(useLod) {
        package.sourcePath=path.string();package.version=68;
        package.names={{NameString("None"),0},{NameString(useLod ? "LodMesh" : "Mesh"),0},{NameString("Fixture"),0}};
        package.imports={{0,0,0,1}};
        package.exports={{-1,0,0,2,static_cast<ObjectFlags>(0),0,37}};
        Index(bytes,0);bytes.insert(bytes.end(),41u,0u);
        Mark("vertices.end");U32(bytes,0);Mark("vertices.count");Index(bytes,4);
        for (const Vec p : {Vec{0,0,0},Vec{10,0,0},Vec{0,10,0},Vec{0,0,10}}) {
            U16(bytes,static_cast<std::uint16_t>(p.x));U16(bytes,static_cast<std::uint16_t>(p.y));U16(bytes,static_cast<std::uint16_t>(p.z));U16(bytes,0);
        }
        LazyEnd("vertices.end");Mark("triangles.end");U32(bytes,0);Index(bytes,useLod ? 0 : 2);
        if(!useLod) for (unsigned face=0u;face<2u;++face) {
            for (const unsigned vertex : face==0u ? std::array<unsigned,3>{0,1,2} : std::array<unsigned,3>{0,3,1}) U16(bytes,static_cast<std::uint16_t>(vertex));
            for (unsigned corner=0u;corner<3u;++corner) {bytes.push_back(static_cast<std::uint8_t>(corner*100u));bytes.push_back(static_cast<std::uint8_t>(face*100u));}
            U32(bytes,face==0u ? 0x100u : 0x00400000u);U32(bytes,face);
        }
        LazyEnd("triangles.end");Index(bytes,0); // No animation sequences.
        Mark("connects.end");U32(bytes,0);Index(bytes,0);LazyEnd("connects.end");bytes.insert(bytes.end(),41u,0u);
        Mark("links.end");U32(bytes,0);Index(bytes,0);LazyEnd("links.end");Index(bytes,3);
        Index(bytes,0);Index(bytes,0);Index(bytes,0);Index(bytes,0);Index(bytes,0);
        U32(bytes,4);U32(bytes,1);U32(bytes,0);U32(bytes,0);
        Mark("scale");Vector(bytes,2,3,4);Mark("origin");Vector(bytes,1,2,3);
        Mark("rotation");U32(bytes,8192);U32(bytes,16384);U32(bytes,static_cast<std::uint32_t>(-4096));
        U32(bytes,0);U32(bytes,0);Index(bytes,0);
        if(useLod) {
            Index(bytes,0);Index(bytes,0);Index(bytes,2);
            for (unsigned face=0u;face<2u;++face) {
                for(const unsigned wedge : face==0u ? std::array<unsigned,3>{0,1,2} : std::array<unsigned,3>{0,3,1}) U16(bytes,static_cast<std::uint16_t>(wedge));
                U16(bytes,static_cast<std::uint16_t>(face));
            }
            Index(bytes,0);Index(bytes,4);
            for(unsigned i=0u;i<4u;++i) {U16(bytes,static_cast<std::uint16_t>(i));bytes.push_back(static_cast<std::uint8_t>(i*70u));bytes.push_back(static_cast<std::uint8_t>(i*20u));}
            Index(bytes,2);U32(bytes,0x100u);U32(bytes,2);U32(bytes,0x00400000u);U32(bytes,1);
            Index(bytes,0);U32(bytes,4);U32(bytes,0);bytes.insert(bytes.end(),24u,0u);
            Index(bytes,remap ? 4 : 0);
            if(remap) {U16(bytes,0);U16(bytes,2);U16(bytes,1);U16(bytes,3);}
            U32(bytes,4);
        }
    }
    void Save() {
        std::ofstream file(package.sourcePath,std::ios::binary|std::ios::trunc);
        Require(static_cast<bool>(file),"Could not create generated mesh fixture");
        const std::array<char,37> prefix{};file.write(prefix.data(),prefix.size());
        file.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));
        Require(static_cast<bool>(file),"Could not write generated mesh fixture");
        package.exports[0].ObjSize=static_cast<std::int32_t>(bytes.size());
    }
};
void SerializedMesh() {
    const auto directory=std::filesystem::temp_directory_path()/
        ("deusex-actor-transform-test-"+std::to_string(std::chrono::high_resolution_clock::now().time_since_epoch().count()));
    Require(std::filesystem::create_directory(directory),"Could not create isolated generated fixture directory");
    struct Cleanup {std::filesystem::path path;~Cleanup(){std::error_code ignored;std::filesystem::remove_all(path,ignored);}} cleanup{directory};
    const auto file=directory/"mesh-payload.bin";
    for(const bool lod : {false,true}) {
        Fixture fixture(file,lod);fixture.Save();
        const auto mesh=LoadPortableLodMesh(fixture.package,0);
        Require(mesh.rotationOriginPitch==8192 && mesh.rotationOriginYaw==16384 && mesh.rotationOriginRoll==-4096,
            "Serialized nonzero RotOrigin dropped");
        Require(mesh.triangles.size()==6u && mesh.frameVertices==4u && mesh.animationFrames==1u,"Serialized mesh topology changed");
        const mat4 matrix=Coords::Rotation(Rotator(8192,16384,-4096)).ToMatrix()*mat4::scale(2,3,4)*mat4::translate(-1,-2,-3);
        Near({mesh.triangles[0].x,mesh.triangles[0].y,mesh.triangles[0].z},Convert((matrix*vec4(0,0,0,1)).xyz()),
            "Mesh RotOrigin/Scale/Origin order differs from actual pinned UMesh");
        Near({mesh.triangles[1].x,mesh.triangles[1].y,mesh.triangles[1].z},Convert((matrix*vec4(10,0,0,1)).xyz()),
            "Mesh vertex did not receive RotOrigin");
        const auto linear=QuestVr::UnrealActorRotation(8192,16384,-4096)*QuestVr::ActorScaleMatrix({2,3,4});
        const auto expected=QuestVr::NormalizeActorVector(linear.NormalMatrix().Transform({0,1,1}));
        Near({mesh.triangles[0].nx,mesh.triangles[0].ny,mesh.triangles[0].nz},expected,
            "Unit face smoothing or object normal transform changed");
        Require(mesh.triangles[0].polyFlags==0x100u && mesh.triangles[3].polyFlags==0x00400000u,
            "Legacy/permaterial poly flags dropped");
        if(lod) Require(mesh.materialTextureIndices==std::vector<std::int32_t>({2,1}) &&
            mesh.materialPolyFlags==std::vector<std::uint32_t>({0x100u,0x00400000u}),"LOD material-to-texture mapping lost");
        Require(mesh.triangles[1].u == (lod ? 70.0f : 100.0f)/255.0f,"Byte UV normalization changed");
        const auto bad=[&](const std::function<void(Fixture&)>& edit,const char* message) {
            Fixture damaged(file,lod);edit(damaged);damaged.Save();
            Rejected([&]{LoadPortableLodMesh(damaged.package,0);},message);
        };
        bad([](Fixture& f){Replace(f.bytes,f.offsets["vertices.end"],0);},"Invalid absolute lazy-array end accepted");
        bad([](Fixture& f){f.bytes[f.offsets["vertices.count"]]=0x3fu;},"Impossible vertex allocation count accepted");
        bad([](Fixture& f){Replace(f.bytes,f.offsets["origin"],0x7fc00000u);},"NaN MeshOrigin accepted");
        bad([](Fixture& f){Replace(f.bytes,f.offsets["scale"],0);},"Singular mesh Scale accepted");
        bad([](Fixture& f){f.bytes.push_back(0);},"Trailing native mesh bytes accepted");
        bad([](Fixture& f){f.bytes.pop_back();},"Truncated native mesh tail accepted");
        bad([](Fixture& f){f.package.version=69;},"Unsupported mesh serialization version guessed");
    }
    Fixture remap(file,true,true);remap.Save();
    const auto mesh=LoadPortableLodMesh(remap.package,0);
    const mat4 matrix=Coords::Rotation(Rotator(8192,16384,-4096)).ToMatrix()*mat4::scale(2,3,4)*mat4::translate(-1,-2,-3);
    Near({mesh.triangles[1].x,mesh.triangles[1].y,mesh.triangles[1].z},Convert((matrix*vec4(0,10,0,1)).xyz()),
        "LOD ReMapAnimVerts ignored");
}
void OriginalMeshes(const std::filesystem::path& root) {
    std::size_t meshes{}, vertices{}, rotated{};
    for(const auto& file : std::filesystem::directory_iterator(root/"System")) {
        if(file.path().extension() != ".u") continue;
        const auto package=LoadPortablePackageTables(file.path().string());
        for(std::size_t i=0u;i<package.exports.size();++i) {
            const auto cls=GetPortableObjectPath(package,package.exports[i].ObjClass);
            const auto leaf=cls.substr(cls.find_last_of('.')+1u);
            if(leaf!="Mesh" && leaf!="LodMesh") continue;
            const auto mesh=LoadPortableLodMesh(package,i);++meshes;vertices+=mesh.triangles.size();
            if(mesh.rotationOriginPitch || mesh.rotationOriginYaw || mesh.rotationOriginRoll) {
                ++rotated;
                std::cout<<"Original rotated mesh "<<file.path().filename().string()<<":"<<GetPortableObjectPath(package,static_cast<std::int32_t>(i+1u))
                    <<" rotation="<<mesh.rotationOriginPitch<<","<<mesh.rotationOriginYaw<<","<<mesh.rotationOriginRoll
                    <<" triangleVertices="<<mesh.triangles.size()<<"\n";
            }
        }
    }
    Require(meshes>0u,"No owned original mesh exports found");
    std::cout<<"Original mesh exports decoded="<<meshes<<" triangleVertices="<<vertices<<" nonzeroRotOrigin="<<rotated<<".\n";
}
}
int main(int argc,char** argv) {
    try {
        RotationAndPlacement();NormalsAndMirrors();SerializedMesh();
        if(argc==3 && std::string(argv[1])=="--game-root") OriginalMeshes(argv[2]);
        else if(argc!=1) throw std::runtime_error("Usage: quest_actor_transform_test [--game-root <owned Deus Ex directory>]");
        std::cout<<"Actor transforms, original pinned Coords differential, serialized mesh RotOrigin/remap/normal/material flags: "
            <<checks<<" checks and "<<rejections<<" rejection controls passed.\n";
        return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<"\n";return 1;}
}
