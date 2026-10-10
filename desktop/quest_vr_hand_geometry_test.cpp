#include "Precomp.h"
#include "quest_player_visual.h"
#include "quest_vr_hand_geometry.h"
#include "visual_renderer.h"

#include <iostream>
#include <functional>
#include <map>
#include <set>

namespace {
std::size_t checks{},rejections{};
void Check(bool passed,const char* label) {
    ++checks;if(!passed) throw std::runtime_error(std::string("VR hand check failed: ")+label);
}
void Reject(const std::function<void()>& action,const char* label) {
    try {action();} catch(const std::exception&) {++checks;++rejections;return;}
    throw std::runtime_error(std::string("VR hand did not reject: ")+label);
}
bool Near(float a,float b,float epsilon=1.0e-6f) {return std::fabs(a-b)<=epsilon;}
bool Near(QuestVr::ActorVec3 a,QuestVr::ActorVec3 b,float epsilon=1.0e-6f) {
    return Near(a.x,b.x,epsilon)&&Near(a.y,b.y,epsilon)&&Near(a.z,b.z,epsilon);
}
struct CapturedNeutralGrip {
    const char* name;
    std::array<float,4> quaternion; // Head-local xyzw, not model coordinates.
    QuestVr::ActorVec3 observedY;
};
// Actual Quest screenshot telemetry, 2026-10-10 21:38:09.234: controllers held
// forward and aim rays away from wearer. Positive head Z points to the wearer.
// These poses independently expose the erroneous source reflection that the
// earlier asset-only anatomy assertions failed to catch.
constexpr CapturedNeutralGrip NeutralGripCapture[]{
    {"right",{.49185f,.28962f,.21244f,.79314f},{-.0521f,.4259f,.9033f}},
    {"left",{-.54664f,.40446f,.21475f,-.70106f},{-.1411f,.3101f,.9402f}}
};
QuestVr::ActorVec3 RotateCapturedGrip(const CapturedNeutralGrip& capture,QuestVr::ActorVec3 p) {
    using namespace QuestVr::VrHandGeometryDetail;
    const auto& q=capture.quaternion;
    const float inverse=1.0f/std::sqrt(q[0]*q[0]+q[1]*q[1]+q[2]*q[2]+q[3]*q[3]);
    const QuestVr::ActorVec3 u{q[0]*inverse,q[1]*inverse,q[2]*inverse};
    const float w=q[3]*inverse;const auto uv=Cross(u,p),uuv=Cross(u,uv);
    return {p.x+2.0f*(w*uv.x+uuv.x),p.y+2.0f*(w*uv.y+uuv.y),p.z+2.0f*(w*uv.z+uuv.z)};
}
QuestVr::VrHandSourceGeometry SyntheticSource() {
    QuestVr::VrHandSourceGeometry source;source.provenance="Synthetic six-rim hand topology fixture";
    std::array<QuestVr::ActorVec3,7> points{};
    for(std::size_t i=0u;i<6u;++i) {
        const float angle=static_cast<float>(i)*3.14159265359f/3.0f;
        points[i]={0.04f*std::cos(angle),0.04f*std::sin(angle),0.0f};
    }
    points[6]={0.0f,0.0f,0.06f};
    for(std::uint32_t i=0u;i<6u;++i) {
        QuestVr::VrHandGeometryTriangle face;face.sourceVertexIds={i,(i+1u)%6u,6u};
        face.sourceFaceIndex=i;face.polyFlags=0x100u;face.textureIndex=7u;face.sourceMaterial=3u;
        const auto normal=QuestVr::NormalizeActorVector(QuestVr::VrHandGeometryDetail::Cross(
            QuestVr::VrHandGeometryDetail::Subtract(points[(i+1u)%6u],points[i]),
            QuestVr::VrHandGeometryDetail::Subtract(points[6],points[i])));
        for(std::size_t c=0u;c<3u;++c)
            face.vertices[c]={points[face.sourceVertexIds[c]],normal,0.1f+0.1f*static_cast<float>(i),0.2f+0.1f*static_cast<float>(c)};
        source.triangles.push_back(face);
    }
    return source;
}
QuestVr::VrHandSleeveClosure SyntheticClosure() {
    QuestVr::VrHandSleeveClosure closure;closure.boundaryVertexIds={0u,1u,2u,3u,4u,5u};
    closure.u=0.4f;closure.v=0.8f;closure.textureIndex=7u;closure.polyFlags=0u;closure.sourceMaterial=3u;
    return closure;
}
void SyntheticTests() {
    using namespace QuestVr;
    const auto source=SyntheticSource();const auto closure=SyntheticClosure();
    for(const auto& capture:NeutralGripCapture) {
        const auto y=RotateCapturedGrip(capture,{0,1,0});
        Check(Near(y,capture.observedY,0.0002f)&&y.z>.8f,"actual neutral Quest grip +Y points toward wearer");
        Check(RotateCapturedGrip(capture,{0,.1f,0}).z>.08f&&RotateCapturedGrip(capture,{0,-.05f,0}).z<-.04f,
            "neutral grip synthetic cuff +Y toward wearer and finger -Y away");
    }
    const auto right=BuildClosedVrHandGeometry(source,closure);
    Check(right.triangles.size()==10u&&right.originalTriangleCount==6u&&right.closureTriangleCount==4u&&right.sourceVertexCount==7u,
        "six-rim closure makes four triangles without new vertices");
    const auto topology=VrHandGeometryDetail::Inspect(right.triangles,{},true);
    Check(topology.boundary.empty()&&topology.edges.size()==15u&&topology.positions.size()==7u,"closed sphere Euler topology");
    for(std::size_t f=0u;f<source.triangles.size();++f) {
        const auto& before=source.triangles[f];const auto& after=right.triangles[f];
        Check(after.origin==VrHandFaceOrigin::OriginalSurface&&after.sourceVertexIds==before.sourceVertexIds&&
            after.sourceFaceIndex==before.sourceFaceIndex&&after.polyFlags==before.polyFlags&&
            after.textureIndex==before.textureIndex&&after.sourceMaterial==before.sourceMaterial,"source face metadata unchanged");
        for(std::size_t c=0u;c<3u;++c) {
            Check(Near(after.vertices[c].position,before.vertices[c].position)&&Near(after.vertices[c].normal,before.vertices[c].normal)&&
                after.vertices[c].u==before.vertices[c].u&&after.vertices[c].v==before.vertices[c].v,"source surface and native UV unchanged");
        }
    }
    for(std::size_t f=source.triangles.size();f<right.triangles.size();++f) {
        const auto& face=right.triangles[f];
        Check(face.origin==VrHandFaceOrigin::DerivedSleeveClosure&&face.sourceFaceIndex==std::numeric_limits<std::uint32_t>::max()&&
            face.textureIndex==closure.textureIndex&&face.sourceMaterial==closure.sourceMaterial&&face.polyFlags==closure.polyFlags,
            "closure explicitly derivative, not an original source face");
        for(std::size_t c=0u;c<3u;++c) Check(face.sourceVertexIds[c]<6u&&face.vertices[c].u==closure.u&&face.vertices[c].v==closure.v,
            "closure uses only exact rim identities and native cuff swatch");
    }
    const auto left=MirrorClosedVrHandGeometry(right);
    Check(left.derivedMirrored&&left.triangles.size()==right.triangles.size()&&left.originalProvenance==right.originalProvenance,
        "mirror records derivative identity");
    for(std::size_t f=0u;f<right.triangles.size();++f) for(std::size_t c=0u;c<3u;++c) {
        const auto other=c==0u?0u:3u-c;const auto& a=right.triangles[f].vertices[other];const auto& b=left.triangles[f].vertices[c];
        Check(Near(b.position,{-a.position.x,a.position.y,a.position.z})&&Near(b.normal,{-a.normal.x,a.normal.y,a.normal.z})&&
            b.u==a.u&&b.v==a.v&&left.triangles[f].sourceVertexIds[c]==right.triangles[f].sourceVertexIds[other],
            "mirror reflects normals, winding, source identities together");
    }
    const auto roundTrip=MirrorClosedVrHandGeometry(left);
    Check(!roundTrip.derivedMirrored,"two prepared reflections restore source-relative mirror parity");
    const auto reflectedSource=MirrorVrHandSourceAcrossQuestX(source);
    Check(reflectedSource.derivedMirrored,"source handedness reflection explicitly marked");
    const auto reflectedClosed=BuildClosedVrHandGeometry(reflectedSource,closure);
    Check(reflectedClosed.derivedMirrored,"closure preserves source-relative mirror parity");
    for(std::size_t f=0u;f<source.triangles.size();++f) for(std::size_t c=0u;c<3u;++c) {
        const auto other=c==0u?0u:3u-c;const auto& before=source.triangles[f].vertices[other];
        const auto& after=reflectedSource.triangles[f].vertices[c];
        Check(Near(after.position,{-before.position.x,before.position.y,before.position.z})&&
            Near(after.normal,{-before.normal.x,before.normal.y,before.normal.z})&&after.u==before.u&&after.v==before.v&&
            reflectedSource.triangles[f].sourceVertexIds[c]==source.triangles[f].sourceVertexIds[other],
            "explicit source reflection preserves source IDs/UV pairing and reverses outward winding");
    }
    VrHandGripBasis basis;basis.origin={0.01f,-0.02f,0.03f};basis.xAxis={0,1,0};basis.yAxis={-1,0,0};basis.zAxis={0,0,1};basis.provenance="Synthetic proper grip basis";
    const auto inGrip=PrepareVrHandSourceInGripSpace(source,basis);
    Check(inGrip.triangles.size()==source.triangles.size(),"proper basis preserves face count");
    for(std::size_t f=0u;f<source.triangles.size();++f) for(std::size_t c=0u;c<3u;++c) {
        const auto p=source.triangles[f].vertices[c].position;const auto& v=inGrip.triangles[f].vertices[c];
        Check(Near(v.position,{p.y+0.02f,0.01f-p.x,p.z-0.03f})&&v.u==source.triangles[f].vertices[c].u&&v.v==source.triangles[f].vertices[c].v,
            "proper grip frame centers and rotates without UV/winding mutation");
    }
    Reject([&]{auto s=source;s.triangles.clear();BuildClosedVrHandGeometry(s,closure);},"empty source");
    Reject([&]{auto s=source;s.provenance.clear();BuildClosedVrHandGeometry(s,closure);},"missing provenance");
    Reject([&]{auto s=source;s.originalPivotObjectUnits.x=std::numeric_limits<float>::infinity();BuildClosedVrHandGeometry(s,closure);},"nonfinite pivot");
    Reject([&]{auto s=source;s.triangles[0].origin=VrHandFaceOrigin::DerivedSleeveClosure;BuildClosedVrHandGeometry(s,closure);},"derivative mislabeled source");
    Reject([&]{auto s=source;s.triangles[0].vertices[0].u=-0.01f;BuildClosedVrHandGeometry(s,closure);},"non-native UV");
    Reject([&]{auto s=source;s.triangles[0].vertices[0].normal.x=std::numeric_limits<float>::quiet_NaN();BuildClosedVrHandGeometry(s,closure);},"nonfinite normal");
    Reject([&]{auto s=source;s.triangles[0].vertices[0].position.x=2.0f;BuildClosedVrHandGeometry(s,closure);},"unbounded controller-local position");
    Reject([&]{auto s=source;s.triangles[0].vertices[1].position=s.triangles[0].vertices[0].position;BuildClosedVrHandGeometry(s,closure);},"degenerate face");
    Reject([&]{auto s=source;s.triangles[0].sourceVertexIds[1]=s.triangles[0].sourceVertexIds[0];BuildClosedVrHandGeometry(s,closure);},"repeated source identities");
    Reject([&]{auto s=source;s.triangles.push_back(s.triangles[0]);BuildClosedVrHandGeometry(s,closure);},"duplicate face");
    Reject([&]{auto s=source;s.triangles[0].vertices[0].position.x+=0.001f;BuildClosedVrHandGeometry(s,closure);},"source identity position seam disagreement");
    Reject([&]{auto s=source;std::swap(s.triangles[0].sourceVertexIds[1],s.triangles[0].sourceVertexIds[2]);std::swap(s.triangles[0].vertices[1],s.triangles[0].vertices[2]);BuildClosedVrHandGeometry(s,closure);},"inconsistent source winding");
    Reject([&]{auto c=closure;c.boundaryVertexIds.pop_back();BuildClosedVrHandGeometry(source,c);},"unapproved boundary identities");
    Reject([&]{auto c=closure;c.boundaryVertexIds.push_back(0u);BuildClosedVrHandGeometry(source,c);},"duplicate boundary approval");
    Reject([&]{auto c=closure;c.v=1.001f;BuildClosedVrHandGeometry(source,c);},"closure UV out of native range");
    Reject([&]{auto c=closure;c.u=std::numeric_limits<float>::quiet_NaN();BuildClosedVrHandGeometry(source,c);},"closure UV NaN");
    Reject([&]{VrHandGeometryLimits l;l.maximumSourceTriangles=5u;BuildClosedVrHandGeometry(source,closure,l);},"source face budget");
    Reject([&]{VrHandGeometryLimits l;l.maximumTotalTriangles=9u;BuildClosedVrHandGeometry(source,closure,l);},"combined face budget");
    Reject([&]{VrHandGeometryLimits l;l.maximumTotalTriangles=8193u;BuildClosedVrHandGeometry(source,closure,l);},"hard maximum limits");
    Reject([&]{VrHandGeometryLimits l;l.maximumClosureDiameter=0.03f;BuildClosedVrHandGeometry(source,closure,l);},"unknown large opening");
    Reject([&]{auto s=source;for(auto& face:s.triangles) for(std::size_t c=0u;c<3u;++c) if(face.sourceVertexIds[c]==0u) face.vertices[c].position.z+=0.03f;BuildClosedVrHandGeometry(s,closure);},"nonplanar opening");
    Reject([&]{auto s=source;for(auto& face:s.triangles) for(std::size_t c=0u;c<3u;++c) if(face.sourceVertexIds[c]==0u) face.vertices[c].position={0.0f,0.0f,0.0f};BuildClosedVrHandGeometry(s,closure);},"concave rim");
    Reject([&]{auto b=basis;b.xAxis.x=0.4f;PrepareVrHandSourceInGripSpace(source,b);},"nonorthogonal grip frame");
    Reject([&]{auto b=basis;b.yAxis={1,0,0};PrepareVrHandSourceInGripSpace(source,b);},"reflected grip frame");
    Reject([&]{auto b=basis;b.origin.x=std::numeric_limits<float>::quiet_NaN();PrepareVrHandSourceInGripSpace(source,b);},"nonfinite grip origin");
    Reject([&]{auto r=right;r.triangles.pop_back();MirrorClosedVrHandGeometry(r);},"mirroring an unclosed hand");
    Reject([&]{auto r=right;for(auto face:right.triangles) {
        for(std::size_t c=0u;c<3u;++c) if(face.sourceVertexIds[c]!=6u) {face.sourceVertexIds[c]+=10u;face.vertices[c].position.x+=0.2f;}
        r.triangles.push_back(face);
    }MirrorClosedVrHandGeometry(r);},"closed bow-tie vertex with disconnected triangle fans");
    Reject([&]{auto s=source;auto face=source.triangles[0];face.sourceVertexIds={6u,0u,7u};
        face.vertices[0].position={0,0,0.06f};face.vertices[1].position={0.04f,0,0};face.vertices[2].position={0.01f,-0.02f,0.07f};
        s.triangles.push_back(face);BuildClosedVrHandGeometry(s,closure);},"three incident faces on an existing edge");
    Reject([&]{auto s=source;for(auto f:source.triangles) {for(auto& id:f.sourceVertexIds) id+=10u;for(auto& v:f.vertices) v.position.x+=0.2f;s.triangles.push_back(f);}BuildClosedVrHandGeometry(s,closure);},"extra hole/disconnected hand");
    Reject([&]{BuildOriginalNanoKeyRingVrHand({},0u);},"unauthored/empty original adapter mesh");
}
using Edge = std::pair<std::uint32_t,std::uint32_t>;
struct Face { std::array<std::uint32_t,3> ids; std::uint16_t material{}; };
void Audit(const std::string& name, const PortableLodMesh& mesh,
    const QuestVr::MeshPose& pose, bool printEdges) {
    std::vector<Face> faces;
    std::map<Edge,std::vector<std::pair<std::uint32_t,std::uint32_t>>> edges;
    std::map<std::uint16_t,std::size_t> materials;
    std::map<std::uint32_t,QuestVr::ActorVec3> positions;
    for (std::size_t first=0; first<mesh.triangles.size(); first+=3) {
        const auto material=mesh.triangles[first].material;
        const auto selected=QuestVr::ResolveActorMeshMaterial({},mesh.texturePaths,
            mesh.materialTextureIndices,material);
        if (selected.texturePath!=QuestVr::PlayerVisualDetail::OriginalHandTexture) continue;
        Face face; face.material=material;
        for(std::size_t c=0;c<3;++c) {
            face.ids[c]=mesh.animation->triangleSourceVertexIndices.at(first+c);
            positions[face.ids[c]]=pose.objectPositions.at(face.ids[c]);
        }
        faces.push_back(face); ++materials[material];
        for(std::size_t c=0;c<3;++c) {
            const auto a=face.ids[c],b=face.ids[(c+1)%3];
            edges[std::minmax(a,b)].push_back({a,b});
        }
    }
    if(faces.empty()) return;
    std::size_t boundary{},nonmanifold{},sameDirection{};
    std::map<std::uint32_t,std::vector<std::uint32_t>> next;
    for(const auto& [key,uses]:edges) {
        (void)key;
        if(uses.size()==1) {++boundary;next[uses[0].first].push_back(uses[0].second);}
        else if(uses.size()!=2) ++nonmanifold;
        else if(uses[0]==uses[1]) ++sameDirection;
    }
    std::cout<<"Candidate "<<name<<" handFaces="<<faces.size()<<" vertices="<<positions.size()
        <<" boundary="<<boundary<<" nonmanifold="<<nonmanifold<<" sameDirection="<<sameDirection<<'\n';
    for(const auto& [material,count]:materials) std::cout<<"  material="<<material<<" faces="<<count<<'\n';
    if(!printEdges) return;
    for(const auto& [key,uses]:edges) {
        (void)key;
        if(uses.size()!=1) continue;
        const auto a=uses[0].first,b=uses[0].second;
        const auto pa=positions.at(a),pb=positions.at(b);
        std::cout<<"  boundary "<<a<<" -> "<<b<<" ("<<pa.x<<','<<pa.y<<','<<pa.z<<") ("
            <<pb.x<<','<<pb.y<<','<<pb.z<<")\n";
    }
    for(const auto& [id,p]:positions)
        std::cout<<"  vertex "<<id<<" ("<<p.x<<','<<p.y<<','<<p.z<<")\n";
    for(const auto& face:faces)
        std::cout<<"  face material="<<face.material<<" "<<face.ids[0]<<','<<face.ids[1]<<','<<face.ids[2]<<'\n';
    if(name=="NanoKeyRingPOV") for(std::size_t first=0;first<mesh.triangles.size();first+=3u) {
        if(mesh.triangles[first].material!=0u) continue;
        std::cout<<"  source-face "<<first/3u;
        for(std::size_t corner=0;corner<3u;++corner) {
            const auto& v=mesh.triangles[first+corner];
            std::cout<<" ["<<mesh.animation->triangleSourceVertexIndices.at(first+corner)<<" uv("<<v.u*256.0f<<','<<v.v*256.0f<<")]";
        }
        std::cout<<'\n';
    }
}
void Label(questvisual::Image& image,int x,int y,std::uint32_t id) {
    static constexpr std::uint16_t glyphs[]{0b111101101101111,0b010110010010111,0b111001111100111,
        0b111001111001111,0b101101111001001,0b111100111001111,0b111100111101111,
        0b111001010010010,0b111101111101111,0b111101111001111};
    const auto draw=[&](int px,int py,std::uint8_t r,std::uint8_t g,std::uint8_t b) {
        if(px<0||py<0||px>=static_cast<int>(image.width)||py>=static_cast<int>(image.height)) return;
        const auto at=(static_cast<std::size_t>(py)*image.width+static_cast<std::size_t>(px))*3u;
        image.rgb[at]=r;image.rgb[at+1]=g;image.rgb[at+2]=b;
    };
    for(int dy=-2;dy<=2;++dy) for(int dx=-2;dx<=2;++dx) draw(x+dx,y+dy,255,30,30);
    const auto number=std::to_string(id);
    for(std::size_t digit=0;digit<number.size();++digit) for(int row=0;row<5;++row) for(int col=0;col<3;++col)
        if(glyphs[number[digit]-'0']&(1u<<static_cast<unsigned>(14-row*3-col)))
            for(int sy=0;sy<2;++sy) for(int sx=0;sx<2;++sx)
                draw(x+4+static_cast<int>(digit)*8+col*2+sx,y-5+row*2+sy,255,255,0);
}
void Preview(const std::string& name,const PortableLodMesh& mesh,
    const QuestVr::MeshPose& pose,const PortableTextureImage& image,
    const std::filesystem::path& directory) {
    const auto transform=QuestVr::PlayerVisualDetail::ObjectToQuestMeters();
    std::set<std::uint32_t> grip;
    for(std::size_t first=0;first<mesh.triangles.size();first+=3) {
        const auto material=mesh.triangles[first].material;
        const auto selected=QuestVr::ResolveActorMeshMaterial({},mesh.texturePaths,mesh.materialTextureIndices,material);
        if(selected.texturePath!=QuestVr::PlayerVisualDetail::OriginalHandTexture) continue;
        if(name=="Crowbar"&&material!=1u) continue;
        for(std::size_t c=0;c<3;++c) grip.insert(mesh.animation->triangleSourceVertexIndices.at(first+c));
    }
    QuestVr::ActorVec3 center;
    for(const auto id:grip) {const auto p=pose.objectPositions.at(id);center.x+=p.x;center.y+=p.y;center.z+=p.z;}
    center.x/=static_cast<float>(grip.size());center.y/=static_cast<float>(grip.size());center.z/=static_cast<float>(grip.size());
    const auto pivot=transform.TransformPoint(center);
    std::cout<<"Preview "<<name<<" unique grasp pivot("<<center.x<<','<<center.y<<','<<center.z<<")\n";
    questvisual::Scene scene;
    scene.actorTextureWidth=image.width;scene.actorTextureHeight=image.height;
    scene.actorTextureLayers=1;scene.actorTextures=image.rgba;
    std::map<std::uint32_t,questvisual::Chunk> chunks;
    for(std::size_t first=0;first<mesh.triangles.size();first+=3) {
        const auto& face=mesh.triangles[first];
        const auto selected=QuestVr::ResolveActorMeshMaterial({},mesh.texturePaths,mesh.materialTextureIndices,face.material);
        if(selected.texturePath!=QuestVr::PlayerVisualDetail::OriginalHandTexture) continue;
        auto& chunk=chunks[face.polyFlags];chunk.textureBank=questvisual::TextureBank::Actor;chunk.polyFlags=face.polyFlags;
        for(auto v:QuestVr::BuildActorTriangle(mesh,first,transform,&pose)) {
            v.position.x-=pivot.x;v.position.y-=pivot.y;v.position.z-=pivot.z;
            chunk.vertices.push_back({{v.position.x,v.position.y,v.position.z},
                {v.normal.x,v.normal.y,v.normal.z},v.u,v.v,0});
        }
    }
    for(auto& [flags,chunk]:chunks) {(void)flags;scene.chunks.push_back(std::move(chunk));}
    struct View {const char* name;questvisual::Vec3 position;float yaw,pitch;};
    const View views[]{ {"front",{0,0,.28f},0,0},{"right",{.28f,0,0},-90,0},
        {"left",{-.28f,0,0},90,0},{"top",{0,.28f,0},0,-90},
        {"bottom",{0,-.28f,0},0,90},{"back",{0,0,-.28f},180,0} };
    for(const auto& view:views) {
        questvisual::Camera camera;camera.position=view.position;camera.yawDegrees=view.yaw;camera.pitchDegrees=view.pitch;
        camera.verticalFovDegrees=65;camera.nearPlane=.005f;
        auto result=questvisual::Render(scene,camera,640,640);
        questvisual::WriteBmp(directory/(name+"-"+view.name+".bmp"),result.image);
        std::cout<<"  "<<view.name<<" pixels="<<result.coveredPixels<<" rasterized="<<result.rasterizedTriangles<<'\n';
        if(name=="NanoKeyRingPOV") {
            const float yaw=camera.yawDegrees*3.14159265359f/180.0f,pitch=camera.pitchDegrees*3.14159265359f/180.0f;
            const QuestVr::ActorVec3 right{std::cos(yaw),0,std::sin(yaw)};
            const QuestVr::ActorVec3 forward{std::sin(yaw)*std::cos(pitch),std::sin(pitch),-std::cos(yaw)*std::cos(pitch)};
            const auto up=QuestVr::VrHandGeometryDetail::Cross(right,forward);
            const float focal=320.0f/std::tan(65.0f*3.14159265359f/360.0f);
            for(const auto id:grip) {
                auto p=transform.TransformPoint(pose.objectPositions.at(id));
                p={p.x-pivot.x-view.position.x,p.y-pivot.y-view.position.y,p.z-pivot.z-view.position.z};
                const auto depth=QuestVr::VrHandGeometryDetail::Dot(p,forward);
                if(depth<=.005f) continue;
                const int x=static_cast<int>(320+focal*QuestVr::VrHandGeometryDetail::Dot(p,right)/depth);
                const int y=static_cast<int>(320-focal*QuestVr::VrHandGeometryDetail::Dot(p,up)/depth);
                Label(result.image,x,y,id);
            }
            questvisual::WriteBmp(directory/(name+"-"+view.name+"-ids.bmp"),result.image);
        }
    }
}
void OriginalAudit(const std::string& root,const std::filesystem::path& directory) {
    const auto package=LoadPortablePackageTables((std::filesystem::path(root)/"System"/"DeusExItems.u").string());
    PortableTextureImage image;
    if(!directory.empty()) {
        std::filesystem::create_directories(directory);image=DecodePortableIndexedTexture(package,"Skins.WeaponHandsTex",false);
        questvisual::Image atlas;atlas.width=image.width;atlas.height=image.height;
        for(std::size_t at=0;at<image.rgba.size();at+=4) {atlas.rgb.push_back(image.rgba[at]);atlas.rgb.push_back(image.rgba[at+1]);atlas.rgb.push_back(image.rgba[at+2]);}
        questvisual::WriteBmp(directory/"original-WeaponHandsTex-preview.bmp",atlas);
    }
    for(std::size_t index=0;index<package.exports.size();++index) {
        const auto cls=GetPortableObjectPath(package,package.exports[index].ObjClass);
        if(cls.substr(cls.find_last_of('.')+1)!="LodMesh") continue;
        auto mesh=LoadPortableLodMesh(package,index);
        for(const auto reference:mesh.textures) {
            auto path=GetPortableObjectPath(package,reference);
            if(reference>0&&!path.empty()) path="DeusExItems."+path;
            mesh.texturePaths.push_back(path);
        }
        if(!mesh.animation||!QuestVr::FindMeshAnimationSequence(*mesh.animation,"Still",false)) continue;
        const auto name=GetPortableObjectPath(package,static_cast<std::int32_t>(index+1));
        const auto pose=QuestVr::PlayerVisualDetail::OriginalStillPose(mesh);
        const bool candidate=name=="NanoKeyRingPOV"||name=="LockpickPOV"||name=="POVCorpse"||name=="Crowbar";
        Audit(name,mesh,pose,name=="Glock"||candidate);
        if(candidate&&!directory.empty()) Preview(name,mesh,pose,image,directory);
    }
}
void ValidateOutputDirectory(const std::string& root,const std::filesystem::path& directory) {
    if(directory.empty()) return;
    const auto original=std::filesystem::weakly_canonical(root);
    const auto target=std::filesystem::weakly_canonical(directory);
    const auto lower=[](std::string text) {for(auto& c:text) if(c>='A'&&c<='Z') c=static_cast<char>(c-'A'+'a');return text;};
    auto a=original.begin(),b=target.begin();
    while(a!=original.end()&&b!=target.end()&&lower(a->string())==lower(b->string())) {++a;++b;}
    if(a==original.end()) throw std::runtime_error("Hand preview output must not modify the user's original installation");
    std::filesystem::create_directories(target);
}
void PreviewClosed(const std::string& name,const QuestVr::VrHandGeometry& geometry,
    const PortableTextureImage& image,const std::filesystem::path& directory) {
    questvisual::Scene scene;scene.actorTextureWidth=image.width;scene.actorTextureHeight=image.height;
    scene.actorTextureLayers=1u;scene.actorTextures=image.rgba;
    std::map<std::uint32_t,questvisual::Chunk> chunks,capChunks;
    std::map<std::uint32_t,QuestVr::ActorVec3> capPositions;QuestVr::ActorVec3 capNormal{};
    for(const auto& face:geometry.triangles) {
        auto& chunk=chunks[face.polyFlags];chunk.textureBank=questvisual::TextureBank::Actor;chunk.polyFlags=face.polyFlags;
        for(const auto& v:face.vertices) chunk.vertices.push_back({{v.position.x,v.position.y,v.position.z},
            {v.normal.x,v.normal.y,v.normal.z},v.u,v.v,0});
        if(face.origin==QuestVr::VrHandFaceOrigin::DerivedSleeveClosure) {
            auto& cap=capChunks[face.polyFlags];cap.textureBank=questvisual::TextureBank::Actor;cap.polyFlags=face.polyFlags;
            for(std::size_t c=0u;c<3u;++c) {
                const auto& v=face.vertices[c];capPositions.emplace(face.sourceVertexIds[c],v.position);
                cap.vertices.push_back({{v.position.x,v.position.y,v.position.z},{v.normal.x,v.normal.y,v.normal.z},v.u,v.v,0});
            }
            capNormal.x+=face.vertices[0].normal.x;capNormal.y+=face.vertices[0].normal.y;capNormal.z+=face.vertices[0].normal.z;
        }
    }
    for(auto& [flags,chunk]:chunks) {(void)flags;scene.chunks.push_back(std::move(chunk));}
    struct View {const char* name;questvisual::Vec3 position;float yaw,pitch;};
    const View views[]{ {"little-side",{0,0,.55f},0,0},{"positive-x",{.55f,0,0},-90,0},
        {"negative-x",{-.55f,0,0},90,0},{"sleeve-side",{0,.55f,0},0,-90},
        {"knuckle-side",{0,-.55f,0},0,90},{"index-side",{0,0,-.55f},180,0} };
    for(const auto& view:views) {
        questvisual::Camera camera;camera.position=view.position;camera.yawDegrees=view.yaw;camera.pitchDegrees=view.pitch;
        camera.verticalFovDegrees=75;camera.nearPlane=.005f;
        const auto result=questvisual::Render(scene,camera,640,640);
        Check(result.coveredPixels>1000u&&result.rasterizedTriangles>0u,"closed canonical hand visible in axis view with original culling");
        questvisual::WriteBmp(directory/(name+"-"+view.name+".bmp"),result.image);
        std::cout<<"Preview "<<name<<' '<<view.name<<" pixels="<<result.coveredPixels<<" rasterized="<<result.rasterizedTriangles<<'\n';
    }
    QuestVr::ActorVec3 center{};const auto divisor=static_cast<float>(capPositions.size());
    for(const auto& [id,p]:capPositions) {(void)id;center.x+=p.x/divisor;center.y+=p.y/divisor;center.z+=p.z/divisor;}
    const auto normal=QuestVr::NormalizeActorVector(capNormal);
    questvisual::Camera cuffCamera;cuffCamera.position={center.x+normal.x*.25f,center.y+normal.y*.25f,center.z+normal.z*.25f};
    cuffCamera.yawDegrees=std::atan2(-normal.x,normal.z)*180.0f/3.14159265359f;
    cuffCamera.pitchDegrees=std::asin(-normal.y)*180.0f/3.14159265359f;
    cuffCamera.verticalFovDegrees=65;cuffCamera.nearPlane=.005f;
    auto capScene=scene;capScene.chunks.clear();
    for(auto& [flags,chunk]:capChunks) {(void)flags;capScene.chunks.push_back(std::move(chunk));}
    const auto full=questvisual::Render(scene,cuffCamera,640,640),cap=questvisual::Render(capScene,cuffCamera,640,640);
    const auto middle=320u*640u+320u;
    Check(cap.coveredPixels>1000u&&cap.rasterizedTriangles>0u&&std::isfinite(cap.depth.at(middle))&&
        Near(full.depth.at(middle),cap.depth.at(middle),1.0e-4f)&&Near(cap.depth.at(middle),.25f,.005f),
        "rear cuff derivative renders outward with native culling and occludes sleeve interior at actual cap depth");
    questvisual::WriteBmp(directory/(name+"-cuff-closure-detail.bmp"),full.image);
    questvisual::WriteBmp(directory/(name+"-cuff-cap-only.bmp"),cap.image);
    std::cout<<"Preview "<<name<<" cuff closure pixels="<<cap.coveredPixels<<" centerDepth="<<cap.depth.at(middle)<<'\n';
}
void OriginalTests(const std::string& root,const std::filesystem::path& directory) {
    using namespace QuestVr;
    ValidateOutputDirectory(root,directory);
    const auto package=LoadPortablePackageTables((std::filesystem::path(root)/"System"/"DeusExItems.u").string());
    auto mesh=LoadPortableLodMesh(package,FindPortableExport(package,"NanoKeyRingPOV"));
    for(const auto reference:mesh.textures) {
        auto path=GetPortableObjectPath(package,reference);
        if(reference>0&&!path.empty()) path="DeusExItems."+path;
        mesh.texturePaths.push_back(path);
    }
    const auto raw=BuildOriginalNanoKeyRingHandSource(mesh,0u);
    const auto basis=BuildOriginalNanoKeyRingGripBasis(raw);
    const auto right=BuildOriginalNanoKeyRingVrHand(mesh,0u),left=MirrorClosedVrHandGeometry(right);
    const auto topology=VrHandGeometryDetail::Inspect(right.triangles,{},true);
    Check(right.triangles.size()==156u&&right.originalTriangleCount==152u&&right.closureTriangleCount==4u&&right.sourceVertexCount==80u,
        "original hand surface/closure counts");
    Check(topology.edges.size()==234u&&topology.positions.size()==80u&&topology.boundary.empty(),"original closed hand Euler/manifold topology");
    Check(left.triangles.size()==156u&&left.derivedMirrored&&!right.derivedMirrored,
        "source-relative handedness: original decoded right, derived mirrored left");
    MeshAnimationState state;state.main.sequence="Still";const auto pose=PrepareMeshPose(mesh,state);
    for(std::size_t f=0u;f<raw.triangles.size();++f) {
        const auto& before=raw.triangles[f];const auto& after=right.triangles[f];
        Check(before.sourceVertexIds==after.sourceVertexIds&&before.sourceFaceIndex==after.sourceFaceIndex&&
            before.textureIndex==after.textureIndex&&before.polyFlags==after.polyFlags&&before.sourceMaterial==after.sourceMaterial&&
            after.origin==VrHandFaceOrigin::OriginalSurface,"original selected face identity/material/flags unchanged in canonical grip");
        for(std::size_t c=0u;c<3u;++c) {
            const auto p=pose.objectPositions.at(before.sourceVertexIds[c]);
            Check(Near(before.vertices[c].position,{p.y/52.5f,p.z/52.5f,-p.x/52.5f}),
                "source identity follows transformed triangle corner, including original axes conversion without extra reflection");
            const auto& v=after.vertices[c];
            const ActorVec3 restored{basis.origin.x+basis.xAxis.x*v.position.x+basis.yAxis.x*v.position.y+basis.zAxis.x*v.position.z,
                basis.origin.y+basis.xAxis.y*v.position.x+basis.yAxis.y*v.position.y+basis.zAxis.y*v.position.z,
                basis.origin.z+basis.xAxis.z*v.position.x+basis.yAxis.z*v.position.y+basis.zAxis.z*v.position.z};
            Check(Near(restored,before.vertices[c].position)&&v.u==before.vertices[c].u&&v.v==before.vertices[c].v,
                "canonical preparation is invertible rigid transform retaining every native UV");
            const ActorVec3 restoredNormal{basis.xAxis.x*v.normal.x+basis.yAxis.x*v.normal.y+basis.zAxis.x*v.normal.z,
                basis.xAxis.y*v.normal.x+basis.yAxis.y*v.normal.y+basis.zAxis.y*v.normal.z,
                basis.xAxis.z*v.normal.x+basis.yAxis.z*v.normal.y+basis.zAxis.z*v.normal.z};
            Check(Near(restoredNormal,before.vertices[c].normal),"canonical original normals retain source-ID/UV pairing without hidden reflection");
        }
    }
    const auto average=[&](std::initializer_list<std::uint32_t> ids) {
        ActorVec3 sum{};for(const auto id:ids) {const auto p=topology.positions.at(id);const auto n=static_cast<float>(ids.size());sum.x+=p.x/n;sum.y+=p.y/n;sum.z+=p.z/n;}return sum;
    };
    const auto palm=average({77u,78u,9u,11u}),back=average({109u,68u,13u,17u});
    const auto little=average({48u,96u,15u}),index=average({20u,64u,30u});
    Check(back.x>palm.x+0.01f&&Near(VrHandGeometryDetail::Dot(basis.xAxis,basis.zAxis),0.0f,1.0e-5f),
        "right +X points into palm, orthogonal to curl tube");
    Check(index.z<little.z-0.05f&&Near(index.x,little.x,1.0e-5f)&&Near(index.y,little.y,1.0e-5f),"right -Z follows little-to-index curl tube");
    Check(Near(ActorVec3{(palm.x+back.x)*0.5f,(palm.y+back.y)*0.5f,(palm.z+back.z)*0.5f},{}),"grip origin centers palm/back landmarks, excluding sleeve");
    const auto cuff=average({1u,3u,38u,2u,63u,25u});
    Check(cuff.y>.15f&&little.y<-.05f&&index.y<-.05f,
        "original right sleeve +Y toward wearer and curled fingers -Y away in grip frame");
    const auto leftTopology=VrHandGeometryDetail::Inspect(left.triangles,{},true);
    const auto averageLeft=[&](std::initializer_list<std::uint32_t> ids) {
        ActorVec3 sum{};for(const auto id:ids) {const auto p=leftTopology.positions.at(id);const auto n=static_cast<float>(ids.size());sum.x+=p.x/n;sum.y+=p.y/n;sum.z+=p.z/n;}return sum;
    };
    const ActorVec3 cuffPoints[]{cuff,averageLeft({1u,3u,38u,2u,63u,25u})};
    const ActorVec3 fingerPoints[]{average({48u,96u,15u,20u,64u,30u}),averageLeft({48u,96u,15u,20u,64u,30u})};
    for(std::size_t hand=0u;hand<2u;++hand) {
        const auto cuffHead=RotateCapturedGrip(NeutralGripCapture[hand],cuffPoints[hand]);
        const auto fingersHead=RotateCapturedGrip(NeutralGripCapture[hand],fingerPoints[hand]);
        Check(cuffHead.z>.20f&&fingersHead.z<-.06f,"actual captured neutral grip puts original cuff toward wearer and fingers away");
        auto wrongCuff=cuffPoints[hand],wrongFingers=fingerPoints[hand];wrongCuff.y=-wrongCuff.y;wrongFingers.y=-wrongFingers.y;
        Check(RotateCapturedGrip(NeutralGripCapture[hand],wrongCuff).z<-.20f&&RotateCapturedGrip(NeutralGripCapture[hand],wrongFingers).z>.06f,
            "captured neutral pose rejects previous reflected-source wrist/finger reversal");
        std::cout<<"Recorded neutral "<<NeutralGripCapture[hand].name<<" cuffHeadZ="<<cuffHead.z<<" fingersHeadZ="<<fingersHead.z<<'\n';
    }
    const auto image=DecodePortableIndexedTexture(package,"Skins.WeaponHandsTex",false);
    Check(image.width==256u&&image.height==256u,"original hand atlas native dimensions");
    for(std::size_t f=152u;f<right.triangles.size();++f) {
        const auto& face=right.triangles[f];
        Check(face.origin==VrHandFaceOrigin::DerivedSleeveClosure&&face.sourceFaceIndex==std::numeric_limits<std::uint32_t>::max(),"original sleeve closure not mislabeled original");
        const auto& v=face.vertices[0];const auto x=static_cast<std::uint32_t>(v.u*image.width),y=static_cast<std::uint32_t>(v.v*image.height);
        const auto at=(static_cast<std::size_t>(y)*image.width+x)*4u;
        Check(x<image.width&&y<image.height&&image.rgba.at(at)<40u&&image.rgba.at(at+1u)<40u&&image.rgba.at(at+2u)<40u&&image.rgba.at(at+3u)==255u,
            "derived sleeve cap samples original dark opaque rear cuff, not skin or invented pixels");
    }
    Reject([&]{auto changed=mesh;changed.materialTextureIndices[0]=-1;BuildOriginalNanoKeyRingVrHand(changed,0u);},"changed original material binding");
    Reject([&]{BuildOriginalNanoKeyRingGripBasis(MirrorVrHandSourceAcrossQuestX(raw));},"extra mirrored source reverses right-grip wrist/finger direction");
    Reject([&]{VrHandGeometryLimits limits;limits.maximumSourceTriangles=151u;BuildOriginalNanoKeyRingVrHand(mesh,0u,limits);},"original source budget");
    Reject([&]{VrHandGeometryLimits limits;limits.maximumTotalTriangles=155u;BuildOriginalNanoKeyRingVrHand(mesh,0u,limits);},"original derived total budget");
    std::cout<<"Original canonical grip pivot object("<<raw.originalPivotObjectUnits.x<<','<<raw.originalPivotObjectUnits.y<<','<<raw.originalPivotObjectUnits.z<<")\n";
    std::cout<<"Original basis X("<<basis.xAxis.x<<','<<basis.xAxis.y<<','<<basis.xAxis.z<<") Y("<<basis.yAxis.x<<','<<basis.yAxis.y<<','<<basis.yAxis.z<<") Z("<<basis.zAxis.x<<','<<basis.zAxis.y<<','<<basis.zAxis.z<<")\n";
    std::cout<<right.originalProvenance<<'\n'<<right.derivativeProvenance<<'\n';
    if(!directory.empty()) {PreviewClosed("closed-right",right,image,directory);PreviewClosed("closed-left",left,image,directory);}
}
}
int main(int argc,char** argv) {
    try {
        SyntheticTests();
        if((argc==3||argc==5)&&std::string(argv[1])=="--audit-original" &&
            (argc==3||std::string(argv[3])=="--output")) {
            ValidateOutputDirectory(argv[2],argc==5?argv[4]:"");OriginalAudit(argv[2],argc==5?argv[4]:"");
        } else if((argc==3||argc==5)&&std::string(argv[1])=="--game-root"&&
            (argc==3||std::string(argv[3])=="--output")) OriginalTests(argv[2],argc==5?argv[4]:"");
        else if(argc!=1) throw std::runtime_error("Usage: quest_vr_hand_geometry_test [--game-root GAME_ROOT | --audit-original GAME_ROOT] [--output DIRECTORY]");
        std::cout<<"VR hand geometry PASS: "<<checks<<" checks, "<<rejections<<" expected rejections\n";
        return 0;
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
