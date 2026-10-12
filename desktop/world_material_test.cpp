#include "quest_world_material.h"
#include <iostream>
#include <stdexcept>
namespace { unsigned checks{};
void Require(bool okay,const char* message) { ++checks; if (!okay) throw std::runtime_error(message); }
}
int main() {
    try {
        const auto glass = QuestVr::ResolveOriginalMaterialState(0x10cu);
        Require(glass.blend == QuestVr::OriginalMaterialBlend::Translucent && !glass.depthWrite && !glass.masked,
            "Authored Training glass must blend without opaque depth writes");
        const auto masked = QuestVr::ResolveOriginalMaterialState(2u);
        Require(masked.masked && masked.depthWrite && !masked.Blended(),"Opaque masked depth precedence changed");
        Require(!QuestVr::ResolveOriginalMaterialState(6u).masked,"Translucency must remove masking");
        Require(QuestVr::ResolveOriginalMaterialState(4u|64u).blend == QuestVr::OriginalMaterialBlend::Translucent,
            "Translucency precedence over modulation changed");
        Require(QuestVr::ResolveOriginalMaterialState(4u|0x80000000u).depthWrite,"Explicit PF_Occlude was lost");
        const std::array<float,3> background{0.2f,0.4f,0.8f};
        Require(QuestVr::BlendOriginalMaterialRgb(glass.blend,{0,0,0},background) == background,
            "Black original glass must preserve the background, not become opaque black");
        Require(QuestVr::BlendOriginalMaterialRgb(glass.blend,{1,1,1},background) == std::array<float,3>{1,1,1},
            "White original translucent texels must screen to white");
        Require(QuestVr::BlendOriginalMaterialRgb(QuestVr::OriginalMaterialBlend::Modulated,{0.5f,0.5f,0.5f},background) == background,
            "Neutral modulated texel must preserve background");
        std::vector<QuestVr::WorldSurfaceRecord> records;
        for (const int surface : {0,0,1,1,2,0}) for (unsigned j=0u;j<6u;++j) records.push_back({surface,static_cast<int>(j/3u)});
        const auto ranges = QuestVr::BuildWorldMaterialRanges(records,{0u,0x10cu,0x10cu},12u);
        Require(ranges.size() == 4u,"Opaque chunk budget or blended surface grouping changed");
        Require(ranges[0].begin == 0u && ranges[0].end == 12u && ranges[1].begin == 12u && ranges[1].end == 24u &&
            ranges[2].begin == 24u && ranges[2].end == 30u && ranges[3].end == 36u,"World split lost/reordered vertices");
        for (const auto& r : ranges) Require(r.begin%6u == 0u && r.end%6u == 0u && r.end-r.begin <= 12u,
            "World split broke front/back collision pairs or upload budget");
        records[4].surface=1;
        bool refused{}; try { static_cast<void>(QuestVr::BuildWorldMaterialRanges(records,{0u,4u})); } catch (const std::exception&) { refused=true; }
        Require(refused,"Malformed mixed-surface pair accepted");
        std::cout << "World material tests passed: " << checks << " checks\n"; return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
