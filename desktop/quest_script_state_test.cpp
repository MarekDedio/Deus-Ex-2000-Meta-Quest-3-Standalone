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
    broken=bytes;broken[6]=8;Reject([&] { DecodeScriptSavedState(broken); },"Unsupported codec version accepted");
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
Ai::State AiManager(const std::string& owner="Map.LevelInfoZ") {
    Ai::State state; state.ownerPath=owner; state.levelPath="Map.MyLevel";
    state.processDepth=7u; state.pendingDeleteCount=2u; state.historyCursor=13u; state.receiverHead=2u;
    state.eventTypes={{"ZNoise",{2u,1u},{2u,1u}},{"Distress",{3u},{}},{"ASmell",{},{3u}}};
    for (std::size_t i=0u;i<3u;++i) {
        Ai::Sender sender; sender.actor="Map.Sender"+std::to_string(i);
        sender.eventType=i==2u ? 2u : 1u; sender.deleted=i==2u; sender.score=-static_cast<float>(i)-0.125f;
        for (std::size_t slot=0u;slot<sender.history.size();++slot) {
            const auto value=static_cast<float>(i*100u+slot);
            sender.history[slot]={value+0.125f,-value-0.25f,value+0.5f,-value-0.75f};
        }
        sender.current={-0.0f,1.25f+static_cast<float>(i),-3.5f,4.75f}; state.senders.push_back(sender);
    }
    for (std::size_t i=0u;i<3u;++i) {
        Ai::Receiver receiver; receiver.actor=i<2u ? "Map.Receiver" : "Map.OtherReceiver";
        receiver.eventType=i==2u ? 3u : 1u; receiver.deleted=i==0u;
        receiver.callback=i==0u ? "" : i==1u ? "OnDistress" : "None";
        receiver.scoreCallback=i==1u ? "ScoreSender" : "";
        receiver.flags={i==0u,i==1u,i==2u,i!=2u}; receiver.callbackPending=i!=0u;
        receiver.eventState=static_cast<std::uint8_t>(i+1u); receiver.detected=i==1u;
        receiver.previousScore=-0.0f; receiver.previousBestActor=i==2u ? "" : "Map.Sender0";
        receiver.historyCursor=static_cast<std::uint8_t>(i+3u);
        receiver.params={i==0u ? "" : "Map.Sender1",-1.25f,2.5f,-3.75f,-0.0f};
        receiver.ringNext=static_cast<Ai::Id>((i+1u)%3u+1u);
        receiver.ringPrev=static_cast<Ai::Id>((i+2u)%3u+1u); state.receivers.push_back(receiver);
    }
    return state;
}
void SameAi(const Ai::State& original,const Ai::State& restored) {
    Require(original==restored,"AI graph identities, arrays, flags, counters or ring changed");
    const auto channels=[](const Ai::Channels& a,const Ai::Channels& b) {
        Require(SameFloat(a.visibility,b.visibility) && SameFloat(a.volume,b.volume) &&
            SameFloat(a.radius,b.radius) && SameFloat(a.smell,b.smell),"AI channel bits/history changed");
    };
    for (std::size_t i=0u;i<original.senders.size();++i) {
        const auto& a=original.senders[i]; const auto& b=restored.senders[i];
        Require(SameFloat(a.score,b.score),"AI sender score bits changed"); channels(a.current,b.current);
        for (std::size_t slot=0u;slot<a.history.size();++slot) channels(a.history[slot],b.history[slot]);
    }
    for (std::size_t i=0u;i<original.receivers.size();++i) {
        const auto& a=original.receivers[i]; const auto& b=restored.receivers[i];
        Require(SameFloat(a.previousScore,b.previousScore) && SameFloat(a.params.score,b.params.score) &&
            SameFloat(a.params.visibility,b.params.visibility) && SameFloat(a.params.volume,b.params.volume) &&
            SameFloat(a.params.smell,b.params.smell),"AI detection/parameter float bits changed");
    }
}
Bytes AiPrefix(std::uint32_t managers=1u) {
    auto bytes=Prefix(0u);bytes[6]=ScriptStateDetail::AiManagerVersion;
    U32(bytes,0u);U32(bytes,0u);U32(bytes,managers);return bytes;
}
void F32(Bytes& bytes,float value) {
    std::uint32_t bits{};std::memcpy(&bits,&value,sizeof(bits));U32(bytes,bits);
}
void AiLiteral(Bytes& bytes,const Ai::State& state) {
    String(bytes,state.ownerPath);String(bytes,state.levelPath);U32(bytes,state.processDepth);
    U32(bytes,state.pendingDeleteCount);bytes.push_back(state.historyCursor);U32(bytes,state.receiverHead);
    U32(bytes,static_cast<std::uint32_t>(state.eventTypes.size()));
    const auto ids=[&](const auto& values) {
        U32(bytes,static_cast<std::uint32_t>(values.size()));for (const auto value : values) U32(bytes,value);
    };
    for (const auto& event : state.eventTypes) {String(bytes,event.name);ids(event.senderIds);ids(event.receiverIds);}
    const auto channels=[&](const Ai::Channels& value) {
        F32(bytes,value.visibility);F32(bytes,value.volume);F32(bytes,value.radius);F32(bytes,value.smell);
    };
    U32(bytes,static_cast<std::uint32_t>(state.senders.size()));
    for (const auto& sender : state.senders) {
        String(bytes,sender.actor);U32(bytes,sender.eventType);bytes.push_back(sender.deleted);F32(bytes,sender.score);
        for (const auto& sample : sender.history) channels(sample);
        channels(sender.current);
    }
    U32(bytes,static_cast<std::uint32_t>(state.receivers.size()));
    for (const auto& receiver : state.receivers) {
        String(bytes,receiver.actor);U32(bytes,receiver.eventType);bytes.push_back(receiver.deleted);
        String(bytes,receiver.callback);String(bytes,receiver.scoreCallback);
        bytes.push_back(receiver.flags.checkVisibility);bytes.push_back(receiver.flags.checkDirection);
        bytes.push_back(receiver.flags.checkCylinder);bytes.push_back(receiver.flags.checkLineOfSight);
        bytes.push_back(receiver.callbackPending);bytes.push_back(receiver.eventState);bytes.push_back(receiver.detected);
        F32(bytes,receiver.previousScore);String(bytes,receiver.previousBestActor);bytes.push_back(receiver.historyCursor);
        String(bytes,receiver.params.bestActor);F32(bytes,receiver.params.score);F32(bytes,receiver.params.visibility);
        F32(bytes,receiver.params.volume);F32(bytes,receiver.params.smell);U32(bytes,receiver.ringNext);U32(bytes,receiver.ringPrev);
    }
}
void AiRoundtripAndLegacy() {
    auto saved=BirthState();saved.aiManagers={AiManager("FixtureMap.LevelInfoZ"),AiManager("FixtureMap.LevelInfoA")};
    const auto bytes=EncodeScriptSavedState(saved);const auto restored=DecodeScriptSavedState(bytes);
    Require(bytes[6]==6u && restored.aiManagers.size()==2u && restored.births.size()==saved.births.size(),
        "AI manager did not select codec6 or changed birth count");
    SameAi(saved.aiManagers[0],restored.aiManagers[1]);SameAi(saved.aiManagers[1],restored.aiManagers[0]);
    Require(EncodeScriptSavedState(restored)==bytes,"AI decode/reencode changed bytes");
    std::reverse(saved.aiManagers.begin(),saved.aiManagers.end());
    Require(EncodeScriptSavedState(saved)==bytes,"AI manager input order changed canonical bytes");
    Ai::State empty;empty.ownerPath="Map.LevelInfo0";empty.levelPath="Map.MyLevel";
    auto minimal=ScriptSavedState{"Map",{}, {}, {},{empty}};
    auto literal=AiPrefix();AiLiteral(literal,empty);
    Require(EncodeScriptSavedState(minimal)==literal,"Literal empty-manager wire layout changed");
    const auto parsed=DecodeScriptSavedState(literal);
    Require(parsed.objects.empty() && parsed.births.empty() && parsed.classDefaults.empty() &&
        parsed.aiManagers.size()==1u,"Present empty manager was dropped or invented actor records");
    SameAi(empty,parsed.aiManagers[0]);
    auto full=ScriptSavedState{"Map",{}, {}, {},{AiManager()}};
    literal=AiPrefix();AiLiteral(literal,full.aiManagers[0]);
    Require(EncodeScriptSavedState(full)==literal,"Literal AI graph wire layout changed");
    SameAi(full.aiManagers[0],DecodeScriptSavedState(literal).aiManagers[0]);
    // Codec structure retains processing fields; the runtime independently
    // refuses to save in-flight processing at its public save boundary.
    Require(DecodeScriptSavedState(literal).aiManagers[0].processDepth==7u,
        "Structural AI codec silently reset processing depth");
    for (std::size_t mode=0u;mode<5u;++mode) {
        auto old=mode==2u ? DefaultsState() : mode==4u ? BirthState() : State();
        if (mode==1u) old.objects[0].state=DefaultsState().objects[0].state;
        if (mode==3u) old.objects[0].lifecycle=Lifecycle();
        const auto prior=EncodeScriptSavedState(old);Require(prior[6]==mode+1u,"AI legacy fixture selected wrong codec");
        old.aiManagers={empty};Require(EncodeScriptSavedState(old)[6]==6u,"AI failed to override legacy codec selection");
        old.aiManagers.clear();Require(EncodeScriptSavedState(old)==prior,"Removing AI graph changed legacy codec1–5 bytes");
        Require(DecodeScriptSavedState(prior).aiManagers.empty(),"Legacy codec invented an AI manager");
    }
    auto copy=restored;copy.aiManagers[0].senders[0].history[0].volume=99.0f;
    Require(EncodeScriptSavedState(restored)==bytes,"Copied AI graph aliases restored history");
}
void InvalidAiGraphs() {
    for (std::size_t mode=0u;mode<27u;++mode) {
        auto manager=AiManager();
        if (mode==0u) manager.ownerPath.clear();
        if (mode==1u) manager.levelPath="Bad\nLevel";
        if (mode==2u) manager.eventTypes[1].name="znoise";
        if (mode==3u) manager.eventTypes[0].name="None";
        if (mode==4u) manager.eventTypes[0].senderIds[0]=0u;
        if (mode==5u) manager.eventTypes[0].senderIds[0]=999u;
        if (mode==6u) manager.eventTypes[0].senderIds[0]=1u;
        if (mode==7u) manager.eventTypes[0].receiverIds.clear();
        if (mode==8u) manager.senders[0].eventType=2u;
        if (mode==9u) manager.senders[1].actor="map.sender0";
        if (mode==10u) {manager.receivers[0].deleted=false;manager.pendingDeleteCount=1u;}
        if (mode==11u) manager.pendingDeleteCount=0u;
        if (mode==12u) manager.historyCursor=16u;
        if (mode==13u) manager.receivers[0].historyCursor=16u;
        if (mode==14u) manager.receivers[1].eventState=4u;
        if (mode==15u) manager.receiverHead=0u;
        if (mode==16u) manager.receiverHead=999u;
        if (mode==17u) manager.receivers[0].ringNext=0u;
        if (mode==18u) manager.receivers[0].ringPrev=999u;
        if (mode==19u) manager.receivers[0].ringPrev=2u;
        if (mode==20u) for (std::size_t i=0u;i<manager.receivers.size();++i)
            manager.receivers[i].ringNext=manager.receivers[i].ringPrev=static_cast<Ai::Id>(i+1u);
        if (mode==21u) manager.senders[0].history[9].smell=std::numeric_limits<float>::infinity();
        if (mode==22u) manager.receivers[1].params.score=std::numeric_limits<float>::quiet_NaN();
        if (mode==23u) manager.receivers[1].callback=std::string("Bad\0Name",8u);
        if (mode==24u) manager.senders[0].actor.clear();
        if (mode==25u) manager.eventTypes[0].name=std::string(8193u,'A');
        if (mode==26u) manager.receivers[1].previousBestActor="Bad\nActor";
        const auto saved=ScriptSavedState{"Map",{}, {}, {},{manager}};
        Reject([&] { EncodeScriptSavedState(saved); },"Invalid input AI graph accepted, mode="+std::to_string(mode));
        auto bytes=AiPrefix();AiLiteral(bytes,manager);
        Reject([&] { DecodeScriptSavedState(bytes); },"Invalid encoded AI graph accepted, mode="+std::to_string(mode));
    }
    const auto valid=ScriptSavedState{"Map",{}, {}, {},{AiManager()}};
    const auto bytes=EncodeScriptSavedState(valid);
    for (std::size_t length=0u;length<bytes.size();++length) {
        const Bytes truncated(bytes.begin(),bytes.begin()+length);
        Reject([&] { DecodeScriptSavedState(truncated); },"Truncated codec6 AI graph accepted");
    }
    for (const auto& owners : {std::array<std::string,2>{"Map.LevelA","Map.LevelA"},
        std::array<std::string,2>{"Map.LevelA","map.levela"},std::array<std::string,2>{"Map.LevelZ","Map.LevelA"}}) {
        auto literal=AiPrefix(2u);AiLiteral(literal,AiManager(owners[0]));AiLiteral(literal,AiManager(owners[1]));
        Reject([&] { DecodeScriptSavedState(literal); },"Duplicate/colliding/unsorted AI manager owners accepted");
    }
    for (const std::uint32_t count : {0u,0xffffffffu}) {
        auto literal=AiPrefix(count);Reject([&] { DecodeScriptSavedState(literal); },"Empty/unbounded AI manager count accepted");
    }
    auto literal=AiPrefix();Ai::State empty;empty.ownerPath="Map.LevelInfo0";empty.levelPath="Map.MyLevel";AiLiteral(literal,empty);
    for (std::size_t count=0u;count<3u;++count) {
        auto broken=literal;
        for (std::size_t byte=0u;byte<4u;++byte) broken[broken.size()-12u+count*4u+byte]=0xffu;
        Reject([&] { DecodeScriptSavedState(broken); },"Unbounded AI event/sender/receiver count accepted");
    }
    literal=bytes;literal.push_back(0u);Reject([&] { DecodeScriptSavedState(literal); },"Trailing AI graph bytes accepted");
    for (const std::uint8_t version : {0u,1u,2u,3u,4u,5u,7u,255u}) {
        literal=bytes;literal[6]=version;Reject([&] { DecodeScriptSavedState(literal); },"AI graph accepted with wrong codec version");
    }
    for (std::size_t mode=0u;mode<2u;++mode) {
        auto duplicate=valid;duplicate.aiManagers.push_back(duplicate.aiManagers[0]);
        if (mode==1u) duplicate.aiManagers[1].ownerPath="map.levelinfoz";
        Reject([&] { EncodeScriptSavedState(duplicate); },"Duplicate/colliding input manager accepted");
    }
}
void AiBudgets() {
    auto saved=ScriptSavedState{"Map",{}, {}, {},{AiManager("Map.LevelInfoA"),AiManager("Map.LevelInfoZ")}};
    const auto bytes=EncodeScriptSavedState(saved);
    ScriptStateLimits measuredLimits;ScriptStateDetail::Writer measured(measuredLimits,nullptr);measured.State(saved);
    const auto measuredBudget=measured.measuredBudget();
    Require(measuredBudget.aiManagers==2u && measuredBudget.aiNodes==12u && measuredBudget.aiEventTypes==6u &&
        measuredBudget.aiLinks>12u,"AI measurement omitted aggregate manager/node/type/link counts");
    for (std::size_t mode=0u;mode<7u;++mode) {
        ScriptStateLimits limits;
        if (mode==0u) limits.maxAiManagers=1u;
        if (mode==1u) limits.maxAiEventTypes=5u;
        if (mode==2u) limits.maxAiNodes=11u;
        if (mode==3u) limits.maxAiLinks=measuredBudget.aiLinks-1u;
        if (mode==4u) limits.maxBytes=measuredBudget.retained-1u;
        if (mode==5u) limits.maxStringBytes=4u;
        if (mode==6u) limits.maxBytes=bytes.size()-1u;
        Reject([&] { EncodeScriptSavedState(saved,limits); },"Aggregate AI encode budget was only per manager");
        Reject([&] { DecodeScriptSavedState(bytes,limits); },"Aggregate AI decode budget was only per manager");
    }
    ScriptStateLimits exact;exact.maxAiManagers=2u;exact.maxAiEventTypes=6u;exact.maxAiNodes=12u;
    exact.maxAiLinks=measuredBudget.aiLinks;exact.totalValueNodes=0u;exact.maxProperties=0u;
    Require(EncodeScriptSavedState(saved,exact)==bytes && EncodeScriptSavedState(DecodeScriptSavedState(bytes,exact),exact)==bytes,
        "Exact cumulative AI budgets rejected native-only graphs or invented script value nodes");
    ScriptStateDetail::Writer live(exact,nullptr);live.MeasureAiManager(saved.aiManagers[0]);live.MeasureAiManager(saved.aiManagers[1]);
    Require(live.measuredBudget().aiNodes==12u && live.measuredBudget().aiLinks==measuredBudget.aiLinks,
        "Live non-copy AI measurement differs from codec graph budget");
    Reject([&] { live.MeasureAiManager(saved.aiManagers[0]); },"Live AI measurement did not accumulate manager budget");
    Bytes output;ScriptStateDetail::Writer emitting(exact,&output);
    Reject([&] { emitting.MeasureAiManager(saved.aiManagers[0]); },"Emitting writer accepted AI-only measurement");
    Require(output.empty(),"Rejected AI-only measurement changed emitted bytes");
    auto mixed=BirthState();mixed.aiManagers=saved.aiManagers;const auto mixedBytes=EncodeScriptSavedState(mixed);
    ScriptStateLimits joint;ScriptStateDetail::Writer jointMeasure(joint,nullptr);jointMeasure.State(mixed);
    joint.maxBytes=jointMeasure.measuredBudget().retained-1u;
    Reject([&] { EncodeScriptSavedState(mixed,joint); },"Mixed actor/birth/AI retained encode budget was not cumulative");
    Reject([&] { DecodeScriptSavedState(mixedBytes,joint); },"Mixed actor/birth/AI retained decode budget was not cumulative");
}
Bytes RandomLiteral() {
    // Independent codec7 oracle: Map, zero actors/defaults/births/AI managers,
    // algorithm1, little-endian seed0x12345678. No optional-presence byte.
    return {'D','X','Q','V','M','S',7u,0u,3u,0u,0u,0u,'M','a','p',
        0u,0u,0u,0u, 0u,0u,0u,0u, 0u,0u,0u,0u, 0u,0u,0u,0u,
        1u,0x78u,0x56u,0x34u,0x12u};
}
void RandomRoundtripAndLegacy() {
    auto only=ScriptSavedState{"Map",{}};only.randomSeed=0x12345678u;
    const auto literal=RandomLiteral();const auto parsed=DecodeScriptSavedState(literal);
    Require(literal.size()==36u && EncodeScriptSavedState(only)==literal,
        "Literal RNG-only codec7 algorithm/seed or empty-section wire layout changed");
    Require(parsed.mapName=="Map" && parsed.randomSeed==only.randomSeed && parsed.objects.empty() &&
        parsed.classDefaults.empty() && parsed.births.empty() && parsed.aiManagers.empty(),
        "RNG-only codec7 fabricated actor/native state or lost the seed");
    Require(EncodeScriptSavedState(parsed)==literal,"RNG-only codec7 decode/reencode changed bytes");
    for (const std::uint32_t seed : {0u,1u,0x12345678u,0xffffffffu}) {
        only.randomSeed=seed;auto expected=literal;expected.resize(expected.size()-4u);U32(expected,seed);
        Require(EncodeScriptSavedState(only)==expected && DecodeScriptSavedState(expected).randomSeed==seed,
            "Codec7 rejected or changed a valid uint32 seed, including zero/all-ones");
    }
    auto mixed=BirthState();mixed.aiManagers={AiManager("FixtureMap.LevelInfoZ"),AiManager("FixtureMap.LevelInfoA")};
    const auto prior=EncodeScriptSavedState(mixed);Require(prior[6]==6u,"Mixed RNG baseline did not select codec6");
    mixed.randomSeed=0xfedcba98u;
    auto expected=prior;expected[6]=7u;expected.push_back(1u);U32(expected,*mixed.randomSeed);
    const auto bytes=EncodeScriptSavedState(mixed);const auto restored=DecodeScriptSavedState(bytes);
    Require(bytes==expected && restored.randomSeed==mixed.randomSeed && restored.objects.size()==mixed.objects.size() &&
        restored.classDefaults.size()==mixed.classDefaults.size() && restored.births.size()==mixed.births.size() &&
        restored.aiManagers.size()==mixed.aiManagers.size(),
        "Mixed codec7 failed to preserve all preceding sections plus the exact RNG tail");
    SameAi(mixed.aiManagers[0],restored.aiManagers[1]);SameAi(mixed.aiManagers[1],restored.aiManagers[0]);
    Require(restored.objects[1].state && restored.objects[1].state->frame && restored.objects[1].lifecycle &&
        restored.objects[1].clock && restored.objects[1].state->frame->statementIndex==73u,
        "Mixed codec7 lost state/frame/lifecycle/clock optional records");
    Require(EncodeScriptSavedState(restored)==bytes,"Mixed codec7 decode/reencode changed bytes");
    auto reordered=mixed;std::reverse(reordered.objects.begin(),reordered.objects.end());
    std::reverse(reordered.classDefaults.begin(),reordered.classDefaults.end());
    std::reverse(reordered.births.begin(),reordered.births.end());std::reverse(reordered.aiManagers.begin(),reordered.aiManagers.end());
    Require(EncodeScriptSavedState(reordered)==bytes,"RNG extension changed canonical identity ordering");
    auto independent=restored;independent.randomSeed=0u;
    Require(EncodeScriptSavedState(restored)==bytes,"Copied RNG seed aliases restored state");
    for (std::size_t mode=0u;mode<6u;++mode) {
        auto old=mode==2u ? DefaultsState() : mode>=4u ? BirthState() : State();
        if (mode==1u) old.objects[0].state=DefaultsState().objects[0].state;
        if (mode==3u) old.objects[0].lifecycle=Lifecycle();
        if (mode==5u) old.aiManagers={AiManager()};
        const auto original=EncodeScriptSavedState(old);
        Require(original[6]==mode+1u && !DecodeScriptSavedState(original).randomSeed,
            "Legacy codec1–6 selected the wrong version or invented an RNG record");
        old.randomSeed=0u;
        Require(EncodeScriptSavedState(old)[6]==7u && DecodeScriptSavedState(EncodeScriptSavedState(old)).randomSeed==0u,
            "Present zero RNG seed failed to override a legacy codec");
        old.randomSeed.reset();
        Require(EncodeScriptSavedState(old)==original,"Removing RNG changed legacy codec1–6 bytes");
    }
}
void InvalidRandomStateAndBudgets() {
    const auto literal=RandomLiteral();
    for (std::size_t length=0u;length<literal.size();++length) {
        const Bytes truncated(literal.begin(),literal.begin()+length);
        Reject([&] { DecodeScriptSavedState(truncated); },"Truncated RNG-only codec7 accepted");
    }
    for (const std::uint8_t algorithm : {0u,2u,255u}) {
        auto broken=literal;broken[broken.size()-5u]=algorithm;
        Reject([&] { DecodeScriptSavedState(broken); },"Unknown RNG algorithm accepted");
    }
    for (const std::uint8_t version : {0u,1u,2u,3u,4u,5u,6u,8u,255u}) {
        auto broken=literal;broken[6]=version;
        Reject([&] { DecodeScriptSavedState(broken); },"RNG payload accepted with incompatible codec version");
    }
    auto broken=literal;broken.push_back(0u);
    Reject([&] { DecodeScriptSavedState(broken); },"Trailing RNG state bytes accepted");
    auto only=ScriptSavedState{"Map",{}};only.randomSeed=0x12345678u;
    ScriptStateLimits exact;ScriptStateDetail::Writer measured(exact,nullptr);measured.State(only);
    const auto retained=measured.measuredBudget().retained;
    Require(measured.size()==literal.size() && measured.measuredBudget().properties==0u &&
        measured.measuredBudget().nodes==0u && measured.measuredBudget().aiManagers==0u,
        "RNG-only state measurement omitted wire bytes or invented value/native records");
    exact.maxBytes=retained;exact.maxObjects=0u;exact.maxProperties=0u;exact.totalValueNodes=0u;
    exact.maxAiManagers=0u;exact.maxAiEventTypes=0u;exact.maxAiNodes=0u;exact.maxAiLinks=0u;
    exact.maxActorLinks=0u;exact.maxStringBytes=3u;
    Require(EncodeScriptSavedState(only,exact)==literal &&
        EncodeScriptSavedState(DecodeScriptSavedState(literal,exact),exact)==literal,
        "Exact RNG-only budgets rejected empty lower sections or changed seed bytes");
    for (const auto cap : {retained-1u,literal.size()-1u}) {
        auto small=exact;small.maxBytes=cap;
        Reject([&] { EncodeScriptSavedState(only,small); },"RNG-only encode exceeded retained/encoded byte cap");
        Reject([&] { DecodeScriptSavedState(literal,small); },"RNG-only decode exceeded retained/encoded byte cap");
    }
    auto mixed=BirthState();mixed.aiManagers={AiManager()};mixed.randomSeed=0xffffffffu;
    const auto bytes=EncodeScriptSavedState(mixed);
    for (std::size_t missing=1u;missing<=5u;++missing) {
        const Bytes truncated(bytes.begin(),bytes.end()-missing);
        Reject([&] { DecodeScriptSavedState(truncated); },"Mixed codec7 accepted missing algorithm/seed tail bytes");
    }
    ScriptStateLimits joint;ScriptStateDetail::Writer combined(joint,nullptr);combined.State(mixed);
    joint.maxBytes=combined.measuredBudget().retained-1u;
    Reject([&] { EncodeScriptSavedState(mixed,joint); },"Mixed RNG/actor/native retained encode budget was not cumulative");
    Reject([&] { DecodeScriptSavedState(bytes,joint); },"Mixed RNG/actor/native retained decode budget was not cumulative");
}
Bytes SleepLiteral() {
    // Independent codec8 wire oracle: one actor-owned signed timer (-1),
    // no reflected properties, clock, frame, lifecycle, defaults/births/AI/RNG.
    return {'D','X','Q','V','M','S',8u,0u,3u,0u,0u,0u,'M','a','p',1u,0u,0u,0u,
        9u,0u,0u,0u,'M','a','p','.','A','c','t','o','r',
        12u,0u,0u,0u,'E','n','g','i','n','e','.','A','c','t','o','r',
        0u,0u,0u,0u,0u,0u,0u,1u,0u,0u,0x80u,0xbfu,
        0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u,0u};
}
void SleepRoundtripAndLegacy() {
    ScriptSavedState only{"Map",{{"Map.Actor","Engine.Actor",{},std::nullopt}}};only.objects[0].latentTimeLeft = -1.0f;
    const auto literal = SleepLiteral();const auto decoded = DecodeScriptSavedState(literal);
    Require(literal.size() == 73u && EncodeScriptSavedState(only) == literal &&
        decoded.objects[0].latentTimeLeft == -1.0f && !decoded.objects[0].state && !decoded.randomSeed &&
        EncodeScriptSavedState(decoded) == literal,"Literal timer-only codec8 changed bytes or invented frame/RNG state");
    for (const auto value : {0.0f,-0.0f,-1.0f,0.1f,std::numeric_limits<float>::max(),std::numeric_limits<float>::lowest()}) {
        only.objects[0].latentTimeLeft = value;const auto encoded = EncodeScriptSavedState(only);
        const auto parsed = DecodeScriptSavedState(encoded);const auto restored = *parsed.objects[0].latentTimeLeft;
        Require(restored == value && std::signbit(restored) == std::signbit(value) && EncodeScriptSavedState(parsed) == encoded,
            "Codec8 lost a signed finite float, including negative zero");
    }
    only.randomSeed = 0u;only.objects[0].latentTimeLeft = -1.0f;
    auto expected = literal;expected.back() = 1u;expected.push_back(1u);U32(expected,0u);
    Require(EncodeScriptSavedState(only) == expected && DecodeScriptSavedState(expected).randomSeed == 0u,
        "Codec8 optional RNG presence/algorithm/zero seed changed codec7's actual stream data");
    for (std::size_t mode = 0u;mode < 7u;++mode) {
        auto old = mode == 2u ? DefaultsState() : mode >= 4u ? BirthState() : State();
        if (mode == 1u) old.objects[0].state = DefaultsState().objects[0].state;
        if (mode == 3u) old.objects[0].lifecycle = Lifecycle();
        if (mode >= 5u) old.aiManagers = {AiManager()};
        if (mode == 6u) old.randomSeed = 0x12345678u;
        const auto prior = EncodeScriptSavedState(old);
        Require(prior[6] == mode+1u && !DecodeScriptSavedState(prior).objects[0].latentTimeLeft,
            "Legacy codec1–7 fabricated an actor native timer");
        old.objects[0].latentTimeLeft = -0.25f;
        const auto extended = EncodeScriptSavedState(old);const auto parsed = DecodeScriptSavedState(extended);
        const auto actor = std::find_if(parsed.objects.begin(),parsed.objects.end(),[&](const auto& object){return object.path == old.objects[0].path;});
        Require(extended[6] == 8u && actor != parsed.objects.end() && actor->latentTimeLeft == -0.25f && parsed.randomSeed == old.randomSeed &&
            EncodeScriptSavedState(parsed) == extended,"Codec8 extension failed mixed older sections or optional RNG");
        old.objects[0].latentTimeLeft.reset();
        Require(EncodeScriptSavedState(old) == prior,"Removing timer changed byte-exact codec1–7 output");
    }
    auto mixed = BirthState();mixed.aiManagers = {AiManager()};mixed.randomSeed = 0xffffffffu;
    mixed.objects[0].latentTimeLeft = 0.5f;mixed.objects[1].latentTimeLeft = -1.0f;
    mixed.objects[0].state->frame->latent = StateLatent::Sleep;
    const auto bytes = EncodeScriptSavedState(mixed);const auto parsed = DecodeScriptSavedState(bytes);
    Require(parsed.objects[1].state->frame->latent == StateLatent::Sleep && parsed.objects[1].latentTimeLeft == 0.5f &&
        parsed.objects[1].clock && parsed.objects[1].lifecycle && parsed.aiManagers.size() == 1u && parsed.randomSeed == 0xffffffffu &&
        EncodeScriptSavedState(parsed) == bytes,"Codec8 lost composed wait/clock/lifecycle/birth/AI/RNG records");
    std::reverse(mixed.objects.begin(),mixed.objects.end());
    Require(EncodeScriptSavedState(mixed) == bytes,"Native timers changed canonical actor ordering");
}
void LandingSharedTimerWire() {
    // Independent codec8 fixture, including the pre-existing ordinal11. The
    // codec is structural: original class/frame eligibility is runtime work.
    Bytes literal{'D','X','Q','V','M','S',8u,0u};String(literal,"Map");U32(literal,1u);
    String(literal,"Map.Pawn");String(literal,"Engine.Pawn");U32(literal,0u);
    literal.insert(literal.end(),{0u,1u,1u,1u,1u}); // No clock; state/stack/override/frame.
    String(literal,"");String(literal,"");U32(literal,23u);literal.push_back(11u);
    U32(literal,0u);U32(literal,0u); // No locals or disabled names.
    literal.insert(literal.end(),{0u,1u});U32(literal,0x40200000u); // No lifecycle; timer2.5.
    U32(literal,0u);U32(literal,0u);U32(literal,0u);literal.push_back(0u); // No defaults/births/AI/RNG.
    ScriptSavedState only{"Map",{{"Map.Pawn","Engine.Pawn",{},std::nullopt}}};
    StateObject state;state.hasStack=true;state.frameOverride=true;state.frame=StateFrame{};
    state.frame->statementIndex=23u;state.frame->latent=StateLatent::WaitForLanding;
    only.objects[0].state=state;only.objects[0].latentTimeLeft=2.5f;
    const auto decoded=DecodeScriptSavedState(literal);
    Require(EncodeScriptSavedState(only)==literal && decoded.objects[0].state->frame->latent==StateLatent::WaitForLanding &&
        decoded.objects[0].state->frame->statementIndex==23u && decoded.objects[0].latentTimeLeft==2.5f &&
        !decoded.randomSeed && EncodeScriptSavedState(decoded)==literal,
        "Original landing counter/frame changed existing codec8 literal or fabricated RNG");
    for (const float timer : {0.0f,-0.0f,-1.0f,2.5f,std::numeric_limits<float>::max(),std::numeric_limits<float>::lowest()}) {
        only.objects[0].latentTimeLeft=timer;const auto bytes=EncodeScriptSavedState(only);
        const auto parsed=DecodeScriptSavedState(bytes);
        Require(bytes[6]==8u && SameFloat(*parsed.objects[0].latentTimeLeft,timer) &&
            parsed.objects[0].state->frame->latent==StateLatent::WaitForLanding && EncodeScriptSavedState(parsed)==bytes,
            "Landing shared signed counter failed bit-exact codec8 restoration");
    }
    only.objects[0].latentTimeLeft.reset();
    const auto old=EncodeScriptSavedState(only);
    Require(old[6]==2u && !DecodeScriptSavedState(old).objects[0].latentTimeLeft &&
        DecodeScriptSavedState(old).objects[0].state->frame->latent==StateLatent::WaitForLanding,
        "Structural codec invented a timer or revised legacy ordinal11 wire");
    only.objects[0].latentTimeLeft=2.5f;only.objects[0].latentTimeLeft.reset();
    Require(EncodeScriptSavedState(only)==old,"Removing landing native counter changed old codec2 bytes");
    auto mixed=BirthState();mixed.aiManagers={AiManager()};mixed.randomSeed=0u;
    mixed.objects[0].latentTimeLeft=-0.25f;mixed.objects[0].state->frame->latent=StateLatent::WaitForLanding;
    const auto bytes=EncodeScriptSavedState(mixed);const auto parsed=DecodeScriptSavedState(bytes);
    const auto actor=std::find_if(parsed.objects.begin(),parsed.objects.end(),[&](const auto& object){return object.path==mixed.objects[0].path;});
    Require(actor!=parsed.objects.end() && actor->state->frame->latent==StateLatent::WaitForLanding && actor->latentTimeLeft==-0.25f &&
        actor->clock && actor->lifecycle && parsed.aiManagers.size()==1u && parsed.randomSeed==0u && EncodeScriptSavedState(parsed)==bytes,
        "Mixed landing wait/clock/lifecycle/birth/AI/RNG lost existing codec8 sections");
}
void InvalidSleepStateAndBudgets() {
    const auto literal = SleepLiteral();
    for (std::size_t length = 0u;length < literal.size();++length) {
        const Bytes truncated(literal.begin(),literal.begin()+length);
        Reject([&] {DecodeScriptSavedState(truncated);},"Truncated codec8 accepted");
    }
    for (const std::uint32_t bits : {0x7f800000u,0xff800000u,0x7fc00000u}) {
        auto broken = literal;for (unsigned i=0u;i<4u;++i) broken[56u+i] = std::uint8_t(bits>>(8u*i));
        Reject([&] {DecodeScriptSavedState(broken);},"Non-finite encoded native timer accepted");
    }
    for (const auto position : {55u,72u}) {
        auto broken = literal;broken[position] = 2u;
        Reject([&] {DecodeScriptSavedState(broken);},"Invalid timer/RNG presence boolean accepted");
    }
    auto absent = literal;absent[55u] = 0u;absent.erase(absent.begin()+56u,absent.begin()+60u);
    Reject([&] {DecodeScriptSavedState(absent);},"Codec8 accepted no native timer records");
    for (const std::uint8_t version : {0u,1u,2u,3u,4u,5u,6u,7u,9u,255u}) {
        auto broken = literal;broken[6] = version;
        Reject([&] {DecodeScriptSavedState(broken);},"Codec8 data accepted under incompatible version");
    }
    auto tail = literal;tail.push_back(0u);Reject([&] {DecodeScriptSavedState(tail);},"Trailing codec8 data accepted");
    tail = literal;tail.back() = 1u;tail.push_back(2u);U32(tail,0u);
    Reject([&] {DecodeScriptSavedState(tail);},"Codec8 accepted unknown optional RNG algorithm");
    ScriptSavedState only{"Map",{{"Map.Actor","Engine.Actor",{},std::nullopt}}};only.objects[0].latentTimeLeft = -1.0f;
    ScriptStateLimits measuredLimits;ScriptStateDetail::Writer measured(measuredLimits,nullptr);measured.State(only);
    Require(measured.size() == literal.size() && measured.measuredBudget().properties == 0u && measured.measuredBudget().nodes == 0u,
        "Timer-only measurement invented reflected values or omitted wire bytes");
    ScriptStateLimits exact;exact.maxBytes = measured.measuredBudget().retained;exact.maxProperties = 0u;exact.totalValueNodes = 0u;
    Require(EncodeScriptSavedState(only,exact) == literal && EncodeScriptSavedState(DecodeScriptSavedState(literal,exact),exact) == literal,
        "Exact retained timer budget failed");
    for (const auto cap : {exact.maxBytes-1u,literal.size()-1u}) {
        auto small = exact;small.maxBytes = cap;
        Reject([&] {EncodeScriptSavedState(only,small);},"Native timer encode exceeded retained/encoded budget");
        Reject([&] {DecodeScriptSavedState(literal,small);},"Native timer decode exceeded retained/encoded budget");
    }
    for (const auto bad : {std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}) {
        only.objects[0].latentTimeLeft = bad;
        Reject([&] {EncodeScriptSavedState(only);},"Non-finite live native timer accepted");
    }
}
}

int main() {
    try {
        RoundtripAndDeterminism();MalformedStreams();InvalidStateAndBudgets();
        DefaultsRoundtripAndLegacy();MalformedDefaults();InvalidDefaultsAndBudgets();
        LifecycleRoundtripAndLegacy();MalformedLifecycle();InvalidLifecycleAndBudgets();
        BirthRoundtripAndLegacy();MalformedBirths();InvalidBirthsAndBudgets();
        AiRoundtripAndLegacy();InvalidAiGraphs();AiBudgets();
        RandomRoundtripAndLegacy();InvalidRandomStateAndBudgets();
        SleepRoundtripAndLegacy();LandingSharedTimerWire();InvalidSleepStateAndBudgets();
        std::cout<<"PASS script-state codec controls="<<checks<<" rejection controls="<<rejections
            <<"; codecs1–7 legacy bytes, codec8 actor-owned signed timer and optional RNG, structural codec only\n";return 0;
    }catch(const std::exception& error) {std::cerr<<"FAIL script-state codec: "<<error.what()<<" after "<<checks<<" controls\n";return 1;}
}
