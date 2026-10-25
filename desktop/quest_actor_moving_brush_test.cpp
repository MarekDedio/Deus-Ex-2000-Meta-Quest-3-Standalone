#include "quest_actor_moving_brush.h"

#include <array>
#include <bit>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace {
unsigned checks{};
void Require(bool value,const char* message) {
    ++checks;if(!value)throw std::runtime_error(message);
}
std::uint32_t Raw(float value) {return std::bit_cast<std::uint32_t>(value);}
float Float(std::uint32_t value) {return std::bit_cast<float>(value);}
using Pose=std::array<float,3u>;
using Bits=std::array<std::uint32_t,3u>;
Bits RawPose(const Pose& pose) {return {Raw(pose[0u]),Raw(pose[1u]),Raw(pose[2u])};}
constexpr std::uintptr_t actor=0x1234u;
const std::vector<std::string> ordered{
    "location0","writeOld0","location1","writeOld1","location2","writeOld2","inner",
    "location2","old2","location1","old1","location0","old0","velocity0","velocity1","velocity2"};

// This explicitly MOCKS the inner method. It proves only the outer dispatch
// protocol, not the full production moving-brush body or a real engine root.
struct Fixture {
    Pose location{1.0f,2.0f,3.0f},old{-7.0f,-8.0f,-9.0f},velocity{70.0f,80.0f,90.0f};
    Pose afterLocation{4.0f,6.0f,8.0f},afterOld{0.0f,1.0f,2.0f};
    std::vector<std::string> events;
    std::size_t workCalls{},workLimit=std::numeric_limits<std::size_t>::max(),innerWork{};
    unsigned innerEffects{};
    std::uint32_t innerDelta{};
    std::function<void(Fixture&)> onInner;
    void Work() {
        const auto current=workCalls++;
        if(current>=workLimit)throw std::runtime_error("mock shared root work exhausted");
    }
    QuestVr::OriginalActorMovingBrushQueries Queries() {
        QuestVr::OriginalActorMovingBrushQueries q;
        q.work=[&] {Work();};
        q.location=[&](std::uintptr_t object,unsigned axis) {
            Require(object==actor,"Location read actor changed");events.push_back("location"+std::to_string(axis));
            return location.at(axis);
        };
        q.oldLocation=[&](std::uintptr_t object,unsigned axis) {
            Require(object==actor,"OldLocation read actor changed");events.push_back("old"+std::to_string(axis));
            return old.at(axis);
        };
        q.writeOldLocation=[&](std::uintptr_t object,unsigned axis,float value) {
            Require(object==actor,"OldLocation store actor changed");events.push_back("writeOld"+std::to_string(axis));
            old.at(axis)=value;
        };
        q.inner=[&](std::uintptr_t object,float delta) {
            Require(object==actor,"Inner actor changed");events.push_back("inner");innerDelta=Raw(delta);
            for(std::size_t i=0u;i<innerWork;++i)Work();
            ++innerEffects;
            if(onInner)onInner(*this);else {location=afterLocation;old=afterOld;}
        };
        q.writeVelocity=[&](std::uintptr_t object,unsigned axis,float value) {
            Require(object==actor,"Velocity store actor changed");events.push_back("velocity"+std::to_string(axis));
            velocity.at(axis)=value;
        };
        return q;
    }
};
template<class Function> void Refuses(Function&& function,const char* message) {
    bool refused{};
    try {function();}catch(const std::exception&) {refused=true;}
    Require(refused,message);
}
void Protocol() {
    Fixture f;
    const auto stats=QuestVr::PerformOriginalActorMovingBrush(actor,0.5f,f.Queries());
    Require(f.events==ordered,"Moving-brush native source order changed");
    Require(f.velocity==Pose{8.0f,10.0f,12.0f},"Fresh post-inner Location/OldLocation were not consumed");
    Require(stats.steps==16u && stats.locationReads==6u && stats.oldLocationReads==3u &&
        stats.oldLocationWrites==3u && stats.innerCalls==1u && stats.velocityWrites==3u,
        "Moving-brush outer counters changed");
    Require(f.workCalls==stats.steps && f.innerEffects==1u && f.innerDelta==Raw(0.5f),
        "Inner or work provider was skipped/duplicated");
    Fixture interleaved;auto q=interleaved.Queries();
    const auto write=q.writeOldLocation;
    q.writeOldLocation=[&](std::uintptr_t object,unsigned axis,float value) {
        write(object,axis,value);
        if(axis<2u)interleaved.location[axis+1u]=40.0f+static_cast<float>(axis);
    };
    interleaved.onInner=[](Fixture& state) {
        Require(state.old==Pose{1.0f,40.0f,41.0f},"Prefix eagerly preloaded Location instead of interleaving copies");
    };
    QuestVr::PerformOriginalActorMovingBrush(actor,1.0f,q);
    Require(interleaved.velocity==Pose{},"True unchanged finite inner did not derive zero velocity");

    Fixture writes;auto wq=writes.Queries();const auto writeVelocity=wq.writeVelocity;
    wq.writeVelocity=[&](std::uintptr_t object,unsigned axis,float value) {
        writeVelocity(object,axis,value);
        if(!axis){writes.location={1000.0f,1000.0f,1000.0f};writes.old={-1000.0f,-1000.0f,-1000.0f};}
    };
    QuestVr::PerformOriginalActorMovingBrush(actor,0.5f,wq);
    Require(writes.velocity==Pose{8.0f,10.0f,12.0f},"Destination stores caused invented source rereads");

    Fixture asymmetric;asymmetric.afterLocation={7.0f,7.0f,7.0f};asymmetric.afterOld={0.0f,0.0f,0.0f};
    QuestVr::PerformOriginalActorMovingBrush(actor,3.0f,asymmetric.Queries());
    Require(RawPose(asymmetric.velocity)==Bits{0x40155556u,0x40155556u,0x40155555u},
        "Retained PC64 Z reciprocal was replaced by the X/Y Float reciprocal");
}
void LazyAndFailure() {
    Fixture null;Refuses([&] {QuestVr::PerformOriginalActorMovingBrush(0u,1.0f,null.Queries());},
        "Null actor accepted");Require(null.events.empty() && !null.workCalls,"Null actor demanded providers");
    for(std::size_t cut=0u;cut<ordered.size();++cut) {
        Fixture f;f.workLimit=cut;
        Refuses([&] {QuestVr::PerformOriginalActorMovingBrush(actor,0.5f,f.Queries());},"Work cutoff was ignored");
        Require(f.events==std::vector<std::string>(ordered.begin(),ordered.begin()+static_cast<std::ptrdiff_t>(cut)),
            "Work cutoff demanded later provider or lost source prefix");
        Require(f.workCalls==cut+1u,"Shared work was not charged before native provider");
    }
    Fixture missing;auto mq=missing.Queries();mq.inner={};
    Refuses([&] {QuestVr::PerformOriginalActorMovingBrush(actor,1.0f,mq);},"Missing genuine inner accepted");
    Require(missing.events==std::vector<std::string>(ordered.begin(),ordered.begin()+6),
        "Missing inner was eagerly demanded or silently replaced");
    Fixture throwing;throwing.onInner=[](Fixture& state) {
        state.location[0u]=99.0f;throw std::runtime_error("mock callback refused");
    };
    Refuses([&] {QuestVr::PerformOriginalActorMovingBrush(actor,1.0f,throwing.Queries());},"Inner failure swallowed");
    Require(throwing.events==std::vector<std::string>(ordered.begin(),ordered.begin()+7) &&
        throwing.location[0u]==99.0f && throwing.old==Pose{1.0f,2.0f,3.0f},
        "Helper invented local rollback or continued after failed callback");
    Fixture storeFailure;auto sq=storeFailure.Queries();const auto write=sq.writeOldLocation;
    sq.writeOldLocation=[&](std::uintptr_t object,unsigned axis,float value) {
        write(object,axis,value);
        if(axis==1u)throw std::runtime_error("mock native store refused after journaling");
    };
    Refuses([&] {QuestVr::PerformOriginalActorMovingBrush(actor,1.0f,sq);},"Native copy failure was swallowed");
    Require(storeFailure.events==std::vector<std::string>(ordered.begin(),ordered.begin()+4) &&
        storeFailure.old==Pose{1.0f,2.0f,-9.0f} && !storeFailure.innerEffects,
        "Failed native copy ran later providers or invented local undo");

    // A small mock of the CALLER's shared rollback root, not a claim about the
    // production journal. Both outer prefixes and nested inner work use it.
    for(std::size_t cut=0u;cut<19u;++cut) {
        Fixture f;f.innerWork=3u;f.workLimit=cut;
        const auto beforeLocation=RawPose(f.location),beforeOld=RawPose(f.old),beforeVelocity=RawPose(f.velocity);
        bool refused{};
        try {QuestVr::PerformOriginalActorMovingBrush(actor,0.5f,f.Queries());}
        catch(const std::exception&) {
            refused=true;
            for(unsigned axis=0u;axis<3u;++axis) {
                f.location[axis]=Float(beforeLocation[axis]);f.old[axis]=Float(beforeOld[axis]);
                f.velocity[axis]=Float(beforeVelocity[axis]);
            }
            f.innerEffects=0u;
        }
        Require(refused && f.workCalls==cut+1u,"Nested inner work escaped shared bound");
        Require(RawPose(f.location)==beforeLocation && RawPose(f.old)==beforeOld &&
            RawPose(f.velocity)==beforeVelocity && !f.innerEffects,"Caller root rollback lost native/nested writes");
    }
    Fixture shared;shared.innerWork=3u;shared.workLimit=19u;
    const auto stats=QuestVr::PerformOriginalActorMovingBrush(actor,0.5f,shared.Queries());
    Require(stats.steps==16u && shared.workCalls==19u,"Inner work incorrectly became a second budget");
}
void ExceptionalAndZero() {
    const float maximum=std::numeric_limits<float>::max();
    const float nan=Float(0x7fc12345u),negativeZero=Float(0x80000000u);
    Fixture repair;repair.location={nan,negativeZero,Float(0xff800000u)};
    repair.onInner=[&](Fixture& state) {
        Require(RawPose(state.old)==Bits{0x7fc12345u,0x80000000u,0xff800000u},
            "Raw prefix sanitized/canonicalized a reached native copy");
        state.location={0.0f,0.0f,0.0f};state.old={0.0f,0.0f,0.0f};
    };
    QuestVr::PerformOriginalActorMovingBrush(actor,1.0f,repair.Queries());
    Require(repair.velocity==Pose{},"Inner could not repair raw nonfinite copied fields before arithmetic");
    for(float delta:{0.0f,negativeZero,nan,std::numeric_limits<float>::infinity(),
        -std::numeric_limits<float>::infinity()}) {
        Fixture f;
        Refuses([&] {QuestVr::PerformOriginalActorMovingBrush(actor,delta,f.Queries());},"Exceptional divisor accepted");
        Require(f.events==std::vector<std::string>(ordered.begin(),ordered.begin()+13) &&
            f.innerEffects==1u && f.innerDelta==Raw(delta),"Delta was rejected before reached copies/inner/displacements");
    }
    for(unsigned axis=0u;axis<3u;++axis)for(bool badOld:{false,true}) {
        Fixture f;if(badOld)f.afterOld[axis]=nan;else f.afterLocation[axis]=nan;
        Refuses([&] {QuestVr::PerformOriginalActorMovingBrush(actor,1.0f,f.Queries());},"Nonfinite displacement accepted");
        const std::size_t end=9u+2u*(2u-axis);
        Require(f.events==std::vector<std::string>(ordered.begin(),ordered.begin()+static_cast<std::ptrdiff_t>(end)),
            "Nonfinite displacement refused before its pair read or after a later axis");
    }
    for(unsigned axis:{1u,0u}) {
        Fixture f;f.afterLocation[axis]=maximum;f.afterOld[axis]=-maximum;
        Refuses([&] {QuestVr::PerformOriginalActorMovingBrush(actor,1.0f,f.Queries());},"Displacement Float overflow accepted");
        const std::size_t end=9u+2u*(2u-axis);
        Require(f.events==std::vector<std::string>(ordered.begin(),ordered.begin()+static_cast<std::ptrdiff_t>(end)),
            "Difference overflow did not stop at the original Y/X Float spill");
    }
    Fixture reciprocal;reciprocal.afterLocation={0.0f,0.0f,0.0f};reciprocal.afterOld={0.0f,0.0f,0.0f};
    Refuses([&] {QuestVr::PerformOriginalActorMovingBrush(actor,std::numeric_limits<float>::denorm_min(),reciprocal.Queries());},
        "Float reciprocal overflow silently became benign zero velocity");
    Require(reciprocal.events==std::vector<std::string>(ordered.begin(),ordered.begin()+13),
        "Reciprocal overflow happened after a velocity store");
    for(unsigned axis:{0u,1u,2u}) {
        Fixture f;f.afterLocation={0.0f,0.0f,0.0f};f.afterOld={0.0f,0.0f,0.0f};f.afterLocation[axis]=maximum;
        Refuses([&] {QuestVr::PerformOriginalActorMovingBrush(actor,0.5f,f.Queries());},"Velocity Float overflow accepted");
        const auto end=axis==2u?14u:13u;
        Require(f.events==std::vector<std::string>(ordered.begin(),ordered.begin()+end),
            "Result overflow changed the X store before final Z spill boundary");
        if(axis==2u)Require(Raw(f.velocity[0u])==0u && f.velocity[1u]==80.0f && f.velocity[2u]==90.0f,
            "Z overflow lost the exact already-written X/native YZ prefix");
    }
    for(float delta:{1.0f,-1.0f})for(float current:{0.0f,negativeZero})for(float old:{0.0f,negativeZero}) {
        Fixture f;f.afterLocation={current,current,current};f.afterOld={old,old,old};
        QuestVr::PerformOriginalActorMovingBrush(actor,delta,f.Queries());
        const bool negativeDifference=Raw(current)==0x80000000u && Raw(old)==0u;
        const auto expected=(negativeDifference!=(delta<0.0f))?0x80000000u:0u;
        Require(RawPose(f.velocity)==Bits{expected,expected,expected},"Signed-zero displacement/product was normalized");
    }
    Fixture negative;
    QuestVr::PerformOriginalActorMovingBrush(actor,-0.5f,negative.Queries());
    Require(negative.velocity==Pose{-8.0f,-10.0f,-12.0f},"Finite negative outer delta was clamped/skipped");
}

#if (defined(__i386__) || defined(__x86_64__)) && defined(__GNUC__)
struct RetailPrecisionOracle {
    unsigned short original{};
    RetailPrecisionOracle() {
        asm volatile("fnstcw %0" : "=m"(original));
        const auto mode=static_cast<unsigned short>((original&~0x0f00u)|0x033fu);
        asm volatile("fldcw %0" : : "m"(mode));
    }
    ~RetailPrecisionOracle() {asm volatile("fnclex\n\tfldcw %0" : : "m"(original));}
};
// Independent hardware x87 arithmetic, NEVER execution of the original DLL.
// Volatile extended stores retain PC64 values; only the original explicit
// Float spill points narrow to binary32. This deliberately uses no dyadics.
Pose Oracle(const Pose& current,const Pose& old,float delta) {
    for(unsigned axis=0u;axis<3u;++axis)if(!std::isfinite(current[axis]) || !std::isfinite(old[axis]))
        throw std::runtime_error("oracle nonfinite displacement");
    volatile long double z=static_cast<long double>(current[2u])-static_cast<long double>(old[2u]);
    volatile long double differenceY=static_cast<long double>(current[1u])-static_cast<long double>(old[1u]);
    volatile float y=static_cast<float>(differenceY);
    if(!std::isfinite(y))throw std::runtime_error("oracle Y displacement spill overflow");
    volatile long double differenceX=static_cast<long double>(current[0u])-static_cast<long double>(old[0u]);
    volatile float x=static_cast<float>(differenceX);
    if(!std::isfinite(x))throw std::runtime_error("oracle X displacement spill overflow");
    if(!std::isfinite(delta) || delta==0.0f)throw std::runtime_error("oracle exceptional divisor");
    volatile long double reciprocal=1.0L/static_cast<long double>(delta);
    volatile float reciprocalFloat=static_cast<float>(reciprocal);
    if(!std::isfinite(reciprocalFloat))throw std::runtime_error("oracle reciprocal spill overflow");
    volatile long double resultZ=z*reciprocal;
    volatile long double resultY=static_cast<long double>(reciprocalFloat)*static_cast<long double>(y);
    volatile long double resultX=static_cast<long double>(reciprocalFloat)*static_cast<long double>(x);
    Pose result{static_cast<float>(resultX),static_cast<float>(resultY),static_cast<float>(resultZ)};
    for(float value:result)if(!std::isfinite(value))throw std::runtime_error("oracle result spill overflow");
    return result;
}
void CheckOracle(const Pose& current,const Pose& old,float delta,unsigned& accepted,unsigned& witnesses) {
    Pose result{},expected{};bool refused{},oracleRefused{};
    QuestVr::OriginalActorMovingBrushQueries q;
    q.location=[&](std::uintptr_t,unsigned axis) {return current.at(axis);};
    q.oldLocation=[&](std::uintptr_t,unsigned axis) {return old.at(axis);};
    q.writeOldLocation=[](std::uintptr_t,unsigned,float) {};
    q.inner=[](std::uintptr_t,float) {}; // Explicit mock supplies fixed post-call fields.
    q.writeVelocity=[&](std::uintptr_t,unsigned axis,float value) {result.at(axis)=value;};
    try {QuestVr::PerformOriginalActorMovingBrush(actor,delta,q);}catch(const std::runtime_error&) {refused=true;}
    try {expected=Oracle(current,old,delta);}catch(const std::runtime_error&) {oracleRefused=true;}
    if(refused!=oracleRefused || (!refused && RawPose(result)!=RawPose(expected))) {
        std::cerr << "x87 mismatch delta=" << std::hex << Raw(delta) << " current=" << Raw(current[0u]) << ','
            << Raw(current[1u]) << ',' << Raw(current[2u]) << " old=" << Raw(old[0u]) << ',' << Raw(old[1u]) << ','
            << Raw(old[2u]) << " result=" << Raw(result[0u]) << ',' << Raw(result[1u]) << ',' << Raw(result[2u])
            << " expected=" << Raw(expected[0u]) << ',' << Raw(expected[1u]) << ',' << Raw(expected[2u]) << std::dec << '\n';
        throw std::runtime_error("Moving-brush outer PC64/x87 oracle mismatch");
    }
    ++checks;
    if(!refused) {
        ++accepted;
        // Equal displacement on all channels must still be able to distinguish
        // retained Z reciprocal/difference from the X/Y Float-spilled path.
        if(Raw(current[0u])==Raw(current[2u]) && Raw(old[0u])==Raw(old[2u]) && Raw(result[0u])!=Raw(result[2u]))
            ++witnesses;
    }
}
void HardwareOracle() {
    static_assert(std::numeric_limits<long double>::digits==64,"x87 oracle requires extended significand64");
    RetailPrecisionOracle precision;
    unsigned accepted{},witnesses{};
    const float zero=Float(0x80000000u),tiny=std::numeric_limits<float>::denorm_min();
    for(float current:{zero,0.0f,tiny,-tiny,Float(0x3f800001u),Float(0x7f7fffffu)})
        for(float old:{zero,0.0f,-tiny,Float(0x3eaaaaabu),Float(0xff7fffffu)})
            for(float delta:{zero,0.0f,tiny,-1.0f,Float(0x3eaaaaabu),Float(0x7f7fffffu)})
                CheckOracle({current,current,current},{old,old,old},delta,accepted,witnesses);
    std::uint32_t random=0x9b85a711u;
    const auto next=[&] {random^=random<<13u;random^=random>>17u;random^=random<<5u;return random;};
    const auto finite=[&] {
        const auto bits=next();return Float((bits&0x80000000u)|((bits&0x7fffffffu)%0x7f800000u));
    };
    for(unsigned i=0u;i<30000u;++i) {
        Pose current{finite(),finite(),finite()},old{finite(),finite(),finite()};
        if(i&1u){current[2u]=current[0u];old[2u]=old[0u];}
        const float delta=finite();CheckOracle(current,old,delta,accepted,witnesses);
    }
    Require(accepted>5000u,"Hardware oracle did not exercise enough finite admitted packets");
    Require(witnesses>100u,"Oracle corpus did not distinguish asymmetric Z/X/Y spills");
    std::cout << "Independent x87 outer packets: 30180; admitted=" << accepted
        << "; asymmetric-spill witnesses=" << witnesses << '\n';
}
#endif
} // namespace

int main() {
    try {
        Protocol();LazyAndFailure();ExceptionalAndZero();
#if (defined(__i386__) || defined(__x86_64__)) && defined(__GNUC__)
        HardwareOracle();
#else
        std::cout << "Independent x87 oracle unavailable; deterministic fixtures only\n";
#endif
        std::cout << "Moving-brush OUTER envelope tests passed: " << checks
            << " checks (mock inner; not whole moving-brush integration)\n";
        return 0;
    }catch(const std::exception& error){std::cerr << error.what() << '\n';return 1;}
}
