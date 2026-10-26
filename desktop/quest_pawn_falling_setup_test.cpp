#include "quest_pawn_falling_setup.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <functional>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace {
using Id=std::uintptr_t;
using Vec=PortableModelVec3;
using Queries=QuestVr::OriginalPawnFallingSetupQueries;
using SetupStats=QuestVr::OriginalPawnFallingSetupStats;
std::size_t checks{},refusals{},oracleCases{};
void Require(bool condition,const char* message){++checks;if(!condition)throw std::runtime_error(message);}
std::uint32_t Bits(float value){return std::bit_cast<std::uint32_t>(value);}
float Float(std::uint32_t value){return std::bit_cast<float>(value);}
bool Same(Vec a,Vec b){return Bits(a.x)==Bits(b.x)&&Bits(a.y)==Bits(b.y)&&Bits(a.z)==Bits(b.z);}
template<class Function>void Refuses(Function&& fn,const char* message){
    bool failed{};try{fn();}catch(const std::exception&){failed=true;}
    Require(failed,message);++refusals;
}
struct State {
    float air=0.1f,rate=100.0f,ground=320.0f,radius=2.0f,height=4.0f;
    std::array<float,3u> acceleration{0,0,7},velocity{},location{100,200,300};
    Id level=2u;
    bool operator==(const State& other)const{
        if(level!=other.level||Bits(air)!=Bits(other.air)||Bits(rate)!=Bits(other.rate)||Bits(ground)!=Bits(other.ground)||
            Bits(radius)!=Bits(other.radius)||Bits(height)!=Bits(other.height))return false;
        for(unsigned i=0u;i<3u;++i)if(Bits(acceleration[i])!=Bits(other.acceleration[i])||
            Bits(velocity[i])!=Bits(other.velocity[i])||Bits(location[i])!=Bits(other.location[i]))return false;
        return true;
    }
};
struct Packet {Id level{},actor{};Vec end{},start{},extent{};std::uint32_t flags{};std::int32_t extra{};};
struct Fixture {
    State state;
    std::vector<std::string> events;
    std::vector<Vec> normals;
    std::vector<Packet> traces;
    std::vector<std::pair<unsigned,std::uint32_t>> accelerationWrites,velocityWrites;
    std::function<void()> onTrace,onNormal,onAccelerationWrite,onVelocityWrite;
    std::size_t work{},limit=4096u,writes{},writeLimit=4096u;
    std::int32_t traceReturn=-23;
    Id hitActor{};
    void Source(Id actor){Require(actor==1u,"Pawn setup changed retained source identity");}
    void Write(){if(writes>=writeLimit)throw std::runtime_error("write cutoff");++writes;}
    static Vec NormalizeKnown(Vec input){
        // Independent exact finite cases used by these provider fixtures; no
        // production scalar/normal helper computes the reference direction.
        if(input.x==0.0f&&input.y==0.0f&&input.z==0.0f)return {};
        if(input.y==0.0f&&input.z==0.0f&&std::isfinite(input.x))return {input.x<0?-1.0f:1.0f,0,0};
        if(input.x==0.0f&&input.z==0.0f&&std::isfinite(input.y))return {0,input.y<0?-1.0f:1.0f,0};
        if(input.x==0.0f&&input.y==0.0f&&std::isfinite(input.z))return {0,0,input.z<0?-1.0f:1.0f};
        if(((input.x==3.0f&&input.y==4.0f)||(input.x==30.0f&&input.y==40.0f))&&input.z==0.0f)return {0.6f,0.8f,0};
        throw std::runtime_error("Test normal provider only admits independently known finite directions");
    }
    Queries Query(){
        Queries q;
        q.work=[&]{if(work>=limit)throw std::runtime_error("work cutoff");++work;};
        q.airControl=[&](Id a){Source(a);events.emplace_back("air");return state.air;};
        q.accelRate=[&](Id a){Source(a);events.emplace_back("rate");return state.rate;};
        q.groundSpeed=[&](Id a){Source(a);events.emplace_back("ground");return state.ground;};
        q.radius=[&](Id a){Source(a);events.emplace_back("radius");return state.radius;};
        q.height=[&](Id a){Source(a);events.emplace_back("height");return state.height;};
        q.acceleration=[&](Id a,unsigned axis){Source(a);events.push_back("a"+std::to_string(axis));return state.acceleration.at(axis);};
        q.velocity=[&](Id a,unsigned axis){Source(a);events.push_back("v"+std::to_string(axis));return state.velocity.at(axis);};
        q.location=[&](Id a,unsigned axis){Source(a);events.push_back("l"+std::to_string(axis));return state.location.at(axis);};
        q.level=[&](Id a){Source(a);events.emplace_back("level");return state.level;};
        q.safeNormal=[&](Vec input){events.emplace_back("normal");normals.push_back(input);const auto result=NormalizeKnown(input);if(onNormal)onNormal();return result;};
        q.singleLineCheck=[&](Id level,QuestVr::OriginalActorLineHit& hit,Id actor,Vec end,Vec start,std::uint32_t flags,Vec extent,std::int32_t extra){
            Source(actor);events.emplace_back("trace");traces.push_back({level,actor,end,start,extent,flags,extra});
            Require(!hit.next&&!hit.actor&&!hit.primitive&&hit.time==1.0f&&hit.item==-1&&Same(hit.location,{})&&Same(hit.normal,{}),
                "Prediction changed original native hit initialization");
            hit.actor=hitActor;if(onTrace)onTrace();return traceReturn;
        };
        q.writeAccelerationWord=[&](Id a,unsigned axis,std::uint32_t word){Source(a);events.push_back("wa"+std::to_string(axis));Write();
            state.acceleration.at(axis)=Float(word);accelerationWrites.emplace_back(axis,word);if(onAccelerationWrite)onAccelerationWrite();};
        q.writeVelocityWord=[&](Id a,unsigned axis,std::uint32_t word){Source(a);events.push_back("wv"+std::to_string(axis));Write();
            state.velocity.at(axis)=Float(word);velocityWrites.emplace_back(axis,word);if(onVelocityWrite)onVelocityWrite();};
        return q;
    }
    SetupStats Direct(float dt,const Queries& q){return QuestVr::SetupOriginalPawnFalling(1u,dt,q);}
    SetupStats Run(float dt=0.25f){const auto before=state;try{return Direct(dt,Query());}catch(...){state=before;throw;}}
    QuestVr::OriginalPawnFallingVelocityCapStats Cap(float cap){const auto before=state;try{
        return QuestVr::CapOriginalPawnFallingVelocity(1u,cap,Query());}catch(...){state=before;throw;}}
};

void ExactSetupAndPrediction(){
    Fixture low;const auto result=low.Run();
    Require(result.lowSpeed&&!result.traceCalls&&!result.normalCalls&&!result.clampedAcceleration&&
        result.limit==50.0f&&result.horizontalSpeedCap==0.0f&&result.accelerationWords==1u,"Low-speed limit/source branch changed");
    Require(low.events==std::vector<std::string>{"air","rate","wa2","v0","v1","v2","a2","a1","a0"},"Low-speed provider schedule changed");
    Require(low.accelerationWrites==std::vector<std::pair<unsigned,std::uint32_t>>{{2u,0u}}&&result.steps==low.work,
        "Acceleration Z raw zero or shared work changed");
    Fixture predicted;predicted.state.air=0.5f;predicted.state.rate=4.0f;predicted.state.acceleration={3,4,0};predicted.state.velocity={10,20,9};
    const auto p=predicted.Run();
    Require(p.traceCalls==1u&&!p.predictedHit&&p.control==0.5f&&p.normalCalls==2u&&p.clampedAcceleration,
        "Negative native trace INT was mistaken for a hit, or clamp skipped");
    Require(predicted.traces.size()==1u&&predicted.traces[0].level==2u&&predicted.traces[0].actor==1u&&
        predicted.traces[0].flags==6u&&!predicted.traces[0].extra&&Same(predicted.traces[0].extent,{2,2,4})&&
        Same(predicted.traces[0].start,{100,200,300})&&Same(predicted.traces[0].end,{102.8f,205.4f,300}),
        "Prediction changed endpoint/start/extent/source/native flags packet");
    const std::vector<std::string> prefix{"air","rate","a2","a1","a0","normal","v1","v0","radius","height","radius",
        "l2","level","l1","l0","l0","l1","l2","level","trace","rate","wa2","v0","v1","v2","ground"};
    Require(predicted.events.size()>=prefix.size()&&std::equal(prefix.begin(),prefix.end(),predicted.events.begin()),"Prediction fresh native read schedule changed");
    Require(predicted.accelerationWrites.size()==7u&&predicted.accelerationWrites[0]==std::pair<unsigned,std::uint32_t>{2u,0u}&&
        predicted.accelerationWrites[1]==std::pair<unsigned,std::uint32_t>{0u,Bits(0.6f)}&&
        predicted.accelerationWrites[2]==std::pair<unsigned,std::uint32_t>{1u,Bits(0.8f)}&&predicted.accelerationWrites[3].first==2u&&
        predicted.accelerationWrites[4]==std::pair<unsigned,std::uint32_t>{0u,Bits(1.2f)}&&
        predicted.accelerationWrites[5]==std::pair<unsigned,std::uint32_t>{1u,Bits(1.6f)}&&predicted.accelerationWrites[6].first==2u,
        "Normalized XYZ were not committed before scaled field reads/stores");
    Fixture hit;hit.state.air=0.5f;hit.state.rate=100;hit.state.acceleration={3,4,0};hit.hitActor=7u;hit.traceReturn=1;
    hit.onTrace=[&]{hit.Write();hit.state.air=0.9f;hit.state.rate=20;hit.state.velocity={30,40,4};};
    const auto h=hit.Run();
    Require(h.predictedHit&&h.control==0.05f&&h.limit==1.0f&&!h.lowSpeed&&h.horizontalSpeedCap==0.0f,
        "Actual hit did not override retained control or fresh post-trace rate/velocity");
    Fixture clear;clear.state.air=0.5f;clear.state.rate=100;clear.state.acceleration={3,4,0};
    clear.onTrace=[&]{clear.Write();clear.state.air=0.01f;clear.state.rate=20;clear.state.velocity={30,40,0};};
    const auto c=clear.Run();Require(c.control==0.5f&&c.limit==10.0f&&!c.predictedHit,"Trace mutation replaced retained control instead of fresh AccelRate only");
    Fixture unordered;unordered.state.air=Float(0x7fa12345u);auto noPrediction=unordered.Query();
    noPrediction.singleLineCheck={};noPrediction.radius={};noPrediction.height={};noPrediction.location={};noPrediction.level={};
    const auto u=unordered.Direct(0.25f,noPrediction);
    Require(!u.traceCalls&&Bits(u.control)==0x7fa12345u&&u.lowSpeed,"Unordered raw AirControl demanded a prediction or lost captured bits");
}

void HighSpeedAndCap(){
    for(float control:{0.05f,std::nextafter(0.05f,0.0f),std::nextafter(0.05f,1.0f),0.1f}){
        Fixture f;f.state.air=control;f.state.velocity={30,40,8};f.state.ground=50.0f;const auto r=f.Run();
        Require(!r.lowSpeed&&r.horizontalSpeedCap==(control>0.05f?50.0f:0.0f)&&
            r.limit==(control>0.05f?control*100.0f:1.0f),"Ground-speed equality/AirControl 0.05 boundary changed");
    }
    Fixture below;below.state.velocity={6,8,7};below.state.ground=11;const auto b=below.Run();
    Require(!b.lowSpeed&&!b.horizontalSpeedCap&&b.limit==10.0f,"Speed10/sub-GroundSpeed took low-speed boost or speed cap");
    Fixture cap;cap.state.velocity={30,40,Float(0x7fa12345u)};
    cap.onNormal=[&]{cap.Write();cap.state.velocity[2]=Float(0xffa23456u);};
    const auto r=cap.Cap(25.0f);
    // Stored Float0.6 times Float25 rounds one ULP above15, not to exact15.
    Require(r.clamped&&r.velocityWords==3u&&Bits(cap.state.velocity[0])==0x41700001u&&cap.state.velocity[1]==20.0f&&Bits(cap.state.velocity[2])==0xffa23456u,
        "Horizontal cap failed to preserve fresh raw Z after normalization");
    Require(cap.events==std::vector<std::string>{"v0","v1","v2","normal","v2","wv0","wv1","wv2"},"Cap source/read/normal/store order changed");
    Require(Same(cap.normals.front(),{30,40,0})&&r.steps==cap.work,"Horizontal cap normalized vertical source or escaped work");
    for(auto word:std::array<std::uint32_t,4u>{0u,0x80000000u,0x7fa12345u,0x7fc12345u}){
        const auto skipped=QuestVr::CapOriginalPawnFallingVelocity(1u,Float(word),{});
        Require(!skipped.steps&&!skipped.clamped&&!skipped.velocityWords,"Zero/NaN cap demanded inactive providers");
    }
    Fixture equal;equal.state.velocity={30,40,2};const auto e=equal.Cap(50.0f);
    Require(!e.clamped&&equal.events==std::vector<std::string>{"v0","v1","v2"},"Equal horizontal speed was clamped");
    Fixture negative;negative.state.velocity={30,40,2};negative.Cap(-25.0f);
    Require(Bits(negative.state.velocity[0])==0xc1700001u&&negative.state.velocity[1]==-20&&negative.state.velocity[2]==2,"Negative native cap was sanitized instead of signed multiplication");
}

void FailuresAndCallerUndo(){
    Refuses([]{QuestVr::SetupOriginalPawnFalling(0u,0.0f,{});},"Null setup source accepted");
    Refuses([]{QuestVr::CapOriginalPawnFallingVelocity(0u,0.0f,{});},"Null cap source accepted");
    for(unsigned missing=0u;missing<11u;++missing){Fixture f;f.state.air=0.5f;f.state.acceleration={3,4,0};auto q=f.Query();
        switch(missing){case 0:q.work={};break;case 1:q.airControl={};break;case 2:q.accelRate={};break;case 3:q.acceleration={};break;
        case 4:q.safeNormal={};break;case 5:q.velocity={};break;case 6:q.radius={};break;case 7:q.height={};break;
        case 8:q.location={};break;case 9:q.level={};break;case 10:q.singleLineCheck={};break;}
        Refuses([&]{f.Direct(0.25f,q);},"Missing prediction provider silently succeeded");if(!missing)Require(f.events.empty(),"Missing work ran source providers");
    }
    Fixture missingWrite;auto noWrite=missingWrite.Query();noWrite.writeAccelerationWord={};
    Refuses([&]{missingWrite.Direct(0.25f,noWrite);},"Missing raw acceleration store succeeded");
    Fixture missingGround;missingGround.state.velocity={10,0,0};auto noGround=missingGround.Query();noGround.groundSpeed={};
    Refuses([&]{missingGround.Direct(0.25f,noGround);},"Reached fresh GroundSpeed became optional");
    Fixture lazy;auto sparse=lazy.Query();sparse.safeNormal={};sparse.singleLineCheck={};sparse.radius={};sparse.height={};sparse.location={};sparse.level={};sparse.groundSpeed={};
    Require(lazy.Direct(0.25f,sparse).lowSpeed,"Low-speed nonprediction eagerly demanded unused providers");
    Fixture nullLevel;nullLevel.state.air=0.5f;nullLevel.state.acceleration={3,4,0};nullLevel.state.level=0;
    Refuses([&]{nullLevel.Run();},"Null reached prediction Level succeeded");Require(nullLevel.traces.empty(),"Null Level still dispatched prediction");
    const auto configure=[](Fixture& f){f.state.air=0.5f;f.state.rate=4;f.state.acceleration={3,4,0};f.state.velocity={10,20,1};};
    Fixture baseline;configure(baseline);const auto complete=baseline.Run();
    for(std::size_t cut=0u;cut<complete.steps;++cut){Fixture f;configure(f);f.limit=cut;const auto before=f.state;
        Refuses([&]{f.Run();},"Setup work cutoff silently succeeded");Require(f.state==before,"Setup work failure escaped explicit caller rollback");}
    for(std::size_t cut=0u;cut<baseline.writes;++cut){Fixture f;configure(f);f.writeLimit=cut;const auto before=f.state;
        Refuses([&]{f.Run();},"Setup write cutoff silently succeeded");Require(f.state==before,"Setup write failure escaped explicit caller rollback");}
    Fixture capBaseline;capBaseline.state.velocity={30,40,2};const auto capComplete=capBaseline.Cap(25);
    for(std::size_t cut=0u;cut<capComplete.steps;++cut){Fixture f;f.state.velocity={30,40,2};f.limit=cut;const auto before=f.state;
        Refuses([&]{f.Cap(25);},"Cap work cutoff silently succeeded");Require(f.state==before,"Cap work failure escaped explicit caller rollback");}
    for(std::size_t cut=0u;cut<3u;++cut){Fixture f;f.state.velocity={30,40,2};f.writeLimit=cut;const auto before=f.state;
        Refuses([&]{f.Cap(25);},"Cap write cutoff silently succeeded");Require(f.state==before,"Cap write failure escaped explicit caller rollback");}
    Fixture direct;auto broken=direct.Query();broken.velocity={};
    Refuses([&]{direct.Direct(0.25f,broken);},"Direct late setup failure was swallowed");
    Require(direct.state.acceleration[2]==0&&direct.writes==1u,"Helper performed forbidden private rollback");
    Fixture capMissing;capMissing.state.velocity={30,40,2};auto noCapWrite=capMissing.Query();noCapWrite.writeVelocityWord={};
    Refuses([&]{QuestVr::CapOriginalPawnFallingVelocity(1u,25,noCapWrite);},"Missing cap raw store silently succeeded");
    Fixture normalMissing;normalMissing.state.velocity={30,40,2};auto noCapNormal=normalMissing.Query();noCapNormal.safeNormal={};
    Refuses([&]{QuestVr::CapOriginalPawnFallingVelocity(1u,25,noCapNormal);},"Missing reached cap SafeNormal silently succeeded");
}

#if defined(__GNUC__) && (defined(__i386__) || defined(__x86_64__))
struct Precision {
    unsigned short saved{},selected{};
    Precision(){static_assert(std::numeric_limits<long double>::digits==64,"Falling oracle requires own x87 PC64");
        asm volatile("fnstcw %0":"=m"(saved));selected=static_cast<unsigned short>((saved&~0x0f00u)|0x033fu);
        asm volatile("fnclex; fldcw %0"::"m"(selected):"memory");}
    ~Precision(){asm volatile("fnclex; fldcw %0"::"m"(saved):"memory");}
};
struct OracleResult {float limit{},cap{};bool low{};};
OracleResult Oracle(float control,float rate,float vx,float vy,float delta,float ground){
    OracleResult out;double sum{};long double speed{};unsigned short status{};
    const double tenDouble=10.0;const float tenFloat=10.0f,threshold=0.05f;
    asm volatile("flds %[control]; fmuls %[rate]; fstps %[limit]; flds %[vy]; fmuls %[vy]; flds %[vx]; fmuls %[vx]; "
        "faddp; fstpl %[sum]; fldl %[sum]; fsqrt; fcoml %[ten]; fnstsw %%ax; fstpt %[speed]":
        [limit]"=m"(out.limit),[sum]"=m"(sum),[speed]"=m"(speed),"=a"(status):
        [control]"m"(control),[rate]"m"(rate),[vx]"m"(vx),[vy]"m"(vy),[ten]"m"(tenDouble):"st","memory");
    if(status&0x0100u){out.low=true;
        // Explicit Intel D8 E1 avoids GAS register FSUB/FSUBR spelling ambiguity.
        asm volatile("fldt %[speed]; flds %[ten]; .byte 0xd8,0xe1; fdivs %[delta]; fadds %[limit]; fstps %[limit]; fstp %%st(0)":
            [limit]"+m"(out.limit):[speed]"m"(speed),[ten]"m"(tenFloat),[delta]"m"(delta):"st","memory");
    }else{
        asm volatile("fldt %1; fcoms %2; fnstsw %%ax; fstp %%st(0)":"=a"(status):"m"(speed),"m"(ground):"st","memory");
        if(!(status&0x0100u)){
            asm volatile("flds %1; fcomps %2; fnstsw %%ax":"=a"(status):"m"(control),"m"(threshold):"st","memory");
            if(!(status&0x4100u))asm volatile("fldt %1; fstps %0":"=m"(out.cap):"m"(speed):"st","memory");
            else out.limit=1.0f;
        }
    }
    return out;
}
void NumericOracle(){Precision precision;
    const auto compare=[](float control,float rate,float vx,float vy,float delta,float ground){
        const auto expected=Oracle(control,rate,vx,vy,delta,ground);Fixture f;f.state.air=control;f.state.rate=rate;
        f.state.velocity={vx,vy,Float(0x7fa12345u)};f.state.ground=ground;f.state.acceleration={0,0,0};
        const auto actual=f.Run(delta);++oracleCases;
        Require(Bits(actual.limit)==Bits(expected.limit)&&Bits(actual.horizontalSpeedCap)==Bits(expected.cap)&&actual.lowSpeed==expected.low,
            "Stored falling limit/cap differs from independent x87 source schedule");
    };
    for(float control:{0.0f,0.05f,0.1f,0.15f})for(float rate:{0.0f,1.0f,100.0f})
        for(const auto& velocity:std::array<std::array<float,2u>,8u>{{{0,0},{3,4},{6,8},{10,0},{30,40},{-30,-40},{320,0},{1000,1}}})
            for(float delta:{0.0f,0.001f,0.25f,10.0f})for(float ground:{10.0f,50.0f,320.0f})
                compare(control,rate,velocity[0],velocity[1],delta,ground);
    std::uint64_t seed=0x103d0d66103d0ea0ull;
    const auto random=[&]{seed^=seed<<13u;seed^=seed>>7u;seed^=seed<<17u;return static_cast<std::uint32_t>(seed);};
    for(unsigned i=0u;i<8192u;++i){const auto a=random(),b=random(),c=random(),d=random();
        const auto rate=Float(((115u+a%28u)<<23u)|(a&0x7fffffu));
        const auto vx=Float((b&0x80000000u)|((110u+b%35u)<<23u)|(b&0x7fffffu));
        const auto vy=Float((c&0x80000000u)|((110u+c%35u)<<23u)|(c&0x7fffffu));
        const auto delta=Float(((110u+d%20u)<<23u)|(d&0x7fffffu));compare(0.1f,rate,vx,vy,delta,320.0f);
    }
}
#else
void NumericOracle(){throw std::runtime_error("Falling setup acceptance requires own GNU x87 oracle, no silent skip");}
#endif
} // namespace

int main(){try{ExactSetupAndPrediction();HighSpeedAndCap();FailuresAndCallerUndo();NumericOracle();
    std::cout<<"Original Pawn falling setup/cap: "<<checks<<" checks / "<<refusals<<" refusals / "<<oracleCases
        <<" independent x87 cases; setup and retained horizontal cap only, NOT Pawn landing/Walking or playable Training\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
