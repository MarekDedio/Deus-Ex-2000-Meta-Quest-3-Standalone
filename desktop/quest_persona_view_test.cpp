#include "quest_persona_view.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
namespace {
struct Vector {
    float x{},y{},z{};
    Vector& operator+=(const Vector& v) {x+=v.x;y+=v.y;z+=v.z;return *this;}
};
struct Rotation {
    float x{},y{},z{},w{1.0f};
    Rotation operator*(const Rotation& q) const {
        return {w*q.x+x*q.w+y*q.z-z*q.y,w*q.y-x*q.z+y*q.w+z*q.x,
            w*q.z+x*q.y-y*q.x+z*q.w,w*q.w-x*q.x-y*q.y-z*q.z};
    }
    Vector Rotate(Vector v) const {
        const auto q=(*this)*Rotation{v.x,v.y,v.z,0.0f}*Rotation{-x,-y,-z,w};return {q.x,q.y,q.z};
    }
};
struct Pose {Vector Translation;struct Rotation Rotation;};
void Require(bool value) {if(!value) throw std::runtime_error("Lowered Persona pose is inconsistent");}
}
int main() {
    try {
        for(int index=-20;index<=20;++index) {
            const float yaw=index*0.1f;
            const Pose head{{2.0f,1.65f,-3.0f},{0.0f,std::sin(yaw*0.5f),0.0f,std::cos(yaw*0.5f)}};
            const auto artwork=QuestVr::PersonaPanelPose(head);
            const auto text=QuestVr::PersonaPanelPose(head,QuestVr::kPersonaTextDistance);
            Require(std::fabs(artwork.Translation.y-1.49f)<0.00001f);
            Require(std::fabs(text.Translation.y-artwork.Translation.y)<0.00001f);
            Require(std::fabs(std::hypot(artwork.Translation.x-head.Translation.x,artwork.Translation.z-head.Translation.z)-
                QuestVr::kPersonaArtworkDistance)<0.00001f);
            const auto edgeDistance=[&](float y) {
                auto edge=artwork.Translation;edge+=artwork.Rotation.Rotate({0.0f,y,0.0f});
                return std::hypot(std::hypot(edge.x-head.Translation.x,edge.y-head.Translation.y),edge.z-head.Translation.z);
            };
            Require(std::fabs(edgeDistance(QuestVr::kPersonaPanelHalfHeight)-edgeDistance(-QuestVr::kPersonaPanelHalfHeight))<0.00001f);
            const auto normal=artwork.Rotation.Rotate({0.0f,0.0f,1.0f});
            const Vector toHead{head.Translation.x-artwork.Translation.x,head.Translation.y-artwork.Translation.y,head.Translation.z-artwork.Translation.z};
            Require(std::fabs(normal.x*toHead.y-normal.y*toHead.x)<0.00001f &&
                std::fabs(normal.z*toHead.y-normal.y*toHead.z)<0.00001f);
        }
        Require(QuestVr::kPersonaPanelCenterY<-0.015f && QuestVr::kPersonaArtworkDistance>QuestVr::kPersonaTextDistance);
        Require(QuestVr::kPersonaPanelWidth==1.20f && QuestVr::kPersonaPanelHeight==0.90f &&
            QuestVr::kPersonaPanelHalfWidth==0.60f && QuestVr::kPersonaPanelHalfHeight==0.45f);
        std::cout<<"PASS lowered eye-facing Persona panel: 207 checks; common page/artwork/text/controller plane, unchanged size, balanced top/bottom distance\n";
        return 0;
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
