#include "quest_map_lighting.h"
#include "Math/hsb.h" // Differential oracle from the pinned engine, not a test copy.

#include <iostream>
#include <limits>
#include <random>
#include <stdexcept>

namespace {
using Vector = OVR::Vector3f;
using Light = QuestVr::MapLight;

void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void Near(float actual, float expected, const char* message, float tolerance = 0.00001f) {
    Require(std::isfinite(actual) && std::fabs(actual - expected) <= tolerance, message);
}

void Near(const Vector& actual, const Vector& expected, const char* message,
          float tolerance = 0.00001f) {
    Near(actual.x, expected.x, message, tolerance);
    Near(actual.y, expected.y, message, tolerance);
    Near(actual.z, expected.z, message, tolerance);
}

Light Point(const Vector& position = {0,5,0}, const Vector& color = {1,1,1}) {
    Light light;
    light.localPosition = position;
    light.color = color;
    light.radiusMeters = 10.0f;
    light.intensity = 1.0f;
    return light;
}

struct Actor {
    std::string classPath;
    float x{}, y{}, z{};
    bool hasLocation{}, light{};
    std::uint8_t lightType{1}, lightEffect{};
    std::uint8_t lightBrightness{64}, lightRadius{64}, lightHue{}, lightSaturation{255};
    std::uint8_t lightCone{128};
    std::int32_t yaw{}, pitch{};
};

void AmbientAndConfiguration() {
    Near(QuestVr::CalculateMapLighting({}, {}, {0,1,0}), {.075f,.075f,.075f},
         "empty light list changed default ambient");
    QuestVr::MapLightingConfig config;
    config.ambient = {.1f,.2f,.3f};
    config.minimumChannel = .15f;
    config.maximumChannel = .25f;
    Near(QuestVr::CalculateMapLighting({}, {}, {0,1,0}, config), {.15f,.2f,.25f},
         "explicit channel/ambient configuration was ignored");
    config.maximumChannel = .1f; // Invalid ordered clamp bounds.
    Near(QuestVr::CalculateMapLighting({}, {}, {}, config), {.075f,.075f,.075f},
         "invalid config violated ordered-clamp precondition");
    config = {};
    config.ambient.x = std::numeric_limits<float>::quiet_NaN();
    Near(QuestVr::CalculateMapLighting({}, {}, {}, config), {.075f,.075f,.075f},
         "non-finite config leaked NaN channels");
}

void ColoredPointAndFalloff() {
    const Light red = Point({0,5,0}, {1,0,0});
    Near(QuestVr::CalculateMapLighting({red}, {}, {0,1,0}), {.325f,.075f,.075f},
         "colored point illumination or half-radius falloff changed");
    Near(QuestVr::CalculateMapLighting({red}, {0,4,0}, {0,1,0}), {.885f,.075f,.075f},
         "near point falloff is not quadratic");
    Near(QuestVr::CalculateMapLighting({red}, {0,-5,0}, {0,1,0}), {.075f,.075f,.075f},
         "point light illuminated its radius boundary");
    Near(QuestVr::CalculateMapLighting({red}, {0,-6,0}, {0,1,0}), {.075f,.075f,.075f},
         "point light illuminated outside radius");
    Near(QuestVr::CalculateMapLighting({red}, {0,5,0}, {0,1,0}), {.075f,.075f,.075f},
         "coincident point behavior changed or divided by zero");
    Near(QuestVr::CalculateMapLighting({red}, {}, {0,-1,0}), {.085f,.075f,.075f},
         "backface diffuse floor changed");
    const Light blue = Point({0,5,0}, {0,0,1});
    Near(QuestVr::CalculateMapLighting({red,blue}, {}, {0,1,0}), {.325f,.075f,.325f},
         "independent authored colors collapsed into uniform lighting");
    const auto clamped = QuestVr::CalculateMapLighting(std::vector<Light>(100, red), {}, {0,1,0});
    Near(clamped, {1.35f,.075f,.075f}, "bright light sum did not clamp each channel");
}

void NormalDirection() {
    const Light light = Point();
    const auto facing = QuestVr::CalculateMapLighting({light}, {}, {0,1,0});
    const auto sideways = QuestVr::CalculateMapLighting({light}, {}, {1,0,0});
    const auto away = QuestVr::CalculateMapLighting({light}, {}, {0,-1,0});
    Near(facing, {.325f,.325f,.325f}, "surface facing light has wrong diffuse");
    Near(sideways, {.085f,.085f,.085f}, "surface perpendicular to light lost backface floor");
    Near(away, sideways, "surface facing away was brighter than perpendicular");
    QuestVr::MapLightingConfig unlitBackface;
    unlitBackface.minimumDiffuse = 0;
    Near(QuestVr::CalculateMapLighting({light}, {}, {0,-1,0}, unlitBackface),
         {.075f,.075f,.075f}, "configured zero diffuse floor was ignored");
}

void SpotlightConeRegression() {
    Light spot = Point({}, {1,1,1});
    spot.direction = {0,0,-1};
    spot.coneCosine = QuestVr::UnrealMapLightConeCosine(128);
    Near(spot.coneCosine, 127.0f/255.0f, "LightCone is still treated as degrees");
    Near(QuestVr::UnrealMapLightConeCosine(0), 1.0f, "zero UE1 cone was widened");
    Near(QuestVr::UnrealMapLightConeCosine(255), 0.0f, "maximum UE1 cone is not a hemisphere");
    Near(QuestVr::CalculateMapLighting({spot}, {0,0,-2}, {0,0,1}), {.715f,.715f,.715f},
         "spotlight beam axis lost point-like center illumination");
    constexpr float pi = 3.14159265358979323846f;
    const float outsideAngle = 70.0f*pi/180.0f;
    const Vector outside{std::sin(outsideAngle),0,-std::cos(outsideAngle)};
    Near(QuestVr::CalculateMapLighting({spot}, outside*2.0f, outside*-1.0f),
         {.075f,.075f,.075f}, "cone128 still illuminates prior incorrect 70-degree region");
    const float insideAngle = 45.0f*pi/180.0f;
    const Vector inside{std::sin(insideAngle),0,-std::cos(insideAngle)};
    const float attenuation = (std::cos(insideAngle)-spot.coneCosine)/(1.0f-spot.coneCosine);
    const float expected = .075f+.64f*attenuation;
    Near(QuestVr::CalculateMapLighting({spot}, inside*2.0f, inside*-1.0f),
         {expected,expected,expected}, "spotlight angular fade changed");
    Near(QuestVr::CalculateMapLighting({spot}, {0,0,2}, {0,0,-1}), {.075f,.075f,.075f},
         "spotlight shines behind its authored direction");
    spot.coneCosine = 1.0f;
    Near(QuestVr::CalculateMapLighting({spot}, {0,0,-2}, {0,0,1}), {.075f,.075f,.075f},
         "empty cone produced NaN or visible illumination");
}

void ExplicitDirectionalLight() {
    Light sun;
    sun.color = {.5f,.25f,1};
    sun.direction = {0,-5,0}; // Normalized by evaluator for explicit directionals.
    sun.intensity = .5f;
    sun.directional = true;
    const auto near = QuestVr::CalculateMapLighting({sun}, {}, {0,1,0});
    Near(near, {.325f,.2f,.575f}, "directional color/direction is wrong");
    Near(QuestVr::CalculateMapLighting({sun}, {1e5f,-1e5f,1e5f}, {0,1,0}), near,
         "directional light incorrectly uses position or radial falloff");
    Near(QuestVr::CalculateMapLighting({sun}, {}, {0,-1,0}), {.085f,.08f,.095f},
         "directional backface response changed");
}

void HueAndSaturationCompatibility() {
    Near(QuestVr::UnrealMapLightColor(0,0), {1,0,0}, "hue zero is not red");
    Near(QuestVr::UnrealMapLightColor(85,0), {0,1,0}, "UE1 hue85 is not green");
    Near(QuestVr::UnrealMapLightColor(170,0), {0,0,1}, "UE1 hue170 is not blue");
    Near(QuestVr::UnrealMapLightColor(128,0), {0,42.0f/85.0f,43.0f/85.0f},
         "UE1 hue midpoint still uses six-sector HSV");
    for (int hue = 0; hue < 256; ++hue) {
        Near(QuestVr::UnrealMapLightColor(static_cast<std::uint8_t>(hue),255), {1,1,1},
             "Unreal saturation255 is not white");
        const auto color = QuestVr::UnrealMapLightColor(static_cast<std::uint8_t>(hue),0);
        Require(color.x >= 0 && color.x <= 1.000001f && color.y >= 0 &&
                color.y <= 1.000001f && color.z >= 0 && color.z <= 1.000001f,
                "hue conversion escaped normalized channels");
    }
    const float saturation128 = (128.0f/2.5f+2.0f)/104.0f;
    Near(QuestVr::UnrealMapLightColor(0,128), {1,saturation128,saturation128},
         "inverse Unreal saturation compatibility changed");
    // Preserve the pinned conversion's end-byte sector behavior, including its
    // discontinuity at exactly hue255; conventional HSV wrapping differs.
    const auto hue255 = hsbtorgb(255,0,255);
    const float white = hsbtorgb(0,255,255).x;
    Near(QuestVr::UnrealMapLightColor(255,0),
         {hue255.x/white,hue255.y/white,hue255.z/white},
         "last hue byte does not match pinned HSB sector behavior");
    Near(QuestVr::UnrealMapHsbColor(0,255,64), Vector(52.101880f/255.0f),
         "pinned raw HSB brightness64 changed");
}

void PinnedHsbDifferentialFixtures() {
    // Compile the actual pinned hsb.cpp table and hsb.h implementation as our
    // oracle. The portable sqrt formula differs only by table rounding, not by
    // hue sectors, inverse saturation, brightness curve, or white thresholds.
    const auto check = [](int hue, int saturation, int brightness) {
        const auto h = static_cast<std::uint8_t>(hue);
        const auto s = static_cast<std::uint8_t>(saturation);
        const auto b = static_cast<std::uint8_t>(brightness);
        const auto original = hsbtorgb(h,s,b);
        Near(QuestVr::UnrealMapHsbColor(h,s,b),
             {original.x,original.y,original.z},
             "shared raw HSB diverges from pinned original conversion", .0000003f);
    };
    for (int hue = 0; hue < 256; ++hue) {
        for (int saturation = 0; saturation < 256; ++saturation) {
            for (int brightness : {1,64,128,255}) check(hue,saturation,brightness);
            const auto original = hsbtorgb(static_cast<std::uint8_t>(hue),
                static_cast<std::uint8_t>(saturation),255);
            const float white = hsbtorgb(0,255,255).x;
            Near(QuestVr::UnrealMapLightColor(static_cast<std::uint8_t>(hue),
                     static_cast<std::uint8_t>(saturation)),
                 {original.x/white,original.y/white,original.z/white},
                 "normalized authored colors differ from pinned HSB", .000001f);
        }
    }
    for (int brightness = 0; brightness < 256; ++brightness) {
        for (int hue : {0,84,85,127,128,169,170,254,255})
            for (int saturation : {0,80,81,249,250,255}) check(hue,saturation,brightness);
        Near(QuestVr::UnrealMapHsbColor(0,255,static_cast<std::uint8_t>(brightness)).x,
             hsbtorgb_v_table[brightness]/255.0f,
             "raw HSB brightness differs from pinned table", .0000003f);
    }
}

void AuthoredLightTypesAndEffects() {
    Actor light;
    light.classPath = "Engine.Light";
    light.hasLocation = light.light = true;
    light.x = -1149.244f; light.y = 825.844f; light.z = -65.103f;
    Actor disabled = light; disabled.lightType = 0;
    Actor staticSpot = light; staticSpot.lightEffect = 8;
    Actor spot = light; spot.lightEffect = 12;
    Actor classNameOnly = light; classNameOnly.classPath = "DeusEx.Spotlight";
    Actor pulse = light; pulse.lightType = 2;
    QuestVr::MapLightBuildStats stats;
    const auto lights = QuestVr::BuildMapLights(
        std::vector<Actor>{disabled,staticSpot,spot,classNameOnly,pulse}, &stats);
    Require(lights.size() == 4 && stats.total == 4 && stats.spotlights == 2,
            "LightType/effect filtering or spotlight classification is wrong");
    Require(stats.lightTypeCounts[0] == 0u && stats.lightTypeCounts[1] == 3u &&
            stats.lightTypeCounts[2] == 1u && stats.lightEffectCounts[0] == 2u &&
            stats.lightEffectCounts[8] == 1u && stats.lightEffectCounts[12] == 1u,
            "authored light type/effect tallies disagree with emitted lights");
    Near(lights[0].coneCosine,127.0f/255.0f,"StaticSpot effect lost authored cone");
    Near(lights[1].coneCosine,127.0f/255.0f,"Spotlight effect lost authored cone");
    Near(lights[2].coneCosine,-1,"Spotlight class overrode authored LE_None");
    Near(lights[3].coneCosine,-1,"dynamic light type invented a cone");
    Near(lights[3].intensity,1,"static approximation no longer retains pulse base intensity");
    // A disabled spot may have nonzero radius/brightness in its defaults.
    disabled.lightEffect = 12;
    Require(QuestVr::BuildMapLights(std::vector<Actor>{disabled}).empty(),
            "LT_None emits despite nonzero authored brightness/radius/effect");
}

void SnapshotCoordinatesAndFilters() {
    Actor start;
    start.classPath = "Engine.PlayerStart";
    start.hasLocation = true;
    start.x = 100; start.y = 200; start.z = 300;
    Actor light;
    light.classPath = "Engine.Light";
    light.hasLocation = light.light = true;
    light.x = 152.5f; light.y = 305; light.z = 457.5f;
    Actor spot = light;
    spot.classPath = "DeusEx.Spotlight";
    spot.lightEffect = 8;
    spot.yaw = 16384;
    spot.lightHue = 0;
    spot.lightSaturation = 0;
    Actor disabled = light; disabled.lightBrightness = 0;
    Actor unlocated = light; unlocated.hasLocation = false;
    Actor notLight = light; notLight.light = false;
    Actor invalid = light; invalid.x = std::numeric_limits<float>::quiet_NaN();
    QuestVr::MapLightBuildStats stats;
    const auto lights = QuestVr::BuildMapLights(
        std::vector<Actor>{start,light,spot,disabled,unlocated,notLight,invalid}, &stats);
    Require(lights.size() == 2 && stats.total == 2 && stats.spotlights == 1 &&
            stats.colored == 1 && stats.invalidLocations == 1,
            "snapshot filtering/statistics changed");
    Near(stats.unrealOrigin, {100,200,300}, "first PlayerStart origin was not selected");
    Near(lights[0].localPosition, {2,4,-1}, "Unreal axes/units/head-height conversion changed");
    Near(lights[1].direction, {1,0,0}, "Unreal quarter-turn yaw points wrong way");
    Near(lights[0].radiusMeters, 65.0f*25.0f/52.5f, "authored radius omitted its extra Unreal unit");
    Near(lights[0].intensity, 1, "brightness64 no longer maps to unit intensity");
    Require(!lights[0].directional && !lights[1].directional,
            "snapshot converter invented a directional authored light");
    Near(QuestVr::UnrealMapLightDirection(0,16384), {0,1,0}, "authored pitch points wrong way");
    Near(QuestVr::UnrealMapLightDirection(0,0), {0,0,-1}, "zero rotation does not face -Z");
    Near(QuestVr::UnrealMapLightDirection(65536,0), {0,0,-1}, "full UE1 turn does not wrap");
    Actor fallback = light;
    fallback.x = -1149.244f; fallback.y = 825.844f; fallback.z = -65.103f;
    const auto fallbackLights = QuestVr::BuildMapLights(std::vector<Actor>{fallback});
    Near(fallbackLights[0].localPosition, {0,1,0}, "fallback origin compatibility changed");
    light.lightBrightness = 1; light.lightRadius = 1;
    spot.lightBrightness = 255;
    const auto limits = QuestVr::BuildMapLights(std::vector<Actor>{start,light,spot});
    Near(limits[0].intensity,.05f,"minimum intensity cap changed");
    Near(limits[0].radiusMeters,2.0f*25.0f/52.5f,"radius1 retained an invented one-meter cap");
    Near(limits[1].intensity,255.0f/64.0f,"maximum byte brightness mapping changed");
}

void AuthoredRadiusAndNonLightEmitters() {
    Actor emitter;
    emitter.classPath = "DeusEx.SomeLuminousDecoration";
    // Production snapshots classify any actor with an inherited non-LT_None
    // light type as light=true. BuildMapLights must not reinstate a class-name
    // restriction and drop this decoration's emitted light.
    emitter.hasLocation = emitter.light = true;
    emitter.x = -1149.244f; emitter.y = 825.844f; emitter.z = -65.103f;
    emitter.lightRadius = 0;
    QuestVr::MapLightBuildStats stats;
    const auto lights = QuestVr::BuildMapLights(std::vector<Actor>{emitter}, &stats);
    Require(lights.size() == 1u && stats.total == 1u && stats.lightTypeCounts[1] == 1u,
            "radius-zero or non-Light-class authored emitter was omitted");
    Near(lights[0].radiusMeters,25.0f/52.5f,
         "radius-zero light does not match pinned UActor::WorldLightRadius");
    const auto lit = QuestVr::CalculateMapLighting(lights, {0,.8f,0}, {0,1,0});
    Require(lit.x > .075f && lit.y > .075f && lit.z > .075f,
            "radius-zero light still behaves like a disabled light");
    Near(QuestVr::CalculateMapLighting(lights,{0,.5f,0},{0,1,0}), {.075f,.075f,.075f},
         "radius-zero emitter extends past its authored 25-Unreal-unit radius");
    // Audit every representable byte, including the former zero rejection and
    // minimum-radius cap. The expected expression is UActor.h WorldLightRadius.
    for (int radius = 0; radius <= 255; ++radius) {
        emitter.lightRadius = static_cast<std::uint8_t>(radius);
        const auto decoded = QuestVr::BuildMapLights(std::vector<Actor>{emitter});
        Require(decoded.size() == 1u,"valid radius byte dropped an authored light");
        Near(decoded[0].radiusMeters, static_cast<float>((radius+1)*25)/52.5f,
             "authored radius byte disagrees with pinned engine radius", .00002f);
    }
    emitter.lightType = 0;
    Require(QuestVr::BuildMapLights(std::vector<Actor>{emitter}).empty(),
            "non-Light actor with LT_None still emits");
    emitter.lightType = 1;
    emitter.lightBrightness = 0;
    Require(QuestVr::BuildMapLights(std::vector<Actor>{emitter}).empty(),
            "non-Light actor with zero brightness still emits");
}

void InvalidLightingInputs() {
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float infinity = std::numeric_limits<float>::infinity();
    const Vector ambient{.075f,.075f,.075f};
    for (int failure = 0; failure < 6; ++failure) {
        Light light = Point();
        if (failure == 0) light.localPosition.x = nan;
        if (failure == 1) light.radiusMeters = -1;
        if (failure == 2) light.radiusMeters = infinity;
        if (failure == 3) light.color.z = nan;
        if (failure == 4) light.intensity = nan;
        if (failure == 5) light.coneCosine = nan;
        Near(QuestVr::CalculateMapLighting({light}, {}, {0,1,0}), ambient,
             "invalid light leaked NaN or illumination");
    }
    Near(QuestVr::CalculateMapLighting({Point()}, {nan,0,0}, {0,1,0}), ambient,
         "invalid sample position leaked NaN");
    Near(QuestVr::CalculateMapLighting({Point()}, {}, {0,infinity,0}), ambient,
         "invalid surface normal leaked NaN");
    Light directional = Point();
    directional.directional = true;
    directional.direction = {infinity,0,0};
    Near(QuestVr::CalculateMapLighting({directional}, {}, {0,1,0}), ambient,
         "invalid directional input was normalized");
}

// Copied formula from the formerly inline native sampler. This differential
// fixture proves extraction preserved valid point/spot evaluation, independent
// of the separately corrected authored LightCone-to-cosine conversion.
Vector LegacyCalculate(const std::vector<Light>& lights, const Vector& position,
                       const Vector& normal) {
    Vector result{.075f,.075f,.075f};
    for (const auto& light : lights) {
        const Vector offset = light.localPosition-position;
        const float squared = offset.LengthSq();
        if (squared <= .000001f || squared >= light.radiusMeters*light.radiusMeters) continue;
        const float distance = std::sqrt(squared);
        const Vector direction = offset*(1.0f/distance);
        float cone = 1;
        if (light.coneCosine >= 0) {
            const float alignment = (direction*-1.0f).Dot(light.direction);
            if (alignment <= light.coneCosine) continue;
            cone = std::clamp((alignment-light.coneCosine)/(1-light.coneCosine),0.0f,1.0f);
        }
        const float fade = 1-distance/light.radiusMeters;
        const float contribution = light.intensity*fade*fade*
            std::max(.04f,normal.Dot(direction))*cone;
        result.x += light.color.x*contribution;
        result.y += light.color.y*contribution;
        result.z += light.color.z*contribution;
    }
    return {std::clamp(result.x,.04f,1.35f), std::clamp(result.y,.04f,1.35f),
            std::clamp(result.z,.04f,1.35f)};
}

void LegacyDifferentialFixtures() {
    std::mt19937 random(0x44585133u);
    std::uniform_real_distribution<float> value(-20,20);
    std::vector<Light> lights;
    for (int index = 0; index < 32; ++index) {
        Light light = Point({value(random),value(random),value(random)},
            QuestVr::UnrealMapLightColor(static_cast<std::uint8_t>(random()%256),
                static_cast<std::uint8_t>(random()%256)));
        light.radiusMeters = 1+std::fabs(value(random));
        light.intensity = .05f+static_cast<float>(random()%255)/64.0f;
        if (index%2 == 0) {
            light.direction = QuestVr::UnrealMapLightDirection(
                static_cast<std::int32_t>(random()%65536),
                static_cast<std::int32_t>(random()%65536));
            light.coneCosine = .01f+static_cast<float>(random()%90)/100.0f;
        }
        lights.push_back(light);
    }
    for (int index = 0; index < 4096; ++index) {
        const Vector position{value(random),value(random),value(random)};
        Vector normal{value(random),value(random),value(random)};
        normal.Normalize();
        Near(QuestVr::CalculateMapLighting(lights,position,normal),
             LegacyCalculate(lights,position,normal),
             "shared sampler changed previously valid native light evaluation", .000001f);
    }
}

void VerifiedOriginOverridesSnapshotOrder() {
    Actor firstStart;
    firstStart.classPath = "Engine.PlayerStart"; firstStart.hasLocation = true;
    firstStart.x = -900.0f; firstStart.y = 700.0f; firstStart.z = 800.0f;
    Actor emitter;
    emitter.classPath = "Engine.Light"; emitter.hasLocation = emitter.light = true;
    emitter.x = 152.5f; emitter.y = 252.5f; emitter.z = 352.5f;
    const Vector verified{100,200,300};
    QuestVr::MapLightBuildStats stats;
    const auto lights = QuestVr::BuildMapLights(std::vector<Actor>{firstStart,emitter},&stats,&verified);
    Require(lights.size() == 1u,"Verified-origin light fixture lost emitter");
    Near(stats.unrealOrigin,verified,"Snapshot-first PlayerStart overrode verified serialized origin");
    Near(lights[0].localPosition,{1,2,-1},"Emitter used a different axis/origin than world cache");
    Vector invalid = verified; invalid.x = std::numeric_limits<float>::infinity();
    bool rejected = false;
    try { (void)QuestVr::BuildMapLights(std::vector<Actor>{emitter},nullptr,&invalid); }
    catch (const std::invalid_argument&) { rejected = true; }
    Require(rejected,"Non-finite explicit map origin accepted");
}
} // namespace

int main() {
    try {
        AmbientAndConfiguration();
        ColoredPointAndFalloff();
        NormalDirection();
        SpotlightConeRegression();
        ExplicitDirectionalLight();
        HueAndSaturationCompatibility();
        PinnedHsbDifferentialFixtures();
        AuthoredLightTypesAndEffects();
        SnapshotCoordinatesAndFilters();
        AuthoredRadiusAndNonLightEmitters();
        InvalidLightingInputs();
        LegacyDifferentialFixtures();
        VerifiedOriginOverridesSnapshotOrder();
        std::cout << "Quest map lighting: 13 regression groups passed "
            "(341760 pinned HSB checks, 256 authored-radius checks, 4096 legacy comparisons)\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Quest map lighting failed: " << error.what() << '\n';
        return 1;
    }
}
