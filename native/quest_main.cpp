#include <openxr/openxr.h>
#include <GLES3/gl3.h>
#include <aaudio/AAudio.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <cstdint>
#include <cmath>
#include <chrono>
#include <ctime>
#include <cstdio>
#include <cstring>
#include <deque>
#include <future>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <numeric>
#include <optional>
#include <set>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <sys/stat.h>

#include "Input/TinyUI.h"
#include "GUI/VRMenuMgr.h"
#include "Render/GeometryBuilder.h"
#include "Render/GeometryRenderer.h"
#include "Render/GlGeometry.h"
#include "Render/GlTexture.h"
#include "Render/SurfaceRender.h"
#include "XrApp.h"
#include "portable_unreal_runtime.h"
#include "portable_object_lookup.h"
#include "portable_engine_collision.h"
#include "portable_input_schema.h"
#include "portable_input_reset.h"
#include "portable_flag_base.h"
#include "portable_system_services.h"
#include "quest_runtime_script_packages.h"
#include "portable_music_sources.h"
#include "quest_native_client.h"
#include "portable_quest_input_source.h"
#include "persona_ui_canvas.h"
#include "quest_dialogue_ui.h"
#include "quest_hud_ui.h"
#include "quest_hud_view.h"
#include "quest_playable_hud.h"
#include "quest_quick_belt_visibility.h"
#include "quest_hud_health_clock.h"
#include "quest_hud_oxygen_clock.h"
#include "quest_persona_view.h"
#include "quest_map_cache.h"
#include "vr_world_transform.h"
#include "quest_vr_input.h"
#include "quest_inventory_navigation.h"
#include "quest_player_visual.h"
#include "quest_held_item_assets.h"
#include "quest_equipped_item_visual.h"
#include "quest_held_visual_cache.h"
#include "quest_player_posture.h"
#include "quest_ground_probe.h"
#include "quest_targeting_frame.h"
#include "quest_target_description.h"
#include "frame_work_budget.h"
#include "map_transition_transaction.h"
#include "quest_map_lighting.h"
#include "quest_map_music.h"
#include "quest_static_lightmap_cache.h"
#include "quest_world_material.h"
#include "portable_model_geometry.h"
#include "quest_actor_geometry.h"
#include "quest_npc_animation.h"
#include "quest_save_metadata.h"
#include "quest_save_bundle.h"
#include "async_result_epoch.h"
#include "retained_bank_publication.h"
#include "actor_visibility_state.h"
#include "quest_actor_scene_selection.h"

#include "portable_mp3_audio.h"

class TexturedGeometryRenderer {
   public:
    using ActorVisibilityMatrices = std::array<OVR::Matrix4f,QuestVr::ActorVisibilityState::MatrixCount>;
    void Init(
        const OVRFW::GlGeometry::Descriptor& descriptor,
        const OVRFW::GlTexture& texture,
        bool cullEnable = true, std::uint32_t polyFlags = 0u,
        ActorVisibilityMatrices* actorVisibility = nullptr,
        const OVRFW::GlTexture* animationPositions = nullptr) {
        static const char* vertexShader = R"glsl(
            attribute highp vec4 Position;
            attribute highp vec2 TexCoord;
            attribute highp vec2 TexCoord1;
            attribute lowp vec4 VertexColor;
            uniform highp mat4 ActorVisibility[32];
            varying highp vec2 oTexCoord;
            varying mediump float oLayer;
            varying lowp vec3 oLight;
            void main() {
                gl_Position = TransformVertex(Position);
                // Every triangle has one actor ordinal. Clipping all three
                // vertices suppresses its color AND depth without a buffer
                // rewrite, new draw calls or a fragment-discard overdraw pass.
                highp int actor = int(floor(TexCoord1.x + 0.5));
                highp int block = actor / 16;
                highp int row = (actor - block * 16) / 4;
                highp int column = actor - block * 16 - row * 4;
                if (ActorVisibility[block][column][row] < 0.5)
                    gl_Position = vec4(2.0, 2.0, 2.0, 1.0);
                oTexCoord = TexCoord;
                oLayer = VertexColor.r * 255.0;
                oLight = VertexColor.gba;
            }
        )glsl";
        static const char* animatedVertexShader = R"glsl(
            attribute highp vec4 Position;
            attribute highp vec2 TexCoord;
            attribute highp vec2 TexCoord1;
            attribute lowp vec4 VertexColor;
            uniform highp mat4 ActorVisibility[32];
            uniform highp sampler2D AnimationPositions;
            uniform highp vec4 AnimationFrame;
            uniform highp mat4 AnimationTransform;
            varying highp vec2 oTexCoord;
            varying mediump float oLayer;
            varying lowp vec3 oLight;
            void main() {
                highp int source = int(floor(TexCoord1.y + 0.5));
                highp vec3 a = texelFetch(AnimationPositions, ivec2(source,int(AnimationFrame.x)),0).xyz;
                highp vec3 b = texelFetch(AnimationPositions, ivec2(source,int(AnimationFrame.y)),0).xyz;
                gl_Position = TransformVertex(AnimationTransform * vec4(mix(a,b,AnimationFrame.z),1.0));
                highp int actor = int(floor(TexCoord1.x + 0.5));
                highp int block = actor / 16;
                highp int row = (actor - block * 16) / 4;
                highp int column = actor - block * 16 - row * 4;
                if (ActorVisibility[block][column][row] < 0.5)
                    gl_Position = vec4(2.0,2.0,2.0,1.0);
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
            {.Name = "ActorVisibility[0]", .Type = OVRFW::ovrProgramParmType::FLOAT_MATRIX4},
            {.Name = "AnimationPositions", .Type = OVRFW::ovrProgramParmType::TEXTURE_SAMPLED},
            {.Name = "AnimationFrame", .Type = OVRFW::ovrProgramParmType::FLOAT_VECTOR4},
            {.Name = "AnimationTransform", .Type = OVRFW::ovrProgramParmType::FLOAT_MATRIX4},
        };
        if (!sharedProgram_.IsValid()) {
            sharedProgram_ = OVRFW::GlProgram::Build(
                "", vertexShader, "", fragmentShader, uniformParms, 3);
        }
        if (!sharedProgram_.IsValid())
            throw std::runtime_error("textured geometry shader creation failed");
        animated_ = animationPositions != nullptr;
        if (animated_ && !animatedProgram_.IsValid())
            animatedProgram_ = OVRFW::GlProgram::Build("",animatedVertexShader,"",fragmentShader,uniformParms,6);
        if (animated_ && (!animatedProgram_.IsValid() || !animationPositions->IsValid()))
            throw std::runtime_error("NPC vertex animation shader/texture creation failed");
        surface_.geo = OVRFW::GlGeometry(descriptor.attribs, descriptor.indices);
        OVRFW::ovrGraphicsCommand& command = surface_.graphicsCommand;
        command.Program = animated_ ? animatedProgram_ : sharedProgram_;
        command.Textures[0] = texture;
        command.UniformData[0].Data = &command.Textures[0];
        if ((polyFlags & 4u) != 0u) polyFlags &= ~2u;
        masked_ = (polyFlags & 2u) != 0u ? 1.0f : 0.0f;
        if ((polyFlags & 0x10000000u) != 0u && (polyFlags & (4u|64u)) == 0u) masked_ = -1.0f;
        command.UniformData[1].Data = &masked_;
        actorVisibility_ = actorVisibility ? actorVisibility : &allVisibleActors_;
        command.UniformData[2].Data = actorVisibility_->data();
        command.UniformData[2].Count = static_cast<int>(actorVisibility_->size());
        if (animated_) {
            command.Textures[1] = *animationPositions;
            command.UniformData[3].Data = &command.Textures[1];
            command.UniformData[4].Data = &animationFrame_;
            command.UniformData[5].Data = &animationTransform_;
        }
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
        if (animatedProgram_.IsValid()) OVRFW::GlProgram::Free(animatedProgram_);
    }
    void SetAnimation(const QuestVr::NpcAnimationFrame& frame,const OVR::Matrix4f& transform) {
        animationFrame_ = {static_cast<float>(frame.first),static_cast<float>(frame.second),frame.fraction,0.0f};
        animationTransform_ = transform;
    }
    void SetPose(const OVR::Posef& pose) { pose_ = pose; }
    void SetModelMatrix(const OVR::Matrix4f& matrix) { modelMatrix_ = matrix; }
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
        surface_.graphicsCommand.UniformData[2].Data = actorVisibility_->data();
        if (animated_) {
            surface_.graphicsCommand.UniformData[3].Data = &surface_.graphicsCommand.Textures[1];
            surface_.graphicsCommand.UniformData[4].Data = &animationFrame_;
            surface_.graphicsCommand.UniformData[5].Data = &animationTransform_;
        }
        surfaces.emplace_back(modelMatrix_, &surface_);
    }

   private:
    OVRFW::ovrSurfaceDef surface_;
    inline static OVRFW::GlProgram sharedProgram_;
    inline static OVRFW::GlProgram animatedProgram_;
    inline static ActorVisibilityMatrices allVisibleActors_ = [] {
        ActorVisibilityMatrices matrices;
        for (auto& matrix : matrices)
            for (auto& row : matrix.M) for (auto& value : row) value = 1.0f;
        return matrices;
    }();
    ActorVisibilityMatrices* actorVisibility_{&allVisibleActors_};
    float masked_{};
    bool blended_{};
    bool animated_{};
    OVR::Vector4f animationFrame_;
    OVR::Matrix4f animationTransform_ = OVR::Matrix4f::Identity();
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
              float gainScale, std::uint32_t polyFlags) {
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
            uniform highp vec3 MaterialMode;
            varying highp vec2 oTexCoord;
            varying highp vec2 oLightmapUv;
            varying highp vec4 oLightmapRect;
            varying mediump float oLayer;
            varying mediump vec2 oLightmapMode;
            varying lowp vec3 oFallbackLight;
            void main() {
                vec4 texel = texture(Texture0, vec3(fract(oTexCoord), floor(oLayer + 0.5)));
                if (MaterialMode.x > 0.5 && texel.a < 0.5) discard;
                vec3 gain = oFallbackLight;
                if (MaterialMode.y > 0.5 || oLightmapMode.y > 0.5) {
                    gain = vec3(1.0); // Authored PF_Unlit, not artificial ambient.
                } else if (oLightmapMode.x >= 0.0) {
                    vec2 uv = clamp(oLightmapUv, oLightmapRect.xy, oLightmapRect.zw);
                    gain = texture(Texture1, vec3(uv, floor(oLightmapMode.x + 0.5))).rgb * AtlasGainScale;
                }
                gl_FragColor = vec4(texel.rgb * gain, MaterialMode.z > 0.5 ? texel.a : 1.0);
            }
        )glsl";
        static OVRFW::ovrProgramParm parms[] = {
            {.Name = "Texture0", .Type = OVRFW::ovrProgramParmType::TEXTURE_SAMPLED},
            {.Name = "Texture1", .Type = OVRFW::ovrProgramParmType::TEXTURE_SAMPLED},
            {.Name = "AtlasGainScale", .Type = OVRFW::ovrProgramParmType::FLOAT},
            {.Name = "MaterialMode", .Type = OVRFW::ovrProgramParmType::FLOAT_VECTOR3},
        };
        if (!sharedProgram_.IsValid())
            sharedProgram_ = OVRFW::GlProgram::Build("",vertexShader,"",fragmentShader,parms,4);
        if (!sharedProgram_.IsValid())
            throw std::runtime_error("baked BSP shader creation failed");
        surface_.geo = OVRFW::GlGeometry(descriptor.attribs,descriptor.indices);
        gainScale_ = gainScale;
        const auto material = QuestVr::ResolveOriginalMaterialState(polyFlags);
        materialMode_ = {material.masked ? 1.0f : 0.0f,
            material.blend == QuestVr::OriginalMaterialBlend::Modulated ? 1.0f : 0.0f,
            material.blend == QuestVr::OriginalMaterialBlend::Highlighted ? 1.0f : 0.0f};
        blended_ = material.Blended();
        localCenter_ = {};
        for (const auto& position : descriptor.attribs.position) localCenter_ += position;
        if (!descriptor.attribs.position.empty()) localCenter_ *= 1.0f/descriptor.attribs.position.size();
        auto& command = surface_.graphicsCommand;
        command.Program = sharedProgram_;
        command.Textures[0] = materials;
        command.Textures[1] = lightmaps;
        command.UniformData[0].Data = &command.Textures[0];
        command.UniformData[1].Data = &command.Textures[1];
        command.UniformData[2].Data = &gainScale_;
        command.UniformData[3].Data = &materialMode_;
        command.GpuState.depthEnable = true;
        command.GpuState.depthMaskEnable = material.depthWrite;
        command.GpuState.blendEnable = OVRFW::ovrGpuState::BLEND_DISABLE;
        if (material.Blended()) {
            command.GpuState.blendEnable = OVRFW::ovrGpuState::BLEND_ENABLE;
            command.GpuState.blendSrc = material.blend == QuestVr::OriginalMaterialBlend::Modulated ? GL_DST_COLOR : GL_ONE;
            command.GpuState.blendDst = material.blend == QuestVr::OriginalMaterialBlend::Translucent ? GL_ONE_MINUS_SRC_COLOR :
                material.blend == QuestVr::OriginalMaterialBlend::Modulated ? GL_SRC_COLOR : GL_ONE_MINUS_SRC_ALPHA;
        }
        // DXQM already contains opposite winding copies of every BSP triangle.
        // Keep culling even for PF_TwoSided, or glass would blend twice.
        command.GpuState.cullEnable = true;
    }
    void Shutdown() { surface_.geo.Free(); }
    static void ShutdownSharedProgram() {
        if (sharedProgram_.IsValid()) OVRFW::GlProgram::Free(sharedProgram_);
    }
    void SetPose(const OVR::Posef& pose) { pose_ = pose; }
    bool IsBlended() const { return blended_; }
    float DepthAlong(const OVR::Vector3f& eye, const OVR::Vector3f& forward) const {
        return (modelMatrix_.Transform(localCenter_)-eye).Dot(forward);
    }
    void Update() { pose_.Rotation.Normalize(); modelMatrix_ = OVR::Matrix4f(pose_); }
    void Render(std::vector<OVRFW::ovrDrawSurface>& surfaces) {
        auto& command = surface_.graphicsCommand;
        command.UniformData[0].Data = &command.Textures[0];
        command.UniformData[1].Data = &command.Textures[1];
        command.UniformData[2].Data = &gainScale_;
        command.UniformData[3].Data = &materialMode_;
        surfaces.emplace_back(modelMatrix_,&surface_);
    }
   private:
    OVRFW::ovrSurfaceDef surface_;
    inline static OVRFW::GlProgram sharedProgram_;
    float gainScale_{1.0f};
    OVR::Vector3f materialMode_, localCenter_;
    bool blended_{};
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
            uniform lowp float Opacity;
            varying lowp vec2 oTexCoord;
            void main() {
                lowp vec4 texel = texture2D(Texture0, oTexCoord);
                texel.a *= Opacity;
                if (texel.a < 0.01) discard;
                // Theme modulation belongs to the neutral background/border
                // layers, not to the original full-color inventory icons.
                gl_FragColor = texel;
            }
        )glsl";
        static OVRFW::ovrProgramParm parms[] = {
            {.Name = "Texture0", .Type = OVRFW::ovrProgramParmType::TEXTURE_SAMPLED},
            {.Name = "Opacity", .Type = OVRFW::ovrProgramParmType::FLOAT},
        };
        program_ = OVRFW::GlProgram::Build(
            "", vertexShader, "", fragmentShader, parms, 2);
        surface_.geo = OVRFW::BuildTesselatedQuad(1, 1, true);
        auto& command = surface_.graphicsCommand;
        command.Program = program_;
        command.Textures[0] = texture;
        command.UniformData[0].Data = &command.Textures[0];
        opacity_=1.0f;
        command.UniformData[1].Data = &opacity_;
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
    void SetPose(const OVR::Posef& pose,const float halfWidth=0.60f,const float halfHeight=0.45f) {
        modelMatrix_ = OVR::Matrix4f(pose) *
            // Original PersonaScreenBaseWindow is 640x480. The stored
            // power-of-two tiles include padding outside that window.
            OVR::Matrix4f::Scaling(halfWidth,halfHeight,1.0f);
    }
    void Render(std::vector<OVRFW::ovrDrawSurface>& surfaces) {
        if (!initialized_ || opacity_<=0.0f) return;
        surface_.graphicsCommand.UniformData[0].Data=&surface_.graphicsCommand.Textures[0];
        surface_.graphicsCommand.UniformData[1].Data=&opacity_;
        surfaces.emplace_back(modelMatrix_, &surface_);
    }
    void SetOpacity(float opacity) { opacity_=std::isfinite(opacity)?std::clamp(opacity,0.0f,1.0f):0.0f; }
    bool IsInitialized() const { return initialized_; }

   private:
    OVRFW::ovrSurfaceDef surface_;
    OVRFW::GlProgram program_;
    OVR::Matrix4f modelMatrix_ = OVR::Matrix4f::Identity();
    bool initialized_{};
    float opacity_{1.0f};
};

// Original FrobDisplayWindow's Extension.Solid strips, batched into one draw.
// The immutable mesh selects one of 16 rectangle uniforms; only its bounds and
// central-camera world billboard change while aiming. There is no frame-by-frame
// buffer allocation, texture generation or head-locked debug-menu dependency.
class TargetingFrameRenderer {
   public:
    void Init() {
        static const char* vertexShader = R"glsl(
            attribute highp vec4 Position;
            uniform highp mat4 StripMatrices[4];
            uniform highp vec4 PixelPlane;
            varying lowp float oWhite;
            void main() {
                int index = int(Position.z + 0.5);
                highp vec4 strip = StripMatrices[index / 4][index % 4];
                highp vec2 pixel = strip.xy + Position.xy * strip.zw;
                highp vec2 local = (pixel - PixelPlane.zw) * PixelPlane.xy;
                gl_Position = TransformVertex(vec4(local.x, -local.y, 0.0, 1.0));
                oWhite = index >= 8 ? 1.0 : 0.0;
            }
        )glsl";
        static const char* fragmentShader = R"glsl(
            precision lowp float;
            uniform lowp vec4 ForegroundColor;
            varying lowp float oWhite;
            void main() { gl_FragColor = vec4(ForegroundColor.rgb*oWhite,1.0); }
        )glsl";
        static OVRFW::ovrProgramParm parms[] = {
            {.Name = "StripMatrices", .Type = OVRFW::ovrProgramParmType::FLOAT_MATRIX4},
            {.Name = "PixelPlane", .Type = OVRFW::ovrProgramParmType::FLOAT_VECTOR4},
            {.Name = "ForegroundColor", .Type = OVRFW::ovrProgramParmType::FLOAT_VECTOR4},
        };
        program_ = OVRFW::GlProgram::Build("",vertexShader,"",fragmentShader,parms,3);
        OVRFW::VertexAttribs attribs;
        std::vector<OVRFW::TriangleIndex> indices;
        for (unsigned strip=0u; strip<16u; ++strip) {
            for (const auto corner : {OVR::Vector2f{0,0},{1,0},{1,1},{0,0},{1,1},{0,1}}) {
                attribs.position.emplace_back(corner.x,corner.y,static_cast<float>(strip));
                indices.push_back(static_cast<OVRFW::TriangleIndex>(indices.size()));
            }
        }
        surface_.surfaceName = "Original FrobDisplayWindow corners";
        surface_.geo.Create(attribs,indices);
        auto& command = surface_.graphicsCommand;
        command.Program = program_;
        command.UniformData[0].Data = stripMatrices_.data();
        command.UniformData[0].Count = 4;
        command.UniformData[1].Data = &pixelPlane_;
        command.UniformData[2].Data = &foregroundColor_;
        command.GpuState.depthEnable = command.GpuState.depthMaskEnable = false;
        command.GpuState.cullEnable = false;
        initialized_ = true;
    }
    void Shutdown() {
        if (!initialized_) return;
        surface_.geo.Free();
        if (program_.IsValid()) OVRFW::GlProgram::Free(program_);
        initialized_ = false;
    }
    void Render(const std::array<QuestVr::TargetFrameRect,16>& strips,
        const QuestVr::TargetFrameProjection& projection, const OVR::Posef& head,
        float depth, std::vector<OVRFW::ovrDrawSurface>& surfaces,
        const std::array<std::uint8_t,3>& color) {
        if (!initialized_) return;
        const auto packed = QuestVr::PackTargetFrameMatrices(strips);
        for (std::size_t matrix=0u; matrix<packed.size(); ++matrix)
            for (std::size_t row=0u; row<4u; ++row)
                for (std::size_t column=0u; column<4u; ++column)
                    stripMatrices_[matrix].M[row][column] = packed[matrix][row*4u+column];
        pixelPlane_ = {depth/projection.focalX,depth/projection.focalY,
            projection.centerX,projection.centerY};
        foregroundColor_={color[0]/255.0f,color[1]/255.0f,color[2]/255.0f,1.0f};
        const auto model = OVR::Matrix4f(head)*OVR::Matrix4f::Translation(0,0,-depth);
        surfaces.emplace_back(model,&surface_);
    }
   private:
    OVRFW::ovrSurfaceDef surface_;
    OVRFW::GlProgram program_;
    std::array<OVR::Matrix4f,4> stripMatrices_;
    OVR::Vector4f pixelPlane_;
    OVR::Vector4f foregroundColor_{1.0f,1.0f,1.0f,1.0f};
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

    std::vector<const char*> GetExtensions() override {
        auto extensions = OVRFW::XrApp::GetExtensions();
        extensions.push_back(XR_EXT_LOCAL_FLOOR_EXTENSION_NAME);
        return extensions;
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
            OVR::Vector3f(0.0f, 0.0f, 0.0f),
            OVR::Vector2f(720.0f, 180.0f));
        if (hudLabel_ == nullptr) return false;
        // TinyUI's zero menu pose does not reset AddItems' auto-layout root.
        // That room-fixed parent offset must also be identity before applying
        // a head-relative child pose. Leave Persona's authored layout alone.
        auto* hudRoot = ui_.GetGuiSys().GetVRMenuMgr().ToObject(hudLabel_->GetParentHandle());
        if (hudRoot == nullptr) return false;
        hudRoot->SetLocalPose(OVR::Posef::Identity());
        hudLabel_->SetTextLocalPosition({0.0f, -0.025f, 0.0f});
        hudLabel_->SetTextColor(OVR::Vector4f(0.82f, 0.68f, 0.25f, 1.0f));
        subtitleLabel_ = ui_.AddLabel("", {}, {720.0f,110.0f});
        if (subtitleLabel_ == nullptr) return false;
        auto* subtitleRoot = ui_.GetGuiSys().GetVRMenuMgr().ToObject(subtitleLabel_->GetParentHandle());
        if (subtitleRoot == nullptr) return false;
        subtitleRoot->SetLocalPose(OVR::Posef::Identity());
        subtitleLabel_->SetSurfaceVisible(0,false);
        subtitleLabel_->SetTextColor({0.82f,0.68f,0.25f,1.0f});
        subtitleLabel_->SetTextLocalScale({0.7f,0.7f,1.0f});
        subtitleLabel_->SetVisible(false);
        controlHintLabel_ = ui_.AddLabel("A: USE DOOR / PICK UP    MENU: INVENTORY", {}, {720.0f,35.0f});
        if (controlHintLabel_ == nullptr) return false;
        auto* hintRoot = ui_.GetGuiSys().GetVRMenuMgr().ToObject(controlHintLabel_->GetParentHandle());
        if (hintRoot == nullptr) return false;
        hintRoot->SetLocalPose(OVR::Posef::Identity());
        controlHintLabel_->SetSurfaceVisible(0,false);
        controlHintLabel_->SetTextColor({0.82f,0.68f,0.25f,1.0f});
        controlHintLabel_->SetTextLocalScale({0.5f,0.5f,1.0f});
        controlHintLabel_->SetVisible(false);
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
        targetingFrameRenderer_.Init();
        return true;
    }

    void AppPaused(const xrJava* context) override {
        musicPlaybackActive_.store(false,std::memory_order_release);
        OVRFW::XrApp::AppPaused(context);
    }

    void AppShutdown(const xrJava* context) override {
        musicPlaybackActive_.store(false,std::memory_order_release);
        RevokeNativeClientBinding();
        DiscardInitialPreparation();
        // Defensive cleanup if no session reached SessionDestroying. AAudio
        // retains this pointer; close it before app audio state is destroyed.
        StopAmbientAudio();
        inventoryLabel_ = personaTabsLabel_ = personaDetailsLabel_ = personaFooterLabel_ = nullptr;
        hudLabel_ = nullptr;
        subtitleLabel_ = nullptr;
        controlHintLabel_ = nullptr;
        targetingFrameRenderer_.Shutdown();
        focusLabelRenderer_.Shutdown();
        if (focusLabelTextureId_!=0u) {glDeleteTextures(1,&focusLabelTextureId_);focusLabelTextureId_=0u;}
        DestroyOriginalCompassUi();
        ui_.Shutdown();
        DestroyOriginalPlayerHudUi();
        DestroyPlayerVisuals();
        DestroyHeldItemVisuals();
        ClearPendingStaticLightmapUpload();
        DestroySceneGeometry();
        BakedWorldGeometryRenderer::ShutdownSharedProgram();
        OVRFW::XrApp::AppShutdown(context);
    }

    static bool ValidateInitialRuntime(
        const std::shared_ptr<QuestVr::NativeClientLifetime>& clientLifetime,
        std::uint64_t clientIdentity, const QuestVr::NativeViewport& viewport,
        const std::shared_ptr<QuestVr::QuestInputSource>& inputSource) {
        try {
            std::vector<PortablePackageTables> scripts;
            scripts.reserve(QuestVr::RuntimeScriptPackages.size());
            for (const char* packageName : QuestVr::RuntimeScriptPackages) {
                scripts.push_back(LoadPortablePackageTables(
                    std::string(
                        "/data/user/0/dev.deusex.questvr.smoketest/files/DeusEx/System/") +
                    packageName + ".u"));
            }
            const PortablePackageTables trainingMap = LoadPortablePackageTables(
                "/data/user/0/dev.deusex.questvr.smoketest/files/DeusEx/Maps/00_Training.dx");
            auto musicSources=LoadPortableRuntimeMusicSources(scripts,trainingMap,
                "/data/user/0/dev.deusex.questvr.smoketest/files/DeusEx/System/DeusEx.ini",
                QuestVr::OriginalLinkerLoadProfile{false,true,true});
            ALOG("DeusExQuest: pre-map Music source admission: %zu packages/%zu actual import bindings",
                musicSources.packages.size(),musicSources.bindings.size());
            for(auto& source:musicSources.packages)scripts.push_back(std::move(source));
            const PortableRuntimeSummary runtime = InitializePortableRuntime(
                scripts, static_cast<std::uint32_t>(std::time(nullptr)),
                QuestVr::OriginalLinkerLoadProfile{false,true,true});
            if (!runtime.passed) {
                ALOG("DeusExQuest: persistent Unreal runtime validation failed");
                return false;
            }
            PreparePortableRuntimeDumpLocationServices(
                "/data/user/0/dev.deusex.questvr.smoketest/files/DeusEx/System/User.ini",
                "/data/user/0/dev.deusex.questvr.smoketest/files/DeusEx/System");
            // Training's null-Brush/null-Mesh colliders require the actual
            // Engine-owned Primitive. Prepare its defaults/typed config and
            // native lifetime on this CPU owner, before publishing the map.
            PreparePortableRuntimeNativeEngineClasses();
            PreparePortableRuntimeNativeMetadataClasses();
            PreparePortableRuntimeNativeConfigClasses();
            PreparePortableRuntimeNativePackages();
            PreparePortableRuntimeNativeInputClasses();
            PreparePortableRuntimeObjectLookup();
            const auto collisionEngine=PreparePortableRuntimeEngineCollision(
                "/data/user/0/dev.deusex.questvr.smoketest/files/DeusEx/System/DeusEx.ini");
            PreparePortableRuntimeInputSchema();
            PreparePortableRuntimeFlagBaseClasses();
            const auto system=PreparePortableRuntimeSystemServices(
                "/data/user/0/dev.deusex.questvr.smoketest/files/DeusEx/System/DeusEx.ini");
            BindPortableRuntimeUserConfigStorage(
                "/data/user/0/dev.deusex.questvr.smoketest/files",
                "/data/user/0/dev.deusex.questvr.smoketest/files/DeusEx/System/User.ini");
            PreparePortableRuntimeInputConfig(
                "/data/user/0/dev.deusex.questvr.smoketest/files/DeusEx/System/User.ini");
            PreparePortableRuntimeInputResetConfigSource(
                "/data/user/0/dev.deusex.questvr.smoketest/files/DeusEx/System/DefUser.ini");
            ALOG("DeusExQuest: rooted native System ready: %s; later input/world startup remains separate",system.objectPath.c_str());
            ALOG("DeusExQuest: collision Engine ready class=%s engine=%s primitive=%s; world startup remains separate",
                collisionEngine.engineClassPath.c_str(),collisionEngine.enginePath.c_str(),collisionEngine.cylinderPath.c_str());
            // This CPU worker receives only an immutable description of the
            // owner's real targets. It must never inspect GL/OpenXR handles.
            BindPortableRuntimeClient(clientLifetime, clientIdentity);
            RegisterPortableRuntimeViewport(clientIdentity, viewport);
            // Original engine-owned Client/Viewport precede map Login. This
            // materializes only the genuine registered stereo session objects;
            // Actor remains None until original Login/SetPlayer/Possess works.
            const auto playerSession=PreparePortableRuntimePlayerObjects(viewport.identity);
            BindPortableRuntimeQuestInputSource(viewport.identity,inputSource);
            ALOG("DeusExQuest: native player session objects ready client=%s viewport=%s class=%s bound=%s",
                playerSession.clientPath.c_str(),playerSession.viewportPath.c_str(),
                playerSession.viewportClassPath.c_str(),playerSession.bound?"true":"false");
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
            // The current playable milestone is the first Training level.
            // Cross-map lifecycle regression belongs in host tests, not in
            // every player startup (three extra loads and retirement passes).
            ALOG("DeusExQuest: Training-first startup retained the initial map; cross-map self-test deferred");
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
        RevokeNativeClientBinding();
        // No prior-session worker may retain ownership of the global portable
        // runtime/name tables when this session starts another bootstrap.
        DiscardInitialPreparation();
        XrReferenceSpaceCreateInfo floorInfo{};
        floorInfo.type = XR_TYPE_REFERENCE_SPACE_CREATE_INFO;
        floorInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL_FLOOR_EXT;
        floorInfo.poseInReferenceSpace.orientation.w = 1.0f;
        XrSpace floorSpace = XR_NULL_HANDLE;
        const XrResult floorResult = xrCreateReferenceSpace(Session, &floorInfo, &floorSpace);
        if (XR_SUCCEEDED(floorResult)) {
            gameplayFloorSpace_ = floorSpace;
            CurrentSpace = gameplayFloorSpace_;
            gameplayReferenceType_ = XR_REFERENCE_SPACE_TYPE_LOCAL_FLOOR_EXT;
            ALOG("DeusExQuest: floor-aware recenterable LOCAL_FLOOR tracking active");
        } else {
            CurrentSpace = StageSpace != XR_NULL_HANDLE ? StageSpace : LocalSpace;
            gameplayReferenceType_ = StageSpace != XR_NULL_HANDLE ?
                XR_REFERENCE_SPACE_TYPE_STAGE : XR_REFERENCE_SPACE_TYPE_LOCAL;
            ALOG("DeusExQuest: LOCAL_FLOOR unavailable (%d); reference fallback %d",
                static_cast<int>(floorResult), static_cast<int>(gameplayReferenceType_));
        }
        hasPreviousHeadStage_ = false;
        headTrackingValid_ = false;
        headTrackingActive_ = false;
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
        playerPosture_ = {};
        seatedCalibrationGate_.Reset();
        seatedPostureRequested_ = gameplayReferenceType_ != XR_REFERENCE_SPACE_TYPE_LOCAL &&
            ReadSeatedPosturePreference();
        postureCalibrationPending_ = seatedPostureRequested_;
        worldPosition_ = {};
        currentHeadStage_ = previousHeadStage_ = {};
        sceneYaw_ = 0.0f;
        selectedInventoryIndex_ = 0u;
        heldVisualCache_.Reset();
        selectedHeldVisual_.reset();
        equippedMeleeProfile_.reset();
        heldEquippedIdentity_.clear();
        playerVisualsCanRender_=false;
        inventoryNavigation_ = {};
        inventoryLayoutCached_ = false;
        debugHudVisible_ = true;
        hideDebugHudAtFirstReady_ = true;
        debugHudToggleLatch_ = true;
        personaPage_ = PersonaPage::Inventory;
        personaOriginalTextReady_ = false;
        SetInventoryMenuOpen(false);
        quickBeltVisibility_.Reset();
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
        hasPublishedRuntimeWorldRevision_ = false;
        runtimeActorTextureUploadPending_ = false;
        actorMaterialMetadata_.clear();
        actorMaterialProbeFallbackLogs_ = 0u;
        initialPreparationPending_ = true;
        initialPreparationFailed_ = false;
        pendingMapName_.clear();
        transitionMapName_.clear();
        transitionPhase_ = MapTransitionPhase::Idle;
        mapNames_.clear();
        if (hudLabel_ != nullptr) hudLabel_->SetText("DEUS EX VR\nSTARTING... PREPARING GAME DATA");
        try {
            // SessionInit follows the SDK's actual swapchain creation. Head
            // tracking, focus and SessionActive are deliberately not gates:
            // an existing target remains a viewport while rendering is paused.
            const QuestVr::NativeViewport viewport = CaptureNativeViewport();
            const auto clientLifetime = std::make_shared<QuestVr::NativeClientLifetime>();
            nativeClientLifetime_ = clientLifetime;
            const std::uint64_t clientIdentity = viewport.identity;
            const auto inputSource=std::make_shared<QuestVr::QuestInputSource>(clientLifetime,clientIdentity,viewport.identity);
            nativeQuestInputSource_=inputSource;questInputPublishFailureReported_=false;
            initialPreparationFuture_ = std::async(std::launch::async,
                [clientLifetime, clientIdentity, viewport,inputSource] {
                    return PrepareInitialRuntime(clientLifetime, clientIdentity, viewport,inputSource);
                });
        } catch (const std::exception& error) {
            RevokeNativeClientBinding();
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
        constexpr XrSpaceLocationFlags trackedHeadFlags =
            XR_SPACE_LOCATION_POSITION_TRACKED_BIT | XR_SPACE_LOCATION_ORIENTATION_TRACKED_BIT;
        headTrackingActive_ = headTrackingValid_ &&
            (location.locationFlags & trackedHeadFlags) == trackedHeadFlags;
        if (!headTrackingActive_) seatedCalibrationGate_.Reset();
        if (!headTrackingReported_ || previouslyValid != headTrackingValid_) {
            ALOG("DeusExQuest: head tracking %s xrLocateSpace=%d flags=0x%llx",
                headTrackingValid_ ? "valid" : "unavailable",
                static_cast<int>(result),
                static_cast<unsigned long long>(location.locationFlags));
            headTrackingReported_ = true;
        }
        if (headTrackingValid_) frame.HeadPose = FromXrPosef(location.pose);
        // The SDK's controller flag requires only a valid position. A visible
        // rigid hand (and its aim) also needs a valid orientation; never draw
        // an identity/stale hand merely because an action is still active.
        const XrSpace grips[]{RightControllerGripSpace,LeftControllerGripSpace};
        const XrSpace aims[]{RightControllerAimSpace,LeftControllerAimSpace};
        bool* tracked[]{&frame.RightRemoteTracked,&frame.LeftRemoteTracked};
        OVR::Posef* gripPoses[]{&frame.RightRemotePose,&frame.LeftRemotePose};
        OVR::Posef* aimPoses[]{&frame.RightRemotePointPose,&frame.LeftRemotePointPose};
        for (std::size_t hand = 0u; hand < 2u; ++hand) {
            if (!*tracked[hand]) continue;
            XrSpaceLocation grip{},aim{};
            grip.type = aim.type = XR_TYPE_SPACE_LOCATION;
            const auto time = static_cast<XrTime>(frame.PredictedDisplayTime*1e9);
            const XrResult gripResult = xrLocateSpace(grips[hand],CurrentSpace,time,&grip);
            const XrResult aimResult = xrLocateSpace(aims[hand],CurrentSpace,time,&aim);
            *tracked[hand] = XR_SUCCEEDED(gripResult) && XR_SUCCEEDED(aimResult) &&
                (grip.locationFlags & required) == required && (aim.locationFlags & required) == required;
            if (*tracked[hand]) {
                *gripPoses[hand] = FromXrPosef(grip.pose);
                *aimPoses[hand] = FromXrPosef(aim.pose);
            }
        }
        PublishQuestInputObservation(frame);
    }

    void PublishQuestInputObservation(const OVRFW::ovrApplFrameIn& frame) {
        if(!nativeQuestInputSource_||!nativeClientLifetime_||!nativeClientLifetime_->Alive())return;
        try {
            QuestVr::QuestInputObservation observation;
            observation.displayTime=frame.PredictedDisplayTime;observation.deltaSeconds=frame.DeltaSeconds;
            observation.focused=Focused;observation.headValid=headTrackingValid_;observation.headTracked=headTrackingActive_;
            observation.mounted=frame.HeadsetIsMounted;
            const auto pose=[](const OVR::Posef& value) {
                return QuestVr::QuestInputPose{{value.Translation.x,value.Translation.y,value.Translation.z},
                    {value.Rotation.x,value.Rotation.y,value.Rotation.z,value.Rotation.w}};
            };
            if(observation.headValid)observation.headPose=pose(frame.HeadPose);
            const std::array<int,7u> buttons{frame.kButtonA,frame.kButtonB,frame.kButtonX,frame.kButtonY,
                frame.kButtonMenu,frame.kButtonLeftThumbStick,frame.kButtonRightThumbStick};
            for(std::size_t index=0u;index<buttons.size();++index)
                if(frame.AllButtons&buttons[index])observation.buttons|=static_cast<std::uint8_t>(1u<<index);
            observation.hands[0u]={frame.LeftRemoteTracked,frame.LeftRemoteJoystick.x,frame.LeftRemoteJoystick.y,
                frame.LeftRemoteIndexTrigger,frame.LeftRemoteGripTrigger,{}, {}};
            observation.hands[1u]={frame.RightRemoteTracked,frame.RightRemoteJoystick.x,frame.RightRemoteJoystick.y,
                frame.RightRemoteIndexTrigger,frame.RightRemoteGripTrigger,{}, {}};
            if(frame.LeftRemoteTracked){observation.hands[0u].gripPose=pose(frame.LeftRemotePose);observation.hands[0u].aimPose=pose(frame.LeftRemotePointPose);}
            if(frame.RightRemoteTracked){observation.hands[1u].gripPose=pose(frame.RightRemotePose);observation.hands[1u].aimPose=pose(frame.RightRemotePointPose);}
            nativeQuestInputSource_->Publish(observation);
        }catch(const std::exception& error) {
            if(!questInputPublishFailureReported_)ALOG("DeusExQuest: input observation refused: %s",error.what());
            questInputPublishFailureReported_=true;
        }
    }

    void AppHandleEvent(XrEventDataBaseHeader* event) override {
        if (event->type != XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING) return;
        const auto& change = *reinterpret_cast<XrEventDataReferenceSpaceChangePending*>(event);
        if (change.session != Session || change.referenceSpaceType != gameplayReferenceType_) return;
        seatedCalibrationGate_.Reset();
        ALOG("DeusExQuest: reference change queued type=%d valid=%d",
            static_cast<int>(change.referenceSpaceType), static_cast<int>(change.poseValid));
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
            const float previousFloor = playerPosture_.virtualFloorY;
            const float previousWorldY = worldPosition_.y;
            if (change.poseValid && std::fabs(up.x) < 0.001f &&
                std::fabs(up.z) < 0.001f && up.y > 0.999f &&
                playerPosture_.ApplyRecenter(origin.Translation.y)) {
                const OVR::Vector3f right = origin.Rotation.Rotate(OVR::Vector3f(1.0f, 0.0f, 0.0f));
                const float originYaw = std::atan2(-right.z, right.x);
                if (selfBodyYawInitialized_)
                    selfBodyStageYaw_=std::remainder(selfBodyStageYaw_-originYaw,2.0f*QuestVr::Pi);
                const OVR::Vector3f oldHead = previousHeadStage_;
                const bool recenterable = gameplayReferenceType_ == XR_REFERENCE_SPACE_TYPE_LOCAL_FLOOR_EXT;
                if (recenterable) {
                    QuestVr::RecenterReferenceSpace(origin.Translation, originYaw,
                        worldPosition_, sceneYaw_, previousHeadStage_);
                } else {
                    QuestVr::RebaseReferenceSpace(origin.Translation, originYaw,
                        worldPosition_, sceneYaw_, previousHeadStage_);
                }
                // A standing floor stays at tracking Y=0 even when the runtime
                // reports a vertical reference-origin change. Preserve map
                // feet rather than sinking them by that origin's Y. Seated
                // calibration transforms its floor and retains the old math.
                worldPosition_.y = QuestVr::WorldYAfterVirtualFloorChange(
                    previousWorldY,previousFloor,playerPosture_.virtualFloorY);
                if (restorePoseAfterTransition_ && !restoredMapLocalPose_) {
                    OVR::Vector3f unusedHead = oldHead;
                    const float previousRestoredWorldY = restoredWorldPosition_.y;
                    if (recenterable) {
                        QuestVr::RecenterReferenceSpace(origin.Translation, originYaw,
                            restoredWorldPosition_, restoredSceneYaw_, unusedHead);
                    } else {
                        QuestVr::RebaseReferenceSpace(origin.Translation, originYaw,
                            restoredWorldPosition_, restoredSceneYaw_, unusedHead);
                    }
                    restoredWorldPosition_.y = QuestVr::WorldYAfterVirtualFloorChange(
                        previousRestoredWorldY,previousFloor,playerPosture_.virtualFloorY);
                }
                ALOG("DeusExQuest: reference change applied; map feet preserved, forward %s; originY=%.3f floor=%.3f->%.3f worldY=%.3f->%.3f",
                    recenterable ? "recentered" : "preserved",origin.Translation.y,
                    previousFloor,playerPosture_.virtualFloorY,previousWorldY,worldPosition_.y);
            } else {
                if (seatedPostureRequested_) {
                    playerPosture_.calibrated = false;
                    postureCalibrationPending_ = true;
                }
                if (hasPreviousHeadStage_) {
                    trackingResumeLocalHead_ = StageToLocal(previousHeadStage_, worldPosition_);
                }
                needsTrackingRebase_ = true;
                hasPreviousHeadStage_ = false;
                selfBodyYawInitialized_=false;
                ALOG("DeusExQuest: OpenXR reference discontinuity; rebasing on valid tracking");
            }
        }
    }

    void Update(const OVRFW::ovrApplFrameIn& frame) override {
        healthUiVisible_=beltUiVisible_=false;
        playerVisualsCanRender_=false;
        // Release any stale held input even through tracking/startup returns.
        // Time is presentation-only and never depends on runtime ownership.
        quickBeltVisibility_.SetHeld(false);
        if(std::isfinite(frame.DeltaSeconds) && frame.DeltaSeconds>=0.0f)
            quickBeltVisibility_.Tick(frame.DeltaSeconds);
        musicPlaybackActive_.store(Focused && runtimeAvailable_ && pendingMapName_.empty() &&
            transitionMapName_.empty(),std::memory_order_release);
        CompleteInitialPreparation();
        if (initialPreparationPending_ || initialPreparationFailed_) {
            // The bootstrap is the sole owner of portable runtime and NameString
            // storage. No map retry, mailbox diagnostic or gameplay query is safe.
            if (headTrackingValid_ && hudLabel_ != nullptr) {
                hudLabel_->SetLocalPose(QuestVr::HeadLockedHudPose(frame.HeadPose));
                ui_.Update(frame);
            }
            return;
        }
        PollDialogueAudioDecode();
        QuestVr::AdvanceDialoguePresentationClock(frame.DeltaSeconds,
            dialogueSubtitleSeconds_,dataLinkElapsedSeconds_);
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
        if (postureCalibrationPending_) {
            const bool calibrationReady = headTrackingActive_ && Focused &&
                pendingReferenceChanges_.empty() && QuestVr::GroundCollisionReady(
                    runtimeAvailable_,!pendingMapName_.empty(),!transitionMapName_.empty(),
                    runtimeActorTextureUploadPending_ || static_cast<bool>(actorGeometryBuild_));
            if (!seatedPostureRequested_ || seatedCalibrationGate_.Observe(
                currentHeadStage_.y,frame.DeltaSeconds,calibrationReady))
                SetPlayerPosture(seatedPostureRequested_,false);
        }
        const OVR::Vector3f saveHeadForward = frame.HeadPose.Rotation.Rotate({0.0f, 0.0f, -1.0f});
        if (std::hypot(saveHeadForward.x, saveHeadForward.z) > 0.0001f)
            currentHeadStageYaw_ = std::atan2(-saveHeadForward.x, -saveHeadForward.z);
        selfBodyStageYaw_=QuestVr::StableSelfBodyYaw(saveHeadForward,
            frame.HeadPose.Rotation.Rotate(OVR::Vector3f{1,0,0}),selfBodyStageYaw_,selfBodyYawInitialized_);
        selfBodyYawInitialized_=true;
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
        const bool mapLoading = !runtimeAvailable_ || ActorPublicationBlocksGameplay() ||
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
        quickBeltVisibility_.SetHeld(gameplayActive && Focused && frame.RightRemoteTracked &&
            frame.RightRemoteGripTrigger>0.75f);
        if (gameplayActive && Focused && !AdvanceRuntimeMoverPublication(frame.DeltaSeconds)) return;
        if (gameplayActive && Focused) PollTouchDataLinks(frame.DeltaSeconds);
        if (!mapLoading && !inventoryMenuOpen_ && frame.LeftRemoteTracked &&
            frame.Clicked(frame.kButtonLeftThumbStick)) {
            SetPlayerPosture(!seatedPostureRequested_,true);
        }
        const bool debugToggle = QuestVr::UpdateAxis(frame.LeftRemoteTracked ?
            frame.LeftRemoteGripTrigger : std::numeric_limits<float>::quiet_NaN(), debugHudToggleLatch_);
        if (!mapLoading && !inventoryMenuOpen_ && debugToggle) ToggleDebugHud();
        const bool turnPressed = QuestVr::UpdateAxis(frame.RightRemoteTracked ?
            frame.RightRemoteJoystick.x : std::numeric_limits<float>::quiet_NaN(), turnLatch_);
        if (gameplayActive && turnPressed) {
            constexpr float snapRadians = 3.14159265358979323846f / 6.0f;
            SnapTurnAroundHead(
                std::copysign(snapRadians, frame.RightRemoteJoystick.x),
                frame.HeadPose.Translation);
        }
        if (!mapLoading && inventoryMenuOpen_) {
            EnsureInventoryNavigation(GetPortableRuntimeInventoryItems());
            const auto navigation = menuNavigationInput_.Update(frame.RightRemoteJoystick.x,
                frame.RightRemoteJoystick.y, frame.RightRemoteTracked, frame.DeltaSeconds);
            const std::size_t pageCount = static_cast<std::size_t>(PersonaPage::Count);
            if (navigation.pageSteps != 0) {
                const auto page = static_cast<int>(personaPage_);
                const auto count = static_cast<int>(pageCount);
                personaPage_ = static_cast<PersonaPage>((page + navigation.pageSteps % count + count) % count);
                inventoryMenuDirty_ = true;
            }
            if (personaPage_ == PersonaPage::Inventory && navigation.itemSteps != 0)
                MoveInventorySelection(navigation.itemSteps);
            if (personaPage_ == PersonaPage::Inventory && frame.RightRemoteTracked &&
                frame.RightRemoteIndexTrigger > 0.75f && !fireLatch_)
                SelectInventoryAtPointer(frame);
        }
        const OVR::Vector3f stageRight = QuestVr::HorizontalHeadRight(
            frame.HeadPose.Rotation.Rotate(OVR::Vector3f(0.0f, 0.0f, -1.0f)),
            frame.HeadPose.Rotation.Rotate(OVR::Vector3f(1.0f, 0.0f, 0.0f)));
        const OVR::Vector3f localRight =
            QuestVr::StageDirectionToLocal(stageRight, sceneYaw_);
        // Construct the movement candidate after turning so accepting it cannot
        // overwrite the head-pivot translation computed by the snap turn.
        const auto moveStick = QuestVr::ApplyRadialDeadzone(
            frame.LeftRemoteJoystick.x, frame.LeftRemoteJoystick.y);
        OVR::Vector3f candidate = QuestVr::LocomotionCandidate(
            worldPosition_, stageRight,
            gameplayActive ? moveStick.x : 0.0f,
            gameplayActive ? moveStick.y : 0.0f,
            2.2f, frame.DeltaSeconds);
        const bool choiceCyclePressed = QuestVr::UpdateAxis(frame.RightRemoteTracked ?
            frame.RightRemoteJoystick.y : std::numeric_limits<float>::quiet_NaN(), choiceCycleLatch_);
        if (!inventoryMenuOpen_ && !pendingChoices_.empty() && choiceCyclePressed) {
            if (frame.RightRemoteJoystick.y > 0.0f) {
                pendingChoiceIndex_ = pendingChoiceIndex_ == 0u
                    ? pendingChoices_.size() - 1u
                    : pendingChoiceIndex_ - 1u;
            } else {
                pendingChoiceIndex_ = (pendingChoiceIndex_ + 1u) % pendingChoices_.size();
            }
            RefreshChoiceStatus();
        }

        // Never ground or roll back against a partially uploaded collision
        // mesh. An early, lower surface can otherwise bury the feet before
        // the real floor arrives beyond the normal 45 cm step-up limit.
        const bool actorCollisionPublicationPending = ActorCollisionPublicationPending();
        const bool collisionReady = QuestVr::GroundCollisionReady(
            runtimeAvailable_,!pendingMapName_.empty(),!transitionMapName_.empty(),actorCollisionPublicationPending);
        bool movementBlocked{};
        if (actorCollisionPublicationPending && pendingMapName_.empty() && transitionMapName_.empty()) {
            // Initial/authoritative publication, or leaving retained collision
            // coverage, cannot ground against a partial replacement. Spatial
            // refresh inside coverage keeps the complete live colliders active.
            candidate = QuestVr::RestoreHorizontalHeadPosition(
                worldPosition_,currentHeadStage_,safeLocalHead,sceneYaw_);
        } else if (collisionReady) {
            const OVR::Vector3f safeFeet{safeLocalHead.x,
                playerPosture_.virtualFloorY-worldPosition_.y,safeLocalHead.z};
            const OVR::Vector3f candidateFeet = StageToLocal(
                {currentHeadStage_.x,playerPosture_.virtualFloorY,currentHeadStage_.z},candidate);
            const auto grounded = QuestVr::ResolveGroundedHorizontalMotion(
                safeFeet,candidateFeet,
                [&](const OVR::Vector3f& feet) { return GroundHeightForFeet(feet); },
                [&](const OVR::Vector3f& feet) {
                    OVR::Vector3f sampleWorldPosition = QuestVr::RestoreHorizontalHeadPosition(
                        candidate,currentHeadStage_,feet,sceneYaw_);
                    sampleWorldPosition.y = playerPosture_.virtualFloorY-feet.y;
                    return CapsuleTouchesWall(currentHeadStage_,sampleWorldPosition);
                });
            movementBlocked = grounded.blocked;
            if (!movementBlocked) candidate.y = playerPosture_.virtualFloorY-grounded.feet.y;
        }
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
                if (!CapsuleTouchesWall(frame.HeadPose.Translation, grounded))
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
        UpdateActorGeometryPose(worldPose);
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
                try { BuildActorMarkers(); }
                catch (const std::exception& error) {
                    ALOG("DeusExQuest: retained actor streaming preparation failed: %s",error.what());
                    SuspendRuntimeActorPublication("ACTOR STREAMING FAILED - RESTART SESSION");
                    return;
                }
            }
        }
        if (!mapLoading &&
            inventoryMenuOpen_ && personaPage_ == PersonaPage::Inventory &&
            frame.RightRemoteTracked &&
            frame.Clicked(frame.kButtonA)) {
            const std::vector<std::string> inventory = GetPortableRuntimeInventoryItems();
            EnsureInventoryNavigation(inventory);
            if (!inventoryNavigation_.SelectedIndex()) {
                inventoryMenuDirty_ = true;
            } else {
                const auto path=inventoryNavigation_.SelectedIdentity();
                const bool equipped=EquipPortableRuntimeInventoryItem(path);
                if(equipped) {quickBeltVisibility_.Reveal();heldVisualCache_.Invalidate();}
                interactionStatus_ = equipped ? "EQUIPPED " + SelectedInventoryDisplayName() : "ITEM CANNOT BE EQUIPPED";
                inventoryActionStatus_ = interactionStatus_;
                hudQuerySeconds_=0.0f;
                interactionStatusSeconds_ = 2.0f;
                inventoryMenuDirty_ = true;
            }
        } else if (gameplayActive &&
            frame.RightRemoteTracked && frame.Clicked(frame.kButtonA)) {
            if (!pendingChoices_.empty()) {
                ConfirmPendingChoice();
            } else if (UseTargetedActor(frame.RightRemotePointPose) &&
                       pendingMapName_.empty() && transitionMapName_.empty()) {
                if (!TryRebuildRuntimeActorPublication()) return;
            }
        }
        const bool firePressed = frame.RightRemoteIndexTrigger > 0.75f;
        if (gameplayActive && pendingMapName_.empty() && transitionMapName_.empty() &&
            frame.RightRemoteTracked && firePressed && !fireLatch_) {
            const std::vector<std::string> inventory = GetPortableRuntimeInventoryItems();
            const float weaponDamage = SelectedWeaponDamage(inventory);
            if (PortableRuntimeKeyRingEquipped()) {
                if(UseTargetedActor(frame.RightRemotePointPose,false) && pendingMapName_.empty() && transitionMapName_.empty())
                    if(!TryRebuildRuntimeActorPublication()) return;
            } else if (weaponDamage <= 0.0f) {
                const auto selected=GetPortableRuntimeEquippedItem();
                if(selected.empty() || !UseSelectedConsumable(selected)) interactionStatus_ = "ITEM HAS NO TRIGGER ACTION";
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
                if (!TryRebuildRuntimeActorPublication()) return;
            }
        }
        fireLatch_ = firePressed;
        const bool gripPressed = frame.RightRemoteGripTrigger > 0.75f;
        if (gameplayActive && pendingMapName_.empty() && transitionMapName_.empty() &&
            frame.RightRemoteTracked && gripPressed && !inventoryCycleLatch_) {
            quickBeltVisibility_.Reveal();
            CyclePortableRuntimeQuickBelt(1);
            heldVisualCache_.Invalidate();
            const auto selected=GetPortableRuntimeEquippedItem();
            EnsureInventoryNavigation(GetPortableRuntimeInventoryItems());
            if(!inventoryNavigation_.SelectIdentity(selected)) selectedInventoryIndex_=invalidRendererIndex_;
            else selectedInventoryIndex_=inventoryNavigation_.SelectedIndex().value_or(invalidRendererIndex_);
            interactionStatus_ = PortableRuntimeKeyRingEquipped() ? "EQUIPPED KEY RING" : "EQUIPPED " + EquippedInventoryLabel();
            interactionStatusSeconds_=2.0f;hudQuerySeconds_=0.0f;
        }
        inventoryCycleLatch_ = gripPressed;
        if(gameplayActive && frame.RightRemoteTracked && frame.Clicked(frame.kButtonRightThumbStick)) {
            quickBeltVisibility_.Reveal();
            const auto belt=ReadPortableRuntimeQuickBelt();
            if(belt.slots[0u]) EquipPortableRuntimeInventoryItem(belt.slots[0u]->actorPath);
            heldVisualCache_.Invalidate();
            interactionStatus_="EQUIPPED KEY RING";interactionStatusSeconds_=2.0f;hudQuerySeconds_=0.0f;
        }
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
        if (gameplayActive && !actorGeometryBuild_ && !runtimeActorTextureUploadPending_ && mapTravelCooldown_ <= 0.0f &&
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
        PollRuntimeActorPublication();
        AdvanceRuntimeActorTexturePublication();
        if (runtimeAvailable_ && pendingMapName_.empty() && transitionPhase_ == MapTransitionPhase::Idle &&
            !runtimeActorTextureUploadPending_ && actorGeometryBuild_) {
            try {
                AdvanceActorGeometry();
            } catch (const std::exception& error) {
                ALOG("DeusExQuest: incremental actor geometry failed: %s", error.what());
                // A queued revision is not a successful GPU publication. Keep
                // no partial targets or stale audio and require map recovery.
                SuspendRuntimeActorPublication("ACTOR GEOMETRY FAILED - RESTART SESSION");
            }
        }
        AdvanceMapTransition();
        CompletePendingMapLoad();
        // A retained actor-bank refresh pauses gameplay, not presentation. Its
        // runtime is exclusively on this thread and the prior complete bank
        // remains visible, so keep the active speech/persona artwork visible.
        // Initial publication, atlas replacement and map workers still hide UI.
        const bool mapLoadingNow = !runtimeAvailable_ || runtimeActorTextureUploadPending_ ||
            !pendingMapName_.empty() || !transitionMapName_.empty() ||
            (actorGeometryBuild_ && activeActorGeometry_ == nullptr);
        if (hudLabel_ != nullptr) {
            hudLabel_->SetLocalPose(QuestVr::HeadLockedHudPose(frame.HeadPose));
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
        UpdateOriginalDataLinkUi(frame,mapLoadingNow);
        UpdateOriginalCompassUi(frame,mapLoadingNow);
        UpdateOriginalPlayerHudUi(frame,mapLoadingNow);
        UpdateHeldItemVisualSelection(frame.DeltaSeconds,mapLoadingNow);
        UpdateNpcWaitingAnimations(frame.DeltaSeconds,mapLoadingNow);
        if (subtitleLabel_ != nullptr) {
            auto pose = frame.HeadPose;
            pose.Translation += pose.Rotation.Rotate({0.0f,-0.35f,-1.4f});
            subtitleLabel_->SetLocalPose(pose);
            std::string text;
            if (dialogueSubtitleSeconds_ > 0.0f && !dialogueSubtitlePages_.empty()) {
                const float elapsed = std::clamp(1.0f-dialogueSubtitleSeconds_/
                    std::max(dialogueSubtitleTotalSeconds_,0.001f),0.0f,1.0f);
                const auto page = std::min(dialogueSubtitlePages_.size()-1u,
                    static_cast<std::size_t>(elapsed*dialogueSubtitlePages_.size()));
                text = dialogueSubtitlePages_[page];
            } else if (!debugHudVisible_ && !interactionStatus_.empty()) {
                text = WrapPersonaEntry(interactionStatus_,4u,58u);
            }
            if (text != displayedSubtitle_) {
                subtitleLabel_->SetText("%s",text.c_str());
                displayedSubtitle_ = text;
            }
            subtitleLabel_->SetVisible(!dataLinkUiVisible_ && !mapLoadingNow && !inventoryMenuOpen_ &&
                !text.empty());
        }
        if (controlHintLabel_ != nullptr) {
            auto pose = frame.HeadPose;
            pose.Translation += pose.Rotation.Rotate({0.0f,-0.54f,-1.4f});
            controlHintLabel_->SetLocalPose(pose);
            controlHintLabel_->SetVisible(!mapLoadingNow && !inventoryMenuOpen_ && !debugHudVisible_);
        }
        if(hudStateDiagnosticRequested_) {
            hudStateDiagnosticRequested_=false;
            ALOG("DeusExQuest: HUD presentation health=%d compass=%d speech=%d belt=%d alpha=%.3f; speech center=(%.4f %.4f) size=(%.4f %.4f); belt centerY=%.3f eyeFacing=true grip=%.3f trigger=%.3f",
                static_cast<int>(healthUiVisible_),static_cast<int>(compassUiVisible_),
                static_cast<int>(dataLinkUiVisible_),static_cast<int>(beltUiVisible_),quickBeltVisibility_.Alpha(),
                QuestVr::kHudDataLinkCenterX,QuestVr::kHudDataLinkCenterY,
                QuestVr::kHudDataLinkHalfWidth*2.0f,QuestVr::kHudDataLinkHalfHeight*2.0f,
                QuestVr::kPlayableBeltCenterY,frame.RightRemoteGripTrigger,frame.RightRemoteIndexTrigger);
        }
        if(heldStateDiagnosticRequested_) {
            heldStateDiagnosticRequested_=false;
            ALOG("DeusExQuest: held presentation equipment=%s model=%s cacheReady=%d drawReady=%d rightTracked=%d meleeDamage=%d rangeUnits=%d",
                heldEquippedIdentity_.c_str(),selectedHeldVisual_?heldItemKeys_[*selectedHeldVisual_].playerViewMesh.c_str():"generic hand",
                static_cast<int>(heldItemVisualsReady_),static_cast<int>(playerVisualsCanRender_),
                static_cast<int>(frame.RightRemoteTracked),equippedMeleeProfile_?equippedMeleeProfile_->hitDamageOriginal:0,
                equippedMeleeProfile_?equippedMeleeProfile_->maxRangeOriginal:0);
        }
        ui_.Update(frame);
    }

    void Render(
        const OVRFW::ovrApplFrameIn& frame,
        OVRFW::ovrRendererOutput& output) override {
        if (!headTrackingValid_) return;
        for (auto& renderer : worldRenderers_) renderer.Render(output.Surfaces);
        const auto eye = frame.HeadPose.Translation;
        const auto forward = frame.HeadPose.Rotation.Rotate(OVR::Vector3f(0,0,-1));
        struct BlendedDraw {
            float depth;
            BakedWorldGeometryRenderer* world;
            TexturedGeometryRenderer* actor;
        };
        std::vector<BlendedDraw> blended;
        for (auto& renderer : bakedWorldRenderers_) {
            if (renderer.IsBlended()) blended.push_back({renderer.DepthAlong(eye,forward),&renderer,nullptr});
            else renderer.Render(output.Surfaces);
        }
        const auto gatherActors = [&](auto& renderers) {
            for (auto& renderer : renderers) {
                if (renderer.IsBlended()) blended.push_back({renderer.DepthAlong(eye,forward),nullptr,&renderer});
                else renderer.Render(output.Surfaces);
            }
        };
        gatherActors(texturedRenderers_);
        if (activeActorGeometry_) {
            for (std::size_t index=0u;index<activeActorGeometry_->world.size();++index)
                if (activeActorGeometry_->visibility.Visible(activeActorGeometry_->markerOwners.at(index)))
                    activeActorGeometry_->world[index].Render(output.Surfaces);
            gatherActors(activeActorGeometry_->textured);
        }
        const std::size_t submittedBodySurfaces = RenderPlayerVisuals(frame, output.Surfaces);
        std::stable_sort(blended.begin(),blended.end(),[](const auto& a,const auto& b) {
            return a.depth > b.depth;
        });
        for (const auto& draw : blended) {
            if (draw.world) draw.world->Render(output.Surfaces);
            else draw.actor->Render(output.Surfaces);
        }
        RenderTargetingFrame(frame,output);
        if (healthUiVisible_) healthHudRenderer_.Render(output.Surfaces);
        if (beltUiVisible_) beltHudRenderer_.Render(output.Surfaces);
        if (compassUiVisible_) compassRenderer_.Render(output.Surfaces);
        if (inventoryMenuOpen_) personaRenderer_.Render(output.Surfaces);
        if (dataLinkUiVisible_) dataLinkRenderer_.Render(output.Surfaces);
        const std::size_t firstUiSurface = output.Surfaces.size();
        ui_.Render(frame, output);
        if (captureScreenshotRequested_ && captureScreenshotDelayFrames_ == 0u) {
            // Capture-time metadata distinguishes the asymmetric left-eye
            // projection from a genuinely displaced HUD; no per-frame spew.
            const auto forward = QuestVr::HeadLockedHudPose(frame.HeadPose).Translation;
            const auto projection = output.FrameMatrices.EyeProjection[0] * output.FrameMatrices.EyeView[0];
            const auto projected = projection.Transform(OVR::Vector4f(forward.x,forward.y,forward.z,1.0f));
            ALOG("DeusExQuest: screenshot poses head=(%.3f %.3f %.3f) grips right=%d left=%d forwardNdc=(%.4f %.4f) reference=%d",
                frame.HeadPose.Translation.x,frame.HeadPose.Translation.y,frame.HeadPose.Translation.z,
                static_cast<int>(frame.RightRemoteTracked),static_cast<int>(frame.LeftRemoteTracked),
                projected.x/projected.w,projected.y/projected.w,static_cast<int>(gameplayReferenceType_));
            const OVR::Posef gripPoses[]{frame.RightRemotePose,frame.LeftRemotePose};
            const OVR::Posef aimPoses[]{frame.RightRemotePointPose,frame.LeftRemotePointPose};
            const bool gripTracked[]{frame.RightRemoteTracked,frame.LeftRemoteTracked};
            for (std::size_t hand=0u;hand<2u;++hand) {
                if (!gripTracked[hand]) continue;
                const auto local=frame.HeadPose.Inverted()*gripPoses[hand];
                const auto aim=frame.HeadPose.Inverted()*aimPoses[hand];
                const auto x=local.Rotation.Rotate(OVR::Vector3f{1,0,0});
                const auto y=local.Rotation.Rotate(OVR::Vector3f{0,1,0});
                const auto z=local.Rotation.Rotate(OVR::Vector3f{0,0,1});
                const auto ray=aim.Rotation.Rotate(OVR::Vector3f{0,0,-1});
                ALOG("DeusExQuest: screenshot %s grip headLocal pos=(%.4f %.4f %.4f) quat=(%.5f %.5f %.5f %.5f) X=(%.4f %.4f %.4f) Y=(%.4f %.4f %.4f) Z=(%.4f %.4f %.4f) aimRay=(%.4f %.4f %.4f)",
                    hand==0u?"right":"left",local.Translation.x,local.Translation.y,local.Translation.z,
                    local.Rotation.x,local.Rotation.y,local.Rotation.z,local.Rotation.w,
                    x.x,x.y,x.z,y.x,y.y,y.z,z.x,z.y,z.z,ray.x,ray.y,ray.z);
            }
            ALOG("DeusExQuest: screenshot player seated=%d virtualFloor=%.3f eyeHeight=%.3f mapFeetY=%.3f",
                static_cast<int>(playerPosture_.seated),playerPosture_.virtualFloorY,
                frame.HeadPose.Translation.y-playerPosture_.virtualFloorY,
                playerPosture_.virtualFloorY-worldPosition_.y);
            if (compassUiVisible_ && activeMapNorth_) {
                const auto center=frame.HeadPose.Translation+frame.HeadPose.Rotation.Rotate(
                    OVR::Vector3f{QuestVr::kHudCompassCenterX,QuestVr::kHudCompassCenterY,-QuestVr::kHudVrDistance});
                const auto clip=projection.Transform(OVR::Vector4f(center.x,center.y,center.z,1.0f));
                ALOG("DeusExQuest: screenshot original compass mapNorth=%d leftEyeNdc=(%.4f %.4f); 73x40 original pixels",
                    activeMapNorth_->trueNorth,clip.x/clip.w,clip.y/clip.w);
            }
            const auto mapHead = StageToLocal(frame.HeadPose.Translation,worldPosition_);
            const auto mapFeet = StageToLocal(OVR::Vector3f{frame.HeadPose.Translation.x,
                playerPosture_.virtualFloorY,frame.HeadPose.Translation.z},worldPosition_);
            float visibleFloor = -std::numeric_limits<float>::infinity();
            ForNearbyTriangles(mapFeet,0,[&](const CollisionTriangle& triangle) {
                const auto height = QuestVr::GroundHeightAtXZ(mapFeet,triangle);
                if (height && *height <= mapHead.y) visibleFloor = std::max(visibleFloor,*height);
            });
            const auto eyeStage = output.FrameMatrices.EyeView[0].Inverted().Transform(OVR::Vector3f{});
            ALOG("DeusExQuest: screenshot map=%s world=(%.3f %.3f %.3f) mapHead=(%.3f %.3f %.3f) floorBelowEye=%.3f actualEyeHeight=%.3f submittedBodySurfaces=%zu eyeStageY=%.3f yaw=%.3f",
                currentMapName_.c_str(),worldPosition_.x,worldPosition_.y,worldPosition_.z,
                mapHead.x,mapHead.y,mapHead.z,visibleFloor,mapHead.y-visibleFloor,
                submittedBodySurfaces,eyeStage.y,sceneYaw_);
            for (std::size_t index = firstUiSurface; index < output.Surfaces.size(); ++index) {
                const auto& draw = output.Surfaces[index];
                if (draw.surface == nullptr || draw.surface->surfaceName.find("DEUS EX VR") == std::string::npos) continue;
                const auto center = draw.modelMatrix.Transform(OVR::Vector3f{});
                const auto relative = frame.HeadPose.Inverted().Transform(center);
                const auto clip = projection.Transform(OVR::Vector4f(center.x,center.y,center.z,1.0f));
                ALOG("DeusExQuest: screenshot HUD center headLocal=(%.4f %.4f %.4f) leftEyeNdc=(%.4f %.4f)",
                    relative.x,relative.y,relative.z,clip.x/clip.w,clip.y/clip.w);
            }
        }
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

    void SessionDestroying() override {
        // The SDK calls this before releasing any eye framebuffer. Atomically
        // revoke the token only: a startup/map worker can still own the global
        // runtime, so reset/GC waits for the existing joins in SessionEnd.
        RevokeNativeClientBinding();
    }

    void SessionEnd() override {
        if (gameplayFloorSpace_ != XR_NULL_HANDLE) {
            CurrentSpace = XR_NULL_HANDLE;
            xrDestroySpace(gameplayFloorSpace_);
            gameplayFloorSpace_ = XR_NULL_HANDLE;
        }
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
        hasPublishedRuntimeWorldRevision_ = false;
        hasTransitionHeadAnchor_ = false;
        ClearPendingPersonaRestore();
        preparedActorSnapshots_.clear();
        preparedNpcWaiting_.clear();
        preparedSpatialAudioEmitters_.clear();
        preparedMapLights_.clear();
        preparedMapNorth_.reset();activeMapNorth_.reset();
        preparedActorTextures_ = {};
        preparedWorldTexture_ = {};
        preparedWorldMesh_ = {};
        ClearPendingStaticLightmapUpload();
        DestroyPlayerVisuals();
        DestroyHeldItemVisuals();
        if (pendingWorldTextureId_ != 0u) {
            glDeleteTextures(1, &pendingWorldTextureId_);
            pendingWorldTextureId_ = 0u;
        }
        pendingWorldTextureRgba_.clear();
        ClearPendingActorTextureUpload();
        pendingWorldMesh_.chunks.clear();
        StopAmbientAudio();
        DestroyActorGeometry();
        for (auto& renderer : worldRenderers_) renderer.Shutdown();
        for (auto& renderer : texturedRenderers_) renderer.Shutdown();
        for (auto& renderer : bakedWorldRenderers_) renderer.Shutdown();
        worldRenderers_.clear();
        texturedRenderers_.clear();
        bakedWorldRenderers_.clear();
        moverVisuals_.clear();
        moverCollisionTriangleCount_ = 0u;
        moverPublicationFrames_ = moverPublicationLargestSubset_ = 0u;
        moverPublicationMilliseconds_ = moverPublicationWorstMilliseconds_ = 0.0;
        personaRenderer_.Shutdown();
        dataLinkRenderer_.Shutdown();
        DestroyOriginalCompassUi();
        DestroyOriginalPlayerHudUi();
        focusLabelRenderer_.Shutdown();
        if (focusLabelTextureId_!=0u) {
            glDeleteTextures(1,&focusLabelTextureId_);
            focusLabelTextureId_=0u;
        }
        focusDescriptionActor_.clear();
        focusLabelText_.clear();
        if (dataLinkTextureId_ != 0u) {
            glDeleteTextures(1,&dataLinkTextureId_);
            dataLinkTextureId_=0u;
        }
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
        actorAtlas_.reset();
        actorTexture_ = {};
        actorTexturePaths_.clear();
        actorTexturePolyFlags_.clear();
        actorMaskedTextureLayers_.clear();
        actorMaterialMetadata_.clear();
        actorMaterialProbeFallbackLogs_ = 0u;
        collisionTriangles_.clear();
        collisionGrid_.clear();
        oversizedCollisionTriangles_.clear();
        actorSnapshots_.clear();
        interactiveActors_.clear();
        ClearPortableRuntimeClient();
        nativeClientLifetime_.reset();
        nativeQuestInputSource_.reset();
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

    struct CollisionTriangle {
        OVR::Vector3f a, b, c, normal;
    };

    struct MoverVisual {
        OVR::Matrix4f inverseInitialTransform;
        OVR::Matrix4f deltaTransform = OVR::Matrix4f::Identity();
        std::vector<std::size_t> rendererIndices;
        std::vector<CollisionTriangle> initialCollision, collision;
        OVR::Vector3f minimum, maximum;
        bool hasBounds{};
        bool blocksPlayer{};
    };

    struct InteractiveActor {
        OVR::Vector3f localPosition;
        std::string objectPath;
        std::string classPath;
        bool travel{};
        std::string destinationMap;
        bool mover{};
        bool usable{}, highlight{};
        float collisionRadius{}, collisionHeight{};
        bool pawn{},inventory{},decoration{};
        bool presentationOnly{}; // Geometry/visibility slot, not a ray-use/fire target.
    };

    struct ActorAtlasGeneration {
        OVRFW::GlTexture texture;
        std::uint64_t generation{};
        ~ActorAtlasGeneration() { if (texture.IsValid()) OVRFW::FreeTexture(texture); }
    };

    struct NpcGpuClip {
        OVRFW::GlTexture texture;
        std::shared_ptr<const QuestVr::NpcAnimationClip> clip;
        ~NpcGpuClip() { if (texture.IsValid()) OVRFW::FreeTexture(texture); }
    };
    struct NpcAnimationDraw {
        std::size_t rendererIndex{};
        std::shared_ptr<NpcGpuClip> gpu;
        OVR::Matrix4f transform;
    };

    struct ActorGeometryBank {
        std::deque<OVRFW::GeometryRenderer> world;
        std::deque<TexturedGeometryRenderer> textured;
        std::unordered_map<std::string,MoverVisual> movers;
        std::vector<InteractiveActor> targets;
        std::shared_ptr<ActorAtlasGeneration> atlas;
        QuestVr::ActorVisibilityState visibility;
        std::set<std::string> lightingActors;
        TexturedGeometryRenderer::ActorVisibilityMatrices visibilityMatrices;
        std::deque<std::size_t> markerOwners;
        std::vector<NpcAnimationDraw> npcAnimations;
        OVR::Vector3f streamingCenter;
        bool spatiallyFiltered{};
        std::size_t collisionTriangles{};
        ActorGeometryBank() {
            for (std::size_t slot=0u;slot<QuestVr::ActorVisibilityState::MaximumActors;++slot)
                visibilityMatrices[QuestVr::ActorVisibilityState::Matrix(slot)].M
                    [QuestVr::ActorVisibilityState::Row(slot)][QuestVr::ActorVisibilityState::Column(slot)] = 1.0f;
        }
        void HideActor(const std::string& path) {
            if (!visibility.Hide(path)) return;
            const auto slot = *visibility.Slot(path);
            visibilityMatrices[QuestVr::ActorVisibilityState::Matrix(slot)].M
                [QuestVr::ActorVisibilityState::Row(slot)][QuestVr::ActorVisibilityState::Column(slot)] = 0.0f;
            for (auto& target : targets)
                if (QuestVr::ActorVisibilityKey(target.objectPath) == QuestVr::ActorVisibilityKey(path))
                    target.usable = target.highlight = false;
        }
        ~ActorGeometryBank() {
            for (auto& renderer:world) renderer.Shutdown();
            for (auto& renderer:textured) renderer.Shutdown();
        }
    };

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
        std::string moverPath;
        bool collisionOnly{};
        std::size_t actorOrdinal{QuestVr::ActorVisibilityState::MaximumActors};
        std::shared_ptr<const QuestVr::NpcAnimationClip> npcClip;
    };

    struct ActorGeometryBuild {
        QuestVr::RetainedBankToken publication;
        QuestVr::ActorRefreshKind kind{QuestVr::ActorRefreshKind::Spatial};
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
        // Borrow decoded immutable assets only while this thread owns runtime.
        // Map replacement cancels the build before its worker starts.
        std::map<std::string, const PortableLodMesh*> meshes;
        std::map<std::string, const PortableLodMesh*> brushes;
        std::map<std::string, std::set<std::uint16_t>> meshMaterials;
        std::unordered_map<std::string, std::size_t> textureLayers;
        OVRFW::GlGeometry::Descriptor spriteDescriptor;
        OVRFW::GlGeometry::Descriptor cubeDescriptor;
        OVRFW::GlGeometry::Descriptor markerChunk;
        OVRFW::GlGeometry::Descriptor texturedChunk;
        std::uint32_t texturedChunkPolyFlags{};
        std::string texturedChunkMoverPath;
        std::shared_ptr<const QuestVr::NpcAnimationClip> texturedChunkNpcClip;
        std::size_t texturedChunkNpcActor{QuestVr::ActorVisibilityState::MaximumActors};
        OVR::Matrix4f texturedChunkNpcTransform;
        std::size_t markerChunkActor{QuestVr::ActorVisibilityState::MaximumActors};
        std::deque<ActorGeometryPart> parts;
    };

    struct ActorMaterialMetadata {
        std::vector<std::string> texturePaths;
        std::vector<std::int32_t> materialTextureIndices;
        std::vector<std::pair<std::uint16_t, std::uint32_t>> materials;
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
        std::uint32_t polyFlags{};
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
        std::string actorPath;
        std::string soundPath;
        std::uint8_t soundPitch{};
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
        std::map<std::string,QuestVr::NpcWaitingInstance> npcWaiting;
        PortableTextureArray actorTextures;
        WorldTexturePreparation worldTexture;
        WorldMeshPreparation worldMesh;
        std::vector<SpatialAudioEmitter> spatialAudioEmitters;
        std::vector<MapLight> lights;
        QuestVr::MapMusicPreparation music;
        std::optional<PortableMapNorthSnapshot> mapNorth;
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
        PortableBitmapFont focusFont;
        QuestVr::PersonaUiImage focusBackground;
        QuestVr::FrobDescriptionMessages focusMessages;
        QuestVr::FrobDescriptionPalette focusPalette;
        QuestVr::PersonaUiImage dataLinkBase;
        QuestVr::HudCompassArtwork compass;
        QuestVr::HudBeltArtwork belt;
        QuestVr::HudHealthArtwork health;
        PortableBitmapFont hudTinyFont;
        std::unordered_map<std::string,QuestVr::PersonaUiImage> hudIcons;
        bool playerHudReady{};
        PortableBitmapFont dataLinkNameFont;
        PortableBitmapFont dataLinkTextFont;
        std::unordered_map<std::string,QuestVr::PersonaUiImage> dataLinkPortraits;
        std::array<std::vector<std::uint8_t>, 4> pageBaseRgba;
    };

    struct InitialPreparation {
        bool passed{};
        std::string error;
        MapPreparation map;
        std::vector<std::string> mapNames;
        AmbientPreparation ambient;
        PersonaPreparation persona;
        QuestVr::PlayerVisualAssets playerVisuals;
        QuestVr::HeldItemVisualAssets heldItemVisuals;
    };

    static InitialPreparation PrepareInitialRuntime(
        const std::shared_ptr<QuestVr::NativeClientLifetime>& clientLifetime,
        std::uint64_t clientIdentity, const QuestVr::NativeViewport& viewport,
        const std::shared_ptr<QuestVr::QuestInputSource>& inputSource) {
        InitialPreparation preparation;
        try {
            if (!ValidateInitialRuntime(clientLifetime, clientIdentity, viewport,inputSource))
                throw std::runtime_error("initial portable runtime validation failed; see quest_main log");
            preparation.mapNames = ReadMapCatalog();
            if (!BuildQuestMapCache(gameRoot_, "00_Training"))
                throw std::runtime_error("generic initial map cache build failed");
            const auto trainingMap = LoadPortablePackageTables(
                std::string(gameRoot_) + "/Maps/00_Training.dx");
            auto& map = preparation.map;
            map.actors = GetPortableRuntimeMapActors();
            std::vector<std::string> npcErrors;
            map.npcWaiting = QuestVr::PrepareNpcWaitingPresentations(map.actors,&npcErrors);
            for (const auto& error:npcErrors) ALOG("DeusExQuest: NPC waiting presentation refused: %s",error.c_str());
            ALOG("DeusExQuest: prepared %zu original-script NPC waiting presentations (render-only)",map.npcWaiting.size());
            map.mapNorth=PrepareOriginalMapNorth();
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
            // Music/dialogue must not depend on the optional old ambience WAV.
            if (!preparation.ambient.passed) {
                preparation.ambient.passed=true;preparation.ambient.sampleRate=48'000u;
            }
            map.music=QuestVr::PrepareOriginalMapMusic(gameRoot_,"00_Training",preparation.ambient.sampleRate);
            map.spatialAudioEmitters = PrepareSpatialAudioEmitters(
                map.actors, preparation.ambient.sampleRate, map.worldMesh.lightmap.unrealOrigin);
            preparation.persona = PrepareOriginalPersonaBackground();
            preparation.playerVisuals = QuestVr::LoadOriginalPlayerVisualAssets(gameRoot_);
            if (!preparation.playerVisuals.passed)
                ALOG("DeusExQuest: original player visuals unavailable: %s",
                    preparation.playerVisuals.error.c_str());
            preparation.heldItemVisuals=QuestVr::LoadOriginalHeldItemAssets(gameRoot_);
            if(!preparation.heldItemVisuals.passed)
                ALOG("DeusExQuest: original held-item CPU preparation unavailable: %s",
                    preparation.heldItemVisuals.error.c_str());
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
        CommitPlayerVisuals(std::move(preparation.playerVisuals));
        CommitHeldItemVisuals(std::move(preparation.heldItemVisuals));
        preparedActorSnapshots_ = std::move(preparation.map.actors);
        preparedNpcWaiting_ = std::move(preparation.map.npcWaiting);
        preparedActorTextures_ = std::move(preparation.map.actorTextures);
        preparedWorldTexture_ = std::move(preparation.map.worldTexture);
        preparedWorldMesh_ = std::move(preparation.map.worldMesh);
        preparedSpatialAudioEmitters_ = std::move(preparation.map.spatialAudioEmitters);
        preparedMapMusic_ = std::move(preparation.map.music);
        preparedMapNorth_ = std::move(preparation.map.mapNorth);
        preparedMapLights_ = std::move(preparation.map.lights);
        runtimeAvailable_ = true;
        transitionMapName_ = currentMapName_;
        transitionPhase_ = MapTransitionPhase::WorldTextureAllocate;
        displayedInventoryCount_ = invalidRendererIndex_;
        ALOG("DeusExQuest: initial CPU preparation committed; staged Training GPU upload pending");
    }

    void DestroyHeldItemVisuals() {
        selectedHeldVisual_.reset();
        heldEquippedIdentity_.clear();equippedMeleeProfile_.reset();
        heldVisualCache_.Reset();heldItemVisualsReady_=false;playerVisualsCanRender_=false;
        for(auto& variant:heldItemGpu_) {
            for(auto& renderer:variant.renderers) renderer.Shutdown();
            variant.renderers.clear();
            for(auto& texture:variant.textures) if(texture.IsValid()) OVRFW::FreeTexture(texture);
            variant.textures.clear();
        }
        heldItemKeys_={};
    }

    void CommitHeldItemVisuals(QuestVr::HeldItemVisualAssets assets) {
        DestroyHeldItemVisuals();
        if(!assets.passed) return; // Retain the already accepted generic hands.
        try {
            for(std::size_t index=0u;index<assets.variants.size();++index) {
                const auto& asset=assets.variants[index];
                const auto plan=QuestVr::BuildPlayerVisualTextureUploadPlan(asset.textures);
                if(asset.rightHandAndItem.triangles.empty())
                    throw std::runtime_error("Prepared held assembly has no original geometry");
                auto& gpu=heldItemGpu_[index];
                gpu.textures.resize(plan.size());
                for(const auto& upload:plan) {
                    GLuint texture{};glGenTextures(1,&texture);
                    gpu.textures.at(upload.textureIndex)=OVRFW::GlTexture(texture,GL_TEXTURE_2D_ARRAY,
                        static_cast<int>(upload.width),static_cast<int>(upload.height));
                    glBindTexture(GL_TEXTURE_2D_ARRAY,texture);
                    glTexStorage3D(GL_TEXTURE_2D_ARRAY,1,GL_RGBA8,upload.width,upload.height,1);
                    glTexSubImage3D(GL_TEXTURE_2D_ARRAY,0,0,0,0,upload.width,upload.height,1,
                        GL_RGBA,GL_UNSIGNED_BYTE,asset.textures.at(upload.textureIndex).image.rgba.data());
                    glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_WRAP_S,GL_REPEAT);
                    glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_WRAP_T,GL_REPEAT);
                    glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
                    glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
                }
                glBindTexture(GL_TEXTURE_2D_ARRAY,0);
                if(glGetError()!=GL_NO_ERROR) throw std::runtime_error("Original held texture upload failed");
                std::map<std::pair<std::uint32_t,std::uint32_t>,OVRFW::GlGeometry::Descriptor> groups;
                for(const auto& triangle:asset.rightHandAndItem.triangles) {
                    const auto flags=triangle.polyFlags|asset.textures.at(triangle.textureIndex).polyFlags;
                    auto& group=groups[{triangle.textureIndex,flags}];
                    if(group.attribs.position.size()+3u>65535u)
                        throw std::runtime_error("Prepared held assembly exceeds its GPU index budget");
                    for(const auto& vertex:triangle.vertices) {
                        group.indices.push_back(static_cast<std::uint16_t>(group.attribs.position.size()));
                        group.attribs.position.emplace_back(vertex.position.x,vertex.position.y,vertex.position.z);
                        group.attribs.normal.emplace_back(vertex.normal.x,vertex.normal.y,vertex.normal.z);
                        group.attribs.uv0.emplace_back(vertex.u,vertex.v);
                        group.attribs.color.emplace_back(0.0f,1.0f,1.0f,1.0f);
                    }
                }
                for(const auto& [key,descriptor]:groups) {
                    const auto [textureIndex,flags]=key;
                    gpu.renderers.emplace_back();
                    gpu.renderers.back().Init(descriptor,gpu.textures.at(textureIndex),
                        QuestVr::CullPlayerVisualPart(1u,flags),flags);
                }
                if(glGetError()!=GL_NO_ERROR) throw std::runtime_error("Original held geometry upload failed");
                auto& key=heldItemKeys_[index];
                key.classPath=asset.classPath;key.playerViewMesh=asset.meshPath;
                key.playerViewScale=asset.playerViewScale;key.skin=asset.materialOverrides.skin.path;
                for(std::size_t skin=0u;skin<key.multiSkins.size();++skin)
                    key.multiSkins[skin]=asset.materialOverrides.multiSkins[skin].path;
                key.supportsNativeStarterKeyRing=asset.supportsNativeStarterKeyRing;
                ALOG("DeusExQuest: held assembly GPU ready class=%s mesh=%s surfaces=%zu original=%zu derivedCuff=%zu nativeTextures=%zu",
                    key.classPath.c_str(),key.playerViewMesh.c_str(),gpu.renderers.size(),
                    asset.rightHandAndItem.originalTriangleCount,asset.rightHandAndItem.derivedClosureTriangleCount,
                    gpu.textures.size());
            }
            heldItemVisualsReady_=true;
        } catch(const std::exception& error) {
            DestroyHeldItemVisuals();
            ALOG("DeusExQuest: held assembly GPU rejected; generic hands retained: %s",error.what());
        }
    }

    bool CanReadEquippedItemVisual() const {
        return runtimeAvailable_ && !initialPreparationPending_ && !initialPreparationFuture_.valid() &&
            !mapCacheFuture_.valid() && pendingMapName_.empty() && transitionMapName_.empty();
    }

    void UpdateHeldItemVisualSelection(float seconds,bool mapLoading) {
        // Only the frame owner may inspect the live equipment graph. Rendering
        // consumes this detached result and never touches runtime or packages.
        if(mapLoading || !CanReadEquippedItemVisual()) {
            selectedHeldVisual_.reset();heldEquippedIdentity_.clear();equippedMeleeProfile_.reset();
            heldVisualCache_.Invalidate();return;
        }
        playerVisualsCanRender_=!inventoryMenuOpen_ && GetPortableRuntimePlayerHealth()>0.0f;
        std::string observedEquipment;
        std::uint64_t observedRevision{};
        bool observed{};
        try {
            heldVisualCache_.Tick(seconds);
            const auto revision=GetPortableRuntimeWorldRevision();
            // Reconciliation inspects owned inventory. Do not repeat it at
            // display frequency; input/mutation paths invalidate immediately.
            if(!heldVisualCache_.NeedsRefresh(heldEquippedIdentity_,currentMapName_,revision)) return;
            observedEquipment=heldEquippedIdentity_;observedRevision=revision;observed=true;
            const auto equipped=GetPortableRuntimeEquippedItem();
            observedEquipment=equipped;observedRevision=revision;observed=true;
            const auto previous=selectedHeldVisual_;
            const bool identityChanged=equipped!=heldEquippedIdentity_;
            selectedHeldVisual_.reset();equippedMeleeProfile_.reset();heldEquippedIdentity_=equipped;
            std::optional<PortableHeldItemVisualDescriptor> descriptor;
            if(!equipped.empty() && !QuestVr::EquippedItemVisualDetail::Same(equipped,QuestVr::kQuickBeltKeyRingIdentity))
                descriptor=ReadPortableRuntimeHeldItemVisualDescriptor(equipped);
            const auto* actual=descriptor?&*descriptor:nullptr;
            equippedMeleeProfile_=QuestVr::ResolveEquippedMeleeProfile(equipped,actual);
            const auto prepared=heldItemVisualsReady_?std::span<const QuestVr::PreparedHeldVisualKey>(heldItemKeys_):
                std::span<const QuestVr::PreparedHeldVisualKey>{};
            const auto selected=QuestVr::SelectEquippedItemVisual(equipped,actual,prepared);
            if(selected.preparedIndex && *selected.preparedIndex<heldItemGpu_.size() &&
                !heldItemGpu_[*selected.preparedIndex].renderers.empty()) selectedHeldVisual_=selected.preparedIndex;
            heldVisualCache_.Refreshed(equipped,currentMapName_,revision);
            heldItemVisualError_.clear();
            if(identityChanged || previous!=selectedHeldVisual_)
                ALOG("DeusExQuest: held selection equipment=%s model=%s status=%u meleeDamage=%d rangeUnits=%d",
                    equipped.c_str(),selectedHeldVisual_?heldItemKeys_[*selectedHeldVisual_].playerViewMesh.c_str():"generic hand",
                    static_cast<unsigned>(selected.status),equippedMeleeProfile_?equippedMeleeProfile_->hitDamageOriginal:0,
                    equippedMeleeProfile_?equippedMeleeProfile_->maxRangeOriginal:0);
        } catch(const std::exception& error) {
            selectedHeldVisual_.reset();equippedMeleeProfile_.reset();
            if(observed) heldVisualCache_.Refreshed(observedEquipment,currentMapName_,observedRevision);
            else heldVisualCache_.Invalidate();
            if(heldItemVisualError_!=error.what()) {
                heldItemVisualError_=error.what();ALOG("DeusExQuest: held selection unavailable: %s",error.what());
            }
        }
    }

    void DestroyPlayerVisuals() {
        for (auto& parts : playerVisualRenderers_) {
            for (auto& renderer : parts) renderer.Shutdown();
            parts.clear();
        }
        for (auto& texture : playerVisualTextures_) {
            if (texture.IsValid()) OVRFW::FreeTexture(texture);
            texture = {};
        }
    }

    void CommitPlayerVisuals(QuestVr::PlayerVisualAssets assets) {
        DestroyPlayerVisuals();
        if (!assets.passed || assets.textures.empty()) return;
        try {
            const auto plan = QuestVr::BuildPlayerVisualTextureUploadPlan(assets.textures);
            // Original JC skins are 128x128 / 128x256, while WeaponHandsTex is
            // 256x256. Native-size one-layer arrays retain UV/bilinear sampling
            // without resizing, padding or assuming all images have one size.
            for (const auto& upload : plan) {
                GLuint texture{};
                glGenTextures(1, &texture);
                playerVisualTextures_.at(upload.textureIndex) = OVRFW::GlTexture(
                    texture, GL_TEXTURE_2D_ARRAY, static_cast<int>(upload.width),
                    static_cast<int>(upload.height));
                glBindTexture(GL_TEXTURE_2D_ARRAY, texture);
                glTexStorage3D(GL_TEXTURE_2D_ARRAY, 1, GL_RGBA8,
                    static_cast<GLsizei>(upload.width), static_cast<GLsizei>(upload.height), 1);
                glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, 0,
                    static_cast<GLsizei>(upload.width), static_cast<GLsizei>(upload.height), 1,
                    GL_RGBA, GL_UNSIGNED_BYTE, assets.textures.at(upload.textureIndex).image.rgba.data());
                glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_REPEAT);
                glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_REPEAT);
                glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            }
            glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
            if (glGetError() != GL_NO_ERROR)
                throw std::runtime_error("original player texture upload failed");
            const QuestVr::PlayerVisualPart* parts[]{&assets.lowerBody,&assets.rightHand,&assets.leftHand};
            for (std::size_t part = 0u; part < 3u; ++part) {
                std::map<std::pair<std::uint32_t,std::uint32_t>,OVRFW::GlGeometry::Descriptor> groups;
                for (const auto& triangle : parts[part]->triangles) {
                    const auto flags = triangle.polyFlags | assets.textures.at(triangle.textureIndex).polyFlags;
                    auto& group = groups[{triangle.textureIndex,flags}];
                    if (group.attribs.position.size() + 3u > 65535u)
                        throw std::runtime_error("player visual exceeds GPU index bound");
                    for (const auto& vertex : triangle.vertices) {
                        group.indices.push_back(static_cast<std::uint16_t>(group.attribs.position.size()));
                        group.attribs.position.emplace_back(vertex.position.x,vertex.position.y,vertex.position.z);
                        group.attribs.normal.emplace_back(vertex.normal.x,vertex.normal.y,vertex.normal.z);
                        group.attribs.uv0.emplace_back(vertex.u,vertex.v);
                        group.attribs.color.emplace_back(0.0f,1.0f,1.0f,1.0f);
                    }
                }
                for (const auto& [key, descriptor] : groups) {
                    const auto [textureIndex,flags] = key;
                    playerVisualRenderers_[part].emplace_back();
                    playerVisualRenderers_[part].back().Init(descriptor, playerVisualTextures_.at(textureIndex),
                        QuestVr::CullPlayerVisualPart(part,flags), flags);
                    // Selected original torso coverage and body-only camera
                    // clearance replace the old uncullable cut-off waist.
                    // Authored material culling, alpha and depth are retained.
                }
            }
            if (glGetError() != GL_NO_ERROR)
                throw std::runtime_error("original player geometry upload failed");
            ALOG("DeusExQuest: player visuals ready: %zu original self-body, each hand %zu original + %zu derived cuff triangles; right-grip local with mirrored left, body clearance %.2fm, rigid no IK",
                assets.lowerBody.originalTriangleCount,assets.rightHand.originalTriangleCount,
                assets.rightHand.derivedClosureTriangleCount,QuestVr::SelfBodyRearClearanceMeters);
        } catch (const std::exception& error) {
            DestroyPlayerVisuals();
            ALOG("DeusExQuest: player visuals rejected: %s",error.what());
        }
    }

    std::size_t RenderPlayerVisuals(const OVRFW::ovrApplFrameIn& frame,
        std::vector<OVRFW::ovrDrawSurface>& surfaces) {
        // Cached presentation only: a startup/map worker may own the runtime.
        if(!playerVisualsCanRender_ || !headTrackingValid_ || !Focused) return 0u;
        std::size_t bodySurfaces{};
        const OVR::Posef body = QuestVr::FeetLocalSelfBodyPose(
            frame.HeadPose,selfBodyStageYaw_,playerPosture_.virtualFloorY);
        // Prepared hands already use canonical OpenXR grip-local axes. The
        // former Glock-specific -90deg rotation must not be applied again.
        const OVR::Posef poses[]{body,frame.RightRemotePose,frame.LeftRemotePose};
        const bool visible[]{gameplayReferenceType_ != XR_REFERENCE_SPACE_TYPE_LOCAL &&
            !postureCalibrationPending_ && frame.HeadPose.Translation.y-playerPosture_.virtualFloorY > 1.05f,
            frame.RightRemoteTracked,frame.LeftRemoteTracked};
        for (std::size_t part = 0u; part < 3u; ++part) {
            if (!visible[part]) continue;
            if(part==1u && selectedHeldVisual_ && *selectedHeldVisual_<heldItemGpu_.size()) {
                const auto index=*selectedHeldVisual_;
                if(!heldItemGpu_[index].renderers.empty()) {
                    for(auto& renderer:heldItemGpu_[index].renderers) {
                        renderer.SetPose(frame.RightRemotePose);renderer.Update();renderer.Render(surfaces);
                    }
                    continue; // Replace the generic right hand, never overlay it.
                }
            }
            for (auto& renderer : playerVisualRenderers_[part]) {
                renderer.SetPose(poses[part]);
                renderer.Update();
                renderer.Render(surfaces);
                if (part == 0u) ++bodySurfaces;
            }
        }
        return bodySurfaces;
    }

    void RevokeNativeClientBinding() {
        if (nativeClientLifetime_) nativeClientLifetime_->Revoke();
    }

    QuestVr::NativeViewport CaptureNativeViewport() {
        if (Session == XR_NULL_HANDLE || GetNumFramebuffers() != 2)
            throw std::runtime_error("native client requires the created stereo XR session");
        QuestVr::NativeViewport viewport{};
        viewport.eyeCount = 2u;
        for (int eye = 0; eye < GetNumFramebuffers(); ++eye) {
            const ovrFramebuffer* frameBuffer = GetFrameBuffer(eye);
            if (frameBuffer == nullptr || frameBuffer->ColorSwapChain.Handle == XR_NULL_HANDLE ||
                frameBuffer->Width <= 0 || frameBuffer->Height <= 0 ||
                frameBuffer->TextureSwapChainLength == 0u ||
                frameBuffer->TextureSwapChainLength > QuestVr::NativeClientRegistry::MaximumSwapchainImages ||
                frameBuffer->ColorSwapChainImage == nullptr || frameBuffer->FrameBuffers == nullptr ||
                frameBuffer->DepthBuffers == nullptr ||
                frameBuffer->ColorSwapChain.Width != static_cast<std::uint32_t>(frameBuffer->Width) ||
                frameBuffer->ColorSwapChain.Height != static_cast<std::uint32_t>(frameBuffer->Height))
                throw std::runtime_error("native client eye target has no created color swapchain");
            const std::uint32_t width = static_cast<std::uint32_t>(frameBuffer->Width);
            const std::uint32_t height = static_cast<std::uint32_t>(frameBuffer->Height);
            if (width > QuestVr::NativeClientRegistry::MaximumDimension ||
                height > QuestVr::NativeClientRegistry::MaximumDimension)
                throw std::runtime_error("native client eye target dimensions exceed descriptor bounds");
            for (std::uint32_t image = 0u; image < frameBuffer->TextureSwapChainLength; ++image) {
                if (frameBuffer->ColorSwapChainImage[image].image == 0u ||
                    frameBuffer->FrameBuffers[image] == 0u || frameBuffer->DepthBuffers[image] == 0u)
                    throw std::runtime_error("native client eye target has incomplete image resources");
            }
            if (eye == 0) {
                viewport.width = width;
                viewport.height = height;
                viewport.swapchainImages = frameBuffer->TextureSwapChainLength;
            } else if (viewport.width != width || viewport.height != height ||
                viewport.swapchainImages != frameBuffer->TextureSwapChainLength) {
                throw std::runtime_error("native client stereo eye targets disagree");
            }
        }
        if (nativeClientGeneration_ == std::numeric_limits<std::uint64_t>::max())
            throw std::runtime_error("native client session identity exhausted");
        // Do not use an XR/GL address as identity: those handles can be reused
        // by the next session, whereas a revoked token must stay revoked.
        viewport.identity = ++nativeClientGeneration_;
        ALOG("DeusExQuest: native stereo viewport %llu: %ux%u, %u images per eye, 2 eyes",
            static_cast<unsigned long long>(viewport.identity), viewport.width, viewport.height,
            viewport.swapchainImages);
        return viewport;
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

    bool ActorPublicationBlocksGameplay() const {
        return runtimeActorTextureUploadPending_ || QuestVr::ActorRefreshBlocksGameplay(
            activeActorGeometry_ != nullptr, static_cast<bool>(actorGeometryBuild_),
            actorGeometryBuild_ ? actorGeometryBuild_->kind : QuestVr::ActorRefreshKind::Spatial);
    }

    bool ActorCollisionPublicationPending() const {
        if (ActorPublicationBlocksGameplay()) return true;
        if (!actorGeometryBuild_ || !activeActorGeometry_) return false;
        const auto player = StageToLocal(currentHeadStage_,worldPosition_);
        return !QuestVr::RetainedActorCoverageContains(activeActorGeometry_->spatiallyFiltered,
            actorStreamingCenter_.x,actorStreamingCenter_.z,player.x,player.z);
    }

    void ResetActorRemovalState() { actorRemovals_.Reset(); }

    bool IsPublishedActorVisible(const std::string& path) const {
        if (!activeActorGeometry_) return false;
        const auto slot = activeActorGeometry_->visibility.Slot(path);
        return slot && activeActorGeometry_->visibility.Visible(*slot);
    }

    bool PublishRuntimeActorRemoval(const std::string& path) {
        if (!activeActorGeometry_ || !activeActorGeometry_->visibility.Slot(path)) return false;
        // Other actors' baked vertex colors may consume this actor's light.
        // Masking its triangles alone cannot reproduce that wider effect.
        if (activeActorGeometry_->lightingActors.count(QuestVr::ActorVisibilityKey(path))) return false;
        const auto started = std::chrono::steady_clock::now();
        actorRemovals_.Remove(path);
        activeActorGeometry_->HideActor(path);
        if (auto* staged = actorGeometryPublication_.Staged()) staged->HideActor(path);
        const auto identity = QuestVr::ActorVisibilityKey(path);
        for (auto& target : interactiveActors_)
            if (QuestVr::ActorVisibilityKey(target.objectPath) == identity)
                target.usable = target.highlight = false;
        for (auto it=moverVisuals_.begin();it!=moverVisuals_.end();)
            if (QuestVr::ActorVisibilityKey(it->first) == identity) it=moverVisuals_.erase(it); else ++it;
        {
            std::lock_guard<std::mutex> lock(audioMutex_);
            spatialAudioEmitters_.erase(std::remove_if(spatialAudioEmitters_.begin(),spatialAudioEmitters_.end(),
                [&](const auto& emitter) { return QuestVr::ActorVisibilityKey(emitter.actorPath) == identity; }),
                spatialAudioEmitters_.end());
        }
        inventoryMenuDirty_ = true;
        displayedInventoryCount_ = invalidRendererIndex_;
        ALOG("DeusExQuest: actor removal delta %.3f ms for %.256s; active/staged masked, no world rebuild or geometry upload",
            std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count(),path.c_str());
        return true;
    }

    void BuildActorMarkers(QuestVr::ActorRefreshKind kind = QuestVr::ActorRefreshKind::Spatial) {
        if (actorGeometryBuild_ && kind == QuestVr::ActorRefreshKind::Spatial) return;
        const auto started = std::chrono::steady_clock::now();
        CancelActorGeometryBuild();
        if (!actorAtlas_ || !actorTexture_.IsValid())
            throw std::runtime_error("Actor bank preparation has no retained atlas");
        // Moving brushes do not bump the general actor revision every frame.
        // A later spatial refresh must not rebuild a now-open idle door from
        // its older closed snapshot. Full reflected mover snapshots cost >100ms
        // on Quest. Seed from the authoritative actor snapshot, then retain all
        // published motion deltas instead of repeating those reads per bank.
        if (kind == QuestVr::ActorRefreshKind::Authoritative || !activeActorGeometry_)
            latestMoverSnapshots_.Replace(actorSnapshots_);
        for (auto& actor : actorSnapshots_) {
            if (!actor.mover) continue;
            if (const auto* live = latestMoverSnapshots_.Find(actor.objectPath)) actor = *live;
        }
        actorGeometryBuild_ = std::make_unique<ActorGeometryBuild>();
        ActorGeometryBuild& build = *actorGeometryBuild_;
        build.kind = kind;
        auto bank = std::make_unique<ActorGeometryBank>();
        bank->atlas = actorAtlas_;
        build.publication = actorGeometryPublication_.Begin(std::move(bank),
            {actorSceneGeneration_,GetPortableRuntimeWorldRevision(),actorAtlas_->generation});
        auto& staged = *actorGeometryPublication_.Staged();
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
        staged.streamingCenter = playerLocal;
        staged.spatiallyFiltered = actorSnapshots_.size() > 1000u;
        build.spriteDescriptor = BuildCrossedSpriteDescriptor();
        build.cubeDescriptor = OVRFW::BuildUnitCubeDescriptor();
        for (std::size_t layer = 0; layer < actorTexturePaths_.size(); ++layer) {
            build.textureLayers.emplace(FoldActorAssetPath(actorTexturePaths_[layer]), layer);
        }
        // Targeting/travel metadata is available immediately, independently of
        // delayed visual chunks. Rendering is not limited to usable classes:
        // original WoodFragment births are Actors, not Decorations/Inventory.
        // Reserve visibility/collider slots for EVERY mover in the retained
        // coverage first. The 512 visual cap must not discard a nearby closed
        // door merely because pickups appeared earlier in export order.
        for (const auto group : {QuestVr::ActorSceneSelection::ActorSceneGroup::Mover,
                QuestVr::ActorSceneSelection::ActorSceneGroup::Gameplay,
                QuestVr::ActorSceneSelection::ActorSceneGroup::Presentation}) {
        for (std::size_t index = 0u; index < actorSnapshots_.size(); ++index) {
            const PortableActorSnapshot& actor = actorSnapshots_[index];
            if (QuestVr::ActorSceneSelection::SelectActorSceneGroup(actor) != group || actorRemovals_.Removed(actor.objectPath)) continue;
            const OVR::Vector3f position(
                (actor.y - originY) * unitsToMeters,
                (actor.z - originZ) * unitsToMeters + 1.0f,
                -(actor.x - originX) * unitsToMeters);
            if (actorSnapshots_.size() > 1000u) {
                const float dx = position.x - playerLocal.x;
                const float dz = position.z - playerLocal.z;
                if (dx * dx + dz * dz > 25.0f * 25.0f) continue;
            }
            if (build.actorIndices.size() >= QuestVr::ActorVisibilityState::MaximumActors) {
                if (group == QuestVr::ActorSceneSelection::ActorSceneGroup::Mover) throw std::runtime_error("Complete retained mover coverage exceeds actor visibility budget");
                break;
            }
            build.actorIndices.push_back(index);
            const auto ordinal = staged.visibility.Add(actor.objectPath,actorRemovals_);
            if (actor.light) staged.lightingActors.insert(QuestVr::ActorVisibilityKey(actor.objectPath));
            if (ordinal+1u != build.actorIndices.size())
                throw std::runtime_error("Actor visibility ordinal lost geometry ownership");
            // Cache the authored collision cylinder and PC highlight policy at
            // publication. Aiming never asks for a full runtime-world snapshot.
            const auto readFloat = [&](const char* name) {
                try {
                    const auto value = ReadPortableActorScriptProperty(actor.objectPath,name);
                    return value.kind == QuestVr::Vm::Kind::Float && std::isfinite(value.floating) &&
                        value.floating >= 0.0f ? value.floating*unitsToMeters : 0.0f;
                } catch (const std::exception&) { return 0.0f; }
            };
            const auto readBool = [&](const char* name, bool fallback) {
                try {
                    const auto value = ReadPortableActorScriptProperty(actor.objectPath,name);
                    return value.kind == QuestVr::Vm::Kind::Bool ? value.boolean : fallback;
                } catch (const std::exception&) { return fallback; }
            };
            const bool usable = !actor.hidden && (actor.pawn || actor.inventory || actor.decoration ||
                actor.travel || (actor.trigger && actor.drawType != 0u) ||
                (actor.mover && readBool("bFrobbable",false)));
            const bool highlight = usable && !actor.trigger && readBool("bHighlight",true);
            // Presentation-only entries share ordinal/visibility ownership but
            // remain non-usable and non-highlighted; no pickup/target behavior
            // or physics state is inferred from merely having a visible mesh.
            staged.targets.push_back({position, actor.objectPath, actor.classPath,
                actor.travel, actor.destinationMap, actor.mover,usable,highlight,
                readFloat("CollisionRadius"),readFloat("CollisionHeight"),
                actor.pawn,actor.inventory,actor.decoration,
                group == QuestVr::ActorSceneSelection::ActorSceneGroup::Presentation});
            if (build.actorIndices.size() >= QuestVr::ActorVisibilityState::MaximumActors) {
                if (group != QuestVr::ActorSceneSelection::ActorSceneGroup::Mover) break;
            }
        }
        if (build.actorIndices.size() >= QuestVr::ActorVisibilityState::MaximumActors) break;
        }
        ALOG("DeusExQuest: queued incremental geometry for %zu scene actors; budget %.1f ms/%zu operations/%zu vertices",
             build.actorIndices.size(), actorGeometryMillisecondsPerFrame_,
             actorGeometryOperationsPerFrame_, actorGeometryChunkVertices_);
        ALOG("DeusExQuest: actor bank snapshot selection %.2f ms; %s refresh, retained gameplay %s",
            std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count(),
            kind == QuestVr::ActorRefreshKind::Spatial ? "spatial" : "authoritative",
            ActorPublicationBlocksGameplay() ? "blocked" : "active");
    }

    void UpdateActorGeometryPose(const OVR::Posef& worldPose) {
        if (!activeActorGeometry_) return;
        for (auto& renderer : activeActorGeometry_->world) {
            renderer.SetPose(worldPose);
            renderer.Update();
        }
        for (auto& renderer : activeActorGeometry_->textured) {
            renderer.SetPose(worldPose);
            renderer.Update();
        }
        for (const auto& [path,visual] : moverVisuals_)
            for (const auto index : visual.rendererIndices)
                activeActorGeometry_->textured.at(index).SetModelMatrix(OVR::Matrix4f(worldPose)*visual.deltaTransform);
    }

    static std::string FoldActorAssetPath(std::string path) {
        for (auto& character : path)
            if (character >= 'A' && character <= 'Z') character += 'a' - 'A';
        return path;
    }

    void NormalizeActorMaskedTextureAliases() {
        // Saved references can spell the same UObject differently. The CPU
        // atlas may retain both spellings while only one requested masking;
        // pair every alias with the same actual masked layer before geometry.
        std::unordered_map<std::string, std::size_t> layers;
        for (std::size_t index = 0u; index < actorTexturePaths_.size(); ++index)
            layers.emplace(FoldActorAssetPath(actorTexturePaths_[index]), index);
        for (std::size_t index = 0u; index < actorMaskedTextureLayers_.size(); ++index) {
            if (actorMaskedTextureLayers_[index] >= 0) continue;
            const auto masked = layers.find(FoldActorAssetPath(actorTexturePaths_[index]) + "#masked");
            if (masked != layers.end()) actorMaskedTextureLayers_[index] = static_cast<std::int32_t>(masked->second);
        }
    }

    const ActorMaterialMetadata* ActorAssetMaterialMetadata(
        const std::string& path, const bool brush) {
        const auto key = std::string(brush ? "brush:" : "mesh:") + FoldActorAssetPath(path);
        const auto found = actorMaterialMetadata_.find(key);
        if (found != actorMaterialMetadata_.end()) return &found->second;
        if (actorMaterialMetadata_.size() >= 4096u)
            throw std::runtime_error("Actor material metadata cache exceeds its map budget");
        // Extract small immutable material metadata once; do not copy the
        // already-decoded triangle array on every asset probe/revision.
        // This runs only on the current portable-runtime owner, never a worker.
        const PortableLodMesh* mesh{};
        try {
            mesh = &(brush ? GetPortableRuntimeBrushView(path) : GetPortableRuntimeMeshView(path));
        } catch (const std::runtime_error& error) {
            // These getters' precise unavailable-asset contract is also handled
            // by PrepareNextActorGeometry's per-asset fallback. Do not turn an
            // unsupported authored brush into a whole-world suspension merely
            // while probing its materials. No successful metadata is cached,
            // so later asset preparation can make the same identity usable.
            const std::string_view reason(error.what());
            const std::string_view unavailable = brush ? "Portable actor brush is not decoded: " :
                "Portable actor mesh is not decoded: ";
            if (reason.substr(0u, unavailable.size()) != unavailable ||
                reason.substr(unavailable.size()) != path) throw;
            if (actorMaterialProbeFallbackLogs_ < 8u) {
                ++actorMaterialProbeFallbackLogs_;
                ALOG("DeusExQuest: actor material probe retains per-asset fallback for %.256s: %.384s",
                    path.c_str(), error.what());
            }
            return nullptr;
        }
        ActorMaterialMetadata metadata;
        metadata.texturePaths = mesh->texturePaths;
        metadata.materialTextureIndices = mesh->materialTextureIndices;
        std::set<std::pair<std::uint16_t, std::uint32_t>> materials;
        for (const auto& vertex : mesh->triangles) materials.emplace(vertex.material, vertex.polyFlags);
        metadata.materials.assign(materials.begin(), materials.end());
        return &actorMaterialMetadata_.emplace(key, std::move(metadata)).first->second;
    }

    bool ActorTextureResourcesMissing(const std::vector<PortableActorSnapshot>& actors) {
        std::unordered_map<std::string, std::size_t> available;
        for (std::size_t index = 0u; index < actorTexturePaths_.size(); ++index)
            available.emplace(FoldActorAssetPath(actorTexturePaths_[index]), index);
        const auto missing = [&](const std::string& path, const bool masked) {
            if (path.empty()) return false;
            const auto found = available.find(FoldActorAssetPath(path));
            if (!actorTexture_.IsValid() || found == available.end()) return true;
            const auto index = found->second;
            const bool needsMask = masked ||
                (index < actorTexturePolyFlags_.size() && (actorTexturePolyFlags_[index] & 2u) != 0u);
            if (!needsMask) return false;
            if (index >= actorMaskedTextureLayers_.size()) return true;
            const auto maskedIndex = actorMaskedTextureLayers_[index];
            return maskedIndex < 0 || static_cast<std::size_t>(maskedIndex) >= actorTexturePaths_.size() ||
                FoldActorAssetPath(actorTexturePaths_[static_cast<std::size_t>(maskedIndex)]) !=
                    FoldActorAssetPath(path) + "#masked";
        };
        for (const auto& actor : actors) {
            const bool sprite = actor.drawType == 1u || actor.drawType == 4u ||
                actor.drawType == 5u || actor.drawType == 7u;
            if (sprite && missing(actor.texturePath, actor.style == 2u)) return true;
            const auto meshMissing = [&](const std::string& path, const bool brush) {
                if (path.empty()) return false;
                const auto* metadata = ActorAssetMaterialMetadata(path, brush);
                if (!metadata) return false;
                for (const auto& [material, sourceFlags] : metadata->materials) {
                    const auto selected = QuestVr::ResolveActorMeshMaterial(actor.materialOverrides,
                        metadata->texturePaths, metadata->materialTextureIndices, material);
                    if (missing(selected.texturePath, (sourceFlags & 2u) != 0u || actor.style == 2u))
                        return true;
                }
                return false;
            };
            if (meshMissing(actor.meshPath, false) ||
                (actor.mover && meshMissing(actor.brushPath, true))) return true;
        }
        return false;
    }

    void RebuildRuntimeActorPublication() {
        const auto started = std::chrono::steady_clock::now();
        // The caller owns the loaded runtime. A background map worker or staged
        // transition must never race this snapshot/revision read.
        if (!runtimeAvailable_ || runtimeActorTextureUploadPending_ || !pendingMapName_.empty() || !transitionMapName_.empty() ||
            transitionPhase_ != MapTransitionPhase::Idle)
            throw std::runtime_error("Actor publication requested while the map runtime is unavailable");
        const auto revision = GetPortableRuntimeWorldRevision();
        auto snapshots = GetPortableRuntimeMapActors();
        auto lights = BuildMapLights(snapshots, activeMapUnrealOrigin_);
        PortableTextureArray replacementTextures;
        // Include inactive actors, as map preparation does: quick-load/pickup
        // revival must not depend on the currently visible subset. A new Skin,
        // sprite, mesh material or masked variant needs fresh layer ordinals.
        const bool replaceTextures = ActorTextureResourcesMissing(GetPortableRuntimeMapActors(true));
        if (replaceTextures) {
            const auto started = std::chrono::steady_clock::now();
            replacementTextures = BuildPortableRuntimeActorTextureArray(96u, 96u);
            if (!replacementTextures.passed)
                throw std::runtime_error("Runtime actor material preparation failed");
            ALOG("DeusExQuest: missing runtime actor materials prepared on runtime owner in %.2f ms; %zu layers; headset frame cost unverified",
                std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count(),
                replacementTextures.texturePaths.size());
        }
        // Retire only staging jobs before replacing the snapshot ordinal list.
        // The complete visible actor bank, colliders and old atlas stay alive
        // until its complete replacement publishes atomically.
        CancelActorGeometryBuild();
        actorSnapshots_ = std::move(snapshots);
        activeMapLights_ = std::move(lights);
        // Refresh the actor-lighting input only. Static BSP lightmaps still
        // describe their map-preparation bake, not a dynamic lighting solver.
        ReconcileSpatialAudioEmitters(actorSnapshots_);
        if (replaceTextures) {
            if (!BeginActorTextureUpload(std::move(replacementTextures)))
                throw std::runtime_error("Runtime actor material GPU allocation failed");
            runtimeActorTextureUploadPending_ = true;
            interactionStatus_ = "UPDATING ACTOR MATERIALS...";
            interactionStatusSeconds_ = 3.0f;
        } else BuildActorMarkers(QuestVr::ActorRefreshKind::Authoritative);
        publishedRuntimeWorldRevision_ = revision;
        hasPublishedRuntimeWorldRevision_ = true;
        inventoryMenuDirty_ = true;
        displayedInventoryCount_ = invalidRendererIndex_;
        ALOG("DeusExQuest: authoritative actor snapshot/resources %.2f ms; %zu live actors; material replacement=%s",
            std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count(),
            actorSnapshots_.size(),replaceTextures ? "yes" : "no");
    }

    void SuspendRuntimeActorPublication(const char* status) {
        // The VM commit/save restoration already succeeded. A render failure
        // cannot roll it back. Invalidate the queued revision and all active
        // actor consumers, retaining immutable map clips for later recovery.
        runtimeAvailable_ = false;
        hasPublishedRuntimeWorldRevision_ = false;
        restorePoseAfterTransition_ = false;
        CancelActorGeometryBuild();
        ClearPendingActorTextureUpload();
        {
            std::lock_guard<std::mutex> lock(audioMutex_);
            spatialAudioEmitters_.clear();
            ambientSamples_.clear();
            ambientCursor_ = 0u;
        }
        interactionStatus_ = status;
        interactionStatusSeconds_ = 10.0f;
    }

    bool TryRebuildRuntimeActorPublication() {
        try {
            RebuildRuntimeActorPublication();
            return true;
        } catch (const std::exception& error) {
            // A committed VM transaction cannot be undone by a renderer error.
            // Do not keep stale/deleted actor chunks or report a successful
            // publication. Suspend unsafe runtime input until map recovery.
            SuspendRuntimeActorPublication("ACTOR VISUAL PUBLICATION FAILED - RESTART SESSION");
            ALOG("DeusExQuest: committed actor revision publication failed: %s", error.what());
            return false;
        }
    }

    void PollRuntimeActorPublication() {
        if (!runtimeAvailable_ || runtimeActorTextureUploadPending_ || !pendingMapName_.empty() || !transitionMapName_.empty() ||
            transitionPhase_ != MapTransitionPhase::Idle) return;
        const auto revision = GetPortableRuntimeWorldRevision();
        if (hasPublishedRuntimeWorldRevision_ && revision == publishedRuntimeWorldRevision_) return;
        if (TryRebuildRuntimeActorPublication())
            ALOG("DeusExQuest: committed runtime actor revision %llu queued; %zu live actors; incremental GPU rebuild, static BSP/lightmap unchanged",
                static_cast<unsigned long long>(revision), actorSnapshots_.size());
    }

    static OVR::Matrix4f ActorTransformMatrix(const QuestVr::ActorTransform& transform) {
        OVR::Matrix4f result = OVR::Matrix4f::Identity();
        for (std::size_t row=0u; row<3u; ++row)
            for (std::size_t column=0u; column<3u; ++column)
                result.M[row][column] = transform.linear.values[row*3u+column];
        result.M[0][3] = transform.translation.x;
        result.M[1][3] = transform.translation.y;
        result.M[2][3] = transform.translation.z;
        return result;
    }

    static CollisionTriangle TransformMoverCollision(const CollisionTriangle& triangle,
        const OVR::Matrix4f& delta, const OVR::Matrix3f& normalTransform) {
        const auto normal = normalTransform.Transform(triangle.normal);
        return {delta.Transform(triangle.a),delta.Transform(triangle.b),
            delta.Transform(triangle.c),normal.LengthSq() > 0.000001f ? normal.Normalized() : OVR::Vector3f{}};
    }

    static void ExpandMoverBounds(MoverVisual& visual, const CollisionTriangle& triangle) {
        for (const auto point : {triangle.a,triangle.b,triangle.c}) {
            if (!visual.hasBounds) {
                visual.minimum = visual.maximum = point;
                visual.hasBounds = true;
            } else {
                visual.minimum.x = std::min(visual.minimum.x,point.x);
                visual.minimum.y = std::min(visual.minimum.y,point.y);
                visual.minimum.z = std::min(visual.minimum.z,point.z);
                visual.maximum.x = std::max(visual.maximum.x,point.x);
                visual.maximum.y = std::max(visual.maximum.y,point.y);
                visual.maximum.z = std::max(visual.maximum.z,point.z);
            }
        }
    }

    void ApplyMoverPose(const PortableActorSnapshot& actor,
        std::unordered_map<std::string,MoverVisual>& visuals,
        std::vector<InteractiveActor>& targets) {
        const auto found = visuals.find(actor.objectPath);
        if (found == visuals.end()) return; // Its streamed brush is not ready yet.
        auto& visual = found->second;
        const auto transform = QuestVr::BuildSnapshotActorTransform(actor,
            {activeMapUnrealOrigin_.x,activeMapUnrealOrigin_.y,activeMapUnrealOrigin_.z},true);
        const auto delta = ActorTransformMatrix(transform)*visual.inverseInitialTransform;
        visual.blocksPlayer = actor.blockPlayers && actor.collideActors;
        bool changed{};
        for (std::size_t row=0u; row<3u; ++row)
            for (std::size_t column=0u; column<4u; ++column)
                changed = changed || delta.M[row][column] != visual.deltaTransform.M[row][column];
        if (!changed) return;
        visual.deltaTransform = delta;
        const auto normalTransform = OVR::Matrix3f(delta).Inverse().Transposed();
        visual.hasBounds = false;
        for (std::size_t i=0u; i<visual.initialCollision.size(); ++i) {
            visual.collision[i] = TransformMoverCollision(visual.initialCollision[i],delta,normalTransform);
            ExpandMoverBounds(visual,visual.collision[i]);
        }
        // The authored pivot is often beside a door, not on its visible face.
        // Triangle targeting below is authoritative for movers; keep the point
        // marker current too for the other existing targeting consumers.
        for (auto& target : targets)
            if (target.objectPath == actor.objectPath)
                target.localPosition = visual.hasBounds ? (visual.minimum+visual.maximum)*0.5f :
                    OVR::Vector3f{transform.translation.x,transform.translation.y,transform.translation.z};
    }

    void PublishMoverPose(const PortableActorSnapshot& actor) {
        latestMoverSnapshots_.Observe(actor);
        ApplyMoverPose(actor,moverVisuals_,interactiveActors_);
        if (auto* staged = actorGeometryPublication_.Staged())
            ApplyMoverPose(actor,staged->movers,staged->targets);
    }

    void RebaseStagedMoverPoses() {
        auto* staged = actorGeometryPublication_.Staged();
        if (!staged) return;
        const auto started = std::chrono::steady_clock::now();
        for (const auto& entry : latestMoverSnapshots_.Entries())
            ApplyMoverPose(entry.second,staged->movers,staged->targets);
        for (auto it=staged->movers.begin();it!=staged->movers.end();)
            if (actorRemovals_.Removed(it->first)) it=staged->movers.erase(it); else ++it;
        ALOG("DeusExQuest: actor bank retained live mover rebase %.3f ms; %zu current mover snapshots",
            std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count(),
            latestMoverSnapshots_.Entries().size());
    }

    bool PlayerTouchesMover(const MoverVisual& visual) const {
        if (!visual.blocksPlayer || !visual.hasBounds) return false;
        constexpr float radius = 0.28f;
        const auto center = StageToLocal({currentHeadStage_.x,playerPosture_.virtualFloorY+0.85f,
            currentHeadStage_.z},worldPosition_);
        if (center.x+radius < visual.minimum.x || center.x-radius > visual.maximum.x ||
            center.z+radius < visual.minimum.z || center.z-radius > visual.maximum.z ||
            center.y+0.85f < visual.minimum.y || center.y-0.85f > visual.maximum.y) return false;
        for (const auto& triangle : visual.collision) {
            for (const auto height : {0.3f,0.85f,1.4f}) {
                const auto point = StageToLocal({currentHeadStage_.x,playerPosture_.virtualFloorY+height,
                    currentHeadStage_.z},worldPosition_);
                if ((point-ClosestPointOnTriangle(point,triangle)).LengthSq() < radius*radius) return true;
            }
        }
        return false;
    }

    bool AdvanceRuntimeMoverPublication(const float seconds) {
        try {
            const auto started = std::chrono::steady_clock::now();
            if (!AdvancePortableRuntimeMovers(seconds)) {
                if (moverPublicationFrames_ != 0u) {
                    ALOG("DeusExQuest: mover publication completed %zu frames; mean %.3f ms worst %.3f ms; largest moving subset %zu",
                        moverPublicationFrames_,moverPublicationMilliseconds_/moverPublicationFrames_,
                        moverPublicationWorstMilliseconds_,moverPublicationLargestSubset_);
                    moverPublicationFrames_ = moverPublicationLargestSubset_ = 0u;
                    moverPublicationMilliseconds_ = moverPublicationWorstMilliseconds_ = 0.0;
                }
                return true;
            }
            const auto movers = GetPortableRuntimeMoverActors(true);
            for (const auto& mover : movers) PublishMoverPose(mover);
            bool corrected{};
            for (const auto& mover : movers) {
                const auto found = moverVisuals_.find(mover.objectPath);
                if (mover.moverClosing && found != moverVisuals_.end() && PlayerTouchesMover(found->second))
                    corrected = BlockPortableRuntimeMover(mover.objectPath) || corrected;
            }
            if (corrected)
                for (const auto& mover : GetPortableRuntimeMoverActors(true)) PublishMoverPose(mover);
            const double milliseconds = std::chrono::duration<double,std::milli>(
                std::chrono::steady_clock::now()-started).count();
            ++moverPublicationFrames_;
            moverPublicationLargestSubset_ = std::max(moverPublicationLargestSubset_,movers.size());
            moverPublicationMilliseconds_ += milliseconds;
            moverPublicationWorstMilliseconds_ = std::max(moverPublicationWorstMilliseconds_,milliseconds);
            return true;
        } catch (const std::exception& error) {
            ALOG("DeusExQuest: live mover publication failed: %s",error.what());
            SuspendRuntimeActorPublication("DOOR MOTION FAILED - RESTART SESSION");
            return false;
        }
    }

    void AdvanceRuntimeActorTexturePublication() {
        if (!runtimeActorTextureUploadPending_) return;
        try {
            if (!runtimeAvailable_ || !pendingMapName_.empty() || !transitionMapName_.empty() ||
                transitionPhase_ != MapTransitionPhase::Idle)
                throw std::runtime_error("Runtime actor atlas upload lost exclusive publication ownership");
            if (!UploadActorTextureLayers(2u))
                throw std::runtime_error("Runtime actor atlas layer upload failed");
            if (pendingActorTextureLayersUploaded_ == pendingActorTextureLayers_) {
                runtimeActorTextureUploadPending_ = false;
                // Every actor chunk was retired before atlas allocation. Build
                // only against the completed replacement's current ordinals.
                BuildActorMarkers(QuestVr::ActorRefreshKind::Authoritative);
                ALOG("DeusExQuest: runtime actor material publication complete; geometry queued, BSP/static atlas retained");
            }
        } catch (const std::exception& error) {
            ALOG("DeusExQuest: runtime actor material publication failed: %s", error.what());
            SuspendRuntimeActorPublication("ACTOR MATERIAL UPLOAD FAILED - RESTART SESSION");
        }
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
        actorGeometryBuild_->parts.back().actorOrdinal = actorGeometryBuild_->nextActor;
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
        part.actorOrdinal = actorGeometryBuild_->nextActor;
        actorGeometryBuild_->parts.push_back(std::move(part));
    }

    std::shared_ptr<NpcGpuClip> NpcClipForUpload(const std::shared_ptr<const QuestVr::NpcAnimationClip>& clip) {
        const auto found = npcGpuClips_.find(clip.get());
        if (found != npcGpuClips_.end()) return found->second;
        auto gpu = std::make_shared<NpcGpuClip>();
        gpu->clip = clip;
        GLint maximum{};glGetIntegerv(GL_MAX_TEXTURE_SIZE,&maximum);
        if (!clip || clip->vertices>static_cast<std::uint32_t>(maximum) ||
            clip->frames>static_cast<std::uint32_t>(maximum) ||
            clip->positionsRgba.size()!=static_cast<std::size_t>(clip->vertices)*clip->frames*4u)
            throw std::runtime_error("NPC vertex clip exceeds device texture bounds");
        GLuint texture{};glGenTextures(1,&texture);
        gpu->texture=OVRFW::GlTexture(texture,GL_TEXTURE_2D,clip->vertices,clip->frames);
        glBindTexture(GL_TEXTURE_2D,texture);
        glTexStorage2D(GL_TEXTURE_2D,1,GL_RGBA32F,clip->vertices,clip->frames);
        glTexSubImage2D(GL_TEXTURE_2D,0,0,0,clip->vertices,clip->frames,GL_RGBA,GL_FLOAT,clip->positionsRgba.data());
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_2D,0);
        if(glGetError()!=GL_NO_ERROR) throw std::runtime_error("Original NPC vertex clip upload failed");
        npcGpuClips_.emplace(clip.get(),gpu);
        ALOG("DeusExQuest: NPC clip GPU ready mesh=%s sequence=%s vertices=%u frames=%u bytes=%zu",
            clip->meshPath.c_str(),clip->sequence.c_str(),clip->vertices,clip->frames,clip->positionsRgba.size()*sizeof(float));
        return gpu;
    }

    void UpdateNpcWaitingAnimations(float elapsed,bool mapLoading) {
        if (mapLoading || !activeActorGeometry_ || !std::isfinite(elapsed) || elapsed<0.0f) return;
        npcAnimationSeconds_ += static_cast<double>(elapsed);
        for (const auto& draw:activeActorGeometry_->npcAnimations)
            activeActorGeometry_->textured.at(draw.rendererIndex).SetAnimation(
                QuestVr::SampleNpcAnimationFrame(*draw.gpu->clip,npcAnimationSeconds_),draw.transform);
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
        auto actor = actorSnapshots_[build.actorIndices[ordinal]];
        std::shared_ptr<const QuestVr::NpcAnimationClip> npcClip;
        const auto waiting = npcWaiting_.find(actor.objectPath);
        if (waiting!=npcWaiting_.end() && QuestVr::NpcWaitingMatchesSnapshot(waiting->second,actor)) {
            npcClip = waiting->second.clip;
            actor.animation = {};
            actor.animation.sequence = npcClip->sequence;
            actor.prePivotX=waiting->second.prePivot.x;
            actor.prePivotY=waiting->second.prePivot.y;
            actor.prePivotZ=waiting->second.prePivot.z;
        }
        auto& staged = *actorGeometryPublication_.Staged();
        if (!staged.visibility.Visible(ordinal)) {
            // A pickup can remove this very actor while its immutable pose
            // worker is unfinished. Release the build's pending slot before
            // advancing, but retain/reap the future without an XR-thread wait.
            QuestVr::AbandonPendingAsyncResult(build.pendingMeshPose,actorPoseEpoch_,
                actorPoseJobEpoch_,actorPoseFuture_.valid());
            ReapRetiredActorPose();
            return true;
        }
        const OVR::Vector3f position = staged.targets[ordinal].localPosition;
        const bool brushActor = actor.mover && !actor.brushPath.empty();
        const bool hidden = actor.hidden || actor.drawType == 0u;
        if (hidden) {
            ++build.hiddenActors;
            if (!brushActor) return true;
        }
        if (!actor.mover && (actor.drawScale == 0.0f || actor.drawScaleX == 0.0f ||
            actor.drawScaleY == 0.0f || actor.drawScaleZ == 0.0f)) return true;
        const auto light = actor.unlit ? OVR::Vector3f(1.0f) :
            CalculateMapLighting(position,{0.0f,1.0f,0.0f});
        bool rendered = false;
        const std::string assetPath = brushActor ? actor.brushPath : actor.meshPath;
        if (!assetPath.empty()) {
            try {
                auto& cache = brushActor ? build.brushes : build.meshes;
                auto found = cache.find(assetPath);
                if (found == cache.end()) found = cache.emplace(assetPath,&(brushActor ?
                    GetPortableRuntimeBrushView(assetPath) : GetPortableRuntimeMeshView(assetPath))).first;
                const auto& mesh = *found->second;
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
                if (brushActor) {
                    MoverVisual visual;
                    visual.inverseInitialTransform = ActorTransformMatrix(transform).Inverted();
                    visual.blocksPlayer = actor.blockPlayers && actor.collideActors;
                    staged.movers.emplace(actor.objectPath,std::move(visual));
                    if (const auto* live = latestMoverSnapshots_.Find(actor.objectPath))
                        ApplyMoverPose(*live,staged.movers,staged.targets);
                }
                const auto* metadata = ActorAssetMaterialMetadata(assetPath,brushActor);
                if (!metadata) throw std::runtime_error("Decoded actor geometry lacks material metadata");
                for (const auto& [material,sourceFlags] : metadata->materials) {
                    const auto selected = QuestVr::ResolveActorMeshMaterial(actor.materialOverrides,
                        mesh.texturePaths,mesh.materialTextureIndices,material);
                    const auto layer = build.textureLayers.find(FoldActorAssetPath(selected.texturePath));
                    if (hidden || layer == build.textureLayers.end() || (sourceFlags & 1u) != 0u) {
                        // Collision is a brush property, not a side effect of a
                        // successful material lookup or PF_Invisible rendering.
                        if (brushActor && (sourceFlags & 8u) == 0u) {
                            QueueActorMeshPart(mesh,material,{},transform,sourceFlags);
                            build.parts.back().sourcePolyFlags = sourceFlags;
                            build.parts.back().moverPath = actor.objectPath;
                            build.parts.back().collisionOnly = true;
                        }
                        continue;
                    }
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
                    build.parts.back().npcClip = npcClip;
                    if (brushActor) build.parts.back().moverPath = actor.objectPath;
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
        if (!hidden && !rendered && sprite && !actor.texturePath.empty()) {
            const auto layer = build.textureLayers.find(FoldActorAssetPath(actor.texturePath));
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
            "DeusExQuest: instantiated %zu scene actors from %zu live actors (%zu vertex meshes, %zu mover brushes, %zu sprites, %zu cube placeholders, %zu hidden, %zu mesh-bearing, %zu mesh formats, %zu map exits)",
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
                actorGeometryPublication_.Staged()->targets.begin(), actorGeometryPublication_.Staged()->targets.end(),
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
        ALOG("DeusExQuest: sampled %zu authored actor poses; %zu omitted poses; NPC GPU waiting playback separate from unfinished native world animation/events",
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
            if (part.textured) chunk.attribs.uv1.emplace_back(static_cast<float>(part.actorOrdinal),0.0f);
            chunk.indices.push_back(static_cast<OVRFW::TriangleIndex>(chunk.indices.size()));
        };
        const std::size_t end = part.nextIndex + count;
        if (part.mesh != nullptr) {
            for (std::size_t triangle = part.nextIndex; triangle < end; triangle += 3u) {
                const auto& source = part.mesh->triangles[triangle];
                if (source.material != part.material || source.polyFlags != part.sourcePolyFlags) continue;
                if (!part.collisionOnly) {
                    const auto vertices=QuestVr::BuildActorTriangle(*part.mesh,triangle,
                        part.actorTransform,part.meshPose.get());
                    const bool reversed=part.actorTransform.mirrored !=
                        (part.mesh->scaleX*part.mesh->scaleY*part.mesh->scaleZ<0.0f);
                    const std::size_t order[3]{0u,reversed?2u:1u,reversed?1u:2u};
                    for (std::size_t corner=0u;corner<3u;++corner) {
                        const auto& vertex=vertices[corner];
                        chunk.attribs.position.emplace_back(vertex.position.x,vertex.position.y,vertex.position.z);
                        chunk.attribs.normal.emplace_back(vertex.normal.x,vertex.normal.y,vertex.normal.z);
                        chunk.attribs.uv0.emplace_back(vertex.u,vertex.v);
                        chunk.attribs.color.push_back(part.color);
                        const float sourceIndex=part.npcClip ? static_cast<float>(
                            part.mesh->animation->triangleSourceVertexIndices.at(triangle+order[corner])) : 0.0f;
                        chunk.attribs.uv1.emplace_back(static_cast<float>(part.actorOrdinal),sourceIndex);
                        chunk.indices.push_back(static_cast<OVRFW::TriangleIndex>(chunk.indices.size()));
                    }
                }
                // Brush vertices are prepared once. Their collider and GPU
                // chunks share the same later rigid-motion delta; neither the
                // static BSP grid nor unrelated actors is rebuilt per tick.
                if (!part.moverPath.empty() && (part.sourcePolyFlags & 8u) == 0u) {
                    const auto vertices = QuestVr::BuildActorTriangle(*part.mesh,triangle,
                        part.actorTransform);
                    auto& staged = *actorGeometryPublication_.Staged();
                    auto& visual = staged.movers.at(part.moverPath);
                    if (visual.initialCollision.size() >= 4096u || staged.collisionTriangles >= 65536u)
                        throw std::runtime_error("Mover collider exceeds its bounded triangle budget");
                    const auto vector = [](const QuestVr::ActorVec3& v) {
                        return OVR::Vector3f{v.x,v.y,v.z};
                    };
                    const CollisionTriangle collision{vector(vertices[0].position),
                        vector(vertices[1].position),vector(vertices[2].position),
                        vector(vertices[0].normal)};
                    visual.initialCollision.push_back(collision);
                    visual.collision.push_back(TransformMoverCollision(collision,visual.deltaTransform,
                        OVR::Matrix3f(visual.deltaTransform).Inverse().Transposed()));
                    ExpandMoverBounds(visual,visual.collision.back());
                    ++staged.collisionTriangles;
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
        auto& staged = *actorGeometryPublication_.Staged();
        OVRFW::GlGeometry::Descriptor& chunk = textured ? build.texturedChunk : build.markerChunk;
        const OVR::Posef pose(
            OVR::Quatf(OVR::Vector3f(0.0f, 1.0f, 0.0f), sceneYaw_), worldPosition_);
        if (textured) {
            if (!staged.atlas || !staged.atlas->texture.IsValid())
                throw std::runtime_error("actor texture disappeared during geometry upload");
            staged.textured.emplace_back();
            const auto npcGpu = build.texturedChunkNpcClip ? NpcClipForUpload(build.texturedChunkNpcClip) : nullptr;
            staged.textured.back().Init(chunk, staged.atlas->texture,
                (build.texturedChunkPolyFlags & 0x100u) == 0u,build.texturedChunkPolyFlags,
                &staged.visibilityMatrices,npcGpu ? &npcGpu->texture : nullptr);
            staged.textured.back().SetPose(pose);
            staged.textured.back().Update();
            if (npcGpu) {
                staged.npcAnimations.push_back({staged.textured.size()-1u,npcGpu,build.texturedChunkNpcTransform});
                staged.textured.back().SetAnimation(QuestVr::SampleNpcAnimationFrame(*npcGpu->clip,npcAnimationSeconds_),
                    build.texturedChunkNpcTransform);
            }
            if (!build.texturedChunkMoverPath.empty()) {
                auto& visual = staged.movers.at(build.texturedChunkMoverPath);
                visual.rendererIndices.push_back(staged.textured.size()-1u);
                staged.textured.back().SetModelMatrix(OVR::Matrix4f(pose)*visual.deltaTransform);
            }
        } else {
            staged.world.emplace_back();
            staged.markerOwners.push_back(build.markerChunkActor);
            staged.world.back().Init(chunk);
            staged.world.back().AmbientLightColor = {0.45f, 0.45f, 0.45f};
            staged.world.back().SetPose(pose);
            staged.world.back().Update();
        }
        if (glGetError() != GL_NO_ERROR)
            throw std::runtime_error("GPU actor geometry chunk upload failed");
        ++build.uploadedChunks;
        // Retain bounded CPU capacity across uploads, avoiding repeated growth
        // allocation on each subsequent actor chunk.
        chunk.attribs.position.clear();
        chunk.attribs.normal.clear();
        chunk.attribs.uv0.clear();
        chunk.attribs.uv1.clear();
        chunk.attribs.color.clear();
        chunk.indices.clear();
        if (!textured) build.markerChunkActor = QuestVr::ActorVisibilityState::MaximumActors;
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
            (allPrepared && !build.markerChunk.indices.empty()) ||
            (!build.markerChunk.indices.empty() && !build.parts.empty() && !build.parts.front().textured &&
                build.parts.front().actorOrdinal != build.markerChunkActor);
        const bool uploadTextured = build.texturedChunk.indices.size() >= actorGeometryChunkVertices_ ||
            (allPrepared && !build.texturedChunk.indices.empty()) ||
            (!build.texturedChunk.indices.empty() && !build.parts.empty() &&
                build.parts.front().textured &&
                (build.parts.front().polyFlags != build.texturedChunkPolyFlags ||
                 build.parts.front().moverPath != build.texturedChunkMoverPath ||
                 build.parts.front().npcClip != build.texturedChunkNpcClip ||
                 (build.texturedChunkNpcClip && build.parts.front().actorOrdinal != build.texturedChunkNpcActor)));
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
                        if (!chunk.indices.empty() && (build.texturedChunkPolyFlags != part.polyFlags ||
                            build.texturedChunkMoverPath != part.moverPath ||
                            build.texturedChunkNpcClip != part.npcClip ||
                            (part.npcClip && build.texturedChunkNpcActor != part.actorOrdinal))) break;
                        build.texturedChunkPolyFlags = part.polyFlags;
                        build.texturedChunkMoverPath = part.moverPath;
                        build.texturedChunkNpcClip = part.npcClip;
                        build.texturedChunkNpcActor = part.actorOrdinal;
                        build.texturedChunkNpcTransform = ActorTransformMatrix(part.actorTransform);
                    } else {
                        if (!chunk.indices.empty() && build.markerChunkActor != part.actorOrdinal) break;
                        build.markerChunkActor = part.actorOrdinal;
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
            const auto commitStarted = std::chrono::steady_clock::now();
            LogActorGeometryBuild();
            RebaseStagedMoverPoses();
            if (!actorAtlas_ || !actorGeometryPublication_.MarkReady(build.publication) ||
                !actorGeometryPublication_.Commit(build.publication,
                    {actorSceneGeneration_,GetPortableRuntimeWorldRevision(),actorAtlas_->generation}))
                throw std::runtime_error("Completed actor bank no longer matches its runtime scene/revision/atlas");
            activeActorGeometry_ = actorGeometryPublication_.Active();
            // No render/interaction runs between this commit and the metadata
            // moves. Renderer addresses, collider poses and targets publish as
            // one complete generation; no partial actor chunks are visible.
            moverVisuals_ = std::move(activeActorGeometry_->movers);
            interactiveActors_ = std::move(activeActorGeometry_->targets);
            moverCollisionTriangleCount_ = activeActorGeometry_->collisionTriangles;
            actorStreamingCenter_ = activeActorGeometry_->streamingCenter;
            UpdateActorGeometryPose(OVR::Posef(
                OVR::Quatf(OVR::Vector3f(0.0f,1.0f,0.0f),sceneYaw_),worldPosition_));
            ALOG("DeusExQuest: complete retained actor bank published atomically; %zu targets %zu chunks atlas=%llu",
                interactiveActors_.size(),activeActorGeometry_->world.size()+activeActorGeometry_->textured.size(),
                static_cast<unsigned long long>(activeActorGeometry_->atlas->generation));
            ALOG("DeusExQuest: actor bank commit/old GPU retirement %.3f ms",
                std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-commitStarted).count());
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
        actorGeometryPublication_.Cancel();
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
        activeActorGeometry_ = nullptr;
        actorGeometryPublication_.Clear();
        moverVisuals_.clear();
        latestMoverSnapshots_.Clear();
        moverCollisionTriangleCount_ = 0u;
        interactiveActors_.clear();
    }

    void DestroySceneGeometry() {
        DestroyActorGeometry();
        npcGpuClips_.clear();npcWaiting_.clear();npcAnimationSeconds_=0.0;
        ResetActorRemovalState();
        if (actorSceneGeneration_ == std::numeric_limits<std::uint64_t>::max())
            throw std::runtime_error("Actor scene generation exceeds its lifetime bound");
        ++actorSceneGeneration_;
        ClearPendingActorTextureUpload();
        for (auto& renderer : worldRenderers_) renderer.Shutdown();
        for (auto& renderer : texturedRenderers_) renderer.Shutdown();
        for (auto& renderer : bakedWorldRenderers_) renderer.Shutdown();
        worldRenderers_.clear();
        texturedRenderers_.clear();
        bakedWorldRenderers_.clear();
        moverVisuals_.clear();
        moverCollisionTriangleCount_ = 0u;
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
        actorAtlas_.reset();
        actorTexture_ = {};
        actorTexturePaths_.clear();
        actorTexturePolyFlags_.clear();
        actorMaskedTextureLayers_.clear();
        actorMaterialMetadata_.clear();
        actorMaterialProbeFallbackLogs_ = 0u;
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
                activeMapNorth_=std::move(preparedMapNorth_);preparedMapNorth_.reset();
                compassDisplayedPositions_.reset();
                ResetOriginalPlayerHudState();
                activeMapLights_ = std::move(preparedMapLights_);
                activeMapUnrealOrigin_ = preparedWorldMesh_.lightmap.unrealOrigin;
                activeMapPlayerStartPath_ = preparedWorldMesh_.lightmap.playerStartPath;
                if (restorePoseAfterTransition_) {
                    if (restoredMapLocalPose_) {
                        QuestVr::RestoreSavedMapPose(restoredMapLocalFeet_, restoredMapLocalHeadYaw_,
                            currentHeadStage_, currentHeadStageYaw_, worldPosition_, sceneYaw_,
                            playerPosture_.virtualFloorY);
                    } else {
                        worldPosition_ = restoredWorldPosition_;
                        sceneYaw_ = restoredSceneYaw_;
                    }
                    restorePoseAfterTransition_ = false;
                } else {
                    worldPosition_ = {0.0f, playerPosture_.virtualFloorY, 0.0f};
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
                npcWaiting_ = std::move(preparedNpcWaiting_);
                ReplaceSpatialAudioEmitters(std::move(preparedSpatialAudioEmitters_));
                PublishMapMusic(std::move(preparedMapMusic_));
                ReconcileSpatialAudioEmitters(actorSnapshots_);
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
                if (hideDebugHudAtFirstReady_) {
                    hideDebugHudAtFirstReady_ = false;
                    debugHudVisible_ = false;
                    if (hudLabel_ != nullptr) hudLabel_->SetVisible(false);
                }
                publishedRuntimeWorldRevision_ = GetPortableRuntimeWorldRevision();
                hasPublishedRuntimeWorldRevision_ = true;
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
            ClearPendingActorTextureUpload();
            pendingWorldMesh_.chunks.clear();
            preparedActorSnapshots_.clear();
            preparedNpcWaiting_.clear();
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
        if (runtimeActorTextureUploadPending_ || !pendingMapName_.empty() || !transitionMapName_.empty() ||
            (runtimeAvailable_ && mapName == currentMapName_)) {
            return;
        }
        // A successful v4/v5 save does not authorize discarding the current map's
        // actor state. Check before cancelling UI/audio/geometry or starting a
        // replacement transaction, whose rollback needs the same archive.
        if (runtimeAvailable_ && GetPortableRuntimeUnsavedScriptState()) {
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
                // Restore can hide an authored light/pawn/pickup. Build masks
                // against the complete authored actor list and preserve every
                // duplicate/inactive static-list ordinal before restoration.
                const auto authoredActors = GetPortableRuntimeMapActors();
                std::vector<std::string> npcErrors;
                preparation.npcWaiting = QuestVr::PrepareNpcWaitingPresentations(authoredActors,&npcErrors);
                for (const auto& error:npcErrors) ALOG("DeusExQuest: NPC waiting presentation refused: %s",error.c_str());
                ALOG("DeusExQuest: prepared %zu original-script NPC waiting presentations (render-only)",preparation.npcWaiting.size());
                PrepareWorldStaticLightmaps(map,authoredActors,preparation.worldMesh);
                // Retain decoded clips for the complete authored population,
                // so a same-map checkpoint can revive an emitter without any
                // synchronous sound decoding on the XR thread. Active emitters
                // are selected from the restored snapshots during publication.
                preparation.spatialAudioEmitters = PrepareSpatialAudioEmitters(
                    authoredActors, targetAudioRate,preparation.worldMesh.lightmap.unrealOrigin);
                if (!restoreRuntimePath.empty() &&
                    !LoadPortableRuntimeState(restoreRuntimePath)) {
                    throw std::runtime_error("saved runtime restoration failed");
                }
                // Save restoration can introduce born meshes or changed Skin
                // materials. The worker still exclusively owns the runtime;
                // prepare the atlas from the complete RESTORED population.
                preparation.actorTextures = BuildPortableRuntimeActorTextureArray(96, 96);
                if (!preparation.actorTextures.passed)
                    throw std::runtime_error("restored actor texture preparation failed");
                preparation.actors = GetPortableRuntimeMapActors();
                preparation.mapNorth=PrepareOriginalMapNorth();
                preparation.lights = BuildMapLights(preparation.actors,preparation.worldMesh.lightmap.unrealOrigin);
                preparation.music=QuestVr::PrepareOriginalMapMusic(gameRoot_,mapName,targetAudioRate);
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
                if (!TryRebuildRuntimeActorPublication()) return;
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
        preparedNpcWaiting_ = std::move(preparation.npcWaiting);
        preparedActorTextures_ = std::move(preparation.actorTextures);
        preparedWorldTexture_ = std::move(preparation.worldTexture);
        preparedWorldMesh_ = std::move(preparation.worldMesh);
        preparedSpatialAudioEmitters_ = std::move(preparation.spatialAudioEmitters);
        preparedMapMusic_ = std::move(preparation.music);
        preparedMapNorth_ = std::move(preparation.mapNorth);
        preparedMapLights_ = std::move(preparation.lights);
        transitionMapName_ = mapName;
        transitionPhase_ = MapTransitionPhase::WorldTextureAllocate;
    }

    void PollMapTransitionRequest() {
        // Leave the single-slot request intact while a worker replaces the
        // runtime or a staged upload still owns it. Consumption must not make
        // gameplay diagnostics race with a transition.
        if (runtimeActorTextureUploadPending_ || !pendingMapName_.empty() || !transitionMapName_.empty()) return;
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
        if (std::strcmp(requested, "HUD") == 0) {
            ToggleDebugHud();
            return;
        }
        if(std::strcmp(requested,"BELT")==0) {
            quickBeltVisibility_.Reveal();
            ALOG("DeusExQuest: quickbar presentation revealed; equipment unchanged");
            return;
        }
        if(std::strcmp(requested,"HUDSTATE")==0) {
            // Report after this frame's UI visibility/poses are resolved.
            hudStateDiagnosticRequested_=true;
            return;
        }
        if(std::strcmp(requested,"HELDSTATE")==0) {
            heldStateDiagnosticRequested_=true;
            return;
        }
        if (std::strcmp(requested, "SEATED") == 0 || std::strcmp(requested, "STANDING") == 0) {
            SetPlayerPosture(std::strcmp(requested,"SEATED") == 0,true);
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
            EnsureInventoryNavigation(GetPortableRuntimeInventoryItems());
            MoveInventorySelection(1);
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
        if (std::strcmp(requested, "MUSIC") == 0) {
            std::lock_guard<std::mutex> lock(audioMutex_);
            ALOG("DeusExQuest: music diagnostic present=%s active=%s rate=%u underrunFrames=%llu",
                musicStream_?"true":"false",musicPlaybackActive_.load(std::memory_order_acquire)?"true":"false",
                audioSampleRate_,static_cast<unsigned long long>(musicStream_?musicStream_->UnderrunFrames():0u));
            return;
        }
        if (std::strcmp(requested, "COMPASS") == 0) {
            ALOG("DeusExQuest: compass diagnostic artwork=%s visible=%s northAvailable=%s mapNorth=%d draw=%d wrap=%d source=%s",
                compassRenderer_.IsInitialized()?"true":"false",compassUiVisible_?"true":"false",
                activeMapNorth_?"true":"false",activeMapNorth_?activeMapNorth_->trueNorth:0,
                compassDisplayedPositions_?compassDisplayedPositions_->draw:0,
                compassDisplayedPositions_?compassDisplayedPositions_->wrap:0,
                activeMapNorth_ && activeMapNorth_->found?activeMapNorth_->actorPath.c_str():"None");
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
            if (currentMapName_.rfind("00_Training", 0u) == 0u) missionNumber = 0;
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
            if (currentMapName_.rfind("00_Training", 0u) == 0u) missionNumber = 0;
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
                if (!TryRebuildRuntimeActorPublication()) return;
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
                if (!TryRebuildRuntimeActorPublication()) return;
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

    static float RayTriangleDistance(const OVR::Vector3f& origin,
        const OVR::Vector3f& direction, const CollisionTriangle& triangle) {
        const auto edge1 = triangle.b-triangle.a;
        const auto edge2 = triangle.c-triangle.a;
        const auto p = direction.Cross(edge2);
        const float determinant = edge1.Dot(p);
        if (std::fabs(determinant) < 0.000001f) return -1.0f;
        const float inverse = 1.0f/determinant;
        const auto offset = origin-triangle.a;
        const float u = offset.Dot(p)*inverse;
        if (u < 0.0f || u > 1.0f) return -1.0f;
        const auto q = offset.Cross(edge1);
        const float v = direction.Dot(q)*inverse;
        if (v < 0.0f || u+v > 1.0f) return -1.0f;
        return edge2.Dot(q)*inverse;
    }

    const InteractiveActor* RayTargetMover(const OVR::Vector3f& originStage,
        const OVR::Vector3f& directionStage, float& nearestDistance, bool usableOnly = false) const {
        const auto origin = StageToLocal(originStage,worldPosition_);
        const auto direction = QuestVr::StageDirectionToLocal(directionStage,sceneYaw_);
        const InteractiveActor* nearest{};
        for (const auto& [path,visual] : moverVisuals_) {
            const auto target = std::find_if(interactiveActors_.begin(),interactiveActors_.end(),
                [&](const InteractiveActor& actor) { return actor.objectPath == path; });
            if (target == interactiveActors_.end() || (usableOnly && !target->usable) || !visual.hasBounds) continue;
            if (!QuestVr::RayTargetFrameBounds({origin.x,origin.y,origin.z},
                {direction.x,direction.y,direction.z},
                {visual.minimum.x,visual.minimum.y,visual.minimum.z},
                {visual.maximum.x,visual.maximum.y,visual.maximum.z},nearestDistance)) continue;
            bool hit{};
            for (const auto& triangle : visual.collision) {
                const auto distance = RayTriangleDistance(origin,direction,triangle);
                if (distance > 0.0f && distance < nearestDistance) {
                    nearestDistance = distance;
                    hit = true;
                }
            }
            if (hit) nearest = &*target;
        }
        return nearest;
    }

    const InteractiveActor* FindUsableRayTarget(const OVR::Posef& pointerPose, float& bestDistance) const {
        if (!runtimeAvailable_ || ActorPublicationBlocksGameplay() ||
            !pendingMapName_.empty() || !transitionMapName_.empty()) return nullptr;
        const OVR::Vector3f origin = pointerPose.Translation;
        const OVR::Vector3f direction =
            pointerPose.Rotation.Rotate(OVR::Vector3f(0.0f, 0.0f, -1.0f));
        const auto originLocal = StageToLocal(origin,worldPosition_);
        const auto directionLocal = QuestVr::StageDirectionToLocal(direction,sceneYaw_);
        const InteractiveActor* best{};
        for (const InteractiveActor& actor : interactiveActors_) {
            if (!actor.usable || actor.mover) continue;
            // Match the object's authored collision extents rather than a
            // universal sphere at its pivot (which missed tall NPCs entirely).
            const float radius = std::max(actor.collisionRadius,0.06f);
            const float height = std::max(actor.collisionHeight,0.06f);
            const auto& p = actor.localPosition;
            const auto distance = QuestVr::RayTargetFrameBounds(
                {originLocal.x,originLocal.y,originLocal.z},
                {directionLocal.x,directionLocal.y,directionLocal.z},
                {p.x-radius,p.y-height,p.z-radius},
                {p.x+radius,p.y+height,p.z+radius},bestDistance);
            if (distance && *distance >= 0.0f && *distance < bestDistance) {
                best = &actor;
                bestDistance = *distance;
            }
        }
        if (const auto mover = RayTargetMover(origin,direction,bestDistance,true)) best = mover;
        if (!best) return nullptr;
        bool occluded{};
        // A 3m use ray lies within the neighboring 8m map cells. Static walls
        // and non-frobbable moving brushes must not reveal or use an NPC behind
        // them. This is bounded local collision data, never a runtime VM query.
        ForNearbyTriangles(originLocal+directionLocal*(bestDistance*0.5f),1,
            [&](const CollisionTriangle& triangle) {
                const auto distance = RayTriangleDistance(originLocal,directionLocal,triangle);
                if (distance > 0.005f && distance+0.01f < bestDistance) occluded = true;
            });
        return occluded ? nullptr : best;
    }

    std::optional<std::string> ReadOriginalTargetDescription(const InteractiveActor& target) {
        QuestVr::FrobDescriptionFields fields;
        fields.mover=target.mover;fields.pawn=target.pawn;
        fields.inventory=target.inventory;fields.decoration=target.decoration;
        const auto readString=[&](const char* path)->std::optional<std::string> {
            try {
                const auto value=ReadPortableActorDeclaredScriptProperty(target.objectPath,path);
                if (value.kind==QuestVr::Vm::Kind::String && value.text.size()<=4096u &&
                    value.text.find('\0')==std::string::npos) return value.text;
            } catch (const std::exception&) {}
            return {};
        };
        const auto readBool=[&](const char* path)->std::optional<bool> {
            try {
                const auto value=ReadPortableActorDeclaredScriptProperty(target.objectPath,path);
                if (value.kind==QuestVr::Vm::Kind::Bool) return value.boolean;
            } catch (const std::exception&) {}
            return {};
        };
        const auto readFloat=[&](const char* path)->std::optional<float> {
            try {
                const auto value=ReadPortableActorDeclaredScriptProperty(target.objectPath,path);
                if (value.kind==QuestVr::Vm::Kind::Float && std::isfinite(value.floating)) return value.floating;
            } catch (const std::exception&) {}
            return {};
        };
        if (target.mover) {
            fields.locked=readBool("DeusEx.DeusExMover.bLocked");
            if (fields.locked && *fields.locked) {
                fields.pickable=readBool("DeusEx.DeusExMover.bPickable");
                fields.breakable=readBool("DeusEx.DeusExMover.bBreakable");
                fields.lockStrength=readFloat("DeusEx.DeusExMover.lockStrength");
                fields.doorStrength=readFloat("DeusEx.DeusExMover.doorStrength");
            }
        } else if (target.inventory) fields.itemName=readString("Engine.Inventory.ItemName");
        else if (target.pawn || target.decoration) {
            fields.familiarName=readString("Engine.Actor.FamiliarName");
            fields.unfamiliarName=readString("Engine.Actor.UnfamiliarName");
            fields.lastConversationEndTime=readFloat("Engine.Actor.LastConEndTime");
            if (target.decoration) fields.itemName=readString("DeusEx.DeusExDecoration.ItemName");
            else fields.bindName=readString("Engine.Actor.BindName");
        } else {
            fields.itemName=readString("DeusEx.DeusExCarcass.ItemName");
            if (!fields.itemName) fields.itemName=readString("DeusEx.DeusExProjectile.ItemName");
        }
        return QuestVr::BuildOriginalFrobDescription(fields,focusMessages_);
    }

    void RenderOriginalTargetDescription(const InteractiveActor& target,
        const QuestVr::OriginalFrobFrameLayout& layout,
        const QuestVr::TargetFrameProjection& projection,const OVR::Posef& head,float depth,
        std::vector<OVRFW::ovrDrawSurface>& surfaces,float elapsed) {
        if (!focusLabelRenderer_.IsInitialized() || focusLabelTextureId_==0u) return;
        const auto revision=GetPortableRuntimeWorldRevision();
        if (std::isfinite(elapsed) && elapsed>0.0f) focusDescriptionPollSeconds_-=elapsed;
        // A door publishes Location each frame; that must not decode every
        // focused object's strings each frame. Selection is immediate and
        // changed mutable descriptions are sampled at most ten times a second.
        if (focusDescriptionActor_!=target.objectPath ||
            (focusDescriptionRevision_!=revision && focusDescriptionPollSeconds_<=0.0f)) {
            focusDescription_=ReadOriginalTargetDescription(target);
            focusDescriptionActor_=target.objectPath;focusDescriptionRevision_=revision;
            focusDescriptionPollSeconds_=0.1f;
        }
        if (!focusDescription_ || focusDescription_->empty()) return;
        if (focusLabelText_!=*focusDescription_) {
            try {
                const auto canvas=QuestVr::BuildOriginalFrobLabelCanvas(focusFont_,*focusDescription_,
                    focusBackground_,[&](std::uint32_t c) {return GetPortableBitmapGlyph(focusFont_,c);},focusPalette_);
                glBindTexture(GL_TEXTURE_2D,focusLabelTextureId_);
                glTexSubImage2D(GL_TEXTURE_2D,0,0,0,512,128,GL_RGBA,GL_UNSIGNED_BYTE,canvas.image.rgba.data());
                const auto error=glGetError();glBindTexture(GL_TEXTURE_2D,0);
                if (error!=GL_NO_ERROR) throw std::runtime_error("Focus label GPU update failed");
                focusLabelTextWidth_=canvas.textWidth;focusLabelTextHeight_=canvas.textHeight;
                focusLabelText_=*focusDescription_;
            } catch (const std::exception& error) {
                // Do not show a stale name or replace original text with an SDK font.
                focusLabelText_.clear();
                ALOG("DeusExQuest: original focus label unavailable: %s",error.what());return;
            }
        }
        const auto label=QuestVr::LayoutOriginalFrobLabel(layout,
            static_cast<float>(focusLabelTextWidth_),static_cast<float>(focusLabelTextHeight_));
        if (!label) return;
        // The retained 512x128 texture is transparent outside its measured
        // original info panel. Position that texture's upper-left at infoX/Y;
        // it shares the corners' binocular plane and original pixel scale.
        const OVR::Vector3f local{(label->x+256.0f-projection.centerX)*depth/projection.focalX,
            -(label->y+64.0f-projection.centerY)*depth/projection.focalY,-depth};
        auto pose=head;pose.Translation=head.Transform(local);
        focusLabelRenderer_.SetPose(pose,256.0f*depth/projection.focalX,64.0f*depth/projection.focalY);
        focusLabelRenderer_.Render(surfaces);
    }

    void RenderTargetingFrame(const OVRFW::ovrApplFrameIn& frame, OVRFW::ovrRendererOutput& output) {
        if (!runtimeAvailable_ || !frame.RightRemoteTracked || inventoryMenuOpen_ ||
            ActorPublicationBlocksGameplay() || !pendingMapName_.empty() ||
            !transitionMapName_.empty() || transitionPhase_ != MapTransitionPhase::Idle) return;
        float distance = 3.0f;
        const auto* target = FindUsableRayTarget(frame.RightRemotePointPose,distance);
        if (!target || !target->highlight) return;
        OVR::Vector3f minimum, maximum;
        if (target->mover) {
            const auto found = moverVisuals_.find(target->objectPath);
            if (found == moverVisuals_.end() || !found->second.hasBounds) return;
            const auto center=(found->second.minimum+found->second.maximum)*0.5f;
            constexpr float half=QuestVr::OriginalFrobMoverHalfExtentUnits/52.5f;
            minimum=center-OVR::Vector3f{half,half,half};maximum=center+OVR::Vector3f{half,half,half};
        } else {
            const auto extents = OVR::Vector3f{target->collisionRadius,
                target->collisionHeight,target->collisionRadius};
            minimum = target->localPosition-extents; maximum = target->localPosition+extents;
        }
        const OVR::Quatf worldRotation(OVR::Vector3f{0,1,0},sceneYaw_);
        const auto headInverse = frame.HeadPose.Inverted();
        std::array<QuestVr::TargetFramePoint,8> points;
        std::size_t index{};
        for (const float x : {minimum.x,maximum.x})
            for (const float y : {minimum.y,maximum.y})
                for (const float z : {minimum.z,maximum.z}) {
                    const auto camera = headInverse.Transform(worldRotation.Rotate({x,y,z})+worldPosition_);
                    points[index++] = {camera.x,camera.y,camera.z};
                }
        const auto center = headInverse.Transform(worldRotation.Rotate((minimum+maximum)*0.5f)+worldPosition_);
        if (center.z >= -0.03f) return;
        QuestVr::TargetFrameProjection projection;
        const auto& left = output.FrameMatrices.EyeProjection[0];
        const auto& right = output.FrameMatrices.EyeProjection[1];
        projection.focalX = 320.0f*(left.M[0][0]+right.M[0][0])*0.5f;
        projection.focalY = 240.0f*(left.M[1][1]+right.M[1][1])*0.5f;
        projection.centerX = 320.0f*(1.0f-(left.M[0][2]+right.M[0][2])*0.5f);
        projection.centerY = 240.0f*(1.0f+(left.M[1][2]+right.M[1][2])*0.5f);
        const auto box = QuestVr::ProjectTargetFrameBounds(points,projection);
        if (!box) return;
        const float radiusUnits=target->mover?0.0f:target->collisionRadius*52.5f;
        const float heightUnits=target->mover?0.0f:target->collisionHeight*52.5f;
        const auto layout=QuestVr::LayoutOriginalFrobFrame(*box,radiusUnits,heightUnits);
        if (!layout) return;
        const auto strips = QuestVr::BuildOriginalFrobCornerStrips(*box,
            static_cast<float>(std::fmod(frame.PredictedDisplayTime,86400.0)),640.0f,480.0f,radiusUnits,heightUnits);
        if (strips) {
            targetingFrameRenderer_.Render(*strips,projection,frame.HeadPose,-center.z,output.Surfaces,focusPalette_.header);
            RenderOriginalTargetDescription(*target,*layout,projection,frame.HeadPose,-center.z,output.Surfaces,frame.DeltaSeconds);
        }
    }

    bool UseTargetedActor(const OVR::Posef& pointerPose,bool revealPickup=true) {
        if (!runtimeAvailable_ || ActorPublicationBlocksGameplay() ||
            !pendingMapName_.empty() || !transitionMapName_.empty()) return false;
        float bestDistance = 3.0f;
        const auto* best = FindUsableRayTarget(pointerPose,bestDistance);
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
            if(interaction.handled && interaction.action=="pickup") {
                if(revealPickup) quickBeltVisibility_.Reveal();
                heldVisualCache_.Invalidate();
            }
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
            // Door motion has its own live transforms/colliders. Rebuilding
            // every actor here would unnecessarily freeze a simple door use.
            if (interaction.worldChanged && interaction.action == "pickup" &&
                PublishRuntimeActorRemoval(interaction.objectPath)) {
                inventoryLayoutCached_ = false;
                inventoryMenuDirty_ = true;
                return false;
            }
            return interaction.worldChanged && !best->mover;
        } else {
            const auto equipped=GetPortableRuntimeEquippedItem();
            if (!equipped.empty() && UseSelectedConsumable(equipped)) return false;
            interactionStatus_ = "NO USABLE TARGET";
            interactionStatusSeconds_ = 2.0f;
            ALOG("DeusExQuest: VR use found no actor within 3m ray");
        }
        return false;
    }

    static std::optional<PortableMapNorthSnapshot> PrepareOriginalMapNorth() {
        try {
            auto north=ReadPortableRuntimeMapNorth();
            ALOG("DeusExQuest: original HUD map north=%d source=%s; actual Level-slot metadata",
                north.trueNorth,north.found?north.actorPath.c_str():"None (original initial zero)");
            return north;
        } catch (const std::exception& error) {
            ALOG("DeusExQuest: original compass map metadata unavailable: %s",error.what());
            return std::nullopt;
        }
    }

    void DestroyOriginalCompassUi() {
        compassUiVisible_=false;
        compassRenderer_.Shutdown();
        if (compassTextureId_!=0u) {glDeleteTextures(1,&compassTextureId_);compassTextureId_=0u;}
        compassArtwork_={};compassDisplayedPositions_.reset();
    }

    static std::string OriginalHudIconKey(std::string path) {
        if (path.size()>4096u || path.find('\0')!=std::string::npos)
            throw std::runtime_error("Original HUD icon identity exceeds bound");
        for (auto& letter:path) if (letter>='A' && letter<='Z') letter=static_cast<char>(letter+('a'-'A'));
        return path;
    }

    void ResetOriginalPlayerHudState() {
        healthUiVisible_=beltUiVisible_=false;
        cachedPlayerHud_.reset();
        hudHealthClock_.Reset();
        hudOxygenClock_.Reset();
        displayedHealthHudPixels_.clear();displayedBeltHudPixels_.clear();
        hudQuerySeconds_=0.0f;
        playerHudError_.clear();
    }

    void DestroyOriginalPlayerHudUi() {
        ResetOriginalPlayerHudState();
        healthHudRenderer_.Shutdown();beltHudRenderer_.Shutdown();
        if (healthHudTextureId_!=0u) {glDeleteTextures(1,&healthHudTextureId_);healthHudTextureId_=0u;}
        if (beltHudTextureId_!=0u) {glDeleteTextures(1,&beltHudTextureId_);beltHudTextureId_=0u;}
        hudBeltArtwork_={};hudHealthArtwork_={};hudTinyFont_={};hudIcons_.clear();
    }

    static bool SamePlayerHudSnapshot(const PortablePlayerHudSnapshot& left,const PortablePlayerHudSnapshot& right) {
        if (left.available!=right.available || left.bound!=right.bound || left.playerPath!=right.playerPath ||
            left.keyRingPath!=right.keyRingPath || left.limbHealth!=right.limbHealth || left.energy!=right.energy ||
            left.energyMax!=right.energyMax || left.headInWater!=right.headInWater || left.swimTimer!=right.swimTimer ||
            left.swimDuration!=right.swimDuration || left.inventory.size()!=right.inventory.size()) return false;
        for (std::size_t index=0u;index<left.inventory.size();++index) {
            const auto& a=left.inventory[index];const auto& b=right.inventory[index];
            if (a.actorPath!=b.actorPath || a.classPath!=b.classPath || a.iconPath!=b.iconPath ||
                a.beltDescription!=b.beltDescription || a.inObjectBelt!=b.inObjectBelt || a.beltPosition!=b.beltPosition) return false;
        }
        return true;
    }

    static GLuint CreateOriginalHudTexture(const QuestVr::PersonaUiImage& canvas) {
        QuestVr::ValidatePersonaUiImage(canvas);
        GLuint texture{};glGenTextures(1,&texture);glBindTexture(GL_TEXTURE_2D,texture);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,static_cast<GLsizei>(canvas.width),static_cast<GLsizei>(canvas.height),
            0,GL_RGBA,GL_UNSIGNED_BYTE,canvas.rgba.data());
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
        const auto error=glGetError();glBindTexture(GL_TEXTURE_2D,0);
        if (texture==0u || error!=GL_NO_ERROR) {
            if (texture!=0u) glDeleteTextures(1,&texture);
            throw std::runtime_error("Original player HUD GPU allocation failed");
        }
        return texture;
    }

    static void UpdateOriginalHudTexture(GLuint texture,const QuestVr::PersonaUiImage& canvas) {
        QuestVr::ValidatePersonaUiImage(canvas);
        glBindTexture(GL_TEXTURE_2D,texture);
        glTexSubImage2D(GL_TEXTURE_2D,0,0,0,static_cast<GLsizei>(canvas.width),static_cast<GLsizei>(canvas.height),
            GL_RGBA,GL_UNSIGNED_BYTE,canvas.rgba.data());
        const auto error=glGetError();glBindTexture(GL_TEXTURE_2D,0);
        if (error!=GL_NO_ERROR) throw std::runtime_error("Original player HUD GPU update failed");
    }

    void UpdateOriginalPlayerHudUi(const OVRFW::ovrApplFrameIn& frame,bool mapLoading) {
        healthUiVisible_=beltUiVisible_=false;
        // Never query portable storage while a map/startup worker owns it.
        // Retained actor-bank refreshes stay on this thread and retain a
        // complete visible world: do not make the HUD blink during pickups.
        if (mapLoading || !runtimeAvailable_ || inventoryMenuOpen_ || initialPreparationPending_ ||
            initialPreparationFuture_.valid() || mapCacheFuture_.valid() || !pendingMapName_.empty() ||
            !transitionMapName_.empty() ||
            !healthHudRenderer_.IsInitialized() || !beltHudRenderer_.IsInitialized()) return;
        hudQuerySeconds_=std::max(0.0f,hudQuerySeconds_-frame.DeltaSeconds);
        if (hudQuerySeconds_<=0.0f) {
            hudQuerySeconds_=0.10f;
            try {
                auto snapshot=ReadPortableRuntimePlayerHud(nativeClientGeneration_);
                if (!snapshot.available || !snapshot.bound) {
                    cachedPlayerHud_.reset();
                    hudHealthClock_.Reset();hudOxygenClock_.Reset();
                } else if (!cachedPlayerHud_ || !SamePlayerHudSnapshot(*cachedPlayerHud_,snapshot)) {
                    cachedPlayerHud_=std::move(snapshot);
                }
            } catch(const std::exception&) {
                // Original possession remains optional for this playable HUD.
                cachedPlayerHud_.reset();hudHealthClock_.Reset();hudOxygenClock_.Reset();
            }
            try {
                const auto playable=ReadPortableRuntimeQuickBelt();
                if(playable.available) {
                    QuestVr::HudBeltSnapshot belt;
                    for(std::size_t slot=0u;slot<playable.slots.size();++slot) {
                        const auto& item=playable.slots[slot];if(!item) continue;
                        const auto found=hudIcons_.find(OriginalHudIconKey(item->iconPath));
                        const auto* icon=found==hudIcons_.end()?nullptr:&found->second;
                        if(!icon) throw std::runtime_error("Original quickbar icon was not predecoded: "+item->iconPath);
                        belt.slots[slot]=QuestVr::HudBeltItem{item->actorPath,icon,item->description,
                            item->keyRing?std::to_string(playable.collectedKeys)+" KEYS":"",
                            item->keyRing,item->selected,false};
                    }
                    const auto glyph=[&](std::uint32_t character) {return GetPortableBitmapGlyph(hudTinyFont_,character);};
                    auto beltCanvas=QuestVr::BuildHudBeltCanvas(hudBeltArtwork_,belt);
                    QuestVr::DrawHudBeltText(beltCanvas,belt,hudTinyFont_,glyph);
                    // Health/oxygen reads may change every query while the
                    // belt is unchanged. Do not upload identical belt pixels.
                    if(beltCanvas.rgba!=displayedBeltHudPixels_) {
                        UpdateOriginalHudTexture(beltHudTextureId_,beltCanvas);
                        displayedBeltHudPixels_=std::move(beltCanvas.rgba);
                    }
                }
                playerHudError_.clear();
            } catch (const std::exception& error) {
                cachedPlayerHud_.reset();
                hudHealthClock_.Reset();hudOxygenClock_.Reset();
                if (playerHudError_!=error.what()) {
                    playerHudError_=error.what();
                    ALOG("DeusExQuest: playable quick inventory HUD unavailable: %s",error.what());
                }
            }
        }
        try {
            // Health presentation ticks every visible frame, independently of
            // bounded10Hz player/inventory reads. A map/player/unbind resets
            // only presentation. Until true original possession is ready, the
            // explicitly unfinished body display reflects aggregate port health.
            auto health=QuestVr::BuildPlayableHealthSnapshot(GetPortableRuntimePlayerHealth());
            if(cachedPlayerHud_ && cachedPlayerHud_->available && cachedPlayerHud_->bound) {
            hudHealthClock_.ResetForContext(cachedPlayerHud_->playerPath,currentMapName_);
            QuestVr::HudHealthClockInput actualHealth;
            for(std::size_t part=0u;part<actualHealth.size();++part) actualHealth[part]=cachedPlayerHud_->limbHealth[part];
            const auto colors=hudHealthClock_.Advance(actualHealth,frame.DeltaSeconds);
            for(std::size_t part=0u;part<colors.size();++part) {
                health.parts[part].displayedHealth=colors[part].displayedHealth;
                health.parts[part].damageFlashFraction=colors[part].damageFlashFraction;
            }
            health.energy=cachedPlayerHud_->energy;health.energyMax=cachedPlayerHud_->energyMax;
            hudOxygenClock_.ResetForContext(cachedPlayerHud_->playerPath,currentMapName_);
            const auto oxygen=hudOxygenClock_.Advance({true,true,true,cachedPlayerHud_->headInWater,
                cachedPlayerHud_->swimTimer,cachedPlayerHud_->swimDuration});
            if(oxygen) {
                health.underwater=oxygen->underwater;health.breathPercent=oxygen->breathPercent;
                if(oxygen->swimTimer) health.swimTimer=*oxygen->swimTimer;
            }
            }
            auto canvas=QuestVr::BuildHudHealthCanvas(hudHealthArtwork_,health);
            QuestVr::DrawHudHealthText(canvas,health,hudTinyFont_,
                [&](std::uint32_t character) {return GetPortableBitmapGlyph(hudTinyFont_,character);});
            // No upload for an unchanged image; original timers may refresh
            // their same color without forcing72Hz texture transfers.
            if(canvas.rgba!=displayedHealthHudPixels_) {
                UpdateOriginalHudTexture(healthHudTextureId_,canvas);
                displayedHealthHudPixels_=std::move(canvas.rgba);
            }
        } catch(const std::exception& error) {
            cachedPlayerHud_.reset();hudHealthClock_.Reset();hudOxygenClock_.Reset();displayedHealthHudPixels_.clear();
            if(playerHudError_!=error.what()) {
                playerHudError_=error.what();
                ALOG("DeusExQuest: original limb-health presentation unavailable: %s",error.what());
            }
            return;
        }
        auto healthPose=frame.HeadPose;
        healthPose.Translation+=healthPose.Rotation.Rotate({QuestVr::kHudHealthCenterX,
            QuestVr::kHudHealthCenterY,-QuestVr::kHudVrDistance});
        healthHudRenderer_.SetPose(healthPose,QuestVr::kHudHealthHalfWidth,QuestVr::kHudHealthHalfHeight);
        const auto beltPose=QuestVr::QuickBeltPanelPose(frame.HeadPose);
        beltHudRenderer_.SetPose(beltPose,QuestVr::kPlayableBeltHalfWidth,QuestVr::kPlayableBeltHalfHeight);
        beltHudRenderer_.SetOpacity(quickBeltVisibility_.Alpha());
        healthUiVisible_=!displayedHealthHudPixels_.empty();
        beltUiVisible_=!displayedBeltHudPixels_.empty() && quickBeltVisibility_.Visible();
    }

    void UpdateOriginalCompassUi(const OVRFW::ovrApplFrameIn& frame,bool mapLoading) {
        compassUiVisible_=false;
        if (mapLoading || inventoryMenuOpen_ || !activeMapNorth_ || !compassRenderer_.IsInitialized()) return;
        const auto yaw=QuestVr::HudRenderedViewYaw(
            frame.HeadPose.Rotation.Rotate(OVR::Vector3f{0,0,-1}),
            frame.HeadPose.Rotation.Rotate(OVR::Vector3f{1,0,0}),sceneYaw_);
        if (!yaw) return;
        const auto positions=QuestVr::OriginalHudCompassPositions(*yaw,activeMapNorth_->trueNorth);
        if (!compassDisplayedPositions_ || positions.draw!=compassDisplayedPositions_->draw ||
            positions.wrap!=compassDisplayedPositions_->wrap) {
            try {
                // Tiny bounded raster update only when the authored strip moves
                // by a pixel. All package decoding happened on the map worker.
                const auto canvas=QuestVr::BuildHudCompassCanvas(compassArtwork_,*yaw,activeMapNorth_->trueNorth);
                glBindTexture(GL_TEXTURE_2D,compassTextureId_);
                glTexSubImage2D(GL_TEXTURE_2D,0,0,0,QuestVr::kHudCompassWidth,QuestVr::kHudCompassHeight,
                    GL_RGBA,GL_UNSIGNED_BYTE,canvas.rgba.data());
                const auto error=glGetError();glBindTexture(GL_TEXTURE_2D,0);
                if (error!=GL_NO_ERROR) throw std::runtime_error("Compass GPU texture update failed");
                compassDisplayedPositions_=positions;
            } catch (const std::exception& error) {
                ALOG("DeusExQuest: original compass unavailable: %s",error.what());
                DestroyOriginalCompassUi();return;
            }
        }
        auto pose=frame.HeadPose;
        pose.Translation+=pose.Rotation.Rotate({QuestVr::kHudCompassCenterX,
            QuestVr::kHudCompassCenterY,-QuestVr::kHudVrDistance});
        compassRenderer_.SetPose(pose,QuestVr::kHudCompassHalfWidth,QuestVr::kHudCompassHalfHeight);
        compassUiVisible_=true;
    }

    void UpdateOriginalDataLinkUi(const OVRFW::ovrApplFrameIn& frame,const bool mapLoading) {
        dataLinkUiVisible_=false;
        if (dialogueSubtitleSeconds_<=0.0f ||
            dataLinkSpeech_.empty() || !dataLinkRenderer_.IsInitialized()) return;
        if (mapLoading || inventoryMenuOpen_) return;
        const auto visibleCharacters=std::min(dataLinkSpeech_.size(),static_cast<std::size_t>(
            dataLinkElapsedSeconds_/QuestVr::kDataLinkSecondsPerCharacter));
        const bool cursorVisible=(static_cast<std::uint32_t>(dataLinkElapsedSeconds_/0.75f)%2u)==0u;
        if (visibleCharacters!=dataLinkDisplayedCharacters_ || cursorVisible!=dataLinkDisplayedCursor_) {
            try {
                QuestVr::PersonaUiImage canvas=dataLinkBase_;
                std::int32_t mission{};
                try { mission=std::stoi(currentMapName_.substr(0u,currentMapName_.find('_'))); }
                catch (const std::exception&) { mission=0; }
                auto portrait=QuestVr::DataLinkPortraitName(dataLinkBindName_,mission);
                std::transform(portrait.begin(),portrait.end(),portrait.begin(),
                    [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                const auto found=dataLinkPortraits_.find(portrait);
                if (found!=dataLinkPortraits_.end()) QuestVr::AddDataLinkPortrait(canvas,found->second);
                QuestVr::DrawDataLinkUiText(canvas,dataLinkNameFont_,dataLinkTextFont_,
                    QuestVr::DataLinkDisplayName(dataLinkBindName_),
                    QuestVr::BuildDataLinkTextWindow(dataLinkSpeech_,visibleCharacters),cursorVisible,
                    [&](std::uint32_t c) { return GetPortableBitmapGlyph(dataLinkNameFont_,c); },
                    [&](std::uint32_t c) { return GetPortableBitmapGlyph(dataLinkTextFont_,c); });
                glBindTexture(GL_TEXTURE_2D,dataLinkTextureId_);
                glTexSubImage2D(GL_TEXTURE_2D,0,0,0,QuestVr::kDataLinkWidth,QuestVr::kDataLinkHeight,
                    GL_RGBA,GL_UNSIGNED_BYTE,canvas.rgba.data());
                const auto error=glGetError();
                glBindTexture(GL_TEXTURE_2D,0);
                if (error!=GL_NO_ERROR) throw std::runtime_error("Original datalink text GPU update failed");
                dataLinkDisplayedCharacters_=visibleCharacters;
                dataLinkDisplayedCursor_=cursorVisible;
            } catch (const std::exception& error) {
                ALOG("DeusExQuest: datalink canvas unavailable; SDK subtitle fallback: %s",error.what());
                dataLinkSpeech_.clear();
                return;
            }
        }
        // Original portrait/text layout, inset beside the health column with
        // a shared top edge. The complete panel remains inside binocular FOV.
        auto pose=frame.HeadPose;
        pose.Translation+=pose.Rotation.Rotate({QuestVr::kHudDataLinkCenterX,
            QuestVr::kHudDataLinkCenterY,-QuestVr::kHudVrDistance});
        dataLinkRenderer_.SetPose(pose,QuestVr::kHudDataLinkHalfWidth,QuestVr::kHudDataLinkHalfHeight);
        dataLinkUiVisible_=true;
    }

    void SetDialogueSubtitle(const std::string& text, const float seconds) {
        dataLinkSpeech_.clear();
        dataLinkUiVisible_=false;
        dataLinkElapsedSeconds_=0.0f;
        dialogueSubtitle_ = text;
        dialogueSubtitlePages_.clear();
        std::string remaining = text;
        for (auto& character : remaining)
            if (character == '\n' || character == '\r' || character == '\t') character = ' ';
        // Page the original text instead of losing the latter half of a long
        // tutorial line (notably DL_F1F2). Audio stays a single original clip.
        while (!remaining.empty() && dialogueSubtitlePages_.size() < 64u) {
            std::string page;
            for (std::size_t line=0u; line<4u && !remaining.empty(); ++line) {
                std::size_t split = std::min<std::size_t>(58u,remaining.size());
                if (split < remaining.size()) {
                    const auto space = remaining.find_last_of(' ',split);
                    if (space != std::string::npos && space != 0u) split = space;
                }
                if (!page.empty()) page += '\n';
                page += remaining.substr(0u,split);
                remaining.erase(0u,split);
                const auto first = remaining.find_first_not_of(' ');
                remaining.erase(0u,first == std::string::npos ? remaining.size() : first);
            }
            dialogueSubtitlePages_.push_back(std::move(page));
        }
        if (!remaining.empty()) ALOG("DeusExQuest: subtitle exceeds 64-page bounded display budget");
        dialogueSubtitleSeconds_ = dialogueSubtitleTotalSeconds_ = seconds;
    }

    void PollTouchDataLinks(const float seconds) {
        touchDataLinkPollSeconds_ += seconds;
        // Collect contacts even while the preceding line plays. Runtime contact
        // selection marks a one-shot trigger immediately, so never call it when
        // this bounded queue is full. This is not original voice-finished event
        // timing; it preserves instructions crossed during another transmission.
        if (touchDataLinkPollSeconds_ >= 0.15f &&
            pendingTouchDataLinks_.size() < maxPendingTouchDataLinks_) {
            touchDataLinkPollSeconds_ = 0.0f;
            const auto feet = StageToLocal({currentHeadStage_.x,playerPosture_.virtualFloorY,
                currentHeadStage_.z},worldPosition_);
            constexpr float unitsPerMeter = 52.5f;
            const float x = activeMapUnrealOrigin_.x-feet.z*unitsPerMeter;
            const float y = activeMapUnrealOrigin_.y+feet.x*unitsPerMeter;
            const float z = activeMapUnrealOrigin_.z+(feet.y-1.0f)*unitsPerMeter+39.0f;
            try {
                PortableDialogueResult dialogue;
                if (GetPortableRuntimeTouchDataLink(x,y,z,17.0f,39.0f,dialogue)) {
                    pendingTouchDataLinks_.push_back(std::move(dialogue));
                    ALOG("DeusExQuest: contact datalink queued event=%s depth=%zu",
                        pendingTouchDataLinks_.back().eventPath.c_str(),pendingTouchDataLinks_.size());
                }
            } catch (const std::exception& error) {
                ALOG("DeusExQuest: touch datalink unavailable: %s",error.what());
                touchDataLinkPollSeconds_ = -5.0f;
            }
        }
        if (pendingTouchDataLinks_.empty() || !pendingChoices_.empty() ||
            dialogueDecodeFuture_.valid() || dialogueSubtitleSeconds_ > 0.0f) return;
        {
            std::lock_guard<std::mutex> lock(audioMutex_);
            if (dialogueCursor_ < dialogueSamples_.size()) return;
        }
        PortableDialogueResult dialogue = std::move(pendingTouchDataLinks_.front());
        pendingTouchDataLinks_.pop_front();
        try {
            PresentDialogueResult(dialogue,{}); // Datalinks are head-centered, not room speakers.
        } catch (const std::exception& error) {
            ALOG("DeusExQuest: queued touch datalink presentation failed: %s",error.what());
        }
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
        if (currentMapName_.rfind("00_Training", 0u) == 0u) missionNumber = 0;
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
        return PresentDialogueResult(dialogue,actorPath,cursor);
    }

    bool PresentDialogueResult(const PortableDialogueResult& dialogue,
        const std::string& actorPath, const std::size_t ordinal = 0u) {
        if (!dialogue.found) return false;
        std::string subtitle = dialogue.speech;
        std::replace(subtitle.begin(), subtitle.end(), '\n', ' ');
        std::replace(subtitle.begin(), subtitle.end(), '\r', ' ');
        const auto fullSubtitle = dialogue.bindName+": "+subtitle;
        if (subtitle.size() > 220u) subtitle.resize(220u);
        interactionStatus_ = dialogue.bindName + ": " + subtitle;
        AppendPersonaLog(interactionStatus_);
        interactionStatusSeconds_ = std::clamp(
            static_cast<float>(subtitle.size()) * 0.055f, 4.0f, 12.0f);
        SetDialogueSubtitle(fullSubtitle,interactionStatusSeconds_);
        if (actorPath.empty()) {
            dataLinkBindName_=dialogue.bindName;
            dataLinkSpeech_=dialogue.speech;
            dataLinkElapsedSeconds_=0.0f;
            dataLinkDisplayedCharacters_=invalidRendererIndex_;
            dataLinkDisplayedCursor_=false;
            // Preserve every character of the authored line, including long
            // Training instructions, before ending the transmission window.
            dialogueSubtitleSeconds_=std::max(dialogueSubtitleSeconds_,
                static_cast<float>(dataLinkSpeech_.size())*QuestVr::kDataLinkSecondsPerCharacter+1.0f);
            dialogueSubtitleTotalSeconds_=dialogueSubtitleSeconds_;
        }
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
            ordinal,
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
        SetDialogueSubtitle(interactionStatus_,interactionStatusSeconds_);
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
        SetDialogueSubtitle(interactionStatus_,interactionStatusSeconds_);
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
            try {
                const auto image=[&](const char* name) {
                    auto source=DecodePortableIndexedTexture(uiPackage,name,true);
                    return QuestVr::PersonaUiImage{source.width,source.height,std::move(source.rgba)};
                };
                preparation.compass={image(QuestVr::kHudCompassBackground),image(QuestVr::kHudCompassBorder),
                    image(QuestVr::kHudCompassTicks),image(QuestVr::kHudCompassTickBox)};
                (void)QuestVr::BuildHudCompassCanvas(preparation.compass,0,0);
            } catch (const std::exception& error) {
                preparation.compass={};
                ALOG("DeusExQuest: original compass artwork unavailable: %s",error.what());
            }
            try {
                const auto image=[&](const char* name) {
                    auto source=DecodePortableIndexedTexture(uiPackage,name,true);
                    return QuestVr::PersonaUiImage{source.width,source.height,std::move(source.rgba)};
                };
                preparation.belt.backgroundLeft=image(QuestVr::kHudBeltBackgroundLeft);
                preparation.belt.backgroundRight=image(QuestVr::kHudBeltBackgroundRight);
                preparation.belt.cell=image(QuestVr::kHudBeltBackgroundCell);
                for (std::size_t piece=0u;piece<3u;++piece)
                    preparation.belt.borders[piece]=image(QuestVr::kHudBeltBorders[piece]);
                constexpr std::array<const char*,9> selectionNames{{
                    "UserInterface.PersonaItemHighlight_TL","UserInterface.PersonaItemHighlight_TR",
                    "UserInterface.PersonaItemHighlight_BL","UserInterface.PersonaItemHighlight_BR",
                    "UserInterface.PersonaItemHighlight_Left","UserInterface.PersonaItemHighlight_Right",
                    "UserInterface.PersonaItemHighlight_Top","UserInterface.PersonaItemHighlight_Bottom",
                    "UserInterface.PersonaItemHighlight_Center"}};
                for (std::size_t piece=0u;piece<9u;++piece) preparation.belt.selection[piece]=image(selectionNames[piece]);
                preparation.health.background=image(QuestVr::kHudHealthBackground);
                preparation.health.border=image(QuestVr::kHudHealthBorder);
                preparation.health.body=image(QuestVr::kHudHealthBody);
                for (std::size_t part=0u;part<6u;++part) preparation.health.parts[part]=image(QuestVr::kHudHealthPartNames[part]);
                preparation.hudTinyFont=DecodePortableBitmapFont(uiPackage,QuestVr::kHudTinyFont);
                if (preparation.hudTinyFont.glyphs.size()<256u)
                    throw std::runtime_error("Original HUD FontTiny lacks byte-indexed glyphs");
                std::size_t iconBytes{};
                for (std::size_t index=0u;index<uiPackage.exports.size();++index) {
                    if (OriginalHudIconKey(GetPortableObjectPath(uiPackage,uiPackage.exports[index].ObjClass))!="engine.texture") continue;
                    const auto path=GetPortableObjectPath(uiPackage,static_cast<std::int32_t>(index+1u));
                    if (OriginalHudIconKey(path).rfind("icons.",0u)!=0u) continue;
                    if (preparation.hudIcons.size()>=512u)
                        throw std::runtime_error("Original HUD icon catalog exceeds 512 textures");
                    auto icon=image(path.c_str());
                    if (icon.width>256u || icon.height>256u || icon.rgba.size()>32u*1024u*1024u-iconBytes)
                        throw std::runtime_error("Original HUD icon catalog exceeds bounded texture/memory budget");
                    iconBytes+=icon.rgba.size();
                    const auto key=OriginalHudIconKey("DeusExUI."+path);
                    if (!preparation.hudIcons.emplace(key,std::move(icon)).second)
                        throw std::runtime_error("Original HUD icon catalog has ambiguous qualified identity");
                }
                (void)QuestVr::BuildHudBeltCanvas(preparation.belt,{});
                (void)QuestVr::BuildHudHealthCanvas(preparation.health,{});
                preparation.playerHudReady=true;
                ALOG("DeusExQuest: original belt/body/FontTiny CPU artwork ready; %zu predecoded icons, %zu bytes; no invented player state",
                    preparation.hudIcons.size(),iconBytes);
            } catch (const std::exception& error) {
                preparation.playerHudReady=false;
                preparation.belt={};preparation.health={};preparation.hudTinyFont={};preparation.hudIcons.clear();
                ALOG("DeusExQuest: original player HUD artwork unavailable: %s",error.what());
            }
            try {
                preparation.focusFont=DecodePortableBitmapFont(uiPackage,"FontMenuSmall_DS");
                auto background=DecodePortableIndexedTexture(uiPackage,"UserInterface.ConWindowBackground",true);
                preparation.focusBackground={background.width,background.height,std::move(background.rgba)};
                const auto scripts=LoadPortablePackageTables(std::string(gameRoot_)+"/System/DeusEx.u");
                const auto focus=LoadPortableClassDescriptor(scripts,FindPortableExport(scripts,"FrobDisplayWindow"));
                // Explicit original Amber presentation palette matching the
                // user's golden PC reference, not a claimed live player theme.
                std::string theme="ColorThemeHUD_Amber";
                std::size_t themeIndex{};
                try {themeIndex=FindPortableExport(scripts,theme);}
                catch (const std::exception&) {theme="ColorThemeHUD_Default";themeIndex=FindPortableExport(scripts,theme);}
                const auto colors=LoadPortableClassDescriptor(scripts,themeIndex);
                std::array<bool,3> foundColor{};
                for (const auto& property:colors.defaults) {
                    if (property.name!="Colors" || property.type!=10u || property.value.size()!=4u) continue;
                    std::array<std::uint8_t,3> color{{property.value[0],property.value[1],property.value[2]}};
                    if (property.arrayIndex==0u) {preparation.focusPalette.background=color;foundColor[0]=true;}
                    else if (property.arrayIndex==1u) {preparation.focusPalette.border=color;foundColor[1]=true;}
                    else if (property.arrayIndex==7u) {preparation.focusPalette.header=color;foundColor[2]=true;}
                }
                if (!foundColor[0] || !foundColor[1] || !foundColor[2])
                    throw std::runtime_error("Original focus palette requires authored background/border/header colors");
                ALOG("DeusExQuest: focus presentation palette %s header=(%u,%u,%u), chosen PC-reference style, not live player theme",
                    theme.c_str(),preparation.focusPalette.header[0],preparation.focusPalette.header[1],preparation.focusPalette.header[2]);
                for (const auto& property:focus.defaults) {
                    if (property.type!=13u || property.arrayIndex!=0u) continue;
                    const auto value=DecodePortableStringProperty(property);
                    if (value.size()>4096u || value.find('\0')!=std::string::npos)
                        throw std::runtime_error("Original focus message exceeds bounded text identity");
                    if (property.name=="msgLocked") preparation.focusMessages.locked=value;
                    else if (property.name=="msgUnlocked") preparation.focusMessages.unlocked=value;
                    else if (property.name=="msgLockStr") preparation.focusMessages.lockStrength=value;
                    else if (property.name=="msgDoorStr") preparation.focusMessages.doorStrength=value;
                    else if (property.name=="msgInf") preparation.focusMessages.infinite=value;
                }
                if (preparation.focusMessages.locked.empty() || preparation.focusMessages.unlocked.empty())
                    throw std::runtime_error("Original focus lock messages unavailable");
            } catch (const std::exception& error) {
                preparation.focusFont={};preparation.focusBackground={};preparation.focusMessages={};
                ALOG("DeusExQuest: original focus label assets unavailable: %s",error.what());
            }
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
            try {
                std::array<PortableTextureImage,2> backgrounds,borders;
                for (std::size_t index=0u;index<2u;++index) {
                    backgrounds[index]=DecodePortableIndexedTexture(uiPackage,
                        QuestVr::kDataLinkBackgroundNames[index],true);
                    borders[index]=DecodePortableIndexedTexture(uiPackage,
                        QuestVr::kDataLinkBorderNames[index],true);
                }
                preparation.dataLinkBase=QuestVr::BuildDataLinkUiCanvas(backgrounds,borders);
                preparation.dataLinkNameFont=DecodePortableBitmapFont(uiPackage,QuestVr::kDataLinkNameFont);
                preparation.dataLinkTextFont=DecodePortableBitmapFont(uiPackage,QuestVr::kDataLinkTextFont);
                // Decode original portraits once on this startup worker. No
                // package reads, font/texture decoding or NPC mesh renders are
                // performed during a moving door or an active transmission.
                const auto portraits=LoadPortablePackageTables(std::string(gameRoot_)+"/Textures/InfoPortraits.utx");
                for (std::size_t index=0u;index<portraits.exports.size();++index) {
                    if (GetPortableObjectPath(portraits,portraits.exports[index].ObjClass)!="Engine.Texture") continue;
                    if (preparation.dataLinkPortraits.size()>=64u) throw std::runtime_error("Original datalink portrait count exceeds bound");
                    const auto name=GetPortableObjectPath(portraits,static_cast<std::int32_t>(index+1u));
                    auto image=DecodePortableIndexedTexture(portraits,name,false);
                    if (image.width!=64u || image.height!=64u) throw std::runtime_error("Original datalink portrait dimensions differ");
                    std::string key=name;
                    std::transform(key.begin(),key.end(),key.begin(),[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                    preparation.dataLinkPortraits.emplace(key,QuestVr::PersonaUiImage{image.width,image.height,std::move(image.rgba)});
                }
                ALOG("DeusExQuest: original HUDInfoLinkDisplay artwork/fonts ready with %zu portraits at 431x114",
                    preparation.dataLinkPortraits.size());
            } catch (const std::exception& error) {
                preparation.dataLinkBase={};
                preparation.dataLinkPortraits.clear();
                ALOG("DeusExQuest: original datalink artwork unavailable; SDK subtitle fallback: %s",error.what());
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
        focusFont_=std::move(preparation.focusFont);
        focusBackground_=std::move(preparation.focusBackground);
        focusMessages_=std::move(preparation.focusMessages);
        focusPalette_=preparation.focusPalette;
        focusDescriptionActor_.clear();focusLabelText_.clear();
        DestroyOriginalPlayerHudUi();
        if (preparation.playerHudReady) {
            try {
                // Fill these from the playable equipment layer and explicitly
                // unfinished aggregate-health presentation on the first frame.
                healthHudTextureId_=CreateOriginalHudTexture(QuestVr::EmptyHudCanvas(QuestVr::kHudHealthWidth,QuestVr::kHudHealthHeight));
                beltHudTextureId_=CreateOriginalHudTexture(QuestVr::EmptyHudCanvas(QuestVr::kHudBeltWidth,QuestVr::kHudBeltHeight));
                hudBeltArtwork_=std::move(preparation.belt);hudHealthArtwork_=std::move(preparation.health);
                hudTinyFont_=std::move(preparation.hudTinyFont);hudIcons_=std::move(preparation.hudIcons);
                healthHudRenderer_.Init(OVRFW::GlTexture(healthHudTextureId_,GL_TEXTURE_2D,
                    static_cast<int>(QuestVr::kHudHealthWidth),static_cast<int>(QuestVr::kHudHealthHeight)));
                beltHudRenderer_.Init(OVRFW::GlTexture(beltHudTextureId_,GL_TEXTURE_2D,
                    static_cast<int>(QuestVr::kHudBeltWidth),static_cast<int>(QuestVr::kHudBeltHeight)));
                ALOG("DeusExQuest: original health/belt artwork ready; playable quick inventory and aggregate-health preview enabled");
            } catch (const std::exception& error) {
                DestroyOriginalPlayerHudUi();
                ALOG("DeusExQuest: original player HUD GPU preparation failed: %s",error.what());
            }
        }
        DestroyOriginalCompassUi();
        if (!preparation.compass.ticks.rgba.empty()) {
            const auto canvas=QuestVr::BuildHudCompassCanvas(preparation.compass,0,0);
            GLuint texture{};glGenTextures(1,&texture);glBindTexture(GL_TEXTURE_2D,texture);
            glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,QuestVr::kHudCompassWidth,QuestVr::kHudCompassHeight,
                0,GL_RGBA,GL_UNSIGNED_BYTE,canvas.rgba.data());
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
            const auto error=glGetError();glBindTexture(GL_TEXTURE_2D,0);
            if (error==GL_NO_ERROR && texture!=0u) {
                compassTextureId_=texture;compassArtwork_=std::move(preparation.compass);
                compassRenderer_.Init(OVRFW::GlTexture(texture,GL_TEXTURE_2D,
                    static_cast<int>(QuestVr::kHudCompassWidth),static_cast<int>(QuestVr::kHudCompassHeight)));
                ALOG("DeusExQuest: original HUDCompassDisplay artwork ready at 73x40; rendered-map heading");
            } else if (texture!=0u) glDeleteTextures(1,&texture);
        }
        if (!focusFont_.glyphs.empty() && !focusBackground_.rgba.empty()) {
            std::vector<std::uint8_t> blank(512u*128u*4u,0u);
            GLuint texture{};glGenTextures(1,&texture);glBindTexture(GL_TEXTURE_2D,texture);
            glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,512,128,0,GL_RGBA,GL_UNSIGNED_BYTE,blank.data());
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
            const auto error=glGetError();glBindTexture(GL_TEXTURE_2D,0);
            if (error==GL_NO_ERROR && texture!=0u) {
                focusLabelTextureId_=texture;
                focusLabelRenderer_.Init(OVRFW::GlTexture(texture,GL_TEXTURE_2D,512,128));
            } else if (texture!=0u) glDeleteTextures(1,&texture);
        }
        dataLinkBase_=std::move(preparation.dataLinkBase);
        dataLinkNameFont_=std::move(preparation.dataLinkNameFont);
        dataLinkTextFont_=std::move(preparation.dataLinkTextFont);
        dataLinkPortraits_=std::move(preparation.dataLinkPortraits);
        if (!dataLinkBase_.rgba.empty()) {
            GLuint texture{};
            glGenTextures(1,&texture);
            glBindTexture(GL_TEXTURE_2D,texture);
            glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,QuestVr::kDataLinkWidth,QuestVr::kDataLinkHeight,
                0,GL_RGBA,GL_UNSIGNED_BYTE,dataLinkBase_.rgba.data());
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
            const auto error=glGetError();
            glBindTexture(GL_TEXTURE_2D,0);
            if (error==GL_NO_ERROR && texture!=0u) {
                dataLinkTextureId_=texture;
                dataLinkRenderer_.Init(OVRFW::GlTexture(texture,GL_TEXTURE_2D,
                    static_cast<int>(QuestVr::kDataLinkWidth),static_cast<int>(QuestVr::kDataLinkHeight)));
            } else if (texture!=0u) {
                glDeleteTextures(1,&texture);
                ALOG("DeusExQuest: original datalink GPU upload failed: %u",static_cast<unsigned>(error));
            }
        }
        // Icon/selection art belongs to this actual loaded UI package generation.
        personaIconCache_.clear();
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

    const PortableTextureImage* GetPersonaIcon(const std::string& qualifiedPath) {
        if (qualifiedPath.empty() || personaUiPackage_.exports.empty()) return nullptr;
        const auto prefix = std::filesystem::path(personaUiPackage_.sourcePath).stem().string()+".";
        const std::string key = personaUiPackage_.sourcePath+"|"+qualifiedPath;
        const auto cached = personaIconCache_.find(key);
        if (cached != personaIconCache_.end())
            return cached->second.width != 0u ? &cached->second : nullptr;
        if (personaIconCache_.size() >= 256u) return nullptr;
        try {
            // An actual property reference is required. No label/substr/class
            // heuristic and no unqualified fallback that may pick another export.
            if (qualifiedPath.compare(0u,prefix.size(),prefix) != 0)
                throw std::runtime_error("Inventory icon is outside the loaded original UI package");
            auto image = DecodePortableIndexedTexture(
                personaUiPackage_,qualifiedPath.substr(prefix.size()),true);
            std::size_t retained{};
            for (const auto& entry : personaIconCache_) retained += entry.second.rgba.size();
            if (image.rgba.size() > 4u*1024u*1024u ||
                retained > 64u*1024u*1024u-image.rgba.size())
                throw std::runtime_error("Inventory artwork exceeds bounded retained cache");
            ALOG(
                "DeusExQuest: decoded original inventory icon %s at %ux%u",
                qualifiedPath.c_str(), image.width, image.height);
            return &personaIconCache_.emplace(key, std::move(image)).first->second;
        } catch (const std::exception& error) {
            ALOG("DeusExQuest: inventory icon %s unavailable: %s", qualifiedPath.c_str(), error.what());
            personaIconCache_.emplace(key, PortableTextureImage{});
            return nullptr;
        }
    }

    const std::array<QuestVr::PersonaUiImage,9>* GetPersonaInventorySelection() {
        constexpr std::array<const char*,9> names{{
            "PersonaItemHighlight_TL","PersonaItemHighlight_TR","PersonaItemHighlight_BL","PersonaItemHighlight_BR",
            "PersonaItemHighlight_Left","PersonaItemHighlight_Right","PersonaItemHighlight_Top",
            "PersonaItemHighlight_Bottom","PersonaItemHighlight_Center"}};
        if (personaChrome_.inventorySelection[0].width == 0u) {
            std::array<QuestVr::PersonaUiImage,9> pieces;
            for (std::size_t index = 0u; index < names.size(); ++index) {
                const auto* image = GetPersonaIcon(std::string("DeusExUI.UserInterface.")+names[index]);
                if (image == nullptr) return nullptr;
                pieces[index] = {image->width,image->height,image->rgba};
            }
            personaChrome_.inventorySelection = std::move(pieces);
        }
        return &personaChrome_.inventorySelection;
    }

    void EnsureInventoryNavigation(const std::vector<std::string>& inventory) {
        const auto revision = GetPortableRuntimeWorldRevision();
        if (inventoryLayoutCached_ && inventory == inventoryNavigationIdentities_ &&
            revision == inventoryNavigationRevision_) return;
        inventoryNavigationIdentities_ = inventory;
        inventoryNavigationRevision_ = revision;
        inventoryLayoutCached_ = true;
        inventoryDescriptors_.clear();
        inventoryGridItems_.clear();
        try {
            inventoryDescriptors_ = ReadPortableRuntimeInventoryDescriptors(inventory);
            if (inventoryDescriptors_.size() != inventory.size())
                throw std::runtime_error("Inventory metadata did not preserve item identities");
            inventoryGridItems_.resize(inventoryDescriptors_.size());
            for (std::size_t index = 0u; index < inventoryDescriptors_.size(); ++index) {
                const auto& descriptor = inventoryDescriptors_[index];
                auto& item = inventoryGridItems_[index];
                item.available = descriptor.status == PortableInventoryDescriptorStatus::Available;
                if (!item.available) continue;
                item.slotsX = static_cast<std::uint32_t>(descriptor.invSlotsX);
                item.slotsY = static_cast<std::uint32_t>(descriptor.invSlotsY);
                item.positionX = descriptor.invPosX; item.positionY = descriptor.invPosY;
                item.iconWidth = static_cast<std::uint32_t>(descriptor.displayWidth);
                item.iconHeight = static_cast<std::uint32_t>(descriptor.displayHeight);
                item.displayable = descriptor.bDisplayableInv;
            }
            const auto layout = QuestVr::BuildPersonaInventoryLayout(inventoryGridItems_);
            if (!inventoryNavigation_.SetLayout(inventory,layout))
                throw std::runtime_error(inventoryNavigation_.State().error);
            selectedInventoryIndex_ = inventoryNavigation_.SelectedIndex().value_or(invalidRendererIndex_);
            const auto& state = inventoryNavigation_.State();
            ALOG("DeusExQuest: inventory navigation visible=%zu hidden=%zu unavailable=%zu overflow=%zu selected=%s",
                state.visibleItems,state.hiddenItems,state.unavailableItems,state.overflowItems,
                inventoryNavigation_.SelectedIdentity().c_str());
        } catch (const std::exception& error) {
            inventoryNavigation_.SetError(error.what(),inventory.size());
            selectedInventoryIndex_ = invalidRendererIndex_;
            ALOG("DeusExQuest: inventory layout unavailable: %s",error.what());
        }
        inventoryMenuDirty_ = true;
        displayedInventoryCount_ = invalidRendererIndex_;
    }

    void MoveInventorySelection(const int steps) {
        if (!inventoryNavigation_.Move(steps)) return;
        inventoryActionStatus_.clear();
        selectedInventoryIndex_ = inventoryNavigation_.SelectedIndex().value_or(invalidRendererIndex_);
        inventoryMenuDirty_ = true;
        displayedInventoryCount_ = invalidRendererIndex_;
    }

    static OVR::Posef PersonaArtworkPose(const OVRFW::ovrApplFrameIn& frame) {
        return QuestVr::PersonaPanelPose(frame.HeadPose);
    }

    void SelectInventoryAtPointer(const OVRFW::ovrApplFrameIn& frame) {
        // Intersect exactly the same head-relative quad used by the renderer,
        // before its scale. Right trigger selects; A remains the use action.
        const auto inverse = PersonaArtworkPose(frame).Inverted();
        const auto origin = inverse.Transform(frame.RightRemotePointPose.Translation);
        const auto direction = inverse.Rotation.Rotate(
            frame.RightRemotePointPose.Rotation.Rotate({0.0f,0.0f,-1.0f}));
        const auto point = QuestVr::IntersectPersonaPanelRay(
            {origin.x,origin.y,origin.z},{direction.x,direction.y,direction.z},
            QuestVr::kPersonaPanelWidth,QuestVr::kPersonaPanelHeight,640u,480u);
        if (!point || !inventoryNavigation_.SelectCanvasPoint(point->x,point->y)) return;
        inventoryActionStatus_.clear();
        selectedInventoryIndex_ = inventoryNavigation_.SelectedIndex().value_or(invalidRendererIndex_);
        inventoryMenuDirty_ = true;
        displayedInventoryCount_ = invalidRendererIndex_;
    }

    std::string SelectedInventoryDisplayName() const {
        const auto selected = inventoryNavigation_.SelectedIndex();
        if (!selected || *selected >= inventoryDescriptors_.size()) return "NO ITEM SELECTED";
        const auto& descriptor = inventoryDescriptors_[*selected];
        return descriptor.itemName && !descriptor.itemName->empty()
            ? *descriptor.itemName : "ITEM NAME UNAVAILABLE";
    }

    void RefreshPersonaInventoryArtwork(const std::vector<std::string>& inventory,
        const std::string& left, const std::string& right) {
        const auto& base = personaPageBaseRgba_[static_cast<std::size_t>(personaPage_)];
        if (personaTextureId_ == 0u || base.empty()) return;
        QuestVr::PersonaUiImage canvas{
            personaTextureWidth_, personaTextureHeight_, base};
        if (personaPage_ == PersonaPage::Inventory) {
            try {
                EnsureInventoryNavigation(inventory);
                if (inventoryNavigation_.State().kind == QuestVr::InventoryViewKind::Error)
                    throw std::runtime_error(inventoryNavigation_.State().error);
                const auto layout = QuestVr::DrawPersonaInventoryGrid(
                    canvas,inventoryGridItems_,selectedInventoryIndex_,
                    [&](std::size_t index) { return GetPersonaIcon(inventoryDescriptors_[index].iconPath); },
                    GetPersonaInventorySelection());
                const bool selectedVisible = std::any_of(layout.placements.begin(),layout.placements.end(),
                    [&](const auto& placement) { return placement.inventoryIndex == selectedInventoryIndex_; });
                ALOG("DeusExQuest: inventory original footprints visible=%zu cells=%zu unavailable=%zu unplaced=%zu displayOnlyPacked=%zu selectedVisible=%u",
                    layout.placements.size(),layout.occupiedCells,layout.unavailable.size(),
                    layout.unplaced.size(),layout.generatedPositions,selectedVisible ? 1u : 0u);
            } catch (const std::exception& error) {
                inventoryNavigation_.SetError(error.what(),inventory.size());
                selectedInventoryIndex_ = invalidRendererIndex_;
                ALOG("DeusExQuest: inventory footprint composition unavailable: %s",error.what());
            }
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
        if (selectedInventoryIndex_ >= inventory.size()) return "NONE";
        const std::string& path = inventory[selectedInventoryIndex_];
        return InventoryItemLabel(path);
    }
    static std::string EquippedInventoryLabel() {
        const auto path=GetPortableRuntimeEquippedItem();
        return path.empty()?"NONE":InventoryItemLabel(path);
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

    void ToggleDebugHud() {
        debugHudVisible_ = !debugHudVisible_;
        if (hudLabel_ != nullptr) hudLabel_->SetVisible(debugHudVisible_ && !inventoryMenuOpen_);
        ALOG("DeusExQuest: debug HUD %s",debugHudVisible_ ? "shown" : "hidden");
    }

    static constexpr const char* posturePreferencePath_ =
        "/data/user/0/dev.deusex.questvr.smoketest/files/DeusEx/quest-posture.cfg";

    static bool ReadSeatedPosturePreference() {
        std::FILE* file = std::fopen(posturePreferencePath_,"rb");
        if (file == nullptr) return false;
        char mode[32]{};
        const bool read = std::fgets(mode,sizeof(mode),file) != nullptr;
        std::fclose(file);
        mode[std::strcspn(mode,"\r\n")] = '\0';
        return read && std::strcmp(mode,"SEATED") == 0;
    }

    void SetPlayerPosture(bool seated, bool persist) {
        if (!headTrackingValid_) return;
        if (seated && gameplayReferenceType_ == XR_REFERENCE_SPACE_TYPE_LOCAL) {
            if (persist) {
                interactionStatus_ = "SEATED HEIGHT REQUIRES FLOOR TRACKING";
                interactionStatusSeconds_ = 4.0f;
            }
            return;
        }
        const float previousFloor = playerPosture_.virtualFloorY;
        const bool changed = seated && playerPosture_.seated && persist ?
            playerPosture_.RecalibrateSeated(currentHeadStage_.y) :
            playerPosture_.SetSeated(seated,currentHeadStage_.y);
        if (!changed) return;
        // Preserve existing map feet while changing the virtual eye height.
        // Otherwise the old floor can lie beyond the step-up search bound.
        worldPosition_.y = QuestVr::WorldYAfterVirtualFloorChange(
            worldPosition_.y,previousFloor,playerPosture_.virtualFloorY);
        if (restorePoseAfterTransition_ && !restoredMapLocalPose_)
            restoredWorldPosition_.y = QuestVr::WorldYAfterVirtualFloorChange(
                restoredWorldPosition_.y,previousFloor,playerPosture_.virtualFloorY);
        seatedPostureRequested_ = seated;
        postureCalibrationPending_ = false;
        seatedCalibrationGate_.Reset();
        if (persist) {
            const std::string scratch = std::string(posturePreferencePath_)+".tmp";
            std::FILE* file = std::fopen(scratch.c_str(),"wb");
            bool saved{};
            if (file != nullptr) {
                const bool written = std::fprintf(file,"%s\n",seated ? "SEATED" : "STANDING") > 0;
                const bool closed = std::fclose(file) == 0;
                saved = written && closed && std::rename(scratch.c_str(),posturePreferencePath_) == 0;
            }
            interactionStatus_ = seated ? "SEATED HEIGHT CALIBRATED" : "STANDING HEIGHT ACTIVE";
            interactionStatusSeconds_ = 4.0f;
            if (!saved) ALOG("DeusExQuest: posture active for session; preference persistence failed");
        }
        ALOG("DeusExQuest: player posture %s physicalHead=%.3f virtualFloor=%.3f eyeHeight=%.3f",
            seated ? "seated" : "standing",currentHeadStage_.y,playerPosture_.virtualFloorY,
            currentHeadStage_.y-playerPosture_.virtualFloorY);
    }

    void SetInventoryMenuOpen(const bool open) {
        if(inventoryMenuOpen_ && !open) quickBeltVisibility_.Reveal();
        inventoryMenuOpen_ = open;
        inventoryMenuDirty_ = true;
        choiceCycleLatch_ = true;
        menuNavigationInput_.Reset();
        inventoryActionStatus_.clear();
        if (open) inventoryLayoutCached_ = false;
        if (hudLabel_ != nullptr) hudLabel_->SetVisible(!open && debugHudVisible_);
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

        const auto menuPose=QuestVr::PersonaPanelPose(frame.HeadPose,QuestVr::kPersonaTextDistance);
        for (OVRFW::VRMenuObject* label : PersonaLabels()) {
            if (label != nullptr) label->SetLocalPose(menuPose);
        }
        personaRenderer_.SetPose(PersonaArtworkPose(frame),QuestVr::kPersonaPanelHalfWidth,QuestVr::kPersonaPanelHalfHeight);
        const std::vector<std::string> inventory = GetPortableRuntimeInventoryItems();
        EnsureInventoryNavigation(inventory);
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
            left = "Inventory";
            const auto selected = inventoryNavigation_.SelectedIndex();
            if (selected && *selected < inventoryDescriptors_.size()) {
                const auto& descriptor = inventoryDescriptors_[*selected];
                right = "ITEM DATA\n\n" + WrapPersonaEntry(SelectedInventoryDisplayName()) + "\n\n";
                right += descriptor.description && !descriptor.description->empty()
                    ? WrapPersonaEntry(*descriptor.description,6u,32u)
                    : "DESCRIPTION UNAVAILABLE";
                if (!inventoryActionStatus_.empty())
                    right += "\n\n" + WrapPersonaEntry(inventoryActionStatus_,3u,32u);
            } else {
                const auto& state = inventoryNavigation_.State();
                right = state.kind == QuestVr::InventoryViewKind::Error
                    ? "INVENTORY LAYOUT UNAVAILABLE\n\n" + WrapPersonaEntry(state.error,6u)
                    : state.kind == QuestVr::InventoryViewKind::Empty ? "INVENTORY EMPTY"
                    : "NO VISIBLE GRID ITEMS\n\n" + std::to_string(state.hiddenItems) + " HIDDEN\n" +
                        std::to_string(state.unavailableItems) + " METADATA UNAVAILABLE\n" +
                        std::to_string(state.overflowItems) + " UNPLACED";
            }
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
        const auto& inventoryState = inventoryNavigation_.State();
        std::string footer = personaPage_ == PersonaPage::Inventory
            ? std::to_string(inventoryState.visibleItems) + " VISIBLE ITEMS   UP / DOWN: SELECT   A: EQUIP\n"
            : "Y: SAVE   X: LOAD\n";
        footer += "LEFT / RIGHT: PAGE   B / MENU: CLOSE";
        if (personaPage_ == PersonaPage::Inventory) {
            footer += "\nPOINT + TRIGGER: SELECT";
            if (inventoryState.overflowItems != 0u)
                footer += "   " + std::to_string(inventoryState.overflowItems) + " UNPLACED";
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

    bool UseSelectedConsumable(const std::string& equippedPath={}) {
        const std::vector<std::string> inventory = GetPortableRuntimeInventoryItems();
        EnsureInventoryNavigation(inventory);
        auto selection = inventoryNavigation_.SelectedIndex();
        if(!equippedPath.empty()) {
            const auto found=std::find(inventory.begin(),inventory.end(),equippedPath);
            if(found==inventory.end()) return false;
            selection=static_cast<std::size_t>(std::distance(inventory.begin(),found));
        }
        if (!selection || *selection >= inventory.size()) return false;
        const std::size_t selected = *selection;
        const std::string& path = inventory[selected];
        const std::string label = InventoryItemLabel(path);
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
        heldVisualCache_.Invalidate();
        inventoryLayoutCached_ = false;
        hudQuerySeconds_=0.0f;
        EnsureInventoryNavigation(GetPortableRuntimeInventoryItems());
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

    std::optional<QuestVr::EquippedMeleeProfile> CurrentEquippedMeleeProfile() const {
        if(!equippedMeleeProfile_ || !CanReadEquippedItemVisual()) return {};
        const auto current=GetPortableRuntimeEquippedItem();
        if(!QuestVr::EquippedItemVisualDetail::Same(current,heldEquippedIdentity_)) return {};
        return equippedMeleeProfile_;
    }

    float SelectedWeaponDamage(const std::vector<std::string>& inventory) {
        static_cast<void>(inventory);
        if(const auto melee=CurrentEquippedMeleeProfile()) return static_cast<float>(melee->hitDamageOriginal);
        const std::string item = EquippedInventoryLabel();
        if (item.find("Weapon") == std::string::npos) return 0.0f;
        if (item.find("GEP") != std::string::npos || item.find("LAW") != std::string::npos) return 80.0f;
        if (item.find("Sniper") != std::string::npos) return 40.0f;
        if (item.find("Shotgun") != std::string::npos) return 32.0f;
        if (item.find("Pistol") != std::string::npos) return 20.0f;
        if (item.find("Crowbar") != std::string::npos || item.find("Baton") != std::string::npos) return 12.0f;
        return 18.0f;
    }

    bool SelectedWeaponHasAmmo(const std::vector<std::string>& inventory) {
        if(CurrentEquippedMeleeProfile()) return true;
        const std::string weapon = EquippedInventoryLabel();
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

    float SelectedWeaponRange(const std::vector<std::string>& inventory) {
        static_cast<void>(inventory);
        if(const auto melee=CurrentEquippedMeleeProfile()) return static_cast<float>(melee->maxRangeOriginal)/52.5f;
        const std::string item = EquippedInventoryLabel();
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
        if (!runtimeAvailable_ || ActorPublicationBlocksGameplay() ||
            !pendingMapName_.empty() || !transitionMapName_.empty()) return false;
        const OVR::Vector3f origin = pointerPose.Translation;
        const OVR::Vector3f direction =
            pointerPose.Rotation.Rotate(OVR::Vector3f(0.0f, 0.0f, -1.0f));
        const OVR::Quatf worldRotation(
            OVR::Vector3f(0.0f, 1.0f, 0.0f), sceneYaw_);
        const InteractiveActor* best{};
        float bestDistance = weaponRange;
        for (const InteractiveActor& actor : interactiveActors_) {
            if (actor.presentationOnly) continue;
            if (!IsPublishedActorVisible(actor.objectPath)) continue;
            if (moverVisuals_.find(actor.objectPath) != moverVisuals_.end()) continue;
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
        // An actual moving brush face occludes a point-targeted NPC behind it.
        if (const auto mover = RayTargetMover(origin,direction,bestDistance)) best = mover;
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
        if (damage.killed && damage.worldChanged && PublishRuntimeActorRemoval(damage.objectPath))
            return false;
        return damage.worldChanged;
    }

    void SaveGameState() {
        if (!runtimeAvailable_ || actorGeometryBuild_ || runtimeActorTextureUploadPending_ || !headTrackingValid_ || !pendingChoices_.empty() ||
            !pendingMapName_.empty() || !transitionMapName_.empty()) return;
        bool saved = false;
        std::uint64_t generation{};
        try {
            const OVR::Vector3f localFeet = StageToLocal(
                {currentHeadStage_.x, playerPosture_.virtualFloorY, currentHeadStage_.z}, worldPosition_);
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
        if (!runtimeAvailable_ || actorGeometryBuild_ || runtimeActorTextureUploadPending_ || !headTrackingValid_ ||
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
            if (GetPortableRuntimeUnsavedScriptState()) return false;
            restoredMapLocalPose_ = metadata.mapLocalPose;
            restoredMapLocalFeet_ = {pose[0], pose[1], pose[2]};
            restoredMapLocalHeadYaw_ = pose[3];
            // Legacy raw tracking poses assumed standing floor zero. New
            // map-local saves retain feet and need no stored posture offset.
            restoredWorldPosition_ = {pose[0], pose[1]+playerPosture_.virtualFloorY, pose[2]};
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
        heldVisualCache_.Invalidate();selectedHeldVisual_.reset();equippedMeleeProfile_.reset();
        ResetActorRemovalState();
        inventoryLayoutCached_ = false;
        ClearPendingConversation();
        InvalidateDialogueAudio();
        // Restore only after the runtime parser succeeds. Failed quick-loads
        // must keep UI history and any in-progress actor chunks unchanged.
        dialogueOffsets_ = std::move(metadata.dialogueOffsets);
        personaLogEntries_ = std::move(metadata.personaLogs);
        inventoryMenuDirty_ = true;
        if (metadata.mapLocalPose) {
            QuestVr::RestoreSavedMapPose(OVR::Vector3f{pose[0], pose[1], pose[2]}, pose[3],
                currentHeadStage_, currentHeadStageYaw_, worldPosition_, sceneYaw_,playerPosture_.virtualFloorY);
        } else {
            worldPosition_ = {pose[0], pose[1]+playerPosture_.virtualFloorY, pose[2]};
            sceneYaw_ = pose[3];
        }
        previousHeadStage_ = currentHeadStage_;
        try {
            RebuildRuntimeActorPublication();
        } catch (const std::exception& error) {
            // State restoration already succeeded; do not claim it was kept
            // unchanged or try an older candidate after a visual allocation
            // failure. Suspend gameplay rather than retain mismatched actors.
            DestroySceneGeometry();
            SuspendRuntimeActorPublication("QUICK-LOAD VISUAL REBUILD FAILED - RESTART SESSION");
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
            emitter.actorPath = actor.objectPath;
            emitter.localPosition = {
                (actor.y - originY) * unitsToMeters,
                (actor.z - originZ) * unitsToMeters + 1.0f,
                -(actor.x - originX) * unitsToMeters};
            emitter.soundPath = actor.ambientSoundPath;
            emitter.soundPitch = actor.soundPitch;
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
        // Prepare the shared-clip catalogue copy before acquiring the callback
        // mutex. It owns no runtime/mesh pointers and survives emitter deletion.
        auto catalog = emitters;
        std::lock_guard<std::mutex> lock(audioMutex_);
        spatialAudioEmitters_ = std::move(emitters);
        spatialAudioEmitterCatalog_ = std::move(catalog);
        ambientSamples_.clear();
        ambientCursor_ = 0u;
    }

    void ReconcileSpatialAudioEmitters(const std::vector<PortableActorSnapshot>& actors) {
        // Snapshot identities are read only by the XR owner. The audio callback
        // only borrows the committed emitter collection under audioMutex_.
        std::unordered_map<std::string, const PortableActorSnapshot*> live;
        live.reserve(actors.size());
        for (const auto& actor : actors) live.emplace(actor.objectPath, &actor);
        constexpr float unitsToMeters = 1.0f / 52.5f;
        const auto sameIdentity = [](const std::string& left, const std::string& right) {
            return left.size() == right.size() && std::equal(left.begin(), left.end(), right.begin(),
                [](unsigned char a, unsigned char b) {
                    if (a >= 'A' && a <= 'Z') a = static_cast<unsigned char>(a + ('a' - 'A'));
                    if (b >= 'A' && b <= 'Z') b = static_cast<unsigned char>(b + ('a' - 'A'));
                    return a == b;
                });
        };
        const auto compatible = [&](const SpatialAudioEmitter& emitter) {
            const auto found = live.find(emitter.actorPath);
            return found != live.end() && found->second->hasLocation &&
                std::isfinite(found->second->x) && std::isfinite(found->second->y) && std::isfinite(found->second->z) &&
                sameIdentity(found->second->ambientSoundPath, emitter.soundPath) &&
                found->second->soundPitch == emitter.soundPitch;
        };
        const auto refresh = [&](SpatialAudioEmitter& emitter) {
            const auto& actor = *live.at(emitter.actorPath);
            emitter.localPosition = {
                (actor.y - activeMapUnrealOrigin_.y) * unitsToMeters,
                (actor.z - activeMapUnrealOrigin_.z) * unitsToMeters + 1.0f,
                -(actor.x - activeMapUnrealOrigin_.x) * unitsToMeters};
            emitter.radiusMeters = std::max(2.0f,
                static_cast<float>(actor.soundRadius) * 25.0f * unitsToMeters);
            emitter.volume = static_cast<float>(actor.soundVolume) / 255.0f;
        };
        std::size_t removed{}, restored{}, unavailable{};
        {
            std::lock_guard<std::mutex> lock(audioMutex_);
            const auto previous = spatialAudioEmitters_.size();
            spatialAudioEmitters_.erase(std::remove_if(spatialAudioEmitters_.begin(), spatialAudioEmitters_.end(),
                [&](const auto& emitter) { return !compatible(emitter); }), spatialAudioEmitters_.end());
            removed = previous - spatialAudioEmitters_.size();
            std::set<std::string> audible;
            for (auto& emitter : spatialAudioEmitters_) {
                refresh(emitter);
                audible.insert(emitter.actorPath);
            }
            for (const auto& emitter : spatialAudioEmitterCatalog_) {
                if (!compatible(emitter) || audible.count(emitter.actorPath)) continue;
                spatialAudioEmitters_.push_back(emitter);
                refresh(spatialAudioEmitters_.back());
                audible.insert(emitter.actorPath);
                ++restored;
            }
            for (const auto& actor : actors)
                if (actor.hasLocation && !actor.ambientSoundPath.empty() && !audible.count(actor.objectPath)) ++unavailable;
        }
        if (removed || restored || unavailable)
            ALOG("DeusExQuest: committed ambient actor publication removed=%zu restored-from-map-clips=%zu unavailable-clips=%zu; no XR-thread decode",
                removed, restored, unavailable);
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

    void PublishMapMusic(QuestVr::MapMusicPreparation preparation) {
        std::shared_ptr<QuestVr::MusicStream> retired;
        {
            std::lock_guard<std::mutex> lock(audioMutex_);
            retired=std::move(musicStream_);
            if (preparation.stream && preparation.stream->SampleRate()==audioSampleRate_)
                musicStream_=std::move(preparation.stream);
        }
        // Thread join/decoder destruction never occurs under callback lock.
        retired.reset();
        if (!preparation.error.empty()) {
            ALOG("DeusExQuest: original map music unavailable: %s",preparation.error.c_str());
        } else if (musicStream_) {
            ALOG("DeusExQuest: original map music playing %s format=%s section=%u rate=%u duration=%.2fs; decoder worker/fixed PCM ring",
                preparation.songPath.c_str(),preparation.format.c_str(),preparation.section,
                audioSampleRate_,musicStream_->DurationSeconds());
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
        if (app->musicStream_ && app->musicPlaybackActive_.load(std::memory_order_acquire))
            app->musicStream_->Mix(output,static_cast<std::size_t>(numFrames),0.35f);
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
        dialogueSubtitle_.clear();
        dialogueSubtitlePages_.clear();
        dialogueSubtitleSeconds_ = dialogueSubtitleTotalSeconds_ = 0.0f;
        dataLinkSpeech_.clear();
        dataLinkUiVisible_=false;
        dataLinkElapsedSeconds_=0.0f;
        touchDataLinkPollSeconds_ = 0.0f;
        pendingTouchDataLinks_.clear();
        if (subtitleLabel_ != nullptr) subtitleLabel_->SetVisible(false);
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
        if (!dialogueSubtitle_.empty() && decoded.targetRate != 0u)
            dialogueSubtitleSeconds_ = std::max(dialogueSubtitleSeconds_,
                static_cast<float>(outputFrames)/static_cast<float>(decoded.targetRate));
        dialogueSubtitleTotalSeconds_ = dialogueSubtitleSeconds_;
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
            AAudioStream_getSampleRate(ambientStream_)!=static_cast<std::int32_t>(sampleRate) ||
            AAudioStream_getChannelCount(ambientStream_)!=2 ||
            AAudioStream_getFormat(ambientStream_)!=AAUDIO_FORMAT_PCM_I16 ||
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
        musicStream_.reset();
        preparedMapMusic_={};
        ambientCursor_ = 0;
        spatialAudioEmitters_.clear();
        spatialAudioEmitterCatalog_.clear();
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
        // Movers own a small, bounded live triangle set. Do not leave their
        // closed-position triangles in the immutable BSP spatial grid.
        const float reach = 8.0f*static_cast<float>(cellRadius+1);
        for (const auto& [path,visual] : moverVisuals_) {
            if (!visual.blocksPlayer || !visual.hasBounds ||
                point.x+reach < visual.minimum.x || point.x-reach > visual.maximum.x ||
                point.z+reach < visual.minimum.z || point.z-reach > visual.maximum.z) continue;
            for (const auto& triangle : visual.collision) callback(triangle);
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

    std::optional<float> GroundHeightForFeet(const OVR::Vector3f& feetLocal) const {
        QuestVr::GroundProbe probe(feetLocal.y);
        ForNearbyTriangles(feetLocal, 1, [&](const CollisionTriangle& triangle) {
            if (const auto floor = QuestVr::GroundHeightWithinRadius(feetLocal,triangle)) probe.Add(*floor);
        });
        return probe.Height();
    }

    void FollowGround(const OVR::Vector3f& head, OVR::Vector3f& worldPosition) const {
        if (!QuestVr::GroundCollisionReady(runtimeAvailable_,
            !pendingMapName_.empty(),!transitionMapName_.empty(),
            runtimeActorTextureUploadPending_ || static_cast<bool>(actorGeometryBuild_))) return;
        const OVR::Vector3f feetStage{head.x, playerPosture_.virtualFloorY, head.z};
        const OVR::Vector3f feetLocal = StageToLocal(feetStage, worldPosition);
        if (const auto floor = GroundHeightForFeet(feetLocal)) worldPosition.y = playerPosture_.virtualFloorY-*floor;
    }

    bool CapsuleTouchesWall(
        const OVR::Vector3f& head,
        const OVR::Vector3f& worldPosition) const {
        if (!QuestVr::GroundCollisionReady(runtimeAvailable_,
            !pendingMapName_.empty(),!transitionMapName_.empty(),
            runtimeActorTextureUploadPending_ || static_cast<bool>(actorGeometryBuild_))) return false;
        const OVR::Vector3f center = StageToLocal({head.x, playerPosture_.virtualFloorY+0.85f, head.z}, worldPosition);
        const OVR::Vector3f feet = StageToLocal({head.x,playerPosture_.virtualFloorY,head.z},worldPosition);
        bool touching = false;
        ForNearbyTriangles(center, 1, [&](const CollisionTriangle& triangle) {
            if (touching) return;
            touching = QuestVr::CapsuleTouchesTriangle(feet,triangle,
                [](const OVR::Vector3f& point, const CollisionTriangle& face) {
                    return ClosestPointOnTriangle(point,face);
                });
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
        // Flags are recovered from the exact original surface IDs in DXQS;
        // no texture-name guesses or cache ABI change. Load after the baker
        // releases its model so two complete BSP models are not retained.
        const auto model = LoadPortableRootModel68(package);
        std::vector<std::uint32_t> surfaceFlags;
        surfaceFlags.reserve(model.surfaces.size());
        for (const auto& surface : model.surfaces) surfaceFlags.push_back(surface.polyFlags);
        std::vector<WorldMeshChunkPreparation> gpuChunks;
        constexpr std::size_t verticesPerGpuBatch=4800u;
        for (std::size_t index=0; index<mesh.chunks.size(); ++index) {
            const auto& raw=mesh.chunks[index];
            const auto& baked=lightmap.vertices[index];
            if (baked.size() != raw.vertices.size() || raw.surfaces.size() != raw.vertices.size())
                throw std::runtime_error("baked lightmap raw vertex count mismatch");
            for (const auto& range : QuestVr::BuildWorldMaterialRanges(raw.surfaces,surfaceFlags,verticesPerGpuBatch)) {
                const auto offset=range.begin, end=range.end;
                WorldMeshChunkPreparation chunk;
                chunk.materialSlot=raw.materialSlot;
                chunk.polyFlags=range.polyFlags;
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
                descriptor,firstTexture_,staticLightmapTexture_,staticLightmapGainScale_,chunk.polyFlags);
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
        try {
            RetainActorAtlas(OVRFW::GlTexture(
                texture,
                GL_TEXTURE_2D_ARRAY,
                static_cast<int>(array.width),
                static_cast<int>(array.height)));
        } catch (...) {
            // The synchronous upload has no pending-upload owner to recover
            // this GL handle if allocation/publication fails.
            glDeleteTextures(1, &texture);
            throw;
        }
        actorTexturePaths_ = std::move(array.texturePaths);
        actorTexturePolyFlags_ = std::move(array.texturePolyFlags);
        actorMaskedTextureLayers_ = std::move(array.maskedTextureLayers);
        NormalizeActorMaskedTextureAliases();
        return actorTexture_.IsValid();
    }

    void RetainActorAtlas(const OVRFW::GlTexture& texture) {
        if (!texture.IsValid() || actorAtlasSerial_ == std::numeric_limits<std::uint64_t>::max())
            throw std::runtime_error("Actor atlas exceeds its lifetime bounds");
        auto atlas = std::make_shared<ActorAtlasGeneration>();
        atlas->generation = ++actorAtlasSerial_;
        atlas->texture = texture;
        actorAtlas_ = std::move(atlas);
        actorTexture_ = actorAtlas_->texture; // Non-owning current-build alias.
    }

    bool BeginActorTextureUpload(PortableTextureArray array) {
        ALOG(
            "DeusExQuest: decoded %zu/%zu actor textures (%zu fallback layers)",
            array.decodedTextures,
            array.texturePaths.size(),
            array.failedTextures);
        if (!array.passed || array.rgba.empty() || array.texturePaths.empty() ||
            pendingActorTextureId_ != 0u || array.width == 0u || array.height == 0u ||
            array.width > 2048u || array.height > 2048u || array.texturePaths.size() > 256u ||
            array.texturePolyFlags.size() != array.texturePaths.size() ||
            array.maskedTextureLayers.size() != array.texturePaths.size() ||
            array.rgba.size() != static_cast<std::size_t>(array.width) * array.height *
                array.texturePaths.size() * 4u) return false;
        if (std::any_of(array.maskedTextureLayers.begin(), array.maskedTextureLayers.end(),
            [&](const auto index) { return index < -1 || (index >= 0 &&
                static_cast<std::size_t>(index) >= array.texturePaths.size()); })) return false;
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

    void ClearPendingActorTextureUpload() {
        if (pendingActorTextureId_ != 0u) glDeleteTextures(1, &pendingActorTextureId_);
        pendingActorTextureId_ = 0u;
        pendingActorTextureWidth_ = pendingActorTextureHeight_ = 0u;
        pendingActorTextureLayers_ = pendingActorTextureLayersUploaded_ = 0u;
        pendingActorTextureRgba_.clear();
        pendingActorTexturePaths_.clear();
        pendingActorTexturePolyFlags_.clear();
        pendingActorMaskedTextureLayers_.clear();
        runtimeActorTextureUploadPending_ = false;
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
            // The old visible bank keeps its own shared atlas generation.
            // Replacing this build alias must not retire the old GL texture
            // while retained renderer commands still address its old layers.
            RetainActorAtlas(OVRFW::GlTexture(
                pendingActorTextureId_,
                GL_TEXTURE_2D_ARRAY,
                static_cast<int>(pendingActorTextureWidth_),
                static_cast<int>(pendingActorTextureHeight_)));
            pendingActorTextureId_ = 0u;
            pendingActorTextureRgba_.clear();
            actorTexturePaths_ = std::move(pendingActorTexturePaths_);
            actorTexturePolyFlags_ = std::move(pendingActorTexturePolyFlags_);
            actorMaskedTextureLayers_ = std::move(pendingActorMaskedTextureLayers_);
            NormalizeActorMaskedTextureAliases();
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
    std::array<std::deque<TexturedGeometryRenderer>,3> playerVisualRenderers_;
    std::array<OVRFW::GlTexture,8> playerVisualTextures_;
    struct HeldItemGpuVariant {
        std::deque<TexturedGeometryRenderer> renderers;
        std::vector<OVRFW::GlTexture> textures;
    };
    std::array<HeldItemGpuVariant,3> heldItemGpu_;
    std::array<QuestVr::PreparedHeldVisualKey,3> heldItemKeys_;
    QuestVr::HeldVisualCache heldVisualCache_;
    std::optional<std::size_t> selectedHeldVisual_;
    std::optional<QuestVr::EquippedMeleeProfile> equippedMeleeProfile_;
    std::string heldEquippedIdentity_,heldItemVisualError_;
    bool heldItemVisualsReady_{},playerVisualsCanRender_{};
    bool heldStateDiagnosticRequested_{};
    std::unique_ptr<ActorGeometryBuild> actorGeometryBuild_;
    QuestVr::RetainedBankPublication<ActorGeometryBank> actorGeometryPublication_;
    ActorGeometryBank* activeActorGeometry_{}; // Publication-owned stable storage.
    std::shared_ptr<ActorAtlasGeneration> actorAtlas_;
    std::uint64_t actorAtlasSerial_{},actorSceneGeneration_{1u};
    // App-owned, so cancelling/destroying ActorGeometryBuild never blocks on an
    // unfinished std::async future. There is exactly one live pose job globally,
    // including retired jobs; final app destruction joins it after XR shutdown.
    std::future<std::shared_ptr<const QuestVr::MeshPose>> actorPoseFuture_;
    QuestVr::AsyncResultEpoch actorPoseEpoch_;
    std::uint64_t actorPoseJobEpoch_{};
    std::size_t actorPoseActorOrdinal_{};
    bool actorGeometryComplete_{};
    std::vector<PortableActorSnapshot> actorSnapshots_;
    QuestVr::ActorRemovalState actorRemovals_;
    QuestVr::RetainedMoverPoseSnapshots<PortableActorSnapshot> latestMoverSnapshots_;
    std::uint64_t publishedRuntimeWorldRevision_{};
    bool hasPublishedRuntimeWorldRevision_{};
    std::vector<MapLight> activeMapLights_;
    std::vector<InteractiveActor> interactiveActors_;
    TargetingFrameRenderer targetingFrameRenderer_;
    OVRFW::GlTexture firstTexture_;
    OVRFW::GlTexture actorTexture_;
    std::vector<std::string> actorTexturePaths_;
    std::vector<std::uint32_t> actorTexturePolyFlags_;
    std::vector<std::int32_t> actorMaskedTextureLayers_;
    std::unordered_map<std::string, ActorMaterialMetadata> actorMaterialMetadata_;
    std::size_t actorMaterialProbeFallbackLogs_{};
    bool runtimeActorTextureUploadPending_{};
    static constexpr std::size_t invalidRendererIndex_ =
        std::numeric_limits<std::size_t>::max();
    AAudioStream* ambientStream_{};
    std::mutex audioMutex_;
    std::vector<std::int16_t> ambientSamples_;
    std::size_t ambientCursor_{};
    std::vector<SpatialAudioEmitter> spatialAudioEmitters_;
    std::vector<SpatialAudioEmitter> spatialAudioEmitterCatalog_;
    std::uint32_t audioSampleRate_{};
    std::shared_ptr<QuestVr::MusicStream> musicStream_;
    std::atomic<bool> musicPlaybackActive_{};
    std::vector<std::int16_t> dialogueSamples_;
    std::string dialogueSubtitle_, displayedSubtitle_;
    std::vector<std::string> dialogueSubtitlePages_;
    static constexpr std::size_t maxPendingTouchDataLinks_ = 16u;
    std::deque<PortableDialogueResult> pendingTouchDataLinks_;
    float dialogueSubtitleSeconds_{}, dialogueSubtitleTotalSeconds_{}, touchDataLinkPollSeconds_{};
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
    std::unordered_map<std::string,MoverVisual> moverVisuals_;
    std::size_t moverCollisionTriangleCount_{};
    std::size_t moverPublicationFrames_{}, moverPublicationLargestSubset_{};
    double moverPublicationMilliseconds_{}, moverPublicationWorstMilliseconds_{};
    std::unordered_map<std::int64_t, std::vector<std::uint32_t>> collisionGrid_;
    std::vector<std::uint32_t> oversizedCollisionTriangles_;
    OVR::Vector3f worldPosition_{0.0f, 0.0f, 0.0f};
    OVR::Vector3f currentHeadStage_{};
    OVR::Vector3f previousHeadStage_{};
    bool hasPreviousHeadStage_{};
    bool headTrackingValid_{};
    bool headTrackingActive_{};
    float currentHeadStageYaw_{};
    float selfBodyStageYaw_{};
    bool selfBodyYawInitialized_{};
    bool headTrackingReported_{};
    bool needsTrackingRebase_{};
    OVR::Vector3f trackingResumeLocalHead_{};
    std::vector<XrEventDataReferenceSpaceChangePending> pendingReferenceChanges_;
    XrSpace gameplayFloorSpace_{XR_NULL_HANDLE};
    XrReferenceSpaceType gameplayReferenceType_{XR_REFERENCE_SPACE_TYPE_STAGE};
    bool hasTransitionHeadAnchor_{};
    OVR::Vector3f transitionHeadLocal_{};
    OVR::Vector3f actorStreamingCenter_{};
    float sceneYaw_{};
    bool turnLatch_{};
    bool choiceCycleLatch_{};
    bool fireLatch_{};
    bool inventoryCycleLatch_{};
    bool debugHudVisible_{true};
    bool hideDebugHudAtFirstReady_{true};
    bool debugHudToggleLatch_{true};
    QuestVr::SeatedPosture playerPosture_;
    QuestVr::SeatedCalibrationGate seatedCalibrationGate_;
    bool seatedPostureRequested_{};
    bool postureCalibrationPending_{};
    inline static std::size_t selectedInventoryIndex_{};
    QuestVr::InventoryNavigationModel inventoryNavigation_;
    QuestVr::MenuNavigationInput menuNavigationInput_;
    std::vector<std::string> inventoryNavigationIdentities_;
    std::vector<PortableInventoryDescriptor> inventoryDescriptors_;
    std::vector<QuestVr::PersonaInventoryItem> inventoryGridItems_;
    std::uint64_t inventoryNavigationRevision_{};
    bool inventoryLayoutCached_{};
    std::string inventoryActionStatus_;
    std::size_t performanceFrames_{};
    float performanceSeconds_{};
    float performanceWorstDelta_{};
    OVRFW::TinyUI ui_;
    std::vector<OVRFW::ovrSurfaceDef> headLockedUiSurfaces_;
    OVRFW::VRMenuObject* hudLabel_{};
    OVRFW::VRMenuObject* subtitleLabel_{};
    OVRFW::VRMenuObject* controlHintLabel_{};
    OVRFW::VRMenuObject* inventoryLabel_{};
    OVRFW::VRMenuObject* personaTabsLabel_{};
    OVRFW::VRMenuObject* personaDetailsLabel_{};
    OVRFW::VRMenuObject* personaFooterLabel_{};
    PersonaUiRenderer personaRenderer_;
    PersonaUiRenderer focusLabelRenderer_;
    PortableBitmapFont focusFont_;
    QuestVr::PersonaUiImage focusBackground_;
    QuestVr::FrobDescriptionMessages focusMessages_;
    QuestVr::FrobDescriptionPalette focusPalette_;
    GLuint focusLabelTextureId_{};
    std::string focusDescriptionActor_,focusLabelText_;
    std::optional<std::string> focusDescription_;
    std::uint64_t focusDescriptionRevision_{std::numeric_limits<std::uint64_t>::max()};
    std::uint32_t focusLabelTextWidth_{},focusLabelTextHeight_{};
    float focusDescriptionPollSeconds_{};
    PersonaUiRenderer dataLinkRenderer_;
    QuestVr::PersonaUiImage dataLinkBase_;
    PortableBitmapFont dataLinkNameFont_;
    PortableBitmapFont dataLinkTextFont_;
    std::unordered_map<std::string,QuestVr::PersonaUiImage> dataLinkPortraits_;
    GLuint dataLinkTextureId_{};
    std::string dataLinkBindName_,dataLinkSpeech_;
    float dataLinkElapsedSeconds_{};
    std::size_t dataLinkDisplayedCharacters_{invalidRendererIndex_};
    bool dataLinkDisplayedCursor_{},dataLinkUiVisible_{};
    QuestVr::HudCompassArtwork compassArtwork_;
    PersonaUiRenderer compassRenderer_;
    GLuint compassTextureId_{};
    bool compassUiVisible_{};
    std::optional<QuestVr::HudCompassPositions> compassDisplayedPositions_;
    std::optional<PortableMapNorthSnapshot> preparedMapNorth_,activeMapNorth_;
    QuestVr::HudBeltArtwork hudBeltArtwork_;
    QuestVr::HudHealthArtwork hudHealthArtwork_;
    PortableBitmapFont hudTinyFont_;
    std::unordered_map<std::string,QuestVr::PersonaUiImage> hudIcons_;
    PersonaUiRenderer healthHudRenderer_,beltHudRenderer_;
    GLuint healthHudTextureId_{},beltHudTextureId_{};
    bool healthUiVisible_{},beltUiVisible_{};
    QuestVr::QuickBeltVisibility quickBeltVisibility_;
    bool hudStateDiagnosticRequested_{};
    std::optional<PortablePlayerHudSnapshot> cachedPlayerHud_;
    QuestVr::HudHealthClock hudHealthClock_;
    QuestVr::HudOxygenClock hudOxygenClock_;
    std::vector<std::uint8_t> displayedHealthHudPixels_,displayedBeltHudPixels_;
    float hudQuerySeconds_{};
    std::string playerHudError_;
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
    std::uint64_t nativeClientGeneration_{};
    std::shared_ptr<QuestVr::NativeClientLifetime> nativeClientLifetime_;
    std::shared_ptr<QuestVr::QuestInputSource> nativeQuestInputSource_;
    bool questInputPublishFailureReported_{};
    std::future<InitialPreparation> initialPreparationFuture_;
    std::future<MapPreparation> mapCacheFuture_;
    std::string transitionMapName_;
    MapTransitionPhase transitionPhase_{MapTransitionPhase::Idle};
    std::vector<PortableActorSnapshot> preparedActorSnapshots_;
    std::map<std::string,QuestVr::NpcWaitingInstance> preparedNpcWaiting_,npcWaiting_;
    std::map<const QuestVr::NpcAnimationClip*,std::shared_ptr<NpcGpuClip>> npcGpuClips_;
    double npcAnimationSeconds_{};
    std::vector<SpatialAudioEmitter> preparedSpatialAudioEmitters_;
    QuestVr::MapMusicPreparation preparedMapMusic_;
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
