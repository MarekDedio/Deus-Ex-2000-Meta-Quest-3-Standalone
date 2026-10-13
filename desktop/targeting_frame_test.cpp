#include "quest_targeting_frame.h"
#include "quest_target_description.h"
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
        const auto inset=QuestVr::LayoutOriginalFrobFrame(*box,16.0f,24.0f);
        Require(inset && Same(inset->bounds.x,284.0f) && Same(inset->bounds.y,186.0f) &&
            Same(inset->bounds.width,72.0f) && Same(inset->bounds.height,108.0f),
            "Authored Unreal-unit radius/height were not divided by four as reference pixels");
        const auto insetStrips=QuestVr::BuildOriginalFrobCornerStrips(*box,0.0f,640,480,16,24);
        Require(insetStrips && Same((*insetStrips)[8].x,inset->bounds.x) &&
            Same((*insetStrips)[8].width,18.0f),"Frame strips and label bounds diverged after original inset");
        const auto invertedByInset=QuestVr::LayoutOriginalFrobFrame({320,240,1,1},16,16);
        Require(invertedByInset && Same(invertedByInset->bounds.width,9.0f),
            "Original screen inset erased a distant tiny target instead of applying original expansion");
        Require(Same(QuestVr::OriginalFrobMoverHalfExtentUnits,16.0f),
            "Original mover center proxy was replaced with physical brush outline dimensions");
        const auto label=QuestVr::LayoutOriginalFrobLabel(*inset,48.0f,7.0f);
        Require(label && Same(label->x,294.0f) && Same(label->y,196.0f) &&
            Same(label->width,56.0f) && Same(label->height,15.0f),
            "Original focus info panel text extent/padding/anchor changed");
        Require(!QuestVr::LayoutOriginalFrobFrame(*box,-1.0f,0.0f),"Negative authored focus radius accepted");
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
        const QuestVr::FrobDescriptionMessages messages{"Locked","Unlocked","Lock Str: ","Door Str: ","INF"};
        QuestVr::FrobDescriptionFields fields;
        fields.mover=true;
        Require(!QuestVr::BuildOriginalFrobDescription(fields,messages),"Missing lock state invented Unlocked");
        fields.locked=false;
        Require(QuestVr::BuildOriginalFrobDescription(fields,messages)=="Unlocked","Original unlocked source message was lost");
        Require(!QuestVr::BuildOriginalFrobDescription(fields,{}),"Missing original message acquired English fallback");
        fields.locked=true;fields.pickable=true;fields.breakable=false;fields.lockStrength=0.255f;
        Require(QuestVr::BuildOriginalFrobDescription(fields,messages)=="Locked\nLock Str: 26%\nDoor Str: INF",
            "Original locked/strength message field binding changed");
        fields.lockStrength=std::numeric_limits<float>::quiet_NaN();
        Require(!QuestVr::BuildOriginalFrobDescription(fields,messages),"Malformed lock strength displayed invented data");
        fields={};fields.decoration=true;fields.itemName="Desk Lamp";
        fields.familiarName="Known Lamp";fields.unfamiliarName="Unfamiliar Lamp";
        fields.lastConversationEndTime=0.0f;
        Require(QuestVr::BuildOriginalFrobDescription(fields,messages)=="Unfamiliar Lamp",
            "Familiar name was used before an original completed conversation");
        fields.lastConversationEndTime=1.0f;
        Require(QuestVr::BuildOriginalFrobDescription(fields,messages)=="Known Lamp","Original familiar-name rule was lost");
        fields.familiarName="";fields.unfamiliarName="";
        Require(QuestVr::BuildOriginalFrobDescription(fields,messages)=="Desk Lamp","Decoration ItemName did not replace technical actor identity");
        fields={};fields.inventory=true;fields.itemName="Crowbar";fields.bindName="WrongActorName";
        Require(QuestVr::BuildOriginalFrobDescription(fields,messages)=="Crowbar","Inventory used BindName instead of declared ItemName");
        fields.itemName.reset();
        Require(!QuestVr::BuildOriginalFrobDescription(fields,messages),"Missing Inventory ItemName acquired a technical-name fallback");
        struct Glyph { std::uint32_t pageIndex{},x{},y{},width{2u},height{3u}; };
        struct Font { std::uint32_t lineHeight{3u};std::vector<QuestVr::PersonaUiImage> pages; } font;
        font.pages.push_back({2u,3u,std::vector<std::uint8_t>(2u*3u*4u,255u)});
        Glyph glyph;
        const QuestVr::PersonaUiImage background{1u,1u,{100u,80u,60u,255u}};
        const auto canvas=QuestVr::BuildOriginalFrobLabelCanvas(font,"A\nAB",background,
            [&](std::uint32_t){return &glyph;});
        Require(canvas.textWidth==4u && canvas.textHeight==6u && canvas.image.width==512u && canvas.image.height==128u,
            "Original label glyph metrics or retained texture bound changed");
        Require(canvas.image.rgba[3u]==255u && canvas.image.rgba[(4u*512u+4u)*4u]==255u &&
            canvas.image.rgba[(127u*512u+511u)*4u+3u]==0u,
            "Original label border/glyph/transparent retained padding changed");
        const QuestVr::FrobDescriptionPalette palette{{{255u,233u,177u}},{{185u,177u,140u}},{{139u,105u,35u}}};
        const auto amber=QuestVr::BuildOriginalFrobLabelCanvas(font,"A",background,
            [&](std::uint32_t){return &glyph;},palette);
        const auto glyphAt=(4u*512u+4u)*4u;
        Require(amber.image.rgba[0u]==185u && amber.image.rgba[1u]==177u && amber.image.rgba[2u]==140u &&
            amber.image.rgba[glyphAt]==255u && amber.image.rgba[glyphAt+1u]==233u && amber.image.rgba[glyphAt+2u]==177u,
            "Selected authored RGB palette was replaced by white or grayscale glyph tint");
        std::cout << "Targeting frame tests passed: " << checks << " checks\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
