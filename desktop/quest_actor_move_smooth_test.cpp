#include "quest_actor_move_smooth.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

// Detached numeric/protocol fixtures only. Mock MoveActor/HitWall providers
// are NOT a runtime movement implementation or evidence of playable physics.
namespace {
using Vec=PortableModelVec3;
using Hit=QuestVr::OriginalActorLineHit;
using Move=QuestVr::OriginalActorSmoothMove;
using Queries=QuestVr::OriginalActorSmoothQueries;
constexpr std::uintptr_t actor=17u;
std::size_t checks{},refusals{},protocols{},firstSlides{},forward{},backward{},faultPrefixes{},roots{};
void Require(bool value,const char* reason) {++checks;if(!value)throw std::runtime_error(reason);}
std::uint32_t Bits(float value) {return std::bit_cast<std::uint32_t>(value);}
bool Same(float a,float b) {return Bits(a)==Bits(b);}
bool Same(Vec a,Vec b) {return Same(a.x,b.x)&&Same(a.y,b.y)&&Same(a.z,b.z);}
bool Same(const Hit& a,const Hit& b) {
    return a.next==b.next&&a.actor==b.actor&&a.primitive==b.primitive&&a.item==b.item&&
        Same(a.time,b.time)&&Same(a.location,b.location)&&Same(a.normal,b.normal);
}
template<class F>void Refuse(F run) {
    bool failed{};try {run();}catch(const std::exception&) {failed=true;}
    Require(failed,"Unsupported moveSmooth provider/arithmetic silently succeeded");++refusals;
}
struct Fixture {
    Queries queries;
    std::vector<std::string> reads;
    std::vector<Move> calls;
    std::vector<std::uintptr_t> levels{101u,102u,201u,202u,301u};
    std::size_t level{},rotationReads{},work{},wall{},throwAt=std::numeric_limits<std::size_t>::max();
    Hit* packet{};
    std::function<std::int32_t(std::size_t,Move,Hit&)> onMove;
    std::function<void(Vec,std::uintptr_t)> onWall;
    void Read(std::string name) {
        const auto index=reads.size();reads.push_back(std::move(name));
        if(index==throwAt)throw std::runtime_error("Injected active provider refusal");
    }
    Fixture() {
        queries.work=[&]{++work;};
        queries.rotationChannel=[&](std::uintptr_t who,unsigned channel) {
            Require(who==actor&&channel<3u,"Rotation provider used wrong actor/channel");
            Read("R"+std::to_string(channel));++rotationReads;
            return static_cast<std::int32_t>(rotationReads*100u+channel);
        };
        queries.xLevel=[&](std::uintptr_t who) {
            Require(who==actor,"XLevel provider used wrong source");Read("X");
            Require(level<levels.size(),"moveSmooth read an extra inactive XLevel");return levels[level++];
        };
        queries.moveActor=[&](Move call,Hit& hit) {
            Read("M");Require(call.actor==actor&&call.options==std::array<std::int32_t,4u>{},
                "MoveActor options/source do not preserve four literal zero flags");
            if(!packet) {packet=&hit;Require(Same(hit,Hit{}),"First MoveActor did not receive original Time1/Item-1 zero packet");}
            else Require(packet==&hit,"moveSmooth reset/copied the live hit packet between attempts");
            const auto index=calls.size();calls.push_back(call);
            return onMove?onMove(index,call,hit):std::int32_t{71};
        };
        queries.hitWall=[&](std::uintptr_t who,Vec normal,std::uintptr_t peer) {
            Require(who==actor,"HitWall dispatched on candidate instead of source");Read("H");++wall;
            if(onWall)onWall(normal,peer);
        };
    }
};
void Protocol() {
    const float nan=std::numeric_limits<float>::quiet_NaN(),inf=std::numeric_limits<float>::infinity();
    for(const std::int32_t returned:{0,1,-7,std::numeric_limits<std::int32_t>::min(),std::numeric_limits<std::int32_t>::max()})
        for(const float time:{1.0f,2.0f,inf}) {
            Fixture f;f.onMove=[&](std::size_t,Move call,Hit& hit) {
                Require(Same(call.delta,Vec{nan,inf,-inf}),"First original Delta raw words changed before MoveActor");
                hit.time=time;hit.normal={nan,inf,-inf};return returned;
            };
            Require(QuestVr::MoveOriginalActorSmooth(actor,{nan,inf,-inf},f.queries)==returned&&f.calls.size()==1u&&f.wall==0u,
                "Ordered first clear read inactive NaN Delta/Normal, normalized return, or dispatched extra movement");
            Require(f.reads==std::vector<std::string>{"R0","X","R1","R2","X","M"},
                "First MoveActor raw pose/dispatch/this sample ordering differs");++protocols;
        }
    for(const std::int32_t returned:{0,-123,54321}) {
        Fixture f;Vec original{4,8,0};
        f.onMove=[&](std::size_t index,Move call,Hit& hit) {
            if(index==0u) {
                Require(Same(call.delta,Vec{4,8,0}),"First MoveActor did not receive original Delta by value");
                hit={71u,0u,89u,{31,32,33},{1,0,0},.5f,109};
                original={-100,-100,0};return returned;
            }
            Require(hit.next==71u&&hit.primitive==89u&&hit.item==109&&Same(hit.location,Vec{31,32,33}),
                "A later move reset packet fields untouched by its earlier provider");
            if(index==1u) {
                Require(Same(call.delta,Vec{0,4,0}),"Slide used callback-mutated caller Delta or wrong first-wall projection");
                hit.normal={0,1,0};hit.actor=0;hit.time=nan;return std::int32_t{999};
            }
            Require(index==2u&&Same(call.delta,Vec{0,3,0}),
                "Third MoveActor ignored post-HitWall Normal/Time or recomputed DesiredDir from mutated caller Delta");
            hit.time=nan;hit.normal={inf,nan,-inf};return std::int32_t{-999};
        };
        f.onWall=[&](Vec normal,std::uintptr_t peer) {
            Require(Same(normal,Vec{0,1,0})&&peer==0u,"HitWall did not receive copied second Normal/None Actor");
            normal={nan,nan,nan}; // Event parameters are copies, not H aliases.
            Require(Same(f.packet->normal,Vec{0,1,0}),"HitWall copy aliases live hit Normal");
            f.packet->normal={1,0,0};f.packet->time=.25f;
        };
        Require(QuestVr::MoveOriginalActorSmooth(actor,original,f.queries)==returned&&f.calls.size()==3u&&f.wall==1u&&f.work==4u,
            "Three-attempt wrapper did not retain exact first native integer or ignored final inactive packet");
        Require(f.reads==std::vector<std::string>{"R0","X","R1","R2","X","M",
            "X","R0","R1","R2","X","M","H","R0","X","R1","R2","M"},
            "Three-attempt native field-read transcript differs from complete original body");
        Require(f.calls[0].dispatchLevel==101u&&f.calls[0].level==102u&&f.calls[1].dispatchLevel==201u&&
            f.calls[1].level==202u&&f.calls[2].dispatchLevel==301u&&f.calls[2].level==301u&&
            f.calls[0].rotation==std::array<std::int32_t,3u>{100,201,302}&&
            f.calls[1].rotation==std::array<std::int32_t,3u>{400,501,602}&&
            f.calls[2].rotation==std::array<std::int32_t,3u>{700,801,902},
            "Move attempts reused stale Rotation/XLevel or conflated dispatchLevel with reread this-Level");++protocols;
    }
    // Projection of Delta along its only wall produces exact zero; ordered
    // gate zero STILL calls MoveActor2, and retained Time calls HitWall/Move3.
    Fixture zero;zero.onMove=[&](std::size_t index,Move call,Hit& hit) {
        if(index==0u) {hit.normal={1,0,0};hit.time=.5f;return std::int32_t{0};}
        Require(Same(call.delta,Vec{}),"Exact-zero slide or two-wall delta was changed");
        return std::int32_t{7}; // Do not reset H: Time remains .5/Normal remains X.
    };
    Require(QuestVr::MoveOriginalActorSmooth(actor,{1,0,0},zero.queries)==0&&zero.calls.size()==3u&&zero.wall==1u,
        "Exact-zero slide suppressed a real second or third MoveActor call");++protocols;
    Fixture away;away.onMove=[](std::size_t,Move,Hit& hit){hit.time=.5f;hit.normal={2,0,0};return -19;};
    Require(QuestVr::MoveOriginalActorSmooth(actor,{1,0,0},away.queries)==-19&&away.calls.size()==1u&&away.wall==0u,
        "Negative forward gate did not stop before SafeNormal/second movement");++protocols;
    Fixture secondClear;secondClear.onMove=[&](std::size_t index,Move,Hit& hit) {
        if(index==0u){hit.time=-1;hit.normal={1,0,0};return 37;}
        hit.time=inf;hit.normal={nan,inf,nan};return -37;
    };
    secondClear.queries.hitWall={};
    Require(QuestVr::MoveOriginalActorSmooth(actor,{4,8,0},secondClear.queries)==37&&secondClear.calls.size()==2u,
        "Second ordered clear required inactive HitWall/TwoWall providers or normalized negative first Time");
    Require(Same(secondClear.calls[1].delta,Vec{0,16,0}),"Finite negative first Time was clamped instead of native one-minus-Time");++protocols;
}

void ProviderFailures() {
    Fixture golden;golden.onMove=[](std::size_t index,Move,Hit& hit) {
        hit.normal={1,0,0};hit.time=.5f;return index==0u?-71:71;
    };
    Require(QuestVr::MoveOriginalActorSmooth(actor,{1,2,0},golden.queries)==-71,"Failure fixture did not reach full protocol");
    for(std::size_t index=0u;index<golden.reads.size();++index) {
        Fixture failure;failure.throwAt=index;failure.onMove=golden.onMove;
        Refuse([&]{QuestVr::MoveOriginalActorSmooth(actor,{1,2,0},failure.queries);});
        Require(failure.reads.size()==index+1u&&std::equal(failure.reads.begin(),failure.reads.end(),golden.reads.begin()),
            "Wrapper continued providers after failure or changed active prefix order");++faultPrefixes;
    }
    Fixture empty;empty.queries.rotationChannel={};Refuse([&]{QuestVr::MoveOriginalActorSmooth(actor,{},empty.queries);});
    Fixture noLevel;noLevel.queries.xLevel={};Refuse([&]{QuestVr::MoveOriginalActorSmooth(actor,{},noLevel.queries);});
    Fixture noMove;noMove.queries.moveActor={};Refuse([&]{QuestVr::MoveOriginalActorSmooth(actor,{},noMove.queries);});
    Fixture noWall;noWall.queries.hitWall={};noWall.onMove=golden.onMove;
    Refuse([&]{QuestVr::MoveOriginalActorSmooth(actor,{1,2,0},noWall.queries);});
    Fixture noSource;Refuse([&]{QuestVr::MoveOriginalActorSmooth(0u,{},noSource.queries);});
    Require(noSource.reads.empty()&&noSource.work==0u,"Null portable source admission read active providers");
    for(const unsigned attempt:{1u,2u,3u}) {
        Fixture bad;bad.onMove=golden.onMove;bad.levels[attempt==1u?0u:attempt==2u?2u:4u]=0u;
        Refuse([&]{QuestVr::MoveOriginalActorSmooth(actor,{1,2,0},bad.queries);});
        const std::vector<std::string> ending=attempt==1u?std::vector<std::string>{"R0","X","R1","R2"}:
            attempt==2u?std::vector<std::string>{"X","R0"}:std::vector<std::string>{"R0","X"};
        Require(bad.reads.size()>=ending.size()&&std::equal(ending.begin(),ending.end(),bad.reads.end()-static_cast<std::ptrdiff_t>(ending.size())),
            "Null dispatch-Level refused at the wrong source load boundary");
    }
    // Calls1/2 choose dispatch from the FIRST Level read but invoke it on the
    // reread receiver. A valid dispatch token cannot legitimize a null this.
    for(const unsigned receiver:{1u,3u}) {
        Fixture bad;bad.onMove=golden.onMove;bad.levels[receiver]=0u;
        Refuse([&]{QuestVr::MoveOriginalActorSmooth(actor,{1,2,0},bad.queries);});
        const std::vector<std::string> expected=receiver==1u?
            std::vector<std::string>{"R0","X","R1","R2","X"}:
            std::vector<std::string>{"R0","X","R1","R2","X","M","X","R0","R1","R2","X"};
        Require(bad.reads==expected&&bad.calls.size()==(receiver==1u?0u:1u),
            "Null receiver XLevel reached MoveActor or reused a nonnull dispatch-Level");
    }
    const float nan=std::numeric_limits<float>::quiet_NaN();
    Fixture badFirst;badFirst.onMove=[&](std::size_t,Move,Hit& hit){hit.time=nan;hit.normal={1,0,0};return 17;};
    Refuse([&]{QuestVr::MoveOriginalActorSmooth(actor,{1,2,0},badFirst.queries);});
    Require(badFirst.calls.size()==1u,"First unordered Time skipped active arithmetic refusal");
    Fixture badAfterEvent;badAfterEvent.onMove=[&](std::size_t,Move,Hit& hit){hit.time=.5f;hit.normal={1,0,0};return 17;};
    badAfterEvent.onWall=[&](Vec,std::uintptr_t){badAfterEvent.packet->time=nan;};
    Refuse([&]{QuestVr::MoveOriginalActorSmooth(actor,{1,2,0},badAfterEvent.queries);});
    Require(badAfterEvent.calls.size()==2u&&badAfterEvent.wall==1u,"Unsafe postevent Time did not refuse before third movement");
}

void CallerOwnedRoot() {
    // Explicit mock root proves the protocol propagates errors so a CALLER
    // can poison/rewind coupled movement. No automatic runtime transaction or
    // accepted original-world movement is supplied by this detached wrapper.
    struct MockRoot {
        int position{},before{};std::size_t used{},limit{100u};bool poison{};
        void Work(){if(++used>limit)throw std::runtime_error("Shared mock root work exhausted");}
        void Call(const std::function<void()>& run){try{run();}catch(...){poison=true;throw;}}
        void Commit(){if(poison)throw std::runtime_error("Poisoned mock root cannot publish");}
        void Rollback(){position=before;}
    };
    MockRoot root;Fixture failure;failure.onMove=[&](std::size_t index,Move,Hit& hit) {
        ++root.position;hit.time=.5f;hit.normal={1,0,0};if(index==1u)throw std::runtime_error("Later real provider failed");return 0;
    };
    failure.queries.work=[&]{root.Work();};
    Refuse([&]{root.Call([&]{static_cast<void>(QuestVr::MoveOriginalActorSmooth(actor,{1,2,0},failure.queries));});});
    Require(root.position==2&&root.poison,"Nested provider error was hidden or local wrapper pretended to undo external movement");
    Refuse([&]{root.Commit();});root.Rollback();Require(root.position==0,"Mock caller failed to restore complete coupled movement");++roots;
    MockRoot success;Fixture ordinary;ordinary.onMove=[&](std::size_t,Move,Hit&){++success.position;return 0;};
    ordinary.queries.work=[&]{success.Work();};
    success.Call([&]{Require(QuestVr::MoveOriginalActorSmooth(actor,{},ordinary.queries)==0,
        "Mock false result changed native return");});success.Commit();
    Require(success.position==1&&!success.poison,"Ordinary native zero result incorrectly discarded successful earlier movement");++roots;
    MockRoot shared;shared.limit=1u;Fixture first,second;
    first.queries.work=second.queries.work=[&]{shared.Work();};
    shared.Call([&]{static_cast<void>(QuestVr::MoveOriginalActorSmooth(actor,{},first.queries));});
    Refuse([&]{shared.Call([&]{static_cast<void>(QuestVr::MoveOriginalActorSmooth(actor,{},second.queries));});});
    Require(shared.used==2u&&second.calls.empty()&&shared.poison,"Wrapper reset a caller's work allowance between nested smooth calls");++roots;
}

#if (defined(__i386__) || defined(__x86_64__)) && defined(__GNUC__)
namespace Oracle {
using Real=long double;
struct Precision {
    unsigned short previous{};
    Precision(){static_assert(std::numeric_limits<Real>::digits==64);asm volatile("fnstcw %0":"=m"(previous));
        const auto mode=static_cast<unsigned short>((previous&~0x0f00u)|0x033fu);asm volatile("fldcw %0"::"m"(mode));}
    ~Precision(){asm volatile("fldcw %0"::"m"(previous));}
};
Real Read(float value){if(!std::isfinite(value))throw std::runtime_error("Oracle active Float is nonfinite");return value;}
Real Add(Real a,Real b){volatile Real x=a,y=b,z=x+y;return z;}
Real Sub(Real a,Real b){volatile Real x=a,y=b,z=x-y;return z;}
Real Mul(Real a,Real b){volatile Real x=a,y=b,z=x*y;return z;}
float Store(Real value){volatile float x=static_cast<float>(value);if(!std::isfinite(x))throw std::runtime_error("Oracle Float overflow");return x;}
struct Slide {Vec delta;bool forward,storedXForward;};
Slide Project(Vec delta,Vec normal,float time) {
    // Independent hardware transcription: no production arithmetic, vector,
    // normalization, projection or trace helper is included in this oracle.
    const auto dot=Add(Add(Mul(Read(delta.z),Read(normal.z)),Mul(Read(delta.y),Read(normal.y))),Mul(Read(normal.x),Read(delta.x)));
    const float pz=Store(Mul(Read(normal.z),dot)),py=Store(Mul(Read(normal.y),dot));
    const auto px=Mul(Read(normal.x),dot);
    const float rz=Store(Sub(Read(delta.z),pz)),ry=Store(Sub(Read(delta.y),py));
    const auto rx=Sub(Read(delta.x),px),remaining=Sub(1.0,Read(time));
    const float z=Store(Mul(rz,remaining)),y=Store(Mul(ry,remaining));
    const auto retainedX=Mul(rx,remaining);const float x=Store(retainedX);
    const auto gate=Add(Add(Mul(Read(delta.y),y),Mul(Read(delta.z),z)),Mul(retainedX,Read(delta.x)));
    const auto wrongGate=Add(Add(Mul(Read(delta.y),y),Mul(Read(delta.z),z)),Mul(x,Read(delta.x)));
    return {{x,y,z},gate>=0.0L,wrongGate>=0.0L};
}
} // namespace Oracle
std::uint64_t Next(std::uint64_t& state){state^=state<<13u;state^=state>>7u;state^=state<<17u;return state;}
float Number(std::uint64_t& state,bool broad) {
    const auto raw=static_cast<std::uint32_t>(Next(state));
    return broad?std::bit_cast<float>((raw&0x807fffffu)|(((raw>>23u)%255u)<<23u)):
        static_cast<float>(static_cast<int>(raw%65537u)-32768)/8192.0f;
}
void CompareSlide(Vec delta,Vec normal,float time) {
    bool failed{},oracleFailed{};QuestVr::OriginalActorSlide actual;Oracle::Slide expected;
    try{actual=QuestVr::ProjectOriginalActorSlide(delta,normal,time);}catch(const std::exception&){failed=true;}
    try{expected=Oracle::Project(delta,normal,time);}catch(const std::exception&){oracleFailed=true;}
    Require(failed==oracleFailed,"First slide finite refusal differs from independent x87");
    if(failed){++refusals;return;}
    Require(Same(actual.delta,expected.delta)&&actual.forward==expected.forward,
        "First slide packet Float words or retained-X forward gate differ from independent x87");
    ++firstSlides;if(actual.forward)++forward;else ++backward;
}
void FirstSlideOracle() {
    Oracle::Precision precision;std::uint64_t state=0x39af7b25c461d803ull;
    // Explicit cancellation witnesses: merely reloading stored Slide.X flips
    // the native forward gate. These words were selected with an independent
    // integer PC64 search, and are checked here against actual hardware x87.
    constexpr std::array<std::array<std::uint32_t,5u>,3u> retainedXWords{{
        {0x414d2400u,0x4138eb00u,0x3f913b00u,0x3e707cf1u,0x3c5c0000u},
        {0x40578800u,0x3fec0800u,0x3f82b100u,0x3e5eaca6u,0x3da75000u},
        {0x412f8200u,0x416c4e00u,0x3f476800u,0x3f2ac848u,0x3dddc000u}}};
    for(const auto& words:retainedXWords) {
        const Vec delta{std::bit_cast<float>(words[0]),std::bit_cast<float>(words[1]),0},
            normal{std::bit_cast<float>(words[2]),std::bit_cast<float>(words[3]),0};
        const float time=std::bit_cast<float>(words[4]);const auto expected=Oracle::Project(delta,normal,time);
        Require(expected.forward!=expected.storedXForward,"Retained-X witness did not distinguish the incorrectly Float-reloaded gate");
        CompareSlide(delta,normal,time);
        Fixture f;f.onMove=[&](std::size_t index,Move call,Hit& hit) {
            if(index==0u){hit.normal=normal;hit.time=time;return -31;}
            Require(index==1u&&Same(call.delta,expected.delta),"Retained-X gate changed second MoveActor Float arguments");
            hit.time=1;return 31;
        };
        Require(QuestVr::MoveOriginalActorSmooth(actor,delta,f.queries)==-31&&
            f.calls.size()==(expected.forward?2u:1u),"Retained-X cancellation changed whether real second movement executes");
        ++protocols;
    }
    for(unsigned i=0u;i<40'000u;++i){const bool broad=i<8'000u;const auto next=[&]{return Number(state,broad);};
        const Vec delta{next(),next(),next()},normal{next(),next(),next()};CompareSlide(delta,normal,next());}
    const std::array<std::uint32_t,13u> words{0u,0x80000000u,1u,0x80000001u,0x3f000000u,0x3f7fffffu,
        0x3f800000u,0x3f800001u,0xbf800000u,0x40000000u,0x4b800000u,0x00800000u,0x7f7fffffu};
    for(auto a:words)for(auto b:words)for(float time:{-1.0f,-0.0f,.25f,.5f,1.0f,2.0f}) {
        const auto x=std::bit_cast<float>(a),y=std::bit_cast<float>(b);
        CompareSlide({x,y,0},{1,0,0},time);CompareSlide({x,1,-y},{.5f,-.25f,.125f},time);
    }
    // Deliberately nonunit normals cross the sign gate; no normalization,
    // positivity constraint or Time interval clamp belongs to first projection.
    for(float edge:{std::nextafter(1.0f,0.0f),1.0f,std::nextafter(1.0f,2.0f),2.0f})
        CompareSlide({1,0,0},{edge,0,0},.5f);
    Require(firstSlides>20'000u&&forward>1'000u&&backward>1'000u,"First-slide oracle missed finite or both gate branches");
}
#else
void FirstSlideOracle(){std::cout<<"SKIP independent hardware-x87 first-slide oracle on non-x86 compiler; protocol controls still run.\n";}
#endif
} // namespace

int main(){try{Protocol();ProviderFailures();CallerOwnedRoot();FirstSlideOracle();
    std::cout<<"PASS original moveSmooth "<<checks<<" checks, "<<refusals<<" explicit refusals, "<<protocols
        <<" protocol controls, "<<faultPrefixes<<" active provider prefixes, "<<roots<<" caller-owned mock roots, "
        <<firstSlides<<" independent x87 first slides ("<<forward<<" forward/"<<backward<<" rejected). "
        "No runtime MoveActor or map physics acceptance.\n";return 0;}
    catch(const std::exception& error){std::cerr<<"Original moveSmooth failed: "<<error.what()<<'\n';return 1;}}
