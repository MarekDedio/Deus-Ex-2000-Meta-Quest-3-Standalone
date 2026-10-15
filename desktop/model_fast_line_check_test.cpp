#include "quest_model_fast_line_check.h"

#include <functional>
#include <iostream>
#include <limits>

namespace {
using Vec=PortableModelVec3;
using Model=PortableModelGeometry;
using Node=PortableModelNode;
namespace Math=QuestVr::ModelFastLineDetail;
std::size_t checks{},refusals{};
void Require(bool value,const char* reason) {++checks;if(!value) throw std::runtime_error(reason);}
template<class Operation> void Refuses(Operation operation) {
    bool refused{};try {operation();} catch(const std::runtime_error&) {refused=true;}
    Require(refused,"Unsafe BSP query did not refuse");++refusals;
}
Node Plane(float nx=1.0f,float ny=0.0f,float nz=0.0f,float w=0.0f,std::uint8_t flags=0u) {
    Node node;node.planeX=nx;node.planeY=ny;node.planeZ=nz;node.planeW=w;node.nodeFlags=flags;
    node.back=node.front=node.plane=-1;node.vertexCount=0u;return node;
}
Model One(Node node=Plane(),bool outside=true) {Model model;model.rootOutside=outside;model.nodes.push_back(node);return model;}
auto Check(const Model& model,Vec end,Vec start,const QuestVr::ModelFastLineCheckLimits& limits={}) {
    return QuestVr::CheckOriginalModelFastLine(model,end,start,limits);
}
[[maybe_unused]] std::uint32_t Next(std::uint32_t& state) {state^=state<<13u;state^=state>>17u;state^=state<<5u;return state;}
[[maybe_unused]] float Number(std::uint32_t& state) {
    const auto bits=Next(state);
    // Broad finite exponents, exact Float inputs, no NaNs/infinities.
    return std::bit_cast<float>((bits&0x807fffffu)|((47u+((bits>>23u)%160u))<<23u));
}
void HandComputed() {
    Model empty;
    for(int outside:{0,1}) {
        empty.rootOutside=outside;
        const auto result=Check(empty,{},{});
        Require(result.clear==static_cast<bool>(outside)&&result.stats.nodeVisits==0u,"Empty Model did not return RootOutside");
    }
    for(unsigned flags=0u;flags<256u;++flags) for(unsigned vertices:{0u,3u}) for(bool outside:{false,true}) {
        auto node=Plane(1,0,0,0,static_cast<std::uint8_t>(flags));node.vertexCount=static_cast<std::uint8_t>(vertices);
        const auto model=One(node,outside);
        Require(Check(model,{4,1,2},{2,1,2}).clear==((flags&1u)?outside:true),"Front/NotCsg/vertex-count gate mismatch");
        Require(Check(model,{-4,1,2},{-2,1,2}).clear==((flags&1u)?outside:false),"Back/NotCsg/vertex-count gate mismatch");
        Require(Check(model,{-4,1,2},{2,1,2}).clear==((flags&1u)?outside:false),"Crossing segment solidity mismatch");
    }
    Require(Check(One(),{0,0,0},{0,0,0}).clear,"Positive-zero coplanar query did not use front leaf");
    Require(!Check(One(),{-0.0001f,1,2},{-0.0002f,1,2}).clear,"FastLineCheck acquired an epsilon from Model.LineCheck");
    const auto zeros=One(Plane(1,1,1,0));
    const Vec minus{-0.0f,-0.0f,-0.0f};
    Require(!Check(zeros,minus,minus).clear,"Signed negative zero was classified by numeric comparison");
    Require(Check(zeros,{},{}).clear,"Signed positive zero did not classify front");
    Refuses([&] {Check(zeros,{},minus);}); // Retail undefined0/0 split is not fabricated.
    auto split=One();split.nodes[0].front=1;split.nodes.push_back(Plane(0,1,0,0,1));
    const auto endBack=Check(split,{-2,1,0},{2,1,0});
    Require(!endBack.clear&&endBack.stats.nodeVisits==1u,"End-side blocked leaf did not prune Start-side child");
    const auto endFront=Check(split,{2,1,0},{-2,1,0});
    Require(!endFront.clear&&endFront.stats.nodeVisits==2u,"End-side child was not visited before blocked Start-side leaf");
    auto deep=One();deep.nodes.assign(2000u,Plane(1,0,0,0,1));
    for(std::size_t index=0u;index+1u<deep.nodes.size();++index) deep.nodes[index].front=static_cast<std::int32_t>(index+1u);
    Require(Check(deep,{1,1,1},{2,1,1}).stats.nodeVisits==2000u,"Deep iterative BSP traversal lost a node");
}
void Safety() {
    auto model=One();model.nodes[0].front=0;Refuses([&] {Check(model,{1,0,0},{2,0,0});});
    model=One();model.nodes[0].back=-2;Refuses([&] {Check(model,{1,0,0},{2,0,0});});
    model=One();model.nodes[0].planeW=std::numeric_limits<float>::infinity();Refuses([&] {Check(model,{},{});});
    model=One();Refuses([&] {Check(model,{std::numeric_limits<float>::quiet_NaN(),0,0},{});});
    auto limits=QuestVr::ModelFastLineCheckLimits{};limits.querySteps=1u;Refuses([&] {Check(model,{1,0,0},{2,0,0},limits);});
    limits={};limits.validationSteps=1u;Refuses([&] {Check(model,{1,0,0},{2,0,0},limits);});
    model.version=69;Refuses([&] {Check(model,{},{});});
    model=One();model.rootOutside=2;Refuses([&] {Check(model,{},{});});
    Refuses([&] {Math::Divide64(Math::Decode(1),Math::Decode(0));});
}

#if (defined(__i386__) || defined(__x86_64__)) && defined(__GNUC__)
struct Precision {
    unsigned short original{};
    Precision() {
        static_assert(std::numeric_limits<long double>::digits==64);
        asm volatile("fnstcw %0":"=m"(original));
        const unsigned short mode=static_cast<unsigned short>((original&~0x0f00u)|0x0300u);
        asm volatile("fldcw %0"::"m"(mode));
    }
    ~Precision() {asm volatile("fldcw %0"::"m"(original));}
};
long double Add(long double a,long double b) {volatile long double x=a,y=b,z=x+y;return z;}
long double Sub(long double a,long double b) {volatile long double x=a,y=b,z=x-y;return z;}
long double Mul(long double a,long double b) {volatile long double x=a,y=b,z=x*y;return z;}
long double Div(long double a,long double b) {volatile long double x=a,y=b,z=x/y;return z;}
float Store(long double value) {volatile float result=static_cast<float>(value);if(!std::isfinite(result)) throw std::runtime_error("Oracle Float overflow");return result;}
float Distance(Vec point,const Node& plane) {
    return Store(Sub(Add(Add(Mul(point.z,plane.planeZ),Mul(point.y,plane.planeY)),Mul(point.x,plane.planeX)),plane.planeW));
}
bool Front(float value) {return std::bit_cast<std::int32_t>(value)>=0;}
struct OracleResult {bool clear{};std::size_t nodes{},leaves{};};
OracleResult Oracle(const Model& model,Vec end,Vec start) {
    OracleResult out;
    // Independent recursive scalar/x87 oracle, only generated valid small DAGs.
    std::function<bool(std::int32_t,Vec,Vec,bool)> visit=[&](std::int32_t index,Vec target,Vec origin,bool outside) {
        if(index==-1) {++out.leaves;return outside;}
        ++out.nodes;const auto& node=model.nodes.at(static_cast<std::size_t>(index));
        const auto d0=Distance(origin,node),d1=Distance(target,node);
        const int s0=Front(d0)?1:0,s1=Front(d1)?1:0;
        const auto child=[&](int side) {return side?node.front:node.back;};
        const auto region=[&](int side) {return static_cast<bool>(side^((side^static_cast<int>(outside))&(node.nodeFlags&1u)));};
        if(s0==s1) return visit(child(s0),target,origin,region(s0));
        const auto fraction=Div(d0,Sub(d0,d1));
        const auto component=[&](float a,float b) {return Store(Add(Mul(Sub(b,a),fraction),a));};
        const Vec middle{component(origin.x,target.x),component(origin.y,target.y),component(origin.z,target.z)};
        return visit(child(s1),middle,target,region(s1))&&visit(child(s0),middle,origin,region(s0));
    };
    out.clear=model.nodes.empty()?model.rootOutside!=0:visit(0,end,start,model.rootOutside!=0);return out;
}
long double Extended(Math::Dyadic value) {
    const auto magnitude=std::ldexp(static_cast<long double>(static_cast<std::uint64_t>(value.significand)),value.exponent);
    return value.negative?-magnitude:magnitude;
}
void ExactOracle() {
    Precision precision;std::uint32_t state=0xa5761341u;
    for(unsigned iteration=0u;iteration<20000u;++iteration) {
        const Vec point{Number(state),Number(state),Number(state)};
        const auto plane=Plane(Number(state),Number(state),Number(state),Number(state));
        bool actualRefused{},oracleRefused{};float actual{},expected{};
        try {actual=Math::Distance(point,plane);} catch(const std::runtime_error&) {actualRefused=true;}
        try {expected=Distance(point,plane);} catch(const std::runtime_error&) {oracleRefused=true;}
        Require(actualRefused==oracleRefused,"PC64 plane distance refusal differs from independent x87");
        if(!actualRefused) Require(std::bit_cast<std::uint32_t>(actual)==std::bit_cast<std::uint32_t>(expected),"PC64 z/y/x distance differs at Float store");
        const float a=Number(state),b=Number(state),c=Number(state),d=Number(state);
        const auto numerator=Math::Add64(Math::Decode(a),Math::Negate(Math::Decode(b)));
        const auto denominator=Math::Add64(Math::Decode(c),Math::Negate(Math::Decode(d)));
        if(denominator.significand) {
            const auto quotient=Extended(Math::Divide64(numerator,denominator));
            const auto oracle=Div(Sub(a,b),Sub(c,d));
            Require(quotient==oracle&&std::signbit(quotient)==std::signbit(oracle),"General signed PC64 division differs from x87");
        }
    }
    for(unsigned iteration=0u;iteration<10000u;++iteration) {
        Model model;model.rootOutside=static_cast<int>(Next(state)&1u);
        const std::size_t count=1u+(Next(state)%7u);
        for(std::size_t index=0u;index<count;++index) {
            const auto coord=[&] {return static_cast<float>(static_cast<int>(Next(state)%8193u)-4096)/64.0f;};
            auto node=Plane(coord(),coord(),coord(),coord(),static_cast<std::uint8_t>(Next(state)));
            node.vertexCount=static_cast<std::uint8_t>(Next(state)%8u);
            if(index+1u<count) {
                const auto child=[&] {return (Next(state)&1u)?-1:static_cast<int>(index+1u+Next(state)%(count-index-1u));};
                node.front=child();node.back=child();
            }
            model.nodes.push_back(node);
        }
        const auto coordinate=[&] {return static_cast<float>(static_cast<int>(Next(state)%32769u)-16384)/16.0f;};
        const Vec end{coordinate(),coordinate(),coordinate()},start{coordinate(),coordinate(),coordinate()};
        const auto actual=Check(model,end,start);const auto expected=Oracle(model,end,start);
        Require(actual.clear==expected.clear&&actual.stats.nodeVisits==expected.nodes&&actual.stats.leafVisits==expected.leaves,
            "Bounded iterative BSP result/order differs from independent recursive x87 oracle");
    }
}
#endif
} // namespace
int main() {
    try {
        HandComputed();Safety();
#if (defined(__i386__) || defined(__x86_64__)) && defined(__GNUC__)
        ExactOracle();
#endif
        std::cout<<"PASS original BSP FastLineCheck: "<<checks<<" checks, "<<refusals
            <<" refusals; signed-zero, NotCsg-only, End-first reversed segments, bounded DAGs and exact PC64 math.\n";return 0;
    } catch(const std::exception& error) {std::cerr<<"FAIL FastLineCheck: "<<error.what()<<'\n';return 1;}
}
