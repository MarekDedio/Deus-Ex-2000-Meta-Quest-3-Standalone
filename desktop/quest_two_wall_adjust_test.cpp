#include "quest_two_wall_adjust.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

#include "quest_two_wall_adjust_oracle.inl"

namespace {
using Vec=PortableModelVec3;
using Slots=std::array<Vec,4u>;
using Partition=std::array<unsigned,4u>;
namespace Oracle=TwoWallOracle;
// Restricted-growth strings enumerate all Bell(4)=15 EXACT shared-object
// partitions, in argument order Desired, Delta, HitNormal, OldHitNormal.
constexpr std::array partitions{Partition{0,0,0,0},Partition{0,0,0,1},Partition{0,0,1,0},
    Partition{0,0,1,1},Partition{0,0,1,2},Partition{0,1,0,0},Partition{0,1,0,1},
    Partition{0,1,0,2},Partition{0,1,1,0},Partition{0,1,1,1},Partition{0,1,1,2},
    Partition{0,1,2,0},Partition{0,1,2,1},Partition{0,1,2,2},Partition{0,1,2,3}};
std::size_t checks{},refusals{},comparisons{},finitePackets{},widePackets{},aliasPackets{},
    thresholdPackets{},retainedFixtures{},factorWitnesses{},inactiveSlots{};
Oracle::Census census;
[[maybe_unused]] std::array<std::size_t,15u> aliasSuccess{};
std::array<std::size_t,15u> aliasRefusal{};
void Require(bool value,const char* reason){++checks;if(!value)throw std::runtime_error(reason);}
std::uint32_t Bits(float value){return std::bit_cast<std::uint32_t>(value);}
bool Same(Vec a,Vec b){return Bits(a.x)==Bits(b.x)&&Bits(a.y)==Bits(b.y)&&Bits(a.z)==Bits(b.z);}
bool Same(const Slots& a,const Slots& b){for(std::size_t i=0u;i<a.size();++i)if(!Same(a[i],b[i]))return false;return true;}
template<class F>void Refuse(F&& run){bool failed{};try{run();}catch(const std::exception&){failed=true;}
    Require(failed,"Unsupported TwoWallAdjust silently succeeded");++refusals;}
void Actual(Slots& slots,const Partition& p,float time){
    QuestVr::AdjustOriginalActorTwoWalls(slots[p[0]],slots[p[1]],slots[p[2]],slots[p[3]],time);
}
void SetComponent(Vec& vector,unsigned component,float value){
    if(component==0u)vector.x=value;else if(component==1u)vector.y=value;else vector.z=value;
}
void Golden(Vec desired,Vec delta,Vec normal,Vec old,float time,Vec expected){
    const auto beforeDesired=desired,beforeNormal=normal,beforeOld=old;
    QuestVr::AdjustOriginalActorTwoWalls(desired,delta,normal,old,time);
    Require(Same(delta,expected)&&Same(desired,beforeDesired)&&Same(normal,beforeNormal)&&Same(old,beforeOld),
        "TwoWallAdjust deterministic output/input raw words differ");
}
void Deterministic(){
    Golden({0,1,0},{1,2,3},{1,0,0},{1,0,0},.25f,{0,1.5f,2.25f});
    Golden({1,0,0},{1,2,3},{1,0,0},{1,0,0},.25f,{}); // Strict-forward face gate.
    Golden({0,0,1},{1,2,3},{1,0,0},{0,1,0},.5f,{0,0,1.5f});
    Golden({0,0,-1},{1,2,3},{1,0,0},{0,1,0},.5f,{-0.0f,-0.0f,-1.5f});
    Golden({1,1,1},{1,1,1},{1,0,0},{-1,0,0},2.0f,{-0.0f,-0.0f,-0.0f});
    Golden({0,-1,0},{0,2,0},{1,0,0},{1,0,0},2.0f,{-0.0f,-2.0f,-0.0f}); // No time clamp.
    Golden({0,1,0},{0,2,0},{1,0,0},{1,0,0},-1.0f,{0,4,0});
    const float tiny=std::numeric_limits<float>::denorm_min();
    Golden({0,1,0},{0,1,0},{1,tiny,0},{0,.25f,0},0.0f,{-tiny,1,0});++retainedFixtures;
    Golden({0,0,-tiny},{0,0,.25f},{1,0,0},{0,1,0},0.0f,{-0.0f,-0.0f,-.25f});++retainedFixtures;
    Golden({0,tiny,0},{0,.25f,0},{1,0,0},{1,0,0},0.0f,{0,.25f,0});++retainedFixtures;
    // Delta aliases Desired: the gate reads the just-written Delta, not the
    // original requested vector. A time>1 result therefore stays backwards.
    Vec shared{0,2,0};const Vec normal{1,0,0};
    QuestVr::AdjustOriginalActorTwoWalls(shared,shared,normal,normal,2.0f);
    Require(Same(shared,Vec{-0.0f,-2.0f,-0.0f}),"Desired==Delta gate used a prewrite Desired snapshot");
    // SafeNormal below/above its literal Double1e-8 squared-length threshold.
    const float below=std::nextafter(.0001f,0.0f),above=std::nextafter(.0001f,1.0f);
    Golden({0,0,1},{0,0,2},{1,0,0},{0,below,0},0.0f,{});
    Golden({0,0,1},{0,0,2},{1,0,0},{0,above,0},0.0f,{0,0,2});
}
void AtomicRefusals(){
    constexpr std::array exceptional{std::uint32_t{0x7fc12345u},std::uint32_t{0xffc54321u},
        std::uint32_t{0x7f812345u},std::uint32_t{0xff812345u},std::uint32_t{0x7f800000u},std::uint32_t{0xff800000u}};
    const Slots initial{{{1,2,3},{2,-3,4},{1,.25f,-.5f},{-1,.5f,.25f}}};
    for(std::size_t index=0u;index<partitions.size();++index){const auto& p=partitions[index];
        unsigned used{};for(const auto slot:p)used=std::max(used,slot+1u);
        for(unsigned slot=0u;slot<used;++slot)for(unsigned component=0u;component<3u;++component)for(const auto word:exceptional){
            auto slots=initial;SetComponent(slots[slot],component,std::bit_cast<float>(word));const auto before=slots;
            Refuse([&]{Actual(slots,p,.25f);});Require(Same(slots,before),"Active nonfinite refusal leaked staged Delta into an aliased input");++aliasRefusal[index];
        }
        for(const auto word:exceptional){auto slots=initial;const auto before=slots;
            Refuse([&]{Actual(slots,p,std::bit_cast<float>(word));});
            Require(Same(slots,before),"Nonfinite HitTime refusal changed caller Vector words");++aliasRefusal[index];
        }
        // Every used vector large and positive forces the face branch and an
        // overflowing projected-normal Float store, for every alias topology.
        auto slots=initial;const float large=std::numeric_limits<float>::max();
        for(unsigned slot=0u;slot<used;++slot)slots[slot]={large,large,large};
        const auto before=slots;
        Refuse([&]{Actual(slots,p,0.0f);});Require(Same(slots,before),"Overflow refusal changed any exact-reference alias");++aliasRefusal[index];
    }
    // Explicit late failures: Desired is consumed only after Delta writes;
    // staging must hide those partial native writes from all caller objects.
    auto slots=initial;slots[0].z=std::bit_cast<float>(0x7fc12345u);const auto before=slots;
    Refuse([&]{Actual(slots,partitions.back(),0.0f);});Require(Same(slots,before),"Late desired-gate failure published earlier output writes");
}
#if (defined(__i386__) || defined(__x86_64__)) && defined(__GNUC__)
void Expected(Slots& slots,const Partition& p,float time,Oracle::Census& counts,
    Oracle::Variant variant=Oracle::Variant::Exact){Oracle::Adjust(slots[p[0]],slots[p[1]],slots[p[2]],slots[p[3]],time,counts,variant);}
void Differential(const Slots& initial,const Partition& p,float time,std::size_t alias=15u){
    auto actual=initial,expected=initial;bool af{},ef{};Oracle::Census counts;
    try{Actual(actual,p,time);}catch(const std::exception&){af=true;}
    try{Expected(expected,p,time,counts);}catch(const std::exception&){ef=true;}
    Require(af==ef,"TwoWallAdjust supported-domain refusal differs from independent x87 protocol");
    if(af){Require(Same(actual,initial),"Refused differential query leaked partial aliased Vector writes");++refusals;if(alias<15u)++aliasRefusal[alias];return;}
    if(!Same(actual,expected))throw std::runtime_error("TwoWallAdjust raw output words differ from independent x87; partition "+std::to_string(alias));
    ++checks;++comparisons;if(alias<15u)++aliasSuccess[alias];
    census.corner+=counts.corner;census.face+=counts.face;census.safeZero+=counts.safeZero;
    census.safeNormalize+=counts.safeNormalize;census.flips+=counts.flips;census.zeros+=counts.zeros;
}
std::uint64_t Next(std::uint64_t& state){state^=state<<13u;state^=state>>7u;state^=state<<17u;return state;}
float Small(std::uint64_t& state){return static_cast<float>(static_cast<int>(Next(state)%8193u)-4096)/1024.0f;}
Vec Vector(std::uint64_t& state){return {Small(state),Small(state),Small(state)};}
float Wide(std::uint64_t& state){auto bits=static_cast<std::uint32_t>(Next(state));
    if((bits&0x7f800000u)==0x7f800000u)bits^=0x00800000u;
    return std::bit_cast<float>(bits);
}
void Witness(const Slots& initial,Oracle::Variant variant){
    auto exact=initial,wrong=initial;Oracle::Census a,b;
    Expected(exact,partitions.back(),0.0f,a);Expected(wrong,partitions.back(),0.0f,b,variant);
    Require(!Same(exact,wrong),"Deliberate retained-gate fixture cannot detect its forbidden Float spill");
    Differential(initial,partitions.back(),0.0f);
}
void HardwareCohorts(){
    Oracle::Precision precision;std::uint64_t rng=0x61741b73ac9025e1ull;
    const float tiny=std::numeric_limits<float>::denorm_min();
    Witness(Slots{{{0,1,0},{0,1,0},{1,tiny,0},{0,.25f,0}}},Oracle::Variant::StoredNormalGate);
    Witness(Slots{{{0,0,-tiny},{0,0,.25f},{1,0,0},{0,1,0}}},Oracle::Variant::StoredDesiredGate);
    Witness(Slots{{{0,tiny,0},{0,.25f,0},{1,0,0},{1,0,0}}},Oracle::Variant::StoredDesiredGate);
    for(unsigned round=0u;round<1600u;++round){Slots initial;for(auto& value:initial)value=Vector(rng);const auto time=Small(rng);
        for(std::size_t alias=0u;alias<partitions.size();++alias){Differential(initial,partitions[alias],time,alias);++aliasPackets;}
        ++finitePackets;
    }
    for(unsigned round=0u;round<4096u;++round){Slots initial;for(auto& v:initial)v={Wide(rng),Wide(rng),Wide(rng)};
        Differential(initial,partitions.back(),Wide(rng));++widePackets;
    }
    // Deliberate degenerate parallel/antiparallel corners are not left to
    // probability: they exercise SafeNormal's exact zero and signed scaling.
    for(unsigned round=0u;round<512u;++round){
        const Slots initial{{Vector(rng),Vector(rng),{1,0,0},{-1,0,0}}};
        Differential(initial,partitions.back(),Small(rng));++thresholdPackets;
    }
    // Explicit exponent sweeps include retained products larger than Float,
    // subnormal output, sum cancellation, and SafeNormal threshold neighbors.
    for(int exponent=-149;exponent<=120;exponent+=7){const float value=std::ldexp(1.0f,exponent);
        for(const float time:{-1.0f,-0.0f,0.0f,.25f,.99999994f,1.0f,std::nextafter(1.0f,2.0f),2.0f}){
            const std::array packets{Slots{{{1,1,1},{value,-value,value},{1,0,0},{0,1,0}}},
                Slots{{{1,1,1},{value,-value,value},{1,0,0},{1,0,0}}},
                Slots{{{1,-1,1},{value,1,-value},{value,1,-value},{value,-1,value}}}};
            for(const auto& packet:packets){Differential(packet,partitions.back(),time);++thresholdPackets;}
        }
    }
    for(const float threshold:{0.0f,-0.0f,std::numeric_limits<float>::denorm_min(),
        std::nextafter(.0001f,0.0f),.0001f,std::nextafter(.0001f,1.0f),-.0001f})
        for(const float sign:{-1.0f,1.0f}){
            Differential(Slots{{{0,0,sign},{1,2,3},{1,0,0},{0,threshold,0}}},partitions.back(),.25f);++thresholdPackets;
        }
    const float up=std::nextafter(1.0f,2.0f),down=std::nextafter(1.0f,0.0f);
    Differential(Slots{{{0,0,1},{.25f,.5f,.75f},{up,1,0},{down,-1,0}}},partitions.back(),.125f);
    // Do not merely assert a retained factor exists: compare a deliberately
    // Float-spilled factor over a fixed safe cohort, require observable words,
    // and submit every exact case to the separate production/oracle check.
    for(unsigned round=0u;round<2048u;++round){
        const Slots initial{{Vector(rng),Vector(rng),{1,1,0},{0,0,1}}};const auto time=Small(rng);
        auto exact=initial,wrong=initial;Oracle::Census a,b;
        Expected(exact,partitions.back(),time,a);Expected(wrong,partitions.back(),time,b,Oracle::Variant::StoredCornerFactor);
        if(!Same(exact,wrong))++factorWitnesses;
        Differential(initial,partitions.back(),time);
    }
    Require(factorWitnesses>10u,"Retained corner-factor cohort did not detect forbidden Float-scale spills");
    // Unreferenced backing slots are deliberately poisoned. Exact aliases
    // address only their used objects; no blanket backing-array scan is legal.
    for(std::size_t alias=0u;alias<partitions.size();++alias){unsigned used{};for(const auto i:partitions[alias])used=std::max(used,i+1u);
        Slots initial{{{.25f,.5f,.75f},{1,0,0},{0,1,0},{1,1,0}}};
        for(unsigned slot=used;slot<4u;++slot){initial[slot]={std::bit_cast<float>(0x7fc12345u),std::numeric_limits<float>::infinity(),-0.0f};++inactiveSlots;}
        Differential(initial,partitions[alias],.25f,alias);
    }
    Require(comparisons>20'000u&&census.corner>1000u&&census.face>1000u&&census.safeZero>100u&&
        census.safeNormalize>1000u&&census.flips>100u&&census.zeros>1000u,
        "Independent TwoWallAdjust cohort missed one numeric branch or gate");
    for(std::size_t alias=0u;alias<partitions.size();++alias)Require(aliasSuccess[alias]>100u&&aliasRefusal[alias]>0u,
        "An exact-reference alias partition lacks successful packets or atomic refusals");
}
#else
void HardwareCohorts(){std::cout<<"SKIP independent x87 TwoWallAdjust oracle on non-x86 compiler; portable deterministic/refusal controls still run.\n";}
#endif
} // namespace
int main(){
    try{Deterministic();AtomicRefusals();HardwareCohorts();std::cout<<"PASS original TwoWallAdjust "<<checks<<" checks / "<<refusals
        <<" explicit refusals, "<<comparisons<<" independent x87 packets, "<<finitePackets<<" finite seeds/"<<aliasPackets
        <<" alias packets across all15 partitions, "<<widePackets<<" broad-exponent packets, "<<thresholdPackets
        <<" exponent/threshold controls; corner/face "<<census.corner<<'/'<<census.face<<", SafeNormal zero/normalize "
        <<census.safeZero<<'/'<<census.safeNormalize<<", flip/zero "<<census.flips<<'/'<<census.zeros<<", "<<retainedFixtures
        <<" retained-gate goldens/"<<factorWitnesses<<" factor-spill witnesses, "<<inactiveSlots<<" inactive raw slots.\n";return 0;}
    catch(const std::exception& error){std::cerr<<"Original TwoWallAdjust failed: "<<error.what()<<'\n';return 1;}
}
