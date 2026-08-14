#include <openxr/openxr.h>

#include <algorithm>
#include <cstdint>
#include <cmath>
#include <cstdio>
#include <limits>
#include <unordered_map>
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
        float headYaw{}, headPitch{}, headRoll{};
        frame.HeadPose.Rotation.GetEulerAngles<OVR::Axis_Y, OVR::Axis_X, OVR::Axis_Z>(
            &headYaw, &headPitch, &headRoll);
        (void)headPitch;
        (void)headRoll;
        const float yaw = headYaw - sceneYaw_;
        const float forwardX = std::sin(yaw);
        const float forwardZ = -std::cos(yaw);
        const float rightX = std::cos(yaw);
        const float rightZ = std::sin(yaw);
        constexpr float moveSpeed = 2.2f;
        const float moveX = frame.LeftRemoteJoystick.x * rightX +
            frame.LeftRemoteJoystick.y * forwardX;
        const float moveZ = frame.LeftRemoteJoystick.x * rightZ +
            frame.LeftRemoteJoystick.y * forwardZ;
        OVR::Vector3f candidate = worldPosition_;
        candidate.x -= moveX * moveSpeed * frame.DeltaSeconds;
        candidate.z -= moveZ * moveSpeed * frame.DeltaSeconds;

        const bool turnPressed = std::fabs(frame.RightRemoteJoystick.x) > 0.7f;
        if (turnPressed && !turnLatch_) {
            constexpr float snapRadians = 3.14159265358979323846f / 6.0f;
            sceneYaw_ -= std::copysign(snapRadians, frame.RightRemoteJoystick.x);
        }
        turnLatch_ = turnPressed;

        FollowGround(frame.HeadPose.Translation, candidate);
        if (!CapsuleTouchesWall(frame.HeadPose.Translation, candidate)) {
            worldPosition_ = candidate;
        } else {
            OVR::Vector3f grounded = worldPosition_;
            FollowGround(frame.HeadPose.Translation, grounded);
            worldPosition_.y = grounded.y;
        }

        const OVR::Posef worldPose(
            OVR::Quatf(OVR::Vector3f(0.0f, 1.0f, 0.0f), sceneYaw_), worldPosition_);
        for (auto& renderer : worldRenderers_) {
            renderer.SetPose(worldPose);
            renderer.Update();
        }
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
        collisionTriangles_.clear();
        collisionGrid_.clear();
        oversizedCollisionTriangles_.clear();
    }

   private:
    struct MeshVertex {
        float px, py, pz;
        float nx, ny, nz;
    };

    struct CollisionTriangle {
        OVR::Vector3f a;
        OVR::Vector3f b;
        OVR::Vector3f c;
        OVR::Vector3f normal;
    };

    static OVR::Vector3f Subtract(const OVR::Vector3f& a, const OVR::Vector3f& b) {
        return {a.x - b.x, a.y - b.y, a.z - b.z};
    }

    static OVR::Vector3f Add(const OVR::Vector3f& a, const OVR::Vector3f& b) {
        return {a.x + b.x, a.y + b.y, a.z + b.z};
    }

    static OVR::Vector3f Scale(const OVR::Vector3f& value, float scale) {
        return {value.x * scale, value.y * scale, value.z * scale};
    }

    static float Dot(const OVR::Vector3f& a, const OVR::Vector3f& b) {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }

    static float LengthSquared(const OVR::Vector3f& value) { return Dot(value, value); }

    static std::int64_t CollisionCellKey(int x, int z) {
        const std::uint64_t bits =
            (static_cast<std::uint64_t>(static_cast<std::uint32_t>(x)) << 32u) |
            static_cast<std::uint32_t>(z);
        return static_cast<std::int64_t>(bits);
    }

    static int CollisionCell(float coordinate) {
        constexpr float cellSize = 2.0f;
        return static_cast<int>(std::floor(coordinate / cellSize));
    }

    void BuildCollisionGrid() {
        collisionGrid_.clear();
        oversizedCollisionTriangles_.clear();
        for (std::uint32_t index = 0; index < collisionTriangles_.size(); ++index) {
            const CollisionTriangle& triangle = collisionTriangles_[index];
            const int minX = CollisionCell(std::min({triangle.a.x, triangle.b.x, triangle.c.x}));
            const int maxX = CollisionCell(std::max({triangle.a.x, triangle.b.x, triangle.c.x}));
            const int minZ = CollisionCell(std::min({triangle.a.z, triangle.b.z, triangle.c.z}));
            const int maxZ = CollisionCell(std::max({triangle.a.z, triangle.b.z, triangle.c.z}));
            const std::int64_t cellCount =
                static_cast<std::int64_t>(maxX - minX + 1) * (maxZ - minZ + 1);
            if (cellCount > 4096) {
                oversizedCollisionTriangles_.push_back(index);
                continue;
            }
            for (int x = minX; x <= maxX; ++x) {
                for (int z = minZ; z <= maxZ; ++z) {
                    collisionGrid_[CollisionCellKey(x, z)].push_back(index);
                }
            }
        }
    }

    template <typename Callback>
    void ForNearbyTriangles(
        const OVR::Vector3f& point,
        int cellRadius,
        Callback callback) const {
        const int centerX = CollisionCell(point.x);
        const int centerZ = CollisionCell(point.z);
        for (int x = centerX - cellRadius; x <= centerX + cellRadius; ++x) {
            for (int z = centerZ - cellRadius; z <= centerZ + cellRadius; ++z) {
                const auto found = collisionGrid_.find(CollisionCellKey(x, z));
                if (found == collisionGrid_.end()) continue;
                for (std::uint32_t index : found->second) callback(collisionTriangles_[index]);
            }
        }
        for (std::uint32_t index : oversizedCollisionTriangles_) {
            callback(collisionTriangles_[index]);
        }
    }

    static OVR::Vector3f ClosestPointOnTriangle(
        const OVR::Vector3f& point,
        const CollisionTriangle& triangle) {
        const OVR::Vector3f ab = Subtract(triangle.b, triangle.a);
        const OVR::Vector3f ac = Subtract(triangle.c, triangle.a);
        const OVR::Vector3f ap = Subtract(point, triangle.a);
        const float d1 = Dot(ab, ap);
        const float d2 = Dot(ac, ap);
        if (d1 <= 0.0f && d2 <= 0.0f) return triangle.a;

        const OVR::Vector3f bp = Subtract(point, triangle.b);
        const float d3 = Dot(ab, bp);
        const float d4 = Dot(ac, bp);
        if (d3 >= 0.0f && d4 <= d3) return triangle.b;

        const float vc = d1 * d4 - d3 * d2;
        if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f) {
            return Add(triangle.a, Scale(ab, d1 / (d1 - d3)));
        }

        const OVR::Vector3f cp = Subtract(point, triangle.c);
        const float d5 = Dot(ab, cp);
        const float d6 = Dot(ac, cp);
        if (d6 >= 0.0f && d5 <= d6) return triangle.c;

        const float vb = d5 * d2 - d1 * d6;
        if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f) {
            return Add(triangle.a, Scale(ac, d2 / (d2 - d6)));
        }

        const float va = d3 * d6 - d5 * d4;
        if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f) {
            const OVR::Vector3f edge = Subtract(triangle.c, triangle.b);
            return Add(triangle.b, Scale(edge, (d4 - d3) / ((d4 - d3) + (d5 - d6))));
        }

        const float denominator = 1.0f / (va + vb + vc);
        return Add(triangle.a, Add(Scale(ab, vb * denominator), Scale(ac, vc * denominator)));
    }

    OVR::Vector3f StageToLocal(
        const OVR::Vector3f& stage,
        const OVR::Vector3f& worldPosition) const {
        const float cosine = std::cos(sceneYaw_);
        const float sine = std::sin(sceneYaw_);
        const float dx = stage.x - worldPosition.x;
        const float dz = stage.z - worldPosition.z;
        return {
            cosine * dx - sine * dz,
            stage.y - worldPosition.y,
            sine * dx + cosine * dz};
    }

    void FollowGround(const OVR::Vector3f& head, OVR::Vector3f& worldPosition) const {
        const OVR::Vector3f feetStage{head.x, 0.0f, head.z};
        const OVR::Vector3f feetLocal = StageToLocal(feetStage, worldPosition);
        float bestFloor = -std::numeric_limits<float>::infinity();
        ForNearbyTriangles(feetLocal, 0, [&](const CollisionTriangle& triangle) {
            if (std::fabs(triangle.normal.y) < 0.55f) return;
            const float denominator =
                (triangle.b.z - triangle.c.z) * (triangle.a.x - triangle.c.x) +
                (triangle.c.x - triangle.b.x) * (triangle.a.z - triangle.c.z);
            if (std::fabs(denominator) < 0.000001f) return;
            const float u = ((triangle.b.z - triangle.c.z) * (feetLocal.x - triangle.c.x) +
                (triangle.c.x - triangle.b.x) * (feetLocal.z - triangle.c.z)) / denominator;
            const float v = ((triangle.c.z - triangle.a.z) * (feetLocal.x - triangle.c.x) +
                (triangle.a.x - triangle.c.x) * (feetLocal.z - triangle.c.z)) / denominator;
            const float w = 1.0f - u - v;
            if (u < -0.001f || v < -0.001f || w < -0.001f) return;
            const float floor = u * triangle.a.y + v * triangle.b.y + w * triangle.c.y;
            if (floor <= feetLocal.y + 0.45f && floor >= feetLocal.y - 2.0f) {
                bestFloor = std::max(bestFloor, floor);
            }
        });
        if (std::isfinite(bestFloor)) worldPosition.y = -bestFloor;
    }

    bool CapsuleTouchesWall(
        const OVR::Vector3f& head,
        const OVR::Vector3f& worldPosition) const {
        constexpr float radiusSquared = 0.28f * 0.28f;
        constexpr float sampleHeights[] = {0.3f, 0.85f, 1.4f};
        const OVR::Vector3f center = StageToLocal({head.x, 0.85f, head.z}, worldPosition);
        bool touching = false;
        ForNearbyTriangles(center, 1, [&](const CollisionTriangle& triangle) {
            if (touching || std::fabs(triangle.normal.y) > 0.65f) return;
            for (float height : sampleHeights) {
                const OVR::Vector3f point = StageToLocal({head.x, height, head.z}, worldPosition);
                if (LengthSquared(Subtract(point, ClosestPointOnTriangle(point, triangle))) <
                    radiusSquared) {
                    touching = true;
                    return;
                }
            }
        });
        return touching;
    }

    bool LoadWorldMesh() {
        constexpr const char* path =
            "/data/user/0/dev.deusex.questvr.smoketest/files/DeusEx/quest-world.mesh";
        std::FILE* file = std::fopen(path, "rb");
        if (file == nullptr) return false;
        collisionTriangles_.clear();
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
            for (std::uint32_t index = 0; index + 2 < vertexCount; index += 6) {
                const MeshVertex& va = vertices[index];
                const MeshVertex& vb = vertices[index + 1];
                const MeshVertex& vc = vertices[index + 2];
                collisionTriangles_.push_back({
                    {va.px, va.py, va.pz},
                    {vb.px, vb.py, vb.pz},
                    {vc.px, vc.py, vc.pz},
                    {va.nx, va.ny, va.nz}});
            }
            worldRenderers_[chunk].Init(descriptor);
            worldRenderers_[chunk].AmbientLightColor = {0.35f, 0.35f, 0.35f};
        }
        std::fclose(file);
        if (!ok) {
            for (auto& renderer : worldRenderers_) renderer.Shutdown();
            worldRenderers_.clear();
            collisionTriangles_.clear();
            collisionGrid_.clear();
            oversizedCollisionTriangles_.clear();
            return false;
        }
        BuildCollisionGrid();
        ALOG(
            "DeusExQuest: loaded training BSP mesh in %u GPU chunks with %zu collision triangles in %zu cells",
            chunkCount,
            collisionTriangles_.size(),
            collisionGrid_.size());
        return true;
    }

    std::vector<OVRFW::GeometryRenderer> worldRenderers_;
    std::vector<CollisionTriangle> collisionTriangles_;
    std::unordered_map<std::int64_t, std::vector<std::uint32_t>> collisionGrid_;
    std::vector<std::uint32_t> oversizedCollisionTriangles_;
    OVR::Vector3f worldPosition_{0.0f, 0.0f, 0.0f};
    float sceneYaw_{};
    bool turnLatch_{};
    OVRFW::ControllerRenderer leftController_;
    OVRFW::ControllerRenderer rightController_;
};

ENTRY_POINT(DeusExQuestApp)
