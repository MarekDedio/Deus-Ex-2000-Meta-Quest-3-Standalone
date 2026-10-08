#include "quest_script_state.h"

#include <functional>
#include <iostream>
#include <limits>

namespace {
using namespace QuestVr;
using Bytes=std::vector<std::uint8_t>;
std::size_t checks{},rejections{};
void Require(bool condition,const std::string& message) {
    if (!condition) throw std::runtime_error(message);
    ++checks;
}
void Reject(const std::function<void()>& operation,const std::string& message) {
    bool rejected{}; try { operation(); } catch (const std::runtime_error&) { rejected=true; }
    Require(rejected,message); ++rejections;
}
Vm::Value Scalar(Vm::Kind kind) { Vm::Value value; value.kind=kind; return value; }
Vm::Value Integer(std::int32_t n) { auto value=Scalar(Vm::Kind::Int); value.integer=n; return value; }
Vm::Value Floating(float n) { auto value=Scalar(Vm::Kind::Float); value.floating=n; return value; }
Vm::Value Text(Vm::Kind kind,const std::string& text) { auto value=Scalar(kind); value.text=text; return value; }
bool SameFloat(float a,float b) { return std::memcmp(&a,&b,sizeof(a))==0; }
bool SameValue(const Vm::Value& a,const Vm::Value& b) {
    if (a.kind!=b.kind) return false;
    switch (a.kind) {
    case Vm::Kind::Nothing: return true;
    case Vm::Kind::Int: case Vm::Kind::Byte: return a.integer==b.integer;
    case Vm::Kind::Float: return SameFloat(a.floating,b.floating);
    case Vm::Kind::Bool: return a.boolean==b.boolean;
    case Vm::Kind::Name: case Vm::Kind::Object: case Vm::Kind::String: return a.text==b.text;
    case Vm::Kind::Rotator: return a.rotation==b.rotation;
    case Vm::Kind::Vector:
        for (std::size_t i=0; i<3u; ++i) if (!SameFloat(a.vector[i],b.vector[i])) return false;
        return true;
    case Vm::Kind::Struct:
        if (a.fields.size()!=b.fields.size()) return false;
        for (const auto& [key,value] : a.fields) {
            const auto found=b.fields.find(key);
            if (found==b.fields.end() || !SameValue(value,found->second)) return false;
        }
        return true;
    }
    return false;
}
void SameState(const StateObject& a,const StateObject& b) {
    Require(a.hasStack==b.hasStack && a.frameOverride==b.frameOverride && a.frame.has_value()==b.frame.has_value() && a.disabled==b.disabled,
        "Object stack/frame/disabled identities changed");
    if (!a.frame) return;
    const auto& x=*a.frame; const auto& y=*b.frame;
    Require(x.codePath==y.codePath && x.localsCodePath==y.localsCodePath && x.statementIndex==y.statementIndex &&
        x.latent==y.latent && x.locals.size()==y.locals.size(),"Code/local declaration/PC/latent changed");
    for (const auto& local : x.locals) {
        const auto found=std::find_if(y.locals.begin(),y.locals.end(),[&](const auto& item) { return item.key==local.key; });
        Require(found!=y.locals.end() && found->values.size()==local.values.size(),"Local identity/array changed");
        for (std::size_t i=0; i<local.values.size(); ++i)
            Require(SameValue(local.values[i],found->values[i]),"Local typed value failed bit-exact roundtrip");
    }
}
void U32(Bytes& bytes,std::uint32_t value) { for (unsigned i=0; i<4; ++i) bytes.push_back(static_cast<std::uint8_t>(value>>(8u*i))); }
void String(Bytes& bytes,const std::string& value) { U32(bytes,static_cast<std::uint32_t>(value.size())); bytes.insert(bytes.end(),value.begin(),value.end()); }
Bytes Prefix(std::uint8_t version,std::uint32_t count=1u) {
    Bytes bytes{'D','X','Q','V','M','S',version,0}; String(bytes,"Map"); U32(bytes,count); return bytes;
}
void Object(Bytes& bytes,const std::string& path="Map.A") { String(bytes,path); String(bytes,"Fixture.Actor"); U32(bytes,0); bytes.push_back(0); }
Bytes StateOnly(const Bytes& tail) {
    auto bytes=Prefix(2); Object(bytes); bytes.push_back(1); bytes.insert(bytes.end(),tail.begin(),tail.end()); return bytes;
}
Bytes NoFrame(std::uint32_t disabledStates=0u,bool overrideFrame=false) { Bytes bytes{1,static_cast<std::uint8_t>(overrideFrame),0}; U32(bytes,disabledStates); return bytes; }
Bytes Frame(std::uint32_t locals=0u,std::uint8_t latent=1u,std::uint32_t pc=0x89abcdefu,
            const std::string& code="",const std::string& localsCode="Fixture.Base.Waiting") {
    Bytes bytes{1,1,1}; String(bytes,code); String(bytes,localsCode); U32(bytes,pc); bytes.push_back(latent); U32(bytes,locals); return bytes;
}
void Local(Bytes& bytes,const std::string& key,const Bytes& value={0}) { String(bytes,key); U32(bytes,1); bytes.insert(bytes.end(),value.begin(),value.end()); }
void Disabled(Bytes& bytes,const std::string& state,const std::vector<std::string>& names) {
    String(bytes,state); U32(bytes,static_cast<std::uint32_t>(names.size())); for (const auto& name : names) String(bytes,name);
}
ScriptSavedState Saved() {
    ScriptSavedState saved; saved.mapName="Map";
    ScriptSavedObject object; object.path="Map.A"; object.classPath="Fixture.Actor";
    StateObject state; state.hasStack=true; state.frameOverride=true; StateFrame frame;
    frame.codePath="Fixture.Derived.Waiting"; frame.localsCodePath="Fixture.Base.StartUp";
    frame.statementIndex=217u; frame.latent=StateLatent::Sleep;
    std::vector<Vm::Value> values{{}};
    auto byte=Scalar(Vm::Kind::Byte); byte.integer=255; values.push_back(byte);
    values.push_back(Integer(std::numeric_limits<std::int32_t>::min()));
    auto boolean=Scalar(Vm::Kind::Bool); boolean.boolean=true; values.push_back(boolean);
    values.push_back(Floating(-0.0f)); values.push_back(Text(Vm::Kind::Name,"None"));
    values.push_back(Text(Vm::Kind::Object,"Map.A")); values.push_back(Text(Vm::Kind::String,"Legacy: \xff"));
    auto vector=Scalar(Vm::Kind::Vector); vector.vector={-0.0f,0.0f,100.25f}; values.push_back(vector);
    auto rotator=Scalar(Vm::Kind::Rotator); rotator.rotation={-65536,65536,std::numeric_limits<std::int32_t>::max()}; values.push_back(rotator);
    auto structure=Scalar(Vm::Kind::Struct); structure.fields["Time"]=Floating(-0.0f);
    auto child=Scalar(Vm::Kind::Struct); child.fields["Target"]=Text(Vm::Kind::Object,""); structure.fields["Child"]=child; values.push_back(structure);
    frame.locals.push_back({"Fixture.Base.StartUp.Zed",values});
    frame.locals.push_back({"Fixture.Base.StartUp.alpha",{Integer(17),Integer(19)}});
    frame.locals.push_back({"Fixture.Base.StartUp.Empty",{}});
    state.frame=std::move(frame);
    state.disabled["None"]={"AnimEnd","CustomEventNotInProbeTable","Tick"};
    state.disabled["Waiting"]={"Some_Future_Event"}; state.disabled["Actor"]={};
    object.state=state; saved.objects.push_back(std::move(object));
    ScriptSavedObject legacy; legacy.path="Map.Legacy"; legacy.classPath="Fixture.Actor";
    legacy.properties.push_back({"Fixture.Actor.Health","Health",0,Integer(42)}); saved.objects.push_back(std::move(legacy));
    return saved;
}
void LegacyIdentity() {
    auto literal=Prefix(1); String(literal,"Map.A"); String(literal,"Fixture.Actor"); U32(literal,1);
    String(literal,"Fixture.Actor.Health"); String(literal,"Health"); U32(literal,0);
    literal.insert(literal.end(),{2,0x78,0x56,0x34,0x12,0});
    auto saved=DecodeScriptSavedState(literal);
    Require(!saved.objects[0].state && saved.objects[0].properties[0].value.integer==0x12345678,"Legacy literal changed");
    Require(EncodeScriptSavedState(saved)==literal,"Legacy v1 byte identity was not retained");
    // The state extension follows the clock; no pre-existing byte changes
    // except the version byte. A no-frame state still requires v2.
    saved.objects[0].clock=ActorAnimationClock{}; const auto clockLegacy=EncodeScriptSavedState(saved);
    saved.objects[0].state=StateObject{}; const auto extended=EncodeScriptSavedState(saved);
    Require(extended[6]==2 && extended.size()==clockLegacy.size()+8u,"State presence did not select version2");
    for (std::size_t i=0; i<clockLegacy.size(); ++i)
        Require(i==6 || clockLegacy[i]==extended[i],"Existing clock/property byte was changed by state extension");
    const auto restored=DecodeScriptSavedState(extended);
    Require(restored.objects[0].clock && restored.objects[0].state && !restored.objects[0].state->frame,
        "Clock/state optionality was not preserved");
    saved.objects[0].state.reset(); Require(EncodeScriptSavedState(saved)==clockLegacy,"Removing all state did not restore exact v1 bytes");
    const auto empty=EncodeScriptSavedState({"Map",{}}); Require(empty[6]==1,"Empty legacy save unnecessarily selected v2");
    auto optional=Prefix(2); Object(optional); optional.push_back(0);
    Require(!DecodeScriptSavedState(optional).objects[0].state,"v2 absent-state object was not accepted");
}
void Roundtrips() {
    const auto saved=Saved(); const auto bytes=EncodeScriptSavedState(saved); const auto restored=DecodeScriptSavedState(bytes);
    Require(bytes[6]==2 && restored.objects.size()==2 && restored.objects[0].state && !restored.objects[1].state,
        "Mixed state/legacy object optionality changed");
    SameState(*saved.objects[0].state,*restored.objects[0].state);
    Require(restored.objects[0].state->frame->locals.front().key=="Fixture.Base.StartUp.alpha",
        "Locals were not sorted by folded identity");
    Require(EncodeScriptSavedState(restored)==bytes,"v2 decode/reencode was not deterministic");
    auto reordered=saved; std::reverse(reordered.objects.begin(),reordered.objects.end());
    std::reverse(reordered.objects[1].state->frame->locals.begin(),reordered.objects[1].state->frame->locals.end());
    Require(EncodeScriptSavedState(reordered)==bytes,"Input object/local order changes the save");
    // State-only record, code-cleared frame, separate local declaration and
    // nonzero PC are structural facts, not grounds for invented resets.
    auto tail=Frame(); U32(tail,0); const auto literal=StateOnly(tail); const auto cleared=DecodeScriptSavedState(literal);
    const auto& frame=*cleared.objects[0].state->frame;
    Require(frame.codePath.empty() && frame.localsCodePath=="Fixture.Base.Waiting" && frame.statementIndex==0x89abcdefu &&
        frame.latent==StateLatent::Stop,"Cleared stopped frame lost code identity/PC/latent");
    Require(cleared.objects[0].properties.empty() && !cleared.objects[0].clock,"State-only record gained unrelated payload");
    Require(EncodeScriptSavedState(cleared)==literal,"Literal state-only frame bytes changed");
    for (std::uint8_t latent=0; latent<=11u; ++latent) {
        auto item=Saved(); auto& state=*item.objects[0].state; state.hasStack=(latent%2u)!=0;
        state.frame->latent=static_cast<StateLatent>(latent); state.frame->statementIndex=std::numeric_limits<std::uint32_t>::max();
        auto roundtrip=DecodeScriptSavedState(EncodeScriptSavedState(item)); SameState(state,*roundtrip.objects[0].state);
    }
    for (const bool stack : {false,true}) for (const bool overrideFrame : {false,true}) {
        auto item=Saved(); item.objects[0].state->hasStack=stack; item.objects[0].state->frameOverride=overrideFrame;
        item.objects[0].state->frame.reset();
        auto roundtrip=DecodeScriptSavedState(EncodeScriptSavedState(item)); SameState(*item.objects[0].state,*roundtrip.objects[0].state);
    }
    // Immutable authored dormant context may own dynamic disabled sets without
    // inventing portable class-local storage or silently clearing that context.
    tail=NoFrame(2); Disabled(tail,"alpha",{"Beta","CustomEvent"}); Disabled(tail,"Beta",{"alpha","Zulu"});
    const auto dormant=DecodeScriptSavedState(StateOnly(tail));
    Require(dormant.objects[0].state->hasStack && !dormant.objects[0].state->frameOverride && !dormant.objects[0].state->frame &&
        dormant.objects[0].state->disabled.at("alpha").count("CustomEvent")==1,"Dormant authored frame context was fabricated");
    Require(EncodeScriptSavedState(dormant)==StateOnly(tail),"Fold-ordered disabled-state/event literal bytes changed");
    tail=Frame(0,0,73,"",""); U32(tail,0);
    const auto clearedContinue=DecodeScriptSavedState(StateOnly(tail));
    Require(clearedContinue.objects[0].state->frameOverride && clearedContinue.objects[0].state->frame->statementIndex==73 &&
        clearedContinue.objects[0].state->frame->latent==StateLatent::Continue &&
        clearedContinue.objects[0].state->frame->localsCodePath.empty(),"Cleared Continue frame was reset or fabricated");
}
void MalformedStreams() {
    const auto bytes=EncodeScriptSavedState(Saved());
    for (std::size_t i=0; i<bytes.size(); ++i) {
        const Bytes truncated(bytes.begin(),bytes.begin()+i);
        Reject([&] { DecodeScriptSavedState(truncated); },"Truncated v2 state accepted");
    }
    auto broken=bytes; broken.push_back(0); Reject([&] { DecodeScriptSavedState(broken); },"Trailing v2 byte accepted");
    for (const std::uint8_t version : {0u,3u,255u}) {
        broken=bytes; broken[6]=version; Reject([&] { DecodeScriptSavedState(broken); },"Unknown codec version accepted");
    }
    for (std::size_t offset=0; offset<3u; ++offset) {
        auto tail=NoFrame(); tail[offset]=2;
        Reject([&] { DecodeScriptSavedState(StateOnly(tail)); },"Bad state stack/override/frame bool accepted");
    }
    broken=Prefix(2); Object(broken); broken.push_back(2);
    Reject([&] { DecodeScriptSavedState(broken); },"Bad has-state bool accepted");
    auto tail=Frame(0,255); U32(tail,0);
    Reject([&] { DecodeScriptSavedState(StateOnly(tail)); },"Unknown latent ordinal accepted");
    tail=Frame(); tail[1]=0; U32(tail,0);
    Reject([&] { DecodeScriptSavedState(StateOnly(tail)); },"Portable frame accepted without frame override");
    tail=Frame(0xffffffffu); U32(tail,0);
    Reject([&] { DecodeScriptSavedState(StateOnly(tail)); },"Unbounded local count accepted");
    tail=Frame(1); String(tail,"Fixture.Value"); U32(tail,0xffffffffu); U32(tail,0);
    Reject([&] { DecodeScriptSavedState(StateOnly(tail)); },"Unbounded local element count accepted");
    tail=NoFrame(0xffffffffu);
    Reject([&] { DecodeScriptSavedState(StateOnly(tail)); },"Unbounded disabled-state count accepted");
    tail=NoFrame(1); String(tail,"None"); U32(tail,0xffffffffu);
    Reject([&] { DecodeScriptSavedState(StateOnly(tail)); },"Unbounded disabled-name count accepted");
    for (const auto& keys : {std::array<std::string,2>{"A","A"},std::array<std::string,2>{"A","a"},
                            std::array<std::string,2>{"Z","A"}}) {
        tail=Frame(2); Local(tail,keys[0]); Local(tail,keys[1]); U32(tail,0);
        Reject([&] { DecodeScriptSavedState(StateOnly(tail)); },"Duplicate/colliding/noncanonical local accepted");
        tail=NoFrame(2); Disabled(tail,keys[0],{}); Disabled(tail,keys[1],{});
        Reject([&] { DecodeScriptSavedState(StateOnly(tail)); },"Duplicate/colliding/noncanonical disabled state accepted");
        tail=NoFrame(1); Disabled(tail,"None",{keys[0],keys[1]});
        Reject([&] { DecodeScriptSavedState(StateOnly(tail)); },"Duplicate/colliding/noncanonical disabled name accepted");
    }
    for (const Bytes& value : {Bytes{255},Bytes{3,2},Bytes{4,0,0,0xc0,0x7f},Bytes{10,255,255,255,255}}) {
        tail=Frame(1); Local(tail,"Fixture.Value",value); U32(tail,0);
        Reject([&] { DecodeScriptSavedState(StateOnly(tail)); },"Malformed local value accepted");
    }
    for (const auto& identity : {std::string(""),std::string("bad\nname"),std::string("A\0B",3)}) {
        tail=NoFrame(1); Disabled(tail,identity,{});
        Reject([&] { DecodeScriptSavedState(StateOnly(tail)); },"Malformed disabled-state name accepted");
        tail=NoFrame(1); Disabled(tail,"None",{identity});
        Reject([&] { DecodeScriptSavedState(StateOnly(tail)); },"Malformed disabled-event name accepted");
        tail=Frame(1); Local(tail,identity); U32(tail,0);
        Reject([&] { DecodeScriptSavedState(StateOnly(tail)); },"Malformed local identity accepted");
    }
}
void InvalidInputsAndBudgets() {
    const auto saved=Saved(); const auto bytes=EncodeScriptSavedState(saved);
    const auto encodeReject=[](const ScriptSavedState& state,const std::string& message) {
        Reject([&] { EncodeScriptSavedState(state); },message);
    };
    auto broken=saved; auto& frame=*broken.objects[0].state->frame;
    frame.latent=static_cast<StateLatent>(255); encodeReject(broken,"Unknown in-memory latent accepted");
    broken=saved; broken.objects[0].state->frameOverride=false; encodeReject(broken,"In-memory frame accepted without override");
    broken=saved; broken.objects[0].state->frame->locals.push_back(broken.objects[0].state->frame->locals[0]);
    encodeReject(broken,"Duplicate in-memory local accepted");
    broken=saved; broken.objects[0].state->frame->locals.push_back({"fixture.base.startup.zed",{}});
    encodeReject(broken,"Case-colliding in-memory local accepted");
    broken=saved; broken.objects[0].state->disabled["none"]={}; encodeReject(broken,"Case-colliding disabled states accepted");
    broken=saved; broken.objects[0].state->disabled["None"].insert("animend"); encodeReject(broken,"Case-colliding disabled names accepted");
    broken=saved; broken.objects[0].state->frame->codePath="Bad\nCode"; encodeReject(broken,"Invalid code identity accepted");
    broken=saved; broken.objects[0].state->frame->localsCodePath=std::string("Bad\0Code",8);
    encodeReject(broken,"Invalid local declaration identity accepted");
    broken=saved; broken.objects[0].state->frame->locals[0].values[4].floating=std::numeric_limits<float>::infinity();
    encodeReject(broken,"Non-finite local accepted");
    broken=saved; broken.objects[0].state->disabled[""]={}; encodeReject(broken,"Empty disabled-state identity accepted");
    broken=saved; broken.objects[0].state->disabled["None"].insert(""); encodeReject(broken,"Empty disabled-name identity accepted");
    for (std::size_t mode=0; mode<9u; ++mode) {
        ScriptStateLimits limits;
        if (mode==0) limits.maxStateLocals=2;
        if (mode==1) limits.maxLocalElements=2;
        if (mode==2) limits.maxDisabledStates=2;
        if (mode==3) limits.maxDisabledNames=2;
        if (mode==4) limits.maxBytes=bytes.size()-1u;
        if (mode==5) limits.totalValueNodes=2;
        if (mode==6) limits.maxStringBytes=8;
        if (mode==7) limits.maxDepth=2;
        if (mode==8) limits.maxObjects=1;
        Reject([&] { EncodeScriptSavedState(saved,limits); },"State encode budget not enforced");
        Reject([&] { DecodeScriptSavedState(bytes,limits); },"State decode budget not enforced");
    }
    // Aggregate counters include all actors, not merely an individual frame.
    auto aggregate=saved; aggregate.objects[1].state=aggregate.objects[0].state;
    const auto aggregateBytes=EncodeScriptSavedState(aggregate);
    for (std::size_t mode=0; mode<4u; ++mode) {
        ScriptStateLimits limits;
        if (mode==0) limits.maxStateLocals=3;
        if (mode==1) limits.maxLocalElements=13;
        if (mode==2) limits.maxDisabledStates=3;
        if (mode==3) limits.maxDisabledNames=4;
        Reject([&] { EncodeScriptSavedState(aggregate,limits); },"State count budget checked only per object on encode");
        Reject([&] { DecodeScriptSavedState(aggregateBytes,limits); },"State count budget checked only per object on decode");
    }
    auto lean=ScriptSavedState{"Map",{{"Map.A","Fixture.Actor",{},{},StateObject{}}}};
    const auto leanBytes=EncodeScriptSavedState(lean); ScriptStateLimits retained; retained.maxBytes=leanBytes.size()+1;
    Reject([&] { EncodeScriptSavedState(lean,retained); },"Retained state object budget missing from encode");
    Reject([&] { DecodeScriptSavedState(leanBytes,retained); },"Retained state object budget missing from decode");
}
void StandaloneMeasurement() {
    const auto saved=Saved(); const auto before=EncodeScriptSavedState(saved);
    const auto& state=*saved.objects[0].state;
    ScriptStateLimits limits;
    ScriptStateDetail::Writer measured(limits,nullptr);
    measured.MeasureStateObject(state);
    const auto firstSize=measured.size();
    Require(firstSize>0u,"Standalone measurement did not count encoded state bytes");
    measured.MeasureStateObject(state);
    Require(measured.size()==firstSize*2u,"Standalone measurement did not share cumulative encoded byte count");
    Require(EncodeScriptSavedState(saved)==before,"Standalone measurement mutated input state or serialized data");
    // Retained standalone object overhead is charged even for empty objects,
    // not merely their tiny encoded hasStack/override/frame/count fields.
    limits.maxBytes=sizeof(StateObject)*2u-1u;
    ScriptStateDetail::Writer retained(limits,nullptr);
    retained.MeasureStateObject(StateObject{});
    Reject([&] { retained.MeasureStateObject(StateObject{}); },"Standalone retained byte budget did not accumulate");
    limits=ScriptStateLimits{}; limits.maxDisabledNames=4u;
    ScriptStateDetail::Writer names(limits,nullptr);
    names.MeasureStateObject(state);
    Reject([&] { names.MeasureStateObject(state); },"Standalone disabled-name count budget did not accumulate");
    limits=ScriptStateLimits{}; limits.maxStateLocals=3u;
    ScriptStateDetail::Writer locals(limits,nullptr);
    locals.MeasureStateObject(state);
    Reject([&] { locals.MeasureStateObject(state); },"Standalone local count budget did not accumulate");
    auto malformed=state; malformed.frameOverride=false;
    ScriptStateDetail::Writer invalid(limits,nullptr);
    Reject([&] { invalid.MeasureStateObject(malformed); },"Standalone measurement bypassed frame override validation");
    malformed=state; malformed.frame->latent=static_cast<StateLatent>(255);
    ScriptStateDetail::Writer latent(limits,nullptr);
    Reject([&] { latent.MeasureStateObject(malformed); },"Standalone measurement bypassed latent validation");
    malformed=state; malformed.disabled["none"]={};
    ScriptStateDetail::Writer collision(limits,nullptr);
    Reject([&] { collision.MeasureStateObject(malformed); },"Standalone measurement bypassed folded duplicate validation");
    Bytes emitted{17,23}; ScriptStateDetail::Writer emitting(limits,&emitted);
    Reject([&] { emitting.MeasureStateObject(state); },"Emitting writer accepted state-only measurement");
    Require(emitting.size()==0u && emitted==Bytes({17,23}),"Rejected measurement modified emitting writer or output");
}
} // namespace

int main() {
    try {
        LegacyIdentity(); Roundtrips(); MalformedStreams(); InvalidInputsAndBudgets(); StandaloneMeasurement();
        std::cout<<"PASS state-frame codec controls="<<checks<<" rejection controls="<<rejections
            <<"; structural continuation data only, no runtime schema/state scheduler implied\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr<<"FAIL state-frame codec: "<<error.what()<<" after "<<checks<<" controls\n"; return 1;
    }
}
