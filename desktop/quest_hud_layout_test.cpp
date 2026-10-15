#include "quest_hud_view.h"
#include "quest_dialogue_ui.h"

#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
using namespace QuestVr;
std::size_t checks{};
void Require(bool value,const char* message) {
    ++checks;
    if(!value) throw std::runtime_error(message);
}
struct Vector {
    float x{},y{},z{};
    Vector& operator+=(Vector value) {x+=value.x;y+=value.y;z+=value.z;return *this;}
};
Vector Difference(Vector a,Vector b) {return {a.x-b.x,a.y-b.y,a.z-b.z};}
float Length(Vector value) {return std::sqrt(value.x*value.x+value.y*value.y+value.z*value.z);}
struct Quaternion {
    float x{},y{},z{},w{1.0f};
    Quaternion operator*(Quaternion q) const {
        return {w*q.x+x*q.w+y*q.z-z*q.y,w*q.y-x*q.z+y*q.w+z*q.x,
            w*q.z+x*q.y-y*q.x+z*q.w,w*q.w-x*q.x-y*q.y-z*q.z};
    }
    Quaternion Inverted() const {return {-x,-y,-z,w};}
    Vector Rotate(Vector value) const {
        const auto rotated=(*this)*Quaternion{value.x,value.y,value.z,0.0f}*Inverted();
        return {rotated.x,rotated.y,rotated.z};
    }
};
struct Pose {Vector Translation;Quaternion Rotation;};
Vector Point(const Pose& pose,Vector point) {
    auto result=pose.Translation;result+=pose.Rotation.Rotate(point);return result;
}
float Degrees(float y,float depth) {return std::atan2(y,depth)*180.0f/Pi;}

void EyeFacingBelt() {
    for(int yaw=-8;yaw<=8;++yaw) for(int pitch=-4;pitch<=4;++pitch)
        for(int roll=-3;roll<=3;++roll) for(const float height:{0.9f,1.65f}) {
            const float y=yaw*0.25f,p=pitch*0.12f,r=roll*0.15f;
            const auto rotation=Quaternion{0,std::sin(y*0.5f),0,std::cos(y*0.5f)}*
                Quaternion{std::sin(p*0.5f),0,0,std::cos(p*0.5f)}*
                Quaternion{0,0,std::sin(r*0.5f),std::cos(r*0.5f)};
            const Pose head{{2.0f,height,-3.0f},rotation};
            const auto panel=QuickBeltPanelPose(head);
            const auto toLocal=[&](Vector world) {return head.Rotation.Inverted().Rotate(Difference(world,head.Translation));};
            const auto center=toLocal(panel.Translation);
            Require(std::fabs(center.x-kPlayableBeltCenterX)<0.00001f &&
                std::fabs(center.y-kPlayableBeltCenterY)<0.00001f &&
                std::fabs(center.z-kPlayableBeltCenterZ)<0.00001f,
                "Eye-facing correction moved the requested quickbar center");
            const auto normal=panel.Rotation.Rotate({0,0,1});
            const auto toHead=Difference(head.Translation,panel.Translation);
            const auto cross=Vector{normal.y*toHead.z-normal.z*toHead.y,
                normal.z*toHead.x-normal.x*toHead.z,normal.x*toHead.y-normal.y*toHead.x};
            Require(Length(cross)<0.00001f && normal.x*toHead.x+normal.y*toHead.y+normal.z*toHead.z>0.0f,
                "Quickbar surface does not face the head viewpoint");
            for(const float x:{-kPlayableBeltHalfWidth,0.0f,kPlayableBeltHalfWidth}) {
                const auto top=Point(panel,{x,kPlayableBeltHalfHeight,0});
                const auto bottom=Point(panel,{x,-kPlayableBeltHalfHeight,0});
                for(const float eyeX:{-0.037f,0.0f,0.037f}) {
                    auto eye=head.Translation;eye+=head.Rotation.Rotate({eyeX,0,0});
                    Require(std::fabs(Length(Difference(top,eye))-Length(Difference(bottom,eye)))<0.00002f,
                        "Quickbar bottom remains farther away than its top for either eye");
                }
                const auto cameraTop=toLocal(top),cameraBottom=toLocal(bottom);
                Require(Degrees(-cameraTop.y,-cameraTop.z)>34.5f &&
                    Degrees(-cameraBottom.y,-cameraBottom.z)<41.0f,
                    "Actual tilted quickbar corners leave the requested lower viewing band");
                for(const auto camera:{cameraTop,cameraBottom}) for(const float eyeX:{-0.037f,0.037f})
                    Require(std::fabs(Degrees(camera.x-eyeX,-camera.z))<29.0f,
                        "Eye-facing quickbar corners exceed the binocular horizontal bounds");
            }
            const auto leftCenter=toLocal(Point(panel,{-kPlayableBeltHalfWidth,0,0}));
            Require(std::fabs(leftCenter.x-kHudColumnLeftX)<0.00001f &&
                std::fabs(leftCenter.z+kHudVrDistance)<0.00001f,
                "Eye-facing quickbar lost column alignment along its middle row");
        }
}

void SpeechBesideColumn() {
    static_assert(kDataLinkWidth==431u && kDataLinkHeight==114u);
    const float speechLeft=kHudDataLinkCenterX-kHudDataLinkHalfWidth;
    const float speechRight=kHudDataLinkCenterX+kHudDataLinkHalfWidth;
    const float healthRight=kHudHealthCenterX+kHudHealthHalfWidth;
    const float compassRight=kHudCompassCenterX+kHudCompassHalfWidth;
    Require(std::fabs(speechLeft-healthRight-kHudDataLinkColumnGap)<0.00001f &&
        speechLeft>compassRight,"NPC portrait/text overlaps health or compass");
    Require(std::fabs(kHudDataLinkCenterY+kHudDataLinkHalfHeight-
        kHudHealthCenterY-kHudHealthHalfHeight)<0.00001f,
        "NPC speech and health no longer share their top edge");
    Require(kHudDataLinkPixelsToMeters<kHudVrPixelsToMeters &&
        kHudDataLinkPixelsToMeters/kHudVrPixelsToMeters>0.8f,
        "NPC speech scale is not a modest reduction of the original VR size");
    Require(std::fabs(kHudDataLinkHalfWidth/kHudDataLinkHalfHeight-
        static_cast<float>(kDataLinkWidth)/kDataLinkHeight)<0.00001f,
        "NPC portrait/text original canvas aspect ratio changed");
    for(int ipd=52;ipd<=74;++ipd) for(const float side:{-1.0f,1.0f}) {
        const float eyeX=side*ipd*0.0005f;
        const auto project=[&](float x) {return (x-eyeX)/kHudVrDistance;};
        Require(project(speechLeft)>project(healthRight) && project(speechLeft)>project(compassRight),
            "Binocular projection lets NPC speech cover the HUD column");
        Require(std::fabs(Degrees(speechLeft-eyeX,kHudVrDistance))<33.5f &&
            std::fabs(Degrees(speechRight-eyeX,kHudVrDistance))<33.5f &&
            Degrees(kHudDataLinkCenterY+kHudDataLinkHalfHeight,kHudVrDistance)<20.0f,
            "NPC speech panel exceeds its binocular inset viewing bounds");
    }
}
}
int main() {
    try {
        EyeFacingBelt();SpeechBesideColumn();
        std::cout<<"PASS HUD layout: "<<checks<<" checks; both-eye equal quickbar edge distances, tracked yaw/pitch/roll, unchanged lower anchor, separated NPC portrait/text and inset FOV bounds\n";
        return 0;
    } catch(const std::exception& error) {
        std::cerr<<"FAIL HUD layout: "<<error.what()<<'\n';return 1;
    }
}
