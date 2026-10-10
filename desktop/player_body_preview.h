#pragma once

#include "quest_player_visual.h"
#include "visual_renderer.h"

#include <filesystem>
#include <iostream>
#include <map>

namespace questvisual {

// Isolated CPU diagnostic using the same selected source triangles, native
// texture dimensions/UVs and culling policy as Quest. Not an OpenXR test.
inline Scene PlayerBodyTextureScene(const QuestVr::PlayerVisualAssets& assets,std::size_t texture) {
    Scene scene;const auto& image=assets.textures.at(texture).image;
    scene.actorTextureWidth=image.width;scene.actorTextureHeight=image.height;
    scene.actorTextureLayers=1u;scene.actorTextures=image.rgba;
    std::map<std::uint32_t,Chunk> groups;
    for (const auto& triangle:assets.lowerBody.triangles) {
        if (triangle.textureIndex!=texture) continue;
        auto flags=triangle.polyFlags|assets.textures.at(texture).polyFlags;
        if (!QuestVr::CullPlayerVisualPart(0u,flags)) flags|=kPolyTwoSided;
        auto& chunk=groups[flags];chunk.textureBank=TextureBank::Actor;chunk.polyFlags=flags;
        for (const auto& vertex:triangle.vertices)
            chunk.vertices.push_back({{vertex.position.x,vertex.position.y,vertex.position.z},
                {vertex.normal.x,vertex.normal.y,vertex.normal.z},vertex.u,vertex.v,0});
    }
    for (auto& [flags,chunk]:groups) {(void)flags;scene.chunks.push_back(std::move(chunk));}
    return scene;
}

inline RenderResult CapturePlayerBody(const QuestVr::PlayerVisualAssets& assets,const Camera& camera) {
    RenderResult combined;
    for (std::size_t texture=0u;texture<assets.textures.size();++texture) {
        const auto scene=PlayerBodyTextureScene(assets,texture);if (scene.chunks.empty()) continue;
        const auto layer=Render(scene,camera,640u,640u);
        if (combined.image.rgb.empty()) {combined=layer;continue;}
        combined.inputTriangles+=layer.inputTriangles;combined.rasterizedTriangles+=layer.rasterizedTriangles;
        for (std::size_t pixel=0u;pixel<layer.depth.size();++pixel) {
            if (layer.depth[pixel]>=combined.depth[pixel]) continue;
            combined.depth[pixel]=layer.depth[pixel];
            for (std::size_t channel=0u;channel<3u;++channel)
                combined.image.rgb[pixel*3u+channel]=layer.image.rgb[pixel*3u+channel];
        }
    }
    if (combined.image.rgb.empty()) throw std::runtime_error("Selected player body has no previewable texture surfaces");
    combined.coveredPixels=0u;for (const auto depth:combined.depth) combined.coveredPixels+=std::isfinite(depth);
    return combined;
}

inline void PreviewOriginalPlayerBody(const QuestVr::PlayerVisualAssets& assets,const std::filesystem::path& directory) {
    std::filesystem::create_directories(directory);
    const auto capture=[&](const std::string& name,const Camera& camera) {
        const auto result=CapturePlayerBody(assets,camera);WriteBmp(directory/(name+".bmp"),result.image);
        std::cout<<"BODY PREVIEW "<<name<<" camera="<<camera.position.x<<','<<camera.position.y<<','<<camera.position.z<<
            " pitch="<<camera.pitchDegrees<<" yaw="<<camera.yawDegrees<<" sourceFaces="<<result.inputTriangles<<
            " rasterized="<<result.rasterizedTriangles<<" coveredPixels="<<result.coveredPixels<<'\n';
    };
    for (const float clearance:{0.0f,0.12f,0.16f}) for (const int pitch:{-45,-65,-85}) {
        Camera camera;camera.position={0.0f,1.65f,-clearance};camera.pitchDegrees=static_cast<float>(pitch);
        capture("body-down"+std::to_string(-pitch)+"-rear"+std::to_string(static_cast<int>(clearance*100.0f)),camera);
    }
    for (const int yaw:{-45,45,180}) {
        Camera camera;camera.position={0.0f,1.65f,-0.16f};camera.pitchDegrees=-85.0f;camera.yawDegrees=static_cast<float>(yaw);
        capture("body-down85-yaw"+std::to_string(yaw)+"-rear16",camera);
    }
    Camera front;front.position={0.0f,1.0f,-2.0f};front.yawDegrees=180.0f;capture("body-exterior-front",front);
    Camera back;back.position={0.0f,1.0f,2.0f};capture("body-exterior-back",back);
}

} // namespace questvisual
