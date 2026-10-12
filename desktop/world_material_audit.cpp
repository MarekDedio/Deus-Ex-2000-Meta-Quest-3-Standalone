#include "Precomp.h"
#include "surreal_portable_package_tables.h"
#include "portable_model_geometry.h"
#include "quest_world_material.h"
#include <algorithm>
#include <limits>
#include <iostream>
#include <map>

int main(int argc, char** argv) {
    try {
        if (argc != 2) throw std::runtime_error("Usage: world_material_audit GAME_ROOT");
        const auto map = LoadPortablePackageTables(std::string(argv[1])+"/Maps/00_Training.dx");
        const auto model = LoadPortableRootModel68(map);
        if (model.surfaces.size() <= 1942u || model.surfaces[1942].polyFlags != 0x10cu ||
            GetPortableObjectPath(map,model.surfaces[1942].materialReference) != "CoreTexGlass.Glass.WindOpacStrek_A")
            throw std::runtime_error("Original opening Training glass fixture changed");
        const auto glass = QuestVr::ResolveOriginalMaterialState(model.surfaces[1942].polyFlags);
        if (glass.blend != QuestVr::OriginalMaterialBlend::Translucent || glass.masked || glass.depthWrite)
            throw std::runtime_error("Original Training glass resolved as opaque/masked/depth-writing");
        PortableModelVec3 low{INFINITY,INFINITY,INFINITY}, high{-INFINITY,-INFINITY,-INFINITY};
        unsigned vertices{};
        for (const auto& node : model.nodes) if (node.surface == 1942) {
            for (unsigned v=0; v<node.vertexCount; ++v) {
                const auto& point=model.points.at(static_cast<std::size_t>(model.vertices.at(static_cast<std::size_t>(node.vertexPool)+v).point));
                low.x=std::min(low.x,point.x); low.y=std::min(low.y,point.y); low.z=std::min(low.z,point.z);
                high.x=std::max(high.x,point.x); high.y=std::max(high.y,point.y); high.z=std::max(high.z,point.z);
                ++vertices;
            }
        }
        const auto& normal=model.vectors.at(static_cast<std::size_t>(model.surfaces[1942].normalVector));
        std::cout << "TRAINING_GLASS_REGRESSION surface=1942 vertices=" << vertices << " normal=(" << normal.x << ',' << normal.y << ',' << normal.z
            << ") min=(" << low.x << ',' << low.y << ',' << low.z << ") max=(" << high.x << ',' << high.y << ',' << high.z << ")\n";
        std::map<std::pair<std::string,std::uint32_t>,unsigned> materials;
        for (std::size_t i=0; i<model.surfaces.size(); ++i) {
            const auto& surface = model.surfaces[i];
            if ((surface.polyFlags&(4u|64u|0x10000000u)) == 0u) continue;
            const auto material = GetPortableObjectPath(map,surface.materialReference);
            ++materials[{material,surface.polyFlags}];
            if (material.find("Glass") == std::string::npos && material.find("glass") == std::string::npos) continue;
            const auto& p = model.points.at(static_cast<std::size_t>(surface.basePoint));
            std::cout << "SURFACE " << i << " material=" << material << " flags=0x" << std::hex << surface.polyFlags << std::dec
                << " base=(" << p.x << ',' << p.y << ',' << p.z << ")\n";
        }
        for (const auto& [key,count] : materials)
            std::cout << "MATERIAL " << key.first << " flags=0x" << std::hex << key.second << std::dec << " surfaces=" << count << '\n';
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
