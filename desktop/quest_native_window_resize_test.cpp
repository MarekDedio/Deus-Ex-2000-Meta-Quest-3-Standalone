#include "quest_native_window_resize.h"
#include <array>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

namespace {
unsigned checks{};
void Require(bool condition,const char* message) {++checks;if(!condition) throw std::runtime_error(message);}
std::uint32_t Raw(float value) {std::uint32_t bits;std::memcpy(&bits,&value,4u);return bits;}
float Float(std::uint32_t bits) {float value;std::memcpy(&value,&bits,4u);return value;}
struct Fixture {
    bool hasParent{true};
    QuestVr::NativeWindowPreferredSize parent{100.0f,80.0f},preferred{10.0f,20.0f};
    std::uint8_t horizontal{},vertical{};
    std::array<float,4u> margins{2.0f,7.0f,3.0f,11.0f}; // h0,h1,v0,v1.
};
struct Recorded {
    bool widthSpecified{},heightSpecified{};
    float seedWidth{},seedHeight{};
    std::array<float,4u> geometry{};
};
struct Operations {
    Fixture fixture;
    Recorded recorded;
    std::vector<std::string> events;
    std::function<void(Operations&)> callback;
    bool echoSpecified{};
    unsigned configurations{};
    QuestVr::NativeWindowPreferredSize CaptureParentSize() {
        events.emplace_back("parent");return fixture.hasParent ? fixture.parent : QuestVr::NativeWindowPreferredSize{0.0f,0.0f};
    }
    std::uint8_t ReadHorizontalAlignment() {events.emplace_back("ha");return fixture.horizontal;}
    std::uint8_t ReadVerticalAlignment() {events.emplace_back("va");return fixture.vertical;}
    float ReadHorizontalMargin0() {events.emplace_back("h0");return fixture.margins[0u];}
    float ReadHorizontalMargin1() {events.emplace_back("h1");return fixture.margins[1u];}
    float ReadVerticalMargin0() {events.emplace_back("v0");return fixture.margins[2u];}
    float ReadVerticalMargin1() {events.emplace_back("v1");return fixture.margins[3u];}
    QuestVr::NativeWindowPreferredSize QueryPreferredSize(bool widthSpecified,float width,bool heightSpecified,float height) {
        events.emplace_back("query");
        recorded.widthSpecified=widthSpecified;recorded.heightSpecified=heightSpecified;
        recorded.seedWidth=width;recorded.seedHeight=height;
        if(callback) callback(*this);
        return {echoSpecified && widthSpecified ? width : fixture.preferred.width,
            echoSpecified && heightSpecified ? height : fixture.preferred.height};
    }
    void ConfigureChild(float x,float y,float width,float height) {
        events.emplace_back("configure");++configurations;recorded.geometry={x,y,width,height};
    }
};
Recorded Compute(const Fixture& fixture) {
    Operations operations;operations.fixture=fixture;QuestVr::ResizeNativeWindowChild(operations);
    Require(operations.configurations==1u,"Resize did not configure exactly once");return operations.recorded;
}
bool Same(const Recorded& a,const Recorded& b) {
    if(a.widthSpecified!=b.widthSpecified || a.heightSpecified!=b.heightSpecified ||
        Raw(a.seedWidth)!=Raw(b.seedWidth) || Raw(a.seedHeight)!=Raw(b.seedHeight)) return false;
    for(std::size_t index=0u;index<a.geometry.size();++index)
        if(Raw(a.geometry[index])!=Raw(b.geometry[index])) return false;
    return true;
}
void Refuses(Fixture fixture) {
    Operations operations;operations.fixture=fixture;bool refused{};
    try {QuestVr::ResizeNativeWindowChild(operations);} catch(const std::runtime_error&) {refused=true;}
    Require(refused,"Unsafe resize arithmetic was accepted");
    Require(operations.configurations==0u,"Refused resize published geometry");
}

#if (defined(__i386__) || defined(__x86_64__)) && defined(__GNUC__)
struct RetailPrecisionOracle {
    unsigned short original{};
    RetailPrecisionOracle() {
        asm volatile("fnstcw %0" : "=m"(original));
        const unsigned short pc64Nearest=static_cast<unsigned short>((original&~0x0f00u)|0x0300u);
        asm volatile("fldcw %0" : : "m"(pc64Nearest));
    }
    ~RetailPrecisionOracle() {asm volatile("fldcw %0" : : "m"(original));}
};
float OracleStore(long double value) {
    const float stored=static_cast<float>(value);
    if(!std::isfinite(stored)) throw std::runtime_error("Oracle Float32 overflow");
    return stored;
}
float OracleStretch(float parent,float margin1,float margin0) {
    for(float value:{parent,margin1,margin0})
        if(!std::isfinite(value)) throw std::runtime_error("Oracle nonfinite input");
    volatile long double a=margin1,b=margin0,p=parent;
    volatile long double sum=a+b;
    volatile long double difference=p-sum;
    return OracleStore(difference);
}
float OraclePosition(std::uint8_t alignment,float parent,float preferred,float margin0) {
    if(!std::isfinite(margin0)) throw std::runtime_error("Oracle nonfinite margin");
    if(alignment!=1u && alignment!=2u) return margin0;
    if(!std::isfinite(parent) || !std::isfinite(preferred)) throw std::runtime_error("Oracle nonfinite dimensions");
    volatile long double p=parent,s=preferred,m=margin0;
    volatile long double difference=p-s;
    if(alignment==1u) {
        volatile long double half=difference*0.5L;
        const long double whole=std::trunc(half);
        if(whole<static_cast<long double>(std::numeric_limits<std::int32_t>::min()) ||
            whole>static_cast<long double>(std::numeric_limits<std::int32_t>::max()))
            throw std::runtime_error("Oracle signed32 center overflow");
        const auto integer=static_cast<std::int32_t>(whole);
        volatile long double converted=integer;
        volatile long double result=converted+m;
        return OracleStore(result);
    }
    volatile long double result=difference-m;
    return OracleStore(result);
}
// Independent host x87 implementation; no original executable or DLL is loaded.
Recorded Oracle(const Fixture& fixture) {
    Recorded recorded;
    const auto parent=fixture.hasParent ? fixture.parent : QuestVr::NativeWindowPreferredSize{0.0f,0.0f};
    recorded.widthSpecified=fixture.horizontal==3u;recorded.heightSpecified=fixture.vertical==3u;
    if(recorded.widthSpecified) recorded.seedWidth=OracleStretch(parent.width,fixture.margins[1u],fixture.margins[0u]);
    if(recorded.heightSpecified) recorded.seedHeight=OracleStretch(parent.height,fixture.margins[3u],fixture.margins[2u]);
    if(!std::isfinite(fixture.preferred.width) || !std::isfinite(fixture.preferred.height))
        throw std::runtime_error("Oracle nonfinite query result");
    recorded.geometry={OraclePosition(fixture.horizontal,parent.width,fixture.preferred.width,fixture.margins[0u]),
        OraclePosition(fixture.vertical,parent.height,fixture.preferred.height,fixture.margins[2u]),
        fixture.preferred.width,fixture.preferred.height};
    return recorded;
}
void CheckOracle(const Fixture& fixture) {
    bool actualRefused{},oracleRefused{};Recorded actual,expected;
    try {actual=Compute(fixture);} catch(const std::runtime_error&) {actualRefused=true;}
    try {expected=Oracle(fixture);} catch(const std::runtime_error&) {oracleRefused=true;}
    if(actualRefused!=oracleRefused || (!actualRefused && !Same(actual,expected))) {
        std::cerr << std::hex << "Resize oracle mismatch parent=" << Raw(fixture.parent.width) << ',' << Raw(fixture.parent.height)
            << " preferred=" << Raw(fixture.preferred.width) << ',' << Raw(fixture.preferred.height)
            << " alignment=" << static_cast<unsigned>(fixture.horizontal) << ',' << static_cast<unsigned>(fixture.vertical)
            << " margins=";
        for(float value:fixture.margins) std::cerr << Raw(value) << ',';
        std::cerr << " actualRefused=" << actualRefused << " oracleRefused=" << oracleRefused << std::dec << '\n';
        throw std::runtime_error("Native child resize PC64 x87 oracle mismatch");
    }
    ++checks;
}
#endif
} // namespace

int main() {
    try {
        const float negativeZero=Float(0x80000000u),maximum=std::numeric_limits<float>::max();
        Operations operations;operations.fixture.hasParent=false;
        operations.fixture.margins={negativeZero,maximum,negativeZero,-maximum};
        QuestVr::ResizeNativeWindowChild(operations);
        Require(!operations.recorded.widthSpecified && !operations.recorded.heightSpecified &&
            Raw(operations.recorded.seedWidth)==0u && Raw(operations.recorded.seedHeight)==0u,
            "Unspecified dimensions did not retain independent positive-zero seeds");
        Require(Raw(operations.recorded.geometry[0u])==0x80000000u && Raw(operations.recorded.geometry[1u])==0x80000000u,
            "Raw margin copies did not preserve signed zero");
        Require(operations.events==std::vector<std::string>{"parent","ha","va","query","ha","h0","va","v0","configure"},
            "Unaligned resize read/callback order differs from original");
        operations=Operations{};operations.fixture.horizontal=3u;operations.fixture.vertical=3u;
        operations.fixture.parent={101.25f,77.5f};operations.fixture.margins={1.25f,2.5f,3.0f,4.0f};operations.echoSpecified=true;
        QuestVr::ResizeNativeWindowChild(operations);
        Require(operations.recorded.widthSpecified && operations.recorded.heightSpecified &&
            operations.recorded.seedWidth==97.5f && operations.recorded.seedHeight==70.5f,
            "Stretch did not supply dimensions minus both margins");
        Require(operations.recorded.geometry==std::array<float,4u>{1.25f,3.0f,97.5f,70.5f},"Stretch configuration wrong");
        Require(operations.events==std::vector<std::string>{"parent","ha","h1","h0","va","v1","v0","query","ha","h0","va","v0","configure"},
            "Stretch did not read margin1 before margin0 or preserve callback order");
        operations=Operations{};operations.fixture.horizontal=3u;operations.fixture.vertical=3u;
        operations.callback=[](Operations& target) {
            target.fixture.parent={1000.0f,2000.0f};target.fixture.horizontal=1u;target.fixture.vertical=2u;
            target.fixture.margins={5.0f,900.0f,6.0f,800.0f};target.fixture.preferred={11.0f,13.0f};
        };
        QuestVr::ResizeNativeWindowChild(operations);
        Require(operations.recorded.seedWidth==91.0f && operations.recorded.seedHeight==66.0f,
            "Query mutation retroactively changed captured stretch seeds");
        Require(operations.recorded.geometry==std::array<float,4u>{49.0f,61.0f,11.0f,13.0f},
            "Post-query alignment/margin rereads or captured parent dimensions wrong");
        Require(operations.events==std::vector<std::string>{"parent","ha","h1","h0","va","v1","v0","query","ha","h0","va","v0","configure"},
            "Mutating query changed native read order");
        operations=Operations{};operations.callback=[](Operations&) {throw std::runtime_error("Script query refusal");};
        bool callbackRefused{};try {QuestVr::ResizeNativeWindowChild(operations);} catch(const std::runtime_error&) {callbackRefused=true;}
        Require(callbackRefused && operations.configurations==0u && operations.events.back()=="query",
            "Failed query continued to post-query reads or configuration");
        Fixture fixture;fixture.horizontal=1u;fixture.vertical=1u;fixture.parent={101.0f,79.0f};fixture.preferred={10.0f,20.0f};
        auto recorded=Compute(fixture);
        Require(recorded.geometry[0u]==47.0f && recorded.geometry[1u]==32.0f,"Center did not truncate before margin addition");
        fixture.parent={0.0f,0.0f};fixture.preferred={11.0f,13.0f};recorded=Compute(fixture);
        Require(recorded.geometry[0u]==-3.0f && recorded.geometry[1u]==-3.0f,"Negative center did not truncate toward zero");
        fixture=Fixture{};fixture.horizontal=2u;fixture.vertical=2u;recorded=Compute(fixture);
        Require(recorded.geometry[0u]==88.0f && recorded.geometry[1u]==57.0f,"Far edge used margin1 instead of margin0");
        fixture.horizontal=1u;fixture.vertical=0u;fixture.parent.width=16777216.0f;fixture.preferred.width=-3.0f;fixture.margins[0u]=0.0f;
        recorded=Compute(fixture);Require(recorded.geometry[0u]==8388609.0f,"Center difference was rounded to Float32 before truncation");
        fixture.parent.width=4294967296.0f;fixture.preferred.width=2.0f;fixture.margins[0u]=-2147483648.0f;
        recorded=Compute(fixture);Require(recorded.geometry[0u]==-1.0f,"Center integer was rounded to Float32 before adding margin");
        fixture.horizontal=2u;fixture.parent.width=16777216.0f;fixture.preferred.width=-1.0f;fixture.margins[0u]=16777216.0f;
        recorded=Compute(fixture);Require(recorded.geometry[0u]==1.0f,"Far-edge live PC64 difference lost its residual");
        fixture.horizontal=3u;fixture.margins[1u]=16777216.0f;fixture.margins[0u]=1.0f;
        recorded=Compute(fixture);Require(recorded.seedWidth==-1.0f,"Stretch margin sum was stored to Float32 too early");
        fixture=Fixture{};fixture.horizontal=3u;fixture.vertical=3u;fixture.parent={negativeZero,negativeZero};fixture.margins.fill(0.0f);
        recorded=Compute(fixture);Require(Raw(recorded.seedWidth)==0x80000000u && Raw(recorded.seedHeight)==0x80000000u,
            "Stretch subtraction changed negative-zero seeds");
        fixture=Fixture{};fixture.hasParent=false;fixture.horizontal=2u;fixture.vertical=2u;
        recorded=Compute(fixture);Require(recorded.geometry[0u]==-12.0f && recorded.geometry[1u]==-23.0f,"Parentless resize did not capture positive-zero dimensions");
        fixture=Fixture{};fixture.horizontal=0u;fixture.vertical=0u;fixture.parent={std::numeric_limits<float>::quiet_NaN(),maximum};
        fixture.margins[1u]=std::numeric_limits<float>::infinity();recorded=Compute(fixture);
        Require(recorded.geometry[0u]==2.0f,"Unused parent dimensions or margin1 were consumed by unaligned resize");
        using QuestVr::ActorTimerDetail::Dyadic;
        using QuestVr::NativeWindowResizeDetail::TruncateSigned32;
        Require(TruncateSigned32(Dyadic{4294967295u,-1,false})==std::numeric_limits<std::int32_t>::max(),"Positive boundary fraction truncated incorrectly");
        Require(TruncateSigned32(Dyadic{4294967297u,-1,true})==std::numeric_limits<std::int32_t>::min(),"Negative boundary fraction truncated incorrectly");
        Require(TruncateSigned32(Dyadic{1u,-149,true})==0,"Tiny negative center did not truncate to integer zero");
        for(float parent:{4294967296.0f,8589934592.0f,-8589934592.0f}) {
            fixture=Fixture{};fixture.horizontal=1u;fixture.parent.width=parent;fixture.preferred.width=0.0f;Refuses(fixture);
        }
        fixture=Fixture{};fixture.horizontal=1u;fixture.parent.width=-4294967296.0f;fixture.preferred.width=0.0f;fixture.margins[0u]=0.0f;
        recorded=Compute(fixture);Require(recorded.geometry[0u]==-2147483648.0f,"Exact negative signed32 center boundary refused");
        fixture=Fixture{};fixture.horizontal=3u;fixture.parent.width=maximum;fixture.margins[0u]=-maximum;fixture.margins[1u]=-maximum;Refuses(fixture);
        fixture=Fixture{};fixture.horizontal=2u;fixture.parent.width=maximum;fixture.preferred.width=-maximum;fixture.margins[0u]=0.0f;Refuses(fixture);
        for(float bad:{std::numeric_limits<float>::infinity(),-std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}) {
            fixture=Fixture{};fixture.preferred.width=bad;Refuses(fixture);
            fixture=Fixture{};fixture.preferred.height=bad;Refuses(fixture);
            fixture=Fixture{};fixture.margins[0u]=bad;Refuses(fixture);
            fixture=Fixture{};fixture.margins[2u]=bad;Refuses(fixture);
            fixture=Fixture{};fixture.horizontal=3u;fixture.margins[1u]=bad;Refuses(fixture);
            fixture=Fixture{};fixture.vertical=3u;fixture.margins[3u]=bad;Refuses(fixture);
            fixture=Fixture{};fixture.horizontal=1u;fixture.parent.width=bad;Refuses(fixture);
            fixture=Fixture{};fixture.vertical=2u;fixture.parent.height=bad;Refuses(fixture);
        }
        for(unsigned alignment=0u;alignment<256u;++alignment) {
            fixture=Fixture{};fixture.horizontal=static_cast<std::uint8_t>(alignment);fixture.vertical=static_cast<std::uint8_t>(255u-alignment);
            fixture.preferred={-3.25f,9.5f};recorded=Compute(fixture);
            Require(recorded.widthSpecified==(alignment==3u) && recorded.heightSpecified==(255u-alignment==3u),
                "An arbitrary alignment byte incorrectly supplied a preferred dimension");
            if(alignment!=1u && alignment!=2u) Require(Raw(recorded.geometry[0u])==Raw(fixture.margins[0u]),"Arbitrary horizontal byte did not raw-copy margin0");
            if(255u-alignment!=1u && 255u-alignment!=2u) Require(Raw(recorded.geometry[1u])==Raw(fixture.margins[2u]),"Arbitrary vertical byte did not raw-copy margin0");
        }
#if (defined(__i386__) || defined(__x86_64__)) && defined(__GNUC__)
        static_assert(std::numeric_limits<long double>::digits==64,"x87 oracle requires extended significand64");
        RetailPrecisionOracle precision;
        unsigned tuples{};
        const float tiny=std::numeric_limits<float>::denorm_min();
        for(float parent:{negativeZero,0.0f,tiny,-tiny,1.0f,-1.0f,16777216.0f,-16777216.0f,maximum,-maximum})
            for(float preferred:{negativeZero,0.0f,tiny,-tiny,1.0f,-1.0f,maximum,-maximum})
                for(unsigned alignment=0u;alignment<4u;++alignment) {
                    fixture=Fixture{};fixture.parent={parent,-parent};fixture.preferred={preferred,-preferred};
                    fixture.horizontal=static_cast<std::uint8_t>(alignment);fixture.vertical=static_cast<std::uint8_t>(3u-alignment);
                    fixture.margins={tiny,parent,negativeZero,-parent};CheckOracle(fixture);++tuples;
                }
        for(unsigned alignment=0u;alignment<256u;++alignment) {
            fixture=Fixture{};fixture.horizontal=static_cast<std::uint8_t>(alignment);fixture.vertical=static_cast<std::uint8_t>(alignment);
            fixture.parent={-13.75f,25.5f};fixture.preferred={17.5f,-12.25f};CheckOracle(fixture);++tuples;
        }
        std::uint32_t random=0x5eed4eb0u;
        const auto next=[&]() {random^=random<<13u;random^=random>>17u;random^=random<<5u;return random;};
        const auto finite=[&]() {const auto bits=next();return Float((bits&0x80000000u)|((bits&0x7fffffffu)%0x7f800000u));};
        for(unsigned index=0u;index<20000u;++index) {
            fixture=Fixture{};fixture.hasParent=(index&15u)!=0u;fixture.parent={finite(),finite()};fixture.preferred={finite(),finite()};
            for(auto& value:fixture.margins) value=finite();
            fixture.horizontal=static_cast<std::uint8_t>(next()&3u);fixture.vertical=static_cast<std::uint8_t>(next()&3u);
            CheckOracle(fixture);++tuples;
        }
        std::cout << "Independent native child resize PC64 x87 oracle tuples: " << tuples << '\n';
#else
        std::cout << "Independent x87 oracle unavailable on this architecture; deterministic fixtures only\n";
#endif
        std::cout << "Native child resize tests passed: " << checks << " checks\n";return 0;
    } catch(const std::exception& error) {std::cerr << error.what() << '\n';return 1;}
}
