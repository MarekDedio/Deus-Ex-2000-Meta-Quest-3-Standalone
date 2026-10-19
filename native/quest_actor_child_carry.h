#pragma once

#include "quest_actor_line_trace.h"
#include "quest_brush_coords.h"

#include <array>
#include <bit>
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <utility>

namespace QuestVr {

struct OriginalActorChildMove {
    std::uintptr_t level{},actor{};
    PortableModelVec3 delta{};
    std::array<std::int32_t,3u> rotation{};
    // bTest, bIgnorePawns, bIgnoreBases, bNoFail: native literal0,0,1,0.
    std::array<std::int32_t,4u> options{0,0,1,0};
};
struct OriginalActorChildCarryQueries {
    std::function<std::uint8_t(std::uintptr_t)> standingCount;
    std::function<std::int32_t(std::uintptr_t)> actorCount;
    std::function<std::uintptr_t(std::uintptr_t,std::int32_t)> actorSlot;
    std::function<std::uintptr_t(std::uintptr_t)> base;
    // Channel Pitch=0/Yaw=1/Roll=2, Location X=0/Y=1/Z=2. Raw words.
    std::function<std::int32_t(std::uintptr_t,unsigned)> rotation,viewRotation;
    std::function<float(std::uintptr_t,unsigned)> location;
    // The pointer load precedes offset spills; the48-byte UnitCoords copy is
    // AFTER them. Neither is called on the no-rotation branch.
    std::function<std::uintptr_t()> mathRoot;
    std::function<OriginalModelCoords(std::uintptr_t)> unitCoords;
    // COMPLETE recursive MoveActor in the SAME Level/VM/hash root, never a
    // collision-free transform setter. Its raw int and entire hit are ignored.
    std::function<std::int32_t(OriginalActorChildMove,OriginalActorLineHit&)> moveActor;
    std::function<bool(std::uintptr_t)> pawn;
    std::function<void(std::uintptr_t,unsigned,std::int32_t)> writeViewRotation;
    std::function<void()> work;
};
enum class OriginalActorCarryFlow { NoDependants,TestBypass,Scanned };
struct OriginalActorCarryResult {
    OriginalActorCarryFlow flow{OriginalActorCarryFlow::NoDependants};
    std::size_t slotVisits{},moves{};
};

namespace ActorChildCarryDetail {
using ActorLineTraceDetail::Load;
using ActorLineTraceDetail::Sub;
using ActorTimerDetail::Decode;
using ActorTimerDetail::Store32;
using ModelFastLineDetail::Multiply64;
using NativeWindowClipDetail::Add64;
template<class Function,class... Arguments>
inline auto Call(const OriginalActorChildCarryQueries& q,const Function& f,Arguments&&... arguments){
    if(!f)throw std::invalid_argument("Original child carry lacks an active native provider");
    if(q.work)q.work();
    return f(std::forward<Arguments>(arguments)...);
}
inline std::int32_t Add(std::int32_t a,std::int32_t b){
    return std::bit_cast<std::int32_t>(std::bit_cast<std::uint32_t>(a)+std::bit_cast<std::uint32_t>(b));
}
inline std::int32_t Difference(std::int32_t a,std::int32_t b){
    return std::bit_cast<std::int32_t>(std::bit_cast<std::uint32_t>(a)-std::bit_cast<std::uint32_t>(b));
}
inline std::int32_t QuantizedYaw(std::int32_t requested,std::int32_t current){
    // AND0000FFFC on EACH operand, not (requested-current)&FFFFFFFC.
    return std::bit_cast<std::int32_t>((std::bit_cast<std::uint32_t>(requested)&0x0000fffcu)-
        (std::bit_cast<std::uint32_t>(current)&0x0000fffcu));
}
inline ActorTimerDetail::Dyadic Row(const PortableModelVec3& offset,const PortableModelVec3& axis){
    return Add64(Add64(Multiply64(Load(offset.x),Load(axis.x)),Multiply64(Load(offset.y),Load(axis.y))),
        Multiply64(Load(offset.z),Load(axis.z)));
}
inline PortableModelVec3 RotationalDelta(const PortableModelVec3& offset,const OriginalModelCoords& coords){
    // Inline103999D0..10399A54: all row sums remain PC64 until XYZ stores.
    // This is VectorBy, not PointBy: coordinate Origin is inactive HERE.
    const auto x=Row(offset,coords.xAxis),y=Row(offset,coords.yAxis),z=Row(offset,coords.zAxis);
    const PortableModelVec3 transformed{Store32(x),Store32(y),Store32(z)};
    const auto dz=Sub(Load(offset.z),Decode(transformed.z));
    const auto dy=Sub(Load(offset.y),Decode(transformed.y));
    const auto dx=Sub(Load(offset.x),Decode(transformed.x));
    return {Store32(dx),Store32(dy),Store32(dz)};
}
inline PortableModelVec3 Offset(std::uintptr_t source,std::uintptr_t child,std::uintptr_t& math,
    const OriginalActorChildCarryQueries& q){
    const auto sz=Call(q,q.location,source,2u),cz=Call(q,q.location,child,2u);const auto z=Sub(Load(sz),Load(cz));
    const auto sy=Call(q,q.location,source,1u),cy=Call(q,q.location,child,1u);const auto y=Sub(Load(sy),Load(cy));
    const auto sx=Call(q,q.location,source,0u),cx=Call(q,q.location,child,0u);const auto x=Sub(Load(sx),Load(cx));
    math=Call(q,q.mathRoot); // Before Float stores, NOT the UnitCoords copy.
    return {Store32(x),Store32(y),Store32(z)};
}
inline PortableModelVec3 Translation(const PortableModelVec3& actual,const PortableModelVec3& rotational){
    PortableModelVec3 result;
    result.z=Store32(Add64(Load(actual.z),Decode(rotational.z)));
    result.y=Store32(Add64(Load(actual.y),Decode(rotational.y)));
    result.x=Store32(Add64(Load(actual.x),Decode(rotational.x)));
    return result;
}
} // namespace ActorChildCarryDetail

// Complete based-child segment103998A0..10399BB5, NOT complete MoveActor.
// Requested rotation/actualDelta are by VALUE; callbacks cannot rewrite the
// caller's by-value parent arguments. Source and child field reads remain live.
// No local transaction is opened: every recursive move, store and refusal MUST
// join the enclosing MoveActor/VM/hash root and its shared work ledger.
inline OriginalActorCarryResult CarryOriginalActorChildren(std::uintptr_t level,std::uintptr_t source,
    PortableModelVec3 actualDelta,std::array<std::int32_t,3u> requested,std::int32_t test,
    OriginalBrushSinTable table,const OriginalActorChildCarryQueries& q,std::size_t maximumVisits=65'536u){
    using namespace ActorChildCarryDetail;
    if(!source)throw std::invalid_argument("Original child carry requires its nonnull source Actor");
    OriginalActorCarryResult result;
    if(!Call(q,q.standingCount,source))return result;
    if(test){result.flow=OriginalActorCarryFlow::TestBypass;return result;}
    if(!level)throw std::invalid_argument("Original child carry requires its original native Level");
    result.flow=OriginalActorCarryFlow::Scanned;
    for(std::int32_t index=0;;++index){
        const auto count=Call(q,q.actorCount,level);
        if(index>=count)return result; // Signed native comparison, including negative counts.
        if(result.slotVisits>=maximumVisits)throw std::runtime_error("Original child carry live slot budget exceeded");
        ++result.slotVisits;
        const auto child=Call(q,q.actorSlot,level,index);
        if(!child||Call(q,q.base,child)!=source)continue;
        const auto parentYaw=Call(q,q.rotation,source,1u);
        const auto parentPitch=Call(q,q.rotation,source,0u);
        const auto rawYaw=Difference(requested[1u],parentYaw);
        PortableModelVec3 rotational{};
        bool rotate=requested[0u]!=parentPitch||requested[1u]!=parentYaw;
        if(!rotate)rotate=requested[2u]!=Call(q,q.rotation,source,2u);
        if(rotate){
            std::uintptr_t math{};const auto offset=Offset(source,child,math,q);
            if(!math)throw std::invalid_argument("Original child carry has a null active GMath identity");
            const auto unit=Call(q,q.unitCoords,math);
            const auto coords=RotateOriginalModelCoords(unit,{0,QuantizedYaw(requested[1u],parentYaw),0},table);
            rotational=RotationalDelta(offset,coords);
        }
        OriginalActorLineHit hit; // FRESH complete +0/Time1/Item-1 packet per child.
        OriginalActorChildMove move;move.level=level;move.actor=child;
        const auto childRoll=Call(q,q.rotation,child,2u),childYaw=Call(q,q.rotation,child,1u),childPitch=Call(q,q.rotation,child,0u);
        move.rotation={childPitch,Add(childYaw,rawYaw),childRoll};
        move.delta=Translation(actualDelta,rotational);
        static_cast<void>(Call(q,q.moveActor,move,hit));++result.moves;
        if(!Call(q,q.pawn,child))continue; // Retained C, not a fresh slot/Base check.
        const auto yaw=Call(q,q.viewRotation,child,1u),pitch=Call(q,q.viewRotation,child,0u),roll=Call(q,q.viewRotation,child,2u);
        Call(q,q.writeViewRotation,child,0u,pitch);
        Call(q,q.writeViewRotation,child,1u,Add(yaw,rawYaw));
        Call(q,q.writeViewRotation,child,2u,roll);
    }
}
} // namespace QuestVr
