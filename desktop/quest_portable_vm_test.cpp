#include "Precomp.h"
#include "surreal_portable_package_tables.h"
#include "quest_portable_vm.h"

#include <algorithm>
#include <chrono>
#include <bit>
#include <cctype>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>
#include <tuple>

namespace {
using Bytes = std::vector<std::uint8_t>;
std::size_t checks{}, rejections{};
void Require(bool condition,const std::string& message) {
    if(!condition) throw std::runtime_error(message);
    ++checks;
}
void U16(Bytes& bytes,std::uint16_t value) {
    bytes.push_back(static_cast<std::uint8_t>(value));bytes.push_back(static_cast<std::uint8_t>(value>>8u));
}
void U32(Bytes& bytes,std::uint32_t value) {
    for(unsigned i=0;i<4;++i) bytes.push_back(static_cast<std::uint8_t>(value>>(i*8u)));
}
void Float(Bytes& bytes,float value) {
    std::uint32_t bits{};std::memcpy(&bits,&value,4);U32(bytes,bits);
}
void Index(Bytes& bytes,std::int32_t value) {
    auto magnitude=static_cast<std::uint32_t>(value<0 ? -static_cast<std::int64_t>(value) : value);
    auto first=static_cast<std::uint8_t>((magnitude&0x3fu)|(value<0 ? 0x80u : 0u));magnitude>>=6u;
    if(magnitude) first|=0x40u;
    bytes.push_back(first);
    while(magnitude) {
        auto byte=static_cast<std::uint8_t>(magnitude&0x7fu);magnitude>>=7u;
        if(magnitude) byte|=0x80u;
        bytes.push_back(byte);
    }
}
struct FunctionFixture {
    PortablePackageTables package;
    std::filesystem::path directory;
    std::filesystem::path path;
    FunctionFixture() {
        directory=std::filesystem::temp_directory_path()/
            ("deusex-portable-vm-test-"+std::to_string(std::chrono::high_resolution_clock::now().time_since_epoch().count()));
        Require(std::filesystem::create_directory(directory),"Could not create isolated generated VM fixture directory");
        path=directory/"FunctionPayload.bin";
        package.sourcePath=path.string();package.version=68;
        package.names={{NameString("None"),0},{NameString("Function"),0},{NameString("Fixture"),0},
            {NameString("TestName"),0},{NameString("TestCall"),0}};
        package.imports={{0,0,0,1}};
        package.exports={{-1,0,0,2,static_cast<ObjectFlags>(0),0,37}};
    }
    ~FunctionFixture() {std::error_code ignored;std::filesystem::remove_all(directory,ignored);}
    void Save(const Bytes& raw,std::uint32_t logicalSize,std::uint16_t nativeIndex=0,std::uint32_t functionFlags=0) {
        Bytes payload;Index(payload,0); // Tagged properties terminated by None.
        for(unsigned i=0;i<4;++i) Index(payload,0); // UField base/next, UStruct text/children.
        Index(payload,2);U32(payload,0);U32(payload,0);U32(payload,logicalSize);
        payload.insert(payload.end(),raw.begin(),raw.end());
        U16(payload,nativeIndex);payload.push_back(0);U32(payload,functionFlags);
        if((functionFlags&0x40u)!=0) U16(payload,0);
        std::ofstream file(path,std::ios::binary|std::ios::trunc);
        Require(static_cast<bool>(file),"Could not create generated VM fixture");
        const std::array<char,37> prefix{};file.write(prefix.data(),static_cast<std::streamsize>(prefix.size()));
        file.write(reinterpret_cast<const char*>(payload.data()),static_cast<std::streamsize>(payload.size()));
        Require(static_cast<bool>(file),"Could not write generated VM fixture");
        package.exports[0].ObjSize=static_cast<std::int32_t>(payload.size());
    }
    PortableScriptBody Decode(const Bytes& raw,const Bytes& normalized) {
        Save(raw,static_cast<std::uint32_t>(normalized.size()));
        const auto script=LoadPortableFunctionScript(package,0);
        Require(script.rawBytes==raw,"Script decoder modified serialized bytecode evidence");
        Require(script.bytecode==normalized && script.logicalSize==normalized.size(),
            "Script native normalization altered logical offsets or fixed32 references");
        return script;
    }
};
void NormalizedSerializedContracts() {
    FunctionFixture fixture;
    // Compact serial object/name references become fixed int32 slots; branch
    // addresses were authored in this logical representation and stay intact.
    Bytes raw{0x07};U16(raw,13);raw.push_back(0x28);
    raw.push_back(0x04);raw.push_back(0x21);Index(raw,3);
    raw.push_back(0x04);raw.push_back(0x20);Index(raw,-1);
    Bytes normalized{0x07};U16(normalized,10);normalized.push_back(0x28);
    normalized.push_back(0x04);normalized.push_back(0x21);U32(normalized,3);
    normalized.push_back(0x04);normalized.push_back(0x20);U32(normalized,static_cast<std::uint32_t>(-1));
    // The two streams carry identical logical offsets, not serialized offsets.
    raw[1]=10;raw[2]=0;
    fixture.Decode(raw,normalized);
    for(const std::array<std::uint8_t,2> native : {std::array<std::uint8_t,2>{0x60,0x03},
        std::array<std::uint8_t,2>{0x60,0x70},std::array<std::uint8_t,2>{0x61,0x2f}}) {
        const Bytes expression{0x04,native[0],native[1],0x26,0x16};
        fixture.Decode(expression,expression);
    }
    raw={0x04,0x1b};Index(raw,4);raw.push_back(0x0b);raw.push_back(0x26);raw.push_back(0x16);
    normalized={0x04,0x1b};U32(normalized,4);normalized.push_back(0x0b);normalized.push_back(0x26);normalized.push_back(0x16);
    fixture.Decode(raw,normalized);
    raw={0x04,0x1c};Index(raw,1);raw.push_back(0x16);
    normalized={0x04,0x1c};U32(normalized,1);normalized.push_back(0x16);
    fixture.Decode(raw,normalized);
    for (const std::uint8_t opcode : {std::uint8_t{0x13},std::uint8_t{0x2e}}) {
        raw={0x04,opcode};Index(raw,-1);raw.push_back(0x20);Index(raw,1);
        normalized={0x04,opcode};U32(normalized,static_cast<std::uint32_t>(-1));
        normalized.push_back(0x20);U32(normalized,1);
        fixture.Decode(raw,normalized);
    }
    raw={0x0f,0x00};Index(raw,1);raw.push_back(0x1e);Float(raw,1.25f);raw.push_back(0x04);raw.push_back(0x0b);
    normalized={0x0f,0x00};U32(normalized,1);normalized.push_back(0x1e);Float(normalized,1.25f);normalized.push_back(0x04);normalized.push_back(0x0b);
    fixture.Decode(raw,normalized);
    const auto reject=[&](const Bytes& broken,std::uint32_t logical,const std::string& context) {
        fixture.Save(broken,logical);
        bool rejected{};try {LoadPortableFunctionScript(fixture.package,0);}catch(const std::runtime_error&) {rejected=true;}
        Require(rejected,context);++rejections;
    };
    reject({0x04,0x1e,0,0},6,"Truncated raw constant accepted");
    reject({0x04,0x26},1,"Declared logical function size shorter than expression accepted");
    reject({0x04,0x26},3,"Declared logical function size longer than expression accepted");
    reject({0x04,0x03},2,"Undefined raw token accepted");
    Bytes nested(70,0x2d);nested.push_back(0x28);
    reject(nested,static_cast<std::uint32_t>(nested.size()),"Unbounded raw expression nesting accepted");
}

namespace Vm=QuestVr::Vm;
using Vm::Value;
Bytes Join(std::initializer_list<Bytes> parts) {
    Bytes bytes;for(const auto& part:parts) bytes.insert(bytes.end(),part.begin(),part.end());return bytes;
}
Bytes Ref(std::uint8_t opcode,std::int32_t index) {Bytes b{opcode};U32(b,static_cast<std::uint32_t>(index));return b;}
Bytes Int(std::int32_t value) {Bytes b{0x1d};U32(b,static_cast<std::uint32_t>(value));return b;}
Bytes Real(float value) {Bytes b{0x1e};Float(b,value);return b;}
Bytes String(const std::string& value) {Bytes b{0x1f};b.insert(b.end(),value.begin(),value.end());b.push_back(0);return b;}
Bytes Native(std::uint16_t index,std::initializer_list<Bytes> args={}) {
    Bytes b;
    if(index>=0x70 && index<=0xff) b.push_back(static_cast<std::uint8_t>(index));
    else {b.push_back(static_cast<std::uint8_t>(0x60u+(index>>8u)));b.push_back(static_cast<std::uint8_t>(index));}
    for(const auto& arg:args) {b.insert(b.end(),arg.begin(),arg.end());}
    b.push_back(0x16);return b;
}
Bytes Call(std::uint8_t opcode,std::int32_t index,std::initializer_list<Bytes> args={}) {
    auto b=Ref(opcode,index);for(const auto& arg:args) {b.insert(b.end(),arg.begin(),arg.end());}
    b.push_back(0x16);return b;
}
Bytes Return(const Bytes& value) {return Join({{0x04},value});}
Bytes Let(const Bytes& lhs,const Bytes& rhs,std::uint8_t opcode=0x0f) {return Join({{opcode},lhs,rhs});}
Bytes Skip(const Bytes& value) {Bytes b{0x18};U16(b,static_cast<std::uint16_t>(value.size()));return Join({b,value});}
Bytes Context(const Bytes& object,const Bytes& child) {
    auto b=Join({{0x19},object});U16(b,static_cast<std::uint16_t>(child.size()));b.push_back(0);return Join({b,child});
}
Bytes Element(const Bytes& index,const Bytes& array) {return Join({{0x1a},index,array});}
Bytes Member(std::int32_t field,const Bytes& parent) {return Join({Ref(0x36,field),parent});}
Bytes Cast(std::uint8_t opcode,std::int32_t target,const Bytes& object) {return Join({Ref(opcode,target),object});}
void PatchU16(Bytes& b,std::size_t position,std::size_t value) {
    Require(value<=65535 && position+1<b.size(),"Synthetic logical branch offset overflow");
    b[position]=static_cast<std::uint8_t>(value);b[position+1]=static_cast<std::uint8_t>(value>>8u);
}

// Keys re-resolve storage on every read/write, so transaction restoration cannot
// invalidate an alias. Observation logs deliberately survive rollback: they also
// prove that a suppressed branch was not evaluated in the first place.
struct TestHost final:Vm::Host {
    using Identity=std::pair<std::string,std::int32_t>;
    using Slot=std::tuple<std::string,Vm::Scope,std::string>;
    std::map<Identity,Vm::Property> properties;
    std::map<Identity,std::string> names,objects;
    std::map<Identity,std::shared_ptr<const Vm::Function>> finals;
    std::map<std::tuple<std::string,Vm::CallKind,std::string>,std::shared_ptr<const Vm::Function>> named;
    std::map<Slot,std::vector<Value>> storage,saved;
    struct CastClass {std::string parent,name;};
    std::map<std::string,CastClass> castClasses;
    std::map<std::string,std::string> castInstances;
    std::optional<Value> castResultOverride;
    std::size_t castClassResolutions{},castCalls{};
    std::vector<std::string> observations;
    std::vector<std::vector<Value>> nativeArguments;
    std::vector<std::string> nativeReceivers,nativeDeclarations;
    std::shared_ptr<Vm::Reference> retainedNativeReference,retainedNativeElement;
    std::size_t beginCount{},commitCount{},rollbackCount{},effects{},savedEffects{};
    std::uint32_t rng{12345},savedRng{};
    bool active{};
    std::set<std::string> disabledFunctions;
    bool CanCall(const Vm::Function& fn, const std::string&) override {
        return !disabledFunctions.contains(fn.path);
    }
    void Begin() override {
        if(active) throw std::runtime_error("Nested host transaction");
        active=true;saved=storage;savedEffects=effects;savedRng=rng;++beginCount;
    }
    void Commit() override {if(!active) throw std::runtime_error("Commit outside transaction");active=false;++commitCount;}
    void Rollback() noexcept override {storage.swap(saved);effects=savedEffects;rng=savedRng;active=false;++rollbackCount;}
    Vm::Property ResolveProperty(const Vm::Function& f,std::int32_t index) override {return properties.at({f.source,index});}
    std::string ResolveName(const Vm::Function& f,std::int32_t index) override {return names.at({f.source,index});}
    std::string ResolveObject(const Vm::Function& f,std::int32_t index) override {return objects.at({f.source,index});}
    static std::string Fold(std::string text) {
        for (char& c : text) c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return text;
    }
    std::string ResolveCastClass(const Vm::Function& f,std::int32_t index) override {
        ++castClassResolutions;observations.push_back("cast-target:"+f.source+":"+std::to_string(index));
        const auto identity=ResolveObject(f,index);
        if (!castClasses.contains(identity)) throw std::runtime_error("Fixture cast target is not an actual Class");
        return identity;
    }
    Value CastObject(const std::string& target,const Value& value,bool meta) override {
        ++castCalls;observations.push_back(meta ? "metacast" : "dynamiccast");
        if (value.kind!=Vm::Kind::Object || !castClasses.contains(target))
            throw std::runtime_error("Fixture cast lacks a typed target/object");
        if (castResultOverride) return *castResultOverride;
        if (value.text.empty()) return Value::Text(Vm::Kind::Object,{});
        const bool sourceClass=castClasses.contains(value.text);
        if (!sourceClass && !castInstances.contains(value.text))
            throw std::runtime_error("Fixture cast object identity missing");
        if (meta && !sourceClass) return Value::Text(Vm::Kind::Object,{});
        auto current=meta ? value.text : sourceClass ? "Core.Class" : castInstances.at(value.text);
        std::set<std::string> visited;
        for (std::size_t depth=0;!current.empty();++depth) {
            if (depth>=128u || !visited.insert(Fold(current)).second)
                throw std::runtime_error("Fixture cast ancestry cycle/depth limit");
            const auto& cls=castClasses.at(current);
            if (meta ? Fold(current)==Fold(target) : Fold(cls.name)==Fold(castClasses.at(target).name))
                return value;
            current=cls.parent;
        }
        return Value::Text(Vm::Kind::Object,{});
    }
    std::shared_ptr<const Vm::Function> ResolveFunction(const Vm::Function& caller,const std::string& receiver,const Vm::Invocation& call) override {
        if(call.kind==Vm::CallKind::Final) return finals.at({caller.source,call.reference});
        return named.at({receiver,call.kind,call.name});
    }
    Vm::Property Property(const Vm::Function& f,std::int32_t ref,const std::string& name,Value zero,
        std::uint32_t flags=0,std::size_t dimension=1,bool local=false) {
        Vm::Property p{local ? f.path+"."+name : "Fixture.Actor."+name,name,std::move(zero),flags,dimension};
        properties[{f.source,ref}]=p;return p;
    }
    void Store(const std::string& receiver,const Vm::Property& p,const std::vector<Value>& values,Vm::Scope scope=Vm::Scope::Instance) {
        Require(values.size()==p.arrayDimension,"Bad generated property array");storage[{receiver,scope,p.key}]=values;
    }
    Value Load(const std::string& receiver,const Vm::Property& p,std::size_t element=0,Vm::Scope scope=Vm::Scope::Instance) const {
        return storage.at({receiver,scope,p.key}).at(element);
    }
    std::shared_ptr<Vm::Reference> Reference(const Slot& slot,const Vm::Property& p,std::size_t index=0) {
        if(!storage.contains(slot)) storage[slot]=std::vector<Value>(p.arrayDimension,p.zero);
        auto ref=std::make_shared<Vm::Reference>();ref->zero=p.zero;ref->dimension=p.arrayDimension;
        ref->read=[this,slot,index] {return storage.at(slot).at(index);};
        ref->write=[this,slot,index](const Value& v) {storage.at(slot).at(index)=v;};
        ref->element=[this,slot,p](std::size_t i) {return Reference(slot,p,i);};
        return ref;
    }
    std::shared_ptr<Vm::Reference> Variable(const std::string& receiver,const Vm::Property& p,Vm::Scope scope) override {
        observations.push_back("variable:"+receiver+":"+p.name);return Reference({receiver,scope,p.key},p);
    }
    Vm::Evaluation Native(std::uint16_t index,const std::string& receiver,
        const std::vector<Vm::Evaluation>& arguments,const Vm::Function* declaration) override {
        std::vector<Value> values;for(const auto& arg:arguments) values.push_back(arg.Load());
        nativeArguments.push_back(values);nativeReceivers.push_back(receiver);
        nativeDeclarations.push_back(declaration ? declaration->path : "");
        if(index==1000) {observations.push_back("effect");++effects;rng=rng*1664525u+1013904223u;return {Value::Bool(true),{}};}
        if(index==1001) {
            observations.push_back("lhs");
            return {Value::Integer(0),{}};
        }
        if(index==1002) {observations.push_back("rhs");return {Value::Integer(23),{}};}
        if(index==1003) {return {values.empty() ? Value{} : values.front(),{}};}
        if(index==1004) {
            if(arguments.empty() || !arguments.front().reference) throw std::runtime_error("Native out argument lost alias");
            arguments.front().reference->write(Value::Integer(9));return {Value::Integer(9),{}};
        }
        if(index==1005) {Value v;v.kind=Vm::Kind::Struct;v.fields["x"]=Value::Integer(44);return {std::move(v),{}};}
        if(index==1006) {
            if(arguments.empty() || !arguments.front().reference) throw std::runtime_error("Native external out argument lost alias");
            arguments.front().reference->write(Value::Integer(17));
            throw std::runtime_error("Fixture failure after external out write");
        }
        if(index==1007) {
            if(arguments.empty() || !arguments.front().reference) throw std::runtime_error("Native return-alias fixture lost argument alias");
            retainedNativeReference=arguments.front().reference;
            if(retainedNativeReference->element) retainedNativeElement=retainedNativeReference->element(0);
            return arguments.front();
        }
        if(index==0 && declaration) {
            return {Value::Integer(values.empty() ? -1 : static_cast<std::int32_t>(values.front().kind)),{}};
        }
        throw std::runtime_error("Fixture unknown native "+std::to_string(index));
    }
};
Vm::Function Function(const std::string& name,Bytes code,const std::string& source="Fixture") {
    Vm::Function f;f.path=source+"."+name;f.source=source;f.bytecode=std::move(code);return f;
}
void Returned(const Vm::Result& r,const std::string& context) {
    Require(r.passed(),context+": "+r.error+" (status "+std::to_string(static_cast<int>(r.status))+", offset "+std::to_string(r.offset)+")");
}
void Failed(const Vm::Result& r,Vm::Status status,const TestHost& host,const std::string& context) {
    Require(r.status==status && !r.error.empty(),context+": wrong failure status or no diagnostic");
    Require(host.beginCount==1 && host.commitCount==0 && host.rollbackCount==1 && !host.active,
        context+": whole-call-tree transaction did not roll back exactly once");++rejections;
}
void ScalarAndLocalContracts() {
    TestHost host;
    auto f=Function("Locals",Join({Let(Ref(0x00,1),Real(7.75f)),Return(Ref(0x00,1))}));
    const auto local=host.Property(f,1,"Temporary",Value::Integer(0),0,1,true);f.variables={local};
    auto r=Vm::Execute(host,f,"Self");Returned(r,"Typed local assignment");
    Require(r.value.kind==Vm::Kind::Int && r.value.integer==7,"Let did not coerce float into typed int local");
    r=Vm::Execute(host,Function("Fresh",Return(Ref(0x00,1))),"Self");
    Require(!r.passed(),"Function accessed a local absent from its variable metadata");
    auto zero=Function("Zero",Return(Ref(0x00,1)));zero.variables={local};
    r=Vm::Execute(host,zero,"Self");Returned(r,"Fresh local zero");
    Require(r.value.kind==Vm::Kind::Int && r.value.integer==0,"Local storage leaked from previous function call");
    Require(host.storage.empty(),"Script local storage escaped into host instance properties");
    for(const auto opcode:{std::uint8_t{0x24},std::uint8_t{0x2c}}) {
        r=Vm::Execute(host,Function("ByteConstant",Return({opcode,7})),"Self");Returned(r,"Pinned byte constant type");
        Require(r.value.kind==Vm::Kind::Byte && r.value.integer==7,"ByteConst/IntConstByte did not retain pinned Byte type");
    }
    struct Conversion {std::uint8_t token;Bytes argument;Vm::Kind kind;std::int32_t integer;bool boolean;};
    for(const auto& c:std::vector<Conversion>{{0x3d,Int(258),Vm::Kind::Byte,2,false},
        {0x3e,Int(-3),Vm::Kind::Bool,0,true},{0x44,Real(-7.9f),Vm::Kind::Int,-7,false},
        {0x4b,String("True"),Vm::Kind::Bool,0,false},{0x4b,String("2"),Vm::Kind::Bool,0,true},
        {0x3a,Int(258),Vm::Kind::Int,2,false},{0x3b,Real(0.5f),Vm::Kind::Bool,0,false},
        {0x3e,Real(0.5f),Vm::Kind::Bool,0,false},{0x44,Int(16777217),Vm::Kind::Int,16777216,false}}) {
        r=Vm::Execute(host,Function("Convert",Return(Join({{c.token},c.argument}))),"Self");Returned(r,"Pinned explicit conversion");
        Require(r.value.kind==c.kind && (c.kind==Vm::Kind::Bool ? r.value.boolean==c.boolean : r.value.integer==c.integer),
            "Explicit byte/int/string conversion diverged from pinned semantics");
    }
    for(const auto& c:std::vector<std::tuple<std::uint8_t,Bytes,float>>{
        {0x3c,Int(258),2.0f},{0x3f,Real(5.75f),5.0f}}) {
        const auto& [token,argument,expected]=c;
        r=Vm::Execute(host,Function("NarrowFloatConversion",Return(Join({{token},argument}))),"Self");Returned(r,"Pinned intermediate conversion narrowing");
        Require(r.value.kind==Vm::Kind::Float && r.value.floating==expected,"Explicit conversion bypassed pinned byte/int intermediate narrowing");
    }
    Require(Vm::Equal(Value::Text(Vm::Kind::Name,"Idle"),Value::Text(Vm::Kind::Name,"IDLE")),"Name equality lost case-insensitive identity");
    Require(Vm::Equal(Value::Text(Vm::Kind::Name,""),Value::Text(Vm::Kind::Name,"None")),"Empty name no longer canonicalizes to None");
    Require(!Vm::Equal(Value::Integer(0),Value{}),"Nothing collapsed into a concrete integer zero");
    host.names[{"Fixture",10}]="";
    r=Vm::Execute(host,Function("ImplicitNameString",Return(Native(112,{Ref(0x21,10),String("suffix")}))),"Self");
    Returned(r,"Implicit empty Name to String");
    Require(r.value.kind==Vm::Kind::String && r.value.text=="Nonesuffix","Implicit Name to String lost canonical None spelling");
}
void ParametersAndReturns() {
    TestHost host;
    auto callee=Function("TypedCallee",Join({Let(Ref(0x00,2),Ref(0x00,1)),Return(Ref(0x00,2))}),"CalleePackage");
    const auto in=host.Property(callee,1,"Input",Value::Integer(0),0x80,1,true);
    const auto out=host.Property(callee,2,"Output",Value::Integer(0),0x180,1,true);
    const auto result=host.Property(callee,3,"ReturnValue",Value::Integer(0),0x480,1,true);
    callee.variables={in,out,result};
    auto caller=Function("TypedCaller",Return(Call(0x1c,4,{Real(6.75f),Ref(0x01,8)})),"CallerPackage");
    const auto actor=host.Property(caller,8,"External",Value::Integer(0));host.Store("Self",actor,{Value::Integer(2)});
    host.finals[{caller.source,4}]=std::make_shared<Vm::Function>(callee);
    auto r=Vm::Execute(host,caller,"Self");Returned(r,"Script input/output and return");
    Require(r.value.kind==Vm::Kind::Int && r.value.integer==6 && host.Load("Self",actor).integer==6,
        "Script typed input/out alias/return did not copy in and out correctly");
    Require(host.beginCount==1 && host.commitCount==1 && host.rollbackCount==0,"Nested script call opened separate transaction");
    callee.bytecode=Return({0x0b});host.finals[{caller.source,4}]=std::make_shared<Vm::Function>(callee);
    r=Vm::Execute(host,caller,"Self");Returned(r,"Typed Nothing return");
    Require(r.value.kind==Vm::Kind::Int && r.value.integer==0,"Return Nothing ignored declared typed return zero");
    callee.bytecode=Return(Real(4.75f));host.finals[{caller.source,4}]=std::make_shared<Vm::Function>(callee);
    r=Vm::Execute(host,caller,"Self");Returned(r,"Concrete return remains its expression type");
    Require(r.value.kind==Vm::Kind::Float && r.value.floating==4.75f,
        "Concrete return was implicitly converted into declared ReturnParm type");
    auto native=Function("OutNative",Return(Native(1004,{Ref(0x01,8)})),"CallerPackage");
    r=Vm::Execute(host,native,"Self");Returned(r,"Native out alias");
    Require(host.Load("Self",actor).integer==9,"Native argument lost writable alias");
}
void AssignmentArraysAndStructs() {
    TestHost host;
    auto f=Function("AssignmentOrder",Return(Let(Element(Native(1001),Ref(0x01,1)),Native(1002))));
    const auto slot=host.Property(f,1,"Slot",Value::Integer(0));host.Store("Self",slot,{Value::Integer(0)});
    auto r=Vm::Execute(host,f,"Self");Returned(r,"Let evaluation order");
    Require(host.observations==std::vector<std::string>{"lhs","variable:Self:Slot","rhs"} && r.value.integer==23,
        "Let did not evaluate LHS before RHS or return the assigned alias");
    f=Function("NothingAssignment",Return(Let({0x0b},Int(17))));
    r=Vm::Execute(host,f,"Self");Returned(r,"Nothing assignment");Require(r.value.integer==17,"Nothing LHS did not preserve RHS");
    f=Function("ArrayClamp",Join({Let(Element(Int(-12),Ref(0x01,1)),Int(11)),
        Let(Element(Int(900),Ref(0x01,1)),Int(99)),Return(Element(Int(1),Ref(0x01,1)))}));
    const auto array=host.Property(f,1,"Array",Value::Integer(0),0,3);
    host.Store("Self",array,{Value::Integer(1),Value::Integer(2),Value::Integer(3)});
    r=Vm::Execute(host,f,"Self");Returned(r,"Pinned fixed array clamping");
    Require(host.Load("Self",array,0).integer==11 && host.Load("Self",array,1).integer==2 &&
        host.Load("Self",array,2).integer==99 && r.value.integer==2,"Fixed array indices did not clamp to first/last element");
    host.observations.clear();
    f.bytecode=Return(Element(Native(1002),Ref(0x01,1)));
    r=Vm::Execute(host,f,"Self");Returned(r,"Array evaluation order");
    Require(host.observations==std::vector<std::string>{"rhs","variable:Self:Array"},"Array expression evaluated before its index");
    Value zero;zero.kind=Vm::Kind::Struct;zero.fields["x"]=Value::Integer(0);zero.fields["untouched"]=Value::Bool(false);
    f=Function("StructAlias",Join({Let(Member(2,Ref(0x01,1)),Int(37)),Return(Member(2,Ref(0x01,1)))}));
    const auto structure=host.Property(f,1,"Structure",zero);host.Property(f,2,"X",Value::Integer(0));host.Store("Self",structure,{zero});
    r=Vm::Execute(host,f,"Self");Returned(r,"Nested struct writable alias");
    const auto stored=host.Load("Self",structure);
    Require(r.value.integer==37 && stored.fields.at("x").integer==37 && !stored.fields.at("untouched").boolean,
        "Struct member assignment lost parent alias, field casing, or unrelated fields");
    f.bytecode=Return(Member(2,Native(1005)));
    r=Vm::Execute(host,f,"Self");Returned(r,"Temporary struct ownership");
    Require(r.value.kind==Vm::Kind::Int && r.value.integer==44,"Temporary struct member alias dangled before return load");
    f=Function("BoolAlias",Join({Let(Join({{0x2d},Ref(0x01,1)}),{0x27},0x14),Return(Ref(0x01,1))}));
    const auto boolean=host.Property(f,1,"Boolean",Value::Bool(false));host.Store("Self",boolean,{Value::Bool(false)});
    r=Vm::Execute(host,f,"Self");Returned(r,"BoolVariable alias");Require(r.value.kind==Vm::Kind::Bool && r.value.boolean,"BoolVariable stripped assignment alias");
    f=Function("Default",Return(Ref(0x02,1)));host.Property(f,1,"Boolean",Value::Bool(false));
    host.Store("Self",boolean,{Value::Bool(false)},Vm::Scope::Default);
    r=Vm::Execute(host,f,"Self");Returned(r,"Default property scope");Require(!r.value.boolean,"DefaultVariable incorrectly read instance storage");
    f=Function("CompoundSnapshots",Return(Native(146,{Native(161,{Ref(0x01,1),Int(1)}),Native(161,{Ref(0x01,1),Int(1)})})));
    const auto counter=host.Property(f,1,"Counter",Value::Integer(0));host.Store("Self",counter,{Value::Integer(1)});
    r=Vm::Execute(host,f,"Self");Returned(r,"Compound native detached return values");
    Require(r.value.integer==5 && host.Load("Self",counter).integer==3,
        "(A+=1)+(A+=1) retained mutable operand aliases instead of native return snapshots");
    f.bytecode=Return(Native(146,{Native(1007,{Ref(0x01,1)}),Native(161,{Ref(0x01,1),Int(1)})}));
    host.Store("Self",counter,{Value::Integer(1)});
    r=Vm::Execute(host,f,"Self");Returned(r,"Host native argument-alias return detaches");
    Require(r.value.integer==3 && host.Load("Self",counter).integer==2,"Host native returned an alias to a mutable out argument");
    const auto expired=[&](const std::function<void()>& action,const std::string& context) {
        bool rejected{};try {action();}catch(const std::runtime_error&) {rejected=true;}
        Require(rejected,context);
    };
    Require(host.retainedNativeReference && host.retainedNativeElement,"Native reference lifetime fixture did not retain aliases");
    expired([&] {host.retainedNativeReference->read();},"Expired native reference read did not fail cleanly");
    expired([&] {host.retainedNativeReference->write(Value::Integer(90));},"Expired native reference write did not fail cleanly");
    expired([&] {host.retainedNativeReference->element(0);},"Expired native element lookup did not fail cleanly");
    expired([&] {host.retainedNativeElement->read();},"Expired derived native element read did not fail cleanly");
    Require(host.Load("Self",counter).integer==2,"Expired native reference mutated committed storage");
}
void LazyAndContextContracts() {
    for(const auto& test:std::vector<std::tuple<std::uint16_t,bool,bool,std::size_t>>{
        {130,false,false,0},{130,true,true,1},{132,true,true,0},{132,false,true,1}}) {
        TestHost host;const auto [native,left,expected,effects]=test;
        const auto f=Function("Lazy",Return(Native(native,{{static_cast<std::uint8_t>(left?0x27:0x28)},Skip(Native(1000))})));
        const auto r=Vm::Execute(host,f,"Self");Returned(r,"Lazy native logical operator");
        Require(r.value.kind==Vm::Kind::Bool && r.value.boolean==expected && host.effects==effects,
            "Logical native evaluated skipped RHS or lost boolean result");
    }
    TestHost host;
    auto f=Function("SkipIsNotBranch",Return(Skip(Native(1000))));
    auto r=Vm::Execute(host,f,"Self");Returned(r,"Standalone Skip");Require(host.effects==1,"Standalone Skip incorrectly suppressed its child");
    f=Function("NullContext",Return(Context({0x2a},Native(1000))));
    r=Vm::Execute(host,f,"Self");Returned(r,"Null context suppression");
    Require(r.value.kind==Vm::Kind::Nothing && host.effects==1,"Null Context evaluated child native or fabricated a concrete zero");
    f=Function("ContextArguments",Return(Context(Ref(0x20,1),Native(1003,{Ref(0x01,2),{0x17}}))));
    host.objects[{f.source,1}]="Other";
    const auto p=host.Property(f,2,"Value",Value::Integer(0));
    host.Store("Self",p,{Value::Integer(12)});host.Store("Other",p,{Value::Integer(77)});
    r=Vm::Execute(host,f,"Self");Returned(r,"Context call arguments");
    Require(r.value.integer==12 && host.nativeReceivers.back()=="Other" && host.nativeArguments.back().size()==2 &&
        host.nativeArguments.back()[1].kind==Vm::Kind::Object && host.nativeArguments.back()[1].text=="Self",
        "Call argument expressions used receiver context rather than original Self");
    f.bytecode=Return(Context(Ref(0x20,1),Ref(0x01,2)));
    r=Vm::Execute(host,f,"Self");Returned(r,"Context property read");Require(r.value.integer==77,"Context did not change instance property receiver");
    // Native130 also stays lazy when reached through a native declaration.
    auto logical=Function("DeclaredAnd",{},"NativePackage");logical.flags=0x400;logical.nativeIndex=130;
    host.finals[{f.source,9}]=std::make_shared<Vm::Function>(logical);
    f.bytecode=Return(Call(0x1c,9,{{0x28},Skip(Native(1000))}));
    r=Vm::Execute(host,f,"Self");Returned(r,"Declared logical native laziness");Require(host.effects==1,"Final native logical call eagerly evaluated RHS");
    {
        TestHost rejected;auto classContext=Context(Ref(0x20,1),Native(1000));classContext[0]=0x12;
        auto function=Function("UnsupportedClassContext",Return(classContext));rejected.objects[{function.source,1}]="ClassObject";
        const auto result=Vm::Execute(rejected,function,"Self");
        Failed(result,Vm::Status::Unsupported,rejected,"ClassContext without class-default identity");
        Require(rejected.effects==0 && rejected.observations.empty(),"Unsupported ClassContext evaluated child on an ordinary object");
    }
}
void OptionalAndReferenceTables() {
    TestHost host;
    auto script=Function("OptionalScript",Return(Ref(0x00,1)),"ScriptSource");
    script.variables={host.Property(script,1,"Rate",Value::Float(0),0x90,1,true),
        host.Property(script,2,"ReturnValue",Value::Float(0),0x480,1,true)};
    auto caller=Function("OptionalCaller",Return(Call(0x1c,7,{{0x0b}})),"CallerSource");
    host.finals[{caller.source,7}]=std::make_shared<Vm::Function>(script);
    auto r=Vm::Execute(host,caller,"Self");Returned(r,"Script explicit omitted optional");
    Require(r.value.kind==Vm::Kind::Float && r.value.floating==0,"Script Nothing optional did not bind typed zero");
    caller.bytecode=Return(Call(0x1c,7));r=Vm::Execute(host,caller,"Self");Returned(r,"Script trailing omitted optional");
    Require(r.value.kind==Vm::Kind::Float && r.value.floating==0,"Trailing omitted script optional did not bind typed zero");
    auto native=Function("OptionalNative",{},"NativeSource");native.flags=0x400;native.nativeIndex=0;
    native.variables={host.Property(native,1,"Rate",Value::Float(0),0x90,1,true)};
    host.finals[{caller.source,8}]=std::make_shared<Vm::Function>(native);
    caller.bytecode=Return(Call(0x1c,8,{{0x0b}}));r=Vm::Execute(host,caller,"Self");Returned(r,"Native explicit omitted optional");
    Require(r.value.integer==static_cast<std::int32_t>(Vm::Kind::Nothing) && host.nativeArguments.back().size()==1 &&
        host.nativeDeclarations.back()==native.path,"Native optional Nothing was coerced before declaration-aware callback");
    caller.bytecode=Return(Call(0x1c,8));r=Vm::Execute(host,caller,"Self");Returned(r,"Native trailing omitted optional");
    Require(host.nativeArguments.back().size()==1 && host.nativeArguments.back()[0].kind==Vm::Kind::Nothing &&
        r.value.integer==static_cast<std::int32_t>(Vm::Kind::Nothing),"Trailing omitted native optional did not pad Nothing to its signature");
    // A nested name reference with the same integer is local to its own source.
    script.bytecode=Return(Ref(0x21,3));script.variables.clear();
    host.names[{script.source,3}]="CalleeName";host.names[{caller.source,3}]="CallerName";
    host.finals[{caller.source,7}]=std::make_shared<Vm::Function>(script);
    caller.bytecode=Return(Call(0x1c,7));r=Vm::Execute(host,caller,"Self");Returned(r,"Nested source name table");
    Require(r.value.kind==Vm::Kind::Name && r.value.text=="CalleeName","Nested function used caller's name table");
    host.names[{caller.source,4}]="Method";
    const auto virtualMethod=std::make_shared<Vm::Function>(Function("StateOverride",Return(Int(1))));
    const auto globalMethod=std::make_shared<Vm::Function>(Function("ClassMethod",Return(Int(2))));
    host.named[{"Self",Vm::CallKind::Virtual,"Method"}]=virtualMethod;
    host.named[{"Self",Vm::CallKind::Global,"Method"}]=globalMethod;
    caller.bytecode=Return(Call(0x1b,4));r=Vm::Execute(host,caller,"Self");Returned(r,"Virtual resolution");Require(r.value.integer==1,"Virtual invocation kind lost");
    caller.bytecode=Return(Call(0x38,4));r=Vm::Execute(host,caller,"Self");Returned(r,"Global resolution");Require(r.value.integer==2,"Global invocation incorrectly used state override");
}
void ControlAndFailureContracts() {
    {
        TestHost host;Bytes code{0x07,0,0,0x28};const auto skipped=Native(1000);code.insert(code.end(),skipped.begin(),skipped.end());
        PatchU16(code,1,code.size());const auto returned=Return(Int(41));code.insert(code.end(),returned.begin(),returned.end());
        auto r=Vm::Execute(host,Function("Branch",code),"Self");Returned(r,"Absolute logical JumpIfNot");
        Require(r.value.integer==41 && host.effects==0,"Logical absolute jump did not skip entire statement");
    }
    for(const auto target:{std::uint16_t{5},std::uint16_t{100}}) {
        TestHost host;Bytes code{0x07};U16(code,target);code.push_back(0x28);const auto returned=Return({0x26});
        code.insert(code.end(),returned.begin(),returned.end());
        const auto r=Vm::Execute(host,Function("BadJump",code),"Self");Failed(r,Vm::Status::Invalid,host,"Jump into nested expression or outside body");
        Require(host.observations.empty(),"Invalid jump produced host effects");
    }
    {
        TestHost host;const auto f=Function("ParseBeforeEffects",Join({Native(1000),{0x04,0x1d,1}}));
        const auto r=Vm::Execute(host,f,"Self");Failed(r,Vm::Status::Invalid,host,"Truncated later expression");
        Require(host.observations.empty() && host.effects==0,"Full bounded parse did not precede native effects");
    }
    // Unsupported-but-well-formed nodes in untaken branches remain legal.
    const Bytes unsupported{0x10,0x25,0x0b};
    {
        TestHost host;Bytes code{0x07,0,0,0x28};code.insert(code.end(),unsupported.begin(),unsupported.end());
        PatchU16(code,1,code.size());const auto returned=Return({0x26});code.insert(code.end(),returned.begin(),returned.end());
        auto r=Vm::Execute(host,Function("UntakenUnsupported",code),"Self");Returned(r,"Untaken supported parse of unsupported dynamic array");
        Require(r.value.integer==1,"Untaken unsupported branch changed result");
    }
    {
        TestHost host;const auto r=Vm::Execute(host,Function("ExecutedUnsupported",Join({Native(1000),unsupported,Return({0x26})})),"Self");
        Failed(r,Vm::Status::Unsupported,host,"Executed dynamic array unsupported");
        Require(host.effects==0 && host.rng==12345 && host.observations==std::vector<std::string>{"effect"},
            "Unsupported opcode did not roll back earlier native effect/RNG");
    }
    {
        TestHost host;const auto r=Vm::Execute(host,Function("UnsupportedSwitch",Join({Native(1000),{0x05,0,0x25},Return({0x26})})),"Self");
        Failed(r,Vm::Status::Unsupported,host,"Switch without control-flow support");
        Require(host.effects==0,"Unsupported Switch committed native prefix effects");
    }
    {
        TestHost host;const auto r=Vm::Execute(host,Function("UnknownNative",Join({Native(1000),Native(4095),Return({0x26})})),"Self");
        Require((r.status==Vm::Status::Unsupported || r.status==Vm::Status::Invalid) && !r.error.empty(),"Unknown host native silently succeeded");
        Require(host.rollbackCount==1 && host.commitCount==0 && host.effects==0 && host.rng==12345,"Unknown native escaped transaction rollback");++rejections;
    }
    {
        TestHost host;const auto function=Function("NativeFailureTrace",Return(Native(4095,{Int(31)})));
        const auto r=Vm::Execute(host,function,"Self");
        Failed(r,Vm::Status::Unsupported,host,"Native failure trace");
        Require(r.function==function.path && r.offset==1 && r.opcode==0x6f,
            "Native failure diagnostic points at its evaluated argument instead of the failing call");
    }
    {
        TestHost host;const auto function=Function("ConversionFailureTrace",Return(Join({{0x46},{0x26}})));
        const auto r=Vm::Execute(host,function,"Self");
        Failed(r,Vm::Status::Unsupported,host,"Unsupported conversion trace");
        Require(r.function==function.path && r.offset==1 && r.opcode==0x46,
            "Conversion failure diagnostic points at its child instead of unsupported conversion");
    }
    {
        TestHost host;const auto r=Vm::Execute(host,Function("IntegerDivideZero",Join({Native(1000),Return(Native(145,{Int(9),Int(0)}))})),"Self");
        Require(!r.passed() && !r.error.empty() && host.rollbackCount==1 && host.effects==0,"Integer divide by zero succeeded or escaped rollback");++rejections;
    }
    {
        TestHost host;const auto r=Vm::Execute(host,Function("NonFiniteConstant",Join({Native(1000),Return(Real(std::numeric_limits<float>::infinity()))})),"Self");
        Failed(r,Vm::Status::Invalid,host,"Non-finite float constant");Require(host.effects==0,"Non-finite constant retained earlier native effect");
    }
    {
        TestHost host;auto latent=Function("LatentNative",{},"NativeSource");latent.flags=0x408;latent.nativeIndex=1000;
        const auto caller=Function("LatentCaller",Return(Call(0x1c,1)));
        host.finals[{caller.source,1}]=std::make_shared<Vm::Function>(latent);
        const auto r=Vm::Execute(host,caller,"Self");Failed(r,Vm::Status::Unsupported,host,"Latent call without continuation engine");
        Require(host.effects==0 && host.observations.empty(),"Unsupported latent native executed host effects before rejection");
    }
    {
        TestHost host;const auto r=Vm::Execute(host,Function("Stop",{0x08}),"Self");
        Require(r.status==Vm::Status::Stopped,"Stop was conflated with a return or unsupported statement");
    }
}
void BudgetAndNestedRollback() {
    {
        TestHost host;auto leaf=Function("LeafLoop",Join({Native(1000),{0x06,0,0}}),"LeafSource");
        auto caller=Function("Caller",Return(Call(0x1c,1)),"RootSource");host.finals[{caller.source,1}]=std::make_shared<Vm::Function>(leaf);
        Vm::Limits limits;limits.instructions=24;
        const auto r=Vm::Execute(host,caller,"Self",{},limits);Failed(r,Vm::Status::Budget,host,"Nested instruction budget");
        Require(r.instructions<=limits.instructions && host.effects==0 && host.rng==12345 && !host.observations.empty(),
            "Nested budget reset per call or did not roll back native/RNG state");
    }
    {
        TestHost host;auto recursive=Function("Recursive",Join({Native(1000),Return(Call(0x1c,1))}));
        host.finals[{recursive.source,1}]=std::make_shared<Vm::Function>(recursive);
        Vm::Limits limits;limits.callDepth=4;
        const auto r=Vm::Execute(host,recursive,"Self",{},limits);Failed(r,Vm::Status::Budget,host,"Call recursion budget");
        Require(host.effects==0 && host.rng==12345 && !r.callStack.empty(),"Recursive failure omitted call stack or escaped root rollback");
    }
    {
        TestHost host;auto callee=Function("FailingOut",Join({Let(Ref(0x00,1),Int(88)),Native(4095),Return({0x0b})}),"Callee");
        callee.variables={host.Property(callee,1,"Output",Value::Integer(0),0x180,1,true)};
        auto caller=Function("Outer",Join({Let(Ref(0x01,2),Int(55)),Return(Call(0x1c,1,{Ref(0x01,2)}))}),"Caller");
        const auto p=host.Property(caller,2,"Value",Value::Integer(0));host.Store("Self",p,{Value::Integer(3)});
        host.finals[{caller.source,1}]=std::make_shared<Vm::Function>(callee);
        const auto r=Vm::Execute(host,caller,"Self");
        Require(!r.passed() && host.rollbackCount==1 && host.Load("Self",p).integer==3,"Failed nested out call did not restore outer writes");++rejections;
    }
    const auto budget=[&](const std::string& context,const Vm::Function& f,const Vm::Limits& limits) {
        TestHost host;const auto r=Vm::Execute(host,f,"Self",{},limits);Failed(r,Vm::Status::Budget,host,context);Require(host.effects==0,"Budget failure committed native effect");
    };
    Vm::Limits limits;limits.sourceBytes=2;budget("Source byte budget",Function("Source",Return(Int(7))),limits);
    limits={};limits.nodes=1;budget("Parsed node budget",Function("Nodes",Return(Int(7))),limits);
    limits={};limits.expressionDepth=2;budget("Expression nesting budget",Function("Depth",Return(Join({{0x2d,0x2d},{0x27}}))),limits);
    limits={};limits.stringBytes=3;budget("String allocation budget",Function("String",Return(String("too long"))),limits);
    limits={};limits.arguments=1;budget("Call argument budget",Function("Args",Return(Native(1003,{{0x25},{0x26}}))),limits);
    {
        TestHost host;auto f=Function("LocalsBudget",Return({0x0b}));
        f.variables={host.Property(f,1,"Array",Value::Integer(0),0,5,true)};limits={};limits.localElements=4;
        const auto r=Vm::Execute(host,f,"Self",{},limits);Failed(r,Vm::Status::Budget,host,"Local element allocation budget");
    }
    {
        TestHost host;auto f=Function("WritesBudget",Join({Let(Ref(0x01,1),Int(1)),Let(Ref(0x01,1),Int(2)),Return({0x0b})}));
        const auto p=host.Property(f,1,"Counter",Value::Integer(0));host.Store("Self",p,{Value::Integer(9)});limits={};limits.writes=1;
        const auto r=Vm::Execute(host,f,"Self",{},limits);Failed(r,Vm::Status::Budget,host,"Shared write budget");
        Require(host.Load("Self",p).integer==9,"Write budget failure retained first write");
    }
    {
        TestHost host;auto f=Function("NativeWritesBudget",Join({Native(1004,{Ref(0x01,1)}),Native(1004,{Ref(0x01,1)}),Return({0x0b})}));
        const auto p=host.Property(f,1,"Counter",Value::Integer(0));host.Store("Self",p,{Value::Integer(2)});limits={};limits.writes=1;
        const auto r=Vm::Execute(host,f,"Self",{},limits);Failed(r,Vm::Status::Budget,host,"Native out alias shared write budget");
        Require(r.writes==1 && host.Load("Self",p).integer==2,"Native alias write bypassed interpreter budget or rollback");
    }
    {
        TestHost host;auto f=Function("RetainedBudget",Return({0x0b}));
        f.variables={host.Property(f,1,"Texts",Value::Text(Vm::Kind::String,"12345678"),0,4,true)};
        limits={};limits.retainedBytes=(sizeof(Value)+8u)*4u-1u;
        const auto r=Vm::Execute(host,f,"Self",{},limits);Failed(r,Vm::Status::Budget,host,"Retained local array byte budget");
    }
    {
        TestHost host;auto f=Function("AggregateStructBudget",Return({0x0b}));
        Value tree;tree.kind=Vm::Kind::Struct;
        for(const auto name:{"one","two","three"}) {
            Value child;child.kind=Vm::Kind::Struct;child.fields["text"]=Value::Text(Vm::Kind::String,"12345678");tree.fields[name]=child;
        }
        f.variables={host.Property(f,1,"Tree",tree,0,1,true)};limits={};limits.localElements=6;
        // Root + three child structs + three scalar leaves is seven retained
        // values although every individual map contains at most three fields.
        const auto r=Vm::Execute(host,f,"Self",{},limits);Failed(r,Vm::Status::Budget,host,"Aggregate struct value node budget");
    }
    {
        TestHost host;auto f=Function("ExternalNativeOut",{},"NativeSource");f.flags=0x400;f.nativeIndex=1006;
        f.variables={host.Property(f,1,"Output",Value::Integer(0),0x180,1,true)};
        Value external=Value::Integer(3);auto alias=std::make_shared<Vm::Reference>();alias->zero=Value::Integer(0);
        alias->read=[&] {return external;};alias->write=[&](const Value& v) {external=v;};
        const auto r=Vm::Execute(host,f,"Self",{{{},alias}});
        Failed(r,Vm::Status::Unsupported,host,"External root native out rollback");
        Require(external.integer==3,"Native guard failed to restore caller-owned reference outside Host storage");
    }
    {
        TestHost host;auto f=Function("PartialExternalCopyOut",Join({Let(Ref(0x00,1),Int(8)),Let(Ref(0x00,2),Int(9)),Return({0x0b})}));
        f.variables={host.Property(f,1,"First",Value::Integer(0),0x180,1,true),
            host.Property(f,2,"Second",Value::Integer(0),0x180,1,true)};
        Value first=Value::Integer(1),second=Value::Integer(2);
        const auto alias=[](Value& slot) {
            auto reference=std::make_shared<Vm::Reference>();reference->zero=Value::Integer(0);
            reference->read=[&slot] {return slot;};reference->write=[&slot](const Value& v) {slot=v;};return reference;
        };
        limits={};limits.writes=3; // Two local writes + first copy-out; second fails.
        const auto r=Vm::Execute(host,f,"Self",{{{},alias(first)},{{},alias(second)}},limits);
        Failed(r,Vm::Status::Budget,host,"External script out rollback after partial copy-out");
        Require(first.integer==1 && second.integer==2,"Late copy-out failure left externally-owned first out parameter modified");
    }
}
void IntegerIncrementContracts() {
    for (const std::uint16_t opcode : {163u,164u,165u,166u}) {
        for (const auto seed : {41,std::numeric_limits<std::int32_t>::max(),std::numeric_limits<std::int32_t>::min()}) {
            TestHost host; auto f=Function("IntegerIncrement",Return(Native(opcode,{Ref(0x01,1)})));
            const auto property=host.Property(f,1,"Counter",Value::Integer(0));
            host.Store("Self",property,{Value::Integer(seed)});
            const auto bits=static_cast<std::uint32_t>(seed);
            const auto next=std::bit_cast<std::int32_t>(opcode==163u || opcode==165u ? bits+1u : bits-1u);
            const auto expected=opcode==163u || opcode==164u ? next : seed;
            const auto result=Vm::Execute(host,f,"Self");Returned(result,"Pinned pre/post integer increment/decrement");
            Require(result.value.kind==Vm::Kind::Int && result.value.integer==expected && result.writes==1u &&
                host.Load("Self",property).integer==next,"Pre/post return snapshot, signed wrapping or one-write contract changed");
        }
    }
    {
        TestHost host; auto f=Function("IncrementSnapshots",Return(Native(146,
            {Native(165,{Ref(0x01,1)}),Native(165,{Ref(0x01,1)})})));
        const auto property=host.Property(f,1,"Counter",Value::Integer(0));
        host.Store("Self",property,{Value::Integer(0)});
        const auto result=Vm::Execute(host,f,"Self");Returned(result,"Postincrement result snapshots");
        Require(result.value.integer==1 && host.Load("Self",property).integer==2 && result.writes==2u,
            "Postincrement result aliased a later update instead of retaining its old value");
    }
    {
        TestHost host;auto f=Function("IncrementArray",Return(Native(165,{Element(Native(1001),Ref(0x01,1))})));
        const auto property=host.Property(f,1,"Counters",Value::Integer(0),0u,2u);
        host.Store("Self",property,{Value::Integer(11),Value::Integer(22)});
        const auto result=Vm::Execute(host,f,"Self");Returned(result,"Array-member integer increment");
        Require(result.value.integer==11 && host.Load("Self",property,0u).integer==12 &&
            host.Load("Self",property,1u).integer==22 &&
            std::count(host.observations.begin(),host.observations.end(),"lhs")==1,
            "Integer increment reevaluated its index or wrote another fixed slot");
    }
    {
        TestHost host;auto f=Function("IncrementStruct",Return(Native(163,{Member(2,Ref(0x01,1))})));
        Value zero;zero.kind=Vm::Kind::Struct;zero.fields["count"]=Value::Integer(0);
        const auto property=host.Property(f,1,"Record",zero);
        host.Property(f,2,"Count",Value::Integer(0));host.Store("Self",property,{zero});
        const auto result=Vm::Execute(host,f,"Self");Returned(result,"Nested-member integer increment");
        Require(result.value.integer==1 && host.Load("Self",property).fields.at("count").integer==1 && result.writes==1u,
            "Integer increment lost the nested property's writable root alias");
    }
    for (const auto kind : {Vm::Kind::Byte,Vm::Kind::Float,Vm::Kind::Bool}) {
        TestHost host;auto f=Function("BadIncrementReference",Return(Native(165,{Ref(0x01,1)})));
        Value zero;zero.kind=kind;host.Property(f,1,"WrongKind",zero);
        Failed(Vm::Execute(host,f,"Self"),Vm::Status::Invalid,host,"Non-Int increment reference");
    }
    for (const auto& expression : {Native(165),Native(165,{Int(1)}),Native(165,{Int(1),Int(2)})}) {
        TestHost host;auto f=Function("BadIncrementArguments",Return(expression));
        Failed(Vm::Execute(host,f,"Self"),Vm::Status::Invalid,host,"Wrong increment arity or non-reference argument");
    }
    {
        TestHost host;auto f=Function("IncrementRollback",Join({Native(165,{Ref(0x01,1)}),Native(4095),Return({0x0b})}));
        const auto property=host.Property(f,1,"Counter",Value::Integer(0));host.Store("Self",property,{Value::Integer(7)});
        const auto result=Vm::Execute(host,f,"Self");Failed(result,Vm::Status::Unsupported,host,"Failure after integer increment");
        Require(result.writes==1u && host.Load("Self",property).integer==7,"Failed call leaked a completed postincrement");
    }
    for (const std::size_t writes : {0u,1u}) {
        TestHost host;auto f=Function("IncrementBudget",Join({Native(165,{Ref(0x01,1)}),Native(163,{Ref(0x01,1)}),Return({0x0b})}));
        const auto property=host.Property(f,1,"Counter",Value::Integer(0));host.Store("Self",property,{Value::Integer(7)});
        Vm::Limits limits;limits.writes=writes;
        const auto result=Vm::Execute(host,f,"Self",{},limits);Failed(result,Vm::Status::Budget,host,"Integer increment write budget");
        Require(result.writes==writes && host.Load("Self",property).integer==7,"Integer increment ignored the shared write budget or rollback");
    }
}
void CastGraph(TestHost& host,const std::string& source="Fixture") {
    host.castClasses={
        {"Core.Object",{"","Object"}}, {"Core.Field",{"Core.Object","Field"}},
        {"Core.Struct",{"Core.Field","Struct"}}, {"Core.Class",{"Core.Struct","Class"}},
        {"Engine.Actor",{"Core.Object","Actor"}}, {"Engine.Inventory",{"Engine.Actor","Inventory"}},
        {"Engine.Ammo",{"Engine.Inventory","Ammo"}}, {"Fixture.Weapon",{"Engine.Inventory","Weapon"}},
        {"Fixture.Pistol",{"Fixture.Weapon","Pistol"}}, {"Other.Weapon",{"Engine.Actor","WEAPON"}}
    };
    host.castInstances={{"Map.Pistol0","Fixture.Pistol"},{"Map.Actor0","Engine.Actor"}};
    host.objects[{source,1}]="Engine.Inventory";
    host.objects[{source,2}]="Fixture.Pistol";
    host.objects[{source,3}]="Map.Pistol0";
    host.objects[{source,4}]="Engine.Ammo";
    host.objects[{source,5}]="Fixture.Weapon";
    host.objects[{source,6}]="Other.Weapon";
    host.objects[{source,7}]="Core.Class";
    host.objects[{source,0}]="";
}
void CastContracts() {
    const auto object=[](std::string identity) {return Value::Text(Vm::Kind::Object,std::move(identity));};
    for (const std::uint8_t opcode : {std::uint8_t{0x13},std::uint8_t{0x2e}}) {
        for (const Bytes& input : {Bytes{0x2a},Bytes{0x0b},Context({0x2a},Native(1000))}) {
            TestHost host;CastGraph(host);auto f=Function("CastNull",Return(Cast(opcode,1,input)));
            const auto r=Vm::Execute(host,f,"Self");Returned(r,"Cast None/Nothing/null context");
            Require(r.value.kind==Vm::Kind::Object && r.value.text.empty() && host.castClassResolutions==1u &&
                host.castCalls==1u && host.effects==0u,"Cast null conversion lost object kind or evaluated null context");
        }
    }
    struct Case {std::uint8_t opcode;std::int32_t target,input;const char* expected;};
    for (const auto& control : {
        Case{0x13,2,2,"Fixture.Pistol"},Case{0x13,1,2,"Fixture.Pistol"},Case{0x13,4,2,""},
        Case{0x13,1,3,""},Case{0x13,6,2,""},Case{0x2e,1,3,"Map.Pistol0"},
        Case{0x2e,2,3,"Map.Pistol0"},Case{0x2e,4,3,""},Case{0x2e,6,3,"Map.Pistol0"},
        Case{0x2e,1,2,""},Case{0x2e,7,2,"Fixture.Pistol"}}) {
        TestHost host;CastGraph(host);
        const auto r=Vm::Execute(host,Function("CastIdentity",Return(Cast(control.opcode,control.target,Ref(0x20,control.input)))),"Self");
        Returned(r,"Exact/inherited/class/instance/namespace cast");
        Require(r.value.kind==Vm::Kind::Object && r.value.text==control.expected && host.castCalls==1u,
            "MetaCast exact class ancestry or DynamicCast NameString ancestry changed");
    }
    for (const std::uint8_t opcode : {std::uint8_t{0x13},std::uint8_t{0x2e}}) {
        // Every non-object value is a type error, even scalar zero/false/None
        // names. Coerce or truth conversion must not invent a null object.
        for (const Bytes& input : {Int(0),Real(0),Bytes{0x28},Bytes{0x24,0},String(""),
            Ref(0x21,10),Bytes{0x22,0,0,0,0,0,0,0,0,0,0,0,0},
            Bytes{0x23,0,0,0,0,0,0,0,0,0,0,0,0},Native(1005)}) {
            TestHost host;CastGraph(host);host.names[{"Fixture",10}]="None";
            const auto f=Function("CastWrongKind",Return(Cast(opcode,1,input)));
            const auto r=Vm::Execute(host,f,"Self");Failed(r,Vm::Status::Unsupported,host,"Cast wrong child value kind");
            Require(host.castClassResolutions==1u && !host.castCalls && r.offset==1u && r.opcode==opcode,
                "Cast wrong kind reached host or reported its child instead of the cast");
        }
        for (const std::int32_t reference : {0,99,10,11,12,3,std::numeric_limits<std::int32_t>::min()}) {
            TestHost host;CastGraph(host);
            host.objects[{"Fixture",10}]="Fixture.Struct";
            host.objects[{"Fixture",11}]="Fixture.Actor.State";
            host.objects[{"Fixture",12}]="Fixture.Actor.Property";
            const auto f=Function("CastWrongTarget",Return(Cast(opcode,reference,Native(1000))));
            const auto r=Vm::Execute(host,f,"Self");Failed(r,Vm::Status::Invalid,host,"Cast target not an actual class");
            Require(!host.castCalls && host.nativeArguments.empty() && host.effects==0u && r.offset==1u && r.opcode==opcode,
                "Cast child ran before typed target resolution or lost target diagnostic");
        }
        // Target loading precedes even parsing its malformed child.
        {
            TestHost host;CastGraph(host);
            const auto r=Vm::Execute(host,Function("CastTargetBeforeChild",Return(Cast(opcode,99,{0x03}))),"Self");
            Failed(r,Vm::Status::Invalid,host,"Bad target before malformed child");
            Require(r.offset==1u && r.opcode==opcode && host.castClassResolutions==1u,
                "Malformed child was parsed before the target Class lookup");
        }
        for (std::size_t size=0u;size<5u;++size) {
            TestHost host;CastGraph(host);auto expression=Ref(opcode,1);expression.resize(size);
            const auto r=Vm::Execute(host,Function("TruncatedCastTarget",Return(expression)),"Self");
            Failed(r,Vm::Status::Invalid,host,"Truncated fixed32 cast target");
            Require(!host.castClassResolutions && !host.castCalls,"Truncated target entered host class lookup");
        }
        {
            TestHost host;CastGraph(host);
            const auto r=Vm::Execute(host,Function("TruncatedCastChild",Return(Ref(opcode,1))),"Self");
            Failed(r,Vm::Status::Invalid,host,"Missing cast child");
            Require(host.castClassResolutions==1u && !host.castCalls,"Missing child was not parsed after target resolution");
        }
        {
            TestHost host;CastGraph(host);Bytes code{0x07,0,0,0x28};
            const auto expression=Cast(opcode,99,Native(1000));code.insert(code.end(),expression.begin(),expression.end());
            PatchU16(code,1u,code.size());const auto returned=Return({0x26});code.insert(code.end(),returned.begin(),returned.end());
            const auto r=Vm::Execute(host,Function("UntakenInvalidCastTarget",code),"Self");
            Failed(r,Vm::Status::Invalid,host,"Untaken branch still decodes typed cast target");
            Require(host.effects==0u && host.nativeArguments.empty(),"Untaken invalid cast target performed runtime effects");
        }
        {
            TestHost host;CastGraph(host);auto f=Function("CastNonVariable",Let(Cast(opcode,1,Ref(0x01,8)),{0x2a}));
            const auto property=host.Property(f,8,"Input",object(""));host.Store("Self",property,{object(opcode==0x13 ? "Fixture.Pistol" : "Map.Pistol0")});
            const auto r=Vm::Execute(host,f,"Self");Failed(r,Vm::Status::Invalid,host,"Cast result is not assignable");
            Require(!r.writes && host.Load("Self",property).text==(opcode==0x13 ? "Fixture.Pistol" : "Map.Pistol0"),
                "Cast preserved an assignable alias to its operand");
        }
    }
    {
        TestHost host;CastGraph(host);auto f=Function("CastSnapshot",Return(Native(1003,{
            Cast(0x13,1,Ref(0x01,8)),Let(Ref(0x01,8),Ref(0x20,4))})));
        const auto property=host.Property(f,8,"Input",object(""));host.Store("Self",property,{object("Fixture.Pistol")});
        const auto r=Vm::Execute(host,f,"Self");Returned(r,"Detached cast snapshot before later argument write");
        Require(r.value.text=="Fixture.Pistol" && host.Load("Self",property).text=="Engine.Ammo" &&
            host.nativeArguments.back()[0].text=="Fixture.Pistol","Cast result changed when its original property was overwritten");
    }
    {
        TestHost host;CastGraph(host,"Caller");CastGraph(host,"Callee");
        host.objects[{"Caller",1}]="Engine.Ammo";host.objects[{"Caller",2}]="Engine.Ammo";
        auto callee=Function("CastSource",Return(Cast(0x13,1,Ref(0x20,2))),"Callee");
        auto caller=Function("CastSourceCaller",Return(Call(0x1c,-17)),"Caller");
        host.finals[{caller.source,-17}]=std::make_shared<Vm::Function>(callee);
        const auto r=Vm::Execute(host,caller,"Self");Returned(r,"Nested function-local cast operand provenance");
        Require(r.value.text=="Fixture.Pistol" && host.observations.front()=="cast-target:Callee:1",
            "Nested cast used caller's target or source object table");
    }
    {
        TestHost host;CastGraph(host);auto f=Function("NestedCast",Return(Cast(0x2e,7,Cast(0x13,1,Ref(0x20,2)))));
        const auto r=Vm::Execute(host,f,"Self");Returned(r,"Nested detached casts");
        Require(r.value.text=="Fixture.Pistol" && host.castClassResolutions==2u && host.castCalls==2u &&
            host.observations==std::vector<std::string>{"cast-target:Fixture:7","cast-target:Fixture:1","metacast","dynamiccast"},
            "Nested cast decode/evaluation ordering differs from pinned expression tree");
    }
    for (const bool failAfterCast : {false,true}) {
        TestHost host;CastGraph(host,"Caller");CastGraph(host,"Callee");
        auto callee=Function("CastChildEffects",Join({Native(1000),Let(Ref(0x01,8),Int(9)),Return(Ref(0x20,2))}),"Callee");
        const auto property=host.Property(callee,8,"Counter",Value::Integer(0));host.Store("Self",property,{Value::Integer(4)});
        auto caller=Function("CastEffects",failAfterCast ? Join({Cast(0x13,1,Call(0x1c,17)),Native(4095),Return({0x0b})}) :
            Return(Cast(0x13,1,Call(0x1c,17))),"Caller");
        host.finals[{caller.source,17}]=std::make_shared<Vm::Function>(callee);
        const auto r=Vm::Execute(host,caller,"Self");
        if (failAfterCast) {
            Failed(r,Vm::Status::Unsupported,host,"Unsupported required action after cast child effects");
            Require(host.effects==0u && host.rng==12345u && host.Load("Self",property).integer==4,
                "Later unsupported action leaked cast child's effects/RNG/property writes");
        } else {
            Returned(r,"Cast evaluates effectful nested child exactly once");
            Require(r.value.text=="Fixture.Pistol" && host.effects==1u && host.Load("Self",property).integer==9,
                "Cast duplicated or skipped effectful child");
        }
        Require(r.writes==1u && host.castCalls==1u &&
            std::count(host.observations.begin(),host.observations.end(),"effect")==1,
            "Cast child was evaluated more than once");
    }
    for (const Value& badResult : {Value{},Value::Integer(0),object("Map.Actor0")}) {
        TestHost host;CastGraph(host);host.castResultOverride=badResult;
        const auto r=Vm::Execute(host,Function("CastHostBadResult",Return(Cast(0x13,1,Ref(0x20,2)))),"Self");
        Failed(r,Vm::Status::Invalid,host,"Cast host changed value kind or object identity");
        Require(r.offset==1u && r.opcode==0x13u,"Host result failure lost cast diagnostic");
    }
    {
        TestHost host;CastGraph(host);host.castResultOverride=object("FIXTURE.PISTOL");
        const auto r=Vm::Execute(host,Function("CastCanonicalCase",Return(Cast(0x13,1,Ref(0x20,2)))),"Self");
        Returned(r,"Host canonical spelling preserves case-insensitive object identity");
    }
    for (const std::uint8_t opcode : {std::uint8_t{0x13},std::uint8_t{0x2e}}) {
        for (const unsigned malformed : {0u,1u,2u,3u}) {
            TestHost host;CastGraph(host);std::string source="Fixture.Broken";
            if (malformed==0u) source="Map.Missing";
            if (malformed==1u) host.castClasses[source]={"Fixture.MissingBase","Broken"};
            if (malformed==2u) {
                host.castClasses[source]={"Fixture.Cycle","Broken"};
                host.castClasses["Fixture.Cycle"]={source,"Cycle"};
            }
            if (malformed==3u) {
                source="Fixture.Deep0";
                for (std::size_t i=0u;i<130u;++i)
                    host.castClasses["Fixture.Deep"+std::to_string(i)]={i==129u ? "Core.Object" : "Fixture.Deep"+std::to_string(i+1u),"Deep"+std::to_string(i)};
            }
            if (opcode==0x2eu && malformed!=0u) {host.castInstances["Map.Broken0"]=source;source="Map.Broken0";}
            host.objects[{"Fixture",20}]=source;
            const auto r=Vm::Execute(host,Function("CastMalformedSource",Return(Cast(opcode,1,Ref(0x20,20)))),"Self");
            Failed(r,Vm::Status::Unsupported,host,"Missing/cyclic/deep actual cast object ancestry");
            Require(r.offset==1u && r.opcode==opcode,"Malformed cast object ancestry lost cast diagnostic");
        }
        for (const std::string& identity : {std::string{},std::string("Fixture.Bad\0Tail",16u)}) {
            TestHost host;CastGraph(host);host.castClasses[identity]={"Core.Object","Bad"};host.objects[{"Fixture",10}]=identity;
            const auto r=Vm::Execute(host,Function("CastMalformedTargetIdentity",Return(Cast(opcode,10,Native(1000)))),"Self");
            Failed(r,Vm::Status::Invalid,host,"Empty/NUL cast target identity");
            Require(!host.castCalls && host.nativeArguments.empty(),"Malformed cast target identity evaluated child");
        }
    }
    // Structural analysis validates normalized references but not UClass kinds.
    {
        TestHost host;CastGraph(host);host.objects[{"Fixture",10}]="Fixture.Actor.State";
        const auto f=Function("CastAnalysis",Return(Cast(0x13,10,Cast(0x2e,3,{0x0b}))));
        const auto layout=Vm::AnalyzeProgram(host,f);
        Require(layout.statementOffsets==std::vector<std::size_t>{0u} && !host.castClassResolutions && !host.castCalls &&
            !host.beginCount && host.observations.empty(),"AnalyzeProgram performed typed cast lookup or execution");
    }
    // Hostile class names are rejected before the child. Aggregate path memory
    // uses the Machine budget across repeated and nested function parses.
    for (const unsigned budget : {0u,1u,2u,3u,4u,5u}) {
        TestHost host;CastGraph(host);auto f=Function("CastBudget",Return(Cast(0x13,1,Ref(0x20,2))));
        Vm::Limits limits;
        if (budget==0u) limits.sourceBytes=f.bytecode.size()-1u;
        if (budget==1u) limits.nodes=2u;
        if (budget==2u) limits.expressionDepth=2u;
        if (budget==3u) limits.instructions=1u;
        if (budget==4u) limits.stringBytes=1u;
        if (budget==5u) limits.retainedBytes=1u;
        const auto r=Vm::Execute(host,f,"Self",{},limits);Failed(r,Vm::Status::Budget,host,"Bounded cast execution");
        Require(!host.castCalls,"Cast exceeded budget before required evaluation");
    }
    {
        TestHost host;CastGraph(host);auto f=Function("CastAggregatePaths",Join({Cast(0x13,1,{0x2a}),Return(Cast(0x13,1,{0x2a}))}));
        Vm::Limits limits;limits.retainedBytes=std::string("Engine.Inventory").capacity();
        const auto r=Vm::Execute(host,f,"Self",{},limits);Failed(r,Vm::Status::Budget,host,"Aggregate parsed cast identity retention");
        Require(host.castClassResolutions==2u && !host.castCalls,"Cast path retention was limited per-string instead of cumulatively");
    }
    {
        TestHost host;CastGraph(host,"Caller");CastGraph(host,"Callee");
        auto leaf=Function("CastRetentionLeaf",Return(Cast(0x13,1,{0x2a})),"Callee");
        auto caller=Function("CastRetentionCaller",Join({Cast(0x13,1,{0x2a}),Return(Call(0x1c,17))}),"Caller");
        host.finals[{caller.source,17}]=std::make_shared<Vm::Function>(leaf);
        Vm::Limits limits;limits.retainedBytes=sizeof(Value);
        // SSO-independent paths larger than half a Value guarantee that two
        // simultaneous parses exceed this budget, while one null value fits.
        const auto identity=std::string(sizeof(Value)/2u+1u,'x');
        host.castClasses[identity]={"Core.Object",identity};
        host.objects[{"Caller",1}]=identity;host.objects[{"Callee",1}]=identity;
        const auto r=Vm::Execute(host,caller,"Self",{},limits);Failed(r,Vm::Status::Budget,host,"Nested cast path retention shares caller budget");
        Require(host.castClassResolutions==2u && host.castCalls==1u,"Nested function reset retained cast identity budget");
    }
    {
        TestHost host;CastGraph(host);auto f=Function("CastChildWriteBudget",Return(Cast(0x13,1,Let(Ref(0x01,8),Ref(0x20,2)))));
        const auto property=host.Property(f,8,"Input",object(""));host.Store("Self",property,{object("Engine.Ammo")});
        Vm::Limits limits;limits.writes=0u;
        const auto r=Vm::Execute(host,f,"Self",{},limits);Failed(r,Vm::Status::Budget,host,"Cast child shares write budget");
        Require(!host.castCalls && host.Load("Self",property).text=="Engine.Ammo","Cast child bypassed write budget or rollback");
    }
}
void ExecutionContracts() {
    ScalarAndLocalContracts();ParametersAndReturns();AssignmentArraysAndStructs();LazyAndContextContracts();
    OptionalAndReferenceTables();ControlAndFailureContracts();BudgetAndNestedRollback();IntegerIncrementContracts();CastContracts();
}
void ProgramAndEligibilityContracts() {
    TestHost host;
    host.names[{"Fixture",0}] = "None";
    host.names[{"Fixture",1}] = "Begin";
    host.names[{"Fixture",2}] = "BEGIN";
    auto labels = Bytes{0x0c}; U32(labels,1u); U32(labels,0u); U32(labels,2u); U32(labels,1u);
    U32(labels,0u); U32(labels,0xffffffffu);
    auto f = Function("StateBlock", Join({{0x0b,0x08}, labels}));
    const auto layout = Vm::AnalyzeProgram(host,f);
    Require(layout.statementOffsets == std::vector<std::size_t>{0u,1u,2u} && layout.terminalLabelTable &&
        layout.labels.size() == 2u && layout.labels[0].name == "Begin" && layout.labels[1].name == "BEGIN" &&
        layout.labels[1].offset == 1u, "State layout lost terminal labels/order/duplicate spellings");
    Require(!host.beginCount && !host.commitCount && !host.rollbackCount && host.observations.empty() &&
        host.nativeArguments.empty(), "Read-only program inspection performed effects/transactions");
    const auto earlier = Vm::AnalyzeProgram(host,Function("Earlier",Join({labels,{0x08}})));
    Require(!earlier.terminalLabelTable && earlier.labels.empty(), "Nonterminal table supplied executable labels");
    const auto nested = Vm::AnalyzeProgram(host,Function("Nested",Join({{0x04},labels})));
    Require(!nested.terminalLabelTable && nested.labels.empty(), "Nested table supplied state labels");
    const auto reject = [&](const Vm::Function& fn, const Vm::Limits& limits = {}) {
        bool rejected{};
        try { Vm::AnalyzeProgram(host,fn,limits); } catch (const std::exception&) { rejected = true; }
        Require(rejected, "Malformed/unbounded state layout accepted"); ++rejections;
    };
    auto bad = f; bad.bytecode[7u] = 3u; reject(bad); // Operand inside table, not a statement.
    bad = f; for (std::size_t i=0;i<4u;++i) bad.bytecode[7u+i] = 0xffu; reject(bad);
    bad = f; bad.bytecode[3u] = 99u; reject(bad);
    for (std::size_t size=3u;size<f.bytecode.size();++size) { bad=f; bad.bytecode.resize(size); reject(bad); }
    reject(Function("BadObject",Ref(0x20,99)));
    reject(Function("BadOpcode",{0x03}));
    auto deep=Bytes(64u,0x2d); deep.push_back(0x0b); reject(Function("Deep",deep));
    for (const unsigned budget : {0u,1u,2u,3u}) {
        auto limits=Vm::Limits{};
        if (budget==0u) limits.sourceBytes=1u;
        if (budget==1u) limits.nodes=1u;
        if (budget==2u) limits.retainedBytes=1u;
        if (budget==3u) limits.stringBytes=1u;
        reject(f,limits);
    }
    auto blocked = Function("Disabled", {0x03}); blocked.flags = 0x8u;
    host.disabledFunctions.insert(blocked.path);
    Value external=Value::Integer(3);
    auto alias=std::make_shared<Vm::Reference>(); alias->zero=Value::Integer(0);
    alias->read=[]() -> Value { throw std::runtime_error("Disabled argument was read"); };
    alias->write=[&](const Value& value) { external=value; };
    auto result=Vm::Execute(host,blocked,"Self",{{{},alias}});
    Returned(result,"Disabled function returns Nothing before latent/argument/parse work");
    Require(result.value.kind==Vm::Kind::Nothing && result.instructions==0u && result.writes==0u && external.integer==3,
        "Disabled function touched arguments/out aliases or executed");
    blocked.flags=0x400u; blocked.nativeIndex=1000u;
    result=Vm::Execute(host,blocked,"Self",{{{},alias}}); Returned(result,"Disabled native declaration");
    Require(host.effects==0u && host.nativeArguments.empty(), "Ineligible native declaration executed");
    host.named[{"Self",Vm::CallKind::Virtual,"Blocked"}]=std::make_shared<Vm::Function>(blocked);
    host.names[{"Fixture",3}]= "Blocked";
    result=Vm::Execute(host,Function("NestedCall",Return(Call(0x1b,3))),"Self");
    Returned(result,"Nested ineligible callee");
    Require(result.value.kind==Vm::Kind::Nothing && host.effects==0u, "Nested callback bypassed CanCall");
}
}

int main() {
    try {
        NormalizedSerializedContracts();
        ExecutionContracts();
        ProgramAndEligibilityContracts();
        std::cout<<"Normalized original-style serialization and bounded script execution: "<<checks<<" checks and "<<rejections<<" rejection controls passed.\n";
        return 0;
    }catch(const std::exception& error) {std::cerr<<error.what()<<"\n";return 1;}
}
