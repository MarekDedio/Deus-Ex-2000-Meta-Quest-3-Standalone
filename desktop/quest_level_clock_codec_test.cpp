#include "quest_script_state.h"

#include <bit>
#include <cmath>
#include <cstdint>
#include <functional>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

// Pure checkpoint structure tests. These neither advance a world nor infer
// native ULevel time from LevelInfo.TimeSeconds or a host wall clock.
namespace {
using namespace QuestVr;
using Bytes=std::vector<std::uint8_t>;
std::size_t checks{},refusals{};
void Require(bool condition,const char* reason) {
    ++checks;if(!condition) throw std::runtime_error(reason);
}
void Reject(const std::function<void()>& operation,const char* reason) {
    bool rejected{};try {operation();} catch(const std::runtime_error&) {rejected=true;}
    Require(rejected,reason);++refusals;
}
void U32(Bytes& bytes,std::uint32_t value) {
    for(unsigned i=0u;i<4u;++i) {bytes.push_back(static_cast<std::uint8_t>(value>>(8u*i)));}
}
void U64(Bytes& bytes,std::uint64_t value) {
    for(unsigned i=0u;i<8u;++i) {bytes.push_back(static_cast<std::uint8_t>(value>>(8u*i)));}
}
void String(Bytes& bytes,const std::string& value) {
    U32(bytes,static_cast<std::uint32_t>(value.size()));bytes.insert(bytes.end(),value.begin(),value.end());
}
void Put32(Bytes& bytes,std::size_t offset,std::uint32_t value) {
    for(unsigned i=0u;i<4u;++i) {bytes.at(offset+i)=static_cast<std::uint8_t>(value>>(8u*i));}
}
void Put64(Bytes& bytes,std::size_t offset,std::uint64_t value) {
    for(unsigned i=0u;i<8u;++i) {bytes.at(offset+i)=static_cast<std::uint8_t>(value>>(8u*i));}
}
Vm::Value Int(std::int32_t value) {Vm::Value result;result.kind=Vm::Kind::Int;result.integer=value;return result;}
ScriptSavedObject Actor() {ScriptSavedObject value;value.path="Map.A";value.classPath="Fixture.Actor";return value;}
ScriptSavedState Legacy(unsigned version) {
    ScriptSavedState saved;saved.mapName="Map";
    if(version==2u) {auto actor=Actor();actor.state.emplace();saved.objects.push_back(std::move(actor));}
    if(version==3u) {saved.classDefaults.push_back({"Fixture.Actor",{{"Fixture.Actor.Counter","Counter",0u,Int(7)}}});}
    if(version==4u) {auto actor=Actor();actor.lifecycle.emplace();saved.objects.push_back(std::move(actor));}
    if(version==5u) {saved.births.push_back({"Map.Born","Fixture.Actor",7u,{}});}
    if(version==6u) {Ai::State manager;manager.ownerPath="Map.Info";manager.levelPath="Map.MyLevel";saved.aiManagers.push_back(std::move(manager));}
    if(version==7u) {saved.randomSeed=0x12345678u;}
    if(version==8u) {auto actor=Actor();actor.latentTimeLeft=.25f;saved.objects.push_back(std::move(actor));}
    if(version==9u) {
        auto actor=Actor();StateObject state;state.hasStack=true;state.frameOverride=true;state.frame.emplace();
        state.frame->codePath="Fixture.Actor.Waiting";state.frame->localsCodePath="Fixture.Actor.Waiting";
        state.frame->control=StateControl{"Fixture.Actor.Waiting",true,0x123456789abcdef0ull};
        actor.state=std::move(state);saved.objects.push_back(std::move(actor));
    }
    if(version==10u) {
        auto actor=Actor();Vm::Value value;value.kind=Vm::Kind::IntArray;value.intArray={1,-2};
        actor.properties.push_back({"Fixture.Actor.Array","Array",0u,std::move(value)});saved.objects.push_back(std::move(actor));
    }
    return saved;
}

// Independently written little-endian legacy vectors, not production-encoder
// outputs used as their own oracle. Each vector selects one original codec1..10.
Bytes LegacyBytes(unsigned version) {
    Bytes bytes{'D','X','Q','V','M','S',static_cast<std::uint8_t>(version),0u};String(bytes,"Map");
    const bool object=version==2u||version==4u||version==8u||version==9u||version==10u;
    U32(bytes,object?1u:0u);
    if(object) {
        String(bytes,"Map.A");String(bytes,"Fixture.Actor");U32(bytes,version==10u?1u:0u);
        if(version==10u) {
            String(bytes,"Fixture.Actor.Array");String(bytes,"Array");U32(bytes,0u);bytes.push_back(11u);
            U32(bytes,2u);U32(bytes,1u);U32(bytes,0xfffffffeu);
        }
        bytes.push_back(0u); // No animation clock.
        if(version>=2u) {
            const bool state=version==2u||version==9u;bytes.push_back(state?1u:0u);
            if(state) {
                if(version==2u) {bytes.insert(bytes.end(),{0u,0u,0u});U32(bytes,0u);}
                else {
                    bytes.insert(bytes.end(),{1u,1u,1u});String(bytes,"Fixture.Actor.Waiting");String(bytes,"Fixture.Actor.Waiting");
                    U32(bytes,0u);bytes.push_back(0u);U32(bytes,0u);bytes.push_back(1u);
                    String(bytes,"Fixture.Actor.Waiting");bytes.push_back(1u);U64(bytes,0x123456789abcdef0ull);U32(bytes,0u);
                }
            }
        }
        if(version>=4u) {
            bytes.push_back(version==4u?1u:0u);
            if(version==4u) {bytes.insert(bytes.end(),5u,0u);U32(bytes,0u);U32(bytes,0u);}
        }
        if(version>=8u) {bytes.push_back(version==8u?1u:0u);if(version==8u) {U32(bytes,0x3e800000u);}}
    }
    if(version>=3u) {
        U32(bytes,version==3u?1u:0u);
        if(version==3u) {
            String(bytes,"Fixture.Actor");U32(bytes,1u);String(bytes,"Fixture.Actor.Counter");String(bytes,"Counter");
            U32(bytes,0u);bytes.push_back(2u);U32(bytes,7u);
        }
    }
    if(version>=5u) {
        U32(bytes,version==5u?1u:0u);
        if(version==5u) {String(bytes,"Map.Born");String(bytes,"Fixture.Actor");U32(bytes,7u);U32(bytes,0u);}
    }
    if(version>=6u) {
        U32(bytes,version==6u?1u:0u);
        if(version==6u) {
            String(bytes,"Map.Info");String(bytes,"Map.MyLevel");U32(bytes,0u);U32(bytes,0u);bytes.push_back(0u);
            U32(bytes,0u);U32(bytes,0u);U32(bytes,0u);U32(bytes,0u);
        }
    }
    if(version>=8u) {bytes.push_back(0u);}
    else if(version==7u) {bytes.push_back(1u);U32(bytes,0x12345678u);}
    if(version>=10u) {bytes.push_back(0u);}
    return bytes;
}

void LegacyCompatibility() {
    const ScriptStateLimits limits;
    for(unsigned version=1u;version<=10u;++version) {
        const auto expected=LegacyBytes(version);const auto actual=EncodeScriptSavedState(Legacy(version));
        Require(actual==expected,"Native level clock changed a legacy codec1..10 byte vector");
        const auto decoded=DecodeScriptSavedState(expected);
        Require(!decoded.nativeLevelClock,"Legacy codec fabricated a native ULevel clock");
        Require(!decoded.nativeLevelTickParity,"Legacy codec fabricated native Level parity");
        Require(EncodeScriptSavedState(decoded)==expected,"Legacy clock absence changed decode/reencode bytes");
        ScriptStateDetail::Reader<false> verifier(expected,limits);static_cast<void>(verifier.State());
    }
}
ScriptSavedState Clock(double time=0.0) {
    ScriptSavedState saved;saved.mapName="Map";saved.nativeLevelClock=ScriptSavedLevelClock{"Map.MyLevel",time};return saved;
}
Bytes ClockBytes(std::uint64_t time) {
    Bytes bytes{'D','X','Q','V','M','S',11u,0u};String(bytes,"Map");
    // No objects, defaults, births, or AI managers. Random/decal optional false.
    for(unsigned i=0u;i<4u;++i) {U32(bytes,0u);}
    bytes.push_back(0u);bytes.push_back(0u);String(bytes,"Map.MyLevel");U64(bytes,time);return bytes;
}
void RoundtripsAndExactDouble() {
    for(const std::uint64_t bits:{0ull,0x8000000000000000ull,1ull,0x000fffffffffffffull,
        0x0010000000000000ull,0x3ff0000000000001ull,0x4340000000000001ull,0x7fefffffffffffffull}) {
        const auto value=std::bit_cast<double>(bits);const auto expected=ClockBytes(bits);
        Require(EncodeScriptSavedState(Clock(value))==expected,"Native level clock bytes are not the independent binary64 vector");
        const auto decoded=DecodeScriptSavedState(expected);
        Require(decoded.nativeLevelClock&&decoded.nativeLevelClock->levelPath=="Map.MyLevel"&&
            std::bit_cast<std::uint64_t>(decoded.nativeLevelClock->timeSeconds)==bits,
            "Native clock roundtrip rounded double, normalized signed zero, or changed Level identity");
        Require(EncodeScriptSavedState(decoded)==expected,"Native level clock decode/reencode is not bit-exact");
    }
    std::uint64_t random=0x2916548347738a57ull;
    for(unsigned i=0u;i<8192u;++i) {
        random^=random<<13u;random^=random>>7u;random^=random<<17u;
        const auto bits=(random&0x7fffffffffffffffull)%0x7ff0000000000000ull;
        const auto saved=Clock(std::bit_cast<double>(bits));const auto encoded=EncodeScriptSavedState(saved);
        const auto decoded=DecodeScriptSavedState(encoded);
        Require(decoded.nativeLevelClock&&std::bit_cast<std::uint64_t>(decoded.nativeLevelClock->timeSeconds)==bits,
            "Generated finite native Double clock lost precision");
    }
    for(unsigned version=1u;version<=10u;++version) {
        auto saved=Legacy(version);saved.nativeLevelClock=ScriptSavedLevelClock{"Map.MyLevel",16777217.0000000037252902984619140625};
        const auto encoded=EncodeScriptSavedState(saved);Require(encoded.at(6u)==11u,"Clock did not supersede older optional codec versions");
        const auto decoded=DecodeScriptSavedState(encoded);
        Require(decoded.nativeLevelClock&&EncodeScriptSavedState(decoded)==encoded,"Clock coexistence changed prior optional data");
        auto withoutClock=decoded;withoutClock.nativeLevelClock.reset();
        Require(EncodeScriptSavedState(withoutClock)==LegacyBytes(version),"Codec11 changed a coexisting legacy feature's payload");
    }
    auto saved=Clock(1.0);saved.nativeDecals.emplace();saved.nativeDecals->mapName="Map";
    saved.nativeDecals->modelPath="Map.WorldModel";saved.nativeDecals->levelPath="Map.MyLevel";
    saved.randomSeed=0x56781234u;const auto combined=EncodeScriptSavedState(saved);const auto decoded=DecodeScriptSavedState(combined);
    Require(decoded.nativeDecals&&decoded.randomSeed==saved.randomSeed&&decoded.nativeLevelClock&&
        EncodeScriptSavedState(decoded)==combined,"Clock truncated or reordered preceding codec10 native graph/RNG sections");
}

void InvalidInputsAndBytes() {
    const ScriptStateLimits limits;
    for(const auto invalid:{-1.0,-std::numeric_limits<double>::denorm_min(),std::numeric_limits<double>::infinity(),
        -std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()}) {
        Reject([&] {static_cast<void>(EncodeScriptSavedState(Clock(invalid)));},"Writer accepted invalid native clock Double");
        const auto bytes=ClockBytes(std::bit_cast<std::uint64_t>(invalid));
        Reject([&] {static_cast<void>(DecodeScriptSavedState(bytes));},"Reader accepted invalid native clock Double");
    }
    for(const auto& invalid:std::vector<std::string>{"","Map","map.MyLevel","Other.MyLevel","Map.","Map..MyLevel",
        "Map.MyLevel.","Map./MyLevel","Map.My\\Level","Map.My Level",std::string("Map.Bad\0Level",13u),"Map.\x80"}) {
        auto saved=Clock();saved.nativeLevelClock->levelPath=invalid;
        Reject([&] {static_cast<void>(EncodeScriptSavedState(saved));},"Writer accepted malformed/noncanonical native Level identity");
        auto bytes=ClockBytes(0u);bytes.resize(33u);String(bytes,invalid);U64(bytes,0u);
        Reject([&] {static_cast<void>(DecodeScriptSavedState(bytes));},"Reader accepted malformed/noncanonical native Level identity");
    }
    const auto valid=ClockBytes(0x3ff0000000000001ull);
    for(std::size_t size=0u;size<valid.size();++size) {
        const Bytes truncated(valid.begin(),valid.begin()+static_cast<std::ptrdiff_t>(size));
        Reject([&] {static_cast<void>(DecodeScriptSavedState(truncated));},"Truncated codec11 clock accepted");
    }
    auto missing=valid;missing.resize(33u);
    Reject([&] {static_cast<void>(DecodeScriptSavedState(missing));},"Codec11 accepted an absent mandatory clock record");
    auto trailing=valid;trailing.push_back(0u);
    Reject([&] {static_cast<void>(DecodeScriptSavedState(trailing));},"Codec11 accepted trailing bytes");
    auto old=valid;old.at(6u)=10u;
    Reject([&] {static_cast<void>(DecodeScriptSavedState(old));},"Codec10 accepted native clock trailing section");
    auto future=valid;future.at(6u)=14u;
    Reject([&] {static_cast<void>(DecodeScriptSavedState(future));},"Unsupported clock codec accepted");
    auto oversized=valid;Put32(oversized,33u,0xffffffffu);
    Reject([&] {static_cast<void>(DecodeScriptSavedState(oversized));},"Clock path byte count bypassed remaining-payload bound");
    for(const auto badBits:{0xfff0000000000000ull,0x7ff0000000000000ull,0x7ff0000000000001ull,0xbff0000000000000ull}) {
        auto forged=valid;Put64(forged,forged.size()-8u,badBits);
        Reject([&] {ScriptStateDetail::Reader<false> verifier(forged,limits);static_cast<void>(verifier.State());},
            "Nonmaterializing verification skipped invalid native clock time");
    }
}

void WorkingBudgets() {
    const auto saved=Clock(.125);const auto bytes=EncodeScriptSavedState(saved);
    ScriptStateLimits limits;ScriptStateDetail::Writer legacy(limits,nullptr),clock(limits,nullptr);
    auto baseline=saved;baseline.nativeLevelClock.reset();legacy.State(baseline);clock.State(saved);
    Require(clock.measuredBudget().retained-legacy.measuredBudget().retained==
        sizeof(ScriptSavedLevelClock)+saved.nativeLevelClock->levelPath.size()+1u,
        "Clock measurement omits native record or Level path storage");
    Require(clock.size()==bytes.size(),"Measured clock encoded length differs from emitted bytes");
    limits.maxBytes=clock.measuredBudget().retained-1u;
    Reject([&] {static_cast<void>(EncodeScriptSavedState(saved,limits));},"Writer accepted an over-budget clock record");
    Reject([&] {static_cast<void>(DecodeScriptSavedState(bytes,limits));},"Reader accepted an over-budget clock record");
    limits=ScriptStateLimits{};limits.maxBytes=bytes.size()-1u;
    Reject([&] {static_cast<void>(DecodeScriptSavedState(bytes,limits));},"Reader ignored whole encoded byte cap");
    limits=ScriptStateLimits{};limits.maxStringBytes=9u;
    Reject([&] {static_cast<void>(EncodeScriptSavedState(saved,limits));},"Writer ignored native Level path string budget");
    Reject([&] {static_cast<void>(DecodeScriptSavedState(bytes,limits));},"Reader ignored native Level path string budget");
    auto longPath=Clock();longPath.nativeLevelClock->levelPath="Map."+std::string(8188u,'A');
    const auto longest=EncodeScriptSavedState(longPath);
    Require(DecodeScriptSavedState(longest).nativeLevelClock->levelPath==longPath.nativeLevelClock->levelPath,
        "Maximum bounded native Level path could not roundtrip");
    longPath.nativeLevelClock->levelPath.push_back('A');
    Reject([&] {static_cast<void>(EncodeScriptSavedState(longPath));},"Clock identity exceeded existing8192-byte policy");
}

ScriptSavedState Parity(std::int32_t parity=0,bool withClock=false,bool withOrder=false) {
    ScriptSavedState saved;saved.mapName="Map";
    saved.nativeLevelTickParity=ScriptSavedLevelTickParity{"Map.MyLevel",parity};
    if(withClock) saved.nativeLevelClock=ScriptSavedLevelClock{"Map.MyLevel",std::bit_cast<double>(0x3ff0000000000001ull)};
    if(withOrder) saved.nativeActorOrder=ScriptSavedNativeActorOrder{"Map.MyLevel",2u,{{},{}}};
    return saved;
}
// Independent codec12/13 vectors: optional clock, codec13 order marker,
// optional order, then codec13's mandatory boundary counter. These are not
// generated by the production encoder before comparison.
Bytes NativePrefix(std::uint8_t version) {
    Bytes bytes{'D','X','Q','V','M','S',version,0u};String(bytes,"Map");
    for(unsigned i=0u;i<4u;++i) U32(bytes,0u);
    bytes.push_back(0u);bytes.push_back(0u);return bytes;
}
void EmptyOrder(Bytes& bytes) {
    String(bytes,"Map.MyLevel");bytes.push_back(1u);U32(bytes,2u);U32(bytes,2u);
    bytes.push_back(0u);bytes.push_back(0u);
}
Bytes OrderBytes() {
    auto bytes=NativePrefix(12u);bytes.push_back(0u);EmptyOrder(bytes);return bytes;
}
Bytes ParityBytes(std::int32_t parity=0,bool withClock=false,bool withOrder=false,
    const std::string& owner="Map.MyLevel") {
    auto bytes=NativePrefix(13u);bytes.push_back(withClock?1u:0u);
    if(withClock) {String(bytes,"Map.MyLevel");U64(bytes,0x3ff0000000000001ull);}
    bytes.push_back(withOrder?1u:0u);if(withOrder) EmptyOrder(bytes);
    String(bytes,owner);U32(bytes,static_cast<std::uint32_t>(parity));return bytes;
}
void RejectParityBytes(const Bytes& bytes,const char* reason,const ScriptStateLimits& limits={}) {
    Reject([&] {static_cast<void>(DecodeScriptSavedState(bytes,limits));},reason);
    Reject([&] {ScriptStateDetail::Reader<false> verifier(bytes,limits);static_cast<void>(verifier.State());},reason);
    Reject([&] {ScriptStateDetail::Reader<true> reader(bytes,limits);static_cast<void>(reader.State());},reason);
}
void ParityRoundtripsAndLegacy() {
    const ScriptStateLimits limits;
    for(const auto parity:{0,1}) for(const bool clock:{false,true}) for(const bool order:{false,true}) {
        const auto saved=Parity(parity,clock,order);const auto expected=ParityBytes(parity,clock,order);
        Require(EncodeScriptSavedState(saved)==expected,"Native parity codec13 differs from independent bytes");
        const auto decoded=DecodeScriptSavedState(expected);
        Require(decoded.nativeLevelTickParity&&decoded.nativeLevelTickParity->levelPath=="Map.MyLevel"&&
            decoded.nativeLevelTickParity->parity==parity,"Native parity owner/counter roundtrip changed");
        Require(decoded.nativeLevelClock.has_value()==clock&&decoded.nativeActorOrder.has_value()==order,
            "Parity-only codec fabricated clock/order or lost an optional native section");
        Require(EncodeScriptSavedState(decoded)==expected,"Native parity reencode changed canonical wire bytes");
        ScriptStateDetail::Reader<false> verifier(expected,limits);static_cast<void>(verifier.State());
    }
    for(unsigned version=1u;version<=12u;++version) {
        auto saved=version<=10u?Legacy(version):version==11u?Clock():Parity(0,false,true);
        saved.nativeLevelTickParity.reset();
        const auto expected=version<=10u?LegacyBytes(version):version==11u?ClockBytes(0u):OrderBytes();
        Require(EncodeScriptSavedState(saved)==expected,"Parity extension changed legacy codec1..12 bytes");
        Require(!DecodeScriptSavedState(expected).nativeLevelTickParity,"Legacy checkpoint fabricated a parity continuation");
        saved.nativeLevelTickParity=ScriptSavedLevelTickParity{"Map.MyLevel",1};
        const auto upgraded=EncodeScriptSavedState(saved);const auto decoded=DecodeScriptSavedState(upgraded);
        Require(upgraded.at(6u)==13u&&decoded.nativeLevelTickParity&&decoded.nativeLevelTickParity->parity==1,
            "Parity did not supersede/coexist with an older checkpoint feature");
        auto legacy=decoded;legacy.nativeLevelTickParity.reset();
        Require(EncodeScriptSavedState(legacy)==expected,"Parity coexistence changed an older optional feature payload");
    }
}
Bytes ParityBirthBytes(const std::vector<std::uint32_t>& indices) {
    Bytes bytes{'D','X','Q','V','M','S',13u,0u};String(bytes,"Map");U32(bytes,0u);U32(bytes,0u);
    U32(bytes,static_cast<std::uint32_t>(indices.size()));
    for(std::size_t i=0u;i<indices.size();++i) {
        String(bytes,"Map.Born"+std::to_string(i));String(bytes,"Fixture.Actor");U32(bytes,indices[i]);U32(bytes,0u);
    }
    U32(bytes,0u);bytes.insert(bytes.end(),4u,0u);String(bytes,"Map.MyLevel");U32(bytes,1u);return bytes;
}
void ParityMalformedAndBudgets() {
    for(const auto invalid:{-1,2,std::numeric_limits<std::int32_t>::min(),std::numeric_limits<std::int32_t>::max()}) {
        Reject([&] {static_cast<void>(EncodeScriptSavedState(Parity(invalid)));},"Parity writer accepted a noncanonical counter");
        RejectParityBytes(ParityBytes(invalid),"Parity reader/verifier accepted a noncanonical counter");
    }
    for(const auto& owner:std::vector<std::string>{"","Map","map.MyLevel","Other.MyLevel","Map.","Map..Level",
        "Map.Level.","Map./Level","Map.My\\Level","Map.My Level",std::string("Map.Bad\0Level",13u)}) {
        auto bad=Parity();bad.nativeLevelTickParity->levelPath=owner;
        Reject([&] {static_cast<void>(EncodeScriptSavedState(bad));},"Parity writer accepted a malformed owner");
        RejectParityBytes(ParityBytes(1,false,false,owner),"Parity reader/verifier accepted a malformed owner");
    }
    const auto bytes=ParityBytes(1);
    for(std::size_t size=0u;size<bytes.size();++size)
        RejectParityBytes(Bytes(bytes.begin(),bytes.begin()+static_cast<std::ptrdiff_t>(size)),"Truncated parity checkpoint accepted");
    auto bad=bytes;bad.push_back(0u);RejectParityBytes(bad,"Parity checkpoint accepted trailing bytes");
    bad=bytes;bad[33u]=2u;RejectParityBytes(bad,"Parity codec accepted invalid clock presence");
    bad=bytes;bad[34u]=2u;RejectParityBytes(bad,"Parity codec accepted invalid order presence");
    bad=bytes;bad[6u]=12u;RejectParityBytes(bad,"Codec12 accepted codec13 parity-only trailing section");
    bad=bytes;bad[6u]=14u;RejectParityBytes(bad,"Unknown future parity codec accepted");

    auto births=Parity(1);births.births={{"Map.Born0","Fixture.Actor",7u,{}},{"Map.Born1","Fixture.Actor",8u,{}}};
    Require(EncodeScriptSavedState(births)==ParityBirthBytes({7u,8u}),"Parity without native order changed legacy birth policy bytes");
    Require(EncodeScriptSavedState(DecodeScriptSavedState(ParityBirthBytes({7u,8u})))==ParityBirthBytes({7u,8u}),
        "Parity without native order could not restore ordinary birth indices");
    for(const auto index:{7u,1'000'000u,std::numeric_limits<std::uint32_t>::max()}) {
        births.births[1].worldActorIndex=index;
        Reject([&] {static_cast<void>(EncodeScriptSavedState(births));},"Parity without native order admitted duplicate/unbounded/detached birth index");
        RejectParityBytes(ParityBirthBytes({7u,index}),"Parity reader without native order bypassed legacy birth-index admission");
    }

    const auto saved=Parity(1);ScriptStateLimits limits;
    ScriptStateDetail::Writer baseline(limits,nullptr),measured(limits,nullptr);
    auto legacy=saved;legacy.nativeLevelTickParity.reset();baseline.State(legacy);measured.State(saved);
    Require(measured.size()==bytes.size(),"Parity measured wire length differs from emitted bytes");
    Require(measured.measuredBudget().retained-baseline.measuredBudget().retained==
        sizeof(ScriptSavedLevelTickParity)+saved.nativeLevelTickParity->levelPath.size()+1u,
        "Parity budget omits native record or owner string storage");
    limits.maxBytes=measured.measuredBudget().retained;
    Require(EncodeScriptSavedState(saved,limits)==bytes&&EncodeScriptSavedState(DecodeScriptSavedState(bytes,limits),limits)==bytes,
        "Exact parity retained-byte budget does not roundtrip");
    --limits.maxBytes;
    Reject([&] {static_cast<void>(EncodeScriptSavedState(saved,limits));},"Parity writer bypassed retained-byte cap");
    RejectParityBytes(bytes,"Parity reader/verifier bypassed retained-byte cap",limits);
    limits={};limits.maxStringBytes=10u;
    Reject([&] {static_cast<void>(EncodeScriptSavedState(saved,limits));},"Parity writer bypassed owner string cap");
    RejectParityBytes(bytes,"Parity reader/verifier bypassed owner string cap",limits);
}
} // namespace

int main() {
    try {
        LegacyCompatibility();RoundtripsAndExactDouble();InvalidInputsAndBytes();WorkingBudgets();
        ParityRoundtripsAndLegacy();ParityMalformedAndBudgets();
        std::cout<<"PASS native level clock/parity codecs11/13: "<<checks<<" checks, "<<refusals
            <<" refusal controls; bit-exact Double, boundary parity, optional actor order, canonical Level identity, legacy1..12 bytes and bounded two-pass decoding.\n";
        return 0;
    } catch(const std::exception& error) {std::cerr<<"FAIL native level clock codec: "<<error.what()<<" after "<<checks<<" checks\n";return 1;}
}
