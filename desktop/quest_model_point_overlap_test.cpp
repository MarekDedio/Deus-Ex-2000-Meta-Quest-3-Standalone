#include "quest_model_point_overlap.h"
#include "quest_model_point_overlap_x87_oracle.h"

#include <iostream>
#include <limits>

namespace {
using Vec=PortableModelVec3;
using Model=PortableModelGeometry;
using Node=PortableModelNode;
using Hit=QuestVr::OriginalActorLineHit;
using Coords=QuestVr::OriginalModelCoords;
using Stats=QuestVr::ModelPointOverlapStats;
using Limits=QuestVr::ModelPointOverlapLimits;
namespace Oracle=QuestVr::ModelPointOverlapX87Oracle;
using Plane=QuestVr::ModelOwnerTraceOracle::Plane;
std::size_t checks{},refusals{},comparisons{},overlaps{},clears{},candidateClears{};
std::size_t exponentQueries{},retainedFixtures{};
std::array<std::size_t,3u> bevelAxes{};
void Require(bool value,const char* reason) {++checks;if(!value) throw std::runtime_error(reason);}
std::uint32_t Bits(float value) {return std::bit_cast<std::uint32_t>(value);}
bool Same(float a,float b) {return Bits(a)==Bits(b);}
bool Same(Vec a,Vec b) {return Same(a.x,b.x)&&Same(a.y,b.y)&&Same(a.z,b.z);}
bool Same(const Hit& a,const Hit& b) {
    return a.next==b.next&&a.actor==b.actor&&a.primitive==b.primitive&&a.item==b.item&&
        Same(a.time,b.time)&&Same(a.location,b.location)&&Same(a.normal,b.normal);
}
Hit Seed() {return {37u,71u,89u,{101,102,103},{104,105,106},17,73};}
Coords Identity() {return {{},{1,0,0},{0,1,0},{0,0,1}};}
[[maybe_unused]] Oracle::Coords Independent(const Coords& value) {return {value.origin,value.xAxis,value.yAxis,value.zAxis};}
[[maybe_unused]] Oracle::Packet Independent(const Hit& value) {
    return {value.next,value.actor,value.primitive,value.location,value.normal,value.time,value.item};
}
[[maybe_unused]] Hit Packet(const Oracle::Packet& value) {
    return {value.next,value.actor,value.primitive,value.location,value.normal,value.time,value.item};
}
Node MakeNode(Plane plane={{},0}) {
    Node result;result.planeX=plane.normal.x;result.planeY=plane.normal.y;result.planeZ=plane.normal.z;result.planeW=plane.w;
    result.front=-1;result.back=-1;result.collisionBound=-1;return result;
}
void AppendHull(Model& model,std::int32_t owner,const std::vector<std::int32_t>& references,
    Vec minimum={-100,-100,-100},Vec maximum={100,100,100}) {
    model.nodes.at(static_cast<std::size_t>(owner)).collisionBound=static_cast<std::int32_t>(model.leafHulls.size());
    model.leafHulls.insert(model.leafHulls.end(),references.begin(),references.end());model.leafHulls.push_back(-1);
    for(float value:{minimum.x,minimum.y,minimum.z,maximum.x,maximum.y,maximum.z})
        model.leafHulls.push_back(std::bit_cast<std::int32_t>(value));
}
Model Hull(const std::vector<Plane>& planes,Vec minimum={-100,-100,-100},Vec maximum={100,100,100}) {
    Model model;model.rootOutside=0;model.nodes.push_back(MakeNode());std::vector<std::int32_t> references;
    for(const auto& plane:planes) {references.push_back(static_cast<std::int32_t>(model.nodes.size()));model.nodes.push_back(MakeNode(plane));}
    AppendHull(model,0,references,minimum,maximum);return model;
}
bool Run(Hit& hit,const Model& model,Vec point,Vec extent={},std::uintptr_t owner=0u,
    const Coords& coords=Identity(),std::uint32_t flags=0u,Stats* stats=nullptr,const Limits& limits={}) {
    return QuestVr::CheckOriginalModelPointOverlap(hit,model,19u,owner,point,extent,flags,[&]{return coords;},limits,stats);
}
template<class F> void Refuse(F run) {
    bool failed{};try {run();}catch(const std::exception&) {failed=true;}
    Require(failed,"Unsupported Model occupancy input silently succeeded");++refusals;
}
void CheckEntry(const Hit& hit,const Hit& seed,Vec location,std::uintptr_t owner,std::int32_t item) {
    Require(hit.next==seed.next&&hit.actor==owner&&hit.primitive==19u&&Same(hit.time,0.0f)&&
        Same(hit.location,location)&&Same(hit.normal,Vec{})&&hit.item==item,
        "Model occupancy entry stores or untouched Next/Item differ");
}

void EmptyAndZero() {
    const float nan=std::numeric_limits<float>::quiet_NaN(),inf=std::numeric_limits<float>::infinity();
    const auto seed=Seed();
    for(const int outside:{0,1}) {
        Model empty;empty.rootOutside=outside;auto hit=seed;unsigned captures{};
        const Vec point{nan,inf,-inf};
        const bool clear=QuestVr::CheckOriginalModelPointOverlap(hit,empty,19u,23u,point,{nan,inf,-inf},~0u,
            [&]()->Coords{++captures;throw std::runtime_error("Inactive empty capture");});
        Require(clear==(outside!=0)&&captures==0u,"Empty Model inspected extent/owner coordinates or lost RootOutside");
        CheckEntry(hit,seed,point,23u,seed.item);
    }
    Model half;half.rootOutside=1;half.nodes.push_back(MakeNode({{1,0,0},0}));half.nodes[0].vertexCount=1u;
    for(const float x:{-1.0f,-0.0f,0.0f,std::numeric_limits<float>::denorm_min(),1.0f}) {
        auto hit=seed;const bool clear=Run(hit,half,{x,0,0});
        Require(clear==(x>0.0f),"Zero-extent halfspace equality must select back and exact positive select front");
        CheckEntry(hit,seed,{x,0,0},0u,x>0.0f?1:0);
    }
    half.nodes[0].collisionBound=std::numeric_limits<std::int32_t>::max();
    half.leafHulls={std::bit_cast<std::int32_t>(nan)};
    auto hit=seed;unsigned captures{};
    Require(!QuestVr::CheckOriginalModelPointOverlap(hit,half,19u,0u,{},
        {nan,-0.0f,nan},~0u,[&]{++captures;return Identity();})&&captures==0u,
        "Zero-or-NaN extent inspected inactive malformed hull/owner or entered box path");
    CheckEntry(hit,seed,{},0u,0);
    // Every caller flag byte is irrelevant in containment. Every node flag
    // byte is checked against fixed0x21, not the supplied flags mask.
    for(unsigned nodeFlags=0;nodeFlags<256u;++nodeFlags) for(unsigned flags=0;flags<256u;++flags) {
        half.nodes[0].nodeFlags=static_cast<std::uint8_t>(nodeFlags);hit=seed;
        const bool csg=(nodeFlags&0x21u)==0u;
        Require(Run(hit,half,{-1,0,0},{},0u,Identity(),flags)==!csg,
            "Zero occupancy used caller flags or wrong CSG vertex/flag predicate");
    }
    half.nodes[0].vertexCount=0;hit=seed;
    Require(Run(hit,half,{-1,0,0}),"Zero-vertex node unexpectedly changed outside state");
    half.nodes[0].vertexCount=1;half.nodes[0].nodeFlags=0;
    const Coords translated{{3,-4,5},{1,0,0},{0,1,0},{0,0,1}};
    hit=seed;captures=0;
    const bool clear=QuestVr::CheckOriginalModelPointOverlap(hit,half,19u,23u,{-3.25f,0,0},{},0u,
        [&]{++captures;return translated;});
    Require(!clear&&captures==1u,"Nonnull containment did not capture actual owner coordinates once");
    CheckEntry(hit,seed,{-3.25f,0,0},23u,0);
    // A positive retained product below half the smallest Float must select
    // front. Rounding a query distance to Float before comparison selects back.
    const float tiny=std::numeric_limits<float>::denorm_min();
    Model underflow;underflow.rootOutside=1;underflow.nodes.push_back(MakeNode({{tiny,0,0},0}));
    underflow.nodes[0].vertexCount=1u;hit=seed;
    Require(Run(hit,underflow,{.25f,0,0})&&hit.item==1,
        "Containment rounded a retained positive sub-Float distance to zero");
    ++retainedFixtures;
    // Product is just above1 in PC64 but would round to Float1 before W is
    // subtracted. The original retains that product through the comparison.
    Model cancellation;cancellation.rootOutside=1;
    cancellation.nodes.push_back(MakeNode({{std::nextafter(1.0f,2.0f),0,0},1.0f}));
    cancellation.nodes[0].vertexCount=1u;hit=seed;
    Require(Run(hit,cancellation,{std::nextafter(1.0f,0.0f),0,0})&&hit.item==1,
        "Containment spilled the normal-product before retained W cancellation");
    ++retainedFixtures;
    // The dead FST Float of OriginDot overflows, but the retained PC64 value
    // cancels with QueryDot. No active output/captured Float overflows here.
    const float large=std::ldexp(1.0f,100);
    cancellation.nodes[0]=MakeNode({{large,0,0},0});cancellation.nodes[0].vertexCount=1u;
    Coords largeOrigin=Identity();largeOrigin.origin={large,0,0};hit=seed;
    Require(!Run(hit,cancellation,{-large,0,0},{},23u,largeOrigin)&&hit.item==0,
        "Containment treated unused Float dot scratch overflow as an active result");
    ++retainedFixtures;
}

void BoxControls() {
    const auto seed=Seed();auto hit=seed;
    const Model half=Hull({{{1,0,0},0}});
    Require(!Run(hit,half,{.5f,0,0},{1,1,1},23u)&&hit.item==1&&Same(hit.normal,Vec{1,0,0})&&
        hit.location.x>1.0f&&Same(hit.time,0.0f)&&hit.next==seed.next,
        "Primary occupancy candidate lacks source node Item/pushout/packet ownership");
    const Model separating=Hull({{{1,0,0},0},{{0,1,0},-4}});
    hit=seed;Require(Run(hit,separating,{.5f,0,0},{1,1,1},23u)&&hit.item==1&&Same(hit.normal,Vec{1,0,0})&&
        hit.location.x>1.0f,"Clear return discarded earlier native candidate packet stores");
    const Model noCandidate=Hull({{{1,0,0},10}});
    hit=seed;Require(!Run(hit,noCandidate,{}, {1,1,1},23u),"Solid hull without positive pushout incorrectly cleared");
    CheckEntry(hit,seed,{},23u,seed.item);
    Model noBound;noBound.rootOutside=0;noBound.nodes.push_back(MakeNode());
    hit=seed;Require(Run(hit,noBound,{}, {1,1,1},23u),"Missing collisionBound must return clear, not fabricated overlap");
    CheckEntry(hit,seed,{},23u,seed.item);
    Model flipped=Hull({{{-1,0,0},0}});flipped.leafHulls[0]|=0x40000000;
    hit=seed;Require(!Run(hit,flipped,{.5f,0,0},{1,1,1},23u)&&hit.item==1&&Same(hit.normal,Vec{1,-0.0f,-0.0f}),
        "Encoded hull flip lost its source node Item or signed normal");
    const Model bounds=Hull({}, {-1,-1,-1},{1,1,1});
    auto highX=seed,highY=seed,highZ=seed,lowX=seed;
    Require(Run(highX,bounds,{1.95f,0,0},{1,1,1})&&Run(highY,bounds,{0,1.95f,0},{1,1,1})&&
        !Run(highZ,bounds,{0,0,1.95f},{1,1,1})&&!Run(lowX,bounds,{-1.95f,0,0},{1,1,1}),
        "Null-owner occupancy lost asymmetric high-X/Y shave versus high-Z/low expansion");
    Require(highZ.item==-1&&Same(highZ.normal,Vec{0,0,1}),"Null bound candidate must carry Item-1 and native axis normal");
    const float nan=std::numeric_limits<float>::quiet_NaN();
    const Model inactiveBounds=Hull({{{1,0,0},0}},{nan,nan,nan},{nan,nan,nan});
    hit=seed;Require(!Run(hit,inactiveBounds,{.5f,0,0},{1,1,1},23u),"Owner numerically inspected inactive packed bounds");
    hit=seed;Refuse([&]{Run(hit,inactiveBounds,{.5f,0,0},{1,1,1});});
    Require(Same(hit,seed),"Active null-owner bounds refusal leaked entry/candidate writes");
    // A false front result does not suppress the back call, whose closer
    // candidate shares the same bestDistance and final caller packet.
    Model branches;branches.rootOutside=0;branches.nodes={MakeNode({{1,0,0},0}),MakeNode(),MakeNode(),
        MakeNode({{1,0,0},0}),MakeNode({{0,1,0},0})};
    branches.nodes[0].front=1;branches.nodes[0].back=2;
    AppendHull(branches,1,{3});AppendHull(branches,2,{4});hit=seed;
    Require(!Run(hit,branches,{.75f,.25f,0},{1,1,1},23u)&&hit.item==4&&Same(hit.normal,Vec{0,1,0}),
        "False front leaf short-circuited back candidate or reset shared best distance");
    // Finite signed dimensions are input to absolute projected radii, not
    // clamped or refused. This Model rule differs from the raw cylinder policy.
    auto positive=seed,negative=seed;
    const auto a=Run(positive,half,{.5f,0,0},{1,2,3},23u),b=Run(negative,half,{.5f,0,0},{-1,-2,-3},23u);
    Require(a==b&&Same(positive,negative),"Finite negative Model extents were refused or changed absolute projection");
    // Exactly64 real hull slots are admissible; pair visitation still reaches
    // all i/j pairs and XYZ gates. Parallel normals produce no bevel planes.
    const Model sixtyFour=Hull(std::vector<Plane>(64u,Plane{{1,0,0},10.0f}));
    hit=seed;Stats fullStats;
    Require(!Run(hit,sixtyFour,{}, {1,1,1},23u,Identity(),0u,&fullStats)&&
        fullStats.hullVisits==1u&&fullStats.planeTests==64u&&fullStats.bevelPairs==2016u&&fullStats.bevelTests==0u,
        "Exact64-plane occupancy hull was refused, truncated, or skipped pair traversal");
    CheckEntry(hit,seed,{},23u,seed.item);
    // Nonzero-box CSG DOES use caller flags. This real one-node source has
    // a bounded solid back leaf when CSG is enabled and two clear leaves when
    // disabled; unlike the zero case, all256 caller bytes must affect masking.
    Model masked;masked.rootOutside=1;masked.nodes.push_back(MakeNode({{1,0,0},0}));
    masked.nodes[0].vertexCount=1u;AppendHull(masked,0,{0});
    for(unsigned nodeFlags=0u;nodeFlags<256u;++nodeFlags)for(unsigned flags=0u;flags<256u;++flags) {
        masked.nodes[0].nodeFlags=static_cast<std::uint8_t>(nodeFlags);hit=seed;Stats census;
        const bool csg=(nodeFlags&static_cast<std::uint8_t>(flags|0x21u))==0u;
        Require(Run(hit,masked,{}, {1,1,1},23u,Identity(),0xcafe0000u|flags,&census)==!csg&&
            census.nodeVisits==1u&&census.leafVisits==2u&&census.hullVisits==(csg?1u:0u)&&
            census.planeTests==(csg?1u:0u)&&Same(hit.normal,Vec{})&&hit.item==seed.item,
            "Box occupancy ignored caller CSG flags, upper-byte narrowing, or original solid-leaf census");
    }
    // Positive retained distance rounds to stored Float0, yet the original's
    // first positivity test still allows a real candidate with bestDistance0.
    const float tiny=std::numeric_limits<float>::denorm_min();
    const Model roundedPositive=Hull({{{tiny,0,0},0}});hit=seed;
    Require(!Run(hit,roundedPositive,{.25f,0,0},{1,1,1},23u)&&hit.item==1&&
        Same(hit.normal,Vec{tiny,0,0})&&Same(hit.location,Vec{.25f,0,0}),
        "Primary hull lost a positive retained distance rounded to candidate Float0");
    ++retainedFixtures;
}

#if (defined(__i386__) || defined(__x86_64__)) && defined(__GNUC__)
void DifferentialOne(const Model& model,Vec point,Vec extent={},std::uintptr_t owner=0u,
    const Coords& coords=Identity(),std::uint32_t flags=0u) {
    const auto seed=Seed();auto actual=seed;Oracle::Result expected;
    bool failed{},oracleFailed{},a{};Stats stats;
    try {a=Run(actual,model,point,extent,owner,coords,flags,&stats);}catch(const std::exception&) {failed=true;}
    try {expected=Oracle::Check(Independent(seed),model,19u,owner,point,extent,flags,Independent(coords));}
    catch(const std::exception&) {oracleFailed=true;}
    Require(failed==oracleFailed,"Model occupancy safety refusal differs from independent x87");
    if(failed) {Require(Same(actual,seed),"Refused occupancy retained entry or candidate writes");++refusals;return;}
    Require(a==expected.clear,"Model occupancy result differs from independent x87");
    if(!Same(actual,Packet(expected.packet))) throw std::runtime_error("Model occupancy packet differs from independent x87: item "+
        std::to_string(actual.item)+"/"+std::to_string(expected.packet.item)+" loc "+std::to_string(Bits(actual.location.x))+","+
        std::to_string(Bits(actual.location.y))+","+std::to_string(Bits(actual.location.z))+" / "+
        std::to_string(Bits(expected.packet.location.x))+","+std::to_string(Bits(expected.packet.location.y))+","+
        std::to_string(Bits(expected.packet.location.z)));
    ++checks;++comparisons;
    Require(stats.nodeVisits==expected.nodes&&stats.leafVisits==expected.leaves&&stats.hullVisits==expected.hulls&&
        stats.planeTests==expected.planes&&stats.bevelPairs==expected.pairs&&stats.bevelTests==expected.bevels,
        "Model occupancy traversal/plane/bevel census differs from independent recursive oracle");
    if(a) {++clears;if(expected.candidates) ++candidateClears;}else ++overlaps;
    for(unsigned axis=0u;axis<3u;++axis)bevelAxes[axis]+=expected.bevelAxes[axis];
}
std::uint64_t Next(std::uint64_t& state) {state^=state<<13u;state^=state>>7u;state^=state<<17u;return state;}
float Number(std::uint64_t& state) {return static_cast<float>(static_cast<int>(Next(state)%8193u)-4096)/1024.0f;}
void Differential() {
    Oracle::Precision precision;std::uint64_t state=0xad5935e8b7334261ull;
    const std::array<Coords,5u> coords{{Identity(),{{3,-4,5},{0,1,0},{-1,0,0},{0,0,1}},
        {{-.125f,.25f,-.5f},{2,.125f,0},{0,.5f,-.25f},{.125f,0,-1}},
        {{},{0,0,0},{0,1,0},{0,0,1}},{{1,2,3},{1,1,0},{0,1,1},{1,0,1}}}};
    for(unsigned i=0;i<12'000u;++i) {
        std::vector<Plane> planes;
        const unsigned count=1u+static_cast<unsigned>(Next(state)%7u);
        for(unsigned j=0u;j<count;++j)planes.push_back({{Number(state),Number(state),Number(state)},Number(state)});
        auto model=Hull(planes,{-20,-20,-20},{20,20,20});
        for(auto& raw:model.leafHulls) {if(raw==-1)break;if((Next(state)&7u)==0u)raw|=0x40000000;}
        const Vec point{Number(state),Number(state),Number(state)},extent{Number(state),Number(state),Number(state)};
        const auto& selected=coords[i%coords.size()];
        DifferentialOne(model,point,extent,i%2u?23u:0u,selected,static_cast<std::uint32_t>(Next(state)));
        if(i%3u==0u) DifferentialOne(model,point,{0,std::numeric_limits<float>::quiet_NaN(),-0.0f},
            i%2u?23u:0u,selected,~0u);
    }
    const std::array<Vec,3u> first{{{1,1,0},{0,1,1},{1,0,1}}},second{{{-1,1,0},{0,-1,1},{1,0,-1}}};
    for(unsigned axis=0u;axis<3u;++axis) {
        const auto model=Hull({{first[axis],0},{second[axis],0}});
        for(float x:{-.5f,-0.0f,0.0f,.25f,.5f}) for(float y:{-.5f,0.0f,.25f,.5f})
            for(float z:{-.5f,0.0f,.25f,.5f}) for(const auto owner:{0u,23u})
                DifferentialOne(model,{x,y,z},{1,1,1},owner);
    }
    for(float edge:{.001f,std::nextafter(.001f,0.0f),std::nextafter(.001f,1.0f),.00001f}) {
        DifferentialOne(Hull({{{1,1,0},0},{{-1,edge,0},0}}),{.25f,.25f,0},{1,1,1},23u);
        DifferentialOne(Hull({{{edge,1,0},0},{{-edge,1,0},0}}),{}, {1,1,1},23u);
    }
    // Boundary-centered queries exercise the positive-distance retained test,
    // Float candidate comparisons and literal .1/1.02 projection spill edges.
    for(float edge:{-1.0f,-0.0f,0.0f,std::numeric_limits<float>::denorm_min(),.1f,.5f,
        std::nextafter(1.0f,0.0f),1.0f,std::nextafter(1.0f,2.0f),1.9f,1.95f,2.1f}) {
        DifferentialOne(Hull({{{1,0,0},0}}),{edge,0,0},{1,1,1},23u);
        DifferentialOne(Hull({}, {-1,-1,-1},{1,1,1}),{edge,0,0},{1,1,1});
        DifferentialOne(Hull({}, {-1,-1,-1},{1,1,1}),{0,0,edge},{1,1,1});
    }
    // Independent recursive oracle versus iterative production across BOTH
    // branches, shared best-distance, CSG masks and a later clear leaf after
    // earlier candidate writes. The random hull cohort above has one leaf.
    Model branched;branched.rootOutside=0;
    branched.nodes={MakeNode({{1,0,0},0}),MakeNode(),MakeNode(),
        MakeNode({{1,1,0},0}),MakeNode({{-1,1,0},0}),
        MakeNode({{0,1,1},0}),MakeNode({{0,-1,1},-.25f})};
    AppendHull(branched,1,{3,4});AppendHull(branched,2,{5,6});
    for(const auto vertexCount:{0u,1u})for(const auto nodeFlags:{0u,2u,0x21u}) {
        branched.nodes[0].vertexCount=vertexCount;
        branched.nodes[0].nodeFlags=static_cast<std::uint8_t>(nodeFlags);
        for(const auto flags:{0u,2u,0xffffffffu})for(float x:{-.75f,0.0f,.75f})
            for(float y:{-.25f,.25f})for(const auto owner:{0u,23u})
                DifferentialOne(branched,{x,y,.125f},{1,1,1},owner,coords[2],flags);
    }
    // Independent packets over finite normal/subnormal exponent ranges,
    // including explicitly refused active Float-store overflow. These source
    // hulls use real finite words; no production arithmetic builds the oracle.
    const auto wide=[&] {
        auto bits=static_cast<std::uint32_t>(Next(state));
        if((bits&0x7f800000u)==0x7f800000u)bits^=0x00800000u;
        return std::bit_cast<float>(bits);
    };
    for(unsigned i=0u;i<4096u;++i) {
        const auto model=Hull({{{wide(),wide(),wide()},wide()}});
        const Vec point{wide(),wide(),wide()},extent{wide(),wide(),wide()};
        DifferentialOne(model,point,extent,i&1u?23u:0u,Identity(),static_cast<std::uint32_t>(Next(state)));
        ++exponentQueries;
    }
    const float tiny=std::numeric_limits<float>::denorm_min(),large=std::ldexp(1.0f,100);
    DifferentialOne(Hull({{{tiny,0,0},0}}),{.25f,0,0},{1,1,1},23u);
    Model small;small.rootOutside=1;small.nodes.push_back(MakeNode({{tiny,0,0},0}));small.nodes[0].vertexCount=1u;
    DifferentialOne(small,{.25f,0,0});
    small.nodes[0]=MakeNode({{std::nextafter(1.0f,2.0f),0,0},1.0f});small.nodes[0].vertexCount=1u;
    DifferentialOne(small,{std::nextafter(1.0f,0.0f),0,0});
    small.nodes[0]=MakeNode({{large,0,0},0});small.nodes[0].vertexCount=1u;
    Coords largeOrigin=Identity();largeOrigin.origin={large,0,0};
    DifferentialOne(small,{-large,0,0},{},23u,largeOrigin);
    Require(comparisons>10'000u&&overlaps>100u&&clears>1'000u&&candidateClears>100u&&
        bevelAxes[0]>100u&&bevelAxes[1]>100u&&bevelAxes[2]>100u,
        "Independent occupancy cohort missed clear/candidate/overlap or XYZ bevel branches");
}
#else
void Differential() {std::cout<<"SKIP independent x87 occupancy oracle on non-x86 compiler; deterministic controls still run.\n";}
#endif

void RefusalsAndBounds() {
    const auto seed=Seed();const Model good=Hull({{{1,0,0},0}});
    const auto reject=[&](const Model& model,Vec point={},Vec extent={1,1,1},const Limits& limits=Limits{}) {
        auto hit=seed;Stats stats;stats.nodeVisits=73u;
        Refuse([&]{Run(hit,model,point,extent,23u,Identity(),0,&stats,limits);});
        Require(Same(hit,seed)&&stats.nodeVisits==73u,"Occupancy refusal changed packet/statistics atomically");
    };
    auto invalid=good;invalid.version=69;reject(invalid);
    invalid=good;invalid.rootOutside=2;reject(invalid);
    invalid=good;invalid.nodes[0].front=static_cast<std::int32_t>(invalid.nodes.size());reject(invalid);
    invalid=good;invalid.nodes[1].front=1;reject(invalid);
    invalid=good;invalid.nodes[0].collisionBound=-2;reject(invalid);
    invalid=good;invalid.leafHulls.pop_back();reject(invalid);
    invalid=good;invalid.leafHulls[0]=static_cast<std::int32_t>(invalid.nodes.size());reject(invalid);
    invalid=good;invalid.leafHulls.assign(65u,1);invalid.leafHulls.push_back(-1);
    for(unsigned i=0u;i<6u;++i)invalid.leafHulls.push_back(0);
    reject(invalid);
    const float nan=std::numeric_limits<float>::quiet_NaN(),inf=std::numeric_limits<float>::infinity();
    invalid=good;invalid.nodes[1].planeX=nan;reject(invalid);
    reject(good,{nan,0,0});reject(good,{}, {nan,1,0});reject(good,{}, {1,0,inf});
    reject(Hull({{{.00001f,1,0},0},{{-.00001f,1,0},0}}));
    for(unsigned field=0u;field<5u;++field) {
        Limits limits;
        if(field==0u)limits.nodes=0u;else if(field==1u)limits.hullWords=0u;
        else if(field==2u)limits.validationSteps=0u;else if(field==3u)limits.querySteps=0u;else limits.pending=0u;
        reject(good,{}, {1,1,1},limits);
    }
    Limits limits;limits.querySteps=1u;reject(good,{}, {1,1,1},limits);
    limits=Limits{};limits.validationSteps=1u;reject(good,{}, {1,1,1},limits);
    auto hit=seed;unsigned captures{};
    Refuse([&]{QuestVr::CheckOriginalModelPointOverlap(hit,good,19u,23u,{}, {1,1,1},0u,
        [&]()->Coords{++captures;throw std::runtime_error("Actual capture failed");});});
    Require(Same(hit,seed)&&captures==1u,"Owner capture failure did not restore staged entry packet");
    // Production containment must be iterative: source graph depth is not
    // silently reduced to the independent recursive box oracle's512 guard.
    Model deep;deep.rootOutside=1;deep.nodes.resize(8192u);
    for(std::size_t i=0u;i<deep.nodes.size();++i) {
        deep.nodes[i]=MakeNode({{1,0,0},0});deep.nodes[i].vertexCount=1u;
        if(i+1u<deep.nodes.size())deep.nodes[i].front=static_cast<std::int32_t>(i+1u);
    }
    hit=seed;Stats stats;
    Require(Run(hit,deep,{1,0,0},{},0u,Identity(),0,&stats)&&hit.item==16383&&stats.nodeVisits==8192u,
        "Deep iterative containment lost final node/side or native visit count");
    // A box at each nonzero-normal plane visits both children; following its front
    // chain must not recurse on the C++ stack or reset pending ownership.
    Model deepBox;deepBox.rootOutside=1;deepBox.nodes.resize(8192u);
    for(std::size_t i=0u;i<deepBox.nodes.size();++i) {
        deepBox.nodes[i]=MakeNode({{1,0,0},0});
        if(i+1u<deepBox.nodes.size())deepBox.nodes[i].front=static_cast<std::int32_t>(i+1u);
    }
    hit=seed;stats={};
    Require(Run(hit,deepBox,{}, {1,1,1},23u,Identity(),0u,&stats)&&
        stats.nodeVisits==8192u&&stats.leafVisits==8193u&&stats.hullVisits==0u,
        "Deep iterative box traversal skipped pending back leaves or used a native recursion limit");
    CheckEntry(hit,seed,{},23u,seed.item);
    // Admit the identical source first, then restrict pending to one. Failure
    // must leave BOTH packet and detached statistics completely untouched.
    limits=Limits{};limits.pending=1u;hit=seed;stats={};stats.nodeVisits=73u;
    Refuse([&]{Run(hit,deepBox,{}, {1,1,1},23u,Identity(),0u,&stats,limits);});
    Require(Same(hit,seed)&&stats.nodeVisits==73u&&stats.leafVisits==0u,
        "Tight box pending refusal published entry stores or partial query statistics");
}
} // namespace

int main() {
    try {EmptyAndZero();BoxControls();Differential();RefusalsAndBounds();
        std::cout<<"PASS original Model point occupancy "<<checks<<" checks, "<<refusals<<" explicit refusals, "
            <<comparisons<<" independent x87 packets ("<<overlaps<<" overlap/"<<clears<<" clear/"<<candidateClears
            <<" candidate-mutating clear), bevel XYZ "<<bevelAxes[0]<<'/'<<bevelAxes[1]<<'/'<<bevelAxes[2]
            <<", "<<exponentQueries<<" finite-exponent queries and "<<retainedFixtures<<" deliberate retained fixtures.\n";return 0;}
    catch(const std::exception& error) {std::cerr<<"Original Model point occupancy failed: "<<error.what()<<'\n';return 1;}
}
