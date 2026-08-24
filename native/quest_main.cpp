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
#include <sys/stat.h>

#include "Input/ControllerRenderer.h"
#include "Input/TinyUI.h"
#include "Render/GeometryBuilder.h"
#include "Render/GeometryRenderer.h"
#include "Render/GlGeometry.h"
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
        const OVRFW::GlTexture& texture,
        bool cullEnable = true) {
        static const char* vertexShader = R"glsl(
            attribute highp vec4 Position;
            attribute highp vec2 TexCoord;
            attribute lowp vec4 VertexColor;
            varying lowp vec2 oTexCoord;
            varying mediump float oLayer;
            varying lowp vec3 oLight;
            void main() {
                gl_Position = TransformVertex(Position);
                oTexCoord = TexCoord;
                oLayer = VertexColor.r * 255.0;
                oLight = VertexColor.gba;
            }
        )glsl";
        static const char* fragmentShader = R"glsl(
            precision lowp float;
            uniform highp sampler2DArray Texture0;
            varying lowp vec2 oTexCoord;
            varying mediump float oLayer;
            varying lowp vec3 oLight;
            void main() {
                lowp vec4 texel = texture(
                    Texture0, vec3(fract(oTexCoord), floor(oLayer + 0.5)));
                // Palette index zero carries transparent alpha in actor layers.
                // Discarding it is safe for opaque meshes and required for UE1
                // masked decorations such as plants and grilles.
                if (texel.a < 0.5) discard;
                gl_FragColor = vec4(texel.rgb * oLight, texel.a);
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
        command.GpuState.cullEnable = cullEnable;
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

class PersonaUiRenderer {
   public:
    void Init(const OVRFW::GlTexture& texture) {
        static const char* vertexShader = R"glsl(
            attribute highp vec4 Position;
            attribute highp vec2 TexCoord;
            varying lowp vec2 oTexCoord;
            void main() {
                gl_Position = TransformVertex(Position);
                oTexCoord = TexCoord;
            }
        )glsl";
        static const char* fragmentShader = R"glsl(
            precision lowp float;
            uniform sampler2D Texture0;
            varying lowp vec2 oTexCoord;
            void main() {
                gl_FragColor = texture2D(Texture0, oTexCoord);
            }
        )glsl";
        static OVRFW::ovrProgramParm parms[] = {
            {.Name = "Texture0", .Type = OVRFW::ovrProgramParmType::TEXTURE_SAMPLED},
        };
        program_ = OVRFW::GlProgram::Build(
            "", vertexShader, "", fragmentShader, parms, 1);
        surface_.geo = OVRFW::BuildTesselatedQuad(1, 1, true);
        auto& command = surface_.graphicsCommand;
        command.Program = program_;
        command.Textures[0] = texture;
        command.UniformData[0].Data = &command.Textures[0];
        command.GpuState.depthEnable = command.GpuState.depthMaskEnable = false;
        command.GpuState.blendEnable = OVRFW::ovrGpuState::BLEND_DISABLE;
        initialized_ = true;
    }
    void Shutdown() {
        if (!initialized_) return;
        surface_.geo.Free();
        if (program_.IsValid()) OVRFW::GlProgram::Free(program_);
        initialized_ = false;
    }
    void SetPose(const OVR::Posef& pose) {
        modelMatrix_ = OVR::Matrix4f(pose) *
            OVR::Matrix4f::Scaling(0.60f, 0.38f, 1.0f);
    }
    void Render(std::vector<OVRFW::ovrDrawSurface>& surfaces) {
        if (initialized_) surfaces.emplace_back(modelMatrix_, &surface_);
    }
    bool IsInitialized() const { return initialized_; }

   private:
    OVRFW::ovrSurfaceDef surface_;
    OVRFW::GlProgram program_;
    OVR::Matrix4f modelMatrix_ = OVR::Matrix4f::Identity();
    bool initialized_{};
};

enum class PersonaPage : std::uint8_t {
    Inventory,
    Health,
    GoalsNotes,
    Logs,
    Count,
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
            OVR::Vector2f(720.0f, 180.0f));
        hudLabel_->SetTextLocalPosition({0.0f, -0.025f, 0.0f});
        hudLabel_->SetTextColor(OVR::Vector4f(0.82f, 0.68f, 0.25f, 1.0f));
        inventoryLabel_ = ui_.AddLabel(
            "INVENTORY GRID",
            OVR::Vector3f(0.0f, 0.0f, 0.0f),
            OVR::Vector2f(950.0f, 600.0f));
        inventoryLabel_->SetTextColor(OVR::Vector4f(0.9f, 0.74f, 0.28f, 1.0f));
        inventoryLabel_->SetSurfaceColor(0, OVR::Vector4f(0.018f, 0.055f, 0.05f, 0.98f));
        inventoryLabel_->SetTextLocalPosition({0.145f, 0.134f, 0.0f});
        inventoryLabel_->SetTextLocalScale({0.412f, 0.412f, 1.0f});
        inventoryLabel_->SetVisible(false);
        return true;
    }

    void AppShutdown(const xrJava* context) override {
        inventoryLabel_ = nullptr;
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
        actorSnapshots_ = GetPortableRuntimeMapActors();
        activeMapLights_ = BuildMapLights(actorSnapshots_);
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
        } else {
            ReplaceSpatialAudioEmitters(
                PrepareSpatialAudioEmitters(actorSnapshots_, audioSampleRate_));
        }
        LoadOriginalPersonaBackground();
        ALOG("DeusExQuest: project-owned OpenXR runtime initialized");
        return true;
    }

    void Update(const OVRFW::ovrApplFrameIn& frame) override {
        PollDialogueAudioDecode();
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
        const bool mapLoading = !pendingMapName_.empty() || !transitionMapName_.empty();
        if (!mapLoading && frame.Clicked(frame.kButtonMenu)) {
            SetInventoryMenuOpen(!inventoryMenuOpen_);
        }
        bool dismissedMenuWithB{};
        if (inventoryMenuOpen_ && frame.Clicked(frame.kButtonB)) {
            SetInventoryMenuOpen(false);
            dismissedMenuWithB = true;
        }
        const bool gameplayActive = playerAlive && !mapLoading && !inventoryMenuOpen_;
        const float yaw = headYaw - sceneYaw_;
        const float forwardX = std::sin(yaw);
        const float forwardZ = -std::cos(yaw);
        const float rightX = std::cos(yaw);
        const float rightZ = std::sin(yaw);
        UpdateSpatialAudioGains(
            StageToLocal(currentHeadStage_, worldPosition_), rightX, rightZ);
        constexpr float moveSpeed = 2.2f;
        const float moveX = gameplayActive
            ? frame.LeftRemoteJoystick.x * rightX + frame.LeftRemoteJoystick.y * forwardX
            : 0.0f;
        const float moveZ = gameplayActive
            ? frame.LeftRemoteJoystick.x * rightZ + frame.LeftRemoteJoystick.y * forwardZ
            : 0.0f;
        OVR::Vector3f candidate = worldPosition_;
        candidate.x -= moveX * moveSpeed * frame.DeltaSeconds;
        candidate.z -= moveZ * moveSpeed * frame.DeltaSeconds;

        const bool turnPressed = std::fabs(frame.RightRemoteJoystick.x) > 0.7f;
        if (gameplayActive && turnPressed && !turnLatch_) {
            constexpr float snapRadians = 3.14159265358979323846f / 6.0f;
            sceneYaw_ += std::copysign(snapRadians, frame.RightRemoteJoystick.x);
        } else if (inventoryMenuOpen_ && turnPressed && !turnLatch_) {
            const std::size_t pageCount = static_cast<std::size_t>(PersonaPage::Count);
            std::size_t page = static_cast<std::size_t>(personaPage_);
            page = frame.RightRemoteJoystick.x > 0.0f
                ? (page + 1u) % pageCount
                : (page + pageCount - 1u) % pageCount;
            personaPage_ = static_cast<PersonaPage>(page);
            inventoryMenuDirty_ = true;
        }
        turnLatch_ = turnPressed;
        const bool choiceCyclePressed = std::fabs(frame.RightRemoteJoystick.y) > 0.7f;
        if (inventoryMenuOpen_ && personaPage_ == PersonaPage::Inventory &&
            choiceCyclePressed && !choiceCycleLatch_) {
            const std::size_t count = GetPortableRuntimeInventoryCount();
            if (count != 0u) {
                if (frame.RightRemoteJoystick.y > 0.0f) {
                    selectedInventoryIndex_ = selectedInventoryIndex_ == 0u
                        ? count - 1u
                        : selectedInventoryIndex_ - 1u;
                } else {
                    selectedInventoryIndex_ = (selectedInventoryIndex_ + 1u) % count;
                }
                inventoryMenuDirty_ = true;
                displayedInventoryCount_ = invalidRendererIndex_;
            }
        } else if (!pendingChoices_.empty() && choiceCyclePressed && !choiceCycleLatch_) {
            if (frame.RightRemoteJoystick.y > 0.0f) {
                pendingChoiceIndex_ = pendingChoiceIndex_ == 0u
                    ? pendingChoices_.size() - 1u
                    : pendingChoiceIndex_ - 1u;
            } else {
                pendingChoiceIndex_ = (pendingChoiceIndex_ + 1u) % pendingChoices_.size();
            }
            RefreshChoiceStatus();
        }
        choiceCycleLatch_ = choiceCyclePressed;

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
        mapTravelCooldown_ = std::max(0.0f, mapTravelCooldown_ - frame.DeltaSeconds);
        if (gameplayActive && mapTravelCooldown_ <= 0.0f) {
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
        if (inventoryMenuOpen_ && personaPage_ == PersonaPage::Inventory &&
            frame.Clicked(frame.kButtonA)) {
            const std::vector<std::string> inventory = GetPortableRuntimeInventoryItems();
            if (inventory.empty()) {
                inventoryMenuDirty_ = true;
            } else if (!UseSelectedConsumable()) {
                interactionStatus_ = "EQUIPPED " +
                    SelectedInventoryLabel(inventory);
                interactionStatusSeconds_ = 2.0f;
                SetInventoryMenuOpen(false);
            } else {
                inventoryMenuDirty_ = true;
            }
        } else if (gameplayActive && frame.RightRemoteTracked && frame.Clicked(frame.kButtonA)) {
            if (!pendingChoices_.empty()) {
                ConfirmPendingChoice();
            } else if (UseTargetedActor(frame.RightRemotePointPose)) {
                DestroyActorGeometry();
                actorSnapshots_ = GetPortableRuntimeMapActors();
                BuildActorMarkers();
            }
        }
        const bool firePressed = frame.RightRemoteIndexTrigger > 0.75f;
        if (gameplayActive && frame.RightRemoteTracked && firePressed && !fireLatch_) {
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
        if (gameplayActive && gripPressed && !inventoryCycleLatch_) {
            const std::size_t count = GetPortableRuntimeInventoryCount();
            if (count != 0u) selectedInventoryIndex_ = (selectedInventoryIndex_ + 1u) % count;
            displayedInventoryCount_ = invalidRendererIndex_;
        }
        inventoryCycleLatch_ = gripPressed;
        if ((gameplayActive || inventoryMenuOpen_) && frame.Clicked(frame.kButtonY)) {
            if (pendingChoices_.empty()) {
                SaveGameState();
            } else {
                interactionStatus_ = "FINISH RESPONSE BEFORE SAVING";
                interactionStatusSeconds_ = 3.0f;
            }
        }
        if (!mapLoading && frame.Clicked(frame.kButtonX)) LoadGameState();
        if (gameplayActive && !dismissedMenuWithB && frame.Clicked(frame.kButtonB)) LoadNextMap();
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
                    "%s   HEALTH %.0f   INVENTORY %zu\nITEM %s\n%s\nA USE   TRIGGER FIRE   GRIP CYCLE\nMENU INVENTORY   B NEXT MAP   Y SAVE   X LOAD",
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
        UpdateInventoryMenu(frame, mapLoading);
        ui_.Update(frame);
    }

    void Render(
        const OVRFW::ovrApplFrameIn& frame,
        OVRFW::ovrRendererOutput& output) override {
        for (auto& renderer : worldRenderers_) renderer.Render(output.Surfaces);
        for (auto& renderer : texturedRenderers_) renderer.Render(output.Surfaces);
        if (inventoryMenuOpen_) personaRenderer_.Render(output.Surfaces);
        ui_.Render(frame, output);
        if (frame.LeftRemoteTracked) leftController_.Render(output.Surfaces);
        if (frame.RightRemoteTracked) rightController_.Render(output.Surfaces);
    }

    void AppRenderFrame(
        const OVRFW::ovrApplFrameIn& frame,
        OVRFW::ovrRendererOutput& output) override {
        Render(frame, output);
        for (int eye = 0; eye < GetNumFramebuffers(); ++eye) {
            ovrFramebuffer* frameBuffer = GetFrameBuffer(eye);
            ovrFramebuffer_Acquire(frameBuffer);
            ovrFramebuffer_SetCurrent(frameBuffer);
            AppEyeGLStateSetup(frame, frameBuffer, eye);
            OVRFW::XrApp::AppRenderEye(frame, output, eye);
            ovrFramebuffer_Resolve(frameBuffer);
            if (eye == 0 && captureScreenshotRequested_) {
                if (captureScreenshotDelayFrames_ == 0u) {
                    captureScreenshotRequested_ = false;
                    ovrFramebuffer_SetNone();
                    CaptureResolvedEyeFramebuffer(*frameBuffer);
                } else {
                    --captureScreenshotDelayFrames_;
                }
            }
            ovrFramebuffer_Release(frameBuffer);
        }
        ovrFramebuffer_SetNone();
    }

    void SessionEnd() override {
        if (dialogueDecodeFuture_.valid()) dialogueDecodeFuture_.wait();
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
        personaRenderer_.Shutdown();
        if (personaTextureId_ != 0u) {
            glDeleteTextures(1, &personaTextureId_);
            personaTextureId_ = 0u;
        }
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

    struct SpatialAudioEmitter {
        OVR::Vector3f localPosition;
        std::string soundPath;
        std::shared_ptr<const std::vector<std::int16_t>> monoSamples;
        std::size_t cursor{};
        float radiusMeters{30.0f};
        float volume{1.0f};
        float leftGain{};
        float rightGain{};
    };

    struct MapLight {
        OVR::Vector3f localPosition;
        OVR::Vector3f color;
        OVR::Vector3f direction;
        float radiusMeters{};
        float intensity{};
        float coneCosine{-1.0f};
    };

    struct MapPreparation {
        bool passed{};
        std::string error;
        std::vector<PortableActorSnapshot> actors;
        PortableTextureArray actorTextures;
        WorldTexturePreparation worldTexture;
        WorldMeshPreparation worldMesh;
        std::vector<SpatialAudioEmitter> spatialAudioEmitters;
        std::vector<MapLight> lights;
    };

    struct InteractiveActor {
        OVR::Vector3f localPosition;
        std::string objectPath;
        std::string classPath;
        bool travel{};
        std::string destinationMap;
    };

    static std::vector<MapLight> BuildMapLights(
        const std::vector<PortableActorSnapshot>& actors) {
        float originX = -1149.244f;
        float originY = 825.844f;
        float originZ = -65.103f;
        for (const PortableActorSnapshot& actor : actors) {
            const std::size_t separator = actor.classPath.find_last_of('.');
            const std::string leafClass = separator == std::string::npos
                ? actor.classPath
                : actor.classPath.substr(separator + 1u);
            if (actor.hasLocation && leafClass == "PlayerStart") {
                originX = actor.x;
                originY = actor.y;
                originZ = actor.z;
                break;
            }
        }
        const auto hsvColor = [](std::uint8_t hue, std::uint8_t unrealSaturation) {
            const float h = static_cast<float>(hue) * 6.0f / 256.0f;
            const float saturation = 1.0f -
                static_cast<float>(unrealSaturation) / 255.0f;
            const float chroma = saturation;
            const float x = chroma * (1.0f - std::fabs(
                std::fmod(h, 2.0f) - 1.0f));
            OVR::Vector3f rgb;
            if (h < 1.0f) rgb = {chroma, x, 0.0f};
            else if (h < 2.0f) rgb = {x, chroma, 0.0f};
            else if (h < 3.0f) rgb = {0.0f, chroma, x};
            else if (h < 4.0f) rgb = {0.0f, x, chroma};
            else if (h < 5.0f) rgb = {x, 0.0f, chroma};
            else rgb = {chroma, 0.0f, x};
            const float white = 1.0f - chroma;
            return OVR::Vector3f(rgb.x + white, rgb.y + white, rgb.z + white);
        };
        std::vector<MapLight> lights;
        constexpr float unitsToMeters = 1.0f / 52.5f;
        constexpr float unrealAngle = 6.28318530717958647692f / 65536.0f;
        std::size_t spotlights{};
        std::size_t colored{};
        for (const PortableActorSnapshot& actor : actors) {
            if (!actor.light || !actor.hasLocation || actor.lightBrightness == 0u ||
                actor.lightRadius == 0u) continue;
            MapLight light;
            light.localPosition = {
                (actor.y - originY) * unitsToMeters,
                (actor.z - originZ) * unitsToMeters + 1.0f,
                -(actor.x - originX) * unitsToMeters};
            light.color = hsvColor(actor.lightHue, actor.lightSaturation);
            light.radiusMeters = std::max(
                1.0f, static_cast<float>(actor.lightRadius) * 25.0f * unitsToMeters);
            light.intensity = std::clamp(
                static_cast<float>(actor.lightBrightness) / 64.0f, 0.05f, 4.0f);
            const bool spotlight = actor.classPath.find("Spotlight") != std::string::npos;
            if (spotlight) {
                const float yaw = static_cast<float>(actor.yaw) * unrealAngle;
                const float pitch = static_cast<float>(actor.pitch) * unrealAngle;
                const float cosinePitch = std::cos(pitch);
                light.direction = {
                    cosinePitch * std::sin(yaw),
                    std::sin(pitch),
                    -cosinePitch * std::cos(yaw)};
                const float coneRadians = std::clamp(
                    static_cast<float>(actor.lightCone) *
                        3.14159265358979323846f / 256.0f,
                    0.0872665f,
                    1.553343f);
                light.coneCosine = std::cos(coneRadians);
                ++spotlights;
            }
            if (actor.lightSaturation < 224u) ++colored;
            lights.push_back(light);
        }
        ALOG(
            "DeusExQuest: prepared %zu map lights (%zu spotlights, %zu colored)",
            lights.size(),
            spotlights,
            colored);
        return lights;
    }

    OVR::Vector3f CalculateMapLighting(
        const OVR::Vector3f& position,
        const OVR::Vector3f& normal) const {
        OVR::Vector3f result{0.075f, 0.075f, 0.075f};
        for (const MapLight& light : activeMapLights_) {
            const OVR::Vector3f offset = light.localPosition - position;
            const float distanceSquared = offset.LengthSq();
            if (distanceSquared <= 0.000001f ||
                distanceSquared >= light.radiusMeters * light.radiusMeters) continue;
            const float distance = std::sqrt(distanceSquared);
            const OVR::Vector3f direction = offset * (1.0f / distance);
            float cone = 1.0f;
            if (light.coneCosine >= 0.0f) {
                const float alignment = (direction * -1.0f).Dot(light.direction);
                if (alignment <= light.coneCosine) continue;
                cone = std::clamp(
                    (alignment - light.coneCosine) / (1.0f - light.coneCosine),
                    0.0f,
                    1.0f);
            }
            const float distanceFade = 1.0f - distance / light.radiusMeters;
            const float diffuse = std::max(0.04f, normal.Dot(direction));
            const float contribution = light.intensity * distanceFade * distanceFade *
                diffuse * cone;
            result.x += light.color.x * contribution;
            result.y += light.color.y * contribution;
            result.z += light.color.z * contribution;
        }
        result.x = std::clamp(result.x, 0.04f, 1.35f);
        result.y = std::clamp(result.y, 0.04f, 1.35f);
        result.z = std::clamp(result.z, 0.04f, 1.35f);
        return result;
    }

    void ResetLightingStats() {
        lightingMinimum_ = std::numeric_limits<float>::infinity();
        lightingMaximum_ = 0.0f;
        lightingSum_ = 0.0;
        lightingSamples_ = 0u;
    }

    void RecordLighting(const OVR::Vector3f& lighting) {
        const float luminance =
            0.2126f * lighting.x + 0.7152f * lighting.y + 0.0722f * lighting.z;
        lightingMinimum_ = std::min(lightingMinimum_, luminance);
        lightingMaximum_ = std::max(lightingMaximum_, luminance);
        lightingSum_ += luminance;
        ++lightingSamples_;
    }

    void LogLightingStats() const {
        ALOG(
            "DeusExQuest: baked map lighting %zu lights over %zu vertices, luminance min=%.3f avg=%.3f max=%.3f",
            activeMapLights_.size(),
            lightingSamples_,
            lightingSamples_ == 0u ? 0.0f : lightingMinimum_,
            lightingSamples_ == 0u ? 0.0 : lightingSum_ / lightingSamples_,
            lightingSamples_ == 0u ? 0.0f : lightingMaximum_);
    }

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

    OVRFW::GlGeometry::Descriptor BuildCrossedSpriteDescriptor() const {
        OVRFW::GlGeometry::Descriptor descriptor;
        constexpr OVR::Vector3f positions[] = {
            {-0.5f, -0.5f, 0.0f}, {0.5f, -0.5f, 0.0f}, {0.5f, 0.5f, 0.0f},
            {-0.5f, -0.5f, 0.0f}, {0.5f, 0.5f, 0.0f}, {-0.5f, 0.5f, 0.0f},
            {0.0f, -0.5f, -0.5f}, {0.0f, -0.5f, 0.5f}, {0.0f, 0.5f, 0.5f},
            {0.0f, -0.5f, -0.5f}, {0.0f, 0.5f, 0.5f}, {0.0f, 0.5f, -0.5f},
        };
        constexpr OVR::Vector2f uv[] = {
            {0.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, 0.0f},
            {0.0f, 1.0f}, {1.0f, 0.0f}, {0.0f, 0.0f},
            {0.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, 0.0f},
            {0.0f, 1.0f}, {1.0f, 0.0f}, {0.0f, 0.0f},
        };
        descriptor.attribs.position.assign(std::begin(positions), std::end(positions));
        descriptor.attribs.uv0.assign(std::begin(uv), std::end(uv));
        descriptor.attribs.normal.assign(
            descriptor.attribs.position.size(), OVR::Vector3f(0.0f, 0.0f, 1.0f));
        descriptor.attribs.color.assign(
            descriptor.attribs.position.size(), OVR::Vector4f(1.0f, 1.0f, 1.0f, 1.0f));
        descriptor.indices.reserve(descriptor.attribs.position.size());
        for (std::size_t index = 0; index < descriptor.attribs.position.size(); ++index) {
            descriptor.indices.push_back(static_cast<OVRFW::TriangleIndex>(index));
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
        std::size_t brushInstances{};
        std::size_t spriteInstances{};
        std::size_t cubePlaceholders{};
        std::size_t hiddenActors{};
        const OVR::Vector3f playerLocal = StageToLocal(currentHeadStage_, worldPosition_);
        actorStreamingCenter_ = playerLocal;
        std::map<std::string, std::size_t> meshClasses;
        std::map<std::string, std::size_t> cubeClasses;
        std::map<std::string, OVRFW::GlGeometry::Descriptor> meshDescriptors;
        std::map<std::string, OVRFW::GlGeometry::Descriptor> brushDescriptors;
        const OVRFW::GlGeometry::Descriptor spriteDescriptor =
            BuildCrossedSpriteDescriptor();
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
            const OVR::Vector3f actorLighting = CalculateMapLighting(
                position, OVR::Vector3f(0.0f, 1.0f, 0.0f));
            if (!actor.meshPath.empty()) ++meshClasses[actor.meshClassPath];
            if (actor.hidden) ++hiddenActors;
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
            if (!actor.hidden && !actor.activated && actor.mover &&
                !actor.brushPath.empty()) {
                try {
                    auto descriptor = brushDescriptors.find(actor.brushPath);
                    if (descriptor == brushDescriptors.end()) {
                        const PortableLodMesh brush = GetPortableRuntimeBrush(actor.brushPath);
                        descriptor = brushDescriptors.emplace(
                            actor.brushPath, BuildLodMeshDescriptor(brush, 0u)).first;
                    }
                    constexpr float unrealAngle =
                        6.28318530717958647692f / 65536.0f;
                    const float yaw = -static_cast<float>(actor.yaw) * unrealAngle;
                    const float pitch = -static_cast<float>(actor.pitch) * unrealAngle;
                    const float roll = static_cast<float>(actor.roll) * unrealAngle;
                    geometry.Add(
                        descriptor->second,
                        OVRFW::GeometryBuilder::kInvalidIndex,
                        OVR::Vector4f(
                            std::clamp(actorLighting.x, 0.18f, 1.0f),
                            std::clamp(actorLighting.y, 0.18f, 1.0f),
                            std::clamp(actorLighting.z, 0.18f, 1.0f),
                            1.0f),
                        OVR::Matrix4f::Translation(position) *
                            OVR::Matrix4f(OVR::Quatf(
                                OVR::Vector3f(0.0f, 1.0f, 0.0f), yaw)) *
                            OVR::Matrix4f(OVR::Quatf(
                                OVR::Vector3f(1.0f, 0.0f, 0.0f), pitch)) *
                            OVR::Matrix4f(OVR::Quatf(
                                OVR::Vector3f(0.0f, 0.0f, 1.0f), roll)));
                    renderedMesh = true;
                    ++brushInstances;
                } catch (const std::exception& error) {
                    ALOG(
                        "DeusExQuest: mover brush render fallback for %s: %s",
                        actor.brushPath.c_str(),
                        error.what());
                }
            }
            if (!renderedMesh && !actor.hidden && !actor.meshPath.empty()) {
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
                        std::int32_t textureIndex = static_cast<std::int32_t>(material);
                        if (material < mesh.materialTextureIndices.size()) {
                            textureIndex = mesh.materialTextureIndices[material];
                        }
                        if (texturePath.empty() && textureIndex >= 0 &&
                            static_cast<std::size_t>(textureIndex) < mesh.texturePaths.size()) {
                            texturePath = mesh.texturePaths[static_cast<std::size_t>(textureIndex)];
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
                                actorLighting.x,
                                actorLighting.y,
                                actorLighting.z),
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
            constexpr std::uint8_t spriteDrawType = 1u;
            constexpr std::uint8_t ropeSpriteDrawType = 4u;
            constexpr std::uint8_t verticalSpriteDrawType = 5u;
            constexpr std::uint8_t spriteOnceDrawType = 7u;
            const bool spriteActor = actor.drawType == spriteDrawType ||
                actor.drawType == ropeSpriteDrawType ||
                actor.drawType == verticalSpriteDrawType ||
                actor.drawType == spriteOnceDrawType;
            if (!renderedMesh && !actor.hidden && spriteActor &&
                !actor.texturePath.empty()) {
                const auto layer = textureLayers.find(actor.texturePath);
                if (layer != textureLayers.end()) {
                    const float spriteScale = actor.inventory ? 0.35f : 0.65f;
                    texturedGeometry.Add(
                        spriteDescriptor,
                        OVRFW::GeometryBuilder::kInvalidIndex,
                        OVR::Vector4f(
                            static_cast<float>(layer->second) / 255.0f,
                            actorLighting.x,
                            actorLighting.y,
                            actorLighting.z),
                        OVR::Matrix4f::Translation(position) *
                            OVR::Matrix4f::Scaling(
                                spriteScale * actor.drawScale,
                                spriteScale * actor.drawScale,
                                spriteScale * actor.drawScale));
                    renderedMesh = true;
                    ++spriteInstances;
                }
            }
            // Trigger, travel, and mover locations are gameplay metadata, not
            // visible cube-shaped objects. Keep a conservative placeholder only
            // for physical actors whose source mesh is not yet renderable.
            if (!renderedMesh && !actor.hidden &&
                (actor.pawn || actor.inventory || actor.decoration)) {
                geometry.Add(
                    OVRFW::BuildUnitCubeDescriptor(),
                    OVRFW::GeometryBuilder::kInvalidIndex,
                    color,
                    OVR::Matrix4f::Translation(position) * OVR::Matrix4f::Scaling(scale));
                ++cubePlaceholders;
                ++cubeClasses[actor.classPath];
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
                texturedGeometry.ToGeometryDescriptor(), actorTexture_, false);
        }
        ALOG(
            "DeusExQuest: instantiated %zu targetable actors from %zu live actors (%zu vertex meshes, %zu mover brushes, %zu sprites, %zu cube placeholders, %zu hidden, %zu mesh-bearing, %zu mesh formats, %zu map exits)",
            visible,
            actorSnapshots_.size(),
            meshInstances,
            brushInstances,
            spriteInstances,
            cubePlaceholders,
            hiddenActors,
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
        for (const auto& cubeClass : cubeClasses) {
            ALOG(
                "DeusExQuest: cube placeholder class %s count=%zu",
                cubeClass.first.c_str(),
                cubeClass.second);
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
                activeMapLights_ = std::move(preparedMapLights_);
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
                    LogLightingStats();
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
                ReplaceSpatialAudioEmitters(std::move(preparedSpatialAudioEmitters_));
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
            preparedSpatialAudioEmitters_.clear();
            preparedMapLights_.clear();
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
        pendingChoices_.clear();
        pendingChoiceActor_.clear();
        pendingChoiceAudioPackage_.clear();
        pendingChoiceIndex_ = 0u;
        pendingMapName_ = mapName;
        displayedInventoryCount_ = invalidRendererIndex_;
        const std::uint32_t targetAudioRate = audioSampleRate_;
        mapCacheFuture_ = std::async(std::launch::async, [mapName, restoreRuntimePath, targetAudioRate]() {
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
                preparation.spatialAudioEmitters = PrepareSpatialAudioEmitters(
                    preparation.actors, targetAudioRate);
                preparation.lights = BuildMapLights(preparation.actors);
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
        preparedSpatialAudioEmitters_ = std::move(preparation.spatialAudioEmitters);
        preparedMapLights_ = std::move(preparation.lights);
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
        if (std::strcmp(requested, "SCREENSHOT") == 0) {
            captureScreenshotRequested_ = true;
            captureScreenshotDelayFrames_ = 2u;
            ALOG("DeusExQuest: in-app eye screenshot requested");
            return;
        }
        if (std::strcmp(requested, "MENU") == 0) {
            SetInventoryMenuOpen(!inventoryMenuOpen_);
            ALOG(
                "DeusExQuest: diagnostic inventory menu %s",
                inventoryMenuOpen_ ? "opened" : "closed");
            return;
        }
        if (std::strcmp(requested, "PAGE") == 0) {
            const std::size_t pageCount = static_cast<std::size_t>(PersonaPage::Count);
            personaPage_ = static_cast<PersonaPage>(
                (static_cast<std::size_t>(personaPage_) + 1u) % pageCount);
            inventoryMenuDirty_ = true;
            ALOG(
                "DeusExQuest: diagnostic Persona page %zu",
                static_cast<std::size_t>(personaPage_));
            return;
        }
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
        if (std::strcmp(requested, "CONFIRM") == 0) {
            const bool available = !pendingChoices_.empty();
            if (available) ConfirmPendingChoice();
            ALOG("DeusExQuest: diagnostic choice confirm result=%s", available ? "selected" : "missing");
            return;
        }
        if (std::strcmp(requested, "CHOICE") == 0) {
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
            bool found{};
            for (const PortableActorSnapshot& actor : actorSnapshots_) {
                if (!actor.pawn) continue;
                const PortableDialogueResult first =
                    GetPortableRuntimeDialogue(actor.objectPath, 0u, missionNumber);
                for (std::size_t ordinal = 0u; ordinal < first.matchingLines; ++ordinal) {
                    const PortableDialogueResult candidate =
                        GetPortableRuntimeDialogue(actor.objectPath, ordinal, missionNumber);
                    const bool selectable = std::any_of(
                        candidate.choices.begin(), candidate.choices.end(),
                        [](const PortableDialogueResult::Choice& choice) {
                            return choice.available;
                        });
                    if (selectable) {
                        dialogueOffsets_[actor.objectPath] = ordinal;
                        found = ShowDialogue(actor.objectPath);
                        break;
                    }
                }
                if (found) break;
            }
            ALOG("DeusExQuest: diagnostic dialogue choice result=%s", found ? "found" : "missing");
            return;
        }
        if (std::strcmp(requested, "EFFECT") == 0 ||
            std::strcmp(requested, "TRIGGER") == 0 ||
            std::strcmp(requested, "TRANSFER") == 0) {
            const bool requireTrigger = std::strcmp(requested, "TRIGGER") == 0;
            const bool requireTransfer = std::strcmp(requested, "TRANSFER") == 0;
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
            bool found{};
            for (const PortableActorSnapshot& actor : actorSnapshots_) {
                if (!actor.pawn) continue;
                const PortableDialogueResult first =
                    GetPortableRuntimeDialogue(actor.objectPath, 0u, missionNumber);
                for (std::size_t ordinal = 0u; ordinal < first.matchingLines; ++ordinal) {
                    const PortableDialogueResult candidate =
                        GetPortableRuntimeDialogue(actor.objectPath, ordinal, missionNumber);
                    const bool matchingEffect = std::any_of(
                        candidate.effects.begin(), candidate.effects.end(),
                        [&](const PortableDialogueResult::Effect& effect) {
                            return (!requireTrigger && !requireTransfer) ||
                                (requireTrigger && effect.type ==
                                    PortableDialogueResult::Effect::Type::Trigger) ||
                                (requireTransfer && effect.type ==
                                    PortableDialogueResult::Effect::Type::TransferObject);
                        });
                    if (matchingEffect) {
                        dialogueOffsets_[actor.objectPath] = ordinal;
                        found = ShowDialogue(actor.objectPath);
                        break;
                    }
                }
                if (found) break;
            }
            ALOG(
                "DeusExQuest: diagnostic dialogue %s result=%s",
                requireTrigger ? "trigger" : (requireTransfer ? "transfer" : "effect"),
                found ? "found" : "missing");
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
        if (std::strcmp(requested, "MOVER") == 0) {
            const auto foundMover = std::find_if(
                actorSnapshots_.begin(), actorSnapshots_.end(),
                [](const PortableActorSnapshot& actor) {
                    return actor.mover && !actor.brushPath.empty();
                });
            if (foundMover != actorSnapshots_.end()) {
                const std::string moverPath = foundMover->objectPath;
                const PortableInteractionResult result =
                    InteractPortableRuntimeActor(moverPath);
                DestroyActorGeometry();
                actorSnapshots_ = GetPortableRuntimeMapActors();
                BuildActorMarkers();
                ALOG(
                    "DeusExQuest: diagnostic mover %s result=%s",
                    moverPath.c_str(),
                    result.action.c_str());
            } else {
                ALOG("DeusExQuest: diagnostic mover result=missing");
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
        AppendPersonaLog(interactionStatus_);
        interactionStatusSeconds_ = std::clamp(
            static_cast<float>(subtitle.size()) * 0.055f, 4.0f, 12.0f);
        PortableSound dialogueSound;
        try {
            dialogueSound = LoadPortableRuntimeDialogueSound(dialogue);
        } catch (const std::exception& error) {
            ALOG("DeusExQuest: dialogue audio resolution failed: %s", error.what());
        }
        const bool audioQueued = QueueDialogueAudio(dialogueSound, actorPath);
        const PortableDialogueEffectResult effects = ApplyPortableDialogueEffects(dialogue);
        if (effects.applied != 0u && !effects.status.empty()) {
            interactionStatus_ += "\n" + effects.status;
            interactionStatusSeconds_ = std::max(interactionStatusSeconds_, 5.0f);
        }
        pendingChoices_.clear();
        for (const PortableDialogueResult::Choice& choice : dialogue.choices) {
            if (choice.available) pendingChoices_.push_back(choice);
        }
        if (!pendingChoices_.empty()) {
            pendingChoiceActor_ = actorPath;
            pendingChoiceAudioPackage_ = dialogue.audioPackageName;
            pendingChoiceIndex_ = 0u;
            RefreshChoiceStatus();
        }
        ALOG(
            "DeusExQuest: dialogue %s bind=%s line=%zu/%zu sound=%d package=%s audio=%s/%zu bytes queued=%s effects=%zu credits=%d skill=%d goals=%zu notes=%zu inventory=%zu text=%s",
            dialogue.eventPath.c_str(),
            dialogue.bindName.c_str(),
            cursor,
            dialogue.matchingLines,
            dialogue.soundId,
            dialogue.audioPackageName.c_str(),
            dialogueSound.format.ToString().c_str(),
            dialogueSound.data.size(),
            audioQueued ? "true" : "false",
            effects.applied,
            effects.credits,
            effects.skillPoints,
            effects.goals,
            effects.notes,
            effects.inventoryCount,
            subtitle.c_str());
        return true;
    }

    void RefreshChoiceStatus() {
        if (pendingChoices_.empty()) return;
        const PortableDialogueResult::Choice& choice =
            pendingChoices_[pendingChoiceIndex_];
        std::string text = choice.text;
        std::replace(text.begin(), text.end(), '\n', ' ');
        std::replace(text.begin(), text.end(), '\r', ' ');
        if (text.size() > 170u) text.resize(170u);
        interactionStatus_ = "RESPONSE " + std::to_string(pendingChoiceIndex_ + 1u) +
            "/" + std::to_string(pendingChoices_.size()) + ": " + text +
            "\nRIGHT STICK SELECT   A CONFIRM";
        interactionStatusSeconds_ = 60.0f;
    }

    void ConfirmPendingChoice() {
        if (pendingChoices_.empty()) return;
        const PortableDialogueResult::Choice choice = pendingChoices_[pendingChoiceIndex_];
        if (choice.targetOrdinal != static_cast<std::size_t>(-1)) {
            dialogueOffsets_[pendingChoiceActor_] = choice.targetOrdinal;
        }
        PortableDialogueResult spokenChoice;
        spokenChoice.found = choice.soundId >= 0;
        spokenChoice.soundId = choice.soundId;
        spokenChoice.audioPackageName = pendingChoiceAudioPackage_;
        bool audioQueued{};
        try {
            audioQueued = QueueDialogueAudio(LoadPortableRuntimeDialogueSound(spokenChoice), {});
        } catch (const std::exception& error) {
            ALOG("DeusExQuest: choice audio resolution failed: %s", error.what());
        }
        std::string text = choice.text;
        std::replace(text.begin(), text.end(), '\n', ' ');
        std::replace(text.begin(), text.end(), '\r', ' ');
        if (text.size() > 210u) text.resize(210u);
        interactionStatus_ = "JC DENTON: " + text;
        AppendPersonaLog(interactionStatus_);
        interactionStatusSeconds_ = std::clamp(
            static_cast<float>(text.size()) * 0.055f, 4.0f, 12.0f);
        ALOG(
            "DeusExQuest: VR choice selected label=%s target=%zu event=%s sound=%d queued=%s text=%s",
            choice.label.c_str(),
            choice.targetOrdinal,
            choice.targetEventPath.c_str(),
            choice.soundId,
            audioQueued ? "true" : "false",
            text.c_str());
        pendingChoices_.clear();
        pendingChoiceActor_.clear();
        pendingChoiceAudioPackage_.clear();
        pendingChoiceIndex_ = 0u;
    }

    void AppendPersonaLog(std::string entry) {
        std::replace(entry.begin(), entry.end(), '\n', ' ');
        std::replace(entry.begin(), entry.end(), '\r', ' ');
        if (entry.size() > 96u) entry.resize(96u);
        if (entry.empty()) return;
        personaLogEntries_.push_back(std::move(entry));
        constexpr std::size_t retainedEntries = 12u;
        if (personaLogEntries_.size() > retainedEntries) {
            personaLogEntries_.erase(
                personaLogEntries_.begin(),
                personaLogEntries_.begin() +
                    static_cast<std::ptrdiff_t>(personaLogEntries_.size() - retainedEntries));
        }
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

    static std::string InventoryItemLabel(const std::string& path) {
        const std::size_t separator = path.find_last_of('.');
        return separator == std::string::npos ? path : path.substr(separator + 1u);
    }

    void LoadOriginalPersonaBackground() {
        if (inventoryLabel_ == nullptr) return;
        try {
            const PortablePackageTables uiPackage = LoadPortablePackageTables(
                std::string(gameRoot_) + "/System/DeusExUI.u");
            std::vector<PortableTextureImage> pieces;
            std::vector<PortableTextureImage> borders;
            pieces.reserve(6u);
            borders.reserve(6u);
            for (std::size_t index = 1u; index <= 6u; ++index) {
                const std::string name = "InventoryBackground_" + std::to_string(index);
                try {
                    pieces.push_back(DecodePortableIndexedTexture(
                        uiPackage, "UserInterface." + name));
                } catch (const std::exception&) {
                    pieces.push_back(DecodePortableIndexedTexture(uiPackage, name));
                }
                ALOG(
                    "DeusExQuest: decoded original Persona texture %s at %ux%u",
                    name.c_str(), pieces.back().width, pieces.back().height);
                const std::string borderName = "InventoryBorder_" + std::to_string(index);
                try {
                    borders.push_back(DecodePortableIndexedTexture(
                        uiPackage, "UserInterface." + borderName, true));
                } catch (const std::exception&) {
                    borders.push_back(DecodePortableIndexedTexture(
                        uiPackage, borderName, true));
                }
                ALOG(
                    "DeusExQuest: decoded original Persona texture %s at %ux%u",
                    borderName.c_str(), borders.back().width, borders.back().height);
            }
            const std::uint32_t topHeight = std::max({
                pieces[0].height, pieces[1].height, pieces[2].height});
            const std::uint32_t bottomHeight = std::max({
                pieces[3].height, pieces[4].height, pieces[5].height});
            const std::uint32_t topWidth =
                pieces[0].width + pieces[1].width + pieces[2].width;
            const std::uint32_t bottomWidth =
                pieces[3].width + pieces[4].width + pieces[5].width;
            const std::uint32_t width = std::max(topWidth, bottomWidth);
            const std::uint32_t height = topHeight + bottomHeight;
            if (width == 0u || height == 0u || width > 2048u || height > 2048u) {
                throw std::runtime_error("stitched Persona texture dimensions are invalid");
            }
            std::vector<std::uint8_t> rgba(
                static_cast<std::size_t>(width) * height * 4u, 0u);
            const auto copyPiece = [&](const PortableTextureImage& piece, std::uint32_t x,
                                       std::uint32_t y, const bool overlay) {
                if (x + piece.width > width || y + piece.height > height) {
                    throw std::runtime_error("Persona texture piece exceeds stitched canvas");
                }
                for (std::uint32_t row = 0u; row < piece.height; ++row) {
                    const std::size_t source = static_cast<std::size_t>(row) * piece.width * 4u;
                    const std::size_t destination =
                        (static_cast<std::size_t>(y + row) * width + x) * 4u;
                    if (!overlay) {
                        std::copy_n(
                            piece.rgba.data() + source,
                            static_cast<std::size_t>(piece.width) * 4u,
                            rgba.data() + destination);
                    } else {
                        for (std::uint32_t column = 0u; column < piece.width; ++column) {
                            const std::size_t sourcePixel = source + column * 4u;
                            if (piece.rgba[sourcePixel + 3u] == 0u) continue;
                            const std::size_t destinationPixel = destination + column * 4u;
                            std::copy_n(
                                piece.rgba.data() + sourcePixel, 4u,
                                rgba.data() + destinationPixel);
                        }
                    }
                }
            };
            for (std::size_t row = 0u; row < 2u; ++row) {
                std::uint32_t x{};
                const std::uint32_t y = row == 0u ? 0u : topHeight;
                for (std::size_t column = 0u; column < 3u; ++column) {
                    const PortableTextureImage& piece = pieces[row * 3u + column];
                    copyPiece(piece, x, y, false);
                    x += piece.width;
                }
            }
            for (std::size_t row = 0u; row < 2u; ++row) {
                std::uint32_t x{};
                const std::uint32_t y = row == 0u ? 0u : topHeight;
                for (std::size_t column = 0u; column < 3u; ++column) {
                    const std::size_t index = row * 3u + column;
                    copyPiece(borders[index], x, y, true);
                    x += pieces[index].width;
                }
            }
            GLuint texture{};
            glGenTextures(1, &texture);
            glBindTexture(GL_TEXTURE_2D, texture);
            glTexImage2D(
                GL_TEXTURE_2D, 0, GL_RGBA8,
                static_cast<GLsizei>(width), static_cast<GLsizei>(height), 0,
                GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            const GLenum error = glGetError();
            glBindTexture(GL_TEXTURE_2D, 0);
            if (error != GL_NO_ERROR) {
                if (texture != 0u) glDeleteTextures(1, &texture);
                throw std::runtime_error("Persona texture GPU upload failed");
            }
            personaRenderer_.Init(OVRFW::GlTexture(
                texture, GL_TEXTURE_2D, static_cast<int>(width), static_cast<int>(height)));
            inventoryLabel_->SetSurfaceVisible(0, false);
            personaUiPackage_ = uiPackage;
            personaBaseRgba_ = std::move(rgba);
            personaTextureId_ = texture;
            personaTextureWidth_ = width;
            personaTextureHeight_ = height;
            ALOG(
                "DeusExQuest: original Persona inventory background active at %ux%u",
                width, height);
        } catch (const std::exception& error) {
            ALOG(
                "DeusExQuest: original Persona background unavailable; using teal fallback: %s",
                error.what());
        }
    }

    static std::string InventoryIconName(const std::string& path) {
        const std::string label = InventoryItemLabel(path);
        if (label.find("Multitool") != std::string::npos) return "LargeIconMultitool";
        if (label.find("Lockpick") != std::string::npos) return "LargeIconLockPick";
        if (label.find("MedKit") != std::string::npos) return "LargeIconMedKit";
        if (label.find("WeaponStealthPistol") != std::string::npos) return "LargeIconStealthPistol";
        if (label.find("WeaponPistol") != std::string::npos) return "LargeIconPistol";
        if (label.find("WeaponPlasmaRifle") != std::string::npos) return "LargeIconPlasmaRifle";
        if (label.find("WeaponRifle") != std::string::npos) return "LargeIconRifle";
        if (label.find("WeaponAssaultGun") != std::string::npos) return "LargeIconAssaultGun";
        if (label.find("WeaponAssaultShotgun") != std::string::npos) return "LargeIconAssaultShotgun";
        if (label.find("WeaponSawedOffShotgun") != std::string::npos) return "LargeIconShotgun";
        if (label.find("WeaponMiniCrossbow") != std::string::npos) return "LargeIconCrossbow";
        if (label.find("WeaponFlamethrower") != std::string::npos) return "LargeIconFlamethrower";
        if (label.find("WeaponGEPGun") != std::string::npos) return "LargeIconGEPGun";
        if (label.find("WeaponPepperGun") != std::string::npos) return "LargeIconPepperGun";
        if (label.find("WeaponLAW") != std::string::npos) return "LargeIconLAW";
        if (label.find("WeaponNanoSword") != std::string::npos) return "LargeIconDragonsTooth";
        if (label.find("WeaponSword") != std::string::npos) return "LargeIconSword";
        if (label.find("WeaponShuriken") != std::string::npos) return "LargeIconShuriken";
        if (label.find("WeaponEMPGrenade") != std::string::npos) return "LargeIconEMPGrenade";
        if (label.find("WeaponGasGrenade") != std::string::npos) return "LargeIconGasGrenade";
        if (label.find("WeaponLAM") != std::string::npos) return "LargeIconLAM";
        if (label.find("WeaponCombatKnife") != std::string::npos) return "LargeIconCombatKnife";
        if (label.find("WeaponBaton") != std::string::npos) return "LargeIconBaton";
        if (label.find("WeaponCrowbar") != std::string::npos) return "LargeIconCrowbar";
        if (label.find("WeaponProd") != std::string::npos) return "LargeIconProd";
        if (label.find("AmmoDartPoison") != std::string::npos) return "LargeIconAmmoDartsPoison";
        if (label.find("AmmoDartFlare") != std::string::npos) return "LargeIconAmmoDartsFlare";
        if (label.find("AmmoDart") != std::string::npos) return "LargeIconAmmoDartsNormal";
        if (label.find("Ammo20mm") != std::string::npos) return "LargeIconAmmo20mm";
        if (label.find("Ammo10mm") != std::string::npos) return "LargeIconAmmo10mm";
        if (label.find("Ammo3006") != std::string::npos) return "LargeIconAmmo30rd";
        if (label.find("AmmoNapalm") != std::string::npos) return "LargeIconAmmoNapalm";
        if (label.find("AmmoPepper") != std::string::npos) return "LargeIconAmmoPepperSpray";
        if (label.find("AmmoPlasma") != std::string::npos) return "LargeIconAmmoPlasmaRifle";
        if (label.find("AmmoProd") != std::string::npos) return "LargeIconAmmoProd";
        if (label.find("AmmoRocketWP") != std::string::npos) return "LargeIconAmmoWPRockets";
        if (label.find("AmmoRocket") != std::string::npos) return "LargeIconAmmoRockets";
        if (label.find("AmmoSabot") != std::string::npos) return "LargeIconAmmoSabot";
        if (label.find("AmmoShell") != std::string::npos) return "LargeIconAmmoShells";
        if (label.find("Ammo") != std::string::npos) return "LargeIconAmmo7mm";
        if (label.find("BioelectricCell") != std::string::npos) return "LargeIconBioCell";
        if (label.find("AdaptiveArmor") != std::string::npos) return "LargeIconArmorAdaptive";
        if (label.find("BallisticArmor") != std::string::npos) return "LargeIconArmorBallistic";
        if (label.find("HazMatSuit") != std::string::npos) return "LargeIconHazMatSuit";
        if (label.find("Rebreather") != std::string::npos) return "LargeIconRebreather";
        if (label.find("TechGoggles") != std::string::npos) return "LargeIconTechGoggles";
        if (label.find("Binoculars") != std::string::npos) return "LargeIconBinoculars";
        if (label.find("FireExtinguisher") != std::string::npos) return "LargeIconFireExtinguisher";
        if (label.find("Flare") != std::string::npos) return "LargeIconFlare";
        if (label.find("AugmentationCannister") != std::string::npos) return "LargeIconAugmentationCannister";
        if (label.find("AugmentationUpgrade") != std::string::npos) return "LargeIconAugmentationUpgrade";
        if (label.find("WeaponModAccuracy") != std::string::npos) return "LargeIconWeaponModAccuracy";
        if (label.find("WeaponModClip") != std::string::npos) return "LargeIconWeaponModClip";
        if (label.find("WeaponModLaser") != std::string::npos) return "LargeIconWeaponModLaser";
        if (label.find("WeaponModRange") != std::string::npos) return "LargeIconWeaponModRange";
        if (label.find("WeaponModRecoil") != std::string::npos) return "LargeIconWeaponModRecoil";
        if (label.find("WeaponModReload") != std::string::npos) return "LargeIconWeaponModReload";
        if (label.find("WeaponModScope") != std::string::npos) return "LargeIconWeaponModScope";
        if (label.find("WeaponModSilencer") != std::string::npos) return "LargeIconWeaponModSilencer";
        if (label.find("Ambrosia") != std::string::npos) return "LargeIconAmbrosiaVial";
        if (label.find("VialCrack") != std::string::npos ||
            label.find("CrackVial") != std::string::npos) return "LargeIconCrackVial";
        if (label.find("SoyFood") != std::string::npos) return "LargeIconSoyFood";
        if (label.find("Candybar") != std::string::npos) return "LargeIconCandyBar";
        if (label.find("SodaCan") != std::string::npos) return "LargeIconSodaCan";
        if (label.find("Beer") != std::string::npos) return "LargeIconBeerBottle";
        if (label.find("Cigarette") != std::string::npos) return "LargeIconCigarettes";
        if (label.find("Liquor") != std::string::npos) return "LargeIconLiquorBottle";
        if (label.find("Wine") != std::string::npos) return "LargeIconWineBottle";
        if (label.find("NanoKey") != std::string::npos) return "LargeIconNanoKeyRing";
        return {};
    }

    const PortableTextureImage* GetPersonaIcon(const std::string& path) {
        const std::string name = InventoryIconName(path);
        if (name.empty() || personaUiPackage_.exports.empty()) return nullptr;
        const auto cached = personaIconCache_.find(name);
        if (cached != personaIconCache_.end()) return &cached->second;
        try {
            PortableTextureImage image;
            try {
                image = DecodePortableIndexedTexture(
                    personaUiPackage_, "Icons." + name, true);
            } catch (const std::exception&) {
                image = DecodePortableIndexedTexture(personaUiPackage_, name, true);
            }
            ALOG(
                "DeusExQuest: decoded original inventory icon %s at %ux%u",
                name.c_str(), image.width, image.height);
            return &personaIconCache_.emplace(name, std::move(image)).first->second;
        } catch (const std::exception& error) {
            ALOG("DeusExQuest: inventory icon %s unavailable: %s", name.c_str(), error.what());
            personaIconCache_.emplace(name, PortableTextureImage{});
            return nullptr;
        }
    }

    void RefreshPersonaInventoryArtwork(const std::vector<std::string>& inventory) {
        if (personaTextureId_ == 0u || personaBaseRgba_.empty()) return;
        std::vector<std::uint8_t> rgba = personaBaseRgba_;
        const auto setPixel = [&](std::uint32_t x, std::uint32_t y,
                                  std::uint8_t red, std::uint8_t green,
                                  std::uint8_t blue, std::uint8_t alpha = 255u) {
            if (x >= personaTextureWidth_ || y >= personaTextureHeight_) return;
            const std::size_t pixel =
                (static_cast<std::size_t>(y) * personaTextureWidth_ + x) * 4u;
            rgba[pixel] = red;
            rgba[pixel + 1u] = green;
            rgba[pixel + 2u] = blue;
            rgba[pixel + 3u] = alpha;
        };
        constexpr std::uint32_t gridX = 50u;
        constexpr std::uint32_t gridY = 75u;
        constexpr std::uint32_t cell = 54u;
        constexpr std::uint32_t gap = 5u;
        constexpr std::size_t visibleItems = 12u;
        if (personaPage_ == PersonaPage::Inventory) {
            const std::size_t first = inventory.size() <= visibleItems
                ? 0u
                : std::min(
                    selectedInventoryIndex_ > visibleItems / 2u
                        ? selectedInventoryIndex_ - visibleItems / 2u
                        : 0u,
                    inventory.size() - visibleItems);
            for (std::size_t slot = 0u; slot < visibleItems; ++slot) {
            const std::uint32_t x = gridX + static_cast<std::uint32_t>(slot % 3u) * (cell + gap);
            const std::uint32_t y = gridY + static_cast<std::uint32_t>(slot / 3u) * (cell + gap);
            const std::size_t inventoryIndex = first + slot;
            const bool selected = inventoryIndex < inventory.size() &&
                inventoryIndex == selectedInventoryIndex_;
            for (std::uint32_t row = 0u; row < cell; ++row) {
                for (std::uint32_t column = 0u; column < cell; ++column) {
                    const bool edge = row < 2u || column < 2u ||
                        row + 2u >= cell || column + 2u >= cell;
                    if (edge) {
                        setPixel(x + column, y + row,
                            selected ? 235u : 116u,
                            selected ? 190u : 102u,
                            selected ? 55u : 54u);
                    } else {
                        setPixel(x + column, y + row, 4u, 18u, 18u);
                    }
                }
            }
            if (inventoryIndex >= inventory.size()) continue;
            const PortableTextureImage* icon = GetPersonaIcon(inventory[inventoryIndex]);
            if (icon == nullptr || icon->width == 0u || icon->height == 0u) continue;
            constexpr std::uint32_t iconLimit = 46u;
            std::uint32_t iconWidth = iconLimit;
            std::uint32_t iconHeight = iconLimit;
            if (icon->width > icon->height) {
                iconHeight = std::max(1u, iconLimit * icon->height / icon->width);
            } else {
                iconWidth = std::max(1u, iconLimit * icon->width / icon->height);
            }
            const std::uint32_t iconX = x + (cell - iconWidth) / 2u;
            const std::uint32_t iconY = y + (cell - iconHeight) / 2u;
            for (std::uint32_t row = 0u; row < iconHeight; ++row) {
                const std::uint32_t sourceY = row * icon->height / iconHeight;
                for (std::uint32_t column = 0u; column < iconWidth; ++column) {
                    const std::uint32_t sourceX = column * icon->width / iconWidth;
                    const std::size_t source =
                        (static_cast<std::size_t>(sourceY) * icon->width + sourceX) * 4u;
                    if (icon->rgba[source + 3u] == 0u) continue;
                    setPixel(
                        iconX + column, iconY + row,
                        icon->rgba[source], icon->rgba[source + 1u],
                        icon->rgba[source + 2u], icon->rgba[source + 3u]);
                }
            }
            }
        }
        glBindTexture(GL_TEXTURE_2D, personaTextureId_);
        glTexSubImage2D(
            GL_TEXTURE_2D, 0, 0, 0,
            static_cast<GLsizei>(personaTextureWidth_),
            static_cast<GLsizei>(personaTextureHeight_),
            GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
        const GLenum updateError = glGetError();
        glBindTexture(GL_TEXTURE_2D, 0);
        const std::size_t diagnosticPixel =
            (static_cast<std::size_t>(gridY) * personaTextureWidth_ + gridX) * 4u;
        ALOG(
            "DeusExQuest: Persona grid upload GL=0x%x pixel=%u,%u,%u iconCache=%zu",
            updateError,
            static_cast<unsigned>(rgba[diagnosticPixel]),
            static_cast<unsigned>(rgba[diagnosticPixel + 1u]),
            static_cast<unsigned>(rgba[diagnosticPixel + 2u]),
            personaIconCache_.size());
    }

    static std::string InventoryItemType(const std::string& path) {
        const std::string label = InventoryItemLabel(path);
        if (label.find("Weapon") != std::string::npos) return "WEAPON";
        if (label.find("Ammo") != std::string::npos) return "AMMUNITION";
        if (label.find("MedKit") != std::string::npos ||
            label.find("Food") != std::string::npos ||
            label.find("Candybar") != std::string::npos ||
            label.find("Soda") != std::string::npos ||
            label.find("Liquor") != std::string::npos ||
            label.find("Wine") != std::string::npos) return "CONSUMABLE";
        if (label.find("Multitool") != std::string::npos ||
            label.find("Lockpick") != std::string::npos) return "TOOL";
        return "INVENTORY ITEM";
    }

    static std::string SelectedInventoryLabel(const std::vector<std::string>& inventory) {
        if (inventory.empty()) return "NONE";
        const std::string& path = inventory[selectedInventoryIndex_ % inventory.size()];
        return InventoryItemLabel(path);
    }

    void SetInventoryMenuOpen(const bool open) {
        inventoryMenuOpen_ = open;
        inventoryMenuDirty_ = true;
        choiceCycleLatch_ = true;
        if (hudLabel_ != nullptr) hudLabel_->SetVisible(!open);
        if (inventoryLabel_ != nullptr) inventoryLabel_->SetVisible(open);
        ALOG("DeusExQuest: VR inventory menu %s", open ? "opened" : "closed");
    }

    void UpdateInventoryMenu(
        const OVRFW::ovrApplFrameIn& frame,
        const bool mapLoading) {
        if (mapLoading && inventoryMenuOpen_) SetInventoryMenuOpen(false);
        if (!inventoryMenuOpen_ || inventoryLabel_ == nullptr) return;

        OVR::Posef menuPose = frame.HeadPose;
        menuPose.Translation += frame.HeadPose.Rotation.Rotate(
            OVR::Vector3f(0.0f, -0.015f, -1.05f));
        inventoryLabel_->SetLocalPose(menuPose);
        OVR::Posef artworkPose = frame.HeadPose;
        artworkPose.Translation += frame.HeadPose.Rotation.Rotate(
            OVR::Vector3f(0.0f, -0.015f, -1.055f));
        personaRenderer_.SetPose(artworkPose);
        const std::vector<std::string> inventory = GetPortableRuntimeInventoryItems();
        if (!inventory.empty()) selectedInventoryIndex_ %= inventory.size();
        const float health = GetPortableRuntimePlayerHealth();
        if (!inventoryMenuDirty_ && inventory.size() == inventoryMenuDisplayedCount_ &&
            std::fabs(health - inventoryMenuDisplayedHealth_) <= 0.01f &&
            selectedInventoryIndex_ == inventoryMenuDisplayedSelection_) {
            return;
        }

        RefreshPersonaInventoryArtwork(inventory);
        const PortablePlayerProgress progress = GetPortableRuntimePlayerProgress();
        std::string tabs;
        switch (personaPage_) {
            case PersonaPage::Inventory:
                tabs = "[ INVENTORY ]  HEALTH  AUGS  SKILLS  GOALS/NOTES  IMAGES  LOGS";
                break;
            case PersonaPage::Health:
                tabs = "INVENTORY  [ HEALTH ]  AUGS  SKILLS  GOALS/NOTES  IMAGES  LOGS";
                break;
            case PersonaPage::GoalsNotes:
                tabs = "INVENTORY  HEALTH  AUGS  SKILLS  [ GOALS/NOTES ]  IMAGES  LOGS";
                break;
            case PersonaPage::Logs:
                tabs = "INVENTORY  HEALTH  AUGS  SKILLS  GOALS/NOTES  IMAGES  [ LOGS ]";
                break;
            case PersonaPage::Count:
                break;
        }
        std::string text = "D E U S  E X   // P E R S O N A        HEALTH " +
            std::to_string(static_cast<int>(health)) + "\n";
        text += currentMapName_ + "\n";
        text += "==============================================================\n";
        text += tabs + "\n";
        text += "--------------------------------------------------------------\n";
        if (personaPage_ == PersonaPage::Inventory) {
            std::string item = "NO ITEM SELECTED";
            std::string type = "INVENTORY EMPTY";
            if (!inventory.empty()) {
                item = InventoryItemLabel(inventory[selectedInventoryIndex_]);
                if (item.size() > 20u) item.resize(20u);
                type = InventoryItemType(inventory[selectedInventoryIndex_]);
            }
            const std::vector<std::string> details = {
                "ITEM  " + item,
                "TYPE  " + type,
                "STATE READY",
                "A  EQUIP / USE"};
            text += "// INVENTORY GRID                  // ITEM DATA\n\n";
            for (const std::string& detail : details) {
                text += std::string(34u, ' ') + detail + "\n";
            }
            text += "\nSLOT " +
                std::to_string(inventory.empty() ? 0u : selectedInventoryIndex_ + 1u) +
                " / " + std::to_string(inventory.size()) + "\n";
        } else if (personaPage_ == PersonaPage::Health) {
            text += "// HEALTH STATUS                   // PLAYER DATA\n\n";
            text += "CURRENT HEALTH  " + std::to_string(static_cast<int>(health)) + " / 100\n";
            text += "CONDITION       " + std::string(health > 50.0f ? "NOMINAL" : "INJURED") + "\n";
            text += "CREDITS         " + std::to_string(progress.credits) + "\n";
            text += "SKILL POINTS    " + std::to_string(progress.skillPoints) + "\n";
            text += "INVENTORY ITEMS " + std::to_string(inventory.size()) + "\n\n";
        } else if (personaPage_ == PersonaPage::GoalsNotes) {
            const auto cleanEntry = [](std::string value) {
                std::replace(value.begin(), value.end(), '\n', ' ');
                std::replace(value.begin(), value.end(), '\r', ' ');
                if (value.empty()) value = "UNNAMED ENTRY";
                if (value.size() > 48u) value.resize(48u);
                return value;
            };
            text += "// GOALS / NOTES\n\n";
            text += "ACTIVE GOALS  " + std::to_string(progress.goals.size()) +
                "     NOTES  " + std::to_string(progress.notes.size()) + "\n";
            std::size_t lines{};
            for (std::size_t index = 0u; index < progress.goals.size() && lines < 3u;
                 ++index, ++lines) {
                text += "G" + std::to_string(index + 1u) + "  " +
                    cleanEntry(progress.goals[index]) + "\n";
            }
            for (std::size_t index = 0u; index < progress.notes.size() && lines < 5u;
                 ++index, ++lines) {
                text += "N" + std::to_string(index + 1u) + "  " +
                    cleanEntry(progress.notes[index]) + "\n";
            }
            if (lines == 0u) text += "NO GOALS OR NOTES RECORDED\n";
            while (lines++ < 5u) text += "\n";
        } else {
            text += "// CONVERSATION LOG\n\n";
            if (personaLogEntries_.empty()) {
                text += "NO CONVERSATIONS RECORDED\n\n\n\n\n";
            } else {
                const std::size_t first = personaLogEntries_.size() > 5u
                    ? personaLogEntries_.size() - 5u
                    : 0u;
                std::size_t lines{};
                for (std::size_t index = first; index < personaLogEntries_.size(); ++index) {
                    std::string entry = personaLogEntries_[index];
                    if (entry.size() > 52u) entry.resize(52u);
                    text += entry + "\n";
                    ++lines;
                }
                while (lines++ < 5u) text += "\n";
            }
        }
        text += "--------------------------------------------------------------\n";
        text += personaPage_ == PersonaPage::Inventory
            ? "R-STICK L/R PAGE  U/D SELECT  A USE  B / MENU CLOSE"
            : "R-STICK L/R PAGE   Y SAVE   X LOAD   B / MENU CLOSE";
        inventoryLabel_->SetText("%s", text.c_str());
        inventoryMenuDisplayedCount_ = inventory.size();
        inventoryMenuDisplayedHealth_ = health;
        inventoryMenuDisplayedSelection_ = selectedInventoryIndex_;
        inventoryMenuDirty_ = false;
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
        const std::uint32_t header[2] = {0x4d515844u, 4u};
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
        const std::uint32_t logCount =
            static_cast<std::uint32_t>(personaLogEntries_.size());
        if (metaSaved) {
            metaSaved = std::fwrite(&logCount, sizeof(logCount), 1, file) == 1;
            for (const std::string& entry : personaLogEntries_) {
                const std::uint32_t bytes = static_cast<std::uint32_t>(entry.size());
                metaSaved = metaSaved && bytes > 0u && bytes <= 256u &&
                    std::fwrite(&bytes, sizeof(bytes), 1, file) == 1 &&
                    std::fwrite(entry.data(), 1, bytes, file) == bytes;
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
        std::vector<std::string> savedPersonaLogs;
        bool metaLoaded = file != nullptr &&
            std::fread(header, sizeof(header), 1, file) == 1 &&
            std::fread(pose, sizeof(pose), 1, file) == 1 &&
            header[0] == 0x4d515844u &&
            header[1] >= 1u && header[1] <= 4u &&
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
        if (metaLoaded && header[1] >= 3u) {
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
        if (metaLoaded && header[1] >= 4u) {
            std::uint32_t logCount{};
            metaLoaded = std::fread(&logCount, sizeof(logCount), 1, file) == 1 &&
                logCount <= 12u;
            for (std::uint32_t index = 0u; metaLoaded && index < logCount; ++index) {
                std::uint32_t bytes{};
                metaLoaded = std::fread(&bytes, sizeof(bytes), 1, file) == 1 &&
                    bytes > 0u && bytes <= 256u;
                std::string entry(bytes, '\0');
                metaLoaded = metaLoaded &&
                    std::fread(entry.data(), 1, bytes, file) == bytes;
                if (metaLoaded) savedPersonaLogs.push_back(std::move(entry));
            }
        }
        if (file != nullptr) std::fclose(file);
        if (!metaLoaded) {
            ALOG("DeusExQuest: VR quick-load failed");
            return;
        }
        dialogueOffsets_ = std::move(savedDialogueOffsets);
        personaLogEntries_ = std::move(savedPersonaLogs);
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

    void CaptureResolvedEyeFramebuffer(const ovrFramebuffer& eyeFramebuffer) {
        GLint previousReadFramebuffer{};
        glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &previousReadFramebuffer);
        const GLsizei width = eyeFramebuffer.Width;
        const GLsizei height = eyeFramebuffer.Height;
        if (width <= 0 || height <= 0 || eyeFramebuffer.ColorSwapChainImage == nullptr ||
            eyeFramebuffer.TextureSwapChainIndex >= eyeFramebuffer.TextureSwapChainLength) {
            ALOG("DeusExQuest: screenshot failed: invalid resolved eye texture");
            return;
        }
        const GLuint eyeTexture = eyeFramebuffer.ColorSwapChainImage[
            eyeFramebuffer.TextureSwapChainIndex].image;
        glFinish();
        GLuint captureFramebuffer{};
        glGenFramebuffers(1, &captureFramebuffer);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, captureFramebuffer);
        glFramebufferTexture2D(
            GL_READ_FRAMEBUFFER,
            GL_COLOR_ATTACHMENT0,
            GL_TEXTURE_2D,
            eyeTexture,
            0);
        bool ok = glCheckFramebufferStatus(GL_READ_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
        std::vector<std::uint8_t> pixels;
        if (ok) {
            pixels.resize(static_cast<std::size_t>(width) * height * 4u);
            glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
            glFinish();
            ok = glGetError() == GL_NO_ERROR;
        }
        glBindFramebuffer(
            GL_READ_FRAMEBUFFER, static_cast<GLuint>(previousReadFramebuffer));
        glDeleteFramebuffers(1, &captureFramebuffer);
        if (!ok) {
            ALOG("DeusExQuest: screenshot GPU readback failed");
            return;
        }

        for (std::size_t offset = 0u; offset + 3u < pixels.size(); offset += 4u) {
            std::swap(pixels[offset], pixels[offset + 2u]);
            pixels[offset + 3u] = 255u;
        }
        constexpr const char* directory =
            "/sdcard/Android/data/dev.deusex.questvr.smoketest/files";
        mkdir(directory, 0700);
        const std::string path = std::string(directory) + "/quest-screenshot.bmp";
        std::uint8_t header[54]{};
        const auto write16 = [&](std::size_t offset, std::uint16_t value) {
            header[offset] = static_cast<std::uint8_t>(value);
            header[offset + 1u] = static_cast<std::uint8_t>(value >> 8u);
        };
        const auto write32 = [&](std::size_t offset, std::uint32_t value) {
            for (std::size_t byte = 0u; byte < 4u; ++byte) {
                header[offset + byte] = static_cast<std::uint8_t>(value >> (byte * 8u));
            }
        };
        header[0] = 'B';
        header[1] = 'M';
        write32(2u, static_cast<std::uint32_t>(sizeof(header) + pixels.size()));
        write32(10u, sizeof(header));
        write32(14u, 40u);
        write32(18u, static_cast<std::uint32_t>(width));
        write32(22u, static_cast<std::uint32_t>(height));
        write16(26u, 1u);
        write16(28u, 32u);
        write32(34u, static_cast<std::uint32_t>(pixels.size()));
        std::FILE* file = std::fopen(path.c_str(), "wb");
        ok = file != nullptr &&
            std::fwrite(header, 1u, sizeof(header), file) == sizeof(header) &&
            std::fwrite(pixels.data(), 1u, pixels.size(), file) == pixels.size();
        if (file != nullptr) std::fclose(file);
        if (ok) {
            interactionStatus_ = "SCREENSHOT SAVED";
            interactionStatusSeconds_ = 3.0f;
            ALOG(
                "DeusExQuest: in-app eye screenshot saved: %s (%dx%d, %zu bytes)",
                path.c_str(),
                width,
                height,
                pixels.size() + sizeof(header));
        } else {
            interactionStatus_ = "SCREENSHOT FAILED";
            interactionStatusSeconds_ = 3.0f;
            ALOG("DeusExQuest: screenshot file write failed: %s", path.c_str());
        }
    }

    struct DecodedMonoAudio {
        std::vector<std::int16_t> samples;
        std::uint32_t sourceRate{};
        std::uint32_t channels{};
    };

    static DecodedMonoAudio DecodeSpatialSound(
        PortableSound sound,
        std::uint32_t targetRate,
        std::uint8_t pitch) {
        DecodedMonoAudio result;
        if (sound.data.empty() || targetRate == 0u) return result;
        std::vector<std::int16_t> source;
        const std::string format = sound.format.ToString();
        if (format == "wav") {
            if (sound.data.size() < 12u || std::memcmp(sound.data.data(), "RIFF", 4) != 0 ||
                std::memcmp(sound.data.data() + 8u, "WAVE", 4) != 0) return result;
            std::uint16_t encoding{}, channels{}, bits{};
            std::uint32_t sampleRate{};
            const std::uint8_t* pcm{};
            std::size_t pcmBytes{};
            for (std::size_t offset = 12u; offset + 8u <= sound.data.size();) {
                const std::uint32_t chunkSize = ReadLe32(sound.data.data() + offset + 4u);
                const std::size_t dataOffset = offset + 8u;
                if (dataOffset + chunkSize > sound.data.size()) return result;
                if (std::memcmp(sound.data.data() + offset, "fmt ", 4) == 0 &&
                    chunkSize >= 16u) {
                    encoding = ReadLe16(sound.data.data() + dataOffset);
                    channels = ReadLe16(sound.data.data() + dataOffset + 2u);
                    sampleRate = ReadLe32(sound.data.data() + dataOffset + 4u);
                    bits = ReadLe16(sound.data.data() + dataOffset + 14u);
                } else if (std::memcmp(sound.data.data() + offset, "data", 4) == 0) {
                    pcm = sound.data.data() + dataOffset;
                    pcmBytes = chunkSize;
                }
                offset = dataOffset + chunkSize + (chunkSize & 1u);
            }
            if (encoding != 1u || (channels != 1u && channels != 2u) ||
                (bits != 8u && bits != 16u) || sampleRate == 0u || pcm == nullptr) return result;
            const std::size_t bytesPerSample = bits / 8u;
            const std::size_t count = pcmBytes / bytesPerSample;
            source.resize(count);
            for (std::size_t index = 0u; index < count; ++index) {
                source[index] = bits == 8u
                    ? static_cast<std::int16_t>(
                          (static_cast<std::int32_t>(pcm[index]) - 128) << 8)
                    : static_cast<std::int16_t>(ReadLe16(pcm + index * 2u));
            }
            result.sourceRate = sampleRate;
            result.channels = channels;
        } else if (format == "mp2" || format == "mp3") {
            mp3dec_ex_t decoder{};
            if (mp3dec_ex_open_buf(
                    &decoder, sound.data.data(), sound.data.size(), MP3D_SEEK_TO_SAMPLE) != 0) {
                return result;
            }
            result.sourceRate = static_cast<std::uint32_t>(decoder.info.hz);
            result.channels = static_cast<std::uint32_t>(decoder.info.channels);
            source.resize(static_cast<std::size_t>(decoder.samples));
            source.resize(mp3dec_ex_read(&decoder, source.data(), source.size()));
            mp3dec_ex_close(&decoder);
        }
        if (source.empty() || result.sourceRate == 0u ||
            (result.channels != 1u && result.channels != 2u)) return {};
        const std::size_t sourceFrames = source.size() / result.channels;
        const double pitchScale = std::clamp(
            static_cast<double>(pitch == 0u ? 64u : pitch) / 64.0, 0.25, 4.0);
        const std::size_t outputFrames = static_cast<std::size_t>(
            sourceFrames * static_cast<double>(targetRate) /
            (static_cast<double>(result.sourceRate) * pitchScale));
        result.samples.resize(outputFrames);
        for (std::size_t frame = 0u; frame < outputFrames; ++frame) {
            const double sourcePosition = frame * static_cast<double>(result.sourceRate) *
                pitchScale / targetRate;
            const std::size_t first = std::min(
                static_cast<std::size_t>(sourcePosition), sourceFrames - 1u);
            const std::size_t second = std::min(first + 1u, sourceFrames - 1u);
            const float fraction = static_cast<float>(sourcePosition - first);
            const auto monoAt = [&](std::size_t sourceFrame) {
                if (result.channels == 1u) return static_cast<float>(source[sourceFrame]);
                return 0.5f * (source[sourceFrame * 2u] + source[sourceFrame * 2u + 1u]);
            };
            result.samples[frame] = static_cast<std::int16_t>(
                monoAt(first) + (monoAt(second) - monoAt(first)) * fraction);
        }
        return result;
    }

    static std::vector<SpatialAudioEmitter> PrepareSpatialAudioEmitters(
        const std::vector<PortableActorSnapshot>& actors,
        std::uint32_t targetRate) {
        std::vector<SpatialAudioEmitter> emitters;
        if (targetRate == 0u) return emitters;
        float originX = -1149.244f;
        float originY = 825.844f;
        float originZ = -65.103f;
        for (const PortableActorSnapshot& actor : actors) {
            const std::size_t separator = actor.classPath.find_last_of('.');
            const std::string leafClass = separator == std::string::npos
                ? actor.classPath
                : actor.classPath.substr(separator + 1u);
            if (actor.hasLocation && leafClass == "PlayerStart") {
                originX = actor.x;
                originY = actor.y;
                originZ = actor.z;
                break;
            }
        }
        std::unordered_map<std::string, std::shared_ptr<const std::vector<std::int16_t>>> decoded;
        std::set<std::string> rejected;
        constexpr float unitsToMeters = 1.0f / 52.5f;
        for (const PortableActorSnapshot& actor : actors) {
            if (!actor.hasLocation || actor.ambientSoundPath.empty()) continue;
            const std::string cacheKey = actor.ambientSoundPath + "#" +
                std::to_string(actor.soundPitch);
            if (rejected.find(cacheKey) != rejected.end()) continue;
            auto found = decoded.find(cacheKey);
            if (found == decoded.end()) {
                try {
                    DecodedMonoAudio sound = DecodeSpatialSound(
                        LoadPortableRuntimeSound(actor.ambientSoundPath),
                        targetRate,
                        actor.soundPitch);
                    if (sound.samples.empty()) continue;
                    found = decoded.emplace(
                        cacheKey,
                        std::make_shared<const std::vector<std::int16_t>>(
                            std::move(sound.samples))).first;
                } catch (const std::exception& error) {
                    ALOG(
                        "DeusExQuest: ambient emitter decode failed for %s: %s",
                        actor.ambientSoundPath.c_str(),
                        error.what());
                    rejected.insert(cacheKey);
                    continue;
                }
            }
            SpatialAudioEmitter emitter;
            emitter.localPosition = {
                (actor.y - originY) * unitsToMeters,
                (actor.z - originZ) * unitsToMeters + 1.0f,
                -(actor.x - originX) * unitsToMeters};
            emitter.soundPath = actor.ambientSoundPath;
            emitter.monoSamples = found->second;
            emitter.radiusMeters = std::max(
                2.0f, static_cast<float>(actor.soundRadius) * 25.0f * unitsToMeters);
            emitter.volume = static_cast<float>(actor.soundVolume) / 255.0f;
            emitters.push_back(std::move(emitter));
        }
        ALOG(
            "DeusExQuest: prepared %zu spatial ambient emitters from %zu decoded clips",
            emitters.size(),
            decoded.size());
        return emitters;
    }

    void ReplaceSpatialAudioEmitters(std::vector<SpatialAudioEmitter> emitters) {
        std::lock_guard<std::mutex> lock(audioMutex_);
        spatialAudioEmitters_ = std::move(emitters);
        ambientSamples_.clear();
        ambientCursor_ = 0u;
    }

    void UpdateSpatialAudioGains(
        const OVR::Vector3f& listener,
        float listenerRightX,
        float listenerRightZ) {
        std::lock_guard<std::mutex> lock(audioMutex_);
        for (SpatialAudioEmitter& emitter : spatialAudioEmitters_) {
            const float dx = emitter.localPosition.x - listener.x;
            const float dy = emitter.localPosition.y - listener.y;
            const float dz = emitter.localPosition.z - listener.z;
            const float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
            const float attenuation = std::clamp(
                1.0f - distance / emitter.radiusMeters, 0.0f, 1.0f);
            const float horizontal = std::sqrt(dx * dx + dz * dz);
            const float pan = horizontal > 0.001f
                ? std::clamp(
                      (dx * listenerRightX + dz * listenerRightZ) / horizontal,
                      -1.0f,
                      1.0f)
                : 0.0f;
            const float gain = 0.35f * emitter.volume * attenuation;
            emitter.leftGain = gain * std::sqrt(0.5f * (1.0f - pan));
            emitter.rightGain = gain * std::sqrt(0.5f * (1.0f + pan));
        }
        if (dialogueSpatialized_) {
            const float dx = dialogueLocalPosition_.x - listener.x;
            const float dz = dialogueLocalPosition_.z - listener.z;
            const float horizontal = std::sqrt(dx * dx + dz * dz);
            const float pan = horizontal > 0.001f
                ? std::clamp(
                      (dx * listenerRightX + dz * listenerRightZ) / horizontal,
                      -1.0f,
                      1.0f)
                : 0.0f;
            const float distance = std::sqrt(
                dx * dx +
                (dialogueLocalPosition_.y - listener.y) *
                    (dialogueLocalPosition_.y - listener.y) +
                dz * dz);
            const float gain = 1.0f / (1.0f + 0.08f * distance * distance);
            dialogueLeftGain_ = gain * std::sqrt(1.0f - pan);
            dialogueRightGain_ = gain * std::sqrt(1.0f + pan);
        } else {
            dialogueLeftGain_ = 1.0f;
            dialogueRightGain_ = 1.0f;
        }
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
        if (!app->spatialAudioEmitters_.empty()) {
            for (std::int32_t frame = 0; frame < numFrames; ++frame) {
                float left{};
                float right{};
                for (SpatialAudioEmitter& emitter : app->spatialAudioEmitters_) {
                    if (!emitter.monoSamples || emitter.monoSamples->empty()) continue;
                    const std::int16_t sample = (*emitter.monoSamples)[emitter.cursor];
                    emitter.cursor = (emitter.cursor + 1u) % emitter.monoSamples->size();
                    left += sample * emitter.leftGain;
                    right += sample * emitter.rightGain;
                }
                output[static_cast<std::size_t>(frame) * 2u] =
                    static_cast<std::int16_t>(std::clamp(left, -32768.0f, 32767.0f));
                output[static_cast<std::size_t>(frame) * 2u + 1u] =
                    static_cast<std::int16_t>(std::clamp(right, -32768.0f, 32767.0f));
            }
        } else if (app->ambientSamples_.empty()) {
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
            const float dialogueGain = (index & 1u) == 0u
                ? app->dialogueLeftGain_
                : app->dialogueRightGain_;
            const std::int32_t mixed = static_cast<std::int32_t>(output[index]) +
                static_cast<std::int32_t>(
                    app->dialogueSamples_[app->dialogueCursor_++] * dialogueGain);
            output[index] = static_cast<std::int16_t>(
                std::clamp(mixed, -32768, 32767));
        }
        return AAUDIO_CALLBACK_RESULT_CONTINUE;
    }

    struct DecodedDialogueAudio {
        std::vector<std::int16_t> stereo;
        std::uint32_t sourceRate{};
        std::uint32_t channels{};
        std::uint32_t targetRate{};
    };

    static DecodedDialogueAudio DecodeDialogueAudio(
        PortableSound sound,
        std::uint32_t targetRate) {
        DecodedDialogueAudio result;
        result.targetRate = targetRate;
        if (sound.format.ToString() != "mp3" || sound.data.empty() || targetRate == 0u) {
            return result;
        }
        mp3dec_ex_t decoder{};
        if (mp3dec_ex_open_buf(
                &decoder, sound.data.data(), sound.data.size(), MP3D_SEEK_TO_SAMPLE) != 0) {
            return result;
        }
        result.sourceRate = static_cast<std::uint32_t>(decoder.info.hz);
        result.channels = static_cast<std::uint32_t>(decoder.info.channels);
        std::vector<mp3d_sample_t> decoded(static_cast<std::size_t>(decoder.samples));
        const std::size_t samples = mp3dec_ex_read(
            &decoder, decoded.data(), decoded.size());
        mp3dec_ex_close(&decoder);
        if (samples == 0u || result.sourceRate == 0u ||
            (result.channels != 1u && result.channels != 2u)) return result;
        const std::size_t sourceFrames = samples / result.channels;
        const std::size_t outputFrames = static_cast<std::size_t>(
            static_cast<std::uint64_t>(sourceFrames) * targetRate / result.sourceRate);
        result.stereo.resize(outputFrames * 2u);
        for (std::size_t frame = 0; frame < outputFrames; ++frame) {
            const double sourcePosition =
                static_cast<double>(frame) * result.sourceRate / targetRate;
            const std::size_t first = std::min(
                static_cast<std::size_t>(sourcePosition), sourceFrames - 1u);
            const std::size_t second = std::min(first + 1u, sourceFrames - 1u);
            const float fraction = static_cast<float>(sourcePosition - first);
            for (std::size_t channel = 0; channel < 2u; ++channel) {
                const std::size_t sourceChannel = result.channels == 1u ? 0u : channel;
                const float a = decoded[first * result.channels + sourceChannel];
                const float b = decoded[second * result.channels + sourceChannel];
                result.stereo[frame * 2u + channel] = static_cast<std::int16_t>(
                    a + (b - a) * fraction);
            }
        }
        return result;
    }

    bool FindActorLocalPosition(
        const std::string& actorPath,
        OVR::Vector3f& localPosition) const {
        const auto found = std::find_if(
            interactiveActors_.begin(), interactiveActors_.end(),
            [&](const InteractiveActor& actor) { return actor.objectPath == actorPath; });
        if (found != interactiveActors_.end()) {
            localPosition = found->localPosition;
            return true;
        }
        const auto snapshot = std::find_if(
            actorSnapshots_.begin(), actorSnapshots_.end(),
            [&](const PortableActorSnapshot& actor) {
                return actor.objectPath == actorPath && actor.hasLocation;
            });
        if (snapshot == actorSnapshots_.end()) return false;
        float originX = -1149.244f;
        float originY = 825.844f;
        float originZ = -65.103f;
        for (const PortableActorSnapshot& actor : actorSnapshots_) {
            const std::size_t separator = actor.classPath.find_last_of('.');
            const std::string leafClass = separator == std::string::npos
                ? actor.classPath
                : actor.classPath.substr(separator + 1u);
            if (actor.hasLocation && leafClass == "PlayerStart") {
                originX = actor.x;
                originY = actor.y;
                originZ = actor.z;
                break;
            }
        }
        constexpr float unitsToMeters = 1.0f / 52.5f;
        localPosition = {
            (snapshot->y - originY) * unitsToMeters,
            (snapshot->z - originZ) * unitsToMeters + 1.0f,
            -(snapshot->x - originX) * unitsToMeters};
        return true;
    }

    bool QueueDialogueAudio(
        const PortableSound& sound,
        const std::string& sourceActorPath) {
        if (sound.format.ToString() != "mp3" || sound.data.empty() || audioSampleRate_ == 0u ||
            (dialogueDecodeFuture_.valid() && dialogueDecodeFuture_.wait_for(
                std::chrono::seconds(0)) != std::future_status::ready)) {
            return false;
        }
        if (dialogueDecodeFuture_.valid()) PollDialogueAudioDecode();
        pendingDialogueSpatialized_ = FindActorLocalPosition(
            sourceActorPath, pendingDialogueLocalPosition_);
        pendingDialogueActorPath_ = pendingDialogueSpatialized_
            ? sourceActorPath
            : std::string();
        const std::uint32_t targetRate = audioSampleRate_;
        dialogueDecodeFuture_ = std::async(
            std::launch::async,
            [sound, targetRate]() mutable {
                return DecodeDialogueAudio(std::move(sound), targetRate);
            });
        return true;
    }

    void PollDialogueAudioDecode() {
        if (!dialogueDecodeFuture_.valid() || dialogueDecodeFuture_.wait_for(
                std::chrono::seconds(0)) != std::future_status::ready) return;
        DecodedDialogueAudio decoded = dialogueDecodeFuture_.get();
        if (decoded.stereo.empty()) return;
        const std::size_t outputFrames = decoded.stereo.size() / 2u;
        {
            std::lock_guard<std::mutex> lock(audioMutex_);
            dialogueSamples_ = std::move(decoded.stereo);
            dialogueCursor_ = 0u;
            dialogueSpatialized_ = pendingDialogueSpatialized_;
            dialogueLocalPosition_ = pendingDialogueLocalPosition_;
        }
        ALOG(
            "DeusExQuest: queued dialogue MP3: %u Hz/%u channels -> %u Hz, %zu frames, spatial=%s actor=%s",
            decoded.sourceRate,
            decoded.channels,
            decoded.targetRate,
            outputFrames,
            dialogueSpatialized_ ? "true" : "false",
            pendingDialogueActorPath_.c_str());
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
        spatialAudioEmitters_.clear();
        dialogueSamples_.clear();
        dialogueCursor_ = 0;
        dialogueSpatialized_ = false;
        pendingDialogueSpatialized_ = false;
        pendingDialogueActorPath_.clear();
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
                const OVR::Vector3f lighting = CalculateMapLighting(
                    {vertex.px, vertex.py, vertex.pz},
                    {vertex.nx, vertex.ny, vertex.nz});
                RecordLighting(lighting);
                descriptor.attribs.color.emplace_back(
                    static_cast<float>(vertex.materialSlot) / 255.0f,
                    lighting.x,
                    lighting.y,
                    lighting.z);
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
        ResetLightingStats();
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
        ResetLightingStats();
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
                    const OVR::Vector3f lighting = CalculateMapLighting(
                        {vertex.px, vertex.py, vertex.pz},
                        {vertex.nx, vertex.ny, vertex.nz});
                    RecordLighting(lighting);
                    descriptor.attribs.color.emplace_back(
                        static_cast<float>(vertex.materialSlot) / 255.0f,
                        lighting.x,
                        lighting.y,
                        lighting.z);
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
        LogLightingStats();
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
    std::vector<MapLight> activeMapLights_;
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
    std::vector<SpatialAudioEmitter> spatialAudioEmitters_;
    std::uint32_t audioSampleRate_{};
    std::vector<std::int16_t> dialogueSamples_;
    std::size_t dialogueCursor_{};
    OVR::Vector3f dialogueLocalPosition_{};
    OVR::Vector3f pendingDialogueLocalPosition_{};
    float dialogueLeftGain_{1.0f};
    float dialogueRightGain_{1.0f};
    bool dialogueSpatialized_{};
    bool pendingDialogueSpatialized_{};
    std::string pendingDialogueActorPath_;
    std::future<DecodedDialogueAudio> dialogueDecodeFuture_;
    std::vector<CollisionTriangle> collisionTriangles_;
    std::unordered_map<std::int64_t, std::vector<std::uint32_t>> collisionGrid_;
    std::vector<std::uint32_t> oversizedCollisionTriangles_;
    OVR::Vector3f worldPosition_{0.0f, 0.0f, 0.0f};
    OVR::Vector3f currentHeadStage_{};
    OVR::Vector3f actorStreamingCenter_{};
    float sceneYaw_{};
    bool turnLatch_{};
    bool choiceCycleLatch_{};
    bool fireLatch_{};
    bool inventoryCycleLatch_{};
    inline static std::size_t selectedInventoryIndex_{};
    std::size_t performanceFrames_{};
    float performanceSeconds_{};
    float performanceWorstDelta_{};
    OVRFW::TinyUI ui_;
    OVRFW::VRMenuObject* hudLabel_{};
    OVRFW::VRMenuObject* inventoryLabel_{};
    PersonaUiRenderer personaRenderer_;
    PortablePackageTables personaUiPackage_;
    std::vector<std::uint8_t> personaBaseRgba_;
    std::unordered_map<std::string, PortableTextureImage> personaIconCache_;
    GLuint personaTextureId_{};
    std::uint32_t personaTextureWidth_{};
    std::uint32_t personaTextureHeight_{};
    bool inventoryMenuOpen_{};
    PersonaPage personaPage_{PersonaPage::Inventory};
    bool inventoryMenuDirty_{true};
    std::size_t inventoryMenuDisplayedCount_{invalidRendererIndex_};
    std::size_t inventoryMenuDisplayedSelection_{invalidRendererIndex_};
    float inventoryMenuDisplayedHealth_{-1.0f};
    std::size_t displayedInventoryCount_{invalidRendererIndex_};
    float displayedPlayerHealth_{-1.0f};
    std::string displayedSelectedInventory_;
    std::string interactionStatus_;
    std::string displayedInteractionStatus_;
    float interactionStatusSeconds_{};
    std::unordered_map<std::string, std::size_t> dialogueOffsets_;
    std::vector<std::string> personaLogEntries_;
    std::vector<PortableDialogueResult::Choice> pendingChoices_;
    std::string pendingChoiceActor_;
    std::string pendingChoiceAudioPackage_;
    std::size_t pendingChoiceIndex_{};
    static constexpr const char* gameRoot_ =
        "/data/user/0/dev.deusex.questvr.smoketest/files/DeusEx";
    std::vector<std::string> mapNames_;
    std::string currentMapName_{"00_Training"};
    std::size_t currentMapIndex_{};
    float mapRequestPollSeconds_{};
    bool captureScreenshotRequested_{};
    std::uint32_t captureScreenshotDelayFrames_{};
    float mapTravelCooldown_{3.0f};
    bool restorePoseAfterTransition_{};
    OVR::Vector3f restoredWorldPosition_{};
    float restoredSceneYaw_{};
    std::string pendingMapName_;
    std::future<MapPreparation> mapCacheFuture_;
    std::string transitionMapName_;
    MapTransitionPhase transitionPhase_{MapTransitionPhase::Idle};
    std::vector<PortableActorSnapshot> preparedActorSnapshots_;
    std::vector<SpatialAudioEmitter> preparedSpatialAudioEmitters_;
    std::vector<MapLight> preparedMapLights_;
    PortableTextureArray preparedActorTextures_;
    WorldTexturePreparation preparedWorldTexture_;
    WorldMeshPreparation preparedWorldMesh_;
    WorldMeshPreparation pendingWorldMesh_;
    std::size_t pendingWorldMeshChunk_{};
    float lightingMinimum_{std::numeric_limits<float>::infinity()};
    float lightingMaximum_{};
    double lightingSum_{};
    std::size_t lightingSamples_{};
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
