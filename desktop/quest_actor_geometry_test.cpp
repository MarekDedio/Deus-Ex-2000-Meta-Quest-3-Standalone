#include "quest_actor_geometry.h"

#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
void Require(bool value) { if (!value) throw std::runtime_error("Shared actor geometry assertion failed"); }
template<class F> void Reject(F f) {
    bool rejected{};
    try { f(); } catch (const std::runtime_error&) { rejected = true; }
    Require(rejected);
}
}
int main() {
    try {
        PortableLodMesh mesh;
        mesh.triangles = {{0,0,0,0,0,0},{1,0,0,1,0,0},{0,1,0,0,1,0}};
        PortableActorSnapshot actor;
        actor.x = 52.5f; actor.y = 105.0f; actor.z = 157.5f;
        actor.prePivotX = 10.0f; actor.prePivotY = 20.0f; actor.prePivotZ = 30.0f;
        const auto transform = QuestVr::BuildSnapshotActorTransform(actor,{});
        const auto triangle = QuestVr::BuildActorTriangle(mesh,0,transform);
        Require(transform.mirrored && triangle[1].v == 1.0f && triangle[2].u == 1.0f);
        Require(std::fabs(triangle[0].position.x-(105.0f+20.0f)/52.5f) < 0.0001f);
        Require(std::fabs(triangle[0].position.y-(157.5f+30.0f)/52.5f-1.0f) < 0.0001f);
        const auto& a = triangle[0].position; const auto& b = triangle[1].position; const auto& c = triangle[2].position;
        const QuestVr::ActorVec3 u{b.x-a.x,b.y-a.y,b.z-a.z},v{c.x-a.x,c.y-a.y,c.z-a.z};
        const auto normal = QuestVr::NormalizeActorVector({u.y*v.z-u.z*v.y,u.z*v.x-u.x*v.z,u.x*v.y-u.y*v.x});
        Require(std::fabs(normal.y-triangle[0].normal.y) < 0.0001f);
        const auto brush = QuestVr::BuildSnapshotActorTransform(actor,{},true);
        Require(std::fabs(brush.translation.x-(105.0f-20.0f)/52.5f) < 0.0001f);
        Require(std::fabs(brush.translation.y-(157.5f-30.0f)/52.5f-1.0f) < 0.0001f);
        actor.style = 3; actor.unlit = true;
        mesh.triangles[0].polyFlags = 0x100u;
        Require(QuestVr::ActorTrianglePolyFlags(actor,mesh.triangles[0],2u) == (0x100u|4u|2u|0x00400000u));
        Reject([&] { QuestVr::BuildActorTriangle(mesh,0,transform); });
        mesh.triangles[0].polyFlags = 0;
        Reject([&] { QuestVr::BuildActorTriangle(mesh,1,transform); });
        Reject([&] { QuestVr::BuildActorTriangle(mesh,3,transform); });
        mesh.triangles[1].material = 1;
        Reject([&] { QuestVr::BuildActorTriangle(mesh,0,transform); });
        mesh.triangles[1].material = 0;
        mesh.triangles[1].u = std::numeric_limits<float>::quiet_NaN();
        Reject([&] { QuestVr::BuildActorTriangle(mesh,0,transform); });
        std::cout << "Shared actor triangle reflection/winding, normal/UV, mesh/brush pivot and material flags passed; 5 rejection controls.\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
