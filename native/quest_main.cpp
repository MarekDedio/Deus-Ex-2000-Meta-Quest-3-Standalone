#include <openxr/openxr.h>
#include <GLES3/gl3.h>
#include <aaudio/AAudio.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cmath>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <deque>
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

#include "Input/TinyUI.h"
#include "Render/GeometryBuilder.h"
#include "Render/GeometryRenderer.h"
#include "Render/GlGeometry.h"
#include "Render/GlTexture.h"
#include "Render/SurfaceRender.h"
#include "XrApp.h"
#include "portable_unreal_runtime.h"
#include "persona_ui_canvas.h"
#include "quest_map_cache.h"
#include "vr_world_transform.h"
#include "frame_work_budget.h"
#include "map_transition_transaction.h"
#include "quest_map_lighting.h"
#include "quest_static_lightmap_cache.h"
#include "quest_actor_geometry.h"
#include "quest_save_metadata.h"
#include "quest_save_bundle.h"
#include "async_result_epoch.h"

#define MINIMP3_IMPLEMENTATION
#include "portable_mp3_audio.h"

class TexturedGeometryRenderer {
   public:
    void Init(
        const OVRFW::GlGeometry::Descriptor& descriptor,
        const OVRFW::GlTexture& texture,
        bool cullEnable = true, std::uint32_t polyFlags = 0u) {
        static const char* vertexShader = R"glsl(
            attribute highp vec4 Position;
            attribute highp vec2 TexCoord;
            attribute lowp vec4 VertexColor;
            varying highp vec2 oTexCoord;
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
            uniform highp float Masked;
            varying highp vec2 oTexCoord;
            varying mediump float oLayer;
            varying lowp vec3 oLight;
            void main() {
                lowp vec4 texel = texture(
                    Texture0, vec3(fract(oTexCoord), floor(oLayer + 0.5)));
                // Only authored masked materials discard palette index zero.
                if (Masked > 0.5 && texel.a < 0.5) discard;
                gl_FragColor = vec4(texel.rgb * oLight, Masked < -0.5 ? texel.a : 1.0);
            }
        )glsl";
        static OVRFW::ovrProgramParm uniformParms[] = {
            {.Name = "Texture0", .Type = OVRFW::ovrProgramParmType::TEXTURE_SAMPLED},
            {.Name = "Masked", .Type = OVRFW::ovrProgramParmType::FLOAT},
        };
        if (!sharedProgram_.IsValid()) {
            sharedProgram_ = OVRFW::GlProgram::Build(
                "", vertexShader, "", fragmentShader, uniformParms, 2);
        }
        if (!sharedProgram_.IsValid())
            throw std::runtime_error("textured geometry shader creation failed");
        surface_.geo = OVRFW::GlGeometry(descriptor.attribs, descriptor.indices);
        OVRFW::ovrGraphicsCommand& command = surface_.graphicsCommand;
        command.Program = sharedProgram_;
        command.Textures[0] = texture;
        command.UniformData[0].Data = &command.Textures[0];
        if ((polyFlags & 4u) != 0u) polyFlags &= ~2u;
        masked_ = (polyFlags & 2u) != 0u ? 1.0f : 0.0f;
        if ((polyFlags & 0x10000000u) != 0u && (polyFlags & (4u|64u)) == 0u) masked_ = -1.0f;
        command.UniformData[1].Data = &masked_;
        command.GpuState.depthEnable = command.GpuState.depthMaskEnable = true;
        command.GpuState.blendEnable = OVRFW::ovrGpuState::BLEND_DISABLE;
        command.GpuState.cullEnable = cullEnable;
        blended_ = (polyFlags & (4u|64u|0x10000000u)) != 0u;
        localCenter_ = {};
        for (const auto& position : descriptor.attribs.position) localCenter_ += position;
        if (!descriptor.attribs.position.empty()) localCenter_ *= 1.0f/descriptor.attribs.position.size();
        if ((polyFlags & (4u|64u)) != 0u) {
            command.GpuState.depthMaskEnable = (polyFlags & 0x80000000u) != 0u;
            command.GpuState.blendEnable = OVRFW::ovrGpuState::BLEND_ENABLE;
            command.GpuState.blendSrc = (polyFlags & 4u) != 0u ? GL_ONE : GL_DST_COLOR;
            command.GpuState.blendDst = (polyFlags & 4u) != 0u ? GL_ONE_MINUS_SRC_COLOR : GL_SRC_COLOR;
        }
        else if ((polyFlags & 0x10000000u) != 0u) {
            command.GpuState.blendEnable = OVRFW::ovrGpuState::BLEND_ENABLE;
            command.GpuState.blendSrc = GL_ONE;
            command.GpuState.blendDst = GL_ONE_MINUS_SRC_ALPHA;
        }
    }

    void Shutdown() {
        surface_.geo.Free();
    }
    static void ShutdownSharedProgram() {
        if (sharedProgram_.IsValid()) OVRFW::GlProgram::Free(sharedProgram_);
    }
    void SetPose(const OVR::Posef& pose) { pose_ = pose; }
    bool IsBlended() const { return blended_; }
    float DepthAlong(const OVR::Vector3f& eye, const OVR::Vector3f& forward) const {
        return (modelMatrix_.Transform(localCenter_)-eye).Dot(forward);
    }
    void Update() {
        pose_.Rotation.Normalize();
        modelMatrix_ = OVR::Matrix4f(pose_);
    }
    void Render(std::vector<OVRFW::ovrDrawSurface>& surfaces) {
        surface_.graphicsCommand.UniformData[0].Data =
            &surface_.graphicsCommand.Textures[0];
        surface_.graphicsCommand.UniformData[1].Data = &masked_;
        surfaces.emplace_back(modelMatrix_, &surface_);
    }

   private:
    OVRFW::ovrSurfaceDef surface_;
    inline static OVRFW::GlProgram sharedProgram_;
    float masked_{};
    bool blended_{};
    OVR::Vector3f localCenter_;
    OVR::Posef pose_ = OVR::Posef::Identity();
    OVR::Matrix4f modelMatrix_ = OVR::Matrix4f::Identity();
};

// BSP lightmaps use their own program; actor vertex lighting and texture
// arrays are intentionally unchanged. Deque ownership preserves uniform
// pointers while incremental world/actor chunks are appended.
class BakedWorldGeometryRenderer {
   public:
    void Init(const OVRFW::GlGeometry::Descriptor& descriptor,
              const OVRFW::GlTexture& materials, const OVRFW::GlTexture& lightmaps,
              float gainScale) {
        if (!materials.IsValid() || !lightmaps.IsValid() ||
            !std::isfinite(gainScale) || gainScale <= 0.0f)
            throw std::runtime_error("baked BSP texture/gain is invalid");
        static const char* vertexShader = R"glsl(
            attribute highp vec4 Position;
            attribute highp vec2 TexCoord;
            attribute highp vec2 TexCoord1;
            attribute highp vec3 Tangent;
            attribute highp vec4 JointWeights;
            attribute lowp vec4 VertexColor;
            varying highp vec2 oTexCoord;
            varying highp vec2 oLightmapUv;
            varying highp vec4 oLightmapRect;
            varying mediump float oLayer;
            varying mediump vec2 oLightmapMode;
            varying lowp vec3 oFallbackLight;
            void main() {
                gl_Position = TransformVertex(Position);
                oTexCoord = TexCoord;
                oLightmapUv = TexCoord1;
                oLightmapRect = JointWeights;
                oLayer = VertexColor.r * 255.0;
                oLightmapMode = Tangent.xy;
                oFallbackLight = VertexColor.gba;
            }
        )glsl";
        static const char* fragmentShader = R"glsl(
            precision highp float;
            uniform highp sampler2DArray Texture0;
            uniform highp sampler2DArray Texture1;
            uniform highp float AtlasGainScale;
            varying highp vec2 oTexCoord;
            varying highp vec2 oLightmapUv;
            varying highp vec4 oLightmapRect;
            varying mediump float oLayer;
            varying mediump vec2 oLightmapMode;
            varying lowp vec3 oFallbackLight;
            void main() {
                vec4 texel = texture(Texture0, vec3(fract(oTexCoord), floor(oLayer + 0.5)));
                if (texel.a < 0.5) discard;
                vec3 gain = oFallbackLight;
                if (oLightmapMode.y > 0.5) {
                    gain = vec3(1.0); // Authored PF_Unlit, not artificial ambient.
                } else if (oLightmapMode.x >= 0.0) {
                    vec2 uv = clamp(oLightmapUv, oLightmapRect.xy, oLightmapRect.zw);
                    gain = texture(Texture1, vec3(uv, floor(oLightmapMode.x + 0.5))).rgb * AtlasGainScale;
                }
                gl_FragColor = vec4(texel.rgb * gain, texel.a);
            }
        )glsl";
        static OVRFW::ovrProgramParm parms[] = {
            {.Name = "Texture0", .Type = OVRFW::ovrProgramParmType::TEXTURE_SAMPLED},
            {.Name = "Texture1", .Type = OVRFW::ovrProgramParmType::TEXTURE_SAMPLED},
            {.Name = "AtlasGainScale", .Type = OVRFW::ovrProgramParmType::FLOAT},
        };
        if (!sharedProgram_.IsValid())
            sharedProgram_ = OVRFW::GlProgram::Build("",vertexShader,"",fragmentShader,parms,3);
        if (!sharedProgram_.IsValid())
            throw std::runtime_error("baked BSP shader creation failed");
        surface_.geo = OVRFW::GlGeometry(descriptor.attribs,descriptor.indices);
        gainScale_ = gainScale;
        auto& command = surface_.graphicsCommand;
        command.Program = sharedProgram_;
        command.Textures[0] = materials;
        command.Textures[1] = lightmaps;
        command.UniformData[0].Data = &command.Textures[0];
        command.UniformData[1].Data = &command.Textures[1];
        command.UniformData[2].Data = &gainScale_;
        command.GpuState.depthEnable = command.GpuState.depthMaskEnable = true;
        command.GpuState.blendEnable = OVRFW::ovrGpuState::BLEND_DISABLE;
        command.GpuState.cullEnable = true;
    }
    void Shutdown() { surface_.geo.Free(); }
    static void ShutdownSharedProgram() {
        if (sharedProgram_.IsValid()) OVRFW::GlProgram::Free(sharedProgram_);
    }
    void SetPose(const OVR::Posef& pose) { pose_ = pose; }
    void Update() { pose_.Rotation.Normalize(); modelMatrix_ = OVR::Matrix4f(pose_); }
    void Render(std::vector<OVRFW::ovrDrawSurface>& surfaces) {
        auto& command = surface_.graphicsCommand;
        command.UniformData[0].Data = &command.Textures[0];
        command.UniformData[1].Data = &command.Textures[1];
        command.UniformData[2].Data = &gainScale_;
        surfaces.emplace_back(modelMatrix_,&surface_);
    }
   private:
    OVRFW::ovrSurfaceDef surface_;
    inline static OVRFW::GlProgram sharedProgram_;
    float gainScale_{1.0f};
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
                lowp vec4 texel = texture2D(Texture0, oTexCoord);
                if (texel.a < 0.01) discard;
                // Theme modulation belongs to the neutral background/border
                // layers, not to the original full-color inventory icons.
                gl_FragColor = texel;
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
        command.GpuState.blendEnable = OVRFW::ovrGpuState::BLEND_ENABLE;
        command.GpuState.blendSrc = GL_SRC_ALPHA;
        command.GpuState.blendDst = GL_ONE_MINUS_SRC_ALPHA;
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
            // Original PersonaScreenBaseWindow is 640x480. The stored
            // power-of-two tiles include padding outside that window.
            OVR::Matrix4f::Scaling(0.60f, 0.45f, 1.0f);
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
    struct InitialPreparation;
    struct PersonaPreparation;
    struct AmbientPreparation;

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
            "Inventory",
            OVR::Vector3f(0.0f, 0.0f, 0.0f),
            OVR::Vector2f(266.0f, 319.0f));
        personaTabsLabel_ = ui_.AddLabel("", {}, {622.0f, 25.0f});
        personaDetailsLabel_ = ui_.AddLabel("", {}, {238.0f, 218.0f});
        personaFooterLabel_ = ui_.AddLabel("", {}, {575.0f, 52.0f});
        for (OVRFW::VRMenuObject* label : PersonaLabels()) {
            if (label == nullptr) return false;
            // ColorThemeHUD_Default: normal text 200, header text 255.
            label->SetTextColor({200.0f / 255.0f, 200.0f / 255.0f,
                                 200.0f / 255.0f, 1.0f});
            label->SetSurfaceVisible(0, false);
            OVRFW::VRMenuFontParms font = label->GetFontParms();
            font.AlignHoriz = OVRFW::HORIZONTAL_LEFT;
            // SDK TOP anchors the bottom of a multiline block. BASELINE
            // keeps the first line fixed when the page's line count changes.
            font.AlignVert = OVRFW::VERTICAL_BASELINE;
            font.MaxLines = 16;
            font.WrapWidth = -1.0f;
            label->SetFontParms(font);
            label->SetVisible(false);
        }
        personaTabsLabel_->SetTextColor({1.0f, 1.0f, 1.0f, 1.0f});
        return true;
    }

    void AppShutdown(const xrJava* context) override {
        DiscardInitialPreparation();
        inventoryLabel_ = personaTabsLabel_ = personaDetailsLabel_ = personaFooterLabel_ = nullptr;
        hudLabel_ = nullptr;
        ui_.Shutdown();
        ClearPendingStaticLightmapUpload();
        DestroySceneGeometry();
        BakedWorldGeometryRenderer::ShutdownSharedProgram();
        OVRFW::XrApp::AppShutdown(context);
    }

    static bool ValidateInitialRuntime() {
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
        return true;
    }

    bool SessionInit() override {
        // No prior-session worker may retain ownership of the global portable
        // runtime/name tables when this session starts another bootstrap.
        DiscardInitialPreparation();
        hasPreviousHeadStage_ = false;
        headTrackingValid_ = false;
        headTrackingReported_ = false;
        needsTrackingRebase_ = false;
        pendingReferenceChanges_.clear();
        hasTransitionHeadAnchor_ = false;
        // The worker will leave a fresh Training runtime. Reset the matching
        // map/pose now rather than retaining a previous session's destination.
        // User quicksaves remain untouched; automatic cross-session runtime
        // resume is not implemented here.
        currentMapName_ = "00_Training";
        currentMapIndex_ = 0u;
        worldPosition_ = {};
        currentHeadStage_ = previousHeadStage_ = {};
        sceneYaw_ = 0.0f;
        selectedInventoryIndex_ = 0u;
        personaPage_ = PersonaPage::Inventory;
        personaOriginalTextReady_ = false;
        SetInventoryMenuOpen(false);
        ClearPendingConversation();
        InvalidateDialogueAudio();
        personaLogEntries_.clear();
        dialogueOffsets_.clear();
        ClearPendingPersonaRestore();
        displayedInventoryCount_ = invalidRendererIndex_;
        displayedPlayerHealth_ = -1.0f;
        displayedSelectedInventory_.clear();
        displayedInteractionStatus_.clear();
        interactionStatus_.clear();
        interactionStatusSeconds_ = 0.0f;
        restorePoseAfterTransition_ = false;
        mapTravelCooldown_ = 3.0f;
        turnLatch_ = fireLatch_ = inventoryCycleLatch_ = false;
        runtimeAvailable_ = false;
        initialPreparationPending_ = true;
        initialPreparationFailed_ = false;
        pendingMapName_.clear();
        transitionMapName_.clear();
        transitionPhase_ = MapTransitionPhase::Idle;
        mapNames_.clear();
        if (hudLabel_ != nullptr) hudLabel_->SetText("DEUS EX VR\nSTARTING... PREPARING GAME DATA");
        try {
            initialPreparationFuture_ = std::async(std::launch::async, [] {
                return PrepareInitialRuntime();
            });
        } catch (const std::exception& error) {
            initialPreparationPending_ = false;
            initialPreparationFailed_ = true;
            interactionStatus_ = error.what();
            if (hudLabel_ != nullptr) hudLabel_->SetText("DEUS EX VR\nSTARTUP FAILED\n%s", error.what());
            ALOG("DeusExQuest: initial worker could not start: %s", error.what());
        }
        // Meta's main loop cannot pump Android window/resume/input or OpenXR
        // events until this returns. The worker owns all CPU/runtime work.
        ALOG("DeusExQuest: XR session startup returned; game data preparation pending");
        return true;
    }

    void SyncActionSets(OVRFW::ovrApplFrameIn& frame) override {
        OVRFW::XrApp::SyncActionSets(frame);
        const bool previouslyValid = headTrackingValid_;
        XrSpaceLocation location{};
        location.type = XR_TYPE_SPACE_LOCATION;
        const XrResult result = xrLocateSpace(
            HeadSpace, CurrentSpace,
            static_cast<XrTime>(frame.PredictedDisplayTime * 1e9), &location);
        constexpr XrSpaceLocationFlags required =
            XR_SPACE_LOCATION_POSITION_VALID_BIT | XR_SPACE_LOCATION_ORIENTATION_VALID_BIT;
        headTrackingValid_ = XR_SUCCEEDED(result) &&
            (location.locationFlags & required) == required;
        if (!headTrackingReported_ || previouslyValid != headTrackingValid_) {
            ALOG("DeusExQuest: head tracking %s xrLocateSpace=%d flags=0x%llx",
                headTrackingValid_ ? "valid" : "unavailable",
                static_cast<int>(result),
                static_cast<unsigned long long>(location.locationFlags));
            headTrackingReported_ = true;
        }
        if (headTrackingValid_) frame.HeadPose = FromXrPosef(location.pose);
    }

    void AppHandleEvent(XrEventDataBaseHeader* event) override {
        if (event->type != XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING) return;
        const auto& change = *reinterpret_cast<XrEventDataReferenceSpaceChangePending*>(event);
        const XrReferenceSpaceType activeType = CurrentSpace == StageSpace &&
            StageSpace != XR_NULL_HANDLE ? XR_REFERENCE_SPACE_TYPE_STAGE : XR_REFERENCE_SPACE_TYPE_LOCAL;
        if (change.session != Session || change.referenceSpaceType != activeType) return;
        pendingReferenceChanges_.push_back(change);
        std::stable_sort(pendingReferenceChanges_.begin(), pendingReferenceChanges_.end(),
            [](const auto& left, const auto& right) { return left.changeTime < right.changeTime; });
    }

    void ApplyReferenceSpaceChanges(XrTime displayTime) {
        while (!pendingReferenceChanges_.empty() &&
               pendingReferenceChanges_.front().changeTime <= displayTime) {
            const auto change = pendingReferenceChanges_.front();
            pendingReferenceChanges_.erase(pendingReferenceChanges_.begin());
            if (!hasPreviousHeadStage_ && !needsTrackingRebase_) continue;
            const OVR::Posef origin = FromXrPosef(change.poseInPreviousSpace);
            const OVR::Vector3f up = origin.Rotation.Rotate(OVR::Vector3f(0.0f, 1.0f, 0.0f));
            if (change.poseValid && std::fabs(up.x) < 0.001f &&
                std::fabs(up.z) < 0.001f && up.y > 0.999f) {
                const OVR::Vector3f right = origin.Rotation.Rotate(OVR::Vector3f(1.0f, 0.0f, 0.0f));
                const float originYaw = std::atan2(-right.z, right.x);
                QuestVr::RebaseReferenceSpace(origin.Translation, originYaw,
                    worldPosition_, sceneYaw_, previousHeadStage_);
                if (restorePoseAfterTransition_ && !restoredMapLocalPose_) {
                    OVR::Vector3f unusedHead{};
                    QuestVr::RebaseReferenceSpace(origin.Translation, originYaw,
                        restoredWorldPosition_, restoredSceneYaw_, unusedHead);
                }
                ALOG("DeusExQuest: preserved map pose across OpenXR recenter");
            } else {
                if (hasPreviousHeadStage_) {
                    trackingResumeLocalHead_ = StageToLocal(previousHeadStage_, worldPosition_);
                }
                needsTrackingRebase_ = true;
                hasPreviousHeadStage_ = false;
                ALOG("DeusExQuest: OpenXR reference discontinuity; rebasing on valid tracking");
            }
        }
    }

    void Update(const OVRFW::ovrApplFrameIn& frame) override {
        CompleteInitialPreparation();
        if (initialPreparationPending_ || initialPreparationFailed_) {
            // The bootstrap is the sole owner of portable runtime and NameString
            // storage. No map retry, mailbox diagnostic or gameplay query is safe.
            if (headTrackingValid_ && hudLabel_ != nullptr) {
                OVR::Posef pose = frame.HeadPose;
                pose.Translation += frame.HeadPose.Rotation.Rotate({0.0f, -0.24f, -0.78f});
                hudLabel_->SetLocalPose(pose);
                ui_.Update(frame);
            }
            return;
        }
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
                "DeusExQuest: Quest frame timing %.1f fps average, %.2f ms worst over %zu frames; actors=%zu collision=%zu tracking=%s",
                static_cast<double>(performanceFrames_) / performanceSeconds_,
                performanceWorstDelta_ * 1000.0f,
                performanceFrames_,
                interactiveActors_.size(),
                collisionTriangles_.size(),
                headTrackingValid_ ? "valid" : "unavailable");
            performanceFrames_ = 0;
            performanceSeconds_ = 0.0f;
            performanceWorstDelta_ = 0.0f;
        }
        ApplyReferenceSpaceChanges(static_cast<XrTime>(frame.PredictedDisplayTime * 1e9));
        if (!headTrackingValid_) {
            if (hasPreviousHeadStage_) {
                trackingResumeLocalHead_ = StageToLocal(previousHeadStage_, worldPosition_);
                needsTrackingRebase_ = true;
                hasPreviousHeadStage_ = false;
            }
            return;
        }
        currentHeadStage_ = frame.HeadPose.Translation;
        const OVR::Vector3f saveHeadForward = frame.HeadPose.Rotation.Rotate({0.0f, 0.0f, -1.0f});
        if (std::hypot(saveHeadForward.x, saveHeadForward.z) > 0.0001f)
            currentHeadStageYaw_ = std::atan2(-saveHeadForward.x, -saveHeadForward.z);
        if (!hasPreviousHeadStage_) {
            if (needsTrackingRebase_) {
                worldPosition_ = QuestVr::RestoreHorizontalHeadPosition(
                    worldPosition_, currentHeadStage_, trackingResumeLocalHead_, sceneYaw_);
                needsTrackingRebase_ = false;
            } else {
                QuestVr::AnchorSpawnToHead(currentHeadStage_, worldPosition_);
            }
            previousHeadStage_ = currentHeadStage_;
            hasPreviousHeadStage_ = true;
        }
        if (hasTransitionHeadAnchor_) {
            worldPosition_ = QuestVr::RestoreHorizontalHeadPosition(
                worldPosition_, currentHeadStage_, transitionHeadLocal_, sceneYaw_);
            previousHeadStage_ = currentHeadStage_;
        }
        const OVR::Vector3f safeLocalHead =
            StageToLocal(previousHeadStage_, worldPosition_);
        const bool mapLoading = !runtimeAvailable_ ||
            !pendingMapName_.empty() || !transitionMapName_.empty();
        const bool playerAlive = !mapLoading && GetPortableRuntimePlayerHealth() > 0.0f;
        if (!mapLoading && frame.Clicked(frame.kButtonMenu)) {
            SetInventoryMenuOpen(!inventoryMenuOpen_);
        }
        bool dismissedMenuWithB{};
        if (inventoryMenuOpen_ && frame.Clicked(frame.kButtonB)) {
            SetInventoryMenuOpen(false);
            dismissedMenuWithB = true;
        }
        const bool gameplayActive = playerAlive && !mapLoading && !inventoryMenuOpen_;
        const bool turnPressed = std::fabs(frame.RightRemoteJoystick.x) > 0.7f;
        if (gameplayActive && turnPressed && !turnLatch_) {
            constexpr float snapRadians = 3.14159265358979323846f / 6.0f;
            SnapTurnAroundHead(
                std::copysign(snapRadians, frame.RightRemoteJoystick.x),
                frame.HeadPose.Translation);
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
        const OVR::Vector3f stageRight = QuestVr::HorizontalHeadRight(
            frame.HeadPose.Rotation.Rotate(OVR::Vector3f(0.0f, 0.0f, -1.0f)),
            frame.HeadPose.Rotation.Rotate(OVR::Vector3f(1.0f, 0.0f, 0.0f)));
        const OVR::Vector3f localRight =
            QuestVr::StageDirectionToLocal(stageRight, sceneYaw_);
        // Construct the movement candidate after turning so accepting it cannot
        // overwrite the head-pivot translation computed by the snap turn.
        OVR::Vector3f candidate = QuestVr::LocomotionCandidate(
            worldPosition_, stageRight,
            gameplayActive ? frame.LeftRemoteJoystick.x : 0.0f,
            gameplayActive ? frame.LeftRemoteJoystick.y : 0.0f,
            2.2f, frame.DeltaSeconds);
        const bool choiceCyclePressed = std::fabs(frame.RightRemoteJoystick.y) > 0.7f;
        if (!mapLoading && inventoryMenuOpen_ && personaPage_ == PersonaPage::Inventory &&
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
        const bool movementBlocked = QuestVr::HorizontalMotionBlocked(
            safeLocalHead, StageToLocal(currentHeadStage_, candidate),
            [&](const OVR::Vector3f& sampleLocalHead) {
                const OVR::Vector3f sampleWorldPosition = QuestVr::RestoreHorizontalHeadPosition(
                    candidate, currentHeadStage_, sampleLocalHead, sceneYaw_);
                return CapsuleTouchesWall(currentHeadStage_, sampleWorldPosition);
            });
        if (!movementBlocked) {
            worldPosition_ = candidate;
        } else {
            OVR::Vector3f compensated = QuestVr::RestoreHorizontalHeadPosition(
                worldPosition_, currentHeadStage_, safeLocalHead, sceneYaw_);
            FollowGround(frame.HeadPose.Translation, compensated);
            if (!CapsuleTouchesWall(frame.HeadPose.Translation, compensated)) {
                worldPosition_ = compensated;
            } else {
                OVR::Vector3f grounded = worldPosition_;
                FollowGround(frame.HeadPose.Translation, grounded);
                worldPosition_.y = grounded.y;
            }
        }
        previousHeadStage_ = currentHeadStage_;
        UpdateSpatialAudioGains(
            StageToLocal(currentHeadStage_, worldPosition_), localRight.x, localRight.z);

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
        for (auto& renderer : bakedWorldRenderers_) {
            renderer.SetPose(worldPose);
            renderer.Update();
        }
        mapTravelCooldown_ = std::max(0.0f, mapTravelCooldown_ - frame.DeltaSeconds);
        if (!mapLoading && actorSnapshots_.size() > 1000u) {
            const OVR::Vector3f playerLocal = StageToLocal(currentHeadStage_, worldPosition_);
            const float dx = playerLocal.x - actorStreamingCenter_.x;
            const float dz = playerLocal.z - actorStreamingCenter_.z;
            if (dx * dx + dz * dz > 10.0f * 10.0f) {
                DestroyActorGeometry();
                BuildActorMarkers();
            }
        }
        if (!mapLoading && inventoryMenuOpen_ && personaPage_ == PersonaPage::Inventory &&
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
            } else if (UseTargetedActor(frame.RightRemotePointPose) &&
                       pendingMapName_.empty() && transitionMapName_.empty()) {
                DestroyActorGeometry();
                actorSnapshots_ = GetPortableRuntimeMapActors();
                BuildActorMarkers();
            }
        }
        const bool firePressed = frame.RightRemoteIndexTrigger > 0.75f;
        if (gameplayActive && pendingMapName_.empty() && transitionMapName_.empty() &&
            frame.RightRemoteTracked && firePressed && !fireLatch_) {
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
        if (gameplayActive && pendingMapName_.empty() && transitionMapName_.empty() &&
            gripPressed && !inventoryCycleLatch_) {
            const std::size_t count = GetPortableRuntimeInventoryCount();
            if (count != 0u) selectedInventoryIndex_ = (selectedInventoryIndex_ + 1u) % count;
            displayedInventoryCount_ = invalidRendererIndex_;
        }
        inventoryCycleLatch_ = gripPressed;
        if (!mapLoading && pendingMapName_.empty() && transitionMapName_.empty() &&
            (gameplayActive || inventoryMenuOpen_) && frame.Clicked(frame.kButtonY)) {
            if (pendingChoices_.empty()) {
                SaveGameState();
            } else {
                interactionStatus_ = "FINISH RESPONSE BEFORE SAVING";
                interactionStatusSeconds_ = 3.0f;
            }
        }
        if (!mapLoading && pendingMapName_.empty() && transitionMapName_.empty() &&
            frame.Clicked(frame.kButtonX)) LoadGameState();
        if ((gameplayActive || (!runtimeAvailable_ && pendingMapName_.empty() &&
                              transitionMapName_.empty())) &&
            !dismissedMenuWithB && frame.Clicked(frame.kButtonB)) LoadNextMap();
        // Travel can start the worker. Run it after this frame's other portable
        // runtime actions so stale gameplayActive cannot write during loading.
        if (gameplayActive && mapTravelCooldown_ <= 0.0f &&
            pendingMapName_.empty() && transitionMapName_.empty()) {
            CheckTravelTriggers(frame.HeadPose.Translation);
        }
        mapRequestPollSeconds_ += frame.DeltaSeconds;
        if (mapRequestPollSeconds_ >= 0.5f) {
            mapRequestPollSeconds_ = 0.0f;
            PollMapTransitionRequest();
        }
        // The map-preparation worker replaces the portable runtime. Never read
        // actor meshes from it until that worker has finished and the staged
        // transition owns the new snapshots/textures.
        if (runtimeAvailable_ && pendingMapName_.empty() && transitionPhase_ == MapTransitionPhase::Idle &&
            actorGeometryBuild_) {
            try {
                AdvanceActorGeometry();
            } catch (const std::exception& error) {
                ALOG("DeusExQuest: incremental actor geometry failed: %s", error.what());
                // Discard all partial actor uploads, not the map's BSP renderers.
                DestroyActorGeometry();
            }
        }
        AdvanceMapTransition();
        CompletePendingMapLoad();
        const bool mapLoadingNow = !runtimeAvailable_ ||
            !pendingMapName_.empty() || !transitionMapName_.empty();
        if (hudLabel_ != nullptr) {
            OVR::Posef hudPose = frame.HeadPose;
            hudPose.Translation += frame.HeadPose.Rotation.Rotate(
                OVR::Vector3f(0.0f, -0.24f, -0.78f));
            hudLabel_->SetLocalPose(hudPose);
            const std::size_t inventoryCount = mapLoadingNow
                ? (displayedInventoryCount_ == invalidRendererIndex_ ? 0u : displayedInventoryCount_)
                : GetPortableRuntimeInventoryCount();
            const float playerHealth = mapLoadingNow
                ? std::max(0.0f, displayedPlayerHealth_)
                : GetPortableRuntimePlayerHealth();
            const std::vector<std::string> inventory = mapLoadingNow
                ? std::vector<std::string>{}
                : GetPortableRuntimeInventoryItems();
            const std::string selectedItem = SelectedInventoryLabel(inventory);
            if (inventoryCount != displayedInventoryCount_ ||
                std::fabs(playerHealth - displayedPlayerHealth_) > 0.01f ||
                selectedItem != displayedSelectedInventory_ ||
                interactionStatus_ != displayedInteractionStatus_) {
                hudLabel_->SetText(
                    "%s   HEALTH %.0f   INVENTORY %zu\nITEM %s\n%s\nA USE   TRIGGER FIRE   GRIP CYCLE\nMENU INVENTORY   B NEXT MAP   Y SAVE   X LOAD",
                    !runtimeAvailable_ ? "MAP ERROR - B RETRY NEXT MAP" :
                        (pendingMapName_.empty() && transitionMapName_.empty()
                            ? currentMapName_.c_str() : "LOADING..."),
                    playerHealth,
                    inventoryCount,
                    selectedItem.c_str(),
                    mapLoadingNow ? "PREPARING WORLD..." : playerHealth <= 0.0f
                        ? "DEAD - PRESS X TO QUICK-LOAD"
                        : (interactionStatus_.empty() ? "READY" : interactionStatus_.c_str()));
                displayedInventoryCount_ = inventoryCount;
                displayedPlayerHealth_ = playerHealth;
                displayedSelectedInventory_ = selectedItem;
                displayedInteractionStatus_ = interactionStatus_;
            }
        }
        UpdateInventoryMenu(frame, mapLoadingNow);
        ui_.Update(frame);
    }

    void Render(
        const OVRFW::ovrApplFrameIn& frame,
        OVRFW::ovrRendererOutput& output) override {
        if (!headTrackingValid_) return;
        for (auto& renderer : worldRenderers_) renderer.Render(output.Surfaces);
        for (auto& renderer : bakedWorldRenderers_) renderer.Render(output.Surfaces);
        std::vector<TexturedGeometryRenderer*> blendedActors;
        for (auto& renderer : texturedRenderers_) {
            if (renderer.IsBlended()) blendedActors.push_back(&renderer);
            else renderer.Render(output.Surfaces);
        }
        const auto eye = frame.HeadPose.Translation;
        const auto forward = frame.HeadPose.Rotation.Rotate(OVR::Vector3f(0,0,-1));
        std::stable_sort(blendedActors.begin(),blendedActors.end(),[&](const auto* a,const auto* b) {
            return a->DepthAlong(eye,forward) > b->DepthAlong(eye,forward);
        });
        for (auto* renderer : blendedActors) renderer->Render(output.Surfaces);
        if (inventoryMenuOpen_) personaRenderer_.Render(output.Surfaces);
        const std::size_t firstUiSurface = output.Surfaces.size();
        ui_.Render(frame, output);
        // TinyUI's batched font surface always enables depth, independently of
        // the menu object's NO_DEPTH flag. These labels are all head-locked
        // overlays: nearby BSP must not hide their text while the artwork stays
        // visible. Copy only the submitted definitions, retaining SDK-owned
        // geometry/uniforms; never mutate the SDK's const surface definitions.
        headLockedUiSurfaces_.clear();
        headLockedUiSurfaces_.reserve(output.Surfaces.size() - firstUiSurface);
        for (std::size_t index = firstUiSurface; index < output.Surfaces.size(); ++index) {
            if (output.Surfaces[index].surface == nullptr) continue;
            headLockedUiSurfaces_.push_back(*output.Surfaces[index].surface);
            auto& definition = headLockedUiSurfaces_.back();
            definition.graphicsCommand.GpuState.depthEnable = false;
            definition.graphicsCommand.GpuState.depthMaskEnable = false;
            output.Surfaces[index].surface = &definition;
        }
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
        DiscardInitialPreparation();
        CancelActorGeometryBuild();
        ClearPendingConversation();
        InvalidateDialogueAudio();
        if (dialogueDecodeFuture_.valid()) dialogueDecodeFuture_.wait();
        // A waited future is still valid. Discard its prior-session result so
        // the next Training session cannot play an old map's queued speech.
        dialogueDecodeFuture_ = {};
        if (mapCacheFuture_.valid()) mapCacheFuture_.wait();
        mapCacheFuture_ = {};
        pendingMapName_.clear();
        transitionMapName_.clear();
        transitionPhase_ = MapTransitionPhase::Idle;
        runtimeAvailable_ = false;
        hasTransitionHeadAnchor_ = false;
        ClearPendingPersonaRestore();
        preparedActorSnapshots_.clear();
        preparedSpatialAudioEmitters_.clear();
        preparedMapLights_.clear();
        preparedActorTextures_ = {};
        preparedWorldTexture_ = {};
        preparedWorldMesh_ = {};
        ClearPendingStaticLightmapUpload();
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
        for (auto& renderer : worldRenderers_) renderer.Shutdown();
        for (auto& renderer : texturedRenderers_) renderer.Shutdown();
        for (auto& renderer : bakedWorldRenderers_) renderer.Shutdown();
        worldRenderers_.clear();
        texturedRenderers_.clear();
        bakedWorldRenderers_.clear();
        actorWorldRendererIndex_ = invalidRendererIndex_;
        actorTexturedRendererIndex_ = invalidRendererIndex_;
        personaRenderer_.Shutdown();
        if (personaTextureId_ != 0u) {
            glDeleteTextures(1, &personaTextureId_);
            personaTextureId_ = 0u;
        }
        TexturedGeometryRenderer::ShutdownSharedProgram();
        BakedWorldGeometryRenderer::ShutdownSharedProgram();
        if (staticLightmapTexture_.IsValid()) {
            OVRFW::FreeTexture(staticLightmapTexture_);
            staticLightmapTexture_ = {};
        }
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
        StaticLightmapAllocate,
        StaticLightmapUpload,
        WorldGeometry,
        WorldGeometryUpload,
        CollisionGrid,
        ActorTextureAllocate,
        ActorTextureUpload,
        ActorGeometry,
        ActorGeometryUpload
    };

    static constexpr std::size_t actorGeometryChunkVertices_ = 6144u;
    static constexpr std::size_t actorGeometryOperationsPerFrame_ = 8u;
    static constexpr double actorGeometryMillisecondsPerFrame_ = 3.0;

    struct ActorGeometryPart {
        const OVRFW::GlGeometry::Descriptor* descriptor{};
        const PortableLodMesh* mesh{};
        OVR::Matrix4f transform;
        OVR::Matrix3f normalTransform;
        OVR::Vector4f color;
        std::size_t nextIndex{};
        bool textured{};
        bool sequential{};
        std::uint16_t material{};
        QuestVr::ActorTransform actorTransform;
        std::uint32_t polyFlags{};
        std::uint32_t sourcePolyFlags{};
        std::shared_ptr<const QuestVr::MeshPose> meshPose;
    };

    struct ActorGeometryBuild {
        float originX{-1149.244f};
        float originY{825.844f};
        float originZ{-65.103f};
        std::vector<std::size_t> actorIndices;
        std::size_t nextActor{};
        std::size_t meshInstances{};
        std::size_t brushInstances{};
        std::size_t spriteInstances{};
        std::size_t cubePlaceholders{};
        std::size_t hiddenActors{};
        std::size_t sampledActorPoses{}, animationOmissions{};
        bool pendingMeshPose{};
        std::size_t frames{};
        std::size_t uploadedChunks{};
        double maximumSliceMilliseconds{};
        std::map<std::string, std::size_t> meshClasses;
        std::map<std::string, std::size_t> cubeClasses;
        std::map<std::string, PortableLodMesh> meshes;
        std::map<std::string, PortableLodMesh> brushes;
        std::map<std::string, std::set<std::uint16_t>> meshMaterials;
        std::unordered_map<std::string, std::size_t> textureLayers;
        OVRFW::GlGeometry::Descriptor spriteDescriptor;
        OVRFW::GlGeometry::Descriptor cubeDescriptor;
        OVRFW::GlGeometry::Descriptor markerChunk;
        OVRFW::GlGeometry::Descriptor texturedChunk;
        std::uint32_t texturedChunkPolyFlags{};
        std::deque<ActorGeometryPart> parts;
    };

    struct MeshVertex {
        float px, py, pz;
        float nx, ny, nz;
        float u, v;
        std::int32_t materialSlot;
    };
    static_assert(sizeof(MeshVertex) == 36u,"DXQM version 2 vertex ABI changed");

    struct WorldMeshChunkPreparation {
        std::int32_t materialSlot{};
        std::vector<MeshVertex> vertices;
        std::vector<QuestVr::WorldSurfaceRecord> surfaces;
        std::vector<QuestVr::StaticLightmapVertex> lightmapVertices;
    };

    struct WorldMeshPreparation {
        bool passed{};
        std::vector<WorldMeshChunkPreparation> chunks;
        QuestVr::StaticLightmapCache lightmap;
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

    using MapLight = QuestVr::MapLight;

    struct MapPreparation {
        bool passed{};
        bool runtimeAvailable{};
        bool rollbackAttempted{};
        std::string error;
        std::vector<PortableActorSnapshot> actors;
        PortableTextureArray actorTextures;
        WorldTexturePreparation worldTexture;
        WorldMeshPreparation worldMesh;
        std::vector<SpatialAudioEmitter> spatialAudioEmitters;
        std::vector<MapLight> lights;
    };

    struct AmbientPreparation {
        bool passed{};
        std::uint32_t sampleRate{};
        std::vector<std::int16_t> samples;
    };

    struct PersonaPreparation {
        bool passed{};
        bool originalTextReady{};
        PortablePackageTables uiPackage;
        QuestVr::PersonaUiChrome chrome;
        PortableBitmapFont headerFont;
        PortableBitmapFont bodyFont;
        std::array<std::vector<std::uint8_t>, 4> pageBaseRgba;
    };

    struct InitialPreparation {
        bool passed{};
        std::string error;
        MapPreparation map;
        std::vector<std::string> mapNames;
        AmbientPreparation ambient;
        PersonaPreparation persona;
    };

    static InitialPreparation PrepareInitialRuntime() {
        InitialPreparation preparation;
        try {
            if (!ValidateInitialRuntime())
                throw std::runtime_error("initial portable runtime validation failed; see quest_main log");
            preparation.mapNames = ReadMapCatalog();
            if (!BuildQuestMapCache(gameRoot_, "00_Training"))
                throw std::runtime_error("generic initial map cache build failed");
            const auto trainingMap = LoadPortablePackageTables(
                std::string(gameRoot_) + "/Maps/00_Training.dx");
            auto& map = preparation.map;
            map.actors = GetPortableRuntimeMapActors();
            map.worldMesh = LoadWorldMeshCacheCpu();
            if (!map.worldMesh.passed)
                throw std::runtime_error("initial DXQM/DXQS mesh cache read failed");
            PrepareWorldStaticLightmaps(trainingMap, map.actors, map.worldMesh);
            map.lights = BuildMapLights(map.actors, map.worldMesh.lightmap.unrealOrigin);
            map.worldTexture = LoadWorldTextureCacheCpu();
            map.actorTextures = BuildPortableRuntimeActorTextureArray(96, 96);
            if (!map.worldTexture.passed || !map.actorTextures.passed)
                throw std::runtime_error("initial material/actor texture preparation failed");
            preparation.ambient = PrepareAmbientAudio();
            map.spatialAudioEmitters = PrepareSpatialAudioEmitters(
                map.actors, preparation.ambient.sampleRate, map.worldMesh.lightmap.unrealOrigin);
            preparation.persona = PrepareOriginalPersonaBackground();
            map.passed = map.runtimeAvailable = true;
            preparation.passed = true;
        } catch (const std::exception& error) {
            preparation.error = error.what();
            ALOG("DeusExQuest: initial CPU preparation failed: %s", error.what());
            // The startup worker is still the exclusive runtime/GC owner.
            ShutdownPortableRuntime();
        }
        return preparation;
    }

    void CompleteInitialPreparation() {
        if (!initialPreparationPending_ || !initialPreparationFuture_.valid() ||
            initialPreparationFuture_.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
            return;
        InitialPreparation preparation;
        try {
            preparation = initialPreparationFuture_.get();
        } catch (const std::exception& error) {
            preparation.error = error.what();
            ShutdownPortableRuntime();
        }
        initialPreparationPending_ = false;
        if (!preparation.passed) {
            initialPreparationFailed_ = true;
            runtimeAvailable_ = false;
            interactionStatus_ = preparation.error;
            if (hudLabel_ != nullptr) hudLabel_->SetText(
                "DEUS EX VR\nSTARTUP FAILED\n%s", preparation.error.c_str());
            ALOG("DeusExQuest: startup failed; runtime controls remain disabled: %s",
                 preparation.error.c_str());
            return;
        }
        // get() is the ownership barrier: only now may the frame thread read
        // runtime/name tables or publish worker-owned data to audio/UI/GL.
        mapNames_ = std::move(preparation.mapNames);
        const auto current = std::find(mapNames_.begin(), mapNames_.end(), currentMapName_);
        currentMapIndex_ = current == mapNames_.end() ? 0u :
            static_cast<std::size_t>(current - mapNames_.begin());
        if (!StartAmbientAudio(std::move(preparation.ambient))) {
            preparation.map.spatialAudioEmitters.clear();
            ALOG("DeusExQuest: ambient AAudio initialization failed");
        }
        CommitOriginalPersonaBackground(std::move(preparation.persona));
        preparedActorSnapshots_ = std::move(preparation.map.actors);
        preparedActorTextures_ = std::move(preparation.map.actorTextures);
        preparedWorldTexture_ = std::move(preparation.map.worldTexture);
        preparedWorldMesh_ = std::move(preparation.map.worldMesh);
        preparedSpatialAudioEmitters_ = std::move(preparation.map.spatialAudioEmitters);
        preparedMapLights_ = std::move(preparation.map.lights);
        runtimeAvailable_ = true;
        transitionMapName_ = currentMapName_;
        transitionPhase_ = MapTransitionPhase::WorldTextureAllocate;
        displayedInventoryCount_ = invalidRendererIndex_;
        ALOG("DeusExQuest: initial CPU preparation committed; staged Training GPU upload pending");
    }

    void DiscardInitialPreparation() {
        if (initialPreparationFuture_.valid()) {
            // Shutdown cannot tear down global runtime/GC/name state while the
            // worker is still constructing it. Never overwrite a live future.
            try {
                initialPreparationFuture_.get();
            } catch (const std::exception& error) {
                ALOG("DeusExQuest: discarded startup worker failed: %s", error.what());
            }
            ShutdownPortableRuntime();
        }
        initialPreparationPending_ = false;
        initialPreparationFailed_ = false;
    }

    struct InteractiveActor {
        OVR::Vector3f localPosition;
        std::string objectPath;
        std::string classPath;
        bool travel{};
        std::string destinationMap;
    };

    static std::vector<MapLight> BuildMapLights(
        const std::vector<PortableActorSnapshot>& actors, const QuestVr::LightmapVec3& origin) {
        QuestVr::MapLightBuildStats stats;
        const OVR::Vector3f verifiedOrigin{origin.x,origin.y,origin.z};
        std::vector<MapLight> lights = QuestVr::BuildMapLights(actors, &stats, &verifiedOrigin);
        ALOG(
            "DeusExQuest: prepared %zu map lights (%zu spotlights, %zu colored)",
            stats.total,
            stats.spotlights,
            stats.colored);
        if (stats.invalidLocations != 0u) {
            ALOG("DeusExQuest: skipped %zu map lights with non-finite locations",
                 stats.invalidLocations);
        }
        return lights;
    }

    OVR::Vector3f CalculateMapLighting(
        const OVR::Vector3f& position,
        const OVR::Vector3f& normal) const {
        return QuestVr::CalculateMapLighting(activeMapLights_, position, normal);
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
        DestroyActorGeometry();
        actorGeometryBuild_ = std::make_unique<ActorGeometryBuild>();
        ActorGeometryBuild& build = *actorGeometryBuild_;
        constexpr float unitsToMeters = 1.0f / 52.5f;
        float& originX = build.originX;
        float& originY = build.originY;
        float& originZ = build.originZ;
        originX = activeMapUnrealOrigin_.x; originY = activeMapUnrealOrigin_.y; originZ = activeMapUnrealOrigin_.z;
        const auto& playerStartPath = activeMapPlayerStartPath_;
        ALOG(
            "DeusExQuest: actor coordinate origin %s at %.3f,%.3f,%.3f",
            playerStartPath.c_str(),
            originX,
            originY,
            originZ);
        const OVR::Vector3f playerLocal = StageToLocal(currentHeadStage_, worldPosition_);
        actorStreamingCenter_ = playerLocal;
        build.spriteDescriptor = BuildCrossedSpriteDescriptor();
        build.cubeDescriptor = OVRFW::BuildUnitCubeDescriptor();
        for (std::size_t layer = 0; layer < actorTexturePaths_.size(); ++layer) {
            build.textureLayers.emplace(actorTexturePaths_[layer], layer);
        }
        // Targeting/travel metadata is available immediately, independently of
        // delayed visual chunks. Preserve the exact previous ordering/filter.
        for (std::size_t index = 0u; index < actorSnapshots_.size(); ++index) {
            const PortableActorSnapshot& actor = actorSnapshots_[index];
            // Serialized editor cameras are not gameplay characters or targets.
            if (actor.classPath == "Engine.Camera") continue;
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
            build.actorIndices.push_back(index);
            interactiveActors_.push_back({position, actor.objectPath, actor.classPath,
                                          actor.travel, actor.destinationMap});
            if (build.actorIndices.size() >= 512u) break;
        }
        ALOG("DeusExQuest: queued incremental geometry for %zu targetable actors; budget %.1f ms/%zu operations/%zu vertices",
             build.actorIndices.size(), actorGeometryMillisecondsPerFrame_,
             actorGeometryOperationsPerFrame_, actorGeometryChunkVertices_);
    }

    void QueueActorGeometryPart(
        const OVRFW::GlGeometry::Descriptor& descriptor, const bool textured,
        const OVR::Vector4f& color, const OVR::Matrix4f& transform,
        const bool sequential = false) {
        const std::size_t count = sequential
            ? descriptor.attribs.position.size() : descriptor.indices.size();
        if (count == 0u) return;
        if (count % 3u != 0u) throw std::runtime_error("actor geometry is not a triangle list");
        const OVR::Matrix3f normalTransform(transform);
        if (!std::isfinite(normalTransform.Determinant()) ||
            std::fabs(normalTransform.Determinant()) < 0.00000001f)
            throw std::runtime_error("actor geometry transform is singular or non-finite");
        actorGeometryBuild_->parts.push_back({&descriptor, nullptr, transform,
            normalTransform.Inverse().Transposed(), color, 0u, textured, sequential, 0u});
    }

    void QueueActorMeshPart(
        const PortableLodMesh& mesh, const std::uint16_t material,
        const OVR::Vector4f& color, const QuestVr::ActorTransform& transform,
        const std::uint32_t polyFlags,
        std::shared_ptr<const QuestVr::MeshPose> meshPose = {}) {
        if (mesh.triangles.empty()) return;
        if (mesh.triangles.size() % 3u != 0u)
            throw std::runtime_error("actor mesh is not a triangle list");
        ActorGeometryPart part;
        part.mesh = &mesh; part.color = color; part.textured = true;
        part.sequential = true; part.material = material;
        part.actorTransform = transform; part.polyFlags = polyFlags;
        part.meshPose = std::move(meshPose);
        actorGeometryBuild_->parts.push_back(std::move(part));
    }

    void ReapRetiredActorPose() {
        if (!actorPoseFuture_.valid()) return;
        // The sole outstanding worker can belong to the current build or to a
        // cancelled one. Never consume a current result before its actor does.
        if (actorGeometryBuild_ && actorGeometryBuild_->pendingMeshPose &&
            actorPoseEpoch_.IsCurrent(actorPoseJobEpoch_) &&
            actorPoseActorOrdinal_ == actorGeometryBuild_->nextActor) return;
        if (actorPoseFuture_.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return;
        try {
            actorPoseFuture_.get();
            ALOG("DeusExQuest: discarded actor pose prepared for an abandoned map/save/geometry build");
        } catch (const std::exception& error) {
            ALOG("DeusExQuest: retired actor pose worker failed: %s",error.what());
        }
    }

    bool PrepareActorMeshPose(
        const std::shared_ptr<const PortableMeshAnimationData>& animation,
        const QuestVr::MeshAnimationState& state,
        std::shared_ptr<const QuestVr::MeshPose>& meshPose) {
        ActorGeometryBuild& build = *actorGeometryBuild_;
        if (build.pendingMeshPose) {
            if (!actorPoseFuture_.valid() || !actorPoseEpoch_.IsCurrent(actorPoseJobEpoch_) ||
                actorPoseActorOrdinal_ != build.nextActor)
                throw std::runtime_error("Actor pose worker no longer belongs to its geometry build");
            if (actorPoseFuture_.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return false;
            build.pendingMeshPose = false;
            try {
                meshPose = actorPoseFuture_.get();
            } catch (const std::exception& error) {
                auto failed = std::make_shared<QuestVr::MeshPose>();
                failed->error = error.what();
                meshPose = std::move(failed);
            }
            return true;
        }
        ReapRetiredActorPose();
        // A cancelled worker owns immutable asset data until it finishes. Keep
        // its future alive rather than blocking in std::async's destructor or
        // starting unbounded workers during repeated interaction/quickloads.
        if (actorPoseFuture_.valid()) return false;
        actorPoseJobEpoch_ = actorPoseEpoch_.Capture();
        actorPoseActorOrdinal_ = build.nextActor;
        try {
            actorPoseFuture_ = std::async(std::launch::async,
                [animation, state]() -> std::shared_ptr<const QuestVr::MeshPose> {
                    // No app, portable runtime, NameString global storage or GL
                    // access. Full animation arrays are shared, never copied.
                    return std::make_shared<QuestVr::MeshPose>(QuestVr::PrepareMeshPose(*animation,state));
                });
            build.pendingMeshPose = true;
            return false;
        } catch (const std::exception& error) {
            auto failed = std::make_shared<QuestVr::MeshPose>();
            failed->error = std::string("Could not start actor pose worker: ")+error.what();
            meshPose = std::move(failed);
            return true;
        }
    }

    // False means the same actor is still waiting for its one background pose;
    // the caller must yield this frame without advancing the actor cursor.
    bool PrepareNextActorGeometry() {
        ActorGeometryBuild& build = *actorGeometryBuild_;
        const std::size_t ordinal = build.nextActor;
        const auto& actor = actorSnapshots_[build.actorIndices[ordinal]];
        const OVR::Vector3f position = interactiveActors_[ordinal].localPosition;
        if (actor.hidden || actor.drawType == 0u) { ++build.hiddenActors; return true; }
        if (!actor.mover && (actor.drawScale == 0.0f || actor.drawScaleX == 0.0f ||
            actor.drawScaleY == 0.0f || actor.drawScaleZ == 0.0f)) return true;
        const auto light = actor.unlit ? OVR::Vector3f(1.0f) :
            CalculateMapLighting(position,{0.0f,1.0f,0.0f});
        bool rendered = false;
        const bool brushActor = actor.mover && !actor.activated && !actor.brushPath.empty();
        const std::string assetPath = brushActor ? actor.brushPath : actor.meshPath;
        if (!assetPath.empty()) {
            try {
                auto& cache = brushActor ? build.brushes : build.meshes;
                auto found = cache.find(assetPath);
                if (found == cache.end()) found = cache.emplace(assetPath,brushActor ?
                    GetPortableRuntimeBrush(assetPath) : GetPortableRuntimeMesh(assetPath)).first;
                const auto& mesh = found->second;
                std::shared_ptr<const QuestVr::MeshPose> meshPose;
                if (!brushActor && mesh.animation) {
                    if (!PrepareActorMeshPose(mesh.animation,
                        QuestVr::BuildSnapshotMeshAnimationState(actor),meshPose)) return false;
                    if (!meshPose || !meshPose->drawable) {
                        ++build.animationOmissions;
                        ALOG("DeusExQuest: actor pose omitted %s sequence=%s frame=%.6f: %s",
                            actor.objectPath.c_str(),actor.animation.sequence.c_str(),
                            actor.animation.frame,meshPose ? meshPose->error.c_str() : "Worker returned no pose");
                        return true; // An omitted/invalid pose must not turn into a cube.
                    }
                    ++build.sampledActorPoses;
                }
                const auto transform = QuestVr::BuildSnapshotActorTransform(actor,
                    {build.originX,build.originY,build.originZ},brushActor);
                std::set<std::pair<std::uint16_t,std::uint32_t>> materials;
                for (const auto& vertex : mesh.triangles) materials.emplace(vertex.material,vertex.polyFlags);
                for (const auto& [material,sourceFlags] : materials) {
                    const auto selected = QuestVr::ResolveActorMeshMaterial(actor.materialOverrides,
                        mesh.texturePaths,mesh.materialTextureIndices,material);
                    const auto layer = build.textureLayers.find(selected.texturePath);
                    if (layer == build.textureLayers.end()) continue;
                    const auto textureFlags = layer->second < actorTexturePolyFlags_.size() ?
                        actorTexturePolyFlags_[layer->second] : 0u;
                    const auto flags = sourceFlags | QuestVr::ActorMaterialPolyFlags(actor.style,
                        actor.unlit,actor.noSmooth,actor.meshEnvironmentMap) | (textureFlags & 2u);
                    const auto maskedLayer = layer->second < actorMaskedTextureLayers_.size() ?
                        actorMaskedTextureLayers_[layer->second] : -1;
                    const auto drawLayer = (flags & 2u) != 0u && maskedLayer >= 0 ?
                        static_cast<std::size_t>(maskedLayer) : layer->second;
                    if ((flags & 1u) != 0u) continue;
                    const auto gain = (flags & (0x00400000u|64u)) != 0u ? OVR::Vector3f(1.0f) : light;
                    QueueActorMeshPart(mesh,material,
                        {static_cast<float>(drawLayer)/255.0f,gain.x,gain.y,gain.z},
                        transform,flags,meshPose);
                    // Store source flags separately: texture/style bits are shared
                    // across this material but do not identify its source faces.
                    build.parts.back().sourcePolyFlags = sourceFlags;
                    rendered = true;
                }
                if (rendered) { if (brushActor) ++build.brushInstances; else ++build.meshInstances; }
            } catch (const std::exception& error) {
                if (build.pendingMeshPose) {
                    // Defensive recovery: an asset-preparation failure must not
                    // advance past an actor while leaving its pending flag set.
                    actorPoseEpoch_.Invalidate(actorPoseJobEpoch_,actorPoseFuture_.valid());
                    build.pendingMeshPose = false;
                    ReapRetiredActorPose();
                }
                ALOG("DeusExQuest: actor asset %s failed: %s",assetPath.c_str(),error.what());
            }
        }
        const bool sprite = actor.drawType == 1u || actor.drawType == 4u ||
            actor.drawType == 5u || actor.drawType == 7u;
        if (!rendered && sprite && !actor.texturePath.empty()) {
            const auto layer = build.textureLayers.find(actor.texturePath);
            if (layer != build.textureLayers.end()) {
                const float scale = (actor.inventory ? 0.35f : 0.65f)*actor.drawScale;
                const auto textureFlags = layer->second < actorTexturePolyFlags_.size() ? actorTexturePolyFlags_[layer->second] : 0u;
                const auto maskedLayer = layer->second < actorMaskedTextureLayers_.size() ? actorMaskedTextureLayers_[layer->second] : -1;
                const auto drawLayer = ((textureFlags & 2u) != 0u || actor.style == 2u) && maskedLayer >= 0 ?
                    static_cast<std::size_t>(maskedLayer) : layer->second;
                QueueActorGeometryPart(build.spriteDescriptor,true,
                    {static_cast<float>(drawLayer)/255.0f,light.x,light.y,light.z},
                    OVR::Matrix4f::Translation(position)*OVR::Matrix4f::Scaling(scale,scale,scale));
                build.parts.back().polyFlags = QuestVr::ActorMaterialPolyFlags(actor.style,
                    actor.unlit,actor.noSmooth,actor.meshEnvironmentMap) | 0x100u |
                    (layer->second < actorTexturePolyFlags_.size() ? actorTexturePolyFlags_[layer->second]&2u : 0u);
                rendered = true; ++build.spriteInstances;
            }
        }
        if (!rendered && (actor.pawn || actor.inventory || actor.decoration)) {
            const OVR::Vector3f scale = actor.pawn ? OVR::Vector3f{0.32f,1.65f,0.32f} :
                actor.inventory ? OVR::Vector3f{0.18f,0.18f,0.18f} : OVR::Vector3f{0.35f,0.35f,0.35f};
            QueueActorGeometryPart(build.cubeDescriptor,false,{0.5f,0.32f,0.15f,1.0f},
                OVR::Matrix4f::Translation(position)*OVR::Matrix4f::Scaling(scale));
            ++build.cubePlaceholders; ++build.cubeClasses[actor.classPath];
        }
        return true;
    }

    void LogActorGeometryBuild() const {
        const ActorGeometryBuild& build = *actorGeometryBuild_;
        ALOG(
            "DeusExQuest: instantiated %zu targetable actors from %zu live actors (%zu vertex meshes, %zu mover brushes, %zu sprites, %zu cube placeholders, %zu hidden, %zu mesh-bearing, %zu mesh formats, %zu map exits)",
            build.actorIndices.size(),
            actorSnapshots_.size(),
            build.meshInstances,
            build.brushInstances,
            build.spriteInstances,
            build.cubePlaceholders,
            build.hiddenActors,
            std::accumulate(
                build.meshClasses.begin(), build.meshClasses.end(), std::size_t{},
                [](std::size_t total, const auto& value) { return total + value.second; }),
            build.meshClasses.size(),
            static_cast<std::size_t>(std::count_if(
                interactiveActors_.begin(), interactiveActors_.end(),
                [](const InteractiveActor& actor) {
                    return actor.travel && !actor.destinationMap.empty();
                })));
        for (const auto& meshClass : build.meshClasses) {
            ALOG(
                "DeusExQuest: actor mesh format %s count=%zu",
                meshClass.first.c_str(),
                meshClass.second);
        }
        for (const auto& cubeClass : build.cubeClasses) {
            ALOG(
                "DeusExQuest: cube placeholder class %s count=%zu",
                cubeClass.first.c_str(),
                cubeClass.second);
        }
        ALOG("DeusExQuest: incremental actor geometry complete over %zu frames, %zu GPU chunks; worst slice %.2f ms",
             build.frames, build.uploadedChunks, build.maximumSliceMilliseconds);
        ALOG("DeusExQuest: sampled %zu authored actor poses; %zu omitted poses; native animation clock/state events not implemented",
             build.sampledActorPoses,build.animationOmissions);
    }

    void AppendActorGeometryPart(ActorGeometryPart& part, const std::size_t count) {
        OVRFW::GlGeometry::Descriptor& chunk = part.textured
            ? actorGeometryBuild_->texturedChunk : actorGeometryBuild_->markerChunk;
        const auto appendVertex = [&](const OVR::Vector3f& position,
                                      const OVR::Vector3f& normal,
                                      const OVR::Vector2f& uv) {
            chunk.attribs.position.push_back(part.transform.Transform(position));
            const OVR::Vector3f transformedNormal = part.normalTransform.Transform(normal);
            chunk.attribs.normal.push_back(transformedNormal.LengthSq() > 0.000001f
                ? transformedNormal.Normalized() : OVR::Vector3f(0.0f));
            chunk.attribs.uv0.push_back(uv);
            chunk.attribs.color.push_back(part.color);
            chunk.indices.push_back(static_cast<OVRFW::TriangleIndex>(chunk.indices.size()));
        };
        const std::size_t end = part.nextIndex + count;
        if (part.mesh != nullptr) {
            for (std::size_t triangle = part.nextIndex; triangle < end; triangle += 3u) {
                const auto& source = part.mesh->triangles[triangle];
                if (source.material != part.material || source.polyFlags != part.sourcePolyFlags) continue;
                for (const auto& vertex : QuestVr::BuildActorTriangle(*part.mesh,triangle,
                    part.actorTransform,part.meshPose.get())) {
                    chunk.attribs.position.emplace_back(vertex.position.x,vertex.position.y,vertex.position.z);
                    chunk.attribs.normal.emplace_back(vertex.normal.x,vertex.normal.y,vertex.normal.z);
                    chunk.attribs.uv0.emplace_back(vertex.u,vertex.v);
                    chunk.attribs.color.push_back(part.color);
                    chunk.indices.push_back(static_cast<OVRFW::TriangleIndex>(chunk.indices.size()));
                }
            }
        } else {
            const auto& source = *part.descriptor;
            for (std::size_t index = part.nextIndex; index < end; ++index) {
                const std::size_t vertex = part.sequential ? index : source.indices[index];
                if (vertex >= source.attribs.position.size())
                    throw std::runtime_error("actor descriptor index is out of range");
                appendVertex(source.attribs.position[vertex],
                    vertex < source.attribs.normal.size()
                        ? source.attribs.normal[vertex] : OVR::Vector3f(0.0f),
                    vertex < source.attribs.uv0.size()
                        ? source.attribs.uv0[vertex] : OVR::Vector2f(0.0f));
            }
        }
        part.nextIndex = end;
    }

    void UploadActorGeometryChunk(const bool textured) {
        ActorGeometryBuild& build = *actorGeometryBuild_;
        OVRFW::GlGeometry::Descriptor& chunk = textured ? build.texturedChunk : build.markerChunk;
        const OVR::Posef pose(
            OVR::Quatf(OVR::Vector3f(0.0f, 1.0f, 0.0f), sceneYaw_), worldPosition_);
        if (textured) {
            if (!actorTexture_.IsValid())
                throw std::runtime_error("actor texture disappeared during geometry upload");
            if (actorTexturedRendererIndex_ == invalidRendererIndex_)
                actorTexturedRendererIndex_ = texturedRenderers_.size();
            texturedRenderers_.emplace_back();
            texturedRenderers_.back().Init(chunk, actorTexture_,
                (build.texturedChunkPolyFlags & 0x100u) == 0u,build.texturedChunkPolyFlags);
            texturedRenderers_.back().SetPose(pose);
            texturedRenderers_.back().Update();
        } else {
            if (actorWorldRendererIndex_ == invalidRendererIndex_)
                actorWorldRendererIndex_ = worldRenderers_.size();
            worldRenderers_.emplace_back();
            worldRenderers_.back().Init(chunk);
            worldRenderers_.back().AmbientLightColor = {0.45f, 0.45f, 0.45f};
            worldRenderers_.back().SetPose(pose);
            worldRenderers_.back().Update();
        }
        if (glGetError() != GL_NO_ERROR)
            throw std::runtime_error("GPU actor geometry chunk upload failed");
        ++build.uploadedChunks;
        // Retain bounded CPU capacity across uploads, avoiding repeated growth
        // allocation on each subsequent actor chunk.
        chunk.attribs.position.clear();
        chunk.attribs.normal.clear();
        chunk.attribs.uv0.clear();
        chunk.attribs.color.clear();
        chunk.indices.clear();
    }

    bool AdvanceActorGeometry() {
        const auto started = std::chrono::steady_clock::now();
        ReapRetiredActorPose();
        if (!actorGeometryBuild_) return actorGeometryComplete_;
        ActorGeometryBuild& build = *actorGeometryBuild_;
        const auto elapsed = [&] {
            return std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - started).count();
        };
        QuestVr::FrameWorkBudget budget(actorGeometryOperationsPerFrame_,
            actorGeometryChunkVertices_, actorGeometryMillisecondsPerFrame_);
        const bool allPrepared = build.nextActor == build.actorIndices.size() && build.parts.empty() && !build.pendingMeshPose;
        const bool uploadMarker = build.markerChunk.indices.size() >= actorGeometryChunkVertices_ ||
            (allPrepared && !build.markerChunk.indices.empty());
        const bool uploadTextured = build.texturedChunk.indices.size() >= actorGeometryChunkVertices_ ||
            (allPrepared && !build.texturedChunk.indices.empty()) ||
            (!build.texturedChunk.indices.empty() && !build.parts.empty() &&
                build.parts.front().textured && build.parts.front().polyFlags != build.texturedChunkPolyFlags);
        // Keep driver allocation/upload separate from preparation; never issue
        // more than one bounded 6144-vertex GPU upload in a frame.
        if (uploadMarker || uploadTextured) {
            UploadActorGeometryChunk(!uploadMarker);
        } else {
            while (!budget.ShouldYield(elapsed())) {
                if (!build.parts.empty()) {
                    ActorGeometryPart& part = build.parts.front();
                    const std::size_t total = part.mesh != nullptr
                        ? part.mesh->triangles.size()
                        : (part.sequential ? part.descriptor->attribs.position.size()
                                           : part.descriptor->indices.size());
                    auto& chunk = part.textured ? build.texturedChunk : build.markerChunk;
                    if (part.textured) {
                        if (!chunk.indices.empty() && build.texturedChunkPolyFlags != part.polyFlags) break;
                        build.texturedChunkPolyFlags = part.polyFlags;
                    }
                    if (part.nextIndex > total || total % 3u != 0u ||
                        part.nextIndex % 3u != 0u ||
                        chunk.indices.size() > actorGeometryChunkVertices_)
                        throw std::runtime_error("incremental actor geometry cursor is invalid");
                    const std::size_t count = QuestVr::TriangleWorkSlice(
                        total, part.nextIndex, chunk.indices.size(), budget.Vertices(),
                        actorGeometryChunkVertices_);
                    if (count == 0u || !budget.CanStart(count, elapsed())) break;
                    AppendActorGeometryPart(part, count);
                    budget.Consume(count);
                    if (part.nextIndex == total) build.parts.pop_front();
                    if (chunk.indices.size() >= actorGeometryChunkVertices_) break;
                } else if (build.nextActor < build.actorIndices.size()) {
                    if (!budget.CanStart(0u, elapsed())) break;
                    if (!PrepareNextActorGeometry()) break;
                    const auto& actor = actorSnapshots_[build.actorIndices[build.nextActor]];
                    if (!actor.meshPath.empty()) ++build.meshClasses[actor.meshClassPath];
                    ++build.nextActor;
                    budget.Consume(0u);
                } else {
                    break;
                }
            }
        }
        const double milliseconds = elapsed();
        ++build.frames;
        build.maximumSliceMilliseconds = std::max(build.maximumSliceMilliseconds, milliseconds);
        ALOG("DeusExQuest: actor geometry slice %.2f ms actors=%zu/%zu ops=%zu scannedVertices=%zu chunks=%zu",
             milliseconds, build.nextActor, build.actorIndices.size(), budget.Operations(),
             budget.Vertices(), build.uploadedChunks);
        if (build.nextActor == build.actorIndices.size() && build.parts.empty() && !build.pendingMeshPose &&
            build.markerChunk.indices.empty() && build.texturedChunk.indices.empty()) {
            LogActorGeometryBuild();
            actorGeometryBuild_.reset();
            actorGeometryComplete_ = true;
            return true;
        }
        return false;
    }

    void CancelActorGeometryBuild() {
        const auto started = std::chrono::steady_clock::now();
        const bool hadWork = static_cast<bool>(actorGeometryBuild_);
        // Invalidate without destroying the single outstanding future. Its
        // immutable-data-only worker may finish after this map/runtime is gone.
        actorPoseEpoch_.Invalidate(actorPoseJobEpoch_,actorPoseFuture_.valid());
        actorGeometryBuild_.reset();
        ReapRetiredActorPose();
        actorGeometryComplete_ = false;
        if (hadWork) {
            ALOG("DeusExQuest: cancelled pending actor geometry in %.2f ms",
                std::chrono::duration<double, std::milli>(
                    std::chrono::steady_clock::now() - started).count());
        }
    }

    void DestroyActorGeometry() {
        CancelActorGeometryBuild();
        while (actorTexturedRendererIndex_ != invalidRendererIndex_ &&
               texturedRenderers_.size() > actorTexturedRendererIndex_) {
            texturedRenderers_.back().Shutdown();
            texturedRenderers_.pop_back();
        }
        while (actorWorldRendererIndex_ != invalidRendererIndex_ &&
               worldRenderers_.size() > actorWorldRendererIndex_) {
            worldRenderers_.back().Shutdown();
            worldRenderers_.pop_back();
        }
        actorTexturedRendererIndex_ = invalidRendererIndex_;
        actorWorldRendererIndex_ = invalidRendererIndex_;
        interactiveActors_.clear();
    }

    void DestroySceneGeometry() {
        CancelActorGeometryBuild();
        for (auto& renderer : worldRenderers_) renderer.Shutdown();
        for (auto& renderer : texturedRenderers_) renderer.Shutdown();
        for (auto& renderer : bakedWorldRenderers_) renderer.Shutdown();
        worldRenderers_.clear();
        texturedRenderers_.clear();
        bakedWorldRenderers_.clear();
        actorWorldRendererIndex_ = invalidRendererIndex_;
        actorTexturedRendererIndex_ = invalidRendererIndex_;
        interactiveActors_.clear();
        actorSnapshots_.clear();
        if (firstTexture_.IsValid()) {
            OVRFW::FreeTexture(firstTexture_);
            firstTexture_ = {};
        }
        if (staticLightmapTexture_.IsValid()) {
            OVRFW::FreeTexture(staticLightmapTexture_);
            staticLightmapTexture_ = {};
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

    static std::vector<std::string> ReadMapCatalog() {
        std::vector<std::string> mapNames;
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
                    mapNames.push_back(std::move(map));
                }
            }
            std::fclose(file);
        }
        if (mapNames.empty()) {
            mapNames = {"00_Training", "00_TrainingCombat", "00_TrainingFinal"};
        }
        ALOG("DeusExQuest: visual map catalog ready with %zu levels", mapNames.size());
        return mapNames;
    }

    bool AdvanceMapTransition() {
        if (transitionPhase_ == MapTransitionPhase::Idle) return true;
        const auto started = std::chrono::steady_clock::now();
        try {
            if (transitionPhase_ == MapTransitionPhase::WorldTextureAllocate) {
                DestroySceneGeometry();
                currentMapName_ = transitionMapName_;
                activeMapLights_ = std::move(preparedMapLights_);
                activeMapUnrealOrigin_ = preparedWorldMesh_.lightmap.unrealOrigin;
                activeMapPlayerStartPath_ = preparedWorldMesh_.lightmap.playerStartPath;
                if (restorePoseAfterTransition_) {
                    if (restoredMapLocalPose_) {
                        QuestVr::RestoreSavedMapPose(restoredMapLocalFeet_, restoredMapLocalHeadYaw_,
                            currentHeadStage_, currentHeadStageYaw_, worldPosition_, sceneYaw_);
                    } else {
                        worldPosition_ = restoredWorldPosition_;
                        sceneYaw_ = restoredSceneYaw_;
                    }
                    restorePoseAfterTransition_ = false;
                } else {
                    worldPosition_ = {0.0f, 0.0f, 0.0f};
                    QuestVr::AnchorSpawnToHead(currentHeadStage_, worldPosition_);
                    sceneYaw_ = 0.0f;
                }
                transitionHeadLocal_ = StageToLocal(currentHeadStage_, worldPosition_);
                hasTransitionHeadAnchor_ = true;
                if (!BeginWorldTextureUpload(std::move(preparedWorldTexture_))) {
                    throw std::runtime_error("GPU world texture allocation failed");
                }
                transitionPhase_ = MapTransitionPhase::WorldTextureUpload;
            } else if (transitionPhase_ == MapTransitionPhase::WorldTextureUpload) {
                if (!UploadWorldTextureLayers(2u)) {
                    throw std::runtime_error("GPU world texture upload failed");
                }
                if (pendingWorldTextureLayersUploaded_ == pendingWorldTextureLayers_) {
                    transitionPhase_ = MapTransitionPhase::StaticLightmapAllocate;
                }
            } else if (transitionPhase_ == MapTransitionPhase::StaticLightmapAllocate) {
                if (!BeginStaticLightmapUpload(std::move(preparedWorldMesh_.lightmap)))
                    throw std::runtime_error("GPU static lightmap atlas allocation failed");
                transitionPhase_ = MapTransitionPhase::StaticLightmapUpload;
            } else if (transitionPhase_ == MapTransitionPhase::StaticLightmapUpload) {
                if (!UploadStaticLightmapRows())
                    throw std::runtime_error("GPU static lightmap atlas row upload failed");
                if (pendingStaticLightmapLayer_ == pendingStaticLightmapLayers_)
                    transitionPhase_ = MapTransitionPhase::WorldGeometry;
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
                    ALOG("DeusExQuest: original shadow-lightmapped BSP geometry upload complete (%zu batches)",
                         pendingWorldMeshChunk_);
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
                transitionPhase_ = MapTransitionPhase::ActorGeometryUpload;
            } else if (transitionPhase_ == MapTransitionPhase::ActorGeometryUpload) {
                if (!AdvanceActorGeometry()) return true;
                const auto found =
                    std::find(mapNames_.begin(), mapNames_.end(), transitionMapName_);
                if (found != mapNames_.end()) {
                    currentMapIndex_ = static_cast<std::size_t>(found - mapNames_.begin());
                }
                if (pendingPersonaRestore_) {
                    dialogueOffsets_ = std::move(restoredDialogueOffsets_);
                    personaLogEntries_ = std::move(restoredPersonaLogs_);
                    inventoryMenuDirty_ = true;
                    ClearPendingPersonaRestore();
                }
                ALOG(
                    "DeusExQuest: staged visual runtime transition complete: %s (%zu actors, %zu BSP collision triangles, health %.1f)",
                    transitionMapName_.c_str(),
                    actorSnapshots_.size(),
                    collisionTriangles_.size(),
                    GetPortableRuntimePlayerHealth());
                transitionMapName_.clear();
                transitionPhase_ = MapTransitionPhase::Idle;
                hasTransitionHeadAnchor_ = false;
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
            DestroyActorGeometry();
            ClearPendingPersonaRestore();
            transitionMapName_.clear();
            transitionPhase_ = MapTransitionPhase::Idle;
            hasTransitionHeadAnchor_ = false;
            if (pendingWorldTextureId_ != 0u) {
                glDeleteTextures(1, &pendingWorldTextureId_);
                pendingWorldTextureId_ = 0u;
            }
            pendingWorldTextureRgba_.clear();
            ClearPendingStaticLightmapUpload();
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
            preparedWorldMesh_ = {};
            preparedWorldTexture_ = {};
            preparedActorTextures_ = {};
            DestroySceneGeometry();
            runtimeAvailable_ = false;
            interactionStatus_ = "VISUAL MAP LOAD FAILED - PRESS B TO RETRY NEXT MAP";
            interactionStatusSeconds_ = 10.0f;
            restorePoseAfterTransition_ = false;
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
        if (!pendingMapName_.empty() || !transitionMapName_.empty() ||
            (runtimeAvailable_ && mapName == currentMapName_)) {
            return;
        }
        // A successful v4/v5 save does not authorize discarding the current map's
        // actor state. Check before cancelling UI/audio/geometry or starting a
        // replacement transaction, whose rollback needs the same archive.
        if (runtimeAvailable_ && GetPortableRuntimeScriptStatePresent()) {
            interactionStatus_ = "MAP CHANGE NEEDS SCRIPT STATE ARCHIVE";
            interactionStatusSeconds_ = 5.0f;
            ALOG("DeusExQuest: map change to %s kept current script state", mapName.c_str());
            return;
        }
        // Queued parts hold references to build-owned mesh copies. Cancel their
        // preparation before the worker mutates the source runtime; completed
        // old-map renderer chunks may remain visible during background loading.
        CancelActorGeometryBuild();
        ClearPendingPersonaRestore();
        ClearPendingConversation();
        InvalidateDialogueAudio();
        pendingMapName_ = mapName;
        displayedInventoryCount_ = invalidRendererIndex_;
        const std::uint32_t targetAudioRate = audioSampleRate_;
        const std::string priorMapName = currentMapName_;
        const bool hadUsableRuntime = runtimeAvailable_;
        try {
        mapCacheFuture_ = std::async(std::launch::async,
            [mapName, restoreRuntimePath, targetAudioRate, priorMapName, hadUsableRuntime]() {
            MapPreparation preparation;
            constexpr const char* checkpointPath =
                "/data/user/0/dev.deusex.questvr.smoketest/files/DeusEx/quest-transition-checkpoint.sav";
            const auto status = QuestVr::RunMapReplacementTransaction(hadUsableRuntime,
                [&] {
                    const auto started = std::chrono::steady_clock::now();
                    const bool saved = SavePortableRuntimeState(checkpointPath);
                    ALOG("DeusExQuest: private transition checkpoint %s in %.2f ms (worker)",
                         saved ? "saved" : "failed",
                         std::chrono::duration<double, std::milli>(
                             std::chrono::steady_clock::now() - started).count());
                    return saved;
                }, [&](bool& mutationStarted) {
                if (!BuildQuestMapCache(gameRoot_, mapName.c_str())) {
                    throw std::runtime_error("visual cache generation failed");
                }
                preparation.worldMesh = LoadWorldMeshCacheCpu();
                if (!preparation.worldMesh.passed) {
                    throw std::runtime_error("world mesh cache read failed");
                }
                preparation.worldTexture = LoadWorldTextureCacheCpu();
                if (!preparation.worldTexture.passed) {
                    throw std::runtime_error("world texture cache read failed");
                }
                const PortablePackageTables map = LoadPortablePackageTables(
                    std::string(gameRoot_) + "/Maps/" + mapName + ".dx");
                mutationStarted = true;
                const PortableMapRuntimeSummary runtime = LoadPortableRuntimeMap(map);
                // Prepare assets from the complete authored population before
                // saved inactivity hides pickups/pawns. An older same-map save
                // can reactivate them without another map-wide texture upload.
                const PortableActorMeshSummary meshes = DecodePortableRuntimeActorMeshes();
                if (!runtime.passed || !meshes.passed) {
                    throw std::runtime_error("runtime or actor mesh replacement failed");
                }
                preparation.actorTextures = BuildPortableRuntimeActorTextureArray(96, 96);
                if (!preparation.actorTextures.passed)
                    throw std::runtime_error("actor texture preparation failed");
                // Restore can hide an authored light/pawn/pickup. Build masks
                // against the complete authored actor list and preserve every
                // duplicate/inactive static-list ordinal before restoration.
                PrepareWorldStaticLightmaps(map,GetPortableRuntimeMapActors(),preparation.worldMesh);
                if (!restoreRuntimePath.empty() &&
                    !LoadPortableRuntimeState(restoreRuntimePath)) {
                    throw std::runtime_error("saved runtime restoration failed");
                }
                preparation.actors = GetPortableRuntimeMapActors();
                preparation.spatialAudioEmitters = PrepareSpatialAudioEmitters(
                    preparation.actors, targetAudioRate,preparation.worldMesh.lightmap.unrealOrigin);
                preparation.lights = BuildMapLights(preparation.actors,preparation.worldMesh.lightmap.unrealOrigin);
                return true;
                }, [&] {
                    const PortablePackageTables previousMap = LoadPortablePackageTables(
                        std::string(gameRoot_) + "/Maps/" + priorMapName + ".dx");
                    const bool loaded = LoadPortableRuntimeMap(previousMap).passed;
                    const bool meshes = loaded && DecodePortableRuntimeActorMeshes().passed;
                    const bool restored = meshes && LoadPortableRuntimeState(checkpointPath);
                    ALOG("DeusExQuest: prior-map rollback %s for %s",
                         restored ? "restored" : "failed", priorMapName.c_str());
                    return restored;
                });
            preparation.passed = status.prepared;
            preparation.runtimeAvailable = status.runtimeAvailable;
            preparation.rollbackAttempted = status.rollbackAttempted;
            preparation.error = status.error;
            return preparation;
        });
        } catch (const std::exception& error) {
            ALOG("DeusExQuest: map worker could not start for %s: %s", mapName.c_str(), error.what());
            pendingMapName_.clear();
            restorePoseAfterTransition_ = false;
            if (runtimeAvailable_) BuildActorMarkers();
            interactionStatus_ = "MAP WORKER COULD NOT START";
            interactionStatusSeconds_ = 5.0f;
            return;
        }
        ALOG("DeusExQuest: background visual cache started for %s", mapName.c_str());
    }

    void CompletePendingMapLoad() {
        if (pendingMapName_.empty() || !mapCacheFuture_.valid() ||
            mapCacheFuture_.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
            return;
        }
        const std::string mapName = pendingMapName_;
        pendingMapName_.clear();
        MapPreparation preparation;
        try {
            preparation = mapCacheFuture_.get();
        } catch (const std::exception& error) {
            ALOG("DeusExQuest: map worker result failed for %s: %s", mapName.c_str(), error.what());
            ClearPendingPersonaRestore();
            runtimeAvailable_ = false;
            restorePoseAfterTransition_ = false;
            DestroySceneGeometry();
            interactionStatus_ = "MAP WORKER FAILED - PRESS B TO RETRY NEXT MAP";
            interactionStatusSeconds_ = 10.0f;
            displayedInventoryCount_ = invalidRendererIndex_;
            return;
        }
        runtimeAvailable_ = preparation.runtimeAvailable;
        if (!preparation.passed) {
            ClearPendingPersonaRestore();
            ALOG(
                "DeusExQuest: background transition preparation failed for %s: %s",
                mapName.c_str(),
                preparation.error.c_str());
            if (runtimeAvailable_) {
                actorSnapshots_ = GetPortableRuntimeMapActors();
                BuildActorMarkers();
                interactionStatus_ = preparation.rollbackAttempted
                    ? "MAP LOAD FAILED - PRIOR MAP RESTORED" : "MAP LOAD CANCELLED";
            } else {
                // Neither the prior map nor the partial replacement is safe to
                // query. Clear mismatched visuals and suspend gameplay/runtime
                // diagnostics until a subsequent map preparation succeeds.
                DestroySceneGeometry();
                interactionStatus_ = "MAP LOAD FAILED - PRESS B TO RETRY NEXT MAP";
            }
            interactionStatusSeconds_ = 10.0f;
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
        // Leave the single-slot request intact while a worker replaces the
        // runtime or a staged upload still owns it. Consumption must not make
        // gameplay diagnostics race with a transition.
        if (!pendingMapName_.empty() || !transitionMapName_.empty()) return;
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
        if (std::strcmp(requested, "TURNLEFT") == 0 ||
            std::strcmp(requested, "TURNRIGHT") == 0) {
            constexpr float snapRadians = 3.14159265358979323846f / 6.0f;
            const float direction =
                std::strcmp(requested, "TURNLEFT") == 0 ? -1.0f : 1.0f;
            SnapTurnAroundHead(direction * snapRadians, currentHeadStage_);
            ALOG(
                "DeusExQuest: diagnostic snap turn %s around head",
                direction < 0.0f ? "left" : "right");
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
        if (!runtimeAvailable_) {
            const auto retry = std::find(mapNames_.begin(), mapNames_.end(), requested);
            if (retry != mapNames_.end()) BeginMapLoad(*retry);
            else ALOG("DeusExQuest: rejected runtime diagnostic %s while map is unavailable", requested);
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

    void ClearPendingConversation() {
        pendingChoices_.clear();
        pendingChoiceActor_.clear();
        pendingChoiceAudioPackage_.clear();
        pendingChoiceIndex_ = 0u;
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
        ClearPendingConversation();
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

    static PersonaPreparation PrepareOriginalPersonaBackground() {
        PersonaPreparation preparation;
        try {
            const PortablePackageTables uiPackage = LoadPortablePackageTables(
                std::string(gameRoot_) + "/System/DeusExUI.u");
            const auto decode = [&](const std::string& name) {
                try {
                    return DecodePortableIndexedTexture(uiPackage, "UserInterface." + name, true);
                } catch (const std::exception&) {
                    return DecodePortableIndexedTexture(uiPackage, name, true);
                }
            };
            static_assert(static_cast<std::size_t>(PersonaPage::Count) ==
                          QuestVr::kPersonaPageLayouts.size());
            for (std::size_t page = 0u; page < QuestVr::kPersonaPageLayouts.size(); ++page) {
                try {
                    const auto& layout = QuestVr::kPersonaPageLayouts[page];
                    std::array<PortableTextureImage, 6> pieces;
                    std::array<PortableTextureImage, 6> borders;
                    for (std::size_t index = 0u; index < layout.backgroundCount; ++index) {
                        pieces[index] = decode(std::string(layout.backgroundPrefix) +
                                               std::to_string(index + 1u));
                    }
                    for (std::size_t index = 0u; index < borders.size(); ++index) {
                        borders[index] = decode(std::string(layout.borderPrefix) +
                                                std::to_string(index + 1u));
                    }
                    QuestVr::PersonaUiImage canvas = QuestVr::BuildPersonaUiCanvas(
                        pieces, borders, layout);
                    if (page == static_cast<std::size_t>(PersonaPage::Health)) {
                        const std::array<PortableTextureImage, 2> body{{
                            decode("HealthBody_1"), decode("HealthBody_2")}};
                        const std::array<PortableTextureImage, 2> overlays{{
                            decode("HealthOverlays_1"), decode("HealthOverlays_2")}};
                        QuestVr::AddPersonaHealthBody(canvas, body, overlays);
                    }
                    preparation.pageBaseRgba[page] = std::move(canvas.rgba);
                    ALOG("DeusExQuest: original Persona page %zu artwork ready at 640x480", page);
                } catch (const std::exception& error) {
                    if (page == static_cast<std::size_t>(PersonaPage::Inventory)) throw;
                    // Never reuse inventory artwork for an unrelated page.
                    preparation.pageBaseRgba[page].assign(
                        QuestVr::kPersonaCanvasWidth * QuestVr::kPersonaCanvasHeight * 4u, 0u);
                    ALOG("DeusExQuest: Persona page %zu artwork unavailable; text-only: %s",
                         page, error.what());
                }
            }
            try {
                const auto uiImage = [&](const std::string& name) {
                    auto image = decode(name);
                    return QuestVr::PersonaUiImage{
                        image.width, image.height, std::move(image.rgba)};
                };
                for (std::size_t index = 0u; index < 3u; ++index) {
                    preparation.chrome.navigationBackgrounds[index] = uiImage(
                        "PersonaNavBarBackground_" + std::to_string(index + 1u));
                    preparation.chrome.navigationBorders[index] = uiImage(
                        "PersonaNavBarBorder_" + std::to_string(index + 1u));
                }
                preparation.chrome.normalButton = {{
                    uiImage("PersonaActionButtonNormal_Left"),
                    uiImage("PersonaActionButtonNormal_Center"),
                    uiImage("PersonaActionButtonNormal_Right")}};
                preparation.chrome.filler = uiImage("PersonaButtonFiller");
                preparation.headerFont = DecodePortableBitmapFont(uiPackage, "FontMenuHeaders");
                preparation.bodyFont = DecodePortableBitmapFont(uiPackage, "FontMenuSmall");
                // Validate all chrome and captions before hiding fallback text.
                QuestVr::PersonaUiImage validation{
                    QuestVr::kPersonaCanvasWidth, QuestVr::kPersonaCanvasHeight,
                    preparation.pageBaseRgba[0]};
                const auto glyph = [&](const std::uint32_t character) {
                    return GetPortableBitmapGlyph(preparation.headerFont, character);
                };
                QuestVr::DrawPersonaNavigation(validation, preparation.chrome, preparation.headerFont,
                    QuestVr::PersonaUiPage::Inventory, glyph);
                QuestVr::DrawPersonaVrActions(validation, preparation.chrome, preparation.headerFont,
                    QuestVr::PersonaUiPage::Inventory, glyph);
                preparation.originalTextReady = true;
                ALOG("DeusExQuest: original Persona bitmap fonts and button chrome ready; headers=%zu body=%zu glyphs",
                    preparation.headerFont.glyphs.size(), preparation.bodyFont.glyphs.size());
            } catch (const std::exception& error) {
                preparation.originalTextReady = false;
                ALOG("DeusExQuest: original Persona font/chrome unavailable; SDK text fallback: %s",
                    error.what());
            }
            preparation.uiPackage = uiPackage;
            preparation.passed = true;
        } catch (const std::exception& error) {
            ALOG("DeusExQuest: original Persona CPU preparation unavailable; text-only fallback: %s",
                 error.what());
        }
        return preparation;
    }

    void CommitOriginalPersonaBackground(PersonaPreparation preparation) {
        if (inventoryLabel_ == nullptr || !preparation.passed) return;
        personaOriginalTextReady_ = preparation.originalTextReady;
        personaPageBaseRgba_ = std::move(preparation.pageBaseRgba);
        personaChrome_ = std::move(preparation.chrome);
        personaHeaderFont_ = std::move(preparation.headerFont);
        personaBodyFont_ = std::move(preparation.bodyFont);
        personaUiPackage_ = std::move(preparation.uiPackage);
        try {
            const std::uint32_t width = QuestVr::kPersonaCanvasWidth;
            const std::uint32_t height = QuestVr::kPersonaCanvasHeight;
            const auto& rgba = personaPageBaseRgba_[static_cast<std::size_t>(PersonaPage::Inventory)];
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
            personaTextureId_ = texture;
            personaTextureWidth_ = width;
            personaTextureHeight_ = height;
            ALOG(
                "DeusExQuest: original Persona inventory background active at %ux%u",
                width, height);
        } catch (const std::exception& error) {
            personaOriginalTextReady_ = false;
            ALOG(
                "DeusExQuest: original Persona background unavailable; using text-only fallback: %s",
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

    void RefreshPersonaInventoryArtwork(const std::vector<std::string>& inventory,
        const std::string& left, const std::string& right) {
        const auto& base = personaPageBaseRgba_[static_cast<std::size_t>(personaPage_)];
        if (personaTextureId_ == 0u || base.empty()) return;
        QuestVr::PersonaUiImage canvas{
            personaTextureWidth_, personaTextureHeight_, base};
        if (personaPage_ == PersonaPage::Inventory) {
            QuestVr::DrawPersonaInventoryGrid(
                canvas, inventory.size(), selectedInventoryIndex_,
                [&](std::size_t index) { return GetPersonaIcon(inventory[index]); });
        }
        if (personaOriginalTextReady_) {
            const auto headerGlyph = [&](const std::uint32_t character) {
                return GetPortableBitmapGlyph(personaHeaderFont_, character);
            };
            const auto bodyGlyph = [&](const std::uint32_t character) {
                return GetPortableBitmapGlyph(personaBodyFont_, character);
            };
            const auto page = static_cast<QuestVr::PersonaUiPage>(personaPage_);
            QuestVr::DrawPersonaNavigation(canvas, personaChrome_, personaHeaderFont_, page, headerGlyph);
            QuestVr::DrawPersonaVrActions(canvas, personaChrome_, personaHeaderFont_, page, headerGlyph);
            QuestVr::DrawPersonaPageText(canvas, personaHeaderFont_, personaBodyFont_, page,
                left, right, headerGlyph, bodyGlyph);
        }
        glBindTexture(GL_TEXTURE_2D, personaTextureId_);
        glTexSubImage2D(
            GL_TEXTURE_2D, 0, 0, 0,
            static_cast<GLsizei>(personaTextureWidth_),
            static_cast<GLsizei>(personaTextureHeight_),
            GL_RGBA, GL_UNSIGNED_BYTE, canvas.rgba.data());
        const GLenum updateError = glGetError();
        glBindTexture(GL_TEXTURE_2D, 0);
        if (updateError != GL_NO_ERROR && personaOriginalTextReady_) {
            personaOriginalTextReady_ = false;
            for (OVRFW::VRMenuObject* label : PersonaLabels()) {
                if (label != nullptr) label->SetVisible(inventoryMenuOpen_);
            }
            ALOG("DeusExQuest: Persona canvas upload failed; restored SDK text fallback");
        }
        const std::size_t diagnosticPixel =
            (static_cast<std::size_t>(QuestVr::kPersonaGridY) * personaTextureWidth_ +
             QuestVr::kPersonaGridX) * 4u;
        ALOG(
            "DeusExQuest: Persona grid upload GL=0x%x pixel=%u,%u,%u iconCache=%zu",
            updateError,
            static_cast<unsigned>(canvas.rgba[diagnosticPixel]),
            static_cast<unsigned>(canvas.rgba[diagnosticPixel + 1u]),
            static_cast<unsigned>(canvas.rgba[diagnosticPixel + 2u]),
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

    std::array<OVRFW::VRMenuObject*, 4> PersonaLabels() const {
        return {inventoryLabel_, personaTabsLabel_, personaDetailsLabel_, personaFooterLabel_};
    }

    static std::string WrapPersonaEntry(std::string value, const std::size_t maxLines = 2u,
                                       const std::size_t columns = 32u) {
        for (char& character : value) {
            if (character == '\n' || character == '\r' || character == '\t') character = ' ';
        }
        if (value.empty()) value = "UNNAMED ENTRY";
        std::string result;
        for (std::size_t line = 0u; line < maxLines && !value.empty(); ++line) {
            if (!result.empty()) result += '\n';
            if (value.size() <= columns) {
                result += value;
                break;
            }
            if (line + 1u == maxLines) {
                result += value.substr(0u, columns - 3u) + "...";
                break;
            }
            std::size_t split = value.find_last_of(' ', columns);
            if (split == std::string::npos || split == 0u) split = columns;
            result += value.substr(0u, split);
            value.erase(0u, split);
            const std::size_t first = value.find_first_not_of(' ');
            value.erase(0u, first == std::string::npos ? value.size() : first);
        }
        return result;
    }

    void SetPersonaPanelText(
        OVRFW::VRMenuObject* label, const std::string& text,
        const float x, const float y, const float width, const float height,
        const float nominalScale = 0.55f) {
        if (label == nullptr) return;
        label->SetText("%s", text.c_str());
        const OVRFW::BitmapFont& font = ui_.GetGuiSys().GetDefaultFont();
        std::size_t length{};
        float measuredWidth{}, measuredHeight{}, ascent{}, descent{}, fontHeight{};
        float lineWidths[16]{};
        int lines{};
        font.CalcTextMetrics(
            text.c_str(), length, measuredWidth, measuredHeight, ascent,
            descent, fontHeight, lineWidths, 16, lines);
        constexpr float pixelScale = 1.2f / 640.0f;
        const float fontScale = label->GetFontParms().Scale * label->GetLocalScale().x;
        float scale = nominalScale;
        if (measuredWidth * fontScale * scale > width * pixelScale) {
            scale = width * pixelScale / (measuredWidth * fontScale);
        }
        if (measuredHeight * fontScale * scale > height * pixelScale) {
            scale = height * pixelScale / (measuredHeight * fontScale);
        }
        label->SetTextLocalScale({scale, scale, 1.0f});
        // Specify the top of the first glyph, then convert to its baseline.
        // Ascent does not depend on how many following lines the page has.
        label->SetTextLocalPosition({
            -0.60f + x * pixelScale,
            0.45f - y * pixelScale - ascent * fontScale * scale,
            0.0f});
        ALOG("DeusExQuest: Persona text pane %.0f,%.0f %.0fx%.0f lines=%d scale=%.3f",
             x, y, width, height, lines, scale);
    }

    void SetInventoryMenuOpen(const bool open) {
        inventoryMenuOpen_ = open;
        inventoryMenuDirty_ = true;
        choiceCycleLatch_ = true;
        if (hudLabel_ != nullptr) hudLabel_->SetVisible(!open);
        for (OVRFW::VRMenuObject* label : PersonaLabels()) {
            if (label != nullptr) label->SetVisible(open && !personaOriginalTextReady_);
        }
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
        for (OVRFW::VRMenuObject* label : PersonaLabels()) {
            if (label != nullptr) label->SetLocalPose(menuPose);
        }
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

        const PortablePlayerProgress progress = GetPortableRuntimePlayerProgress();
        std::string tabs;
        switch (personaPage_) {
            case PersonaPage::Inventory:
                tabs = "[ INVENTORY ]   HEALTH   GOALS / NOTES   LOGS";
                break;
            case PersonaPage::Health:
                tabs = "INVENTORY   [ HEALTH ]   GOALS / NOTES   LOGS";
                break;
            case PersonaPage::GoalsNotes:
                tabs = "INVENTORY   HEALTH   [ GOALS / NOTES ]   LOGS";
                break;
            case PersonaPage::Logs:
                tabs = "INVENTORY   HEALTH   GOALS / NOTES   [ LOGS ]";
                break;
            case PersonaPage::Count:
                break;
        }
        SetPersonaPanelText(personaTabsLabel_, tabs, 9.0f, 7.0f, 622.0f, 20.0f);
        std::string left;
        std::string right;
        if (personaPage_ == PersonaPage::Inventory) {
            std::string item = "NO ITEM SELECTED";
            std::string type = "INVENTORY EMPTY";
            if (!inventory.empty()) {
                item = InventoryItemLabel(inventory[selectedInventoryIndex_]);
                type = InventoryItemType(inventory[selectedInventoryIndex_]);
            }
            left = "Inventory";
            right = "ITEM DATA\n\n" + WrapPersonaEntry(item) + "\n\n" +
                WrapPersonaEntry(type) + "\n\nSTATE READY\nA: EQUIP / USE";
        } else if (personaPage_ == PersonaPage::Health) {
            left = "Health";
            right = "CURRENT HEALTH\n" + std::to_string(static_cast<int>(health)) +
                " / 100\n\nCONDITION\n" + (health > 50.0f ? "NOMINAL" : "INJURED") +
                "\n\nCREDITS\n" + std::to_string(progress.credits) +
                "\n\nSKILL POINTS\n" + std::to_string(progress.skillPoints) +
                "\n\nINVENTORY ITEMS\n" + std::to_string(inventory.size());
        } else if (personaPage_ == PersonaPage::GoalsNotes) {
            left = "ACTIVE GOALS: " + std::to_string(progress.goals.size()) + "\n\n";
            for (std::size_t index = 0u; index < progress.goals.size() && index < 3u; ++index) {
                left += WrapPersonaEntry("G" + std::to_string(index + 1u) + " " +
                                         progress.goals[index]) + "\n\n";
            }
            if (progress.goals.empty()) left += "NO GOALS RECORDED";
            right = "NOTES: " + std::to_string(progress.notes.size()) + "\n\n";
            for (std::size_t index = 0u; index < progress.notes.size() && index < 5u; ++index) {
                right += WrapPersonaEntry("N" + std::to_string(index + 1u) + " " +
                                          progress.notes[index]) + "\n";
            }
            if (progress.notes.empty()) right += "NO NOTES RECORDED";
        } else {
            left = "CONVERSATION LOG\n\n";
            if (personaLogEntries_.empty()) {
                left += "NO CONVERSATIONS RECORDED";
            } else {
                const std::size_t first = personaLogEntries_.size() > 5u
                    ? personaLogEntries_.size() - 5u
                    : 0u;
                for (std::size_t index = first; index < personaLogEntries_.size(); ++index) {
                    left += WrapPersonaEntry(personaLogEntries_[index], 2u, 48u) + "\n";
                }
            }
        }
        RefreshPersonaInventoryArtwork(inventory, left, right);
        // Canvas coordinates from each original page's client/window defaults.
        // Keep health text clear of the original body graphic; Goals and Notes
        // are stacked panes, while Logs has one central scrolling column.
        switch (personaPage_) {
            case PersonaPage::Inventory:
                SetPersonaPanelText(inventoryLabel_, left, 42.0f,48.0f,266.0f,12.0f);
                SetPersonaPanelText(personaDetailsLabel_, right, 370.0f,60.0f,238.0f,218.0f);
                break;
            case PersonaPage::Health:
                SetPersonaPanelText(inventoryLabel_, left, 34.0f,42.0f,266.0f,12.0f);
                SetPersonaPanelText(personaDetailsLabel_, right, 373.0f,59.0f,238.0f,239.0f);
                break;
            case PersonaPage::GoalsNotes:
                SetPersonaPanelText(inventoryLabel_, left, 31.0f,60.0f,574.0f,154.0f);
                SetPersonaPanelText(personaDetailsLabel_, right, 31.0f,265.0f,574.0f,182.0f);
                break;
            case PersonaPage::Logs:
                SetPersonaPanelText(inventoryLabel_, left, 121.0f,68.0f,394.0f,361.0f);
                SetPersonaPanelText(personaDetailsLabel_, "", 121.0f,68.0f,394.0f,361.0f);
                break;
            case PersonaPage::Count:
                break;
        }
        std::string footer = personaPage_ == PersonaPage::Inventory
            ? "SLOT " + std::to_string(inventory.empty() ? 0u : selectedInventoryIndex_ + 1u) +
                " / " + std::to_string(inventory.size()) + "   UP / DOWN: SELECT   A: EQUIP / USE\n"
            : "Y: SAVE   X: LOAD\n";
        footer += "LEFT / RIGHT: PAGE   B / MENU: CLOSE";
        if (personaPage_ == PersonaPage::Inventory) {
            footer += "\n" + currentMapName_;
            SetPersonaPanelText(personaFooterLabel_, footer, 42.0f,391.0f,575.0f,70.0f);
        } else if (personaPage_ == PersonaPage::Health) {
            SetPersonaPanelText(personaFooterLabel_, footer, 38.0f,444.0f,572.0f,34.0f);
        } else if (personaPage_ == PersonaPage::GoalsNotes) {
            SetPersonaPanelText(personaFooterLabel_, footer, 25.0f,450.0f,590.0f,28.0f);
        } else {
            SetPersonaPanelText(personaFooterLabel_, footer, 115.0f,432.0f,395.0f,44.0f);
        }
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
        if (!runtimeAvailable_ || !headTrackingValid_ || !pendingChoices_.empty() ||
            !pendingMapName_.empty() || !transitionMapName_.empty()) return;
        bool saved = false;
        std::uint64_t generation{};
        try {
            const OVR::Vector3f localFeet = StageToLocal(
                {currentHeadStage_.x, 0.0f, currentHeadStage_.z}, worldPosition_);
            const QuestVr::QuestSaveMetadata metadata{
                {localFeet.x, localFeet.y, localFeet.z,
                    std::remainder(currentHeadStageYaw_ - sceneYaw_, 2.0f * QuestVr::Pi)},
                currentMapName_, dialogueOffsets_, personaLogEntries_, true};
            std::vector<std::uint8_t> encodedMetadata, runtime;
            const std::string prefix = QuickSavePrefix();
            const std::string capturePath = prefix + ".capture.runtime.tmp";
            // Only a scratch file is replaced while collecting state. Neither
            // committed slot nor the legacy two-file save is ever truncated.
            if (QuestVr::EncodeQuestSaveMetadata(metadata, encodedMetadata) &&
                QuestVr::WriteDurableSaveFile(capturePath, {0u}) &&
                SavePortableRuntimeState(capturePath) &&
                QuestVr::ReadBoundedSaveFile(capturePath,
                    QuestVr::kMaximumSaveRuntimeBytes, runtime)) {
                const auto published = QuestVr::PublishSaveBundle(
                    QuestVr::MakeSaveBundlePaths(prefix), encodedMetadata, runtime);
                saved = published.saved;
                generation = published.generation;
            }
        } catch (const std::exception& error) {
            ALOG("DeusExQuest: quick-save exception: %s", error.what());
        }
        interactionStatus_ = saved ? "QUICK-SAVE COMPLETE" : "QUICK-SAVE FAILED - PRIOR SAVE KEPT";
        interactionStatusSeconds_ = 4.0f;
        ALOG("DeusExQuest: VR quick-save %s generation=%llu map=%s health=%.1f",
            saved ? "completed" : "failed", static_cast<unsigned long long>(generation),
            currentMapName_.c_str(), GetPortableRuntimePlayerHealth());
    }

    static std::string QuickSavePrefix() {
        return "/data/user/0/dev.deusex.questvr.smoketest/files/DeusEx/quest-save-0";
    }

    void ClearPendingPersonaRestore() {
        pendingPersonaRestore_ = false;
        restoredDialogueOffsets_.clear();
        restoredPersonaLogs_.clear();
    }

    void LoadGameState() {
        if (!runtimeAvailable_ || !headTrackingValid_ ||
            !pendingMapName_.empty() || !transitionMapName_.empty()) return;
        try {
            const std::string prefix = QuickSavePrefix();
            const std::string restorePath = prefix + ".load.runtime.tmp";
            const auto candidates = QuestVr::LoadSaveBundleCandidates(
                QuestVr::MakeSaveBundlePaths(prefix));
            for (const auto& candidate : candidates) {
                QuestVr::QuestSaveMetadata metadata;
                if (QuestVr::DecodeQuestSaveMetadata(candidate.metadata, currentMapName_, metadata) &&
                    SaveMapAvailable(metadata.mapName) &&
                    QuestVr::WriteDurableSaveFile(restorePath, candidate.runtime) &&
                    ValidatePortableRuntimeState(restorePath, metadata.mapName) &&
                    RestoreGameState(std::move(metadata), restorePath)) {
                    ALOG("DeusExQuest: quick-load accepted bundle generation=%llu slot=%u",
                        static_cast<unsigned long long>(candidate.generation), candidate.slotIndex);
                    return;
                }
                ALOG("DeusExQuest: quick-load rejected bundle generation=%llu",
                    static_cast<unsigned long long>(candidate.generation));
            }
            // Read-only migration: retain the old files as a last-resort save.
            // New publications never change them, even if bundle recovery fails.
            std::vector<std::uint8_t> oldMetadata, runtime;
            QuestVr::QuestSaveMetadata metadata;
            if (QuestVr::ReadBoundedSaveFile(prefix + ".meta",
                    QuestVr::kQuestSaveMetadataLimit, oldMetadata) &&
                QuestVr::DecodeQuestSaveMetadata(oldMetadata, currentMapName_, metadata) &&
                SaveMapAvailable(metadata.mapName) &&
                QuestVr::ReadBoundedSaveFile(prefix + ".runtime",
                    QuestVr::kMaximumSaveRuntimeBytes, runtime) &&
                QuestVr::WriteDurableSaveFile(restorePath, runtime) &&
                ValidatePortableRuntimeState(restorePath, metadata.mapName) &&
                RestoreGameState(std::move(metadata), restorePath)) {
                ALOG("DeusExQuest: quick-load accepted legacy save (files kept unchanged)");
                return;
            }
        } catch (const std::exception& error) {
            ALOG("DeusExQuest: quick-load exception: %s", error.what());
        }
        interactionStatus_ = "QUICK-LOAD FAILED - CURRENT STATE KEPT";
        interactionStatusSeconds_ = 4.0f;
        ALOG("DeusExQuest: VR quick-load failed without replacing current state");
    }

    bool SaveMapAvailable(const std::string& mapName) const {
        return mapName == currentMapName_ ||
            std::find(mapNames_.begin(), mapNames_.end(), mapName) != mapNames_.end();
    }

    bool RestoreGameState(QuestVr::QuestSaveMetadata metadata, const std::string& runtimePath) {
        const auto& pose = metadata.pose;
        if (metadata.mapName != currentMapName_) {
            if (GetPortableRuntimeScriptStatePresent()) return false;
            restoredMapLocalPose_ = metadata.mapLocalPose;
            restoredMapLocalFeet_ = {pose[0], pose[1], pose[2]};
            restoredMapLocalHeadYaw_ = pose[3];
            restoredWorldPosition_ = {pose[0], pose[1], pose[2]};
            restoredSceneYaw_ = pose[3];
            restorePoseAfterTransition_ = true;
            BeginMapLoad(metadata.mapName, runtimePath);
            if (pendingMapName_ != metadata.mapName) {
                restorePoseAfterTransition_ = false;
                return false;
            }
            pendingPersonaRestore_ = true;
            restoredDialogueOffsets_ = std::move(metadata.dialogueOffsets);
            restoredPersonaLogs_ = std::move(metadata.personaLogs);
            interactionStatus_ = "QUICK-LOAD RESTORING MAP";
            interactionStatusSeconds_ = 4.0f;
            ALOG("DeusExQuest: VR quick-load restoring map %s", metadata.mapName.c_str());
            return true;
        }
        if (!LoadPortableRuntimeState(runtimePath)) {
            ALOG("DeusExQuest: VR quick-load runtime failed");
            return false;
        }
        ClearPendingConversation();
        InvalidateDialogueAudio();
        // Restore only after the runtime parser succeeds. Failed quick-loads
        // must keep UI history and any in-progress actor chunks unchanged.
        dialogueOffsets_ = std::move(metadata.dialogueOffsets);
        personaLogEntries_ = std::move(metadata.personaLogs);
        inventoryMenuDirty_ = true;
        if (metadata.mapLocalPose) {
            QuestVr::RestoreSavedMapPose(OVR::Vector3f{pose[0], pose[1], pose[2]}, pose[3],
                currentHeadStage_, currentHeadStageYaw_, worldPosition_, sceneYaw_);
        } else {
            worldPosition_ = {pose[0], pose[1], pose[2]};
            sceneYaw_ = pose[3];
        }
        previousHeadStage_ = currentHeadStage_;
        try {
            DestroyActorGeometry();
            actorSnapshots_ = GetPortableRuntimeMapActors();
            BuildActorMarkers();
        } catch (const std::exception& error) {
            // State restoration already succeeded; do not claim it was kept
            // unchanged or try an older candidate after a visual allocation
            // failure. Suspend gameplay rather than retain mismatched actors.
            DestroySceneGeometry();
            runtimeAvailable_ = false;
            interactionStatus_ = "QUICK-LOAD VISUAL REBUILD FAILED - RESTART SESSION";
            interactionStatusSeconds_ = 10.0f;
            ALOG("DeusExQuest: restored state but visual rebuild failed: %s", error.what());
            return true;
        }
        interactionStatus_ = "QUICK-LOAD COMPLETE";
        interactionStatusSeconds_ = 4.0f;
        ALOG(
            "DeusExQuest: VR quick-load completed health=%.1f",
            GetPortableRuntimePlayerHealth());
        return true;
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
        std::uint32_t targetRate, const QuestVr::LightmapVec3& origin) {
        std::vector<SpatialAudioEmitter> emitters;
        if (targetRate == 0u) return emitters;
        const float originX = origin.x, originY = origin.y, originZ = origin.z;
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

    using DecodedDialogueAudio = QuestVr::DecodedMp3Audio;

    void InvalidateDialogueAudio() {
        // Main-thread epochs invalidate a pending decoder without destroying
        // its std::async future (which would wait on the render thread). The
        // decoder owns only compressed bytes and can finish independently.
        dialogueAudioEpoch_.Invalidate(dialogueDecodeEpoch_, dialogueDecodeFuture_.valid());
        pendingDialogueSpatialized_ = false;
        pendingDialogueActorPath_.clear();
        pendingDialogueLocalPosition_ = {};
        std::lock_guard<std::mutex> lock(audioMutex_);
        dialogueSamples_.clear();
        dialogueCursor_ = 0u;
        dialogueSpatialized_ = false;
        dialogueLocalPosition_ = {};
        dialogueLeftGain_ = dialogueRightGain_ = 1.0f;
    }

    static DecodedDialogueAudio DecodeDialogueAudio(
        const std::vector<std::uint8_t>& bytes,
        std::uint32_t targetRate) {
        return QuestVr::DecodePortableMp3Audio(bytes, targetRate);
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
        try {
            dialogueDecodeEpoch_ = dialogueAudioEpoch_.Capture();
            dialogueDecodeFuture_ = std::async(
                std::launch::async,
                [bytes = sound.data, targetRate]() {
                    // The main thread validated MP3 above. Do not read NameString
                    // global storage while another worker loads map/package names.
                    return DecodeDialogueAudio(bytes, targetRate);
                });
        } catch (const std::exception& error) {
            ALOG("DeusExQuest: dialogue worker could not start: %s", error.what());
            return false;
        }
        return true;
    }

    void PollDialogueAudioDecode() {
        if (!dialogueDecodeFuture_.valid() || dialogueDecodeFuture_.wait_for(
                std::chrono::seconds(0)) != std::future_status::ready) return;
        DecodedDialogueAudio decoded;
        try {
            decoded = dialogueDecodeFuture_.get();
        } catch (const std::exception& error) {
            ALOG("DeusExQuest: dialogue decode failed without stopping gameplay: %s", error.what());
            return;
        }
        if (!dialogueAudioEpoch_.IsCurrent(dialogueDecodeEpoch_)) {
            ALOG("DeusExQuest: discarded dialogue decoded for an abandoned map/save state");
            return;
        }
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

    static AmbientPreparation PrepareAmbientAudio() {
        AmbientPreparation preparation;
        constexpr const char* path =
            "/data/user/0/dev.deusex.questvr.smoketest/files/DeusEx/quest-ambient.wav";
        std::FILE* file = std::fopen(path, "rb");
        if (file == nullptr) return {};
        std::fseek(file, 0, SEEK_END);
        const long fileSize = std::ftell(file);
        std::fseek(file, 0, SEEK_SET);
        std::vector<std::uint8_t> wav(fileSize > 0 ? static_cast<std::size_t>(fileSize) : 0u);
        const bool read = !wav.empty() &&
            std::fread(wav.data(), 1, wav.size(), file) == wav.size();
        std::fclose(file);
        if (!read || wav.size() < 12 || std::memcmp(wav.data(), "RIFF", 4) != 0 ||
            std::memcmp(wav.data() + 8, "WAVE", 4) != 0) return {};

        std::uint16_t format{}, channels{}, bits{};
        std::uint32_t sampleRate{};
        const std::uint8_t* pcm{};
        std::size_t pcmBytes{};
        for (std::size_t offset = 12; offset + 8 <= wav.size();) {
            const std::uint32_t chunkSize = ReadLe32(wav.data() + offset + 4);
            const std::size_t dataOffset = offset + 8;
            if (dataOffset + chunkSize > wav.size()) return {};
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
            pcm == nullptr || pcmBytes == 0) return {};
        const std::size_t bytesPerSample = bits / 8u;
        const std::size_t frames = pcmBytes / (channels * bytesPerSample);
        preparation.samples.resize(frames * 2u);
        for (std::size_t frame = 0; frame < frames; ++frame) {
            for (std::size_t outputChannel = 0; outputChannel < 2; ++outputChannel) {
                const std::size_t sourceChannel = channels == 1 ? 0 : outputChannel;
                const std::size_t source = (frame * channels + sourceChannel) * bytesPerSample;
                const std::int16_t sample = bits == 8
                    ? static_cast<std::int16_t>(
                          (static_cast<std::int32_t>(pcm[source]) - 128) << 8)
                    : static_cast<std::int16_t>(ReadLe16(pcm + source));
                preparation.samples[frame * 2u + outputChannel] = sample;
            }
        }
        preparation.sampleRate = sampleRate;
        preparation.passed = true;

        return preparation;
    }

    bool StartAmbientAudio(AmbientPreparation preparation) {
        if (!preparation.passed) return false;
        ambientSamples_ = std::move(preparation.samples);
        audioSampleRate_ = preparation.sampleRate;
        const std::uint32_t sampleRate = audioSampleRate_;
        const std::size_t frames = ambientSamples_.size() / 2u;

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
        return QuestVr::StageToLocal(stage, worldPosition, sceneYaw_);
    }

    void SnapTurnAroundHead(float deltaYaw, const OVR::Vector3f& headStage) {
        QuestVr::SnapTurnAroundHead(deltaYaw, headStage, worldPosition_, sceneYaw_);
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
        std::unique_ptr<std::FILE, int(*)(std::FILE*)> file(std::fopen(path,"rb"),std::fclose);
        if (!file) return result;
        try {
            const auto sidecar = QuestVr::ReadWorldSurfaceStream(std::string(path)+".surfaces");
            std::uint32_t magic{}, version{}, chunkCount{};
            if (std::fread(&magic,sizeof(magic),1,file.get()) != 1 ||
                std::fread(&version,sizeof(version),1,file.get()) != 1 ||
                std::fread(&chunkCount,sizeof(chunkCount),1,file.get()) != 1 ||
                magic != 0x4d515844u || version != 2u || chunkCount == 0u ||
                chunkCount > QuestVr::kMaximumWorldSurfaceChunks || chunkCount != sidecar.size())
                throw std::runtime_error("DXQM/DXQS header or chunk count mismatch");
            result.chunks.reserve(chunkCount);
            std::size_t totalVertices{};
            for (std::uint32_t index=0; index<chunkCount; ++index) {
                WorldMeshChunkPreparation chunk;
                std::uint32_t vertexCount{};
                if (std::fread(&chunk.materialSlot,sizeof(chunk.materialSlot),1,file.get()) != 1 ||
                    std::fread(&vertexCount,sizeof(vertexCount),1,file.get()) != 1 ||
                    (chunk.materialSlot != 0 && chunk.materialSlot != -1) ||
                    vertexCount == 0u || vertexCount > QuestVr::kMaximumWorldSurfaceChunkVertices ||
                    vertexCount%3u != 0u || vertexCount != sidecar[index].records.size() ||
                    chunk.materialSlot != sidecar[index].materialSlot ||
                    vertexCount > QuestVr::kMaximumWorldSurfaceVertices-totalVertices)
                    throw std::runtime_error("DXQM/DXQS raw chunk material/count mismatch");
                chunk.vertices.resize(vertexCount);
                if (std::fread(chunk.vertices.data(),sizeof(MeshVertex),vertexCount,file.get()) != vertexCount)
                    throw std::runtime_error("DXQM raw vertex stream is truncated");
                for (const auto& vertex:chunk.vertices)
                    if (!std::isfinite(vertex.px) || !std::isfinite(vertex.py) || !std::isfinite(vertex.pz) ||
                        !std::isfinite(vertex.nx) || !std::isfinite(vertex.ny) || !std::isfinite(vertex.nz) ||
                        !std::isfinite(vertex.u) || !std::isfinite(vertex.v) ||
                        (chunk.materialSlot == 0 && (vertex.materialSlot < 0 || vertex.materialSlot >= 255)))
                        throw std::runtime_error("DXQM raw vertex has invalid values");
                chunk.surfaces = sidecar[index].records;
                totalVertices += vertexCount;
                result.chunks.emplace_back(std::move(chunk));
            }
            if (std::fgetc(file.get()) != EOF || std::ferror(file.get()))
                throw std::runtime_error("DXQM has trailing bytes or a read error");
            result.passed = true;
        } catch (const std::exception& error) {
            ALOG("DeusExQuest: world mesh CPU validation failed: %s",error.what());
            result = {};
        }
        return result;
    }

    static void PrepareWorldStaticLightmaps(const PortablePackageTables& package,
        const std::vector<PortableActorSnapshot>& authoredActors, WorldMeshPreparation& mesh) {
        if (!mesh.passed || mesh.chunks.empty())
            throw std::runtime_error("world mesh unavailable for static lightmap preparation");
        std::vector<QuestVr::StaticLightmapMeshChunk> rawChunks;
        rawChunks.reserve(mesh.chunks.size());
        for (const auto& chunk:mesh.chunks) {
            QuestVr::StaticLightmapMeshChunk raw;
            raw.materialSlot = chunk.materialSlot;
            raw.surfaces = chunk.surfaces;
            raw.localPositions.reserve(chunk.vertices.size());
            for (const auto& vertex:chunk.vertices)
                raw.localPositions.push_back({vertex.px,vertex.py,vertex.pz});
            rawChunks.emplace_back(std::move(raw));
        }
        auto lightmap = QuestVr::BuildQuestStaticLightmapCache(package,authoredActors,rawChunks);
        if (lightmap.vertices.size() != mesh.chunks.size())
            throw std::runtime_error("baked lightmap raw chunk count mismatch");
        std::vector<WorldMeshChunkPreparation> gpuChunks;
        constexpr std::size_t verticesPerGpuBatch=4800u;
        for (std::size_t index=0; index<mesh.chunks.size(); ++index) {
            const auto& raw=mesh.chunks[index];
            const auto& baked=lightmap.vertices[index];
            if (baked.size() != raw.vertices.size() || raw.surfaces.size() != raw.vertices.size())
                throw std::runtime_error("baked lightmap raw vertex count mismatch");
            for (std::size_t offset=0; offset<raw.vertices.size(); offset+=verticesPerGpuBatch) {
                const auto end=std::min(offset+verticesPerGpuBatch,raw.vertices.size());
                WorldMeshChunkPreparation chunk;
                chunk.materialSlot=raw.materialSlot;
                chunk.vertices.assign(raw.vertices.begin()+static_cast<std::ptrdiff_t>(offset),
                                      raw.vertices.begin()+static_cast<std::ptrdiff_t>(end));
                chunk.surfaces.assign(raw.surfaces.begin()+static_cast<std::ptrdiff_t>(offset),
                                      raw.surfaces.begin()+static_cast<std::ptrdiff_t>(end));
                chunk.lightmapVertices.assign(baked.begin()+static_cast<std::ptrdiff_t>(offset),
                                             baked.begin()+static_cast<std::ptrdiff_t>(end));
                gpuChunks.emplace_back(std::move(chunk));
            }
        }
        ALOG("DeusExQuest: static lightmap CPU bake: %zu surfaces/%zu pixels, %zu listed/%zu added/%zu disabled lights, "
             "%zu shadowed/%zu visible mask bits; %zu unlit/%zu no-lightmap vertices; atlas=%ux%ux%u gain=%.2f quantization=%.6f",
             lightmap.bakedSurfaces,lightmap.pixelSamples,lightmap.bakeStats.listedLights,
             lightmap.bakeStats.addedLights,lightmap.bakeStats.disabledLights,
             lightmap.shadowedMaskSamples,lightmap.visibleMaskSamples,
             lightmap.unlitVertices,lightmap.noLightmapVertices,lightmap.width,lightmap.height,lightmap.layers,
             lightmap.gainScale,lightmap.maximumQuantizationError);
        if (lightmap.bakeStats.unsupportedTypes || lightmap.bakeStats.unsupportedEffects) {
            ALOG("DeusExQuest: static bake incomplete dynamic components: %zu unsupported types/%zu effects; "
                 "these are diagnosed, not fabricated steady illumination",
                 lightmap.bakeStats.unsupportedTypes,lightmap.bakeStats.unsupportedEffects);
            for (std::size_t type=0; type<lightmap.bakeStats.unsupportedTypeCounts.size(); ++type)
                if (lightmap.bakeStats.unsupportedTypeCounts[type])
                    ALOG("DeusExQuest: unsupported static light type %zu: %zu surface-list entries",
                         type,lightmap.bakeStats.unsupportedTypeCounts[type]);
            for (std::size_t effect=0; effect<lightmap.bakeStats.unsupportedEffectCounts.size(); ++effect)
                if (lightmap.bakeStats.unsupportedEffectCounts[effect])
                    ALOG("DeusExQuest: unsupported static light effect %zu: %zu surface-list entries",
                         effect,lightmap.bakeStats.unsupportedEffectCounts[effect]);
        }
        // Drop duplicate CPU streams after matching/splitting. Atlas RGBA is
        // transferred into its independently staged upload; chunk streams keep
        // only the vertex metadata needed by the shared BSP shader.
        lightmap.vertices.clear();
        mesh.chunks=std::move(gpuChunks);
        mesh.lightmap=std::move(lightmap);
    }

    bool UploadWorldMeshChunk(WorldMeshChunkPreparation& chunk) {
        if (chunk.vertices.empty() || chunk.vertices.size() > 4800u ||
            chunk.vertices.size()%3u != 0u || chunk.surfaces.size() != chunk.vertices.size() ||
            chunk.lightmapVertices.size() != chunk.vertices.size()) return false;
        const std::uint32_t firstCollisionTriangle =
            static_cast<std::uint32_t>(collisionTriangles_.size());
        const std::uint32_t vertexCount = static_cast<std::uint32_t>(chunk.vertices.size());
        OVRFW::GlGeometry::Descriptor descriptor;
        descriptor.attribs.position.reserve(vertexCount);
        descriptor.attribs.normal.reserve(vertexCount);
        descriptor.attribs.uv0.reserve(vertexCount);
        descriptor.attribs.uv1.reserve(vertexCount);
        descriptor.attribs.tangent.reserve(vertexCount);
        descriptor.attribs.jointWeights.reserve(vertexCount);
        descriptor.attribs.color.reserve(vertexCount);
        descriptor.indices.reserve(vertexCount);
        for (std::uint32_t index = 0; index < vertexCount; ++index) {
            const MeshVertex& vertex = chunk.vertices[index];
            descriptor.attribs.position.emplace_back(vertex.px, vertex.py, vertex.pz);
            descriptor.attribs.normal.emplace_back(vertex.nx, vertex.ny, vertex.nz);
            descriptor.attribs.uv0.emplace_back(vertex.u, vertex.v);
            const auto& lightmap = chunk.lightmapVertices[index];
            if (!std::isfinite(lightmap.u) || !std::isfinite(lightmap.v) ||
                !std::isfinite(lightmap.minU) || !std::isfinite(lightmap.minV) ||
                !std::isfinite(lightmap.maxU) || !std::isfinite(lightmap.maxV) ||
                lightmap.minU > lightmap.maxU || lightmap.minV > lightmap.maxV ||
                lightmap.minU < 0.0f || lightmap.minV < 0.0f || lightmap.maxU > 1.0f || lightmap.maxV > 1.0f ||
                (lightmap.flags & ~1u) != 0u ||
                lightmap.page < -1 || (lightmap.page >= 0 &&
                    static_cast<std::uint32_t>(lightmap.page) >= staticLightmapLayers_)) return false;
            descriptor.attribs.uv1.emplace_back(lightmap.u,lightmap.v);
            descriptor.attribs.tangent.emplace_back(static_cast<float>(lightmap.page),
                static_cast<float>(lightmap.flags),0.0f);
            descriptor.attribs.jointWeights.emplace_back(
                lightmap.minU,lightmap.minV,lightmap.maxU,lightmap.maxV);
            const float shade = 0.25f + 0.55f * (vertex.nz * 0.5f + 0.5f);
            if (chunk.materialSlot == 0) {
                descriptor.attribs.color.emplace_back(
                    static_cast<float>(vertex.materialSlot) / 255.0f,
                    1.0f,1.0f,1.0f);
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
            bakedWorldRenderers_.emplace_back();
            bakedWorldRenderers_.back().Init(
                descriptor,firstTexture_,staticLightmapTexture_,staticLightmapGainScale_);
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
        pendingWorldMesh_.chunks[pendingWorldMeshChunk_].surfaces.clear();
        pendingWorldMesh_.chunks[pendingWorldMeshChunk_].lightmapVertices.clear();
        ++pendingWorldMeshChunk_;
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
        actorTexturePolyFlags_ = std::move(array.texturePolyFlags);
        actorMaskedTextureLayers_ = std::move(array.maskedTextureLayers);
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
        pendingActorTexturePolyFlags_ = std::move(array.texturePolyFlags);
        pendingActorMaskedTextureLayers_ = std::move(array.maskedTextureLayers);
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
            actorTexturePolyFlags_ = std::move(pendingActorTexturePolyFlags_);
            actorMaskedTextureLayers_ = std::move(pendingActorMaskedTextureLayers_);
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

    void ClearPendingStaticLightmapUpload() {
        if (pendingStaticLightmapId_ != 0u) glDeleteTextures(1,&pendingStaticLightmapId_);
        pendingStaticLightmapId_ = 0u;
        pendingStaticLightmapRgba_.clear();
        pendingStaticLightmapLayer_ = pendingStaticLightmapRow_ = 0u;
        pendingStaticLightmapLayers_ = pendingStaticLightmapWidth_ = pendingStaticLightmapHeight_ = 0u;
    }

    bool BeginStaticLightmapUpload(QuestVr::StaticLightmapCache cache) {
        ClearPendingStaticLightmapUpload();
        const auto bytes = static_cast<std::uint64_t>(cache.width)*cache.height*cache.layers*4u;
        GLint maxSize{}, maxLayers{};
        glGetIntegerv(GL_MAX_TEXTURE_SIZE,&maxSize);
        glGetIntegerv(GL_MAX_ARRAY_TEXTURE_LAYERS,&maxLayers);
        if (cache.width == 0u || cache.height == 0u || cache.layers == 0u ||
            cache.width > 1024u || cache.height > 1024u || cache.layers > 16u ||
            maxSize <= 0 || maxLayers <= 0 || cache.width > static_cast<std::uint32_t>(maxSize) ||
            cache.height > static_cast<std::uint32_t>(maxSize) || cache.layers > static_cast<std::uint32_t>(maxLayers) ||
            bytes != cache.rgba.size() || bytes > 64u*1024u*1024u ||
            !std::isfinite(cache.gainScale) || cache.gainScale <= 0.0f || cache.gainScale > 16.0f ||
            glGetError() != GL_NO_ERROR) return false;
        GLuint texture{};
        glGenTextures(1,&texture);
        glBindTexture(GL_TEXTURE_2D_ARRAY,texture);
        glTexStorage3D(GL_TEXTURE_2D_ARRAY,1,GL_RGBA8,
            static_cast<GLsizei>(cache.width),static_cast<GLsizei>(cache.height),static_cast<GLsizei>(cache.layers));
        glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        const GLenum error = glGetError();
        glBindTexture(GL_TEXTURE_2D_ARRAY,0);
        if (!texture || error != GL_NO_ERROR) {
            if (texture) glDeleteTextures(1,&texture);
            return false;
        }
        pendingStaticLightmapId_ = texture;
        pendingStaticLightmapWidth_ = cache.width; pendingStaticLightmapHeight_ = cache.height;
        pendingStaticLightmapLayers_ = cache.layers; pendingStaticLightmapGainScale_ = cache.gainScale;
        pendingStaticLightmapRgba_ = std::move(cache.rgba);
        return true;
    }

    bool UploadStaticLightmapRows() {
        if (!pendingStaticLightmapId_ || pendingStaticLightmapLayer_ >= pendingStaticLightmapLayers_) return false;
        constexpr std::size_t bytesPerFrame = 128u*1024u;
        const std::size_t bytesPerRow = static_cast<std::size_t>(pendingStaticLightmapWidth_)*4u;
        const auto rows = static_cast<std::uint32_t>(std::min<std::size_t>(bytesPerFrame/bytesPerRow,
            pendingStaticLightmapHeight_-pendingStaticLightmapRow_));
        if (rows == 0u) return false;
        const std::size_t offset = (static_cast<std::size_t>(pendingStaticLightmapLayer_)*pendingStaticLightmapHeight_+
            pendingStaticLightmapRow_)*bytesPerRow;
        glBindTexture(GL_TEXTURE_2D_ARRAY,pendingStaticLightmapId_);
        glTexSubImage3D(GL_TEXTURE_2D_ARRAY,0,0,static_cast<GLint>(pendingStaticLightmapRow_),
            static_cast<GLint>(pendingStaticLightmapLayer_),static_cast<GLsizei>(pendingStaticLightmapWidth_),
            static_cast<GLsizei>(rows),1,GL_RGBA,GL_UNSIGNED_BYTE,pendingStaticLightmapRgba_.data()+offset);
        const GLenum error = glGetError();
        glBindTexture(GL_TEXTURE_2D_ARRAY,0);
        if (error != GL_NO_ERROR) return false;
        pendingStaticLightmapRow_ += rows;
        if (pendingStaticLightmapRow_ == pendingStaticLightmapHeight_) {
            pendingStaticLightmapRow_ = 0u; ++pendingStaticLightmapLayer_;
        }
        if (pendingStaticLightmapLayer_ == pendingStaticLightmapLayers_) {
            staticLightmapTexture_ = OVRFW::GlTexture(pendingStaticLightmapId_,GL_TEXTURE_2D_ARRAY,
                static_cast<int>(pendingStaticLightmapWidth_),static_cast<int>(pendingStaticLightmapHeight_));
            staticLightmapGainScale_ = pendingStaticLightmapGainScale_;
            staticLightmapLayers_ = pendingStaticLightmapLayers_;
            pendingStaticLightmapId_ = 0u;
            pendingStaticLightmapRgba_.clear();
            ALOG("DeusExQuest: original static lightmap atlas uploaded in <=128KiB row slices: %ux%ux%u gain=%.2f",
                pendingStaticLightmapWidth_,pendingStaticLightmapHeight_,pendingStaticLightmapLayers_,staticLightmapGainScale_);
        }
        return true;
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


    // SDK graphics commands keep pointers into their owning renderer (uniforms
    // and textures). Deque growth preserves those addresses when world/actor
    // chunks are appended; vector relocation left stale uniform pointers.
    std::deque<OVRFW::GeometryRenderer> worldRenderers_;
    std::deque<TexturedGeometryRenderer> texturedRenderers_;
    std::deque<BakedWorldGeometryRenderer> bakedWorldRenderers_;
    std::unique_ptr<ActorGeometryBuild> actorGeometryBuild_;
    // App-owned, so cancelling/destroying ActorGeometryBuild never blocks on an
    // unfinished std::async future. There is exactly one live pose job globally,
    // including retired jobs; final app destruction joins it after XR shutdown.
    std::future<std::shared_ptr<const QuestVr::MeshPose>> actorPoseFuture_;
    QuestVr::AsyncResultEpoch actorPoseEpoch_;
    std::uint64_t actorPoseJobEpoch_{};
    std::size_t actorPoseActorOrdinal_{};
    bool actorGeometryComplete_{};
    std::vector<PortableActorSnapshot> actorSnapshots_;
    std::vector<MapLight> activeMapLights_;
    std::vector<InteractiveActor> interactiveActors_;
    OVRFW::GlTexture firstTexture_;
    OVRFW::GlTexture actorTexture_;
    std::vector<std::string> actorTexturePaths_;
    std::vector<std::uint32_t> actorTexturePolyFlags_;
    std::vector<std::int32_t> actorMaskedTextureLayers_;
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
    // Both values are main-thread only; the decoder never reads app state.
    QuestVr::AsyncResultEpoch dialogueAudioEpoch_;
    std::uint64_t dialogueDecodeEpoch_{};
    std::vector<CollisionTriangle> collisionTriangles_;
    std::unordered_map<std::int64_t, std::vector<std::uint32_t>> collisionGrid_;
    std::vector<std::uint32_t> oversizedCollisionTriangles_;
    OVR::Vector3f worldPosition_{0.0f, 0.0f, 0.0f};
    OVR::Vector3f currentHeadStage_{};
    OVR::Vector3f previousHeadStage_{};
    bool hasPreviousHeadStage_{};
    bool headTrackingValid_{};
    float currentHeadStageYaw_{};
    bool headTrackingReported_{};
    bool needsTrackingRebase_{};
    OVR::Vector3f trackingResumeLocalHead_{};
    std::vector<XrEventDataReferenceSpaceChangePending> pendingReferenceChanges_;
    bool hasTransitionHeadAnchor_{};
    OVR::Vector3f transitionHeadLocal_{};
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
    std::vector<OVRFW::ovrSurfaceDef> headLockedUiSurfaces_;
    OVRFW::VRMenuObject* hudLabel_{};
    OVRFW::VRMenuObject* inventoryLabel_{};
    OVRFW::VRMenuObject* personaTabsLabel_{};
    OVRFW::VRMenuObject* personaDetailsLabel_{};
    OVRFW::VRMenuObject* personaFooterLabel_{};
    PersonaUiRenderer personaRenderer_;
    PortablePackageTables personaUiPackage_;
    QuestVr::PersonaUiChrome personaChrome_;
    PortableBitmapFont personaHeaderFont_;
    PortableBitmapFont personaBodyFont_;
    bool personaOriginalTextReady_{};
    std::array<std::vector<std::uint8_t>, 4> personaPageBaseRgba_;
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
    bool pendingPersonaRestore_{};
    std::unordered_map<std::string, std::size_t> restoredDialogueOffsets_;
    std::vector<std::string> restoredPersonaLogs_;
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
    bool restoredMapLocalPose_{};
    OVR::Vector3f restoredMapLocalFeet_{};
    float restoredMapLocalHeadYaw_{};
    float restoredSceneYaw_{};
    std::string pendingMapName_;
    bool runtimeAvailable_{};
    bool initialPreparationPending_{};
    bool initialPreparationFailed_{};
    std::future<InitialPreparation> initialPreparationFuture_;
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
    GLuint pendingStaticLightmapId_{};
    std::uint32_t pendingStaticLightmapWidth_{}, pendingStaticLightmapHeight_{}, pendingStaticLightmapLayers_{};
    std::uint32_t pendingStaticLightmapLayer_{}, pendingStaticLightmapRow_{};
    float pendingStaticLightmapGainScale_{1.0f}, staticLightmapGainScale_{1.0f};
    std::vector<std::uint8_t> pendingStaticLightmapRgba_;
    OVRFW::GlTexture staticLightmapTexture_;
    std::uint32_t staticLightmapLayers_{};
    QuestVr::LightmapVec3 activeMapUnrealOrigin_;
    std::string activeMapPlayerStartPath_;
    GLuint pendingActorTextureId_{};
    std::uint32_t pendingActorTextureWidth_{};
    std::uint32_t pendingActorTextureHeight_{};
    std::uint32_t pendingActorTextureLayers_{};
    std::uint32_t pendingActorTextureLayersUploaded_{};
    std::vector<std::uint8_t> pendingActorTextureRgba_;
    std::vector<std::string> pendingActorTexturePaths_;
    std::vector<std::uint32_t> pendingActorTexturePolyFlags_;
    std::vector<std::int32_t> pendingActorMaskedTextureLayers_;
};

ENTRY_POINT(DeusExQuestApp)
