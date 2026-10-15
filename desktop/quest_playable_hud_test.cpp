#include "quest_playable_hud.h"

#include <array>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
using namespace QuestVr;
std::size_t checks{};
void Require(bool value,const char* message) {
    ++checks;
    if (!value) throw std::runtime_error(message);
}
template<class Action> void Reject(Action&& action,const char* message) {
    bool refused{};
    try { action(); } catch (const std::runtime_error&) { refused=true; }
    Require(refused,message);
}
PersonaUiImage Solid(std::uint32_t width,std::uint32_t height,
    std::array<std::uint8_t,4> color) {
    auto image=EmptyHudCanvas(width,height);
    for (std::size_t pixel=0u;pixel<image.rgba.size();pixel+=4u)
        std::copy(color.begin(),color.end(),image.rgba.begin()+pixel);
    return image;
}
void Health() {
    for (const auto& [input,expected]:std::array<std::pair<float,std::int32_t>,9>{{
        {-std::numeric_limits<float>::max(),0},{-1.0f,0},{-0.0f,0},{0.0f,0},
        {0.99f,0},{54.75f,54},{99.99f,99},{100.0f,100},
        {std::numeric_limits<float>::max(),100}}}) {
        const auto snapshot=BuildPlayableHealthSnapshot(input);
        ValidateHudHealthSnapshot(snapshot);
        for (const auto& part:snapshot.parts) {
            Require(part.displayedHealth==expected,"Unfinished body segments do not reflect clamped aggregate health");
            Require(part.damageFlashFraction==0.0f,"Aggregate preview fabricated an independent damage flash");
        }
        Require(snapshot.energy==100.0f && snapshot.energyMax==100.0f,
            "Unfinished energy display is not explicitly full-scale");
        Require(!snapshot.underwater && !snapshot.breathPercent && snapshot.swimTimer==0.0f,
            "Aggregate preview fabricated underwater/oxygen state");
    }
    Reject([] {(void)BuildPlayableHealthSnapshot(std::numeric_limits<float>::quiet_NaN());},
        "NaN aggregate health was accepted");
    Reject([] {(void)BuildPlayableHealthSnapshot(std::numeric_limits<float>::infinity());},
        "Infinite aggregate health was accepted");
    Reject([] {(void)BuildPlayableHealthSnapshot(-std::numeric_limits<float>::infinity());},
        "Negative infinite aggregate health was accepted");

    // Generated artwork checks publication, not fidelity to the owned PC assets.
    HudHealthArtwork art{Solid(60u,76u,{20u,20u,20u,255u}),
        Solid(84u,106u,{40u,40u,40u,255u}),Solid(34u,68u,{5u,5u,5u,255u}),{}};
    for (std::size_t part=0u;part<art.parts.size();++part)
        art.parts[part]=Solid(kHudHealthPartRects[part].width,
            kHudHealthPartRects[part].height,{255u,255u,255u,255u});
    const auto full=BuildHudHealthCanvas(art,BuildPlayableHealthSnapshot(100.0f));
    const auto repeated=BuildHudHealthCanvas(art,BuildPlayableHealthSnapshot(100.0f));
    const auto dead=BuildHudHealthCanvas(art,BuildPlayableHealthSnapshot(0.0f));
    Require(full.width==kHudHealthWidth && full.height==kHudHealthHeight,
        "Playable health changed original artwork dimensions");
    Require(full.rgba==repeated.rgba,"Unchanged preview does not permit pixel upload caching");
    Require(full.rgba!=dead.rgba,"Aggregate health does not affect the visible body colors");
    const auto anyVisible=[](const PersonaUiImage& image) {
        for (std::size_t alpha=3u;alpha<image.rgba.size();alpha+=4u)
            if (image.rgba[alpha]!=0u) return true;
        return false;
    };
    Require(anyVisible(full) && anyVisible(dead),"Health panel disappeared at full health or death");
}
void Placement() {
    constexpr double radiansToDegrees=57.2957795130823208768;
    const auto angle=[](float extent) {
        return std::atan2(std::fabs(static_cast<double>(extent)),
            -static_cast<double>(kPlayableBeltCenterZ))*radiansToDegrees;
    };
    Require(kPlayableBeltCenterX==0.0f && kPlayableBeltCenterY<0.0f && kPlayableBeltCenterZ<0.0f,
        "Quickbar is not centered below the forward view");
    Require(kPlayableBeltCenterZ==-kHudVrDistance,
        "Quickbar depth disagrees with the accepted HUD plane");
    Require(kPlayableBeltHalfWidth>0.0f && kPlayableBeltHalfHeight>0.0f,
        "Quickbar has invalid dimensions");
    Require(angle(kPlayableBeltCenterX-kPlayableBeltHalfWidth)<27.0 &&
        angle(kPlayableBeltCenterX+kPlayableBeltHalfWidth)<27.0,
        "Quickbar horizontal edges exceed the central-view comfort bounds");
    Require(angle(kPlayableBeltCenterY-kPlayableBeltHalfHeight)<40.0,
        "Further-lowered quickbar bottom exceeds forty degrees below the eye line");
    Require(angle(kPlayableBeltCenterY+kPlayableBeltHalfHeight)>35.0 &&
        angle(kPlayableBeltCenterY+kPlayableBeltHalfHeight)<36.0,
        "Quickbar top does not clear the central thirty-five degrees");
    Require(angle(kPlayableBeltCenterY)>37.5 && angle(kPlayableBeltCenterY)<38.5,
        "Quickbar center did not move substantially below the rejected twenty-seven-degree position");
    Require(kPlayableBeltCenterY+kPlayableBeltHalfHeight<0.0f,
        "Quickbar crosses the forward aim point");
    Require(std::fabs(kPlayableBeltHalfWidth/kPlayableBeltHalfHeight-
        static_cast<float>(kHudBeltWidth)/static_cast<float>(kHudBeltHeight))<0.00001f,
        "Quickbar artwork aspect ratio changed");
    Require(kHudVrPixelsToMeters==0.004f && kPlayableBeltPixelsToMeters==0.003f &&
        kPlayableBeltCenterY==-1.28f && kHudCompassCenterY==0.07f,
        "Requested lower HUD placement or unchanged artwork scale regressed");
    Require(std::fabs(kHudHealthCenterX-(-0.6435f))<0.00001f &&
        std::fabs(kHudHealthCenterY-0.378f)<0.00001f &&
        std::fabs(kHudCompassCenterX-(-0.6655f))<0.00001f,
        "Bar-only correction moved the health/compass column");
    const auto beltLeft=kPlayableBeltCenterX-kPlayableBeltHalfWidth;
    Require(std::fabs(kHudHealthCenterX-kHudHealthHalfWidth-beltLeft)<0.00001f &&
        std::fabs(kHudCompassCenterX-kHudCompassHalfWidth-beltLeft)<0.00001f,
        "Health, compass and quickbar no longer share their left edge");
    Require(std::fabs(kHudCompassCenterX-(-0.50f)-(-0.1655f))<0.00001f &&
        std::fabs(kHudCompassCenterY-0.25f-(-0.18f))<0.00001f,
        "Column did not move slightly left and down from the accepted baseline");
    Require(kHudHealthCenterY+kHudHealthHalfHeight<0.60f &&
        kHudHealthCenterY-kHudHealthHalfHeight>kHudCompassCenterY+kHudCompassHalfHeight,
        "Lowered health is too high or overlaps the compass");
    Require(kHudCompassCenterY-kHudCompassHalfHeight>
        kPlayableBeltCenterY+kPlayableBeltHalfHeight,
        "Quickbar overlaps the health/compass column");
    // Both eyes retain the aligned edges on one plane, even at wide IPD.
    // This is a geometric regression bound, not headset comfort acceptance.
    for(int ipdMm=52;ipdMm<=74;++ipdMm) for(const float side:{-1.0f,1.0f}) {
        const float eyeX=side*static_cast<float>(ipdMm)*0.0005f;
        const auto projected=[&](float x) {return (x-eyeX)/kHudVrDistance;};
        Require(std::fabs(projected(kHudHealthCenterX-kHudHealthHalfWidth)-
            projected(beltLeft))<0.00001f &&
            std::fabs(projected(kHudCompassCenterX-kHudCompassHalfWidth)-projected(beltLeft))<0.00001f,
            "Binocular projection broke the shared left edge");
        Require(angle(beltLeft-eyeX)<28.0 &&
            angle(kPlayableBeltCenterX+kPlayableBeltHalfWidth-eyeX)<28.0,
            "Lowered HUD horizontal edges exceed binocular inset bounds");
    }
}
} // namespace

int main() {
    try {
        Health();Placement();
        std::cout << "PASS playable HUD presentation: " << checks
            << " checks; explicit unfinished aggregate body, finite clamps, stable visible pixels and comfortable quickbar geometry\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL playable HUD presentation: " << error.what() << '\n';
        return 1;
    }
}
