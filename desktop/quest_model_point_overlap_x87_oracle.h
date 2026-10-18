#pragma once

#include "quest_model_owner_trace_oracle.h"

#include <optional>

// Independent TEST-ONLY occupancy oracle. Shared helpers below are the
// earlier test-only hardware-x87 scalar/coordinate/bevel transcription, not
// production dyadic/trace/occupancy helpers. Occupancy traversal, original
// candidate writes and all comparison/spill boundaries are separate here.
namespace QuestVr::ModelPointOverlapX87Oracle {
using namespace ModelOwnerTraceOracle;
using namespace ModelOwnerTraceOracle::Detail;
struct Packet {
    std::uintptr_t next{},actor{},primitive{};
    Vec location{},normal{};
    float time{};
    std::int32_t item{};
};
struct Result {
    Packet packet;
    bool clear{};
    std::size_t nodes{},leaves{},hulls{},planes{},pairs{},bevels{},candidates{};
    std::array<std::size_t,3u> bevelAxes{};
};
inline Coords Identity() {return {{},{1,0,0},{0,1,0},{0,0,1}};}
inline Real Radius(Vec extent,Vec normal) {
    return Add(Add(Abs(Mul(extent.z,normal.z)),Abs(Mul(extent.y,normal.y))),Abs(Mul(extent.x,normal.x)));
}
inline Real PlaneDistance(Vec point,Plane plane) {return Sub(DotZYX(point,plane.normal),plane.w);}
inline bool ZeroExtent(Vec extent) {
    const auto equal=[](float value){return value==0.0f||std::isnan(value);};
    return equal(extent.x)&&equal(extent.y)&&equal(extent.z);
}
inline Real DotYXZ(Vec a,Vec b) {return Add(Add(Mul(a.y,b.y),Mul(a.x,b.x)),Mul(a.z,b.z));}
inline Real ZeroDistance(Vec point,Plane plane,const Coords& coords) {
    const float nz=Store(DotYXZ(plane.normal,coords.zAxis)),
        ny=Store(DotYXZ(plane.normal,coords.yAxis)),nx=Store(DotYXZ(plane.normal,coords.xAxis));
    const Vec normal{nx,ny,nz};const auto origin=VectorByCoords(coords.origin,coords);
    const auto dot=DotZYX(origin,normal);
    // Original non-popping Float FST is dead scratch. Its overflow/inexact
    // status is masked; retained PC64 dot, not a finite Float reload, is used.
    return Sub(DotZYX(point,normal),Sub(plane.w,dot));
}
inline std::array<Plane,6u> Bounds(Vec minimum,Vec maximum) {
    Finite(minimum);Finite(maximum);
    if(minimum.x>maximum.x||minimum.y>maximum.y||minimum.z>maximum.z) Fail("Occupancy oracle reversed bounds");
    constexpr double margin=std::bit_cast<double>(std::uint64_t{0x3fb999999999999aull});
    return {{{{0,0,-1},Store(Sub(margin,minimum.z))},{{0,0,1},Store(Add(maximum.z,margin))},
        {{-1,0,0},Store(Sub(margin,minimum.x))},{{1,0,0},Store(Sub(maximum.x,margin))},
        {{0,-1,0},Store(Sub(margin,minimum.y))},{{0,1,0},Store(Sub(maximum.y,margin))}}};
}
inline Result Check(Packet seed,const PortableModelGeometry& model,std::uintptr_t modelIdentity,
    std::uintptr_t ownerIdentity,Vec point,Vec extent,std::uint32_t flags,
    const Coords& coords=Identity()) {
    Precision precision;Result result;result.packet=seed;
    auto& hit=result.packet;
    hit.primitive=modelIdentity;hit.actor=ownerIdentity;hit.time=0.0f;hit.normal={};hit.location=point;
    if(model.nodes.empty()) {result.clear=model.rootOutside!=0;return result;}
    Finite(point);
    if(ZeroExtent(extent)) {
        auto index=std::int32_t{0};bool outside=model.rootOutside!=0;
        for(std::size_t work=0u;;++work) {
            if(work>=1'000'000u) Fail("Occupancy oracle zero traversal exhausted");
            ++result.nodes;const auto& node=model.nodes.at(static_cast<std::size_t>(index));
            const bool front=ZeroDistance(point,Authored(node),ownerIdentity?coords:Identity())>0.0L;
            const bool csg=node.vertexCount!=0u&&(node.nodeFlags&0x21u)==0u;
            outside=front?(outside||csg):(outside&&!csg);
            const auto child=front?node.front:node.back;
            if(child==-1) {hit.item=2*index+(front?1:0);++result.leaves;result.clear=outside;return result;}
            index=child;
        }
    }
    Finite(extent);
    float bestDistance=100000.0f;
    const auto update=[&](Plane plane,std::int32_t item,Real distance,Real radius,bool bevel) {
        bestDistance=Store(distance);++result.candidates;
        const float scale=Store(Sub(radius,distance));
        constexpr float amount=std::bit_cast<float>(std::uint32_t{0x3f828f5c});
        const auto normal=plane.normal;
        const auto scaledX=Mul(normal.x,amount);
        const float scaledY=Store(Mul(normal.y,amount)),scaledZ=Store(Mul(normal.z,amount));
        const auto retainedX=Mul(bevel?scaledX:static_cast<Real>(Store(scaledX)),scale);
        const auto xOffset=bevel?retainedX:static_cast<Real>(Store(retainedX));
        const float yOffset=Store(Mul(scaledY,scale)),zOffset=Store(Mul(scaledZ,scale));
        hit.location={Store(Add(xOffset,point.x)),Store(Add(yOffset,point.y)),Store(Add(zOffset,point.z))};
        hit.normal=normal;hit.actor=ownerIdentity;hit.primitive=modelIdentity;hit.time=0;hit.item=item;
    };
    const auto primary=[&](Plane plane,std::int32_t item) {
        ++result.planes;
        const float radius=Store(Radius(extent,plane.normal));
        const auto retained=PlaneDistance(point,plane);
        const float distance=Store(retained);
        if(retained>0.0L&&distance<bestDistance) {
            if(distance>=radius) return false;
            update(plane,item,distance,radius,false);
        }
        return distance<radius;
    };
    const auto bevelTest=[&](Plane plane) {
        ++result.planes;++result.bevels;
        const auto radius=Radius(extent,plane.normal),distance=PlaneDistance(point,plane);
        if(distance>0.0L&&distance<bestDistance) {
            if(distance>=radius) return false;
            update(plane,-1,distance,radius,true);
        }
        return distance<radius;
    };
    std::function<bool(std::int32_t,std::int32_t,bool,std::size_t)> visit;
    visit=[&](std::int32_t index,std::int32_t parent,bool outside,std::size_t depth) {
        if(depth>512u||result.nodes+result.leaves>1'000'000u) Fail("Occupancy oracle recursion guard exhausted");
        if(index!=-1) {
            ++result.nodes;const auto& node=model.nodes.at(static_cast<std::size_t>(index));
            auto plane=Authored(node);if(ownerIdentity) plane=TraversalPlane(plane,coords);
            const Vec padded{Store(Mul(extent.x,1.1f)),Store(Mul(extent.y,1.1f)),Store(Mul(extent.z,1.1f))};
            const float radius=Store(Radius(padded,plane.normal));
            const float distance=Store(Sub(DotYXZ(point,plane.normal),plane.w));
            const bool csg=node.vertexCount!=0u&&(node.nodeFlags&static_cast<std::uint8_t>(flags|0x21u))==0u;
            bool clear=true;
            if(distance> -radius) clear=visit(node.front,index,outside||csg,depth+1u);
            if(distance<=radius) {const bool back=visit(node.back,index,outside&&!csg,depth+1u);clear=back&&clear;}
            return clear;
        }
        ++result.leaves;if(outside) return true;
        const auto offset=model.nodes.at(static_cast<std::size_t>(parent)).collisionBound;
        if(offset==-1) return true;
        ++result.hulls;std::size_t work{};
        const auto hull=ReadHull(model,offset,work,16'000'000u);
        std::vector<Plane> planes;planes.reserve(hull.references.size());
        for(const auto raw:hull.references) {
            const auto bits=static_cast<std::uint32_t>(raw);
            auto plane=Authored(model.nodes.at(bits&~0x40000000u));
            if(ownerIdentity) plane=HullPlane(plane,coords);
            if(bits&0x40000000u) plane=Flip(plane);
            planes.push_back(plane);
        }
        for(std::size_t i=0u;i<planes.size();++i)
            if(!primary(planes[i],static_cast<std::int32_t>(static_cast<std::uint32_t>(hull.references[i])&~0x40000000u))) return true;
        if(!ownerIdentity) for(const auto plane:Bounds(hull.minimum,hull.maximum)) if(!primary(plane,-1)) return true;
        for(std::size_t i=0u;i<planes.size();++i) for(std::size_t j=0u;j<i;++j) {
            ++result.pairs;
            const auto a=Components(planes[i].normal),b=Components(planes[j].normal);
            for(unsigned axis=0u;axis<3u;++axis) {
                if(!((a[axis]<0&&b[axis]>0)||(a[axis]>0&&b[axis]<0))) continue;
                const Vec basis{axis==0u?1.0f:0.0f,axis==1u?1.0f:0.0f,axis==2u?1.0f:0.0f};
                if(DotZYX(Cross(basis,planes[i].normal),Cross(basis,planes[j].normal))<=
                    std::bit_cast<double>(std::uint64_t{0x3f50624dd2f1a9fcull})) continue;
                ++result.bevelAxes[axis];
                if(!bevelTest(Bevel(planes[i],planes[j],basis))) return true;
            }
        }
        return false;
    };
    result.clear=visit(0,0,model.rootOutside!=0,1u);return result;
}
} // namespace QuestVr::ModelPointOverlapX87Oracle
