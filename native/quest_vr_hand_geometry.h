#pragma once

#include "quest_actor_geometry.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <map>
#include <numeric>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace QuestVr {

// A bounded self-view derivative, not an original closed mesh, finger rig,
// independent original left hand, arm IK system or runtime player pawn.
enum class VrHandFaceOrigin { OriginalSurface, DerivedSleeveClosure };

struct VrHandGeometryTriangle {
    std::array<ActorTriangleVertex,3> vertices;
    std::array<std::uint32_t,3> sourceVertexIds{};
    std::uint32_t polyFlags{},textureIndex{};
    std::uint16_t sourceMaterial{};
    VrHandFaceOrigin origin{VrHandFaceOrigin::OriginalSurface};
    // The export face index is meaningful only for OriginalSurface. Closure
    // triangles use UINT32_MAX; their vertices still name exact source points.
    std::uint32_t sourceFaceIndex{std::numeric_limits<std::uint32_t>::max()};
};

struct VrHandSourceGeometry {
    std::vector<VrHandGeometryTriangle> triangles;
    ActorVec3 originalPivotObjectUnits;
    std::string provenance;
    bool derivedMirrored{}; // Relative to the decoded original mesh, not a rig.
};

struct VrHandGeometryLimits {
    std::size_t maximumSourceTriangles{512u},maximumTotalTriangles{1024u};
    float maximumAbsoluteCoordinate{1.0f}; // Controller-local meters.
    float maximumClosureDiameter{0.16f},maximumClosurePlanarityError{0.005f};
};

struct VrHandSleeveClosure {
    // Exact independently audited boundary identities. Unknown extra holes,
    // branches, nonmanifold vertices or changed rings are rejected, not capped.
    std::vector<std::uint32_t> boundaryVertexIds;
    float u{},v{}; // Explicit native cuff swatch; never rewrites source UVs.
    std::uint32_t textureIndex{},polyFlags{};
    std::uint16_t sourceMaterial{};
};

struct VrHandGeometry {
    std::vector<VrHandGeometryTriangle> triangles;
    ActorVec3 originalPivotObjectUnits;
    std::string originalProvenance,derivativeProvenance;
    bool derivedMirrored{};
    std::size_t originalTriangleCount{},closureTriangleCount{},sourceVertexCount{};
};

// Axes and origin are expressed in the raw decoded hand's Quest-meter space.
// Applying dot products with this proper orthonormal basis is a rotation, not
// an additional mirror. The left derivative is mirrored only after preparing
// the right grip. The OpenXR right grip has +X into the palm and -Z along the
// curled-finger tube from little finger toward index/thumb.
struct VrHandGripBasis {
    ActorVec3 origin, xAxis, yAxis, zAxis;
    std::string provenance;
};

namespace VrHandGeometryDetail {
using Edge=std::pair<std::uint32_t,std::uint32_t>;
struct EdgeUse {std::uint32_t a{},b{};};
struct Topology {
    std::map<std::uint32_t,ActorVec3> positions;
    std::map<Edge,std::vector<EdgeUse>> edges;
    std::vector<std::uint32_t> boundary;
};
inline ActorVec3 Subtract(ActorVec3 a,ActorVec3 b) {return {a.x-b.x,a.y-b.y,a.z-b.z};}
inline ActorVec3 Cross(ActorVec3 a,ActorVec3 b) {return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
inline float Dot(ActorVec3 a,ActorVec3 b) {return a.x*b.x+a.y*b.y+a.z*b.z;}
inline float DistanceSquared(ActorVec3 a,ActorVec3 b) {const auto d=Subtract(a,b);return Dot(d,d);}
inline void ValidateGripBasis(const VrHandGripBasis& basis) {
    if(!IsFiniteActorVector(basis.origin)||!IsFiniteActorVector(basis.xAxis)||
        !IsFiniteActorVector(basis.yAxis)||!IsFiniteActorVector(basis.zAxis)||basis.provenance.empty()||
        std::fabs(Dot(basis.xAxis,basis.xAxis)-1.0f)>1.0e-4f||
        std::fabs(Dot(basis.yAxis,basis.yAxis)-1.0f)>1.0e-4f||
        std::fabs(Dot(basis.zAxis,basis.zAxis)-1.0f)>1.0e-4f||
        std::fabs(Dot(basis.xAxis,basis.yAxis))>1.0e-4f||
        std::fabs(Dot(basis.xAxis,basis.zAxis))>1.0e-4f||
        std::fabs(Dot(basis.yAxis,basis.zAxis))>1.0e-4f||
        Dot(Cross(basis.xAxis,basis.yAxis),basis.zAxis)<0.9999f)
        throw std::runtime_error("VR hand grip basis is not a finite proper orthonormal frame");
}
inline void ValidateLimits(const VrHandGeometryLimits& limits) {
    if(limits.maximumSourceTriangles==0u||limits.maximumSourceTriangles>4096u||
        limits.maximumTotalTriangles==0u||limits.maximumTotalTriangles>8192u||
        !std::isfinite(limits.maximumAbsoluteCoordinate)||limits.maximumAbsoluteCoordinate<=0.0f||
        limits.maximumAbsoluteCoordinate>2.0f||!std::isfinite(limits.maximumClosureDiameter)||
        limits.maximumClosureDiameter<=0.0f||limits.maximumClosureDiameter>0.25f||
        !std::isfinite(limits.maximumClosurePlanarityError)||limits.maximumClosurePlanarityError<0.0f||
        limits.maximumClosurePlanarityError>0.02f)
        throw std::runtime_error("VR hand geometry limits are invalid");
}
inline Topology Inspect(const std::vector<VrHandGeometryTriangle>& faces,
    const VrHandGeometryLimits& limits,bool requireClosed) {
    if(faces.empty()||faces.size()>limits.maximumTotalTriangles)
        throw std::runtime_error("VR hand face count exceeds its bounded topology budget");
    Topology topology;
    std::set<std::array<std::uint32_t,3>> faceKeys;
    std::map<std::uint32_t,std::vector<Edge>> links;
    for(const auto& face:faces) {
        auto key=face.sourceVertexIds;std::sort(key.begin(),key.end());
        if(key[0]==key[1]||key[1]==key[2]||!faceKeys.insert(key).second)
            throw std::runtime_error("VR hand has repeated source identities or duplicate faces");
        const auto cross=Cross(Subtract(face.vertices[1].position,face.vertices[0].position),
            Subtract(face.vertices[2].position,face.vertices[0].position));
        if(!IsFiniteActorVector(cross)||Dot(cross,cross)<=1.0e-16f)
            throw std::runtime_error("VR hand has a degenerate geometric face");
        for(std::size_t c=0;c<3u;++c) {
            const auto& v=face.vertices[c];
            if(!IsFiniteActorVector(v.position)||!IsFiniteActorVector(v.normal)||
                !std::isfinite(v.u)||!std::isfinite(v.v)||v.u<0.0f||v.u>1.0f||v.v<0.0f||v.v>1.0f||
                std::fabs(v.position.x)>limits.maximumAbsoluteCoordinate||
                std::fabs(v.position.y)>limits.maximumAbsoluteCoordinate||
                std::fabs(v.position.z)>limits.maximumAbsoluteCoordinate)
                throw std::runtime_error("VR hand position, normal or native UV is outside its finite budget");
            const auto id=face.sourceVertexIds[c];
            const auto [at,inserted]=topology.positions.emplace(id,v.position);
            if(!inserted&&DistanceSquared(at->second,v.position)>1.0e-12f)
                throw std::runtime_error("VR hand source identity disagrees across a UV seam");
            const auto b=face.sourceVertexIds[(c+1u)%3u];
            topology.edges[std::minmax(id,b)].push_back({id,b});
            links[id].push_back(std::minmax(b,face.sourceVertexIds[(c+2u)%3u]));
        }
    }
    std::map<std::uint32_t,std::uint32_t> next;
    std::map<std::uint32_t,std::size_t> incoming;
    for(const auto& [key,uses]:topology.edges) {
        (void)key;
        if(uses.size()==1u) {
            if(requireClosed) throw std::runtime_error("VR hand retains an open boundary");
            if(!next.emplace(uses[0].a,uses[0].b).second||++incoming[uses[0].b]!=1u)
                throw std::runtime_error("VR hand boundary is branched or pinched");
        } else if(uses.size()!=2u||uses[0].a!=uses[1].b||uses[0].b!=uses[1].a)
            throw std::runtime_error("VR hand has a nonmanifold or inconsistently oriented edge");
    }
    if(!next.empty()) {
        for(const auto& [id,following]:next) {(void)following;if(incoming[id]!=1u)
            throw std::runtime_error("VR hand boundary is not an oriented simple loop");}
        const auto first=next.begin()->first;auto current=first;
        do {
            topology.boundary.push_back(current);current=next.at(current);
            if(topology.boundary.size()>next.size()) throw std::runtime_error("VR hand boundary cannot close");
        } while(current!=first);
        if(topology.boundary.size()!=next.size())
            throw std::runtime_error("VR hand has unapproved additional boundary loops");
    }
    // A closed edge count alone permits bow-tie vertices. Each closed vertex's
    // triangle link must independently be one connected cycle.
    if(requireClosed) for(const auto& [id,segments]:links) {
        (void)id;std::map<std::uint32_t,std::vector<std::uint32_t>> graph;
        for(const auto& [a,b]:segments) {graph[a].push_back(b);graph[b].push_back(a);}
        for(const auto& [v,adjacent]:graph) {(void)v;if(adjacent.size()!=2u)
            throw std::runtime_error("VR hand closed vertex link is not manifold");}
        std::set<std::uint32_t> visited;std::vector<std::uint32_t> pending{graph.begin()->first};
        while(!pending.empty()) {const auto v=pending.back();pending.pop_back();if(!visited.insert(v).second) continue;
            for(const auto other:graph.at(v)) pending.push_back(other);}
        if(visited.size()!=graph.size()) throw std::runtime_error("VR hand closed vertex has disconnected fans");
    }
    // All original hand/sleeve surfaces must belong to one connected component.
    std::map<std::uint32_t,std::vector<std::uint32_t>> graph;
    for(const auto& [edge,uses]:topology.edges) {(void)uses;graph[edge.first].push_back(edge.second);graph[edge.second].push_back(edge.first);}
    std::set<std::uint32_t> visited;std::vector<std::uint32_t> pending{graph.begin()->first};
    while(!pending.empty()) {const auto v=pending.back();pending.pop_back();if(!visited.insert(v).second) continue;
        for(const auto other:graph.at(v)) pending.push_back(other);}
    if(visited.size()!=topology.positions.size()) throw std::runtime_error("VR hand has disconnected source components");
    return topology;
}
struct Point2 {double x{},y{};};
inline double Area(Point2 a,Point2 b,Point2 c) {return (b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x);}
inline bool WithinTriangle(Point2 p,Point2 a,Point2 b,Point2 c) {
    constexpr double epsilon=1.0e-12;
    return Area(a,b,p)>=-epsilon&&Area(b,c,p)>=-epsilon&&Area(c,a,p)>=-epsilon;
}
inline std::vector<std::array<std::uint32_t,3>> TriangulateClosure(
    const Topology& topology,const VrHandGeometryLimits& limits) {
    // Reverse the original boundary: every cap edge must oppose its source use.
    auto ring=topology.boundary;std::reverse(ring.begin(),ring.end());
    if(ring.size()<3u||ring.size()>16u) throw std::runtime_error("VR hand closure ring size is not bounded");
    ActorVec3 center{},normal{};
    for(std::size_t i=0;i<ring.size();++i) {
        const auto a=topology.positions.at(ring[i]),b=topology.positions.at(ring[(i+1u)%ring.size()]);
        center.x+=a.x;center.y+=a.y;center.z+=a.z;
        normal.x+=(a.y-b.y)*(a.z+b.z);normal.y+=(a.z-b.z)*(a.x+b.x);normal.z+=(a.x-b.x)*(a.y+b.y);
        for(const auto other:ring) if(DistanceSquared(a,topology.positions.at(other))>
            limits.maximumClosureDiameter*limits.maximumClosureDiameter)
            throw std::runtime_error("VR hand closure exceeds the audited cuff diameter");
    }
    const auto n=NormalizeActorVector(normal);
    if(Dot(n,n)<0.5f) throw std::runtime_error("VR hand closure plane is degenerate");
    center.x/=static_cast<float>(ring.size());center.y/=static_cast<float>(ring.size());center.z/=static_cast<float>(ring.size());
    const auto tangent=NormalizeActorVector(Subtract(topology.positions.at(ring[0]),center));
    const auto bitangent=NormalizeActorVector(Cross(n,tangent));
    std::map<std::uint32_t,Point2> points;
    for(const auto id:ring) {
        const auto d=Subtract(topology.positions.at(id),center);
        if(std::fabs(Dot(d,n))>limits.maximumClosurePlanarityError)
            throw std::runtime_error("VR hand closure is not the audited near-planar sleeve rim");
        points[id]={Dot(d,tangent),Dot(d,bitangent)};
    }
    // The audited cuff is convex. Explicitly reject folds/concavity rather than
    // inventing a triangulation for an unknown palm/finger opening.
    for(std::size_t i=0;i<ring.size();++i)
        if(Area(points.at(ring[i]),points.at(ring[(i+1u)%ring.size()]),points.at(ring[(i+2u)%ring.size()]))<=1.0e-12)
            throw std::runtime_error("VR hand closure is not a strictly convex audited cuff");
    std::vector<std::array<std::uint32_t,3>> result;
    while(ring.size()>3u) {
        std::size_t best=ring.size();double bestQuality=-1.0;
        for(std::size_t i=0;i<ring.size();++i) {
            const auto a=ring[(i+ring.size()-1u)%ring.size()],b=ring[i],c=ring[(i+1u)%ring.size()];
            bool contains{};
            for(const auto other:ring) if(other!=a&&other!=b&&other!=c&&
                WithinTriangle(points.at(other),points.at(a),points.at(b),points.at(c))) {contains=true;break;}
            if(contains) continue;
            const auto pa=topology.positions.at(a),pb=topology.positions.at(b),pc=topology.positions.at(c);
            const double perimeterSquared=DistanceSquared(pa,pb)+DistanceSquared(pb,pc)+DistanceSquared(pc,pa);
            const double quality=Area(points.at(a),points.at(b),points.at(c))/perimeterSquared;
            if(quality>bestQuality) {bestQuality=quality;best=i;}
        }
        if(best==ring.size()) throw std::runtime_error("VR hand sleeve closure has no valid convex ear");
        result.push_back({ring[(best+ring.size()-1u)%ring.size()],ring[best],ring[(best+1u)%ring.size()]});
        ring.erase(ring.begin()+static_cast<std::ptrdiff_t>(best));
    }
    result.push_back({ring[0],ring[1],ring[2]});return result;
}
} // namespace VrHandGeometryDetail

inline VrHandGeometry BuildClosedVrHandGeometry(const VrHandSourceGeometry& source,
    const VrHandSleeveClosure& closure,const VrHandGeometryLimits& limits={}) {
    using namespace VrHandGeometryDetail;
    ValidateLimits(limits);
    if(source.triangles.empty()||source.triangles.size()>limits.maximumSourceTriangles||
        !IsFiniteActorVector(source.originalPivotObjectUnits)||source.provenance.empty())
        throw std::runtime_error("VR hand original source/provenance exceeds its budget");
    for(const auto& triangle:source.triangles) if(triangle.origin!=VrHandFaceOrigin::OriginalSurface)
        throw std::runtime_error("VR hand source must distinguish original faces from derivatives");
    if(!std::isfinite(closure.u)||!std::isfinite(closure.v)||closure.u<0.0f||closure.u>1.0f||closure.v<0.0f||closure.v>1.0f)
        throw std::runtime_error("VR hand sleeve closure UV is not a native texture coordinate");
    const auto topology=Inspect(source.triangles,limits,false);
    const std::set<std::uint32_t> actual(topology.boundary.begin(),topology.boundary.end());
    const std::set<std::uint32_t> expected(closure.boundaryVertexIds.begin(),closure.boundaryVertexIds.end());
    if(actual.empty()||actual!=expected||expected.size()!=closure.boundaryVertexIds.size())
        throw std::runtime_error("VR hand boundary does not match its explicit audited sleeve identities");
    const auto caps=TriangulateClosure(topology,limits);
    if(source.triangles.size()+caps.size()>limits.maximumTotalTriangles)
        throw std::runtime_error("VR hand source plus sleeve closure exceeds its face budget");
    VrHandGeometry result;result.triangles=source.triangles;
    result.originalPivotObjectUnits=source.originalPivotObjectUnits;result.originalProvenance=source.provenance;
    result.derivedMirrored=source.derivedMirrored;
    result.derivativeProvenance="Bounded VR derivative: original surfaces retained; audited rear sleeve rim closed by convex-ear triangles with an original cuff texture swatch; no finger/arm rig";
    result.originalTriangleCount=source.triangles.size();result.closureTriangleCount=caps.size();result.sourceVertexCount=topology.positions.size();
    for(const auto& ids:caps) {
        VrHandGeometryTriangle triangle;triangle.sourceVertexIds=ids;
        triangle.origin=VrHandFaceOrigin::DerivedSleeveClosure;triangle.polyFlags=closure.polyFlags;
        triangle.textureIndex=closure.textureIndex;triangle.sourceMaterial=closure.sourceMaterial;
        const auto normal=NormalizeActorVector(Cross(Subtract(topology.positions.at(ids[1]),topology.positions.at(ids[0])),
            Subtract(topology.positions.at(ids[2]),topology.positions.at(ids[0]))));
        for(std::size_t c=0;c<3u;++c) triangle.vertices[c]={topology.positions.at(ids[c]),normal,closure.u,closure.v};
        result.triangles.push_back(triangle);
    }
    Inspect(result.triangles,limits,true);return result;
}

inline VrHandGeometry MirrorClosedVrHandGeometry(const VrHandGeometry& right,
    const VrHandGeometryLimits& limits={}) {
    VrHandGeometryDetail::ValidateLimits(limits);
    VrHandGeometryDetail::Inspect(right.triangles,limits,true);
    auto left=right;left.derivedMirrored=!right.derivedMirrored;
    left.derivativeProvenance="Quest-X reflection of the prepared bounded VR hand; reflected normals and reversed winding; mirror parity toggled relative to decoded source, not an independent authored hand asset. "+right.derivativeProvenance;
    for(auto& face:left.triangles) {
        for(auto& v:face.vertices) {v.position.x=-v.position.x;v.normal.x=-v.normal.x;}
        std::swap(face.vertices[1],face.vertices[2]);std::swap(face.sourceVertexIds[1],face.sourceVertexIds[2]);
    }
    VrHandGeometryDetail::Inspect(left.triangles,limits,true);return left;
}

inline VrHandSourceGeometry PrepareVrHandSourceInGripSpace(const VrHandSourceGeometry& source,
    const VrHandGripBasis& basis,const VrHandGeometryLimits& limits={}) {
    using namespace VrHandGeometryDetail;
    ValidateLimits(limits);ValidateGripBasis(basis);Inspect(source.triangles,limits,false);
    auto result=source;
    const auto rotate=[&](ActorVec3 p) {return ActorVec3{Dot(p,basis.xAxis),Dot(p,basis.yAxis),Dot(p,basis.zAxis)};};
    for(auto& face:result.triangles) for(auto& vertex:face.vertices) {
        vertex.position=rotate(Subtract(vertex.position,basis.origin));
        vertex.normal=NormalizeActorVector(rotate(vertex.normal));
    }
    result.provenance+="; "+basis.provenance;
    Inspect(result.triangles,limits,false);return result;
}

inline VrHandSourceGeometry MirrorVrHandSourceAcrossQuestX(const VrHandSourceGeometry& source,
    const VrHandGeometryLimits& limits={}) {
    using namespace VrHandGeometryDetail;
    ValidateLimits(limits);Inspect(source.triangles,limits,false);
    auto result=source;result.derivedMirrored=!source.derivedMirrored;
    result.provenance+="; explicit Quest-X source handedness reflection, source IDs/UV corners and outward normals reflected with reversed winding";
    for(auto& face:result.triangles) {
        for(auto& vertex:face.vertices) {vertex.position.x=-vertex.position.x;vertex.normal.x=-vertex.normal.x;}
        std::swap(face.vertices[1],face.vertices[2]);std::swap(face.sourceVertexIds[1],face.sourceVertexIds[2]);
    }
    Inspect(result.triangles,limits,false);return result;
}

// Decode the user's original NanoKeyRingPOV Still hand only. Neither the key
// ring nor any commercial mesh coordinates/pixels are embedded in this code.
// Source IDs follow BuildActorTriangle's actual reflected corner order, rather
// than assuming its output has the same winding as the serialized triangles.
inline VrHandSourceGeometry BuildOriginalNanoKeyRingHandSource(const PortableLodMesh& mesh,
    std::uint32_t textureIndex,const VrHandGeometryLimits& limits={}) {
    using namespace VrHandGeometryDetail;
    ValidateLimits(limits);
    if(!mesh.animation||mesh.triangles.size()%3u!=0u||mesh.triangles.size()/3u>limits.maximumTotalTriangles||
        mesh.animation->triangleSourceVertexIndices.size()!=mesh.triangles.size()||
        !FindMeshAnimationSequence(*mesh.animation,"Still",false))
        throw std::runtime_error("Original NanoKeyRingPOV hand lacks its bounded Still topology");
    MeshAnimationState state;state.main.sequence="Still";
    const auto pose=PrepareMeshPose(mesh,state);
    if(!pose.drawable||pose.fallbackUsed||!MeshAnimationNamesEqual(pose.resolvedSequence,"Still"))
        throw std::runtime_error("Original NanoKeyRingPOV Still pose is unavailable");
    auto transform=BuildActorToQuest({},{},0,0,0,1.0f,{1.0f,1.0f,1.0f},{});
    transform.translation.y-=1.0f;
    const bool reversed=transform.mirrored!=(mesh.scaleX*mesh.scaleY*mesh.scaleZ<0.0f);
    const std::size_t order[3]{0u,reversed?2u:1u,reversed?1u:2u};
    VrHandSourceGeometry result;
    result.provenance="Original DeusExItems.NanoKeyRingPOV Still: only its authored right-hand WeaponHandsTex hand and connected sleeve surfaces, native UVs/material/flags retained; right-handedness checked against OpenXR grip axes and recorded neutral-controller poses, not inferred from a thumbnail view alone";
    for(std::size_t first=0u;first<mesh.triangles.size();first+=3u) {
        const auto& vertex=mesh.triangles[first];
        const auto material=ResolveActorMeshMaterial({},mesh.texturePaths,mesh.materialTextureIndices,vertex.material);
        if(material.texturePath!="DeusExItems.Skins.WeaponHandsTex") continue;
        if(vertex.material!=0u||result.triangles.size()>=limits.maximumSourceTriangles)
            throw std::runtime_error("Original NanoKeyRingPOV hand material/face budget differs from its audit");
        VrHandGeometryTriangle triangle;
        triangle.vertices=BuildActorTriangle(mesh,first,transform,&pose);
        triangle.polyFlags=vertex.polyFlags;triangle.sourceMaterial=vertex.material;
        triangle.textureIndex=textureIndex;triangle.sourceFaceIndex=static_cast<std::uint32_t>(first/3u);
        for(std::size_t corner=0u;corner<3u;++corner)
            triangle.sourceVertexIds[corner]=mesh.animation->triangleSourceVertexIndices.at(first+order[corner]);
        result.triangles.push_back(triangle);
    }
    const auto topology=Inspect(result.triangles,limits,false);
    const std::set<std::uint32_t> rim(topology.boundary.begin(),topology.boundary.end());
    if(result.triangles.size()!=152u||topology.positions.size()!=80u||rim!=std::set<std::uint32_t>{1u,3u,38u,2u,63u,25u})
        throw std::runtime_error("Original NanoKeyRingPOV hand topology differs from its 152-face/80-vertex/six-rim audit");
    // Texture-region identities independently distinguish the palm/back
    // landmarks from the long cuff; no sleeve vertex participates in the pivot.
    const auto region=[&](std::uint32_t id,float minU,float maxU,float minV,float maxV) {
        for(const auto& face:result.triangles) for(std::size_t c=0u;c<3u;++c) {
            const auto& v=face.vertices[c];
            if(face.sourceVertexIds[c]==id&&v.u>=minU&&v.u<=maxU&&v.v>=minV&&v.v<=maxV) return true;
        }
        return false;
    };
    for(const auto id:{77u,78u,9u,11u}) if(!region(id,0.15f,0.35f,0.25f,0.46f))
        throw std::runtime_error("Original NanoKeyRingPOV central palm texture landmarks differ from the audit");
    for(const auto id:{109u,68u,13u,17u}) if(!region(id,0.45f,0.75f,0.30f,0.48f))
        throw std::runtime_error("Original NanoKeyRingPOV central back texture landmarks differ from the audit");
    ActorVec3 pivot{};
    for(const auto id:{77u,78u,9u,11u,109u,68u,13u,17u}) {
        const auto p=pose.objectPositions.at(id);pivot.x+=p.x/8.0f;pivot.y+=p.y/8.0f;pivot.z+=p.z/8.0f;
    }
    result.originalPivotObjectUnits=pivot;return result;
}

inline VrHandGripBasis BuildOriginalNanoKeyRingGripBasis(const VrHandSourceGeometry& source,
    const VrHandGeometryLimits& limits={}) {
    using namespace VrHandGeometryDetail;
    ValidateLimits(limits);
    const auto topology=Inspect(source.triangles,limits,false);
    const auto mean=[&](std::initializer_list<std::uint32_t> ids) {
        ActorVec3 result{};
        for(const auto id:ids) {
            const auto p=topology.positions.at(id);const auto divisor=static_cast<float>(ids.size());
            result.x+=p.x/divisor;result.y+=p.y/divisor;result.z+=p.z/divisor;
        }
        return result;
    };
    const auto palm=mean({77u,78u,9u,11u}),back=mean({109u,68u,13u,17u});
    const auto little=mean({48u,96u,15u}),index=mean({20u,64u,30u});
    const auto intoPalm=Subtract(back,palm),towardIndex=Subtract(index,little);
    if(Dot(intoPalm,intoPalm)<0.0001f||Dot(towardIndex,towardIndex)<0.0001f)
        throw std::runtime_error("Original NanoKeyRingPOV grip anatomical landmarks are collapsed");
    VrHandGripBasis basis;
    basis.origin={(palm.x+back.x)*0.5f,(palm.y+back.y)*0.5f,(palm.z+back.z)*0.5f};
    const auto curl=NormalizeActorVector(towardIndex);
    basis.zAxis={-curl.x,-curl.y,-curl.z};
    const auto along=Dot(intoPalm,basis.zAxis);
    basis.xAxis=NormalizeActorVector({intoPalm.x-along*basis.zAxis.x,
        intoPalm.y-along*basis.zAxis.y,intoPalm.z-along*basis.zAxis.z});
    basis.yAxis=NormalizeActorVector(Cross(basis.zAxis,basis.xAxis));
    // In a neutral right grip +X goes into the palm, -Z goes little-to-index,
    // and therefore +Y points toward the wrist/wearer. The original right hand
    // already has this chirality; mirroring it before making a proper basis
    // reverses the finger/wrist direction. Recorded Quest neutral-grip axes
    // independently verify +Y toward the wearer. This is an asset-specific
    // regression guard, not a general handedness inference rule.
    const auto cuff=mean({1u,3u,38u,2u,63u,25u});
    if(source.derivedMirrored||Dot(Subtract(cuff,basis.origin),basis.yAxis)<0.15f)
        throw std::runtime_error("Original NanoKeyRingPOV right grip requires original chirality with cuff toward +Y/wearer");
    basis.provenance="Audited OpenXR original right-grip derivative: origin is midpoint of central palm IDs77/78/9/11 and back IDs109/68/13/17; +X points palm-to-back (into palm); -Z follows little IDs48/96/15 toward index IDs20/64/30; orthogonal +Y is Z cross X toward wrist/wearer, checked against recorded Quest neutral-grip poses; no extra handedness reflection or legacy Glock grip rotation";
    ValidateGripBasis(basis);return basis;
}

inline VrHandGeometry BuildOriginalNanoKeyRingVrHand(const PortableLodMesh& mesh,
    std::uint32_t textureIndex,const VrHandGeometryLimits& limits={}) {
    const auto raw=BuildOriginalNanoKeyRingHandSource(mesh,textureIndex,limits);
    const auto basis=BuildOriginalNanoKeyRingGripBasis(raw,limits);
    const auto source=PrepareVrHandSourceInGripSpace(raw,basis,limits);
    VrHandSleeveClosure closure;
    closure.boundaryVertexIds={1u,3u,38u,2u,63u,25u};closure.textureIndex=textureIndex;
    bool found{};
    // Reuse an exact original rear-cuff UV, not an invented skin/black color.
    for(const auto& face:source.triangles) for(std::size_t c=0u;c<3u;++c)
        if(!found&&face.sourceVertexIds[c]==38u&&face.vertices[c].v>0.95f) {
            closure.u=face.vertices[c].u;closure.v=face.vertices[c].v;
            closure.polyFlags=face.polyFlags;closure.sourceMaterial=face.sourceMaterial;found=true;
        }
    if(!found) throw std::runtime_error("Original NanoKeyRingPOV rear cuff lacks its audited native texture swatch");
    auto result=BuildClosedVrHandGeometry(source,closure,limits);
    if(result.originalTriangleCount!=152u||result.closureTriangleCount!=4u||result.sourceVertexCount!=80u)
        throw std::runtime_error("Original NanoKeyRingPOV closed grip differs from its bounded derivative audit");
    return result;
}

} // namespace QuestVr
