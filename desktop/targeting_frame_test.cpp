#include "quest_targeting_frame.h"
#include <iostream>
#include <stdexcept>

namespace {
unsigned checks{};
void Require(bool pass, const char* message) { ++checks; if (!pass) throw std::runtime_error(message); }
bool Same(float a,float b) { return std::fabs(a-b) < 0.0001f; }
}
int main() {
    try {
        std::array<QuestVr::TargetFramePoint,8> points;
        unsigned index{};
        for (float x : {-0.25f,0.25f}) for (float y : {-0.5f,0.5f})
            for (float z : {-2.0f,-2.5f}) points[index++] = {x,y,z};
        const auto box = QuestVr::ProjectTargetFrameBounds(points,{});
        Require(box.has_value(),"Forward world bounds were not projected");
        Require(Same(box->x,280.0f) && Same(box->y,180.0f) && Same(box->width,80.0f) && Same(box->height,120.0f),
            "Projection changed pinned central-eye bounds");
        const auto strips = QuestVr::BuildOriginalFrobCornerStrips(*box,0.0f);
        Require(strips.has_value(),"Finite original frame was refused");
        Require(Same((*strips)[8].x,280.0f) && Same((*strips)[8].y,180.0f) && Same((*strips)[8].width,20.0f) && Same((*strips)[8].height,1.0f),
            "Original top-left corner formula changed");
        Require(Same((*strips)[0].x,(*strips)[8].x+1.0f) && Same((*strips)[0].y,(*strips)[8].y+1.0f),
            "Original black shadow is not one pixel offset");
        const auto animated = QuestVr::BuildOriginalFrobCornerStrips(*box,0.25f);
        Require(animated && Same((*animated)[8].x,286.0f) && Same((*animated)[8].y,186.0f),"Original 24px/sec 0.3sec inset changed");
        const auto large = QuestVr::BuildOriginalFrobCornerStrips({-100.0f,-100.0f,1000.0f,1000.0f},0.0f);
        Require(large && Same((*large)[8].x,70.0f) && Same((*large)[8].width,40.0f),"Original screen margin/corner clamp changed");
        const auto tiny = QuestVr::BuildOriginalFrobCornerStrips({320.0f,240.0f,0.0f,0.0f},0.0f);
        Require(tiny && Same((*tiny)[8].x,312.0f) && Same((*tiny)[8].width,4.0f),"Original tiny-target expansion changed");
        for (const auto& strip : *strips) Require(strip.width > 0.0f && strip.height > 0.0f,"Zero-width strip");
        // Match the real SDK's FLOAT_MATRIX4 Count>1 CPU transpose then
        // glUniformMatrix4fv(...,GL_FALSE,...) path and GLSL column indexing.
        // FLOAT_VECTOR4 ignores Count in this SDK; its old array transport
        // uploaded only the first black strip and left all white strips zero.
        const auto matrices = QuestVr::PackTargetFrameMatrices(*strips);
        for (std::size_t i=0u; i<strips->size(); ++i) {
            const auto& matrix = matrices[i/4u];
            std::array<float,16> uploaded;
            for (std::size_t row=0u; row<4u; ++row)
                for (std::size_t column=0u; column<4u; ++column)
                    uploaded[row*4u+column] = matrix[column*4u+row];
            const float components[]{(*strips)[i].x,(*strips)[i].y,(*strips)[i].width,(*strips)[i].height};
            for (std::size_t component=0u; component<4u; ++component)
                Require(Same(uploaded[(i%4u)*4u+component],components[component]),
                    "SDK matrix upload changed a GLSL strip component");
        }
        for (auto& point : points) point.z = 1.0f;
        Require(!QuestVr::ProjectTargetFrameBounds(points,{}),"Behind-head target was projected");
        points[0].z = std::numeric_limits<float>::quiet_NaN();
        Require(!QuestVr::ProjectTargetFrameBounds(points,{}),"NaN bounds were projected");
        Require(!QuestVr::BuildOriginalFrobCornerStrips(*box,-1.0f),"Negative time accepted");
        Require(!QuestVr::BuildOriginalFrobCornerStrips({0,0,-1,1},0.0f),"Negative bounds accepted");
        const auto hit = QuestVr::RayTargetFrameBounds({0,0,0},{0,0,-1},{-1,-1,-3},{1,1,-2},4);
        Require(hit && Same(*hit,2.0f),"Door broad phase hit changed");
        const auto inside = QuestVr::RayTargetFrameBounds({0,0,-2.5f},{0,0,-1},{-1,-1,-3},{1,1,-2},3);
        Require(inside && *inside >= 0.0f && *inside < 3.0f && Same(*inside,0.0f),
            "Controller inside pickup bounds must remain an in-range use candidate");
        Require(!QuestVr::RayTargetFrameBounds({2,0,0},{0,0,-1},{-1,-1,-3},{1,1,-2},4),"Parallel ray outside bounds hit");
        Require(!QuestVr::RayTargetFrameBounds({0,0,0},{0,0,-1},{-1,-1,-3},{1,1,-2},1),"Out-of-reach door hit");
        Require(!QuestVr::RayTargetFrameBounds({0,0,0},{0,0,-1},{1,-1,-3},{-1,1,-2},4),"Inverted bounds accepted");
        std::cout << "Targeting frame tests passed: " << checks << " checks\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
