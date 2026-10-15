#include "quest_hud_view.h"
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
struct Vector {float x{},y{},z{};};
std::size_t checks{};
void Require(bool value,const char* what) {++checks;if(!value) throw std::runtime_error(what);}
bool Near(std::uint16_t a,std::uint16_t b) {
    const auto delta=(static_cast<std::uint32_t>(a)-b)&65535u;
    return delta<=1u || delta>=65535u;
}
std::uint16_t Heading(Vector forward,Vector right,float scene=0.0f) {
    const auto value=QuestVr::HudRenderedViewYaw(forward,right,scene);
    Require(value.has_value(),"Valid rendered direction was refused");return *value;
}
}
int main() {
    try {
        constexpr Vector north{0,0,-1},east{1,0,0},south{0,0,1},west{-1,0,0};
        Require(Heading(north,east)==0u,"Rendered UE +X did not read north");
        Require(Heading(east,south)==16384u,"Rendered UE +Y did not read east");
        Require(Heading(south,west)==32768u,"Rendered UE -X did not read south");
        Require(Heading(west,north)==49152u,"Rendered UE -Y did not read west");
        for (int i=-32;i<=32;++i) {
            const float physical=static_cast<float>(i)*0.071f;
            const Vector forward{std::sin(physical),0,-std::cos(physical)};
            const Vector right{std::cos(physical),0,std::sin(physical)};
            const auto base=Heading(forward,right,0.37f);
            const auto up=Heading({0,1,0},right,0.37f);
            const auto down=Heading({0,-1,0},right,0.37f);
            Require(Near(base,up) && Near(base,down),"Vertical gaze lost horizontal heading");
            // Room-space reference rebase preserves the rendered map heading.
            constexpr float originYaw=0.25f;
            Vector position{1,0,2},previousHead{2,1,3};float scene=0.37f;
            QuestVr::RebaseReferenceSpace(Vector{0.3f,0,0.7f},originYaw,position,scene,previousHead);
            const auto newForward=QuestVr::StageDirectionToLocal(forward,originYaw);
            const auto newRight=QuestVr::StageDirectionToLocal(right,originYaw);
            Require(Near(base,Heading(newForward,newRight,scene)),"Room rebase changed compass heading");
            // A recenter intentionally resets physical forward: compass follows
            // the newly rendered direction, not an obsolete tracking yaw cache.
            scene=0.37f;
            QuestVr::RecenterReferenceSpace(Vector{0.3f,0,0.7f},originYaw,position,scene,previousHead);
            Require(Near(Heading(newForward,newRight,scene),Heading(forward,right,0.62f)),
                "Meta recenter compass did not follow the actual rendered view");
        }
        Require(Near(Heading(north,east,QuestVr::Pi/2),16384u),"Snap turn not included in compass");
        Require(Near(Heading(north,east,-QuestVr::Pi/2),49152u),"Reverse snap turn not included in compass");
        const float invalid=std::numeric_limits<float>::quiet_NaN();
        Require(!QuestVr::HudRenderedViewYaw(north,east,invalid),"Invalid scene heading accepted");
        Require(!QuestVr::HudRenderedViewYaw(Vector{invalid,0,-1},east,0),"Invalid forward accepted");
        Require(!QuestVr::HudRenderedViewYaw(north,Vector{1,invalid,0},0),"Invalid right accepted");
        Require(!QuestVr::HudRenderedViewYaw(Vector{0,1,0},Vector{0,-1,0},0),"Undefined heading invented north");
        Require(QuestVr::kHudCompassHalfWidth>0 && QuestVr::kHudCompassCenterX<0 &&
            QuestVr::kHudCompassCenterY>0 && QuestVr::kHudVrDistance>0,"Original HUD placement is invalid");
        const auto angle=[](float offset) {return std::atan2(std::fabs(offset),QuestVr::kHudVrDistance)*180.0f/QuestVr::Pi;};
        Require(angle(QuestVr::kHudCompassCenterX-QuestVr::kHudCompassHalfWidth)<27.0f &&
            angle(QuestVr::kHudCompassCenterY+QuestVr::kHudCompassHalfHeight)<6.0f,
            "Compass is outside the lowered/aligned column bounds");
        Require(angle(QuestVr::kHudHealthCenterX-QuestVr::kHudHealthHalfWidth)<27.0f &&
            angle(QuestVr::kHudHealthCenterY+QuestVr::kHudHealthHalfHeight)<20.0f,
            "Health column is outside the lowered/aligned bounds");
        Require(std::fabs((QuestVr::kHudCompassCenterX-QuestVr::kHudCompassHalfWidth)-
            (QuestVr::kHudHealthCenterX-QuestVr::kHudHealthHalfWidth))<0.00001f,
            "Inset HUD column lost original left-edge alignment");
        Require(std::fabs((QuestVr::kHudCompassCenterX-QuestVr::kHudCompassHalfWidth)-
            (QuestVr::kPlayableBeltCenterX-QuestVr::kPlayableBeltHalfWidth))<0.00001f,
            "Health/compass left edge is not aligned with the quickbar");
        Require(std::fabs((QuestVr::kHudHealthCenterY-QuestVr::kHudHealthHalfHeight)-
            (QuestVr::kHudCompassCenterY+QuestVr::kHudCompassHalfHeight)-4.0f*QuestVr::kHudVrPixelsToMeters)<0.00001f,
            "Inset HUD column lost original four-pixel panel gap");
        std::cout<<"PASS rendered compass heading: "<<checks<<" checks; map axes, physical yaw, vertical gaze, snap turns, room rebases, Meta recenter, invalid tracking and lowered quickbar-aligned column\n";
        return 0;
    } catch (const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
