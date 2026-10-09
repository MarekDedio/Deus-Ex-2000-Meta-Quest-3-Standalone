#include "quest_script_state.h"

#include <iostream>
#include <functional>
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
    bool rejected{};
    try { operation(); } catch (const std::runtime_error&) { rejected=true; }
    Require(rejected,message); ++rejections;
}
Vm::Value Scalar(Vm::Kind kind) { Vm::Value value; value.kind=kind; return value; }
Vm::Value Integer(std::int32_t n) { auto value=Scalar(Vm::Kind::Int); value.integer=n; return value; }
Vm::Value Floating(float n) { auto value=Scalar(Vm::Kind::Float); value.floating=n; return value; }
Vm::Value Text(Vm::Kind kind,const std::string& text) { auto value=Scalar(kind); value.text=text; return value; }
bool SameFloat(float a,float b) { return std::memcmp(&a,&b,sizeof(a))==0; }
bool SameValue(const Vm::Value& a,const Vm::Value& b) {
    if (a.kind!=b.kind) return false;
    switch(a.kind) {
    case Vm::Kind::Nothing: return true;
    case Vm::Kind::Byte: case Vm::Kind::Int: return a.integer==b.integer;
    case Vm::Kind::Bool: return a.boolean==b.boolean;
    case Vm::Kind::Float: return SameFloat(a.floating,b.floating);
    case Vm::Kind::Name: case Vm::Kind::Object: case Vm::Kind::String: return a.text==b.text;
    case Vm::Kind::Rotator: return a.rotation==b.rotation;
    case Vm::Kind::Vector:
        for (std::size_t i=0;i<3;++i) if (!SameFloat(a.vector[i],b.vector[i])) return false;
        return true;
    case Vm::Kind::Struct:
        if(a.fields.size()!=b.fields.size()) return false;
        for(const auto& [key,value] : a.fields) {
            const auto found=b.fields.find(key);
            if(found==b.fields.end() || !SameValue(value,found->second)) return false;
        }
        return true;
    }
    return false;
}
void SameChannel(const MeshAnimationChannel& a,const MeshAnimationChannel& b) {
    Require(a.sequence==b.sequence && SameFloat(a.normalizedFrame,b.normalizedFrame),"Clock channel sequence/frame changed");
    Require(a.previous.vertexOffset0==b.previous.vertexOffset0 && a.previous.vertexOffset1==b.previous.vertexOffset1 &&
        SameFloat(a.previous.fraction,b.previous.fraction),"Native previous-frame metadata changed");
}
void SameClock(const ActorAnimationClock& a,const ActorAnimationClock& b) {
    SameChannel(a.pose.main,b.pose.main);
    Require(SameFloat(a.main.rate,b.main.rate) && SameFloat(a.main.last,b.main.last) &&
        SameFloat(a.main.minRate,b.main.minRate) && SameFloat(a.main.tweenRate,b.main.tweenRate) &&
        SameFloat(a.main.oldRate,b.main.oldRate),"Main clock rates changed");
    Require(a.main.loop==b.main.loop && a.main.notify==b.main.notify && a.main.finished==b.main.finished &&
        a.main.finishAnimWaiting==b.main.finishAnimWaiting,"Main clock/latent flags changed");
    for(std::size_t i=0;i<4;++i) {
        SameChannel(a.pose.blends[i],b.pose.blends[i]); const auto& x=a.blends[i]; const auto& y=b.blends[i];
        Require(SameFloat(x.rate,y.rate) && SameFloat(x.last,y.last) && SameFloat(x.minRate,y.minRate) &&
            SameFloat(x.tweenRate,y.tweenRate) && SameFloat(x.oldRate,y.oldRate),"Blend clock rates changed");
        for(std::size_t j=0;j<4;++j) Require(SameFloat(x.simulated[j],y.simulated[j]),"Blend simulated property changed");
    }
    Require(std::memcmp(&a.simulationTime,&b.simulationTime,sizeof(double))==0 && a.remoteRole==b.remoteRole &&
        a.pose.fatness==b.pose.fatness,"Clock time/role/fatness changed");
}
ActorAnimationClock Clock() {
    ActorAnimationClock clock;
    clock.pose.main={"Primary",-0.125f,{0xffffffffu,123456u,4.0f}};
    clock.main={-2.5f,0.875f,-0.125f,1.25f,0.75f,true,false,true,true};
    for(std::size_t i=0;i<4;++i) {
        clock.pose.blends[i]={i==0 ? "None" : "Blend"+std::to_string(i),i==3 ? 1.0f : -0.25f,
            {static_cast<std::uint32_t>(i*128u),static_cast<std::uint32_t>(i*64u),i==0 ? -1.0f : static_cast<float>(i)}};
        auto& blend=clock.blends[i]; blend.rate=-static_cast<float>(i+1u); blend.last=0.75f;
        blend.minRate=0.5f; blend.tweenRate=1.25f; blend.oldRate=2.25f;
        blend.simulated={-3.25f,static_cast<float>(i),1234.5f,0.125f};
    }
    clock.simulationTime=123456789.125; clock.remoteRole=4; clock.pose.fatness=255;
    return clock;
}
ScriptSavedState State() {
    ScriptSavedState state; state.mapName="FixtureMap";
    ScriptSavedObject first; first.path="FixtureMap.ActorZ"; first.classPath="Fixture.Pawn"; first.clock=Clock();
    std::vector<Vm::Value> values;
    values.push_back({}); auto byte=Scalar(Vm::Kind::Byte);byte.integer=255;values.push_back(byte);
    values.push_back(Integer(std::numeric_limits<std::int32_t>::min()));
    auto boolean=Scalar(Vm::Kind::Bool);boolean.boolean=true;values.push_back(boolean);
    values.push_back(Floating(-0.0f)); values.push_back(Text(Vm::Kind::Name,"None"));
    values.push_back(Text(Vm::Kind::Object,"FixtureMap.ActorA"));
    values.push_back(Text(Vm::Kind::String,std::string("UTF-8: \xc5\xbc; legacy: \xff")));
    auto vector=Scalar(Vm::Kind::Vector);vector.vector={-0.0f,-0.125f,12345.5f};values.push_back(vector);
    auto rotation=Scalar(Vm::Kind::Rotator);rotation.rotation={-2147483647,65536,2147483647};values.push_back(rotation);
    auto structure=Scalar(Vm::Kind::Struct); structure.fields["Beta"]=values[8];
    auto nested=Scalar(Vm::Kind::Struct);nested.fields["Text"]=Text(Vm::Kind::String,"nested");
    nested.fields["NoneObject"]=Text(Vm::Kind::Object,"");structure.fields["Alpha"]=nested;values.push_back(structure);
    values.push_back(Text(Vm::Kind::Name,""));values.push_back(Text(Vm::Kind::Object,""));
    for(std::size_t i=0;i<values.size();++i) {
        const auto name="Property"+std::to_string(i);
        first.properties.push_back({"Fixture.Pawn."+name,name,static_cast<std::uint32_t>(i),values[i]});
    }
    first.properties.push_back({"Fixture.Pawn.Fixed","Fixed",0,Integer(7)});
    first.properties.push_back({"Fixture.Pawn.Fixed","Fixed",1,Integer(8)});
    state.objects.push_back(std::move(first));
    ScriptSavedObject second;second.path="FixtureMap.ActorA";second.classPath="Fixture.Actor";
    second.properties.push_back({"Fixture.Actor.Label","Label",0,Text(Vm::Kind::String,"untouched")});
    state.objects.push_back(std::move(second)); return state;
}
void U32(Bytes& bytes,std::uint32_t value) { for(unsigned i=0;i<4;++i) bytes.push_back(static_cast<std::uint8_t>(value>>(8u*i))); }
void String(Bytes& bytes,const std::string& text) { U32(bytes,static_cast<std::uint32_t>(text.size()));bytes.insert(bytes.end(),text.begin(),text.end()); }
Bytes Prefix(std::uint32_t objects) {
    Bytes bytes(ScriptStateDetail::Magic.begin(),ScriptStateDetail::Magic.end());String(bytes,"Map");U32(bytes,objects);return bytes;
}
void Object(Bytes& bytes,const std::string& path,std::uint32_t properties) { String(bytes,path);String(bytes,"Fixture.Actor");U32(bytes,properties); }
void Property(Bytes& bytes,const std::string& key,const std::string& name,std::uint32_t index) { String(bytes,key);String(bytes,name);U32(bytes,index); }
Bytes OneValue(const Bytes& value) {
    auto bytes=Prefix(1);Object(bytes,"Map.A",1);Property(bytes,"Fixture.Actor.Value","Value",0);
    bytes.insert(bytes.end(),value.begin(),value.end());bytes.push_back(0);return bytes;
}
void RoundtripAndDeterminism() {
    const auto state=State();const auto bytes=EncodeScriptSavedState(state);const auto restored=DecodeScriptSavedState(bytes);
    Require(restored.mapName==state.mapName && restored.objects.size()==2,"Saved root/map changed");
    Require(restored.objects[0].path=="FixtureMap.ActorA" && restored.objects[1].path=="FixtureMap.ActorZ",
        "Objects are not canonical sorted by identity");
    for(const auto& original : state.objects) {
        const auto found=std::find_if(restored.objects.begin(),restored.objects.end(),[&](const auto& object) { return object.path==original.path; });
        Require(found!=restored.objects.end() && found->classPath==original.classPath &&
            found->properties.size()==original.properties.size(),"Actor identity/class/property count changed");
        for(const auto& property : original.properties) {
            const auto p=std::find_if(found->properties.begin(),found->properties.end(),[&](const auto& saved) {
                return saved.key==property.key && saved.index==property.index;
            });
            Require(p!=found->properties.end() && p->name==property.name && SameValue(p->value,property.value),
                "Typed property or array slot failed exact roundtrip: "+property.name);
        }
        Require(found->clock.has_value()==original.clock.has_value(),"Clock optionality changed");
        if(original.clock) SameClock(*original.clock,*found->clock);
    }
    Require(EncodeScriptSavedState(restored)==bytes,"Decode/reencode is not byte-deterministic");
    auto reordered=state;std::reverse(reordered.objects.begin(),reordered.objects.end());
    for(auto& object : reordered.objects) std::reverse(object.properties.begin(),object.properties.end());
    Require(EncodeScriptSavedState(reordered)==bytes,"Input object/property iteration order changes the save");
    auto empty=ScriptSavedState{"Map",{}};
    Require(DecodeScriptSavedState(EncodeScriptSavedState(empty)).objects.empty(),"Empty script state failed roundtrip");
    // Explicit format tags and little-endian integers are verified without
    // deriving them from Vm::Kind enum ordinals or machine struct layout.
    const auto literal=OneValue({2,0x78,0x56,0x34,0x12});
    const auto parsed=DecodeScriptSavedState(literal);
    Require(parsed.objects[0].properties[0].value.kind==Vm::Kind::Int &&
        parsed.objects[0].properties[0].value.integer==0x12345678,"Explicit little-endian Int tag contract changed");
    Require(EncodeScriptSavedState(parsed)==literal,"Canonical synthetic format bytes changed");
}
void MalformedStreams() {
    const auto bytes=EncodeScriptSavedState(State());
    for(std::size_t length=0;length<bytes.size();++length) {
        const Bytes truncated(bytes.begin(),bytes.begin()+length);
        Reject([&] { DecodeScriptSavedState(truncated); },"Truncated saved state was accepted");
    }
    auto broken=bytes;broken.push_back(0);Reject([&] { DecodeScriptSavedState(broken); },"Trailing save bytes accepted");
    broken=bytes;broken[0]^=1;Reject([&] { DecodeScriptSavedState(broken); },"Bad save magic accepted");
    broken=bytes;broken[6]=6;Reject([&] { DecodeScriptSavedState(broken); },"Unsupported codec version accepted");
    for(const Bytes& value : {Bytes{255},Bytes{3,2},Bytes{4,0,0,0xc0,0x7f},Bytes{4,0,0,0x80,0x7f},
        Bytes{8,0,0,0x80,0x7f,0,0,0,0,0,0,0,0}}) {
        broken=OneValue(value);Reject([&] { DecodeScriptSavedState(broken); },"Bad value tag/bool/non-finite payload accepted");
    }
    broken=Prefix(0xffffffffu);Reject([&] { DecodeScriptSavedState(broken); },"Unbounded object count accepted");
    broken=Prefix(1);Object(broken,"Map.A",0xffffffffu);
    Reject([&] { DecodeScriptSavedState(broken); },"Unbounded property count accepted");
    broken=OneValue({10,255,255,255,255});Reject([&] { DecodeScriptSavedState(broken); },"Unbounded struct count accepted");
    broken=Prefix(1);Object(broken,"Map.A",0);broken.push_back(2);
    Reject([&] { DecodeScriptSavedState(broken); },"Invalid optional clock boolean accepted");
    for(const auto& paths : {std::array<std::string,2>{"Map.A","Map.A"},
        std::array<std::string,2>{"Map.A","map.a"},std::array<std::string,2>{"Map.Z","Map.A"}}) {
        broken=Prefix(2);for(const auto& path : paths) { Object(broken,path,0);broken.push_back(0); }
        Reject([&] { DecodeScriptSavedState(broken); },"Duplicate/case-colliding/noncanonical object path accepted");
    }
    for(const bool collision : {false,true}) {
        broken=Prefix(1);Object(broken,"Map.A",2);Property(broken,"Fixture.Value","Value",0);broken.push_back(0);
        Property(broken,collision ? "fixture.value" : "Fixture.Value","Value",collision ? 1u : 0u);
        broken.push_back(0);broken.push_back(0);
        Reject([&] { DecodeScriptSavedState(broken); },"Duplicate/case-colliding property accepted");
        Bytes fields{10};U32(fields,2);String(fields,"Alpha");fields.push_back(0);
        String(fields,collision ? "alpha" : "Alpha");fields.push_back(0);broken=OneValue(fields);
        Reject([&] { DecodeScriptSavedState(broken); },"Duplicate/case-colliding struct field accepted");
    }
}
void InvalidStateAndBudgets() {
    auto state=State();const auto valid=EncodeScriptSavedState(state);
    const auto rejection=[&](ScriptSavedState broken,const std::string& message) {
        Reject([&] { EncodeScriptSavedState(broken); },message);
    };
    auto broken=state;broken.objects.push_back(broken.objects[0]);rejection(broken,"Duplicate input object accepted");
    broken=state;broken.objects[1].path="fixturemap.actorz";rejection(broken,"Case-colliding input object accepted");
    broken=state;broken.objects[0].properties.push_back(broken.objects[0].properties[0]);rejection(broken,"Duplicate input property accepted");
    broken=state;auto collision=broken.objects[0].properties[0];collision.key="fixture.pawn.property0";collision.index=999;
    broken.objects[0].properties.push_back(collision);rejection(broken,"Case-colliding input property accepted");
    broken=state;auto nested=Scalar(Vm::Kind::Struct);nested.fields["Alpha"]=Integer(1);nested.fields["alpha"]=Integer(2);
    broken.objects[0].properties[0].value=nested;rejection(broken,"Case-colliding input field accepted");
    broken=state;broken.mapName=std::string(129,'A');rejection(broken,"Overlong map identity accepted");
    broken=state;broken.objects[0].path="Bad\nPath";rejection(broken,"Nonprintable actor identity accepted");
    broken=state;broken.objects[0].properties[7].value=Text(Vm::Kind::String,std::string("bad\0text",8));
    rejection(broken,"NUL value string accepted");
    broken=state;broken.objects[0].properties[1].value.integer=256;rejection(broken,"Byte outside retained type range accepted");
    broken=state;broken.objects[0].properties[4].value=Floating(std::numeric_limits<float>::quiet_NaN());
    rejection(broken,"Non-finite typed value accepted");
    for(std::size_t mode=0;mode<6;++mode) {
        ScriptStateLimits limits;
        if(mode==0) limits.maxBytes=valid.size()-1;
        if(mode==1) limits.maxObjects=1;
        if(mode==2) limits.maxProperties=1;
        if(mode==3) limits.totalValueNodes=1;
        if(mode==4) limits.maxStringBytes=2;
        if(mode==5) limits.maxDepth=1;
        Reject([&] { EncodeScriptSavedState(state,limits); },"Encode budget is not enforced");
        Reject([&] { DecodeScriptSavedState(valid,limits); },"Decode budget is not enforced");
    }
    // Each actor individually fits; the second crosses the TOTAL count.
    ScriptStateLimits aggregate;aggregate.maxProperties=state.objects[0].properties.size();
    Reject([&] { EncodeScriptSavedState(state,aggregate); },"Property budget was only per actor during encode");
    Reject([&] { DecodeScriptSavedState(valid,aggregate); },"Property budget was only per actor during decode");
    auto lean=ScriptSavedState{"Map",{{"Map.A","Fixture.Actor",{{"Fixture.Actor.Value","Value",0,{}}},{}}}};
    const auto leanBytes=EncodeScriptSavedState(lean);ScriptStateLimits retained;retained.maxBytes=leanBytes.size()+1u;
    Reject([&] { DecodeScriptSavedState(leanBytes,retained); },"Small encoded payload bypassed retained-object allocation budget");
    Reject([&] { EncodeScriptSavedState(lean,retained); },"Encode did not check eventual retained-state budget");
    auto deep=Scalar(Vm::Kind::Nothing);
    for(std::size_t i=0;i<8;++i) { auto parent=Scalar(Vm::Kind::Struct);parent.fields["Child"]=std::move(deep);deep=std::move(parent); }
    lean.objects[0].properties[0].value=std::move(deep);const auto deepBytes=EncodeScriptSavedState(lean);
    ScriptStateLimits depth;depth.maxDepth=8;
    Reject([&] { EncodeScriptSavedState(lean,depth); },"Nested encode depth limit is off by one");
    Reject([&] { DecodeScriptSavedState(deepBytes,depth); },"Nested decode depth limit is off by one");
    for(std::size_t mode=0;mode<8;++mode) {
        broken=state;auto& clock=*broken.objects[0].clock;
        if(mode==0) clock.pose.main.normalizedFrame=1.001f;
        if(mode==1) clock.main.last=1;
        if(mode==2) clock.main.tweenRate=-1;
        if(mode==3) clock.blends[3].simulated[2]=std::numeric_limits<float>::infinity();
        if(mode==4) clock.simulationTime=-1;
        if(mode==5) clock.simulationTime=std::numeric_limits<double>::quiet_NaN();
        if(mode==6) clock.pose.blends[2].previous.fraction=std::numeric_limits<float>::quiet_NaN();
        if(mode==7) clock.blends[1].last=-0.1f;
        rejection(broken,"Invalid native clock field accepted");
    }
    // Decode validates clock structure too, not merely float finiteness. The
    // final10bytes are double simulationTime, byte role and byte fatness.
    broken=ScriptSavedState{"Map",{{"Map.A","Fixture.Actor",{},Clock()}}};
    auto clockBytes=EncodeScriptSavedState(broken);const double negative=-1;std::uint64_t bits;std::memcpy(&bits,&negative,8);
    for(unsigned i=0;i<8;++i) clockBytes[clockBytes.size()-10u+i]=static_cast<std::uint8_t>(bits>>(8u*i));
    Reject([&] { DecodeScriptSavedState(clockBytes); },"Negative decoded simulation time accepted");
    clockBytes=EncodeScriptSavedState(broken);
    const double nonfinite=std::numeric_limits<double>::infinity();std::memcpy(&bits,&nonfinite,8);
    for(unsigned i=0;i<8;++i) clockBytes[clockBytes.size()-10u+i]=static_cast<std::uint8_t>(bits>>(8u*i));
    Reject([&] { DecodeScriptSavedState(clockBytes); },"Non-finite decoded simulation time accepted");
}
Bytes DefaultsPrefix(std::uint32_t classes=1u) {
    auto bytes=Prefix(0u);bytes[6]=ScriptStateDetail::ClassDefaultsVersion;U32(bytes,classes);return bytes;
}
void Defaults(Bytes& bytes,const std::string& cls,std::uint32_t properties) {
    String(bytes,cls);U32(bytes,properties);
}
Bytes DefaultsValue(const Bytes& value) {
    auto bytes=DefaultsPrefix();Defaults(bytes,"Fixture.Actor",1u);
    Property(bytes,"Fixture.Actor.Value","Value",0u);bytes.insert(bytes.end(),value.begin(),value.end());return bytes;
}
ScriptSavedState DefaultsState() {
    auto saved=State();
    StateObject state;state.hasStack=true;state.frameOverride=true;StateFrame frame;
    frame.codePath="Fixture.Pawn.Waiting";frame.localsCodePath=frame.codePath;
    frame.statementIndex=73u;frame.latent=StateLatent::Stop;
    frame.locals.push_back({"Fixture.Pawn.Waiting.Target",{Text(Vm::Kind::Object,"FixtureMap.ActorA")}});
    state.frame=std::move(frame);state.disabled["Waiting"]={"Tick"};saved.objects[0].state=std::move(state);
    saved.classDefaults.push_back({"Fixture.Pawn",saved.objects[0].properties});
    saved.classDefaults.push_back({"Fixture.Actor",{{"Fixture.Actor.Shared","Shared",0u,Integer(42)}}});
    return saved;
}
void DefaultsRoundtripAndLegacy() {
    const auto saved=DefaultsState();const auto bytes=EncodeScriptSavedState(saved);
    const auto restored=DecodeScriptSavedState(bytes);
    Require(bytes[6]==3u && restored.classDefaults.size()==2u && restored.objects.size()==2u,
        "Shared defaults did not select codec3 or changed actor count");
    Require(restored.classDefaults[0].classPath=="Fixture.Actor" && restored.classDefaults[1].classPath=="Fixture.Pawn",
        "Shared defaults classes are not canonical");
    for (const auto& original : saved.classDefaults) {
        const auto found=std::find_if(restored.classDefaults.begin(),restored.classDefaults.end(),[&](const auto& item) {
            return item.classPath==original.classPath;
        });
        Require(found!=restored.classDefaults.end() && found->properties.size()==original.properties.size(),
            "Shared defaults class/property count changed");
        for (const auto& property : original.properties) {
            const auto value=std::find_if(found->properties.begin(),found->properties.end(),[&](const auto& item) {
                return item.key==property.key && item.index==property.index;
            });
            Require(value!=found->properties.end() && value->name==property.name && SameValue(value->value,property.value),
                "Shared defaults typed value/array slot changed: "+property.name);
        }
    }
    const auto& actor=restored.objects[1];
    Require(actor.clock && actor.state && actor.state->hasStack && actor.state->frameOverride && actor.state->frame &&
        actor.state->frame->statementIndex==73u && actor.state->frame->latent==StateLatent::Stop &&
        actor.state->frame->codePath=="Fixture.Pawn.Waiting" && actor.state->frame->localsCodePath=="Fixture.Pawn.Waiting" &&
        actor.state->disabled.at("Waiting").count("Tick")==1u &&
        actor.state->frame->locals[0].values[0].text=="FixtureMap.ActorA",
        "Codec3 changed existing actor state/frame/local data");
    SameClock(*saved.objects[0].clock,*actor.clock);
    Require(!restored.objects[0].clock && !restored.objects[0].state,"Codec3 fabricated absent actor clock/state");
    Require(EncodeScriptSavedState(restored)==bytes,"Codec3 decode/reencode is not byte deterministic");
    auto reordered=saved;std::reverse(reordered.objects.begin(),reordered.objects.end());
    std::reverse(reordered.classDefaults.begin(),reordered.classDefaults.end());
    for (auto& item : reordered.classDefaults) std::reverse(item.properties.begin(),item.properties.end());
    Require(EncodeScriptSavedState(reordered)==bytes,"Shared class/default property input order changes the save");
    // Explicit class-only bytes contain neither actor records nor clock/state
    // flags after the default properties. They reuse the stable Int tag.
    const auto literal=DefaultsValue({2u,0x78u,0x56u,0x34u,0x12u});
    const auto parsed=DecodeScriptSavedState(literal);
    Require(parsed.objects.empty() && parsed.classDefaults.size()==1u &&
        parsed.classDefaults[0].properties[0].value.integer==0x12345678,
        "Literal defaults-only little-endian value changed");
    Require(EncodeScriptSavedState(parsed)==literal,"Literal codec3 bytes changed");
    // Empty defaults must retain EXACT existing v1/v2 bytes and optionality.
    auto old=State();const auto v1=EncodeScriptSavedState(old);
    old.classDefaults=saved.classDefaults;Require(EncodeScriptSavedState(old)[6]==3u,"CDO-only extension failed without frames");
    old.classDefaults.clear();Require(EncodeScriptSavedState(old)==v1,"Removing defaults changed legacy codec1 bytes");
    old.objects[0].state=saved.objects[0].state;const auto v2=EncodeScriptSavedState(old);
    Require(v2[6]==2u,"Existing state no longer selects codec2");
    old.classDefaults=saved.classDefaults;Require(EncodeScriptSavedState(old)[6]==3u,"Shared defaults did not override codec2 selection");
    old.classDefaults.clear();Require(EncodeScriptSavedState(old)==v2,"Removing defaults changed legacy codec2 bytes");
    auto minimal=Prefix(1u);minimal[6]=2u;Object(minimal,"Map.A",0u);
    minimal.insert(minimal.end(),{0u,1u,0u,0u,0u});U32(minimal,0u);
    Require(EncodeScriptSavedState(DecodeScriptSavedState(minimal))==minimal,
        "Literal codec2 no-frame bytes changed");
    Require(DecodeScriptSavedState(v1).classDefaults.empty() && DecodeScriptSavedState(v2).classDefaults.empty(),
        "Legacy codec invented shared defaults");
    auto independent=restored;independent.classDefaults[1].properties.back().value=Integer(-17);
    Require(EncodeScriptSavedState(restored)==bytes,"Copied default properties alias the restored values");
}
void MalformedDefaults() {
    const auto bytes=EncodeScriptSavedState(DefaultsState());
    for (std::size_t length=0u;length<bytes.size();++length) {
        const Bytes truncated(bytes.begin(),bytes.begin()+length);
        Reject([&] { DecodeScriptSavedState(truncated); },"Truncated codec3 defaults accepted");
    }
    auto broken=bytes;broken.push_back(0u);Reject([&] { DecodeScriptSavedState(broken); },"Trailing codec3 bytes accepted");
    for (const std::uint8_t version : {0u,1u,2u,4u,255u}) {
        broken=bytes;broken[6]=version;
        Reject([&] { DecodeScriptSavedState(broken); },"Incompatible defaults codec version accepted");
    }
    broken=EncodeScriptSavedState(State());broken[6]=3u;
    Reject([&] { DecodeScriptSavedState(broken); },"Legacy layout accepted as codec3 without class section");
    for (const std::uint32_t count : {0u,0xffffffffu}) {
        broken=DefaultsPrefix(count);Reject([&] { DecodeScriptSavedState(broken); },"Empty/unbounded class section accepted");
    }
    broken=DefaultsPrefix();Defaults(broken,"Fixture.Actor",0u);
    Reject([&] { DecodeScriptSavedState(broken); },"Empty defaults record accepted");
    broken=DefaultsPrefix();Defaults(broken,"Fixture.Actor",0xffffffffu);
    Reject([&] { DecodeScriptSavedState(broken); },"Unbounded defaults property count accepted");
    for (const auto& classes : {std::array<std::string,2>{"Fixture.A","Fixture.A"},
        std::array<std::string,2>{"Fixture.A","fixture.a"},std::array<std::string,2>{"Fixture.Z","Fixture.A"}}) {
        broken=DefaultsPrefix(2u);
        for (const auto& cls : classes) { Defaults(broken,cls,1u);Property(broken,"Fixture.Actor.Value","Value",0u);broken.push_back(0u); }
        Reject([&] { DecodeScriptSavedState(broken); },"Duplicate/colliding/unsorted class defaults accepted");
    }
    for (std::size_t mode=0u;mode<7u;++mode) {
        broken=DefaultsPrefix();Defaults(broken,"Fixture.Actor",2u);
        Property(broken,mode==3u ? "Fixture.Z" : "Fixture.Value","Value",mode==5u ? 1u : 0u);broken.push_back(0u);
        const auto key=mode==1u ? "fixture.value" : mode==2u ? "Fixture.Value2" :
            mode==3u ? "Fixture.A" : mode==6u ? "Fixture.Z" : "Fixture.Value";
        const auto name=mode==4u ? "Other" : mode==6u ? "value" : "Value";
        Property(broken,key,name,mode==1u || mode==4u ? 1u : 0u);broken.push_back(0u);
        Reject([&] { DecodeScriptSavedState(broken); },"Default property key/alias/index collision or ordering accepted");
    }
    for (const auto& identity : {std::string(""),std::string("bad\nidentity"),std::string("bad\0identity",12),std::string(1,'\xff')}) {
        broken=DefaultsPrefix();Defaults(broken,identity,1u);Property(broken,"Fixture.Value","Value",0u);broken.push_back(0u);
        Reject([&] { DecodeScriptSavedState(broken); },"Malformed default class identity accepted");
        broken=DefaultsPrefix();Defaults(broken,"Fixture.Actor",1u);Property(broken,identity,"Value",0u);broken.push_back(0u);
        Reject([&] { DecodeScriptSavedState(broken); },"Malformed default property key accepted");
        broken=DefaultsPrefix();Defaults(broken,"Fixture.Actor",1u);Property(broken,"Fixture.Value",identity,0u);broken.push_back(0u);
        Reject([&] { DecodeScriptSavedState(broken); },"Malformed default property name accepted");
    }
    for (const Bytes& value : {Bytes{255u},Bytes{3u,2u},Bytes{4u,0u,0u,0xc0u,0x7fu},Bytes{4u,0u,0u,0x80u,0x7fu},
        Bytes{10u,255u,255u,255u,255u}}) {
        broken=DefaultsValue(value);Reject([&] { DecodeScriptSavedState(broken); },"Malformed defaults typed value accepted");
    }
    // Actor flags retain v2 validation even when shared defaults select v3.
    for (const std::size_t flag : {0u,1u}) {
        broken=Prefix(1u);broken[6]=3u;Object(broken,"Map.A",0u);
        broken.push_back(flag==0u ? 2u : 0u);broken.push_back(flag==1u ? 2u : 0u);
        U32(broken,1u);Defaults(broken,"Fixture.Actor",1u);Property(broken,"Fixture.Value","Value",0u);broken.push_back(0u);
        Reject([&] { DecodeScriptSavedState(broken); },"Codec3 invalid actor clock/state flag accepted");
    }
    broken=DefaultsValue({0u});broken.insert(broken.end(),{1u,0u,0u,0u});
    Reject([&] { DecodeScriptSavedState(broken); },"Invented CDO clock/state payload accepted");
}
void InvalidDefaultsAndBudgets() {
    const auto saved=DefaultsState();const auto valid=EncodeScriptSavedState(saved);
    const auto rejection=[](const ScriptSavedState& item,const std::string& message) {
        Reject([&] { EncodeScriptSavedState(item); },message);
    };
    auto broken=saved;broken.classDefaults.push_back(broken.classDefaults[0]);rejection(broken,"Duplicate input CDO accepted");
    broken=saved;broken.classDefaults[1].classPath="fixture.pawn";rejection(broken,"Case-colliding input CDO accepted");
    broken=saved;broken.classDefaults[0].properties.clear();rejection(broken,"Empty input CDO accepted");
    broken=saved;broken.classDefaults[0].classPath="Bad\nClass";rejection(broken,"Malformed input CDO identity accepted");
    for (std::size_t mode=0u;mode<5u;++mode) {
        broken=saved;auto duplicate=broken.classDefaults[1].properties[0];
        if (mode==1u) { duplicate.key="Fixture.Actor.Shared2"; }
        if (mode==2u) { duplicate.key="Fixture.Actor.Shared2";duplicate.name="shared"; }
        if (mode==3u) { duplicate.key="fixture.actor.shared";duplicate.index=1u; }
        if (mode==4u) { duplicate.name="Other";duplicate.index=1u; }
        broken.classDefaults[1].properties.push_back(std::move(duplicate));
        rejection(broken,"Input default property key/alias/index collision accepted");
    }
    broken=saved;broken.classDefaults[1].properties[0].value=Floating(std::numeric_limits<float>::infinity());
    rejection(broken,"Nonfinite input CDO value accepted");
    broken=saved;broken.classDefaults[1].properties[0].value=Text(Vm::Kind::String,std::string("a\0b",3));
    rejection(broken,"NUL input CDO value accepted");
    for (std::size_t mode=0u;mode<6u;++mode) {
        ScriptStateLimits limits;
        if (mode==0u) limits.maxObjects=saved.objects.size()+saved.classDefaults.size()-1u;
        if (mode==1u) limits.maxProperties=saved.objects[0].properties.size()+saved.objects[1].properties.size();
        if (mode==2u) limits.totalValueNodes=25u;
        if (mode==3u) limits.maxBytes=valid.size()-1u;
        if (mode==4u) limits.maxStringBytes=8u;
        if (mode==5u) limits.maxDepth=2u;
        Reject([&] { EncodeScriptSavedState(saved,limits); },"Aggregate mixed actor/CDO encode budget not enforced");
        Reject([&] { DecodeScriptSavedState(valid,limits); },"Aggregate mixed actor/CDO decode budget not enforced");
    }
    auto lean=ScriptSavedState{"Map",{},{{"Fixture.Actor",{{"Fixture.Actor.Value","Value",0u,{}}}}}};
    const auto leanBytes=EncodeScriptSavedState(lean);ScriptStateLimits retained;retained.maxBytes=leanBytes.size()+1u;
    Reject([&] { EncodeScriptSavedState(lean,retained); },"CDO retained-state budget missing during encode");
    Reject([&] { DecodeScriptSavedState(leanBytes,retained); },"CDO retained-state budget missing during decode");
    ScriptStateLimits noRecords;noRecords.maxObjects=0u;
    Reject([&] { EncodeScriptSavedState(lean,noRecords); },"CDO record bypasses zero object budget during encode");
    Reject([&] { DecodeScriptSavedState(leanBytes,noRecords); },"CDO record bypasses zero object budget during decode");
    auto two=lean;two.classDefaults.push_back({"Fixture.Pawn",{{"Fixture.Pawn.Value","Value",0u,{}}}});
    const auto twoBytes=EncodeScriptSavedState(two);ScriptStateLimits one;one.maxObjects=1u;
    Reject([&] { EncodeScriptSavedState(two,one); },"CDO record count is only bounded per class during encode");
    Reject([&] { DecodeScriptSavedState(twoBytes,one); },"CDO record count is only bounded per class during decode");
    one=ScriptStateLimits{};one.maxProperties=1u;
    Reject([&] { EncodeScriptSavedState(two,one); },"CDO property count is only bounded per class during encode");
    Reject([&] { DecodeScriptSavedState(twoBytes,one); },"CDO property count is only bounded per class during decode");
    one=ScriptStateLimits{};one.totalValueNodes=1u;
    Reject([&] { EncodeScriptSavedState(two,one); },"CDO value nodes are only bounded per class during encode");
    Reject([&] { DecodeScriptSavedState(twoBytes,one); },"CDO value nodes are only bounded per class during decode");
    auto deep=Scalar(Vm::Kind::Nothing);
    for (std::size_t i=0u;i<8u;++i) { auto parent=Scalar(Vm::Kind::Struct);parent.fields["Child"]=std::move(deep);deep=std::move(parent); }
    lean.classDefaults[0].properties[0].value=std::move(deep);const auto deepBytes=EncodeScriptSavedState(lean);
    one=ScriptStateLimits{};one.maxDepth=8u;
    Reject([&] { EncodeScriptSavedState(lean,one); },"CDO nested encode depth is off by one");
    Reject([&] { DecodeScriptSavedState(deepBytes,one); },"CDO nested decode depth is off by one");
}
Bytes LifecyclePrefix() {
    auto bytes=Prefix(1u);bytes[6]=ScriptStateDetail::ActorLifecycleVersion;
    Object(bytes,"Map.A",0u);bytes.insert(bytes.end(),{0u,0u,1u});
    return bytes;
}
void Links(Bytes& bytes,const std::vector<std::string>& paths) {
    U32(bytes,static_cast<std::uint32_t>(paths.size()));
    for (const auto& path : paths) String(bytes,path);
}
Bytes LifecycleLiteral(const std::vector<std::string>& children={},const std::vector<std::string>& based={}) {
    auto bytes=LifecyclePrefix();bytes.insert(bytes.end(),{1u,1u,0u,1u,0u});
    Links(bytes,children);Links(bytes,based);U32(bytes,0u);return bytes;
}
ScriptSavedActorLifecycle Lifecycle() {
    return {{"FixtureMap.ActorZ","FixtureMap.ActorA","FixtureMap.ActorZ","FixtureMap.ActorA"},
        {"FixtureMap.ActorA","FixtureMap.ActorZ","FixtureMap.ActorA"},{true,false,true,false},true};
}
void SameLifecycle(const ScriptSavedActorLifecycle& a,const ScriptSavedActorLifecycle& b) {
    Require(a.children==b.children && a.basedActors==b.basedActors,"Native actor lists were sorted, deduplicated or changed");
    Require(a.touchEventSent==b.touchEventSent && a.worldRemoved==b.worldRemoved,"Native actor lifecycle flags changed");
}
void LifecycleRoundtripAndLegacy() {
    auto saved=DefaultsState();saved.objects[0].lifecycle=Lifecycle();
    const auto bytes=EncodeScriptSavedState(saved);const auto restored=DecodeScriptSavedState(bytes);
    Require(bytes[6]==4u && restored.classDefaults.size()==2u && restored.objects.size()==2u,
        "Native lifecycle did not select codec4 or changed existing record counts");
    Require(restored.objects[1].lifecycle.has_value() && !restored.objects[0].lifecycle,
        "Codec4 lost native lifecycle optionality");
    SameLifecycle(*saved.objects[0].lifecycle,*restored.objects[1].lifecycle);
    SameClock(*saved.objects[0].clock,*restored.objects[1].clock);
    Require(restored.objects[1].state && restored.objects[1].state->frame &&
        restored.objects[1].state->frame->statementIndex==73u &&
        restored.objects[1].state->frame->locals[0].values[0].text=="FixtureMap.ActorA",
        "Codec4 changed existing state frame/local storage");
    Require(EncodeScriptSavedState(restored)==bytes,"Codec4 decode/reencode is not byte deterministic");
    auto reordered=saved;std::reverse(reordered.objects.begin(),reordered.objects.end());
    std::reverse(reordered.classDefaults.begin(),reordered.classDefaults.end());
    for (auto& object : reordered.objects) std::reverse(object.properties.begin(),object.properties.end());
    for (auto& item : reordered.classDefaults) std::reverse(item.properties.begin(),item.properties.end());
    Require(EncodeScriptSavedState(reordered)==bytes,"Codec4 canonical record/property ordering changed");
    reordered=saved;std::reverse(reordered.objects[0].lifecycle->children.begin(),reordered.objects[0].lifecycle->children.end());
    Require(EncodeScriptSavedState(reordered)!=bytes,"Codec4 discarded native list order/repeated occurrences");
    auto independent=restored;independent.objects[1].lifecycle->children[0]="FixtureMap.Other";
    Require(EncodeScriptSavedState(restored)==bytes,"Copied native lifecycle links alias restored storage");
    const std::vector<std::string> children{"Map.B","Map.A","Map.B"},based{"Map.C","Map.C"};
    const auto literal=LifecycleLiteral(children,based);const auto parsed=DecodeScriptSavedState(literal);
    Require(parsed.objects.size()==1u && parsed.objects[0].lifecycle && parsed.classDefaults.empty() &&
        !parsed.objects[0].clock && !parsed.objects[0].state,"Literal codec4 fabricated clocks, states or class defaults");
    SameLifecycle(*parsed.objects[0].lifecycle,{children,based,{true,false,true,false},true});
    Require(EncodeScriptSavedState(parsed)==literal,"Literal codec4 byte layout changed");
    auto empty=ScriptSavedState{"Map",{{"Map.A","Fixture.Actor",{},{},{},ScriptSavedActorLifecycle{}}}};
    const auto emptyBytes=EncodeScriptSavedState(empty);const auto emptyParsed=DecodeScriptSavedState(emptyBytes);
    Require(emptyBytes[6]==4u && emptyParsed.objects[0].lifecycle && emptyParsed.objects[0].lifecycle->children.empty() &&
        emptyParsed.objects[0].lifecycle->basedActors.empty() && !emptyParsed.objects[0].lifecycle->worldRemoved,
        "Empty but present native lifecycle record was discarded");
    for (std::size_t flags=0u;flags<32u;++flags) {
        auto& life=*empty.objects[0].lifecycle;life.worldRemoved=(flags&16u)!=0u;
        for (std::size_t slot=0u;slot<4u;++slot) life.touchEventSent[slot]=(flags&(1u<<slot))!=0u;
        SameLifecycle(life,*DecodeScriptSavedState(EncodeScriptSavedState(empty)).objects[0].lifecycle);
    }
    // The optional native record must not add flags or an empty defaults
    // section to ANY legacy encoding once the final lifecycle is removed.
    for (std::size_t mode=0u;mode<3u;++mode) {
        auto old=mode==2u ? DefaultsState() : State();
        if (mode==1u) old.objects[0].state=saved.objects[0].state;
        const auto prior=EncodeScriptSavedState(old);
        Require(prior[6]==mode+1u,"Legacy fixture selected the wrong codec version");
        old.objects[0].lifecycle=ScriptSavedActorLifecycle{};
        Require(EncodeScriptSavedState(old)[6]==4u,"Lifecycle failed to override legacy codec selection");
        old.objects[0].lifecycle.reset();
        Require(EncodeScriptSavedState(old)==prior,"Removing native lifecycle changed legacy codec bytes");
        for (const auto& actor : DecodeScriptSavedState(prior).objects)
            Require(!actor.lifecycle,"Legacy codec invented native lifecycle storage");
    }
    const auto v1=OneValue({2u,0x78u,0x56u,0x34u,0x12u});
    Require(EncodeScriptSavedState(DecodeScriptSavedState(v1))==v1,"Native lifecycle extension changed literal codec1");
    auto v2=Prefix(1u);v2[6]=2u;Object(v2,"Map.A",0u);
    v2.insert(v2.end(),{0u,1u,0u,0u,0u});U32(v2,0u);
    Require(EncodeScriptSavedState(DecodeScriptSavedState(v2))==v2,"Native lifecycle extension changed literal codec2");
    const auto v3=DefaultsValue({2u,0x78u,0x56u,0x34u,0x12u});
    Require(EncodeScriptSavedState(DecodeScriptSavedState(v3))==v3,"Native lifecycle extension changed literal codec3");
}
void MalformedLifecycle() {
    const auto bytes=LifecycleLiteral({"Map.B","Map.B"},{"Map.C"});
    for (std::size_t length=0u;length<bytes.size();++length) {
        const Bytes truncated(bytes.begin(),bytes.begin()+length);
        Reject([&] { DecodeScriptSavedState(truncated); },"Truncated codec4 lifecycle accepted");
    }
    auto broken=bytes;broken.push_back(0u);Reject([&] { DecodeScriptSavedState(broken); },"Trailing codec4 bytes accepted");
    const auto prefix=LifecyclePrefix();
    for (std::size_t flag=0u;flag<6u;++flag) {
        broken=bytes;broken[prefix.size()-1u+flag]=2u;
        Reject([&] { DecodeScriptSavedState(broken); },"Invalid native lifecycle optional/world/touch boolean accepted");
    }
    for (const std::uint8_t version : {0u,1u,2u,3u,5u,255u}) {
        broken=bytes;broken[6]=version;
        Reject([&] { DecodeScriptSavedState(broken); },"Native lifecycle payload accepted with incompatible codec version");
    }
    broken=Prefix(0u);broken[6]=4u;U32(broken,0u);
    Reject([&] { DecodeScriptSavedState(broken); },"Codec4 accepted without actors/lifecycle");
    broken=Prefix(1u);broken[6]=4u;Object(broken,"Map.A",0u);broken.insert(broken.end(),{0u,0u,0u});U32(broken,0u);
    Reject([&] { DecodeScriptSavedState(broken); },"Codec4 accepted with no native lifecycle record");
    for (std::size_t list=0u;list<2u;++list) {
        for (const std::uint32_t count : {1u,0xffffffffu}) {
            broken=prefix;broken.insert(broken.end(),{0u,0u,0u,0u,0u});
            if (list==1u) U32(broken,0u);
            U32(broken,count);
            Reject([&] { DecodeScriptSavedState(broken); },"Unbounded/truncated native link list count accepted");
        }
        for (const auto& path : {std::string(""),std::string("Bad\nPath"),std::string("Bad\0Path",8u),std::string(1u,'\xff')}) {
            broken=prefix;broken.insert(broken.end(),{0u,0u,0u,0u,0u});
            if (list==1u) U32(broken,0u);
            Links(broken,{path});if (list==0u) U32(broken,0u);U32(broken,0u);
            Reject([&] { DecodeScriptSavedState(broken); },"Malformed native actor-link identity accepted");
        }
    }
    broken=LifecycleLiteral();
    broken.resize(broken.size()-4u);U32(broken,0xffffffffu);
    Reject([&] { DecodeScriptSavedState(broken); },"Codec4 unbounded class-default count accepted");
    broken=LifecycleLiteral();broken.resize(broken.size()-4u);U32(broken,1u);Defaults(broken,"Fixture.Actor",0u);
    Reject([&] { DecodeScriptSavedState(broken); },"Codec4 accepted an empty class-default record");
}
void InvalidLifecycleAndBudgets() {
    auto saved=ScriptSavedState{"Map",{{"Map.A","Fixture.Actor",{},{},{},ScriptSavedActorLifecycle{}}}};
    for (std::size_t list=0u;list<2u;++list) {
        for (const auto& path : {std::string(""),std::string("Bad\nPath"),std::string("Bad\0Path",8u),std::string(1u,'\xff'),std::string(8193u,'A')}) {
            auto broken=saved;auto& life=*broken.objects[0].lifecycle;
            (list==0u ? life.children : life.basedActors).push_back(path);
            Reject([&] { EncodeScriptSavedState(broken); },"Malformed input native actor-link identity accepted");
        }
    }
    auto& life=*saved.objects[0].lifecycle;life.children={"Map.B","Map.B"};life.basedActors={"Map.C","Map.A"};
    const auto bytes=EncodeScriptSavedState(saved);
    ScriptStateLimits exact;exact.maxActorLinks=4u;
    Require(EncodeScriptSavedState(saved,exact)==bytes && EncodeScriptSavedState(DecodeScriptSavedState(bytes,exact),exact)==bytes,
        "Exact actor-link budget rejects ordered duplicates");
    for (std::size_t cap=0u;cap<4u;++cap) {
        ScriptStateLimits limits;limits.maxActorLinks=cap;
        Reject([&] { EncodeScriptSavedState(saved,limits); },"Combined child/based link encode budget not enforced");
        Reject([&] { DecodeScriptSavedState(bytes,limits); },"Combined child/based link decode budget not enforced");
    }
    auto two=saved;two.objects.push_back({"Map.Z","Fixture.Actor",{},{},{},ScriptSavedActorLifecycle{{"Map.A"},{},{},false}});
    const auto twoBytes=EncodeScriptSavedState(two);
    Reject([&] { EncodeScriptSavedState(two,exact); },"Native link encode budget was only per actor");
    Reject([&] { DecodeScriptSavedState(twoBytes,exact); },"Native link decode budget was only per actor");
    for (std::size_t mode=0u;mode<4u;++mode) {
        ScriptStateLimits limits;
        if (mode==0u) limits.maxObjects=0u;
        if (mode==1u) limits.maxStringBytes=4u;
        if (mode==2u) limits.maxBytes=bytes.size()-1u;
        if (mode==3u) {
            ScriptStateDetail::Writer measured(limits,nullptr);measured.State(saved);
            limits.maxBytes=measured.measuredBudget().retained-1u;
        }
        Reject([&] { EncodeScriptSavedState(saved,limits); },"Native lifecycle global encode budget not enforced");
        Reject([&] { DecodeScriptSavedState(bytes,limits); },"Native lifecycle global decode budget not enforced");
    }
    auto empty=saved;empty.objects[0].lifecycle=ScriptSavedActorLifecycle{};
    ScriptStateLimits none;none.maxActorLinks=0u;
    const auto emptyBytes=EncodeScriptSavedState(empty,none);
    Require(EncodeScriptSavedState(DecodeScriptSavedState(emptyBytes,none),none)==emptyBytes,
        "Empty lifecycle failed with zero native actor-link budget");
    ScriptStateDetail::Writer measured(exact,nullptr);measured.MeasureActorLifecycle(life);
    Require(measured.measuredBudget().actorLinks==4u && measured.measuredBudget().retained>=sizeof(ScriptSavedActorLifecycle),
        "Non-copy lifecycle measurement did not retain native topology/strings");
    Reject([&] { measured.MeasureActorLifecycle(life); },"Non-copy lifecycle measurement did not accumulate actor-link budget");
    Bytes output;ScriptStateDetail::Writer emitted(exact,&output);
    Reject([&] { emitted.MeasureActorLifecycle(life); },"Emitting writer accepted non-copy lifecycle measurement");
    Require(output.empty(),"Rejected non-copy lifecycle measurement changed output bytes");
    // All native links are identity strings, never value nodes. The independent
    // maxActorLinks cap still applies when no script property nodes are allowed.
    ScriptStateLimits noValues;noValues.totalValueNodes=0u;
    Require(EncodeScriptSavedState(saved,noValues)==bytes && DecodeScriptSavedState(bytes,noValues).objects[0].lifecycle,
        "Native lifecycle invented script value nodes");
}
void Birth(Bytes& bytes,const std::string& path,const std::string& cls,std::uint32_t properties,std::uint32_t worldActorIndex=0u) {
    String(bytes,path);String(bytes,cls);U32(bytes,worldActorIndex);U32(bytes,properties);
}
Bytes BirthPrefix(std::uint32_t births=1u) {
    auto bytes=Prefix(0u);bytes[6]=ScriptStateDetail::BirthManifestVersion;U32(bytes,0u);U32(bytes,births);return bytes;
}
Bytes BirthLiteral(const Bytes& value={}) {
    auto bytes=BirthPrefix();Birth(bytes,"Map.Born","Fixture.Actor",value.empty() ? 0u : 1u);
    if (!value.empty()) { Property(bytes,"Fixture.Actor.Value","Value",0u);bytes.insert(bytes.end(),value.begin(),value.end()); }
    return bytes;
}
ScriptSavedState BirthState() {
    auto saved=DefaultsState();saved.objects[0].lifecycle=Lifecycle();
    saved.births.push_back({"FixtureMap.BornZ","Fixture.Pawn",101u,saved.objects[0].properties});
    saved.births.push_back({"FixtureMap.BornA","Fixture.Actor",102u,{
        {"Fixture.Actor.Owner","Owner",0u,Text(Vm::Kind::Object,"FixtureMap.BornZ")},
        {"Fixture.Actor.Label","Label",0u,Text(Vm::Kind::String,"Frozen at allocation")}}});
    saved.objects.push_back({"FixtureMap.BornZ","Fixture.Pawn",{
        {"Fixture.Pawn.Health","Health",0u,Integer(41)},
        {"Fixture.Pawn.Other","Other",0u,Text(Vm::Kind::Object,"FixtureMap.BornA")}},Clock(),{},ScriptSavedActorLifecycle{}});
    return saved;
}
void BirthRoundtripAndLegacy() {
    const auto saved=BirthState();const auto bytes=EncodeScriptSavedState(saved);const auto restored=DecodeScriptSavedState(bytes);
    Require(bytes[6]==5u && restored.births.size()==2u && restored.objects.size()==3u && restored.classDefaults.size()==2u,
        "Birth manifest did not select codec5 or changed old record counts");
    Require(restored.births[0].path=="FixtureMap.BornA" && restored.births[1].path=="FixtureMap.BornZ",
        "Birth manifest identities are not canonical");
    for (const auto& original : saved.births) {
        const auto found=std::find_if(restored.births.begin(),restored.births.end(),[&](const auto& item) { return item.path==original.path; });
        Require(found!=restored.births.end() && found->classPath==original.classPath && found->worldActorIndex==original.worldActorIndex &&
            found->frozenDefaults.size()==original.frozenDefaults.size(),
            "Birth identity/class/frozen property count changed");
        for (const auto& property : original.frozenDefaults) {
            const auto item=std::find_if(found->frozenDefaults.begin(),found->frozenDefaults.end(),[&](const auto& value) {
                return value.key==property.key && value.index==property.index;
            });
            Require(item!=found->frozenDefaults.end() && item->name==property.name && SameValue(item->value,property.value),
                "Frozen birth concrete value changed: "+property.name);
        }
    }
    const auto& born=restored.objects[2];
    Require(born.path=="FixtureMap.BornZ" && born.classPath=="Fixture.Pawn" && born.lifecycle && born.clock && !born.state &&
        born.properties[0].value.integer==41,"Birth overlay optional/native/clock storage changed");
    SameClock(*born.clock,*saved.objects[2].clock);
    SameLifecycle(*restored.objects[1].lifecycle,*saved.objects[0].lifecycle);
    Require(restored.objects[1].state && restored.objects[1].state->frame && restored.objects[1].state->frame->statementIndex==73u,
        "Birth extension changed old authored actor state");
    Require(EncodeScriptSavedState(restored)==bytes,"Codec5 decode/reencode is not byte deterministic");
    Require(restored.births[0].worldActorIndex==102u && restored.births[1].worldActorIndex==101u,
        "Canonical birth path sorting rewrote original actor tail indices");
    auto reordered=saved;std::reverse(reordered.births.begin(),reordered.births.end());
    std::reverse(reordered.objects.begin(),reordered.objects.end());std::reverse(reordered.classDefaults.begin(),reordered.classDefaults.end());
    for (auto& birth : reordered.births) std::reverse(birth.frozenDefaults.begin(),birth.frozenDefaults.end());
    Require(EncodeScriptSavedState(reordered)==bytes,"Birth/frozen property input ordering changes canonical bytes");
    auto independent=restored;independent.births[1].frozenDefaults.back().value=Integer(-99);
    Require(EncodeScriptSavedState(restored)==bytes,"Frozen birth copies alias restored defaults");
    const auto literal=BirthLiteral({2u,0x78u,0x56u,0x34u,0x12u});const auto parsed=DecodeScriptSavedState(literal);
    Require(parsed.objects.empty() && parsed.classDefaults.empty() && parsed.births.size()==1u &&
        parsed.births[0].frozenDefaults[0].value.integer==0x12345678,"Literal birth-only concrete Int changed");
    Require(EncodeScriptSavedState(parsed)==literal,"Literal codec5 birth-only wire bytes changed");
    const auto empty=BirthLiteral();const auto emptyParsed=DecodeScriptSavedState(empty);
    Require(emptyParsed.births.size()==1u && emptyParsed.births[0].frozenDefaults.empty() && EncodeScriptSavedState(emptyParsed)==empty,
        "Structurally empty birth frozen-default list failed roundtrip");
    auto overlap=ScriptSavedState{"Map",{{"map.born","fixture.actor",{},{}}},{},{{"Map.Born","Fixture.Actor",0u,{}}}};
    ScriptStateLimits one;one.maxObjects=1u;
    const auto overlapBytes=EncodeScriptSavedState(overlap,one);
    Require(DecodeScriptSavedState(overlapBytes,one).objects.size()==1u &&
        EncodeScriptSavedState(DecodeScriptSavedState(overlapBytes,one),one)==overlapBytes,
        "Birth identity and same-class overlay were double-counted or normalized");
    for (std::size_t mode=0u;mode<4u;++mode) {
        auto old=mode==2u ? DefaultsState() : State();
        if (mode==1u) old.objects[0].state=saved.objects[0].state;
        if (mode==3u) old.objects[0].lifecycle=Lifecycle();
        const auto prior=EncodeScriptSavedState(old);Require(prior[6]==mode+1u,"Birth legacy fixture selected wrong codec");
        old.births.push_back({"FixtureMap.Born","Fixture.Actor",0u,{}});
        Require(EncodeScriptSavedState(old)[6]==5u,"Birth did not override old codec selection");
        old.births.clear();Require(EncodeScriptSavedState(old)==prior,"Removing births changed legacy codec1–4 bytes");
        Require(DecodeScriptSavedState(prior).births.empty(),"Old codec invented a birth manifest");
    }
    const auto v4=LifecycleLiteral({"Map.B","Map.B"},{"Map.C"});
    Require(EncodeScriptSavedState(DecodeScriptSavedState(v4))==v4,"Birth extension changed literal codec4 bytes");
}
void MalformedBirths() {
    const auto bytes=BirthLiteral({2u,0x78u,0x56u,0x34u,0x12u});
    for (std::size_t length=0u;length<bytes.size();++length) {
        const Bytes truncated(bytes.begin(),bytes.begin()+length);
        Reject([&] { DecodeScriptSavedState(truncated); },"Truncated codec5 birth accepted");
    }
    auto broken=bytes;broken.push_back(0u);Reject([&] { DecodeScriptSavedState(broken); },"Trailing codec5 bytes accepted");
    for (const std::uint8_t version : {0u,1u,2u,3u,4u,6u,255u}) {
        broken=bytes;broken[6]=version;Reject([&] { DecodeScriptSavedState(broken); },"Birth payload accepted with wrong codec version");
    }
    for (const auto count : {0u,0xffffffffu}) {
        broken=BirthPrefix(count);Reject([&] { DecodeScriptSavedState(broken); },"Empty/unbounded birth manifest accepted");
    }
    for (const auto& paths : {std::array<std::string,2>{"Map.A","Map.A"},
        std::array<std::string,2>{"Map.A","map.a"},std::array<std::string,2>{"Map.Z","Map.A"}}) {
        broken=BirthPrefix(2u);std::uint32_t slot{};for (const auto& path : paths) Birth(broken,path,"Fixture.Actor",0u,slot++);
        Reject([&] { DecodeScriptSavedState(broken); },"Duplicate/colliding/noncanonical birth identities accepted");
    }
    for (const auto& path : {std::string(""),std::string("Map"),std::string("Map."),std::string("Other.A"),
        std::string("Map..A"),std::string("Map.A."),std::string("Map.A/B"),std::string("Map.A\\B"),std::string("Map.A B"),
        std::string("Map.Bad\nPath"),std::string("Map.Bad\0Path",12u),std::string("Map.")+std::string(1u,'\xff')}) {
        broken=BirthPrefix();Birth(broken,path,"Fixture.Actor",0u);
        Reject([&] { DecodeScriptSavedState(broken); },"Malformed/nonmap birth identity accepted");
    }
    for (const auto& cls : {std::string(""),std::string("Bad\nClass"),std::string("Bad\0Class",9u),std::string(1u,'\xff')}) {
        broken=BirthPrefix();Birth(broken,"Map.Born",cls,0u);
        Reject([&] { DecodeScriptSavedState(broken); },"Malformed birth class identity accepted");
    }
    broken=BirthPrefix(2u);Birth(broken,"Map.A","Fixture.Actor",0u,12u);Birth(broken,"Map.B","Fixture.Actor",0u,12u);
    Reject([&] { DecodeScriptSavedState(broken); },"Duplicate world actor indices accepted");
    for (const auto slot : {1'000'000u,0xffffffffu}) {
        broken=BirthPrefix();Birth(broken,"Map.A","Fixture.Actor",0u,slot);
        Reject([&] { DecodeScriptSavedState(broken); },"Birth actor index beyond fixed slot cap accepted");
    }
    for (std::size_t flag=0u;flag<3u;++flag) {
        broken=Prefix(1u);broken[6]=5u;Object(broken,"Map.A",0u);
        for (std::size_t slot=0u;slot<3u;++slot) broken.push_back(slot==flag ? 2u : 0u);
        U32(broken,0u);U32(broken,1u);Birth(broken,"Map.Born","Fixture.Actor",0u);
        Reject([&] { DecodeScriptSavedState(broken); },"Codec5 invalid optional clock/state/lifecycle boolean accepted");
    }
    for (std::size_t mode=0u;mode<3u;++mode) {
        broken=Prefix(mode==2u ? 0u : 1u);broken[6]=5u;
        if (mode!=2u) { Object(broken,mode==0u ? "Map.Born" : "map.born",0u);broken.insert(broken.end(),{0u,0u,0u}); }
        U32(broken,mode==2u ? 1u : 0u);
        if (mode==2u) { Defaults(broken,"map.born",1u);Property(broken,"Fixture.Value","Value",0u);broken.push_back(0u); }
        U32(broken,1u);Birth(broken,"Map.Born","Fixture.Other",0u);
        Reject([&] { DecodeScriptSavedState(broken); },"Birth overlay class mismatch/class-default identity alias accepted");
    }
    broken=BirthPrefix();Birth(broken,"Map.Born","Fixture.Actor",0xffffffffu);
    Reject([&] { DecodeScriptSavedState(broken); },"Unbounded frozen-default property count accepted");
    for (std::size_t mode=0u;mode<7u;++mode) {
        broken=BirthPrefix();Birth(broken,"Map.Born","Fixture.Actor",2u);
        Property(broken,mode==3u ? "Fixture.Z" : "Fixture.Value","Value",mode==5u ? 1u : 0u);broken.push_back(0u);
        const auto key=mode==1u ? "fixture.value" : mode==2u ? "Fixture.Value2" : mode==3u ? "Fixture.A" : mode==6u ? "Fixture.Z" : "Fixture.Value";
        Property(broken,key,mode==4u ? "Other" : mode==6u ? "value" : "Value",mode==1u || mode==4u ? 1u : 0u);broken.push_back(0u);
        Reject([&] { DecodeScriptSavedState(broken); },"Frozen property duplicate/key/alias/index/order collision accepted");
    }
    for (const Bytes& value : {Bytes{255u},Bytes{3u,2u},Bytes{4u,0u,0u,0xc0u,0x7fu},Bytes{10u,255u,255u,255u,255u}}) {
        broken=BirthLiteral(value);Reject([&] { DecodeScriptSavedState(broken); },"Malformed frozen-default concrete value accepted");
    }
}
void InvalidBirthsAndBudgets() {
    const auto saved=BirthState();const auto bytes=EncodeScriptSavedState(saved);
    for (std::size_t mode=0u;mode<10u;++mode) {
        auto broken=saved;
        if (mode==0u) broken.births.push_back(broken.births[0]);
        if (mode==1u) broken.births[1].path="fixturemap.bornz";
        if (mode==2u) broken.births[0].path="OtherMap.Born";
        if (mode==3u) broken.births[0].classPath="Bad\nClass";
        if (mode==4u) broken.objects[2].classPath="Fixture.Other";
        if (mode==5u) broken.classDefaults[0].classPath="fixturemap.bornz";
        if (mode==6u) broken.births[0].frozenDefaults.push_back(broken.births[0].frozenDefaults[0]);
        if (mode==7u) { auto alias=broken.births[0].frozenDefaults[0];alias.key+="Alias";broken.births[0].frozenDefaults.push_back(std::move(alias)); }
        if (mode==8u) broken.births[1].worldActorIndex=broken.births[0].worldActorIndex;
        if (mode==9u) broken.births[0].worldActorIndex=1'000'000u;
        Reject([&] { EncodeScriptSavedState(broken); },"Invalid input birth identity/class/property collision accepted");
    }
    // Three object records include the BornZ overlay. BornA adds one identity;
    // two mutable CDO records add two. Frozen manifest does not count BornZ twice.
    ScriptStateLimits six;six.maxObjects=6u;
    Require(EncodeScriptSavedState(saved,six)==bytes && EncodeScriptSavedState(DecodeScriptSavedState(bytes,six),six)==bytes,
        "Union actor/birth plus class-default budget double-counted a birth overlay");
    ScriptStateLimits five;five.maxObjects=5u;
    Reject([&] { EncodeScriptSavedState(saved,five); },"Authored actor/birth/CDO union encode budget not enforced");
    Reject([&] { DecodeScriptSavedState(bytes,five); },"Authored actor/birth/CDO union decode budget not enforced");
    auto only=ScriptSavedState{"Map",{}, {},{{"Map.A","Fixture.Actor",0u,{}},{"Map.B","Fixture.Actor",1u,{}}}};
    ScriptStateLimits one;one.maxObjects=1u;const auto onlyBytes=EncodeScriptSavedState(only);
    Reject([&] { EncodeScriptSavedState(only,one); },"Birth-only identity count encode budget not enforced");
    Reject([&] { DecodeScriptSavedState(onlyBytes,one); },"Birth-only identity count decode budget not enforced");
    std::size_t properties{};for (const auto& object : saved.objects) properties+=object.properties.size();
    for (const auto& cls : saved.classDefaults) properties+=cls.properties.size();
    for (std::size_t mode=0u;mode<7u;++mode) {
        ScriptStateLimits limits;
        if (mode==0u) limits.maxProperties=properties;
        if (mode==1u) limits.totalValueNodes=40u;
        if (mode==2u) limits.maxStringBytes=8u;
        if (mode==3u) limits.maxDepth=2u;
        if (mode==4u) limits.maxActorLinks=0u;
        if (mode==5u) limits.maxBytes=bytes.size()-1u;
        if (mode==6u) { ScriptStateDetail::Writer measured(limits,nullptr);measured.State(saved);limits.maxBytes=measured.measuredBudget().retained-1u; }
        Reject([&] { EncodeScriptSavedState(saved,limits); },"Shared birth/property/state/lifecycle encode budget not enforced");
        Reject([&] { DecodeScriptSavedState(bytes,limits); },"Shared birth/property/state/lifecycle decode budget not enforced");
    }
    auto lean=ScriptSavedState{"Map",{}, {},{{"Map.Born","Fixture.Actor",0u,{{"Fixture.Actor.Value","Value",0u,Integer(1)}}}}};
    const auto leanBytes=EncodeScriptSavedState(lean);one=ScriptStateLimits{};one.maxBytes=leanBytes.size()+1u;
    Reject([&] { EncodeScriptSavedState(lean,one); },"Small birth-only payload bypassed retained encode budget");
    Reject([&] { DecodeScriptSavedState(leanBytes,one); },"Small birth-only payload bypassed retained decode budget");
    auto empty=ScriptSavedState{"Map",{}, {},{{"Map.Born","Fixture.Actor",999'999u,{}}}};
    one=ScriptStateLimits{};one.totalValueNodes=0u;one.maxProperties=0u;one.maxObjects=1u;
    Require(DecodeScriptSavedState(EncodeScriptSavedState(empty,one),one).births.size()==1u &&
        DecodeScriptSavedState(EncodeScriptSavedState(empty,one),one).births[0].worldActorIndex==999'999u,
        "Empty frozen birth structural record invented property/value nodes");
    ScriptSavedState copied;copied.mapName="Map";
    copied.objects.push_back({"Map.Born","Fixture.Actor",{{"Fixture.Actor.Value","Value",0u,Integer(2)}},{},{},{}});
    copied.births.push_back({"Map.Born","Fixture.Actor",0u,{{"Fixture.Actor.Value","Value",0u,Integer(1)}}});
    const auto copiedBytes=EncodeScriptSavedState(copied);const auto copiedParsed=DecodeScriptSavedState(copiedBytes);
    copied.objects[0].properties[0].value=Integer(3);
    Require(copied.births[0].frozenDefaults[0].value.integer==1 && copiedParsed.objects[0].properties[0].value.integer==2 &&
        copiedParsed.births[0].frozenDefaults[0].value.integer==1,
        "Mutable born actor overlay aliased immutable birth CDO snapshot");
}
}

int main() {
    try {
        RoundtripAndDeterminism();MalformedStreams();InvalidStateAndBudgets();
        DefaultsRoundtripAndLegacy();MalformedDefaults();InvalidDefaultsAndBudgets();
        LifecycleRoundtripAndLegacy();MalformedLifecycle();InvalidLifecycleAndBudgets();
        BirthRoundtripAndLegacy();MalformedBirths();InvalidBirthsAndBudgets();
        std::cout<<"PASS script-state codec controls="<<checks<<" rejection controls="<<rejections
            <<"; codecs1–4 legacy bytes and codec5 frozen actor births, structural codec only\n";return 0;
    }catch(const std::exception& error) {std::cerr<<"FAIL script-state codec: "<<error.what()<<" after "<<checks<<" controls\n";return 1;}
}
