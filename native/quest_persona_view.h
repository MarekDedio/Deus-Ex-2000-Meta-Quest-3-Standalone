#pragma once

#include <type_traits>
#include <cmath>

namespace QuestVr {
// All original Persona pages share one comfortable lowered attachment.
// Keep the artwork, fallback text and controller-ray hit plane in agreement.
inline constexpr float kPersonaPanelCenterY=-0.16f;
inline constexpr float kPersonaArtworkDistance=1.055f;
inline constexpr float kPersonaTextDistance=1.05f;
inline constexpr float kPersonaPanelWidth=1.20f;
inline constexpr float kPersonaPanelHeight=0.90f;
inline constexpr float kPersonaPanelHalfWidth=kPersonaPanelWidth*0.5f;
inline constexpr float kPersonaPanelHalfHeight=kPersonaPanelHeight*0.5f;
template<class Pose>
Pose PersonaPanelPose(const Pose& head,float distance=kPersonaArtworkDistance) {
    auto pose=head;
    using Vector=std::remove_cvref_t<decltype(pose.Translation)>;
    pose.Translation+=pose.Rotation.Rotate(Vector{0.0f,kPersonaPanelCenterY,-distance});
    // A translated but parallel quad foreshortens its lower edge. Face the
    // eye/head centre so top and bottom edges are equally far away, instead of
    // making users look down at an apparently tilted screen.
    using Rotation=std::remove_cvref_t<decltype(pose.Rotation)>;
    const float pitch=std::atan2(kPersonaPanelCenterY,distance);
    pose.Rotation=pose.Rotation*Rotation{std::sin(pitch*0.5f),0.0f,0.0f,std::cos(pitch*0.5f)};
    return pose;
}
} // namespace QuestVr
