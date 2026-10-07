#pragma once

#include <array>
#include <cmath>
#include <cstdint>
#include <stdexcept>

// UE1 actor transforms, independent of either desktop or OpenXR math types.
// Rotation follows the pinned Coords::Rotation(...).ToMatrix() implementation.
namespace QuestVr {

struct ActorVec3 {
    float x{}, y{}, z{};
};

struct ActorMatrix3 {
    // Row-major; vectors are columns.
    std::array<float, 9> values{1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f};

    ActorVec3 Transform(const ActorVec3& v) const {
        return {values[0]*v.x + values[1]*v.y + values[2]*v.z,
                values[3]*v.x + values[4]*v.y + values[5]*v.z,
                values[6]*v.x + values[7]*v.y + values[8]*v.z};
    }
    float Determinant() const {
        return values[0]*(values[4]*values[8]-values[5]*values[7])
             + values[1]*(values[5]*values[6]-values[3]*values[8])
             + values[2]*(values[3]*values[7]-values[4]*values[6]);
    }
    ActorMatrix3 NormalMatrix() const {
        const float determinant = Determinant();
        if (!std::isfinite(determinant) || std::fabs(determinant) < 1.0e-12f)
            throw std::runtime_error("Actor transform is singular or non-finite");
        const float inverse = 1.0f / determinant;
        return {{{(values[4]*values[8]-values[5]*values[7])*inverse,
                  (values[5]*values[6]-values[3]*values[8])*inverse,
                  (values[3]*values[7]-values[4]*values[6])*inverse,
                  (values[2]*values[7]-values[1]*values[8])*inverse,
                  (values[0]*values[8]-values[2]*values[6])*inverse,
                  (values[1]*values[6]-values[0]*values[7])*inverse,
                  (values[1]*values[5]-values[2]*values[4])*inverse,
                  (values[2]*values[3]-values[0]*values[5])*inverse,
                  (values[0]*values[4]-values[1]*values[3])*inverse}}};
    }
};

inline ActorMatrix3 operator*(const ActorMatrix3& a, const ActorMatrix3& b) {
    ActorMatrix3 result;
    for (std::size_t row = 0; row < 3u; ++row)
        for (std::size_t column = 0; column < 3u; ++column)
            result.values[row*3u+column] =
                a.values[row*3u]*b.values[column] +
                a.values[row*3u+1u]*b.values[column+3u] +
                a.values[row*3u+2u]*b.values[column+6u];
    return result;
}

inline bool IsFiniteActorVector(const ActorVec3& v) {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

inline ActorVec3 NormalizeActorVector(const ActorVec3& v) {
    const float length = std::sqrt(v.x*v.x + v.y*v.y + v.z*v.z);
    if (!std::isfinite(length)) throw std::runtime_error("Actor normal is non-finite");
    if (length <= 1.0e-12f) return {};
    return {v.x/length, v.y/length, v.z/length};
}

inline ActorMatrix3 UnrealActorRotation(
    const std::int32_t pitch, const std::int32_t yaw, const std::int32_t roll) {
    // Match Rotator::...Radians(): reduce before converting to float, including
    // negative and large rotations. UE1 yaw is around +Z, pitch around -Y, and
    // roll around -X. Their object-to-world order is yaw * pitch * roll.
    constexpr float radians = 3.14159265359f / 32768.0f;
    const float p = static_cast<float>(static_cast<std::uint32_t>(pitch) & 0xffffu)*radians;
    const float y = static_cast<float>(static_cast<std::uint32_t>(yaw) & 0xffffu)*radians;
    const float r = static_cast<float>(static_cast<std::uint32_t>(roll) & 0xffffu)*radians;
    const float cp = std::cos(p), sp = std::sin(p);
    const float cy = std::cos(y), sy = std::sin(y);
    const float cr = std::cos(r), sr = std::sin(r);
    const ActorMatrix3 yawMatrix{{{cy, -sy, 0.0f, sy, cy, 0.0f, 0.0f, 0.0f, 1.0f}}};
    const ActorMatrix3 pitchMatrix{{{cp, 0.0f, -sp, 0.0f, 1.0f, 0.0f, sp, 0.0f, cp}}};
    const ActorMatrix3 rollMatrix{{{1.0f, 0.0f, 0.0f, 0.0f, cr, sr, 0.0f, -sr, cr}}};
    return yawMatrix * pitchMatrix * rollMatrix;
}

inline ActorMatrix3 ActorScaleMatrix(const ActorVec3& scale) {
    if (!IsFiniteActorVector(scale)) throw std::runtime_error("Actor scale is non-finite");
    return {{{scale.x, 0.0f, 0.0f, 0.0f, scale.y, 0.0f, 0.0f, 0.0f, scale.z}}};
}

inline ActorMatrix3 UnrealToQuestActorAxes() {
    constexpr float meters = 1.0f / 52.5f;
    return {{{0.0f, meters, 0.0f, 0.0f, 0.0f, meters, -meters, 0.0f, 0.0f}}};
}

struct ActorTransform {
    ActorMatrix3 linear, normalLinear;
    ActorVec3 translation;
    // Reverse the triangle's second/third corner when preserving source outward
    // normals across a reflection; base UE->Quest conversion is itself mirrored.
    bool mirrored{};

    ActorVec3 TransformPoint(const ActorVec3& point) const {
        const ActorVec3 p = linear.Transform(point);
        return {p.x+translation.x, p.y+translation.y, p.z+translation.z};
    }
    ActorVec3 TransformNormal(const ActorVec3& normal) const {
        return NormalizeActorVector(normalLinear.Transform(normal));
    }
};

inline ActorTransform BuildActorToQuest(
    const ActorVec3& location, const ActorVec3& prePivot,
    const std::int32_t pitch, const std::int32_t yaw, const std::int32_t roll,
    const float drawScale, const ActorVec3& drawScaleXYZ,
    const ActorVec3& verifiedOrigin) {
    if (!IsFiniteActorVector(location) || !IsFiniteActorVector(prePivot) ||
        !IsFiniteActorVector(verifiedOrigin) || !std::isfinite(drawScale))
        throw std::runtime_error("Actor placement is non-finite");
    // Mesh vertices already contain mesh Scale/Origin/RotOrigin. Mesh PrePivot
    // is added in world axes, as VisibleMesh.cpp does (not brush semantics).
    ActorTransform result;
    const ActorMatrix3 axes = UnrealToQuestActorAxes();
    result.linear = axes * UnrealActorRotation(pitch, yaw, roll) *
        ActorScaleMatrix({drawScale*drawScaleXYZ.x, drawScale*drawScaleXYZ.y,
                          drawScale*drawScaleXYZ.z});
    result.normalLinear = result.linear.NormalMatrix();
    result.translation = axes.Transform({location.x+prePivot.x-verifiedOrigin.x,
                                        location.y+prePivot.y-verifiedOrigin.y,
                                        location.z+prePivot.z-verifiedOrigin.z});
    result.translation.y += 1.0f;
    result.mirrored = result.linear.Determinant() < 0.0f;
    return result;
}

// Movers use MainScale and *subtractive local* PrePivot, unlike mesh actors.
// The pinned renderer does not implement MainScale shear; only Scale is used.
inline ActorTransform BuildBrushToQuest(
    const ActorVec3& location, const ActorVec3& prePivot,
    const std::int32_t pitch, const std::int32_t yaw, const std::int32_t roll,
    const ActorVec3& mainScale, const ActorVec3& verifiedOrigin) {
    if (!IsFiniteActorVector(location) || !IsFiniteActorVector(prePivot) ||
        !IsFiniteActorVector(verifiedOrigin))
        throw std::runtime_error("Brush placement is non-finite");
    ActorTransform result;
    const ActorMatrix3 axes = UnrealToQuestActorAxes();
    result.linear = axes * UnrealActorRotation(pitch, yaw, roll) * ActorScaleMatrix(mainScale);
    result.normalLinear = result.linear.NormalMatrix();
    const ActorVec3 pivot = result.linear.Transform(prePivot);
    result.translation = axes.Transform({location.x-verifiedOrigin.x,
                                        location.y-verifiedOrigin.y,
                                        location.z-verifiedOrigin.z});
    result.translation.x -= pivot.x;
    result.translation.y += 1.0f - pivot.y;
    result.translation.z -= pivot.z;
    result.mirrored = result.linear.Determinant() < 0.0f;
    return result;
}

} // namespace QuestVr
