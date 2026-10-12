#include "Precomp.h"
#include "quest_ground_probe.h"
#include "portable_model_geometry.h"
#include "surreal_portable_package_tables.h"

#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace {
struct Vec { float x{},y{},z{}; };
struct Triangle { Vec a,b,c,normal; };
std::size_t checks{};
void Require(bool value,const std::string& message) {
    ++checks;
    if (!value) throw std::runtime_error(message);
}
Vec Add(Vec a,Vec b) { return {a.x+b.x,a.y+b.y,a.z+b.z}; }
Vec Sub(Vec a,Vec b) { return {a.x-b.x,a.y-b.y,a.z-b.z}; }
Vec Scale(Vec a,float b) { return {a.x*b,a.y*b,a.z*b}; }
float Dot(Vec a,Vec b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
Vec Cross(Vec a,Vec b) { return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x}; }
Vec Closest(Vec point,const Triangle& triangle) {
    const auto ab=Sub(triangle.b,triangle.a),ac=Sub(triangle.c,triangle.a),ap=Sub(point,triangle.a);
    const float d1=Dot(ab,ap),d2=Dot(ac,ap);
    if (d1<=0 && d2<=0) return triangle.a;
    const auto bp=Sub(point,triangle.b);
    const float d3=Dot(ab,bp),d4=Dot(ac,bp);
    if (d3>=0 && d4<=d3) return triangle.b;
    const float vc=d1*d4-d3*d2;
    if (vc<=0 && d1>=0 && d3<=0) return Add(triangle.a,Scale(ab,d1/(d1-d3)));
    const auto cp=Sub(point,triangle.c);
    const float d5=Dot(ab,cp),d6=Dot(ac,cp);
    if (d6>=0 && d5<=d6) return triangle.c;
    const float vb=d5*d2-d1*d6;
    if (vb<=0 && d2>=0 && d6<=0) return Add(triangle.a,Scale(ac,d2/(d2-d6)));
    const float va=d3*d6-d5*d4;
    if (va<=0 && d4-d3>=0 && d5-d6>=0)
        return Add(triangle.b,Scale(Sub(triangle.c,triangle.b),(d4-d3)/((d4-d3)+(d5-d6))));
    const float denominator=1.0f/(va+vb+vc);
    return Add(triangle.a,Add(Scale(ab,vb*denominator),Scale(ac,vc*denominator)));
}
struct Scene {
    std::vector<Triangle> triangles;
    void TriangleAt(Vec a,Vec b,Vec c) {
        auto normal=Cross(Sub(b,a),Sub(c,a));
        const float length=std::sqrt(Dot(normal,normal));
        if (length>0.000001f) triangles.push_back({a,b,c,Scale(normal,1.0f/length)});
    }
    void Quad(Vec a,Vec b,Vec c,Vec d) { TriangleAt(a,b,c); TriangleAt(a,c,d); }
    void Floor(float start,float end,float height) {
        Quad({start,height,-1},{end,height,-1},{end,height,1},{start,height,1});
    }
    void Riser(float x,float low,float high) {
        Quad({x,low,-1},{x,high,-1},{x,high,1},{x,low,1});
    }
    std::optional<float> FloorAt(Vec feet,bool footprint=true) const {
        QuestVr::GroundProbe probe(feet.y);
        for (const auto& face:triangles) {
            const auto height=footprint ? QuestVr::GroundHeightWithinRadius(feet,face) :
                                         QuestVr::GroundHeightAtXZ(feet,face);
            if (height) probe.Add(*height);
        }
        return probe.Height();
    }
    bool Touches(Vec feet,bool original=false) const {
        for (const auto& face:triangles) {
            if (original) {
                if (std::fabs(face.normal.y)>0.65f) continue;
                for (float height:{0.3f,0.85f,1.4f}) {
                    const Vec sample{feet.x,feet.y+height,feet.z};
                    const auto delta=Sub(sample,Closest(sample,face));
                    if (Dot(delta,delta)<0.28f*0.28f) return true;
                }
            } else if (QuestVr::CapsuleTouchesTriangle(feet,face,Closest)) return true;
        }
        return false;
    }
    std::pair<Vec,bool> Walk(Vec start,Vec target,bool original=false) const {
        const float distance=std::hypot(target.x-start.x,target.z-start.z);
        const auto steps=std::max(1,static_cast<int>(std::ceil(distance/0.025f)));
        Vec accepted=start;
        for (int step=1;step<=steps;++step) {
            const float fraction=static_cast<float>(step)/steps;
            Vec candidate{start.x+(target.x-start.x)*fraction,accepted.y,
                          start.z+(target.z-start.z)*fraction};
            if (original) {
                if (const auto floor=FloorAt(candidate,false)) candidate.y=*floor;
                if (Touches(candidate,true)) return {accepted,true};
                accepted=candidate;
            } else {
                const auto result=QuestVr::ResolveGroundedHorizontalMotion(accepted,candidate,
                    [&](Vec feet) { return FloorAt(feet); },[&](Vec feet) { return Touches(feet); });
                if (result.blocked) return {accepted,true};
                accepted=result.feet;
            }
        }
        return {accepted,false};
    }
};

void Synthetic() {
    constexpr float rise=12.0f/52.5f,run=16.0f/52.5f;
    Scene stairs;
    stairs.Floor(-2.0f,0.0f,0.0f);
    for (int i=0;i<8;++i) {
        stairs.Riser(i*run,i*rise,(i+1)*rise);
        stairs.Floor(i*run,(i+1)*run,(i+1)*rise);
    }
    stairs.Floor(8*run,8*run+2.0f,8*rise);
    const Vec bottom{-0.6f,0,0},top{8*run+0.6f,8*rise,0};
    Require(stairs.Walk(bottom,top,true).second,"Point-floor regression did not reproduce blocked ascent");
    Require(stairs.Walk(top,bottom,true).second,"Point-floor regression did not reproduce blocked descent");
    const auto up=stairs.Walk(bottom,top),down=stairs.Walk(top,bottom);
    Require(!up.second && std::fabs(up.first.y-8*rise)<0.00001f,"Eight-tread ascent did not reach upper landing");
    Require(!down.second && std::fabs(down.first.y)<0.00001f,"Eight-tread descent did not reach lower landing");
    const auto sweepUp=QuestVr::ResolveGroundedHorizontalMotion(bottom,top,
        [&](Vec feet) { return stairs.FloorAt(feet); },[&](Vec feet) { return stairs.Touches(feet); });
    const auto sweepDown=QuestVr::ResolveGroundedHorizontalMotion(top,bottom,
        [&](Vec feet) { return stairs.FloorAt(feet); },[&](Vec feet) { return stairs.Touches(feet); });
    Require(!sweepUp.blocked && std::fabs(sweepUp.feet.y-8*rise)<0.00001f,
            "Multi-sample ascending room-scale sweep used destination height too early");
    Require(!sweepDown.blocked && std::fabs(sweepDown.feet.y)<0.00001f,
            "Multi-sample descending room-scale sweep used destination height too early");
    // Diagonal and reversed-axis paths exercise the full XZ footprint, not a
    // special case for a stair facing +X.
    for (auto& face:stairs.triangles) {
        const auto rotate=[](Vec p) { return Vec{p.z,p.y,-p.x}; };
        face={rotate(face.a),rotate(face.b),rotate(face.c),rotate(face.normal)};
    }
    const auto rotatedUp=stairs.Walk({0,0,0.6f},{0,8*rise,-top.x});
    const auto rotatedDown=stairs.Walk({0,8*rise,-top.x},{0,0,0.6f});
    Require(!rotatedUp.second && !rotatedDown.second,"Negative-Z stairs lost ascent/descent support");

    Scene wall;
    wall.Floor(-2,2,0); wall.Riser(0,0,3);
    Require(wall.Walk({-0.6f,0,0},{0.6f,0,0}).second,"Footprint grounding bypassed a full-height wall");
    const auto acrossWall=QuestVr::ResolveGroundedHorizontalMotion(Vec{-0.6f,0,0},Vec{0.6f,0,0},
        [&](Vec feet) { return wall.FloorAt(feet); },[&](Vec feet) { return wall.Touches(feet); });
    Require(acrossWall.blocked && acrossWall.feet.x==-0.6f,"Room-scale sweep crossed a thin wall or published partial movement");
    Scene tooHigh;
    tooHigh.Floor(-2,0,0); tooHigh.Riser(0,0,0.6f); tooHigh.Floor(0,2,0.6f);
    Require(tooHigh.Walk({-0.6f,0,0},{0.6f,0,0}).second,"A ledge above the 45cm step bound was climbed");
    Scene ceiling;
    ceiling.Floor(-2,0,0); ceiling.Riser(0,0,rise); ceiling.Floor(0,2,rise);
    ceiling.Floor(0,2,1.9f);
    Require(ceiling.Walk({-0.6f,0,0},{0.6f,0,0}).second,"Step-up capsule bypassed its low ceiling");
    const auto floor=stairs.triangles.front();
    Require(!QuestVr::GroundHeightWithinRadius(Vec{20,0,20},floor),"Finite triangle was replaced by an infinite floor plane");
    Require(!QuestVr::GroundHeightWithinRadius(Vec{},floor,-1),"Negative support radius accepted");
    const auto invalid=QuestVr::ResolveGroundedHorizontalMotion(Vec{},Vec{8,0,0},
        [](Vec)->std::optional<float> { return 0; },[](Vec) { return false; });
    Require(invalid.blocked && invalid.feet.x==0,"Oversized room-scale jump bypassed bounded sweep");
    const auto nan=QuestVr::ResolveGroundedHorizontalMotion(Vec{},Vec{0.1f,0,0},
        [](Vec)->std::optional<float> { return std::numeric_limits<float>::quiet_NaN(); },[](Vec) { return false; });
    Require(nan.blocked && nan.feet.x==0,"Nonfinite floor callback published a moved capsule");
    std::cout << "PASS synthetic: eight-step up/down, rotated stairs, full walls, high ledges, low ceilings, finite footprint and tracking bounds\n";
}

Scene OriginalScene(const PortableModelGeometry& model) {
    Scene scene;
    const auto convert=[](PortableModelVec3 p) { return Vec{p.y/52.5f,p.z/52.5f,-p.x/52.5f}; };
    for (const auto& node:model.nodes) {
        if (node.vertexCount<3 || node.surface<0 || node.vertexPool<0) continue;
        const auto& surface=model.surfaces.at(static_cast<std::size_t>(node.surface));
        if ((surface.polyFlags&(0x00000001u|0x04000000u))!=0) continue;
        const auto point=[&](unsigned index) {
            return convert(model.points.at(static_cast<std::size_t>(model.vertices.at(
                static_cast<std::size_t>(node.vertexPool)+index).point)));
        };
        for (unsigned index=1;index+1u<node.vertexCount;++index) scene.TriangleAt(point(0),point(index),point(index+1));
    }
    return scene;
}

void Original(const std::filesystem::path& root) {
    for (const std::string map: {"00_Training","01_NYC_UNATCOHQ"}) {
        const auto path=root/"Maps"/(map+".dx");
        const auto before=std::filesystem::last_write_time(path);
        const auto package=LoadPortablePackageTables(path.string());
        const auto model=LoadPortableRootModel68(package);
        const auto scene=OriginalScene(model);
        std::size_t routes{};
        // Discover authored vertical stair risers (not fabricated stairs).
        // Both sides must have distinct finite treads at the riser's levels.
        // Keep each audited route short and bounded; this is a locomotion
        // regression, not an assertion that the entire campaign is playable.
        for (const auto& face:scene.triangles) {
            if (std::fabs(face.normal.y)>0.001f) continue;
            const float low=std::min({face.a.y,face.b.y,face.c.y});
            const float high=std::max({face.a.y,face.b.y,face.c.y});
            if (high-low<0.08f || high-low>QuestVr::GroundMaximumStepUp) continue;
            const Vec center{(face.a.x+face.b.x+face.c.x)/3,low,(face.a.z+face.b.z+face.c.z)/3};
            const Vec forward{face.normal.x,0,face.normal.z};
            const Vec a=Add(center,Scale(forward,-0.03f)),b=Add(center,Scale(forward,0.03f));
            const auto floorA=scene.FloorAt(a,false),floorB=scene.FloorAt(b,false);
            if (!floorA || !floorB || std::fabs(*floorA-*floorB)<0.08f) continue;
            const float sign=*floorB>*floorA ? 1.0f : -1.0f;
            const Vec direction=Scale(forward,sign);
            Vec start=Add(center,Scale(direction,-0.65f)),end=Add(center,Scale(direction,0.65f));
            start.y=std::min(*floorA,*floorB);
            if (const auto floor=scene.FloorAt(start,false)) start.y=*floor; else continue;
            end.y=std::max(*floorA,*floorB);
            if (const auto floor=scene.FloorAt(end,false)) end.y=*floor; else continue;
            if (end.y-start.y<0.15f) continue;
            const auto up=scene.Walk(start,end);
            if (up.second || up.first.y-start.y<0.15f) continue;
            const auto down=scene.Walk(up.first,start);
            if (down.second || std::fabs(down.first.y-start.y)>0.0001f) continue;
            if (!scene.Walk(start,end,true).second || !scene.Walk(up.first,start,true).second) continue;
            Require(!up.second && !down.second,map+" discovered authored stair route failed");
            ++routes;
            std::cout << "PASS original " << map << " route=" << routes << " rise=" << up.first.y-start.y
                      << " from=(" << start.x << ',' << start.y << ',' << start.z << ") to=("
                      << up.first.x << ',' << up.first.y << ',' << up.first.z << ") old-point-ascent/descent=blocked\n";
            if (routes==3u) break;
        }
        Require(routes>0,map+" lacked an authored stairs route reproducing both old ascent and descent failures");
        Require(std::filesystem::last_write_time(path)==before,"Original map was modified by stair inspection");
    }
}
} // namespace

int main(int argc,char** argv) {
    try {
        Require(argc<=2,"Usage: quest_stair_locomotion_test [original-game-root]");
        Synthetic();
        if (argc==2) Original(argv[1]);
        std::cout << "PASS: " << checks << " stair locomotion checks. Shared Quest support/capsule/sweep helpers; "
                  << "no OpenXR/device acceptance, full BSP hull physics, gravity or full-campaign claim.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
