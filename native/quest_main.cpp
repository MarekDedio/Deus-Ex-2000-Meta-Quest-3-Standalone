#include <openxr/openxr.h>

#include <cstdint>
#include <cstdio>
#include <vector>

#include "Input/ControllerRenderer.h"
#include "Render/GeometryBuilder.h"
#include "Render/GeometryRenderer.h"
#include "XrApp.h"

class DeusExQuestApp final : public OVRFW::XrApp {
   public:
    DeusExQuestApp() {
        BackgroundColor = OVR::Vector4f(0.005f, 0.01f, 0.008f, 1.0f);
    }

    bool SessionInit() override {
        if (!LoadWorldMesh()) {
            OVRFW::GeometryBuilder geometry;
            geometry.Add(
                OVRFW::BuildUnitCubeDescriptor(),
                OVRFW::GeometryBuilder::kInvalidIndex,
                OVR::Vector4f(0.8f, 0.1f, 0.1f, 1.0f),
                OVR::Matrix4f::Translation(0.0f, 1.35f, -2.0f) *
                    OVR::Matrix4f::Scaling(0.35f, 0.35f, 0.35f));
            worldRenderers_.resize(1);
            worldRenderers_[0].Init(geometry.ToGeometryDescriptor());
            ALOG("DeusExQuest: world mesh unavailable; showing error cube");
        }

        if (!leftController_.Init(true) || !rightController_.Init(false)) {
            ALOG("DeusExQuest: controller renderer initialization failed");
            return false;
        }
        ALOG("DeusExQuest: project-owned OpenXR runtime initialized");
        return true;
    }

    void Update(const OVRFW::ovrApplFrameIn& frame) override {
        for (auto& renderer : worldRenderers_) renderer.Update();
        if (frame.LeftRemoteTracked) leftController_.Update(frame.LeftRemotePose);
        if (frame.RightRemoteTracked) rightController_.Update(frame.RightRemotePose);
    }

    void Render(
        const OVRFW::ovrApplFrameIn& frame,
        OVRFW::ovrRendererOutput& output) override {
        for (auto& renderer : worldRenderers_) renderer.Render(output.Surfaces);
        if (frame.LeftRemoteTracked) leftController_.Render(output.Surfaces);
        if (frame.RightRemoteTracked) rightController_.Render(output.Surfaces);
    }

    void SessionEnd() override {
        leftController_.Shutdown();
        rightController_.Shutdown();
        for (auto& renderer : worldRenderers_) renderer.Shutdown();
        worldRenderers_.clear();
    }

   private:
    struct MeshVertex {
        float px, py, pz;
        float nx, ny, nz;
    };

    bool LoadWorldMesh() {
        constexpr const char* path =
            "/data/user/0/dev.deusex.questvr.smoketest/files/DeusEx/quest-world.mesh";
        std::FILE* file = std::fopen(path, "rb");
        if (file == nullptr) return false;
        std::uint32_t magic{}, version{}, chunkCount{};
        bool ok = std::fread(&magic, sizeof(magic), 1, file) == 1 &&
            std::fread(&version, sizeof(version), 1, file) == 1 &&
            std::fread(&chunkCount, sizeof(chunkCount), 1, file) == 1 &&
            magic == 0x4d515844u && version == 1 && chunkCount > 0 && chunkCount < 128;
        if (ok) worldRenderers_.resize(chunkCount);
        for (std::uint32_t chunk = 0; ok && chunk < chunkCount; ++chunk) {
            std::uint32_t vertexCount{};
            ok = std::fread(&vertexCount, sizeof(vertexCount), 1, file) == 1 &&
                vertexCount > 0 && vertexCount <= 60000 && vertexCount % 3 == 0;
            std::vector<MeshVertex> vertices(vertexCount);
            if (ok) ok = std::fread(
                vertices.data(), sizeof(MeshVertex), vertexCount, file) == vertexCount;
            if (!ok) break;

            OVRFW::GlGeometry::Descriptor descriptor;
            descriptor.attribs.position.reserve(vertexCount);
            descriptor.attribs.normal.reserve(vertexCount);
            descriptor.attribs.color.reserve(vertexCount);
            descriptor.indices.reserve(vertexCount);
            for (std::uint32_t index = 0; index < vertexCount; ++index) {
                const MeshVertex& vertex = vertices[index];
                descriptor.attribs.position.emplace_back(vertex.px, vertex.py, vertex.pz);
                descriptor.attribs.normal.emplace_back(vertex.nx, vertex.ny, vertex.nz);
                const float shade = 0.25f + 0.55f * (vertex.nz * 0.5f + 0.5f);
                descriptor.attribs.color.emplace_back(0.15f * shade, 0.8f * shade, 0.55f * shade, 1.0f);
                descriptor.indices.push_back(static_cast<OVRFW::TriangleIndex>(index));
            }
            worldRenderers_[chunk].Init(descriptor);
            worldRenderers_[chunk].AmbientLightColor = {0.35f, 0.35f, 0.35f};
        }
        std::fclose(file);
        if (!ok) {
            for (auto& renderer : worldRenderers_) renderer.Shutdown();
            worldRenderers_.clear();
            return false;
        }
        ALOG("DeusExQuest: loaded training BSP mesh in %u GPU chunks", chunkCount);
        return true;
    }

    std::vector<OVRFW::GeometryRenderer> worldRenderers_;
    OVRFW::ControllerRenderer leftController_;
    OVRFW::ControllerRenderer rightController_;
};

ENTRY_POINT(DeusExQuestApp)
