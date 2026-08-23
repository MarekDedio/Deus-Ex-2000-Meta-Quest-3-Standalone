#include <openxr/openxr.h>
#include <GLES3/gl3.h>
#include <aaudio/AAudio.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cmath>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <future>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <numeric>
#include <set>
#include <unordered_map>
#include <vector>

#include "Input/ControllerRenderer.h"
#include "Input/TinyUI.h"
#include "Render/GeometryBuilder.h"
#include "Render/GeometryRenderer.h"
#include "Render/GlTexture.h"
#include "Render/SurfaceRender.h"
#include "XrApp.h"
#include "portable_unreal_runtime.h"
#include "quest_map_cache.h"

#define MINIMP3_IMPLEMENTATION
#include "minimp3_ex.h"

class TexturedGeometryRenderer {
   public:
    void Init(
        const OVRFW::GlGeometry::Descriptor& descriptor,
        const OVRFW::GlTexture& texture) {
        static const char* vertexShader = R"glsl(
            attribute highp vec4 Position;
            attribute highp vec2 TexCoord;
            attribute lowp vec4 VertexColor;
            varying lowp vec2 oTexCoord;
            varying mediump float oLayer;
            void main() {
                gl_Position = TransformVertex(Position);
                oTexCoord = TexCoord;
                oLayer = VertexColor.r * 255.0;
            }
        )glsl";
        static const char* fragmentShader = R"glsl(
            precision lowp float;
            uniform highp sampler2DArray Texture0;
            varying lowp vec2 oTexCoord;
            varying mediump float oLayer;
            void main() {
                gl_FragColor = texture(Texture0, vec3(fract(oTexCoord), floor(oLayer + 0.5)));
            }
        )glsl";
        static OVRFW::ovrProgramParm uniformParms[] = {
            {.Name = "Texture0", .Type = OVRFW::ovrProgramParmType::TEXTURE_SAMPLED},
        };
        if (!sharedProgram_.IsValid()) {
            sharedProgram_ = OVRFW::GlProgram::Build(
                "", vertexShader, "", fragmentShader, uniformParms, 1);
        }
        surface_.geo = OVRFW::GlGeometry(descriptor.attribs, descriptor.indices);
        OVRFW::ovrGraphicsCommand& command = surface_.graphicsCommand;
        command.Program = sharedProgram_;
        command.Textures[0] = texture;
        command.UniformData[0].Data = &command.Textures[0];
        command.GpuState.depthEnable = command.GpuState.depthMaskEnable = true;
        command.GpuState.blendEnable = OVRFW::ovrGpuState::BLEND_DISABLE;
    }

    void Shutdown() {
        surface_.geo.Free();
    }
    static void ShutdownSharedProgram() {
        if (sharedProgram_.IsValid()) OVRFW::GlProgram::Free(sharedProgram_);
    }
    void SetPose(const OVR::Posef& pose) { pose_ = pose; }
    void Update() {
        pose_.Rotation.Normalize();
        modelMatrix_ = OVR::Matrix4f(pose_);
    }
    void Render(std::vector<OVRFW::ovrDrawSurface>& surfaces) {
        surface_.graphicsCommand.UniformData[0].Data =
            &surface_.graphicsCommand.Textures[0];
        surfaces.emplace_back(modelMatrix_, &surface_);
    }

   private:
    OVRFW::ovrSurfaceDef surface_;
    inline static OVRFW::GlProgram sharedProgram_;
    OVR::Posef pose_ = OVR::Posef::Identity();
    OVR::Matrix4f modelMatrix_ = OVR::Matrix4f::Identity();
};

class DeusExQuestApp final : public OVRFW::XrApp {
   public:
    DeusExQuestApp() {
        BackgroundColor = OVR::Vector4f(0.005f, 0.01f, 0.008f, 1.0f);
    }

    bool AppInit(const xrJava* context) override {
        if (!ui_.Init(
                context,
                GetFileSys(),
                true,
                16 * 1024,
                "apk://localhost/assets/efigs.fnt")) {
            ALOG("DeusExQuest: TinyUI initialization failed");
            return false;
        }
        hudLabel_ = ui_.AddLabel(
            "DEUS EX VR",
            OVR::Vector3f(0.0f, 1.3f, -1.5f),
            OVR::Vector2f(700.0f, 110.0f));
        hudLabel_->SetTextLocalPosition({-0.32f, -0.025f, 0.0f});
        return true;
    }

    void AppShutdown(const xrJava* context) override {
        hudLabel_ = nullptr;
        ui_.Shutdown();
        OVRFW::XrApp::AppShutdown(context);
    }

    bool SessionInit() override {
        try {
            static constexpr const char* packageNames[] = {
                "ConSys",
                "Core",
                "DeusEx",
                "DeusExCharacters",
                "DeusExConAudioAIBarks",
                "DeusExConAudioEndGame",
                "DeusExConAudioHK_Shared",
                "DeusExConAudioIntro",
                "DeusExConAudioMission00",
                "DeusExConAudioMission01",
                "DeusExConAudioMission02",
                "DeusExConAudioMission03",
                "DeusExConAudioMission04",
                "DeusExConAudioMission05",
                "DeusExConAudioMission08",
                "DeusExConAudioMission09",
                "DeusExConAudioMission10",
                "DeusExConAudioMission11",
                "DeusExConAudioMission12",
                "DeusExConAudioMission14",
                "DeusExConAudioMission15",
                "DeusExConAudioNYShared",
                "DeusExConText",
                "DeusExConversations",
                "DeusExDeco",
                "DeusExItems",
                "DeusExSounds",
                "DeusExText",
                "DeusExUI",
                "Editor",
                "Engine",
                "Extension",
                "Fire",
                "IpDrv",
                "IpServer",
                "MPCharacters",
                "UBrowser",
                "UWindow"};
            std::vector<PortablePackageTables> scripts;
            scripts.reserve(sizeof(packageNames) / sizeof(packageNames[0]));
            for (const char* packageName : packageNames) {
                scripts.push_back(LoadPortablePackageTables(
                    std::string(
                        "/data/user/0/dev.deusex.questvr.smoketest/files/DeusEx/System/") +
                    packageName + ".u"));
            }
            const PortableRuntimeSummary runtime = InitializePortableRuntime(scripts);
            if (!runtime.passed) {
                ALOG("DeusExQuest: persistent Unreal runtime validation failed");
                return false;
            }
            ALOG(
                "DeusExQuest: persistent Unreal runtime ready: %zu objects, %zu classes, %zu functions, %zu properties, %zu bytecode bytes, %zu class defaults/%zu default properties, %zu links resolved/%zu external",
                runtime.objects,
                runtime.classes,
                runtime.functions,
                runtime.properties,
                runtime.normalizedBytecodeBytes,
                runtime.serializedClassDefaults,
                runtime.classDefaultProperties,
                runtime.resolvedLinks,
                runtime.unresolvedExternalLinks);
            const PortableConversationSummary conversations =
                GetPortableConversationSummary();
            if (runtime.conversationObjects == 0u ||
                runtime.conversationLoadFailures != 0u ||
                conversations.conversations == 0u || conversations.events == 0u ||
                conversations.speechLines == 0u) {
                ALOG(
                    "DeusExQuest: portable conversation-data validation failed: loaded=%zu failures=%zu conversations=%zu events=%zu speech=%zu",
                    runtime.conversationObjects,
                    runtime.conversationLoadFailures,
                    conversations.conversations,
                    conversations.events,
                    conversations.speechLines);
                return false;
            }
            ALOG(
                "DeusExQuest: portable conversation data ready: %zu objects/%zu properties, %zu conversations, %zu events, %zu speech objects/%zu lines; sample=%s",
                runtime.conversationObjects,
                runtime.conversationProperties,
                conversations.conversations,
                conversations.events,
                conversations.speechObjects,
                conversations.speechLines,
                conversations.sampleSpeech.c_str());
            const PortablePackageTables trainingMap = LoadPortablePackageTables(
                "/data/user/0/dev.deusex.questvr.smoketest/files/DeusEx/Maps/00_Training.dx");
            const PortableMapRuntimeSummary mapRuntime =
                LoadPortableRuntimeMap(trainingMap);
            if (!mapRuntime.passed) {
                ALOG("DeusExQuest: live training actor runtime validation failed");
                return false;
            }
            ALOG(
                "DeusExQuest: live training map ready: %zu exports, %zu actors, %zu instance properties, %zu classes resolved/%zu unresolved",
                mapRuntime.exports,
                mapRuntime.actors,
                mapRuntime.actorProperties,
                mapRuntime.resolvedClasses,
                mapRuntime.unresolvedClasses);
            const PortablePackageTables combatMap = LoadPortablePackageTables(
                "/data/user/0/dev.deusex.questvr.smoketest/files/DeusEx/Maps/00_TrainingCombat.dx");
            const PortableMapRuntimeSummary combatRuntime =
                LoadPortableRuntimeMap(combatMap);
            const PortablePackageTables finalMap = LoadPortablePackageTables(
                "/data/user/0/dev.deusex.questvr.smoketest/files/DeusEx/Maps/00_TrainingFinal.dx");
            const PortableMapRuntimeSummary finalRuntime =
                LoadPortableRuntimeMap(finalMap);
            const PortableMapRuntimeSummary restoredTraining =
                LoadPortableRuntimeMap(trainingMap);
            if (!combatRuntime.passed || !finalRuntime.passed ||
                !restoredTraining.passed || combatRuntime.replacedExports != mapRuntime.exports ||
                finalRuntime.replacedExports != combatRuntime.exports ||
                restoredTraining.replacedExports != finalRuntime.exports) {
                ALOG("DeusExQuest: training map transition validation failed");
                return false;
            }
            ALOG(
                "DeusExQuest: runtime map replacement verified: Training(%zu actors) -> Combat(%zu) -> Final(%zu) -> Training(%zu), prior worlds collected",
                mapRuntime.actors,
                combatRuntime.actors,
                finalRuntime.actors,
                restoredTraining.actors);
            const PortableActorMeshSummary actorMeshes =
                DecodePortableRuntimeActorMeshes();
            if (!actorMeshes.passed) {
                ALOG("DeusExQuest: actor LodMesh decode validation failed");
                return false;
            }
            ALOG(
                "DeusExQuest: decoded %zu/%zu actor LodMeshes with %zu triangle vertices",
                actorMeshes.decodedMeshes,
                actorMeshes.referencedMeshes,
                actorMeshes.triangleVertices);
            if (!VerifyPortableRuntimeInteraction()) {
                ALOG("DeusExQuest: live pickup/inventory mutation validation failed");
                return false;
            }
            ALOG("DeusExQuest: live pickup/inventory mutation verified and restored");
            if (!VerifyPortableRuntimeDamage()) {
                ALOG("DeusExQuest: live pawn damage mutation validation failed");
                return false;
            }
            ALOG("DeusExQuest: live pawn damage/death mutation verified and restored");
            constexpr const char* validationSave =
                "/data/user/0/dev.deusex.questvr.smoketest/files/DeusEx/quest-runtime-validation.sav";
            if (!SavePortableRuntimeState(validationSave) ||
                !LoadPortableRuntimeState(validationSave)) {
                ALOG("DeusExQuest: portable runtime save/load validation failed");
                return false;
            }
            ALOG("DeusExQuest: portable runtime save/load round trip verified");
            const PortableVmValue stomp =
                ExecutePortableFunction("ScriptedPawn.WillTakeStompDamage");
            const PortableVmValue shield =
                ExecutePortableFunction("ScriptedPawn.ShieldDamage");
            const PortableVmValue cancel =
                ExecutePortableFunction("MenuUIChoice.CancelSetting");
            if (stomp.type != PortableVmValueType::Boolean || !stomp.boolean ||
                shield.type != PortableVmValueType::Float ||
                std::fabs(shield.floating - 1.0f) > 0.0001f ||
                cancel.type != PortableVmValueType::Nothing) {
                ALOG("DeusExQuest: portable Unreal VM result validation failed");
                return false;
            }
            ALOG(
                "DeusExQuest: executed real UnrealScript returns: WillTakeStompDamage=true, ShieldDamage=%.1f, CancelSetting=void",
                shield.floating);
        } catch (const std::exception& error) {
            ALOG("DeusExQuest: persistent Unreal runtime failed: %s", error.what());
            return false;
        }
        LoadMapCatalog();
        if (!BuildQuestMapCache(gameRoot_, currentMapName_.c_str())) {
            ALOG("DeusExQuest: generic initial map cache build failed");
            return false;
        }
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
        actorSnapshots_ = GetPortableRuntimeMapActors();
        if (!LoadActorTextures()) {
            ALOG("DeusExQuest: actor texture array unavailable; using marker fallback");
        }
        BuildActorMarkers();

        if (!leftController_.Init(true) || !rightController_.Init(false)) {
            ALOG("DeusExQuest: controller renderer initialization failed");
            return false;
        }
        if (!StartAmbientAudio()) {
            ALOG("DeusExQuest: ambient AAudio initialization failed");
        }
        ALOG("DeusExQuest: project-owned OpenXR runtime initialized");
        return true;
    }

    void Update(const OVRFW::ovrApplFrameIn& frame) override {
        if (interactionStatusSeconds_ > 0.0f) {
            interactionStatusSeconds_ = std::max(
                0.0f, interactionStatusSeconds_ - frame.DeltaSeconds);
            if (interactionStatusSeconds_ == 0.0f) interactionStatus_.clear();
        }
        ++performanceFrames_;
        performanceSeconds_ += frame.DeltaSeconds;
        performanceWorstDelta_ = std::max(performanceWorstDelta_, frame.DeltaSeconds);
        if (performanceSeconds_ >= 10.0f) {
            ALOG(
                "DeusExQuest: Quest frame timing %.1f fps average, %.2f ms worst over %zu frames; actors=%zu collision=%zu",
                static_cast<double>(performanceFrames_) / performanceSeconds_,
                performanceWorstDelta_ * 1000.0f,
                performanceFrames_,
                interactiveActors_.size(),
                collisionTriangles_.size());
            performanceFrames_ = 0;
            performanceSeconds_ = 0.0f;
            performanceWorstDelta_ = 0.0f;
        }
        float headYaw{}, headPitch{}, headRoll{};
        currentHeadStage_ = frame.HeadPose.Translation;
        frame.HeadPose.Rotation.GetEulerAngles<OVR::Axis_Y, OVR::Axis_X, OVR::Axis_Z>(
            &headYaw, &headPitch, &headRoll);
        (void)headPitch;
        (void)headRoll;
        const bool playerAlive = GetPortableRuntimePlayerHealth() > 0.0f;
        const float yaw = headYaw - sceneYaw_;
        const float forwardX = std::sin(yaw);
        const float forwardZ = -std::cos(yaw);
        const float rightX = std::cos(yaw);
        const float rightZ = std::sin(yaw);
        constexpr float moveSpeed = 2.2f;
        const float moveX = playerAlive
            ? frame.LeftRemoteJoystick.x * rightX + frame.LeftRemoteJoystick.y * forwardX
            : 0.0f;
        const float moveZ = playerAlive
            ? frame.LeftRemoteJoystick.x * rightZ + frame.LeftRemoteJoystick.y * forwardZ
            : 0.0f;
        OVR::Vector3f candidate = worldPosition_;
        candidate.x -= moveX * moveSpeed * frame.DeltaSeconds;
        candidate.z -= moveZ * moveSpeed * frame.DeltaSeconds;

        const bool turnPressed = std::fabs(frame.RightRemoteJoystick.x) > 0.7f;
        if (playerAlive && turnPressed && !turnLatch_) {
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
        for (auto& renderer : texturedRenderers_) {
            renderer.SetPose(worldPose);
            renderer.Update();
        }
        if (frame.LeftRemoteTracked) leftController_.Update(frame.LeftRemotePose);
        if (frame.RightRemoteTracked) rightController_.Update(frame.RightRemotePose);
        const bool mapLoading = !pendingMapName_.empty() || !transitionMapName_.empty();
        mapTravelCooldown_ = std::max(0.0f, mapTravelCooldown_ - frame.DeltaSeconds);
        if (playerAlive && !mapLoading && mapTravelCooldown_ <= 0.0f) {
            CheckTravelTriggers(frame.HeadPose.Translation);
        }
        if (!mapLoading && actorSnapshots_.size() > 1000u) {
            const OVR::Vector3f playerLocal = StageToLocal(currentHeadStage_, worldPosition_);
            const float dx = playerLocal.x - actorStreamingCenter_.x;
            const float dz = playerLocal.z - actorStreamingCenter_.z;
            if (dx * dx + dz * dz > 10.0f * 10.0f) {
                DestroyActorGeometry();
                BuildActorMarkers();
            }
        }
        if (playerAlive && !mapLoading && frame.RightRemoteTracked && frame.Clicked(frame.kButtonA)) {
            if (UseTargetedActor(frame.RightRemotePointPose)) {
                DestroyActorGeometry();
                actorSnapshots_ = GetPortableRuntimeMapActors();
                BuildActorMarkers();
            }
        }
        const bool firePressed = frame.RightRemoteIndexTrigger > 0.75f;
        if (playerAlive && !mapLoading && frame.RightRemoteTracked && firePressed && !fireLatch_) {
            const std::vector<std::string> inventory = GetPortableRuntimeInventoryItems();
            const float weaponDamage = SelectedWeaponDamage(inventory);
            if (weaponDamage <= 0.0f) {
                interactionStatus_ = "SELECT A WEAPON";
                interactionStatusSeconds_ = 2.0f;
                ALOG("DeusExQuest: VR fire ignored; selected inventory item is not a weapon");
            } else if (!SelectedWeaponHasAmmo(inventory)) {
                interactionStatus_ = "NO COMPATIBLE AMMO";
                interactionStatusSeconds_ = 2.0f;
                ALOG("DeusExQuest: VR fire ignored; selected weapon has no compatible ammo");
            } else if (FireTargetedActor(
                           frame.RightRemotePointPose,
                           weaponDamage,
                           SelectedWeaponRange(inventory))) {
                DestroyActorGeometry();
                actorSnapshots_ = GetPortableRuntimeMapActors();
                BuildActorMarkers();
            }
        }
        fireLatch_ = firePressed;
        const bool gripPressed = frame.RightRemoteGripTrigger > 0.75f;
        if (playerAlive && !mapLoading && gripPressed && !inventoryCycleLatch_) {
            const std::size_t count = GetPortableRuntimeInventoryCount();
            if (count != 0u) selectedInventoryIndex_ = (selectedInventoryIndex_ + 1u) % count;
            displayedInventoryCount_ = invalidRendererIndex_;
        }
        inventoryCycleLatch_ = gripPressed;
        if (playerAlive && !mapLoading && frame.Clicked(frame.kButtonY)) SaveGameState();
        if (!mapLoading && frame.Clicked(frame.kButtonX)) LoadGameState();
        if (playerAlive && frame.Clicked(frame.kButtonB)) LoadNextMap();
        mapRequestPollSeconds_ += frame.DeltaSeconds;
        if (mapRequestPollSeconds_ >= 0.5f) {
            mapRequestPollSeconds_ = 0.0f;
            PollMapTransitionRequest();
        }
        AdvanceMapTransition();
        CompletePendingMapLoad();
        if (hudLabel_ != nullptr) {
            OVR::Posef hudPose = frame.HeadPose;
            hudPose.Translation += frame.HeadPose.Rotation.Rotate(
                OVR::Vector3f(0.0f, -0.24f, -0.78f));
            hudLabel_->SetLocalPose(hudPose);
            const std::size_t inventoryCount = mapLoading
                ? displayedInventoryCount_
                : GetPortableRuntimeInventoryCount();
            const float playerHealth = mapLoading
                ? displayedPlayerHealth_
                : GetPortableRuntimePlayerHealth();
            const std::vector<std::string> inventory = mapLoading
                ? std::vector<std::string>{}
                : GetPortableRuntimeInventoryItems();
            const std::string selectedItem = SelectedInventoryLabel(inventory);
            if (inventoryCount != displayedInventoryCount_ ||
                std::fabs(playerHealth - displayedPlayerHealth_) > 0.01f ||
                selectedItem != displayedSelectedInventory_ ||
                interactionStatus_ != displayedInteractionStatus_) {
                hudLabel_->SetText(
                    "%s   HEALTH %.0f   INVENTORY %zu   ITEM %s\n%s\nA USE   TRIGGER FIRE   GRIP CYCLE   B NEXT MAP   Y SAVE   X LOAD",
                    pendingMapName_.empty() && transitionMapName_.empty()
                        ? currentMapName_.c_str()
                        : "LOADING...",
                    playerHealth,
                    inventoryCount,
                    selectedItem.c_str(),
                    playerHealth <= 0.0f
                        ? "DEAD - PRESS X TO QUICK-LOAD"
                        : (interactionStatus_.empty() ? "READY" : interactionStatus_.c_str()));
                displayedInventoryCount_ = inventoryCount;
                displayedPlayerHealth_ = playerHealth;
                displayedSelectedInventory_ = selectedItem;
                displayedInteractionStatus_ = interactionStatus_;
            }
        }
        ui_.Update(frame);
    }

    void Render(
        const OVRFW::ovrApplFrameIn& frame,
        OVRFW::ovrRendererOutput& output) override {
        for (auto& renderer : worldRenderers_) renderer.Render(output.Surfaces);
        for (auto& renderer : texturedRenderers_) renderer.Render(output.Surfaces);
        ui_.Render(frame, output);
        if (frame.LeftRemoteTracked) leftController_.Render(output.Surfaces);
        if (frame.RightRemoteTracked) rightController_.Render(output.Surfaces);
    }

    void SessionEnd() override {
        if (mapCacheFuture_.valid()) mapCacheFuture_.wait();
        if (pendingWorldTextureId_ != 0u) {
            glDeleteTextures(1, &pendingWorldTextureId_);
            pendingWorldTextureId_ = 0u;
        }
        pendingWorldTextureRgba_.clear();
        if (pendingActorTextureId_ != 0u) {
            glDeleteTextures(1, &pendingActorTextureId_);
            pendingActorTextureId_ = 0u;
        }
        pendingActorTextureRgba_.clear();
        pendingActorTexturePaths_.clear();
        pendingWorldMesh_.chunks.clear();
        StopAmbientAudio();
        leftController_.Shutdown();
        rightController_.Shutdown();
        for (auto& renderer : worldRenderers_) renderer.Shutdown();
        for (auto& renderer : texturedRenderers_) renderer.Shutdown();
        worldRenderers_.clear();
        texturedRenderers_.clear();
        TexturedGeometryRenderer::ShutdownSharedProgram();
        if (firstTexture_.IsValid()) {
            OVRFW::FreeTexture(firstTexture_);
            firstTexture_ = {};
        }
        if (actorTexture_.IsValid()) {
            OVRFW::FreeTexture(actorTexture_);
            actorTexture_ = {};
        }
        actorTexturePaths_.clear();
        collisionTriangles_.clear();
        collisionGrid_.clear();
        oversizedCollisionTriangles_.clear();
        actorSnapshots_.clear();
        interactiveActors_.clear();
        ShutdownPortableRuntime();
    }

   private:
    enum class MapTransitionPhase {
        Idle,
        WorldTextureAllocate,
        WorldTextureUpload,
        WorldGeometry,
        WorldGeometryUpload,
        CollisionGrid,
        ActorTextureAllocate,
        ActorTextureUpload,
        ActorGeometry
    };

    struct MeshVertex {
        float px, py, pz;
        float nx, ny, nz;
        float u, v;
        std::int32_t materialSlot;
    };

    struct WorldMeshChunkPreparation {
        std::int32_t materialSlot{};
        std::vector<MeshVertex> vertices;
    };

    struct WorldMeshPreparation {
        bool passed{};
        std::vector<WorldMeshChunkPreparation> chunks;
    };

    struct WorldTexturePreparation {
        bool passed{};
        std::uint32_t width{};
        std::uint32_t height{};
        std::uint32_t layers{};
        std::vector<std::uint8_t> rgba;
    };

    struct MapPreparation {
        bool passed{};
        std::string error;
        std::vector<PortableActorSnapshot> actors;
        PortableTextureArray actorTextures;
        WorldTexturePreparation worldTexture;
        WorldMeshPreparation worldMesh;
    };

    struct InteractiveActor {
        OVR::Vector3f localPosition;
        std::string objectPath;
        std::string classPath;
        bool travel{};
        std::string destinationMap;
    };

    OVRFW::GlGeometry::Descriptor BuildLodMeshDescriptor(
        const PortableLodMesh& mesh,
        std::uint16_t material) const {
        OVRFW::GlGeometry::Descriptor descriptor;
        descriptor.attribs.position.reserve(mesh.triangles.size());
        descriptor.attribs.normal.reserve(mesh.triangles.size());
        descriptor.attribs.uv0.reserve(mesh.triangles.size());
        descriptor.attribs.color.reserve(mesh.triangles.size());
        descriptor.indices.reserve(mesh.triangles.size());
        constexpr float unitsToMeters = 1.0f / 52.5f;
        for (std::size_t triangle = 0;
             triangle + 2 < mesh.triangles.size();
             triangle += 3) {
            if (mesh.triangles[triangle].material != material) continue;
            OVR::Vector3f positions[3];
            for (std::size_t corner = 0; corner < 3; ++corner) {
                const PortableMeshVertex& vertex = mesh.triangles[triangle + corner];
                positions[corner] = {
                    vertex.y * unitsToMeters,
                    vertex.z * unitsToMeters,
                    -vertex.x * unitsToMeters};
            }
            const OVR::Vector3f a = positions[1] - positions[0];
            const OVR::Vector3f b = positions[2] - positions[0];
            OVR::Vector3f normal{
                a.y * b.z - a.z * b.y,
                a.z * b.x - a.x * b.z,
                a.x * b.y - a.y * b.x};
            const float length = std::sqrt(normal.LengthSq());
            if (length > 0.000001f) normal *= 1.0f / length;
            for (std::size_t corner = 0; corner < 3; ++corner) {
                const PortableMeshVertex& vertex = mesh.triangles[triangle + corner];
                descriptor.attribs.position.push_back(positions[corner]);
                descriptor.attribs.normal.push_back(normal);
                descriptor.attribs.uv0.emplace_back(vertex.u, vertex.v);
                descriptor.attribs.color.emplace_back(1.0f, 1.0f, 1.0f, 1.0f);
                descriptor.indices.push_back(static_cast<OVRFW::TriangleIndex>(
                    descriptor.indices.size()));
            }
        }
        return descriptor;
    }

    void BuildActorMarkers() {
        interactiveActors_.clear();
        OVRFW::GeometryBuilder geometry;
        OVRFW::GeometryBuilder texturedGeometry;
        constexpr float unitsToMeters = 1.0f / 52.5f;
        float originX = -1149.244f;
        float originY = 825.844f;
        float originZ = -65.103f;
        std::string playerStartPath = "fallback";
        for (const PortableActorSnapshot& actor : actorSnapshots_) {
            const std::size_t separator = actor.classPath.find_last_of('.');
            const std::string leafClass = separator == std::string::npos
                ? actor.classPath
                : actor.classPath.substr(separator + 1);
            if (actor.hasLocation && leafClass == "PlayerStart") {
                originX = actor.x;
                originY = actor.y;
                originZ = actor.z;
                playerStartPath = actor.objectPath;
                break;
            }
        }
        ALOG(
            "DeusExQuest: actor coordinate origin %s at %.3f,%.3f,%.3f",
            playerStartPath.c_str(),
            originX,
            originY,
            originZ);
        std::size_t visible{};
        std::size_t meshInstances{};
        const OVR::Vector3f playerLocal = StageToLocal(currentHeadStage_, worldPosition_);
        actorStreamingCenter_ = playerLocal;
        std::map<std::string, std::size_t> meshClasses;
        std::map<std::string, OVRFW::GlGeometry::Descriptor> meshDescriptors;
        std::unordered_map<std::string, std::size_t> textureLayers;
        for (std::size_t layer = 0; layer < actorTexturePaths_.size(); ++layer) {
            textureLayers.emplace(actorTexturePaths_[layer], layer);
        }
        for (const PortableActorSnapshot& actor : actorSnapshots_) {
            if (!actor.hasLocation ||
                !(actor.pawn || actor.inventory || actor.decoration ||
                  actor.mover || actor.trigger || actor.travel)) {
                continue;
            }
            const OVR::Vector3f position(
                (actor.y - originY) * unitsToMeters,
                (actor.z - originZ) * unitsToMeters + 1.0f,
                -(actor.x - originX) * unitsToMeters);
            if (actorSnapshots_.size() > 1000u) {
                const float dx = position.x - playerLocal.x;
                const float dz = position.z - playerLocal.z;
                if (dx * dx + dz * dz > 25.0f * 25.0f) continue;
            }
            if (!actor.meshPath.empty()) ++meshClasses[actor.meshClassPath];
            OVR::Vector4f color;
            OVR::Vector3f scale;
            if (actor.pawn) {
                color = {0.75f, 0.22f, 0.12f, 1.0f};
                scale = {0.32f, 1.65f, 0.32f};
            } else if (actor.inventory) {
                color = {0.12f, 0.75f, 0.35f, 1.0f};
                scale = {0.18f, 0.18f, 0.18f};
            } else if (actor.travel) {
                color = {0.2f, 0.75f, 0.9f, 1.0f};
                scale = {0.2f, 0.2f, 0.2f};
            } else if (actor.mover) {
                color = {0.25f, 0.4f, 0.8f, 1.0f};
                scale = {0.12f, 0.12f, 0.12f};
            } else if (actor.trigger) {
                color = {0.85f, 0.75f, 0.12f, 1.0f};
                scale = {0.09f, 0.09f, 0.09f};
            } else {
                color = {0.5f, 0.32f, 0.15f, 1.0f};
                scale = {0.35f, 0.35f, 0.35f};
            }
            bool renderedMesh = false;
            if (!actor.meshPath.empty()) {
                try {
                    const PortableLodMesh mesh = GetPortableRuntimeMesh(actor.meshPath);
                    constexpr float unrealAngle =
                        6.28318530717958647692f / 65536.0f;
                    const float yaw = -static_cast<float>(actor.yaw) * unrealAngle;
                    const float pitch = -static_cast<float>(actor.pitch) * unrealAngle;
                    const float roll = static_cast<float>(actor.roll) * unrealAngle;
                    const OVR::Matrix4f transform =
                        OVR::Matrix4f::Translation(position) *
                        OVR::Matrix4f(OVR::Quatf(
                            OVR::Vector3f(0.0f, 1.0f, 0.0f), yaw)) *
                        OVR::Matrix4f(OVR::Quatf(
                            OVR::Vector3f(1.0f, 0.0f, 0.0f), pitch)) *
                        OVR::Matrix4f(OVR::Quatf(
                            OVR::Vector3f(0.0f, 0.0f, 1.0f), roll)) *
                        OVR::Matrix4f::Scaling(
                            actor.drawScale * actor.drawScaleY,
                            actor.drawScale * actor.drawScaleZ,
                            actor.drawScale * actor.drawScaleX);
                    std::set<std::uint16_t> materials;
                    for (const PortableMeshVertex& vertex : mesh.triangles) {
                        materials.insert(vertex.material);
                    }
                    for (const std::uint16_t material : materials) {
                        std::string texturePath = actor.texturePath;
                        if (texturePath.empty() && material < mesh.texturePaths.size()) {
                            texturePath = mesh.texturePaths[material];
                        }
                        const auto layer = textureLayers.find(texturePath);
                        if (layer == textureLayers.end()) continue;
                        const std::string descriptorKey =
                            actor.meshPath + "#" + std::to_string(material);
                        auto descriptor = meshDescriptors.find(descriptorKey);
                        if (descriptor == meshDescriptors.end()) {
                            descriptor = meshDescriptors.emplace(
                                descriptorKey,
                                BuildLodMeshDescriptor(mesh, material)).first;
                        }
                        texturedGeometry.Add(
                            descriptor->second,
                            OVRFW::GeometryBuilder::kInvalidIndex,
                            OVR::Vector4f(
                                static_cast<float>(layer->second) / 255.0f,
                                1.0f,
                                1.0f,
                                1.0f),
                            transform);
                        renderedMesh = true;
                    }
                    if (renderedMesh) ++meshInstances;
                } catch (const std::exception& error) {
                    ALOG(
                        "DeusExQuest: actor mesh render fallback for %s: %s",
                        actor.meshPath.c_str(),
                        error.what());
                }
            }
            if (!renderedMesh) {
                geometry.Add(
                    OVRFW::BuildUnitCubeDescriptor(),
                    OVRFW::GeometryBuilder::kInvalidIndex,
                    color,
                    OVR::Matrix4f::Translation(position) * OVR::Matrix4f::Scaling(scale));
            }
            interactiveActors_.push_back({
                position,
                actor.objectPath,
                actor.classPath,
                actor.travel,
                actor.destinationMap});
            ++visible;
            if (visible >= 512) break;
        }
        if (!geometry.Nodes().empty()) {
            actorWorldRendererIndex_ = worldRenderers_.size();
            worldRenderers_.emplace_back();
            worldRenderers_.back().Init(geometry.ToGeometryDescriptor());
            worldRenderers_.back().AmbientLightColor = {0.45f, 0.45f, 0.45f};
        }
        if (!texturedGeometry.Nodes().empty() && actorTexture_.IsValid()) {
            actorTexturedRendererIndex_ = texturedRenderers_.size();
            texturedRenderers_.emplace_back();
            texturedRenderers_.back().Init(
                texturedGeometry.ToGeometryDescriptor(), actorTexture_);
        }
        ALOG(
            "DeusExQuest: instantiated %zu targetable actors from %zu live actors (%zu real LodMesh instances, %zu mesh-bearing, %zu mesh formats, %zu map exits)",
            visible,
            actorSnapshots_.size(),
            meshInstances,
            std::accumulate(
                meshClasses.begin(), meshClasses.end(), std::size_t{},
                [](std::size_t total, const auto& value) { return total + value.second; }),
            meshClasses.size(),
            static_cast<std::size_t>(std::count_if(
                interactiveActors_.begin(), interactiveActors_.end(),
                [](const InteractiveActor& actor) {
                    return actor.travel && !actor.destinationMap.empty();
                })));
        for (const auto& meshClass : meshClasses) {
            ALOG(
                "DeusExQuest: actor mesh format %s count=%zu",
                meshClass.first.c_str(),
                meshClass.second);
        }
    }

    void DestroyActorGeometry() {
        if (actorTexturedRendererIndex_ != invalidRendererIndex_ &&
            actorTexturedRendererIndex_ + 1 == texturedRenderers_.size()) {
            texturedRenderers_.back().Shutdown();
            texturedRenderers_.pop_back();
        }
        if (actorWorldRendererIndex_ != invalidRendererIndex_ &&
            actorWorldRendererIndex_ + 1 == worldRenderers_.size()) {
            worldRenderers_.back().Shutdown();
            worldRenderers_.pop_back();
        }
        actorTexturedRendererIndex_ = invalidRendererIndex_;
        actorWorldRendererIndex_ = invalidRendererIndex_;
        interactiveActors_.clear();
    }

    void DestroySceneGeometry() {
        for (auto& renderer : worldRenderers_) renderer.Shutdown();
        for (auto& renderer : texturedRenderers_) renderer.Shutdown();
        worldRenderers_.clear();
        texturedRenderers_.clear();
        actorWorldRendererIndex_ = invalidRendererIndex_;
        actorTexturedRendererIndex_ = invalidRendererIndex_;
        interactiveActors_.clear();
        actorSnapshots_.clear();
        if (firstTexture_.IsValid()) {
            OVRFW::FreeTexture(firstTexture_);
            firstTexture_ = {};
        }
        if (actorTexture_.IsValid()) {
            OVRFW::FreeTexture(actorTexture_);
            actorTexture_ = {};
        }
        actorTexturePaths_.clear();
        collisionTriangles_.clear();
        collisionGrid_.clear();
        oversizedCollisionTriangles_.clear();
    }

    void LoadMapCatalog() {
        mapNames_.clear();
        std::FILE* file = std::fopen(
            "/data/user/0/dev.deusex.questvr.smoketest/files/DeusEx/quest-map-catalog.txt", "rb");
        if (file != nullptr) {
            char line[512];
            while (std::fgets(line, sizeof(line), file) != nullptr) {
                char fileName[256]{};
                if (std::sscanf(line, "%255s", fileName) != 1) continue;
                std::string map(fileName);
                if (map.size() > 3 && map.substr(map.size() - 3) == ".dx") {
                    map.resize(map.size() - 3);
                    mapNames_.push_back(std::move(map));
                }
            }
            std::fclose(file);
        }
        if (mapNames_.empty()) {
            mapNames_ = {"00_Training", "00_TrainingCombat", "00_TrainingFinal"};
        }
        const auto current = std::find(mapNames_.begin(), mapNames_.end(), currentMapName_);
        currentMapIndex_ = current == mapNames_.end()
            ? 0u
            : static_cast<std::size_t>(current - mapNames_.begin());
        ALOG("DeusExQuest: visual map catalog ready with %zu levels", mapNames_.size());
    }

    bool AdvanceMapTransition() {
        if (transitionPhase_ == MapTransitionPhase::Idle) return true;
        const auto started = std::chrono::steady_clock::now();
        try {
            if (transitionPhase_ == MapTransitionPhase::WorldTextureAllocate) {
                DestroySceneGeometry();
                currentMapName_ = transitionMapName_;
                if (restorePoseAfterTransition_) {
                    worldPosition_ = restoredWorldPosition_;
                    sceneYaw_ = restoredSceneYaw_;
                    restorePoseAfterTransition_ = false;
                } else {
                    worldPosition_ = {0.0f, 0.0f, 0.0f};
                    sceneYaw_ = 0.0f;
                }
                if (!BeginWorldTextureUpload(std::move(preparedWorldTexture_))) {
                    throw std::runtime_error("GPU world texture allocation failed");
                }
                transitionPhase_ = MapTransitionPhase::WorldTextureUpload;
            } else if (transitionPhase_ == MapTransitionPhase::WorldTextureUpload) {
                if (!UploadWorldTextureLayers(2u)) {
                    throw std::runtime_error("GPU world texture upload failed");
                }
                if (pendingWorldTextureLayersUploaded_ == pendingWorldTextureLayers_) {
                    transitionPhase_ = MapTransitionPhase::WorldGeometry;
                }
            } else if (transitionPhase_ == MapTransitionPhase::WorldGeometry) {
                if (!BeginWorldMeshUpload(std::move(preparedWorldMesh_))) {
                    throw std::runtime_error("world mesh preparation failed");
                }
                transitionPhase_ = MapTransitionPhase::WorldGeometryUpload;
            } else if (transitionPhase_ == MapTransitionPhase::WorldGeometryUpload) {
                if (!UploadNextWorldMeshChunk()) {
                    throw std::runtime_error("GPU world chunk upload failed");
                }
                if (pendingWorldMeshChunk_ == pendingWorldMesh_.chunks.size()) {
                    pendingWorldMesh_.chunks.clear();
                    transitionPhase_ = MapTransitionPhase::CollisionGrid;
                }
            } else if (transitionPhase_ == MapTransitionPhase::CollisionGrid) {
                ALOG(
                    "DeusExQuest: incrementally indexed %zu collision triangles in %zu cells",
                    collisionTriangles_.size(),
                    collisionGrid_.size());
                transitionPhase_ = MapTransitionPhase::ActorTextureAllocate;
            } else if (transitionPhase_ == MapTransitionPhase::ActorTextureAllocate) {
                actorSnapshots_ = preparedActorSnapshots_;
                preparedActorSnapshots_.clear();
                if (!BeginActorTextureUpload(std::move(preparedActorTextures_))) {
                    throw std::runtime_error("GPU actor texture allocation failed");
                }
                transitionPhase_ = MapTransitionPhase::ActorTextureUpload;
            } else if (transitionPhase_ == MapTransitionPhase::ActorTextureUpload) {
                if (!UploadActorTextureLayers(2u)) {
                    throw std::runtime_error("GPU actor texture upload failed");
                }
                if (pendingActorTextureLayersUploaded_ == pendingActorTextureLayers_) {
                    transitionPhase_ = MapTransitionPhase::ActorGeometry;
                }
            } else if (transitionPhase_ == MapTransitionPhase::ActorGeometry) {
                BuildActorMarkers();
                const auto found =
                    std::find(mapNames_.begin(), mapNames_.end(), transitionMapName_);
                if (found != mapNames_.end()) {
                    currentMapIndex_ = static_cast<std::size_t>(found - mapNames_.begin());
                }
                ALOG(
                    "DeusExQuest: staged visual runtime transition complete: %s (%zu actors, %zu BSP collision triangles, health %.1f)",
                    transitionMapName_.c_str(),
                    actorSnapshots_.size(),
                    collisionTriangles_.size(),
                    GetPortableRuntimePlayerHealth());
                transitionMapName_.clear();
                transitionPhase_ = MapTransitionPhase::Idle;
                displayedInventoryCount_ = invalidRendererIndex_;
                mapTravelCooldown_ = 3.0f;
            }
            const float milliseconds = std::chrono::duration<float, std::milli>(
                std::chrono::steady_clock::now() - started).count();
            ALOG("DeusExQuest: map transition stage completed in %.2f ms", milliseconds);
            return true;
        } catch (const std::exception& error) {
            ALOG(
                "DeusExQuest: staged map transition %s failed: %s",
                transitionMapName_.c_str(),
                error.what());
            transitionMapName_.clear();
            transitionPhase_ = MapTransitionPhase::Idle;
            if (pendingWorldTextureId_ != 0u) {
                glDeleteTextures(1, &pendingWorldTextureId_);
                pendingWorldTextureId_ = 0u;
            }
            pendingWorldTextureRgba_.clear();
            if (pendingActorTextureId_ != 0u) {
                glDeleteTextures(1, &pendingActorTextureId_);
                pendingActorTextureId_ = 0u;
            }
            pendingActorTextureRgba_.clear();
            pendingActorTexturePaths_.clear();
            pendingWorldMesh_.chunks.clear();
            preparedActorSnapshots_.clear();
            displayedInventoryCount_ = invalidRendererIndex_;
            return false;
        }
    }

    void LoadNextMap() {
        if (mapNames_.empty() || !pendingMapName_.empty() || !transitionMapName_.empty()) return;
        const std::size_t next = (currentMapIndex_ + 1u) % mapNames_.size();
        BeginMapLoad(mapNames_[next]);
    }

    void BeginMapLoad(
        const std::string& mapName,
        const std::string& restoreRuntimePath = {}) {
        if (!pendingMapName_.empty() || !transitionMapName_.empty() || mapName == currentMapName_) {
            return;
        }
        pendingMapName_ = mapName;
        displayedInventoryCount_ = invalidRendererIndex_;
        mapCacheFuture_ = std::async(std::launch::async, [mapName, restoreRuntimePath]() {
            MapPreparation preparation;
            try {
                if (!BuildQuestMapCache(gameRoot_, mapName.c_str())) {
                    preparation.error = "visual cache generation failed";
                    return preparation;
                }
                preparation.worldMesh = LoadWorldMeshCacheCpu();
                if (!preparation.worldMesh.passed) {
                    preparation.error = "world mesh cache read failed";
                    return preparation;
                }
                preparation.worldTexture = LoadWorldTextureCacheCpu();
                if (!preparation.worldTexture.passed) {
                    preparation.error = "world texture cache read failed";
                    return preparation;
                }
                const PortablePackageTables map = LoadPortablePackageTables(
                    std::string(gameRoot_) + "/Maps/" + mapName + ".dx");
                const PortableMapRuntimeSummary runtime = LoadPortableRuntimeMap(map);
                if (!restoreRuntimePath.empty() &&
                    !LoadPortableRuntimeState(restoreRuntimePath)) {
                    preparation.error = "saved runtime restoration failed";
                    return preparation;
                }
                const PortableActorMeshSummary meshes = DecodePortableRuntimeActorMeshes();
                if (!runtime.passed || !meshes.passed) {
                    preparation.error = "runtime or actor mesh replacement failed";
                    return preparation;
                }
                preparation.actors = GetPortableRuntimeMapActors();
                preparation.actorTextures = BuildPortableRuntimeActorTextureArray(96, 96);
                preparation.passed = preparation.actorTextures.passed;
                if (!preparation.passed) preparation.error = "actor texture preparation failed";
            } catch (const std::exception& error) {
                preparation.error = error.what();
            }
            return preparation;
        });
        ALOG("DeusExQuest: background visual cache started for %s", mapName.c_str());
    }

    void CompletePendingMapLoad() {
        if (pendingMapName_.empty() || !mapCacheFuture_.valid() ||
            mapCacheFuture_.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
            return;
        }
        const std::string mapName = pendingMapName_;
        pendingMapName_.clear();
        MapPreparation preparation = mapCacheFuture_.get();
        if (!preparation.passed) {
            ALOG(
                "DeusExQuest: background transition preparation failed for %s: %s",
                mapName.c_str(),
                preparation.error.c_str());
            restorePoseAfterTransition_ = false;
            displayedInventoryCount_ = invalidRendererIndex_;
            return;
        }
        preparedActorSnapshots_ = std::move(preparation.actors);
        preparedActorTextures_ = std::move(preparation.actorTextures);
        preparedWorldTexture_ = std::move(preparation.worldTexture);
        preparedWorldMesh_ = std::move(preparation.worldMesh);
        transitionMapName_ = mapName;
        transitionPhase_ = MapTransitionPhase::WorldTextureAllocate;
    }

    void PollMapTransitionRequest() {
        constexpr const char* requestPath =
            "/data/user/0/dev.deusex.questvr.smoketest/files/DeusEx/quest-map.request";
        std::FILE* file = std::fopen(requestPath, "rb");
        if (file == nullptr) return;
        char requested[256]{};
        const bool read = std::fscanf(file, "%255s", requested) == 1;
        std::fclose(file);
        std::remove(requestPath);
        if (!read) return;
        if (std::strcmp(requested, "SAVE") == 0) {
            SaveGameState();
            return;
        }
        if (std::strcmp(requested, "LOAD") == 0) {
            LoadGameState();
            return;
        }
        if (std::strcmp(requested, "DAMAGE") == 0) {
            ALOG(
                "DeusExQuest: diagnostic player damage, health=%.1f",
                DamagePortableRuntimePlayer(10.0f));
            return;
        }
        if (std::strcmp(requested, "CYCLE") == 0) {
            const std::size_t count = GetPortableRuntimeInventoryCount();
            if (count != 0u) selectedInventoryIndex_ = (selectedInventoryIndex_ + 1u) % count;
            displayedInventoryCount_ = invalidRendererIndex_;
            ALOG(
                "DeusExQuest: diagnostic inventory cycle selected=%s",
                SelectedInventoryLabel(GetPortableRuntimeInventoryItems()).c_str());
            return;
        }
        if (std::strcmp(requested, "CONSUME") == 0) {
            const bool used = UseSelectedConsumable();
            ALOG("DeusExQuest: diagnostic consume result=%s", used ? "used" : "not_consumable");
            return;
        }
        if (std::strcmp(requested, "READY") == 0) {
            const std::vector<std::string> inventory = GetPortableRuntimeInventoryItems();
            ALOG(
                "DeusExQuest: diagnostic weapon ready selected=%s damage=%.1f ammo=%s",
                SelectedInventoryLabel(inventory).c_str(),
                SelectedWeaponDamage(inventory),
                SelectedWeaponHasAmmo(inventory) ? "ready" : "blocked");
            return;
        }
        if (std::strcmp(requested, "DIALOGUE") == 0) {
            bool found{};
            for (const PortableActorSnapshot& actor : actorSnapshots_) {
                if (actor.pawn && ShowDialogue(actor.objectPath)) {
                    found = true;
                    break;
                }
            }
            ALOG("DeusExQuest: diagnostic dialogue result=%s", found ? "found" : "missing");
            return;
        }
        if (std::strcmp(requested, "PICKUP") == 0) {
            const auto foundInventory = std::find_if(
                actorSnapshots_.begin(), actorSnapshots_.end(),
                [](const PortableActorSnapshot& actor) { return actor.inventory; });
            if (foundInventory != actorSnapshots_.end()) {
                const PortableInteractionResult result =
                    InteractPortableRuntimeActor(foundInventory->objectPath);
                DestroyActorGeometry();
                actorSnapshots_ = GetPortableRuntimeMapActors();
                BuildActorMarkers();
                displayedInventoryCount_ = invalidRendererIndex_;
                ALOG(
                    "DeusExQuest: diagnostic pickup %s inventory=%zu selected=%s",
                    result.objectPath.c_str(),
                    result.inventoryCount,
                    SelectedInventoryLabel(GetPortableRuntimeInventoryItems()).c_str());
            }
            return;
        }
        const auto found = std::find(mapNames_.begin(), mapNames_.end(), requested);
        if (found == mapNames_.end()) {
            ALOG("DeusExQuest: rejected unknown requested map %s", requested);
            return;
        }
        BeginMapLoad(*found);
    }

    bool UseTargetedActor(const OVR::Posef& pointerPose) {
        const OVR::Vector3f origin = pointerPose.Translation;
        const OVR::Vector3f direction =
            pointerPose.Rotation.Rotate(OVR::Vector3f(0.0f, 0.0f, -1.0f));
        const OVR::Quatf worldRotation(
            OVR::Vector3f(0.0f, 1.0f, 0.0f), sceneYaw_);
        const InteractiveActor* best{};
        float bestDistance = 3.0f;
        for (const InteractiveActor& actor : interactiveActors_) {
            const OVR::Vector3f position =
                worldRotation.Rotate(actor.localPosition) + worldPosition_;
            const OVR::Vector3f toActor = position - origin;
            const float distance = toActor.Dot(direction);
            if (distance <= 0.0f || distance >= bestDistance) continue;
            const OVR::Vector3f closest = origin + direction * distance;
            if ((position - closest).LengthSq() <= 0.35f * 0.35f) {
                best = &actor;
                bestDistance = distance;
            }
        }
        if (best != nullptr) {
            const PortableInteractionResult interaction =
                InteractPortableRuntimeActor(best->objectPath);
            const std::size_t separator = best->objectPath.find_last_of('.');
            const std::string actorName = separator == std::string::npos
                ? best->objectPath
                : best->objectPath.substr(separator + 1u);
            interactionStatus_ = interaction.handled
                ? interaction.action + ": " + actorName
                : "CANNOT USE: " + actorName;
            interactionStatusSeconds_ = 3.0f;
            if (interaction.action == "conversation") ShowDialogue(best->objectPath);
            ALOG(
                "DeusExQuest: VR use %s on %s (%s) at %.2fm; inventory=%zu",
                interaction.action.c_str(),
                best->objectPath.c_str(),
                best->classPath.c_str(),
                bestDistance,
                interaction.inventoryCount);
            if (!interaction.destinationMap.empty()) {
                RequestDestinationMap(interaction.destinationMap);
            }
            return interaction.worldChanged;
        } else {
            if (UseSelectedConsumable()) return false;
            interactionStatus_ = "NO USABLE TARGET";
            interactionStatusSeconds_ = 2.0f;
            ALOG("DeusExQuest: VR use found no actor within 3m ray");
        }
        return false;
    }

    bool ShowDialogue(const std::string& actorPath) {
        std::size_t& cursor = dialogueOffsets_[actorPath];
        std::int32_t missionNumber{std::numeric_limits<std::int32_t>::min()};
        const std::size_t separator = currentMapName_.find('_');
        if (separator != std::string::npos && separator > 0u) {
            try {
                missionNumber = std::stoi(currentMapName_.substr(0u, separator));
            } catch (const std::exception&) {
                missionNumber = std::numeric_limits<std::int32_t>::min();
            }
        }
        if (currentMapName_.rfind("00_Training", 0u) == 0u) missionNumber = -1;
        const PortableDialogueResult dialogue =
            GetPortableRuntimeDialogue(actorPath, cursor, missionNumber);
        if (!dialogue.found) {
            interactionStatus_ = "NO DIALOGUE DATA";
            interactionStatusSeconds_ = 2.0f;
            ALOG(
                "DeusExQuest: no serialized dialogue matched %s bind=%s mission candidates=%s",
                actorPath.c_str(),
                dialogue.bindName.c_str(),
                dialogue.missionCandidates.c_str());
            return false;
        }
        ++cursor;
        std::string subtitle = dialogue.speech;
        std::replace(subtitle.begin(), subtitle.end(), '\n', ' ');
        std::replace(subtitle.begin(), subtitle.end(), '\r', ' ');
        if (subtitle.size() > 220u) subtitle.resize(220u);
        interactionStatus_ = dialogue.bindName + ": " + subtitle;
        interactionStatusSeconds_ = std::clamp(
            static_cast<float>(subtitle.size()) * 0.055f, 4.0f, 12.0f);
        PortableSound dialogueSound;
        try {
            dialogueSound = LoadPortableRuntimeDialogueSound(dialogue);
        } catch (const std::exception& error) {
            ALOG("DeusExQuest: dialogue audio resolution failed: %s", error.what());
        }
        const bool audioQueued = QueueDialogueAudio(dialogueSound);
        ALOG(
            "DeusExQuest: dialogue %s bind=%s line=%zu/%zu sound=%d package=%s audio=%s/%zu bytes queued=%s text=%s",
            dialogue.eventPath.c_str(),
            dialogue.bindName.c_str(),
            cursor,
            dialogue.matchingLines,
            dialogue.soundId,
            dialogue.audioPackageName.c_str(),
            dialogueSound.format.ToString().c_str(),
            dialogueSound.data.size(),
            audioQueued ? "true" : "false",
            subtitle.c_str());
        return true;
    }

    bool RequestDestinationMap(std::string destination) {
        const std::size_t option = destination.find_first_of("?#");
        if (option != std::string::npos) destination.resize(option);
        const std::size_t slash = destination.find_last_of("/\\");
        if (slash != std::string::npos) destination.erase(0, slash + 1u);
        if (destination.size() > 3u) {
            std::string extension = destination.substr(destination.size() - 3u);
            std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) {
                return static_cast<char>(std::tolower(c));
            });
            if (extension == ".dx") destination.resize(destination.size() - 3u);
        }
        const auto found = std::find_if(
            mapNames_.begin(), mapNames_.end(), [&](const std::string& candidate) {
                return candidate.size() == destination.size() && std::equal(
                    candidate.begin(), candidate.end(), destination.begin(),
                    [](unsigned char left, unsigned char right) {
                        return std::tolower(left) == std::tolower(right);
                    });
            });
        if (found == mapNames_.end()) {
            ALOG("DeusExQuest: map exit destination is not in catalog: %s", destination.c_str());
            return false;
        }
        BeginMapLoad(*found);
        return !pendingMapName_.empty();
    }

    void CheckTravelTriggers(const OVR::Vector3f& headPosition) {
        const OVR::Quatf worldRotation(
            OVR::Vector3f(0.0f, 1.0f, 0.0f), sceneYaw_);
        for (const InteractiveActor& actor : interactiveActors_) {
            if (!actor.travel || actor.destinationMap.empty()) continue;
            const OVR::Vector3f position =
                worldRotation.Rotate(actor.localPosition) + worldPosition_;
            const OVR::Vector3f delta = position - headPosition;
            if (delta.x * delta.x + delta.z * delta.z <= 0.75f * 0.75f &&
                std::fabs(delta.y) <= 1.6f && RequestDestinationMap(actor.destinationMap)) {
                ALOG(
                    "DeusExQuest: player entered map exit %s -> %s",
                    actor.objectPath.c_str(),
                    actor.destinationMap.c_str());
                mapTravelCooldown_ = 3.0f;
                return;
            }
        }
    }

    static std::string SelectedInventoryLabel(const std::vector<std::string>& inventory) {
        if (inventory.empty()) return "NONE";
        const std::string& path = inventory[selectedInventoryIndex_ % inventory.size()];
        const std::size_t separator = path.find_last_of('.');
        return separator == std::string::npos ? path : path.substr(separator + 1u);
    }

    bool UseSelectedConsumable() {
        const std::vector<std::string> inventory = GetPortableRuntimeInventoryItems();
        if (inventory.empty()) return false;
        const std::size_t selected = selectedInventoryIndex_ % inventory.size();
        const std::string& path = inventory[selected];
        const std::string label = SelectedInventoryLabel(inventory);
        float healing{};
        if (label.find("MedKit") != std::string::npos) {
            healing = 25.0f;
        } else if (label.find("SoyFood") != std::string::npos ||
                   label.find("Candybar") != std::string::npos) {
            healing = 5.0f;
        } else if (label.find("SodaCan") != std::string::npos ||
                   label.find("Liquor") != std::string::npos ||
                   label.find("WineBottle") != std::string::npos) {
            healing = 2.0f;
        } else {
            return false;
        }
        const float before = GetPortableRuntimePlayerHealth();
        if (before >= 100.0f || !ConsumePortableRuntimeInventoryItem(path)) {
            interactionStatus_ = before >= 100.0f
                ? "HEALTH ALREADY FULL"
                : "ITEM UNAVAILABLE";
            interactionStatusSeconds_ = 2.0f;
            return true;
        }
        const float after = HealPortableRuntimePlayer(healing);
        const std::size_t remaining = GetPortableRuntimeInventoryCount();
        if (remaining != 0u) selectedInventoryIndex_ %= remaining;
        interactionStatus_ = "USED " + label + "  HEALTH " +
            std::to_string(static_cast<int>(after));
        interactionStatusSeconds_ = 3.0f;
        ALOG(
            "DeusExQuest: VR consumed %s; health %.1f -> %.1f inventory=%zu",
            path.c_str(),
            before,
            after,
            remaining);
        return true;
    }

    static float SelectedWeaponDamage(const std::vector<std::string>& inventory) {
        const std::string item = SelectedInventoryLabel(inventory);
        if (item.find("Weapon") == std::string::npos) return 0.0f;
        if (item.find("GEP") != std::string::npos || item.find("LAW") != std::string::npos) return 80.0f;
        if (item.find("Sniper") != std::string::npos) return 40.0f;
        if (item.find("Shotgun") != std::string::npos) return 32.0f;
        if (item.find("Pistol") != std::string::npos) return 20.0f;
        if (item.find("Crowbar") != std::string::npos || item.find("Baton") != std::string::npos) return 12.0f;
        return 18.0f;
    }

    static bool SelectedWeaponHasAmmo(const std::vector<std::string>& inventory) {
        const std::string weapon = SelectedInventoryLabel(inventory);
        if (weapon.find("Weapon") == std::string::npos) return false;
        if (weapon.find("Crowbar") != std::string::npos ||
            weapon.find("Baton") != std::string::npos ||
            weapon.find("CombatKnife") != std::string::npos ||
            weapon.find("Sword") != std::string::npos) {
            return true;
        }
        const auto owns = [&](const char* className) {
            return std::any_of(inventory.begin(), inventory.end(), [&](const std::string& item) {
                const std::size_t separator = item.find_last_of('.');
                const std::string label = separator == std::string::npos
                    ? item
                    : item.substr(separator + 1u);
                return label.find(className) != std::string::npos;
            });
        };
        if (weapon.find("Shotgun") != std::string::npos) return owns("AmmoShell");
        if (weapon.find("Sniper") != std::string::npos) return owns("Ammo3006");
        if (weapon.find("GEP") != std::string::npos ||
            weapon.find("LAW") != std::string::npos) return owns("AmmoRocket");
        if (weapon.find("Plasma") != std::string::npos) return owns("AmmoPlasma");
        if (weapon.find("Flamethrower") != std::string::npos) return owns("AmmoNapalm");
        if (weapon.find("Crossbow") != std::string::npos) return owns("AmmoDart");
        if (weapon.find("Prod") != std::string::npos) return owns("AmmoBattery");
        if (weapon.find("Pistol") != std::string::npos ||
            weapon.find("AssaultGun") != std::string::npos) return owns("Ammo10mm");
        return true;
    }

    static float SelectedWeaponRange(const std::vector<std::string>& inventory) {
        const std::string item = SelectedInventoryLabel(inventory);
        if (item.find("Crowbar") != std::string::npos ||
            item.find("Baton") != std::string::npos ||
            item.find("Knife") != std::string::npos ||
            item.find("Sword") != std::string::npos) {
            return 2.0f;
        }
        return 30.0f;
    }

    bool FireTargetedActor(
        const OVR::Posef& pointerPose,
        float weaponDamage,
        float weaponRange) {
        const OVR::Vector3f origin = pointerPose.Translation;
        const OVR::Vector3f direction =
            pointerPose.Rotation.Rotate(OVR::Vector3f(0.0f, 0.0f, -1.0f));
        const OVR::Quatf worldRotation(
            OVR::Vector3f(0.0f, 1.0f, 0.0f), sceneYaw_);
        const InteractiveActor* best{};
        float bestDistance = weaponRange;
        for (const InteractiveActor& actor : interactiveActors_) {
            const OVR::Vector3f position =
                worldRotation.Rotate(actor.localPosition) + worldPosition_;
            const OVR::Vector3f toActor = position - origin;
            const float distance = toActor.Dot(direction);
            if (distance <= 0.0f || distance >= bestDistance) continue;
            const OVR::Vector3f closest = origin + direction * distance;
            if ((position - closest).LengthSq() <= 0.5f * 0.5f) {
                best = &actor;
                bestDistance = distance;
            }
        }
        if (best == nullptr) {
            interactionStatus_ = "SHOT MISSED";
            interactionStatusSeconds_ = 1.5f;
            ALOG("DeusExQuest: VR fire hit no pawn");
            return false;
        }
        const PortableDamageResult damage =
            DamagePortableRuntimeActor(best->objectPath, weaponDamage);
        if (!damage.handled) {
            interactionStatus_ = "TARGET NOT DAMAGEABLE";
            interactionStatusSeconds_ = 2.0f;
            ALOG(
                "DeusExQuest: VR fire struck non-damageable %s at %.2fm",
                best->objectPath.c_str(),
                bestDistance);
            return false;
        }
        interactionStatus_ = damage.killed
            ? "TARGET DOWN"
            : "HIT  HEALTH " + std::to_string(static_cast<int>(damage.remainingHealth));
        interactionStatusSeconds_ = 2.0f;
        ALOG(
            "DeusExQuest: VR fire damaged %s at %.2fm; health=%.1f killed=%s",
            best->objectPath.c_str(),
            bestDistance,
            damage.remainingHealth,
            damage.killed ? "true" : "false");
        return damage.worldChanged;
    }

    void SaveGameState() {
        constexpr const char* metaPath =
            "/data/user/0/dev.deusex.questvr.smoketest/files/DeusEx/quest-save-0.meta";
        constexpr const char* runtimePath =
            "/data/user/0/dev.deusex.questvr.smoketest/files/DeusEx/quest-save-0.runtime";
        std::FILE* file = std::fopen(metaPath, "wb");
        const std::uint32_t header[2] = {0x4d515844u, 3u};
        const float pose[4] = {
            worldPosition_.x, worldPosition_.y, worldPosition_.z, sceneYaw_};
        const std::uint32_t mapNameBytes = static_cast<std::uint32_t>(currentMapName_.size());
        bool metaSaved = file != nullptr &&
            std::fwrite(header, sizeof(header), 1, file) == 1 &&
            std::fwrite(pose, sizeof(pose), 1, file) == 1 &&
            std::fwrite(&mapNameBytes, sizeof(mapNameBytes), 1, file) == 1 &&
            std::fwrite(currentMapName_.data(), 1, mapNameBytes, file) == mapNameBytes;
        const std::uint32_t dialogueCount =
            static_cast<std::uint32_t>(dialogueOffsets_.size());
        if (metaSaved) {
            metaSaved = std::fwrite(&dialogueCount, sizeof(dialogueCount), 1, file) == 1;
            for (const auto& entry : dialogueOffsets_) {
                const std::uint32_t pathBytes = static_cast<std::uint32_t>(entry.first.size());
                const std::uint64_t cursor = static_cast<std::uint64_t>(entry.second);
                metaSaved = metaSaved && pathBytes > 0u && pathBytes <= 1024u &&
                    std::fwrite(&pathBytes, sizeof(pathBytes), 1, file) == 1 &&
                    std::fwrite(entry.first.data(), 1, pathBytes, file) == pathBytes &&
                    std::fwrite(&cursor, sizeof(cursor), 1, file) == 1;
                if (!metaSaved) break;
            }
        }
        if (file != nullptr) std::fclose(file);
        const bool runtimeSaved = SavePortableRuntimeState(runtimePath);
        ALOG(
            "DeusExQuest: VR quick-save %s map=%s health=%.1f",
            metaSaved && runtimeSaved ? "completed" : "failed",
            currentMapName_.c_str(),
            GetPortableRuntimePlayerHealth());
    }

    void LoadGameState() {
        constexpr const char* metaPath =
            "/data/user/0/dev.deusex.questvr.smoketest/files/DeusEx/quest-save-0.meta";
        constexpr const char* runtimePath =
            "/data/user/0/dev.deusex.questvr.smoketest/files/DeusEx/quest-save-0.runtime";
        std::FILE* file = std::fopen(metaPath, "rb");
        std::uint32_t header[2]{};
        float pose[4]{};
        std::string savedMapName;
        std::unordered_map<std::string, std::size_t> savedDialogueOffsets;
        bool metaLoaded = file != nullptr &&
            std::fread(header, sizeof(header), 1, file) == 1 &&
            std::fread(pose, sizeof(pose), 1, file) == 1 &&
            header[0] == 0x4d515844u &&
            (header[1] == 1u || header[1] == 2u || header[1] == 3u) &&
            std::isfinite(pose[0]) && std::isfinite(pose[1]) &&
            std::isfinite(pose[2]) && std::isfinite(pose[3]);
        if (metaLoaded && header[1] >= 2u) {
            std::uint32_t mapNameBytes{};
            metaLoaded = std::fread(&mapNameBytes, sizeof(mapNameBytes), 1, file) == 1 &&
                mapNameBytes > 0u && mapNameBytes <= 255u;
            if (metaLoaded) {
                savedMapName.resize(mapNameBytes);
                metaLoaded = std::fread(savedMapName.data(), 1, mapNameBytes, file) == mapNameBytes;
            }
        } else if (metaLoaded) {
            savedMapName = currentMapName_;
        }
        if (metaLoaded && header[1] == 3u) {
            std::uint32_t dialogueCount{};
            metaLoaded = std::fread(&dialogueCount, sizeof(dialogueCount), 1, file) == 1 &&
                dialogueCount <= 4096u;
            for (std::uint32_t index = 0; metaLoaded && index < dialogueCount; ++index) {
                std::uint32_t pathBytes{};
                std::uint64_t cursor{};
                metaLoaded = std::fread(&pathBytes, sizeof(pathBytes), 1, file) == 1 &&
                    pathBytes > 0u && pathBytes <= 1024u;
                std::string path(pathBytes, '\0');
                metaLoaded = metaLoaded &&
                    std::fread(path.data(), 1, pathBytes, file) == pathBytes &&
                    std::fread(&cursor, sizeof(cursor), 1, file) == 1;
                if (metaLoaded) savedDialogueOffsets.emplace(
                    std::move(path), static_cast<std::size_t>(cursor));
            }
        }
        if (file != nullptr) std::fclose(file);
        if (!metaLoaded) {
            ALOG("DeusExQuest: VR quick-load failed");
            return;
        }
        dialogueOffsets_ = std::move(savedDialogueOffsets);
        if (savedMapName != currentMapName_) {
            const auto found = std::find(mapNames_.begin(), mapNames_.end(), savedMapName);
            if (found == mapNames_.end()) {
                ALOG("DeusExQuest: VR quick-load map is unavailable: %s", savedMapName.c_str());
                return;
            }
            restoredWorldPosition_ = {pose[0], pose[1], pose[2]};
            restoredSceneYaw_ = pose[3];
            restorePoseAfterTransition_ = true;
            BeginMapLoad(*found, runtimePath);
            ALOG("DeusExQuest: VR quick-load restoring map %s", savedMapName.c_str());
            return;
        }
        if (!LoadPortableRuntimeState(runtimePath)) {
            ALOG("DeusExQuest: VR quick-load runtime failed");
            return;
        }
        worldPosition_ = {pose[0], pose[1], pose[2]};
        sceneYaw_ = pose[3];
        DestroyActorGeometry();
        actorSnapshots_ = GetPortableRuntimeMapActors();
        BuildActorMarkers();
        ALOG(
            "DeusExQuest: VR quick-load completed health=%.1f",
            GetPortableRuntimePlayerHealth());
    }

    struct CollisionTriangle {
        OVR::Vector3f a;
        OVR::Vector3f b;
        OVR::Vector3f c;
        OVR::Vector3f normal;
    };

    static std::uint16_t ReadLe16(const std::uint8_t* bytes) {
        return static_cast<std::uint16_t>(bytes[0]) |
            (static_cast<std::uint16_t>(bytes[1]) << 8u);
    }

    static std::uint32_t ReadLe32(const std::uint8_t* bytes) {
        return static_cast<std::uint32_t>(bytes[0]) |
            (static_cast<std::uint32_t>(bytes[1]) << 8u) |
            (static_cast<std::uint32_t>(bytes[2]) << 16u) |
            (static_cast<std::uint32_t>(bytes[3]) << 24u);
    }

    static aaudio_data_callback_result_t AmbientAudioCallback(
        AAudioStream*,
        void* userData,
        void* audioData,
        std::int32_t numFrames) {
        auto* app = static_cast<DeusExQuestApp*>(userData);
        auto* output = static_cast<std::int16_t*>(audioData);
        const std::size_t sampleCount = static_cast<std::size_t>(numFrames) * 2u;
        std::lock_guard<std::mutex> lock(app->audioMutex_);
        if (app->ambientSamples_.empty()) {
            std::fill(output, output + sampleCount, 0);
        } else {
            for (std::size_t index = 0; index < sampleCount; ++index) {
                output[index] = static_cast<std::int16_t>(
                    app->ambientSamples_[app->ambientCursor_] / 4);
                app->ambientCursor_ = (app->ambientCursor_ + 1u) % app->ambientSamples_.size();
            }
        }
        for (std::size_t index = 0; index < sampleCount; ++index) {
            if (app->dialogueCursor_ >= app->dialogueSamples_.size()) break;
            const std::int32_t mixed = static_cast<std::int32_t>(output[index]) +
                app->dialogueSamples_[app->dialogueCursor_++];
            output[index] = static_cast<std::int16_t>(
                std::clamp(mixed, -32768, 32767));
        }
        return AAUDIO_CALLBACK_RESULT_CONTINUE;
    }

    bool QueueDialogueAudio(const PortableSound& sound) {
        if (sound.format.ToString() != "mp3" || sound.data.empty() || audioSampleRate_ == 0u) {
            return false;
        }
        mp3dec_ex_t decoder{};
        if (mp3dec_ex_open_buf(
                &decoder, sound.data.data(), sound.data.size(), MP3D_SEEK_TO_SAMPLE) != 0) {
            return false;
        }
        const std::uint32_t sourceRate = static_cast<std::uint32_t>(decoder.info.hz);
        const std::uint32_t channels = static_cast<std::uint32_t>(decoder.info.channels);
        std::vector<mp3d_sample_t> decoded(static_cast<std::size_t>(decoder.samples));
        const std::size_t samples = mp3dec_ex_read(
            &decoder, decoded.data(), decoded.size());
        mp3dec_ex_close(&decoder);
        if (samples == 0u || sourceRate == 0u || (channels != 1u && channels != 2u)) return false;
        const std::size_t sourceFrames = samples / channels;
        const std::size_t outputFrames = static_cast<std::size_t>(
            static_cast<std::uint64_t>(sourceFrames) * audioSampleRate_ / sourceRate);
        std::vector<std::int16_t> stereo(outputFrames * 2u);
        for (std::size_t frame = 0; frame < outputFrames; ++frame) {
            const double sourcePosition = static_cast<double>(frame) * sourceRate / audioSampleRate_;
            const std::size_t first = std::min(
                static_cast<std::size_t>(sourcePosition), sourceFrames - 1u);
            const std::size_t second = std::min(first + 1u, sourceFrames - 1u);
            const float fraction = static_cast<float>(sourcePosition - first);
            for (std::size_t channel = 0; channel < 2u; ++channel) {
                const std::size_t sourceChannel = channels == 1u ? 0u : channel;
                const float a = decoded[first * channels + sourceChannel];
                const float b = decoded[second * channels + sourceChannel];
                stereo[frame * 2u + channel] = static_cast<std::int16_t>(
                    a + (b - a) * fraction);
            }
        }
        {
            std::lock_guard<std::mutex> lock(audioMutex_);
            dialogueSamples_ = std::move(stereo);
            dialogueCursor_ = 0u;
        }
        ALOG(
            "DeusExQuest: queued dialogue MP3: %u Hz/%u channels -> %u Hz, %zu frames",
            sourceRate,
            channels,
            audioSampleRate_,
            outputFrames);
        return true;
    }

    bool StartAmbientAudio() {
        constexpr const char* path =
            "/data/user/0/dev.deusex.questvr.smoketest/files/DeusEx/quest-ambient.wav";
        std::FILE* file = std::fopen(path, "rb");
        if (file == nullptr) return false;
        std::fseek(file, 0, SEEK_END);
        const long fileSize = std::ftell(file);
        std::fseek(file, 0, SEEK_SET);
        std::vector<std::uint8_t> wav(fileSize > 0 ? static_cast<std::size_t>(fileSize) : 0u);
        const bool read = !wav.empty() &&
            std::fread(wav.data(), 1, wav.size(), file) == wav.size();
        std::fclose(file);
        if (!read || wav.size() < 12 || std::memcmp(wav.data(), "RIFF", 4) != 0 ||
            std::memcmp(wav.data() + 8, "WAVE", 4) != 0) return false;

        std::uint16_t format{}, channels{}, bits{};
        std::uint32_t sampleRate{};
        const std::uint8_t* pcm{};
        std::size_t pcmBytes{};
        for (std::size_t offset = 12; offset + 8 <= wav.size();) {
            const std::uint32_t chunkSize = ReadLe32(wav.data() + offset + 4);
            const std::size_t dataOffset = offset + 8;
            if (dataOffset + chunkSize > wav.size()) return false;
            if (std::memcmp(wav.data() + offset, "fmt ", 4) == 0 && chunkSize >= 16) {
                format = ReadLe16(wav.data() + dataOffset);
                channels = ReadLe16(wav.data() + dataOffset + 2);
                sampleRate = ReadLe32(wav.data() + dataOffset + 4);
                bits = ReadLe16(wav.data() + dataOffset + 14);
            } else if (std::memcmp(wav.data() + offset, "data", 4) == 0) {
                pcm = wav.data() + dataOffset;
                pcmBytes = chunkSize;
            }
            offset = dataOffset + chunkSize + (chunkSize & 1u);
        }
        if (format != 1 || (channels != 1 && channels != 2) ||
            (bits != 8 && bits != 16) || sampleRate < 8000 || sampleRate > 192000 ||
            pcm == nullptr || pcmBytes == 0) return false;
        const std::size_t bytesPerSample = bits / 8u;
        const std::size_t frames = pcmBytes / (channels * bytesPerSample);
        ambientSamples_.resize(frames * 2u);
        for (std::size_t frame = 0; frame < frames; ++frame) {
            for (std::size_t outputChannel = 0; outputChannel < 2; ++outputChannel) {
                const std::size_t sourceChannel = channels == 1 ? 0 : outputChannel;
                const std::size_t source = (frame * channels + sourceChannel) * bytesPerSample;
                const std::int16_t sample = bits == 8
                    ? static_cast<std::int16_t>(
                          (static_cast<std::int32_t>(pcm[source]) - 128) << 8)
                    : static_cast<std::int16_t>(ReadLe16(pcm + source));
                ambientSamples_[frame * 2u + outputChannel] = sample;
            }
        }
        audioSampleRate_ = sampleRate;

        AAudioStreamBuilder* builder{};
        if (AAudio_createStreamBuilder(&builder) != AAUDIO_OK) return false;
        AAudioStreamBuilder_setDirection(builder, AAUDIO_DIRECTION_OUTPUT);
        AAudioStreamBuilder_setPerformanceMode(builder, AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);
        AAudioStreamBuilder_setSharingMode(builder, AAUDIO_SHARING_MODE_SHARED);
        AAudioStreamBuilder_setFormat(builder, AAUDIO_FORMAT_PCM_I16);
        AAudioStreamBuilder_setChannelCount(builder, 2);
        AAudioStreamBuilder_setSampleRate(builder, static_cast<std::int32_t>(sampleRate));
        AAudioStreamBuilder_setDataCallback(builder, AmbientAudioCallback, this);
        const aaudio_result_t opened = AAudioStreamBuilder_openStream(builder, &ambientStream_);
        AAudioStreamBuilder_delete(builder);
        if (opened != AAUDIO_OK || ambientStream_ == nullptr ||
            AAudioStream_requestStart(ambientStream_) != AAUDIO_OK) {
            StopAmbientAudio();
            return false;
        }
        ALOG(
            "DeusExQuest: ambient AAudio started: %u Hz, %zu stereo frames",
            sampleRate,
            frames);
        return true;
    }

    void StopAmbientAudio() {
        if (ambientStream_ != nullptr) {
            AAudioStream_requestStop(ambientStream_);
            AAudioStream_close(ambientStream_);
            ambientStream_ = nullptr;
        }
        ambientSamples_.clear();
        ambientCursor_ = 0;
        dialogueSamples_.clear();
        dialogueCursor_ = 0;
        audioSampleRate_ = 0u;
    }

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
        constexpr float cellSize = 8.0f;
        return static_cast<int>(std::floor(coordinate / cellSize));
    }

    void BuildCollisionGrid() {
        collisionGrid_.clear();
        oversizedCollisionTriangles_.clear();
        AddCollisionGridRange(0u);
    }

    void AddCollisionGridRange(std::uint32_t firstIndex) {
        for (std::uint32_t index = firstIndex; index < collisionTriangles_.size(); ++index) {
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

    static WorldMeshPreparation LoadWorldMeshCacheCpu() {
        constexpr const char* path =
            "/data/user/0/dev.deusex.questvr.smoketest/files/DeusEx/quest-world.mesh";
        WorldMeshPreparation result;
        std::FILE* file = std::fopen(path, "rb");
        if (file == nullptr) return result;
        std::uint32_t magic{}, version{}, chunkCount{};
        result.passed = std::fread(&magic, sizeof(magic), 1, file) == 1 &&
            std::fread(&version, sizeof(version), 1, file) == 1 &&
            std::fread(&chunkCount, sizeof(chunkCount), 1, file) == 1 &&
            magic == 0x4d515844u && version == 2u && chunkCount > 0u && chunkCount < 128u;
        result.chunks.reserve(chunkCount);
        for (std::uint32_t chunk = 0; result.passed && chunk < chunkCount; ++chunk) {
            WorldMeshChunkPreparation prepared;
            std::uint32_t vertexCount{};
            result.passed =
                std::fread(&prepared.materialSlot, sizeof(prepared.materialSlot), 1, file) == 1 &&
                std::fread(&vertexCount, sizeof(vertexCount), 1, file) == 1 &&
                vertexCount > 0u && vertexCount <= 60000u && vertexCount % 3u == 0u;
            if (!result.passed) break;
            prepared.vertices.resize(vertexCount);
            result.passed = std::fread(
                prepared.vertices.data(), sizeof(MeshVertex), vertexCount, file) == vertexCount;
            if (result.passed) {
                constexpr std::size_t verticesPerGpuBatch = 4800u;
                for (std::size_t offset = 0; offset < prepared.vertices.size();
                     offset += verticesPerGpuBatch) {
                    WorldMeshChunkPreparation batch;
                    batch.materialSlot = prepared.materialSlot;
                    const std::size_t end = std::min(
                        offset + verticesPerGpuBatch, prepared.vertices.size());
                    batch.vertices.assign(
                        prepared.vertices.begin() + static_cast<std::ptrdiff_t>(offset),
                        prepared.vertices.begin() + static_cast<std::ptrdiff_t>(end));
                    result.chunks.emplace_back(std::move(batch));
                }
            }
        }
        std::fclose(file);
        if (!result.passed) result.chunks.clear();
        return result;
    }

    bool UploadWorldMeshChunk(WorldMeshChunkPreparation& chunk) {
        const std::uint32_t firstCollisionTriangle =
            static_cast<std::uint32_t>(collisionTriangles_.size());
        const std::uint32_t vertexCount = static_cast<std::uint32_t>(chunk.vertices.size());
        OVRFW::GlGeometry::Descriptor descriptor;
        descriptor.attribs.position.reserve(vertexCount);
        descriptor.attribs.normal.reserve(vertexCount);
        descriptor.attribs.uv0.reserve(vertexCount);
        descriptor.attribs.color.reserve(vertexCount);
        descriptor.indices.reserve(vertexCount);
        for (std::uint32_t index = 0; index < vertexCount; ++index) {
            const MeshVertex& vertex = chunk.vertices[index];
            descriptor.attribs.position.emplace_back(vertex.px, vertex.py, vertex.pz);
            descriptor.attribs.normal.emplace_back(vertex.nx, vertex.ny, vertex.nz);
            descriptor.attribs.uv0.emplace_back(vertex.u, vertex.v);
            const float shade = 0.25f + 0.55f * (vertex.nz * 0.5f + 0.5f);
            if (chunk.materialSlot == 0) {
                descriptor.attribs.color.emplace_back(
                    static_cast<float>(vertex.materialSlot) / 255.0f, 1.0f, 1.0f, 1.0f);
            } else {
                descriptor.attribs.color.emplace_back(
                    0.15f * shade, 0.8f * shade, 0.55f * shade, 1.0f);
            }
            descriptor.indices.push_back(static_cast<OVRFW::TriangleIndex>(index));
        }
        for (std::uint32_t index = 0; index + 2 < vertexCount; index += 6) {
            const MeshVertex& va = chunk.vertices[index];
            const MeshVertex& vb = chunk.vertices[index + 1];
            const MeshVertex& vc = chunk.vertices[index + 2];
            collisionTriangles_.push_back({
                {va.px, va.py, va.pz},
                {vb.px, vb.py, vb.pz},
                {vc.px, vc.py, vc.pz},
                {va.nx, va.ny, va.nz}});
        }
        AddCollisionGridRange(firstCollisionTriangle);
        if (chunk.materialSlot == 0) {
            texturedRenderers_.emplace_back();
            texturedRenderers_.back().Init(descriptor, firstTexture_);
        } else {
            worldRenderers_.emplace_back();
            worldRenderers_.back().Init(descriptor);
            worldRenderers_.back().AmbientLightColor = {0.35f, 0.35f, 0.35f};
        }
        return glGetError() == GL_NO_ERROR;
    }

    bool BeginWorldMeshUpload(WorldMeshPreparation preparation) {
        if (!preparation.passed || preparation.chunks.empty()) return false;
        collisionTriangles_.clear();
        collisionGrid_.clear();
        oversizedCollisionTriangles_.clear();
        pendingWorldMesh_ = std::move(preparation);
        pendingWorldMeshChunk_ = 0u;
        return true;
    }

    bool UploadNextWorldMeshChunk() {
        if (pendingWorldMeshChunk_ >= pendingWorldMesh_.chunks.size()) return false;
        if (!UploadWorldMeshChunk(pendingWorldMesh_.chunks[pendingWorldMeshChunk_])) return false;
        pendingWorldMesh_.chunks[pendingWorldMeshChunk_].vertices.clear();
        ++pendingWorldMeshChunk_;
        return true;
    }

    bool LoadWorldMesh(bool loadTexture = true, bool buildCollisionGrid = true) {
        constexpr const char* path =
            "/data/user/0/dev.deusex.questvr.smoketest/files/DeusEx/quest-world.mesh";
        if (loadTexture && !LoadFirstTexture()) return false;
        std::FILE* file = std::fopen(path, "rb");
        if (file == nullptr) return false;
        collisionTriangles_.clear();
        std::uint32_t magic{}, version{}, chunkCount{};
        bool ok = std::fread(&magic, sizeof(magic), 1, file) == 1 &&
            std::fread(&version, sizeof(version), 1, file) == 1 &&
            std::fread(&chunkCount, sizeof(chunkCount), 1, file) == 1 &&
            magic == 0x4d515844u && version == 2 && chunkCount > 0 && chunkCount < 128;
        std::uint32_t texturedChunks{};
        for (std::uint32_t chunk = 0; ok && chunk < chunkCount; ++chunk) {
            std::int32_t materialSlot{};
            std::uint32_t vertexCount{};
            ok = std::fread(&materialSlot, sizeof(materialSlot), 1, file) == 1 &&
                std::fread(&vertexCount, sizeof(vertexCount), 1, file) == 1 &&
                vertexCount > 0 && vertexCount <= 60000 && vertexCount % 3 == 0;
            if (materialSlot == 0) ++texturedChunks;
            std::vector<MeshVertex> vertices(vertexCount);
            if (ok) ok = std::fread(
                vertices.data(), sizeof(MeshVertex), vertexCount, file) == vertexCount;
            if (!ok) break;

            OVRFW::GlGeometry::Descriptor descriptor;
            descriptor.attribs.position.reserve(vertexCount);
            descriptor.attribs.normal.reserve(vertexCount);
            descriptor.attribs.uv0.reserve(vertexCount);
            descriptor.attribs.color.reserve(vertexCount);
            descriptor.indices.reserve(vertexCount);
            for (std::uint32_t index = 0; index < vertexCount; ++index) {
                const MeshVertex& vertex = vertices[index];
                descriptor.attribs.position.emplace_back(vertex.px, vertex.py, vertex.pz);
                descriptor.attribs.normal.emplace_back(vertex.nx, vertex.ny, vertex.nz);
                descriptor.attribs.uv0.emplace_back(vertex.u, vertex.v);
                const float shade = 0.25f + 0.55f * (vertex.nz * 0.5f + 0.5f);
                if (materialSlot == 0) {
                    descriptor.attribs.color.emplace_back(
                        static_cast<float>(vertex.materialSlot) / 255.0f, 1.0f, 1.0f, 1.0f);
                } else {
                    descriptor.attribs.color.emplace_back(
                        0.15f * shade, 0.8f * shade, 0.55f * shade, 1.0f);
                }
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
            if (materialSlot == 0) {
                texturedRenderers_.emplace_back();
                texturedRenderers_.back().Init(descriptor, firstTexture_);
            } else {
                worldRenderers_.emplace_back();
                worldRenderers_.back().Init(descriptor);
                worldRenderers_.back().AmbientLightColor = {0.35f, 0.35f, 0.35f};
            }
        }
        std::fclose(file);
        if (!ok) {
            for (auto& renderer : worldRenderers_) renderer.Shutdown();
            worldRenderers_.clear();
            for (auto& renderer : texturedRenderers_) renderer.Shutdown();
            texturedRenderers_.clear();
            collisionTriangles_.clear();
            collisionGrid_.clear();
            oversizedCollisionTriangles_.clear();
            return false;
        }
        if (buildCollisionGrid) BuildCollisionGrid();
        ALOG(
            "DeusExQuest: loaded %s BSP mesh in %u GPU chunks (%u textured) with %zu collision triangles in %zu cells",
            currentMapName_.c_str(),
            chunkCount,
            texturedChunks,
            collisionTriangles_.size(),
            collisionGrid_.size());
        return true;
    }

    bool LoadActorTextures() {
        PortableTextureArray array;
        try {
            array = BuildPortableRuntimeActorTextureArray(96, 96);
        } catch (const std::exception& error) {
            ALOG("DeusExQuest: actor texture decode failed: %s", error.what());
            return false;
        }
        return UploadActorTextures(std::move(array));
    }

    bool UploadActorTextures(PortableTextureArray array) {
        ALOG(
            "DeusExQuest: decoded %zu/%zu actor textures (%zu fallback layers)",
            array.decodedTextures,
            array.texturePaths.size(),
            array.failedTextures);
        if (!array.passed) return false;
        GLuint texture{};
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D_ARRAY, texture);
        glTexImage3D(
            GL_TEXTURE_2D_ARRAY,
            0,
            GL_RGBA8,
            static_cast<GLsizei>(array.width),
            static_cast<GLsizei>(array.height),
            static_cast<GLsizei>(array.texturePaths.size()),
            0,
            GL_RGBA,
            GL_UNSIGNED_BYTE,
            array.rgba.data());
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        const GLenum error = glGetError();
        glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
        if (error != GL_NO_ERROR) {
            if (texture != 0) glDeleteTextures(1, &texture);
            ALOG("DeusExQuest: actor texture-array upload failed with GL error 0x%x", error);
            return false;
        }
        actorTexture_ = OVRFW::GlTexture(
            texture,
            GL_TEXTURE_2D_ARRAY,
            static_cast<int>(array.width),
            static_cast<int>(array.height));
        actorTexturePaths_ = std::move(array.texturePaths);
        return actorTexture_.IsValid();
    }

    bool BeginActorTextureUpload(PortableTextureArray array) {
        ALOG(
            "DeusExQuest: decoded %zu/%zu actor textures (%zu fallback layers)",
            array.decodedTextures,
            array.texturePaths.size(),
            array.failedTextures);
        if (!array.passed || array.rgba.empty() || array.texturePaths.empty()) return false;
        GLuint texture{};
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D_ARRAY, texture);
        glTexImage3D(
            GL_TEXTURE_2D_ARRAY,
            0,
            GL_RGBA8,
            static_cast<GLsizei>(array.width),
            static_cast<GLsizei>(array.height),
            static_cast<GLsizei>(array.texturePaths.size()),
            0,
            GL_RGBA,
            GL_UNSIGNED_BYTE,
            nullptr);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        const GLenum error = glGetError();
        glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
        if (error != GL_NO_ERROR) {
            if (texture != 0u) glDeleteTextures(1, &texture);
            return false;
        }
        pendingActorTextureId_ = texture;
        pendingActorTextureWidth_ = array.width;
        pendingActorTextureHeight_ = array.height;
        pendingActorTextureLayers_ = static_cast<std::uint32_t>(array.texturePaths.size());
        pendingActorTextureLayersUploaded_ = 0u;
        pendingActorTextureRgba_ = std::move(array.rgba);
        pendingActorTexturePaths_ = std::move(array.texturePaths);
        return true;
    }

    bool UploadActorTextureLayers(std::uint32_t maximumLayers) {
        if (pendingActorTextureId_ == 0u || maximumLayers == 0u ||
            pendingActorTextureLayersUploaded_ >= pendingActorTextureLayers_) {
            return false;
        }
        const std::uint32_t layers = std::min(
            maximumLayers,
            pendingActorTextureLayers_ - pendingActorTextureLayersUploaded_);
        const std::size_t bytesPerLayer = static_cast<std::size_t>(
            pendingActorTextureWidth_) * pendingActorTextureHeight_ * 4u;
        glBindTexture(GL_TEXTURE_2D_ARRAY, pendingActorTextureId_);
        glTexSubImage3D(
            GL_TEXTURE_2D_ARRAY,
            0,
            0,
            0,
            static_cast<GLint>(pendingActorTextureLayersUploaded_),
            static_cast<GLsizei>(pendingActorTextureWidth_),
            static_cast<GLsizei>(pendingActorTextureHeight_),
            static_cast<GLsizei>(layers),
            GL_RGBA,
            GL_UNSIGNED_BYTE,
            pendingActorTextureRgba_.data() +
                bytesPerLayer * pendingActorTextureLayersUploaded_);
        glFinish();
        const GLenum error = glGetError();
        glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
        if (error != GL_NO_ERROR) return false;
        pendingActorTextureLayersUploaded_ += layers;
        if (pendingActorTextureLayersUploaded_ == pendingActorTextureLayers_) {
            actorTexture_ = OVRFW::GlTexture(
                pendingActorTextureId_,
                GL_TEXTURE_2D_ARRAY,
                static_cast<int>(pendingActorTextureWidth_),
                static_cast<int>(pendingActorTextureHeight_));
            pendingActorTextureId_ = 0u;
            pendingActorTextureRgba_.clear();
            actorTexturePaths_ = std::move(pendingActorTexturePaths_);
            ALOG(
                "DeusExQuest: staged actor texture upload complete: %u layers at %ux%u",
                pendingActorTextureLayers_,
                pendingActorTextureWidth_,
                pendingActorTextureHeight_);
        }
        return true;
    }

    static WorldTexturePreparation LoadWorldTextureCacheCpu() {
        constexpr const char* path =
            "/data/user/0/dev.deusex.questvr.smoketest/files/DeusEx/quest-material-array.rgba";
        WorldTexturePreparation result;
        std::FILE* file = std::fopen(path, "rb");
        if (file == nullptr) return result;
        std::uint32_t magic{}, version{};
        result.passed = std::fread(&magic, sizeof(magic), 1, file) == 1 &&
            std::fread(&version, sizeof(version), 1, file) == 1 &&
            std::fread(&result.width, sizeof(result.width), 1, file) == 1 &&
            std::fread(&result.height, sizeof(result.height), 1, file) == 1 &&
            std::fread(&result.layers, sizeof(result.layers), 1, file) == 1 &&
            magic == 0x41515844u && version == 1u &&
            result.width > 0u && result.height > 0u && result.layers > 0u &&
            result.layers <= 255u && result.width <= 2048u && result.height <= 2048u;
        if (result.passed) {
            result.rgba.resize(
                static_cast<std::size_t>(result.width) * result.height * result.layers * 4u);
            result.passed =
                std::fread(result.rgba.data(), 1, result.rgba.size(), file) == result.rgba.size();
        }
        std::fclose(file);
        if (!result.passed) result.rgba.clear();
        return result;
    }

    bool BeginWorldTextureUpload(WorldTexturePreparation preparation) {
        if (!preparation.passed || preparation.rgba.empty()) return false;
        GLuint texture{};
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D_ARRAY, texture);
        glTexImage3D(
            GL_TEXTURE_2D_ARRAY,
            0,
            GL_RGBA8,
            static_cast<GLsizei>(preparation.width),
            static_cast<GLsizei>(preparation.height),
            static_cast<GLsizei>(preparation.layers),
            0,
            GL_RGBA,
            GL_UNSIGNED_BYTE,
            nullptr);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        const GLenum error = glGetError();
        glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
        if (error != GL_NO_ERROR) {
            if (texture != 0u) glDeleteTextures(1, &texture);
            return false;
        }
        pendingWorldTextureId_ = texture;
        pendingWorldTextureWidth_ = preparation.width;
        pendingWorldTextureHeight_ = preparation.height;
        pendingWorldTextureLayers_ = preparation.layers;
        pendingWorldTextureLayersUploaded_ = 0u;
        pendingWorldTextureRgba_ = std::move(preparation.rgba);
        return true;
    }

    bool UploadWorldTextureLayers(std::uint32_t maximumLayers) {
        if (pendingWorldTextureId_ == 0u || maximumLayers == 0u ||
            pendingWorldTextureLayersUploaded_ >= pendingWorldTextureLayers_) {
            return false;
        }
        const std::uint32_t layers = std::min(
            maximumLayers,
            pendingWorldTextureLayers_ - pendingWorldTextureLayersUploaded_);
        const std::size_t bytesPerLayer = static_cast<std::size_t>(
            pendingWorldTextureWidth_) * pendingWorldTextureHeight_ * 4u;
        glBindTexture(GL_TEXTURE_2D_ARRAY, pendingWorldTextureId_);
        glTexSubImage3D(
            GL_TEXTURE_2D_ARRAY,
            0,
            0,
            0,
            static_cast<GLint>(pendingWorldTextureLayersUploaded_),
            static_cast<GLsizei>(pendingWorldTextureWidth_),
            static_cast<GLsizei>(pendingWorldTextureHeight_),
            static_cast<GLsizei>(layers),
            GL_RGBA,
            GL_UNSIGNED_BYTE,
            pendingWorldTextureRgba_.data() +
                bytesPerLayer * pendingWorldTextureLayersUploaded_);
        glFinish();
        const GLenum error = glGetError();
        glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
        if (error != GL_NO_ERROR) return false;
        pendingWorldTextureLayersUploaded_ += layers;
        if (pendingWorldTextureLayersUploaded_ == pendingWorldTextureLayers_) {
            firstTexture_ = OVRFW::GlTexture(
                pendingWorldTextureId_,
                GL_TEXTURE_2D_ARRAY,
                static_cast<int>(pendingWorldTextureWidth_),
                static_cast<int>(pendingWorldTextureHeight_));
            pendingWorldTextureId_ = 0u;
            pendingWorldTextureRgba_.clear();
            ALOG(
                "DeusExQuest: staged UE1 material array upload complete: %u layers at %ux%u",
                pendingWorldTextureLayers_,
                pendingWorldTextureWidth_,
                pendingWorldTextureHeight_);
        }
        return true;
    }

    bool LoadFirstTexture() {
        constexpr const char* path =
            "/data/user/0/dev.deusex.questvr.smoketest/files/DeusEx/quest-material-array.rgba";
        std::FILE* file = std::fopen(path, "rb");
        if (file == nullptr) return false;
        std::uint32_t magic{}, version{}, width{}, height{}, layers{};
        bool ok = std::fread(&magic, sizeof(magic), 1, file) == 1 &&
            std::fread(&version, sizeof(version), 1, file) == 1 &&
            std::fread(&width, sizeof(width), 1, file) == 1 &&
            std::fread(&height, sizeof(height), 1, file) == 1 &&
            std::fread(&layers, sizeof(layers), 1, file) == 1 &&
            magic == 0x41515844u && version == 1 && width > 0 && height > 0 &&
            layers > 0 && layers <= 255 && width <= 2048 && height <= 2048;
        std::vector<std::uint8_t> rgba;
        if (ok) {
            rgba.resize(static_cast<std::size_t>(width) * height * layers * 4u);
            ok = std::fread(rgba.data(), 1, rgba.size(), file) == rgba.size();
        }
        std::fclose(file);
        if (!ok) return false;
        GLuint texture{};
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_2D_ARRAY, texture);
        glTexImage3D(
            GL_TEXTURE_2D_ARRAY,
            0,
            GL_RGBA8,
            static_cast<GLsizei>(width),
            static_cast<GLsizei>(height),
            static_cast<GLsizei>(layers),
            0,
            GL_RGBA,
            GL_UNSIGNED_BYTE,
            rgba.data());
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        const GLenum error = glGetError();
        glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
        if (error != GL_NO_ERROR) {
            if (texture != 0) glDeleteTextures(1, &texture);
            ALOG("DeusExQuest: texture-array upload failed with GL error 0x%x", error);
            return false;
        }
        firstTexture_ = OVRFW::GlTexture(
            texture, GL_TEXTURE_2D_ARRAY, static_cast<int>(width), static_cast<int>(height));
        ALOG(
            "DeusExQuest: uploaded UE1 material array: %u layers at %ux%u",
            layers,
            width,
            height);
        return firstTexture_.IsValid();
    }

    std::vector<OVRFW::GeometryRenderer> worldRenderers_;
    std::vector<TexturedGeometryRenderer> texturedRenderers_;
    std::vector<PortableActorSnapshot> actorSnapshots_;
    std::vector<InteractiveActor> interactiveActors_;
    OVRFW::GlTexture firstTexture_;
    OVRFW::GlTexture actorTexture_;
    std::vector<std::string> actorTexturePaths_;
    static constexpr std::size_t invalidRendererIndex_ =
        std::numeric_limits<std::size_t>::max();
    std::size_t actorWorldRendererIndex_{invalidRendererIndex_};
    std::size_t actorTexturedRendererIndex_{invalidRendererIndex_};
    AAudioStream* ambientStream_{};
    std::mutex audioMutex_;
    std::vector<std::int16_t> ambientSamples_;
    std::size_t ambientCursor_{};
    std::uint32_t audioSampleRate_{};
    std::vector<std::int16_t> dialogueSamples_;
    std::size_t dialogueCursor_{};
    std::vector<CollisionTriangle> collisionTriangles_;
    std::unordered_map<std::int64_t, std::vector<std::uint32_t>> collisionGrid_;
    std::vector<std::uint32_t> oversizedCollisionTriangles_;
    OVR::Vector3f worldPosition_{0.0f, 0.0f, 0.0f};
    OVR::Vector3f currentHeadStage_{};
    OVR::Vector3f actorStreamingCenter_{};
    float sceneYaw_{};
    bool turnLatch_{};
    bool fireLatch_{};
    bool inventoryCycleLatch_{};
    inline static std::size_t selectedInventoryIndex_{};
    std::size_t performanceFrames_{};
    float performanceSeconds_{};
    float performanceWorstDelta_{};
    OVRFW::TinyUI ui_;
    OVRFW::VRMenuObject* hudLabel_{};
    std::size_t displayedInventoryCount_{invalidRendererIndex_};
    float displayedPlayerHealth_{-1.0f};
    std::string displayedSelectedInventory_;
    std::string interactionStatus_;
    std::string displayedInteractionStatus_;
    float interactionStatusSeconds_{};
    std::unordered_map<std::string, std::size_t> dialogueOffsets_;
    static constexpr const char* gameRoot_ =
        "/data/user/0/dev.deusex.questvr.smoketest/files/DeusEx";
    std::vector<std::string> mapNames_;
    std::string currentMapName_{"00_Training"};
    std::size_t currentMapIndex_{};
    float mapRequestPollSeconds_{};
    float mapTravelCooldown_{3.0f};
    bool restorePoseAfterTransition_{};
    OVR::Vector3f restoredWorldPosition_{};
    float restoredSceneYaw_{};
    std::string pendingMapName_;
    std::future<MapPreparation> mapCacheFuture_;
    std::string transitionMapName_;
    MapTransitionPhase transitionPhase_{MapTransitionPhase::Idle};
    std::vector<PortableActorSnapshot> preparedActorSnapshots_;
    PortableTextureArray preparedActorTextures_;
    WorldTexturePreparation preparedWorldTexture_;
    WorldMeshPreparation preparedWorldMesh_;
    WorldMeshPreparation pendingWorldMesh_;
    std::size_t pendingWorldMeshChunk_{};
    GLuint pendingWorldTextureId_{};
    std::uint32_t pendingWorldTextureWidth_{};
    std::uint32_t pendingWorldTextureHeight_{};
    std::uint32_t pendingWorldTextureLayers_{};
    std::uint32_t pendingWorldTextureLayersUploaded_{};
    std::vector<std::uint8_t> pendingWorldTextureRgba_;
    GLuint pendingActorTextureId_{};
    std::uint32_t pendingActorTextureWidth_{};
    std::uint32_t pendingActorTextureHeight_{};
    std::uint32_t pendingActorTextureLayers_{};
    std::uint32_t pendingActorTextureLayersUploaded_{};
    std::vector<std::uint8_t> pendingActorTextureRgba_;
    std::vector<std::string> pendingActorTexturePaths_;
    OVRFW::ControllerRenderer leftController_;
    OVRFW::ControllerRenderer rightController_;
};

ENTRY_POINT(DeusExQuestApp)
