#include "Precomp.h"
#include "GC/GC.h"
#include "portable_unreal_runtime.h"

#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

// Commercial-data-free v68 packages execute the production native1013 path.
// The Mesh/LodMesh contain real serialized geometry and raw texture references;
// expected lookup results come from the retail branch order, not the selector.
namespace {
using Bytes=std::vector<std::uint8_t>;
using Value=QuestVr::Vm::Value;
using Kind=QuestVr::Vm::Kind;
using Evaluation=QuestVr::Vm::Evaluation;
std::size_t checks{},refusals{};
void Require(bool condition,const std::string& reason) {++checks;if(!condition) throw std::runtime_error(reason);}
void U16(Bytes& bytes,std::uint16_t value) {bytes.push_back(static_cast<std::uint8_t>(value));bytes.push_back(static_cast<std::uint8_t>(value>>8u));}
void U32(Bytes& bytes,std::uint32_t value) {U16(bytes,static_cast<std::uint16_t>(value));U16(bytes,static_cast<std::uint16_t>(value>>16u));}
void U64(Bytes& bytes,std::uint64_t value) {U32(bytes,static_cast<std::uint32_t>(value));U32(bytes,static_cast<std::uint32_t>(value>>32u));}
void Float(Bytes& bytes,float value) {U32(bytes,std::bit_cast<std::uint32_t>(value));}
void Vector(Bytes& bytes,float x,float y,float z) {Float(bytes,x);Float(bytes,y);Float(bytes,z);}
void Index(Bytes& bytes,std::int32_t value) {
    auto magnitude=static_cast<std::uint32_t>(value<0?-static_cast<std::int64_t>(value):value);
    auto first=static_cast<std::uint8_t>((magnitude&63u)|(value<0?128u:0u));magnitude>>=6u;
    if(magnitude) first|=64u;
    bytes.push_back(first);
    while(magnitude) {auto next=static_cast<std::uint8_t>(magnitude&127u);magnitude>>=7u;if(magnitude) next|=128u;bytes.push_back(next);}
}
void Append(Bytes& bytes,const Bytes& suffix) {bytes.insert(bytes.end(),suffix.begin(),suffix.end());}
void Replace32(Bytes& bytes,std::size_t offset,std::uint32_t value) {
    for(unsigned byte=0u;byte<4u;++byte) bytes.at(offset+byte)=static_cast<std::uint8_t>(value>>(8u*byte));
}
Bytes ReadBytes(const std::filesystem::path& path) {
    const auto size=std::filesystem::file_size(path);Require(size<=16u*1024u*1024u,"Mesh query fixture/save exceeds read bound");
    Bytes bytes(static_cast<std::size_t>(size));std::ifstream file(path,std::ios::binary);
    Require(static_cast<bool>(file),"Cannot read mesh query fixture/save");
    if(!bytes.empty()) file.read(reinterpret_cast<char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));
    Require(static_cast<bool>(file),"Mesh query fixture/save truncated");return bytes;
}
void WriteBytes(const std::filesystem::path& path,const Bytes& bytes) {
    std::ofstream file(path,std::ios::binary|std::ios::trunc);Require(static_cast<bool>(file),"Cannot create generated mesh query file");
    file.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));
    Require(static_cast<bool>(file),"Cannot write generated mesh query file");
}
struct Code {Bytes raw;std::size_t logical{};};
Code Token(std::uint8_t opcode) {return {{opcode},1u};}
Code Ref(std::uint8_t opcode,std::int32_t reference) {Code code{{opcode},5u};Index(code.raw,reference);return code;}
Code Integer(std::int32_t value) {Code code{{0x1du},5u};U32(code.raw,static_cast<std::uint32_t>(value));return code;}
Code Join(std::initializer_list<Code> parts) {Code code;for(const auto& part:parts) {Append(code.raw,part.raw);code.logical+=part.logical;}return code;}
Code Return(Code expression=Token(0x0bu)) {return Join({Token(0x04u),std::move(expression)});}
Code Assign(Code left,Code right) {return Join({Token(0x0fu),std::move(left),std::move(right)});}
Code Slot(std::int32_t property,Code index,bool defaults=false) {return Join({Token(0x1au),std::move(index),Ref(defaults?0x02u:0x01u,property)});}
Code Native(std::uint16_t native,std::initializer_list<Code> arguments={}) {
    Code code;
    if(native>=256u) {code.raw={static_cast<std::uint8_t>(0x60u+(native>>8u)),static_cast<std::uint8_t>(native)};code.logical=2u;}
    else {code.raw={static_cast<std::uint8_t>(native)};code.logical=1u;}
    for(const auto& argument:arguments) {Append(code.raw,argument.raw);code.logical+=argument.logical;}
    return Join({code,Token(0x16u)});
}
Code Final(std::int32_t function,std::initializer_list<Code> arguments={}) {
    auto code=Ref(0x1cu,function);for(const auto& argument:arguments) {Append(code.raw,argument.raw);code.logical+=argument.logical;}
    return Join({code,Token(0x16u)});
}
Code ClassContext(std::int32_t classReference,Code expression) {
    Require(expression.logical<=65535u,"Generated mesh ClassContext skip exceeds bound");
    auto code=Join({Token(0x12u),Ref(0x20u,classReference)});
    U16(code.raw,static_cast<std::uint16_t>(expression.logical));code.raw.push_back(4u);code.logical+=3u;
    Append(code.raw,expression.raw);code.logical+=expression.logical;return code;
}

struct Package {
    std::string stem;
    std::vector<std::string> names{"None"};
    std::vector<ImportTableEntry> imports;
    std::vector<ExportTableEntry> exports;
    std::vector<std::function<Bytes(std::int32_t,std::int32_t)>> builders;
    std::map<std::int32_t,std::vector<std::int32_t>> children;
    // Each pair is a body-local lazy-array end pointer and its body-local end.
    std::map<std::int32_t,std::vector<std::pair<std::size_t,std::size_t>>> absoluteEnds;
    std::int32_t Name(const std::string& name) {
        const auto found=std::find(names.begin(),names.end(),name);if(found!=names.end()) return static_cast<std::int32_t>(found-names.begin());
        names.push_back(name);return static_cast<std::int32_t>(names.size()-1u);
    }
    std::int32_t Import(const std::string& name,std::int32_t outer,const std::string& type="Class",const std::string& source="Core") {
        imports.push_back({Name(source),Name(type),outer,Name(name)});return -static_cast<std::int32_t>(imports.size());
    }
    std::int32_t ImportPackage(const std::string& name) {return Import(name,0,"Package");}
    std::int32_t Export(const std::string& name,std::int32_t cls=0,std::int32_t base=0,std::int32_t outer=0,bool field=false) {
        exports.push_back({cls,base,outer,Name(name),ObjectFlags{},0,-1});builders.emplace_back();
        const auto reference=static_cast<std::int32_t>(exports.size());if(field) children[outer].push_back(reference);return reference;
    }
    void Body(std::int32_t reference,Bytes body) {
        builders.at(static_cast<std::size_t>(reference-1))=[body=std::move(body)](std::int32_t,std::int32_t) {return body;};
    }
    void ObjectTag(Bytes& bytes,const std::string& name,std::int32_t value,std::uint8_t slot=0u) {
        Bytes data;Index(data,value);Index(bytes,Name(name));bytes.push_back(static_cast<std::uint8_t>(slot?0xd5u:0x55u));
        bytes.push_back(static_cast<std::uint8_t>(data.size()));if(slot) bytes.push_back(slot);Append(bytes,data);
    }
    void ClassBody(std::int32_t reference,Bytes defaults={}) {
        builders.at(static_cast<std::size_t>(reference-1))=[this,reference,defaults=std::move(defaults)](std::int32_t next,std::int32_t first) {
            const auto& entry=exports.at(static_cast<std::size_t>(reference-1));Bytes body;
            Index(body,entry.ObjBase);Index(body,next);Index(body,0);Index(body,first);Index(body,entry.ObjName);
            U32(body,0);U32(body,0);U32(body,0);U64(body,~std::uint64_t{});U64(body,~std::uint64_t{});
            U16(body,0xffffu);U32(body,0);U32(body,0);body.resize(body.size()+16u,0u);
            for(unsigned index=0u;index<4u;++index) Index(body,0);
            Append(body,defaults);Index(body,0);return body;
        };
    }
    std::int32_t Property(const std::string& name,const std::string& type,std::int32_t owner,std::int32_t core,
        std::int32_t target=0,std::uint32_t flags=0u,std::uint32_t dimension=1u) {
        const auto reference=Export(name,Import(type,core),0,owner,true);
        builders.at(static_cast<std::size_t>(reference-1))=[type,target,flags,dimension](std::int32_t next,std::int32_t) {
            Bytes body{0u};Index(body,0);Index(body,next);U32(body,dimension);U32(body,flags);Index(body,0);
            if((flags&0x20u)!=0u) U16(body,0u); // Authored CPF_Net replication offset precedes subtype data.
            if(type=="ObjectProperty" || type=="ByteProperty" || type=="StructProperty") Index(body,target);
            return body;
        };return reference;
    }
    std::int32_t Function(const std::string& name,std::int32_t owner,std::int32_t core,Code code={},
        std::uint16_t native=0u,std::uint32_t flags=2u) {
        const auto reference=Export(name,Import("Function",core),0,owner,true);FunctionCode(reference,std::move(code),native,flags);return reference;
    }
    void FunctionCode(std::int32_t reference,Code code,std::uint16_t native=0u,std::uint32_t flags=2u) {
        builders.at(static_cast<std::size_t>(reference-1))=[this,reference,code=std::move(code),native,flags](std::int32_t next,std::int32_t first) {
            Bytes body{0u};Index(body,0);Index(body,next);Index(body,0);Index(body,first);
            Index(body,exports.at(static_cast<std::size_t>(reference-1)).ObjName);U32(body,0);U32(body,0);
            U32(body,static_cast<std::uint32_t>(code.logical));Append(body,code.raw);U16(body,native);body.push_back(0u);U32(body,flags);return body;
        };
    }
    void ActorBody(std::int32_t reference,Bytes tags={}) {Index(tags,0);Body(reference,std::move(tags));}
    Bytes Serialize() {
        std::map<std::int32_t,std::int32_t> next;
        for(const auto& [owner,list]:children) {static_cast<void>(owner);for(std::size_t index=1u;index<list.size();++index) next[list[index-1u]]=list[index];}
        Bytes bytes;U32(bytes,0x9e2a83c1u);U16(bytes,68u);U16(bytes,0u);bytes.resize(56u,0u);
        std::vector<Bytes> bodies;std::vector<std::int32_t> offsets;
        for(std::size_t index=0u;index<exports.size();++index) {
            const auto reference=static_cast<std::int32_t>(index+1u);const auto found=children.find(reference);
            auto body=builders[index]?builders[index](next[reference],found==children.end()||found->second.empty()?0:found->second.front()):Bytes{};
            offsets.push_back(static_cast<std::int32_t>(bytes.size()));
            for(const auto& [pointer,end]:absoluteEnds[reference]) Replace32(body,pointer,static_cast<std::uint32_t>(bytes.size()+end));
            Append(bytes,body);bodies.push_back(std::move(body));
        }
        Replace32(bytes,12u,static_cast<std::uint32_t>(names.size()));Replace32(bytes,16u,static_cast<std::uint32_t>(bytes.size()));
        for(const auto& name:names) {Index(bytes,static_cast<std::int32_t>(name.size()+1u));bytes.insert(bytes.end(),name.begin(),name.end());bytes.push_back(0u);U32(bytes,0);}
        Replace32(bytes,20u,static_cast<std::uint32_t>(exports.size()));Replace32(bytes,24u,static_cast<std::uint32_t>(bytes.size()));
        for(std::size_t index=0u;index<exports.size();++index) {
            const auto& entry=exports[index];Index(bytes,entry.ObjClass);Index(bytes,entry.ObjBase);U32(bytes,static_cast<std::uint32_t>(entry.ObjOuter));
            Index(bytes,entry.ObjName);U32(bytes,static_cast<std::uint32_t>(entry.ObjFlags));Index(bytes,static_cast<std::int32_t>(bodies[index].size()));
            if(!bodies[index].empty()) Index(bytes,offsets[index]);
        }
        Replace32(bytes,28u,static_cast<std::uint32_t>(imports.size()));Replace32(bytes,32u,static_cast<std::uint32_t>(bytes.size()));
        for(const auto& entry:imports) {Index(bytes,entry.ClassPackage);Index(bytes,entry.ClassName);U32(bytes,static_cast<std::uint32_t>(entry.ObjOuter));Index(bytes,entry.ObjName);}
        return bytes;
    }
};
struct Fixture {
    std::filesystem::path parent,directory;
    std::map<std::filesystem::path,Bytes> sources;
    Fixture() {
        parent=std::filesystem::canonical(std::filesystem::temp_directory_path());
        const auto stamp=std::chrono::high_resolution_clock::now().time_since_epoch().count();
        for(unsigned attempt=0u;attempt<20u;++attempt) {
            const auto candidate=parent/("deusex-mesh-texture-test-"+std::to_string(stamp)+'-'+std::to_string(attempt));
            if(std::filesystem::create_directory(candidate)) {directory=std::filesystem::canonical(candidate);break;}
        }
        Require(!directory.empty()&&directory.parent_path()==parent,"Mesh texture fixture escaped owned temporary parent");
        std::filesystem::create_directory(directory/"System");std::filesystem::create_directory(directory/"Maps");
    }
    ~Fixture() {
        ShutdownPortableRuntime();std::error_code error;const auto actual=std::filesystem::weakly_canonical(directory,error);
        if(!error&&!directory.empty()&&actual==directory&&actual.parent_path()==parent&&
            actual.filename().string().rfind("deusex-mesh-texture-test-",0u)==0u) std::filesystem::remove_all(actual,error);
    }
    PortablePackageTables Write(Package& package,bool map=false) {
        const auto path=directory/(map?"Maps":"System")/(package.stem+(map?".dx":".u"));
        const auto bytes=package.Serialize();WriteBytes(path,bytes);sources.emplace(path,bytes);
        try {return LoadPortablePackageTables(path.string());}
        catch(const std::exception& error) {throw std::runtime_error(package.stem+" table decoding: "+error.what());}
    }
    Bytes Snapshot(const std::string& label) const {
        const auto path=directory/(label+".sav");Require(SavePortableRuntimeState(path.string()),"Cannot snapshot mesh texture runtime");return ReadBytes(path);
    }
    void UnchangedSources() const {for(const auto& [path,bytes]:sources) Require(ReadBytes(path)==bytes,"Mesh native query modified package bytes");}
};

// Serialized native mesh body adapted from the independent transform test.
// Absolute lazy offsets are fixed only after the complete package layout exists.
void MeshBody(Package& package,std::int32_t reference,const std::array<std::int32_t,3u>& textures,bool lod) {
    Bytes bytes{0u};bytes.insert(bytes.end(),41u,0u);
    const auto lazy=[&](const std::function<void()>& contents) {
        const auto pointer=bytes.size();U32(bytes,0);contents();package.absoluteEnds[reference].emplace_back(pointer,bytes.size());
    };
    lazy([&] {Index(bytes,4);for(const auto& point:std::array<std::array<std::uint16_t,3u>,4u>{{{0,0,0},{10,0,0},{0,10,0},{0,0,10}}}) {
        for(const auto component:point) U16(bytes,component);
        U16(bytes,0);
    }});
    lazy([&] {Index(bytes,lod?0:2);if(!lod) for(unsigned face=0u;face<2u;++face) {
        for(const auto vertex:face==0u?std::array<unsigned,3u>{0,1,2}:std::array<unsigned,3u>{0,3,1}) U16(bytes,static_cast<std::uint16_t>(vertex));
        for(unsigned corner=0u;corner<3u;++corner) {bytes.push_back(static_cast<std::uint8_t>(corner*100u));bytes.push_back(static_cast<std::uint8_t>(face*100u));}
        U32(bytes,0);U32(bytes,face);
    }});
    Index(bytes,0);lazy([&] {Index(bytes,0);});bytes.insert(bytes.end(),41u,0u);lazy([&] {Index(bytes,0);});
    Index(bytes,3);for(const auto texture:textures) Index(bytes,texture);Index(bytes,0);Index(bytes,0);
    U32(bytes,4);U32(bytes,1);U32(bytes,0);U32(bytes,0);Vector(bytes,1,1,1);Vector(bytes,0,0,0);
    U32(bytes,0);U32(bytes,0);U32(bytes,0);U32(bytes,0);U32(bytes,0);Index(bytes,0);
    if(lod) {
        Index(bytes,0);Index(bytes,0);Index(bytes,2);
        for(unsigned face=0u;face<2u;++face) {
            for(const auto wedge:face==0u?std::array<unsigned,3u>{0,1,2}:std::array<unsigned,3u>{0,3,1}) U16(bytes,static_cast<std::uint16_t>(wedge));
            U16(bytes,static_cast<std::uint16_t>(face));
        }
        Index(bytes,0);Index(bytes,4);for(unsigned vertex=0u;vertex<4u;++vertex) {
            U16(bytes,static_cast<std::uint16_t>(vertex));bytes.push_back(static_cast<std::uint8_t>(vertex*70u));bytes.push_back(static_cast<std::uint8_t>(vertex*20u));
        }
        // Deliberate material0->rawtexture2 and material1->rawtexture1 mapping.
        Index(bytes,2);U32(bytes,0);U32(bytes,2);U32(bytes,0);U32(bytes,1);
        Index(bytes,0);U32(bytes,4);U32(bytes,0);bytes.insert(bytes.end(),24u,0u);Index(bytes,0);U32(bytes,4);
    }
    package.Body(reference,std::move(bytes));
}

enum class Variant {Good,WrongFlags,RequiredIndex,WrongReturnFlags,WrongReturnClass,
    WrongMeshClass,WrongSkinClass,WrongMultiDimension,NonNativeActor,ForgedRawTexture};
struct Tables {std::vector<PortablePackageTables> scripts;PortablePackageTables map;};
Tables Build(Fixture& fixture,Variant variant=Variant::Good) {
    Package core;core.stem="Core";const auto object=core.Export("Object"),metaclass=core.Export("Class",0,object);
    core.ClassBody(object);core.ClassBody(metaclass);const auto coreTable=fixture.Write(core);
    Package fake;fake.stem="Fake";const auto fc=fake.ImportPackage("Core"),fo=fake.Import("Object",fc);
    const auto fakeMesh=fake.Export("Mesh",0,fo),fakeTexture=fake.Export("Texture",0,fo);
    fake.ClassBody(fakeMesh);fake.ClassBody(fakeTexture);const auto forged=fake.Export("ForgedTexture",fakeTexture);fake.ActorBody(forged);
    const auto fakeTable=fixture.Write(fake);
    Package engine;engine.stem="Engine";const auto ec=engine.ImportPackage("Core"),eo=engine.Import("Object",ec);
    const auto actor=engine.Export("Actor",0,eo),primitive=engine.Export("Primitive",0,eo),mesh=engine.Export("Mesh",0,primitive);
    const auto lod=engine.Export("LodMesh",0,mesh),bitmap=engine.Export("Bitmap",0,eo),texture=engine.Export("Texture",0,bitmap);
    for(const auto cls:{actor,primitive,mesh,lod,bitmap,texture}) {
        engine.exports.at(static_cast<std::size_t>(cls-1)).ObjFlags=static_cast<ObjectFlags>(0x04000000u);engine.ClassBody(cls);
    }
    if(variant==Variant::NonNativeActor) engine.exports.at(static_cast<std::size_t>(actor-1)).ObjFlags=ObjectFlags{};
    const auto fakePackage=engine.ImportPackage("Fake"),wrongMesh=engine.Import("Mesh",fakePackage),wrongTexture=engine.Import("Texture",fakePackage);
    engine.Property("Mesh","ObjectProperty",actor,ec,variant==Variant::WrongMeshClass?wrongMesh:mesh,0x21u);
    engine.Property("Skin","ObjectProperty",actor,ec,variant==Variant::WrongSkinClass?wrongTexture:texture,0x21u);
    engine.Property("MultiSkins","ObjectProperty",actor,ec,texture,0x21u,variant==Variant::WrongMultiDimension?7u:8u);
    engine.Property("Texture","ObjectProperty",actor,ec,texture);
    const auto get=engine.Function("GetMeshTexture",actor,ec,{},1013u,variant==Variant::WrongFlags?0x501u:0x401u);
    const auto input=engine.Property("texnum","IntProperty",get,ec,0,variant==Variant::RequiredIndex?0x80u:0x90u);
    engine.Property("ReturnValue","ObjectProperty",get,ec,variant==Variant::WrongReturnClass?wrongTexture:texture,
        variant==Variant::WrongReturnFlags?0x480u:0x580u);
    engine.FunctionCode(get,Ref(0x29u,input),1013u,variant==Variant::WrongFlags?0x501u:0x401u);
    const auto engineTable=fixture.Write(engine);
    Package assets;assets.stem="MeshAssets";const auto ae=assets.ImportPackage("Engine"),at=assets.Import("Texture",ae);
    const auto t0=assets.Export("Raw0",at),t1=assets.Export("Raw1",at),skin=assets.Export("Skin",at),multi=assets.Export("Multi",at);
    for(const auto textureRef:{t0,t1,skin,multi}) assets.ActorBody(textureRef);
    const auto legacy=assets.Export("Legacy",assets.Import("Mesh",ae)),lodAsset=assets.Export("Lod",assets.Import("LodMesh",ae));
    const auto forgedRef=assets.Import("ForgedTexture",assets.ImportPackage("Fake"),"Texture","Fake");
    MeshBody(assets,legacy,{t0,variant==Variant::ForgedRawTexture?forgedRef:t1,0},false);
    MeshBody(assets,lodAsset,{t0,t1,0},true);const auto assetsTable=fixture.Write(assets);
    for(const auto asset:{legacy,lodAsset}) {
        PortableLodMesh decoded;
        try {decoded=LoadPortableLodMesh(assetsTable,static_cast<std::size_t>(asset-1));}
        catch(const std::exception& error) {throw std::runtime_error(std::string(asset==legacy?"Legacy":"LOD")+" generated mesh payload decoding: "+error.what());}
        Require(decoded.frameVertices==4u&&decoded.animationFrames==1u&&decoded.triangles.size()==6u&&decoded.textures.size()==3u,
            "Generated native Mesh/LodMesh did not decode real geometry/raw texture slots");
        if(asset==lodAsset) Require(decoded.materialTextureIndices==std::vector<std::int32_t>({2,1}),"LOD raw/material regression mapping lost");
    }
    Package classes;classes.stem="MeshClasses";const auto cc=classes.ImportPackage("Core"),ce=classes.ImportPackage("Engine");
    const auto ca=classes.Import("Actor",ce),ct=classes.Import("Texture",ce),cm=classes.Import("Mesh",ce);
    const auto probe=classes.Export("Probe",0,ca),derived=classes.Export("Derived",0,probe),shadow=classes.Export("Shadow",0,probe);
    const auto nativeMesh=classes.Import("Mesh",ca,"ObjectProperty"),nativeSkin=classes.Import("Skin",ca,"ObjectProperty");
    const auto nativeMulti=classes.Import("MultiSkins",ca,"ObjectProperty"),getTexture=classes.Import("GetMeshTexture",ca,"Function");
    const auto counter=classes.Property("Counter","IntProperty",probe,cc);
    const auto assetPackage=classes.ImportPackage("MeshAssets"),legacyRef=classes.Import("Legacy",assetPackage,"Mesh","Engine");
    const auto skinRef=classes.Import("Skin",assetPackage,"Texture","Engine"),multiRef=classes.Import("Multi",assetPackage,"Texture","Engine");
    const auto query=[&](const char* name,Code body) {
        const auto function=classes.Function(name,probe,cc,std::move(body));classes.Property("ReturnValue","ObjectProperty",function,cc,ct,0x580u);return function;
    };
    query("QueryDefault",Return(Native(1013u)));query("QueryNothing",Return(Native(1013u,{Token(0x0bu)})));
    query("QueryFinalDefault",Return(Final(getTexture)));query("QueryFinalNothing",Return(Final(getTexture,{Token(0x0bu)})));
    for(const bool reflected:{false,true}) {
        const auto function=classes.Function(reflected?"QueryFinal":"QueryRaw",probe,cc);
        const auto index=classes.Property("Index","IntProperty",function,cc,0,0x80u);classes.Property("ReturnValue","ObjectProperty",function,cc,ct,0x580u);
        classes.FunctionCode(function,Return(reflected?Final(getTexture,{Ref(0x00u,index)}):Native(1013u,{Ref(0x00u,index)})));
    }
    const auto setObject=[&](const char* name,std::int32_t field,std::int32_t constraint,bool defaults=false,std::int32_t owner=0) {
        const auto function=classes.Function(name,owner?owner:probe,cc);const auto value=classes.Property("Value","ObjectProperty",function,cc,constraint,0x80u);
        classes.FunctionCode(function,Join({Assign(Ref(defaults?0x02u:0x01u,field),Ref(0x00u,value)),Return()}));return function;
    };
    setObject("SetMesh",nativeMesh,cm);setObject("SetSkin",nativeSkin,ct);setObject("SetDefaultSkin",nativeSkin,ct,true);
    const auto setMulti=classes.Function("SetMulti",probe,cc);const auto mi=classes.Property("Index","IntProperty",setMulti,cc,0,0x80u);
    const auto mv=classes.Property("Value","ObjectProperty",setMulti,cc,ct,0x80u);
    classes.FunctionCode(setMulti,Join({Assign(Slot(nativeMulti,Ref(0x00u,mi)),Ref(0x00u,mv)),Return()}));
    query("WriteQueryThenFail",Join({Assign(Ref(0x01u,counter),Integer(44)),Native(1013u,{Integer(1)}),Native(999u),Return()}));
    const auto setThenIndex=classes.Function("SetMultiReturnIndex",probe,cc,
        Join({Assign(Slot(nativeMulti,Integer(1)),Ref(0x20u,multiRef)),Return(Integer(1))}));
    classes.Property("ReturnValue","IntProperty",setThenIndex,cc,0,0x580u);
    query("QueryArgumentWrites",Return(Native(1013u,{Final(setThenIndex)})));
    query("QueryCdo",Return(ClassContext(probe,Native(1013u))));
    const auto shadowSkin=classes.Property("Skin","ObjectProperty",shadow,cc,ct);
    const auto shadowMulti=classes.Property("MultiSkins","ObjectProperty",shadow,cc,ct,0,8u);
    const auto shadowMesh=classes.Property("Mesh","ObjectProperty",shadow,cc,cm);
    setObject("SetShadowSkin",shadowSkin,ct,false,shadow);
    Bytes defaults;classes.ObjectTag(defaults,"Mesh",legacyRef);classes.ObjectTag(defaults,"Skin",skinRef);
    classes.ObjectTag(defaults,"MultiSkins",multiRef,3u);classes.ClassBody(probe,std::move(defaults));classes.ClassBody(derived);
    Bytes shadowDefaults;classes.ObjectTag(shadowDefaults,"Skin",multiRef);classes.ObjectTag(shadowDefaults,"Mesh",0);
    classes.ObjectTag(shadowDefaults,"MultiSkins",multiRef,0u);classes.ClassBody(shadow,std::move(shadowDefaults));
    static_cast<void>(shadowMulti);static_cast<void>(shadowMesh);const auto classesTable=fixture.Write(classes);
    Package map;map.stem="MeshFixture";const auto mc=map.ImportPackage("MeshClasses"),ma=map.ImportPackage("MeshAssets");
    const auto mp=map.Import("Probe",mc),ms=map.Import("Shadow",mc),md=map.Import("Derived",mc);
    const auto lodRef=map.Import("Lod",ma,"LodMesh","Engine"),multiMap=map.Import("Multi",ma,"Texture","Engine");
    for(const auto* name:{"Driver","Plain","NoSkin","NoMesh","Derived","Shadow"}) {
        const auto actorRef=map.Export(name,std::string(name)=="Shadow"?ms:std::string(name)=="Derived"?md:mp);Bytes tags;
        if(std::string(name)=="Driver") map.ObjectTag(tags,"Mesh",lodRef);
        if(std::string(name)=="NoSkin") map.ObjectTag(tags,"Skin",0);
        if(std::string(name)=="NoMesh") {map.ObjectTag(tags,"Mesh",0);map.ObjectTag(tags,"MultiSkins",multiMap);}
        map.ActorBody(actorRef,std::move(tags));
    }
    return {{coreTable,engineTable,fakeTable,assetsTable,classesTable},fixture.Write(map,true)};
}

std::string Path(const std::string& actor) {return "MeshFixture."+actor;}
Evaluation Int(std::int32_t value) {return {Value::Integer(value),{}};}
Evaluation Object(const std::string& value) {return {Value::Text(Kind::Object,value),{}};}
QuestVr::Vm::Result Call(const char* actor,const char* function,const std::vector<Evaluation>& arguments={}) {
    const auto result=ExecutePortableActorFunction(Path(actor),function,arguments);
    Require(result.passed()&&result.committed,std::string(actor)+'.'+function+" failed: "+result.error+" at "+result.function+':'+std::to_string(result.offset));return result;
}
void Texture(const QuestVr::Vm::Result& result,const std::string& expected,const char* reason) {
    Require(result.value.kind==Kind::Object&&result.value.text==expected,std::string(reason)+": expected "+expected+", got "+result.value.text);
}
void Refused(Fixture& fixture,const char* actor,const char* function,const std::vector<Evaluation>& arguments={}) {
    const auto before=fixture.Snapshot("BeforeRefusal");const auto revision=GetPortableRuntimeWorldRevision();
    const auto gc=GC::GetStats();const auto result=ExecutePortableActorFunction(Path(actor),function,arguments);const auto after=GC::GetStats();
    Require(!result.passed()&&!result.committed,std::string("Invalid mesh native call committed: ")+function);
    Require(fixture.Snapshot("AfterRefusal")==before&&GetPortableRuntimeWorldRevision()==revision&&
        gc.numObjects==after.numObjects&&gc.memoryUsage==after.memoryUsage,"Refused mesh native leaked writes/revision/UObject roots");++refusals;
}
void Positive() {
    Fixture fixture;const auto tables=Build(fixture);Require(InitializePortableRuntime(tables.scripts).passed&&LoadPortableRuntimeMap(tables.map).passed,
        "Generated mesh texture runtime/map failed initialization");
    Texture(Call("Driver","QueryCdo"),"MeshAssets.Skin","Read-only native1013 incorrectly refused actual concrete Actor default object");
    const auto baseline=fixture.Snapshot("Baseline");const auto revision=GetPortableRuntimeWorldRevision();const auto gc=GC::GetStats();
    for(const auto* actor:{"Driver","Plain"}) {
        for(const auto* function:{"GetMeshTexture","QueryDefault","QueryNothing","QueryFinalDefault","QueryFinalNothing"})
            Texture(Call(actor,function),"MeshAssets.Skin","Optional/default raw texture index is not zero");
        for(const auto index:{std::numeric_limits<std::int32_t>::min(),-9,-1,0,1,2,3,4,7,8,9,std::numeric_limits<std::int32_t>::max()}) {
            const std::string expected=index==1?"MeshAssets.Raw1":index==3?"MeshAssets.Multi":"MeshAssets.Skin";
            for(const auto* function:{"GetMeshTexture","QueryRaw","QueryFinal"}) Texture(Call(actor,function,{Int(index)}),expected,
                "Native raw texture priority/signed-index result differs from independent branch tree");
        }
    }
    for(const auto index:{-1,0,1,2,3,7,8,std::numeric_limits<std::int32_t>::max()}) {
        const std::string expected=index==0?"MeshAssets.Raw0":index==1?"MeshAssets.Raw1":index==3?"MeshAssets.Multi":"";
        Texture(Call("NoSkin","QueryRaw",{Int(index)}),expected,"Explicit None Skin failed raw-slot/None fallback");
        Texture(Call("NoMesh","QueryRaw",{Int(index)}),"","Mesh=None failed early return before Skin/MultiSkins");
    }
    const auto after=GC::GetStats();Require(fixture.Snapshot("AfterReadOnly")==baseline&&GetPortableRuntimeWorldRevision()==revision&&
        gc.numObjects==after.numObjects&&gc.memoryUsage==after.memoryUsage,"Read-only native1013 changed complete state/revision/roots");
    Refused(fixture,"Driver","GetMeshTexture",{Int(0),Int(1)});Refused(fixture,"Driver","WriteQueryThenFail");
    Refused(fixture,"Driver","SetSkin",{Object(Path("Driver"))});Refused(fixture,"Driver","SetMesh",{Object("Fake.ForgedTexture")});
    Refused(fixture,"Driver","SetSkin",{Object("Fake.ForgedTexture")});
    Texture(Call("Driver","QueryArgumentWrites"),"MeshAssets.Multi","Index-expression writes were not visible to native texture lookup");
    Call("Driver","SetMulti",{Int(1),Object("")});Texture(Call("Driver","QueryRaw",{Int(1)}),"MeshAssets.Raw1","None override incorrectly masked positive raw mesh slot");
    Call("Driver","SetMulti",{Int(0),Object("MeshAssets.Multi")});Texture(Call("Driver","QueryDefault"),"MeshAssets.Multi","MultiSkins0 did not precede Skin");
    Call("Driver","SetMulti",{Int(0),Object("")});Texture(Call("Driver","QueryDefault"),"MeshAssets.Skin","Cleared MultiSkins0 did not restore Skin fallback");
    Call("Driver","SetMulti",{Int(7),Object("MeshAssets.Multi")});Texture(Call("Driver","QueryRaw",{Int(7)}),"MeshAssets.Multi","Eight-slot override incorrectly depended on raw texture count");
    Texture(Call("Driver","QueryRaw",{Int(8)}),"MeshAssets.Skin","Slot8 wrapped or clamped to MultiSkins7");
    Call("Driver","SetDefaultSkin",{Object("MeshAssets.Multi")});
    Require(ReadPortableClassDefault("MeshClasses.Probe","Skin").text=="MeshAssets.Multi","Default Skin setter failed to mutate concrete CDO");
    Texture(Call("Driver","QueryCdo"),"MeshAssets.Multi","Read-only CDO query ignored committed actual native-field default mutation");
    for(const auto* actor:{"Driver","Plain","Derived"}) Texture(Call(actor,"QueryDefault"),"MeshAssets.Skin","Post-birth CDO mutation rewrote frozen instance material");
    Require(ReadPortableClassDefault("MeshClasses.Derived","Skin").text=="MeshAssets.Skin","Parent CDO mutation rewrote frozen derived default block");
    // Shadow fields and actual native fields have independent declaring paths.
    Require(ReadPortableActorScriptProperty(Path("Shadow"),"Skin").text=="MeshAssets.Multi","Shadow regression did not create same-name distinct Skin field");
    Require(ReadPortableActorDeclaredScriptProperty(Path("Shadow"),"Engine.Actor.Skin").text=="MeshAssets.Skin",
        "Inherited native Skin field was not independently retained");
    Texture(Call("Shadow","QueryDefault"),"MeshAssets.Skin","Native query used closest same-name shadow Mesh/Skin/MultiSkins");
    Call("Shadow","SetShadowSkin",{Object("")});Texture(Call("Shadow","QueryRaw",{Int(1)}),"MeshAssets.Raw1","Shadow Skin write changed native inherited positive slot");
    Call("Driver","SetSkin",{Object("")});Texture(Call("Driver","QueryRaw",{Int(2)}),"","Null raw slot manufactured actor.Texture/default texture fallback");
    Call("Driver","SetMesh",{Object("")});Texture(Call("Driver","QueryRaw",{Int(7)}),"","Live Mesh clear failed early return despite override");
    fixture.UnchangedSources();
}
void NegativeDeclarations() {
    for(const auto variant:{Variant::WrongFlags,Variant::RequiredIndex,Variant::WrongReturnFlags,Variant::WrongReturnClass,
        Variant::WrongMeshClass,Variant::WrongSkinClass,Variant::WrongMultiDimension,Variant::NonNativeActor,Variant::ForgedRawTexture}) {
        Fixture fixture;const auto tables=Build(fixture,variant);
        const auto initialized=InitializePortableRuntime(tables.scripts);
        if(!initialized.passed) {++refusals;fixture.UnchangedSources();continue;}
        const auto loaded=LoadPortableRuntimeMap(tables.map);
        if(!loaded.passed) {++refusals;fixture.UnchangedSources();continue;}
        if(variant==Variant::WrongSkinClass) {
            Texture(Call("Plain","QueryRaw",{Int(1)}),"MeshAssets.Raw1","Nonnull positive mesh slot unnecessarily read malformed unused Skin");
            Texture(Call("Plain","GetMeshTexture",{Int(1)}),"MeshAssets.Raw1","Reflected positive mesh slot unnecessarily read malformed unused Skin");
            Refused(fixture,"Plain","QueryRaw",{Int(0)});Refused(fixture,"Plain","GetMeshTexture",{Int(2)});
        } else {
            Refused(fixture,"Plain","QueryRaw",{Int(1)});Refused(fixture,"Plain","GetMeshTexture",{Int(1)});
        }
        Refused(fixture,"Plain","WriteQueryThenFail");fixture.UnchangedSources();
    }
}
} // namespace
int main() {
    try {Positive();NegativeDeclarations();std::cout<<"PASS generated original GetMeshTexture1013: "<<checks<<" checks, "<<refusals
        <<" refusals; genuine decoded Mesh/LodMesh, raw slot priority, optional bounds, native declaring fields and complete rollback.\n";return 0;}
    catch(const std::exception& error) {std::cerr<<"FAIL generated GetMeshTexture1013: "<<error.what()<<'\n';return 1;}
}
