#include "quest_collision_hash_math.h"
#include "quest_actor_line_trace_x87_oracle.h"

#include <bit>
#include <iostream>
#include <limits>

// Detached native arithmetic/table controls only. No original binary execution,
// native hash lifecycle, actor membership, MoveActor or startup acceptance.
namespace {
using Vec=PortableModelVec3;
using Box=QuestVr::OriginalCollisionHashBox;
using Range=QuestVr::OriginalCollisionHashRange;
using Tables=QuestVr::OriginalCollisionHashTables;
std::size_t checks{},refusals{},differential{};
void Require(bool condition,const char* message) {++checks;if(!condition) throw std::runtime_error(message);}
std::uint32_t Bits(float value) {return std::bit_cast<std::uint32_t>(value);}
bool Same(const Vec& a,const Vec& b) {return Bits(a.x)==Bits(b.x)&&Bits(a.y)==Bits(b.y)&&Bits(a.z)==Bits(b.z);}
bool Same(const Box& a,const Box& b) {return Same(a.minimum,b.minimum)&&Same(a.maximum,b.maximum);}
bool Same(const Tables& a,const Tables& b) {return a.x==b.x&&a.y==b.y&&a.z==b.z;}
template<class Operation> void Refuse(Operation operation) {
    bool refused{};try {operation();} catch(const std::exception&) {refused=true;}
    Require(refused,"Unsafe collision hash arithmetic silently succeeded");++refusals;
}
std::uint64_t Fingerprint(const Tables& tables) {
    std::uint64_t hash=14695981039346656037ull;
    for(const auto* axis:{&tables.x,&tables.y,&tables.z}) for(const auto value:*axis)
        for(unsigned byte=0;byte<4u;++byte) {hash^=(value>>(byte*8u))&255u;hash*=1099511628211ull;}
    return hash;
}
Tables IndependentTables(std::uint32_t& seed) {
    // Independent fixed-width CRT recurrence, not ScriptRandom::Draw15. Three
    // swaps per fixed index, X/Y/Z; not Fisher-Yates or std::shuffle.
    Tables result;
    for(std::uint32_t i=0;i<16384u;++i) result.x[i]=result.y[i]=result.z[i]=i;
    std::uint64_t state=seed;
    for(std::size_t i=0;i<16384u;++i) for(auto* axis:{&result.x,&result.y,&result.z}) {
        state=(state*214013ull+2531011ull)&0xffffffffull;
        const auto j=static_cast<std::size_t>((state>>16u)&32767u)%16384u;
        const auto original=(*axis)[i];(*axis)[i]=(*axis)[j];(*axis)[j]=original;
    }
    seed=static_cast<std::uint32_t>(state);return result;
}
void TablesAndCells() {
    struct Golden {std::uint32_t seed,after;std::uint64_t fingerprint;};
    // Independently calculated little-endian FNV64 over all three complete
    // permutation arrays, not merely a few selected entries.
    const std::array<Golden,4u> goldens{{
        {0u,1839644672u,0x612b4ac567317ec5ull},
        {1u,1264435201u,0x824ced30eeab2a09ull},
        {0xffffffffu,2414854143u,0x0e18286741aaed15ull},
        {0x12345678u,3886225016u,0x6f178fb7c7bb7255ull}}};
    for(const auto& golden:goldens) {
        auto actualSeed=golden.seed,expectedSeed=golden.seed;
        const auto actual=QuestVr::InitializeOriginalCollisionHashTables(actualSeed);
        const auto expected=IndependentTables(expectedSeed);
        Require(actualSeed==expectedSeed&&actualSeed==golden.after&&Same(actual,expected),
            "Native permutation draw count, shared seed or X/Y/Z swap order differs");
        Require(Fingerprint(actual)==golden.fingerprint,"Complete native permutation differs from independent fingerprint");
        QuestVr::ValidateOriginalCollisionHashTables(actual);
        for(std::int32_t i=0;i<16384;i+=127) {
            const auto x=i,y=(i*7)%16384,z=(i*13)%16384;
            Require(QuestVr::OriginalCollisionHashBucket(actual,x,y,z)==(actual.x[x]^actual.y[y]^actual.z[z]),
                "Native bucket did not XOR corresponding actual axis permutations");
        }
        for(const auto budget:{std::size_t{0},std::size_t{1},std::size_t{49150},std::size_t{49151},std::size_t{49153}}) {
            auto failedSeed=golden.seed;
            Refuse([&]{QuestVr::InitializeOriginalCollisionHashTables(failedSeed,budget);});
            Require(failedSeed==golden.seed,"Failed permutation initialization consumed authoritative shared seed");
        }
        // A requested second initialization consumes a second set of draws;
        // lifecycle caching is explicitly the owning caller's responsibility.
        const auto again=QuestVr::InitializeOriginalCollisionHashTables(actualSeed);
        const auto expectedAgain=IndependentTables(expectedSeed);
        Require(actualSeed==expectedSeed&&Same(again,expectedAgain),"Pure initialization concealed a per-process magic cache/reseed");
    }
    auto seed=3u;auto malformed=QuestVr::InitializeOriginalCollisionHashTables(seed);
    malformed.x[0]=malformed.x[1];Refuse([&]{QuestVr::ValidateOriginalCollisionHashTables(malformed);});
    malformed.x[0]=16384u;Refuse([&]{QuestVr::OriginalCollisionHashBucket(malformed,0,0,0);});
    // Active bucket reads need only their three selected entries. Full table
    // admission is a distinct initialization/state operation.
    Require(QuestVr::OriginalCollisionHashBucket(malformed,2,2,2)<16384u,"Bucket read an inactive malformed permutation entry");
    for(std::int32_t bad:{-1,16384,std::numeric_limits<std::int32_t>::max()}) {
        Refuse([&]{QuestVr::OriginalCollisionHashEncodedCell(bad,0,0);});
        Refuse([&]{QuestVr::OriginalCollisionHashBucket(malformed,0,bad,0);});
    }
    Require(QuestVr::OriginalCollisionHashEncodedCell(256,1,0)==512u,
        "Overlapping encoded-cell fields were OR-ed instead of added");
    Require(QuestVr::OriginalCollisionHashEncodedCell(256,0,0)==QuestVr::OriginalCollisionHashEncodedCell(0,1,0),
        "Original encoded-cell alias was replaced by unique coordinates");
    Require(QuestVr::OriginalCollisionHashEncodedCell(16383,16383,16383)==1077886719u,
        "Extreme native encoded cell differs from literal shifts/additions");
    Require(QuestVr::CheckedOriginalCollisionHashCellCount({{1,2,3},{1,2,3}},1u)==1u,"Inclusive single cell was empty");
    Require(QuestVr::CheckedOriginalCollisionHashCellCount({{1,2,3},{2,4,6}},24u)==24u,"Inclusive cell product differs");
    Require(QuestVr::CheckedOriginalCollisionHashCellCount({{2,0,0},{1,16383,16383}},0u)==0u,
        "Inverted range was repaired or charged as a nonempty range");
    Refuse([&]{QuestVr::CheckedOriginalCollisionHashCellCount({{1,2,3},{2,4,6}},23u);});
    Refuse([&]{QuestVr::CheckedOriginalCollisionHashCellCount({{0,0,0},{16383,16383,16383}},1'000'000u);});
    Refuse([&]{QuestVr::CheckedOriginalCollisionHashCellCount({{-1,0,0},{0,0,0}},1u);});
    Refuse([&]{QuestVr::CheckedOriginalCollisionHashCellCount({{0,0,0},{0,0,0}},0u);});
}
void NumericControls() {
    using QuestVr::OriginalCollisionHashCell;
    Require(OriginalCollisionHashCell(0)==128&&OriginalCollisionHashCell(-0.0f)==128,"Shifted signed-zero grid differs");
    Require(OriginalCollisionHashCell(128)==128&&OriginalCollisionHashCell(384)==130,
        "FISTP ties were floored/truncated or rounded away instead of nearest-even");
    Require(OriginalCollisionHashCell(-128)==128&&OriginalCollisionHashCell(-384)==126,"Negative-world grid tie differs");
    Require(OriginalCollisionHashCell(-32768)==0&&OriginalCollisionHashCell(-65536)==0&&
        OriginalCollisionHashCell(10'000'000)==16383,"Native post-FISTP integer clamp differs");
    const auto box=QuestVr::OriginalPrimitiveCollisionBounds({10,20,30},2,3);
    Require(Same(box,{{7,17,26},{13,23,34}}),"Primitive registration lost exact +1 pad");
    // Radius+1 is retained16777217 while its Float copy is16777216. X
    // uses the copy, Y retains the original before subtracting Location.
    const auto retained=QuestVr::OriginalPrimitiveCollisionBounds({16777216,16777216,0},16777216,0);
    Require(Bits(retained.minimum.x)==Bits(0)&&Bits(retained.minimum.y)==Bits(-1),
        "Primitive Y radius incorrectly reloaded its Float spill");
    const auto range=QuestVr::OriginalCollisionHashBoxRange({{384,128,0},{128,384,0}});
    Require(range.minimum[0]==130&&range.maximum[0]==128,"Box range normalized reversed coordinates");
    const auto minusBox=QuestVr::CollisionHashMathDetail::EndpointBox({0,0,0},{-0.0f,-0.0f,-0.0f});
    Require(Same(minusBox,{{-0.0f,-0.0f,-0.0f},{-0.0f,-0.0f,-0.0f}}),"FBox equality did not retain signed-zero Start");
    const auto plusBox=QuestVr::CollisionHashMathDetail::EndpointBox({-0.0f,-0.0f,-0.0f},{0,0,0});
    Require(Same(plusBox,{{0,0,0},{0,0,0}}),"FBox equality replaced Start with End's signed zero");
    constexpr float extent=5.0f/1024.0f;
    const auto low=QuestVr::OriginalCollisionHashSweptRange({65408,65408,65408},{65408,65408,65408},{extent,extent,extent});
    Require(low.minimum==std::array<std::int32_t,3u>{383,384,384},"Swept minimum X was Float-spilled like Y/Z");
    const auto high=QuestVr::OriginalCollisionHashSweptRange({65152,65152,65152},{65152,65152,65152},{extent,extent,extent});
    Require(high.maximum==std::array<std::int32_t,3u>{383,382,382},"Swept maximum X was Float-spilled like Y/Z");
    const auto nan=std::numeric_limits<float>::quiet_NaN(),infinity=std::numeric_limits<float>::infinity();
    for(float bad:{nan,infinity,-infinity,std::numeric_limits<float>::max()}) Refuse([&]{OriginalCollisionHashCell(bad);});
    // The actual signed32 minimum is valid then clamps0; adjacent positive
    //2^31 conversion is invalid and must not become high-clamped16383.
    Require(QuestVr::CollisionHashMathDetail::Integer(-2147483648.0f)==std::numeric_limits<std::int32_t>::min(),
        "FISTP valid negative signed32 minimum was refused");
    Refuse([&]{QuestVr::CollisionHashMathDetail::Integer(2147483648.0f);});
    Refuse([&]{QuestVr::OriginalCollisionHashRetainedCell({1u,std::numeric_limits<int>::max(),false});});
    Refuse([&]{QuestVr::OriginalCollisionHashRetainedCell({QuestVr::ActorTimerDetail::Wide{1u}<<65u,0,false});});
    Refuse([&]{QuestVr::OriginalPrimitiveCollisionBounds({},-1,0);});
    Refuse([&]{QuestVr::OriginalPrimitiveCollisionBounds({},0,-1);});
    Refuse([&]{QuestVr::OriginalPrimitiveCollisionBounds({nan,0,0},0,0);});
    Refuse([&]{QuestVr::OriginalCollisionHashSweptRange({}, {},{0,-1,0});});
    Refuse([&]{QuestVr::OriginalCollisionHashBoxRange({{nan,0,0},{0,0,0}});});
}

#if (defined(__i386__) || defined(__x86_64__)) && defined(__GNUC__)
namespace Hardware=QuestVr::ActorLineX87Oracle;
namespace Math=Hardware::Detail;
using Real=Hardware::Real;
std::int32_t HardwareCell(Real coordinate) {
    const float scaled=Math::Store(Math::Mul(Math::Add(coordinate,32768.0f),1.0f/256.0f));
    std::int32_t integer{};
    asm volatile("flds %1; fistpl %0":"=m"(integer):"m"(scaled):"st");
    // The integer-indefinite result is ambiguous with the valid -2^31. Check
    // the stored Float domain independently before accepting the instruction.
    if(scaled>=2147483648.0f||scaled< -2147483648.0f) throw std::runtime_error("Hardware FISTP overflow");
    return integer<0?0:integer>16383?16383:integer;
}
Box HardwareBounds(Vec location,float radius,float height) {
    if(radius<0||height<0) throw std::runtime_error("Hardware negative dimensions");
    const auto retained=Math::Add(Math::Read(radius),1.0f);
    const float stored=Math::Store(retained),storedHeight=Math::Store(Math::Add(Math::Read(height),1.0f));
    const float maxX=Math::Store(Math::Add(stored,Math::Read(location.x))),
        maxY=Math::Store(Math::Add(retained,Math::Read(location.y))),maxZ=Math::Store(Math::Add(storedHeight,Math::Read(location.z)));
    const float minX=Math::Store(Math::Sub(Math::Read(location.x),stored)),
        minY=Math::Store(Math::Sub(Math::Read(location.y),retained)),minZ=Math::Store(Math::Sub(Math::Read(location.z),storedHeight));
    return {{minX,minY,minZ},{maxX,maxY,maxZ}};
}
Range HardwareRange(const Box& box) {
    Range result;
    result.minimum={HardwareCell(Math::Read(box.minimum.x)),HardwareCell(Math::Read(box.minimum.y)),HardwareCell(Math::Read(box.minimum.z))};
    result.maximum={HardwareCell(Math::Read(box.maximum.x)),HardwareCell(Math::Read(box.maximum.y)),HardwareCell(Math::Read(box.maximum.z))};
    return result;
}
Range HardwareSwept(Vec end,Vec start,Vec extent) {
    Box box{start,start};
    // Independently model Core FBox += using its strict tests, not generic
    // fmin/fmax whose equal-zero selection can differ.
    const auto add=[](float endValue,float& low,float& high) {
        Math::Read(endValue);Math::Read(low);Math::Read(high);
        if(low>endValue) low=endValue;
        if(high<endValue) high=endValue;
    };
    add(end.x,box.minimum.x,box.maximum.x);add(end.y,box.minimum.y,box.maximum.y);add(end.z,box.minimum.z,box.maximum.z);
    for(float value:{extent.x,extent.y,extent.z}) if(value<0) throw std::runtime_error("Hardware negative extent");
    const float minZ=Math::Store(Math::Sub(box.minimum.z,Math::Read(extent.z))),
        minY=Math::Store(Math::Sub(box.minimum.y,Math::Read(extent.y)));
    Range result;
    result.minimum={HardwareCell(Math::Sub(box.minimum.x,Math::Read(extent.x))),HardwareCell(minY),HardwareCell(minZ)};
    const float maxZ=Math::Store(Math::Add(box.maximum.z,Math::Read(extent.z))),
        maxY=Math::Store(Math::Add(box.maximum.y,Math::Read(extent.y)));
    result.maximum={HardwareCell(Math::Add(Math::Read(extent.x),box.maximum.x)),HardwareCell(maxY),HardwareCell(maxZ)};
    return result;
}
bool Same(const Range& a,const Range& b) {return a.minimum==b.minimum&&a.maximum==b.maximum;}
std::uint64_t Next(std::uint64_t& value) {value^=value<<13u;value^=value>>7u;value^=value<<17u;return value;}
float Number(std::uint64_t& seed,bool broad) {
    const auto raw=static_cast<std::uint32_t>(Next(seed));
    return broad?std::bit_cast<float>((raw&0x807fffffu)|(((raw>>23u)%255u)<<23u)):
        static_cast<float>(static_cast<int>(raw%16'777'217u)-8'388'608)/16.0f;
}
template<class Actual,class Expected,class Equal> void DifferentialOne(Actual actual,Expected expected,Equal equal) {
    using Value=decltype(actual());Value a{},b{};bool refused{},hardwareRefused{};
    try {a=actual();} catch(const std::exception&) {refused=true;}
    try {b=expected();} catch(const std::exception&) {hardwareRefused=true;}
    Require(refused==hardwareRefused,"Collision hash finite refusal differs from independent hardware x87");
    if(refused) {++refusals;return;}
    Require(equal(a,b),"Collision hash scalar result differs from independent hardware x87");++differential;
}
void Differential() {
    Hardware::Precision precision;std::uint64_t seed=0x6348f0d71be26a95ull;
    for(unsigned index=0;index<18'000u;++index) {
        const bool broad=index<4'000u;const auto number=[&]{return Number(seed,broad);};
        const float point=number();
        DifferentialOne([&]{return QuestVr::OriginalCollisionHashCell(point);},[&]{return HardwareCell(Math::Read(point));},
            [](auto a,auto b){return a==b;});
        const Vec location{number(),number(),number()};const float radius=std::fabs(number()),height=std::fabs(number());
        DifferentialOne([&]{return QuestVr::OriginalPrimitiveCollisionBounds(location,radius,height);},
            [&]{return HardwareBounds(location,radius,height);},[](const Box& a,const Box& b){return Same(a,b);});
        const Vec start{number(),number(),number()},end{number(),number(),number()},extent{std::fabs(number()),std::fabs(number()),std::fabs(number())};
        DifferentialOne([&]{return QuestVr::OriginalCollisionHashSweptRange(end,start,extent);},
            [&]{return HardwareSwept(end,start,extent);},[](const Range& a,const Range& b){return Same(a,b);});
        const Box box{start,end};
        DifferentialOne([&]{return QuestVr::OriginalCollisionHashBoxRange(box);},[&]{return HardwareRange(box);},
            [](const Range& a,const Range& b){return Same(a,b);});
        // Exercise explicit retained-input grid, with no accidental Float
        // subtraction before +32768 and scaling.
        DifferentialOne([&]{return QuestVr::OriginalCollisionHashRetainedCell(QuestVr::CollisionHashMathDetail::Sub(
            QuestVr::CollisionHashMathDetail::Load(start.x),QuestVr::CollisionHashMathDetail::Load(end.x)));},
            [&]{return HardwareCell(Math::Sub(Math::Read(start.x),Math::Read(end.x)));},[](auto a,auto b){return a==b;});
    }
    for(int cell=0;cell<16384;cell+=17) {
        const float half=static_cast<float>(cell)*256.0f+128.0f-32768.0f;
        for(float value:{std::nextafter(half,-std::numeric_limits<float>::infinity()),half,
            std::nextafter(half,std::numeric_limits<float>::infinity())})
            DifferentialOne([&]{return QuestVr::OriginalCollisionHashCell(value);},[&]{return HardwareCell(value);},[](auto a,auto b){return a==b;});
    }
    Require(differential>60'000u,"Independent numeric controls missed ordinary scalar/grid branches");
}
#else
void Differential() {std::cout<<"SKIP hardware x87 collision-hash oracle on non-x86 compiler; deterministic controls still run.\n";}
#endif
} // namespace
int main() {
    try {TablesAndCells();NumericControls();Differential();std::cout<<"PASS original collision hash math "<<checks<<" checks, "
        <<refusals<<" explicit safety refusals, "<<differential<<" independent x87 comparisons.\n";return 0;}
    catch(const std::exception& error) {std::cerr<<"Original collision hash math failed: "<<error.what()<<'\n';return 1;}
}
