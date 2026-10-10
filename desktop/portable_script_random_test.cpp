#include "Precomp.h"
#include "GC/GC.h"
#include "portable_unreal_runtime.h"
#include "quest_script_random.h"
#include "quest_script_state.h"

#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

// Commercial-data-free UE1 packages and actual production VM transactions.
// The independent oracle deliberately uses uint64 arithmetic and an exact
// rational representation of appFrand's binary32 multiplier, not the helper.
namespace {
using Bytes=std::vector<std::uint8_t>;
using Value=QuestVr::Vm::Value;
using Kind=QuestVr::Vm::Kind;
using Evaluation=QuestVr::Vm::Evaluation;
namespace Random=QuestVr::ScriptRandom;
std::size_t checks{},refusals{};
void Require(bool condition,const std::string& description) {
    if(!condition) throw std::runtime_error(description);
    ++checks;
}
std::uint32_t OracleDraw(std::uint32_t& seed) {
    const auto wide=static_cast<std::uint64_t>(seed)*214013ull+2531011ull;
    seed=static_cast<std::uint32_t>(wide&0xffffffffull);
    return static_cast<std::uint32_t>((wide>>16u)&32767ull);
}
std::int32_t OracleRand(std::uint32_t& seed,std::int32_t maximum) {
    if(maximum<=0) return 0;
    return static_cast<std::int32_t>(OracleDraw(seed)%static_cast<std::uint32_t>(maximum));
}
float OracleFRand(std::uint32_t& seed) {
    const auto numerator=static_cast<std::uint64_t>(OracleDraw(seed))*32769ull;
    return static_cast<float>(std::ldexp(static_cast<double>(numerator),-30));
}
std::uint32_t EndpointSeed(std::uint32_t sample) {
    for(std::uint32_t candidate=0;candidate<1'000'000u;++candidate) {
        auto state=candidate;
        if(OracleDraw(state)==sample) return candidate;
    }
    throw std::runtime_error("Independent bounded oracle could not find FRand endpoint seed");
}
void PureContracts() {
    Require(Random::Algorithm==1u && Random::InitialSeed==1u,"Script RNG algorithm/default seed is not the explicit MSVCRT contract");
    std::uint32_t seed=1u;
    for(const auto sample:std::array<std::uint32_t,10>{41u,18467u,6334u,26500u,19169u,15724u,11478u,29358u,26962u,24464u})
        Require(Random::Draw15(seed)==sample,"MSVCRT published seed-1 sequence changed");
    for(const auto initial:std::array<std::uint32_t,7>{0u,1u,17u,0x7fffffffu,0x80000000u,0xfffffffeu,0xffffffffu}) {
        auto actual=initial,oracle=initial;
        for(std::size_t i=0;i<4096u;++i) {
            switch(i%4u) {
            case 0: Require(Random::Draw15(actual)==OracleDraw(oracle),"Draw15 disagrees with independent uint64 wrap oracle");break;
            case 1: {
                const auto maximum=std::array<std::int32_t,7>{1,2,7,32767,32768,1'000'000,std::numeric_limits<std::int32_t>::max()}[(i/4u)%7u];
                Require(Random::Rand(actual,maximum)==OracleRand(oracle,maximum),"Mixed Rand draw differs from modulo oracle");break;
            }
            case 2: Require(std::bit_cast<std::uint32_t>(Random::FRand(actual))==std::bit_cast<std::uint32_t>(OracleFRand(oracle)),
                "Mixed FRand differs from exact appFrand multiplier rounded to binary32");break;
            default: {
                const auto maximum=(i&4u) ? std::numeric_limits<std::int32_t>::min() : 0;
                Require(Random::Rand(actual,maximum)==0,"Nonpositive Rand result is not zero");break;
            }
            }
            Require(actual==oracle,"Mixed Rand/FRand consumed the wrong number of global stream draws");
        }
        for(const auto maximum:{std::numeric_limits<std::int32_t>::min(),-100,-1,0}) {
            const auto before=actual;
            Require(Random::Rand(actual,maximum)==0 && actual==before,"Nonpositive Rand advanced the stream");
        }
        const auto before=actual;
        Require(Random::Rand(actual,1)==0 && actual!=before,"Rand(1) failed to consume a draw");
    }
    for(const auto sample:{0u,32767u}) {
        const auto initial=EndpointSeed(sample);auto actual=initial,oracle=initial;
        const auto result=Random::FRand(actual);
        Require(std::bit_cast<std::uint32_t>(result)==(sample ? 0x3f800000u : 0u) && actual==static_cast<std::uint32_t>(
            static_cast<std::uint64_t>(initial)*214013ull+2531011ull),"FRand closed endpoint or seed transition changed");
        Require(std::bit_cast<std::uint32_t>(result)==std::bit_cast<std::uint32_t>(OracleFRand(oracle)),"Independent FRand endpoint oracle differs");
        actual=initial;Require(Random::Rand(actual,1'000'000)==static_cast<std::int32_t>(sample),"Large positive Rand bound was clamped or resampled");
    }
    std::cout<<"Pure MSVCRT stream, mixed draw-count and binary32 endpoint contracts passed.\n";
}
void U16(Bytes& bytes,std::uint16_t value) {bytes.push_back(static_cast<std::uint8_t>(value));bytes.push_back(static_cast<std::uint8_t>(value>>8u));}
void U32(Bytes& bytes,std::uint32_t value) {U16(bytes,static_cast<std::uint16_t>(value));U16(bytes,static_cast<std::uint16_t>(value>>16u));}
void U64(Bytes& bytes,std::uint64_t value) {U32(bytes,static_cast<std::uint32_t>(value));U32(bytes,static_cast<std::uint32_t>(value>>32u));}
void Index(Bytes& bytes,std::int32_t value) {
    auto rest=static_cast<std::uint32_t>(value<0 ? -static_cast<std::int64_t>(value) : value);
    auto first=static_cast<std::uint8_t>((rest&63u)|(value<0 ? 128u : 0u));rest>>=6u;
    if(rest) first|=64u;
    bytes.push_back(first);
    while(rest) {auto next=static_cast<std::uint8_t>(rest&127u);rest>>=7u;if(rest) next|=128u;bytes.push_back(next);}
}
void Replace32(Bytes& bytes,std::size_t offset,std::uint32_t value) {
    for(unsigned i=0;i<4u;++i) bytes.at(offset+i)=static_cast<std::uint8_t>(value>>(8u*i));
}
void Append(Bytes& bytes,const Bytes& other) {bytes.insert(bytes.end(),other.begin(),other.end());}
Bytes ReadBytes(const std::filesystem::path& path) {
    const auto size=std::filesystem::file_size(path);Require(size<=1u<<20u,"Generated RNG file exceeded independent read bound");
    Bytes bytes(static_cast<std::size_t>(size));std::ifstream file(path,std::ios::binary);
    Require(static_cast<bool>(file),"Cannot inspect generated RNG file");
    if(!bytes.empty()) file.read(reinterpret_cast<char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));
    Require(static_cast<bool>(file),"Generated RNG file was truncated");return bytes;
}
void WriteBytes(const std::filesystem::path& path,const Bytes& bytes) {
    std::ofstream file(path,std::ios::binary|std::ios::trunc);Require(static_cast<bool>(file),"Cannot create generated RNG file");
    file.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));
    Require(static_cast<bool>(file),"Cannot write generated RNG file");
}
struct Code {Bytes raw;std::size_t logical{};};
Code Token(std::uint8_t token) {return {{token},1u};}
Code Ref(std::uint8_t token,std::int32_t reference) {Code code{{token},5u};Index(code.raw,reference);return code;}
Code Int(std::int32_t value) {Code code{{0x1du},5u};U32(code.raw,static_cast<std::uint32_t>(value));return code;}
Code Float(float value) {Code code{{0x1eu},5u};U32(code.raw,std::bit_cast<std::uint32_t>(value));return code;}
Code Text(const std::string& text) {Code code{{0x1fu},text.size()+2u};code.raw.insert(code.raw.end(),text.begin(),text.end());code.raw.push_back(0u);return code;}
Code Join(std::initializer_list<Code> parts) {Code code;for(const auto& part:parts) {Append(code.raw,part.raw);code.logical+=part.logical;}return code;}
Code Return(Code value=Token(0x0bu)) {return Join({Token(0x04u),std::move(value)});}
Code Assign(Code left,Code right) {return Join({Token(0x0fu),std::move(left),std::move(right)});}
Code Native(std::uint16_t native,std::initializer_list<Code> arguments={}) {
    Code code;
    if(native>=256u) {code.raw={static_cast<std::uint8_t>(0x60u+(native>>8u)),static_cast<std::uint8_t>(native)};code.logical=2u;}
    else {code.raw={static_cast<std::uint8_t>(native)};code.logical=1u;}
    for(const auto& argument:arguments) {Append(code.raw,argument.raw);code.logical+=argument.logical;}
    code.raw.push_back(0x16u);++code.logical;return code;
}
Code Call(std::int32_t function,std::initializer_list<Code> arguments={}) {
    auto code=Ref(0x1cu,function);for(const auto& argument:arguments) {Append(code.raw,argument.raw);code.logical+=argument.logical;}
    code.raw.push_back(0x16u);++code.logical;return code;
}
Code Context(Code receiver,Code expression) {
    Require(expression.logical<=std::numeric_limits<std::uint16_t>::max(),"Generated RNG Context exceeds logical skip range");
    auto code=Join({Token(0x19u),std::move(receiver)});U16(code.raw,static_cast<std::uint16_t>(expression.logical));
    code.raw.push_back(0u);code.logical+=3u;Append(code.raw,expression.raw);code.logical+=expression.logical;return code;
}
struct Package {
    std::string stem;std::vector<std::string> names{"None"};std::vector<ImportTableEntry> imports;std::vector<ExportTableEntry> exports;
    std::vector<std::function<Bytes(std::int32_t,std::int32_t)>> builders;std::map<std::int32_t,std::vector<std::int32_t>> children;
    std::int32_t Name(const std::string& value) {
        const auto found=std::find(names.begin(),names.end(),value);if(found!=names.end()) return static_cast<std::int32_t>(found-names.begin());
        names.push_back(value);return static_cast<std::int32_t>(names.size()-1u);
    }
    std::int32_t Import(const std::string& name,std::int32_t outer,const std::string& type="Class") {
        imports.push_back({Name("Core"),Name(type),outer,Name(name)});return -static_cast<std::int32_t>(imports.size());
    }
    std::int32_t ImportPackage(const std::string& name) {return Import(name,0,"Package");}
    std::int32_t Export(const std::string& name,std::int32_t cls=0,std::int32_t base=0,std::int32_t outer=0,bool field=false) {
        exports.push_back({cls,base,outer,Name(name),ObjectFlags{},0,-1});builders.emplace_back();
        const auto reference=static_cast<std::int32_t>(exports.size());if(field) children[outer].push_back(reference);return reference;
    }
    void ClassBody(std::int32_t reference) {
        builders.at(static_cast<std::size_t>(reference-1))=[this,reference](std::int32_t next,std::int32_t child) {
            const auto& entry=exports.at(static_cast<std::size_t>(reference-1));Bytes body;
            Index(body,entry.ObjBase);Index(body,next);Index(body,0);Index(body,child);Index(body,entry.ObjName);
            U32(body,0);U32(body,0);U32(body,0);U64(body,~std::uint64_t{});U64(body,~std::uint64_t{});U16(body,0xffffu);U32(body,0);
            U32(body,0);body.resize(body.size()+16u,0u);for(unsigned i=0;i<4u;++i) Index(body,0);Index(body,0);return body;
        };
    }
    std::int32_t Property(const std::string& name,const std::string& type,std::int32_t owner,std::int32_t core,std::uint32_t flags=0u) {
        const auto reference=Export(name,Import(type,core),0,owner,true);
        builders.at(static_cast<std::size_t>(reference-1))=[type,flags](std::int32_t next,std::int32_t) {
            Bytes body{0u};Index(body,0);Index(body,next);U32(body,1u);U32(body,flags);Index(body,0);
            if(type=="ByteProperty" || type=="ObjectProperty") Index(body,0);
            return body;
        };return reference;
    }
    std::int32_t Function(const std::string& name,std::int32_t owner,std::int32_t core,Code code={},std::uint16_t native=0u) {
        const auto reference=Export(name,Import("Function",core),0,owner,true);FunctionCode(reference,std::move(code),native);return reference;
    }
    void FunctionCode(std::int32_t reference,Code code,std::uint16_t native=0u) {
        builders.at(static_cast<std::size_t>(reference-1))=[this,reference,native,code=std::move(code)](std::int32_t next,std::int32_t child) {
            Bytes body{0u};Index(body,0);Index(body,next);Index(body,0);Index(body,child);Index(body,exports.at(static_cast<std::size_t>(reference-1)).ObjName);
            U32(body,0);U32(body,0);U32(body,static_cast<std::uint32_t>(code.logical));Append(body,code.raw);U16(body,native);body.push_back(0u);U32(body,native ? 0x402u : 2u);return body;
        };
    }
    void ActorBody(std::int32_t reference) {
        const auto countName=Name("Count");
        builders.at(static_cast<std::size_t>(reference-1))=[countName](std::int32_t,std::int32_t) {
            Bytes body;Index(body,countName);body.push_back(0x22u);U32(body,0u);Index(body,0);return body;
        };
    }
    Bytes Serialize() {
        std::map<std::int32_t,std::int32_t> next;for(const auto& [owner,list]:children) {(void)owner;for(std::size_t i=1;i<list.size();++i) next[list[i-1u]]=list[i];}
        Bytes bytes;U32(bytes,0x9e2a83c1u);U16(bytes,68u);U16(bytes,0u);bytes.resize(56u,0u);
        std::vector<Bytes> bodies;std::vector<std::int32_t> offsets;
        for(std::size_t i=0;i<exports.size();++i) {
            const auto reference=static_cast<std::int32_t>(i+1u);const auto first=children.find(reference);
            auto body=builders[i] ? builders[i](next[reference],first==children.end() || first->second.empty() ? 0 : first->second.front()) : Bytes{};
            offsets.push_back(static_cast<std::int32_t>(bytes.size()));Append(bytes,body);bodies.push_back(std::move(body));
        }
        Replace32(bytes,12u,static_cast<std::uint32_t>(names.size()));Replace32(bytes,16u,static_cast<std::uint32_t>(bytes.size()));
        for(const auto& name:names) {Index(bytes,static_cast<std::int32_t>(name.size()+1u));bytes.insert(bytes.end(),name.begin(),name.end());bytes.push_back(0u);U32(bytes,0u);}
        Replace32(bytes,20u,static_cast<std::uint32_t>(exports.size()));Replace32(bytes,24u,static_cast<std::uint32_t>(bytes.size()));
        for(std::size_t i=0;i<exports.size();++i) {
            const auto& entry=exports[i];Index(bytes,entry.ObjClass);Index(bytes,entry.ObjBase);U32(bytes,static_cast<std::uint32_t>(entry.ObjOuter));Index(bytes,entry.ObjName);
            U32(bytes,static_cast<std::uint32_t>(entry.ObjFlags));Index(bytes,static_cast<std::int32_t>(bodies[i].size()));if(!bodies[i].empty()) Index(bytes,offsets[i]);
        }
        Replace32(bytes,28u,static_cast<std::uint32_t>(imports.size()));Replace32(bytes,32u,static_cast<std::uint32_t>(bytes.size()));
        for(const auto& entry:imports) {Index(bytes,entry.ClassPackage);Index(bytes,entry.ClassName);U32(bytes,static_cast<std::uint32_t>(entry.ObjOuter));Index(bytes,entry.ObjName);}
        return bytes;
    }
};
struct Fixture {
    std::filesystem::path parent,directory;std::map<std::filesystem::path,Bytes> sources;
    Fixture() {
        parent=std::filesystem::canonical(std::filesystem::temp_directory_path());
        const auto stamp=std::chrono::high_resolution_clock::now().time_since_epoch().count();
        for(unsigned attempt=0;attempt<20u;++attempt) {
            const auto candidate=parent/("deusex-script-random-test-"+std::to_string(stamp)+'-'+std::to_string(attempt));
            if(std::filesystem::create_directory(candidate)) {directory=std::filesystem::canonical(candidate);break;}
        }
        Require(!directory.empty() && directory.parent_path()==parent,"Generated RNG fixture escaped owned temporary parent");
        std::filesystem::create_directory(directory/"System");std::filesystem::create_directory(directory/"Maps");
    }
    ~Fixture() {
        ShutdownPortableRuntime();std::error_code error;const auto actual=std::filesystem::weakly_canonical(directory,error);
        if(!error && !directory.empty() && actual==directory && actual.parent_path()==parent && actual.filename().string().rfind("deusex-script-random-test-",0u)==0u)
            std::filesystem::remove_all(actual,error);
    }
    PortablePackageTables Write(Package& package,bool map=false) {
        const auto path=directory/(map ? "Maps" : "System")/(package.stem+(map ? ".dx" : ".u"));auto bytes=package.Serialize();WriteBytes(path,bytes);sources.emplace(path,std::move(bytes));
        const auto table=LoadPortablePackageTables(path.string());Require(table.version==68u && table.exports.size()==package.exports.size(),"Generated RNG package failed real loader");return table;
    }
    Bytes Snapshot(const std::string& name) const {
        const auto path=directory/(name+".sav");Require(SavePortableRuntimeState(path.string()),"Cannot snapshot generated RNG runtime");return ReadBytes(path);
    }
    void UnchangedSources() const {for(const auto& [path,bytes]:sources) Require(ReadBytes(path)==bytes,"Script RNG changed immutable source packages");}
};
std::uint32_t Read32(const Bytes& bytes,std::size_t& cursor) {
    Require(cursor<=bytes.size() && bytes.size()-cursor>=4u,"Independent RNG checkpoint inspection encountered truncation");
    std::uint32_t result{};for(unsigned i=0;i<4u;++i) result|=static_cast<std::uint32_t>(bytes[cursor++])<<(8u*i);return result;
}
std::size_t PrefixSize(const Bytes& bytes) {
    std::size_t cursor{};Require(Read32(bytes,cursor)==0x53515844u,"Independent RNG runtime envelope magic mismatch");
    const auto version=Read32(bytes,cursor);Require(version>=1u && version<=10u,"Independent RNG runtime envelope version mismatch");
    const auto skipString=[&]() {const auto size=Read32(bytes,cursor);Require(cursor<=bytes.size() && size<=bytes.size()-cursor,"Independent RNG checkpoint string exceeds input");cursor+=size;};
    const auto skipStrings=[&]() {const auto count=Read32(bytes,cursor);for(std::uint32_t i=0;i<count;++i) skipString();};
    skipStrings();skipStrings();skipStrings();
    if(version>=2u) {static_cast<void>(Read32(bytes,cursor));const auto count=Read32(bytes,cursor);for(std::uint32_t i=0;i<count;++i) {skipString();static_cast<void>(Read32(bytes,cursor));}}
    if(version>=3u) {static_cast<void>(Read32(bytes,cursor));static_cast<void>(Read32(bytes,cursor));for(unsigned i=0;i<4u;++i) skipStrings();}
    return cursor;
}
Bytes Blob(const Bytes& bytes) {
    auto cursor=PrefixSize(bytes);const auto size=Read32(bytes,cursor);Require(size==bytes.size()-cursor,"Independent RNG script trailer size mismatch");
    return Bytes(bytes.begin()+static_cast<std::ptrdiff_t>(cursor),bytes.end());
}
QuestVr::ScriptSavedState Decode(const Bytes& bytes) {return QuestVr::DecodeScriptSavedState(Blob(bytes));}
Bytes EnvelopeBlob(const Bytes& prefixSource,const Bytes& blob,std::uint32_t version=10u) {
    const auto prefix=PrefixSize(prefixSource);Bytes bytes(prefixSource.begin(),prefixSource.begin()+static_cast<std::ptrdiff_t>(prefix));Replace32(bytes,4u,version);
    U32(bytes,static_cast<std::uint32_t>(blob.size()));Append(bytes,blob);return bytes;
}
Bytes Envelope(const Bytes& prefixSource,const QuestVr::ScriptSavedState& state,std::uint32_t version=10u) {
    return EnvelopeBlob(prefixSource,QuestVr::EncodeScriptSavedState(state),version);
}
struct Tables {PortablePackageTables core,engine,classes,map,otherMap;};
Tables Build(Fixture& fixture) {
    Package core;core.stem="Core";const auto corePackage=core.ImportPackage("Core"),object=core.Export("Object");
    const auto rand=core.Function("Rand",object,corePackage,{},167u);
    core.Property("Max","IntProperty",rand,corePackage,0x80u);core.Property("ReturnValue","IntProperty",rand,corePackage,0x480u);
    const auto frand=core.Function("FRand",object,corePackage,{},195u);core.Property("ReturnValue","FloatProperty",frand,corePackage,0x480u);
    core.ClassBody(object);const auto coreTable=fixture.Write(core);
    Package engine;engine.stem="Engine";const auto engineCore=engine.ImportPackage("Core"),importedObject=engine.Import("Object",engineCore);
    const auto actor=engine.Export("Actor",0,importedObject);engine.ClassBody(actor);const auto engineTable=fixture.Write(engine);
    Package classes;classes.stem="RandomClasses";const auto classesCore=classes.ImportPackage("Core"),classesEngine=classes.ImportPackage("Engine");
    const auto probe=classes.Export("Probe",0,classes.Import("Actor",classesEngine));
    const auto count=classes.Property("Count","IntProperty",probe,classesCore);classes.ClassBody(probe);
    const auto drawFloat=classes.Function("DrawFloat",probe,classesCore,Return(Native(195u)));
    const auto classesObject=classes.Import("Object",classesCore),importedFRand=classes.Import("FRand",classesObject,"Function");
    for(const auto& [name,expression]:std::vector<std::pair<std::string,Code>>{
        {"DrawOtherRaw",Native(195u)},{"DrawOtherDeclaration",Call(importedFRand)},{"DrawOtherNested",Call(drawFloat)}}) {
        const auto function=classes.Function(name,probe,classesCore),target=classes.Property("Target","ObjectProperty",function,classesCore,0x80u);
        classes.FunctionCode(function,Return(Context(Ref(0x00u,target),expression)));
    }
    const auto draw=classes.Function("DrawInt",probe,classesCore),maximum=classes.Property("Maximum","IntProperty",draw,classesCore,0x80u);
    classes.FunctionCode(draw,Return(Native(167u,{Ref(0x00u,maximum)})));
    const auto leaf=classes.Function("LeafDraw",probe,classesCore,Join({Native(195u),Native(167u,{Int(71)}),Return()}));
    classes.Function("NestedThenFail",probe,classesCore,Join({Call(leaf),Native(4095u),Return()}));
    classes.Function("DrawThenWrite",probe,classesCore,Join({Native(195u),Assign(Ref(0x01u,count),Int(99)),Return()}));
    classes.Function("TwoDraws",probe,classesCore,Join({Native(195u),Return(Native(195u))}));
    classes.Function("WriteCount",probe,classesCore,Join({Assign(Ref(0x01u,count),Int(77)),Return()}));
    const auto outLeaf=classes.Function("OutDrawLeaf",probe,classesCore),out=classes.Property("Output","IntProperty",outLeaf,classesCore,0x180u);
    classes.FunctionCode(outLeaf,Join({Native(195u),Assign(Ref(0x00u,out),Int(88)),Return()}));
    classes.Function("NestedOutThenFail",probe,classesCore,Join({Call(outLeaf,{Ref(0x01u,count)}),Native(4095u),Return()}));
    classes.Function("RandMissing",probe,classesCore,Return(Native(167u)));
    classes.Function("RandExcess",probe,classesCore,Return(Native(167u,{Int(10),Int(20)})));
    classes.Function("FRandExcess",probe,classesCore,Return(Native(195u,{Int(10)})));
    classes.Function("RandBool",probe,classesCore,Return(Native(167u,{Token(0x27u)})));
    classes.Function("RandName",probe,classesCore,Return(Native(167u,{Ref(0x21u,classes.Name("BadBound"))})));
    classes.Function("RandString",probe,classesCore,Return(Native(167u,{Text("12")})));
    classes.Function("RandObject",probe,classesCore,Return(Native(167u,{Token(0x17u)})));
    classes.Function("RandNonFinite",probe,classesCore,Return(Native(167u,{Float(std::numeric_limits<float>::infinity())})));
    classes.Function("RandOutsideInt",probe,classesCore,Return(Native(167u,{Float(2147483648.0f)})));
    classes.Function("RandFraction",probe,classesCore,Return(Native(167u,{Float(7.9f)})));
    const auto classesTable=fixture.Write(classes);
    const auto makeMap=[&](const std::string& name) {
        Package map;map.stem=name;const auto importedClasses=map.ImportPackage("RandomClasses"),importedProbe=map.Import("Probe",importedClasses);
        map.ActorBody(map.Export("Probe0",importedProbe));map.ActorBody(map.Export("Probe1",importedProbe));return fixture.Write(map,true);
    };
    return {coreTable,engineTable,classesTable,makeMap("RandomFixture"),makeMap("OtherRandomFixture")};
}
std::string Actor(const std::string& name="Probe0",const std::string& map="RandomFixture") {return map+'.'+name;}
QuestVr::Vm::Result Call(const std::string& actor,const std::string& function,const std::vector<Evaluation>& arguments={}) {
    auto result=ExecutePortableActorFunction(actor,function,arguments);
    Require(result.passed() && result.committed,function+" failed: "+result.error+" at "+result.function+':'+std::to_string(result.offset));return result;
}
std::int32_t DrawInt(const std::string& actor,std::int32_t maximum,const std::string& function="DrawInt") {
    const auto result=Call(actor,function,{{Value::Integer(maximum),{}}});Require(result.value.kind==Kind::Int,"Rand did not return an Int");return result.value.integer;
}
float DrawFloat(const std::string& actor,const std::string& function="DrawFloat") {
    const auto result=Call(actor,function);Require(result.value.kind==Kind::Float,"FRand did not return a Float");return result.value.floating;
}
void Seed(const Fixture& fixture,const Bytes& baseline,std::uint32_t seed,const std::string& map="RandomFixture") {
    QuestVr::ScriptSavedState state;state.mapName=map;state.randomSeed=seed;
    const auto path=fixture.directory/"Seed.sav";WriteBytes(path,Envelope(baseline,state));
    Require(ValidatePortableRuntimeState(path.string(),map) && LoadPortableRuntimeState(path.string()),"Cannot seed fixture via genuine versioned checkpoint");
}
void RefusedCall(const Fixture& fixture,const std::string& function,const QuestVr::Vm::Limits& limits={}) {
    const auto before=fixture.Snapshot("BeforeCallRefusal");const auto revision=GetPortableRuntimeWorldRevision();
    const auto result=ExecutePortableActorFunction(Actor(),function,{},limits);
    Require(!result.passed() && !result.committed && !result.error.empty(),"Invalid generated RNG call silently committed: "+function);
    Require(fixture.Snapshot("AfterCallRefusal")==before && GetPortableRuntimeWorldRevision()==revision,
        "Failed RNG callback changed stream, actor state or world revision: "+function);++refusals;
    if(function=="NestedOutThenFail") Require(result.writes==2u,"Nested random failure did not reach actual local OUT write and caller copy-back");
    if(function=="DrawThenWrite") Require(result.status==QuestVr::Vm::Status::Budget && result.writes==0u,"Draw-then-write fixture did not fail at its configured write budget");
    if(function=="TwoDraws") Require(result.status==QuestVr::Vm::Status::Budget && result.instructions==limits.instructions,"Draw-then-instruction-budget fixture failed for a different reason");
}
void RefusedCheckpoint(const Fixture& fixture,const Bytes& bytes,const std::string& name) {
    const auto before=fixture.Snapshot("BeforeBadLoad");const auto revision=GetPortableRuntimeWorldRevision();const auto path=fixture.directory/(name+".sav");WriteBytes(path,bytes);
    Require(!ValidatePortableRuntimeState(path.string(),"RandomFixture"),"Invalid RNG checkpoint validated: "+name);
    Require(fixture.Snapshot("AfterBadValidation")==before && GetPortableRuntimeWorldRevision()==revision,"Rejected RNG validation mutated stream/state: "+name);
    Require(!LoadPortableRuntimeState(path.string()),"Invalid RNG checkpoint loaded: "+name);
    Require(fixture.Snapshot("AfterBadLoad")==before && GetPortableRuntimeWorldRevision()==revision,"Rejected RNG load partially changed stream/state: "+name);++refusals;
}
void RuntimeContracts() {
    Fixture fixture;const auto tables=Build(fixture);
    const auto initialize=[&](std::uint32_t initialSeed=1u) {
        const auto initialized=InitializePortableRuntime({tables.core,tables.engine,tables.classes},initialSeed);
        Require(initialized.passed && initialized.functions>=15u,"Generated RNG metadata initialization failed");
        const auto loaded=LoadPortableRuntimeMap(tables.map);
        Require(loaded.passed,"Generated RNG actors failed real map loader: exports="+std::to_string(loaded.exports)+
            " actors="+std::to_string(loaded.actors)+" properties="+std::to_string(loaded.actorProperties)+
            " resolved="+std::to_string(loaded.resolvedClasses)+" unresolved="+std::to_string(loaded.unresolvedClasses));
    };
    const auto explicitSeed=0x13579bdfu;
    initialize(explicitSeed);
    Require(GetPortableRuntimeScriptStatePresent() && !GetPortableRuntimeUnsavedScriptState(),"Explicit nondefault initial seed was not immediately retained as global-only state");
    const auto preDrawRevision=GetPortableRuntimeWorldRevision();
    const auto preDraw=fixture.Snapshot("BeforeFirstDraw");const auto preDrawState=Decode(preDraw);
    Require(preDraw[4u]==10u && Blob(preDraw).at(6u)==7u && preDrawState.randomSeed==explicitSeed && preDrawState.objects.empty() &&
        GetPortableRuntimeWorldRevision()==preDrawRevision,"Saving before first draw lost explicit initial seed or created actor state");
    auto firstOracle=explicitSeed;const auto firstFloat=OracleFRand(firstOracle);const auto firstInt=OracleRand(firstOracle,12345);
    const auto firstOutputs=[&]() {
        Require(std::bit_cast<std::uint32_t>(DrawFloat(Actor()))==std::bit_cast<std::uint32_t>(firstFloat) && DrawInt(Actor("Probe1"),12345)==firstInt,
            "Pre-first-draw save did not restore the explicit seed's first mixed outputs");
    };
    firstOutputs();Require(LoadPortableRuntimeState((fixture.directory/"BeforeFirstDraw.sav").string()),"Warm pre-first-draw restoration failed");firstOutputs();
    ShutdownPortableRuntime();initialize(0x2468ace0u);
    Require(Decode(fixture.Snapshot("OtherInitialSeed")).randomSeed==0x2468ace0u,"Cold explicit initial seed was not retained before draw");
    Require(LoadPortableRuntimeState((fixture.directory/"BeforeFirstDraw.sav").string()),"Cold pre-first-draw restoration failed");firstOutputs();
    ShutdownPortableRuntime();
    initialize();Require(!GetPortableRuntimeScriptStatePresent() && !GetPortableRuntimeUnsavedScriptState(),"Read-only RNG metadata initialization fabricated state");
    const auto baseline=fixture.Snapshot("Legacy");Require(baseline.size()>8u && baseline[4u]==3u,"Pristine RNG fixture should retain legacy version-3 envelope");
    // Inactive is a legacy gameplay-prefix flag, not Destroy/bDeleteMe. Core
    // randomness remains available to genuine Context/nested calls on it.
    auto inactivePrefix=baseline;std::size_t prefixCursor=8u;
    Require(Read32(inactivePrefix,prefixCursor)==0u && Read32(inactivePrefix,prefixCursor)==0u,"Generated inactive prefix expected empty inventory and inactive lists");
    const auto inactiveActor=Actor("Probe1");Bytes inactiveEntry;U32(inactiveEntry,static_cast<std::uint32_t>(inactiveActor.size()));
    inactiveEntry.insert(inactiveEntry.end(),inactiveActor.begin(),inactiveActor.end());Replace32(inactivePrefix,12u,1u);
    inactivePrefix.insert(inactivePrefix.begin()+16u,inactiveEntry.begin(),inactiveEntry.end());
    Seed(fixture,inactivePrefix,explicitSeed);auto inactiveOracle=explicitSeed;
    const auto inactiveRevision=GetPortableRuntimeWorldRevision();
    for(const auto& function:{"DrawOtherRaw","DrawOtherDeclaration","DrawOtherNested"}) {
        const auto result=Call(Actor(),function,{{Value::Text(Kind::Object,inactiveActor),{}}});
        Require(result.value.kind==Kind::Float && std::bit_cast<std::uint32_t>(result.value.floating)==std::bit_cast<std::uint32_t>(OracleFRand(inactiveOracle)),
            "Inactive Context receiver incorrectly blocked Core FRand or consumed a different nested stream");
        Require(GetPortableRuntimeWorldRevision()==inactiveRevision,"Inactive Core random call changed actor renderer revision");
    }
    Require(Decode(fixture.Snapshot("InactiveCoreRandom")).randomSeed==inactiveOracle,"Inactive nested Core random continuation was not committed");
    const auto inactiveDirect=ExecutePortableActorFunction(inactiveActor,"FRand");
    Require(!inactiveDirect.passed() && !inactiveDirect.committed,"Core Context exemption bypassed explicit inactive-actor invocation guard");++refusals;
    Require(LoadPortableRuntimeState((fixture.directory/"Legacy.sav").string()),"Cannot reset inactive Core receiver controls");
    const auto initialRevision=GetPortableRuntimeWorldRevision();
    for(const auto maximum:{std::numeric_limits<std::int32_t>::min(),-17,-1,0})
        Require(DrawInt(Actor(),maximum)==0,"Runtime Rand nonpositive result was not zero");
    Require(!GetPortableRuntimeScriptStatePresent() && fixture.Snapshot("AfterNonpositive")==baseline && GetPortableRuntimeWorldRevision()==initialRevision,
        "Rand nonpositive bound created/advanced a seed or touched world state");
    for(const auto& function:{"RandMissing","RandExcess","FRandExcess","RandBool","RandName","RandString","RandObject","RandNonFinite","RandOutsideInt"})
        RefusedCall(fixture,function);
    Require(!GetPortableRuntimeScriptStatePresent(),"Rejected RNG arguments created a stream before validation");
    auto oracle=Random::InitialSeed;
    Require(DrawInt(Actor(),1)==OracleRand(oracle,1),"Runtime Rand(1) result differs");
    Require(GetPortableRuntimeScriptStatePresent() && !GetPortableRuntimeUnsavedScriptState() && GetPortableRuntimeWorldRevision()==initialRevision,
        "RNG-only commit was invisible to saving, guarded maps, or invalidated actor renderer revision");
    auto current=fixture.Snapshot("RandOne");auto saved=Decode(current);
    Require(current[4u]==10u && Blob(current).at(6u)==7u && saved.randomSeed==oracle && saved.objects.empty() && saved.classDefaults.empty() && saved.births.empty() && saved.aiManagers.empty(),
        "RNG-only commit omitted env10/codec7 seed or fabricated map actor state");
    for(std::size_t i=0;i<64u;++i) {
        const auto actor=Actor(i&1u ? "Probe1" : "Probe0");
        if(i%3u==0u) Require(std::bit_cast<std::uint32_t>(DrawFloat(actor,i&1u ? "FRand" : "DrawFloat"))==std::bit_cast<std::uint32_t>(OracleFRand(oracle)),
            "Actors/declaration/raw native do not share one FRand stream");
        else {
            const auto maximum=std::array<std::int32_t,5>{1,7,32768,1'000'000,std::numeric_limits<std::int32_t>::max()}[i%5u];
            Require(DrawInt(actor,maximum,i&1u ? "Rand" : "DrawInt")==OracleRand(oracle,maximum),"Actors/declaration/raw native do not share one Rand stream");
        }
        Require(GetPortableRuntimeWorldRevision()==initialRevision && !GetPortableRuntimeUnsavedScriptState(),"Shared RNG-only commit touched map renderer state");
    }
    const auto fraction=Call(Actor(),"RandFraction");Require(fraction.value.kind==Kind::Int && fraction.value.integer==OracleRand(oracle,7),"Rand Float argument failed pinned ToInt truncation");
    const auto beforeRefusals=fixture.Snapshot("MixedStream");
    for(const auto& function:{"NestedThenFail","NestedOutThenFail","RandMissing","RandExcess","FRandExcess","RandBool","RandName","RandString","RandObject","RandNonFinite","RandOutsideInt"})
        RefusedCall(fixture,function);
    QuestVr::Vm::Limits writes;writes.writes=0u;RefusedCall(fixture,"DrawThenWrite",writes);
    QuestVr::Vm::Limits instructions;instructions.instructions=3u;RefusedCall(fixture,"TwoDraws",instructions);
    Require(QuestVr::Vm::ToInt(ReadPortableActorScriptProperty(Actor(),"Count"))==0,"Failed random/nested OUT/write-budget path retained actor writes");
    Require(fixture.Snapshot("AfterAllRefusals")==beforeRefusals && Decode(beforeRefusals).randomSeed==oracle,"Failures changed exact saved global RNG continuation");
    Require(std::bit_cast<std::uint32_t>(DrawFloat(Actor()))==std::bit_cast<std::uint32_t>(OracleFRand(oracle)),"Failed RNG transaction consumed draws before the next successful callback");
    for(const auto sample:{0u,32767u}) {
        const auto seed=EndpointSeed(sample);Seed(fixture,baseline,seed);auto endpointOracle=seed;
        const auto actual=DrawFloat(Actor());Require(std::bit_cast<std::uint32_t>(actual)==(sample ? 0x3f800000u : 0u) &&
            std::bit_cast<std::uint32_t>(actual)==std::bit_cast<std::uint32_t>(OracleFRand(endpointOracle)),"Production FRand endpoint differed after real checkpoint seeding");
        Require(Decode(fixture.Snapshot("Endpoint")).randomSeed==endpointOracle,"Production endpoint draw did not persist the resulting seed");
    }
    for(const auto seed:{0u,0xffffffffu}) {
        Seed(fixture,baseline,seed);Require(Decode(fixture.Snapshot("ExplicitSeed")).randomSeed==seed,"Checkpoint zero/all-ones seed was treated as absent or altered");
        oracle=seed;Require(DrawInt(Actor(),std::numeric_limits<std::int32_t>::max())==OracleRand(oracle,std::numeric_limits<std::int32_t>::max()),"Production Rand overflow wrap/large bound changed");
    }
    current=fixture.Snapshot("Continuation");saved=Decode(current);const auto blob=Blob(current);
    Require(saved.randomSeed==oracle && blob.at(blob.size()-5u)==1u,"Codec7 lacks the explicit algorithm identifier before its seed tail");
    std::size_t tail=blob.size()-4u;Require(Read32(blob,tail)==oracle,"Codec7 seed is not fixed little-endian uint32 at its tail");
    Require(QuestVr::EncodeScriptSavedState(saved)==blob,"Codec7 RNG continuation did not round-trip canonically");
    const auto validationRevision=GetPortableRuntimeWorldRevision();
    Require(ValidatePortableRuntimeState((fixture.directory/"Continuation.sav").string(),"RandomFixture"),"Production RNG checkpoint failed read-only validation");
    Require(!ValidatePortableRuntimeState((fixture.directory/"Continuation.sav").string(),"DifferentMap"),"RNG checkpoint validation ignored metadata map binding");
    Require(fixture.Snapshot("AfterValidation")==current && GetPortableRuntimeWorldRevision()==validationRevision,"Read-only RNG validation changed stream or revision");
    const auto expectedNext=OracleFRand(oracle);
    Require(std::bit_cast<std::uint32_t>(DrawFloat(Actor()))==std::bit_cast<std::uint32_t>(expectedNext),"Saved RNG next sample differs");
    const auto expectedAfter=OracleRand(oracle,12345);
    Require(DrawInt(Actor("Probe1"),12345)==expectedAfter,"Saved RNG second sample differs");
    Require(LoadPortableRuntimeState((fixture.directory/"Continuation.sav").string()),"Warm RNG restoration failed");
    Require(std::bit_cast<std::uint32_t>(DrawFloat(Actor("Probe1")))==std::bit_cast<std::uint32_t>(expectedNext) && DrawInt(Actor(),12345)==expectedAfter,
        "Warm load did not restore shared mixed-type continuation exactly");
    ShutdownPortableRuntime();initialize(explicitSeed);
    Require(GetPortableRuntimeScriptStatePresent() && !GetPortableRuntimeUnsavedScriptState() && Decode(fixture.Snapshot("ColdExplicitSeed")).randomSeed==explicitSeed,
        "Cold runtime failed to retain its own explicit seed independently of prior draws");
    Require(LoadPortableRuntimeState((fixture.directory/"Continuation.sav").string()),"Cold RNG restoration failed");
    Require(std::bit_cast<std::uint32_t>(DrawFloat(Actor()))==std::bit_cast<std::uint32_t>(expectedNext) && DrawInt(Actor("Probe1"),12345)==expectedAfter,
        "Cold Initialize+map+Load did not restore next two RNG values");
    Require(LoadPortableRuntimeState((fixture.directory/"Continuation.sav").string()),"Cannot restore RNG checkpoint for malformed-load controls");
    for(const auto algorithm:{0u,2u,255u}) {auto broken=blob;broken[broken.size()-5u]=static_cast<std::uint8_t>(algorithm);RefusedCheckpoint(fixture,EnvelopeBlob(current,broken),"Algorithm"+std::to_string(algorithm));}
    for(std::size_t missing=1u;missing<=5u;++missing) {
        auto broken=blob;Require(broken.size()>=missing,"Generated RNG tail truncation exceeds its bounded input");
        for(std::size_t i=0;i<missing;++i) broken.pop_back();
        RefusedCheckpoint(fixture,EnvelopeBlob(current,broken),"TruncatedTail"+std::to_string(missing));
    }
    {auto broken=blob;broken.push_back(0u);RefusedCheckpoint(fixture,EnvelopeBlob(current,broken),"ExtraTail");}
    RefusedCheckpoint(fixture,EnvelopeBlob(current,blob,9u),"NewCodecOldEnvelope");
    QuestVr::ScriptSavedState old;old.mapName="RandomFixture";RefusedCheckpoint(fixture,Envelope(current,old,10u),"NewEnvelopeMissingRng");
    auto badState=saved;badState.objects.push_back({Actor(),"RandomClasses.Probe",{{"RandomClasses.Probe.Count","Count",0u,Value::Text(Kind::Name,"BadInt")}},std::nullopt});
    badState.randomSeed=0u;RefusedCheckpoint(fixture,Envelope(current,badState),"InvalidActorAfterNewSeed");
    auto wrongMap=saved;wrongMap.mapName="OtherRandomFixture";RefusedCheckpoint(fixture,Envelope(current,wrongMap),"OtherMap");
    Require(std::bit_cast<std::uint32_t>(DrawFloat(Actor()))==std::bit_cast<std::uint32_t>(expectedNext),"Failed checkpoint parsing/schema application altered next RNG sample");
    Require(LoadPortableRuntimeState((fixture.directory/"Legacy.sav").string()),"Legacy envelope reset failed");
    Require(!GetPortableRuntimeScriptStatePresent() && !GetPortableRuntimeUnsavedScriptState() && fixture.Snapshot("LegacyReset")==baseline,
        "Legacy load failed to remove global RNG presence and restore legacy envelope");
    oracle=Random::InitialSeed;Require(std::bit_cast<std::uint32_t>(DrawFloat(Actor()))==std::bit_cast<std::uint32_t>(OracleFRand(oracle)),"Legacy reset did not restart seed1 stream");
    Require(LoadPortableRuntimeMap(tables.otherMap).passed,"RNG-only state incorrectly blocked map replacement");
    Require(!GetPortableRuntimeUnsavedScriptState() && GetPortableRuntimeScriptStatePresent(),"Map replacement erased global RNG presence or fabricated map overlays");
    Require(DrawInt(Actor("Probe1","OtherRandomFixture"),123)==OracleRand(oracle,123),"Global random stream reset on map replacement");
    const auto otherState=Decode(fixture.Snapshot("OtherMapRng"));Require(otherState.mapName=="OtherRandomFixture" && otherState.randomSeed==oracle && otherState.objects.empty(),
        "RNG-only map replacement retained old map identity/actor state or lost global seed");
    Require(LoadPortableRuntimeMap(tables.map).passed,"Cannot replace RNG-only map back to initial fixture");
    Call(Actor(),"WriteCount");Require(GetPortableRuntimeUnsavedScriptState(),"Genuine actor overlay failed to activate map archive guard");
    const auto overlay=fixture.Snapshot("ActorOverlay");const auto overlayRevision=GetPortableRuntimeWorldRevision();
    Require(!LoadPortableRuntimeMap(tables.otherMap).passed && fixture.Snapshot("AfterGuard")==overlay && GetPortableRuntimeWorldRevision()==overlayRevision,
        "RNG global-map exemption bypassed the existing actor-overlay map guard");++refusals;
    Require(Decode(overlay).randomSeed==oracle && Decode(overlay).objects.size()==1u,"Actor-overlay save dropped shared RNG state or actor write");
    Require(LoadPortableRuntimeState((fixture.directory/"Legacy.sav").string()),"Final generated RNG state cleanup failed");
    fixture.UnchangedSources();
    std::cout<<"Generated production RNG transactions, mixed receivers, codec7/envelope10, cold restore and map-lifetime contracts passed.\n";
}
}
int main() {
    try {
        const auto gcBefore=GC::GetStats().numObjects;
        PureContracts();RuntimeContracts();GC::Collect();Require(GC::GetStats().numObjects==gcBefore,"Generated script RNG fixture leaked rooted runtime objects");
        std::cout<<"Script random: "<<checks<<" checks and "<<refusals<<" rejection controls passed.\n";return 0;
    }
    catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
