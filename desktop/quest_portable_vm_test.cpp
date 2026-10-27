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
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <limits>
#include <map>
#include <new>
#include <set>
#include <stdexcept>
#include <tuple>

namespace VmPublicationAllocationFault {
thread_local bool denied{};
thread_local std::size_t attempts{};
#if defined(__GNUC__)
__attribute__((noinline))
#endif
void* Allocate(std::size_t bytes) {
    if(denied) {++attempts;throw std::bad_alloc();}
    if(auto* memory=std::malloc(bytes?bytes:1u)) return memory;
    throw std::bad_alloc();
}
}
void* operator new(std::size_t bytes) {return VmPublicationAllocationFault::Allocate(bytes);}
void* operator new[](std::size_t bytes) {return VmPublicationAllocationFault::Allocate(bytes);}
#if defined(__GNUC__)
__attribute__((noinline))
#endif
void operator delete(void* memory) noexcept {std::free(memory);}
#if defined(__GNUC__)
__attribute__((noinline))
#endif
void operator delete(void* memory,std::size_t) noexcept {std::free(memory);}
#if defined(__GNUC__)
__attribute__((noinline))
#endif
void operator delete[](void* memory) noexcept {std::free(memory);}
#if defined(__GNUC__)
__attribute__((noinline))
#endif
void operator delete[](void* memory,std::size_t) noexcept {std::free(memory);}

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
Bytes Context(const Bytes& object,const Bytes& child,std::uint8_t zeroFill=0u,std::uint8_t opcode=0x19u) {
    auto b=Join({{opcode},object});U16(b,static_cast<std::uint16_t>(child.size()));b.push_back(zeroFill);return Join({b,child});
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
    std::function<std::optional<Value>(std::uint16_t,const std::string&,std::size_t,
        const Vm::Function*)> argumentSeed;
    std::function<std::optional<Vm::ResultLayout>(std::uint16_t,const std::string&,std::size_t,
        const Vm::Function*)> uninitializedArgumentLayout;
    std::function<bool(std::uint16_t,const std::string&,const Vm::Function*)> hasSequentialSeeds;
    std::function<bool(std::uint16_t,const std::string&,std::size_t,const Vm::Function*)> materializeOmission;
    std::function<std::optional<Value>(std::uint16_t,const std::string&,std::size_t,
        const Vm::Function*,const std::vector<Value>&)> sequentialArgumentSeed;
    std::function<std::optional<Value>(std::uint16_t,const std::string&,const Vm::Function*)> legacyNativeContext;
    std::function<std::optional<Value>(std::uint16_t,const std::string&,const Vm::Function*,const Vm::Function*)> nativeCallSiteContext;
    std::function<Vm::Evaluation(std::uint16_t,const std::string&,const std::vector<Vm::Evaluation>&,
        const Vm::Function*,Vm::Execution&,const std::optional<Value>&)> contextualNative;
    std::map<Slot,std::vector<Value>> storage,saved;
    struct CastClass {std::string parent,name;};
    std::map<std::string,CastClass> castClasses;
    std::map<std::string,std::string> castInstances;
    std::optional<Value> castResultOverride;
    std::size_t castClassResolutions{},castCalls{},objectResolutions{};
    std::vector<std::string> observations;
    std::vector<std::vector<Value>> nativeArguments;
    std::vector<std::uint16_t> nativeIndices;
    std::vector<std::string> nativeReceivers,nativeDeclarations;
    std::map<std::string,std::string> objectDisplayPaths;
    std::map<std::string,std::string> defaultObjects;
    std::vector<std::string> defaultSelections;
    std::vector<std::string> objectStringLookups;
    std::shared_ptr<Vm::Reference> retainedNativeReference,retainedNativeElement;
    std::size_t referenceReads{};
    Vm::ResultLayout suppliedReferenceLayout{Vm::ResultLayout::Unknown};
    std::size_t beginCount{},commitCount{},rollbackCount{},effects{},savedEffects{};
    std::uint32_t rng{12345},savedRng{};
    bool active{};
    bool rejectBegin{},rejectCommit{},rejectEvent{};
    bool denyAllocationsAfterCommit{};
    std::vector<Value> iteratorValues{Value::Integer(1),Value::Integer(2),Value::Integer(3)};
    std::size_t iteratorCreates{},iteratorDestroys{},iteratorNextCalls{},iteratorBytes{128u};
    std::optional<QuestVr::StateObject> state,savedState;
    std::map<std::string,QuestVr::StateObject> phaseStates,savedPhaseStates;
    std::map<std::string,Vm::Function> statePrograms;
    std::vector<std::string> stateProgramReads;
    std::function<bool(const std::string&,float,Vm::Execution&)> statePoll;
    std::function<bool(const std::string&)> stateDeleted;
    QuestVr::NativeWork::Budget phaseWork;
    bool trackPhaseWork{};
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
        savedState=state;savedPhaseStates=phaseStates;savedStateRevision=stateRevision;++beginCount;
    }
    void BeginRoot(const Vm::Limits& limits) override {
        if(trackPhaseWork)phaseWork.Reset(limits.nativeWork);
        Begin();
    }
    std::optional<QuestVr::NativeWork::Usage> ReadNativeWork() const noexcept override {
        return trackPhaseWork?std::optional<QuestVr::NativeWork::Usage>(phaseWork.Read()):std::nullopt;
    }
    void Commit() override {
        if(!active) throw std::runtime_error("Commit outside transaction");
        if(rejectCommit) throw std::runtime_error("Fixture rejected commit");
        active=false;++commitCount;
        if(denyAllocationsAfterCommit) {
            VmPublicationAllocationFault::attempts=0u;VmPublicationAllocationFault::denied=true;
        }
    }
    void Rollback() noexcept override {
        storage.swap(saved);effects=savedEffects;rng=savedRng;
        state.swap(savedState);phaseStates.swap(savedPhaseStates);stateRevision=savedStateRevision;active=false;++rollbackCount;
    }
    const QuestVr::StateObject* ReadState(const std::string& receiver) override {
        const auto found=phaseStates.find(receiver);
        return found!=phaseStates.end()?&found->second:state?&*state:nullptr;
    }
    QuestVr::StateObject* MutableState(const std::string& receiver) override {
        const auto found=phaseStates.find(receiver);
        return found!=phaseStates.end()?&found->second:state?&*state:nullptr;
    }
    bool IsStateDeleted(const std::string& receiver) override {return stateDeleted&&stateDeleted(receiver);}
    bool PollState(const std::string& receiver,float elapsed,Vm::Execution& execution) override {
        return statePoll?statePoll(receiver,elapsed,execution):false;
    }
    std::uint64_t StateLocalRevision(const std::string&) override {return stateRevision;}
    std::shared_ptr<const Vm::Function> StateProgram(const std::string&,const std::string& path) override {
        stateProgramReads.push_back(path);
        return std::make_shared<Vm::Function>(statePrograms.at(path));
    }
    std::shared_ptr<Vm::Reference> StateVariable(const std::string& receiver,const Vm::Property& property) override {
        return Reference({receiver,Vm::Scope::Instance,property.key},property);
    }
    std::unique_ptr<Vm::Iterator> CreateIterator(const std::uint16_t index,const std::string& receiver,
        const std::vector<Vm::Evaluation>& arguments,const Vm::Function* declaration) override {
        if(index!=1040u || arguments.empty() || !arguments[0].reference)
            throw std::runtime_error("Fixture invalid iterator factory/out binding");
        ++iteratorCreates;observations.push_back("factory:"+receiver);
        std::vector<Value> values;for(const auto& arg:arguments) values.push_back(arg.Load());
        nativeArguments.push_back(values);nativeReceivers.push_back(receiver);nativeIndices.push_back(index);
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
    std::string ResolveObject(const Vm::Function& f,std::int32_t index) override {++objectResolutions;return objects.at({f.source,index});}
    std::string ObjectString(const std::string& identity) override {
        objectStringLookups.push_back(identity);
        const auto found=objectDisplayPaths.find(identity);
        if(found==objectDisplayPaths.end()) throw std::runtime_error("Fixture object has no actual path identity");
        return found->second;
    }
    std::string ClassDefaultObject(const std::string& identity) override {
        defaultSelections.push_back(identity);
        const auto found=defaultObjects.find(identity);
        if(found==defaultObjects.end()) throw std::runtime_error("ClassContext requires class-default object identity");
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
    std::optional<Value> NativeArgumentSeed(std::uint16_t index,const std::string& receiver,
        std::size_t slot,const Vm::Function* declaration) override {
        return argumentSeed ? argumentSeed(index,receiver,slot,declaration) : std::nullopt;
    }
    std::optional<Vm::ResultLayout> NativeArgumentUninitializedLayout(std::uint16_t index,const std::string& receiver,
        std::size_t slot,const Vm::Function* declaration) override {
        return uninitializedArgumentLayout ? uninitializedArgumentLayout(index,receiver,slot,declaration) : std::nullopt;
    }
    bool HasSequentialNativeArgumentSeeds(std::uint16_t index,const std::string& receiver,
        const Vm::Function* declaration) override {
        return hasSequentialSeeds ? hasSequentialSeeds(index,receiver,declaration) : false;
    }
    bool MaterializeNativeArgumentOmission(std::uint16_t index,const std::string& receiver,
        std::size_t slot,const Vm::Function* declaration) override {
        return materializeOmission ? materializeOmission(index,receiver,slot,declaration) : false;
    }
    std::optional<Value> SequentialNativeArgumentSeed(std::uint16_t index,const std::string& receiver,
        std::size_t slot,const Vm::Function* declaration,const std::vector<Value>& prefix) override {
        return sequentialArgumentSeed ? sequentialArgumentSeed(index,receiver,slot,declaration,prefix) : std::nullopt;
    }
    Vm::Property Property(const Vm::Function& f,std::int32_t ref,const std::string& name,Value zero,
        std::uint32_t flags=0,std::size_t dimension=1,bool local=false,
        Vm::ResultLayout layout=Vm::ResultLayout::Unknown) {
        Vm::Property p{local ? f.path+"."+name : "Fixture.Actor."+name,name,std::move(zero),flags,dimension,layout};
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
        ref->resultLayout=suppliedReferenceLayout;
        ref->read=[this,slot,index] {++referenceReads;return storage.at(slot).at(index);};
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
        nativeArguments.push_back(values);nativeReceivers.push_back(receiver);nativeIndices.push_back(index);
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
        if((index==261u || index==262u || index==283u || index==264u || index==548u || index==1008u || index==0u || index==1410u || index==1413u || index==1415u || index==1416u || index==1420u || index==1421u || index==1422u ||
            index==1430u || index==1431u || index==1432u || index==1433u || index==1438u || index==1454u || index==1461u || index==1465u || index==1467u || index==1475u || index==1476u ||
            index==1556u || index==1557u || index==1558u || index==1572u || index==1573u ||
            index==1270u || index==1272u || index==1486u || index==1483u || index==1550u ||
            index==1242u || index==1280u || index==1281u || index==1469u ||
            index==1535u || index==1538u || index==1539u || index==1540u ||
            index==1541u || index==1544u || index==1545u) && executionNative) {
            std::vector<Value> values;for(const auto& argument:arguments) values.push_back(index==1465u||index==1280u?argument.value:argument.Load());
            nativeArguments.push_back(std::move(values));nativeReceivers.push_back(receiver);nativeIndices.push_back(index);
            nativeDeclarations.push_back(declaration ? declaration->path : "");
            return executionNative(receiver,arguments,declaration,execution);
        }
        return Native(index,receiver,arguments,declaration);
    }
    std::optional<Value> CaptureNativeContext(std::uint16_t index,const std::string& receiver,
        const Vm::Function* declaration) override {
        return legacyNativeContext ? legacyNativeContext(index,receiver,declaration) : std::nullopt;
    }
    std::optional<Value> CaptureNativeCallSiteContext(std::uint16_t index,const std::string& receiver,
        const Vm::Function* declaration,const Vm::Function* caller) override {
        return nativeCallSiteContext ? nativeCallSiteContext(index,receiver,declaration,caller) :
            Vm::Host::CaptureNativeCallSiteContext(index,receiver,declaration,caller);
    }
    Vm::Evaluation NativeWithContext(std::uint16_t index,const std::string& receiver,
        const std::vector<Vm::Evaluation>& arguments,const Vm::Function* declaration,Vm::Execution& execution,
        const std::optional<Value>& context) override {
        return contextualNative ? contextualNative(index,receiver,arguments,declaration,execution,context) :
            NativeWithExecution(index,receiver,arguments,declaration,execution);
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
    for(const auto& [a,b,expected] : std::vector<std::array<float,3>>{{0.3f,0.25f,0.25f},{0.3f,0.5f,0.3f},{-7.0f,3.0f,-7.0f},
            {std::numeric_limits<float>::lowest(),std::numeric_limits<float>::max(),std::numeric_limits<float>::lowest()}})
        Require(evaluate(Native(244u,{Real(a),Real(b)}),"FMin finite operands").floating==expected,"FMin selected wrong finite operand");
    for(const auto first : {0.0f,-0.0f}) {
        const auto value=evaluate(Native(244u,{Real(first),Real(-first)}),"FMin first operand wins signed-zero tie");
        Require(std::bit_cast<std::uint32_t>(value.floating)==std::bit_cast<std::uint32_t>(first),"FMin discarded first operand on equal zeros");
    }
    for(const bool declared : {false,true}) {
        TestHost host;
        const auto mutation=Native(1060u,{Ref(0x01,1),Real(-100.0f),Real(5.0f)});
        auto expression=Native(244u,{Ref(0x01,1),mutation});
        if(declared) {
            auto declaration=Function("FMin",{},"NativeSource");declaration.flags=0x2401u;declaration.nativeIndex=244u;
            declaration.variables={{"FMin.A","A",Value::Float(0),0x80u,1u},{"FMin.B","B",Value::Float(0),0x80u,1u}};
            host.finals[{"Fixture",9}]=std::make_shared<Vm::Function>(declaration);
            expression=Call(0x1c,9,{Ref(0x01,1),mutation});
        }
        const auto function=Function("FMinInputSnapshot",Return(expression));
        const auto property=host.Property(function,1,"Number",Value::Float(0));host.Store("Self",property,{Value::Float(7.0f)});
        const auto result=Vm::Execute(host,function,"Self");Returned(result,"FMin original sequential input copies");
        Require(result.value.kind==Vm::Kind::Float && result.value.floating==5.0f && host.Load("Self",property).floating==-100.0f &&
            result.writes==1u,"FMin reloaded first argument after second mutated it");
    }
    for(const auto& [a,b,expected] : std::vector<std::array<float,3>>{{0.3f,0.25f,0.3f},{0.3f,0.5f,0.5f},
            {-7.0f,3.0f,3.0f},{3.0f,-7.0f,3.0f},{-7.0f,-3.0f,-3.0f},{-3.0f,-7.0f,-3.0f},
            {0.1f,0.1f,0.1f},{std::numeric_limits<float>::lowest(),std::numeric_limits<float>::max(),std::numeric_limits<float>::max()},
            {std::numeric_limits<float>::max(),std::numeric_limits<float>::lowest(),std::numeric_limits<float>::max()},
            {std::numeric_limits<float>::denorm_min(),0.0f,std::numeric_limits<float>::denorm_min()}})
        Require(evaluate(Native(245u,{Real(a),Real(b)}),"FMax finite ordered/reversed operands").floating==expected,
            "FMax selected the wrong finite operand");
    for(const auto first : {0.0f,-0.0f}) {
        const auto value=evaluate(Native(245u,{Real(first),Real(-first)}),"FMax first operand wins signed-zero tie");
        Require(std::bit_cast<std::uint32_t>(value.floating)==std::bit_cast<std::uint32_t>(first),
            "FMax discarded first operand on equal zeros");
    }
    for(const bool declared : {false,true}) {
        TestHost host;
        const auto mutation=Native(1060u,{Ref(0x01,1),Real(100.0f),Real(5.0f)});
        auto expression=Native(245u,{Ref(0x01,1),mutation});
        if(declared) {
            auto declaration=Function("FMax",{},"Core.Object");declaration.flags=0x2401u;declaration.nativeIndex=245u;
            declaration.variables={{"FMax.A","A",Value::Float(0),0x80u,1u},{"FMax.B","B",Value::Float(0),0x80u,1u}};
            host.finals[{"Fixture",9}]=std::make_shared<Vm::Function>(declaration);
            expression=Call(0x1c,9,{Ref(0x01,1),mutation});
        }
        const auto function=Function("FMaxInputSnapshot",Return(expression));
        const auto property=host.Property(function,1,"Number",Value::Float(0));host.Store("Self",property,{Value::Float(7.0f)});
        const auto result=Vm::Execute(host,function,"Self");Returned(result,"FMax original sequential input copies");
        Require(result.value.kind==Vm::Kind::Float && result.value.floating==7.0f && host.Load("Self",property).floating==100.0f &&
            result.writes==1u,"FMax reloaded first argument after second mutated it");
        Require(host.observations==std::vector<std::string>{"variable:Self:Number","variable:Self:Number"},
            "FMax changed left-to-right argument evaluation or repeated a source expression");
    }
    for(const auto& [v,a,b,expected] : std::vector<std::array<float,4>>{{5,0,10,5},{-1,0,10,0},{11,0,10,10},{5,10,0,10},{15,10,0,0}})
        Require(evaluate(Native(246u,{Real(v),Real(a),Real(b)}),"FClamp ordered/reversed bounds").floating==expected,
            "FClamp sorted bounds or selected wrong branch");
    const std::array<std::int32_t,9> clampInputs{{std::numeric_limits<std::int32_t>::min(),
        std::numeric_limits<std::int32_t>::min()+1,-10,-1,0,1,7,
        std::numeric_limits<std::int32_t>::max()-1,std::numeric_limits<std::int32_t>::max()}};
    for(const auto value:clampInputs) for(const auto low:clampInputs) for(const auto high:clampInputs) {
        const auto expected=value<low ? low : value>=high ? high : value;
        expectInt(Native(251u,{Int(value),Int(low),Int(high)}),expected,"Clamp signed32 ordered/reversed/equal boundaries");
    }
    expectInt(Native(251u,{Bytes{0x24,255u},Int(0),Int(7)}),7,"Clamp byte input fills zero-initialized Int destination");
    expectInt(Native(251u,{{0x0b},Int(-5),Int(5)}),0,"Clamp Nothing value retains initialized zero");
    expectInt(Native(251u,{Int(-2),{0x0b},Int(10)}),0,"Clamp Nothing lower bound retains initialized zero");
    expectInt(Native(251u,{Int(2),Int(-10),{0x0b}}),0,"Clamp Nothing upper bound retains initialized zero");
    expectInt(Native(251u,{Join({{0x44},Real(5.9f)}),Int(0),Int(7)}),5,"Clamp actual FloatToInt conversion input");
    for(const bool declared:{false,true}) for(const bool mutateLower:{false,true}) {
        TestHost host;
        const auto mutation=Native(1060u,{Ref(0x01,1),Int(100),Int(mutateLower ? 10 : 0)});
        auto expression=mutateLower ? Native(251u,{Int(5),Ref(0x01,1),mutation}) :
            Native(251u,{Ref(0x01,1),mutation,Int(10)});
        if(declared) {
            auto declaration=Function("Clamp",{},"Core.Object");declaration.flags=0x2401u;declaration.nativeIndex=251u;
            declaration.variables={{"Clamp.V","V",Value::Integer(0),0x80u,1u},
                {"Clamp.A","A",Value::Integer(0),0x80u,1u},{"Clamp.B","B",Value::Integer(0),0x80u,1u}};
            host.finals[{"Fixture",9}]=std::make_shared<Vm::Function>(declaration);
            expression=mutateLower ? Call(0x1c,9,{Int(5),Ref(0x01,1),mutation}) :
                Call(0x1c,9,{Ref(0x01,1),mutation,Int(10)});
        }
        const auto function=Function("ClampInputSnapshot",Return(expression));
        const auto property=host.Property(function,1,"Number",Value::Integer(0));
        host.Store("Self",property,{Value::Integer(mutateLower ? 0 : 5)});
        const auto result=Vm::Execute(host,function,"Self");Returned(result,"Clamp original sequential Int input copies");
        Require(result.value.kind==Vm::Kind::Int && result.value.integer==5 && host.Load("Self",property).integer==100 &&
            result.writes==1u,"Clamp reloaded an earlier Int input after a later argument mutated it");
        Require(host.observations==std::vector<std::string>{"variable:Self:Number","variable:Self:Number"},
            "Clamp repeated or reordered its argument expressions");
    }
    {
        TestHost host;auto function=Function("ClampArgumentOrder",Return(Native(251u,{
            Native(1001u),Native(1002u),Native(1060u,{Ref(0x01,1),Int(100),Int(10)})})));
        const auto property=host.Property(function,1,"Number",Value::Integer(0));host.Store("Self",property,{Value::Integer(0)});
        const auto result=Vm::Execute(host,function,"Self");Returned(result,"Clamp signed branch and argument order");
        Require(result.value.integer==23 && host.observations==std::vector<std::string>{"lhs","rhs","variable:Self:Number"},
            "Clamp evaluated V/A/B out of order or sorted reversed bounds");
    }
    for(const bool budgetFailure:{false,true}) {
        TestHost host;auto limits=Vm::Limits{};if(budgetFailure) limits.writes=0u;
        const auto function=Function("ClampMutationRollback",Join({Native(1000u),
            Native(251u,{Ref(0x01,1),Native(1060u,{Ref(0x01,1),Int(100),Int(0)}),Int(10)}),Native(4095u),Return()}));
        const auto property=host.Property(function,1,"Number",Value::Integer(0));host.Store("Self",property,{Value::Integer(5)});
        const auto result=Vm::Execute(host,function,"Self",{},limits);
        Failed(result,budgetFailure ? Vm::Status::Budget : Vm::Status::Unsupported,host,"Clamp nested argument effects roll back");
        Require(host.Load("Self",property).integer==5 && host.effects==0u && host.rng==12345u,
            "Clamp input snapshot detached a genuine OUT mutation from rollback");
    }
    const auto zero=evaluate(Native(246u,{Real(-0.0f),Real(0.0f),Real(0.0f)}),"FClamp signed zero retention");
    Require(std::bit_cast<std::uint32_t>(zero.floating)==0x80000000u,"FClamp discarded in-range negative zero");
    for(const auto number : {0.0f,-0.0f,0.25f,-0.5f,1.0f}) {
        const auto value=evaluate(Native(189u,{Real(number)}),"Tan float input/double operation/float result policy");
        const auto expected=static_cast<float>(std::tan(static_cast<double>(number)));
        Require(value.kind==Vm::Kind::Float && std::bit_cast<std::uint32_t>(value.floating)==std::bit_cast<std::uint32_t>(expected),
            "Tan changed argument units or portable float/double promotion policy");
    }
    for(const auto number:{0.0f,-0.0f,0.25f,-0.5f,1.0f,-1.0f,3.1415927f,6.2831855f,
        0x1p63f,-0x1p63f,std::numeric_limits<float>::max(),-std::numeric_limits<float>::max()}) {
        const auto value=evaluate(Native(187u,{Real(number)}),"Sin retail Float/double/Float sequence");
        const float expected=std::abs(number)>=0x1p63f?number:static_cast<float>(std::sin(static_cast<double>(number)));
        Require(value.kind==Vm::Kind::Float && std::bit_cast<std::uint32_t>(value.floating)==std::bit_cast<std::uint32_t>(expected),
            "Sin changed radians, signed zero, x87 range refusal or portable float/double promotion");
    }
    for(const bool declared:{false,true}) for(const float number:{0.0f,-0.0f,0.25f,-0.5f,1.0f,-1.0f,
        std::numeric_limits<float>::denorm_min(),-std::numeric_limits<float>::denorm_min(),
        std::numeric_limits<float>::max(),-std::numeric_limits<float>::max()}) {
        TestHost host;auto native=Function("Abs",{},"Core.Object");native.nativeIndex=186u;native.flags=0x2401u;
        native.variables={{"Abs.A","A",Value::Float(0),0x80u,1u},
            {"Abs.ReturnValue","ReturnValue",Value::Float(0),0x580u,1u}};
        host.finals[{"Fixture",7}]=std::make_shared<Vm::Function>(native);
        const auto caller=Function("RetailAbsSignedBits",Return(declared?Call(0x1c,7,{Real(number)}):Native(186u,{Real(number)})));
        const auto result=Vm::Execute(host,caller,"Self");Returned(result,"Abs conditional sign change, not fabs");
        const auto expected=number<0.0f?-number:number;
        Require(result.value.kind==Vm::Kind::Float && std::bit_cast<std::uint32_t>(result.value.floating)==
            std::bit_cast<std::uint32_t>(expected) && result.writes==0u && host.nativeArguments.empty(),
            "Abs lost signed zero/subnormal bits or performed host-side native work");
        const auto direct=Vm::Execute(host,native,"Self",{{Value::Float(number),{}}});
        Returned(direct,"Direct reflected Abs entry");
        Require(std::bit_cast<std::uint32_t>(direct.value.floating)==std::bit_cast<std::uint32_t>(expected),
            "Direct reflected Abs changed its finite Float result");
    }
    for(const auto& input:{Bytes{0x0bu},Context({0x2au},Native(4095u),0u),Context({0x2au},Native(4095u),1u),
        Context({0x2au},Native(4095u),2u),Context({0x2au},Native(4095u),3u),Context({0x2au},Native(4095u),4u)}) {
        const auto result=evaluate(Native(186u,{input}),"Abs initialized Float+0 result temporary");
        Require(result.kind==Vm::Kind::Float && std::bit_cast<std::uint32_t>(result.floating)==0u,
            "Abs omitted/no-write result did not preserve its initialized positive zero");
    }
    for(const bool budget:{false,true}) {
        TestHost host;auto limits=Vm::Limits{};if(budget) limits.writes=0u;
        const auto caller=Function("AbsArgumentMutationRollback",Join({Native(1000u),
            Native(186u,{Native(1060u,{Ref(0x01,1),Real(100.0f),Real(-0.5f)})}),Native(4095u),Return()}));
        const auto property=host.Property(caller,1,"Number",Value::Float(0));host.Store("Self",property,{Value::Float(7.0f)});
        const auto result=Vm::Execute(host,caller,"Self",{},limits);
        Failed(result,budget?Vm::Status::Budget:Vm::Status::Unsupported,host,"Abs argument effects roll back on later/budget failure");
        Require(host.Load("Self",property).floating==7.0f && host.effects==0u && host.rng==12345u,
            "Abs retained a nested argument mutation or earlier effect on failure");
    }
    Require(evaluate(Native(187u,{{0x0b}}),"Sin Nothing retains native initialized Float0").floating==0.0f,
        "Sin Nothing read an uninitialized native result");
    {
        TestHost host;const auto function=Function("SinRealArgumentSideEffect",Return(Native(187u,{
            Native(1060u,{Ref(0x01,1),Real(100.0f),Real(0.5f)})})));
        const auto property=host.Property(function,1,"Number",Value::Float(0));host.Store("Self",property,{Value::Float(7.0f)});
        const auto result=Vm::Execute(host,function,"Self");Returned(result,"Sin argument evaluates once and genuine mutation commits");
        Require(result.value.floating==static_cast<float>(std::sin(0.5)) && host.Load("Self",property).floating==100.0f &&
            result.writes==1u && host.observations==std::vector<std::string>{"variable:Self:Number"},
            "Sin repeated argument evaluation or lost original argument side effects");
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
    for(const auto bytes:{5u,255u})
        refuse(Native(186u,{Context({0x2au},Native(4095u),static_cast<std::uint8_t>(bytes))}),
            Vm::Status::Unsupported,"Abs overflowing result clearing remains unavailable");
    for(const auto& expression : {Native(125u,{Int(1)}),Native(124u,{String("a"),Int(1)}),Native(186u,{String("1")}),Native(187u,{String("1")}),Native(189u,{String("1")}),
        Native(244u,{Real(1),String("0")}),Native(245u,{Real(1),String("0")}),Native(245u,{String("0"),Real(1)}),
        Native(246u,{Real(1),String("0"),Real(2)}),Native(249u,{String("1"),Int(2)}),
        Native(251u,{String("1"),Int(0),Int(7)}),Native(251u,{Int(1),Real(0),Int(7)}),
        Native(251u,{Int(1),Int(0),{0x28}}),Native(251u,{{0x2a},Int(0),Int(7)}),Join({{0x56},Int(1)})})
        refuse(expression,Vm::Status::Unsupported,"Startup concrete argument types refused");
    for(const auto& expression : {Native(125u,{}),Native(126u,{String("a")}),Native(127u,{String("a")}),Native(186u,{}),Native(186u,{Real(1),Real(2)}),Native(187u,{}),Native(187u,{Real(1),Real(2)}),
        Native(127u,{String("a"),Int(0),Int(1),Int(2)}),Native(168u,{String("a")}),Native(244u,{Real(1)}),
        Native(245u,{}),Native(245u,{Real(1)}),Native(245u,{Real(1),Real(2),Real(3)}),Native(249u,{Int(1)}),
        Native(251u,{}),Native(251u,{Int(1)}),Native(251u,{Int(1),Int(0)}),Native(251u,{Int(1),Int(0),Int(7),Int(9)})})
        refuse(expression,Vm::Status::Invalid,"Startup native arity refused");
    for(const auto nonfinite : {std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity()}) {
        refuse(Native(186u,{Real(nonfinite)}),Vm::Status::Invalid,"Abs nonfinite ingress policy");
        refuse(Native(187u,{Real(nonfinite)}),Vm::Status::Invalid,"Sin nonfinite ingress policy");
        refuse(Native(189u,{Real(nonfinite)}),Vm::Status::Invalid,"Tan nonfinite ingress policy");
        refuse(Native(244u,{Real(nonfinite),Real(1)}),Vm::Status::Invalid,"FMin nonfinite first argument policy");
        refuse(Native(244u,{Real(1),Real(nonfinite)}),Vm::Status::Invalid,"FMin nonfinite second argument policy");
        refuse(Native(245u,{Real(nonfinite),Real(1)}),Vm::Status::Invalid,"FMax nonfinite first argument policy");
        refuse(Native(245u,{Real(1),Real(nonfinite)}),Vm::Status::Invalid,"FMax nonfinite second argument policy");
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
void IntegerToStringContracts() {
    const auto initialChecks=checks,initialRejections=rejections;
    const auto conversion=[](const Bytes& child) {return Join({{0x53u},child});};
    // Independent decimal formatting, including INT_MIN without signed negation.
    const auto decimal=[](const std::int32_t value) {
        auto magnitude=value<0 ? 0u-static_cast<std::uint32_t>(value) : static_cast<std::uint32_t>(value);
        std::string text;
        do {text.push_back(static_cast<char>('0'+magnitude%10u));magnitude/=10u;} while(magnitude);
        if(value<0) text.push_back('-');
        std::reverse(text.begin(),text.end());return text;
    };
    const auto check=[&](const Bytes& operand,const std::string& expected) {
        TestHost host;
        const auto result=Vm::Execute(host,Function("IntegerText",Return(conversion(operand))),"Self");
        Returned(result,"Initialized retail IntToString decimal result");
        Require(result.value.kind==Vm::Kind::String && result.value.text==expected && result.writes==0u &&
            host.nativeIndices.empty(),"IntToString changed signed decimal formatting or invented a native call/write");
    };
    check(Int(std::numeric_limits<std::int32_t>::min()),"-2147483648");
    check(Int(std::numeric_limits<std::int32_t>::max()),"2147483647");
    for(const auto value:{-1000000000,-1000000,-65536,-1000,-100,-10,-1,0,1,9,10,99,100,999,1000,
        65535,65536,16777217,1000000,1000000000}) check(Int(value),decimal(value));
    std::uint32_t random=0x53a931d7u;
    for(std::size_t i=0u;i<1024u;++i) {
        random=random*1664525u+1013904223u;
        const auto value=std::bit_cast<std::int32_t>(random);check(Int(value),decimal(value));
    }
    for(unsigned value=0u;value<256u;++value)
        check({0x24u,static_cast<std::uint8_t>(value)},decimal(static_cast<std::int32_t>(value)));
    check({0x25u},"0");check({0x26u},"1");check({0x0bu},"0");
    // The pinned evaluator also admits defined Float-to-Int truncation. This
    // tests that bounded typed policy, not arbitrary retail storage type-punning.
    for(const auto value:{-2147483648.0f,-2147483520.0f,-16777216.0f,-32768.75f,-10.9f,-1.9f,
        -0.99f,-0.0f,0.0f,0.99f,1.9f,10.9f,32768.75f,16777216.0f,2147483520.0f})
        check(Real(value),decimal(static_cast<std::int32_t>(value)));
    for(const auto bytes:{0u,1u,2u,3u,4u}) {
        TestHost host;
        const auto function=Function("IntegerTextNull",Return(conversion(
            Context({0x2au},Native(1000u),static_cast<std::uint8_t>(bytes)))));
        const auto result=Vm::Execute(host,function,"Self");Returned(result,"Int0 temporary survives absent/partial/full zero result writes");
        Require(result.value.kind==Vm::Kind::String && result.value.text=="0" && host.effects==0u &&
            host.nativeIndices.empty(),"IntToString null operand evaluated its skipped child or lost zero initialization");
    }
    for(const auto bytes:{5u,8u,12u,255u}) {
        TestHost host;
        const auto function=Function("IntegerTextOversizedFill",Join({Native(1000u),Return(conversion(
            Context({0x2au},Int(1),static_cast<std::uint8_t>(bytes))))}));
        const auto result=Vm::Execute(host,function,"Self");
        Failed(result,Vm::Status::Unsupported,host,"IntToString operand cannot overwrite beyond its four-byte temporary");
        Require(result.opcode==0x53u && host.effects==0u && host.rng==12345u,
            "Oversized IntToString zero-fill lost converter trace or leaked prior effects");
    }
    {
        TestHost host;
        auto function=Function("IntegerTextNoWrite",Return(conversion(Let(Ref(0x01u,1),Int(73)))));
        const auto property=host.Property(function,1,"Counter",Value::Integer(0));host.Store("Self",property,{Value::Integer(5)});
        const auto result=Vm::Execute(host,function,"Self");Returned(result,"Let evaluates once but does not replace IntToString's initialized operand destination");
        Require(result.value.text=="0" && result.writes==1u && host.Load("Self",property).integer==73,
            "IntToString fabricated a result from unwritten Let or skipped its side effect");
    }
    {
        TestHost host;
        auto function=Function("IntegerTextReceiver",Return(Context(Ref(0x20u,1),conversion(Ref(0x01u,2)))));
        host.objects[{function.source,1}]="Other";
        const auto property=host.Property(function,2,"Count",Value::Integer(0));
        host.Store("Self",property,{Value::Integer(-123)});host.Store("Other",property,{Value::Integer(987)});
        const auto result=Vm::Execute(host,function,"Self");Returned(result,"IntToString operand uses original FFrame.Object");
        Require(result.value.text=="-123" && host.observations==std::vector<std::string>{"variable:Self:Count"},
            "IntToString read changed opcode receiver instead of its frame object");
        auto callee=Function("IntegerTextCallee",Return(conversion(Ref(0x01u,2))),"Callee");
        host.properties[{callee.source,2}]=property;host.finals[{function.source,3}]=std::make_shared<Vm::Function>(callee);
        function.bytecode=Return(Context(Ref(0x20u,1),Call(0x1cu,3)));
        const auto nested=Vm::Execute(host,function,"Self");Returned(nested,"Nested converter uses callee's actual frame object");
        Require(nested.value.text=="987" && host.preparations.back()=="Other:"+callee.path,
            "Nested IntToString kept caller's frame object instead of actual callee receiver");
    }
    {
        TestHost host;
        auto function=Function("IntegerTextSnapshot",Return(Native(1003u,{conversion(Ref(0x01u,1)),
            Native(1060u,{Ref(0x01u,1),Int(42)})})));
        const auto property=host.Property(function,1,"Count",Value::Integer(0));host.Store("Self",property,{Value::Integer(-17)});
        const auto result=Vm::Execute(host,function,"Self");Returned(result,"IntToString snapshots before later argument mutation");
        Require(result.value.text=="-17" && host.Load("Self",property).integer==42 && result.writes==1u,
            "IntToString retained a mutable input alias or reordered later argument evaluation");
    }
    {
        TestHost host;
        auto function=Function("IntegerTextReplace",Join({Let(Ref(0x01u,1),conversion(Int(-42))),Return(Ref(0x01u,1))}));
        const auto property=host.Property(function,1,"Text",Value::Text(Vm::Kind::String,{}));
        host.Store("Self",property,{Value::Text(Vm::Kind::String,"Previous longer destination")});
        const auto result=Vm::Execute(host,function,"Self");Returned(result,"IntToString replaces an existing FString destination");
        Require(result.value.text=="-42" && host.Load("Self",property).text=="-42" && result.writes==1u,
            "IntToString appended to old destination or preserved stale suffix bytes");
        host.Store("Self",property,{Value::Text(Vm::Kind::String,"changed")});
        Require(result.value.text=="-42","IntToString's returned String retained a mutable destination alias");
    }
    for(const auto& wrong:std::vector<Bytes>{{0x27u},{0x28u},{0x2au},String("123"),Ref(0x21u,1),
        Vector({1,2,3}),Rotation({1,2,3}),Native(1005u)}) {
        TestHost host;host.names[{"Fixture",1}]="123";
        const auto function=Function("IntegerTextWrongType",Join({Native(1000u),Return(conversion(wrong))}));
        const auto result=Vm::Execute(host,function,"Self");Failed(result,Vm::Status::Unsupported,host,"IntToString rejects unrelated value kinds");
        Require(result.opcode==0x53u && host.effects==0u && host.rng==12345u,
            "IntToString type refusal lost converter trace or leaked prior effects");
    }
    for(const auto value:{-2147483904.0f,2147483648.0f,std::numeric_limits<float>::max(),-std::numeric_limits<float>::max()}) {
        TestHost host;
        const auto function=Function("IntegerTextOverflow",Join({Native(1000u),Return(conversion(Real(value)))}));
        const auto result=Vm::Execute(host,function,"Self");Failed(result,Vm::Status::Unsupported,host,"IntToString rejects undefined Float-to-Int overflow");
        Require(result.opcode==0x53u && host.effects==0u,"Float-to-Int overflow leaked converter effects or trace");
    }
    for(const auto value:{std::numeric_limits<float>::infinity(),-std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}) {
        TestHost host;
        const auto function=Function("IntegerTextNonFinite",Join({Native(1000u),Return(conversion(Real(value)))}));
        const auto result=Vm::Execute(host,function,"Self");Failed(result,Vm::Status::Unsupported,host,"IntToString refuses unsupported non-finite numeric conversion");
        Require(host.effects==0u && host.rng==12345u,"Non-finite integer text operand escaped rollback");
    }
    for(const bool enough:{false,true}) {
        TestHost host;Vm::Limits limits;limits.stringBytes=enough?11u:10u;
        const auto function=Function("I",Join({Native(1000u),Return(conversion(Int(std::numeric_limits<std::int32_t>::min())))}),"I");
        const auto result=Vm::Execute(host,function,"Self",{},limits);
        if(enough) {
            Returned(result,"Eleven-byte minimum signed Int string fits exact limit");
            Require(result.value.text=="-2147483648" && host.effects==1u,"Exact IntToString string budget rejected valid output");
        } else {
            Failed(result,Vm::Status::Budget,host,"IntToString output uses the shared String budget");
            Require(result.opcode==0x53u && host.effects==0u && host.rng==12345u,
                "IntToString String budget failed before operand/result or escaped effect rollback");
        }
    }
    {
        TestHost host;
        auto function=Function("IntegerTextLateFailure",Join({Let(Ref(0x01u,1),conversion(Int(42))),Native(4095u),Return()}));
        const auto property=host.Property(function,1,"Text",Value::Text(Vm::Kind::String,{}));
        host.Store("Self",property,{Value::Text(Vm::Kind::String,"original")});
        const auto result=Vm::Execute(host,function,"Self");Failed(result,Vm::Status::Unsupported,host,"Later unsupported action restores converted FString destination");
        Require(result.writes==1u && host.Load("Self",property).text=="original",
            "Later failure leaked successfully converted String assignment");
    }
    std::cout<<"Generated original initialized IntToString decimal, frame receiver, result-write and budget contracts: "
        <<checks-initialChecks<<" checks and "<<rejections-initialRejections<<" rejection controls passed.\n";
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
void CoreGetEnumInputContracts() {
    const auto initialChecks=checks,initialRejections=rejections;
    const auto configure=[](TestHost& host,const std::string& path="Core.Object.GetEnum") {
        auto native=Function("GetEnum",{},"Core");native.path=path;native.nativeIndex=0u;native.flags=0x2401u;
        native.variables={{path+".E","E",Value::Text(Vm::Kind::Object,{}),0x80u,1u},
            {path+".i","i",Value::Integer(0),0x80u,1u},
            {path+".ReturnValue","ReturnValue",Value::Text(Vm::Kind::Name,{}),0x580u,1u}};
        host.finals[{"Fixture",7}]=std::make_shared<Vm::Function>(native);
        host.argumentSeed=[native,&host](std::uint16_t index,const std::string& receiver,std::size_t slot,
            const Vm::Function* declaration)->std::optional<Value> {
            // A raw numeric zero or another native-zero declaration has no
            // GetEnum initialized-temporary authority.
            if(index!=0u||!declaration||TestHost::Fold(declaration->path)!="core.object.getenum")return std::nullopt;
            Require(TestHost::Fold(declaration->path)==TestHost::Fold(native.path)&&declaration->source==native.source&&
                declaration->nativeIndex==0u&&declaration->flags==0x2401u&&declaration->variables.size()==3u&&slot<2u&&
                (receiver=="Self"||receiver=="Other"),"GetEnum seed lost its original declaration/receiver/parameter count");
            for(std::size_t i=0u;i<3u;++i) {
                const auto& actual=declaration->variables[i];const auto& expected=native.variables[i];
                Require(actual.key==expected.key&&actual.name==expected.name&&actual.flags==expected.flags&&
                    actual.arrayDimension==1u&&actual.zero.kind==expected.zero.kind,
                    "GetEnum seed accepted a changed native-zero parameter schema");
            }
            host.observations.push_back("getenum-seed"+std::to_string(slot));return native.variables[slot].zero;
        };
        return native;
    };
    const auto install=[](TestHost& host,const Vm::Function& native,const Vm::Function& caller,unsigned route,
        const std::string& receiver) {
        if(route==0u)host.finals[{caller.source,7}]=std::make_shared<Vm::Function>(native);
        else if(route<3u) {
            host.names[{caller.source,7}]="GetEnum";
            host.named[{receiver,route==1u?Vm::CallKind::Virtual:Vm::CallKind::Global,"GetEnum"}]=
                std::make_shared<Vm::Function>(native);
        }
    };
    const auto opcode=[](unsigned route){return route==0u?std::uint8_t{0x1cu}:route==1u?std::uint8_t{0x1bu}:std::uint8_t{0x38u};};
    for(const auto& path:{std::string("Core.Object.GetEnum"),std::string("cOrE.oBjEcT.gEtEnUm")})
        for(unsigned route=0u;route<4u;++route)for(const bool lateFailure:{false,true}) {
        TestHost host;const auto native=configure(host,path);
        const auto caller=Function("GetEnumCopiedCallerFrame",Return(Context(Ref(0x20u,9),
            Call(opcode(route),7,{Ref(0x01u,1),Ref(0x01u,2)}))));
        const auto object=host.Property(caller,1,"EnumInput",Value::Text(Vm::Kind::Object,{}));
        const auto index=host.Property(caller,2,"EnumIndex",Value::Integer(0));
        host.Store("Self",object,{Value::Text(Vm::Kind::Object,"Engine.Actor.EInputKey")});host.Store("Self",index,{Value::Integer(7)});
        host.Store("Other",object,{Value::Text(Vm::Kind::Object,"Other.Enum")});host.Store("Other",index,{Value::Integer(-3)});
        host.objects[{caller.source,9}]="Other";install(host,native,caller,route,"Other");bool entered{};
        host.executionNative=[&](const std::string& receiver,const std::vector<Vm::Evaluation>& args,
            const Vm::Function* actual,Vm::Execution& execution) {
            entered=true;
            Require(receiver=="Other"&&actual&&actual->path==path&&actual->flags==0x2401u&&args.size()==2u&&
                !args[0].reference&&!args[1].reference&&args[0].value.kind==Vm::Kind::Object&&
                args[0].value.text=="Engine.Actor.EInputKey"&&args[1].value.kind==Vm::Kind::Int&&args[1].value.integer==7&&
                host.referenceReads==2u,"GetEnum copied inputs were reread, kept aliases or used the opcode receiver frame");
            execution.WriteReference(host.Reference({"Self",Vm::Scope::Instance,object.key},object),Value::Text(Vm::Kind::Object,"Changed.Enum"));
            execution.WriteReference(host.Reference({"Self",Vm::Scope::Instance,index.key},index),Value::Integer(99));
            Require(args[0].Load().text=="Engine.Actor.EInputKey"&&args[1].Load().integer==7,
                "GetEnum ordinary temporary changed when its original property was written");
            return Vm::Evaluation{Value::Text(Vm::Kind::Name,"IK_Fixture"),{}};
        };
        const auto result=Vm::ExecuteTransaction(host,[&](Vm::Execution& execution) {
            execution.CallFunction(Function("GetEnumPrefixEffect",Join({Native(1000u),Return()})),"Self");
            const auto value=route==3u?execution.CallFunction(native,"Other",{
                {{},host.Reference({"Self",Vm::Scope::Instance,object.key},object)},
                {{},host.Reference({"Self",Vm::Scope::Instance,index.key},index)}}):execution.CallFunction(caller,"Self");
            if(lateFailure)execution.CallFunction(Function("GetEnumLaterFailure",Return(Native(4095u))),"Self");
            return value;
        });
        if(lateFailure) {
            Failed(result,Vm::Status::Unsupported,host,"GetEnum native-zero copies preserve enclosing rollback");
            Require(entered&&host.Load("Self",object).text=="Engine.Actor.EInputKey"&&host.Load("Self",index).integer==7&&
                host.effects==0u&&host.rng==12345u,"GetEnum late failure retained copied-input changes or prefix effects");
        } else {
            Returned(result,"GetEnum direct/final/named static native-zero inputs use one caller-frame copy");
            Require(entered&&result.value.kind==Vm::Kind::Name&&result.value.text=="IK_Fixture"&&result.writes==2u&&
                host.Load("Self",object).text=="Changed.Enum"&&host.Load("Self",index).integer==99&&
                host.Load("Other",object).text=="Other.Enum"&&host.Load("Other",index).integer==-3,
                "GetEnum native Name return or committed source-frame writes changed");
        }
        Require(std::count(host.observations.begin(),host.observations.end(),"getenum-seed0")==1&&
            std::count(host.observations.begin(),host.observations.end(),"getenum-seed1")==1,
            "GetEnum declaration-specific initialized seeds repeated at internal native entry");
    }
    for(unsigned route=0u;route<3u;++route) {
        TestHost host;const auto native=configure(host);
        const auto caller=Function("GetEnumLaterIndexMutatesObject",Return(Call(opcode(route),7,{Ref(0x01u,1),
            Native(1060u,{Ref(0x01u,1),Ref(0x20u,2),Int(11)})})));
        const auto property=host.Property(caller,1,"EnumInput",Value::Text(Vm::Kind::Object,{}));
        host.Store("Self",property,{Value::Text(Vm::Kind::Object,"Original.Enum")});host.objects[{caller.source,2}]="Changed.Enum";
        install(host,native,caller,route,"Self");
        host.executionNative=[&](const std::string&,const std::vector<Vm::Evaluation>& args,const Vm::Function*,Vm::Execution&) {
            Require(args.size()==2u&&!args[0].reference&&!args[1].reference&&args[0].value.text=="Original.Enum"&&
                args[1].value.integer==11&&host.Load("Self",property).text=="Changed.Enum",
                "GetEnum E was copied after the later i expression mutated its source");
            return Vm::Evaluation{Value::Text(Vm::Kind::Name,{}),{}};
        };
        const auto result=Vm::Execute(host,caller,"Self");Returned(result,"GetEnum copies E before evaluating i");
        Require(result.writes==1u&&host.Load("Self",property).text=="Changed.Enum","GetEnum suppressed later argument's real mutation");
    }
    {
        TestHost host;const auto native=configure(host);std::string object="Original.Enum";std::vector<unsigned> reads;
        auto e=std::make_shared<Vm::Reference>(),i=std::make_shared<Vm::Reference>();e->zero=Value::Text(Vm::Kind::Object,{});i->zero=Value::Integer(0);
        e->read=[&]{reads.push_back(0u);return Value::Text(Vm::Kind::Object,object);};
        i->read=[&]{reads.push_back(1u);object="Changed.Enum";return Value::Integer(13);};
        host.executionNative=[](const std::string&,const std::vector<Vm::Evaluation>& args,const Vm::Function*,Vm::Execution&) {
            Require(!args[0].reference&&!args[1].reference&&args[0].value.text=="Original.Enum"&&args[1].value.integer==13,
                "Direct GetEnum lost independently copied E/i inputs");return Vm::Evaluation{Value::Text(Vm::Kind::Name,"Value13"),{}};
        };
        const auto result=Vm::Execute(host,native,"Self",{{{},e},{{},i}});Returned(result,"Direct GetEnum copies each incoming reference once before the next");
        Require(reads==std::vector<unsigned>{0u,1u}&&object=="Changed.Enum"&&result.value.text=="Value13",
            "Direct GetEnum input read order/count changed");
    }
    for(unsigned route=0u;route<4u;++route)for(unsigned selected=0u;selected<2u;++selected)
        for(const auto disposition:{Vm::ResultDisposition::NoWrite,Vm::ResultDisposition::ZeroFill})for(const auto bytes:{0u,1u,2u,3u,4u}) {
        if(selected==0u&&bytes>0u&&bytes<4u)continue; // Packed Object has no partial-clear authority.
        TestHost host;const auto native=configure(host);bool read{},entered{};
        auto stale=std::make_shared<Vm::Reference>();stale->zero=native.variables[selected].zero;
        stale->read=[&]() ->Value {read=true;throw std::runtime_error("GetEnum unwritten input read stale alias");};
        const auto absent=Context({0x2au},Native(4095u),static_cast<std::uint8_t>(disposition==Vm::ResultDisposition::NoWrite?0u:bytes));
        const auto caller=Function("GetEnumInitializedDefaults",Return(Call(opcode(route),7,
            {selected==0u?absent:Bytes{0x2au},selected==1u?absent:Int(0)})));
        install(host,native,caller,route,"Self");
        host.executionNative=[&](const std::string&,const std::vector<Vm::Evaluation>& args,const Vm::Function*,Vm::Execution&) {
            entered=true;Require(args.size()==2u&&!args[0].reference&&!args[1].reference&&
                args[0].value.kind==Vm::Kind::Object&&args[0].value.text.empty()&&args[1].value.kind==Vm::Kind::Int&&args[1].value.integer==0,
                "GetEnum lost the declaration-authorized None/INT0 initialized input");return Vm::Evaluation{Value::Text(Vm::Kind::Name,{}),{}};
        };
        std::vector<Vm::Evaluation> inputs={{Value::Text(Vm::Kind::Object,{}),{}},{Value::Integer(0),{}}};
        inputs[selected]={Value::Text(Vm::Kind::String,"stale"),stale,disposition,static_cast<std::uint8_t>(bytes)};
        const auto result=route==3u?Vm::Execute(host,native,"Self",inputs):Vm::Execute(host,caller,"Self");
        Returned(result,"GetEnum NoWrite and bounded native zero-fill preserve None/INT0 defaults");
        Require(entered&&!read&&result.value.kind==Vm::Kind::Name&&result.value.text.empty()&&result.writes==0u,
            "GetEnum absent input retained stale references or changed native Name None result");
    }
    for(const bool direct:{false,true})for(unsigned selected=0u;selected<2u;++selected)for(const auto bytes:{1u,3u,5u}) {
        if(selected==1u&&bytes<4u)continue; // Initialized INT0 allows bounded prefix clears.
        TestHost host;const auto native=configure(host);bool entered{},read{};
        auto stale=std::make_shared<Vm::Reference>();stale->zero=native.variables[selected].zero;
        stale->read=[&]() ->Value {read=true;throw std::runtime_error("Invalid GetEnum clear read stale storage");};
        const auto invalid=Context({0x2au},Native(4095u),static_cast<std::uint8_t>(bytes));
        const auto caller=Function("GetEnumInvalidClear",Return(Call(0x1cu,7,{selected==0u?invalid:Bytes{0x2au},selected==1u?invalid:Int(0)})));
        host.executionNative=[&](const std::string&,const std::vector<Vm::Evaluation>&,const Vm::Function*,Vm::Execution&) {entered=true;return Vm::Evaluation{};};
        std::vector<Vm::Evaluation> inputs={{Value::Text(Vm::Kind::Object,{}),{}},{Value::Integer(0),{}}};
        inputs[selected]={native.variables[selected].zero,stale,Vm::ResultDisposition::ZeroFill,static_cast<std::uint8_t>(bytes)};
        const auto result=Vm::ExecuteTransaction(host,[&](Vm::Execution& execution) {
            execution.CallFunction(Function("GetEnumClearPrefix",Join({Native(1000u),Return()})),"Self");
            try{if(direct)execution.CallFunction(native,"Self",inputs);else execution.CallFunction(caller,"Self");}catch(const std::exception&){}return Value{};
        });
        Failed(result,Vm::Status::Unsupported,host,"GetEnum invalid Object prefix/overflow poisons caller transaction");
        Require(!entered&&!read&&host.effects==0u&&host.rng==12345u,"GetEnum invalid clear invoked native, read stale alias or retained effects");
    }
    for(const bool direct:{false,true}) {
        TestHost host;const auto native=configure(host);const auto realSeed=host.argumentSeed;
        host.argumentSeed=[&](std::uint16_t index,const std::string& receiver,std::size_t slot,const Vm::Function* actual)->std::optional<Value> {
            const auto value=realSeed(index,receiver,slot,actual);if(actual&&slot==1u)throw std::runtime_error("GetEnum second seed refused");return value;
        };
        const auto caller=Function("GetEnumSeedFailureBeforeArguments",Return(Call(0x1cu,7,{Native(1000u),Native(1000u)})));
        const auto result=Vm::ExecuteTransaction(host,[&](Vm::Execution& execution) {
            execution.CallFunction(Function("GetEnumSeedPrefix",Join({Native(1000u),Return()})),"Self");
            try{if(direct)execution.CallFunction(native,"Self",{{Value::Text(Vm::Kind::Object,{}),{}},{Value::Integer(0),{}}});
                else execution.CallFunction(caller,"Self");}catch(const std::exception&){}return Value{};
        });
        Failed(result,Vm::Status::Unsupported,host,"GetEnum declaration seed failure happens before arguments and poisons root");
        Require(host.effects==0u&&host.rng==12345u&&host.observations==std::vector<std::string>{"effect","getenum-seed0","getenum-seed1"}&&
            host.nativeArguments.size()==1u,"GetEnum seed failure evaluated arguments or retained prefix effects");
    }
    for(const bool direct:{false,true})for(unsigned altered=0u;altered<4u;++altered) {
        TestHost host;auto native=configure(host);
        if(altered==0u)native.flags=0x401u;
        else if(altered==1u)native.variables[0].flags=0x180u;
        else if(altered==2u)native.variables[1].arrayDimension=2u;
        else native.variables[2].zero=Value::Text(Vm::Kind::String,{});
        host.finals[{"Fixture",7}]=std::make_shared<Vm::Function>(native);
        const auto caller=Function("GetEnumMalformedDeclaration",Return(Call(0x1cu,7,{Native(1000u),Native(1000u)})));
        const auto result=Vm::ExecuteTransaction(host,[&](Vm::Execution& execution) {
            execution.CallFunction(Function("GetEnumMalformedPrefix",Join({Native(1000u),Return()})),"Self");
            try{if(direct)execution.CallFunction(native,"Self",{{Value::Text(Vm::Kind::Object,{}),{}},{Value::Integer(0),{}}});
                else execution.CallFunction(caller,"Self");}catch(const std::exception&){}return Value{};
        });
        Failed(result,Vm::Status::Unsupported,host,"GetEnum exact declaration authority rejects changed flags/OUT/dimension/return schema");
        Require(host.observations==std::vector<std::string>{"effect"}&&host.nativeArguments.size()==1u&&host.effects==0u&&host.rng==12345u,
            "Malformed GetEnum declaration captured defaults, evaluated pending inputs or retained root effects");
    }
    for(const auto& foreign:{std::string{},std::string("Fixture.Object.GetEnum"),std::string("Core.Object.GetEnumSuffix")}) {
        TestHost host;auto native=configure(host);native.path=foreign;
        const auto expression=foreign.empty()?Native(0u,{Ref(0x01u,1),Int(0)}):Call(0x1cu,7,{Ref(0x01u,1),Int(0)});
        const auto caller=Function("GenericNativeZeroKeepsAlias",Return(expression));
        const auto property=host.Property(caller,1,"ObjectInput",Value::Text(Vm::Kind::Object,{}));
        host.Store("Self",property,{Value::Text(Vm::Kind::Object,"Original.Object")});
        if(!foreign.empty())host.finals[{caller.source,7}]=std::make_shared<Vm::Function>(native);
        host.executionNative=[&](const std::string&,const std::vector<Vm::Evaluation>& args,const Vm::Function* actual,Vm::Execution&) {
            Require(static_cast<bool>(actual)==!foreign.empty()&&args.size()==2u&&args[0].reference&&args[0].Load().text=="Original.Object",
                "Raw/foreign native zero inherited GetEnum ordinary-input copy semantics");
            args[0].reference->write(Value::Text(Vm::Kind::Object,"Changed.Object"));return Vm::Evaluation{Value::Integer(17),{}};
        };
        const auto result=Vm::Execute(host,caller,"Self");Returned(result,"Raw numeric zero/foreign declaration do not gain GetEnum semantics");
        Require(result.value.integer==17&&result.writes==1u&&host.Load("Self",property).text=="Changed.Object"&&
            std::none_of(host.observations.begin(),host.observations.end(),[](const std::string& value){return value.starts_with("getenum-seed");}),
            "Generic native-zero behavior lost its real alias or acquired GetEnum defaults");
    }
    std::cout<<"Original Core.GetEnum declaration-specific native-zero input copies/defaults: "<<checks-initialChecks
        <<" checks and "<<rejections-initialRejections<<" refusal controls.\n";
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
    auto f=Function("AssignmentOrder",Join({Let(Element(Native(1001),Ref(0x01,1)),Native(1002)),Return()}));
    const auto slot=host.Property(f,1,"Slot",Value::Integer(0));host.Store("Self",slot,{Value::Integer(0)});
    auto r=Vm::Execute(host,f,"Self");Returned(r,"Let evaluation order");
    Require(host.observations==std::vector<std::string>{"lhs","variable:Self:Slot","rhs"} &&
        r.value.kind==Vm::Kind::Nothing && host.Load("Self",slot).integer==23,
        "Let did not evaluate LHS before RHS, store through its address, or leave outer Result unwritten");
    f=Function("NothingAssignment",Return(Let({0x0b},Native(1002))));
    const auto observations=host.observations.size();
    r=Vm::Execute(host,f,"Self");Returned(r,"Nothing assignment scratch result");
    Require(r.value.kind==Vm::Kind::Nothing && !r.writes && host.observations.size()==observations+1u &&
        host.observations.back()=="rhs","Nothing LHS did not execute RHS once without writing the enclosing result");
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
void FixedStructArrayMemberContracts() {
    const auto initialChecks=checks,initialRejections=rejections;
    const auto envelope=[](const std::vector<Value>& values) {
        Value result;result.kind=Vm::Kind::Struct;
        for(std::size_t i=0u;i<values.size();++i)result.fields.emplace(std::to_string(i),values[i]);
        return result;
    };
    const auto structure=[&](const std::vector<Value>& values,std::int32_t sibling=0) {
        Value result;result.kind=Vm::Kind::Struct;result.fields["slots"]=envelope(values);
        result.fields["sibling"]=Value::Integer(sibling);return result;
    };
    const auto nested=[](std::int32_t count) {
        Value result;result.kind=Vm::Kind::Struct;result.fields["count"]=Value::Integer(count);
        result.fields["label"]=Value::Text(Vm::Kind::Name,count?"Present":"None");return result;
    };
    for(const auto& [kind,zero,values,replacement]:std::vector<std::tuple<std::string,Value,std::vector<Value>,Value>>{
        {"Int",Value::Integer(0),{Value::Integer(1),Value::Integer(2),Value::Integer(3)},Value::Integer(44)},
        {"Name",Value::Text(Vm::Kind::Name,{}),{Value::Text(Vm::Kind::Name,"A"),Value::Text(Vm::Kind::Name,"B"),Value::Text(Vm::Kind::Name,"C")},Value::Text(Vm::Kind::Name,"D")},
        {"Object",Value::Text(Vm::Kind::Object,{}),{Value::Text(Vm::Kind::Object,"Map.A"),Value::Text(Vm::Kind::Object,"Map.B"),Value::Text(Vm::Kind::Object,"Map.C")},Value::Text(Vm::Kind::Object,"Map.D")},
        {"Struct",nested(0),{nested(1),nested(2),nested(3)},nested(44)}}) {
        TestHost host;const auto zeroParent=structure({zero,zero,zero});
        const auto function=Function("FixedMember"+kind,Join({
            Let(Element(Int(-100),Member(2,Ref(0x01,1))),Native(1008u)),
            Let(Element(Int(100),Member(2,Ref(0x01,1))),Native(1008u)),
            Let(Member(2,Ref(0x01,1)),Native(1008u)),Return(Member(2,Ref(0x01,1)))}));
        const auto parent=host.Property(function,1,"Root",zeroParent);
        host.Property(function,2,"SLOTS",zero,0u,3u);host.Store("Self",parent,{structure(values,17)});
        host.executionNative=[&](const std::string&,const std::vector<Vm::Evaluation>&,const Vm::Function*,Vm::Execution&) {
            return Vm::Evaluation{replacement,{}};
        };
        const auto result=Vm::Execute(host,function,"Self");Returned(result,"General fixed StructMember "+kind);
        const auto stored=host.Load("Self",parent);
        Require(Vm::Equal(result.value,replacement) && Vm::Equal(stored.fields.at("slots").fields.at("0"),replacement) &&
            Vm::Equal(stored.fields.at("slots").fields.at("1"),values[1]) &&
            Vm::Equal(stored.fields.at("slots").fields.at("2"),replacement) && stored.fields.at("sibling").integer==17 && result.writes==3u,
            "Fixed StructMember lost reflected element zero, first/last clamp, field casing or sibling values");
    }
    {
        TestHost host;const auto zero=structure({Value::Integer(0),Value::Integer(0),Value::Integer(0)});
        auto function=Function("OuterArrayThenMemberArray",Return(Element(Native(1001u),Member(2,Element(Native(1002u),Ref(0x01,1))))));
        const auto parent=host.Property(function,1,"Roots",zero,0u,2u);host.Property(function,2,"Slots",Value::Integer(0),0u,3u);
        const auto first=structure({Value::Integer(1),Value::Integer(2),Value::Integer(3)},10);
        const auto second=structure({Value::Integer(4),Value::Integer(5),Value::Integer(6)},20);
        host.Store("Self",parent,{first,second});auto result=Vm::Execute(host,function,"Self");Returned(result,"Nested fixed-array address order");
        Require(result.value.integer==4 && host.observations==std::vector<std::string>{"lhs","rhs","variable:Self:Roots"},
            "Member-array index did not precede parent-array index/address evaluation");
        function.bytecode=Join({Let(Element(Int(1),Member(2,Element(Int(0),Ref(0x01,1)))),Int(99)),Return(Ref(0x01,1))});
        result=Vm::Execute(host,function,"Self");Returned(result,"Outer fixed Struct array/member fixed array write");
        Require(host.Load("Self",parent,0u).fields.at("slots").fields.at("1").integer==99 &&
            host.Load("Self",parent,0u).fields.at("sibling").integer==10 && Vm::Equal(host.Load("Self",parent,1u),second),
            "Nested member-array write replaced another outer element or unrelated field");
    }
    for(const unsigned failure:{0u,1u,2u}) {
        TestHost host;const auto zero=structure({Value::Integer(0),Value::Integer(0)});
        const auto original=structure({Value::Integer(1),Value::Integer(2)},10);
        const auto assignment=Let(Element(Int(1),Member(2,Ref(0x01,1))),Native(1008u,{Ref(0x01,1)}));
        const auto function=Function("FreshMemberParent",failure==1u?Join({assignment,Native(4095u),Return()}):Join({assignment,Return(Ref(0x01,1))}));
        const auto parent=host.Property(function,1,"Root",zero);host.Property(function,2,"Slots",Value::Integer(0),0u,2u);
        host.Store("Self",parent,{original});
        host.executionNative=[&](const std::string&,const std::vector<Vm::Evaluation>& arguments,const Vm::Function*,Vm::Execution&) {
            auto changed=arguments.at(0).Load();changed.fields.at("sibling")=Value::Integer(20);
            changed.fields.at("slots").fields.at("0")=Value::Integer(33);
            arguments.at(0).reference->write(changed);return Vm::Evaluation{Value::Integer(77),{}};
        };
        auto limits=Vm::Limits{};if(failure==2u)limits.writes=1u;
        const auto result=Vm::Execute(host,function,"Self",{},limits);
        if(failure) {
            Failed(result,failure==1u?Vm::Status::Unsupported:Vm::Status::Budget,host,"Member-array late refusal/write-budget rollback");
            Require(Vm::Equal(host.Load("Self",parent),original),"Failed member-array write retained RHS parent/sibling mutations");
        } else {
            Returned(result,"Member-array fresh parent after RHS callback");const auto stored=host.Load("Self",parent);
            Require(stored.fields.at("sibling").integer==20 && stored.fields.at("slots").fields.at("0").integer==33 &&
                stored.fields.at("slots").fields.at("1").integer==77 && result.writes==2u,
                "Captured member-array address restored a stale parent over RHS sibling changes");
        }
    }
    for(const std::size_t cutoff:{0u,1u,2u,3u}) {
        TestHost host;const auto zero=structure({Value::Integer(0),Value::Integer(0)});
        const auto original=structure({Value::Integer(1),Value::Integer(2)},17);
        const auto function=Function("EveryMemberWriteCutoff",Join({Native(1000u),
            Let(Element(Int(0),Member(2,Ref(0x01,1))),Int(40)),Let(Element(Int(1),Member(2,Ref(0x01,1))),Int(50)),
            Let(Member(2,Ref(0x01,1)),Int(60)),Native(4095u),Return()}));
        const auto parent=host.Property(function,1,"Root",zero);host.Property(function,2,"Slots",Value::Integer(0),0u,2u);
        host.Store("Self",parent,{original});auto limits=Vm::Limits{};limits.writes=cutoff;
        const auto result=Vm::Execute(host,function,"Self",{},limits);
        Failed(result,cutoff<3u?Vm::Status::Budget:Vm::Status::Unsupported,host,"Every member-array write cutoff and post-last-slot refusal");
        Require(Vm::Equal(host.Load("Self",parent),original) && !host.effects && host.rng==12345u,
            "Member-array cutoff retained selected slots, sibling values or earlier native effects");
    }
    for(const bool wholeStruct:{false,true}) {
        TestHost host;const auto zero=structure({Value::Integer(0),Value::Integer(0)});
        const auto original=structure({Value::Integer(1),Value::Integer(2)},10);
        auto callee=Function("MemberOut",{},"MemberCallee");auto caller=Function("MemberOutCaller",{},"MemberCaller");
        const auto parent=host.Property(caller,1,"Root",zero);host.Store("Self",parent,{original});
        host.Property(caller,2,"Slots",Value::Integer(0),0u,2u);host.Property(callee,1,"Root",zero);
        host.Property(callee,2,"Slots",Value::Integer(0),0u,2u);
        callee.variables={host.Property(callee,4,"Output",wholeStruct?zero:Value::Integer(0),0x180u,1u,true)};
        const auto outTarget=wholeStruct?Element(Int(1),Member(2,Ref(0x00,4))):Ref(0x00,4);
        callee.bytecode=Join({Native(1008u,{wholeStruct?Ref(0x00,4):Ref(0x01,1)}),Let(outTarget,Int(88)),Return()});
        host.finals[{caller.source,7}]=std::make_shared<Vm::Function>(callee);
        caller.bytecode=Join({Call(0x1c,7,{wholeStruct?Ref(0x01,1):Element(Int(1),Member(2,Ref(0x01,1)))}),Return(Ref(0x01,1))});
        host.executionNative=[&](const std::string&,const std::vector<Vm::Evaluation>& arguments,const Vm::Function*,Vm::Execution&) {
            auto changed=arguments.at(0).Load();changed.fields.at("sibling")=Value::Integer(30);
            changed.fields.at("slots").fields.at("0")=Value::Integer(55);arguments.at(0).reference->write(changed);return Vm::Evaluation{};
        };
        auto result=Vm::Execute(host,caller,"Self");Returned(result,"Fixed member element/whole Struct OUT writeback");
        const auto stored=host.Load("Self",parent);
        Require(stored.fields.at("sibling").integer==30 && stored.fields.at("slots").fields.at("0").integer==55 &&
            stored.fields.at("slots").fields.at("1").integer==88,"OUT member/whole Struct writeback lost fresh parent changes");
        host.Store("Self",parent,{original});
        caller.bytecode=Join({Call(0x1c,7,{wholeStruct?Ref(0x01,1):Element(Int(1),Member(2,Ref(0x01,1)))}),Native(4095u),Return()});
        const auto counts=std::array<std::size_t,3u>{host.beginCount,host.commitCount,host.rollbackCount};
        result=Vm::Execute(host,caller,"Self");
        Require(result.status==Vm::Status::Unsupported&&!result.error.empty()&&
            host.beginCount==counts[0u]+1u&&host.commitCount==counts[1u]&&host.rollbackCount==counts[2u]+1u&&!host.active,
            "OUT member-array enclosing rollback did not undo exactly the new root");++rejections;
        Require(Vm::Equal(host.Load("Self",parent),original),"OUT member-array callback/copyback escaped enclosing rollback");
    }
    {
        TestHost host;const auto parentValue=structure({nested(1),nested(2)});
        auto function=Function("TemporaryMemberArray",Return(Member(3,Element(Int(1),Member(2,Native(1008u))))));
        host.Property(function,2,"Slots",nested(0),0u,2u);host.Property(function,3,"Count",Value::Integer(0));
        host.executionNative=[&](const std::string&,const std::vector<Vm::Evaluation>&,const Vm::Function*,Vm::Execution&) {
            return Vm::Evaluation{parentValue,{}};
        };
        auto result=Vm::Execute(host,function,"Self");Returned(result,"Owned temporary member-array/nested scalar load");
        Require(result.value.integer==2 && !result.writes,"Temporary array member address dangled or returned its envelope");
        function.bytecode=Let(Member(3,Element(Int(1),Member(2,Native(1008u)))),Int(99));
        function.bytecode=Join({function.bytecode,Return()});
        const auto counts=std::array<std::size_t,3u>{host.beginCount,host.commitCount,host.rollbackCount};
        result=Vm::Execute(host,function,"Self");
        Require(result.status==Vm::Status::Invalid&&!result.error.empty()&&
            host.beginCount==counts[0u]+1u&&host.commitCount==counts[1u]&&host.rollbackCount==counts[2u]+1u&&!host.active,
            "Temporary member-array references acquired a writable alias or failed outside their new root");++rejections;
    }
    for(const bool noWrite:{false,true}) {
        TestHost host;const auto parentValue=structure({Value::Integer(1),Value::Integer(2)});
        const auto parentExpression=noWrite?Native(1008u):Context({0x2au},Native(4095u),12u);
        const auto function=Function("AbsentMemberArray",Return(Element(Int(1),Member(2,parentExpression))));
        host.Property(function,2,"Slots",Value::Integer(0),0u,2u);
        host.executionNative=[&](const std::string&,const std::vector<Vm::Evaluation>&,const Vm::Function*,Vm::Execution&) {
            return Vm::Evaluation{parentValue,{},Vm::ResultDisposition::NoWrite};
        };
        const auto result=Vm::Execute(host,function,"Self");Returned(result,"Absent StructMember array retains no-result disposition");
        Require(result.value.kind==Vm::Kind::Nothing && !result.writes,"Absent member-array parent fabricated an envelope/element/zero-fill result");
    }
    {
        TestHost host;const auto zero=structure({Value::Integer(0),Value::Integer(0)});
        const auto original=structure({Value::Integer(1),Value::Integer(2)});
        const auto function=Function("MemberArrayExpiredGuard",Return(Native(1007u,{Element(Int(1),Member(2,Ref(0x01,1)))})));
        const auto parent=host.Property(function,1,"Root",zero);host.Property(function,2,"Slots",Value::Integer(0),0u,2u);
        host.Store("Self",parent,{original});const auto result=Vm::Execute(host,function,"Self");Returned(result,"Member-array guarded OUT address lifetime");
        Require(result.value.integer==2 && host.retainedNativeReference && host.retainedNativeElement,"Member-array native reference fixture lost its selected element");
        for(const unsigned operation:{0u,1u,2u,3u}) {
            bool refused{};try {
                if(operation==0u)host.retainedNativeReference->read();
                if(operation==1u)host.retainedNativeReference->write(Value::Integer(99));
                if(operation==2u)host.retainedNativeReference->element(0u);
                if(operation==3u)host.retainedNativeElement->read();
            }catch(const std::runtime_error&){refused=true;}
            Require(refused,"Expired member-array native address escaped its execution guard");
        }
        Require(Vm::Equal(host.Load("Self",parent),original),"Expired member-array OUT address mutated committed storage");
    }
    {
        TestHost host;const auto zero=structure({Value::Rotator({}),Value::Rotator({})});
        const auto original=structure({Value::Rotator({1,2,3}),Value::Rotator({10,20,30})});
        const auto function=Function("MemberArrayNative316Copy",Return(Native(316u,{Element(Int(1),Member(2,Ref(0x01,1))),Native(1008u)})));
        const auto parent=host.Property(function,1,"Root",zero);host.Property(function,2,"Slots",Value::Rotator({}),0u,2u,false,Vm::ResultLayout::CoreRotator12);
        host.Store("Self",parent,{original});
        host.executionNative=[&](const std::string&,const std::vector<Vm::Evaluation>&,const Vm::Function*,Vm::Execution& execution) {
            Require(host.referenceReads==1u,"Native316 member-array Address/Copy path loaded parent before its final selected element");
            const auto raw=host.Reference({"Self",Vm::Scope::Instance,parent.key},parent);
            execution.WriteReference(raw,structure({Value::Rotator({9,9,9}),Value::Rotator({99,99,99})}));
            return Vm::Evaluation{Value::Rotator({4,5,6}),{}};
        };
        const auto result=Vm::Execute(host,function,"Self");Returned(result,"Member-array native316 detached typed element snapshot");
        Require(result.value.rotation==std::array<std::int32_t,3u>{14,25,36},"Native316 member-array copy changed after a later operand mutated the parent");
    }
    {
        TestHost host;const auto function=Function("AbsentMemberArray316Address",Return(Native(316u,{
            Element(Int(1),Member(2,Context({0x2au},Native(4095u),12u))),Rotation({1,2,3})})));
        host.Property(function,2,"Slots",Value::Rotator({}),0u,std::numeric_limits<std::size_t>::max(),false,Vm::ResultLayout::CoreRotator12);
        const auto result=Vm::Execute(host,function,"Self");
        Failed(result,Vm::Status::Unsupported,host,"Native316 missing member-array address retains unwritten operand refusal");
        Require(result.error.find("dimension")==std::string::npos && host.nativeIndices.empty(),
            "Inactive native316 member-array address consumed metadata shape/dimension or executed suppressed operands");
    }
    for(unsigned malformed=0u;malformed<9u;++malformed) {
        TestHost host;const auto zero=structure({Value::Integer(0),Value::Integer(0)});
        auto original=structure({Value::Integer(1),Value::Integer(2)});auto dimension=std::size_t{2u};
        if(malformed==0u)original.fields.at("slots")=Value::Integer(1);
        if(malformed==1u)original.fields.at("slots").fields.erase("1");
        if(malformed==2u){original.fields.at("slots").fields["01"]=Value::Integer(2);original.fields.at("slots").fields.erase("1");}
        if(malformed==3u)original.fields.at("slots").fields["1"]=Value::Float(2.0f);
        if(malformed==4u)original.fields.at("slots").fields["extra"]=Value::Integer(3);
        if(malformed==5u)original.fields.at("slots").text="not an authored envelope";
        if(malformed==6u)original.fields.at("slots").fields.at("0").fields["extra"]=Value::Integer(1);
        if(malformed==7u)dimension=0u;
        if(malformed==8u)dimension=std::numeric_limits<std::size_t>::max();
        const auto function=Function("MalformedMemberArray",Join({Native(1000u),Return(Element(Int(1),Member(2,Ref(0x01,1))))}));
        const auto parent=host.Property(function,1,"Root",zero);host.Property(function,2,"Slots",Value::Integer(0),0u,dimension);
        host.Store("Self",parent,{original});const auto result=Vm::Execute(host,function,"Self");
        Failed(result,malformed==8u?Vm::Status::Budget:Vm::Status::Invalid,host,"Malformed reflected member array fails closed");
        Require(Vm::Equal(host.Load("Self",parent),original) && !host.effects && host.rng==12345u,
            "Malformed member-array refusal retained an earlier effect or changed stored data");
    }
    for(const unsigned budget:{0u,1u}) {
        TestHost host;const auto zero=structure({Value::Integer(0),Value::Integer(0)});
        const auto function=Function("MemberArrayBudgets",Return(Element(Int(1),Member(2,Ref(0x01,1)))));
        const auto parent=host.Property(function,1,"Root",zero);host.Property(function,2,"Slots",Value::Integer(0),0u,2u);host.Store("Self",parent,{zero});
        auto limits=Vm::Limits{};if(budget)limits.retainedBytes=sizeof(Vm::Reference);else limits.localElements=1u;
        const auto result=Vm::Execute(host,function,"Self",{},limits);Failed(result,Vm::Status::Budget,host,"Member-array metadata/envelope/reference storage bounds");
        Require(!result.writes,"Member-array storage-budget refusal wrote a parent");
    }
    std::cout<<"General reflected fixed-array StructMember addresses: "<<checks-initialChecks<<" checks and "
        <<rejections-initialRejections<<" refusal controls.\n";
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
void ReceiverAndContextMetadataContracts() {
    const auto initialChecks=checks,initialRejections=rejections;
    TestHost host;
    auto f=Function("RetailReceivers",Return(Context(Ref(0x20,1),{0x17})));
    host.objects[{f.source,1}]="Other";
    auto r=Vm::Execute(host,f,"Self");Returned(r,"Self uses incoming opcode receiver");
    Require(r.value.kind==Vm::Kind::Object && r.value.text=="Other",
        "Changed-context Self incorrectly returned FFrame.Object");
    const auto value=host.Property(f,2,"Value",Value::Integer(0));
    const auto source=host.Property(f,3,"Source",Value::Integer(0));
    host.Store("Self",value,{Value::Integer(12)});host.Store("Other",value,{Value::Integer(77)});
    host.Store("Self",source,{Value::Integer(34)});host.Store("Other",source,{Value::Integer(98)});
    f.bytecode=Return(Context(Ref(0x20,1),Context({0x17},Ref(0x01,2))));
    r=Vm::Execute(host,f,"Self");Returned(r,"Nested Context selector uses incoming receiver");
    Require(r.value.integer==77,"Nested Context rebound its selector to the frame object");
    f.bytecode=Join({Context(Ref(0x20,1),Let(Ref(0x01,2),Ref(0x01,3))),Return(Ref(0x01,2))});
    r=Vm::Execute(host,f,"Self");Returned(r,"Let operands use frame object");
    Require(r.value.integer==34 && host.Load("Other",value).integer==77 && r.writes==1u,
        "Changed-context Let redirected its target or source away from FFrame.Object");
    const auto bit=host.Property(f,4,"Bit",Value::Bool(false));
    const auto bitSource=host.Property(f,5,"BitSource",Value::Bool(false));
    host.Store("Self",bit,{Value::Bool(true)});host.Store("Other",bit,{Value::Bool(false)});
    host.Store("Self",bitSource,{Value::Bool(false)});host.Store("Other",bitSource,{Value::Bool(true)});
    f.bytecode=Join({Context(Ref(0x20,1),Let(Ref(0x01,4),Ref(0x01,5),0x14u)),Return(Ref(0x01,4))});
    r=Vm::Execute(host,f,"Self");Returned(r,"LetBool operands use frame object");
    Require(!r.value.boolean && !host.Load("Other",bit).boolean && r.writes==1u,
        "Changed-context LetBool redirected its reflected operands");
    const auto array=host.Property(f,6,"Slots",Value::Integer(0),0u,3u);
    const auto index=host.Property(f,7,"Index",Value::Integer(0));
    host.Store("Self",index,{Value::Integer(1)});host.Store("Other",index,{Value::Integer(2)});
    host.Store("Self",array,{Value::Integer(10),Value::Integer(11),Value::Integer(12)});
    host.Store("Other",array,{Value::Integer(70),Value::Integer(71),Value::Integer(72)});
    f.bytecode=Return(Context(Ref(0x20,1),Element(Ref(0x01,7),Ref(0x01,6))));
    r=Vm::Execute(host,f,"Self");Returned(r,"Array index frame versus array receiver");
    Require(r.value.integer==71,"ArrayElement did not separate frame index and incoming array receiver");
    host.objectDisplayPaths["Self"]="Map.Player0";host.objectDisplayPaths["Other"]="Map.Other0";
    f.bytecode=Return(Context(Ref(0x20,1),Join({{0x56},{0x17}})));
    r=Vm::Execute(host,f,"Self");Returned(r,"ObjectToString operand uses frame object");
    Require(r.value.text=="Map.Player0" && host.objectStringLookups==std::vector<std::string>{"Self"},
        "ObjectToString evaluated its operand against a changed opcode receiver");
    auto callee=Function("ActualReceiverFrame",Return({0x17}),"Callee");
    host.finals[{f.source,8}]=std::make_shared<Vm::Function>(callee);
    f.bytecode=Return(Context(Ref(0x20,1),Call(0x1c,8)));
    r=Vm::Execute(host,f,"Self");Returned(r,"Callee creates frame from actual receiver");
    Require(r.value.text=="Other" && host.preparations.back()=="Other:"+callee.path,
        "Nested scripted call kept caller frame Object instead of callee receiver");
    {
        TestHost failed;
        const auto fn=Function("ChangedReceiverRollback",Join({
            Context(Ref(0x20,1),Let(Ref(0x01,2),Int(66))),Native(4095u),Return()}));
        failed.objects[{fn.source,1}]="Other";
        const auto target=failed.Property(fn,2,"Value",Value::Integer(0));
        failed.Store("Self",target,{Value::Integer(12)});failed.Store("Other",target,{Value::Integer(77)});
        const auto failure=Vm::Execute(failed,fn,"Self");
        Failed(failure,Vm::Status::Unsupported,failed,"Changed-receiver assignment rollback");
        Require(failure.writes==1u && failed.Load("Self",target).integer==12 && failed.Load("Other",target).integer==77,
            "Later failure leaked a corrected frame-object assignment");
    }
    {
        TestHost structural;
        const auto fn=Function("ClassContextMetadata",Return(Context({0x17},Join({{0x2d},Ref(0x02,100)}),4u,0x12u)));
        structural.objects[{fn.source,100}]="Engine.Pawn.bIsHuman";
        const auto layout=Vm::AnalyzeProgram(structural,fn);
        Require(layout.expressions.size()==5u && layout.expressions[0].opcode==0x04u &&
            layout.expressions[0].children==std::vector<std::size_t>{1u} &&
            layout.expressions[1].offset==1u && layout.expressions[1].opcode==0x12u &&
            layout.expressions[1].target==6u && layout.expressions[1].zeroFillSize==4u &&
            layout.expressions[1].children==std::vector<std::size_t>{2u,3u} &&
            layout.expressions[2].opcode==0x17u && layout.expressions[3].opcode==0x2du &&
            layout.expressions[3].children==std::vector<std::size_t>{4u} &&
            layout.expressions[4].opcode==0x02u && layout.expressions[4].reference==100,
            "Structural AST lost context skip/zero-fill, preorder children or qualified reference");
        Require(!structural.beginCount && structural.observations.empty() && !structural.castClassResolutions,
            "Structural context metadata extraction executed or resolved typed fields");
        auto limits=Vm::Limits{};limits.retainedBytes=1u;
        bool refused{};try {Vm::AnalyzeProgram(structural,fn,limits);}catch(const std::exception&) {refused=true;}
        Require(refused,"Detached expression metadata escaped the retained-byte budget");++rejections;
        // Find the exact parser-only budget via its last/sole object lookup.
        // All parser Retain calls precede this DefaultVariable leaf; exported
        // AST allocation follows Parse. A 1-byte test alone cannot prove this
        // NEW allocation is charged, since the old parser already rejects it.
        std::size_t low=0u,high=Vm::Limits{}.retainedBytes;
        while(low<high) {
            const auto middle=low+(high-low)/2u;limits.retainedBytes=middle;
            structural.objectResolutions=0u;
            try {Vm::AnalyzeProgram(structural,fn,limits);}catch(const std::exception&) {}
            if(structural.objectResolutions) high=middle; else low=middle+1u;
        }
        limits.retainedBytes=low;structural.objectResolutions=0u;refused=false;
        try {Vm::AnalyzeProgram(structural,fn,limits);}catch(const std::exception&) {refused=true;}
        Require(refused && structural.objectResolutions==1u,
            "Detached expression allocation was not charged after the complete parser-only budget");++rejections;
    }
    for(const auto opcode:{std::uint8_t{0x12u},std::uint8_t{0x19u}}) {
        for(const auto skip:{std::uint16_t{0u},std::uint16_t{1u},std::uint16_t{65535u}}) {
            TestHost rejected;
            auto context=Context({0x2a},Int(7),4u,opcode);PatchU16(context,2u,skip);
            const auto fn=Function("MalformedContextSpan",Join({Native(1000u),Return(context)}));
            const auto failure=Vm::Execute(rejected,fn,"Self");
            Failed(failure,Vm::Status::Invalid,rejected,"Malformed logical Context child span");
            Require(failure.error=="VM context skip does not match its child expression span" &&
                failure.offset==Native(1000u).size()+1u && failure.opcode==opcode &&
                !rejected.effects && rejected.observations.empty(),"Malformed context span reached prefix effects");
            bool refused{};try {Vm::AnalyzeProgram(rejected,fn);}catch(const std::exception&) {refused=true;}
            Require(refused,"Structural inspection accepted malformed context skip metadata");++rejections;
        }
    }
    std::cout<<"Retail receiver/context structural foundation: "<<checks-initialChecks
        <<" checks, "<<rejections-initialRejections<<" rejection controls.\n";
}
void ClassContextContracts() {
    const auto initialChecks=checks,initialRejections=rejections;
    TestHost host;
    const auto refused=[&](const Vm::Function& function,const Vm::Status status,const std::string& label,
        const Vm::Limits& limits=Vm::Limits{}) {
        const auto counts=std::array<std::size_t,3>{host.beginCount,host.commitCount,host.rollbackCount};
        const auto storage=host.storage;const auto effects=host.effects;const auto random=host.rng;
        const auto result=Vm::Execute(host,function,"Self",{},limits);
        Require(result.status==status && !result.passed() && !result.committed && !result.error.empty(),label+": invalid refusal diagnostic");
        Require(host.beginCount==counts[0]+1u && host.commitCount==counts[1] && host.rollbackCount==counts[2]+1u &&
            !host.active,label+": enclosing transaction did not roll back exactly once");
        Require(host.storage.size()==storage.size() && host.effects==effects && host.rng==random,label+": host effects/storage count escaped rollback");
        for(const auto& [key,values]:storage) {
            const auto found=host.storage.find(key);Require(found!=host.storage.end() && found->second.size()==values.size(),label+": storage alias shape escaped rollback");
            for(std::size_t i=0u;i<values.size();++i) Require(Vm::Equal(values[i],found->second[i]),label+": reflected value escaped rollback");
        }
        ++rejections;return result;
    };
    {
        TestHost assignment;
        auto function=Function("RetailAssignmentReturn",Join({Let(Ref(0x00,2),Int(91)),
            Return(Let(Ref(0x01,1),Native(1002u)))}));
        const auto target=assignment.Property(function,1,"Target",Value::Integer(0));
        const auto returned=assignment.Property(function,2,"ReturnValue",Value::Integer(0),0x480u,1u,true);
        function.variables={returned};assignment.Store("Self",target,{Value::Integer(7)});
        auto result=Vm::Execute(assignment,function,"Self");Returned(result,"Let leaves caller result unwritten");
        Require(result.value.integer==91 && assignment.Load("Self",target).integer==23 && result.writes==2u,
            "Let returned its assigned lvalue or lost already initialized result-local storage");
        function.bytecode=Join({Let(Ref(0x00,2),Int(91)),Return(Let({0x0b},Native(1002u)))});
        result=Vm::Execute(assignment,function,"Self");Returned(result,"Null Let scratch leaves initialized caller result");
        Require(result.value.integer==91 && result.writes==1u && assignment.Load("Self",target).integer==23,
            "Null Let scratch wrote outer result or a real reflected target");
    }
    auto f=Function("RealClassContext",Return(Context(Ref(0x20,1),Ref(0x02,2),4u,0x12u)));
    host.objects[{f.source,1}]="Engine.GameInfo";
    host.defaultObjects["Engine.GameInfo"]="@Default:Engine.GameInfo";
    host.defaultObjects["DeusEx.DeusExGameInfo"]="@Default:DeusEx.DeusExGameInfo";
    const auto p=host.Property(f,2,"DefaultPlayerName",Value::Text(Vm::Kind::String,{}));
    host.Store("@Default:Engine.GameInfo",p,{Value::Text(Vm::Kind::String,"Player")},Vm::Scope::Default);
    host.Store("@Default:DeusEx.DeusExGameInfo",p,{Value::Text(Vm::Kind::String,"JC")},Vm::Scope::Default);
    auto r=Vm::Execute(host,f,"Self");Returned(r,"Selected actual class default");
    Require(r.value.text=="Player" && host.defaultSelections==std::vector<std::string>{"Engine.GameInfo"},
        "ClassContext substituted derived defaults or evaluated selector twice");
    f.bytecode=Return(Context(Ref(0x20,1),{0x17},4u,0x12u));
    r=Vm::Execute(host,f,"Self");Returned(r,"Default block Self identity");
    Require(r.value.text=="@Default:Engine.GameInfo" && r.value.text!="Engine.GameInfo",
        "ClassContext Self returned its UClass instead of distinct default-object identity");
    const auto selector=host.Property(f,3,"ClassSelector",Value::Text(Vm::Kind::Object,{}));
    host.Store("Self",selector,{Value::Text(Vm::Kind::Object,"Engine.GameInfo")});
    host.Store("Other",selector,{Value::Text(Vm::Kind::Object,"DeusEx.DeusExGameInfo")});
    host.objects[{f.source,4}]="Other";
    f.bytecode=Return(Context(Ref(0x20,4),Context(Ref(0x01,3),Ref(0x02,2),0u,0x12u)));
    r=Vm::Execute(host,f,"Self");Returned(r,"Class selector uses frame Object");
    Require(r.value.text=="Player","Nested ClassContext selector used incoming receiver rather than frame Object");
    f.bytecode=Return(Context(Native(1003u,{Ref(0x20,1),Native(1000u)}),Ref(0x02,2),0u,0x12u));
    const auto beforeEffects=host.effects;
    r=Vm::Execute(host,f,"Self");Returned(r,"Class selector effects exactly once");
    Require(r.value.text=="Player" && host.effects==beforeEffects+1u,"ClassContext selector effects repeated");
    const auto value=host.Property(f,5,"Target",Value::Integer(0));host.Store("Self",value,{Value::Integer(97)});
    for(const auto op:{std::uint8_t{0x12u},std::uint8_t{0x19u}}) {
        f.bytecode=Join({Let(Ref(0x01,5),Context({0x2a},Native(1000u),0u,op)),Return(Ref(0x01,5))});
        const auto effects=host.effects;
        r=Vm::Execute(host,f,"Self");Returned(r,"Null zero-size context preserves destination");
        Require(r.value.integer==97 && !r.writes && host.effects==effects,"Zero-size null Context cleared target or evaluated child");
        f.bytecode=Join({Let(Ref(0x01,5),Context({0x2a},Int(10),4u,op)),Return(Ref(0x01,5))});
        r=Vm::Execute(host,f,"Self");Returned(r,"Null context full result zeroing");
        Require(r.value.integer==0 && r.writes==1u,"Null context did not zero complete Int destination");
        host.Store("Self",value,{Value::Integer(97)});
        f.bytecode=Join({Let(Ref(0x01,5),Element(Int(0),Context({0x2a},Ref(0x02,2),32u,op))),Return(Ref(0x01,5))});
        r=Vm::Execute(host,f,"Self");Returned(r,"Null array base does not write outer result");
        Require(r.value.integer==97 && !r.writes,"ArrayElement propagated null base's whole-array zero fill");
    }
    const auto bit=host.Property(f,6,"Bit",Value::Bool(false));host.Store("Self",bit,{Value::Bool(true)});
    f.bytecode=Join({Let(Ref(0x01,6),Context({0x2a},Ref(0x02,6),0u,0x12u),0x14u),Return(Ref(0x01,6))});
    r=Vm::Execute(host,f,"Self");Returned(r,"LetBool prezeros no-write RHS");
    Require(!r.value.boolean && r.writes==1u,"LetBool preserved old bit for no-write RHS");
    f.bytecode=Return(Join({{0x2d},Context({0x2a},Ref(0x02,6),4u,0x12u)}));
    r=Vm::Execute(host,f,"Self");Returned(r,"BoolVariable null address");Require(!r.value.boolean,"Null BoolVariable did not yield false");
    f.bytecode=Join({Let(Context({0x2a},Ref(0x02,5),0u,0x12u),Native(1000u)),Return()});
    const auto effects=host.effects;r=Vm::Execute(host,f,"Self");Returned(r,"Null LHS evaluates RHS effects");
    Require(host.effects==effects+1u && !r.writes,"Null reflected LHS suppressed RHS or manufactured a target");
    f.bytecode=Join({Let(Context(Ref(0x20,1),Ref(0x02,2),0u,0x12u),String("Changed")),Native(4095u),Return()});
    r=refused(f,Vm::Status::Unsupported,"Class default later-failure rollback");
    Require(host.Load("@Default:Engine.GameInfo",p,0,Vm::Scope::Default).text=="Player" && r.writes==1u,
        "ClassContext default assignment escaped rollback");
    f.bytecode=Join({Let(Ref(0x01,5),Int(23)),Let(Ref(0x01,5),Context({0x2a},Int(10),1u,0x12u)),Return()});
    r=refused(f,Vm::Status::Unsupported,"Unaudited partial zero fill refusal");
    Require(host.Load("Self",value).integer==97 && r.error=="VM partial context zero-fill destination is not established",
        "Partial zero fill was silently widened or prefix assignment escaped rollback");
    auto callee=Function("OriginalStaticShape",Return(Ref(0x00,9)),"StaticSource");callee.flags=0x2002u;
    callee.variables={host.Property(callee,9,"Caller",Value::Text(Vm::Kind::Object,{}),0x80u,1u,true)};
    host.finals[{f.source,8}]=std::make_shared<Vm::Function>(callee);
    f.bytecode=Return(Context(Ref(0x20,1),Call(0x1c,8,{{0x17}}),4u,0x12u));
    r=Vm::Execute(host,f,"Self");Returned(r,"Static call receiver versus caller arguments");
    Require(r.value.text=="Self" && host.preparations.back()=="@Default:Engine.GameInfo:"+callee.path,
        "Static flag retargeted receiver or changed caller argument frame");
    for (const auto& [zero,bytes]:std::vector<std::pair<Value,std::uint8_t>>{
        {Value::Byte(0u),1u},{Value::Integer(0),4u},{Value::Float(0),4u},{Value::Bool(false),4u},
        {Value::Text(Vm::Kind::Name,"None"),4u},{Value::Text(Vm::Kind::Object,{}),4u},
        {Value::Text(Vm::Kind::String,{}),12u},{Value::Vector({}),12u},{Value::Rotator({}),12u}}) {
        auto argumentCallee=Function("TypedContextArgument",Return(Ref(0x00,10)),"ParameterSource");
        argumentCallee.variables={host.Property(argumentCallee,10,"Value",zero,0x80u,1u,true)};
        host.finals[{f.source,11}]=std::make_shared<Vm::Function>(argumentCallee);
        for(const auto size:{std::uint8_t{0u},bytes}) {
            f.bytecode=Return(Call(0x1c,11,{Context({0x2a},Native(4095u),size,0x12u)}));
            r=Vm::Execute(host,f,"Self");Returned(r,"Typed script parameter null context destination");
            Require(Vm::Equal(r.value,zero),"Null ClassContext argument did not retain/zero actual declared parameter layout");
        }
    }
    f.bytecode=Return(Native(174u,{Context({0x2a},Real(99),4u,0x12u),Real(2.5f)}));
    r=Vm::Execute(host,f,"Self");Returned(r,"Direct numeric native null Float destination");
    Require(r.value.kind==Vm::Kind::Float && r.value.floating==2.5f,"Float null argument got untyped Nothing instead of actual Float zero");
    for(const auto first:{false,true}) {
        const auto nullFloat=Context({0x2a},Native(4095u),4u,0x12u);
        f.bytecode=Return(first ? Native(244u,{nullFloat,Real(2.5f)}) : Native(244u,{Real(2.5f),nullFloat}));
        r=Vm::Execute(host,f,"Self");Returned(r,"FMin typed null Float argument destination");
        Require(r.value.kind==Vm::Kind::Float && r.value.floating==0.0f,"FMin did not use actual four-byte Float zero destination");
    }
    for(const auto bytes:{std::uint8_t{0u},std::uint8_t{4u}}) for(const bool first:{false,true}) {
        const auto nullFloat=Context({0x2a},Native(4095u),bytes,0x12u);
        f.bytecode=Return(first ? Native(245u,{nullFloat,Real(-2.5f)}) : Native(245u,{Real(-2.5f),nullFloat}));
        r=Vm::Execute(host,f,"Self");Returned(r,"FMax typed null Float argument destination");
        Require(r.value.kind==Vm::Kind::Float && r.value.floating==0.0f,
            "FMax did not retain/zero its actual four-byte Float argument destination");
    }
    for(const auto bytes:{std::uint8_t{0u},std::uint8_t{4u}}) for(unsigned slot=0u;slot<3u;++slot) {
        const auto nullInt=Context({0x2a},Native(4095u),bytes,0x12u);
        f.bytecode=Return(slot==0u ? Native(251u,{nullInt,Int(-5),Int(5)}) :
            slot==1u ? Native(251u,{Int(-2),nullInt,Int(10)}) : Native(251u,{Int(2),Int(-10),nullInt}));
        r=Vm::Execute(host,f,"Self");Returned(r,"Clamp typed null Int argument destination");
        Require(r.value.kind==Vm::Kind::Int && r.value.integer==0,
            "Clamp did not retain/zero its actual four-byte Int argument destination");
    }
    f.bytecode=Return(Native(215u,{Context({0x2a},Native(4095u),12u,0x12u),
        Bytes{0x23,0,0,128,63,0,0,0,64,0,0,64,64}}));
    r=Vm::Execute(host,f,"Self");Returned(r,"Direct Vector native null destination");
    Require(r.value.kind==Vm::Kind::Vector && r.value.vector==std::array<float,3>{1,2,3},"Vector null argument layout was lost");
    f.bytecode=Return(Native(1003u,{Context({0x2a},Int(9),4u,0x12u)}));
    r=refused(f,Vm::Status::Unsupported,"Unaudited direct native argument destination");
    Require(r.error=="VM context argument result destination layout is unavailable", "Unknown direct native guessed null argument layout from byte count");
    std::cout<<"Class default receiver/result destinations: "<<checks-initialChecks<<" checks, "
        <<rejections-initialRejections<<" refusal controls.\n";
}
void CoreColorResultLayoutContracts() {
    const auto initialChecks=checks,initialRejections=rejections;
    const auto color=[](std::uint8_t r,std::uint8_t g,std::uint8_t b,std::uint8_t a) {
        Value value;value.kind=Vm::Kind::Struct;
        value.fields={{"r",Value::Byte(r)},{"g",Value::Byte(g)},{"b",Value::Byte(b)},{"a",Value::Byte(a)}};
        return value;
    };
    const auto zero=color(0u,0u,0u,0u),original=color(11u,37u,91u,203u);
    const auto layout=Vm::ResultLayout::CoreColor4;
    const auto nullResult=[](std::uint8_t bytes,std::uint8_t op=0x19u) {return Context({0x2a},Native(4095u),bytes,op);};
    Require(Vm::Property{}.resultLayout==Vm::ResultLayout::Unknown && Vm::Reference{}.resultLayout==Vm::ResultLayout::Unknown,
        "Result layout capability is not Unknown by default");
    for(const auto op:{std::uint8_t{0x19u},std::uint8_t{0x12u}}) for(const int mode:{-1,0,4}) {
        const auto expression=mode<0?Bytes{0x0b}:nullResult(static_cast<std::uint8_t>(mode),op);
        const auto expected=mode==4?zero:original;
        for(const std::string path:{"instance","default","local","array","local-array","member","context"}) {
            TestHost host;auto function=Function("ColorDestination"+path,{});Bytes destination;Bytes prefix;
            Vm::Property target;Value stored=original;auto scope=Vm::Scope::Instance;std::string receiver="Self";
            if(path=="member") {
                stored.kind=Vm::Kind::Struct;stored.fields={{"tint",original},{"sentinel",Value::Integer(77)}};
                target=host.Property(function,1,"Container",stored);host.Store("Self",target,{stored});
                host.Property(function,2,"Tint",zero,0u,1u,false,layout);destination=Member(2,Ref(0x01,1));
            } else if(path=="local" || path=="local-array") {
                const auto count=path=="local-array"?2u:1u;
                target=host.Property(function,1,"ColorLocal",zero,0u,count,true,layout);function.variables={target};
                const auto source=host.Property(function,2,"InitialColor",zero);host.Store("Self",source,{original});
                destination=count==1u?Ref(0x00,1):Element(Int(1),Ref(0x00,1));
                prefix=Let(destination,Ref(0x01,2));
            } else {
                const auto count=path=="array"?2u:1u;
                target=host.Property(function,1,"ColorTarget",zero,0u,count,false,layout);
                if(path=="default") scope=Vm::Scope::Default;
                if(path=="context") {receiver="Other";host.objects[{function.source,3}]=receiver;}
                host.Store(receiver,target,count==1u?std::vector<Value>{original}:std::vector<Value>{original,original},scope);
                destination=Ref(scope==Vm::Scope::Default?0x02u:0x01u,1);
                if(path=="array") destination=Element(Int(1),destination);
                if(path=="context") destination=Context(Ref(0x20,3),destination);
            }
            function.bytecode=Join({prefix,Let(destination,expression),Return(destination)});
            const auto result=Vm::Execute(host,function,"Self");Returned(result,"CoreColor4 "+path+" result destination");
            Require(Vm::Equal(result.value,expected),"CoreColor4 "+path+" zero/no-write destination changed incorrectly");
            if(path=="member") {
                const auto actual=host.Load("Self",target);
                Require(Vm::Equal(actual.fields.at("tint"),expected) && actual.fields.at("sentinel").integer==77,
                    "Color member result write cleared unrelated aggregate storage");
            } else if(path=="array")
                Require(Vm::Equal(host.Load("Self",target,0u),original) && Vm::Equal(host.Load("Self",target,1u),expected),
                    "Color array element result write cleared another element");
            Require(result.writes==(path=="local" || path=="local-array"?1u:0u)+(mode==4?1u:0u),
                "Color zero-size/NoWrite created a write or full result write was omitted");
        }
        {
            TestHost host;auto function=Function("CoreColorReturnDestination",Return(expression));
            function.variables={host.Property(function,1,"ReturnValue",original,0x480u,1u,true,layout)};
            const auto result=Vm::Execute(host,function,"Self");Returned(result,"CoreColor4 return result destination");
            Require(Vm::Equal(result.value,expected) && result.writes==0u,"Color return lost its trusted destination or fabricated property writes");
        }
        for(const bool native:{false,true}) for(const bool seeded:{false,true}) {
            if(seeded && !native) continue;
            TestHost host;auto caller=Function("DeclaredCoreColorArgument",Return(Call(0x1c,7,{expression})));
            auto callee=Function("ColorCallee",Return(Ref(0x00,1)),"ColorSource");
            callee.variables={host.Property(callee,1,"Color",seeded?zero:original,0x90u,1u,true,layout)};
            if(native) {callee.flags=0x400u;callee.nativeIndex=1454u;}
            host.finals[{caller.source,7}]=std::make_shared<Vm::Function>(callee);
            if(seeded) host.argumentSeed=[&](std::uint16_t,const std::string&,std::size_t,const Vm::Function*) {
                return std::optional<Value>{original};
            };
            host.executionNative=[&](const std::string&,const std::vector<Vm::Evaluation>& arguments,const Vm::Function*,Vm::Execution&) {
                auto value=arguments.at(0).Load();
                Require((value.kind==Vm::Kind::Nothing)==(mode<0),"Trusted Color native argument consumed explicit Nothing");
                if(value.kind==Vm::Kind::Nothing) value=original; // Native wrapper owns its omitted default.
                return Vm::Evaluation{value,{}};
            };
            const auto result=Vm::Execute(host,caller,"Self");Returned(result,"Declared Color script/native/seed result destination");
            Require(Vm::Equal(result.value,expected),"Declared Color parameter/native seed did not use its own layout authority");
        }
    }
    {
        TestHost host;auto function=Function("CoreColorStateLocal",Join({Let(Element(Int(1),Ref(0x00,1)),nullResult(4u)),{0x08}}));
        const auto property=host.Property(function,1,"StateColors",zero,0u,2u,true,layout);function.variables={property};
        host.Store("Self",property,{original,original});
        QuestVr::StateFrame frame;frame.codePath=frame.localsCodePath=function.path;
        host.state=QuestVr::StateObject{true,true,frame,{}};host.statePrograms[function.path]=function;
        const auto result=Vm::ResumeState(host,"Self");Returned(result,"CoreColor4 retained state local element");
        Require(result.status==Vm::Status::Stopped && Vm::Equal(host.Load("Self",property,0u),original) &&
            Vm::Equal(host.Load("Self",property,1u),zero),"State local/element lost trusted Color layout or wrote another element");
    }
    for(const bool trusted:{false,true}) for(const auto bytes:{1u,2u,3u,4u,5u,255u}) {
        if(trusted && bytes==4u) continue;
        TestHost host;auto function=Function("ColorLayoutRefusal",Join({Native(1000u),Let(Ref(0x01,2),Int(55)),
            Let(Ref(0x01,1),nullResult(static_cast<std::uint8_t>(bytes))),Return()}));
        const auto target=host.Property(function,1,"Color",zero,0u,1u,false,trusted?layout:Vm::ResultLayout::Unknown);
        const auto marker=host.Property(function,2,"Marker",Value::Integer(0));host.Store("Self",target,{original});host.Store("Self",marker,{Value::Integer(3)});
        const auto result=Vm::ExecuteTransaction(host,[&](Vm::Execution& execution) {
            try { execution.CallFunction(function,"Self"); } catch(const std::exception&) {}return Value{};
        });
        Failed(result,Vm::Status::Unsupported,host,"Color partial/overflow/untrusted-shape result refuses transactionally");
        Require(Vm::Equal(host.Load("Self",target),original) && host.Load("Self",marker).integer==3 && host.effects==0u && host.rng==12345u,
            "Color result layout refusal committed preceding state/effects");
    }
    {
        TestHost host;const auto function=Function("UnknownColorZeroSize",Join({Let(Ref(0x01,1),nullResult(0u)),Return(Ref(0x01,1))}));
        const auto target=host.Property(function,1,"SameShapedStruct",zero);host.Store("Self",target,{original});
        const auto result=Vm::Execute(host,function,"Self");Returned(result,"Unknown same-shaped Color zero-size does not require a byte layout");
        Require(Vm::Equal(result.value,original) && result.writes==0u,"Unknown Color zero-size lost original NoWrite contract");
    }
    {
        TestHost host;host.suppliedReferenceLayout=layout;
        const auto function=Function("PropertyOwnsColorAuthority",Join({Let(Ref(0x01,1),nullResult(4u)),Return()}));
        const auto target=host.Property(function,1,"UntrustedColorShape",zero);host.Store("Self",target,{original});
        const auto result=Vm::Execute(host,function,"Self");
        Failed(result,Vm::Status::Unsupported,host,"Unknown Property cannot inherit a conflicting host Reference Color capability");
        Require(Vm::Equal(host.Load("Self",target),original),"Conflicting host reference capability changed an untrusted property");
    }
    {
        TestHost host;const auto function=Function("UnknownResultCapability",Return(Ref(0x01,1)));
        const auto target=host.Property(function,1,"Color",zero,0u,1u,false,static_cast<Vm::ResultLayout>(99));
        host.Store("Self",target,{original});
        const auto result=Vm::Execute(host,function,"Self");
        Failed(result,Vm::Status::Invalid,host,"Unknown trusted result capability is not silently accepted");
    }
    for(const bool sequential:{false,true}) for(const bool trusted:{false,true}) for(const auto bytes:{0u,1u,2u,3u,4u,5u}) {
        TestHost host;const auto index=sequential?1438u:1454u;
        const auto caller=Function("ColorNativeSeedAuthority",Return(Call(0x1c,7,{nullResult(static_cast<std::uint8_t>(bytes))})));
        auto callee=Function("NativeColorSeed",{},"NativeColorSource");callee.flags=0x400u;callee.nativeIndex=index;
        callee.variables={host.Property(callee,1,"Color",zero,0x90u,1u,true,trusted?layout:Vm::ResultLayout::Unknown)};
        host.finals[{caller.source,7}]=std::make_shared<Vm::Function>(callee);
        host.argumentSeed=[&](std::uint16_t,const std::string&,std::size_t,const Vm::Function*) {
            return sequential?std::optional<Value>{}:std::optional<Value>{original};
        };
        host.hasSequentialSeeds=[&](std::uint16_t actual,const std::string&,const Vm::Function*) {return sequential && actual==index;};
        host.sequentialArgumentSeed=[&](std::uint16_t,const std::string&,std::size_t,const Vm::Function*,const std::vector<Value>&) {
            return std::optional<Value>{original};
        };
        host.executionNative=[](const std::string&,const std::vector<Vm::Evaluation>& arguments,const Vm::Function*,Vm::Execution&) {
            return Vm::Evaluation{arguments.at(0).Load(),{}};
        };
        const auto result=Vm::Execute(host,caller,"Self");
        if(bytes==0u || (trusted && bytes==4u)) {
            Returned(result,"Declared native Color seed layout authority");
            Require(Vm::Equal(result.value,bytes==4u?zero:original),"Native Color seed exact/zero-size result differs");
        } else Failed(result,Vm::Status::Unsupported,host,"Native Color seed cannot infer shape/partial packed Color writes");
    }
    {
        TestHost host;host.argumentSeed=[&](std::uint16_t,const std::string&,std::size_t,const Vm::Function*) {return std::optional<Value>{original};};
        const auto result=Vm::Execute(host,Function("OpcodeColorSeedNoAuthority",Return(Native(1454u,{nullResult(4u)}))),"Self");
        Failed(result,Vm::Status::Unsupported,host,"Opcode-only Color-shaped seed remains Unknown without a trusted declaration");
    }
    {
        TestHost host;const auto result=Vm::Execute(host,Function("NoDestinationForColorContext",Return(nullResult(4u))),"Self");
        Returned(result,"Null Context without a declared result destination stays absent");
        Require(result.value.kind==Vm::Kind::Nothing && host.nativeArguments.empty(),"Color byte count manufactured destination authority");
    }
    std::vector<Value> malformed;
    auto bad=original;bad.fields.erase("a");malformed.push_back(bad);
    bad=original;bad.fields["extra"]=Value::Byte(1u);malformed.push_back(bad);
    bad=original;bad.fields["R"]=bad.fields.at("r");bad.fields.erase("r");malformed.push_back(bad);
    bad=original;bad.fields["r"]=Value::Integer(11);malformed.push_back(bad);
    bad=original;bad.fields["r"].integer=-1;malformed.push_back(bad);
    bad=original;bad.fields["r"].integer=256;malformed.push_back(bad);
    bad=original;bad.fields["r"].fields["nested"]=Value::Byte(1u);malformed.push_back(bad);
    bad=original;bad.text="not packed storage";malformed.push_back(bad);
    bad=original;bad.intArray={1};malformed.push_back(bad);
    malformed.push_back(Value::Vector({1.0f,2.0f,3.0f}));
    for(const auto& invalid:malformed) for(const std::string ingress:{"stored","schema","argument","seed","sequential-seed","written","return","native-return"}) {
        TestHost host;auto function=Function("MalformedTrustedColor",{});Vm::Property target;
        if(ingress=="stored" || ingress=="schema" || ingress=="written") {
            target=host.Property(function,1,"Color",ingress=="schema"?invalid:zero,0u,1u,false,layout);
            host.Store("Self",target,{ingress=="stored"?invalid:original});
            const auto source=host.Property(function,2,"UntrustedSource",zero);host.Store("Self",source,{invalid});
            function.bytecode=Join({Native(1000u),Let(Ref(0x01,1),ingress=="written"?Ref(0x01,2):nullResult(4u)),Return()});
        } else {
            const bool returning=ingress=="return" || ingress=="native-return";
            const bool seeded=ingress=="seed" || ingress=="sequential-seed";
            auto callee=Function("MalformedColorCallee",returning?Return(Native(1008u)):Return(Ref(0x00,1)),"BadColorSource");
            callee.variables={host.Property(callee,1,"Color",zero,returning?0x480u:0x80u,1u,true,layout)};
            if(seeded || ingress=="native-return") {
                callee.flags=0x400u;callee.nativeIndex=ingress=="seed"?1454u:ingress=="sequential-seed"?1438u:1008u;
            }
            host.finals[{function.source,7}]=std::make_shared<Vm::Function>(callee);
            const auto source=host.Property(function,2,"UntrustedSource",zero);host.Store("Self",source,{invalid});
            function.bytecode=Join({Native(1000u),Return(Call(0x1c,7,returning?std::initializer_list<Bytes>{}:
                std::initializer_list<Bytes>{seeded?nullResult(4u):Ref(0x01,2)}))});
            host.argumentSeed=[&](std::uint16_t actual,const std::string&,std::size_t,const Vm::Function*) {
                return actual==1454u?std::optional<Value>{invalid}:std::nullopt;
            };
            host.hasSequentialSeeds=[](std::uint16_t actual,const std::string&,const Vm::Function*) {return actual==1438u;};
            host.sequentialArgumentSeed=[&](std::uint16_t,const std::string&,std::size_t,const Vm::Function*,const std::vector<Value>&) {
                return std::optional<Value>{invalid};
            };
            host.executionNative=[&](const std::string&,const std::vector<Vm::Evaluation>&,const Vm::Function*,Vm::Execution&) {
                return Vm::Evaluation{invalid,{}};
            };
        }
        const auto result=Vm::Execute(host,function,"Self");
        Failed(result,Vm::Status::Invalid,host,"Malformed CoreColor4 "+ingress+" ingress refuses before canonicalization");
        Require(host.effects==0u && host.rng==12345u,"Malformed trusted Color committed prior side effects");
        if(ingress=="stored" || ingress=="schema" || ingress=="written")
            Require(Vm::Equal(host.Load("Self",target),ingress=="stored"?invalid:original),"Malformed Color ingress modified target before rejection");
    }
    {
        TestHost host;const auto function=Function("GuardedColorOut",Return(Native(1008u,{Element(Int(1),Ref(0x01,1))})));
        const auto target=host.Property(function,1,"Colors",zero,0u,2u,false,layout);host.Store("Self",target,{original,original});
        host.executionNative=[&](const std::string&,const std::vector<Vm::Evaluation>& arguments,const Vm::Function*,Vm::Execution&) {
            const auto reference=arguments.at(0).reference;
            Require(reference && reference->resultLayout==layout,"Guarded native OUT element lost Color authority");
            reference->write(zero);return Vm::Evaluation{reference->read(),{}};
        };
        const auto result=Vm::Execute(host,function,"Self");Returned(result,"Guarded native OUT reference preserves Color authority");
        Require(Vm::Equal(result.value,zero) && Vm::Equal(host.Load("Self",target,0u),original) &&
            Vm::Equal(host.Load("Self",target,1u),zero),"Guarded Color write changed another fixed array element");
    }
    {
        TestHost host;const auto function=Function("TrustedColorRollback",Join({Native(1000u),Let(Ref(0x01,1),nullResult(4u)),Native(4095u),Return()}));
        const auto target=host.Property(function,1,"Color",zero,0u,1u,false,layout);host.Store("Self",target,{original});
        const auto result=Vm::Execute(host,function,"Self");Failed(result,Vm::Status::Unsupported,host,"Later failure rolls back a successful full Color zero write");
        Require(result.writes==1u && Vm::Equal(host.Load("Self",target),original) && host.effects==0u && host.rng==12345u,
            "Color write rollback failed to restore the original four bytes/effects");
    }
    for(const bool budget:{false,true}) {
        TestHost host;auto limits=Vm::Limits{};if(budget) limits.writes=1u;
        const auto function=Function("CaughtGuardedColorFailure",Join({Native(1000u),Let(Ref(0x01,2),Int(55)),
            Return(Native(1008u,{Ref(0x01,1)}))}));
        const auto target=host.Property(function,1,"Color",zero,0u,1u,false,layout);
        const auto marker=host.Property(function,2,"Marker",Value::Integer(0));host.Store("Self",target,{original});host.Store("Self",marker,{Value::Integer(3)});
        bool caught{};auto invalid=original;invalid.fields["r"].integer=256;
        host.executionNative=[&](const std::string&,const std::vector<Vm::Evaluation>& arguments,const Vm::Function*,Vm::Execution&) {
            const auto reference=arguments.at(0).reference;
            Require(reference && reference->resultLayout==layout,"Caught Color guard fixture lost its layout");
            try { reference->write(budget?zero:invalid); } catch(const std::exception&) {caught=true;}
            return Vm::Evaluation{Value::Bool(true),{}};
        };
        const auto result=Vm::Execute(host,function,"Self",{},limits);
        Failed(result,budget?Vm::Status::Budget:Vm::Status::Invalid,host,"Caught guarded Color validation/write-budget failure poisons shared execution");
        Require(caught && Vm::Equal(host.Load("Self",target),original) && host.Load("Self",marker).integer==3 &&
            host.effects==0u && host.rng==12345u,"Native suppressed a guarded Color failure and committed its earlier prefix");
    }
    std::cout<<"Trusted Core.Object.Color result layout: "<<checks-initialChecks<<" checks and "
        <<rejections-initialRejections<<" refusal controls.\n";
}
void UninitializedNativeArgumentContracts() {
    const auto initialChecks=checks,initialRejections=rejections;
    const auto color=[](std::uint8_t r,std::uint8_t g,std::uint8_t b,std::uint8_t a) {
        Value value;value.kind=Vm::Kind::Struct;
        value.fields={{"r",Value::Byte(r)},{"g",Value::Byte(g)},{"b",Value::Byte(b)},{"a",Value::Byte(a)}};return value;
    };
    const auto original=color(11u,37u,91u,203u),zero=color(0u,0u,0u,0u),replacement=color(1u,2u,3u,4u);
    const auto layout=Vm::ResultLayout::CoreColor4;
    const auto configure=[&](TestHost& host) {
        auto native=Function("SetTileColor",{},"Extension.Window");native.nativeIndex=1475u;native.flags=0x401u;
        native.variables={host.Property(native,11,"NewColor",zero,0x80u,1u,true,layout)};
        host.finals[{"Fixture",7}]=std::make_shared<Vm::Function>(native);
        host.uninitializedArgumentLayout=[&host,layout](std::uint16_t index,const std::string& receiver,std::size_t slot,
            const Vm::Function* declaration)->std::optional<Vm::ResultLayout> {
            if(index!=1475u) return std::nullopt;
            if(slot!=0u) throw std::runtime_error("Fixture SetTileColor has exactly one native result slot");
            Require(receiver=="Self" || receiver=="Other","Uninitialized native layout used an argument/source rather than native receiver");
            Require(!declaration || declaration->path=="Extension.Window.SetTileColor","Uninitialized native layout lost actual declaration");
            host.observations.push_back("uninitialized-layout");return layout;
        };
        host.executionNative=[](const std::string&,const std::vector<Vm::Evaluation>& arguments,const Vm::Function*,Vm::Execution&) {
            if(arguments.size()!=1u) throw std::runtime_error("Fixture SetTileColor requires one Color");
            Require(!arguments[0].reference,"Uninitialized native ordinary Color input retained an OUT alias");
            return Vm::Evaluation{arguments[0].Load(),{}};
        };
        return native;
    };
    {
        TestHost host;const auto native=configure(host);bool read{};
        auto alias=std::make_shared<Vm::Reference>();alias->zero=zero;
        alias->read=[&]()->Value {read=true;throw std::runtime_error("Full zero-fill must not read an uninitialized alias");};
        alias->write=[](const Value&) {throw std::runtime_error("Ordinary input must not write the alias");};
        const Vm::Evaluation argument{{},alias,Vm::ResultDisposition::ZeroFill,4u};
        const auto result=Vm::ExecuteTransaction(host,[&](Vm::Execution& execution) {
            return execution.CallFunction(native,"Self",{argument});
        });
        Returned(result,"Full four-byte uninitialized Color clearing does not read an alias");
        Require(!read && Vm::Equal(result.value,zero),"Complete zero-fill read old pointer-like native temporary storage");
    }
    for(const bool declared:{false,true}) for(const auto op:{std::uint8_t{0x19u},std::uint8_t{0x12u}})
        for(const bool written:{false,true}) {
        TestHost host;configure(host);
        const auto input=written?Ref(0x01,1):Context({0x2au},Native(4095u),4u,op);
        const auto caller=Function("CompleteUninitializedNativeColor",Return(declared?Call(0x1c,7,{input}):Native(1475u,{input})));
        const auto source=host.Property(caller,1,"SourceColor",zero);host.Store("Self",source,{original});
        const auto result=Vm::Execute(host,caller,"Self");Returned(result,"Uninitialized native accepts only complete written Color/full-four-byte Context results");
        Require(Vm::Equal(result.value,written?original:zero) &&
            std::count(host.observations.begin(),host.observations.end(),"uninitialized-layout")==1,
            "Uninitialized Color used a declaration zero for written bytes, repeated its hook, or lost full-zero semantics");
        Require(host.observations.front()=="uninitialized-layout","Uninitialized layout was not captured before its argument expression");
    }
    for(const bool declared:{false,true}) {
        TestHost host;configure(host);
        const auto caller=Function("UninitializedColorArgumentFrame",Return(Context(Ref(0x20,9),
            declared?Call(0x1c,7,{Ref(0x01,1)}):Native(1475u,{Ref(0x01,1)}))));
        const auto source=host.Property(caller,1,"SourceColor",zero);host.Store("Self",source,{original});host.Store("Other",source,{replacement});
        host.objects[{caller.source,9}]="Other";
        host.executionNative=[&](const std::string& receiver,const std::vector<Vm::Evaluation>& arguments,const Vm::Function*,Vm::Execution& execution) {
            Require(receiver=="Other" && !arguments[0].reference && Vm::Equal(arguments[0].Load(),original),
                "1475 argument used its native receiver instead of FFrame.Object or retained the source alias");
            execution.WriteReference(host.Reference({"Self",Vm::Scope::Instance,source.key},source),replacement);
            Require(Vm::Equal(arguments[0].Load(),original),"1475 ordinary copied input changed after host entry mutated its source");
            return Vm::Evaluation{arguments[0].Load(),{}};
        };
        const auto result=Vm::Execute(host,caller,"Self");Returned(result,"1475 preserves source-frame evaluation and copies native input");
        Require(Vm::Equal(result.value,original) && Vm::Equal(host.Load("Self",source),replacement) && result.writes==1u,
            "1475 native input copy suppressed the actual later write or retained its alias");
    }
    for(const bool declared:{false,true}) for(const auto op:{std::uint8_t{0x19u},std::uint8_t{0x12u}})
        for(const int bytes:{-1,0,1,2,3,5,255}) {
        TestHost host;configure(host);
        const auto input=bytes<0?Bytes{0x0bu}:Context({0x2au},Native(4095u),static_cast<std::uint8_t>(bytes),op);
        const auto caller=Function("IncompleteUninitializedNativeColor",Join({Native(1000u),Let(Ref(0x01,1),Int(55)),
            Return(declared?Call(0x1c,7,{input}):Native(1475u,{input}))}));
        const auto marker=host.Property(caller,1,"Marker",Value::Integer(0));host.Store("Self",marker,{Value::Integer(3)});
        const auto result=Vm::ExecuteTransaction(host,[&](Vm::Execution& execution) {
            try { execution.CallFunction(caller,"Self"); } catch(const std::exception&) {}return Value::Integer(123);
        });
        Failed(result,Vm::Status::Unsupported,host,"1475 incomplete result never falls back to declaration Color zero");
        Require(host.effects==0u && host.rng==12345u && host.Load("Self",marker).integer==3 &&
            std::find(host.nativeIndices.begin(),host.nativeIndices.end(),1475u)==host.nativeIndices.end(),
            "Incomplete native Color reached its body or committed its earlier transaction prefix");
    }
    for(const bool directEvent:{false,true}) for(const auto disposition:{Vm::ResultDisposition::Written,Vm::ResultDisposition::NoWrite,Vm::ResultDisposition::ZeroFill})
        for(const auto bytes:{0u,1u,2u,3u,4u,5u,255u}) {
        TestHost host;const auto native=configure(host);host.events[{"Self","SetTileColor",false}]=std::make_shared<Vm::Function>(native);
        const Vm::Evaluation argument{original,{},disposition,static_cast<std::uint8_t>(bytes)};
        const bool complete=disposition==Vm::ResultDisposition::Written || (disposition==Vm::ResultDisposition::ZeroFill && bytes==4u);
        const auto result=Vm::ExecuteTransaction(host,[&](Vm::Execution& execution) {
            return directEvent?execution.CallEvent("Self","SetTileColor",false,{argument}):execution.CallFunction(native,"Self",{argument});
        });
        if(complete) {
            Returned(result,"Direct Execution uninitialized Color inputs preserve write disposition until checked");
            Require(Vm::Equal(result.value,disposition==Vm::ResultDisposition::Written?original:zero),
                "Direct native input snapshots erased its disposition before full-write validation");
        } else Failed(result,Vm::Status::Unsupported,host,"Direct Execution rejects incomplete Color even with a canonical payload");
        Require(std::count(host.observations.begin(),host.observations.end(),"uninitialized-layout")==1,
            "Direct uninitialized native argument captured its authority more than once");
    }
    for(const auto disposition:{Vm::ResultDisposition::NoWrite,Vm::ResultDisposition::ZeroFill}) {
        TestHost host;const auto native=configure(host);bool read{};
        auto reference=std::make_shared<Vm::Reference>();reference->zero=zero;
        reference->read=[&]() ->Value {read=true;throw std::runtime_error("Unwritten native result read uninitialized storage");};
        const Vm::Evaluation argument{original,reference,disposition,0u};
        const auto result=Vm::Execute(host,native,"Self",{argument});
        Failed(result,Vm::Status::Unsupported,host,"Uninitialized direct argument never reads a no-write alias");
        Require(!read,"Uninitialized argument read its alias before checking write disposition");
    }
    for(const bool direct:{false,true}) for(const bool sequential:{false,true}) {
        TestHost host;const auto native=configure(host);bool argumentEvaluated{};
        host.argumentSeed=[&](std::uint16_t index,const std::string&,std::size_t,const Vm::Function*)->std::optional<Value> {
            return index==1475u && !sequential?std::optional<Value>{original}:std::nullopt;
        };
        host.hasSequentialSeeds=[&](std::uint16_t index,const std::string&,const Vm::Function*) {return index==1475u && sequential;};
        host.sequentialArgumentSeed=[&](std::uint16_t,const std::string&,std::size_t,const Vm::Function*,const std::vector<Value>& prefix) {
            Require(prefix.empty(),"First uninitialized native slot saw a nonempty seed prefix");return std::optional<Value>{original};
        };
        const auto caller=Function("ConflictingUninitializedNativeSeed",Return(Native(1475u,{Native(1000u)})));
        auto reference=std::make_shared<Vm::Reference>();reference->zero=zero;
        reference->read=[&] {argumentEvaluated=true;return original;};
        const auto result=direct?Vm::Execute(host,native,"Self",{{{},reference}}):Vm::Execute(host,caller,"Self");
        Failed(result,Vm::Status::Invalid,host,"Uninitialized native result cannot also have an initialized entry/sequential seed");
        Require(!argumentEvaluated && host.effects==0u && host.nativeArguments.empty(),"Conflicting native seed evaluated its argument or invoked native body");
    }
    for(const bool direct:{false,true}) for(const auto badLayout:{Vm::ResultLayout::Unknown,static_cast<Vm::ResultLayout>(99)}) {
        TestHost host;const auto native=configure(host);
        host.uninitializedArgumentLayout=[&](std::uint16_t index,const std::string&,std::size_t,const Vm::Function*) {
            return index==1475u?std::optional<Vm::ResultLayout>{badLayout}:std::nullopt;
        };
        const auto caller=Function("UnknownUninitializedNativeLayout",Return(Native(1475u,{Native(1000u)})));
        const auto result=direct?Vm::Execute(host,native,"Self",{{original,{}}}):Vm::Execute(host,caller,"Self");
        Failed(result,badLayout==Vm::ResultLayout::Unknown?Vm::Status::Unsupported:Vm::Status::Invalid,host,
            "Uninitialized native requires established explicit layout authority");
        Require(host.effects==0u && host.nativeArguments.empty(),"Unknown uninitialized layout evaluated arguments or native work");
    }
    for(const bool direct:{false,true}) {
        TestHost host;auto native=configure(host);native.variables[0].resultLayout=Vm::ResultLayout::Unknown;
        host.finals[{"Fixture",7}]=std::make_shared<Vm::Function>(native);
        const auto caller=Function("UninitializedNativeDeclarationMismatch",Return(Call(0x1c,7,{Native(1000u)})));
        const auto result=direct?Vm::Execute(host,native,"Self",{{original,{}}}):Vm::Execute(host,caller,"Self");
        Failed(result,Vm::Status::Invalid,host,"Native uninitialized layout cannot override conflicting declaration authority");
        Require(host.effects==0u && host.nativeArguments.empty(),"Conflicting native declaration reached argument effects/body");
    }
    {
        TestHost host;configure(host);host.uninitializedArgumentLayout={};
        host.argumentSeed=[&](std::uint16_t index,const std::string&,std::size_t,const Vm::Function*) {
            return index==1475u?std::optional<Value>{original}:std::nullopt;
        };
        const auto result=Vm::Execute(host,Function("UninitializedColorShapeIsNotAuthority",Return(Native(1475u,
            {Context({0x2au},Native(4095u),4u)}))),"Self");
        Failed(result,Vm::Status::Unsupported,host,"A same-shaped raw Struct seed cannot grant opcode-only Color ABI authority");
    }
    std::vector<Value> malformed{Value{},Value::Integer(0)};auto bad=original;
    bad.fields.erase("a");malformed.push_back(bad);bad=original;bad.fields["extra"]=Value::Byte(1u);malformed.push_back(bad);
    bad=original;bad.fields["r"]=Value::Integer(11);malformed.push_back(bad);bad=original;bad.fields["r"].integer=256;malformed.push_back(bad);
    bad=original;bad.fields["r"].integer=-1;malformed.push_back(bad);bad=original;bad.fields["r"].fields["nested"]=Value::Byte(0u);malformed.push_back(bad);
    for(const auto& invalid:malformed) for(const int dispatch:{0,1,2}) {
        TestHost host;const auto native=configure(host);
        const auto caller=Function("MalformedUninitializedNativeColor",Join({Native(1000u),Return(dispatch==1?
            Call(0x1c,7,{Ref(0x01,1)}):Native(1475u,{Ref(0x01,1)}))}));
        const auto source=host.Property(caller,1,"UntrustedSource",invalid);host.Store("Self",source,{invalid});
        const auto result=dispatch==2?Vm::ExecuteTransaction(host,[&](Vm::Execution& execution) {
            execution.CallFunction(Function("EarlierEffect",Join({Native(1000u),Return()})),"Self");
            return execution.CallFunction(native,"Self",{{invalid,{}}});
        }):Vm::Execute(host,caller,"Self");
        Failed(result,Vm::Status::Invalid,host,"Fully written native Color still requires strict canonical Byte channels");
        Require(host.effects==0u && host.rng==12345u,"Malformed uninitialized native Color committed preceding effects");
    }
    {
        TestHost host;configure(host);auto caller=Function("AllUninitializedLayoutsBeforeArguments",Return(Native(1475u,
            {Native(1000u),Native(1000u)})));
        const auto result=Vm::Execute(host,caller,"Self");
        Failed(result,Vm::Status::Unsupported,host,"All uninitialized native layout lookups precede every argument");
        Require(host.effects==0u && host.nativeArguments.empty(),"A rejected second uninitialized layout evaluated the first argument");
    }
    for(const bool direct:{false,true}) {
        TestHost host;const auto native=configure(host);
        const auto caller=Function("MissingUninitializedNativeArgument",Return(Call(0x1c,7)));
        const auto result=direct?Vm::Execute(host,native,"Self"):Vm::Execute(host,caller,"Self");
        Failed(result,Vm::Status::Invalid,host,"Uninitialized native argument remains required");
        Require(host.nativeArguments.empty(),"Missing required uninitialized native argument reached its body");
    }
    for(const bool write:{false,true}) {
        TestHost host;configure(host);Vm::Execution* active{};bool attempted{};
        const auto caller=Function("ReadOnlyUninitializedNativeLayout",Return(Native(1475u,{Native(1000u)})));
        const auto marker=host.Property(caller,1,"Marker",Value::Integer(0));host.Store("Self",marker,{Value::Integer(3)});
        host.uninitializedArgumentLayout=[&](std::uint16_t index,const std::string&,std::size_t,const Vm::Function*)->std::optional<Vm::ResultLayout> {
            if(index!=1475u) return std::nullopt;
            attempted=true;
            try {
                if(write) active->WriteReference(host.Reference({"Self",Vm::Scope::Instance,marker.key},marker),Value::Integer(55));
                else active->CallFunction(Function("ForbiddenHookCallback",Join({Native(1000u),Return()})),"Self");
            } catch(const std::exception&) {}return layout;
        };
        const auto result=Vm::ExecuteTransaction(host,[&](Vm::Execution& execution) {
            active=&execution;try { execution.CallFunction(caller,"Self"); } catch(const std::exception&) {}return Value{};
        });
        Failed(result,Vm::Status::Invalid,host,"Uninitialized native layout hook is read-only and caught callbacks poison execution");
        Require(attempted && host.effects==0u && host.Load("Self",marker).integer==3 && result.writes==0u,
            "Read-only uninitialized hook performed a VM mutation or escaped shared poison");
    }
    {
        TestHost host;configure(host);
        const auto caller=Function("ThrowingUninitializedNativeLayout",Join({Native(1000u),Return(Native(1475u,{Native(1000u)}))}));
        host.uninitializedArgumentLayout=[](std::uint16_t index,const std::string&,std::size_t,const Vm::Function*)->std::optional<Vm::ResultLayout> {
            if(index==1475u) throw std::runtime_error("Fixture uninitialized native authority failure");
            return std::nullopt;
        };
        const auto result=Vm::ExecuteTransaction(host,[&](Vm::Execution& execution) {
            try { execution.CallFunction(caller,"Self"); } catch(const std::exception&) {}return Value{};
        });
        Failed(result,Vm::Status::Unsupported,host,"Caught uninitialized layout hook exception poisons enclosing transaction");
        Require(host.effects==0u && host.rng==12345u && std::count(host.observations.begin(),host.observations.end(),"effect")==1,
            "Uninitialized layout hook failure evaluated later input or committed prior effects");
    }
    {
        TestHost host;configure(host);auto limits=Vm::Limits{};limits.localElements=4u;
        const auto caller=Function("UninitializedColorNodeBudget",Return(Native(1475u,{Context({0x2au},Native(4095u),4u)})));
        const auto result=Vm::ExecuteTransaction(host,[&](Vm::Execution& execution) {
            try { execution.CallFunction(caller,"Self"); } catch(const std::exception&) {}return Value{};
        },limits);
        Failed(result,Vm::Status::Budget,host,"Complete uninitialized Color materialization retains aggregate Value node bounds");
        Require(host.nativeArguments.empty(),"Uninitialized Color node-budget failure reached native body");
    }
    {
        TestHost host;const auto native=configure(host);auto limits=Vm::Limits{};
        limits.retainedBytes=sizeof(std::optional<Vm::ResultLayout>)-1u;
        const auto result=Vm::ExecuteTransaction(host,[&](Vm::Execution& execution) {
            try { execution.CallFunction(native,"Self",{{original,{}}}); } catch(const std::exception&) {}return Value{};
        },limits);
        Failed(result,Vm::Status::Budget,host,"Uninitialized native layout slot storage is bounded before argument reads");
        Require(result.error=="VM uninitialized native argument layout storage limit" && host.nativeArguments.empty(),
            "Uninitialized native layout capacity limit was not checked before materialization/native work");
    }
    {
        TestHost host;const auto native=configure(host);auto limits=Vm::Limits{};limits.retainedBytes=100000u;
        auto reserved=original;reserved.fields.at("r").text.reserve(200000u);
        std::vector<Vm::Evaluation> incoming;incoming.push_back({std::move(reserved),{}});
        Require(incoming[0].value.fields.at("r").text.capacity()>limits.retainedBytes,"Uninitialized Color capacity fixture lost its reserved payload");
        const auto result=Vm::ExecuteTransaction(host,[&](Vm::Execution& execution) {
            try { execution.CallFunction(native,"Self",incoming); } catch(const std::exception&) {}return Value{};
        },limits);
        Failed(result,Vm::Status::Budget,host,"Written uninitialized Color capacity is validated before detached copies can shrink it");
        Require(host.nativeArguments.empty(),"Oversized uninitialized Color allocation reached native body");
    }
    for(const bool declared:{false,true}) {
        TestHost host;configure(host);
        const auto caller=Function("NativeWrittenUninitializedColor",Return(declared?Call(0x1c,7,{Native(1008u)}):Native(1475u,{Native(1008u)})));
        host.executionNative=[&](const std::string&,const std::vector<Vm::Evaluation>& arguments,const Vm::Function*,Vm::Execution&) {
            return Vm::Evaluation{arguments.empty()?original:arguments[0].Load(),{}};
        };
        const auto result=Vm::Execute(host,caller,"Self");Returned(result,"Fully written native Color return initializes1475 destination");
        Require(Vm::Equal(result.value,original),"1475 failed to preserve a complete native Color return");
    }
    for(const bool declared:{false,true}) {
        TestHost host;configure(host);
        const auto caller=Function("AssignmentDoesNotInitializeNativeColor",Return(declared?Call(0x1c,7,{Let(Ref(0x01,1),Ref(0x01,2))}):
            Native(1475u,{Let(Ref(0x01,1),Ref(0x01,2))})));
        const auto target=host.Property(caller,1,"Target",zero),source=host.Property(caller,2,"Source",zero);
        host.Store("Self",target,{original});host.Store("Self",source,{replacement});
        const auto result=Vm::Execute(host,caller,"Self");Failed(result,Vm::Status::Unsupported,host,"Side-effect-only Let does not initialize1475 Color result");
        Require(result.writes==1u && Vm::Equal(host.Load("Self",target),original) && host.nativeIndices.empty(),
            "Uninitialized result refusal failed to roll back assignment-argument side effects");
    }
    {
        TestHost host;configure(host);auto caller=Function("ScriptDoesNotAskUninitializedNativeLayout",Return(Call(0x1c,9,{Int(7)})));
        auto script=Function("ScriptInput",Return(Ref(0x00,12)),"ScriptSource");
        script.variables={host.Property(script,12,"Value",Value::Integer(0),0x80u,1u,true)};
        host.finals[{caller.source,9}]=std::make_shared<Vm::Function>(script);
        host.uninitializedArgumentLayout=[](std::uint16_t,const std::string&,std::size_t,const Vm::Function*)->std::optional<Vm::ResultLayout> {
            throw std::runtime_error("Script incorrectly asked for native uninitialized layout");
        };
        const auto result=Vm::Execute(host,caller,"Self");Returned(result,"Script calls do not capture native uninitialized result contracts");
        Require(result.value.integer==7,"Uninitialized-native opt-in changed ordinary script argument binding");
    }
    std::cout<<"Uninitialized native Color argument result contracts: "<<checks-initialChecks<<" checks and "
        <<rejections-initialRejections<<" refusal controls.\n";
}
void DirectNativeInitializedArgumentContracts() {
    const auto initialChecks=checks,initialRejections=rejections;
    const auto declaration=[](std::uint16_t index,const std::vector<Value>& zeroes) {
        auto native=Function("DirectInitializedNative",{},"FixtureNative");native.flags=0x400u;native.nativeIndex=index;
        for(std::size_t slot=0u;slot<zeroes.size();++slot)
            native.variables.push_back({"Native.Input"+std::to_string(slot),"Input"+std::to_string(slot),zeroes[slot],0x80u,1u});
        return native;
    };
    for(const bool event:{false,true}) for(const auto disposition:{Vm::ResultDisposition::NoWrite,Vm::ResultDisposition::ZeroFill})
        for(const auto bytes:{0u,1u,2u,3u,4u}) {
        TestHost host;const auto native=declaration(250u,{Value::Integer(999),Value::Integer(999)});
        host.events[{"Self","Max",false}]=std::make_shared<Vm::Function>(native);
        bool read{};auto alias=std::make_shared<Vm::Reference>();alias->zero=Value::Integer(999);
        alias->read=[&]() ->Value {read=true;throw std::runtime_error("Direct no-write Max input read stale storage");};
        const Vm::Evaluation input{Value::Integer(999),alias,disposition,static_cast<std::uint8_t>(bytes)};
        const auto result=Vm::ExecuteTransaction(host,[&](Vm::Execution& execution) {
            return event?execution.CallEvent("Self","Max",false,{input,{Value::Integer(-7),{}}}):
                execution.CallFunction(native,"Self",{input,{Value::Integer(-7),{}}});
        });
        Returned(result,"Direct initialized native ignores stale payload/alias and uses audited zero temporary");
        Require(!read && result.value.kind==Vm::Kind::Int && result.value.integer==0 && result.writes==0u,
            "Direct Max used stale input/declaration zero or read unwritten storage");
    }
    for(const auto index:{std::uint16_t{186u},std::uint16_t{250u}}) for(const bool declared:{false,true})
        for(const auto bytes:{0u,1u,2u,3u,4u}) {
        TestHost host;const auto scalar=index==186u?Value::Float(99.0f):Value::Integer(99);
        const auto native=declaration(index,index==186u?std::vector<Value>{scalar}:std::vector<Value>{scalar,scalar});
        host.finals[{"Fixture",7}]=std::make_shared<Vm::Function>(native);
        const auto input=Context({0x2au},Native(4095u),static_cast<std::uint8_t>(bytes));
        const auto call=index==186u?(declared?Call(0x1c,7,{input}):Native(index,{input})):
            (declared?Call(0x1c,7,{input,Int(-7)}):Native(index,{input,Int(-7)}));
        const auto result=Vm::Execute(host,Function("KnownBuiltinZeroNotDeclarationValue",Return(call)),"Self");
        Returned(result,"Known builtin initialized scalar zero controls partial result writes");
        Require(index==186u?result.value.floating==0.0f:result.value.integer==0,
            "Builtin zero destination incorrectly inherited a nonzero declaration template");
    }
    for(const bool sequential:{false,true}) for(const auto bytes:{0u,1u,2u,3u,4u}) {
        TestHost host;const auto native=declaration(1454u,{Value::Float(99.0f)});
        host.argumentSeed=[&](std::uint16_t index,const std::string&,std::size_t,const Vm::Function*)->std::optional<Value> {
            return index==1454u?std::optional<Value>{Value::Float(-1.0f)}:std::nullopt;
        };
        host.hasSequentialSeeds=[&](std::uint16_t index,const std::string&,const Vm::Function*) {return index==1454u && sequential;};
        host.sequentialArgumentSeed=[](std::uint16_t,const std::string&,std::size_t,const Vm::Function*,const std::vector<Value>& prefix) {
            Require(prefix.empty(),"First direct sequential native input has a nonempty prefix");return std::optional<Value>{Value::Float(-2.0f)};
        };
        host.executionNative=[](const std::string&,const std::vector<Vm::Evaluation>& arguments,const Vm::Function*,Vm::Execution&) {
            Require(!arguments[0].reference && arguments[0].disposition==Vm::ResultDisposition::Written,
                "Direct initialized native argument was not copied/materialized");return Vm::Evaluation{arguments[0].Load(),{}};
        };
        const auto result=Vm::Execute(host,native,"Self",{{Value::Float(99.0f),{},Vm::ResultDisposition::ZeroFill,static_cast<std::uint8_t>(bytes)}});
        Returned(result,"Direct entry/sequential scalar defaults preserve native partial-byte semantics");
        const auto bits=std::bit_cast<std::uint32_t>(sequential?-2.0f:-1.0f);
        const auto expected=bytes==4u?0u:bytes==0u?bits:bits&~((std::uint32_t{1u}<<(bytes*8u))-1u);
        Require(std::bit_cast<std::uint32_t>(result.value.floating)==expected,
            "Direct scalar partial write lost a nonzero audited native default or sequential precedence");
    }
    {
        TestHost host;const auto native=declaration(1438u,{Value::Integer(0),Value::Integer(0),Value::Float(0),
            Value::Float(0),Value::Float(0),Value::Float(0)});
        host.argumentSeed=[&](std::uint16_t,const std::string&,std::size_t slot,const Vm::Function*)->std::optional<Value> {
            host.observations.push_back("entry"+std::to_string(slot));return std::nullopt;
        };
        host.hasSequentialSeeds=[&](std::uint16_t,const std::string&,const Vm::Function*) {host.observations.push_back("opt-in");return true;};
        host.sequentialArgumentSeed=[&](std::uint16_t,const std::string&,std::size_t slot,const Vm::Function*,const std::vector<Value>& prefix)->std::optional<Value> {
            host.observations.push_back("seq"+std::to_string(slot));Require(prefix.size()==slot,"Direct dependent prefix changed size/order");
            return slot>=4u?std::optional<Value>{prefix.at(slot-2u)}:std::nullopt;
        };
        auto first=std::make_shared<Vm::Reference>(),second=std::make_shared<Vm::Reference>();
        first->zero=second->zero=Value::Float(0.0f);Value source=Value::Float(-1.0f);
        first->read=[&] {host.observations.push_back("read2");return source;};
        second->read=[&] {host.observations.push_back("read3");source=Value::Float(99.0f);return Value::Float(0.75f);};
        host.executionNative=[](const std::string&,const std::vector<Vm::Evaluation>& arguments,const Vm::Function*,Vm::Execution&) {
            for(const auto& arg:arguments) Require(!arg.reference,"Direct ordinary sequential native input retained alias");
            return Vm::Evaluation{Value::Vector({arguments[2].Load().floating,arguments[4].Load().floating,arguments[5].Load().floating}),{}};
        };
        const auto result=Vm::Execute(host,native,"Self",{{Value::Integer(1),{}},{Value::Integer(2),{}},{{},first},{{},second},
            {Value::Float(999),{},Vm::ResultDisposition::NoWrite},{Value::Float(999),{},Vm::ResultDisposition::ZeroFill,0u}});
        Returned(result,"Direct dependent defaults use detached prior inputs before later reads mutate their sources");
        Require(Vm::Equal(result.value,Value::Vector({-1.0f,-1.0f,0.75f})) && source.floating==99.0f &&
            host.observations==std::vector<std::string>{"opt-in","entry0","entry1","entry2","entry3","entry4","entry5",
                "seq0","seq1","seq2","read2","seq3","read3","seq4","seq5"},
            "Direct entry/sequential/default evaluation order or copy timing changed");
    }
    for(const auto index:{std::uint16_t{1008u},std::uint16_t{1432u}})
        for(const auto disposition:{Vm::ResultDisposition::NoWrite,Vm::ResultDisposition::ZeroFill}) {
        TestHost host;const auto native=declaration(index,{Value::Integer(123)});bool read{};
        auto alias=std::make_shared<Vm::Reference>();alias->zero=Value::Integer(123);
        alias->read=[&]() ->Value {read=true;throw std::runtime_error("Unknown native no-write alias read");};
        host.executionNative=[&](const std::string&,const std::vector<Vm::Evaluation>& arguments,const Vm::Function*,Vm::Execution&) {
            Require(arguments[0].disposition==disposition && arguments[0].zeroFillBytes==0u &&
                arguments[0].Load().kind==Vm::Kind::Nothing && !arguments[0].reference,
                "Unknown direct no-write/zero-byte result changed disposition or became an invented written/declaration-zero payload");
            return Vm::Evaluation{Value::Integer(7),{}};
        };
        const auto result=Vm::Execute(host,native,"Self",{{Value::Integer(999),alias,disposition,0u}});
        Returned(result,"Unaudited native preserves exact absence disposition without reading stale result storage");
        Require(!read && result.value.integer==7,"Unknown direct NoWrite/ZeroFill0 disposition was not preserved");
    }
    {
        TestHost host;const auto native=declaration(1004u,{Value::Integer(0)});
        const auto property=host.Property(native,1,"Out",Value::Integer(0));host.Store("Self",property,{Value::Integer(3)});
        host.argumentSeed=[](std::uint16_t,const std::string&,std::size_t,const Vm::Function*) {return std::optional<Value>{Value::Integer(-1)};};
        const auto result=Vm::Execute(host,native,"Self",{{{},host.Reference({"Self",Vm::Scope::Instance,property.key},property)}});
        Returned(result,"Direct native initialized defaults preserve genuinely written OUT aliases");
        Require(host.Load("Self",property).integer==9 && result.writes==1u,"Direct seed capture detached a genuine OUT alias");
    }
    for(const unsigned mode:{0u,1u,2u,3u,4u,5u}) {
        TestHost host;auto native=declaration(mode==5u?1008u:1454u,{mode==3u?Value::Bool(false):Value::Float(0)});
        bool read{};auto alias=std::make_shared<Vm::Reference>();alias->zero=Value::Float(0);
        alias->read=[&] {read=true;return Value::Float(99);};
        if(mode!=5u) host.argumentSeed=[&](std::uint16_t,const std::string&,std::size_t,const Vm::Function*)->std::optional<Value> {
            if(mode==0u) throw std::runtime_error("Direct seed hook failed before alias read");
            if(mode==1u) return Value::Vector({std::numeric_limits<float>::quiet_NaN(),0.0f,0.0f});
            if(mode==2u) {Value value=Value::Float(1);value.text.reserve(200000u);return value;}
            return mode==3u?Value::Bool(true):Value::Float(-1);
        };
        host.executionNative=[](const std::string&,const std::vector<Vm::Evaluation>&,const Vm::Function*,Vm::Execution&) {return Vm::Evaluation{};};
        auto limits=Vm::Limits{};if(mode==2u) limits.retainedBytes=100000u;
        const Vm::Evaluation input{Value::Float(99),alias,Vm::ResultDisposition::ZeroFill,static_cast<std::uint8_t>(mode==3u?1u:mode==4u?5u:4u)};
        const auto result=Vm::ExecuteTransaction(host,[&](Vm::Execution& execution) {
            execution.CallFunction(Function("EarlierDirectEffect",Join({Native(1000u),Return()})),"Self");
            try {execution.CallFunction(native,"Self",{input});}catch(const std::exception&){}return Value::Integer(1);
        },limits);
        Failed(result,mode==1u?Vm::Status::Invalid:mode==2u?Vm::Status::Budget:Vm::Status::Unsupported,host,
            "Direct seed/type/layout/budget failures poison caught enclosing transactions");
        Require(!read && host.effects==0u && host.rng==12345u && host.nativeIndices==std::vector<std::uint16_t>{1000u},
            "Direct initialized argument failure read alias, reached native body or committed an earlier effect");
    }
    for(const bool mutation:{false,true}) {
        TestHost host;const auto native=declaration(1454u,{Value::Float(0)});Vm::Execution* active{};
        host.argumentSeed=[&](std::uint16_t index,const std::string&,std::size_t,const Vm::Function*)->std::optional<Value> {
            if(index!=1454u) return std::nullopt;
            try {
                if(mutation) active->WriteReference({},Value::Integer(1));
                else active->CallFunction(Function("ForbiddenDirectSeedCallback",Join({Native(1000u),Return()})),"Self");
            }catch(const std::exception&){}return Value::Float(-1);
        };
        const auto result=Vm::ExecuteTransaction(host,[&](Vm::Execution& execution) {
            active=&execution;try {execution.CallFunction(native,"Self",{{Value::Float(7),{}}});}catch(const std::exception&){}return Value{};
        });
        Failed(result,Vm::Status::Invalid,host,"Direct entry seeds remain readonly even when they suppress forbidden callback/write failures");
        Require(host.effects==0u && host.nativeArguments.empty(),"Direct entry seed escaped readonly transaction poison");
    }
    std::cout<<"Direct initialized native argument result contracts: "<<checks-initialChecks<<" checks and "
        <<rejections-initialRejections<<" refusal controls.\n";
}
void IntegerMaximumContracts() {
    const auto initialChecks=checks,initialRejections=rejections;
    const auto configure=[](TestHost& host) {
        auto native=Function("Max",{},"Core.Object");native.nativeIndex=250u;native.flags=0x2401u;
        native.variables={{"Max.A","A",Value::Integer(0),0x80u,1u},{"Max.B","B",Value::Integer(0),0x80u,1u},
            {"Max.ReturnValue","ReturnValue",Value::Integer(0),0x580u,1u}};
        host.finals[{"Fixture",7}]=std::make_shared<Vm::Function>(native);return native;
    };
    const std::array<std::int32_t,9u> values{std::numeric_limits<std::int32_t>::min(),std::numeric_limits<std::int32_t>::min()+1,
        -123,-1,0,1,123,std::numeric_limits<std::int32_t>::max()-1,std::numeric_limits<std::int32_t>::max()};
    for(const int dispatch:{0,1,2}) for(const auto first:values) for(const auto second:values) {
        TestHost host;const auto native=configure(host);
        const auto caller=Function("SignedMaxOriginalComparison",Return(dispatch==1?Call(0x1c,7,{Int(first),Int(second)}):Native(250u,{Int(first),Int(second)})));
        const auto result=dispatch==2?Vm::Execute(host,native,"Self",{{Value::Integer(first),{}},{Value::Integer(second),{}}}):Vm::Execute(host,caller,"Self");
        Returned(result,"Max signed32 boundaries/reversed/equal operands across all entries");
        Require(result.value.kind==Vm::Kind::Int && result.value.integer==(first<second?second:first) &&
            result.writes==0u && host.nativeArguments.empty(),"Max selected wrong signed operand or performed host-side side effects");
    }
    for(const bool declared:{false,true}) for(const auto& input:{Bytes{0x24u,255u},Bytes{0x0bu},Join({{0x44u},Real(255.9f)})}) {
        TestHost host;configure(host);
        const auto caller=Function("MaxNativeInputTypes",Return(declared?Call(0x1c,7,{input,Int(-1)}):Native(250u,{input,Int(-1)})));
        const auto result=Vm::Execute(host,caller,"Self");Returned(result,"Max accepts Byte, absence zero and actual FloatToInt conversion");
        Require(result.value.integer==(input==Bytes{0x0bu}?0:255),"Max confused actual converted/Byte/absent Int input");
    }
    for(const bool declared:{false,true}) {
        TestHost host;configure(host);const auto mutation=Native(1060u,{Ref(0x01,1),Int(100),Int(5)});
        const auto caller=Function("MaxCopiesPriorInput",Return(declared?Call(0x1c,7,{Ref(0x01,1),mutation}):Native(250u,{Ref(0x01,1),mutation})));
        const auto source=host.Property(caller,1,"Number",Value::Integer(0));host.Store("Self",source,{Value::Integer(7)});
        std::size_t seedCalls{};host.argumentSeed=[&](std::uint16_t index,const std::string&,std::size_t,const Vm::Function*)->std::optional<Value> {
            if(index==250u) ++seedCalls;
            return std::nullopt;
        };
        const auto result=Vm::Execute(host,caller,"Self");Returned(result,"Max captures A before B mutates its source");
        Require(result.value.integer==7 && host.Load("Self",source).integer==100 && result.writes==1u && seedCalls==2u &&
            host.observations==std::vector<std::string>{"variable:Self:Number","variable:Self:Number"},
            "Max reread/mutated the earlier input or duplicated hooks at internal Run");
    }
    for(const auto& invalid:{Real(1.0f),String("1"),Bytes{0x27u},Bytes{0x2au},Join({{0x28u}})})
        for(const bool declared:{false,true}) {
        TestHost host;configure(host);const auto caller=Function("MaxConcreteTypesRefuse",Join({Native(1000u),
            Return(declared?Call(0x1c,7,{invalid,Int(2)}):Native(250u,{invalid,Int(2)}))}));
        const auto result=Vm::Execute(host,caller,"Self");Failed(result,Vm::Status::Unsupported,host,"Max requires a real Int/Byte result, not implicit Float/Bool/Object/String conversion");
        Require(host.effects==0u && host.rng==12345u,"Max rejected input committed earlier effects");
    }
    for(const auto& inputs:{std::vector<Bytes>{},std::vector<Bytes>{Int(1)},std::vector<Bytes>{Int(1),Int(2),Int(3)}}) {
        TestHost host;const auto native=configure(host);std::vector<Vm::Evaluation> direct;
        for(std::size_t i=0u;i<inputs.size();++i) direct.push_back({Value::Integer(static_cast<std::int32_t>(i)),{}});
        const auto result=Vm::Execute(host,native,"Self",direct);Failed(result,Vm::Status::Invalid,host,"Max exact required arity is preserved");
    }
    for(const bool budget:{false,true}) {
        TestHost host;configure(host);auto limits=Vm::Limits{};if(budget) limits.writes=0u;
        const auto caller=Function("MaxOriginalMutationRollback",Join({Native(1000u),Native(250u,{Ref(0x01,1),
            Native(1060u,{Ref(0x01,1),Int(99),Int(2)})}),Native(4095u),Return()}));
        const auto source=host.Property(caller,1,"Number",Value::Integer(0));host.Store("Self",source,{Value::Integer(7)});
        const auto result=Vm::Execute(host,caller,"Self",{},limits);
        Failed(result,budget?Vm::Status::Budget:Vm::Status::Unsupported,host,"Max genuine argument mutation shares enclosing failure/write budget rollback");
        Require(host.Load("Self",source).integer==7 && host.effects==0u && host.rng==12345u,"Max left argument mutation committed after failure");
    }
    std::cout<<"Original signed Integer Max250: "<<checks-initialChecks<<" checks and "<<rejections-initialRejections<<" refusal controls.\n";
}
void NativeArgumentSeedContracts() {
    const auto initialChecks=checks,initialRejections=rejections;
    const auto nullResult=[](std::uint8_t bytes) {return Context({0x2a},Native(4095u),bytes);};
    const auto run=[&](const Value& seed,const Bytes& argument,const Value& expected,const std::string& label) {
        TestHost host;
        host.argumentSeed=[&](std::uint16_t index,const std::string& receiver,std::size_t slot,const Vm::Function* declaration)
            ->std::optional<Value> {
            Require(index==1454u && receiver=="Self" && slot==0u && !declaration,label+": seed identity mismatch");
            host.observations.push_back("seed");return seed;
        };
        host.executionNative=[&](const std::string&,const std::vector<Vm::Evaluation>& arguments,
            const Vm::Function*,Vm::Execution&) {
            Require(arguments.size()==1u && !arguments[0].reference,label+": materialized argument retained an alias");
            return Vm::Evaluation{arguments[0].Load(),{}};
        };
        const auto result=Vm::Execute(host,Function("SeededCursor",Return(Native(1454u,{argument}))),"Self");
        Returned(result,label);
        Require(Vm::Equal(result.value,expected),label+": native temporary seed/result differs");
        Require(host.observations==std::vector<std::string>{"seed"} && host.effects==0u,
            label+": null result evaluated its skipped child or captured more than once");
    };
    run(Value::Float(-1.0f),nullResult(0u),Value::Float(-1.0f),"Native cursor NoWrite retains negative-one seed");
    run(Value::Float(-1.0f),nullResult(4u),Value::Float(0.0f),"Native cursor full Float zero fill");
    run(Value::Float(-1.0f),nullResult(3u),Value::Float(-0.5f),"Native cursor partial Float preserves remaining seed bytes");
    run(Value::Integer(0x11223344),nullResult(2u),Value::Integer(0x11220000),"Native partial Int preserves high seed bytes");
    run(Value::Byte(255u),nullResult(1u),Value::Byte(0u),"Native Byte full result zero fill");
    run(Value::Float(-1.0f),{0x0b},Value{},"Native explicit Nothing remains omitted despite captured seed");
    run(Value::Float(-1.0f),Real(3.5f),Value::Float(3.5f),"Written native input replaces seed normally");
    {
        TestHost host;
        auto caller=Function("DeclaredCursor",Return(Call(0x1c,7,{nullResult(0u)})));
        auto declaration=Function("SetDefaultCursor",{},"Extension.Window");
        declaration.flags=0x400u;declaration.nativeIndex=1454u;
        declaration.variables={host.Property(declaration,1,"HotX",Value::Float(0.0f),0x90u,1u,true)};
        host.finals[{caller.source,7}]=std::make_shared<Vm::Function>(declaration);
        host.argumentSeed=[&](std::uint16_t index,const std::string&,std::size_t slot,const Vm::Function* actual)
            ->std::optional<Value> {
            Require(index==1454u && slot==0u && actual && actual->path==declaration.path,
                "Native seed did not receive actual resolved declaration");return Value::Float(-1.0f);
        };
        host.executionNative=[](const std::string&,const std::vector<Vm::Evaluation>& arguments,const Vm::Function*,Vm::Execution&) {
            return Vm::Evaluation{arguments.at(0).Load(),{}};
        };
        const auto result=Vm::Execute(host,caller,"Self");Returned(result,"Actual native seed overrides declared Float zero");
        Require(result.value.kind==Vm::Kind::Float && result.value.floating==-1.0f,
            "Declared parameter zero replaced actual native temporary seed");
    }
    {
        TestHost host;
        const auto caller=Function("AllSeedsBeforeArguments",Return(Context(Ref(0x20,9),Native(1454u,{
            Native(1060u,{Ref(0x01,1),Real(99.0f)}),nullResult(0u)}))));
        const auto current=host.Property(caller,1,"CurrentHotspot",Value::Float(0.0f));
        host.Store("Self",current,{Value::Float(-1.0f)});host.objects[{caller.source,9}]="WindowReceiver";
        host.argumentSeed=[&](std::uint16_t index,const std::string& receiver,std::size_t slot,const Vm::Function*)
            ->std::optional<Value> {
            if(index!=1454u) return std::nullopt;
            Require(receiver=="WindowReceiver" && slot<2u,"Native seeds used argument frame rather than native receiver");
            host.observations.push_back("seed"+std::to_string(slot));return host.Load("Self",current);
        };
        host.executionNative=[&](const std::string& receiver,const std::vector<Vm::Evaluation>& arguments,const Vm::Function*,Vm::Execution&) {
            Require(receiver=="WindowReceiver" && arguments.size()==2u && arguments[0].Load().floating==99.0f &&
                arguments[1].Load().floating==-1.0f,"A seed was recaptured after an earlier argument mutated its source");
            return Vm::Evaluation{arguments[1].Load(),{}};
        };
        const auto result=Vm::Execute(host,caller,"Self");Returned(result,"All detached seeds captured before argument evaluation");
        Require(host.observations==std::vector<std::string>{"seed0","seed1","variable:Self:CurrentHotspot"} &&
            host.Load("Self",current).floating==99.0f,"Seed capture order or caller argument frame changed");
    }
    {
        TestHost host;auto caller=Function("OrderedNativeArguments",Return(Native(1454u,{Native(1001u),Native(1002u)})));
        host.argumentSeed=[&](std::uint16_t index,const std::string&,std::size_t slot,const Vm::Function*)->std::optional<Value> {
            if(index==1454u) {host.observations.push_back("seed"+std::to_string(slot));return Value::Integer(-1);}
            return std::nullopt;
        };
        host.executionNative=[](const std::string&,const std::vector<Vm::Evaluation>& arguments,const Vm::Function*,Vm::Execution&) {
            return Vm::Evaluation{arguments.at(1).Load(),{}};
        };
        const auto result=Vm::Execute(host,caller,"Self");Returned(result,"Native argument evaluation remains left-to-right");
        Require(host.observations==std::vector<std::string>{"seed0","seed1","lhs","rhs"},
            "Native seeds or written arguments were evaluated in the wrong order");
    }
    {
        TestHost host;const auto caller=Function("CursorInputCopiesBeforeLaterMutation",Return(Native(1454u,{
            Ref(0x01,1),Native(1060u,{Ref(0x01,1),Ref(0x20,2)})})));
        const auto texture=host.Property(caller,1,"CursorTexture",Value::Text(Vm::Kind::Object,{}));
        host.Store("Self",texture,{Value::Text(Vm::Kind::Object,"Original.Cursor")});
        host.objects[{caller.source,2}]="Replacement.Cursor";
        host.argumentSeed=[](std::uint16_t index,const std::string&,std::size_t,const Vm::Function*)->std::optional<Value> {
            return index==1454u ? std::optional<Value>{Value::Text(Vm::Kind::Object,{})} : std::nullopt;
        };
        host.executionNative=[](const std::string&,const std::vector<Vm::Evaluation>& arguments,const Vm::Function*,Vm::Execution&) {
            Require(arguments.size()==2u && !arguments[0].reference && !arguments[1].reference &&
                arguments[0].Load().text=="Original.Cursor" && arguments[1].Load().text=="Replacement.Cursor",
                "Cursor ordinary input alias changed during a later argument expression");
            return Vm::Evaluation{arguments[0].Load(),{}};
        };
        const auto result=Vm::Execute(host,caller,"Self");Returned(result,"Cursor input sequential snapshots preserve earlier texture");
        Require(result.value.text=="Original.Cursor" && host.Load("Self",texture).text=="Replacement.Cursor",
            "Cursor snapshot lost actual argument effects or retained a mutable alias");
    }
    for(const bool declared:{false,true}) {
        TestHost host;
        const std::initializer_list<Bytes> inputs{Ref(0x01,1),Native(1060u,{Ref(0x01,1),Real(99.0f),Real(55.0f)})};
        auto call=Native(1431u,inputs);
        if(declared) {
            auto native=Function("SetSize",{},"Extension.Window");native.flags=0x400u;native.nativeIndex=1431u;
            native.variables={{"Width","Width",Value::Float(0.0f),0x80u,1u},{"Height","Height",Value::Float(0.0f),0x80u,1u}};
            host.finals[{"Fixture",7}]=std::make_shared<Vm::Function>(native);call=Call(0x1c,7,inputs);
        }
        const auto caller=Function("SetSizeInputCopiesBeforeLaterMutation",Return(call));
        const auto width=host.Property(caller,1,"Width",Value::Float(0.0f));host.Store("Self",width,{Value::Float(12.0f)});
        host.executionNative=[](const std::string&,const std::vector<Vm::Evaluation>& arguments,const Vm::Function*,Vm::Execution&) {
            Require(arguments.size()==2u && !arguments[0].reference && !arguments[1].reference &&
                arguments[0].Load().floating==12.0f && arguments[1].Load().floating==55.0f,
                "SetSize first ordinary input reloaded its source after the second argument mutation");
            return Vm::Evaluation{arguments[0].Load(),{}};
        };
        const auto result=Vm::Execute(host,caller,"Self");Returned(result,"SetSize copies width before evaluating height");
        Require(result.value.floating==12.0f && host.Load("Self",width).floating==99.0f && result.writes==1u,
            "SetSize input snapshot changed earlier value or suppressed later argument side effects");
    }
    for(const bool declared:{false,true}) {
        TestHost host;
        const auto show=Native(1060u,{Ref(0x01,1),Ref(0x20,2),{0x27}});
        auto expression=Native(1410u,{Ref(0x01,1),show});
        if(declared) {
            auto native=Function("NewChild",{},"Extension.Window");native.nativeIndex=1410u;native.flags=0x401u;
            native.variables={{"NewClass","NewClass",Value::Text(Vm::Kind::Object,{}),0x80u,1u},
                {"bShow","bShow",Value::Bool(false),0x90u,1u}};
            host.finals[{"Fixture",9}]=std::make_shared<Vm::Function>(native);
            expression=Call(0x1c,9,{Ref(0x01,1),show});
        }
        const auto caller=Function("NewChildSequentialClassSnapshot",Return(expression));
        const auto selected=host.Property(caller,1,"ClassChoice",Value::Text(Vm::Kind::Object,{}));
        host.Store("Self",selected,{Value::Text(Vm::Kind::Object,"Original.WindowClass")});
        host.objects[{caller.source,2}]="Replacement.WindowClass";
        host.argumentSeed=[](std::uint16_t index,const std::string&,std::size_t slot,const Vm::Function*)->std::optional<Value> {
            if(index!=1410u) return std::nullopt;
            return slot==0u?Value::Text(Vm::Kind::Object,{}):Value::Bool(true);
        };
        host.executionNative=[](const std::string&,const std::vector<Vm::Evaluation>& arguments,const Vm::Function*,Vm::Execution&) {
            Require(arguments.size()==2u && !arguments[0].reference && !arguments[1].reference &&
                arguments[0].Load().text=="Original.WindowClass" && arguments[1].Load().kind==Vm::Kind::Bool &&
                arguments[1].Load().boolean,"NewChild reloaded class after bShow changed its source");
            return Vm::Evaluation{arguments[0].Load(),{}};
        };
        const auto result=Vm::Execute(host,caller,"Self");Returned(result,"NewChild inputs retain original class and evaluated show");
        Require(result.value.text=="Original.WindowClass" && host.Load("Self",selected).text=="Replacement.WindowClass" &&
            result.writes==1u,"NewChild class snapshot suppressed the real later argument mutation");
    }
    for(const auto& [show,expected]:std::vector<std::pair<Bytes,bool>>{{Bytes{0x0b},true},
        {Context({0x2a},{0x27},0u),true},{Context({0x2a},{0x27},4u),false}}) {
        TestHost host;const auto caller=Function("NewChildShowNativeSeed",Return(Native(1410u,{Ref(0x20,1),show})));
        host.objects[{caller.source,1}]="Original.WindowClass";
        host.argumentSeed=[](std::uint16_t index,const std::string&,std::size_t slot,const Vm::Function*)->std::optional<Value> {
            if(index!=1410u) return std::nullopt;
            return slot==0u?Value::Text(Vm::Kind::Object,{}):Value::Bool(true);
        };
        host.executionNative=[](const std::string&,const std::vector<Vm::Evaluation>& arguments,const Vm::Function*,Vm::Execution&) {
            auto value=arguments.at(1u).Load();
            // The VM preserves EX_Nothing for the declaration-aware native
            // reader; the actual NewChild wrapper applies its true default.
            if(value.kind==Vm::Kind::Nothing) value=Value::Bool(true);
            return Vm::Evaluation{std::move(value),{}};
        };
        const auto result=Vm::Execute(host,caller,"Self");Returned(result,"NewChild Bool seed versus null-result writes");
        Require(result.value.kind==Vm::Kind::Bool && result.value.boolean==expected,
            "NewChild omitted/no-write Show did not retain true or full-zero did not clear it");
        Require(host.nativeArguments.size()==1u && host.nativeArguments.front().at(1u).kind==
            (show==Bytes{0x0b}?Vm::Kind::Nothing:Vm::Kind::Bool),"NewChild lost the explicit optional-argument marker");
    }
    {
        TestHost host;const auto caller=Function("ConsoleCommandCopiedInput",Return(Call(0x1c,8,{
            Native(1007u,{Ref(0x01,1)})})));
        auto command=Function("ConsoleCommand",{},"Engine.PlayerPawn");command.flags=0x400u;command.nativeIndex=0u;
        command.variables={host.Property(command,2,"Command",Value::Text(Vm::Kind::String,{}),0x80u,1u,true)};
        host.finals[{caller.source,8}]=std::make_shared<Vm::Function>(command);
        const auto text=host.Property(caller,1,"CommandText",Value::Text(Vm::Kind::String,{}));
        host.Store("Self",text,{Value::Text(Vm::Kind::String,"get Engine.Engine Root")});
        host.executionNative=[](const std::string&,const std::vector<Vm::Evaluation>& arguments,const Vm::Function*,Vm::Execution&) {
            Require(arguments.size()==1u && !arguments[0].reference,"PlayerPawn ConsoleCommand retained an ordinary FString alias");
            return Vm::Evaluation{arguments[0].Load(),{}};
        };
        const auto result=Vm::Execute(host,caller,"Self");Returned(result,"Native-zero ConsoleCommand snapshot uses actual declaration");
        Require(result.value.text=="get Engine.Engine Root","ConsoleCommand input copy changed its text");
    }
    {
        TestHost host;auto caller=Function("ScriptDoesNotCaptureNativeSeeds",Return(Call(0x1c,1,{Real(2.0f)})));
        auto script=Function("ScriptCallee",Return(Ref(0x00,2)),"ScriptSource");
        script.variables={host.Property(script,2,"Value",Value::Float(0.0f),0x80u,1u,true)};
        host.finals[{caller.source,1}]=std::make_shared<Vm::Function>(script);
        host.argumentSeed=[](std::uint16_t,const std::string&,std::size_t,const Vm::Function*)->std::optional<Value> {
            throw std::runtime_error("Script call incorrectly captured native argument seeds");
        };
        const auto result=Vm::Execute(host,caller,"Self");Returned(result,"Script functions do not ask for native seeds");
        Require(result.value.kind==Vm::Kind::Float && result.value.floating==2.0f,"Script argument binding regressed");
    }
    {
        TestHost host;auto caller=Function("SeededOutAlias",Return(Native(1004u,{Ref(0x01,1)})));
        const auto target=host.Property(caller,1,"Out",Value::Integer(0));host.Store("Self",target,{Value::Integer(3)});
        host.argumentSeed=[](std::uint16_t,const std::string&,std::size_t,const Vm::Function*) {return std::optional<Value>{Value::Integer(-1)};};
        const auto result=Vm::Execute(host,caller,"Self");Returned(result,"Native seeds preserve written OUT aliases");
        Require(host.Load("Self",target).integer==9 && result.writes==1u,"Seed detached an actual OUT binding");
    }
    const auto refuse=[&](const Value& seed,const std::uint8_t bytes,const Vm::Status status,const std::string& label,
        const Vm::Limits& limits=Vm::Limits{}) {
        TestHost host;
        bool captured{};
        host.argumentSeed=[&](std::uint16_t index,const std::string&,std::size_t,const Vm::Function*)->std::optional<Value> {
            if(index!=1454u) return std::nullopt;
            captured=true;
            Value value=seed;value.text.reserve(seed.text.capacity());
            return value;
        };
        const auto caller=Function("RefusedNativeSeed",Join({Native(1000u),Let(Ref(0x01,1),Int(55)),
            Return(Native(1454u,{nullResult(bytes)}))}));
        const auto target=host.Property(caller,1,"Target",Value::Integer(0));host.Store("Self",target,{Value::Integer(3)});
        const auto result=Vm::Execute(host,caller,"Self",{},limits);Failed(result,status,host,label);
        Require(captured && host.effects==0u && host.rng==12345u && host.Load("Self",target).integer==3,
            label+": seed validation/materialization escaped transaction rollback");
        return result;
    };
    refuse(Value::Bool(true),1u,Vm::Status::Unsupported,"Partial packed Bool native seed remains unsupported");
    Value structure;structure.kind=Vm::Kind::Struct;structure.fields["R"]=Value::Byte(255u);
    refuse(structure,1u,Vm::Status::Unsupported,"Partial Struct native seed remains unsupported");
    refuse(Value::Text(Vm::Kind::Object,"Some.Object"),1u,Vm::Status::Unsupported,"Partial Object native seed remains unsupported");
    refuse(Value::Float(-1.0f),5u,Vm::Status::Unsupported,"Native seed zero fill cannot overrun destination");
    refuse(Value::Vector({std::numeric_limits<float>::quiet_NaN(),0.0f,0.0f}),0u,Vm::Status::Invalid,"Nonfinite Vector native seed rejected before argument evaluation");
    auto stringLimit=Vm::Limits{};stringLimit.stringBytes=64u;
    refuse(Value::Text(Vm::Kind::String,std::string(65u,'x')),0u,Vm::Status::Budget,"Native seed text storage is bounded",stringLimit);
    Value retained;retained.kind=Vm::Kind::String;retained.text="x";retained.text.reserve(200000u);
    auto storageLimit=Vm::Limits{};storageLimit.retainedBytes=100000u;
    refuse(retained,0u,Vm::Status::Budget,"Native seed allocated capacity is bounded",storageLimit);
    {
        TestHost host;
        host.argumentSeed=[&](std::uint16_t index,const std::string&,std::size_t slot,const Vm::Function*)->std::optional<Value> {
            if(index!=1454u) return std::nullopt;
            host.observations.push_back("seed"+std::to_string(slot));
            if(slot==1u) throw std::runtime_error("Fixture rejected second native seed");
            return Value::Integer(-1);
        };
        const auto caller=Function("SeedFailureBeforeAnyArgument",Return(Native(1454u,{Native(1000u),Native(1000u)})));
        const auto result=Vm::Execute(host,caller,"Self");Failed(result,Vm::Status::Unsupported,host,"Seed failure before argument work");
        Require(host.observations==std::vector<std::string>{"seed0","seed1"} && host.effects==0u && host.nativeArguments.empty(),
            "Failed seed capture evaluated arguments or invoked native body");
    }
    for(const unsigned operation:{0u,1u,2u,3u}) {
        TestHost host;Vm::Execution* active{};bool attempted{};
        const auto owner=Function("RetainBeforeReadOnlyEntrySeed",Return(Native(1007u,{Ref(0x01,1)})));
        const auto marker=host.Property(owner,1,"Marker",Value::Integer(0));host.Store("Self",marker,{Value::Integer(3)});
        const auto caller=Function("ReadOnlyNativeEntrySeed",Join({Native(1000u),Return(Native(1454u,{Native(1000u)}))}));
        host.argumentSeed=[&](std::uint16_t index,const std::string&,std::size_t,const Vm::Function*)->std::optional<Value> {
            if(index!=1454u) return std::nullopt;
            attempted=true;
            if(operation==3u) throw std::runtime_error("Fixture read-only entry seed failure");
            try {
                if(operation==0u) active->CallFunction(Function("ForbiddenEntrySeedCallback",Join({Native(1000u),Return()})),"Self");
                else if(operation==1u) active->WriteReference(host.Reference({"Self",Vm::Scope::Instance,marker.key},marker),Value::Integer(55));
                else {
                    Require(static_cast<bool>(host.retainedNativeReference),"Read-only entry-seed fixture did not retain guarded reference");
                    host.retainedNativeReference->write(Value::Integer(55));
                }
            } catch(const std::exception&) {}return Value::Float(-1.0f);
        };
        const auto result=Vm::ExecuteTransaction(host,[&](Vm::Execution& execution) {
            active=&execution;execution.CallFunction(owner,"Self");
            try { execution.CallFunction(caller,"Self"); } catch(const std::exception&) {}return Value{};
        });
        Failed(result,operation==3u?Vm::Status::Unsupported:Vm::Status::Invalid,host,
            "Native entry seed is read-only and caught callback/write/hook failures poison transaction");
        Require(attempted && host.effects==0u && host.rng==12345u && host.Load("Self",marker).integer==3 && result.writes==0u &&
            std::count(host.observations.begin(),host.observations.end(),"effect")==1,
            "Native entry seed mutation/failure escaped read-only guard or evaluated the pending argument");
    }
    std::cout<<"Detached native argument result seeds: "<<checks-initialChecks<<" checks and "
        <<rejections-initialRejections<<" refusal controls.\n";
}
void SequentialNativeArgumentSeedContracts() {
    const auto initialChecks=checks,initialRejections=rejections;
    const auto nullResult=[](std::uint8_t bytes) {return Context({0x2a},Native(4095u),bytes);};
    const auto enable=[](TestHost& host) {
        host.hasSequentialSeeds=[](std::uint16_t index,const std::string&,const Vm::Function*) {return index==1438u;};
        host.sequentialArgumentSeed=[](std::uint16_t index,const std::string&,std::size_t slot,
            const Vm::Function*,const std::vector<Value>& prefix)->std::optional<Value> {
            Require(index==1438u && prefix.size()==slot,"Sequential seed prefix is not the already-evaluated inputs");
            if(slot<2u) return Value::Integer(0);
            if(slot<4u) return Value::Float(0.0f);
            return prefix.at(slot-2u);
        };
    };
    for(const auto index:{1416u,1467u}) for(const bool declared:{false,true}) {
        TestHost host;const auto input=Native(1007u,{Ref(0x01,1)});
        auto call=Native(static_cast<std::uint16_t>(index),{input});
        if(declared) {
            auto native=Function("BooleanWindowInput",{},"Extension.Window");native.flags=0x400u;native.nativeIndex=static_cast<std::uint16_t>(index);
            native.variables={{"bValue","bValue",Value::Bool(false),0x80u,1u}};
            host.finals[{"Fixture",7}]=std::make_shared<Vm::Function>(native);call=Call(0x1c,7,{input});
        }
        const auto caller=Function("WindowBooleanInputSnapshot",Return(call));
        const auto source=host.Property(caller,1,"BooleanInput",Value::Bool(false));host.Store("Self",source,{Value::Bool(true)});
        host.executionNative=[](const std::string&,const std::vector<Vm::Evaluation>& arguments,const Vm::Function*,Vm::Execution&) {
            Require(arguments.size()==1u && !arguments[0].reference && arguments[0].Load().kind==Vm::Kind::Bool &&
                arguments[0].Load().boolean,"Audited1416/1467 retained its ordinary Bool input alias");
            return Vm::Evaluation{arguments[0].Load(),{}};
        };
        const auto result=Vm::Execute(host,caller,"Self");Returned(result,"Audited window Bool inputs snapshot their evaluated values");
        Require(result.value.kind==Vm::Kind::Bool && result.value.boolean,"Window Bool input snapshot changed its written value");
    }
    for(const bool declared:{false,true}) {
        for(const auto& [expression,expected]:std::vector<std::pair<Bytes,float>>{
            {nullResult(0u),-1.0f},{nullResult(4u),0.0f},{nullResult(3u),-0.5f},{{0x0b},-1.0f},{Real(2.5f),2.5f}}) {
            TestHost host;enable(host);
            const std::initializer_list<Bytes> inputs{Int(1),Int(2),Real(-1.0f),Real(0.75f),expression,nullResult(0u)};
            auto call=Native(1438u,inputs);
            if(declared) {
                auto native=Function("SetWindowAlignments",{},"Extension.Window");native.flags=0x400u;native.nativeIndex=1438u;
                native.variables={{"H","H",Value::Integer(0),0x80u,1u},{"V","V",Value::Integer(0),0x80u,1u},
                    {"H0","H0",Value::Float(0.0f),0x90u,1u},{"V0","V0",Value::Float(0.0f),0x90u,1u},
                    {"H1","H1",Value::Float(0.0f),0x90u,1u},{"V1","V1",Value::Float(0.0f),0x90u,1u}};
                host.finals[{"Fixture",7}]=std::make_shared<Vm::Function>(native);call=Call(0x1c,7,inputs);
            }
            host.hasSequentialSeeds=[&](std::uint16_t index,const std::string& receiver,const Vm::Function* actual) {
                Require(index==1438u && receiver=="WindowReceiver" && static_cast<bool>(actual)==declared,
                    "Sequential opt-in lost native identity/receiver/declaration");return true;
            };
            host.argumentSeed=[](std::uint16_t,const std::string&,std::size_t slot,const Vm::Function*)->std::optional<Value> {
                // Sequential seeds take precedence over an existing entry seed.
                return slot>=4u?std::optional<Value>{Value::Float(99.0f)}:std::nullopt;
            };
            const bool omitted=expression==Bytes{0x0b};
            host.executionNative=[&](const std::string& receiver,const std::vector<Vm::Evaluation>& arguments,
                const Vm::Function* actual,Vm::Execution&) {
                Require(receiver=="WindowReceiver" && static_cast<bool>(actual)==declared && arguments.size()==6u,
                    "Sequential native invocation identity changed");
                for(const auto& argument:arguments) Require(!argument.reference,"Audited1438 ordinary input retained an alias");
                auto h1=arguments[4].Load();
                Require((h1.kind==Vm::Kind::Nothing)==omitted,"Sequential seed consumed explicit Nothing omission");
                if(omitted) h1=arguments[2].Load(); // The real native reader applies the dependent default.
                return Vm::Evaluation{Value::Vector({Vm::ToFloat(h1),Vm::ToFloat(arguments[5].Load()),0.0f}),{}};
            };
            const auto caller=Function("SequentialDependentDefaults",Return(Context(Ref(0x20,9),call)));
            host.objects[{caller.source,9}]="WindowReceiver";
            const auto result=Vm::Execute(host,caller,"Self");Returned(result,"Sequential dependent Float result defaults");
            Require(Vm::Equal(result.value,Value::Vector({expected,0.75f,0.0f})),
                "Sequential NoWrite/full/partial/written/omitted result differs from dependent native temporary");
        }
    }
    {
        TestHost host;enable(host);
        std::size_t floatingExpression{};
        host.hasSequentialSeeds=[&](std::uint16_t index,const std::string&,const Vm::Function*) {
            if(index==1438u) host.observations.push_back("opt-in");
            return index==1438u;
        };
        host.argumentSeed=[&](std::uint16_t index,const std::string&,std::size_t slot,const Vm::Function*)->std::optional<Value> {
            if(index==1438u) host.observations.push_back("pre"+std::to_string(slot));
            return std::nullopt;
        };
        host.sequentialArgumentSeed=[&](std::uint16_t,const std::string&,std::size_t slot,const Vm::Function*,
            const std::vector<Value>& prefix)->std::optional<Value> {
            host.observations.push_back("seq"+std::to_string(slot));Require(prefix.size()==slot,"Sequential prefix ordering differs");
            if(slot>=4u) return prefix.at(slot-2u);
            return std::nullopt;
        };
        host.executionNative=[&](const std::string&,const std::vector<Vm::Evaluation>& arguments,const Vm::Function*,Vm::Execution&) {
            if(arguments.empty()) {
                host.observations.push_back(floatingExpression==0u?"h0":"v0");
                return Vm::Evaluation{Value::Float(floatingExpression++==0u?-1.0f:0.75f),{}};
            }
            return Vm::Evaluation{arguments.at(4u).Load(),{}};
        };
        const auto result=Vm::Execute(host,Function("SequentialHookOrdering",Return(Native(1438u,
            {Native(1001u),Native(1002u),Native(1008u),Native(1008u),nullResult(0u),nullResult(0u)}))),"Self");
        Returned(result,"Entry seeds precede all args; sequential seeds precede each arg");
        Require(host.observations==std::vector<std::string>{"opt-in","pre0","pre1","pre2","pre3","pre4","pre5",
            "seq0","lhs","seq1","rhs","seq2","h0","seq3","v0","seq4","seq5"},
            "Entry/sequential seed evaluation order changed");
    }
    for(const bool declared:{false,true}) {
        TestHost host;enable(host);
        const std::initializer_list<Bytes> inputs{Int(1),Int(2),Ref(0x01,1),
            Native(1060u,{Ref(0x01,1),Real(99.0f),Real(0.75f)}),nullResult(0u),nullResult(0u)};
        auto call=Native(1438u,inputs);
        if(declared) {
            auto native=Function("SetWindowAlignments",{},"Extension.Window");native.flags=0x400u;native.nativeIndex=1438u;
            host.finals[{"Fixture",7}]=std::make_shared<Vm::Function>(native);call=Call(0x1c,7,inputs);
        }
        const auto caller=Function("SequentialDefaultSnapshotsLvalue",Return(call));
        const auto h0=host.Property(caller,1,"H0",Value::Float(0.0f));host.Store("Self",h0,{Value::Float(-1.0f)});
        host.executionNative=[](const std::string&,const std::vector<Vm::Evaluation>& arguments,const Vm::Function*,Vm::Execution&) {
            Require(!arguments[2].reference && arguments[2].Load().floating==-1.0f,
                "Later1438 argument mutation changed earlier copied ordinary input");
            return Vm::Evaluation{arguments[4].Load(),{}};
        };
        const auto result=Vm::Execute(host,caller,"Self");Returned(result,"Dependent seed retains copied earlier lvalue");
        Require(result.value.floating==-1.0f && host.Load("Self",h0).floating==99.0f && result.writes==1u,
            "Dependent seed used a reloaded lvalue or suppressed actual argument mutation");
    }
    {
        TestHost host;host.hasSequentialSeeds=[](std::uint16_t index,const std::string&,const Vm::Function*) {return index==1004u;};
        host.sequentialArgumentSeed=[](std::uint16_t,const std::string&,std::size_t,const Vm::Function*,
            const std::vector<Value>& prefix)->std::optional<Value> {
            Require(prefix.empty(),"First OUT seed had a nonempty prefix");return Value::Integer(-1);
        };
        const auto caller=Function("SequentialSeedsPreserveOut",Return(Native(1004u,{Ref(0x01,1)})));
        const auto target=host.Property(caller,1,"Out",Value::Integer(0));host.Store("Self",target,{Value::Integer(3)});
        const auto result=Vm::Execute(host,caller,"Self");Returned(result,"Sequential prefix does not detach OUT binding");
        Require(host.Load("Self",target).integer==9 && result.writes==1u,"Sequential capture detached unknown native OUT argument");
    }
    {
        TestHost host;const auto caller=Function("NoSequentialHookForScript",Return(Call(0x1c,7,{Real(2.0f)})));
        auto script=Function("Callee",Return(Ref(0x00,2)),"ScriptSource");
        script.variables={host.Property(script,2,"Input",Value::Float(0.0f),0x80u,1u,true)};
        host.finals[{caller.source,7}]=std::make_shared<Vm::Function>(script);
        host.hasSequentialSeeds=[](std::uint16_t,const std::string&,const Vm::Function*)->bool {
            throw std::runtime_error("Script called sequential opt-in");
        };
        const auto result=Vm::Execute(host,caller,"Self");Returned(result,"Script calls do not consult sequential native hooks");
        Require(result.value.floating==2.0f,"Script argument binding changed with sequential API");
    }
    {
        TestHost host;host.sequentialArgumentSeed=[](std::uint16_t,const std::string&,std::size_t,const Vm::Function*,
            const std::vector<Value>&)->std::optional<Value> {throw std::runtime_error("Non-opted-in native called sequential hook");};
        const auto caller=Function("NoSequentialHookWithoutOptIn",Return(Native(1003u,{Ref(0x01,1)})));
        const auto target=host.Property(caller,1,"Input",Value::Integer(0));host.Store("Self",target,{Value::Integer(7)});
        const auto result=Vm::Execute(host,caller,"Self");
        Returned(result,"Ordinary natives do not call sequential seed hook");
        Require(result.value.integer==7 && host.referenceReads==2u,"Ordinary native gained an extra prefix-snapshot read");
    }
    const auto refuse=[&](const Value& seed,const Vm::Status status,const std::string& label,
        const Vm::Limits& limits=Vm::Limits{},const std::uint8_t bytes=0u) {
        TestHost host;enable(host);bool reached{};
        host.sequentialArgumentSeed=[&](std::uint16_t,const std::string&,std::size_t slot,const Vm::Function*,
            const std::vector<Value>&)->std::optional<Value> {
            if(slot!=4u) return std::nullopt;
            reached=true;
            auto value=seed;value.text.reserve(seed.text.capacity());return value;
        };
        const auto caller=Function("RefusedSequentialSeed",Return(Native(1438u,
            {Native(1000u),Int(2),Native(1060u,{Ref(0x01,1),Int(55)}),Real(0.75f),nullResult(bytes),Native(1000u)})));
        const auto target=host.Property(caller,1,"Target",Value::Integer(0));host.Store("Self",target,{Value::Integer(3)});
        const auto result=Vm::ExecuteTransaction(host,[&](Vm::Execution& execution) {
            try { execution.CallFunction(caller,"Self"); } catch(const std::exception&) {}
            return Value::Integer(123); // A caught hook/budget failure cannot commit the prefix.
        },limits);
        Failed(result,status,host,label);
        Require(reached && host.effects==0u && host.rng==12345u && host.Load("Self",target).integer==3 && result.writes==1u,
            label+": sequential seed failure did not roll back evaluated prefix");
        Require(std::count(host.observations.begin(),host.observations.end(),"effect")==1,
            label+": failing seed evaluated its own/later argument");
    };
    refuse(Value::Vector({std::numeric_limits<float>::quiet_NaN(),0.0f,0.0f}),Vm::Status::Invalid,"Nonfinite Vector sequential seed poisons shared execution");
    refuse(Value::Float(-1.0f),Vm::Status::Unsupported,"Sequential seed zero fill cannot exceed destination",Vm::Limits{},5u);
    refuse(Value::Bool(true),Vm::Status::Unsupported,"Sequential partial Bool seed remains unsupported",Vm::Limits{},1u);
    auto stringLimit=Vm::Limits{};stringLimit.stringBytes=64u;
    refuse(Value::Text(Vm::Kind::String,std::string(65u,'x')),Vm::Status::Budget,"Sequential seed text limit",stringLimit);
    Value reserved=Value::Text(Vm::Kind::String,"x");reserved.text.reserve(200000u);
    auto storageLimit=Vm::Limits{};storageLimit.retainedBytes=100000u;
    refuse(reserved,Vm::Status::Budget,"Sequential seed allocated capacity limit",storageLimit);
    {
        TestHost host;enable(host);
        host.sequentialArgumentSeed=[](std::uint16_t,const std::string&,std::size_t slot,const Vm::Function*,
            const std::vector<Value>&)->std::optional<Value> {
            if(slot==1u) throw std::runtime_error("Fixture sequential seed failure");
            return std::nullopt;
        };
        const auto caller=Function("ThrowingSequentialSeed",Return(Native(1438u,{Native(1000u),Native(1000u)})));
        const auto result=Vm::ExecuteTransaction(host,[&](Vm::Execution& execution) {
            try { execution.CallFunction(caller,"Self"); } catch(const std::exception&) {}return Value{};
        });
        Failed(result,Vm::Status::Unsupported,host,"Sequential hook exception poisons shared execution");
        Require(host.effects==0u && std::count(host.observations.begin(),host.observations.end(),"effect")==1,
            "Throwing sequential hook did not preserve rollback or evaluated later argument");
    }
    for(const bool optInHook:{false,true}) for(const bool write:{false,true}) {
        TestHost host;enable(host);Vm::Execution* activeExecution{};bool attempted{};
        const auto caller=Function("ReadOnlySequentialHooks",Return(Native(1438u,{Native(1000u)})));
        const auto target=host.Property(caller,1,"Target",Value::Integer(0));host.Store("Self",target,{Value::Integer(3)});
        const auto reference=host.Reference({"Self",Vm::Scope::Instance,target.key},target);
        const auto callback=Function("ForbiddenCallback",Join({Native(1000u),Return()}));
        const auto attempt=[&] {
            attempted=true;
            try {
                if(write) activeExecution->WriteReference(reference,Value::Integer(55));
                else activeExecution->CallFunction(callback,"Self");
            } catch(const std::exception&) {}
        };
        host.hasSequentialSeeds=[&](std::uint16_t index,const std::string&,const Vm::Function*) {
            if(index==1438u && optInHook) attempt();
            return index==1438u;
        };
        host.sequentialArgumentSeed=[&](std::uint16_t,const std::string&,std::size_t,const Vm::Function*,
            const std::vector<Value>&)->std::optional<Value> {
            if(!optInHook) attempt();
            return Value::Integer(0);
        };
        const auto result=Vm::ExecuteTransaction(host,[&](Vm::Execution& execution) {
            activeExecution=&execution;try { execution.CallFunction(caller,"Self"); } catch(const std::exception&) {}return Value{};
        });
        Failed(result,Vm::Status::Invalid,host,"Read-only sequential hook refuses VM callbacks even when caught");
        Require(attempted && host.effects==0u && host.Load("Self",target).integer==3 && result.writes==0u,
            "Read-only sequential hook executed callback/mutation or escaped shared poison");
    }
    {
        TestHost host;enable(host);bool attempted{};
        const auto first=Function("RetainGuardBeforeSequentialHook",Return(Native(1007u,{Ref(0x01,1)})));
        const auto target=host.Property(first,1,"Target",Value::Integer(0));host.Store("Self",target,{Value::Integer(3)});
        const auto second=Function("ReadOnlyHookCannotWriteGuard",Return(Native(1438u,{Native(1000u)})));
        host.sequentialArgumentSeed=[&](std::uint16_t,const std::string&,std::size_t,const Vm::Function*,
            const std::vector<Value>&)->std::optional<Value> {
            Require(static_cast<bool>(host.retainedNativeReference),"Read-only hook fixture did not capture guarded reference");
            attempted=true;
            try { host.retainedNativeReference->write(Value::Integer(55)); } catch(const std::exception&) {}
            return Value::Integer(0);
        };
        const auto result=Vm::ExecuteTransaction(host,[&](Vm::Execution& execution) {
            execution.CallFunction(first,"Self");
            try { execution.CallFunction(second,"Self"); } catch(const std::exception&) {}return Value{};
        });
        Failed(result,Vm::Status::Invalid,host,"Read-only sequential hook refuses guarded OUT writes");
        Require(attempted && host.effects==0u && host.Load("Self",target).integer==3 && result.writes==0u,
            "Read-only sequential hook mutated guarded OUT reference or escaped shared poison");
    }
    {
        TestHost host;enable(host);auto limits=Vm::Limits{};limits.localElements=3u;
        const auto caller=Function("SequentialPrefixNodeLimit",Return(Native(1438u,{Native(1000u),Int(2),Int(3),Int(4),Native(1000u)})));
        const auto result=Vm::ExecuteTransaction(host,[&](Vm::Execution& execution) {
            try { execution.CallFunction(caller,"Self"); } catch(const std::exception&) {}return Value{};
        },limits);
        Failed(result,Vm::Status::Budget,host,"Detached sequential prefix aggregate node limit");
        Require(host.effects==0u && std::count(host.observations.begin(),host.observations.end(),"effect")==1,
            "Prefix aggregate node refusal evaluated later argument or failed rollback");
    }
    std::cout<<"Sequential native argument result seeds: "<<checks-initialChecks<<" checks and "
        <<rejections-initialRejections<<" refusal controls.\n";
}
void CollisionSetterInputContracts() {
    const auto initialChecks=checks,initialRejections=rejections;
    const auto configure=[](TestHost& host,std::uint16_t index,const std::array<Vm::Property,3u>& flags) {
        auto native=Function(index==262u?"SetCollision":"SetCollisionSize",{},"Engine.Actor");native.flags=0x401u;native.nativeIndex=index;
        const std::array<const char*,3u> names=index==262u?
            std::array<const char*,3u>{"NewColActors","NewBlockActors","NewBlockPlayers"}:
            std::array<const char*,3u>{"NewRadius","NewHeight","ReturnValue"};
        for(std::size_t slot=0u;slot<3u;++slot) native.variables.push_back(host.Property(native,
            static_cast<std::int32_t>(slot+1u),names[slot],index==262u||slot==2u?Value::Bool(false):Value::Float(0.0f),
            index==262u?0x90u:slot==2u?0x580u:0x80u,1u,true));
        host.hasSequentialSeeds=[index](std::uint16_t actual,const std::string&,const Vm::Function*) {return actual==index;};
        host.materializeOmission=[index](std::uint16_t actual,const std::string&,std::size_t slot,const Vm::Function*) {
            return actual==index&&slot<(index==262u?3u:2u);
        };
        host.sequentialArgumentSeed=[&host,index,flags](std::uint16_t actual,const std::string& receiver,std::size_t slot,
            const Vm::Function*,const std::vector<Value>& prefix)->std::optional<Value> {
            Require(actual==index&&prefix.size()==slot&&slot<(index==262u?3u:2u),"Collision setter seed order/schema changed");
            return index==262u?host.Load(receiver,flags[slot]):Value::Float(0.0f);
        };
        return native;
    };
    const auto nullResult=[] {return Context(Bytes{0x2a},Int(99),0u);};
    for(const std::uint16_t index:{262u,283u})for(const bool reflected:{false,true})for(const bool failLater:{false,true}) {
        TestHost host;auto caller=Function("CollisionSetterCopiesEarlierAlias",{});
        std::array<Vm::Property,3u> flags;
        for(std::size_t slot=0u;slot<flags.size();++slot) {
            flags[slot]=host.Property(caller,static_cast<std::int32_t>(slot+1u),"Flag"+std::to_string(slot),Value::Bool(false));
            host.Store("Self",flags[slot],{Value::Bool(slot!=1u)});
        }
        const auto source=index==262u?flags[0u]:host.Property(caller,4,"RadiusSource",Value::Float(0.0f));
        if(index==283u) host.Store("Self",source,{Value::Float(17.5f)});
        const auto native=configure(host,index,flags);
        host.finals[{caller.source,7}]=std::make_shared<Vm::Function>(native);
        const auto input=Ref(0x01,index==262u?1:4);
        const auto changed=index==262u?Bytes{0x28u}:Real(99.0f);
        const auto second=index==262u?Bytes{0x27u}:Real(23.0f);
        const auto mutation=Native(1060u,{input,changed,second});
        const auto call=index==262u?
            (reflected?Call(0x1c,7,{input,mutation,Bytes{0x0bu}}):Native(index,{input,mutation,Bytes{0x0bu}})):
            (reflected?Call(0x1c,7,{input,mutation}):Native(index,{input,mutation}));
        bool called{};host.executionNative=[&](const std::string&,const std::vector<Vm::Evaluation>& arguments,
            const Vm::Function* declaration,Vm::Execution&) {
            called=true;Require(static_cast<bool>(declaration)==reflected,"Collision setter reflected/raw identity changed");
            Require(arguments.size()==(index==262u?3u:2u),"Collision setter copied argument count changed");
            for(const auto& argument:arguments) Require(!argument.reference,"Collision setter retained an ordinary input alias");
            Require(Vm::Equal(arguments[0u].Load(),index==262u?Value::Bool(true):Value::Float(17.5f))&&
                Vm::Equal(arguments[1u].Load(),index==262u?Value::Bool(true):Value::Float(23.0f)),
                "Later setter argument retroactively replaced an earlier consumed input");
            if(index==262u) Require(arguments[2u].Load().boolean,"SetCollision omission lost the per-expression current flag seed");
            return Vm::Evaluation{index==283u?Value::Bool(true):Value{}, {}};
        };
        caller.bytecode=failLater?Join({call,Native(4095u),Return()}):Return(call);
        const auto result=Vm::Execute(host,caller,"Self");
        if(failLater) Failed(result,Vm::Status::Unsupported,host,"Later failure rolls back collision-setter argument effects");
        else Returned(result,"Collision setter raw/reflected inputs copy at each Step");
        Require(called&&result.writes==1u&&Vm::Equal(host.Load("Self",source),failLater?
            (index==262u?Value::Bool(true):Value::Float(17.5f)):(index==262u?Value::Bool(false):Value::Float(99.0f))),
            "Collision setter lost real argument mutation or root rollback");
    }
    for(const bool reflected:{false,true}) {
        TestHost host;auto caller=Function("SetCollisionSequentialOptionalDefaults",{});
        std::array<Vm::Property,3u> flags;
        for(std::size_t slot=0u;slot<flags.size();++slot) {
            flags[slot]=host.Property(caller,static_cast<std::int32_t>(slot+1u),"Flag"+std::to_string(slot),Value::Bool(false));
            host.Store("Self",flags[slot],{Value::Bool(false)});
        }
        const auto native=configure(host,262u,flags);host.finals[{caller.source,7}]=std::make_shared<Vm::Function>(native);
        const auto first=Native(1060u,{Ref(0x01,2),Bytes{0x27u},Bytes{0x28u}});
        const auto third=Native(1060u,{Ref(0x01,2),Bytes{0x28u},Bytes{0x27u}});
        caller.bytecode=Return(reflected?Call(0x1c,7,{first,nullResult(),third}):Native(262u,{first,nullResult(),third}));
        host.executionNative=[&](const std::string&,const std::vector<Vm::Evaluation>& arguments,const Vm::Function*,Vm::Execution&) {
            Require(arguments.size()==3u&&!arguments[0u].reference&&!arguments[1u].reference&&!arguments[2u].reference&&
                !arguments[0u].Load().boolean&&arguments[1u].Load().boolean&&arguments[2u].Load().boolean&&
                !host.Load("Self",flags[1u]).boolean,"SetCollision default was captured before prior expression or reloaded after a later expression");
            return Vm::Evaluation{};
        };
        const auto result=Vm::Execute(host,caller,"Self");Returned(result,"SetCollision optional Bool seed occurs immediately before each expression");
        Require(result.writes==2u,"SetCollision optional-default fixture did not execute both real input mutations");
    }
    for(const std::uint16_t index:{262u,283u})for(const bool failLater:{false,true}) {
        TestHost host;const auto fixture=Function("DirectCollisionInputFixture",{});
        std::array<Vm::Property,3u> flags;
        for(std::size_t slot=0u;slot<flags.size();++slot) {
            flags[slot]=host.Property(fixture,static_cast<std::int32_t>(slot+1u),"Flag"+std::to_string(slot),Value::Bool(false));
            host.Store("Self",flags[slot],{Value::Bool(slot!=1u)});
        }
        const auto native=configure(host,index,flags);
        auto first=std::make_shared<Value>(index==262u?Value::Bool(true):Value::Float(17.5f));
        const auto old=*first;std::size_t firstReads{},secondReads{};
        auto firstAlias=std::make_shared<Vm::Reference>();firstAlias->zero=index==262u?Value::Bool(false):Value::Float(0.0f);
        firstAlias->read=[&] {++firstReads;return *first;};firstAlias->write=[first](const Value& value) {*first=value;};
        auto secondAlias=std::make_shared<Vm::Reference>();secondAlias->zero=firstAlias->zero;
        secondAlias->read=[&] {++secondReads;*first=index==262u?Value::Bool(false):Value::Float(99.0f);
            return index==262u?Value::Bool(true):Value::Float(23.0f);};
        bool called{};host.executionNative=[&](const std::string&,const std::vector<Vm::Evaluation>& arguments,const Vm::Function*,Vm::Execution&) {
            called=true;Require(!arguments[0u].reference&&!arguments[1u].reference&&Vm::Equal(arguments[0u].Load(),old)&&
                Vm::Equal(arguments[1u].Load(),index==262u?Value::Bool(true):Value::Float(23.0f)),
                "Direct Execution setter call reloaded an earlier alias after consuming a later input");
            return Vm::Evaluation{index==283u?Value::Bool(true):Value{}, {}};
        };
        const auto result=Vm::ExecuteTransaction(host,[&](Vm::Execution& execution) {
            // Establish root-owned undo before the deliberately side-effecting
            // diagnostic read; direct incoming aliases must still read once.
            execution.WriteReference(firstAlias,old);firstReads=0u;
            auto returned=execution.CallFunction(native,"Self",{{{},firstAlias},{{},secondAlias}});
            if(failLater) throw std::runtime_error("Later direct collision coordinator refusal");
            return returned;
        });
        if(failLater) Failed(result,Vm::Status::Unsupported,host,"Direct setter alias snapshots survive a later root failure");
        else Returned(result,"Direct Execution collision setter consumes each incoming alias once");
        Require(called&&firstReads==1u&&secondReads==1u&&result.writes==1u&&Vm::Equal(*first,failLater?old:
            (index==262u?Value::Bool(false):Value::Float(99.0f))),
            "Direct setter snapshot repeated an alias read or lost external-alias rollback");
    }
    for(unsigned supplied=0u;supplied<3u;++supplied) {
        TestHost host;std::array<Vm::Property,3u> flags{};const auto native=configure(host,283u,flags);
        bool called{};host.executionNative=[&](const std::string&,const std::vector<Vm::Evaluation>& arguments,const Vm::Function*,Vm::Execution&) {
            called=true;Require(arguments.size()==2u&&!arguments[0u].reference&&!arguments[1u].reference&&
                arguments[0u].Load().kind==Vm::Kind::Float&&arguments[1u].Load().kind==Vm::Kind::Float&&
                arguments[0u].Load().floating==0.0f&&arguments[1u].Load().floating==0.0f,
                "SetCollisionSize omitted result buffers did not retain local Float0 seeds");
            return Vm::Evaluation{Value::Bool(true),{}};
        };
        std::vector<Vm::Evaluation> arguments(supplied);
        const auto result=Vm::Execute(host,native,"Self",arguments);
        if(supplied<2u) {
            Failed(result,Vm::Status::Invalid,host,"SetCollisionSize Float0 temporary seed does not make required parameters optional");
            Require(!called,"Missing required collision size parameter entered native body");
        }else {
            Returned(result,"SetCollisionSize supplied NoWrite operands consume their initialized Float0 buffers");
            Require(called&&result.value.kind==Vm::Kind::Bool&&result.value.boolean,"SetCollisionSize initialized required inputs lost wrapper success");
        }
    }
    std::cout<<"Collision setter262/283 copied inputs and per-expression defaults: "<<checks-initialChecks<<" checks and "
        <<rejections-initialRejections<<" rejection controls passed.\n";
}
void NativeResultDispositionContracts() {
    const auto initialChecks=checks,initialRejections=rejections;
    const auto declaration=[] {
        auto function=Function("ResultNative",{},"NativeFixture");function.nativeIndex=1008u;function.flags=0x400u;
        function.variables={{"NativeFixture.ResultNative.ReturnValue","ReturnValue",Value::Integer(0),0x400u,1u}};
        return function;
    };
    const auto call=[&](TestHost& host,const unsigned dispatch) {
        if(dispatch==0u) return Native(1008u);
        const auto native=std::make_shared<Vm::Function>(declaration());
        if(dispatch==1u) host.finals[{"Fixture",7}]=native;
        else {
            host.names[{"Fixture",7}]="ResultNative";
            host.named[{"Self",dispatch==2u?Vm::CallKind::Virtual:Vm::CallKind::Global,"ResultNative"}]=native;
        }
        return Call(dispatch==1u?0x1cu:dispatch==2u?0x1bu:0x38u,7);
    };
    // Empty native return, explicit NoWrite with hostile payload, unwritten
    // alias, zero-size clear, full clear and genuine written scalar. Each
    // expression must preserve its disposition until the actual destination.
    for(unsigned dispatch=0u;dispatch<4u;++dispatch) for(unsigned mode=0u;mode<6u;++mode)
        for(unsigned destination=0u;destination<4u;++destination) {
        TestHost host;bool readUnwritten{};std::size_t nativeCalls{};
        auto stale=std::make_shared<Vm::Reference>();stale->zero=Value::Integer(0);
        stale->read=[&]() -> Value {readUnwritten=true;throw std::runtime_error("Read unwritten native result alias");};
        host.executionNative=[&](const std::string&,const std::vector<Vm::Evaluation>& arguments,
            const Vm::Function*,Vm::Execution&) {
            if(!arguments.empty()) {
                Require(arguments.size()==1u && !arguments[0].reference,"Native argument destination did not materialize a detached temporary");
                return Vm::Evaluation{arguments[0].Load(),{}};
            }
            ++nativeCalls;
            if(mode==0u) return Vm::Evaluation{};
            if(mode==1u) return Vm::Evaluation{Value::Integer(999),{},Vm::ResultDisposition::NoWrite};
            if(mode==2u) return Vm::Evaluation{Value::Integer(999),stale,Vm::ResultDisposition::NoWrite};
            if(mode==3u || mode==4u) return Vm::Evaluation{Value::Integer(999),stale,
                Vm::ResultDisposition::ZeroFill,static_cast<std::uint8_t>(mode==4u?4u:0u)};
            return Vm::Evaluation{Value::Integer(43),{}};
        };
        const auto expression=call(host,dispatch);
        auto caller=Function("NativeResultDestination",{});
        if(destination==0u) {
            const auto target=host.Property(caller,1,"Target",Value::Integer(0));host.Store("Self",target,{Value::Integer(41)});
            caller.bytecode=Join({Let(Ref(0x01u,1),expression),Return(Ref(0x01u,1))});
        } else if(destination==1u) {
            caller.variables={host.Property(caller,2,"ReturnValue",Value::Integer(0),0x400u,1u,true)};
            caller.bytecode=Join({Let(Ref(0x00u,2),Int(49)),Return(expression)});
        } else if(destination==2u) caller.bytecode=Return(expression);
        else {
            host.argumentSeed=[](std::uint16_t index,const std::string&,std::size_t slot,const Vm::Function*) {
                return index==1008u && slot==0u?std::optional<Value>{Value::Integer(7)}:std::nullopt;
            };
            caller.bytecode=Return(Native(1008u,{expression}));
        }
        const auto result=Vm::Execute(host,caller,"Self");Returned(result,"Native raw/reflected destination-sensitive result disposition");
        Require(!readUnwritten && nativeCalls==1u,"Native result read unwritten storage or executed more than once");
        const bool writes=mode==4u || mode==5u;
        if(destination==2u) Require(writes && mode==5u ? result.value.kind==Vm::Kind::Int && result.value.integer==43 :
            result.value.kind==Vm::Kind::Nothing,"Destination-free native result manufactured typed/stale storage");
        else {
            const auto expected=mode==5u?43:mode==4u?0:destination==0u?41:destination==1u?49:7;
            Require(result.value.kind==Vm::Kind::Int && result.value.integer==expected,
                "Native NoWrite/full clear/written result did not use its real destination");
        }
        Require(result.writes==(destination==0u?static_cast<unsigned>(writes):destination==1u?1u:0u),
            "Native return disposition invented/suppressed a destination write");
    }
    for(const bool coordinated:{false,true}) for(const bool event:{false,true}) {
        TestHost host;const auto native=declaration();bool read{};
        auto stale=std::make_shared<Vm::Reference>();stale->zero=Value::Integer(0);
        stale->read=[&] {read=true;return Value::Integer(999);};
        host.executionNative=[&](const std::string&,const std::vector<Vm::Evaluation>&,const Vm::Function*,Vm::Execution&) {
            return Vm::Evaluation{Value::Integer(999),stale,Vm::ResultDisposition::NoWrite};
        };
        host.events[{"Self","ResultNative",false}]=std::make_shared<Vm::Function>(native);
        const auto result=coordinated?Vm::ExecuteTransaction(host,[&](Vm::Execution& execution) {
            return event?execution.CallEvent("Self","ResultNative",false):execution.CallFunction(native,"Self");
        }):Vm::Execute(host,native,"Self");
        Returned(result,"Direct Value API exposes unwritten native result as absence");
        Require(!read && result.value.kind==Vm::Kind::Nothing && result.writes==0u,
            "Direct native entry inferred a scripted zero or read an unwritten alias");
    }
    for(const bool reflected:{false,true}) {
        TestHost host;const auto expression=call(host,reflected?1u:0u);
        host.executionNative=[](const std::string&,const std::vector<Vm::Evaluation>&,const Vm::Function*,Vm::Execution&) {
            return Vm::Evaluation{Value::Integer(999),{},Vm::ResultDisposition::NoWrite};
        };
        auto script=Function("ScriptedReturnZero",Return(expression));
        script.variables={host.Property(script,2,"ReturnValue",Value::Integer(0),0x400u,1u,true)};
        host.finals[{"Fixture",8}]=std::make_shared<Vm::Function>(script);
        auto caller=Function("ScriptedReturnIsWritten",Join({Let(Ref(0x01u,1),Call(0x1cu,8)),Return(Ref(0x01u,1))}));
        const auto target=host.Property(caller,1,"Target",Value::Integer(0));host.Store("Self",target,{Value::Integer(41)});
        const auto result=Vm::Execute(host,caller,"Self");Returned(result,"Scripted initialized return local remains a genuine written zero");
        Require(result.value.kind==Vm::Kind::Int && result.value.integer==0 && result.writes==1u,
            "Native NoWrite incorrectly escaped a SCRIPTED initialized return local");
    }
    for(const bool reflected:{false,true}) {
        TestHost host;const auto expression=call(host,reflected?1u:0u);
        host.executionNative=[](const std::string&,const std::vector<Vm::Evaluation>&,const Vm::Function*,Vm::Execution&) {
            return Vm::Evaluation{{},{},Vm::ResultDisposition::ZeroFill,1u};
        };
        const auto caller=Function("NativePartialReturnRefused",Join({Native(1000u),Let(Ref(0x01u,1),expression),Return()}));
        const auto target=host.Property(caller,1,"Target",Value::Integer(0));host.Store("Self",target,{Value::Integer(41)});
        const auto result=Vm::Execute(host,caller,"Self");Failed(result,Vm::Status::Unsupported,host,"Native partial return clear cannot silently become a full scalar zero");
        Require(host.effects==0u && host.Load("Self",target).integer==41,"Native partial-return refusal retained prefix effects/storage");
    }
    std::cout<<"Native return dispositions: "<<checks-initialChecks<<" checks and "<<rejections-initialRejections<<" refusal controls.\n";
}
void MaterializedNativeOmissionContracts() {
    const auto initialChecks=checks,initialRejections=rejections;
    for(unsigned dispatch=0u;dispatch<3u;++dispatch) for(const bool radiusMutation:{false,true}) {
        TestHost host;auto native=Function("InitializedSoundInputs",{},"NativeFixture");native.flags=0x400u;native.nativeIndex=264u;
        const std::array<Value,6> zeros{Value::Text(Vm::Kind::Object,{}),Value::Byte(0u),Value::Float(0.0f),
            Value::Bool(false),Value::Float(0.0f),Value::Float(0.0f)};
        for(std::size_t slot=0u;slot<zeros.size();++slot)
            native.variables.push_back({"Input"+std::to_string(slot),"Input"+std::to_string(slot),zeros[slot],slot==0u?0x80u:0x90u,1u});
        host.finals[{"Fixture",7}]=std::make_shared<Vm::Function>(native);
        auto caller=Function("StepTimeOmissions",{});
        const auto volume=host.Property(caller,1,"Volume",Value::Float(0.0f));host.Store("Self",volume,{Value::Float(1.0f)});
        const auto radius=host.Property(caller,2,"Radius",Value::Float(0.0f));host.Store("Self",radius,{Value::Float(32.0f)});
        host.objects[{"Fixture",9}]="Fixture.Sound";
        std::size_t capabilities{},seeds{};std::vector<std::string> order;
        host.hasSequentialSeeds=[](std::uint16_t index,const std::string&,const Vm::Function*) {return index==264u;};
        host.materializeOmission=[&](std::uint16_t index,const std::string& receiver,std::size_t slot,const Vm::Function* declaration) {
            if(index!=264u) return false;
            Require(receiver=="Self" && slot<6u && static_cast<bool>(declaration)==(dispatch!=0u),
                "Omission capability lost native receiver/declaration/slot");
            Require(seeds==0u && host.Load("Self",volume).floating==1.0f && host.Load("Self",radius).floating==32.0f,
                "Omission capabilities were not all captured before supplied argument reads/effects");
            ++capabilities;order.push_back("cap"+std::to_string(slot));return true;
        };
        host.sequentialArgumentSeed=[&](std::uint16_t index,const std::string&,std::size_t slot,const Vm::Function*,
            const std::vector<Value>& prefix)->std::optional<Value> {
            Require(index==264u && capabilities==6u && prefix.size()==slot,"Sequential omission seed ordering/prefix differs");
            ++seeds;order.push_back("seed"+std::to_string(slot));
            if(slot==0u) return zeros[0];
            if(slot==1u) return zeros[1];
            if(slot==2u) return host.Load("Self",volume);
            if(slot==3u) {
                Require(prefix[2].kind==Vm::Kind::Float && prefix[2].floating==2.0f,
                    "Materialized omitted volume was not snapshotted into later prefix");
                return zeros[3];
            }
            if(slot==4u) return host.Load("Self",radius);
            return Value::Float(1.0f);
        };
        host.executionNative=[&](const std::string&,const std::vector<Vm::Evaluation>& inputs,const Vm::Function*,Vm::Execution&) {
            Require(inputs.size()==6u && capabilities==6u && seeds==6u,"Initialized native input count/hook repetition differs");
            for(const auto& input:inputs) Require(!input.reference && input.disposition==Vm::ResultDisposition::Written &&
                input.Load().kind!=Vm::Kind::Nothing,"Initialized native retained omission marker or ordinary input alias");
            Require(inputs[0].Load().text=="Fixture.Sound" && inputs[2].Load().floating==2.0f &&
                inputs[4].Load().floating==(radiusMutation?64.0f:32.0f) && inputs[5].Load().floating==1.0f,
                "Omitted defaults used native-entry or final receiver state instead of their own argument Step");
            return Vm::Evaluation{Value::Bool(true),{}};
        };
        Vm::Result result;
        if(dispatch<2u) {
            const std::initializer_list<Bytes> inputs{
                Native(1060u,{Ref(0x01u,1),Real(2.0f),Ref(0x20u,9)}),{0x0bu},{0x0bu},
                radiusMutation?Native(1060u,{Ref(0x01u,2),Real(64.0f),Bytes{0x28u}}):Bytes{0x0bu},
                radiusMutation?Bytes{0x0bu}:Native(1060u,{Ref(0x01u,1),Real(9.0f),Real(32.0f)}),
                radiusMutation?Native(1060u,{Ref(0x01u,2),Real(128.0f),Real(1.0f)}):Bytes{0x0bu}};
            caller.bytecode=Return(dispatch==0u?Native(264u,inputs):Call(0x1cu,7,inputs));
            result=Vm::Execute(host,caller,"Self");
        } else {
            auto sound=std::make_shared<Vm::Reference>();sound->zero=zeros[0];
            sound->read=[&] {host.Store("Self",volume,{Value::Float(2.0f)});return Value::Text(Vm::Kind::Object,"Fixture.Sound");};
            auto firstMutation=std::make_shared<Vm::Reference>();firstMutation->zero=radiusMutation?zeros[3]:zeros[4];
            firstMutation->read=[&] {
                host.Store("Self",radiusMutation?radius:volume,{Value::Float(radiusMutation?64.0f:9.0f)});
                return radiusMutation?Value::Bool(false):Value::Float(32.0f);
            };
            auto lastMutation=std::make_shared<Vm::Reference>();lastMutation->zero=zeros[5];
            lastMutation->read=[&] {host.Store("Self",radius,{Value::Float(128.0f)});return Value::Float(1.0f);};
            std::vector<Vm::Evaluation> inputs(6u);inputs[0].reference=sound;
            inputs[radiusMutation?3u:4u].reference=firstMutation;
            if(radiusMutation) inputs[5].reference=lastMutation;
            result=Vm::ExecuteTransaction(host,[&](Vm::Execution& execution) {return execution.CallFunction(native,"Self",inputs);});
        }
        Returned(result,"Raw/reflected/direct initialized optional omissions consume per-Step sequential defaults");
        Require(result.value.kind==Vm::Kind::Bool && result.value.boolean && capabilities==6u && seeds==6u,
            "Omission materialization changed actual native result or repeated internal reflected hooks");
        Require(order==std::vector<std::string>{"cap0","cap1","cap2","cap3","cap4","cap5",
            "seed0","seed1","seed2","seed3","seed4","seed5"},"Immutable capabilities and sequential Step ordering changed");
        Require(host.Load("Self",radiusMutation?radius:volume).floating==(radiusMutation?128.0f:9.0f),
            "Materializing an earlier default suppressed later real argument side effects");
    }
    for(const bool direct:{false,true}) {
        TestHost host;auto native=Function("NoSeedForOmission",{},"NativeFixture");native.flags=0x400u;native.nativeIndex=1008u;
        native.variables={{"Input","Input",Value::Integer(0),0x90u,1u}};
        host.finals[{"Fixture",7}]=std::make_shared<Vm::Function>(native);
        host.materializeOmission=[](std::uint16_t index,const std::string&,std::size_t,const Vm::Function*) {return index==1008u;};
        const auto caller=Function("DeclaredZeroIsNotNativeDefault",Return(Call(0x1cu,7,{{0x0bu}})));
        const auto result=direct?Vm::Execute(host,native,"Self",{{}}):Vm::Execute(host,caller,"Self");
        Failed(result,Vm::Status::Unsupported,host,"Materialized omission needs an audited initialized seed, not a declaration zero");
        Require(host.nativeIndices.empty(),"Missing native omission seed executed native body");
    }
    for(const bool write:{false,true}) {
        TestHost host;Vm::Execution* active{};bool attempted{};
        const auto caller=Function("ReadOnlyOmissionCapability",Return(Native(1008u,{Native(1000u)})));
        const auto marker=host.Property(caller,1,"Marker",Value::Integer(0));host.Store("Self",marker,{Value::Integer(3)});
        host.materializeOmission=[&](std::uint16_t index,const std::string&,std::size_t,const Vm::Function*) {
            if(index!=1008u) return false;
            attempted=true;
            try {
                if(write) active->WriteReference(host.Reference({"Self",Vm::Scope::Instance,marker.key},marker),Value::Integer(55));
                else active->CallFunction(Function("ForbiddenOmissionCallback",Join({Native(1000u),Return()})),"Self");
            } catch(const std::exception&) {}return true;
        };
        const auto result=Vm::ExecuteTransaction(host,[&](Vm::Execution& execution) {
            active=&execution;try {execution.CallFunction(caller,"Self");} catch(const std::exception&) {}return Value{};
        });
        Failed(result,Vm::Status::Invalid,host,"Omission capability is immutable/read-only even when forbidden callback is caught");
        Require(attempted && host.effects==0u && host.Load("Self",marker).integer==3 && result.writes==0u,
            "Omission capability performed a mutation/effect or escaped shared poison");
    }
    std::cout<<"Per-Step materialized native omissions: "<<checks-initialChecks<<" checks and "<<rejections-initialRejections<<" refusal controls.\n";
}
void NativeCallSiteContextContracts() {
    const auto initialChecks=checks,initialRejections=rejections;
    const auto declaration=[] {
        auto native=Function("CallSiteNative",{},"NativeFixture");native.nativeIndex=264u;native.flags=0x400u;return native;
    };
    for(const bool state:{false,true}) for(const bool reflected:{false,true}) {
        TestHost host;const auto native=declaration();host.finals[{"Fixture",7}]=std::make_shared<Vm::Function>(native);
        auto caller=Function(state?"RealStateCaller":"RealFunctionCaller",{});caller.flags=0x100u;
        const auto marker=host.Property(caller,1,"Marker",Value::Integer(0));host.Store("Self",marker,{Value::Integer(3)});
        host.objects[{"Fixture",9}]="OtherReceiver";
        const auto argument=Native(1060u,{Ref(0x01u,1),Int(9)});
        const auto expression=Context(Ref(0x20u,9),reflected?Call(0x1cu,7,{argument}):Native(264u,{argument}));
        caller.bytecode=state?Join({expression,{0x08u}}):Return(expression);
        std::size_t captures{},calls{};
        host.nativeCallSiteContext=[&](std::uint16_t index,const std::string& receiver,const Vm::Function* actual,const Vm::Function* source)
            ->std::optional<Value> {
            if(index!=264u) return std::nullopt;
            ++captures;
            Require(receiver=="OtherReceiver" && static_cast<bool>(actual)==reflected && source &&
                source->path==caller.path && source->source==caller.source && source->flags==0x100u,
                "Native call-site capture confused caller frame, callee declaration or opcode receiver");
            Require(host.Load("Self",marker).integer==3,"Native caller context was captured after evaluating argument side effects");
            return Value::Text(Vm::Kind::String,source->path);
        };
        host.contextualNative=[&](std::uint16_t index,const std::string& receiver,const std::vector<Vm::Evaluation>& args,
            const Vm::Function* actual,Vm::Execution& execution,const std::optional<Value>& context) {
            if(index!=264u) return host.NativeWithExecution(index,receiver,args,actual,execution);
            ++calls;Require(captures==1u && context && context->text==caller.path && host.Load("Self",marker).integer==9,
                "Native context was recaptured/lost across reflected Run or failed to precede supplied arguments");
            return Vm::Evaluation{Value::Integer(23),{}};
        };
        Vm::Result result;
        if(state) {
            QuestVr::StateFrame frame;frame.codePath=frame.localsCodePath=caller.path;
            host.state=QuestVr::StateObject{true,true,frame,{}};host.statePrograms[caller.path]=caller;
            result=Vm::ResumeState(host,"Self");
        } else result=Vm::Execute(host,caller,"Self");
        Returned(result,"Raw/reflected script/state call sites preserve their actual frame before arguments");
        Require(captures==1u && calls==1u && result.writes==1u,"Native call-site context repeated capture or changed argument side effects");
    }
    for(unsigned dispatch=0u;dispatch<3u;++dispatch) {
        TestHost host;const auto native=declaration();host.events[{"Self","CallSiteNative",false}]=std::make_shared<Vm::Function>(native);
        std::size_t captures{};
        host.nativeCallSiteContext=[&](std::uint16_t index,const std::string&,const Vm::Function* actual,const Vm::Function* caller)
            ->std::optional<Value> {
            Require(index==264u && actual && actual->path==native.path && !caller,
                "Top-level direct native entry inferred its callee as a caller");++captures;return Value::Integer(31);
        };
        host.contextualNative=[&](std::uint16_t,const std::string&,const std::vector<Vm::Evaluation>&,
            const Vm::Function*,Vm::Execution&,const std::optional<Value>& context) {
            Require(context && context->integer==31,"Direct null-caller context was lost");return Vm::Evaluation{*context,{}};
        };
        const auto result=dispatch==0u?Vm::Execute(host,native,"Self"):
            Vm::ExecuteTransaction(host,[&](Vm::Execution& execution) {
                return dispatch==2u?execution.CallEvent("Self","CallSiteNative",false):execution.CallFunction(native,"Self");
            });
        Returned(result,"Direct API passes explicit null caller identity");
        Require(captures==1u && result.value.integer==31,"Direct call site repeated capture or changed native result");
    }
    for(const bool event:{false,true}) {
        TestHost host;const auto inner=declaration();auto outer=Function("ActualNativeCaller",{},"NativeFixture");
        outer.flags=0x500u;outer.nativeIndex=1008u;
        host.events[{"Inner","CallSiteNative",false}]=std::make_shared<Vm::Function>(inner);
        std::vector<std::string> callers;
        host.nativeCallSiteContext=[&](std::uint16_t index,const std::string&,const Vm::Function*,const Vm::Function* caller)
            ->std::optional<Value> {
            if(index==264u) callers.push_back(caller?caller->path:"<null>");
            return std::nullopt;
        };
        host.executionNative=[&](const std::string&,const std::vector<Vm::Evaluation>&,const Vm::Function* actual,Vm::Execution& execution) {
            if(actual && actual->path==outer.path) return Vm::Evaluation{
                event?execution.CallEvent("Inner","CallSiteNative",false):execution.CallFunction(inner,"Inner"),{}};
            return Vm::Evaluation{Value::Integer(23),{}};
        };
        const auto result=Vm::ExecuteTransaction(host,[&](Vm::Execution& execution) {
            execution.CallFunction(outer,"Self");return execution.CallFunction(inner,"Self");
        });
        Returned(result,"Synchronous Execution bridge captures its active caller, then restores null coordinator context");
        Require(callers==std::vector<std::string>{outer.path,"<null>"},"Nested bridge lost caller or retained stale prior caller after return");
    }
    {
        TestHost host;std::size_t captures{};
        host.legacyNativeContext=[&](std::uint16_t index,const std::string& receiver,const Vm::Function* actual)->std::optional<Value> {
            Require(index==264u && receiver=="Self" && !actual,"Legacy native context delegation changed declaration/receiver");
            ++captures;return Value::Integer(17);
        };
        host.contextualNative=[](std::uint16_t,const std::string&,const std::vector<Vm::Evaluation>&,
            const Vm::Function*,Vm::Execution&,const std::optional<Value>& context) {return Vm::Evaluation{*context,{}};};
        const auto result=Vm::Execute(host,Function("LegacyCaptureStillWorks",Return(Native(264u))),"Self");
        Returned(result,"New call-site hook delegates existing CaptureNativeContext hosts unchanged");
        Require(captures==1u && result.value.integer==17,"Legacy native context lost compatibility");
    }
    std::cout<<"Native caller-frame context: "<<checks-initialChecks<<" checks and "<<rejections-initialRejections<<" refusal controls.\n";
}
void FastTraceNativeInputContracts() {
    const auto initialChecks=checks,initialRejections=rejections;
    const auto layout=Vm::ResultLayout::CoreVector12;
    const auto zero=Value::Vector({}),endpoint=Value::Vector({101.0f,202.0f,303.0f});
    const auto entryLocation=Value::Vector({11.0f,22.0f,33.0f});
    const auto steppedLocation=Value::Vector({44.0f,55.0f,66.0f});
    const auto laterLocation=Value::Vector({77.0f,88.0f,99.0f});
    const auto configure=[&](TestHost& host) {
        auto native=Function("FastTrace",{},"Engine.Actor");native.nativeIndex=548u;native.flags=0x401u;
        native.variables={{"Engine.Actor.FastTrace.TraceEnd","TraceEnd",zero,0x80u,1u,layout},
            {"Engine.Actor.FastTrace.TraceStart","TraceStart",zero,0x90u,1u,layout},
            {"Engine.Actor.FastTrace.ReturnValue","ReturnValue",Value::Bool(false),0x400u,1u}};
        host.finals[{"Fixture",7}]=std::make_shared<Vm::Function>(native);
        host.uninitializedArgumentLayout=[layout](std::uint16_t index,const std::string&,std::size_t slot,
            const Vm::Function*)->std::optional<Vm::ResultLayout> {
            return index==548u && slot==0u?std::optional<Vm::ResultLayout>{layout}:std::nullopt;
        };
        host.hasSequentialSeeds=[](std::uint16_t index,const std::string&,const Vm::Function*) {return index==548u;};
        host.materializeOmission=[](std::uint16_t index,const std::string&,std::size_t slot,const Vm::Function*) {
            return index==548u && slot==1u;
        };
        return native;
    };
    // End's expression can change the opcode receiver's Location. Start's
    // initialized temporary must sample that changed value at its own Step,
    // not at native entry or after its expression changes Location again.
    for(unsigned dispatch=0u;dispatch<3u;++dispatch) for(unsigned startMode=0u;startMode<3u;++startMode) {
        TestHost host;const auto native=configure(host);
        auto caller=Function("FastTraceSequentialStart",{});
        const auto location=host.Property(caller,1,"Location",zero,0u,1u,false,layout);
        const auto source=host.Property(caller,2,"EndSource",zero,0u,1u,false,layout);
        host.Store("Self",location,{Value::Vector({-1.0f,-2.0f,-3.0f})});host.Store("Other",location,{entryLocation});
        host.Store("Self",source,{endpoint});host.Store("Other",source,{Value::Vector({-4.0f,-5.0f,-6.0f})});
        host.objects[{caller.source,9}]="Other";
        std::size_t hooks{},nativeCalls{};
        host.sequentialArgumentSeed=[&](std::uint16_t index,const std::string& receiver,std::size_t slot,
            const Vm::Function* actual,const std::vector<Value>& prefix)->std::optional<Value> {
            Require(index==548u && receiver=="Other" && static_cast<bool>(actual)==(dispatch!=0u) && prefix.size()==slot,
                "548 sequential seed confused caller frame, opcode receiver, native declaration or prefix");
            ++hooks;
            if(slot==0u) return std::nullopt;
            Require(slot==1u && Vm::Equal(prefix[0],endpoint) && Vm::Equal(host.Load("Other",location),steppedLocation),
                "548 optional Start default did not follow the completed End expression");
            return host.Load(receiver,location);
        };
        host.executionNative=[&](const std::string& receiver,const std::vector<Vm::Evaluation>& inputs,
            const Vm::Function* actual,Vm::Execution& execution) {
            ++nativeCalls;Require(receiver=="Other" && static_cast<bool>(actual)==(dispatch!=0u) && inputs.size()==2u,
                "548 native receiver/declaration/input count changed");
            Require(hooks==2u && !inputs[0].reference && !inputs[1].reference &&
                inputs[0].disposition==Vm::ResultDisposition::Written && inputs[1].disposition==Vm::ResultDisposition::Written &&
                Vm::Equal(inputs[0].Load(),endpoint) && Vm::Equal(inputs[1].Load(),steppedLocation),
                "548 supplied inputs were not detached complete vectors at their own Step");
            execution.WriteReference(host.Reference({"Self",Vm::Scope::Instance,source.key},source),laterLocation);
            execution.WriteReference(host.Reference({"Other",Vm::Scope::Instance,location.key},location),laterLocation);
            Require(Vm::Equal(inputs[0].Load(),endpoint) && Vm::Equal(inputs[1].Load(),steppedLocation),
                "548 ordinary input alias changed after the native mutated its source");
            return Vm::Evaluation{Value::Bool(true),{}};
        };
        Vm::Result result;
        if(dispatch<2u) {
            auto end=Function("EndExpression",Join({Let(Ref(0x01u,1),Vector(steppedLocation.vector)),Return(Ref(0x01u,2))}),"EndFixture");
            host.properties[{end.source,1}]=location;host.properties[{end.source,2}]=source;
            host.finals[{"Fixture",8}]=std::make_shared<Vm::Function>(end);
            const auto endExpression=Context(Ref(0x20u,9),Call(0x1cu,8));
            // The nested End call intentionally runs on Other but reads its
            // caller-independent own source. Supply the desired endpoint there.
            host.Store("Other",source,{endpoint});
            Bytes startExpression{0x0bu};
            if(startMode==1u) startExpression=Context({0x2au},Native(4095u),0u);
            if(startMode==2u) {
                auto start=Function("StartExpression",Join({Let(Ref(0x01u,1),Vector(laterLocation.vector)),Return({0x0bu})}),"StartFixture");
                host.properties[{start.source,1}]=location;host.finals[{"Fixture",10}]=std::make_shared<Vm::Function>(start);
                startExpression=Context(Ref(0x20u,9),Call(0x1cu,10));
            }
            caller.bytecode=Return(Context(Ref(0x20u,9),dispatch==0u?
                Native(548u,{endExpression,startExpression}):Call(0x1cu,7,{endExpression,startExpression})));
            result=Vm::Execute(host,caller,"Self");
        } else {
            auto end=std::make_shared<Vm::Reference>();end->zero=zero;end->resultLayout=layout;
            end->read=[&] {host.Store("Other",location,{steppedLocation});return host.Load("Self",source);};
            Vm::Evaluation start;
            if(startMode==1u) start={endpoint,{},Vm::ResultDisposition::ZeroFill,0u};
            if(startMode==2u) {
                // A NoWrite direct input must not read even an attached stale
                // alias. Side effects for this supplied slot happened before
                // its already-evaluated Evaluation reaches Execution.
                auto stale=std::make_shared<Vm::Reference>();stale->zero=zero;
                stale->read=[]()->Value {throw std::runtime_error("548 read an unwritten optional Start alias");};
                start={endpoint,stale,Vm::ResultDisposition::NoWrite};
            }
            result=Vm::ExecuteTransaction(host,[&](Vm::Execution& execution) {
                return execution.CallFunction(native,"Other",{{{},end},start});
            });
        }
        Returned(result,"548 raw/reflected/direct optional Start snapshots receiver Location after End");
        Require(nativeCalls==1u && hooks==2u && result.value.kind==Vm::Kind::Bool && result.value.boolean &&
            Vm::Equal(host.Load("Self",location),Value::Vector({-1.0f,-2.0f,-3.0f})),
            "548 repeated native/hooks, lost its result, or wrote the caller's Location");
    }
    // Ordinary argument expressions use FFrame.Object, not the opcode's
    // different receiver. Start can mutate the earlier End source, but must
    // not retroactively replace the copied End temporary with its new value.
    for(unsigned dispatch=0u;dispatch<3u;++dispatch) {
        TestHost host;const auto native=configure(host);
        auto caller=Function("FastTraceCallerFrameAndSnapshots",{});
        const auto source=host.Property(caller,1,"EndSource",zero,0u,1u,false,layout);
        const auto location=host.Property(caller,2,"Location",zero,0u,1u,false,layout);
        host.Store("Self",source,{endpoint});host.Store("Other",source,{laterLocation});
        host.Store("Self",location,{laterLocation});host.Store("Other",location,{entryLocation});
        host.objects[{caller.source,9}]="Other";
        host.sequentialArgumentSeed=[&](std::uint16_t,const std::string& receiver,std::size_t slot,const Vm::Function*,
            const std::vector<Value>& prefix)->std::optional<Value> {
            Require(receiver=="Other" && prefix.size()==slot,"548 default receiver/prefix changed");
            if(slot==0u) return std::nullopt;
            Require(Vm::Equal(prefix[0],endpoint),"548 End read the opcode receiver rather than FFrame.Object");
            return host.Load(receiver,location);
        };
        host.executionNative=[&](const std::string& receiver,const std::vector<Vm::Evaluation>& inputs,const Vm::Function*,Vm::Execution&) {
            Require(receiver=="Other" && inputs.size()==2u && !inputs[0].reference && !inputs[1].reference &&
                Vm::Equal(inputs[0].Load(),endpoint) && Vm::Equal(inputs[1].Load(),entryLocation) &&
                Vm::Equal(host.Load("Self",source),steppedLocation) && Vm::Equal(host.Load("Other",source),laterLocation),
                "548 Start mutation used wrong frame or changed the previously copied End");
            return Vm::Evaluation{Value::Bool(true),{}};
        };
        Vm::Result result;
        if(dispatch<2u) {
            const auto start=Native(1060u,{Ref(0x01u,1),Vector(steppedLocation.vector),Vector(entryLocation.vector)});
            caller.bytecode=Return(Context(Ref(0x20u,9),dispatch==0u?
                Native(548u,{Ref(0x01u,1),start}):Call(0x1cu,7,{Ref(0x01u,1),start})));
            result=Vm::Execute(host,caller,"Self");
        } else {
            auto end=host.Reference({"Self",Vm::Scope::Instance,source.key},source);
            auto start=std::make_shared<Vm::Reference>();start->zero=zero;start->resultLayout=layout;
            start->read=[&] {host.Store("Self",source,{steppedLocation});return entryLocation;};
            result=Vm::Execute(host,native,"Other",{{{},end},{{},start}});
        }
        Returned(result,"548 snapshots End before a later Start expression mutates its original alias");
        Require(result.value.boolean,"548 input snapshot controls changed its native Bool result");
    }
    // Required End is genuinely uninitialized. NoWrite/EX_Nothing and partial
    // Context clears cannot manufacture an endpoint from its declaration zero.
    for(unsigned dispatch=0u;dispatch<3u;++dispatch) for(const int bytes:{-2,-1,0,1,4,8,11,13,255}) {
        TestHost host;const auto native=configure(host);bool staleRead{},body{};unsigned startSeeds{};
        host.sequentialArgumentSeed=[&](std::uint16_t,std::string const&,std::size_t slot,const Vm::Function*,
            const std::vector<Value>&)->std::optional<Value> {
            if(slot==1u) {++startSeeds;return entryLocation;}return std::nullopt;
        };
        host.executionNative=[&](const std::string&,const std::vector<Vm::Evaluation>&,const Vm::Function*,Vm::Execution&) {
            body=true;return Vm::Evaluation{Value::Bool(true),{}};
        };
        const auto end=bytes==-2?Let(Ref(0x01u,1),Int(55)):bytes==-1?Bytes{0x0bu}:
            Context({0x2au},Native(4095u),static_cast<std::uint8_t>(bytes));
        const auto caller=Function("FastTraceUnwrittenEnd",Join({Native(1000u),Return(dispatch==1u?
            Call(0x1cu,7,{end,Native(1000u)}):Native(548u,{end,Native(1000u)}))}));
        const auto marker=host.Property(caller,1,"Marker",Value::Integer(0));host.Store("Self",marker,{Value::Integer(3)});
        Vm::Result result;
        if(dispatch<2u) result=Vm::Execute(host,caller,"Self");
        else {
            auto stale=std::make_shared<Vm::Reference>();stale->zero=zero;stale->resultLayout=layout;
            stale->read=[&] {staleRead=true;return endpoint;};
            const Vm::Evaluation input{endpoint,stale,bytes<0?Vm::ResultDisposition::NoWrite:Vm::ResultDisposition::ZeroFill,
                static_cast<std::uint8_t>(bytes<0?0u:bytes)};
            result=Vm::ExecuteTransaction(host,[&](Vm::Execution& execution) {
                execution.CallFunction(Function("EarlierEffect",Join({Native(1000u),Return()})),"Self");
                return execution.CallFunction(native,"Self",{input,{entryLocation,{}}});
            });
        }
        Failed(result,Vm::Status::Unsupported,host,"548 required End refuses incomplete uninitialized Vector12 writes");
        Require(!staleRead && !body && startSeeds==0u && host.effects==0u && host.rng==12345u &&
            host.Load("Self",marker).integer==3,"548 incomplete End read stale storage/evaluated Start/committed prefix effects");
    }
    for(unsigned dispatch=0u;dispatch<3u;++dispatch) for(const bool zeroEnd:{false,true}) {
        TestHost host;const auto native=configure(host);bool read{};
        host.sequentialArgumentSeed=[&](std::uint16_t,const std::string&,std::size_t slot,const Vm::Function*,
            const std::vector<Value>&)->std::optional<Value> {return slot==1u?std::optional<Value>{entryLocation}:std::nullopt;};
        host.executionNative=[&](const std::string&,const std::vector<Vm::Evaluation>& inputs,const Vm::Function*,Vm::Execution&) {
            Require(inputs.size()==2u && !inputs[0].reference && !inputs[1].reference &&
                Vm::Equal(inputs[0].Load(),zeroEnd?zero:endpoint) && Vm::Equal(inputs[1].Load(),zero),
                "548 full12 Context clearing did not independently overwrite exact End/Start temporaries");
            return Vm::Evaluation{Value::Bool(true),{}};
        };
        const auto end=zeroEnd?Context({0x2au},Native(4095u),12u):Vector(endpoint.vector);
        const auto start=Context({0x2au},Native(4095u),12u);
        const auto caller=Function("FastTraceFullVectorClear",Return(dispatch==1u?Call(0x1cu,7,{end,start}):Native(548u,{end,start})));
        auto stale=std::make_shared<Vm::Reference>();stale->zero=zero;stale->resultLayout=layout;
        stale->read=[&] {read=true;return endpoint;};
        const auto result=dispatch<2u?Vm::Execute(host,caller,"Self"):
            Vm::Execute(host,native,"Self",{zeroEnd?Vm::Evaluation{endpoint,stale,Vm::ResultDisposition::ZeroFill,12u}:
                Vm::Evaluation{endpoint,{}},{endpoint,stale,Vm::ResultDisposition::ZeroFill,12u}});
        Returned(result,"548 complete12-byte clearing initializes uninitialized End and overwrites optional Start");
        Require(!read && result.value.boolean,"548 full-vector clears read stale aliases or changed native Bool");
    }
    for(unsigned dispatch=0u;dispatch<3u;++dispatch) {
        TestHost host;const auto native=configure(host);bool body{};
        const auto caller=Function("FastTraceMissingEnd",Return(dispatch==1u?Call(0x1cu,7):Native(548u)));
        // Raw opcode has no reflected required-parameter check; its handler is
        // responsible for arity. The synthetic host mirrors that strict gate.
        host.executionNative=[&](const std::string&,const std::vector<Vm::Evaluation>& inputs,const Vm::Function*,Vm::Execution&) -> Vm::Evaluation {
            body=true;if(inputs.empty()) throw std::runtime_error("548 requires End");return {Value::Bool(true),{}};
        };
        const auto result=dispatch<2u?Vm::Execute(host,caller,"Self"):Vm::Execute(host,native,"Self");
        Failed(result,dispatch==0u?Vm::Status::Unsupported:Vm::Status::Invalid,host,"548 missing required End never gets declaration zero");
        Require(dispatch==0u?body:!body,"548 reflected/direct missing End reached body or raw strict arity gate was skipped");
    }
    auto vectorWithFields=endpoint;vectorWithFields.fields.emplace("padding",Value::Integer(1));
    auto vectorWithText=endpoint;vectorWithText.text="untrusted extra storage";
    for(const auto& invalid:std::vector<Value>{Value::Integer(0),Value::Rotator({1,2,3}),vectorWithFields,vectorWithText,
        Value::Vector({std::numeric_limits<float>::quiet_NaN(),2.0f,3.0f}),
        Value::Vector({1.0f,std::numeric_limits<float>::infinity(),3.0f})}) for(unsigned dispatch=0u;dispatch<3u;++dispatch) {
        TestHost host;const auto native=configure(host);bool body{};
        host.sequentialArgumentSeed=[&](std::uint16_t,const std::string&,std::size_t slot,const Vm::Function*,
            const std::vector<Value>&)->std::optional<Value> {return slot==1u?std::optional<Value>{entryLocation}:std::nullopt;};
        host.executionNative=[&](const std::string&,const std::vector<Vm::Evaluation>&,const Vm::Function*,Vm::Execution&) {
            body=true;return Vm::Evaluation{Value::Bool(true),{}};
        };
        const auto caller=Function("FastTraceMalformedEnd",Return(dispatch==1u?Call(0x1cu,7,{Ref(0x01u,1)}):Native(548u,{Ref(0x01u,1)})));
        const auto source=host.Property(caller,1,"MalformedEnd",invalid);host.Store("Self",source,{invalid});
        const auto result=dispatch<2u?Vm::Execute(host,caller,"Self"):Vm::Execute(host,native,"Self",{{invalid,{}}});
        Failed(result,Vm::Status::Invalid,host,"548 uninitialized Vector12 requires a finite typed Vector, never a same-size scalar/Rotator");
        Require(!body,"548 malformed/nonfinite required End reached native body");
    }
    for(const bool direct:{false,true}) for(const bool sequential:{false,true}) {
        TestHost host;const auto native=configure(host);bool read{},body{};
        if(sequential) host.sequentialArgumentSeed=[&](std::uint16_t,const std::string&,std::size_t slot,const Vm::Function*,
            const std::vector<Value>&)->std::optional<Value> {return slot==0u?std::optional<Value>{endpoint}:std::nullopt;};
        else host.argumentSeed=[&](std::uint16_t index,const std::string&,std::size_t slot,const Vm::Function*)->std::optional<Value> {
            return index==548u && slot==0u?std::optional<Value>{endpoint}:std::nullopt;
        };
        auto reference=std::make_shared<Vm::Reference>();reference->zero=zero;reference->resultLayout=layout;
        reference->read=[&] {read=true;return endpoint;};
        host.executionNative=[&](const std::string&,const std::vector<Vm::Evaluation>&,const Vm::Function*,Vm::Execution&) {
            body=true;return Vm::Evaluation{Value::Bool(true),{}};
        };
        const auto caller=Function("FastTraceEndCannotBeSeeded",Return(Native(548u,{Native(1000u)})));
        const auto result=direct?Vm::Execute(host,native,"Self",{{{},reference}}):Vm::Execute(host,caller,"Self");
        Failed(result,Vm::Status::Invalid,host,"548 required End cannot also obtain an initialized entry/sequential seed");
        Require(!read && !body && host.effects==0u,"548 conflicting End seed evaluated an argument or reached native body");
    }
    for(const bool direct:{false,true}) {
        TestHost host;auto native=configure(host);native.variables[0].resultLayout=Vm::ResultLayout::Unknown;
        host.finals[{"Fixture",7}]=std::make_shared<Vm::Function>(native);
        const auto caller=Function("FastTraceEndLayoutAuthority",Return(Call(0x1cu,7,{Native(1000u)})));
        const auto result=direct?Vm::Execute(host,native,"Self",{{endpoint,{}}}):Vm::Execute(host,caller,"Self");
        Failed(result,Vm::Status::Invalid,host,"548 canonical Vector value cannot override a conflicting reflected layout declaration");
        Require(host.effects==0u && host.nativeIndices.empty(),"548 conflicting declaration evaluated End or native body");
    }
    std::cout<<"FastTrace548 native argument temporaries: "<<checks-initialChecks<<" checks and "
        <<rejections-initialRejections<<" refusal controls.\n";
}
void TextWindowInputSnapshotContracts() {
    const auto initialChecks=checks,initialRejections=rejections;
    const auto byte=[](std::uint8_t value) {return Bytes{0x24u,value};};
    for(const auto index:{std::uint16_t{1556u},std::uint16_t{1557u},std::uint16_t{1558u}})
        for(const bool declared:{false,true}) for(const bool lateFailure:{false,true}) {
        TestHost host;
        const auto zero=index==1556u?Value::Byte(0u):index==1557u?Value::Float(0.0f):Value::Integer(0);
        const auto original=index==1556u?Value::Byte(17u):index==1557u?Value::Float(1.25f):Value::Integer(2);
        const auto replacement=index==1556u?Value::Byte(99u):index==1557u?Value::Float(99.0f):Value::Integer(99);
        const auto second=index==1556u?Value::Byte(23u):index==1557u?Value::Float(-2.5f):Value::Integer(4);
        const auto changed=index==1556u?byte(99u):index==1557u?Real(99.0f):Int(99);
        const auto returned=index==1556u?byte(23u):index==1557u?Real(-2.5f):Int(4);
        const auto mutation=Native(1060u,{Ref(0x01u,1),changed,returned});
        auto native=Function(index==1556u?"SetTextAlignments":index==1557u?"SetTextMargins":"SetLines",{},"Extension.TextWindow");
        native.flags=0x401u;native.nativeIndex=index;
        native.variables={{native.path+".First","First",zero,0x80u,1u},
            {native.path+".Second","Second",zero,0x80u,1u}};
        if(declared) host.finals[{"Fixture",7}]=std::make_shared<Vm::Function>(native);
        const auto call=declared?Call(0x1cu,7,{Ref(0x01u,1),mutation}):Native(index,{Ref(0x01u,1),mutation});
        const auto expression=Context(Ref(0x20u,9),call);
        const auto caller=Function("TextSetterStepSnapshots",lateFailure?
            Join({Native(1000u),expression,Native(4095u),Return()}):Join({Native(1000u),Return(expression)}));
        const auto source=host.Property(caller,1,"SourceInput",zero);
        host.Store("Self",source,{original});host.Store("Other",source,{second});
        host.objects[{caller.source,9}]="Other";
        bool entered{};
        host.executionNative=[&](const std::string& receiver,const std::vector<Vm::Evaluation>& arguments,
            const Vm::Function* actual,Vm::Execution&) {
            entered=true;
            Require(receiver=="Other" && static_cast<bool>(actual)==declared && arguments.size()==2u,
                "Text setter copy changed native receiver, declaration or arity");
            Require(!arguments[0u].reference && !arguments[1u].reference &&
                Vm::Equal(arguments[0u].Load(),original) && Vm::Equal(arguments[1u].Load(),second),
                "Text setter reloaded its first Step after the second Step mutated its source");
            Require(Vm::Equal(host.Load("Self",source),replacement) && Vm::Equal(host.Load("Other",source),second),
                "Text setter snapshot suppressed the genuine OUT mutation or evaluated it on the native receiver");
            return Vm::Evaluation{arguments[0u].Load(),{}};
        };
        const auto result=Vm::Execute(host,caller,"Self");
        if(lateFailure) {
            Failed(result,Vm::Status::Unsupported,host,"Late Text setter failure rolls back genuine argument mutation");
            Require(entered && Vm::Equal(host.Load("Self",source),original) &&
                Vm::Equal(host.Load("Other",source),second) && host.effects==0u && host.rng==12345u,
                "Late Text setter failure retained OUT mutation, prefix effect/RNG or changed receiver state");
        } else {
            Returned(result,"Each Text setter Step copies its input before evaluating the next expression");
            Require(entered && Vm::Equal(result.value,original) && Vm::Equal(host.Load("Self",source),replacement) &&
                Vm::Equal(host.Load("Other",source),second) && result.writes==1u && host.effects==1u,
                "Text setter copied input lost its committed OUT mutation or returned the changed source");
        }
    }
    const auto color=[](std::uint8_t value) {
        Value result;result.kind=Vm::Kind::Struct;
        result.fields={{"r",Value::Byte(value)},{"g",Value::Byte(37u)},
            {"b",Value::Byte(91u)},{"a",Value::Byte(203u)}};return result;
    };
    for(const auto index:{std::uint16_t{1476u},std::uint16_t{1572u},std::uint16_t{1573u},
        std::uint16_t{1270u},std::uint16_t{1272u},std::uint16_t{1486u},std::uint16_t{1483u},std::uint16_t{1550u},std::uint16_t{1413u}})
        for(const bool declared:{false,true}) {
        TestHost host;
        const bool gcObject=index==1270u||index==1486u;
        const auto zero=index==1476u?color(0u):index==1572u?Value::Float(0.0f):index==1272u?Value::Integer(-1):
            gcObject?Value::Text(Vm::Kind::Object,{}):index==1550u?Value::Text(Vm::Kind::String,{}):Value::Bool(false);
        const auto original=index==1476u?color(11u):index==1572u?Value::Float(-0.0f):index==1272u?Value::Integer(17):
            gcObject?Value::Text(Vm::Kind::Object,"OriginalContext"):index==1550u?Value::Text(Vm::Kind::String,"Original|nText"):Value::Bool(true);
        const auto replacement=index==1476u?color(99u):index==1572u?Value::Float(99.0f):index==1272u?Value::Integer(99):
            gcObject?Value::Text(Vm::Kind::Object,"ReplacementContext"):index==1550u?Value::Text(Vm::Kind::String,"Replacement"):Value::Bool(false);
        const auto other=index==1476u?color(23u):index==1572u?Value::Float(23.0f):index==1272u?Value::Integer(23):
            gcObject?Value::Text(Vm::Kind::Object,"OtherContext"):index==1550u?Value::Text(Vm::Kind::String,"Other"):Value::Bool(false);
        const auto layout=index==1476u?Vm::ResultLayout::CoreColor4:Vm::ResultLayout::Unknown;
        auto native=Function(index==1476u?"SetTextColor":index==1572u?"SetTextTimeout":index==1270u?"CopyGC":
            index==1272u?"PopGC":index==1486u?"ReleaseGC":index==1483u?"EnableTranslucentText":index==1550u?"SetText":index==1413u?"Show":"PauseLog",{},
            index==1476u||index==1486u||index==1483u||index==1413u?"Extension.Window":index==1270u||index==1272u?"Extension.GC":
                index==1550u?"Extension.TextWindow":"Extension.TextLogWindow");
        native.flags=0x401u;native.nativeIndex=index;
        native.variables={{native.path+".Input","Input",zero,index==1483u||index==1413u?0x90u:index==1550u?0x400880u:0x80u,1u,layout}};
        if(declared) host.finals[{"Fixture",7}]=std::make_shared<Vm::Function>(native);
        if(index==1476u) {
            host.suppliedReferenceLayout=layout;
            host.uninitializedArgumentLayout=[layout](std::uint16_t actual,const std::string&,std::size_t slot,
                const Vm::Function*)->std::optional<Vm::ResultLayout> {
                if(actual!=1476u) return std::nullopt;
                Require(slot==0u,"Text color copy requested an extra uninitialized slot");return layout;
            };
        } else host.argumentSeed=[index,zero](std::uint16_t actual,const std::string&,std::size_t slot,
            const Vm::Function*)->std::optional<Value> {
            if(actual!=index) return std::nullopt;
            Require(slot==0u,"TextLog scalar copy requested an extra initialized slot");
            return index==1483u||index==1413u?Value::Bool(true):zero;
        };
        const auto call=declared?Call(0x1cu,7,{Ref(0x01u,1)}):Native(index,{Ref(0x01u,1)});
        const auto expression=Context(Ref(0x20u,9),call);
        const auto caller=Function("TextSingleInputSourceFrame",Join({Native(1000u),Return(expression)}));
        const auto source=host.Property(caller,1,"SourceInput",zero,0u,1u,false,layout);
        host.Store("Self",source,{original});host.Store("Other",source,{other});host.objects[{caller.source,9}]="Other";
        unsigned entered{};
        host.executionNative=[&](const std::string& receiver,const std::vector<Vm::Evaluation>& arguments,
            const Vm::Function* actual,Vm::Execution& execution) {
            ++entered;
            Require(receiver=="Other" && static_cast<bool>(actual)==declared && arguments.size()==1u && !arguments[0u].reference,
                "Single Text native retained an ordinary input alias or changed source/receiver identity");
            const auto copied=arguments[0u].Load();
            Require(Vm::Equal(copied,original) && (index!=1572u ||
                std::bit_cast<std::uint32_t>(copied.floating)==0x80000000u),
                "Single Text native changed its source-frame copied value/raw signed-zero Float");
            execution.WriteReference(host.Reference({"Self",Vm::Scope::Instance,source.key},source),replacement);
            Require(Vm::Equal(arguments[0u].Load(),original),"Single Text native reread its source after host mutation");
            return Vm::Evaluation{copied,{}};
        };
        const auto result=Vm::Execute(host,caller,"Self");Returned(result,"Single Text native copies source-frame argument before native entry");
        Require(entered==1u && Vm::Equal(result.value,original) && Vm::Equal(host.Load("Self",source),replacement) &&
            Vm::Equal(host.Load("Other",source),other) && result.writes==1u && host.effects==1u,
            "Single Text input copy lost its real native write, changed source frame or suppressed prefix effects");
        host.Store("Self",source,{original});
        const auto previousRng=host.rng;const auto previousEffects=host.effects;
        const auto failing=Function("TextSingleInputLateRollback",Join({Native(1000u),expression,Native(4095u),Return()}));
        const auto rejected=Vm::Execute(host,failing,"Self");
        Require(rejected.status==Vm::Status::Unsupported && host.rollbackCount==1u && host.commitCount==1u &&
            entered==2u && Vm::Equal(host.Load("Self",source),original) && Vm::Equal(host.Load("Other",source),other) &&
            host.effects==previousEffects && host.rng==previousRng,
            "Late single Text input failure retained its real source write or prefix effects");
        ++rejections;
    }
    std::cout<<"TextWindow/TextLog native Step snapshots: "<<checks-initialChecks<<" checks and "
        <<rejections-initialRejections<<" refusal controls.\n";
}
void WindowInputSnapshotContracts() {
    const auto initialChecks=checks;
    const auto expression=[](std::uint16_t index,bool declared,const std::vector<Bytes>& inputs) {
        auto code=declared?Call(0x1c,7):Native(index);code.pop_back();
        for(const auto& input:inputs) Append(code,input);
        code.push_back(0x16u);return code;
    };
    const auto declaration=[](TestHost& host,std::uint16_t index,const std::vector<Value>& zeros) {
        auto native=Function("AuditedCopiedWindowInputs",{},"Extension.Window");native.flags=0x400u;native.nativeIndex=index;
        for(std::size_t slot=0u;slot<zeros.size();++slot)
            native.variables.push_back({"Input"+std::to_string(slot),"Input"+std::to_string(slot),zeros[slot],0x80u,1u});
        host.finals[{"Fixture",7}]=std::make_shared<Vm::Function>(native);
    };
    for(const auto index:{std::uint16_t{1421u},std::uint16_t{1422u},std::uint16_t{1433u},std::uint16_t{1538u},std::uint16_t{1539u},std::uint16_t{1540u}})
        for(const bool declared:{false,true}) {
        const bool byte=index==1422u || index==1538u;
        TestHost host;const auto zero=index==1421u?Value::Text(Vm::Kind::Object,{}):byte?Value::Byte(0u):Value::Float(0.0f);
        const auto input=index==1421u?Value::Text(Vm::Kind::Object,"Original.Window"):byte?Value::Byte(7u):Value::Float(12.5f);
        if(declared) declaration(host,index,{zero});
        const auto caller=Function("WindowScalarInputCopy",Return(expression(index,declared,{Native(1007u,{Ref(0x01,1)})})));
        const auto source=host.Property(caller,1,"Input",zero);host.Store("Self",source,{input});
        host.executionNative=[&](const std::string&,const std::vector<Vm::Evaluation>& arguments,const Vm::Function* actual,Vm::Execution&) {
            Require(static_cast<bool>(actual)==declared && arguments.size()==1u && !arguments[0].reference && Vm::Equal(arguments[0].Load(),input),
                "Audited Window/Tile ordinary scalar input retained its alias or changed native declaration identity");
            return Vm::Evaluation{arguments[0].Load(),{}};
        };
        const auto result=Vm::Execute(host,caller,"Self");Returned(result,"Audited Window/Tile Object/Byte/Float inputs are detached during evaluation");
        Require(Vm::Equal(result.value,input),"Audited Window/Tile scalar snapshot changed input value");
    }
    for(const auto index:{std::uint16_t{1430u},std::uint16_t{1432u},std::uint16_t{1535u}}) for(const bool declared:{false,true})
        for(std::size_t earlier=0u;earlier<(index==1430u?3u:1u);++earlier) {
        const auto count=index==1430u?4u:2u;
        TestHost host;if(declared) declaration(host,index,std::vector<Value>(count,Value::Float(0.0f)));
        const std::array<float,4u> values{12.0f,23.0f,34.0f,45.0f};std::vector<Bytes> inputs;
        for(std::size_t slot=0u;slot<count-1u;++slot) inputs.push_back(Ref(0x01,static_cast<std::int32_t>(slot+1u)));
        inputs.push_back(Native(1060u,{Ref(0x01,static_cast<std::int32_t>(earlier+1u)),Real(99.0f),Real(values[count-1u])}));
        const auto caller=Function("WindowFloatInputCopies",Return(expression(index,declared,inputs)));
        std::vector<Vm::Property> properties;
        for(std::size_t slot=0u;slot<count;++slot) {
            properties.push_back(host.Property(caller,static_cast<std::int32_t>(slot+1u),"Input"+std::to_string(slot),Value::Float(0.0f)));
            host.Store("Self",properties.back(),{Value::Float(values[slot])});
        }
        host.executionNative=[&](const std::string&,const std::vector<Vm::Evaluation>& arguments,const Vm::Function* actual,Vm::Execution&) {
            Require(static_cast<bool>(actual)==declared && arguments.size()==count,"Audited1430/1432/1535 lost its declaration or input count");
            for(std::size_t slot=0u;slot<count;++slot)
                Require(!arguments[slot].reference && arguments[slot].Load().floating==values[slot],
                    "Audited1430/1432/1535 reloaded an earlier Float after a later input mutated its source");
            return Vm::Evaluation{arguments[earlier].Load(),{}};
        };
        const auto result=Vm::Execute(host,caller,"Self");Returned(result,"Audited1430/1432/1535 copies each ordinary Float before evaluating later inputs");
        Require(result.value.floating==values[earlier] && host.Load("Self",properties[earlier]).floating==99.0f && result.writes==1u,
            "Audited1430/1432/1535 input copies lost the real later mutation or returned the mutated source");
    }
    for(const auto index:{std::uint16_t{1421u},std::uint16_t{1422u},std::uint16_t{1430u},std::uint16_t{1432u},std::uint16_t{1433u},
        std::uint16_t{1535u},std::uint16_t{1538u},std::uint16_t{1539u},std::uint16_t{1540u}}) for(const bool declared:{false,true}) {
        const bool byte=index==1422u || index==1538u;
        const auto zero=index==1421u?Value::Text(Vm::Kind::Object,{}):byte?Value::Byte(0u):Value::Float(0.0f);
        const auto count=index==1430u?4u:index==1432u || index==1535u?2u:1u;
        const auto sizes=byte?std::vector<int>{-1,0,1}:index==1421u?std::vector<int>{-1,0,4}:std::vector<int>{-1,0,1,2,3,4};
        for(std::size_t selected=0u;selected<count;++selected) for(const int bytes:sizes) {
            TestHost host;if(declared) declaration(host,index,std::vector<Value>(count,zero));
            std::vector<Bytes> inputs(count,index==1421u?Ref(0x20,9):byte?Bytes{0x24u,7u}:Real(12.0f));
            inputs[selected]=bytes<0?Bytes{0x0bu}:Context({0x2au},Native(4095u),static_cast<std::uint8_t>(bytes));
            const auto caller=Function("WindowNativeSeededTypedInputs",Return(expression(index,declared,inputs)));
            host.objects[{caller.source,9}]="Original.Window";
            host.argumentSeed=[&](std::uint16_t actual,const std::string&,std::size_t slot,const Vm::Function* resolved)->std::optional<Value> {
                if(actual!=index) return std::nullopt;
                Require(slot<count && static_cast<bool>(resolved)==declared,"Audited native input seed lost its slot/declaration");return zero;
            };
            host.executionNative=[&](const std::string&,const std::vector<Vm::Evaluation>& arguments,const Vm::Function*,Vm::Execution&) {
                Require(arguments.size()==count && !arguments[selected].reference,"Typed native seed retained a result alias");
                const auto value=arguments[selected].Load();
                Require((value.kind==Vm::Kind::Nothing)==(bytes<0),"Audited native seed consumed its explicit omission marker");
                return Vm::Evaluation{value,{}};
            };
            const auto result=Vm::Execute(host,caller,"Self");Returned(result,"Audited native zero seeds preserve NoWrite/full-zero/explicit omissions");
            Require(Vm::Equal(result.value,bytes<0?Value{}:zero),"Audited native seeded result used the wrong typed zero");
        }
    }
    std::cout<<"Additional audited Window ordinary input copies and zero seeds: "<<checks-initialChecks<<" checks.\n";
}
void TileNativeInputContracts() {
    const auto initialChecks=checks,initialRejections=rejections;
    const auto configure=[](TestHost& host,std::uint16_t index) {
        const bool byte=index==1538u || index==1541u,boolean=index==1544u || index==1545u;
        const auto count=index==1535u || index==1541u?2u:1u;
        const auto zero=byte?Value::Byte(0u):boolean?Value::Bool(false):Value::Float(0.0f);
        auto native=Function("AuditedTileNativeInput",{},"Extension.TileWindow");native.nativeIndex=index;native.flags=0x401u;
        for(std::size_t slot=0u;slot<count;++slot)
            native.variables.push_back({"Tile.Input"+std::to_string(slot),"Input"+std::to_string(slot),zero,0x80u,1u});
        host.argumentSeed=[&host,index,count,zero](std::uint16_t actual,const std::string& receiver,std::size_t slot,
            const Vm::Function* declaration)->std::optional<Value> {
            if(actual!=index) return std::nullopt;
            Require(receiver=="Self" && slot<count && (!declaration || declaration->nativeIndex==index),
                "Direct Tile seed lost native receiver, slot or declaration");
            host.observations.push_back("seed"+std::to_string(slot));return zero;
        };
        return native;
    };
    for(const auto index:{std::uint16_t{1535u},std::uint16_t{1538u},std::uint16_t{1539u},std::uint16_t{1540u},
        std::uint16_t{1541u},std::uint16_t{1544u},std::uint16_t{1545u}}) {
        const bool byte=index==1538u || index==1541u,boolean=index==1544u || index==1545u;
        const auto count=index==1535u || index==1541u?2u:1u;
        const auto zero=byte?Value::Byte(0u):boolean?Value::Bool(false):Value::Float(0.0f);
        for(std::size_t selected=0u;selected<count;++selected)
            for(const auto disposition:{Vm::ResultDisposition::NoWrite,Vm::ResultDisposition::ZeroFill})
                for(const auto bytes:byte?std::vector<unsigned>{0u,1u}:boolean?std::vector<unsigned>{0u,4u}:std::vector<unsigned>{0u,1u,2u,3u,4u}) {
            TestHost host;const auto native=configure(host,index);bool read{};
            auto alias=std::make_shared<Vm::Reference>();alias->zero=zero;
            alias->read=[&]() ->Value {read=true;throw std::runtime_error("Unwritten Tile input read stale alias");};
            std::vector<Vm::Evaluation> inputs(count,{byte?Value::Byte(12u):boolean?Value::Bool(true):Value::Float(12.5f),{}});
            inputs[selected]={byte?Value::Byte(99u):boolean?Value::Bool(true):Value::Float(99.0f),alias,disposition,static_cast<std::uint8_t>(bytes)};
            host.executionNative=[&](const std::string&,const std::vector<Vm::Evaluation>& arguments,const Vm::Function*,Vm::Execution&) {
                for(const auto& argument:arguments) Require(!argument.reference,"Ordinary Tile input kept an OUT alias");
                return Vm::Evaluation{arguments[selected].Load(),{}};
            };
            const auto result=Vm::Execute(host,native,"Self",inputs);
            Returned(result,"Direct Tile independently initialized scalar defaults and partial clearing");
            Require(!read && Vm::Equal(result.value,zero) && result.writes==0u &&
                host.observations==(count==2u?std::vector<std::string>{"seed0","seed1"}:std::vector<std::string>{"seed0"}),
                "Tile default read stale storage, used other margin, repeated seed hooks or lost independent +0");
        }
        for(const auto number:{-12.5f,-0.0f,0.0f,12.5f}) {
            if(byte || boolean) continue;
            TestHost host;const auto native=configure(host,index);
            std::vector<Vm::Evaluation> inputs(count,{Value::Float(number),{}});
            host.executionNative=[](const std::string&,const std::vector<Vm::Evaluation>& arguments,const Vm::Function*,Vm::Execution&) {
                for(const auto& argument:arguments) Require(!argument.reference,"Written Tile scalar input kept an OUT alias");
                return Vm::Evaluation{arguments[0].Load(),{}};
            };
            const auto result=Vm::Execute(host,native,"Self",inputs);Returned(result,"Tile input bridge preserves finite negative and signed-zero bits");
            Require(result.value.kind==Vm::Kind::Float && std::bit_cast<std::uint32_t>(result.value.floating)==std::bit_cast<std::uint32_t>(number),
                "Tile VM bridge incorrectly clamped or changed signed-zero input before runtime setter");
        }
        if(index==1541u || boolean) for(const bool declared:{false,true})
            for(unsigned input=0u;input<(byte?256u:2u);++input) {
            TestHost host;const auto native=configure(host,index);
            host.finals[{"Fixture",7}]=std::make_shared<Vm::Function>(native);
            const auto value=byte?Value::Byte(static_cast<std::uint8_t>(input)):Value::Bool(input!=0u);
            const auto changed=byte?Value::Byte(99u):Value::Bool(input==0u);
            std::vector<Bytes> arguments{Native(1007u,{Ref(0x01,1)})};
            if(count==2u) arguments.push_back(Native(1060u,{Ref(0x01,1),Bytes{0x24u,99u},Bytes{0x24u,static_cast<std::uint8_t>(255u-input)}}));
            auto code=declared?Call(0x1c,7):Native(index);code.pop_back();
            for(const auto& argument:arguments) Append(code,argument);
            code.push_back(0x16u);
            const auto caller=Function("TileByteBoolOpcodeInputCopies",Return(code));
            const auto source=host.Property(caller,1,"SourceInput",zero);host.Store("Self",source,{value});
            host.executionNative=[&](const std::string&,const std::vector<Vm::Evaluation>& actual,const Vm::Function* function,Vm::Execution& execution) {
                Require(static_cast<bool>(function)==declared && actual.size()==count,
                    "Tile Byte/Bool opcode input lost declaration or argument count");
                Require(!actual[0].reference && Vm::Equal(actual[0].Load(),value),
                    "Tile Byte/Bool input retained alias, clamped an out-of-enum Byte or reloaded after later input");
                if(count==2u) Require(!actual[1].reference && actual[1].Load().integer==static_cast<std::int32_t>(255u-input),
                    "Tile vertical Byte input changed its copied ordinary value");
                execution.WriteReference(host.Reference({"Self",Vm::Scope::Instance,source.key},source),changed);
                Require(Vm::Equal(actual[0].Load(),value),"Tile Byte/Bool copied input reloaded its source in the setter");
                return Vm::Evaluation{actual[0].Load(),{}};
            };
            const auto result=Vm::Execute(host,caller,"Self");Returned(result,"Tile alignment/equal-size copies raw opcode and resolved input values");
            Require(Vm::Equal(result.value,value) && Vm::Equal(host.Load("Self",source),changed) &&
                result.writes==(count==2u?2u:1u),"Tile Byte/Bool snapshot lost real source mutations or original input");
        }
        if(boolean) for(const unsigned bytes:{1u,2u,3u}) {
            TestHost host;const auto native=configure(host,index);bool read{};
            auto alias=std::make_shared<Vm::Reference>();alias->zero=zero;alias->read=[&] {read=true;return Value::Bool(true);};
            const auto result=Vm::ExecuteTransaction(host,[&](Vm::Execution& execution) {
                execution.CallFunction(Function("EarlierTileBoolInputEffect",Join({Native(1000u),Return()})),"Self");
                try {execution.CallFunction(native,"Self",{{Value::Bool(true),alias,Vm::ResultDisposition::ZeroFill,static_cast<std::uint8_t>(bytes)}});}
                catch(const std::exception&){}return Value{};
            });
            Failed(result,Vm::Status::Unsupported,host,"Tile equal-size input refuses unaudited partial packed Bool clearing");
            Require(!read && host.effects==0u && host.rng==12345u && host.nativeIndices==std::vector<std::uint16_t>{1000u},
                "Tile Bool partial-clear refusal read stale storage, reached body or escaped rollback");
        }
        for(const auto bytes:{byte?2u:5u,255u}) {
            TestHost host;const auto native=configure(host,index);bool read{};
            auto alias=std::make_shared<Vm::Reference>();alias->zero=zero;alias->read=[&] {read=true;return zero;};
            std::vector<Vm::Evaluation> inputs(count,{zero,{}});inputs[0]={zero,alias,Vm::ResultDisposition::ZeroFill,static_cast<std::uint8_t>(bytes)};
            const auto result=Vm::ExecuteTransaction(host,[&](Vm::Execution& execution) {
                execution.CallFunction(Function("EarlierTileInputEffect",Join({Native(1000u),Return()})),"Self");
                try {execution.CallFunction(native,"Self",inputs);}catch(const std::exception&){}return Value{};
            });
            Failed(result,Vm::Status::Unsupported,host,"Tile scalar result clearing cannot overflow its audited initialized destination");
            Require(!read && host.effects==0u && host.rng==12345u && host.nativeIndices==std::vector<std::uint16_t>{1000u},
                "Overflowing Tile input read stale storage, reached its body or escaped caught-failure rollback");
        }
        {
            TestHost host;const auto result=Vm::Execute(host,Function("TileDoesNotInferGlobalDefault",Return(Native(index,
                {Context({0x2au},Native(4095u),static_cast<std::uint8_t>(byte?1u:4u))}))),"Self");
            Failed(result,Vm::Status::Unsupported,host,"Opcode-only Tile default requires runtime-owned native declaration/seed authority");
            Require(host.nativeArguments.empty(),"Tile native index alone granted an unaudited global result default");
        }
    }
    {
        TestHost host;const auto native=configure(host,1535u);Value source=Value::Float(-12.5f);
        auto first=std::make_shared<Vm::Reference>(),second=std::make_shared<Vm::Reference>();
        first->zero=second->zero=Value::Float(0.0f);
        first->read=[&] {host.observations.push_back("h-read");return source;};
        second->read=[&] {host.observations.push_back("v-read");source=Value::Float(99.0f);return Value::Float(23.5f);};
        host.executionNative=[](const std::string&,const std::vector<Vm::Evaluation>& arguments,const Vm::Function*,Vm::Execution&) {
            Require(arguments.size()==2u && !arguments[0].reference && !arguments[1].reference,
                "Direct Tile.SetMargins lost its two copied ordinary inputs");
            return Vm::Evaluation{Value::Vector({arguments[0].Load().floating,arguments[1].Load().floating,0.0f}),{}};
        };
        const auto result=Vm::Execute(host,native,"Self",{{{},first},{{},second}});
        Returned(result,"Direct Tile.SetMargins copies horizontal input before vertical input source effects");
        Require(Vm::Equal(result.value,Value::Vector({-12.5f,23.5f,0.0f})) && source.floating==99.0f &&
            host.observations==std::vector<std::string>{"seed0","seed1","h-read","v-read"},
            "Tile.SetMargins reread horizontal source, reordered inputs or repeated native hooks");
    }
    std::cout<<"Audited Tile native input copies/defaults: "<<checks-initialChecks<<" checks and "
        <<rejections-initialRejections<<" refusal controls.\n";
}
void WindowConfigureChildInputContracts() {
    const auto initialChecks=checks,initialRejections=rejections;
    const auto expression=[](bool declared,const std::vector<Bytes>& inputs) {
        auto code=declared?Call(0x1c,7):Native(1461u);code.pop_back();
        for(const auto& input:inputs) Append(code,input);
        code.push_back(0x16u);return code;
    };
    const auto configure=[](TestHost& host) {
        auto native=Function("ConfigureChild",{},"Extension.Window");native.nativeIndex=1461u;native.flags=0x401u;
        for(const auto* name:{"newX","newY","newWidth","newHeight"})
            native.variables.push_back({"ConfigureChild."+std::string(name),name,Value::Float(0.0f),0x80u,1u});
        host.finals[{"Fixture",7}]=std::make_shared<Vm::Function>(native);
        host.argumentSeed=[&host](std::uint16_t index,const std::string& receiver,std::size_t slot,
            const Vm::Function* declaration)->std::optional<Value> {
            if(index!=1461u) return std::nullopt;
            Require(slot<4u && (receiver=="Self" || receiver=="Other") &&
                (!declaration || declaration->path=="Extension.Window.ConfigureChild"),
                "ConfigureChild initialized Float seed lost its receiver/slot/declaration");
            host.observations.push_back("configure-seed"+std::to_string(slot));return Value::Float(0.0f);
        };
        return native;
    };
    const auto seeds=std::vector<std::string>{"configure-seed0","configure-seed1","configure-seed2","configure-seed3"};
    for(const bool declared:{false,true}) for(std::size_t earlier=0u;earlier<3u;++earlier) {
        TestHost host;configure(host);const std::array<float,4> values{-12.5f,23.0f,34.0f,45.0f};
        const auto caller=Function("ConfigureChildCopiesBeforeLaterInputs",Return(expression(declared,
            {Ref(0x01,1),Ref(0x01,2),Ref(0x01,3),Native(1060u,
                {Ref(0x01,static_cast<std::int32_t>(earlier+1u)),Real(99.0f),Real(values[3])})})));
        std::vector<Vm::Property> properties;
        for(std::size_t slot=0u;slot<3u;++slot) {
            properties.push_back(host.Property(caller,static_cast<std::int32_t>(slot+1u),
                "Input"+std::to_string(slot),Value::Float(0.0f)));
            host.Store("Self",properties.back(),{Value::Float(values[slot])});
        }
        host.executionNative=[&](const std::string& receiver,const std::vector<Vm::Evaluation>& arguments,
            const Vm::Function* actual,Vm::Execution&) {
            Require(receiver=="Self" && static_cast<bool>(actual)==declared && arguments.size()==4u,
                "ConfigureChild lost its native receiver, actual declaration or four inputs");
            for(std::size_t slot=0u;slot<4u;++slot)
                Require(!arguments[slot].reference && arguments[slot].Load().kind==Vm::Kind::Float &&
                    arguments[slot].Load().floating==values[slot],
                    "ConfigureChild reloaded an earlier Float after a later expression mutated its source");
            return Vm::Evaluation{arguments[earlier].Load(),{}};
        };
        const auto result=Vm::Execute(host,caller,"Self");Returned(result,"ConfigureChild independently copies four ordinary Float expressions");
        Require(result.value.floating==values[earlier] && host.Load("Self",properties[earlier]).floating==99.0f && result.writes==1u,
            "ConfigureChild snapshot suppressed real later mutation or returned the mutated source");
        Require(host.observations.size()>=4u && std::equal(seeds.begin(),seeds.end(),host.observations.begin()),
            "ConfigureChild did not capture all four seed contracts before expression side effects");
    }
    for(const bool declared:{false,true}) {
        TestHost host;configure(host);std::vector<Bytes> inputs;
        for(std::int32_t slot=1;slot<=4;++slot) inputs.push_back(Ref(0x01,slot));
        const auto caller=Function("ConfigureChildUsesArgumentSourceFrame",Return(Context(Ref(0x20,9),expression(declared,inputs))));
        host.objects[{caller.source,9}]="Other";std::vector<Vm::Property> properties;
        for(std::int32_t slot=1;slot<=4;++slot) {
            properties.push_back(host.Property(caller,slot,"FrameInput"+std::to_string(slot),Value::Float(0.0f)));
            host.Store("Self",properties.back(),{Value::Float(static_cast<float>(slot))});
            host.Store("Other",properties.back(),{Value::Float(-static_cast<float>(slot))});
        }
        host.executionNative=[&](const std::string& receiver,const std::vector<Vm::Evaluation>& arguments,
            const Vm::Function* actual,Vm::Execution& execution) {
            Require(receiver=="Other" && static_cast<bool>(actual)==declared && arguments.size()==4u,
                "ConfigureChild context receiver/declaration changed");
            for(std::size_t slot=0u;slot<4u;++slot) {
                Require(!arguments[slot].reference && arguments[slot].Load().floating==static_cast<float>(slot+1u),
                    "ConfigureChild evaluated argument in native receiver instead of FFrame.Object");
                execution.WriteReference(host.Reference({"Self",Vm::Scope::Instance,properties[slot].key},properties[slot]),Value::Float(99.0f));
                Require(arguments[slot].Load().floating==static_cast<float>(slot+1u),
                    "ConfigureChild kept an ordinary input alias during native body work");
            }
            return Vm::Evaluation{arguments[3].Load(),{}};
        };
        const auto result=Vm::Execute(host,caller,"Self");Returned(result,"ConfigureChild source-frame inputs remain detached from native body mutations");
        Require(result.value.floating==4.0f && result.writes==4u,"ConfigureChild lost copied height or actual body mutations");
    }
    for(const unsigned mode:{0u,1u,2u}) for(std::size_t selected=0u;selected<4u;++selected)
        for(const float input:{-12.5f,-0.0f,0.0f,12.5f}) {
        TestHost host;const auto native=configure(host);
        host.executionNative=[&](const std::string&,const std::vector<Vm::Evaluation>& arguments,const Vm::Function*,Vm::Execution&) {
            Require(arguments.size()==4u && !arguments[selected].reference,"ConfigureChild finite input retained an OUT alias");
            return Vm::Evaluation{arguments[selected].Load(),{}};
        };
        std::vector<Bytes> inputs(4u,Real(25.0f));inputs[selected]=Real(input);
        std::vector<Vm::Evaluation> direct(4u,{Value::Float(25.0f),{}});direct[selected]={Value::Float(input),{}};
        const auto result=mode==2u?Vm::Execute(host,native,"Self",direct):
            Vm::Execute(host,Function("ConfigureChildFiniteInputBits",Return(expression(mode==1u,inputs))),"Self");
        Returned(result,"ConfigureChild input bridge preserves finite negative/signed-zero bits without layout clamps");
        Require(result.value.kind==Vm::Kind::Float && std::bit_cast<std::uint32_t>(result.value.floating)==std::bit_cast<std::uint32_t>(input) &&
            result.writes==0u && host.observations==seeds,"ConfigureChild VM bridge clamped an input or changed its signed zero");
    }
    for(const bool declared:{false,true}) for(std::size_t selected=0u;selected<4u;++selected) for(const int bytes:{-1,0,1,2,3,4}) {
        TestHost host;configure(host);std::vector<Bytes> inputs(4u,Real(12.5f));
        inputs[selected]=bytes<0?Bytes{0x0bu}:Context({0x2au},Native(4095u),static_cast<std::uint8_t>(bytes));
        host.executionNative=[&](const std::string&,const std::vector<Vm::Evaluation>& arguments,const Vm::Function*,Vm::Execution&) {
            Require(arguments.size()==4u,"ConfigureChild default lost independent input slots");
            for(const auto& argument:arguments) Require(!argument.reference,"ConfigureChild default retained an OUT target");
            const auto value=arguments[selected].Load();
            Require(value.kind==(bytes<0?Vm::Kind::Nothing:Vm::Kind::Float),"ConfigureChild default consumed explicit Nothing or lost initialized Float destination");
            return Vm::Evaluation{value.kind==Vm::Kind::Nothing?Value::Float(0.0f):value,{}};
        };
        const auto result=Vm::Execute(host,Function("ConfigureChildInitializedFloatTemporary",Return(expression(declared,inputs))),"Self");
        Returned(result,"ConfigureChild initialized four independent Float+0 temporaries for Nothing and partial/full context clearing");
        Require(result.value.kind==Vm::Kind::Float && std::bit_cast<std::uint32_t>(result.value.floating)==0u &&
            host.observations==seeds && result.writes==0u,"ConfigureChild default inherited another slot or repeated seed hooks");
    }
    for(std::size_t selected=0u;selected<4u;++selected)
        for(const auto disposition:{Vm::ResultDisposition::NoWrite,Vm::ResultDisposition::ZeroFill}) for(const unsigned bytes:{0u,1u,2u,3u,4u}) {
        TestHost host;const auto native=configure(host);bool read{};
        auto alias=std::make_shared<Vm::Reference>();alias->zero=Value::Float(0.0f);
        alias->read=[&]() ->Value {read=true;throw std::runtime_error("Unwritten ConfigureChild alias read stale storage");};
        std::vector<Vm::Evaluation> inputs(4u,{Value::Float(12.5f),{}});
        inputs[selected]={Value::Float(99.0f),alias,disposition,static_cast<std::uint8_t>(bytes)};
        host.executionNative=[&](const std::string&,const std::vector<Vm::Evaluation>& arguments,const Vm::Function*,Vm::Execution&) {
            for(const auto& argument:arguments) Require(!argument.reference,"ConfigureChild direct ordinary input retained a reference");
            return Vm::Evaluation{arguments[selected].Load(),{}};
        };
        const auto result=Vm::Execute(host,native,"Self",inputs);Returned(result,"Direct ConfigureChild unwritten initialized temporary never reads stale alias");
        Require(!read && result.value.kind==Vm::Kind::Float && std::bit_cast<std::uint32_t>(result.value.floating)==0u &&
            host.observations==seeds && result.writes==0u,"Direct ConfigureChild read stale payload or did not preserve independent initialized +0");
    }
    {
        TestHost host;const auto native=configure(host);Value source=Value::Float(-12.5f);std::vector<Vm::Evaluation> inputs;
        for(std::size_t slot=0u;slot<4u;++slot) {
            auto alias=std::make_shared<Vm::Reference>();alias->zero=Value::Float(0.0f);
            alias->read=[&,slot] {host.observations.push_back("read"+std::to_string(slot));
                if(slot==0u) return source;
                source=Value::Float(99.0f);return Value::Float(static_cast<float>(slot));};
            inputs.push_back({{},alias});
        }
        host.executionNative=[](const std::string&,const std::vector<Vm::Evaluation>& arguments,const Vm::Function*,Vm::Execution&) {
            for(const auto& argument:arguments) Require(!argument.reference,"Direct ConfigureChild written input kept an alias");
            Require(arguments[1].Load().floating==1.0f && arguments[2].Load().floating==2.0f && arguments[3].Load().floating==3.0f,
                "Direct ConfigureChild input read order changed");return Vm::Evaluation{arguments[0].Load(),{}};
        };
        const auto result=Vm::Execute(host,native,"Self",inputs);Returned(result,"Direct ConfigureChild captures input aliases once in order after all seed hooks");
        auto expected=seeds;for(unsigned slot=0u;slot<4u;++slot) expected.push_back("read"+std::to_string(slot));
        Require(result.value.floating==-12.5f && source.floating==99.0f && host.observations==expected,
            "Direct ConfigureChild reread an earlier source, reordered input reads or repeated seed capture");
    }
    for(const bool direct:{false,true}) for(std::size_t selected=0u;selected<4u;++selected) for(const unsigned bytes:{5u,255u}) {
        TestHost host;const auto native=configure(host);bool read{};
        auto alias=std::make_shared<Vm::Reference>();alias->zero=Value::Float(0.0f);alias->read=[&] {read=true;return Value::Float(99.0f);};
        std::vector<Bytes> inputs(4u,Real(12.5f));inputs[selected]=Context({0x2au},Native(4095u),static_cast<std::uint8_t>(bytes));
        std::vector<Vm::Evaluation> incoming(4u,{Value::Float(12.5f),{}});
        incoming[selected]={Value::Float(99.0f),alias,Vm::ResultDisposition::ZeroFill,static_cast<std::uint8_t>(bytes)};
        const auto caller=Function("ConfigureChildOverflowResult",Return(expression(false,inputs)));
        const auto result=Vm::ExecuteTransaction(host,[&](Vm::Execution& execution) {
            execution.CallFunction(Function("EarlierConfigureChildEffect",Join({Native(1000u),Return()})),"Self");
            try {if(direct) execution.CallFunction(native,"Self",incoming);else execution.CallFunction(caller,"Self");}
            catch(const std::exception&){}return Value{};
        });
        Failed(result,Vm::Status::Unsupported,host,"ConfigureChild cannot clear beyond four-byte result destination; caught failure poisons transaction");
        Require(!read && host.effects==0u && host.rng==12345u && host.nativeIndices==std::vector<std::uint16_t>{1000u},
            "ConfigureChild result overflow read stale storage, entered native body or committed earlier effects");
    }
    for(const bool direct:{false,true}) for(std::size_t selected=0u;selected<4u;++selected) {
        TestHost host;const auto native=configure(host);host.argumentSeed={};bool read{};
        auto alias=std::make_shared<Vm::Reference>();alias->zero=Value::Float(0.0f);alias->read=[&] {read=true;return Value::Float(99.0f);};
        std::vector<Bytes> inputs(4u,Real(12.5f));inputs[selected]=Context({0x2au},Native(4095u),4u);
        std::vector<Vm::Evaluation> incoming(4u,{Value::Float(12.5f),{}});incoming[selected]={Value::Float(99.0f),alias,Vm::ResultDisposition::ZeroFill,4u};
        const auto result=direct?Vm::Execute(host,native,"Self",incoming):
            Vm::Execute(host,Function("ConfigureChildNoGlobalDefault",Return(expression(false,inputs))),"Self");
        Failed(result,Vm::Status::Unsupported,host,"ConfigureChild native index or declaration alone cannot grant initialized result authority");
        Require(!read && host.nativeArguments.empty(),"ConfigureChild inferred a global default, read stale input or entered its native body");
    }
    for(const bool storageBudget:{false,true}) for(std::size_t failedSlot=0u;failedSlot<4u;++failedSlot) {
        TestHost host;const auto native=configure(host);bool read{};
        auto alias=std::make_shared<Vm::Reference>();alias->zero=Value::Float(0.0f);alias->read=[&] {read=true;return Value::Float(12.5f);};
        host.argumentSeed=[&](std::uint16_t index,const std::string&,std::size_t slot,const Vm::Function*)->std::optional<Value> {
            if(index!=1461u) return std::nullopt;
            if(slot==failedSlot) {if(!storageBudget) throw std::runtime_error("ConfigureChild seed lookup failed");
                auto value=Value::Float(0.0f);value.text.reserve(200000u);return value;}
            return Value::Float(0.0f);
        };
        auto limits=Vm::Limits{};if(storageBudget) limits.retainedBytes=100000u;
        const auto result=Vm::ExecuteTransaction(host,[&](Vm::Execution& execution) {
            execution.CallFunction(Function("EarlierConfigureChildSeedEffect",Join({Native(1000u),Return()})),"Self");
            try {execution.CallFunction(native,"Self",std::vector<Vm::Evaluation>(4u,{{},alias}));}
            catch(const std::exception&){}return Value{};
        },limits);
        Failed(result,storageBudget?Vm::Status::Budget:Vm::Status::Unsupported,host,"All ConfigureChild seed capture/budget failures precede any input read");
        Require(!read && host.effects==0u && host.rng==12345u && host.nativeIndices==std::vector<std::uint16_t>{1000u},
            "ConfigureChild seed failure read pending aliases or committed prior effects");
    }
    std::cout<<"Audited Window ConfigureChild input copies/defaults: "<<checks-initialChecks<<" checks and "
        <<rejections-initialRejections<<" refusal controls.\n";
}
void WindowPreferredSizeOutContracts() {
    const auto initialChecks=checks,initialRejections=rejections;
    const auto configure=[](TestHost& host) {
        auto native=Function("QueryPreferredSize",{},"Extension.Window");native.nativeIndex=1465u;native.flags=0x401u;
        native.variables={{"QueryPreferredSize.preferredWidth","preferredWidth",Value::Float(0.0f),0x180u,1u},
            {"QueryPreferredSize.preferredHeight","preferredHeight",Value::Float(0.0f),0x180u,1u}};
        host.finals[{"Fixture",7}]=std::make_shared<Vm::Function>(native);
        host.argumentSeed=[&host](std::uint16_t index,const std::string& receiver,std::size_t slot,
            const Vm::Function* declaration)->std::optional<Value> {
            if(index!=1465u) return std::nullopt;
            Require((receiver=="Self" || receiver=="Other") && slot<2u &&
                (!declaration || declaration->nativeIndex==1465u),"Preferred-size seed lost its original receiver/OUT slot");
            host.observations.push_back("query-seed"+std::to_string(slot));return Value::Float(0.0f);
        };
        return native;
    };
    for(const bool declared:{false,true}) {
        TestHost host;configure(host);
        const auto call=declared?Call(0x1c,7,{Ref(0x01,1),Native(1060u,{Ref(0x01,1),Real(99.0f),Real(23.0f)})}):
            Native(1465u,{Ref(0x01,1),Native(1060u,{Ref(0x01,1),Real(99.0f),Real(23.0f)})});
        const auto caller=Function("PreferredSizeSourceFrameOut",Join({Context(Ref(0x20,9),call),Return(Ref(0x01,1))}));
        const auto source=host.Property(caller,1,"WidthSource",Value::Float(0.0f));
        host.Store("Self",source,{Value::Float(12.0f)});host.Store("Other",source,{Value::Float(8.0f)});
        host.objects[{caller.source,9}]="Other";
        host.executionNative=[&](const std::string& receiver,const std::vector<Vm::Evaluation>& arguments,const Vm::Function* function,Vm::Execution& execution) {
            Require(receiver=="Other" && static_cast<bool>(function)==declared && arguments.size()==2u &&
                arguments[0].reference && !arguments[1].reference,"Preferred-size OUT pointer was detached or receiver lost");
            Require(arguments[0].value.kind==Vm::Kind::Float && arguments[0].value.floating==12.0f &&
                arguments[1].value.floating==23.0f && host.Load("Self",source).floating==99.0f,
                "Preferred-size wrapper reloaded earlier temporary after later argument evaluation");
            static_cast<void>(execution);
            arguments[0].reference->write(Value::Float(42.0f));return Vm::Evaluation{};
        };
        const auto result=Vm::Execute(host,caller,"Self");Returned(result,"Preferred-size raw/resolved OUT aliases retain captured source-frame references");
        Require(result.value.floating==42.0f && host.Load("Self",source).floating==42.0f &&
            host.Load("Other",source).floating==8.0f && result.writes==2u,
            "Preferred-size OUT write targeted receiver instead of source expression or lost side effects; result="+
                std::to_string(result.value.floating)+" self="+std::to_string(host.Load("Self",source).floating)+
                " other="+std::to_string(host.Load("Other",source).floating)+" writes="+std::to_string(result.writes));
    }
    {
        TestHost host;const auto native=configure(host);float first=12.0f,second=23.0f;unsigned firstReads{},secondReads{};
        auto width=std::make_shared<Vm::Reference>(),height=std::make_shared<Vm::Reference>();
        width->zero=height->zero=Value::Float(0.0f);
        width->read=[&] {++firstReads;return Value::Float(first);};width->write=[&](const Value& value) {first=value.floating;};
        height->read=[&] {++secondReads;first=99.0f;return Value::Float(second);};height->write=[&](const Value& value) {second=value.floating;};
        host.executionNative=[&](const std::string&,const std::vector<Vm::Evaluation>& arguments,const Vm::Function*,Vm::Execution& execution) {
            Require(arguments[0].reference && arguments[1].reference && firstReads==1u && secondReads==1u &&
                arguments[0].value.floating==12.0f && arguments[1].value.floating==23.0f && first==99.0f,
                "Direct preferred-size OUT setup read an alias twice or lost sequential copies");
            static_cast<void>(execution);
            arguments[0].reference->write(Value::Float(32.0f));
            arguments[1].reference->write(Value::Float(16.0f));return Vm::Evaluation{};
        };
        const auto result=Vm::Execute(host,native,"Self",{{{},width},{{},height}});
        Returned(result,"Direct preferred-size native receives separate ordinary temporaries and live OUT references");
        // Journal reads precede each real write; they are distinct from the
        // expression copies and intentionally see current target state.
        Require(firstReads==2u && secondReads==2u && second==16.0f && result.writes==2u,
            "Preferred-size output journaling lost its actual reads/writes");
    }
    for(const bool direct:{false,true}) for(const bool declared:{false,true}) for(unsigned selected=0u;selected<2u;++selected)
        for(const auto bytes:{0u,1u,2u,3u,4u}) {
        if(direct && !declared) continue;
        TestHost host;const auto native=configure(host);bool read{};
        auto stale=std::make_shared<Vm::Reference>();stale->zero=Value::Float(0.0f);
        stale->read=[&]() ->Value {read=true;throw std::runtime_error("Absent OUT expression read stale alias");};
        host.executionNative=[&](const std::string&,const std::vector<Vm::Evaluation>& arguments,const Vm::Function*,Vm::Execution&) {
            Require(!arguments[selected].reference && arguments[selected].value.kind==Vm::Kind::Float &&
                std::bit_cast<std::uint32_t>(arguments[selected].value.floating)==0u,
                "Unwritten preferred-size expression fabricated an OUT pointer or lost initialized Float0");
            return Vm::Evaluation{};
        };
        std::vector<Vm::Evaluation> inputs(2u,{Value::Float(9.0f),{}});
        inputs[selected]={Value::Float(99.0f),stale,Vm::ResultDisposition::ZeroFill,static_cast<std::uint8_t>(bytes)};
        const auto zero=Context({0x2au},Native(4095u),static_cast<std::uint8_t>(bytes));
        const auto first=selected==0u?zero:Real(9.0f),second=selected==1u?zero:Real(9.0f);
        const auto caller=Function("PreferredSizeInitializedOutTemporary",Return(declared?Call(0x1c,7,{first,second}):Native(1465u,{first,second})));
        const auto result=direct?Vm::Execute(host,native,"Self",inputs):Vm::Execute(host,caller,"Self");
        Returned(result,"Preferred-size OUT temporary preserves native initialized defaults without inventing aliases");
        Require(!read && result.writes==0u,"Absent preferred-size result read stale target or wrote a temporary externally");
    }
    for(const bool direct:{false,true}) for(const auto bytes:{5u,255u}) {
        TestHost host;const auto native=configure(host);bool read{};
        auto stale=std::make_shared<Vm::Reference>();stale->zero=Value::Float(0.0f);stale->read=[&] {read=true;return Value::Float(99.0f);};
        const auto caller=Function("PreferredSizeOutOverflow",Return(Native(1465u,
            {Context({0x2au},Native(4095u),static_cast<std::uint8_t>(bytes)),Real(0.0f)})));
        const auto result=Vm::ExecuteTransaction(host,[&](Vm::Execution& execution) {
            execution.CallFunction(Function("EarlierOutSetupEffect",Join({Native(1000u),Return()})),"Self");
            try {
                if(direct) execution.CallFunction(native,"Self",{{Value::Float(99.0f),stale,Vm::ResultDisposition::ZeroFill,static_cast<std::uint8_t>(bytes)},{Value::Float(0.0f),{}}});
                else execution.CallFunction(caller,"Self");
            }catch(const std::exception&){}return Value{};
        });
        Failed(result,Vm::Status::Unsupported,host,"Preferred-size OUT temporary refuses overflow and poisons outer transaction");
        Require(!read && host.effects==0u,"Preferred-size setup overflow read stale alias or escaped rollback");
    }
    for(const bool laterFailure:{false,true}) {
        TestHost host;const auto native=configure(host);float width=1.0f,height=2.0f;
        auto w=std::make_shared<Vm::Reference>(),h=std::make_shared<Vm::Reference>();w->zero=h->zero=Value::Float(0.0f);
        w->read=[&] {return Value::Float(width);};w->write=[&](const Value& value) {width=value.floating;};
        h->read=[&] {return Value::Float(height);};h->write=[&](const Value& value) {height=value.floating;};
        std::shared_ptr<Vm::Reference> retained;
        host.executionNative=[&](const std::string&,const std::vector<Vm::Evaluation>& arguments,const Vm::Function*,Vm::Execution& execution) {
            static_cast<void>(execution);
            retained=arguments[0].reference;arguments[0].reference->write(Value::Float(32.0f));
            if(!laterFailure) throw std::runtime_error("Native output failure after first store");
            arguments[1].reference->write(Value::Float(16.0f));return Vm::Evaluation{};
        };
        const auto result=Vm::ExecuteTransaction(host,[&](Vm::Execution& execution) {
            try {execution.CallFunction(native,"Self",{{{},w},{{},h}});}
            catch(const std::exception&){}if(laterFailure) execution.CallFunction(Function("LaterOutFailure",Return(Native(4095u))),"Self");return Value{};
        });
        Failed(result,Vm::Status::Unsupported,host,"Native preferred-size output failure rolls back earlier target stores");
        Require(width==1.0f && height==2.0f,"Preferred-size OUT writes escaped failed transaction");
        bool expired{};try {retained->write(Value::Float(88.0f));}catch(const std::exception&) {expired=true;}
        Require(expired && width==1.0f,"Preferred-size native retained usable output guard after execution expired");
    }
    std::cout<<"Preferred-size native copied OUT temporaries/references: "<<checks-initialChecks<<" checks and "
        <<rejections-initialRejections<<" refusal controls.\n";
}
void GcTextNativeInputContracts() {
    const auto initialChecks=checks,initialRejections=rejections;
    // These fixtures reproduce the audited Extension.u declarations. The host
    // grants initialized temporary authority only after matching that identity;
    // the VM native number itself is never a source of these defaults.
    const auto configure=[](TestHost& host,std::uint16_t index) {
        auto native=Function(index==1242u?"SetFont":index==1280u?"GetTextExtent":
            index==1281u?"GetFontHeight":"ConvertScriptString",{},index==1469u?"Extension.Window":"Extension.GC");
        native.nativeIndex=index;native.flags=0x401u;
        const auto parameter=[&](const std::string& name,Value zero,std::uint32_t flags) {
            native.variables.push_back({native.path+"."+name,name,std::move(zero),flags,1u});
        };
        if(index==1242u) parameter("NewFont",Value::Text(Vm::Kind::Object,{}),0x80u);
        else if(index==1280u) {
            parameter("destWidth",Value::Float(0.0f),0x80u);
            parameter("xExtent",Value::Float(0.0f),0x180u);
            parameter("yExtent",Value::Float(0.0f),0x180u);
            parameter("textStr",Value::Text(Vm::Kind::String,{}),0x400880u);
        } else if(index==1281u) {
            parameter("bIncludeSpace",Value::Bool(false),0x90u);
            parameter("ReturnValue",Value::Float(0.0f),0x580u);
        } else {
            parameter("oldStr",Value::Text(Vm::Kind::String,{}),0x400080u);
            parameter("ReturnValue",Value::Text(Vm::Kind::String,{}),0x400580u);
        }
        host.finals[{"Fixture",7}]=std::make_shared<Vm::Function>(native);
        host.argumentSeed=[native](std::uint16_t actual,const std::string& receiver,std::size_t slot,
            const Vm::Function* declaration)->std::optional<Value> {
            if(actual!=native.nativeIndex) return std::nullopt;
            Require(receiver=="Self"||receiver=="Other","GC text seed changed its receiver identity");
            if(declaration) {
                Require(declaration->path==native.path && declaration->source==native.source &&
                    declaration->nativeIndex==native.nativeIndex && declaration->flags==native.flags &&
                    declaration->variables.size()==native.variables.size(),"GC text seed accepted a foreign declaration");
                for(std::size_t i=0;i<native.variables.size();++i) {
                    const auto& supplied=declaration->variables[i];const auto& expected=native.variables[i];
                    Require(supplied.key==expected.key && supplied.name==expected.name && supplied.flags==expected.flags &&
                        supplied.arrayDimension==1u && supplied.zero.kind==expected.zero.kind,
                        "GC text seed accepted a changed parameter identity/layout");
                }
            }
            const auto count=native.nativeIndex==1280u?4u:1u;
            Require(slot<count,"GC text seed requested a return/excess parameter");
            return native.variables[slot].zero;
        };
        return native;
    };
    const auto call=[](std::uint16_t index,bool declared,const std::vector<Bytes>& inputs) {
        auto expression=declared?Call(0x1cu,7):Native(index);expression.pop_back();
        for(const auto& input:inputs) Append(expression,input);
        expression.push_back(0x16u);return expression;
    };
    for(const auto index:{std::uint16_t{1242u},std::uint16_t{1281u},std::uint16_t{1469u}})
        for(unsigned route=0u;route<3u;++route) for(const bool lateFailure:{false,true}) {
        TestHost host;const auto native=configure(host,index);
        const auto original=index==1242u?Value::Text(Vm::Kind::Object,"OriginalFont"):
            index==1281u?Value::Bool(true):Value::Text(Vm::Kind::String,"Original|nText");
        const auto replacement=index==1242u?Value::Text(Vm::Kind::Object,"OtherFont"):
            index==1281u?Value::Bool(false):Value::Text(Vm::Kind::String,"Changed");
        const auto expression=Context(Ref(0x20u,9),call(index,route==1u,{Ref(0x01u,1)}));
        const auto caller=Function("GcTextSingleCopiedSourceFrame",Return(expression));
        const auto property=host.Property(caller,1,"SourceInput",native.variables[0].zero);
        host.Store("Self",property,{original});host.Store("Other",property,{replacement});
        host.objects[{caller.source,9}]="Other";
        bool entered{};
        host.executionNative=[&](const std::string& receiver,const std::vector<Vm::Evaluation>& args,
            const Vm::Function* actual,Vm::Execution& execution) {
            entered=true;
            Require(receiver=="Other" && static_cast<bool>(actual)==(route!=0u) && args.size()==1u && !args[0].reference &&
                Vm::Equal(args[0].value,original) && host.referenceReads==1u,
                "GC text ordinary Step lost its one-read copied source or receiver/declaration");
            execution.WriteReference(host.Reference({"Self",Vm::Scope::Instance,property.key},property),replacement);
            Require(Vm::Equal(args[0].Load(),original),"GC text copied input changed after its source was written");
            return Vm::Evaluation{args[0].value,{}};
        };
        const auto result=Vm::ExecuteTransaction(host,[&](Vm::Execution& execution) {
            execution.CallFunction(Function("EarlierGcTextEffect",Join({Native(1000u),Return()})),"Self");
            const auto value=route==2u?execution.CallFunction(native,"Other",
                {{{},host.Reference({"Self",Vm::Scope::Instance,property.key},property)}}):execution.CallFunction(caller,"Self");
            if(lateFailure) execution.CallFunction(Function("LaterGcTextFailure",Return(Native(4095u))),"Self");
            return value;
        });
        if(lateFailure) {
            Failed(result,Vm::Status::Unsupported,host,"GC text input copy rolls back a later failure");
            Require(entered && Vm::Equal(host.Load("Self",property),original) && host.effects==0u && host.rng==12345u,
                "GC text late failure retained copied-input mutation or prefix effect/RNG");
        } else {
            Returned(result,"GC text raw/resolved/direct wrappers copy each ordinary input once");
            Require(entered && Vm::Equal(result.value,original) && Vm::Equal(host.Load("Self",property),replacement) &&
                Vm::Equal(host.Load("Other",property),replacement) && result.writes==1u && host.effects==1u,
                "GC text ordinary copied input lost its committed native write");
        }
    }
    for(const bool declared:{false,true}) {
        TestHost host;configure(host,1280u);
        const auto mutateWidth=Native(1060u,{Ref(0x01u,1),Real(99.0f),Real(12.0f)});
        const auto mutateX=Native(1060u,{Ref(0x01u,2),Real(88.0f),String("A|nB")});
        const auto caller=Function("GcExtentSequentialSteps",Return(Context(Ref(0x20u,9),
            call(1280u,declared,{Ref(0x01u,1),mutateWidth,Ref(0x01u,2),mutateX}))));
        const auto width=host.Property(caller,1,"Width",Value::Float(0.0f));
        const auto output=host.Property(caller,2,"Output",Value::Float(0.0f));
        host.Store("Self",width,{Value::Float(40.0f)});host.Store("Self",output,{Value::Float(17.0f)});
        host.Store("Other",width,{Value::Float(3.0f)});host.Store("Other",output,{Value::Float(4.0f)});
        host.objects[{caller.source,9}]="Other";
        host.executionNative=[&](const std::string& receiver,const std::vector<Vm::Evaluation>& args,
            const Vm::Function* actual,Vm::Execution&) {
            Require(receiver=="Other" && static_cast<bool>(actual)==declared && args.size()==4u &&
                !args[0].reference && !args[1].reference && args[2].reference && !args[3].reference &&
                args[0].value.floating==40.0f && args[1].value.floating==12.0f && args[2].value.floating==17.0f &&
                args[3].value.kind==Vm::Kind::String && args[3].value.text=="A|nB" && host.Load("Self",width).floating==99.0f &&
                host.Load("Self",output).floating==88.0f,
                "GC extent later Steps reloaded prior width/OUT copies or changed caller-frame writes");
            args[2].reference->write(Value::Float(64.0f));return Vm::Evaluation{};
        };
        const auto result=Vm::Execute(host,caller,"Self");Returned(result,"GC extent snapshots each Step before evaluating later inputs");
        Require(host.Load("Self",output).floating==64.0f && host.Load("Other",output).floating==4.0f && result.writes==3u,
            "GC extent captured OUT pointer wrote receiver storage or lost earlier real effects");
    }
    for(unsigned outputForm=0u;outputForm<5u;++outputForm) for(const bool lateFailure:{false,true}) {
        TestHost host;const auto native=configure(host,1280u);
        float width=40.0f,x=12.0f,y=23.0f;std::string text="A|nB";
        unsigned widthReads{},xReads{},yReads{},textReads{};std::vector<unsigned> stores;
        auto w=std::make_shared<Vm::Reference>(),xr=std::make_shared<Vm::Reference>(),
            yr=std::make_shared<Vm::Reference>(),str=std::make_shared<Vm::Reference>();
        w->zero=xr->zero=yr->zero=Value::Float(0.0f);str->zero=Value::Text(Vm::Kind::String,{});
        w->read=[&] {++widthReads;return Value::Float(width);};
        xr->read=[&] {++xReads;width=99.0f;return Value::Float(x);};
        yr->read=[&] {++yReads;return Value::Float(y);};
        str->read=[&] {++textReads;return Value::Text(Vm::Kind::String,text);};
        xr->write=[&](const Value& v) {stores.push_back(1u);x=v.floating;};
        yr->write=[&](const Value& v) {stores.push_back(2u);y=v.floating;};
        const auto first=outputForm==2u||outputForm==4u?std::shared_ptr<Vm::Reference>{}:xr;
        const auto second=outputForm==3u||outputForm==4u?std::shared_ptr<Vm::Reference>{}:outputForm==1u?xr:yr;
        std::shared_ptr<Vm::Reference> retained;
        host.executionNative=[&](const std::string&,const std::vector<Vm::Evaluation>& args,const Vm::Function*,Vm::Execution&) {
            Require(args.size()==4u && !args[0].reference && !args[3].reference &&
                static_cast<bool>(args[1].reference)==static_cast<bool>(first) &&
                static_cast<bool>(args[2].reference)==static_cast<bool>(second) &&
                args[0].value.floating==40.0f && args[3].value.text=="A|nB" && widthReads==1u && textReads==1u &&
                xReads==(static_cast<unsigned>(static_cast<bool>(first))+static_cast<unsigned>(second==xr)) &&
                yReads==static_cast<unsigned>(second==yr),
                "Direct GC extent copies/OUT aliases performed duplicate loads or retained ordinary aliases");
            text="changed after Step";
            Require(args[3].value.text=="A|nB","GC extent String was not an independent caller-frame copy");
            if(args[1].reference) {retained=args[1].reference;args[1].reference->write(Value::Float(32.0f));}
            if(args[2].reference) {if(!retained) retained=args[2].reference;args[2].reference->write(Value::Float(16.0f));}
            return Vm::Evaluation{};
        };
        const auto result=Vm::ExecuteTransaction(host,[&](Vm::Execution& execution) {
            execution.CallFunction(native,"Self",{{{},w},{Value::Float(12.0f),first},
                {Value::Float(23.0f),second},{{},str}});
            if(lateFailure) execution.CallFunction(Function("GcExtentLateFailure",Return(Native(4095u))),"Self");
            return Value{};
        });
        if(lateFailure) {
            Failed(result,Vm::Status::Unsupported,host,"GC extent OUT aliases roll back late failure");
            Require(x==12.0f && y==23.0f,"GC extent shared/distinct OUT changes escaped rollback");
        } else {
            Returned(result,"GC extent direct outputs preserve ordered shared/distinct/discarded references");
            const std::vector<unsigned> expected=outputForm==0u?std::vector<unsigned>{1u,2u}:
                outputForm==1u?std::vector<unsigned>{1u,1u}:outputForm==2u?std::vector<unsigned>{2u}:
                outputForm==3u?std::vector<unsigned>{1u}:std::vector<unsigned>{};
            Require(stores==expected && result.writes==expected.size() &&
                x==(outputForm==1u?16.0f:first?32.0f:12.0f) && y==(second==yr?16.0f:23.0f),
                "GC extent OUT stores lost X-before-Y order, shared-alias last write or discarded output independence");
        }
        if(retained) {
            const auto before=x;bool expired{};try {retained->write(Value::Float(88.0f));}catch(const std::exception&) {expired=true;}
            Require(expired && x==before,"GC extent retained a writable OUT guard after its execution expired");
        }
    }
    for(const auto index:{std::uint16_t{1242u},std::uint16_t{1280u},std::uint16_t{1281u},std::uint16_t{1469u}})
        for(unsigned route=0u;route<3u;++route) for(unsigned selected=0u;selected<(index==1280u?4u:1u);++selected)
        for(const bool noWrite:{false,true}) {
        TestHost host;const auto native=configure(host,index);bool read{},entered{};
        auto stale=std::make_shared<Vm::Reference>();stale->zero=native.variables[selected].zero;
        stale->read=[&]() ->Value {read=true;throw std::runtime_error("GC text absent input read stale alias");};
        std::vector<Vm::Evaluation> inputs;std::vector<Bytes> expressions;
        const auto count=index==1280u?4u:1u;
        for(unsigned slot=0u;slot<count;++slot) {
            const auto zero=native.variables[slot].zero;
            inputs.push_back({zero,{}});
            expressions.push_back(zero.kind==Vm::Kind::String?String("A|nB"):zero.kind==Vm::Kind::Object?Bytes{0x2au}:
                zero.kind==Vm::Kind::Bool?Bytes{0x27u}:Real(17.0f));
        }
        inputs[selected]={Value::Text(Vm::Kind::String,"stale"),stale,
            noWrite?Vm::ResultDisposition::NoWrite:Vm::ResultDisposition::ZeroFill,0u};
        expressions[selected]=Context({0x2au},Native(4095u),0u);
        host.executionNative=[&](const std::string&,const std::vector<Vm::Evaluation>& args,const Vm::Function*,Vm::Execution&) {
            entered=true;
            Require(args.size()==count && !args[selected].reference &&
                Vm::Equal(args[selected].value,native.variables[selected].zero),
                "GC text initialized temporary lost its authenticated default or kept an unwritten OUT alias");
            return Vm::Evaluation{};
        };
        const auto caller=Function("GcTextInitializedNoWrite",Return(call(index,route==1u,expressions)));
        const auto result=route==2u?Vm::Execute(host,native,"Self",inputs):Vm::Execute(host,caller,"Self");
        Returned(result,"GC text NoWrite/zero-byte result uses its authenticated initialized temporary");
        Require(entered && !read && result.writes==0u,"GC text absence read stale input, invented OUT stores or skipped native body");
    }
    for(unsigned route=0u;route<3u;++route) for(const bool explicitNothing:{false,true}) {
        TestHost host;const auto native=configure(host,1281u);bool entered{};
        host.executionNative=[&](const std::string&,const std::vector<Vm::Evaluation>& args,const Vm::Function*,Vm::Execution&) {
            entered=true;
            Require(args.empty()||(args.size()==1u&&!args.front().reference&&args.front().value.kind==Vm::Kind::Nothing),
                "Optional GetFontHeight input invented a true default or an input alias");
            return Vm::Evaluation{Value::Float(13.0f),{}};
        };
        const auto caller=Function("GcFontHeightOptionalFalse",Return(call(1281u,route==1u,
            explicitNothing?std::vector<Bytes>{Bytes{0x0bu}}:std::vector<Bytes>{})));
        const auto result=route==2u?Vm::Execute(host,native,"Self",explicitNothing?
            std::vector<Vm::Evaluation>{Vm::Evaluation{}}:std::vector<Vm::Evaluation>{}):Vm::Execute(host,caller,"Self");
        Returned(result,"GetFontHeight accepts genuinely omitted/default-false optional input");
        Require(entered&&result.value.floating==13.0f&&result.writes==0u,"Optional font height bypassed its body or mutated state");
    }
    for(const bool direct:{false,true}) for(const auto selected:{0u,1u,2u,3u}) for(const auto bytes:{1u,2u,3u,4u,5u,12u,13u,255u}) {
        TestHost host;const auto native=configure(host,1280u);bool read{},entered{};
        auto stale=std::make_shared<Vm::Reference>();stale->zero=native.variables[selected].zero;
        stale->read=[&]() ->Value {read=true;throw std::runtime_error("GC text zero-fill read a stale alias");};
        std::vector<Vm::Evaluation> inputs={{Value::Float(40.0f),{}},{Value::Float(0.0f),{}},
            {Value::Float(0.0f),{}},{Value::Text(Vm::Kind::String,"A"),{}}};
        inputs[selected]={native.variables[selected].zero,stale,Vm::ResultDisposition::ZeroFill,static_cast<std::uint8_t>(bytes)};
        std::vector<Bytes> expressions={Real(40.0f),Real(0.0f),Real(0.0f),String("A")};
        expressions[selected]=Context({0x2au},Native(4095u),static_cast<std::uint8_t>(bytes));
        host.executionNative=[&](const std::string&,const std::vector<Vm::Evaluation>& args,const Vm::Function*,Vm::Execution&) {
            entered=true;
            Require(!args[selected].reference&&Vm::Equal(args[selected].value,native.variables[selected].zero),
                "GC extent initialized clear did not retain its actual zero temporary");return Vm::Evaluation{};
        };
        const auto caller=Function("GcExtentInitializedClearBounds",Return(call(1280u,!direct,expressions)));
        const auto result=direct?Vm::Execute(host,native,"Self",inputs):Vm::Execute(host,caller,"Self");
        const bool accepted=selected==3u?bytes==12u:bytes<=4u;
        if(accepted)Returned(result,"GC extent partial Float/full FString result clear uses authenticated initialized storage");
        else Failed(result,Vm::Status::Unsupported,host,"GC extent refuses partial FString and overflowing result clears");
        Require(!read&&entered==accepted&&result.writes==0u,"GC extent invalid result clear read stale state or entered native body");
    }
    for(const bool direct:{false,true}) for(const auto selected:{0u,1u,2u,3u}) {
        TestHost host;const auto native=configure(host,1280u);host.argumentSeed={};bool read{};
        auto stale=std::make_shared<Vm::Reference>();stale->zero=native.variables[selected].zero;
        stale->read=[&] {read=true;return native.variables[selected].zero;};
        const auto bytes=selected==3u?1u:3u;
        std::vector<Vm::Evaluation> inputs={{Value::Float(40.0f),{}},{Value::Float(0.0f),{}},
            {Value::Float(0.0f),{}},{Value::Text(Vm::Kind::String,"A"),{}}};
        inputs[selected]={native.variables[selected].zero,stale,Vm::ResultDisposition::ZeroFill,static_cast<std::uint8_t>(bytes)};
        std::vector<Bytes> expressions={Real(40.0f),Real(0.0f),Real(0.0f),String("A")};
        expressions[selected]=Context({0x2au},Native(4095u),static_cast<std::uint8_t>(bytes));
        const auto caller=Function("GcExtentNoGlobalSeed",Return(call(1280u,!direct,expressions)));
        const auto result=direct?Vm::Execute(host,native,"Self",inputs):Vm::Execute(host,caller,"Self");
        Failed(result,Vm::Status::Unsupported,host,"GC extent declaration/native index alone cannot grant initialized partial-write authority");
        Require(!read && host.nativeIndices.empty(),"GC extent inferred a global native seed or entered its body after unsupported partial result");
    }
    for(const bool direct:{false,true}) for(unsigned failedSlot=0u;failedSlot<4u;++failedSlot) {
        TestHost host;const auto native=configure(host,1280u);const auto authenticatedSeed=host.argumentSeed;bool read{};
        host.argumentSeed=[&](std::uint16_t index,const std::string& receiver,std::size_t slot,
            const Vm::Function* declaration)->std::optional<Value> {
            if(index==1280u && slot==failedSlot) throw std::runtime_error("GC extent authenticated seed lookup failed");
            return authenticatedSeed(index,receiver,slot,declaration);
        };
        auto stale=std::make_shared<Vm::Reference>();stale->zero=Value::Float(0.0f);
        stale->read=[&] {read=true;return Value::Float(99.0f);};
        const auto caller=Function("GcExtentSeedBeforeEffects",Return(call(1280u,false,
            {Native(1000u),Real(0.0f),Real(0.0f),String("A")})));
        const auto result=Vm::ExecuteTransaction(host,[&](Vm::Execution& execution) {
            execution.CallFunction(Function("EarlierGcExtentSeedEffect",Join({Native(1000u),Return()})),"Self");
            try {
                if(direct) execution.CallFunction(native,"Self",{{{},stale},{Value::Float(0.0f),{}},
                    {Value::Float(0.0f),{}},{Value::Text(Vm::Kind::String,"A"),{}}});
                else execution.CallFunction(caller,"Self");
            }catch(const std::exception&) {}return Value{};
        });
        Failed(result,Vm::Status::Unsupported,host,"GC extent seed failure precedes all input reads/effects and poisons caught outer root");
        Require(!read && host.effects==0u && host.rng==12345u && host.nativeIndices==std::vector<std::uint16_t>{1000u},
            "GC extent failed seed allowed pending inputs or prefix effects to commit");
    }
    std::cout<<"GC font/text native copied inputs and captured extent outputs: "<<checks-initialChecks<<" checks and "
        <<rejections-initialRejections<<" refusal controls.\n";
}
void WindowWidthInputContracts() {
    const auto initialChecks=checks,initialRejections=rejections;
    const auto configure=[](TestHost& host) {
        auto native=Function("SetWidth",{},"Extension.Window");native.nativeIndex=1433u;native.flags=0x401u;
        native.variables={{"SetWidth.newWidth","newWidth",Value::Float(0.0f),0x80u,1u}};
        host.finals[{"Fixture",7}]=std::make_shared<Vm::Function>(native);
        host.argumentSeed=[&host](std::uint16_t index,const std::string&,std::size_t slot,const Vm::Function*)->std::optional<Value> {
            if(index!=1433u) return std::nullopt;
            Require(slot==0u,"SetWidth seeded an excess result slot");host.observations.push_back("width-seed");return Value::Float(0.0f);
        };
        host.executionNative=[](const std::string&,const std::vector<Vm::Evaluation>& arguments,const Vm::Function*,Vm::Execution&) {
            Require(arguments.size()==1u && !arguments[0].reference,"SetWidth did not copy its one ordinary Float input");
            const auto value=arguments[0].Load();
            return Vm::Evaluation{value.kind==Vm::Kind::Nothing?Value::Float(0.0f):value,{}};
        };
        return native;
    };
    for(const bool declared:{false,true}) for(const float input:{-51.0f,-0.0f,0.0f,44.0f,std::numeric_limits<float>::denorm_min()}) {
        TestHost host;configure(host);
        const auto caller=Function("SetWidthInputBits",Return(declared?Call(0x1c,7,{Real(input)}):Native(1433u,{Real(input)})));
        const auto result=Vm::Execute(host,caller,"Self");Returned(result,"SetWidth bridge preserves Float bits before actual runtime clamping");
        Require(result.value.kind==Vm::Kind::Float && std::bit_cast<std::uint32_t>(result.value.floating)==std::bit_cast<std::uint32_t>(input) &&
            host.observations==std::vector<std::string>{"width-seed"},"SetWidth bridge clamped input, lost signed zero or duplicated hooks");
    }
    for(const bool declared:{false,true}) {
        TestHost host;configure(host);
        const auto caller=Function("SetWidthInputSourceCopy",Return(declared?Call(0x1c,7,{Native(1007u,{Ref(0x01,1)})}):
            Native(1433u,{Native(1007u,{Ref(0x01,1)})})));
        const auto source=host.Property(caller,1,"WidthSource",Value::Float(0));host.Store("Self",source,{Value::Float(44)});
        host.executionNative=[&](const std::string&,const std::vector<Vm::Evaluation>& arguments,const Vm::Function*,Vm::Execution& execution) {
            Require(!arguments[0].reference && arguments[0].Load().floating==44.0f,"SetWidth input retained its source alias");
            execution.WriteReference(host.Reference({"Self",Vm::Scope::Instance,source.key},source),Value::Float(99));
            Require(arguments[0].Load().floating==44.0f,"SetWidth reread input after source changed in native body");
            return Vm::Evaluation{arguments[0].Load(),{}};
        };
        const auto result=Vm::Execute(host,caller,"Self");Returned(result,"SetWidth copied argument survives genuine later source writes");
        Require(result.value.floating==44.0f && host.Load("Self",source).floating==99.0f && result.writes==1u &&
            host.observations==std::vector<std::string>{"width-seed","variable:Self:WidthSource"},
            "SetWidth repeated source expression or lost native-body source mutation");
    }
    for(const auto disposition:{Vm::ResultDisposition::NoWrite,Vm::ResultDisposition::ZeroFill}) for(const auto bytes:{0u,1u,2u,3u,4u}) {
        TestHost host;const auto native=configure(host);bool read{};
        auto alias=std::make_shared<Vm::Reference>();alias->zero=Value::Float(0);
        alias->read=[&]() ->Value {read=true;throw std::runtime_error("SetWidth unwritten argument read stale storage");};
        const auto result=Vm::Execute(host,native,"Self",{{Value::Float(999),alias,disposition,static_cast<std::uint8_t>(bytes)}});
        Returned(result,"Direct SetWidth initialized Float0 handles NoWrite/full/partial clearing before snapshots");
        Require(!read && result.value.kind==Vm::Kind::Float && std::bit_cast<std::uint32_t>(result.value.floating)==0u &&
            host.observations==std::vector<std::string>{"width-seed"},"SetWidth used stale alias/declaration data or duplicated its seed");
    }
    for(const bool direct:{false,true}) for(const auto bytes:{5u,255u}) {
        TestHost host;const auto native=configure(host);bool read{};
        auto alias=std::make_shared<Vm::Reference>();alias->zero=Value::Float(0);alias->read=[&] {read=true;return Value::Float(999);};
        const auto caller=Function("SetWidthOverflowResult",Return(Native(1433u,{Context({0x2au},Native(4095u),static_cast<std::uint8_t>(bytes))})));
        const auto result=Vm::ExecuteTransaction(host,[&](Vm::Execution& execution) {
            execution.CallFunction(Function("EarlierWidthEffect",Join({Native(1000u),Return()})),"Self");
            try {if(direct) execution.CallFunction(native,"Self",{{Value::Float(999),alias,Vm::ResultDisposition::ZeroFill,static_cast<std::uint8_t>(bytes)}});
                else execution.CallFunction(caller,"Self");}catch(const std::exception&){}return Value{};
        });
        Failed(result,Vm::Status::Unsupported,host,"SetWidth initialized result cannot be cleared past four bytes");
        Require(!read && host.effects==0u && host.rng==12345u && host.nativeIndices==std::vector<std::uint16_t>{1000u},
            "SetWidth overflow failure read stale input, reached setter or committed its earlier transaction");
    }
    for(const bool direct:{false,true}) {
        TestHost host;const auto native=configure(host);host.argumentSeed={};
        const auto caller=Function("SetWidthNoGlobalDefault",Return(Native(1433u,{Context({0x2au},Native(4095u),4u)})));
        const auto result=direct?Vm::Execute(host,native,"Self",{{Value::Float(99),{},Vm::ResultDisposition::ZeroFill,4u}}):Vm::Execute(host,caller,"Self");
        Failed(result,Vm::Status::Unsupported,host,"SetWidth scalar default requires runtime-owned seed authority, not native-index guessing");
        Require(host.nativeArguments.empty(),"SetWidth entered native setter without an audited initialized result destination");
    }
    std::cout<<"Audited Window width input copies/defaults: "<<checks-initialChecks<<" checks and "
        <<rejections-initialRejections<<" refusal controls.\n";
}
void WindowSelectabilityInputContracts() {
    const auto initialChecks=checks,initialRejections=rejections;
    const auto configure=[](TestHost& host) {
        auto native=Function("SetSelectability",{},"Extension.Window");native.nativeIndex=1420u;native.flags=0x401u;
        native.variables={{"SetSelectability.newSelectability","newSelectability",Value::Bool(false),0x80u,1u}};
        host.finals[{"Fixture",7}]=std::make_shared<Vm::Function>(native);
        host.argumentSeed=[&host](std::uint16_t index,const std::string& receiver,std::size_t slot,
            const Vm::Function* declaration)->std::optional<Value> {
            if(index!=1420u) return std::nullopt;
            Require(slot==0u && (receiver=="Self" || receiver=="Other") &&
                (!declaration || declaration->path=="Extension.Window.SetSelectability"),
                "Selectability Bool seed lost audited receiver/slot/declaration");
            host.observations.push_back("selectability-seed");return Value::Bool(false);
        };
        host.executionNative=[](const std::string&,const std::vector<Vm::Evaluation>& arguments,const Vm::Function*,Vm::Execution&) {
            Require(arguments.size()==1u && !arguments[0].reference,"Selectability ordinary Bool input lost its one copied result");
            const auto value=arguments[0].Load();
            Require(value.kind==Vm::Kind::Bool || value.kind==Vm::Kind::Nothing,"Selectability mock received non-Bool input");
            return Vm::Evaluation{value.kind==Vm::Kind::Nothing?Value::Bool(false):value,{}};
        };
        return native;
    };
    for(const bool declared:{false,true}) for(const auto& [input,expected]:std::vector<std::pair<Bytes,bool>>{
        {{0x0bu},false},{{0x27u},true},{{0x28u},false},{Context({0x2au},Native(4095u),0u),false},
        {Context({0x2au},Native(4095u),4u),false}}) {
        TestHost host;configure(host);
        const auto caller=Function("SelectabilityRuntimeInitializedBool",Return(declared?Call(0x1c,7,{input}):Native(1420u,{input})));
        const auto result=Vm::Execute(host,caller,"Self");Returned(result,"Selectability runtime-owned Bool0 temporary input contract");
        Require(result.value.kind==Vm::Kind::Bool && result.value.boolean==expected && result.writes==0u &&
            host.observations==std::vector<std::string>{"selectability-seed"},
            "Selectability input default changed or native seed repeated at internal Run entry");
        Require(host.nativeArguments[0][0].kind==(input==Bytes{0x0bu}?Vm::Kind::Nothing:Vm::Kind::Bool),
            "Selectability input bridge consumed the expression's explicit Nothing marker");
    }
    for(const bool declared:{false,true}) {
        TestHost host;configure(host);
        const auto caller=Function("SelectabilitySourceFrameCopy",Return(Context(Ref(0x20,9),
            declared?Call(0x1c,7,{Native(1007u,{Ref(0x01,1)})}):Native(1420u,{Native(1007u,{Ref(0x01,1)})}))));
        const auto source=host.Property(caller,1,"SelectableInput",Value::Bool(false));
        host.Store("Self",source,{Value::Bool(true)});host.Store("Other",source,{Value::Bool(false)});host.objects[{caller.source,9}]="Other";
        host.executionNative=[&](const std::string& receiver,const std::vector<Vm::Evaluation>& arguments,const Vm::Function* actual,Vm::Execution& execution) {
            Require(receiver=="Other" && static_cast<bool>(actual)==declared && !arguments[0].reference && arguments[0].Load().boolean,
                "Selectability argument used native receiver instead of source FFrame.Object or retained its alias");
            execution.WriteReference(host.Reference({"Self",Vm::Scope::Instance,source.key},source),Value::Bool(false));
            Require(arguments[0].Load().boolean,"Selectability copied input reloaded source after native body mutation");
            return Vm::Evaluation{arguments[0].Load(),{}};
        };
        const auto result=Vm::Execute(host,caller,"Self");Returned(result,"Selectability copies actual source-frame input before native work");
        Require(result.value.boolean && !host.Load("Self",source).boolean && result.writes==1u &&
            host.observations==std::vector<std::string>{"selectability-seed","variable:Self:SelectableInput"},
            "Selectability repeated its source/hook or suppressed the real later mutation");
    }
    for(const bool written:{false,true}) for(const auto disposition:{Vm::ResultDisposition::NoWrite,Vm::ResultDisposition::ZeroFill})
        for(const auto bytes:{0u,4u}) {
        TestHost host;const auto native=configure(host);bool read{};
        auto alias=std::make_shared<Vm::Reference>();alias->zero=Value::Bool(false);
        alias->read=[&]() ->Value {read=true;if(!written) throw std::runtime_error("Unwritten selectability alias read stale storage");return Value::Bool(true);};
        const Vm::Evaluation argument{Value::Bool(true),alias,written?Vm::ResultDisposition::Written:disposition,static_cast<std::uint8_t>(bytes)};
        const auto result=Vm::Execute(host,native,"Self",{argument});Returned(result,"Direct selectability copies written results and materializes unwritten Bool0 without stale alias reads");
        Require(read==written && result.value.kind==Vm::Kind::Bool && result.value.boolean==written &&
            host.observations==std::vector<std::string>{"selectability-seed"},
            "Direct selectability consumed stale payload, repeated reads/default hooks or lost written input");
    }
    for(const bool direct:{false,true}) for(const auto bytes:{1u,2u,3u,5u,255u}) {
        TestHost host;const auto native=configure(host);bool read{};
        auto alias=std::make_shared<Vm::Reference>();alias->zero=Value::Bool(false);
        alias->read=[&] {read=true;return Value::Bool(true);};
        const auto caller=Function("SelectabilityUnsupportedBoolByteWrites",Return(Native(1420u,
            {Context({0x2au},Native(4095u),static_cast<std::uint8_t>(bytes))})));
        const auto result=Vm::ExecuteTransaction(host,[&](Vm::Execution& execution) {
            execution.CallFunction(Function("EarlierSelectabilityEffect",Join({Native(1000u),Return()})),"Self");
            try {
                if(direct) execution.CallFunction(native,"Self",{{Value::Bool(true),alias,Vm::ResultDisposition::ZeroFill,static_cast<std::uint8_t>(bytes)}});
                else execution.CallFunction(caller,"Self");
            }catch(const std::exception&){}return Value{};
        });
        Failed(result,Vm::Status::Unsupported,host,"Selectability partial/overflow packed Bool clearing remains explicit refusal");
        Require(!read && host.effects==0u && host.rng==12345u && host.nativeIndices==std::vector<std::uint16_t>{1000u},
            "Partial Bool refusal read stale storage, reached native setter or committed earlier effects");
    }
    for(const bool direct:{false,true}) {
        TestHost host;const auto native=configure(host);host.argumentSeed={};bool read{};
        auto alias=std::make_shared<Vm::Reference>();alias->zero=Value::Bool(false);alias->read=[&] {read=true;return Value::Bool(true);};
        const auto caller=Function("SelectabilityNoGlobalDefault",Return(Native(1420u,{Context({0x2au},Native(4095u),4u)})));
        const auto result=direct?Vm::Execute(host,native,"Self",{{Value::Bool(true),alias,Vm::ResultDisposition::ZeroFill,4u}}):Vm::Execute(host,caller,"Self");
        Failed(result,Vm::Status::Unsupported,host,"Selectability native index/declaration alone does not grant an initialized Bool temporary");
        Require(!read && host.nativeArguments.empty(),"Unknown selectability default read input or entered native work");
    }
    for(const bool storageBudget:{false,true}) {
        TestHost host;const auto native=configure(host);bool read{};
        auto alias=std::make_shared<Vm::Reference>();alias->zero=Value::Bool(false);alias->read=[&] {read=true;return Value::Bool(true);};
        host.argumentSeed=[&](std::uint16_t index,const std::string&,std::size_t,const Vm::Function*)->std::optional<Value> {
            if(index!=1420u) return std::nullopt;
            if(!storageBudget) throw std::runtime_error("Selectability seed lookup failed");
            auto value=Value::Bool(false);value.text.reserve(200000u);return value;
        };
        auto limits=Vm::Limits{};if(storageBudget) limits.retainedBytes=100000u;
        const auto result=Vm::ExecuteTransaction(host,[&](Vm::Execution& execution) {
            execution.CallFunction(Function("EarlierSelectabilitySeedEffect",Join({Native(1000u),Return()})),"Self");
            try {execution.CallFunction(native,"Self",{{{},alias}});}catch(const std::exception&){}return Value{};
        },limits);
        Failed(result,storageBudget?Vm::Status::Budget:Vm::Status::Unsupported,host,"Selectability seed failure/allocation bounds precede incoming alias reads and poison transactions");
        Require(!read && host.effects==0u && host.rng==12345u && host.nativeIndices==std::vector<std::uint16_t>{1000u},
            "Selectability seed failure read pending input, reached setter or committed prior effects");
    }
    std::cout<<"Audited Window selectability input copies/defaults: "<<checks-initialChecks<<" checks and "
        <<rejections-initialRejections<<" refusal controls.\n";
}
void WindowVisibilityInputContracts() {
    const auto initialChecks=checks,initialRejections=rejections;
    for(const auto nativeIndex:{std::uint16_t{1415u},std::uint16_t{1413u}})
    for(const bool declared:{false,true}) for(const int mode:{0,1,2,3,4,5,6,7}) {
        TestHost host;auto native=Function(nativeIndex==1415u?"IsVisible":"Show",{},"Extension.Window");
        native.nativeIndex=nativeIndex;native.flags=0x401u;
        native.variables={{native.path+".Input",nativeIndex==1415u?"bRecurse":"bShow",Value::Bool(false),0x90u,1u}};
        if(nativeIndex==1415u)native.variables.push_back({"IsVisible.ReturnValue","ReturnValue",Value::Bool(false),0x580u,1u});
        host.finals[{"Fixture",7}]=std::make_shared<Vm::Function>(native);
        host.argumentSeed=[nativeIndex](std::uint16_t index,const std::string&,std::size_t slot,const Vm::Function*)->std::optional<Value> {
            if(index!=nativeIndex) return std::nullopt;
            Require(slot==0u,"Visibility native seeded a return/excess input slot");return Value::Bool(true);
        };
        host.materializeOmission=[](std::uint16_t index,const std::string&,std::size_t slot,const Vm::Function*) {
            return index==1413u&&slot==0u;
        };
        host.executionNative=[](const std::string&,const std::vector<Vm::Evaluation>& arguments,const Vm::Function*,Vm::Execution&) {
            Require(arguments.size()<=1u,"IsVisible exceeded its single optional input");
            const auto value=arguments.empty()?Value::Bool(true):arguments[0].Load();
            if(!arguments.empty()) Require(!arguments[0].reference,"IsVisible ordinary input retained an OUT alias");
            return Vm::Evaluation{value.kind==Vm::Kind::Nothing?Value::Bool(true):value,{}};
        };
        Bytes input;
        if(mode==1) input={0x0bu};
        else if(mode==2) input={0x28u};
        else if(mode>=3) input=Context({0x2au},Native(4095u),static_cast<std::uint8_t>(mode==3?0u:mode==4?4u:mode-4u));
        const auto expression=mode==0?(declared?Call(0x1c,7):Native(nativeIndex)):
            (declared?Call(0x1c,7,{input}):Native(nativeIndex,{input}));
        const auto caller=Function("IsVisibleNativeDefault",Return(expression));
        const auto result=Vm::Execute(host,caller,"Self");
        if(mode>=5) Failed(result,Vm::Status::Unsupported,host,"IsVisible partial Bool temporary clearing remains explicit refusal");
        else {
            Returned(result,"IsVisible native true default and exact context clearing");
            Require(result.value.kind==Vm::Kind::Bool && result.value.boolean==(mode==0 || mode==1 || mode==3) && result.writes==0u,
                "IsVisible replaced native true seed with declaration false or lost full clearing");
        }
    }
    std::cout<<"Window visibility native input defaults: "<<checks-initialChecks<<" checks and "
        <<rejections-initialRejections<<" refusal controls.\n";
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
        TestHost host;
        // An ineligible root legitimately returns without entering Locate(),
        // leaving Result.function with no reserved string storage. The public
        // Execute success identity must therefore materialize BEFORE commit.
        const auto function=Function("DisabledRootWithLongSuccessIdentityRequiringHeapStorage",Return(Int(7)));
        host.disabledFunctions.insert(function.path);host.denyAllocationsAfterCommit=true;
        const auto result=Vm::Execute(host,function,"Self");
        const auto allocations=VmPublicationAllocationFault::attempts;VmPublicationAllocationFault::denied=false;
        Returned(result,"Execute publication tail is allocation-free");
        Require(result.function==function.path&&result.value.kind==Vm::Kind::Nothing&&host.commitCount==1u&&
            host.rollbackCount==0u&&allocations==0u,"Execute allocated/threw after committing native host state");
    }
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
        Returned(r,"Original infinity FloatConst");
        Require(r.value.kind==Vm::Kind::Float&&std::bit_cast<std::uint32_t>(r.value.floating)==0x7f800000u&&host.effects==1,
            "Original infinity FloatConst was rejected/normalized or rolled back a successful effect");
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
    Require(sizeof(payload.intArray)==sizeof(void*)&&zero.intArray.empty()&&!zero.intArray.capacity()&&
        !zero.intArray.ContainerBytes()&&zero.intArray.begin()==zero.intArray.end(),
        "Empty native INT-array storage retained an eager vector container or invalid empty range");
    for(const auto& scalar:std::vector<Value>{Value{},Value::Integer(1),Value::Byte(1),Value::Bool(true),Value::Float(1),
        Value::Text(Vm::Kind::String,"text"),Value::Text(Vm::Kind::Name,"Name"),Value::Text(Vm::Kind::Object,"Map.Actor"),
        Value::Vector({}),Value::Rotator({})})
        Require(scalar.intArray.empty()&&!scalar.intArray.capacity()&&!scalar.intArray.ContainerBytes()&&
            scalar.intArray.begin()==scalar.intArray.end(),"Scalar Value eagerly allocated native INT-array storage");
    {
        Value array=zero;array.intArray.reserve(0u);array.intArray.clear();
        Require(!array.intArray.ContainerBytes(),"Zero reserve/clear allocated an absent native INT-array container");
        array.intArray.reserve(64u);const auto capacity=array.intArray.capacity();
        Require(array.intArray.empty()&&capacity>=64u&&array.intArray.ContainerBytes()==sizeof(std::vector<std::int32_t>),
            "Reserved empty native INT array lost its real vector container/allocation");
        const auto emptyCopy=array;const std::vector<std::int32_t> baselineEmptyCopy(static_cast<const std::vector<std::int32_t>&>(array.intArray));
        Require(emptyCopy.intArray.empty()&&emptyCopy.intArray.capacity()==baselineEmptyCopy.capacity(),
            "Copying a reserved empty INT array preserved spare capacity unlike std::vector copy construction");
        array.intArray.push_back(7);array.intArray.push_back(-3);
        auto copied=array;const std::vector<std::int32_t> expectedCopy(static_cast<const std::vector<std::int32_t>&>(array.intArray));
        Require(copied.intArray==expectedCopy&&expectedCopy==copied.intArray&&
            copied.intArray.capacity()==expectedCopy.capacity()&&copied.intArray.ContainerBytes()==sizeof(expectedCopy),
            "INT-array deep copy changed vector copy capacity, container accounting or raw-vector equality");
        copied.intArray[0]=99;
        Require(array.intArray[0]==7&&copied.intArray[0]==99,"Copied native INT arrays share mutable element storage");
        auto assigned=payload;assigned=array;assigned.intArray[1]=81;
        Require(array.intArray[1]==-3&&assigned.intArray[1]==81,"INT-array copy assignment aliases source payload");
        const auto* selfAlias=&assigned;assigned=*selfAlias;
        Require(assigned.intArray[0]==7&&assigned.intArray[1]==81,"INT-array self-copy assignment changed values");
        array.intArray.clear();
        Require(array.intArray.empty()&&array.intArray.capacity()==capacity&&array.intArray.ContainerBytes()==sizeof(expectedCopy),
            "Clearing native INT-array values discarded retained capacity/container storage");
        auto moved=std::move(array);
        Require(moved.intArray.empty()&&moved.intArray.capacity()==capacity&&moved.intArray.ContainerBytes()==sizeof(expectedCopy)&&
            array.intArray.empty()&&!array.intArray.capacity()&&!array.intArray.ContainerBytes(),
            "Native INT-array move construction lost reserved-empty allocation ownership");
        copied=std::move(moved);
        Require(copied.intArray.empty()&&copied.intArray.capacity()==capacity&&copied.intArray.ContainerBytes()==sizeof(expectedCopy)&&
            moved.intArray.empty()&&!moved.intArray.ContainerBytes(),"INT-array move assignment copied or lost retained capacity");
        auto extracted=std::move(copied.intArray).Take();
        Require(extracted.empty()&&extracted.capacity()==capacity&&copied.intArray.empty()&&!copied.intArray.capacity()&&
            !copied.intArray.ContainerBytes(),"Native INT-array vector extraction copied or retained the moved allocation");
        copied=Value::IntArray(std::move(extracted));
        Require(copied.intArray.empty()&&copied.intArray.capacity()==capacity&&copied.intArray.ContainerBytes()==sizeof(expectedCopy),
            "Native INT-array factory discarded extracted reserved-empty vector capacity");
        auto fullWidth=payload;
        auto extractedValues=std::move(fullWidth.intArray).Take();
        const auto reconstructed=Value::IntArray(std::move(extractedValues));
        Require(Vm::Equal(reconstructed,payload)&&fullWidth.intArray.empty()&&!fullWidth.intArray.ContainerBytes(),
            "Native INT-array extraction/factory round trip changed full-width ordered duplicate values");
        copied=zero;Require(copied.intArray.empty()&&!copied.intArray.capacity()&&!copied.intArray.ContainerBytes(),
            "Assigning absent native INT-array storage retained a stale container");
    }
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
        Vm::Limits limits;limits.retainedBytes=sizeof(Value)+array.intArray.ContainerBytes()+capacity*sizeof(std::int32_t)-(excess ? 1u : 0u);
        const auto result=Vm::ExecuteTransaction(host,[&](Vm::Execution&) {return std::move(array);},limits);
        if(excess) Failed(result,Vm::Status::Budget,host,"INT-array spare allocation contributes retained bytes");
        else Returned(result,"INT-array exact retained byte boundary");
    }
    for(const bool excess:{false,true}) {
        TestHost host;Value structure;structure.kind=Vm::Kind::Struct;
        structure.fields["first"]=reserved(32u);structure.fields["second"]=reserved(48u);
        const auto allocation=(structure.fields.at("first").intArray.capacity()+structure.fields.at("second").intArray.capacity())*sizeof(std::int32_t)+
            structure.fields.at("first").intArray.ContainerBytes()+structure.fields.at("second").intArray.ContainerBytes();
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
    RotatorDirectionContracts();ScalarAndLocalContracts();ParametersAndReturns();DecalNativeInputContracts();AssignmentArraysAndStructs();FixedStructArrayMemberContracts();LazyAndContextContracts();ReceiverAndContextMetadataContracts();ClassContextContracts();
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
            if(invalid==3u) return Value::Vector({std::numeric_limits<float>::infinity(),0.0f,0.0f});
            return Value::Text(Vm::Kind::String,std::string(8193u,'x'));
        };
        const auto result=Vm::ExecuteTransaction(host,callback);
        const auto status=invalid==1u ? Vm::Status::Unsupported : invalid==4u ? Vm::Status::Budget : Vm::Status::Invalid;
        Failed(result,status,host,"Coordinator failure/return validation uses ordinary transaction policy");
        const std::array<std::string,5> errors{"Missing VM transaction callback","Coordinator failure","Unknown VM transaction failure",
            "Non-finite VM vector","VM value storage limit"};
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
void StatePhaseContracts() {
    using Phase=Vm::StatePhaseStatus;
    using Latent=QuestVr::StateLatent;
    const auto initialChecks=checks,initialRejections=rejections;
    const auto install=[](TestHost& host,const Vm::Function& program,const std::string& receiver="Self") {
        QuestVr::StateFrame frame;frame.codePath=frame.localsCodePath=program.path;
        frame.control=QuestVr::StateControl{program.path,true,~std::uint64_t{0}};
        if(receiver=="Self")host.state=QuestVr::StateObject{true,true,frame,{}};
        else host.phaseStates[receiver]=QuestVr::StateObject{true,true,frame,{}};
        host.statePrograms[program.path]=program;
    };
    const auto sameFrame=[](const QuestVr::StateFrame& a,const QuestVr::StateFrame& b) {
        return a.codePath==b.codePath && a.localsCodePath==b.localsCodePath &&
            a.statementIndex==b.statementIndex && a.latent==b.latent && a.control && b.control &&
            a.control->selectedPath==b.control->selectedPath && a.control->codePresent==b.control->codePresent &&
            a.control->liveProbeMask==b.control->liveProbeMask;
    };
    const auto phaseStatus=[](Vm::Status status) {
        return status==Vm::Status::Waiting?Phase::Waiting:status==Vm::Status::Stopped?Phase::Stopped:Phase::Returned;
    };
    // Both public routes enter exactly the same core as a child phase. Waiting,
    // stopped/null Code, returned values and ineligible polls stay distinct.
    for(const bool poll:{false,true})for(unsigned scenario=0u;scenario<5u;++scenario) {
        TestHost standalone,coordinated;std::size_t publicPolls{},childPolls{};
        const auto program=Function("PhaseEquivalence",scenario==2u?Return(Int(31)):Bytes{0x08u});
        for(auto* host:{&standalone,&coordinated}) {
            install(*host,program);
            if(scenario==0u)host->state->frame->latent=Latent::FinishAnim;
            if(scenario==4u)host->state->frame->control->codePresent=false;
        }
        standalone.statePoll=[&](const std::string&,float elapsed,Vm::Execution&) {++publicPolls;Require(elapsed==0.125f,"Public phase changed elapsed");return scenario!=3u;};
        coordinated.statePoll=[&](const std::string&,float elapsed,Vm::Execution&) {++childPolls;Require(elapsed==0.125f,"Child phase changed elapsed");return scenario!=3u;};
        const auto publicResult=poll?Vm::AdvanceState(standalone,"Self",0.125f):Vm::ResumeState(standalone,"Self");
        Vm::StatePhaseResult child;
        const auto root=Vm::ExecuteTransaction(coordinated,[&](Vm::Execution& execution) {
            child=execution.RunStatePhase("Self",poll?std::optional<float>(0.125f):std::nullopt);
            return Value::Integer(77);
        });
        Returned(publicResult,"Public/shared state core equivalence");Returned(root,"Child state phase shares outer acceptance");
        Require(root.status==Vm::Status::Returned && root.value.integer==77 && child.status==phaseStatus(publicResult.status) &&
            Vm::Equal(child.value,publicResult.value) && root.instructions==publicResult.instructions && root.writes==publicResult.writes &&
            publicPolls==childPolls && sameFrame(*standalone.state->frame,*coordinated.state->frame),
            "Public and child state phases diverged or child disposition leaked into root");
        Require(coordinated.beginCount==1u && coordinated.commitCount==1u && coordinated.rollbackCount==0u,
            "Child phase opened/committed a nested host transaction");
    }
    // A waiting/stopped child invoked by an executing parent must not stop the
    // parent statement stream, nor poison a later independent child result.
    for(const bool waiting:{false,true}) {
        TestHost host;const auto child=Function("ChildState",{0x08u});
        install(host,child,"Child");if(waiting)host.phaseStates.at("Child").frame->latent=Latent::FinishAnim;
        const auto parent=Function("ParentState",Join({Native(1008u),Native(1000u),Return(Int(19))}));install(host,parent);
        host.executionNative=[&](const std::string& receiver,const std::vector<Vm::Evaluation>&,
            const Vm::Function*,Vm::Execution& execution) {
            Require(receiver=="Self","Parent native receiver changed");
            const auto result=execution.RunStatePhase("Child");
            Require(result.status==(waiting?Phase::Waiting:Phase::Stopped),"Child phase lost its own disposition");
            return Vm::Evaluation{};
        };
        const auto result=Vm::ResumeState(host,"Self");Returned(result,"Nested state child leaves parent running");
        Require(result.status==Vm::Status::Returned && result.value.integer==19 && host.effects==1u &&
            host.state->frame->statementIndex==3u,"Child Waiting/Stopped leaked into parent state outcome");
    }
    // FinishAnim newly installed by a native expression yields only after its
    // entire enclosing statement. A no-time resume never invokes its poll.
    {
        TestHost host;const auto program=Function("FinishAnimStatement",Join({
            Native(1003u,{Native(261u),Native(1000u)}),Native(1000u),{0x08u}}));install(host,program);
        std::size_t polls{},finishes{};
        host.statePoll=[&](const std::string&,float,Vm::Execution&) {++polls;return true;};
        host.executionNative=[&](const std::string&,const std::vector<Vm::Evaluation>& arguments,
            const Vm::Function* declaration,Vm::Execution&) {
            Require(!declaration && arguments.empty(),"Raw FinishAnim acquired a fake declaration/argument");
            ++finishes;host.state->frame->latent=Latent::FinishAnim;return Vm::Evaluation{};
        };
        const auto result=Vm::ExecuteTransaction(host,[&](Vm::Execution& execution) {
            const auto first=execution.RunStatePhase("Self",0.0f);
            Require(first.status==Phase::Waiting && host.effects==1u && host.state->frame->statementIndex==1u,
                "New FinishAnim wait interrupted a nested expression or ran the following statement");
            const auto again=execution.RunStatePhase("Self");
            Require(again.status==Phase::Waiting && host.effects==1u && polls==1u,
                "No-time child resume polled/reexecuted a FinishAnim wait");
            return Value{};
        });
        Returned(result,"FinishAnim statement boundary preserves root");
        Require(result.status==Vm::Status::Returned && finishes==1u && polls==1u,"Child wait was polled twice or changed root status");
    }
    // Initial null Code/deletion gates must avoid native polling. Role and
    // selected-state simulated eligibility remain the host's initial poll gate.
    for(unsigned scenario=0u;scenario<4u;++scenario) {
        TestHost host;install(host,Function("PhaseInactive",{0x08u}));std::size_t polls{},deletionReads{};
        if(scenario==0u)host.state->frame->control->codePresent=false;
        if(scenario==3u)host.state.reset();
        host.stateDeleted=[&](const std::string&) {++deletionReads;return scenario==1u;};
        host.statePoll=[&](const std::string&,float,Vm::Execution&) {++polls;return false;};
        const auto result=Vm::ExecuteTransaction(host,[&](Vm::Execution& execution) {
            const auto phase=execution.RunStatePhase("Self",0.1f);
            Require(phase.status==(scenario<2u?Phase::Stopped:Phase::Returned),"Inactive state phase manufactured a wait/continuation");
            return Value{};
        });
        Returned(result,"Inactive child state poll");
        Require(polls==(scenario<2u?0u:1u) && host.stateProgramReads.empty() &&
            (scenario!=0u || deletionReads==0u),"Inactive phase eagerly polled/prepared/deletion-tested null Code");
    }
    // Poll callbacks can replace every part of the frame. Continue only using
    // that replacement; do not restore the old pending wait/cursor afterward.
    for(unsigned replacement=0u;replacement<4u;++replacement) {
        TestHost host;const auto old=Function("BeforePoll",Join({Native(4095u),{0x08u}}));install(host,old);
        host.state->frame->latent=Latent::FinishAnim;
        const auto next=Function("AfterPoll",Join({Native(1000u),Return(Int(51))}));host.statePrograms[next.path]=next;
        host.stateDeleted=[&](const std::string&) {return host.effects==9u;};
        host.statePoll=[&](const std::string&,float,Vm::Execution&) {
            QuestVr::StateFrame frame;frame.codePath=frame.localsCodePath=next.path;
            frame.control=QuestVr::StateControl{next.path,replacement!=1u,~std::uint64_t{0}};
            if(replacement==2u)frame.latent=Latent::FinishAnim;
            host.state->frame=frame;++host.stateRevision;
            if(replacement==3u)host.effects=9u;
            return true;
        };
        const auto result=Vm::ExecuteTransaction(host,[&](Vm::Execution& execution) {
            const auto phase=execution.RunStatePhase("Self",0.25f);
            Require(phase.status==(replacement==0u?Phase::Returned:replacement==2u?Phase::Waiting:Phase::Stopped),
                "Post-poll replacement Code/deletion/latent gate was not reread");
            if(replacement==0u)Require(phase.value.integer==51,"Replacement state returned old program value");
            return Value{};
        });
        Returned(result,"Synchronous poll replacement stays in same state phase");
        Require(host.state->frame->codePath==next.path && host.state->frame->latent==(replacement==2u?Latent::FinishAnim:Latent::Continue) &&
            host.effects==(replacement==0u?1u:replacement==3u?9u:0u),"Poll replacement was overwritten by captured frame state");
    }
    for(const bool child:{false,true}) {
        TestHost host;Bytes code=Native(1008u);Append(code,{0x06u,0u,0u});
        install(host,Function("SelectedChangeSlice",code));std::size_t calls{};
        host.statePoll=[&](const std::string&,float,Vm::Execution&) {
            host.state->frame->control->selectedPath="Fixture.SelectedAfterPoll";return true;
        };
        host.executionNative=[&](const std::string&,const std::vector<Vm::Evaluation>&,const Vm::Function*,Vm::Execution&) {
            if(calls++)host.state->frame->control->selectedPath="Fixture.Selected"+std::to_string(calls);
            return Vm::Evaluation{};
        };
        const auto result=child?Vm::ExecuteTransaction(host,[&](Vm::Execution& execution) {
            const auto phase=execution.RunStatePhase("Self",0.0f);
            Require(phase.status==Phase::Returned,"Selected-change cap invented a stop/wait");return phase.value;
        }):Vm::AdvanceState(host,"Self",0.0f);
        Returned(result,"Public/child state phase retains selected node before poll");
        Require(calls==5u && host.state->frame->statementIndex==1u && host.state->frame->control->codePresent,
            "Poll-selected replacement was not counted toward original fifth observed selected change");
    }
    // A late ordinary call must undo earlier actions, all state phases and the
    // native poll's writes/effects in the same root, including external aliases.
    for(unsigned fail=0u;fail<3u;++fail) {
        TestHost host;const auto program=Function("PhaseRollback",Join({Let(Ref(0x01u,1),Int(22)),Native(1000u),{0x08u}}));install(host,program);
        const auto marker=host.Property(program,1,"PhaseMarker",Value::Integer(0));host.Store("Self",marker,{Value::Integer(3)});
        const auto before=*host.state->frame;
        Value outside=Value::Integer(5);auto alias=std::make_shared<Vm::Reference>();alias->zero=Value::Integer(0);
        alias->read=[&] {return outside;};alias->write=[&](const Value& value) {outside=value;};
        host.statePoll=[&](const std::string&,float,Vm::Execution& execution) {
            execution.WriteReference(alias,Value::Integer(88));return true;
        };
        Vm::Limits limits;if(fail==1u)limits.writes=2u;if(fail==2u)limits.instructions=5u;
        const auto result=Vm::ExecuteTransaction(host,[&](Vm::Execution& execution) {
            execution.WriteReference(host.Reference({"Self",Vm::Scope::Instance,marker.key},marker),Value::Integer(11));
            static_cast<void>(execution.RunStatePhase("Self",0.1f));
            return execution.CallFunction(Function("LaterFailure",Return(Native(4095u))),"Self");
        },limits);
        Failed(result,fail==0u?Vm::Status::Unsupported:Vm::Status::Budget,host,"Late state-phase failure rolls back entire prefix");
        Require(outside.integer==5 && host.Load("Self",marker).integer==3 && sameFrame(before,*host.state->frame) &&
            host.effects==0u && host.rng==12345u,"State phase committed native/script/cursor/OUT prefix independently");
    }
    // Invalid nested phase input is a poisoned execution entry, not an empty
    // success that the coordinator can catch and continue committing.
    for(unsigned invalid=0u;invalid<3u;++invalid) {
        TestHost host;const auto first=Function("BeforeInvalidPhase",Join({Native(1000u),Return()}));
        Vm::Limits limits;if(invalid==2u)limits.stringBytes=32u;
        bool caught{},rejectedLater{};
        const auto result=Vm::ExecuteTransaction(host,[&](Vm::Execution& execution) {
            execution.CallFunction(first,"Self");
            try {static_cast<void>(execution.RunStatePhase(invalid==2u?std::string(33u,'R'):"Self",
                invalid==0u?-1.0f:invalid==1u?std::numeric_limits<float>::quiet_NaN():0.0f));}
            catch(const std::exception&) {caught=true;}
            try {execution.CallFunction(Function("Later",Return(Native(1000u))),"Self");}
            catch(const std::exception&) {rejectedLater=true;}
            return Value{};
        },limits);
        Failed(result,invalid==2u?Vm::Status::Budget:Vm::Status::Invalid,host,"Caught invalid phase cannot commit earlier root prefix");
        Require(caught && rejectedLater && host.effects==0u && result.function.empty() && result.callStack.empty(),
            "Invalid coordinator phase reset poison, retained effects or blamed a stale prior script");
    }
    // Native polling errors keep the active real caller, whereas a failure in
    // state code adds its actual state frame. A swallowed child error retains
    // the FIRST responsible location even if a later call/host error follows.
    for(const bool duringState:{false,true}) {
        TestHost host;install(host,Function("FailingPhase",Return(Native(4095u))));
        const auto caller=Function("PhaseNativeCaller",Return(Native(1008u)));
        host.statePoll=[&](const std::string&,float,Vm::Execution&) {
            if(!duringState)throw std::runtime_error("original phase poll failed");
            return true;
        };
        host.executionNative=[&](const std::string&,const std::vector<Vm::Evaluation>&,const Vm::Function*,Vm::Execution& execution) {
            try {static_cast<void>(execution.RunStatePhase("Self",0.1f));}catch(const std::exception&) {}
            return Vm::Evaluation{};
        };
        const auto result=Vm::ExecuteTransaction(host,[&](Vm::Execution& execution) -> Value {
            try {execution.CallFunction(caller,"Self");}catch(const std::exception&) {}
            throw std::runtime_error("later unrelated coordinator failure");
        });
        Failed(result,Vm::Status::Unsupported,host,"Child state phase preserves first error location");
        Require(result.function==(duringState?"Fixture.FailingPhase":caller.path) &&
            result.callStack==std::vector<std::string>(duringState?std::vector<std::string>{caller.path,"Fixture.FailingPhase"}:
                std::vector<std::string>{caller.path}) &&
            result.error.find(duringState?"4095":"original phase poll failed")!=std::string::npos,
            "Nested phase error lost active caller/state identity or preferred later coordinator failure");
    }
    {
        TestHost host;install(host,Function("CompletedChild",Return(Int(9))));
        const auto caller=Function("RepeatedPhaseCaller",Return(Native(1008u)));
        host.executionNative=[&](const std::string&,const std::vector<Vm::Evaluation>&,const Vm::Function*,Vm::Execution& execution) {
            const auto first=execution.RunStatePhase("Self");Require(first.value.integer==9,"Initial child phase did not return");
            try {static_cast<void>(execution.RunStatePhase("Self",-1.0f));}catch(const std::exception&) {}
            return Vm::Evaluation{};
        };
        const auto result=Vm::Execute(host,caller,"Self");
        Failed(result,Vm::Status::Invalid,host,"Later pre-entry phase failure belongs to restored actual caller");
        Require(result.function==caller.path && result.callStack==std::vector<std::string>{caller.path} && result.offset==1u,
            "Successful child left its stale diagnostic PC for a later pre-entry failure");
    }
    for(const float elapsed:{-1.0f,std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity()}) {
        TestHost host;const auto result=Vm::AdvanceState(host,"Self",elapsed);
        Require(result.status==Vm::Status::Invalid && !result.committed && !host.beginCount && !host.rollbackCount,
            "Public invalid state elapsed no longer rejects before opening a root");++rejections;
    }
    // Unchanged views are reused across slices; recreating locals under the
    // same code identity gets a NEW view and never escapes root allowances.
    {
        TestHost host;auto program=Function("PhaseCachedView",Join({Native(165u,{Ref(0x00u,1)}),{0x08u}}));
        const auto local=host.Property(program,1,"Counter",Value::Integer(0),0u,1u,true);program.variables={local};
        install(host,program);host.Store("Self",local,{Value::Integer(1)});
        const auto result=Vm::ExecuteTransaction(host,[&](Vm::Execution& execution) {
            for(unsigned slice=0u;slice<3u;++slice) {
                auto& frame=*host.state->frame;frame.control->codePresent=true;frame.statementIndex=0u;
                if(slice==2u) {++host.stateRevision;host.Store("Self",local,{Value::Integer(100)});}
                const auto phase=execution.RunStatePhase("Self");
                Require(phase.status==Phase::Stopped && host.Load("Self",local).integer==(slice==2u?101:2+static_cast<int>(slice)),
                    "Repeated state view used stale/recreated locals or reset continuation");
            }
            return Value{};
        });
        Returned(result,"State views retained across same-Execution phases");
        Require(host.stateProgramReads==std::vector<std::string>{program.path,program.path} && result.writes==3u,
            "State view cache was cleared between child phases or reused across local storage revision");
    }
    // Operation, script and native budgets are one root allowance even when
    // phases finish or do no work. No child is allowed to reset the ledger.
    for(unsigned budget=0u;budget<4u;++budget) {
        TestHost host;install(host,Function("SharedPhaseBudget",Join({Native(1000u),{0x08u}})));
        host.trackPhaseWork=budget==3u;
        host.statePoll=[&](const std::string&,float,Vm::Execution&) {if(host.trackPhaseWork)host.phaseWork.Step();return true;};
        Vm::Limits limits;if(budget==0u)limits.operations=1u;if(budget==1u)limits.instructions=3u;
        if(budget==2u)limits.localElements=1u;
        if(budget==3u)limits.nativeWork=1u;
        if(budget==2u) {
            auto& program=host.statePrograms.at(host.state->frame->codePath);
            const auto local=host.Property(program,1,"RetainedLocal",Value::Integer(0),0u,1u,true);program.variables={local};
            host.Store("Self",local,{Value::Integer(0)});
        }
        const auto result=Vm::ExecuteTransaction(host,[&](Vm::Execution& execution) {
            static_cast<void>(execution.RunStatePhase("Self",0.1f));
            host.state->frame->control->codePresent=true;host.state->frame->statementIndex=0u;
            if(budget==2u)++host.stateRevision;
            return execution.RunStatePhase("Self",0.1f).value;
        },limits);
        Failed(result,Vm::Status::Budget,host,"State phases share operation/instruction/local/native limits");
        Require(host.effects==0u && host.stateRevision==0u && host.state->frame->statementIndex==0u,
            "Budget-refused child phase retained earlier slice effects/cursor");
        if(budget==3u)Require(result.nativeWork && result.nativeWork->used==1u && result.nativeWork->limit==1u,
            "State phase reset or lost terminal native work allowance");
    }
    // A zero-operation root cannot smuggle even an inactive/no-frame child.
    {
        TestHost host;Vm::Limits limits;limits.operations=0u;
        const auto result=Vm::ExecuteTransaction(host,[&](Vm::Execution& execution) {return execution.RunStatePhase("Missing").value;},limits);
        Failed(result,Vm::Status::Budget,host,"Inactive child state entry still consumes operation admission");
    }
    std::cout<<"Same-Execution state phases: "<<checks-initialChecks<<" checks / "
        <<rejections-initialRejections<<" refusal controls.\n";
}
void FinishAnimDeclarationContracts() {
    const auto initialChecks=checks,initialRejections=rejections;
    const auto declaration=[] {
        auto method=Function("FinishAnim",{},"Engine.Actor");method.flags=0x409u;method.nativeIndex=261u;return method;
    };
    for(unsigned route=0u;route<4u;++route) {
        TestHost host;const auto method=declaration();std::size_t calls{};
        host.executionNative=[&](const std::string& receiver,const std::vector<Vm::Evaluation>& arguments,
            const Vm::Function* actual,Vm::Execution&) {
            ++calls;Require(receiver=="Self" && arguments.empty() && static_cast<bool>(actual)==(route!=3u),
                "FinishAnim direct/reflected/raw packet changed declaration/inputs");return Vm::Evaluation{};
        };
        host.finals[{"Fixture",1}]=std::make_shared<Vm::Function>(method);
        const auto result=route==0u?Vm::Execute(host,method,"Self"):
            route==1u?Vm::ExecuteTransaction(host,[&](Vm::Execution& execution) {return execution.CallFunction(method,"Self");}):
            Vm::Execute(host,Function("FinishAnimCaller",Return(route==2u?Call(0x1cu,1):Native(261u))),"Self");
        Returned(result,"Exact latent native261 declaration is admitted");
        Require(calls==1u && result.value.kind==Vm::Kind::Nothing,"FinishAnim invented a return value or repeated its native entry");
    }
    for(unsigned malformed=0u;malformed<5u;++malformed)for(const bool reflected:{false,true}) {
        TestHost host;auto method=declaration();
        if(malformed==0u)method.flags=0x408u;
        if(malformed==1u)method.flags=0x401u;
        if(malformed==2u)method.variables={host.Property(method,1,"UnexpectedLocal",Value::Integer(0),0u,1u,true)};
        if(malformed==3u)method.variables={host.Property(method,1,"ReturnValue",Value::Integer(0),0x580u,1u,true)};
        if(malformed==4u)method.bytecode=Return();
        host.finals[{"Fixture",1}]=std::make_shared<Vm::Function>(method);
        const auto result=reflected?Vm::Execute(host,Function("MalformedFinishAnimCaller",Return(Call(0x1cu,1))),"Self"):
            Vm::Execute(host,method,"Self");
        Failed(result,Vm::Status::Unsupported,host,"Native261 needs exact flags409/empty declaration");
        Require(host.nativeIndices.empty(),"Malformed FinishAnim declaration reached native body");
    }
    for(unsigned route=0u;route<3u;++route) {
        TestHost host;const auto method=declaration();host.finals[{"Fixture",1}]=std::make_shared<Vm::Function>(method);
        const auto result=route==0u?Vm::Execute(host,method,"Self",{{Value::Integer(1),{}}}):
            Vm::Execute(host,Function("UnexpectedFinishAnimArgument",Return(route==1u?Call(0x1cu,1,{Native(1000u)}):
                Native(261u,{Native(1000u)}))),"Self");
        Failed(result,Vm::Status::Unsupported,host,"FinishAnim cannot evaluate an invented argument expression");
        Require(host.nativeIndices.empty() && host.effects==0u,"Zero-argument FinishAnim evaluated extra argument effects");
    }
    std::cout<<"FinishAnim declaration admission: "<<checks-initialChecks<<" checks / "
        <<rejections-initialRejections<<" refusal controls.\n";
}
void ScalarFloatContracts() {
    const auto initialChecks=checks,initialRejections=rejections;
    const auto word=[](float value){return std::bit_cast<std::uint32_t>(value);};
    const auto real=[](std::uint32_t bits){return Real(std::bit_cast<float>(bits));};
    const auto evaluate=[&](const Bytes& expression) {
        TestHost host;const auto result=Vm::Execute(host,Function("ScalarFloat",Return(expression)),"Self");
        Returned(result,"Original scalar Float expression");return result.value;
    };
    for(const auto& [expression,expected]:std::vector<std::pair<Bytes,std::uint32_t>>{
        {Native(172u,{Real(-1),Real(0)}),0xff800000u},
        {Native(172u,{Real(0),Real(0)}),0xffc00000u},
        {Native(171u,{real(0x7f7fffffu),real(0x7f7fffffu)}),0x7f800000u},
        {Native(174u,{real(0x7f800000u),real(0xff800000u)}),0xffc00000u},
        {Native(175u,{real(0x7f800000u),real(0x7f800000u)}),0xffc00000u},
        {Native(184u,{Real(2),Real(3)}),0x40a00000u},
        {Native(184u,{Context({0x2a},Native(4095u),4u),Real(2)}),0x40000000u},
        {real(0xff800001u),0xffc00001u},{Native(169u,{real(0x7f800001u)}),0xffc00001u}}) {
        const auto value=evaluate(expression);
        Require(value.kind==Vm::Kind::Float&&word(value.floating)==expected,"VM scalar arithmetic/constant lost IEEE bits");
    }
    for(std::uint16_t index=176u;index<=181u;++index) {
        const auto value=evaluate(Native(index,{real(0x7fc12345u),Real(1)}));
        Require(value.kind==Vm::Kind::Bool&&value.boolean==(index==176u||index==178u||index==180u),
            "VM scalar comparison used C++ NaN predicates instead of original C0/C3");
    }
    for(const auto bits:{0x7f800000u,0xff800000u,0x7fc12345u,0xffc12345u,0x7f812345u,0xff812345u}) {
        TestHost host;auto function=Function("ScalarRawCopy",Join({Let(Ref(0x01,2),Ref(0x01,1)),Return(Ref(0x01,2))}));
        const auto source=host.Property(function,1,"Source",Value::Float(0)),destination=host.Property(function,2,"Destination",Value::Float(0));
        host.Store("Self",source,{Value::Float(std::bit_cast<float>(bits))});host.Store("Self",destination,{Value::Float(7)});
        const auto result=Vm::Execute(host,function,"Self");Returned(result,"Raw scalar property copy");
        Require(word(result.value.floating)==bits&&word(host.Load("Self",destination).floating)==bits&&result.writes==1u,
            "Property copying quieted/normalized an IEEE scalar word");
    }
    // Pure wrappers copy lhs before RHS callbacks. Compounds instead retain
    // its address and use the freshly mutated lhs after the copied RHS Step.
    for(const auto& [index,expected]:std::vector<std::pair<std::uint16_t,float>>{
        {171u,14},{172u,3.5f},{174u,9},{175u,5},{182u,200},{183u,50},{184u,102},{185u,98}}) {
        TestHost host;const auto mutator=Native(1060u,{Ref(0x01,1),Real(100),Real(2)});
        auto function=Function("ScalarInputs",Return(Native(index,{Ref(0x01,1),mutator})));
        const auto property=host.Property(function,1,"Number",Value::Float(0));host.Store("Self",property,{Value::Float(7)});
        const auto result=Vm::Execute(host,function,"Self");Returned(result,"Original scalar argument copy/reference order");
        Require(result.value.kind==Vm::Kind::Float&&result.value.floating==expected,
            "Pure scalar reloaded lhs or compound reused obsolete lhs");
        Require(host.Load("Self",property).floating==(index>=182u?expected:100.0f),"Scalar input snapshot lost actual OUT write");
    }
    for(std::size_t budget=0u;budget<=3u;++budget) {
        TestHost host;
        auto function=Function("ScalarRecovery",Join({Native(184u,{Ref(0x01,1),Native(172u,{Real(-1),Real(0)})}),
            Let(Ref(0x01,2),Ref(0x01,1)),Let(Ref(0x01,1),Real(0)),Return(Ref(0x01,1))}));
        const auto alpha=host.Property(function,1,"Alpha",Value::Float(0)),observed=host.Property(function,2,"Observed",Value::Float(0));
        host.Store("Self",alpha,{Value::Float(0)});host.Store("Self",observed,{Value::Float(7)});
        auto limits=Vm::Limits{};limits.writes=budget;
        const auto result=Vm::Execute(host,function,"Self",{},limits);
        if(budget==3u) {
            Returned(result,"Original intermediate infinity/recovery");
            Require(word(host.Load("Self",observed).floating)==0xff800000u&&host.Load("Self",alpha).floating==0,
                "VM did not store the infinity before script recovery");
        } else {
            Failed(result,Vm::Status::Budget,host,"Scalar infinity/recovery write prefix");
            Require(host.Load("Self",alpha).floating==0&&host.Load("Self",observed).floating==7,
                "Scalar exceptional value escaped whole-root write undo");
        }
    }
    {
        TestHost host;const auto result=Vm::Execute(host,Function("VectorStillFinite",Return(Vector({
            std::numeric_limits<float>::infinity(),0,0}))),"Self");
        Failed(result,Vm::Status::Invalid,host,"Scalar Float capability does not relax Vector validation");
    }
    std::cout<<"Original scalar Float storage/arithmetic/argument order/IEEE recovery: "<<checks-initialChecks
        <<" checks / "<<rejections-initialRejections<<" refusal controls.\n";
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
        IntegerToStringContracts();
        ExecutionContracts();
        ScalarFloatContracts();
        TransactionContracts();
        StatePhaseContracts();
        FinishAnimDeclarationContracts();
        ProgramAndEligibilityContracts();
        NativeArgumentSeedContracts();
        CoreGetEnumInputContracts();
        CoreColorResultLayoutContracts();
        UninitializedNativeArgumentContracts();
        DirectNativeInitializedArgumentContracts();
        IntegerMaximumContracts();
        SequentialNativeArgumentSeedContracts();
        CollisionSetterInputContracts();
        NativeResultDispositionContracts();
        MaterializedNativeOmissionContracts();
        NativeCallSiteContextContracts();
        FastTraceNativeInputContracts();
        TextWindowInputSnapshotContracts();
        WindowInputSnapshotContracts();
        TileNativeInputContracts();
        WindowConfigureChildInputContracts();
        WindowPreferredSizeOutContracts();
        GcTextNativeInputContracts();
        WindowWidthInputContracts();
        WindowSelectabilityInputContracts();
        WindowVisibilityInputContracts();
        VectorNativeContracts();
        StartupPrimitiveContracts();
        IteratorContracts();
        std::cout<<"Normalized original-style serialization and bounded script execution: "<<checks<<" checks and "<<rejections<<" rejection controls passed.\n";
        return 0;
    }catch(const std::exception& error) {std::cerr<<error.what()<<"\n";return 1;}
}
