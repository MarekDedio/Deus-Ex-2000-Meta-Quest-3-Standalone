#include "Precomp.h"
#include "surreal_portable_package_tables.h"
#include "quest_portable_vm.h"
#include "quest_state_frame.h"
#include "Math/coords.h"

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
    // Switch carries an ignored value-size byte. Case links remain absolute
    // logical offsets despite compact serialized Name references, and 0xffff
    // is a childless default label rather than a branch address.
    raw={0x05,255,0x21};Index(raw,3);raw.push_back(0x0a);U16(raw,21);
    raw.push_back(0x21);Index(raw,3);raw.push_back(0x04);raw.push_back(0x1d);U32(raw,1);
    raw.push_back(0x0a);U16(raw,0xffff);raw.push_back(0x04);raw.push_back(0x1d);U32(raw,0);
    normalized={0x05,255,0x21};U32(normalized,3);normalized.push_back(0x0a);U16(normalized,21);
    normalized.push_back(0x21);U32(normalized,3);normalized.push_back(0x04);normalized.push_back(0x1d);U32(normalized,1);
    normalized.push_back(0x0a);U16(normalized,0xffff);normalized.push_back(0x04);normalized.push_back(0x1d);U32(normalized,0);
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
Bytes Unicode(const std::u16string& value) {
    Bytes b{0x34};for(const auto unit:value) U16(b,unit);U16(b,0u);return b;
}
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
Bytes Return(const Bytes& value={0x0b}) {return Join({{0x04},value});}
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
    std::map<std::tuple<std::string,std::string,bool>,std::shared_ptr<const Vm::Function>> events;
    std::map<std::string,Vm::Function> preparedFunctions;
    std::vector<std::string> preparations;
    std::function<Vm::Evaluation(const std::string&,const std::vector<Vm::Evaluation>&,
        const Vm::Function*,Vm::Execution&)> executionNative;
    std::map<Slot,std::vector<Value>> storage,saved;
    struct CastClass {std::string parent,name;};
    std::map<std::string,CastClass> castClasses;
    std::map<std::string,std::string> castInstances;
    std::optional<Value> castResultOverride;
    std::size_t castClassResolutions{},castCalls{};
    std::vector<std::string> observations;
    std::vector<std::vector<Value>> nativeArguments;
    std::vector<std::string> nativeReceivers,nativeDeclarations;
    std::map<std::string,std::string> objectDisplayPaths;
    std::vector<std::string> objectStringLookups;
    std::shared_ptr<Vm::Reference> retainedNativeReference,retainedNativeElement;
    std::size_t beginCount{},commitCount{},rollbackCount{},effects{},savedEffects{};
    std::uint32_t rng{12345},savedRng{};
    bool active{};
    bool rejectBegin{},rejectCommit{},rejectEvent{};
    std::vector<Value> iteratorValues{Value::Integer(1),Value::Integer(2),Value::Integer(3)};
    std::size_t iteratorCreates{},iteratorDestroys{},iteratorNextCalls{},iteratorBytes{128u};
    std::optional<QuestVr::StateObject> state,savedState;
    std::map<std::string,Vm::Function> statePrograms;
    std::uint64_t stateRevision{},savedStateRevision{};
    std::set<std::string> disabledFunctions;
    bool CanCall(const Vm::Function& fn, const std::string&) override {
        return !disabledFunctions.contains(fn.path);
    }
    std::shared_ptr<const Vm::Function> PrepareFunction(const Vm::Function& identity,const std::string& receiver) override {
        preparations.push_back(receiver+":"+identity.path);
        const auto found=preparedFunctions.find(identity.path);
        return std::make_shared<Vm::Function>(found==preparedFunctions.end() ? identity : found->second);
    }
    std::shared_ptr<const Vm::Function> ResolveEvent(const std::string& receiver,const std::string& event,bool enumDispatch) override {
        observations.push_back("event:"+receiver+":"+event+(enumDispatch ? ":enum" : ":named"));
        if(rejectEvent) throw std::runtime_error("Fixture rejected event resolution");
        const auto found=events.find({receiver,event,enumDispatch});
        return found==events.end() ? nullptr : found->second;
    }
    void Begin() override {
        if(active) throw std::runtime_error("Nested host transaction");
        if(rejectBegin) throw std::runtime_error("Fixture rejected begin");
        active=true;saved=storage;savedEffects=effects;savedRng=rng;
        savedState=state;savedStateRevision=stateRevision;++beginCount;
    }
    void Commit() override {
        if(!active) throw std::runtime_error("Commit outside transaction");
        if(rejectCommit) throw std::runtime_error("Fixture rejected commit");
        active=false;++commitCount;
    }
    void Rollback() noexcept override {
        storage.swap(saved);effects=savedEffects;rng=savedRng;
        state.swap(savedState);stateRevision=savedStateRevision;active=false;++rollbackCount;
    }
    const QuestVr::StateObject* ReadState(const std::string&) override {return state ? &*state : nullptr;}
    QuestVr::StateObject* MutableState(const std::string&) override {return state ? &*state : nullptr;}
    std::uint64_t StateLocalRevision(const std::string&) override {return stateRevision;}
    std::shared_ptr<const Vm::Function> StateProgram(const std::string&,const std::string& path) override {
        return std::make_shared<Vm::Function>(statePrograms.at(path));
    }
    std::unique_ptr<Vm::Iterator> CreateIterator(const std::uint16_t index,const std::string& receiver,
        const std::vector<Vm::Evaluation>& arguments,const Vm::Function* declaration) override {
        if(index!=1040u || arguments.empty() || !arguments[0].reference)
            throw std::runtime_error("Fixture invalid iterator factory/out binding");
        ++iteratorCreates;observations.push_back("factory:"+receiver);
        std::vector<Value> values;for(const auto& arg:arguments) values.push_back(arg.Load());
        nativeArguments.push_back(values);nativeReceivers.push_back(receiver);
        nativeDeclarations.push_back(declaration ? declaration->path : "");
        retainedNativeReference=arguments[0].reference;
        struct Cursor final:Vm::Iterator {
            TestHost& owner;
            std::shared_ptr<Vm::Reference> output;
            std::vector<Value> values;
            std::size_t cursor{};
            Cursor(TestHost& host,std::shared_ptr<Vm::Reference> target,std::vector<Value> items)
                :owner(host),output(std::move(target)),values(std::move(items)) {}
            ~Cursor() override {++owner.iteratorDestroys;}
            bool Next() override {
                ++owner.iteratorNextCalls;
                const bool available=cursor<values.size();
                output->write(available ? values[cursor++] : output->zero);
                return available;
            }
            std::size_t RetainedBytes() const override {return owner.iteratorBytes+values.size()*sizeof(Value);}
        };
        return std::make_unique<Cursor>(*this,arguments[0].reference,iteratorValues);
    }
    Vm::Property ResolveProperty(const Vm::Function& f,std::int32_t index) override {return properties.at({f.source,index});}
    std::string ResolveName(const Vm::Function& f,std::int32_t index) override {return names.at({f.source,index});}
    std::string ResolveObject(const Vm::Function& f,std::int32_t index) override {return objects.at({f.source,index});}
    std::string ObjectString(const std::string& identity) override {
        objectStringLookups.push_back(identity);
        const auto found=objectDisplayPaths.find(identity);
        if(found==objectDisplayPaths.end()) throw std::runtime_error("Fixture object has no actual path identity");
        return found->second;
    }
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
        if(index==1060u) {
            if(arguments.size()<2u || arguments.size()>3u || !arguments[0].reference)
                throw std::runtime_error("Fixture snapshot mutation needs OUT target/value");
            arguments[0].reference->write(values[1]);
            return {values.size()==3u ? values[2] : values[1],{}};
        }
        if(index==1043u) {
            if(!state || !state->frame) throw std::runtime_error("Fixture lacks a state frame");
            if(values.empty()) ++stateRevision;
            else if(values[0].integer==1) state->frame->codePath="Fixture.OtherState";
            else if(values[0].integer==2) state->frame->latent=QuestVr::StateLatent::Stop;
            else state->frame->codePath.clear();
            return {};
        }
        if(index==0 && declaration) {
            return {Value::Integer(values.empty() ? -1 : static_cast<std::int32_t>(values.front().kind)),{}};
        }
        throw std::runtime_error("Fixture unknown native "+std::to_string(index));
    }
    Vm::Evaluation NativeWithExecution(std::uint16_t index,const std::string& receiver,
        const std::vector<Vm::Evaluation>& arguments,const Vm::Function* declaration,Vm::Execution& execution) override {
        // 1008 belongs only to this generated fixture. Native zero is keyed by
        // its declaration; individual tests provide the host-side observation.
        if((index==1008u || index==0u) && executionNative) {
            std::vector<Value> values;for(const auto& argument:arguments) values.push_back(argument.Load());
            nativeArguments.push_back(std::move(values));nativeReceivers.push_back(receiver);
            nativeDeclarations.push_back(declaration ? declaration->path : "");
            return executionNative(receiver,arguments,declaration,execution);
        }
        return Native(index,receiver,arguments,declaration);
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
void Append(Bytes& destination,const Bytes& source) {destination.insert(destination.end(),source.begin(),source.end());}
std::size_t BeginIterator(Bytes& code,const Bytes& factory) {
    code.push_back(0x2f);Append(code,factory);const auto target=code.size();U16(code,0u);return target;
}
void EndIterator(Bytes& code,const std::size_t target) {
    code.push_back(0x31);PatchU16(code,target,code.size());code.push_back(0x30);
}
Bytes Vector(const std::array<float,3>& value) {
    Bytes result{0x23};for(const auto component:value) Float(result,component);return result;
}
Bytes Rotation(const std::array<std::int32_t,3>& rotation);
Bytes Switch(const Bytes& selector,std::uint8_t valueSize=1u) {return Join({{0x05,valueSize},selector});}
std::size_t BeginCase(Bytes& code,const Bytes& label) {
    code.push_back(0x0a);const auto link=code.size();U16(code,0u);Append(code,label);return link;
}
void DefaultCase(Bytes& code) {code.push_back(0x0a);U16(code,0xffffu);}
std::size_t BeginJump(Bytes& code) {code.push_back(0x06);const auto target=code.size();U16(code,0u);return target;}
Bytes SingleCaseSwitch(const Bytes& selector,const Bytes& label,std::uint8_t valueSize=1u) {
    auto code=Switch(selector,valueSize);const auto next=BeginCase(code,label);Append(code,Return(Int(1)));
    PatchU16(code,next,code.size());DefaultCase(code);Append(code,Return(Int(0)));return code;
}
void VectorNativeContracts() {
    for(const std::array<float,3> components : {std::array<float,3>{0.0f,-0.0f,0.0f},
        std::array<float,3>{3.0f,4.0f,-12.0f},std::array<float,3>{-0.125f,73.5f,1.0e-18f}}) {
        for(const float scale : {-7.5f,-0.0f,0.0f,0.125f,100.0f}) {
            TestHost host;const auto function=Function("VectorAssign",Return(Native(221u,{Ref(0x01,1),Real(scale)})));
            const auto property=host.Property(function,1,"Vector",Value::Vector({}));
            host.Store("Self",property,{Value::Vector(components)});
            const auto result=Vm::Execute(host,function,"Self");Returned(result,"Pinned Vector *= Float");
            vec3 expected(components[0],components[1],components[2]);expected*=scale;
            const std::array<float,3> values{expected.x,expected.y,expected.z};
            Require(result.value.kind==Vm::Kind::Vector && result.writes==1u &&
                Vm::Equal(host.Load("Self",property),result.value),"Vector assignment return/write differs");
            for(std::size_t i=0;i<3u;++i)
                Require(std::bit_cast<std::uint32_t>(result.value.vector[i])==std::bit_cast<std::uint32_t>(values[i]),
                    "Vector assignment changed pinned component multiplication");
        }
        TestHost host;const auto result=Vm::Execute(host,Function("VSize",Return(Native(225u,{Vector(components)}))),"Self");
        Returned(result,"Pinned VSize");
        Require(result.value.kind==Vm::Kind::Float && std::bit_cast<std::uint32_t>(result.value.floating)==
            std::bit_cast<std::uint32_t>(length(vec3(components[0],components[1],components[2]))),
            "VSize changed pinned binary32 dot/sqrt evaluation");
    }
    {
        TestHost host;const auto result=Vm::Execute(host,Function("NothingVSize",Return(Native(225u,{{0x0b}}))),"Self");
        Returned(result,"VSize Nothing zero");Require(result.value.floating==0.0f,"VSize Nothing did not become zero");
    }
    for(const Bytes& argument : {Int(3),String("vector"),Rotation({1,2,3})}) {
        TestHost host;const auto result=Vm::Execute(host,Function("WrongVSize",Return(Native(225u,{argument}))),"Self");
        Failed(result,Vm::Status::Unsupported,host,"VSize wrong concrete type");
    }
    for(const bool overflow : {false,true}) {
        TestHost host;auto function=Function("InvalidVectorAssign",{});
        const auto property=host.Property(function,1,"Vector",Value::Vector({}));
        const auto before=Value::Vector({std::numeric_limits<float>::max(),1.0f,2.0f});host.Store("Self",property,{before});
        function.bytecode=Return(Native(221u,{overflow ? Ref(0x01,1) : Vector({1,2,3}),Real(2.0f)}));
        const auto result=Vm::Execute(host,function,"Self");Failed(result,Vm::Status::Invalid,host,"Unsafe vector assignment");
        Require(Vm::Equal(host.Load("Self",property),before),"Invalid vector assignment altered its target");
    }
    {
        TestHost host;const auto result=Vm::Execute(host,Function("OverflowVSize",Return(Native(225u,
            {Vector({std::numeric_limits<float>::max(),0,0})}))),"Self");
        Failed(result,Vm::Status::Invalid,host,"VSize binary32 overflow is refused");
    }
    {
        TestHost host;auto function=Function("VectorRollback",{});
        const auto property=host.Property(function,1,"Vector",Value::Vector({}));
        const auto before=Value::Vector({1,2,3});host.Store("Self",property,{before});
        function.bytecode=Join({Native(221u,{Ref(0x01,1),Real(100.0f)}),Native(4095u),Return()});
        const auto result=Vm::Execute(host,function,"Self");Failed(result,Vm::Status::Unsupported,host,"Vector assignment nested rollback");
        Require(Vm::Equal(host.Load("Self",property),before),"Vector assignment survived later failure");
    }
}
void StartupPrimitiveContracts() {
    const auto initialChecks=checks, initialRejections=rejections;
    const auto evaluate=[](const Bytes& expression,const std::string& context,const Vm::Limits& limits=Vm::Limits{}) {
        TestHost host;const auto result=Vm::Execute(host,Function("StartupPrimitive",Return(expression)),"Self",{},limits);
        Returned(result,context);Require(host.commitCount==1u && host.rollbackCount==0u && result.writes==0u,
            context+": pure primitive changed transactional storage");return result.value;
    };
    const auto expectText=[&](const Bytes& expression,const std::string& text,const std::string& context,
        const Vm::Limits& limits=Vm::Limits{}) {
        const auto value=evaluate(expression,context,limits);
        Require(value.kind==Vm::Kind::String && value.text==text,context+": code-unit/string mismatch");
    };
    const auto expectInt=[&](const Bytes& expression,std::int32_t number,const std::string& context) {
        const auto value=evaluate(expression,context);
        Require(value.kind==Vm::Kind::Int && value.integer==number,context+": int mismatch");
    };
    const std::string face="\xf0\x9f\x98\x80", high="\xed\xa0\xbd", low="\xed\xb8\x80", lone="\xed\xa0\x80";
    const std::u16string units{u'A',0xd83d,0xde00,0x00e9,u'Z',0xd800};
    const auto unicode=Unicode(units);
    expectText(unicode,"A"+face+"\xc3\xa9"+"Z"+lone,"UnicodeConst preserves pairs/lone units");
    expectText(Unicode({}),"","Empty UnicodeConst");
    expectInt(Native(125u,{unicode}),6,"Len uses UTF16 units not UTF8 bytes/scalars");
    expectInt(Native(126u,{unicode,Unicode(std::u16string{0xd83d,0xde00})}),1,"InStr UTF16 pair index");
    expectInt(Native(126u,{unicode,Unicode(std::u16string{0xd83d})}),1,"InStr can match isolated high unit");
    expectInt(Native(126u,{unicode,Unicode(u"Z")}),4,"InStr offsets after non-BMP pair");
    expectText(Native(127u,{unicode,Int(1),Int(1)}),high,"Mid preserves split high surrogate");
    expectText(Native(127u,{unicode,Int(2),Int(1)}),low,"Mid preserves split low surrogate");
    expectText(Native(128u,{unicode,Int(2)}),"A"+high,"Left can split a surrogate pair");
    expectText(Native(234u,{unicode,Int(4)}),low+"\xc3\xa9"+"Z"+lone,"Right indexes code units");
    expectText(String(std::string("\x80\xe9\xff",3)),"\xc2\x80\xc3\xa9\xc3\xbf","ANSI constants zero-extend high bytes");
    expectInt(Native(125u,{String(std::string("\x80\xe9\xff",3))}),3,"ANSI constant wide-unit count");
    {
        TestHost host;auto function=Function("LongAnsiAdvance",Join({Let(Ref(0x01,1),String(std::string(10000u,'x'))),Return(Int(37))}));
        const auto p=host.Property(function,1,"Text",Value::Text(Vm::Kind::String,{}));host.Store("Self",p,{p.zero});
        const auto result=Vm::Execute(host,function,"Self");Returned(result,"ANSI truncation consumes complete literal");
        Require(result.value.integer==37 && host.Load("Self",p).text==std::string(1023u,'x') && result.writes==1u,
            "ANSI 1023-unit conversion cap altered bytecode Code advance");
    }
    {
        auto limits=Vm::Limits{};limits.stringBytes=160000u;
        expectText(Native(127u,{Unicode(std::u16string(70000u,u'x')),Int(0)}),std::string(65535u,'x'),
            "Mid default count is65535 not remaining string length",limits);
    }
    for(const auto& [start,count,expected] : std::vector<std::tuple<std::int32_t,std::int32_t,std::string>>{
        {-1,2,""},{1,-1,""},{1,-2,"bc"},{0,-1,"abc"},{5,2,""},{1,100,"bc"},{0,0,""},
        {std::numeric_limits<std::int32_t>::min(),std::numeric_limits<std::int32_t>::min(),""},
        {1,std::numeric_limits<std::int32_t>::max(),"bc"}})
        expectText(Native(127u,{String("abc"),Int(start),Int(count)}),expected,"Mid original unsigned wrapping clamp");
    expectText(Native(127u,{String("abc"),Int(1),{0x0b}}),"bc","Mid explicit Nothing means omitted count");
    for(const auto index : {128u,234u}) {
        expectText(Native(index,{String("abc"),Int(-1)}),"","Left/Right negative count");
        expectText(Native(index,{String("abc"),Int(0)}),"","Left/Right zero count");
        expectText(Native(index,{String("abc"),Int(100)}),"abc","Left/Right excessive count");
        expectText(Native(index,{String(""),Int(2)}),"","Left/Right empty source");
    }
    expectInt(Native(126u,{String("abcabc"),String("bc")}),1,"InStr first match");
    expectInt(Native(126u,{String("Abc"),String("abc")}),-1,"InStr is case-sensitive");
    expectInt(Native(126u,{String("abc"),String("")}),0,"InStr empty needle");
    expectInt(Native(126u,{String(""),String("")}),0,"InStr two empty strings");
    expectText(Native(168u,{String(""),String("")})," ","@ retains space with empty operands");
    expectText(Native(168u,{String("a "),String(" b")}),"a   b","@ does not trim inputs");
    expectText(Native(235u,{Unicode(std::u16string{u'a',0x00e9,u'z',0xd800})}),"A\xc3\xa9"+std::string("Z")+lone,
        "Caps changes only ASCII lowercase code units");
    Require(evaluate(Native(124u,{String("AbC"),String("aBc")}),"~= ASCII fold").boolean,"~= lost ASCII folding");
    Require(!evaluate(Native(124u,{Unicode(u"\u00e9"),Unicode(u"\u00c9")}),"~= explicit portable C-locale policy").boolean,
        "~= fabricated a Unicode case expansion");
    {
        const auto stored=std::string("a\0bc",4);
        const auto inspect=[&](std::uint16_t index,const Bytes& second,const Value& expected) {
            TestHost host;auto function=Function("StoredNul",Return(Native(index,{Ref(0x01,1),second})));
            const auto p=host.Property(function,1,"Text",Value::Text(Vm::Kind::String,{}));host.Store("Self",p,{Value::Text(Vm::Kind::String,stored)});
            const auto result=Vm::Execute(host,function,"Self");Returned(result,"Native stored-NUL semantics");
            Require(Vm::Equal(result.value,expected) && host.Load("Self",p).text==stored,"Native changed/truncated storage input");
        };
        inspect(126u,String("bc"),Value::Integer(-1));
        inspect(124u,String("A"),Value::Bool(true));
        inspect(128u,Int(4),Value::Text(Vm::Kind::String,std::string("a\0\0\0",4)));
        inspect(234u,Int(4),Value::Text(Vm::Kind::String,"a"));
        inspect(234u,Int(3),Value::Text(Vm::Kind::String,{}));
        inspect(234u,Int(2),Value::Text(Vm::Kind::String,"bc"));
        for(const auto index : {112u,168u}) {
            TestHost host;auto function=Function("ConcatNul",Return(Native(index,{Ref(0x01,1),Ref(0x01,2)})));
            const auto a=host.Property(function,1,"A",Value::Text(Vm::Kind::String,{}));
            const auto b=host.Property(function,2,"B",a.zero);host.Store("Self",a,{Value::Text(Vm::Kind::String,std::string("a\0x",3))});
            host.Store("Self",b,{Value::Text(Vm::Kind::String,std::string("b\0y",3))});
            const auto result=Vm::Execute(host,function,"Self");Returned(result,"Concat retains A storage/B C-prefix");
            Require(result.value.text==std::string("a\0x",3)+(index==168u ? " " : "")+"b","Concat embedded-NUL operand asymmetry");
        }
        TestHost host;auto function=Function("MidNul",Return(Native(127u,{Ref(0x01,1),Int(1),Int(3)})));
        const auto p=host.Property(function,1,"Text",Value::Text(Vm::Kind::String,{}));host.Store("Self",p,{Value::Text(Vm::Kind::String,stored)});
        const auto result=Vm::Execute(host,function,"Self");Returned(result,"Mid zero-pads encountered source NUL");
        Require(result.value.text==std::string(3u,'\0'),"Mid copied hidden nonzero suffix after NUL");
    }
    // Each ordinary input is detached when evaluated, before later arguments
    // mutate its original reference. Both direct opcodes and resolved natives
    // retain the original first argument, while the genuine OUT helper aliases.
    for(const auto index : {112u,168u,124u,127u,128u,234u}) {
        for(const bool declared : {false,true}) {
            TestHost host;Bytes mutation;std::vector<Bytes> arguments{Ref(0x01,1)};Value expected;
            if(index==112u || index==168u) {
                mutation=Native(1060u,{Ref(0x01,1),String("XYZ"),String("suffix")});
                expected=Value::Text(Vm::Kind::String,index==112u ? "abcsuffix" : "abc suffix");
            } else if(index==124u) {
                mutation=Native(1060u,{Ref(0x01,1),String("XYZ"),String("ABC")});expected=Value::Bool(true);
            } else {
                mutation=Native(1060u,{Ref(0x01,1),String("XYZ"),Int(1)});
                expected=Value::Text(Vm::Kind::String,index==127u ? "bc" : index==128u ? "a" : "c");
            }
            arguments.push_back(mutation);Bytes expression;
            if(declared) {
                auto declaration=Function("OriginalInputNative",{},"NativeSource");declaration.flags=0x400u;declaration.nativeIndex=index;
                declaration.variables={{"Native.A","A",Value::Text(Vm::Kind::String,{}),0x80u,1u},
                    {"Native.B","B",index==112u || index==168u || index==124u ? Value::Text(Vm::Kind::String,{}) : Value::Integer(0),0x80u,1u}};
                host.finals[{"Fixture",9}]=std::make_shared<Vm::Function>(declaration);
                expression=Call(0x1c,9,{arguments[0],arguments[1]});
            } else expression=Native(index,{arguments[0],arguments[1]});
            auto function=Function("SnapshotInput",Return(expression));
            const auto p=host.Property(function,1,"Text",Value::Text(Vm::Kind::String,{}));host.Store("Self",p,{Value::Text(Vm::Kind::String,"abc")});
            const auto result=Vm::Execute(host,function,"Self");Returned(result,"Original pure input snapshot");
            Require(Vm::Equal(result.value,expected) && host.Load("Self",p).text=="XYZ" && result.writes==1u,
                "Later argument mutation re-read earlier native input / lost genuine OUT alias");
        }
    }
    for(const auto index : {249u,246u}) {
        TestHost host;Bytes expression=index==249u ? Native(index,{Ref(0x01,1),Native(1060u,{Ref(0x01,1),Int(1),Int(5)})}) :
            Native(index,{Ref(0x01,1),Native(1060u,{Ref(0x01,1),Real(100.0f),Real(0.0f)}),Real(10.0f)});
        auto function=Function("MathSnapshot",Return(expression));const auto p=host.Property(function,1,"Number",
            index==249u ? Value::Integer(0) : Value::Float(0));host.Store("Self",p,{index==249u ? Value::Integer(7) : Value::Float(5)});
        const auto result=Vm::Execute(host,function,"Self");Returned(result,"Math earlier-input snapshot");
        Require(index==249u ? result.value.integer==5 : result.value.floating==5.0f,"Math reloaded mutated first input");
    }
    expectInt(Native(249u,{Int(-7),Int(3)}),-7,"Min signed operands");
    expectInt(Native(249u,{Int(std::numeric_limits<std::int32_t>::min()),Int(std::numeric_limits<std::int32_t>::max())}),
        std::numeric_limits<std::int32_t>::min(),"Min signed32 boundaries");
    for(const auto& [v,a,b,expected] : std::vector<std::array<float,4>>{{5,0,10,5},{-1,0,10,0},{11,0,10,10},{5,10,0,10},{15,10,0,0}})
        Require(evaluate(Native(246u,{Real(v),Real(a),Real(b)}),"FClamp ordered/reversed bounds").floating==expected,
            "FClamp sorted bounds or selected wrong branch");
    const auto zero=evaluate(Native(246u,{Real(-0.0f),Real(0.0f),Real(0.0f)}),"FClamp signed zero retention");
    Require(std::bit_cast<std::uint32_t>(zero.floating)==0x80000000u,"FClamp discarded in-range negative zero");
    for(const auto number : {0.0f,-0.0f,0.25f,-0.5f,1.0f}) {
        const auto value=evaluate(Native(189u,{Real(number)}),"Tan float input/double operation/float result policy");
        const auto expected=static_cast<float>(std::tan(static_cast<double>(number)));
        Require(value.kind==Vm::Kind::Float && std::bit_cast<std::uint32_t>(value.floating)==std::bit_cast<std::uint32_t>(expected),
            "Tan changed argument units or portable float/double promotion policy");
    }
    for(const auto& expression : {Join({{0x56},{0x2a}}),Join({{0x56},{0x0b}})})
        expectText(expression,"None","ObjectToString null avoids host identity lookup");
    {
        TestHost host;host.objects[{"Fixture",1}]="opaque-id";host.objectDisplayPaths["opaque-id"]="OwnedMap.Package.Outer.Actor0";
        const auto result=Vm::Execute(host,Function("FullObjectPath",Return(Join({{0x56},Ref(0x20,1)}))),"Self");
        Returned(result,"ObjectToString validated host full path");
        Require(result.value.text=="OwnedMap.Package.Outer.Actor0" && host.objectStringLookups==std::vector<std::string>{"opaque-id"},
            "ObjectToString echoed operand spelling/lost nested Outer or evaluated twice");
    }
    const auto refuse=[&](const Bytes& expression,Vm::Status status,const std::string& context,const Vm::Limits& limits=Vm::Limits{}) {
        TestHost host;const auto result=Vm::Execute(host,Function("RejectedStartup",Join({Native(1000u),Return(expression)})),"Self",{},limits);
        Failed(result,status,host,context);Require(host.effects==0u && host.rng==12345u,context+": earlier effect/RNG escaped rollback");
    };
    for(const auto& expression : {Native(125u,{Int(1)}),Native(124u,{String("a"),Int(1)}),Native(189u,{String("1")}),
        Native(246u,{Real(1),String("0"),Real(2)}),Native(249u,{String("1"),Int(2)}),Join({{0x56},Int(1)})})
        refuse(expression,Vm::Status::Unsupported,"Startup concrete argument types refused");
    for(const auto& expression : {Native(125u,{}),Native(126u,{String("a")}),Native(127u,{String("a")}),
        Native(127u,{String("a"),Int(0),Int(1),Int(2)}),Native(168u,{String("a")}),Native(249u,{Int(1)})})
        refuse(expression,Vm::Status::Invalid,"Startup native arity refused");
    for(const auto nonfinite : {std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity()}) {
        refuse(Native(189u,{Real(nonfinite)}),Vm::Status::Invalid,"Tan nonfinite ingress policy");
        for(const auto& expression : {Native(246u,{Real(nonfinite),Real(0),Real(1)}),Native(246u,{Real(0),Real(nonfinite),Real(1)}),
            Native(246u,{Real(0),Real(0),Real(nonfinite)})})
            refuse(expression,Vm::Status::Invalid,"Clamp nonfinite ingress refused before x87 branch emulation");
    }
    for(const auto& malformed : {Bytes{0x34,0x41},Bytes{0x34,0x41,0},Bytes{0x1f,'a'}})
        refuse(malformed,Vm::Status::Invalid,"Truncated string constant refused");
    auto stringLimit=Vm::Limits{};stringLimit.stringBytes=3u;
    expectText(Unicode(u"ab"),"ab","Unicode ASCII length uses transport-byte budget",stringLimit);
    refuse(Unicode(u"\u00e9\u00e9"),Vm::Status::Budget,"Unicode expanded transport budget",stringLimit);
    refuse(String(std::string("\xff\xff",2)),Vm::Status::Budget,"ANSI expanded transport budget",stringLimit);
    refuse(Native(168u,{String("aa"),String("b")}),Vm::Status::Budget,"@ inserted space budget",stringLimit);
    {
        TestHost host;auto function=Function("CapsUnsafeNul",Join({Native(1000u),Return(Native(235u,{Ref(0x01,1)}))}));
        const auto p=host.Property(function,1,"Text",Value::Text(Vm::Kind::String,{}));host.Store("Self",p,{Value::Text(Vm::Kind::String,std::string("a\0b",3))});
        const auto result=Vm::Execute(host,function,"Self");Failed(result,Vm::Status::Invalid,host,"Original unsafe Caps storage refusal");
        Require(host.effects==0u && host.Load("Self",p).text==std::string("a\0b",3),"Unsafe Caps failed atomic rollback");
    }
    {
        TestHost host;auto function=Function("SnapshotRollback",Join({Native(168u,{Ref(0x01,1),Native(1060u,{Ref(0x01,1),String("changed")})}),Native(4095u),Return()}));
        const auto p=host.Property(function,1,"Text",Value::Text(Vm::Kind::String,{}));host.Store("Self",p,{Value::Text(Vm::Kind::String,"original")});
        const auto result=Vm::Execute(host,function,"Self");Failed(result,Vm::Status::Unsupported,host,"Snapshot mutation rollback on later failure");
        Require(host.Load("Self",p).text=="original","Pure native snapshot detached OUT mutation from transaction");
    }
    {
        TestHost host;host.objects[{"Fixture",1}]="unresolved";
        const auto result=Vm::Execute(host,Function("UnresolvedObjectPath",Join({Native(1000u),Return(Join({{0x56},Ref(0x20,1)}))})),"Self");
        Failed(result,Vm::Status::Unsupported,host,"ObjectToString refuses unvalidated host identity");Require(host.effects==0u,"Failed object path leaked earlier native effect");
    }
    std::cout<<"Original-audited startup primitives/UTF16/input snapshots: "<<checks-initialChecks<<" checks and "
        <<rejections-initialRejections<<" rejection controls passed.\n";
}
void IteratorContracts() {
    const auto configure=[](TestHost& host,const Vm::Function& function) {
        const auto output=host.Property(function,1,"Output",Value::Integer(0));
        const auto sum=host.Property(function,2,"Sum",Value::Integer(0));
        host.Store("Self",output,{Value::Integer(99)});host.Store("Self",sum,{Value::Integer(0)});
        return std::pair{output,sum};
    };
    const auto summing=[](const Bytes& factory) {
        Bytes code;const auto target=BeginIterator(code,factory);
        Append(code,Let(Ref(0x01,2),Native(146u,{Ref(0x01,2),Ref(0x01,1)})));
        EndIterator(code,target);Append(code,Return(Ref(0x01,2)));return code;
    };
    for(const bool empty : {false,true}) {
        TestHost host;if(empty) host.iteratorValues.clear();
        const auto function=Function("Foreach",summing(Native(1040u,{Ref(0x01,1)})));
        const auto [output,sum]=configure(host,function);
        const auto result=Vm::Execute(host,function,"Self");Returned(result,"Native iterator exhaustion/pop");
        Require(result.value.integer==(empty ? 0 : 6) && host.Load("Self",output).integer==0 &&
            host.Load("Self",sum).integer==result.value.integer,"Iterator output/exhaustion/body differs");
        Require(host.iteratorCreates==1u && host.iteratorDestroys==1u && host.iteratorNextCalls==(empty ? 1u : 4u),
            "Iterator push/first Next/exhaustion/Pop lifetime differs");
        bool expired{};try {host.retainedNativeReference->read();}catch(const std::runtime_error&) {expired=true;}
        Require(expired,"Iterator alias escaped its Machine lifetime");
    }
    {
        TestHost host;Bytes code;const auto outer=BeginIterator(code,Native(1040u,{Ref(0x01,1)}));
        const auto inner=BeginIterator(code,Native(1040u,{Ref(0x01,3)}));
        Append(code,Native(165u,{Ref(0x01,2)}));EndIterator(code,inner);EndIterator(code,outer);Append(code,Return(Ref(0x01,2)));
        const auto function=Function("NestedForeach",code);configure(host,function);
        const auto innerOutput=host.Property(function,3,"InnerOutput",Value::Integer(0));host.Store("Self",innerOutput,{Value::Integer(0)});
        const auto result=Vm::Execute(host,function,"Self");Returned(result,"Nested iterator LIFO stacks");
        Require(result.value.integer==9 && host.iteratorCreates==4u && host.iteratorDestroys==4u &&
            host.iteratorNextCalls==16u,"Nested iterator cursor or cleanup interfered with its parent");
    }
    for(const std::uint8_t invocation : {std::uint8_t{0x1b},std::uint8_t{0x1c},std::uint8_t{0x38}}) {
        TestHost host;auto declaration=Function("NativeIterator",{},"NativeSource");declaration.flags=0x405u;declaration.nativeIndex=1040u;
        declaration.variables={{"Native.Out","Out",Value::Integer(0),0x180u,1u},
            {"Native.Optional","Optional",Value::Text(Vm::Kind::Name,"None"),0x90u,1u}};
        const auto function=Function("DeclaredForeach",summing(Call(invocation,10,{Ref(0x01,1)})));
        configure(host,function);host.names[{function.source,10}]="Iterator";
        host.finals[{function.source,10}]=std::make_shared<Vm::Function>(declaration);
        host.named[{"Self",invocation==0x38 ? Vm::CallKind::Global : Vm::CallKind::Virtual,"Iterator"}]=
            std::make_shared<Vm::Function>(declaration);
        const auto result=Vm::Execute(host,function,"Self");Returned(result,"Declared native iterator invocation");
        Require(result.value.integer==6 && host.nativeDeclarations.front()==declaration.path &&
            host.nativeArguments.front().size()==2u && host.nativeArguments.front()[1].kind==Vm::Kind::Nothing,
            "Iterator declaration/omitted optional argument handling differs");
    }
    {
        TestHost host;const auto function=Function("ContextForeach",summing(Context(Ref(0x20,10),Native(1040u,{Ref(0x01,1)}))));
        const auto [output,sum]=configure(host,function);(void)sum;host.objects[{function.source,10}]="Other";
        const auto result=Vm::Execute(host,function,"Self");Returned(result,"Context iterator keeps Self argument evaluation");
        Require(result.value.integer==6 && host.nativeReceivers.front()=="Other" && host.Load("Self",output).integer==0 &&
            !host.storage.contains({"Other",Vm::Scope::Instance,output.key}),"Iterator Context redirected argument variables");
    }
    {
        TestHost host;Bytes code;const auto target=BeginIterator(code,Native(1040u,{Ref(0x01,1)}));
        Append(code,Return(Ref(0x01,1)));EndIterator(code,target);Append(code,Return());
        const auto function=Function("ReturnInsideForeach",code);const auto [output,sum]=configure(host,function);(void)sum;
        const auto result=Vm::Execute(host,function,"Self");Returned(result,"Function Return owns remaining iterators");
        Require(result.value.integer==1 && host.Load("Self",output).integer==1 && host.iteratorDestroys==1u &&
            host.iteratorNextCalls==1u,"Function Return cleared output or leaked its iterator");
    }
    for(const auto opcode : {std::uint8_t{0x30},std::uint8_t{0x31}}) {
        TestHost host;const auto result=Vm::Execute(host,Function("UnmatchedIterator",Join({Native(1000u),{opcode},Return()})),"Self");
        Failed(result,Vm::Status::Invalid,host,"Unmatched iterator control");Require(host.effects==0u,"Unmatched iterator escaped rollback");
    }
    for(const bool operand : {false,true}) {
        TestHost host;auto code=summing(Native(1040u,{Ref(0x01,1)}));
        const auto target=1u+Native(1040u,{Ref(0x01,1)}).size();
        PatchU16(code,target,operand ? 2u : code.size()-6u);
        const auto function=Function("MalformedIteratorEnd",code);configure(host,function);
        const auto result=Vm::Execute(host,function,"Self");Failed(result,Vm::Status::Invalid,host,"Iterator target requires top-level Pop");
        Require(host.iteratorCreates==0u,"Malformed iterator target ran a factory before rejection");
    }
    for(const bool outside : {false,true}) {
        TestHost host;Bytes code;
        if(outside) code=Join({Native(1040u,{Ref(0x01,1)}),Return()});
        else {const auto target=BeginIterator(code,Native(1040u,{Ref(0x01,1)}));Append(code,Native(4095u));EndIterator(code,target);Append(code,Return());}
        const auto function=Function("IteratorFailure",code);const auto [output,sum]=configure(host,function);(void)sum;
        const auto result=Vm::Execute(host,function,"Self");Failed(result,Vm::Status::Unsupported,host,"Iterator factory/Next cannot escape failure");
        Require(host.Load("Self",output).integer==99 && host.iteratorCreates==(outside ? 0u : 1u) &&
            host.iteratorDestroys==host.iteratorCreates,"Iterator failure lost rollback or leaked its cursor");
    }
    for(const bool storage : {false,true}) {
        TestHost host;const auto function=Function("IteratorBudget",summing(Native(1040u,{Ref(0x01,1)})));
        const auto [output,sum]=configure(host,function);(void)sum;Vm::Limits limits;
        if(storage) host.iteratorBytes=limits.retainedBytes+1u;else limits.writes=2u;
        const auto result=Vm::Execute(host,function,"Self",{},limits);Failed(result,Vm::Status::Budget,host,"Iterator shared storage/write budget");
        Require(host.Load("Self",output).integer==99 && host.iteratorDestroys==1u,"Iterator budget failed rollback/cleanup");
    }
    {
        TestHost host;const auto function=Function("NullContextForeach",summing(Context({0x2a},Native(1040u,{Ref(0x01,1)}))));
        configure(host,function);const auto result=Vm::Execute(host,function,"Self");
        Failed(result,Vm::Status::Invalid,host,"Accessed None iterator has no factory");
        Require(host.iteratorCreates==0u,"Null Context iterator evaluated its call");
    }
    {
        TestHost host;Bytes code;std::vector<std::size_t> targets;
        for(std::size_t depth=0;depth<5u;++depth) targets.push_back(BeginIterator(code,Native(1040u,{Ref(0x01,1)})));
        Append(code,Native(1000u));
        for(auto target=targets.rbegin();target!=targets.rend();++target) EndIterator(code,*target);
        Append(code,Return());const auto function=Function("IteratorStackBudget",code);configure(host,function);
        Vm::Limits limits;limits.expressionDepth=4u;
        const auto result=Vm::Execute(host,function,"Self",{},limits);Failed(result,Vm::Status::Budget,host,"Iterator stack shares bounded depth");
        Require(host.iteratorCreates==4u && host.iteratorDestroys==4u && host.effects==0u,"Iterator stack budget leaked active cursors or ran body");
    }
    {
        TestHost host;const auto function=Function("IteratorInstructionBudget",summing(Native(1040u,{Ref(0x01,1)})));
        const auto [output,sum]=configure(host,function);(void)sum;Vm::Limits limits;limits.instructions=12u;
        const auto result=Vm::Execute(host,function,"Self",{},limits);Failed(result,Vm::Status::Budget,host,"Iterator instructions share execution budget");
        Require(host.Load("Self",output).integer==99 && host.iteratorDestroys==1u,"Iterator instruction budget leaked output/cursor");
    }
    {
        TestHost host;auto declaration=Function("NotAnIterator",{});declaration.flags=0x400u;declaration.nativeIndex=1040u;
        const auto function=Function("NonIteratorDeclaration",summing(Call(0x1c,10,{Ref(0x01,1)})));configure(host,function);
        host.finals[{function.source,10}]=std::make_shared<Vm::Function>(declaration);
        const auto result=Vm::Execute(host,function,"Self");Failed(result,Vm::Status::Unsupported,host,"Factory requires iterator declaration flag");
        Require(host.iteratorCreates==0u,"Noniterator declaration reached native iterator factory");
    }
    for(const int transition : {-1,0,1,2,3,4,5}) {
        TestHost host;Bytes code;const auto target=BeginIterator(code,Native(1040u,{Ref(0x01,1)}));
        if(transition>=0 && transition<4) {
            const auto helper=Function("ReplaceState",Join({transition==0 ? Native(1043u) : Native(1043u,{Int(transition)}),Return()}));
            host.finals[{"Fixture",10}]=std::make_shared<Vm::Function>(helper);Append(code,Call(0x1c,10));
        } else if(transition==4) code.push_back(0x08);
        else if(transition==5) Append(code,Return());
        else Append(code,Native(165u,{Ref(0x01,2)}));
        EndIterator(code,target);code.push_back(0x08);
        const auto function=Function("StateForeach",code);const auto [output,sum]=configure(host,function);
        QuestVr::StateFrame frame;frame.codePath=frame.localsCodePath=function.path;
        host.state=QuestVr::StateObject{true,true,frame,{}};host.statePrograms[function.path]=function;
        host.statePrograms["Fixture.OtherState"]=Function("OtherState",{0x08});
        const auto result=Vm::ResumeState(host,"Self");
        if(transition<0) {
            Require(result.passed() && result.status==Vm::Status::Stopped && result.committed &&
                host.Load("Self",sum).integer==3 && host.Load("Self",output).integer==0 && host.iteratorDestroys==1u,
                "Synchronous state iterator failed to complete and Pop before Stop");
        } else {
            Failed(result,Vm::Status::Unsupported,host,"State iterator continuation/code/local replacement is explicit");
            Require(host.state->frame->statementIndex==0u && host.state->frame->codePath==function.path &&
                host.stateRevision==0u && host.Load("Self",output).integer==99 && host.iteratorDestroys==1u,
                "Unsupported state iterator continuation was partly committed or leaked");
        }
    }
}
Bytes Rotation(const std::array<std::int32_t, 3>& rotation) {
    Bytes bytes{0x22};
    for (const auto component : rotation) U32(bytes, std::bit_cast<std::uint32_t>(component));
    return bytes;
}
void RotatorDirectionContracts() {
    TestHost host;
    const auto convert = [&](const std::array<std::int32_t, 3>& rotation) {
        const auto result = Vm::Execute(host,
            Function("RotatorDirection", Return(Join({{0x39}, Rotation(rotation)}))), "Self");
        Returned(result, "Pinned RotatorToVector");
        Require(result.value.kind == Vm::Kind::Vector, "RotatorToVector lost detached Vector type");
        const auto expected = Coords::Rotation(Rotator(rotation[0], rotation[1], rotation[2])).XAxis;
        const std::array<float, 3> values{expected.x, expected.y, expected.z};
        for (std::size_t axis = 0; axis < values.size(); ++axis)
            Require(std::bit_cast<std::uint32_t>(result.value.vector[axis]) ==
                std::bit_cast<std::uint32_t>(values[axis]),
                "RotatorToVector changed pinned float math/sign/wrapping");
        return result.value;
    };
    for (std::int32_t angle = 0; angle < 65536; ++angle) {
        (void)convert({17371, angle, -3421});
        (void)convert({angle, -12743, 21897});
    }
    for (const auto angle : {std::numeric_limits<std::int32_t>::min(),
        std::numeric_limits<std::int32_t>::max(), -65537, -65536, -49152, -32768,
        -16384, -1, 0, 1, 16384, 32768, 49152, 65535, 65536, 65537})
        (void)convert({angle, angle, angle});
    std::uint32_t random = 0x294315c7u;
    for (std::size_t i = 0; i < 4096; ++i) {
        std::array<std::int32_t, 3> rotation{};
        for (auto& component : rotation) {
            random = random * 1664525u + 1013904223u;
            component = std::bit_cast<std::int32_t>(random);
        }
        (void)convert(rotation);
    }
    const auto close = [](float actual, float expected) { return std::fabs(actual - expected) < 1.0e-6f; };
    for (const auto& cardinal : std::vector<std::pair<std::array<std::int32_t, 3>, std::array<float, 3>>>{
        {{0,0,0},{1,0,0}}, {{0,16384,0},{0,1,0}}, {{0,32768,0},{-1,0,0}},
        {{0,49152,0},{0,-1,0}}, {{16384,0,0},{0,0,1}}, {{49152,0,0},{0,0,-1}},
        {{0,0,16384},{1,0,0}}}) {
        const auto value = convert(cardinal.first);
        for (std::size_t axis = 0; axis < 3u; ++axis)
            Require(close(value.vector[axis], cardinal.second[axis]),
                "RotatorToVector violated independent cardinal direction");
    }
    {
        TestHost nullable;
        const auto result = Vm::Execute(nullable, Function("NothingDirection", Return({0x39,0x0b})), "Self");
        Returned(result, "Nothing RotatorToVector");
        Require(result.value.kind == Vm::Kind::Vector && result.value.vector == std::array<float,3>{1,0,0},
            "Nothing RotatorToVector is not pinned zero-rotation forward");
    }
    {
        TestHost once;
        const auto function = Function("DirectionChildOnce", Return(Join({{0x39},
            Native(1003, {Rotation({1283,-2467,13}), Native(1000)})})));
        const auto result = Vm::Execute(once, function, "Self");
        Returned(result, "RotatorToVector child effects");
        Require(result.value.kind == Vm::Kind::Vector && once.effects == 1u &&
            once.nativeArguments.size() == 2u && once.beginCount == 1u && once.commitCount == 1u,
            "RotatorToVector duplicated child execution or transaction");
    }
    {
        TestHost alias;
        auto function = Function("DirectionAlias", Return(Join({{0x39},
            Native(1007, {Ref(0x01,1)})})));
        const auto property = alias.Property(function,1,"Rotation",Value::Rotator({0,0,0}));
        alias.Store("Self",property,{Value::Rotator({13,29,47})});
        const auto result = Vm::Execute(alias,function,"Self");
        Returned(result,"RotatorToVector source alias");
        Require(result.value.kind == Vm::Kind::Vector && alias.Load("Self",property).rotation ==
            std::array<std::int32_t,3>{13,29,47}, "RotatorToVector mutated source alias");
        alias.Store("Self",property,{Value::Rotator({1000,2000,3000})});
        const auto expected = Coords::Rotation(Rotator(13,29,47)).XAxis;
        Require(result.value.vector == std::array<float,3>{expected.x,expected.y,expected.z},
            "RotatorToVector retained source alias instead of detached snapshot");
    }
    for (const auto& wrong : std::vector<Bytes>{Int(0),Real(0),String("0,0,0"),{0x27},{0x2a},
        Native(1005),Native(1000),Bytes{0x23,0,0,0,0,0,0,0,0,0,0,0,0}}) {
        TestHost invalid;
        const auto function = Function("WrongDirectionType",Join({Native(1000),Return(Join({{0x39},wrong}))}));
        const auto result = Vm::Execute(invalid,function,"Self");
        Failed(result,Vm::Status::Unsupported,invalid,"RotatorToVector type refusal");
        Require(result.function == function.path && result.opcode == 0x39 &&
            invalid.effects == 0u && invalid.rng == 12345u,
            "RotatorToVector type failure lost trace or retained child/prior effects");
    }
    {
        TestHost invalid;
        auto function = Function("DirectionIsNotLvalue",Return(Let(
            Join({{0x39},Rotation({0,0,0})}), Bytes{0x23,0,0,0,0,0,0,0,0,0,0,0,0})));
        const auto result = Vm::Execute(invalid,function,"Self");
        Failed(result,Vm::Status::Invalid,invalid,"RotatorToVector detached result assignment");
    }
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
void DecalNativeInputContracts() {
    const auto initialChecks=checks;
    const auto declaration=[](TestHost& host,const std::string& path,bool out=false) {
        auto function=Function("AttachDecal",{},"Engine.Decal");
        function.path=path;function.flags=0x500u;function.nativeIndex=0u;
        function.variables={host.Property(function,101,"TraceDistance",Value::Float(0),out ? 0x180u : 0x80u,1u,true),
            host.Property(function,102,"DecalDir",Value::Vector({}),0x90u,1u,true),
            host.Property(function,103,"ReturnValue",Value::Bool(false),0x580u,1u,true)};
        return function;
    };
    const auto direction=std::array<float,3>{1.25f,-2.5f,3.75f};
    // The original native-zero declaration copies P_GET_FLOAT and P_GET_VECTOR
    // as each expression runs. A later expression must not reload the already
    // consumed first alias. Exact qualified identity is case-insensitive; a
    // same-named function in another class must not inherit this policy.
    for(const auto& path:{std::string("Engine.Decal.AttachDecal"),std::string("eNgInE.dEcAl.aTtAcHdEcAl")}) {
        for(const auto opcode:{std::uint8_t{0x1c},std::uint8_t{0x1b},std::uint8_t{0x38}}) {
            TestHost host;
            const auto attach=declaration(host,path);
            const auto caller=Function("DecalInputOrder",Return(Call(opcode,7u,{Ref(0x01,1),
                Native(1060u,{Ref(0x01,1),Real(99.0f),Vector(direction)})})),"DecalCaller");
            const auto distance=host.Property(caller,1,"TraceDistance",Value::Float(0));
            host.Store("Self",distance,{Value::Float(35.0f)});
            if(opcode==0x1c) host.finals[{caller.source,7}]=std::make_shared<Vm::Function>(attach);
            else {
                host.names[{caller.source,7}]="AttachDecal";
                host.named[{"Self",opcode==0x38 ? Vm::CallKind::Global : Vm::CallKind::Virtual,"AttachDecal"}]=
                    std::make_shared<Vm::Function>(attach);
            }
            std::size_t calls{};
            host.executionNative=[&](const std::string& receiver,const std::vector<Vm::Evaluation>& arguments,
                const Vm::Function* actual,Vm::Execution&) {
                ++calls;host.observations.push_back("attach");
                Require(receiver=="Self" && actual && actual->path==path && actual->flags==0x500u &&
                    actual->nativeIndex==0u && actual->variables.size()==3u &&
                    actual->variables[0].name=="TraceDistance" && actual->variables[0].flags==0x80u &&
                    actual->variables[1].name=="DecalDir" && actual->variables[1].flags==0x90u &&
                    actual->variables[2].name=="ReturnValue" && actual->variables[2].flags==0x580u &&
                    actual->variables[2].zero.kind==Vm::Kind::Bool,
                    "Attach observation lost original native-zero identity/signature");
                Require(arguments.size()==2u && !arguments[0].reference && !arguments[1].reference &&
                    arguments[0].Load().kind==Vm::Kind::Float && arguments[0].Load().floating==35.0f &&
                    arguments[1].Load().kind==Vm::Kind::Vector && arguments[1].Load().vector==direction,
                    "Attach reloaded earlier Float alias after the Vector expression mutated it");
                Require(host.Load("Self",distance).floating==99.0f,
                    "Attach snapshot suppressed the later argument's real mutation");
                return Vm::Evaluation{Value::Bool(true),{}};
            };
            const auto result=Vm::Execute(host,caller,"Self");Returned(result,"Original Attach native expression-time inputs");
            Require(result.value.kind==Vm::Kind::Bool && result.value.boolean && calls==1u &&
                host.nativeArguments.size()==2u && host.nativeArguments[1][0].floating==35.0f &&
                host.nativeDeclarations==std::vector<std::string>{"",path} &&
                host.observations==std::vector<std::string>{"variable:Self:TraceDistance","variable:Self:TraceDistance","attach"},
                "Attach argument order, copied value, native return or declaration routing changed");
            Require(host.beginCount==1u && host.commitCount==1u && host.rollbackCount==0u &&
                host.Load("Self",distance).floating==99.0f && host.effects==0u && host.rng==12345u,
                "Attach observation invented native effects/RNG or lost the argument mutation transaction");
        }
        {
            TestHost host;
            const auto attach=declaration(host,path);
            Value distance=Value::Float(41.0f);
            auto first=std::make_shared<Vm::Reference>();first->zero=Value::Float(0);
            auto second=std::make_shared<Vm::Reference>();second->zero=Value::Vector({});
            std::vector<std::string> reads;
            first->read=[&] {reads.push_back("distance");return distance;};
            second->read=[&] {reads.push_back("direction");distance=Value::Float(88.0f);return Value::Vector(direction);};
            host.executionNative=[&](const std::string&,const std::vector<Vm::Evaluation>& arguments,
                const Vm::Function* actual,Vm::Execution&) {
                Require(actual && actual->path==path && arguments.size()==2u &&
                    !arguments[0].reference && !arguments[1].reference &&
                    arguments[0].Load().floating==41.0f && arguments[1].Load().vector==direction,
                    "Direct Attach entry retained incoming aliases rather than successive value copies");
                return Vm::Evaluation{Value::Bool(true),{}};
            };
            const auto result=Vm::Execute(host,attach,"Self",{{{},first},{{},second}});
            Returned(result,"Direct original Attach incoming-alias snapshots");
            Require(result.value.kind==Vm::Kind::Bool && result.value.boolean && distance.floating==88.0f &&
                reads==std::vector<std::string>{"distance","direction"},
                "Direct Attach reordered or repeatedly loaded incoming aliases");
        }
        {
            TestHost host;
            const auto attach=declaration(host,path);
            host.executionNative=[&](const std::string&,const std::vector<Vm::Evaluation>& arguments,
                const Vm::Function*,Vm::Execution&) {
                Require(arguments.size()==2u && !arguments[0].reference && !arguments[1].reference &&
                    arguments[0].Load().floating==35.0f && arguments[1].Load().kind==Vm::Kind::Nothing,
                    "Attach optional Vector omission lost its native Nothing discriminator");
                return Vm::Evaluation{Value::Bool(false),{}};
            };
            const auto result=Vm::Execute(host,attach,"Self",{{Value::Float(35.0f),{}}});
            Returned(result,"Original Attach omitted optional Vector");
            Require(result.value.kind==Vm::Kind::Bool && !result.value.boolean,
                "Attach typed native Bool return was replaced by an input kind/value");
        }
    }
    for(const auto& path:{std::string("Fixture.Decal.AttachDecal"),std::string("Engine.Decal.AttachDecalSuffix"),
        std::string("Fixture.Actor.GeneratedOut")}) {
        TestHost host;
        const auto unknown=declaration(host,path,true);
        const auto caller=Function("UnknownZeroOut",Return(Call(0x1c,7u,{Ref(0x01,1),
            Native(1060u,{Ref(0x01,1),Real(99.0f),Vector(direction)})})),"UnknownZeroCaller");
        const auto distance=host.Property(caller,1,"TraceDistance",Value::Float(0));
        host.Store("Self",distance,{Value::Float(35.0f)});
        host.finals[{caller.source,7}]=std::make_shared<Vm::Function>(unknown);
        host.executionNative=[&](const std::string&,const std::vector<Vm::Evaluation>& arguments,
            const Vm::Function* actual,Vm::Execution&) {
            Require(actual && actual->path==path && arguments.size()==2u && arguments[0].reference &&
                arguments[0].Load().floating==99.0f,
                "Unknown native-zero OUT alias was mistaken for audited Attach ordinary input");
            arguments[0].reference->write(Value::Float(42.0f));
            return Vm::Evaluation{Value::Bool(true),{}};
        };
        const auto result=Vm::Execute(host,caller,"Self");Returned(result,"Generic native-zero OUT alias preservation");
        Require(result.value.kind==Vm::Kind::Bool && result.value.boolean && host.Load("Self",distance).floating==42.0f &&
            host.nativeArguments.size()==2u && host.nativeArguments[1][0].floating==99.0f,
            "Native-zero qualified exclusion lost genuine OUT writeback");
    }
    std::cout<<"Generated original Attach expression-time Float/Vector input contracts: "
        <<checks-initialChecks<<" checks passed.\n";
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
        Failed(r,Vm::Status::Invalid,host,"Switch requires an immediate Case label");
        Require(host.effects==0 && host.observations.empty(),"Malformed Switch evaluated native prefix effects before control validation");
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
void SwitchContracts() {
    const auto initialChecks=checks,initialRejections=rejections;
    const auto addNamesAndObjects=[](TestHost& host,const std::string& source="Fixture") {
        host.names[{source,0}]="None";host.names[{source,1}]="Alpha";host.names[{source,2}]="aLpHa";
        host.names[{source,3}]="None";host.names[{source,4}]="NONE";host.names[{source,5}]="";
        host.objects[{source,0}]="";host.objects[{source,1}]="Map.Actor0";
        host.objects[{source,2}]="mAP.aCTOR0";host.objects[{source,3}]="Map.Other0";
    };
    struct Comparison {const char* name;Bytes selector,label;bool match;};
    // IsEqual is selector-directed, not symmetric typed Coerce/Equal. In
    // particular Byte promotes to Int rather than wrapping a label to uint8.
    const std::vector<Comparison> comparisons{
        {"ByteInt",{0x24,1},Int(1),true},{"ByteNotModulo",{0x24,1},Int(257),false},
        {"ByteNotNegativeModulo",{0x24,255},Int(-1),false},{"ByteFloatTruncate",{0x24,1},Real(1.9f),true},
        {"ByteNothingZero",{0x24,0},{0x0b},true},{"ByteNothingNonzero",{0x24,1},{0x0b},false},
        {"IntByte",Int(255),{0x24,255},true},{"IntFloatTruncate",Int(-1),Real(-1.9f),true},
        {"IntNothingZero",Int(0),{0x0b},true},{"IntNothingNonzero",Int(1),{0x0b},false},
        {"IntKeepsPrecision",Int(16777217),Real(16777216.0f),false},
        {"FloatInt",Real(1.0f),Int(1),true},{"FloatNotTruncated",Real(1.9f),Int(1),false},
        {"FloatByte",Real(255.0f),{0x24,255},true},{"FloatRoundsInt",Real(16777216.0f),Int(16777217),true},
        {"FloatNothingZero",Real(-0.0f),{0x0b},true},{"FloatNothingNonzero",Real(0.5f),{0x0b},false},
        {"BoolTrue",{0x27},{0x27},true},{"BoolDifferent",{0x27},{0x28},false},
        {"BoolNothingZero",{0x28},{0x0b},true},{"BoolNothingNonzero",{0x27},{0x0b},false},
        {"NameFold",Ref(0x21,1),Ref(0x21,2),true},{"NameDifferent",Ref(0x21,1),Ref(0x21,3),false},
        {"NameNoneNothing",Ref(0x21,3),{0x0b},true},{"NameEmptyNone",Ref(0x21,5),Ref(0x21,4),true},
        {"StringExact",String("Alpha"),String("Alpha"),true},
        {"StringNotFolded",String("Alpha"),String("aLpHa"),false},
        {"StringNameExact",String("Alpha"),Ref(0x21,1),true},
        {"StringNameNotFolded",String("Alpha"),Ref(0x21,2),false},
        {"StringEmptyNothing",String(""),{0x0b},true},{"StringNoneNothing",String("None"),{0x0b},false},
        {"StringNoneEmptyName",String("None"),Ref(0x21,5),true},
        {"ObjectIdentity",Ref(0x20,1),Ref(0x20,2),true},{"ObjectDifferent",Ref(0x20,1),Ref(0x20,3),false},
        {"ObjectNullNothing",{0x2a},{0x0b},true},{"ObjectNonNullNothing",Ref(0x20,1),{0x0b},false},
        {"NothingNothing",{0x0b},{0x0b},true},{"NothingNullObject",{0x0b},{0x2a},true},
        {"NothingNonNullObject",{0x0b},Ref(0x20,1),false},
        {"NothingNotIntZero",{0x0b},Int(0),false},{"NothingNotByteZero",{0x0b},{0x24,0},false},
        {"NothingNotFloatZero",{0x0b},Real(0.0f),false},{"NothingNotBoolZero",{0x0b},{0x28},false},
        {"NothingNotNameNone",{0x0b},Ref(0x21,3),false},{"NothingNotEmptyString",{0x0b},String(""),false},
        {"NothingNotVectorZero",{0x0b},Vector({}),false},{"NothingNotRotatorZero",{0x0b},Rotation({}),false},
        {"NothingNotStruct",{0x0b},Native(1005u),false},
        {"VectorExact",Vector({1,-2,3}),Vector({1,-2,3}),true},
        {"VectorDifferent",Vector({1,-2,3}),Vector({1,-2,4}),false},
        {"VectorNothingZero",Vector({}),{0x0b},true},{"VectorNothingNonzero",Vector({1,0,0}),{0x0b},false},
        {"RotatorExact",Rotation({1,-2,3}),Rotation({1,-2,3}),true},
        {"RotatorNotWrapped",Rotation({0,0,0}),Rotation({65536,0,0}),false},
        {"RotatorNothingZero",Rotation({}),{0x0b},true},{"RotatorNothingNonzero",Rotation({1,0,0}),{0x0b},false}
    };
    for(const auto& comparison:comparisons) {
        TestHost host;addNamesAndObjects(host);
        const auto result=Vm::Execute(host,Function(comparison.name,SingleCaseSwitch(comparison.selector,comparison.label)),"Self");
        Returned(result,std::string("Selector-directed Switch ")+comparison.name);
        Require(result.value.kind==Vm::Kind::Int && result.value.integer==(comparison.match ? 1 : 0),
            std::string("Switch comparison changed pinned selector-directed conversion: ")+comparison.name);
    }
    for(const auto valueSize:{std::uint8_t{0},std::uint8_t{1},std::uint8_t{4},std::uint8_t{255}}) {
        TestHost host;const auto result=Vm::Execute(host,Function("IgnoredSwitchValueSize",SingleCaseSwitch(Int(1),Int(1),valueSize)),"Self");
        Returned(result,"Switch value-size byte is not a selector coercion or offset");
        Require(result.value.integer==1,"Switch changed semantics with its ignored serialized value-size byte");
    }
    const std::vector<std::pair<Bytes,Bytes>> wrongTypes{
        {Int(1),{0x27}},{{0x24,1},String("1")},{Real(1),Ref(0x21,1)},{{0x27},Int(1)},
        {Ref(0x21,1),String("Alpha")},{Ref(0x20,1),Ref(0x21,1)},
        {Vector({}),Rotation({})},{Rotation({}),Vector({})},
        {Vector({}),Native(1005u)},{Rotation({}),Native(1005u)},
        {Native(1005u),Native(1005u)},{Native(1005u),{0x0b}},{Native(1005u),Vector({})}
    };
    for(const auto& [selector,label]:wrongTypes) {
        TestHost host;addNamesAndObjects(host);
        const auto function=Function("SwitchWrongComparisonType",SingleCaseSwitch(selector,label));
        const auto result=Vm::Execute(host,function,"Self");Failed(result,Vm::Status::Unsupported,host,"Switch typed comparison / unknown Struct identity");
        Require(result.function==function.path && result.offset==Switch(selector).size() && result.opcode==0x0a,
            "Switch comparison failure reports a evaluated operand instead of the responsible Case");
    }
    {
        TestHost host;auto code=Switch(Native(1005u));DefaultCase(code);Append(code,Return(Int(7)));
        const auto result=Vm::Execute(host,Function("StructDefaultOnly",code),"Self");Returned(result,"Unknown Struct selector needs no comparison for default-only Switch");
        Require(result.value.integer==7 && host.nativeArguments.size()==1u,"Default-only Switch compared a Struct or reevaluated its selector");
    }
    {
        TestHost host;auto code=Switch(Native(165u,{Ref(0x01,1)}));
        const auto first=BeginCase(code,Native(1002u));Append(code,Return(Int(23)));
        PatchU16(code,first,code.size());const auto second=BeginCase(code,Int(1));Append(code,Return(Int(11)));
        PatchU16(code,second,code.size());const auto third=BeginCase(code,Native(1003u,{Int(1),Native(1000u)}));
        Append(code,Return(Int(22)));PatchU16(code,third,code.size());DefaultCase(code);Append(code,Return(Int(0)));
        const auto function=Function("SelectorOnceFirstDuplicate",code);
        const auto property=host.Property(function,1,"Selector",Value::Integer(0));host.Store("Self",property,{Value::Integer(1)});
        const auto result=Vm::Execute(host,function,"Self");Returned(result,"Switch evaluates selector once and only visited case expressions");
        Require(result.value.integer==11 && result.writes==1u && host.Load("Self",property).integer==2 && host.effects==0u &&
            host.observations==std::vector<std::string>{"variable:Self:Selector","rhs"},
            "Switch reevaluated its selector, missed first duplicate, or evaluated an unvisited label");
    }
    for(const bool nativeSnapshot:{false,true}) {
        TestHost host;auto code=Switch(nativeSnapshot ? Native(1007u,{Ref(0x01,1)}) : Ref(0x01,1));
        const auto first=BeginCase(code,Native(165u,{Ref(0x01,1)}));Append(code,Return(Int(1)));
        PatchU16(code,first,code.size());const auto second=BeginCase(code,Int(2));Append(code,Return(Int(2)));
        PatchU16(code,second,code.size());DefaultCase(code);Append(code,Return(Int(0)));
        const auto function=Function(nativeSnapshot ? "SelectorNativeDetached" : "SelectorAliasAfterCaseEffects",code);
        const auto property=host.Property(function,1,"Selector",Value::Integer(0));host.Store("Self",property,{Value::Integer(1)});
        const auto result=Vm::Execute(host,function,"Self");Returned(result,"Switch distinguishes a variable alias from a detached native result");
        Require(result.value.integer==(nativeSnapshot ? 1 : 2) && result.writes==1u && host.Load("Self",property).integer==2 &&
            host.nativeArguments.size()==(nativeSnapshot ? 1u : 0u) &&
            (!nativeSnapshot || host.nativeArguments.front()[0].integer==1) &&
            std::count(host.observations.begin(),host.observations.end(),"variable:Self:Selector")==2,
            "Switch detached a variable selector before the case write, or reevaluated its expression");
    }
    {
        TestHost host;auto code=Switch(Int(1));const auto first=BeginCase(code,Int(1));Append(code,Native(1000u));
        PatchU16(code,first,code.size());const auto second=BeginCase(code,Native(4095u));Append(code,Native(1000u));
        PatchU16(code,second,code.size());DefaultCase(code);Append(code,Native(1000u));Append(code,Return(Int(9)));
        const auto result=Vm::Execute(host,Function("CaseFallthrough",code),"Self");Returned(result,"Matched Case body falls through labels/default without reevaluation");
        Require(result.value.integer==9 && host.effects==3u && host.nativeArguments.size()==3u,
            "Fallthrough tested a later Case expression or failed to execute all body statements");
    }
    {
        TestHost host;Bytes code;const auto jump=BeginJump(code);Append(code,Native(4095u));
        PatchU16(code,jump,code.size());const auto next=BeginCase(code,Native(4095u));Append(code,Native(1000u));Append(code,Return(Int(5)));
        PatchU16(code,next,code.size());DefaultCase(code);Append(code,Return(Int(0)));
        const auto result=Vm::Execute(host,Function("JumpDirectlyToCase",code),"Self");Returned(result,"Authored Jump to a Case does not evaluate its label");
        Require(result.value.integer==5 && host.effects==1u && host.nativeArguments.size()==1u,
            "Direct Case entry evaluated its label or treated it as an active Switch comparison");
    }
    {
        TestHost host;auto code=Switch(Int(1));const auto outerNext=BeginCase(code,Int(1));
        Append(code,Switch(Int(2)));const auto innerNext=BeginCase(code,Int(2));Append(code,Native(1000u));
        const auto innerBreak=BeginJump(code);PatchU16(code,innerNext,code.size());DefaultCase(code);Append(code,Native(4095u));
        PatchU16(code,innerBreak,code.size());Append(code,Native(1000u));const auto outerBreak=BeginJump(code);
        PatchU16(code,outerNext,code.size());DefaultCase(code);Append(code,Native(4095u));
        PatchU16(code,outerBreak,code.size());Append(code,Return(Int(8)));
        const auto result=Vm::Execute(host,Function("NestedSwitchBreak",code),"Self");Returned(result,"Nested Switches and authored break Jumps");
        Require(result.value.integer==8 && host.effects==2u && host.nativeArguments.size()==2u,
            "Nested Switch consumed outer Case links, or a break entered a default body");
    }
    {
        TestHost host;auto code=Switch(Int(1));const auto first=BeginCase(code,Int(0));Append(code,Native(4095u));
        const auto skipped=BeginCase(code,Native(4095u));Append(code,Native(4095u));
        PatchU16(code,first,code.size());const auto matched=BeginCase(code,Int(1));Append(code,Return(Int(3)));
        PatchU16(code,skipped,code.size());PatchU16(code,matched,code.size());DefaultCase(code);Append(code,Return(Int(0)));
        const auto result=Vm::Execute(host,Function("LinkedCasesNotSequential",code),"Self");Returned(result,"Switch follows authored Case links, not physical next label");
        Require(result.value.integer==3 && host.nativeArguments.empty(),"Switch evaluated a physically adjacent but unlinked label/body");
    }
    {
        TestHost host;addNamesAndObjects(host,"Caller");addNamesAndObjects(host,"Callee");
        host.names[{"Caller",1}]="Wrong";
        const auto callee=Function("SwitchSource",SingleCaseSwitch(Ref(0x21,1),Ref(0x21,2)),"Callee");
        const auto caller=Function("SwitchSourceCaller",Return(Call(0x1c,17)),"Caller");
        host.finals[{caller.source,17}]=std::make_shared<Vm::Function>(callee);
        const auto result=Vm::Execute(host,caller,"Self");Returned(result,"Switch selector/Case references use their own callee source tables");
        Require(result.value.integer==1,"Nested Switch read caller Name references");
    }
    {
        TestHost host;const auto function=Function("SwitchAnalysis",SingleCaseSwitch(Int(1),Native(4095u)));
        const auto layout=Vm::AnalyzeProgram(host,function);
        const auto first=Switch(Int(1)).size(),body=first+3u+Native(4095u).size(),fallback=body+Return(Int(1)).size();
        Require(layout.statementOffsets==std::vector<std::size_t>{0u,first,body,fallback,fallback+3u} &&
            layout.labels.empty() && !layout.terminalLabelTable && host.observations.empty() && host.nativeArguments.empty() && !host.beginCount,
            "AnalyzeProgram changed logical Case boundaries, evaluated labels, or began an execution transaction");
    }
    // Both Execute and structural AnalyzeProgram reject malformed links before
    // earlier property/native/RNG effects. Diagnostics identify the owner node.
    struct Broken {Bytes code;std::size_t offset;std::uint8_t opcode;};
    std::vector<Broken> broken;
    const auto selection=SingleCaseSwitch(Int(1),Int(1));const auto caseOffset=Switch(Int(1)).size();
    for(const auto target:std::array<std::size_t,7>{0u,caseOffset+1u,caseOffset+3u,caseOffset+4u,caseOffset+8u,selection.size(),65534u}) {
        auto code=selection;PatchU16(code,caseOffset+1u,target);broken.push_back({std::move(code),caseOffset,0x0a});
    }
    broken.push_back({Join({Switch(Int(1)),Return(Int(0))}),0u,0x05});
    broken.push_back({Switch(Int(1)),0u,0x05});
    broken.push_back({{0x05},0u,0x05});broken.push_back({{0x05,1u},0u,0x05});
    broken.push_back({Join({Switch(Int(1)),{0x0a}}),caseOffset,0x0a});
    broken.push_back({Join({Switch(Int(1)),{0x0a,0}}),caseOffset,0x0a});
    broken.push_back({Join({Switch(Int(1)),{0x0a,0,0}}),caseOffset,0x0a});
    broken.push_back({Join({Switch(Int(1)),{0x0a,0,0,0x1d,0}}),caseOffset+3u,0x1d});
    for(const auto& control:broken) {
        TestHost host;const auto function=Function("MalformedSwitchControl",control.code);
        const auto result=Vm::Execute(host,function,"Self");Failed(result,Vm::Status::Invalid,host,"Malformed Switch/Case topology or operands");
        Require(result.function==function.path && result.offset==control.offset && result.opcode==control.opcode &&
            host.nativeArguments.empty() && host.effects==0u,"Malformed Switch/Case lost exact failing logical offset or performed effects");
        bool rejected{};try {Vm::AnalyzeProgram(host,function);}catch(const std::exception&) {rejected=true;}
        Require(rejected,"AnalyzeProgram accepted the same malformed Switch/Case rejected by execution");++rejections;
    }
    {
        TestHost host;auto code=Join({Native(1000u),Let(Ref(0x01,1),Int(55)),Switch(Int(1)),Return(Int(0))});
        const auto function=Function("MalformedSwitchBeforeEffects",code);
        const auto property=host.Property(function,1,"Value",Value::Integer(0));host.Store("Self",property,{Value::Integer(3)});
        const auto result=Vm::Execute(host,function,"Self");Failed(result,Vm::Status::Invalid,host,"Whole program Switch validation precedes prefix effects");
        Require(host.Load("Self",property).integer==3 && host.effects==0u && host.rng==12345u && host.observations.empty(),
            "Malformed later Switch executed an earlier property/native/RNG prefix");
    }
    {
        TestHost host;auto code=Join({Native(1000u),Return(Switch(Int(1)))});
        DefaultCase(code);Append(code,Return(Int(0)));const auto function=Function("NestedExpressionSwitch",code);
        const auto result=Vm::Execute(host,function,"Self");Failed(result,Vm::Status::Unsupported,host,"Switch used as a value expression remains explicit unsupported");
        Require(result.offset==Native(1000u).size()+1u && result.opcode==0x05 && host.effects==0u && host.rng==12345u,
            "Nested Switch silently returned a value, lost its own diagnostic, or escaped rollback");
    }
    for(const std::size_t instructionLimit:{10u,11u}) {
        TestHost host;auto code=Join({Native(1000u),Let(Ref(0x01,1),Int(55)),Switch(Int(1))});
        const auto next=BeginCase(code,Int(0));PatchU16(code,next,next-1u);Append(code,Return(Int(0)));
        const auto function=Function("CyclicCaseLinks",code);
        const auto property=host.Property(function,1,"Value",Value::Integer(0));host.Store("Self",property,{Value::Integer(3)});
        host.objects[{function.source,1}]=property.key;
        const auto layout=Vm::AnalyzeProgram(host,function);Require(!layout.statementOffsets.empty(),"Bounded Case cycle was rejected structurally instead of executing under its instruction limit");
        Vm::Limits limits;limits.instructions=instructionLimit;
        const auto result=Vm::Execute(host,function,"Self",{},limits);Failed(result,Vm::Status::Budget,host,"Case chain cycle uses shared instruction budget");
        Require(result.instructions==instructionLimit && result.offset==(instructionLimit==10u ? next-1u : next+2u) &&
            result.opcode==(instructionLimit==10u ? 0x0a : 0x1d) && host.Load("Self",property).integer==3 &&
            host.effects==0u && host.rng==12345u && std::count(host.observations.begin(),host.observations.end(),"effect")==1,
            "Case cycle bypassed the shared budget, lost failing Case/label offsets, or retained prefix changes");
    }
    {
        const auto function=Function("SwitchInstructionBoundary",SingleCaseSwitch(Int(1),Int(1)));
        const std::array<std::pair<std::size_t,std::uint8_t>,6> failures{{{0u,0x05},{2u,0x1d},{7u,0x0a},{10u,0x1d},{15u,0x04},{16u,0x1d}}};
        for(std::size_t limit=0;limit<failures.size();++limit) {
            TestHost host;Vm::Limits limits;limits.instructions=limit;
            const auto result=Vm::Execute(host,function,"Self",{},limits);Failed(result,Vm::Status::Budget,host,"Switch and visited Case expressions are individually budgeted");
            Require(result.instructions==limit && result.offset==failures[limit].first && result.opcode==failures[limit].second,
                "Switch instruction budget diagnostics do not identify the actual next instruction");
        }
        TestHost host;Vm::Limits limits;limits.instructions=6u;const auto result=Vm::Execute(host,function,"Self",{},limits);
        Returned(result,"Exact successful Switch instruction boundary");Require(result.instructions==6u && result.value.integer==1,"Switch reset or inflated its instruction budget");
    }
    // A nested script can complete OUT copy-back while evaluating a label.
    // Failed comparison, matched body, and late write-budget failures must
    // restore both those changes and the preceding caller/native/RNG prefix.
    for(const unsigned failure:{0u,1u,2u}) {
        TestHost host;auto callee=Function("CaseOut",Join({Native(1000u),Let(Ref(0x00,1),Int(88)),
            Return(failure==0u ? Ref(0x21,7) : Int(1))}),"Callee");
        callee.variables={host.Property(callee,1,"Output",Value::Integer(0),0x180u,1u,true)};
        host.names[{callee.source,7}]="WrongCaseType";
        auto code=Join({Native(1000u),Let(Ref(0x01,2),Int(55)),Switch(Int(1))});
        const auto next=BeginCase(code,Call(0x1c,17,{Ref(0x01,2)}));const auto body=code.size();
        Append(code,failure==2u ? Let(Ref(0x01,2),Int(99)) : Native(4095u));Append(code,Return(Int(1)));
        PatchU16(code,next,code.size());DefaultCase(code);Append(code,Return(Int(0)));
        const auto function=Function("SwitchNestedOutRollback",code,"Caller");
        const auto property=host.Property(function,2,"Value",Value::Integer(0));host.Store("Self",property,{Value::Integer(3)});
        host.finals[{function.source,17}]=std::make_shared<Vm::Function>(callee);
        Vm::Limits limits;if(failure==2u) limits.writes=3u;
        const auto result=Vm::Execute(host,function,"Self",{},limits);
        Failed(result,failure==2u ? Vm::Status::Budget : Vm::Status::Unsupported,host,"Case nested OUT/property/native/RNG rollback");
        Require(result.writes==3u && host.Load("Self",property).integer==3 && host.effects==0u && host.rng==12345u &&
            std::count(host.observations.begin(),host.observations.end(),"effect")==2,
            "Switch failure retained a nested OUT copy-back, caller property prefix, or native/RNG side effect");
        Require(result.function==function.path && result.offset==(failure==0u ? next-1u : body) &&
            result.opcode==(failure==0u ? 0x0a : failure==1u ? 0x6f : 0x0f),
            "Nested Case comparison/body/write-budget failure lost the responsible instruction diagnostic");
    }
    {
        TestHost host;auto code=Join({Native(1000u),Let(Ref(0x01,1),Int(55)),Switch(Int(1))});
        const auto next=BeginCase(code,Native(1003u,{Int(1),Native(1006u,{Ref(0x01,1)})}));
        Append(code,Return(Int(1)));PatchU16(code,next,code.size());DefaultCase(code);Append(code,Return(Int(0)));
        const auto function=Function("FailedCaseNativeOut",code);
        const auto property=host.Property(function,1,"Value",Value::Integer(0));host.Store("Self",property,{Value::Integer(3)});
        const auto result=Vm::Execute(host,function,"Self");Failed(result,Vm::Status::Unsupported,host,"Failure inside a visited Case native OUT expression");
        Require(result.offset==next+2u+2u+Int(1).size() && result.opcode==0x63 && result.writes==2u &&
            host.Load("Self",property).integer==3 && host.effects==0u && host.rng==12345u,
            "Case native child failure reported its wrapper/last argument or escaped guarded OUT rollback");
    }
    // Persistent state execution uses the same selection/Case-no-op contracts,
    // while its statement ordinal and stop state participate in the journal.
    for(const bool fail:{false,true}) {
        TestHost host;auto code=Switch(Int(1));const auto next=BeginCase(code,Int(1));Append(code,Native(1000u));
        PatchU16(code,next,code.size());DefaultCase(code);Append(code,fail ? Native(4095u) : Native(1000u));code.push_back(0x08);
        const auto function=Function("StateSwitch",code);QuestVr::StateFrame frame;frame.codePath=frame.localsCodePath=function.path;
        host.state=QuestVr::StateObject{true,true,frame,{}};host.statePrograms[function.path]=function;
        const auto result=Vm::ResumeState(host,"Self");
        if(fail) {
            Failed(result,Vm::Status::Unsupported,host,"State Switch failure restores persistent cursor");
            Require(host.state->frame->statementIndex==0u && host.state->frame->latent==QuestVr::StateLatent::Continue &&
                host.effects==0u && host.rng==12345u,"Failed state Switch committed its selected position or native/RNG prefix");
        } else {
            Require(result.passed() && result.status==Vm::Status::Stopped && result.committed && host.effects==2u &&
                host.state->frame->latent==QuestVr::StateLatent::Stop && host.state->frame->statementIndex==6u,
                "State Switch did not commit fallthrough/default bodies followed by its authored Stop");
        }
    }
    {
        FunctionFixture fixture;Bytes raw{0x05,255,0x21};Index(raw,3);raw.push_back(0x0a);U16(raw,21);
        raw.push_back(0x21);Index(raw,3);Append(raw,Return(Int(1)));DefaultCase(raw);Append(raw,Return(Int(0)));
        auto normalized=SingleCaseSwitch(Ref(0x21,3),Ref(0x21,3),255u);const auto body=fixture.Decode(raw,normalized);
        TestHost host;host.names[{"Fixture",3}]="TestName";const auto function=Function("SerializedSwitch",body.bytecode);
        const auto layout=Vm::AnalyzeProgram(host,function);Require(layout.statementOffsets==std::vector<std::size_t>{0u,7u,15u,21u,24u},
            "Decoded Switch/Case topology used serialized offsets after compact reference widening");
        const auto result=Vm::Execute(host,function,"Self");Returned(result,"Raw package normalization feeds real Switch execution");
        Require(result.value.integer==1,"Real normalized Name selector/Case did not choose its matched body");
    }
    std::cout<<"Generated Switch/Case control, typed comparison and transactional failure contracts: "
        <<checks-initialChecks<<" checks and "<<rejections-initialRejections<<" rejection controls passed.\n";
}
void DynamicIntArrayContracts() {
    const auto initialChecks=checks,initialRejections=rejections;
    static_assert(static_cast<unsigned>(Vm::Kind::Struct)==10u);
    static_assert(static_cast<unsigned>(Vm::Kind::IntArray)==11u);
    const auto zero=Value::IntArray({});
    const auto payload=Value::IntArray({std::numeric_limits<std::int32_t>::min(),17,17,
        std::numeric_limits<std::int32_t>::max(),0,-1});
    Require(payload.kind==Vm::Kind::IntArray && payload.intArray.size()==6u &&
        payload.intArray[0]==std::numeric_limits<std::int32_t>::min() && payload.intArray[1]==payload.intArray[2] &&
        payload.intArray[3]==std::numeric_limits<std::int32_t>::max(),
        "Native INT-array factory lost ordered duplicate/full-width elements");
    Require(Vm::Equal(Vm::Coerce(payload,zero),payload) && Vm::Equal(Vm::Coerce({},zero),zero) &&
        Vm::Equal(Vm::Coerce({},payload),payload),"INT-array exact coercion or typed Nothing default changed values");
    Require(!Vm::Equal(payload,Value::Integer(17)) && !Vm::Equal(payload,Value::IntArray({17,17})) &&
        !Vm::Equal(Value::IntArray({1,2,1}),Value::IntArray({1,1,2})) &&
        !Vm::Equal(Value::IntArray({1,1}),Value::IntArray({1})),
        "INT-array equality ignored element type, order, length or duplicates");
    const auto rejectValue=[&](const std::function<void()>& operation,const std::string& context) {
        bool rejected{};try {operation();} catch(const std::runtime_error&) {rejected=true;}
        Require(rejected,context);++rejections;
    };
    for(const auto& scalar:std::vector<Value>{Value::Integer(1),Value::Byte(1),Value::Bool(true),Value::Float(1),
        Value::Text(Vm::Kind::String,"1"),Value::Text(Vm::Kind::Name,"One"),Value::Text(Vm::Kind::Object,"Map.Actor"),
        Value::Vector({}),Value::Rotator({})}) {
        rejectValue([&] {Vm::Coerce(scalar,zero);},"Non-INT-array value was coerced to an INT array");
        rejectValue([&] {Vm::Coerce(payload,scalar);},"INT array was coerced to an unrelated scalar value");
    }
    rejectValue([&] {Vm::ToInt(payload);},"INT array implicitly became an integer");
    rejectValue([&] {Vm::ToFloat(payload);},"INT array implicitly became a float");
    rejectValue([&] {Vm::ToBool(payload);},"INT array implicitly became a boolean");
    {
        TestHost host;
        auto callee=Function("ArrayCopy",Join({Let(Ref(0x00,2),Ref(0x00,1)),Return(Ref(0x00,2))}),"ArrayCallee");
        callee.variables={host.Property(callee,1,"Input",zero,0x80u,1u,true),
            host.Property(callee,2,"Output",zero,0x180u,1u,true),
            host.Property(callee,3,"ReturnValue",zero,0x480u,1u,true)};
        const auto caller=Function("ArrayCaller",Return(Call(0x1c,4,{Ref(0x01,8),Ref(0x01,9)})),"ArrayCaller");
        const auto input=host.Property(caller,8,"Input",zero),output=host.Property(caller,9,"Output",zero);
        host.Store("Self",input,{payload});host.Store("Self",output,{Value::IntArray({5})});
        host.finals[{caller.source,4}]=std::make_shared<Vm::Function>(callee);
        const auto result=Vm::Execute(host,caller,"Self");Returned(result,"Whole INT-array input/local/OUT/return");
        Require(Vm::Equal(result.value,payload) && Vm::Equal(host.Load("Self",output),payload) &&
            Vm::Equal(host.Load("Self",input),payload),"Whole INT-array call changed ordering, input or OUT copyback");
    }
    {
        TestHost host;auto function=Function("ArrayNotReflectedElements",Return(Ref(0x00,1)));
        function.variables={host.Property(function,1,"Input",zero,0x80u,1u,true)};
        Vm::Limits limits;limits.localElements=1u;
        const auto result=Vm::Execute(host,function,"Self",{{payload,{}}},limits);
        Returned(result,"INT-array entries do not consume reflected local element slots");
        Require(Vm::Equal(result.value,payload),"Separate INT-array/local budgets changed a legal parameter");
    }
    {
        TestHost host;Value structure;structure.kind=Vm::Kind::Struct;
        structure.fields["surfaces"]=zero;structure.fields["keep"]=Value::Bool(true);
        auto function=Function("NestedArrayWrite",Join({Let(Member(2,Ref(0x01,1)),Ref(0x01,3)),Return(Ref(0x01,1))}));
        const auto record=host.Property(function,1,"Record",structure),input=host.Property(function,3,"Input",zero);
        host.Property(function,2,"Surfaces",zero);host.Store("Self",record,{structure});host.Store("Self",input,{payload});
        const auto result=Vm::Execute(host,function,"Self");Returned(result,"Nested struct INT-array alias write");
        Require(Vm::Equal(result.value.fields.at("surfaces"),payload) && result.value.fields.at("keep").boolean &&
            Vm::Equal(host.Load("Self",record),result.value),"INT-array member write lost its parent or unrelated fields");
        Require(Vm::Equal(Vm::Coerce(result.value,structure),result.value),"Recursive struct coercion lost its INT-array member");
    }
    {
        TestHost host;const auto ordered=Function("NativeArrayWrite",Join({
            Native(1060u,{Ref(0x01,1),Ref(0x01,2)}),Return(Native(1007u,{Ref(0x01,1)}))}));
        const auto output=host.Property(ordered,1,"Output",zero),input=host.Property(ordered,2,"Input",zero);
        host.Store("Self",output,{Value::IntArray({9})});host.Store("Self",input,{payload});
        const auto result=Vm::Execute(host,ordered,"Self");Returned(result,"Guarded native whole INT-array write");
        Require(Vm::Equal(result.value,payload) && Vm::Equal(host.Load("Self",output),payload),
            "Native whole INT-array write or detached alias return changed payload");
        rejectValue([&] {host.retainedNativeReference->write(Value::IntArray({99}));},"Expired native INT-array alias remained writable");
        Require(Vm::Equal(host.Load("Self",output),payload),"Expired native array alias changed committed storage");
    }
    {
        TestHost host;const auto function=Function("ArrayRollback",Join({
            Native(1060u,{Ref(0x01,1),Ref(0x01,2)}),Native(1000u),Return(Native(4095u))}));
        const auto output=host.Property(function,1,"Output",zero),input=host.Property(function,2,"Input",zero);
        const auto previous=Value::IntArray({7,7,3});host.Store("Self",output,{previous});host.Store("Self",input,{payload});
        const auto result=Vm::Execute(host,function,"Self");Failed(result,Vm::Status::Unsupported,host,"Whole INT-array native rollback");
        Require(Vm::Equal(host.Load("Self",output),previous) && Vm::Equal(host.Load("Self",input),payload) &&
            host.effects==0u && host.rng==12345u,"Failed native prefix leaked INT-array data/effects/RNG");
    }
    for(const auto index:{-1,0,1000000}) {
        TestHost host;
        // Neither child is evaluated when the audited-but-unimplemented
        // opcode is rejected; the index expression would record "rhs".
        const auto expression=Join({{0x10},Native(1002u,{Int(index)}),Ref(0x01,1)});
        const auto function=Function("UnimplementedDynamicIndex",Join({
            Native(1060u,{Ref(0x01,1),Ref(0x01,2)}),Native(1000u),Return(expression)}));
        const auto output=host.Property(function,1,"Output",zero),input=host.Property(function,2,"Input",zero);
        const auto previous=Value::IntArray({4,4});host.Store("Self",output,{previous});host.Store("Self",input,{payload});
        host.objects[{function.source,1}]=output.key;host.objects[{function.source,2}]=input.key;
        const auto layout=Vm::AnalyzeProgram(host,function);
        Require(layout.statementOffsets.size()==3u,"Dynamic-array token lost its known structural operand layout");
        const auto result=Vm::Execute(host,function,"Self");Failed(result,Vm::Status::Unsupported,host,"Original dynamic indexing remains unsupported");
        Require(result.opcode==0x10u && result.error.find("0x10")!=std::string::npos &&
            std::find(host.observations.begin(),host.observations.end(),"rhs")==host.observations.end() &&
            Vm::Equal(host.Load("Self",output),previous) && host.effects==0u && host.rng==12345u,
            "Unsupported dynamic indexing evaluated operands, fabricated clamping/growth, or leaked its prefix");
    }
    for(const auto opcode:{std::uint8_t{0x32},std::uint8_t{0x33}}) {
        TestHost host;const auto function=Function("NoScriptArrayComparison",Return(Join({Ref(opcode,7),Ref(0x01,1),Ref(0x01,1)})));
        host.objects[{function.source,7}]="Fixture.ArrayLayout";
        const auto property=host.Property(function,1,"Array",zero);host.Store("Self",property,{payload});
        const auto result=Vm::Execute(host,function,"Self");Failed(result,Vm::Status::Unsupported,host,"No invented script INT-array comparison");
    }
    {
        TestHost host;const auto function=Function("NoSwitchArrayComparison",SingleCaseSwitch(Ref(0x01,1),Ref(0x01,1)));
        const auto property=host.Property(function,1,"Array",zero);host.Store("Self",property,{payload});
        const auto result=Vm::Execute(host,function,"Self");Failed(result,Vm::Status::Unsupported,host,"No invented INT-array Switch comparison");
    }
    const auto reserved=[](std::size_t count) {std::vector<std::int32_t> values;values.reserve(count);return Value::IntArray(std::move(values));};
    for(const bool excess:{false,true}) {
        TestHost host;auto array=reserved(64u);const auto capacity=array.intArray.capacity();
        Vm::Limits limits;limits.intArrayElements=excess ? capacity-1u : capacity;
        const auto result=Vm::ExecuteTransaction(host,[&](Vm::Execution&) {return std::move(array);},limits);
        if(excess) Failed(result,Vm::Status::Budget,host,"INT-array spare capacity exceeds its element bound");
        else {Returned(result,"INT-array capacity at exact element bound");Require(result.value.intArray.empty() &&
            result.value.intArray.capacity()==capacity,"Native INT-array move discarded its retained allocation");}
    }
    for(const bool excess:{false,true}) {
        TestHost host;auto array=reserved(64u);const auto capacity=array.intArray.capacity();
        Vm::Limits limits;limits.retainedBytes=sizeof(Value)+capacity*sizeof(std::int32_t)-(excess ? 1u : 0u);
        const auto result=Vm::ExecuteTransaction(host,[&](Vm::Execution&) {return std::move(array);},limits);
        if(excess) Failed(result,Vm::Status::Budget,host,"INT-array spare allocation contributes retained bytes");
        else Returned(result,"INT-array exact retained byte boundary");
    }
    for(const bool excess:{false,true}) {
        TestHost host;Value structure;structure.kind=Vm::Kind::Struct;
        structure.fields["first"]=reserved(32u);structure.fields["second"]=reserved(48u);
        const auto allocation=(structure.fields.at("first").intArray.capacity()+structure.fields.at("second").intArray.capacity())*sizeof(std::int32_t);
        Vm::Limits limits;limits.localElements=3u;limits.retainedBytes=3u*sizeof(Value)+11u+allocation-(excess ? 1u : 0u);
        const auto result=Vm::ExecuteTransaction(host,[&](Vm::Execution&) {return std::move(structure);},limits);
        if(excess) Failed(result,Vm::Status::Budget,host,"Nested INT-array allocations share aggregate value budget");
        else Returned(result,"Nested INT-array allocations at exact aggregate boundary");
    }
    {
        TestHost host;auto function=Function("BoundedArrayParameter",Return(Ref(0x00,1)));
        function.variables={host.Property(function,1,"Input",zero,0x80u,1u,true)};
        Vm::Limits limits;limits.intArrayElements=payload.intArray.size()-1u;
        const auto result=Vm::Execute(host,function,"Self",{{payload,{}}},limits);
        Failed(result,Vm::Status::Budget,host,"Oversized whole INT-array parameter is bounded");
    }
    {
        TestHost host;const auto function=Function("BoundedNativeCapacity",Return(Native(1008u)));
        host.executionNative=[&](const std::string&,const std::vector<Vm::Evaluation>&,const Vm::Function*,Vm::Execution&) {
            return Vm::Evaluation{reserved(64u),{}};
        };
        Vm::Limits limits;limits.intArrayElements=63u;
        const auto result=Vm::Execute(host,function,"Self",{},limits);
        Failed(result,Vm::Status::Budget,host,"Detached native array capacity is checked before snapshot copying");
    }
    {
        TestHost host;const auto function=Function("BoundedNativeWrite",Return(Native(1008u,{Ref(0x01,1)})));
        const auto output=host.Property(function,1,"Output",zero);host.Store("Self",output,{Value::IntArray({7})});
        host.executionNative=[&](const std::string&,const std::vector<Vm::Evaluation>& arguments,const Vm::Function*,Vm::Execution&) {
            arguments.at(0).reference->write(reserved(64u));return Vm::Evaluation{};
        };
        Vm::Limits limits;limits.intArrayElements=63u;
        const auto result=Vm::Execute(host,function,"Self",{},limits);
        Failed(result,Vm::Status::Budget,host,"Native array write capacity is checked before whole-value coercion");
        Require(Vm::Equal(host.Load("Self",output),Value::IntArray({7})),"Over-capacity native array write changed its destination");
    }
    {
        TestHost host;const auto function=Function("RepeatedNativeArrayGrowth",Return(Native(1008u,{Ref(0x01,1)})));
        const auto output=host.Property(function,1,"Output",zero);host.Store("Self",output,{zero});
        std::size_t completedWrites{};
        host.executionNative=[&](const std::string&,const std::vector<Vm::Evaluation>& arguments,const Vm::Function*,Vm::Execution&) {
            const auto large=Value::IntArray(std::vector<std::int32_t>(8192u,13));
            arguments.at(0).reference->write(large);++completedWrites;
            arguments.at(0).reference->write(large);++completedWrites;
            return Vm::Evaluation{};
        };
        Vm::Limits limits;limits.retainedBytes=40u*1024u;
        const auto result=Vm::Execute(host,function,"Self",{},limits);
        Failed(result,Vm::Status::Budget,host,"Native replacement arrays and rollback snapshots share retained budget");
        Require(completedWrites==1u && Vm::Equal(host.Load("Self",output),zero),
            "Repeated native array growth bypassed allocation accounting or rollback");
    }
    std::cout<<"Generated whole native INT-array value, rollback and allocation contracts: "
        <<checks-initialChecks<<" checks and "<<rejections-initialRejections<<" rejection controls passed.\n";
}
void ExecutionContracts() {
    RotatorDirectionContracts();ScalarAndLocalContracts();ParametersAndReturns();DecalNativeInputContracts();AssignmentArraysAndStructs();LazyAndContextContracts();
    OptionalAndReferenceTables();ControlAndFailureContracts();BudgetAndNestedRollback();IntegerIncrementContracts();CastContracts();SwitchContracts();DynamicIntArrayContracts();
}
void TransactionContracts() {
    const auto initialChecks=checks,initialRejections=rejections;
    const auto declaration=[](const std::string& name,std::uint16_t index) {
        auto result=Function(name,{});result.flags=0x400u;result.nativeIndex=index;return result;
    };
    const auto externalReference=[](Value& value) {
        auto alias=std::make_shared<Vm::Reference>();alias->zero=Value::Integer(0);
        alias->read=[&value] {return value;};alias->write=[&value](const Value& next) {value=next;};return alias;
    };
    {
        TestHost host;Value output=Value::Integer(3);const auto alias=externalReference(output);
        const auto result=Vm::ExecuteTransaction(host,[&](Vm::Execution& execution) {
            execution.WriteReference(alias,Value::Integer(4));
            execution.WriteReference(alias,Value::Integer(5));
            return output;
        });
        Returned(result,"Trusted host writes use the shared VM journal");
        Require(output.integer==5 && result.value.integer==5 && result.writes==2u &&
            result.instructions==0u && result.function.empty() && host.commitCount==1u,
            "Host writes escaped accounting or invented a bytecode frame");
    }
    for(unsigned mode=0u;mode<5u;++mode) {
        TestHost host;Value output=Value::Integer(3);const auto alias=externalReference(output);
        auto limits=Vm::Limits{};
        if(mode==0u) limits.writes=0u;
        if(mode==1u) limits.writes=1u;
        if(mode==2u) limits.retainedBytes=0u;
        if(mode==3u) limits.retainedBytes=sizeof(Value);
        if(mode==4u) limits.operations=1u;
        unsigned completed{};
        const auto result=Vm::ExecuteTransaction(host,[&](Vm::Execution& execution) {
            execution.WriteReference(alias,Value::Integer(4));++completed;
            execution.WriteReference(alias,Value::Integer(5));++completed;
            return output;
        },limits);
        Failed(result,Vm::Status::Budget,host,"Host write/storage/operation budget restores all earlier writes");
        Require(output.integer==3 && result.instructions==0u && result.function.empty() &&
            completed==((mode==0u || mode==2u) ? 0u : 1u) &&
            result.writes==(mode==0u ? 0u : mode==3u ? 2u : 1u),
            "Budgeted host write failed after incorrect work or retained partial external storage");
    }
    {
        TestHost host;Value output=Value::Integer(3);const auto alias=externalReference(output);
        auto limits=Vm::Limits{};limits.writes=1u;bool caught{},blocked{};
        const auto next=declaration("AfterCaughtHostWrite",1000u);
        const auto result=Vm::ExecuteTransaction(host,[&](Vm::Execution& execution) {
            execution.WriteReference(alias,Value::Integer(4));
            try {execution.WriteReference(alias,Value::Integer(5));} catch(...) {caught=true;}
            const auto preparations=host.preparations.size();
            try {static_cast<void>(execution.CallFunction(next,"Self"));} catch(...) {blocked=true;}
            Require(host.preparations.size()==preparations,"A caught host-write failure entered another function");
            return output;
        },limits);
        Failed(result,Vm::Status::Budget,host,"Caught host-write failure poisons the root transaction");
        Require(caught && blocked && output.integer==3 && !host.effects && host.rng==12345u &&
            result.function.empty() && result.writes==1u,"Caught write failure committed or lost its first diagnostic");
    }
    {
        TestHost host;Value output=Value::Integer(3);auto alias=externalReference(output);alias->write={};
        const auto result=Vm::ExecuteTransaction(host,[&](Vm::Execution& execution) {
            execution.WriteReference(alias,Value::Integer(4));return output;
        });
        Failed(result,Vm::Status::Invalid,host,"Read-only raw host references stay read-only");
        Require(output.integer==3 && result.writes==0u,"Read-only host write changed external storage");
    }
    {
        TestHost host;
        auto first=Function("First",Join({Let(Ref(0x01,1),Int(10)),Let(Ref(0x00,2),Int(11)),Native(1000u),Return({0x17})}));
        const auto firstProperty=host.Property(first,1,"FirstValue",Value::Integer(0));
        first.variables={host.Property(first,2,"Output",Value::Integer(0),0x180u,1u,true)};
        host.Store("FirstReceiver",firstProperty,{Value::Integer(3)});
        const auto second=Function("Second",Join({Let(Ref(0x01,1),Int(20)),Native(1000u),Return({0x17})}),"OtherSource");
        const auto secondProperty=host.Property(second,1,"SecondValue",Value::Integer(0));
        host.Store("SecondReceiver",secondProperty,{Value::Integer(4)});
        const auto firstBytes=first.bytecode,secondBytes=second.bytecode;
        const auto retain=declaration("RetainOutput",1007u);
        Value output=Value::Integer(5);const auto alias=externalReference(output);
        Value firstValue;
        const auto result=Vm::ExecuteTransaction(host,[&](Vm::Execution& execution) {
            firstValue=execution.CallFunction(first,"FirstReceiver",{{{},alias}});
            Require(host.active && host.beginCount==1u && host.commitCount==0u && output.integer==11,
                "A first direct function committed before its transaction completed");
            const auto secondValue=execution.CallFunction(second,"SecondReceiver");
            execution.CallFunction(retain,"SecondReceiver",{{{},alias}});
            Require(host.active && host.commitCount==0u,"A later native call opened/committed a separate transaction");
            return secondValue;
        });
        Returned(result,"Several actual declarations share one transaction");
        Require(result.committed && host.beginCount==1u && host.commitCount==1u && !host.rollbackCount && !host.active,
            "Successful coordinated calls did not commit exactly once");
        Require(firstValue.kind==Vm::Kind::Object && firstValue.text=="FirstReceiver" &&
            result.value.kind==Vm::Kind::Object && result.value.text=="SecondReceiver" &&
            host.Load("FirstReceiver",firstProperty).integer==10 && host.Load("SecondReceiver",secondProperty).integer==20 &&
            output.integer==11 && host.effects==2u && host.rng==((12345u*1664525u+1013904223u)*1664525u+1013904223u),
            "A coordinated function changed receiver, source table, OUT output or native/RNG order");
        Require(first.bytecode==firstBytes && second.bytecode==secondBytes && first.source=="Fixture" && second.source=="OtherSource" &&
            alias->read().integer==11,"Direct execution mutated caller function identity or its unguarded alias");
        bool expired{};try {host.retainedNativeReference->write(Value::Integer(99));} catch(const std::runtime_error&) {expired=true;}
        Require(expired && output.integer==11,"A coordinated native alias remained usable after commit");++rejections;
    }
    {
        TestHost host;
        auto first=Function("Prefix",Join({Let(Ref(0x01,1),Int(17)),Let(Ref(0x00,2),Int(31)),Native(1000u),Return(Int(99))}));
        const auto property=host.Property(first,1,"Value",Value::Integer(0));
        first.variables={host.Property(first,2,"Output",Value::Integer(0),0x180u,1u,true)};
        host.Store("Self",property,{Value::Integer(3)});
        Value output=Value::Integer(4);const auto alias=externalReference(output);
        const auto fail=declaration("FailureAfterWrite",1006u);
        const auto result=Vm::ExecuteTransaction(host,[&](Vm::Execution& execution) {
            execution.CallFunction(first,"Self",{{{},alias}});
            Require(output.integer==31 && host.Load("Self",property).integer==17 && host.effects==1u,
                "The later-call rollback fixture did not execute its real prefix");
            return execution.CallFunction(fail,"Self",{{{},alias}});
        });
        Failed(result,Vm::Status::Unsupported,host,"Later native failure reverses earlier direct calls");
        Require(result.error=="Fixture failure after external out write" && result.function==fail.path && result.offset==0u &&
            result.opcode==0u && result.callStack==std::vector<std::string>{fail.path} && !result.committed &&
            result.value.kind==Vm::Kind::Nothing && output.integer==4 && host.Load("Self",property).integer==3 &&
            host.effects==0u && host.rng==12345u,"Failure lost its exact callee diagnostic or retained a prefix OUT/property/native/RNG write");
    }
    for(const bool unrelatedThrow : {false,true}) {
        TestHost host;const auto prefix=declaration("PrefixEffect",1000u);
        const auto bad=Function("Bad",Join({Native(1000u),Return(Native(4095u))}));
        bool caught{},blocked{};
        const auto result=Vm::ExecuteTransaction(host,[&](Vm::Execution& execution) -> Value {
            execution.CallFunction(prefix,"Self");
            try {execution.CallFunction(bad,"Self");} catch(const std::exception&) {caught=true;}
            const auto preparations=host.preparations.size();
            try {execution.CallFunction(prefix,"Other");} catch(const std::exception&) {blocked=true;}
            Require(host.preparations.size()==preparations,"A poisoned coordinator entered another declaration");
            if(unrelatedThrow) throw std::runtime_error("Unrelated callback exception");
            return Value::Integer(123);
        });
        Failed(result,Vm::Status::Unsupported,host,"Caught callback failure cannot commit a prefix");
        Require(caught && blocked && result.error=="Fixture unknown native 4095" && result.function==bad.path &&
            result.offset==4u && result.opcode==0x6fu && result.callStack==std::vector<std::string>{bad.path} &&
            host.effects==0u && host.rng==12345u,"Poisoning changed the first script failure or committed native effects");
    }
    {
        TestHost host;
        const auto completed=Function("SucceededBeforeCoordinatorFailure",Join({Native(1000u),Return(Int(7))}));
        const auto result=Vm::ExecuteTransaction(host,[&](Vm::Execution& execution) -> Value {
            const auto value=execution.CallFunction(completed,"Self");
            Require(value.integer==7 && host.effects==1u,"Coordinator failure control did not complete its preceding real script");
            throw std::runtime_error("Coordinator schema failure after successful script");
        });
        Failed(result,Vm::Status::Unsupported,host,"A coordinator failure cannot retain a completed script location");
        Require(result.error=="Coordinator schema failure after successful script" && result.function.empty() &&
            result.offset==0u && result.opcode==0u && result.callStack.empty() && result.instructions>0u &&
            host.effects==0u && host.rng==12345u,"Non-bytecode failure retained a stale successful function/PC/stack or committed effects");
    }
    for(unsigned boundary=0u;boundary<4u;++boundary) {
        TestHost host;auto limits=Vm::Limits{};
        const auto completed=Function("SucceededBeforeEventFailure",Join({Native(1000u),Return(Int(7))}));
        const auto event=declaration("ResolvedEventFailure",1000u);
        host.events[{"Self","StartupEvent",false}]=std::make_shared<Vm::Function>(event);
        if(boundary==0u) host.rejectEvent=true;
        if(boundary==1u) limits.operations=1u;
        if(boundary==3u) {auto changed=event;changed.source="WrongSource";host.preparedFunctions[event.path]=changed;}
        const auto result=Vm::ExecuteTransaction(host,[&](Vm::Execution& execution) {
            const auto value=execution.CallFunction(completed,"Self");
            Require(value.integer==7 && host.effects==1u,"Event failure control did not finish its preceding script");
            return execution.CallEvent("Self",boundary==2u ? std::string(limits.stringBytes+1u,'x') : "StartupEvent",false);
        },limits);
        const auto status=boundary==0u ? Vm::Status::Unsupported : boundary==3u ? Vm::Status::Invalid : Vm::Status::Budget;
        Failed(result,status,host,"A coordinator event failure cannot retain a preceding successful script location");
        const std::array<std::string,4> errors{"Fixture rejected event resolution","VM operation entry limit",
            "VM callback identity/name limit","Prepared VM function changed callable identity"};
        Require(result.error==errors[boundary] && result.function==(boundary==3u ? event.path : "") &&
            result.offset==0u && result.opcode==0u && result.callStack.empty() && result.instructions>0u &&
            host.effects==0u && host.rng==12345u,"Event boundary retained a stale script cursor or committed its prefix");
    }
    {
        TestHost host;const auto outer=declaration("NativeBeforeEventResolutionFailure",1008u);host.rejectEvent=true;
        host.executionNative=[](const std::string&,const std::vector<Vm::Evaluation>&,
            const Vm::Function*,Vm::Execution& execution) -> Vm::Evaluation {
            return {execution.CallEvent("InnerReceiver","MissingEvent",false),{}};
        };
        const auto result=Vm::ExecuteTransaction(host,[&](Vm::Execution& execution) {return execution.CallFunction(outer,"Self");});
        Failed(result,Vm::Status::Unsupported,host,"Nested event resolution retains its active native caller");
        Require(result.error=="Fixture rejected event resolution" && result.function==outer.path && result.offset==0u &&
            result.opcode==0u && result.callStack==std::vector<std::string>{outer.path},
            "A nested unresolved event erased its real active caller");
    }
    for(const bool transaction : {false,true}) {
        TestHost host;const auto outer=declaration("SwallowingOuter",1008u);
        const auto bad=Function("InnerFailure",Join({Native(1000u),Return(Native(4095u))}));
        bool caught{};
        host.executionNative=[&](const std::string&,const std::vector<Vm::Evaluation>&,
            const Vm::Function*,Vm::Execution& execution) -> Vm::Evaluation {
            try {execution.CallFunction(bad,"InnerReceiver");} catch(const std::exception&) {caught=true;}
            // Returning a value must not hide the nested failure or pop its
            // original diagnostic frame before the public entry reports it.
            return {Value::Bool(true),{}};
        };
        const auto result=transaction ? Vm::ExecuteTransaction(host,[&](Vm::Execution& execution) {
            return execution.CallFunction(outer,"OuterReceiver");
        }) : Vm::Execute(host,outer,"OuterReceiver");
        Failed(result,Vm::Status::Unsupported,host,"A native cannot swallow a failed nested declaration");
        Require(caught && result.function==bad.path && result.offset==4u && result.opcode==0x6fu &&
            result.callStack==std::vector<std::string>{outer.path,bad.path} && result.error=="Fixture unknown native 4095" &&
            host.effects==0u && host.rng==12345u,"Caught native callback failure lost its first call stack/location or committed effects");
    }
    {
        const auto function=Function("Budget",Return(Int(7)));
        TestHost single;const auto one=Vm::Execute(single,function,"Self");Returned(one,"Single instruction-budget reference");
        TestHost host;auto limits=Vm::Limits{};limits.instructions=one.instructions;
        const auto result=Vm::ExecuteTransaction(host,[&](Vm::Execution& execution) {
            execution.CallFunction(function,"Self");return execution.CallFunction(function,"Self");
        },limits);
        Failed(result,Vm::Status::Budget,host,"Sequential calls share instruction budget");
        Require(result.instructions==one.instructions && result.error=="VM instruction limit",
            "A later direct call reset the instruction budget");
    }
    {
        TestHost host;auto limits=Vm::Limits{};limits.nodes=2u;
        const auto first=Function("NodeFirst",Return(Int(1))),second=Function("NodeSecond",Return(Int(2)));
        const auto result=Vm::ExecuteTransaction(host,[&](Vm::Execution& execution) {
            execution.CallFunction(first,"Self");return execution.CallFunction(second,"Self");
        },limits);
        Failed(result,Vm::Status::Budget,host,"Sequential calls share parsed-node budget");
        Require(result.function==second.path && result.instructions==2u,"Node failure was charged to a reset/incorrect function");
    }
    {
        TestHost host;auto limits=Vm::Limits{};limits.writes=1u;
        const auto function=Function("WriteBudget",Join({Let(Ref(0x01,1),Int(9)),Return()}));
        const auto property=host.Property(function,1,"Value",Value::Integer(0));host.Store("Self",property,{Value::Integer(3)});
        const auto result=Vm::ExecuteTransaction(host,[&](Vm::Execution& execution) {
            execution.CallFunction(function,"Self");return execution.CallFunction(function,"Self");
        },limits);
        Failed(result,Vm::Status::Budget,host,"Sequential calls share write budget");
        Require(result.writes==1u && host.Load("Self",property).integer==3,"Later write budget failure committed the first write");
    }
    {
        TestHost host;const auto native=declaration("RetainedInputBudget",1003u);
        const auto input=Value::Text(Vm::Kind::String,std::string(256u,'x'));
        auto limits=Vm::Limits{};limits.retainedBytes=sizeof(Value)+input.text.size();
        const auto result=Vm::ExecuteTransaction(host,[&](Vm::Execution& execution) {
            const auto first=execution.CallFunction(native,"Self",{{input,{}}});
            Require(first.text==input.text,"Retained-byte control failed before its first call returned");
            return execution.CallFunction(native,"Self",{{input,{}}});
        },limits);
        Failed(result,Vm::Status::Budget,host,"Sequential calls share retained-value budget");
        Require(result.error=="VM retained byte limit" && result.instructions==1u && host.nativeArguments.size()==1u,
            "A later call reset retained storage or performed its native work before the budget guard");
    }
    for(const bool absent : {false,true}) {
        TestHost host;auto limits=Vm::Limits{};limits.operations=2u;
        const auto disabled=Function("DisabledEntry",{0x03});host.disabledFunctions.insert(disabled.path);
        auto alias=std::make_shared<Vm::Reference>();alias->zero=Value::Integer(0);
        alias->read=[]() -> Value {throw std::runtime_error("Ineligible transaction argument read");};
        const auto result=Vm::ExecuteTransaction(host,[&](Vm::Execution& execution) {
            for(unsigned i=0;i<3u;++i) {
                if(absent) execution.CallEvent("Self","Absent",true,{{{},alias}});
                else execution.CallFunction(disabled,"Self",{{{},alias}});
            }
            return Value{};
        },limits);
        Failed(result,Vm::Status::Budget,host,"Absent/ineligible attempts share bounded operation entries");
        Require(result.error=="VM operation entry limit" && result.instructions==0u && result.writes==0u &&
            host.preparations.empty() && host.observations.size()==(absent ? 2u : 0u),
            "Suppressed direct entries consumed arguments or evaded the operation guard");
    }
    {
        TestHost host;auto limits=Vm::Limits{};limits.operations=1u;
        const auto disabled=Function("MixedDisabled",{0x03});host.disabledFunctions.insert(disabled.path);
        const auto result=Vm::ExecuteTransaction(host,[&](Vm::Execution& execution) {
            execution.CallEvent("Self","Missing",false);return execution.CallFunction(disabled,"Self");
        },limits);
        Failed(result,Vm::Status::Budget,host,"Named events and direct functions use one operation counter");
        Require(host.observations.size()==1u && host.preparations.empty(),"Mixed entry budget was separate per API");
    }
    for(const bool fail : {false,true}) {
        TestHost host;const auto outer=declaration("NestedOuter",1008u),inner=declaration("NestedInnerEffect",1000u);
        auto event=Function("RealEvent",Join({Let(Ref(0x01,1),Int(27)),Let(Ref(0x00,2),Int(18)),Native(1000u),Return({0x17})}),"EventSource");
        const auto property=host.Property(event,1,"EventValue",Value::Integer(0));host.Store("EventReceiver",property,{Value::Integer(6)});
        event.variables={host.Property(event,2,"Output",Value::Integer(0),0x180u,1u,true)};
        host.events[{"EventReceiver","StartupEvent",true}]=std::make_shared<Vm::Function>(event);
        Value output=Value::Integer(8);const auto alias=externalReference(output);
        host.executionNative=[&](const std::string& receiver,const std::vector<Vm::Evaluation>& arguments,
            const Vm::Function* actual,Vm::Execution& execution) -> Vm::Evaluation {
            Require(receiver=="OuterReceiver" && actual && actual->path==outer.path,
                "Nested native callback lost its original receiver/declaration");
            const auto value=execution.CallEvent("EventReceiver","StartupEvent",true,arguments);
            execution.CallFunction(inner,"InnerReceiver");
            Require(host.beginCount==1u && !host.commitCount && host.active,"Nested callbacks opened another host transaction");
            return {value,{}};
        };
        const auto bad=declaration("AfterNestedFailure",4095u);
        const auto result=Vm::ExecuteTransaction(host,[&](Vm::Execution& execution) {
            const auto value=execution.CallFunction(outer,"OuterReceiver",{{{},alias}});
            if(fail) return execution.CallFunction(bad,"FailureReceiver");
            return value;
        });
        if(fail) {
            Failed(result,Vm::Status::Unsupported,host,"Nested direct/event calls join the outer rollback");
            Require(host.Load("EventReceiver",property).integer==6 && output.integer==8 && !host.effects && host.rng==12345u,
                "Failed later operation retained nested callback properties/OUT/native/RNG writes");
        } else {
            Returned(result,"Nested direct native and event calls share one Machine");
            Require(result.value.text=="EventReceiver" && host.Load("EventReceiver",property).integer==27 && output.integer==18 &&
                host.effects==2u && host.beginCount==1u && host.commitCount==1u && host.rollbackCount==0u &&
                host.nativeReceivers==std::vector<std::string>{"OuterReceiver","EventReceiver","InnerReceiver"},
                "Nested calls changed source/receiver identities or committed separately");
        }
    }
    {
        TestHost host;const auto recursive=declaration("RecursiveDirect",1008u),effect=declaration("RecursiveEffect",1000u);
        auto limits=Vm::Limits{};limits.callDepth=2u;
        host.executionNative=[&](const std::string& receiver,const std::vector<Vm::Evaluation>&,
            const Vm::Function*,Vm::Execution& execution) -> Vm::Evaluation {
            execution.CallFunction(effect,receiver);return {execution.CallFunction(recursive,receiver),{}};
        };
        const auto result=Vm::ExecuteTransaction(host,[&](Vm::Execution& execution) {
            return execution.CallFunction(recursive,"Self");
        },limits);
        Failed(result,Vm::Status::Budget,host,"Direct callback recursion uses the current call-depth budget");
        Require(result.error=="VM call depth limit" && result.callStack==std::vector<std::string>{recursive.path,recursive.path} &&
            host.effects==0u && host.rng==12345u,"Nested direct-call depth failure escaped its caller or rollback");
    }
    {
        TestHost host;auto disabled=Function("Suppressed",{0x03});disabled.flags=0x8u;
        host.disabledFunctions.insert(disabled.path);host.events[{"Self","Suppressed",false}]=std::make_shared<Vm::Function>(disabled);
        auto alias=std::make_shared<Vm::Reference>();alias->zero=Value::Integer(0);
        alias->read=[]() -> Value {throw std::runtime_error("Suppressed entry loaded an external argument");};
        const auto result=Vm::ExecuteTransaction(host,[&](Vm::Execution& execution) {
            execution.CallFunction(disabled,"Self",{{{},alias}});
            execution.CallEvent("Self","Suppressed",false,{{{},alias}});
            execution.CallFunction(disabled,"",{{{},alias}});
            host.disabledFunctions.clear();
            return execution.CallFunction(declaration("NowEligible",1000u),"Self");
        });
        Returned(result,"Every direct/event entry rechecks eligibility before arguments/preparation");
        Require(host.preparations==std::vector<std::string>{"Self:Fixture.NowEligible"} && host.effects==1u &&
            result.instructions==1u && result.writes==0u,"Suppressed declarations performed latent/argument/preparation work");
    }
    {
        TestHost host;const auto identity=declaration("ChangingEligibility",1000u);
        auto alias=std::make_shared<Vm::Reference>();alias->zero=Value::Integer(0);
        alias->read=[]() -> Value {throw std::runtime_error("Changed eligibility read a later argument");};
        const auto result=Vm::ExecuteTransaction(host,[&](Vm::Execution& execution) {
            execution.CallFunction(identity,"Self");host.disabledFunctions.insert(identity.path);
            return execution.CallFunction(identity,"Self",{{{},alias}});
        });
        Returned(result,"A repeated direct declaration rechecks changed eligibility");
        Require(result.value.kind==Vm::Kind::Nothing && host.effects==1u && result.instructions==1u &&
            host.preparations==std::vector<std::string>{"Self:"+identity.path},
            "A later call reused cached eligibility or prepared/loaded its suppressed argument");
    }
    for(const bool changeNative : {false,true}) {
        TestHost host;const auto prefix=declaration("PreparedPrefix",1000u),identity=declaration("PreparedIdentity",1000u);
        auto changed=identity;if(changeNative) changed.nativeIndex=1001u;else changed.source="WrongSource";
        host.preparedFunctions[identity.path]=changed;
        const auto result=Vm::ExecuteTransaction(host,[&](Vm::Execution& execution) {
            execution.CallFunction(prefix,"Self");return execution.CallFunction(identity,"Self");
        });
        Failed(result,Vm::Status::Invalid,host,"Direct preparation cannot change callable source/native identity");
        Require(result.error=="Prepared VM function changed callable identity" && host.effects==0u && host.rng==12345u &&
            identity.source=="Fixture" && identity.nativeIndex==1000u,"Invalid preparation mutated its declaration or committed a prefix");
    }
    {
        TestHost host;host.rejectCommit=true;const auto effect=declaration("BeforeCommit",1000u),write=declaration("CommitOut",1004u);
        Value output=Value::Integer(4);const auto alias=externalReference(output);
        const auto result=Vm::ExecuteTransaction(host,[&](Vm::Execution& execution) {
            execution.CallFunction(effect,"Self");return execution.CallFunction(write,"Self",{{{},alias}});
        });
        Failed(result,Vm::Status::Unsupported,host,"Rejected final commit restores Machine and host journals");
        Require(result.error=="Fixture rejected commit" && output.integer==4 && !host.effects && host.rng==12345u,
            "Commit failure retained external OUT/native/RNG writes");
    }
    {
        TestHost host;const auto retain=declaration("OwnedOutputRetain",1007u),write=declaration("OwnedOutputWrite",1004u);
        std::weak_ptr<Value> output;
        const auto result=Vm::ExecuteTransaction(host,[&](Vm::Execution& execution) -> Value {
            // Both closures own the backing value. The callback's local
            // shared_ptr dies before Machine performs its rollback.
            auto storage=std::make_shared<Value>(Value::Integer(4));output=storage;
            auto alias=std::make_shared<Vm::Reference>();alias->zero=Value::Integer(0);
            alias->read=[storage] {return *storage;};alias->write=[storage](const Value& value) {*storage=value;};
            execution.CallFunction(retain,"Self",{{{},alias}});
            execution.CallFunction(write,"Self",{{{},alias}});
            Require(storage->integer==9,"Owned callback-local OUT control did not write its storage");
            throw std::runtime_error("Owned callback failed after its OUT write");
        });
        Failed(result,Vm::Status::Unsupported,host,"Owned callback-local OUT storage survives callback unwinding");
        Require(result.error=="Owned callback failed after its OUT write" && output.lock() && output.lock()->integer==4,
            "Rollback used dead callback-local OUT storage or failed to restore it");
        host.retainedNativeReference.reset();host.retainedNativeElement.reset();
        Require(output.expired(),"A finished VM transaction leaked its owned OUT storage/journal");
    }
    for(unsigned invalid=0u;invalid<5u;++invalid) {
        TestHost host;std::function<Value(Vm::Execution&)> callback;
        if(invalid!=0u) callback=[invalid](Vm::Execution&) -> Value {
            if(invalid==1u) throw std::runtime_error("Coordinator failure");
            if(invalid==2u) throw 42;
            if(invalid==3u) return Value::Float(std::numeric_limits<float>::infinity());
            return Value::Text(Vm::Kind::String,std::string(8193u,'x'));
        };
        const auto result=Vm::ExecuteTransaction(host,callback);
        const auto status=invalid==1u ? Vm::Status::Unsupported : invalid==4u ? Vm::Status::Budget : Vm::Status::Invalid;
        Failed(result,status,host,"Coordinator failure/return validation uses ordinary transaction policy");
        const std::array<std::string,5> errors{"Missing VM transaction callback","Coordinator failure","Unknown VM transaction failure",
            "Non-finite VM float","VM value storage limit"};
        Require(result.error==errors[invalid] && !result.committed && result.value.kind==Vm::Kind::Nothing,
            "Coordinator failure produced an incorrect status/error or return value");
    }
    {
        TestHost host;host.rejectBegin=true;bool called{};
        const auto result=Vm::ExecuteTransaction(host,[&](Vm::Execution&) {called=true;return Value{};});
        Require(result.status==Vm::Status::Unsupported && result.error=="Fixture rejected begin" && !result.committed && !called &&
            host.beginCount==0u && host.commitCount==0u && host.rollbackCount==0u && !host.active,
            "A failed Begin executed the coordinator or rolled back an unentered host transaction");++rejections;
    }
    {
        TestHost host;auto limits=Vm::Limits{};limits.operations=0u;
        const auto result=Vm::ExecuteTransaction(host,[](Vm::Execution&) {return Value::Integer(9);},limits);
        Returned(result,"A coordinator with no script entries can commit bounded host-only work");
        Require(result.value.integer==9 && result.instructions==0u && result.writes==0u && result.function.empty() &&
            host.beginCount==1u && host.commitCount==1u && host.rollbackCount==0u,
            "An empty operation sequence invented a script frame or failed its single final commit");
    }
    std::cout<<"Generated multi-operation VM transactions, shared budgets and callback rollback: "
        <<checks-initialChecks<<" checks and "<<rejections-initialRejections<<" rejection controls passed.\n";
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
        TransactionContracts();
        ProgramAndEligibilityContracts();
        VectorNativeContracts();
        StartupPrimitiveContracts();
        IteratorContracts();
        std::cout<<"Normalized original-style serialization and bounded script execution: "<<checks<<" checks and "<<rejections<<" rejection controls passed.\n";
        return 0;
    }catch(const std::exception& error) {std::cerr<<error.what()<<"\n";return 1;}
}
