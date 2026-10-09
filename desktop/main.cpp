#include "visual_renderer.h"
#include "surreal_gc_probe.h"
#include "surreal_portable_package_tables.h"
#include "persona_preview.h"
#include "persona_ui_canvas.h"
#include "authored_lighting_preview.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>

extern "C" bool BuildQuestMapCacheToDirectory(
    const char* gameRoot, const char* mapName, const char* outputRoot);

namespace {
using namespace questvisual;
struct Options {
    bool selfTest{};
    bool personaPreview{};
    bool authoredLighting{};
    bool bakedLighting{};
    bool actors{};
    std::filesystem::path gameRoot, cacheRoot, mesh, materials, output, report, baseline;
    std::string map;
    std::string isolatedActor;
    ActorPosePreviewOptions posePreview;
    Camera camera;
    std::uint32_t width{1280}, height{720};
    double maxMeanError{0.0};
    std::optional<double> minimumCoverage;
    std::vector<std::string> personaIcons;
    std::size_t personaSelected{};
    QuestVr::PersonaUiPage personaPage{QuestVr::PersonaUiPage::Inventory};
};
std::string CanonicalPathKey(const std::filesystem::path& path) {
    std::string result = std::filesystem::weakly_canonical(std::filesystem::absolute(path)).generic_string();
#if defined(_WIN32)
    std::transform(result.begin(),result.end(),result.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
#endif
    return result;
}
bool SameFile(const std::filesystem::path& a, const std::filesystem::path& b) {
    if (a.empty() || b.empty()) return false;
    std::error_code error;
    // equivalent also detects existing hard links; canonical names handle future paths.
    if (std::filesystem::equivalent(a,b,error) && !error) return true;
    return CanonicalPathKey(a) == CanonicalPathKey(b);
}
bool Within(const std::filesystem::path& child, const std::filesystem::path& root) {
    const auto childKey = CanonicalPathKey(child), rootKey = CanonicalPathKey(root);
    return childKey == rootKey || childKey.rfind(rootKey+"/",0) == 0u;
}
std::string Lower(std::string value) {
    std::transform(value.begin(),value.end(),value.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}
void InspectTextures(const std::filesystem::path& path, const std::string& filter) {
    const auto package = LoadPortablePackageTables(path.string());
    std::size_t matches{};
    for (std::size_t i = 0; i < package.exports.size(); ++i) {
        const auto name = GetPortableObjectPath(package,static_cast<std::int32_t>(i+1u));
        if (Lower(name).find(Lower(filter)) == std::string::npos) continue;
        ++matches;
        const auto& entry = package.exports[i];
        std::cout << name << " class=" << GetPortableObjectPath(package,entry.ObjClass)
                  << " bytes=" << entry.ObjSize << '\n';
        const auto properties = LoadPortableExportProperties(package,i);
        for (const auto& property : properties.properties) {
            std::cout << "  " << property.name.ToString() << " type=" << unsigned(property.type);
            if (property.type == 2u && property.value.size() == 4u) {
                std::int32_t value{};
                std::memcpy(&value,property.value.data(),4u);
                std::cout << " value=" << value;
            } else if (property.type == 5u || property.type == 8u) {
                std::cout << " object=" << GetPortableObjectPath(package,DecodePortableObjectReference(property));
            }
            std::cout << '\n';
        }
        std::ifstream file(path,std::ios::binary);
        file.seekg(static_cast<std::uint64_t>(entry.ObjOffset)+properties.bytesConsumed);
        const int head = file.get();
        std::cout << "  nativePayloadFirstByte=" << head << '\n';
    }
    std::cout << "Matching exports: " << matches << '\n';
}
void ProtectCaptureInputs(const Options& options) {
    if (SameFile(options.output,options.report))
        throw std::runtime_error("Capture output and report must be different files");
    const auto surfaces = options.mesh.empty() ? std::filesystem::path{} :
        std::filesystem::path(options.mesh.string()+".surfaces");
    for (const auto& input : {options.baseline,options.mesh,options.materials,surfaces}) {
        if (SameFile(options.output,input) || SameFile(options.report,input))
            throw std::runtime_error("Output/report must not alias baseline or source cache files");
    }
    if (SameFile(options.baseline,options.mesh) || SameFile(options.baseline,options.materials) || SameFile(options.baseline,surfaces))
        throw std::runtime_error("Baseline must not alias source/generated cache files");
    if (!options.gameRoot.empty() && (Within(options.output,options.gameRoot) ||
        Within(options.report,options.gameRoot) ||
        (!options.cacheRoot.empty() && Within(options.cacheRoot,options.gameRoot))))
        throw std::runtime_error("Generated captures, reports and caches must be outside GameRoot");
}
std::string Quote(const std::string& value) {
    std::string result{"\""};
    for (const unsigned char c : value) {
        if (c == '\\' || c == '"') { result += '\\'; result += static_cast<char>(c); }
        else if (c < 32) {
            std::ostringstream escape;
            escape << "\\u" << std::hex << std::setw(4) << std::setfill('0') << unsigned(c);
            result += escape.str();
        } else result += static_cast<char>(c);
    }
    return result+'"';
}
float Float(const std::string& value) {
    std::size_t consumed{};
    const float result = std::stof(value,&consumed);
    if (consumed != value.size() || !std::isfinite(result))
        throw std::runtime_error("Invalid finite number: " + value);
    return result;
}
std::uint32_t Dimension(const std::string& value) {
    std::size_t consumed{};
    const unsigned long result = std::stoul(value,&consumed);
    if (consumed != value.size() || result < 32u || result > 4096u)
        throw std::runtime_error("Capture dimensions must be 32..4096");
    return static_cast<std::uint32_t>(result);
}
void Help() {
    std::cout << "No-headset Quest world-cache visual capture\n"
        "  --self-test                       Synthetic pipeline verification only\n"
        "  --game-root PATH --map NAME        Decode original packages with Quest code\n"
        "  --cache-root PATH                  Write generated cache outside game installation\n"
        "  --mesh PATH --materials PATH       Render existing DXQM v2/DXQA v1 cache\n"
        "  --map NAME                         Optional provenance label for existing cache\n"
        "  --output PATH.bmp --report PATH.json\n"
        "  --camera X Y Z --yaw DEGREES --pitch DEGREES --fov DEGREES\n"
        "  --width PIXELS --height PIXELS --near METERS\n"
        "  --baseline PATH.bmp --max-mean-error FRACTION\n"
        "  --min-coverage FRACTION             Optional viewpoint-specific empty-frame gate\n"
        "  --authored-lighting (--lit-preview) Optional shared Quest direct vertex lighting\n"
        "  --baked-lighting          Original static BSP lightmaps/shadow masks and zone ambient\n"
        "  --actors                  Original actor meshes, skin overrides and mover brushes\n"
        "  --actor-isolate PATH      Close-up of one original actor, without world BSP\n"
        "  --actor-animation-sequence NAME  Explicit isolated pose fixture (not an idle heuristic)\n"
        "  --actor-animation-frame FRACTION Normalized isolated fixture frame in [0,1)\n"
        "  --actor-fatness BYTE      Isolated fixture fatness, neutral 128\n"
        "  --actor-script-function NAME  Execute one original compiled helper before isolated capture\n"
        "  --actor-script-name NAME / --actor-script-float N / --actor-script-object PATH\n"
        "                                    Append a typed Name, Float or original Object/class identity\n"
        "  --actor-script-use-result         Isolate a nonnull Actor Object returned by the helper/native\n"
        "                                    Requires original game/map; self-test uses fixture lights\n"
        "  --inspect-textures PACKAGE FILTER  List matching export/class/properties for diagnosis\n"
        "  --persona-preview --game-root PATH Original Persona artwork, fonts and fixture text\n"
        "  --persona-icon NAME                Repeat for each original icon asset (preview fixture)\n"
        "  --persona-selected INDEX           Select an icon in the fixture inventory grid\n"
        "  --persona-page PAGE                Inventory, Health, GoalsNotes or Logs\n"
        "Camera uses Quest-cache meters; default (0,1.65,0) looks -Z. Positive yaw turns right.\n"
        "Default captures show world BSP albedo. --baked-lighting includes static lightmaps.\n"
        "--actors samples authored poses; isolated helpers may change a pose, but no live ticking or actor shadowing.\n"
        "OpenXR, live UI, campaign scripts, and Quest GPU performance are not verified.\n";
}
Options Parse(int argc, char** argv) {
    Options options;
    for (int i = 1; i < argc; ++i) {
        const std::string argument = argv[i];
        const auto next = [&]() -> std::string {
            if (++i >= argc) throw std::runtime_error("Missing value for " + argument);
            return argv[i];
        };
        if (argument == "--self-test") options.selfTest = true;
        else if (argument == "--authored-lighting" || argument == "--lit-preview") options.authoredLighting = true;
        else if (argument == "--baked-lighting") { options.authoredLighting = true; options.bakedLighting = true; }
        else if (argument == "--actors") options.actors = true;
        else if (argument == "--actor-isolate") { options.actors = true; options.isolatedActor = next(); }
        else if (argument == "--actor-animation-sequence") options.posePreview.sequence = next();
        else if (argument == "--actor-animation-frame") {
            const auto frame = Float(next());
            if (frame < 0.0f || frame >= 1.0f) throw std::runtime_error("Pose fixture frame must be in [0,1)");
            options.posePreview.frame = frame;
        }
        else if (argument == "--actor-fatness") {
            const auto value = next();
            std::size_t consumed{};
            const auto number = std::stoul(value,&consumed);
            if (consumed != value.size() || number > 255u) throw std::runtime_error("Fatness must be an integer byte 0..255");
            options.posePreview.fatness = static_cast<std::uint8_t>(number);
        }
        else if (argument == "--actor-script-function") options.posePreview.scriptFunction = next();
        else if (argument == "--actor-script-name") options.posePreview.scriptArguments.push_back(
            {QuestVr::Vm::Value::Text(QuestVr::Vm::Kind::Name,next()),{}});
        else if (argument == "--actor-script-float") options.posePreview.scriptArguments.push_back(
            {QuestVr::Vm::Value::Float(Float(next())),{}});
        else if (argument == "--actor-script-object") {
            const auto path = next();
            if (path.size() > 8192u || std::any_of(path.begin(),path.end(),[](unsigned char character) {
                return character < 32u || character > 126u;
            })) throw std::runtime_error("Script Object argument must be a bounded printable original object identity");
            options.posePreview.scriptArguments.push_back({QuestVr::Vm::Value::Text(QuestVr::Vm::Kind::Object,path),{}});
        }
        else if (argument == "--actor-script-use-result") options.posePreview.scriptUseResultActor = true;
        else if (argument == "--persona-preview") options.personaPreview = true;
        else if (argument == "--persona-icon") options.personaIcons.push_back(next());
        else if (argument == "--persona-page") {
            const auto page = Lower(next());
            if (page == "inventory") options.personaPage = QuestVr::PersonaUiPage::Inventory;
            else if (page == "health") options.personaPage = QuestVr::PersonaUiPage::Health;
            else if (page == "goalsnotes") options.personaPage = QuestVr::PersonaUiPage::GoalsNotes;
            else if (page == "logs") options.personaPage = QuestVr::PersonaUiPage::Logs;
            else throw std::runtime_error("Unknown Persona page: " + page);
        }
        else if (argument == "--persona-selected") {
            const auto text = next();
            std::size_t consumed{};
            const auto value = std::stoul(text,&consumed);
            if (consumed != text.size() || value > 100000u)
                throw std::runtime_error("Invalid Persona selection index");
            options.personaSelected = value;
        }
        else if (argument == "--game-root") options.gameRoot = next();
        else if (argument == "--cache-root") options.cacheRoot = next();
        else if (argument == "--mesh") options.mesh = next();
        else if (argument == "--materials") options.materials = next();
        else if (argument == "--output") options.output = next();
        else if (argument == "--report") options.report = next();
        else if (argument == "--baseline") options.baseline = next();
        else if (argument == "--map") options.map = next();
        else if (argument == "--camera") {
            options.camera.position.x = Float(next());
            options.camera.position.y = Float(next());
            options.camera.position.z = Float(next());
        }
        else if (argument == "--yaw") options.camera.yawDegrees = Float(next());
        else if (argument == "--pitch") options.camera.pitchDegrees = Float(next());
        else if (argument == "--fov") options.camera.verticalFovDegrees = Float(next());
        else if (argument == "--near") options.camera.nearPlane = Float(next());
        else if (argument == "--width") options.width = Dimension(next());
        else if (argument == "--height") options.height = Dimension(next());
        else if (argument == "--max-mean-error") options.maxMeanError = Float(next());
        else if (argument == "--min-coverage") options.minimumCoverage = Float(next());
        else throw std::runtime_error("Unknown argument: " + argument);
    }
    if (options.output.empty()) throw std::runtime_error("--output PATH.bmp is required");
    if (options.output.extension() != ".bmp") throw std::runtime_error("Output must have .bmp extension");
    if (options.report.empty()) { options.report = options.output; options.report.replace_extension(".json"); }
    if (options.maxMeanError < 0.0 || options.maxMeanError > 1.0 ||
        (options.minimumCoverage && (*options.minimumCoverage < 0.0 || *options.minimumCoverage > 1.0)))
        throw std::runtime_error("Image error and coverage fractions must be 0..1");
    if ((options.posePreview.sequence || options.posePreview.frame || options.posePreview.fatness) && options.isolatedActor.empty())
        throw std::runtime_error("Explicit pose fixtures require --actor-isolate PATH");
    options.posePreview.actorPath = options.isolatedActor;
    if (!options.posePreview.scriptArguments.empty() && options.posePreview.scriptFunction.empty())
        throw std::runtime_error("Script arguments require --actor-script-function");
    if (options.posePreview.scriptUseResultActor && options.posePreview.scriptFunction.empty())
        throw std::runtime_error("--actor-script-use-result requires --actor-script-function");
    if (!options.posePreview.scriptFunction.empty() && (options.isolatedActor.empty() ||
        options.posePreview.sequence || options.posePreview.frame || options.posePreview.fatness))
        throw std::runtime_error("Compiled helper captures require --actor-isolate and cannot mix with manual pose overrides");
    if (options.posePreview.scriptArguments.size() > 16u)
        throw std::runtime_error("Isolated helper capture argument limit exceeded");
    const bool hasCache = !options.mesh.empty() || !options.materials.empty();
    const bool hasGame = !options.gameRoot.empty();
    if (options.personaPreview) {
        if (!hasGame || hasCache || options.selfTest || !options.cacheRoot.empty() || options.minimumCoverage || options.authoredLighting || options.actors)
            throw std::runtime_error("--persona-preview requires --game-root and cannot use map/cache/coverage modes");
        if (options.personaPage == QuestVr::PersonaUiPage::Inventory) {
            if (options.personaIcons.empty()) options.personaIcons = {"LargeIconPistol","LargeIconMedKit",
                "LargeIconMultitool","LargeIconLockPick","LargeIconBioCell","LargeIconRifle","LargeIconLAM"};
            if (options.personaSelected >= options.personaIcons.size())
                throw std::runtime_error("Persona selection is outside the icon fixture");
        } else if (!options.personaIcons.empty() || options.personaSelected != 0u) {
            throw std::runtime_error("Persona icon fixtures/selection apply only to Inventory");
        }
        return options;
    }
    if (!options.personaIcons.empty() || options.personaSelected != 0u ||
        options.personaPage != QuestVr::PersonaUiPage::Inventory)
        throw std::runtime_error("Persona icon options require --persona-preview");
    if (static_cast<int>(options.selfTest)+static_cast<int>(hasCache)+static_cast<int>(hasGame) != 1)
        throw std::runtime_error("Choose exactly one of --self-test, cache files, or --game-root/--map");
    if (hasCache && (options.mesh.empty() || options.materials.empty()))
        throw std::runtime_error("Both --mesh and --materials are required");
    if (hasGame && (options.gameRoot.empty() || options.map.empty()))
        throw std::runtime_error("Both --game-root and --map are required");
    if (options.authoredLighting && !hasGame && !options.selfTest)
        throw std::runtime_error("Authored lighting requires original --game-root/--map, not an unverified external cache");
    if (options.bakedLighting && !hasGame)
        throw std::runtime_error("Baked lighting requires original game packages, not synthetic fixtures");
    if (options.actors && !hasGame)
        throw std::runtime_error("--actors requires the original game packages, not a synthetic/external cache");
    if ((options.authoredLighting || options.actors) && hasGame && (options.map.size()>128u ||
        std::any_of(options.map.begin(),options.map.end(),[](unsigned char character) {
            return !std::isalnum(character) && character != '_' && character != '-';
        }))) throw std::runtime_error("Authored lighting requires a safe original map basename without extension");
    return options;
}
void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(std::string("Synthetic pipeline regression: ")+message);
}
Camera FrameIsolatedActor(Camera camera, const Vec3 minimum, const Vec3 maximum) {
    const Vec3 center{(minimum.x+maximum.x)*0.5f,
        (minimum.y+maximum.y)*0.5f,(minimum.z+maximum.z)*0.5f};
    const float extent = std::max({maximum.x-minimum.x,maximum.y-minimum.y,maximum.z-minimum.z});
    const float distance = std::max(0.1f,extent*1.5f);
    const float yaw = std::remainder(camera.yawDegrees,360.0f)*3.14159265358979323846f/180.0f;
    const float pitch = std::remainder(camera.pitchDegrees,360.0f)*3.14159265358979323846f/180.0f;
    // Orbit opposite the renderer's forward vector, including requested pitch.
    // Flat pickup meshes need an elevated view; silently forcing pitch=0 made
    // their edge-on silhouette appear to be missing model geometry.
    camera.position = {center.x-std::sin(yaw)*std::cos(pitch)*distance,
        center.y-std::sin(pitch)*distance,center.z+std::cos(yaw)*std::cos(pitch)*distance};
    camera.verticalFovDegrees = 60.0f;
    return camera;
}
void VerifyIsolatedActorCamera() {
    const Vec3 minimum{-1.0f,2.0f,-5.0f},maximum{3.0f,4.0f,-3.0f},center{1.0f,3.0f,-4.0f};
    for (const float yaw : {0.0f,45.0f,90.0f,180.0f,-90.0f})
        for (const float pitch : {0.0f,-45.0f,45.0f,-90.0f}) {
            Camera request;request.yawDegrees=yaw;request.pitchDegrees=pitch;
            const auto framed=FrameIsolatedActor(request,minimum,maximum);
            const float yr=yaw*3.14159265358979323846f/180.0f,pr=pitch*3.14159265358979323846f/180.0f;
            const Vec3 right{std::cos(yr),0.0f,std::sin(yr)};
            const Vec3 up{-std::sin(yr)*std::sin(pr),std::cos(pr),std::cos(yr)*std::sin(pr)};
            const Vec3 delta{center.x-framed.position.x,center.y-framed.position.y,center.z-framed.position.z};
            Require(std::fabs(delta.x*right.x+delta.y*right.y+delta.z*right.z)<0.00001f &&
                std::fabs(delta.x*up.x+delta.y*up.y+delta.z*up.z)<0.00001f,
                "isolated camera orbit lost the selected actor's center");
            Require(framed.pitchDegrees==pitch && framed.yawDegrees==yaw &&
                std::fabs(delta.x*delta.x+delta.y*delta.y+delta.z*delta.z-36.0f)<0.0001f,
                "isolated camera discarded pitch/yaw or changed framing distance");
        }
    Camera huge;huge.yawDegrees=std::numeric_limits<float>::max();huge.pitchDegrees=-std::numeric_limits<float>::max();
    const auto framed=FrameIsolatedActor(huge,minimum,maximum);
    huge.yawDegrees=std::remainder(huge.yawDegrees,360.0f);huge.pitchDegrees=std::remainder(huge.pitchDegrees,360.0f);
    const auto reduced=FrameIsolatedActor(huge,minimum,maximum);
    Require(framed.position.x==reduced.position.x && framed.position.y==reduced.position.y &&
        framed.position.z==reduced.position.z,"isolated camera overflowed large finite angles");
}
void VerifyLightingRenderer() {
    VerifyIsolatedActorCamera();
    const Camera camera;
    auto fixture = MakeSyntheticScene();
    const auto albedo = Render(fixture,camera,256u,256u);
    fixture.vertexLighting.resize(fixture.chunks.size());
    for (std::size_t chunk = 0u; chunk < fixture.chunks.size(); ++chunk)
        fixture.vertexLighting[chunk].assign(fixture.chunks[chunk].vertices.size(),{1.0f,1.0f,1.0f});
    Require(Render(fixture,camera,256u,256u).image.rgb == albedo.image.rgb,
            "unit lighting altered default albedo pixels");
    const auto metadata = BuildSyntheticLightingPreview(fixture);
    const auto lit = Render(fixture,camera,256u,256u);
    Require(metadata.synthetic && metadata.enabled && !metadata.originVerified &&
            metadata.lightStats.total == 1u && metadata.texturedVertices > 0u,
            "synthetic lighting fixture was mislabeled as original map evidence");
    Require(lit.frameHash != albedo.frameHash && lit.coveredPixels == albedo.coveredPixels &&
            lit.clippedTriangles == albedo.clippedTriangles && lit.depth == albedo.depth,
            "optional direct lighting did not preserve world geometry/depth/near clipping");
    Require(metadata.maximumLuminance > metadata.minimumLuminance &&
            metadata.meanLuminance >= metadata.minimumLuminance &&
            metadata.meanLuminance <= metadata.maximumLuminance,
            "shared-light fixture statistics are invalid or spatially uniform");

    // Same oblique geometry as the UV regression, now with RGB gains as varyings.
    // A vertex-light affine interpolation would put substantially more red at
    // the center; the native shader's perspective-correct result is about 43.
    Scene gradient;
    gradient.textureWidth = gradient.textureHeight = gradient.textureLayers = 1u;
    gradient.textures = {255u,255u,255u,255u};
    gradient.chunks.push_back({0, {
        {{-1,-1,-1},{0,0,1},0,0,0},
        {{3,-3,-3},{0,0,1},0,0,0},
        {{0,3,-3},{0,0,1},0,0,0}}});
    gradient.vertexLighting = {{{0,1,.5f},{1,1,.5f},{0,1,.5f}}};
    Camera origin; origin.position = {};
    const auto colored = Render(gradient,origin,256u,256u);
    const auto center = (128u*256u+128u)*3u;
    Require(colored.image.rgb[center] >= 41u && colored.image.rgb[center] <= 45u &&
            colored.image.rgb[center+1u] == 255u && colored.image.rgb[center+2u] >= 127u &&
            colored.image.rgb[center+2u] <= 128u,
            "vertex light RGB interpolation is not perspective correct or tint is wrong");
    gradient.textures[3u] = 0u;
    const auto transparent = Render(gradient,origin,256u,256u);
    Require(transparent.coveredPixels == 0u && transparent.transparentSamples > 0u,
            "lit masked geometry wrote opaque pixels or depth");
    const auto reject = [&](const Scene& invalid) {
        bool rejected{};
        try { (void)Render(invalid,origin,256u,256u); }
        catch (const std::runtime_error&) { rejected = true; }
        return rejected;
    };
    auto invalid = gradient;
    invalid.vertexLighting.push_back({});
    Require(reject(invalid),"lighting stream with extra chunk accepted");
    invalid = gradient;
    invalid.vertexLighting[0u].pop_back();
    Require(reject(invalid),"lighting stream with missing vertex accepted");
    invalid = gradient;
    invalid.vertexLighting[0u][0u].x = std::numeric_limits<float>::quiet_NaN();
    Require(reject(invalid),"non-finite lighting gain accepted");
    invalid.vertexLighting[0u][0u].x = -1.0f;
    Require(reject(invalid),"negative lighting gain accepted");
    invalid.vertexLighting[0u][0u].x = 17.0f;
    Require(reject(invalid),"oversized lighting gain accepted");
}
void VerifySyntheticPipeline(const Scene& scene, const Options& options,
                             const RenderResult& capture) {
    // Test with a fixed camera so user-configured screenshots do not alter regression expectations.
    const Camera camera;
    const auto fixed = Render(scene,camera,256,256);
    const auto center = (128u*256u+128u)*3u;
    Require(fixed.image.rgb[center] > 200u && fixed.image.rgb[center+1u] < 60u,
            "foreground depth test did not hide far wall");
    Require(std::abs(fixed.depth[128u*256u+128u]-2.0f) < 0.001f,"foreground depth is incorrect");
    Require(fixed.clippedTriangles > 0,"near-crossing triangle was not clipped");
    Require(fixed.coveredPixels > fixed.depth.size()/5u,"fixture unexpectedly empty");
    Require(Render(scene,options.camera,options.width,options.height).frameHash == capture.frameHash,
            "same camera produced nondeterministic capture");

    // An oblique gradient triangle distinguishes perspective-correct UVs from affine interpolation.
    Scene gradient;
    gradient.textureWidth = gradient.textureHeight = 256u;
    gradient.textureLayers = 1u;
    gradient.textures.resize(256u*256u*4u);
    for (std::size_t y = 0; y < 256u; ++y) for (std::size_t x = 0; x < 256u; ++x) {
        const auto offset = (y*256u+x)*4u;
        gradient.textures[offset] = static_cast<std::uint8_t>(x);
        gradient.textures[offset+3u] = 255u;
    }
    gradient.chunks.push_back({0, {
        {{-1,-1,-1},{0,0,1},0,0.5f,0},
        {{3,-3,-3},{0,0,1},1,0.5f,0},
        {{0,3,-3},{0,0,1},0,0.5f,0}}});
    Camera origin; origin.position = {};
    const auto perspective = Render(gradient,origin,256,256);
    Require(perspective.image.rgb[center] >= 40u && perspective.image.rgb[center] <= 45u,
            "UV interpolation is not perspective correct");

    // Transparent geometry must leave the depth buffer empty and preserve the background.
    std::fill(gradient.textures.begin(),gradient.textures.end(),0u);
    const auto transparent = Render(gradient,origin,256,256);
    Require(transparent.coveredPixels == 0u && transparent.transparentSamples > 0u,
            "alpha cutoff wrote opaque/depth pixels");
    Require(ReadBmp(options.output).rgb == capture.image.rgb,"BMP round-trip changed capture pixels");
    Camera hugeAngles = camera;
    hugeAngles.yawDegrees = std::numeric_limits<float>::max();
    hugeAngles.pitchDegrees = -std::numeric_limits<float>::max();
    Camera reducedAngles = hugeAngles;
    reducedAngles.yawDegrees = std::remainder(hugeAngles.yawDegrees,360.0f);
    reducedAngles.pitchDegrees = std::remainder(hugeAngles.pitchDegrees,360.0f);
    Require(Render(scene,hugeAngles,256,256).frameHash == Render(scene,reducedAngles,256,256).frameHash,
            "large finite angles overflowed the camera basis");
    Scene extremeUv = scene;
    for (auto& chunk : extremeUv.chunks) for (auto& vertex : chunk.vertices)
        vertex.u = std::numeric_limits<float>::max();
    bool rejectedExtremeUv{};
    try { (void)Render(extremeUv,camera,256,256); }
    catch (const std::runtime_error&) { rejectedExtremeUv = true; }
    Require(rejectedExtremeUv,"overflowed UV interpolation reached integer sampling");

    // Independent package-table fixture mirrors the real Palette/FireTexture name collision.
    PortablePackageTables table;
    for (const char* name : {"None","Fire","FireTexture","Engine","Palette","Laser",
                             "LaserSpot1","background","NYCVent_Wall_A","Texture"})
        table.names.push_back({NameString(name),0u});
    table.imports = {{0,0,0,1},{0,0,-1,2},{0,0,0,3},{0,0,-3,4},{0,0,-3,9}};
    table.exports = {{0,0,0,5,ObjectFlags{},0,0},{-4,0,1,6,ObjectFlags{},0,0},
        {-2,0,1,6,ObjectFlags{},2,0},{0,0,0,7,ObjectFlags{},0,0},
        {-5,0,4,8,ObjectFlags{},0,0}};
    Require(FindPortableTextureExport(table,"laser.laserspot1") == 2u,
            "texture resolution chose the identically named Palette export");
    Require(FindPortableExport(table,"Background.NYCVent_Wall_A") == 4u,
            "Unreal case-insensitive object path was rejected");
    bool rejectedNonTexture{};
    try { (void)FindPortableTextureExport(table,"Laser"); }
    catch (const std::runtime_error&) { rejectedNonTexture = true; }
    Require(rejectedNonTexture,"a non-texture export passed texture class filtering");
    const auto mipFixture = options.output.parent_path()/"synthetic-mip.fixture";
    table.sourcePath = mipFixture.string();
    const auto writeMipHeader = [&](unsigned char mipCount) {
        std::ofstream file(mipFixture,std::ios::binary);
        file.put(0); // None property terminator.
        file.put(static_cast<char>(mipCount));
        if (!file) throw std::runtime_error("Cannot write synthetic mip fixture");
    };
    writeMipHeader(0u);
    Require(LoadPortableTextureMipmaps(table,2u).empty(),"valid zero stored mips were rejected");
    writeMipHeader(33u);
    bool rejectedMipCount{};
    try { (void)LoadPortableTextureMipmaps(table,2u); }
    catch (const std::runtime_error&) { rejectedMipCount = true; }
    Require(rejectedMipCount,"corrupt stored mip count was accepted");
    const auto gc = RunPortableGcProbe();
    Require(gc.passed,"the shared Surreal GC probe failed on the host");
    VerifyLightingRenderer();
}
void Report(const Options& options, const Scene& scene, const RenderResult& result,
            const std::optional<double>& difference, bool passed,
            const AuthoredLightingPreview& lighting) {
    if (!options.report.parent_path().empty()) std::filesystem::create_directories(options.report.parent_path());
    std::ofstream file(options.report);
    if (!file) throw std::runtime_error("Cannot write visual capture report");
    const bool packageCapture = !options.gameRoot.empty();
    Vec3 minimum{std::numeric_limits<float>::infinity(),std::numeric_limits<float>::infinity(),
                 std::numeric_limits<float>::infinity()};
    Vec3 maximum{-minimum.x,-minimum.y,-minimum.z};
    for (const auto& chunk : scene.chunks) for (const auto& vertex : chunk.vertices) {
        minimum.x = std::min(minimum.x,vertex.position.x);
        minimum.y = std::min(minimum.y,vertex.position.y);
        minimum.z = std::min(minimum.z,vertex.position.z);
        maximum.x = std::max(maximum.x,vertex.position.x);
        maximum.y = std::max(maximum.y,vertex.position.y);
        maximum.z = std::max(maximum.z,vertex.position.z);
    }
    file << std::setprecision(10)
        << "{\n  \"passed\": " << (passed ? "true" : "false")
        << ",\n  \"source\": " << Quote(options.selfTest ? "synthetic-fixture" :
            (packageCapture ? "original-game-packages" : "external-quest-cache"))
        << ",\n  \"realMapsDecoded\": " << (packageCapture ? 1 : 0)
        << ",\n  \"campaignPlayabilityVerified\": false"
        << ",\n  \"isolatedActor\": " << Quote(options.isolatedActor)
        << ",\n  \"scope\": " << Quote(!options.isolatedActor.empty() ?
            "Original-asset actor close-up without world BSP; automatic bounds framing; shared Quest CPU poses/transforms/materials. Manual overrides and explicitly invoked original helpers/natives or returned actors are isolated fixtures, not startup, live ticking, campaign playability or Quest GPU evidence" : lighting.actors.enabled ?
            "software original actor meshes/authored pose sampling/skin overrides and mover brushes with shared Quest CPU paths; optional BSP lightmaps; native animation clock, sprites, actor shadowing, environment mapping, UI, OpenXR and Quest GPU unverified" : lighting.baked ?
            "software BSP/material textures with original static light lists, baked shadow masks, zone ambient and Unlit; dynamic lighting, actors, UI, OpenXR, stereo and Quest performance not verified" : lighting.enabled ?
            "software world BSP/material textures with shared Quest direct vertex lighting approximation; no UE1 lightmaps/BSP shadow occlusion, actor meshes, UI, OpenXR, stereo or Quest performance" :
            "software world BSP/material albedo; no actor meshes, map lighting, UI, OpenXR, stereo or Quest performance")
        << ",\n  \"lightingMode\": " << Quote(lighting.baked ? "original-static-shadow-lightmaps" : !lighting.enabled ? "albedo-only" :
            (lighting.synthetic ? "synthetic-direct-vertex-fixture" : "original-map-direct-vertex-approximation"))
        << ",\n  \"authoredLightingPropertiesLoaded\": " << (lighting.enabled && !lighting.synthetic ? "true" : "false")
        << ",\n  \"ue1LightmapsVerified\": false"
        << ",\n  \"bspShadowOcclusionVerified\": false"
        << ",\n  \"ue1StaticLightmapsDecoded\": " << (lighting.baked ? "true" : "false")
        << ",\n  \"authoredShadowMasksApplied\": " << (lighting.baked ? "true" : "false")
        << ",\n  \"gpuLightingNumericalEquivalenceVerified\": false"
        << ",\n  \"lighting\": {\"enabled\": " << (lighting.enabled ? "true" : "false")
        << ", \"synthetic\": " << (lighting.synthetic ? "true" : "false")
        << ", \"worldOriginVerified\": " << (lighting.originVerified ? "true" : "false")
        << ", \"playerStartPath\": " << Quote(lighting.playerStartPath)
        << ", \"unrealOrigin\": ";
    if (lighting.originVerified) file << '[' << lighting.lightStats.unrealOrigin.x << ", "
        << lighting.lightStats.unrealOrigin.y << ", " << lighting.lightStats.unrealOrigin.z << ']';
    else file << "null";
    const auto writeLightByteCounts = [&](const std::array<std::size_t,256>& counts) {
        file << '{';
        bool comma = false;
        for (std::size_t value = 0u; value < counts.size(); ++value) {
            if (counts[value] == 0u) continue;
            if (comma) file << ", ";
            file << Quote(std::to_string(value)) << ": " << counts[value];
            comma = true;
        }
        file << '}';
    };
    file << ", \"emitterTalliesFromAuthoredSnapshots\": "
        << (lighting.emitterTalliesFromAuthoredSnapshots ? "true" : "false")
        << ", \"lightTypeCounts\": ";
    writeLightByteCounts(lighting.lightStats.lightTypeCounts);
    file << ", \"lightEffectCounts\": ";
    writeLightByteCounts(lighting.lightStats.lightEffectCounts);
    file << ", \"lightCountScope\": \"Accepted located, finite, active, nonzero-brightness emitters; type/effect counts are authored byte values, not simulated animations\""
        << ", \"authoredRadiusModel\": \"(LightRadius + 1) * 25 Unreal units; 52.5 Unreal units per meter\"";
    file << ", \"runtimePackages\": " << lighting.packagePaths.size()
        << ", \"runtimeActors\": " << lighting.runtimeActors
        << ", \"unresolvedMapClasses\": " << lighting.unresolvedMapClasses
        << ", \"lightCount\": " << lighting.lightStats.total
        << ", \"spotlightCount\": " << lighting.lightStats.spotlights
        << ", \"coloredLightCount\": " << lighting.lightStats.colored
        << ", \"invalidLightLocations\": " << lighting.lightStats.invalidLocations
        << ", \"texturedVertices\": " << lighting.texturedVertices
        << ", \"minimumVertexLightLuminance\": " << lighting.minimumLuminance
        << ", \"meanVertexLightLuminance\": " << lighting.meanLuminance
        << ", \"maximumVertexLightLuminance\": " << lighting.maximumLuminance
        << ", \"defaultAmbientRgb\": " << (lighting.baked ? "null" : "[0.075, 0.075, 0.075]")
        << ", \"interpolation\": " << Quote(lighting.baked ?
            "Perspective-correct raw UV; per-fragment tile clamp and bilinear sampling of guttered atlas; RGBA8 quantized, not GPU numerical equivalence" :
            "Perspective-correct smooth RGB gains; native shader semantic, not GPU precision verification")
        << ", \"gainModel\": " << Quote(lighting.baked ?
            "Pinned HSB brightness table, zone ambient, ordered static lights, authored padded per-light shadow masks and static light effects; unsupported dynamic sources explicitly omitted" : !lighting.enabled ? "Not applied" :
            (lighting.synthetic ? "Explicit white fixture light and shared direct gain evaluator" :
                "Shared normalized authored HSB hue/saturation and brightness/64 direct-intensity approximation"))
        << ", \"normalInput\": " << Quote(lighting.baked ?
            "Original UModel surface normal and authored Unreal-unit texel world positions" :
            "Unchanged DXQM surface normals; no per-pixel normal lighting")
        << ", \"dynamicLightEffectsSimulated\": false"
        << ", \"staticLightmapBake\": {\"enabled\": " << (lighting.baked ? "true" : "false")
        << ", \"sourceLightmaps\": " << lighting.staticLightmaps.modelLightMaps
        << ", \"sourceShadowBytes\": " << lighting.staticLightmaps.shadowBytes
        << ", \"bakedSurfaceAmbientTiles\": " << lighting.staticLightmaps.bakedSurfaces
        << ", \"pixelSamples\": " << lighting.staticLightmaps.pixelSamples
        << ", \"visibleMaskSamples\": " << lighting.staticLightmaps.visibleMaskSamples
        << ", \"shadowedMaskSamples\": " << lighting.staticLightmaps.shadowedMaskSamples
        << ", \"unlitVertices\": " << lighting.staticLightmaps.unlitVertices
        << ", \"noLightmapVertices\": " << lighting.staticLightmaps.noLightmapVertices
        << ", \"atlasPages\": " << lighting.staticLightmaps.layers
        << ", \"hdrGainScale\": " << lighting.staticLightmaps.gainScale
        << ", \"maximumGain\": " << lighting.staticLightmaps.maximumGain
        << ", \"maximumRgba8QuantizationError\": " << lighting.staticLightmaps.maximumQuantizationError
        << ", \"addedLightReferences\": " << lighting.staticLightmaps.bakeStats.addedLights
        << ", \"unsupportedDynamicTypeReferences\": " << lighting.staticLightmaps.bakeStats.unsupportedTypes
        << ", \"unsupportedDynamicEffectReferences\": " << lighting.staticLightmaps.bakeStats.unsupportedEffects << '}'
        << ", \"flatDiagnosticChunksAuthoredLit\": false, \"packagePaths\": [";
    for (std::size_t index = 0u; index < lighting.packagePaths.size(); ++index) {
        if (index != 0u) file << ", ";
        file << Quote(lighting.packagePaths[index]);
    }
    file << "]}";
    const auto& actors = lighting.actors;
    file << ",\n  \"actors\": {\"enabled\": " << (actors.enabled ? "true" : "false")
        << ", \"meshInstances\": " << actors.meshInstances << ", \"brushInstances\": " << actors.brushInstances
        << ", \"hiddenActors\": " << actors.hiddenActors << ", \"unsupportedActors\": " << actors.unsupportedActors
        << ", \"overriddenMaterials\": " << actors.overriddenMaterials << ", \"missingMaterials\": " << actors.missingMaterials
        << ", \"decodedTextures\": " << actors.decodedTextures << ", \"fallbackTextures\": " << actors.fallbackTextures
        << ", \"maskedTextureVariants\": " << actors.maskedTextureVariants
        << ", \"vertices\": " << actors.vertices << ", \"environmentMappedTrianglesUnverified\": " << actors.environmentMappedTriangles
        << ", \"spriteActorsOmitted\": " << actors.spriteActorsOmitted
        << ", \"sampledPoses\": " << actors.sampledPoses << ", \"poseOmissions\": " << actors.poseOmissions
        << ", \"cubePlaceholdersRendered\": 0, \"authoredPoseSampling\": " << (actors.enabled ? "true" : "false")
        << ", \"nativeAnimationClockImplemented\": true, \"liveClockTicked\": false, \"animationVerified\": false, \"actorShadowingVerified\": false"
        << ", \"isolatedCompiledHelperExecuted\": " << (!lighting.scriptFunction.empty() ? "true" : "false")
        << ", \"compiledHelperFunction\": " << Quote(lighting.scriptFunction)
        << ", \"compiledHelperReceiver\": " << Quote(options.posePreview.actorPath)
        << ", \"compiledHelperUseResultActor\": " << (options.posePreview.scriptUseResultActor ? "true" : "false")
        << ", \"compiledHelperResultActor\": " << Quote(lighting.scriptResultActor)
        << ", \"compiledHelperInstructions\": " << lighting.scriptInstructions
        << ", \"compiledHelperWrites\": " << lighting.scriptWrites
        << ", \"compiledHelperArguments\": [";
    for (std::size_t i = 0u; i < options.posePreview.scriptArguments.size(); ++i) {
        if (i) file << ", ";
        const auto value = options.posePreview.scriptArguments[i].Load();
        file << "{\"kind\": " << Quote(value.kind == QuestVr::Vm::Kind::Object ? "Object" :
            value.kind == QuestVr::Vm::Kind::Name ? "Name" : "Float") << ", \"value\": ";
        if (value.kind == QuestVr::Vm::Kind::Float) file << value.floating;
        else file << Quote(value.text);
        file << '}';
    }
    file << "], \"texturePaths\": [";
    for (std::size_t i = 0u; i < actors.texturePaths.size(); ++i) {
        if (i) file << ", ";
        file << Quote(actors.texturePaths[i]);
    }
    file << "], \"records\": [";
    for (std::size_t i = 0u; i < actors.records.size(); ++i) {
        const auto& actor = actors.records[i];
        if (i) file << ", ";
        file << "{\"path\": " << Quote(actor.path) << ", \"classPath\": " << Quote(actor.classPath)
            << ", \"assetPath\": " << Quote(actor.assetPath) << ", \"error\": " << Quote(actor.error)
            << ", \"position\": [" << actor.position.x << ',' << actor.position.y << ',' << actor.position.z << ']'
            << ", \"bounds\": [[" << actor.minimum.x << ',' << actor.minimum.y << ',' << actor.minimum.z
            << "],[" << actor.maximum.x << ',' << actor.maximum.y << ',' << actor.maximum.z << "]]"
            << ", \"triangles\": " << actor.triangles << ", \"overriddenMaterials\": " << actor.overriddenMaterials
            << ", \"missingMaterials\": " << actor.missingMaterials
            << ", \"brush\": " << (actor.brush ? "true" : "false")
            << ", \"hidden\": " << (actor.hidden ? "true" : "false")
            << ", \"poseSampled\": " << (actor.poseSampled ? "true" : "false")
            << ", \"poseFixture\": " << (actor.poseFixture ? "true" : "false")
            << ", \"animationSource\": " << Quote(actor.animationSource)
            << ", \"requestedSequence\": " << Quote(actor.requestedSequence)
            << ", \"resolvedSequence\": " << Quote(actor.resolvedSequence)
            << ", \"sequenceFallback\": " << (actor.sequenceFallback ? "true" : "false")
            << ", \"selectedOriginalSpanInvalid\": " << (actor.selectedOriginalSpanInvalid ? "true" : "false")
            << ", \"animationFrame\": " << actor.animationFrame << ", \"fatness\": " << unsigned(actor.fatness)
            << ", \"sampledFrames\": " << actor.sampledFrames
            << ", \"normalTriangleSamples\": " << actor.normalTriangleSamples
            << ", \"retainedAnimationBytes\": " << actor.animationBytes
            << ", \"availableSequences\": [";
        for (std::size_t s = 0u; s < actor.availableSequences.size(); ++s) {
            if (s) file << ", ";
            file << Quote(actor.availableSequences[s]);
        }
        file << "], \"invalidOriginalSpanSequences\": [";
        for (std::size_t s = 0u; s < actor.invalidOriginalSpanSequences.size(); ++s) {
            if (s) file << ", ";
            file << Quote(actor.invalidOriginalSpanSequences[s]);
        }
        file << "]}";
    }
    file << "]}"
        << ",\n  \"map\": " << Quote(options.map)
        << ",\n  \"capture\": " << Quote(std::filesystem::absolute(options.output).generic_string())
        << ",\n  \"mesh\": " << Quote(options.mesh.generic_string())
        << ",\n  \"materials\": " << Quote(options.materials.generic_string())
        << ",\n  \"cameraMeters\": [" << options.camera.position.x << ", "
        << options.camera.position.y << ", " << options.camera.position.z << "]"
        << ",\n  \"yawDegrees\": " << options.camera.yawDegrees
        << ",\n  \"pitchDegrees\": " << options.camera.pitchDegrees
        << ",\n  \"verticalFovDegrees\": " << options.camera.verticalFovDegrees
        << ",\n  \"nearPlaneMeters\": " << options.camera.nearPlane
        << ",\n  \"width\": " << result.image.width << ",\n  \"height\": " << result.image.height
        << ",\n  \"triangles\": " << result.inputTriangles
        << ",\n  \"nearClippedTriangles\": " << result.clippedTriangles
        << ",\n  \"rasterizedTriangles\": " << result.rasterizedTriangles
        << ",\n  \"flatTriangles\": " << result.flatTriangles
        << ",\n  \"textureWidth\": " << scene.textureWidth
        << ",\n  \"textureHeight\": " << scene.textureHeight
        << ",\n  \"textureLayers\": " << scene.textureLayers
        << ",\n  \"worldBoundsMeters\": [[" << minimum.x << ", " << minimum.y << ", " << minimum.z
        << "], [" << maximum.x << ", " << maximum.y << ", " << maximum.z << "]]"
        << ",\n  \"coveredPixels\": " << result.coveredPixels
        << ",\n  \"emptyFrame\": " << (result.coveredPixels == 0u ? "true" : "false")
        << ",\n  \"nearlyUniformFrame\": " << (result.luminanceDeviation < 0.005 ? "true" : "false")
        << ",\n  \"coverageFraction\": " << static_cast<double>(result.coveredPixels)/result.depth.size()
        << ",\n  \"meanLuminance\": " << result.meanLuminance
        << ",\n  \"luminanceDeviation\": " << result.luminanceDeviation
        << ",\n  \"frameFnv1a64\": \"" << std::hex << result.frameHash << std::dec << "\""
        << ",\n  \"baselineMeanAbsoluteError\": ";
    if (difference) file << *difference; else file << "null";
    file << ",\n  \"baseline\": " << Quote(options.baseline.generic_string())
         << ",\n  \"maxMeanError\": " << options.maxMeanError
         << ",\n  \"minimumCoverageGate\": ";
    if (options.minimumCoverage) file << *options.minimumCoverage; else file << "null";
    file << "\n}\n";
    if (!file) throw std::runtime_error("Failed writing visual report");
}
int CapturePersona(Options options) {
    options.mesh = options.gameRoot/"System"/"DeusExUI.u";
    ProtectCaptureInputs(options);
    const std::optional<Image> baseline = options.baseline.empty() ? std::nullopt :
        std::optional<Image>{ReadBmp(options.baseline)};
    const auto preview = BuildDesktopPersonaPreview(options.mesh,options.personaIcons,
        options.personaSelected,options.personaPage);
    const auto& layout = QuestVr::GetPersonaUiLayout(options.personaPage);
    const bool inventory = options.personaPage == QuestVr::PersonaUiPage::Inventory;
    std::optional<double> difference;
    if (baseline) difference = MeanAbsoluteImageError(preview.image,*baseline);
    const bool passed = !difference || *difference <= options.maxMeanError;
    WriteBmp(options.output,preview.image);
    if (!options.report.parent_path().empty()) std::filesystem::create_directories(options.report.parent_path());
    std::ofstream report(options.report);
    if (!report) throw std::runtime_error("Cannot write Persona preview report");
    report << "{\n  \"passed\": " << (passed ? "true" : "false")
        << ",\n  \"source\": \"original-DeusExUI-package\""
        << ",\n  \"scope\": \"Shared Quest CPU Persona page artwork, navigation/button chrome and original bitmap-font composition. Text and inventory are explicit preview fixtures, not live campaign state. Health uses neutral original body/overlays. Checkerboard reveals transparency. GL blending, OpenXR, live menus and controller interaction are not verified.\""
        << ",\n  \"campaignPlayabilityVerified\": false"
        << ",\n  \"fontsAndTextVerified\": true"
        << ",\n  \"fontsAndTextVerificationScope\": \"Original atlas decoding and shared CPU fixture composition only\""
        << ",\n  \"liveRuntimeStateVerified\": false"
        << ",\n  \"glRenderingVerified\": false"
        << ",\n  \"openXrVerified\": false"
        << ",\n  \"controllerInteractionVerified\": false"
        << ",\n  \"originalTextEncoding\": \"Single-byte UE1 character indices; Unicode/localized text not verified\""
        << ",\n  \"navigationAvailableTabs\": [\"Inventory\", \"Health\", \"Goals/Notes\", \"Logs\"]"
        << ",\n  \"navigationDisabledTabs\": [\"Augs\", \"Skills\", \"Conversations\", \"Images\"]"
        << ",\n  \"actionCaptionScope\": \"VR binding labels on original button artwork, not every original desktop action\""
        << ",\n  \"package\": " << Quote(std::filesystem::absolute(options.mesh).generic_string())
        << ",\n  \"capture\": " << Quote(std::filesystem::absolute(options.output).generic_string())
        << ",\n  \"width\": 640,\n  \"height\": 480"
        << ",\n  \"page\": " << Quote(DesktopPersonaPageName(options.personaPage))
        << ",\n  \"backgroundTileCount\": " << layout.backgroundCount
        << ",\n  \"backgroundColumns\": " << layout.backgroundColumns
        << ",\n  \"clientRect\": [" << layout.client.x << ", " << layout.client.y << ", "
        << layout.client.width << ", " << layout.client.height << "]"
        << ",\n  \"borderRect\": [" << layout.border.x << ", " << layout.border.y << ", "
        << layout.border.width << ", " << layout.border.height << "]"
        << ",\n  \"gridRect\": " << (inventory ? "[42, 62, 266, 319]" : "null")
        << ",\n  \"gridColumns\": " << (inventory ? 5 : 0)
        << ",\n  \"gridRows\": " << (inventory ? 6 : 0)
        << ",\n  \"originalInventoryFootprints\": " << (preview.originalInventoryFootprints ? "true" : "false")
        << ",\n  \"inventoryPlacementScope\": " << Quote(!inventory ? "No inventory fixture on this page" : preview.originalInventoryFootprints ?
            "Actual original class-default sizes and logical icon windows; detached row-major display packing, not saved/live invPos or executed pickup lifecycle" :
            "Legacy one-cell asset-only fixture; no original item footprint metadata")
        << ",\n  \"inventoryOccupiedCells\": " << preview.inventoryLayout.occupiedCells
        << ",\n  \"inventoryDisplayOnlyPacked\": " << preview.inventoryLayout.generatedPositions
        << ",\n  \"inventoryUnplacedCount\": " << preview.inventoryLayout.unplaced.size()
        << ",\n  \"healthBodyRect\": " << (options.personaPage == QuestVr::PersonaUiPage::Health ?
            "[49, 73, 219, 357]" : "null")
        << ",\n  \"selectedFixtureIndex\": ";
    if (inventory) report << options.personaSelected; else report << "null";
    report
        << ",\n  \"transparentPixels\": " << preview.transparentPixels
        << ",\n  \"opaquePixels\": " << preview.opaquePixels
        << ",\n  \"partialAlphaPixels\": " << preview.partialAlphaPixels
        << ",\n  \"visibleBoundsInclusive\": [" << preview.visibleMinX << ", " << preview.visibleMinY
        << ", " << preview.visibleMaxX << ", " << preview.visibleMaxY << "]"
        << ",\n  \"rgbaFnv1a64\": \"" << std::hex << preview.rgbaHash << "\""
        << ",\n  \"flattenedFnv1a64\": \"" << preview.flattenedHash << "\"" << std::dec
        << ",\n  \"artworkPaths\": [";
    for (std::size_t i = 0; i < preview.artworkPaths.size(); ++i) {
        if (i != 0u) report << ", ";
        report << Quote(preview.artworkPaths[i]);
    }
    report << "],\n  \"iconPaths\": [";
    for (std::size_t i = 0; i < preview.iconPaths.size(); ++i) {
        if (i != 0u) report << ", ";
        report << Quote(preview.iconPaths[i]);
    }
    report << "],\n  \"inventoryClassPaths\": [";
    for (std::size_t index = 0u; index < preview.inventoryClassPaths.size(); ++index) {
        if (index != 0u) report << ", ";
        report << Quote(preview.inventoryClassPaths[index]);
    }
    report << "],\n  \"inventoryMetadataSources\": [";
    for (std::size_t index = 0u; index < preview.inventoryMetadataSources.size(); ++index) {
        if (index != 0u) report << ", ";
        report << Quote(preview.inventoryMetadataSources[index]);
    }
    report << "],\n  \"inventoryPlacements\": [";
    for (std::size_t index = 0u; index < preview.inventoryLayout.placements.size(); ++index) {
        if (index != 0u) report << ", ";
        const auto& placed = preview.inventoryLayout.placements[index];
        report << "{\"fixtureIndex\": " << placed.inventoryIndex << ", \"displayOnlyPacked\": "
            << (placed.generatedPosition ? "true" : "false") << ", \"rect\": ["
            << placed.rect.x << ", " << placed.rect.y << ", " << placed.rect.width << ", " << placed.rect.height << "]}";
    }
    report << "],\n  \"fonts\": [";
    for (std::size_t index = 0u; index < preview.fonts.size(); ++index) {
        if (index != 0u) report << ", ";
        const auto& font = preview.fonts[index];
        report << "{\"objectPath\": " << Quote(font.objectPath)
            << ", \"glyphCount\": " << font.glyphCount
            << ", \"charactersPerPage\": " << font.charactersPerPage
            << ", \"lineHeight\": " << font.lineHeight
            << ", \"atlasPaths\": [";
        for (std::size_t atlas = 0u; atlas < font.atlasPaths.size(); ++atlas) {
            if (atlas != 0u) report << ", ";
            report << Quote(font.atlasPaths[atlas]);
        }
        report << "], \"atlasDimensions\": [";
        for (std::size_t atlas = 0u; atlas < font.atlasDimensions.size(); ++atlas) {
            if (atlas != 0u) report << ", ";
            report << '[' << font.atlasDimensions[atlas][0] << ", " << font.atlasDimensions[atlas][1] << ']';
        }
        report << "]}";
    }
    report << "],\n  \"fixtureText\": {\"left\": " << Quote(preview.fixtureLeftText)
        << ", \"right\": " << Quote(preview.fixtureRightText) << "}"
        << ",\n  \"baseline\": " << Quote(options.baseline.generic_string())
        << ",\n  \"maxMeanError\": " << options.maxMeanError
        << ",\n  \"baselineMeanAbsoluteError\": ";
    if (difference) report << *difference; else report << "null";
    report << "\n}\n";
    if (!report) throw std::runtime_error("Failed writing Persona preview report");
    std::cout << "Original Persona artwork preview: " << std::filesystem::absolute(options.output).string()
        << "\n" << DesktopPersonaPageName(options.personaPage) << ": "
        << preview.artworkPaths.size() << " original page artwork assets; " << preview.iconPaths.size()
        << " icon fixture assets; " << preview.transparentPixels << " transparent pixels.\n"
        << "Original bitmap fonts and text are CPU fixture renders. GL, OpenXR and live interaction remain unverified.\n";
    return passed ? 0 : 2;
}
} // namespace

int main(int argc, char** argv) {
    try {
        if (argc == 1 || (argc == 2 && std::string(argv[1]) == "--help")) { Help(); return 0; }
        if (argc == 4 && std::string(argv[1]) == "--inspect-textures") {
            InspectTextures(argv[2],argv[3]); return 0;
        }
        if (argc == 2 && std::string(argv[1]) == "--test-persona-canvas") {
            VerifySharedPersonaCanvas();
            std::cout << "PASS: shared Persona masks, clipping, tints, padding, grid, icon aspect, bitmap text, button strips and disabled-tab checks.\n";
            return 0;
        }
        Options options = Parse(argc,argv);
        if (options.personaPreview) return CapturePersona(std::move(options));
        std::filesystem::path fixture;
        if (options.selfTest) {
            fixture = options.output.parent_path()/"synthetic-cache";
            options.mesh = fixture/"quest-world.mesh";
            options.materials = fixture/"quest-material-array.rgba";
        } else if (!options.gameRoot.empty()) {
            if (!std::filesystem::is_directory(options.gameRoot/"Maps"))
                throw std::runtime_error("GameRoot must contain original Maps and texture packages");
            if (options.cacheRoot.empty()) options.cacheRoot = options.output.parent_path()/"decoded-cache"/options.map;
            const auto cache = std::filesystem::weakly_canonical(options.cacheRoot);
            options.mesh = cache/"quest-world.mesh";
            options.materials = cache/"quest-material-array.rgba";
        }
        // Validate aliases and load the baseline before any fixture/cache/capture writes.
        ProtectCaptureInputs(options);
        const std::optional<Image> baseline = options.baseline.empty() ? std::nullopt :
            std::optional<Image>{ReadBmp(options.baseline)};
        if (options.selfTest) WriteSyntheticCache(fixture,MakeSyntheticScene());
        else if (!options.gameRoot.empty()) {
            const auto game = std::filesystem::weakly_canonical(options.gameRoot);
            const auto cache = std::filesystem::weakly_canonical(options.cacheRoot);
            if (!BuildQuestMapCacheToDirectory(game.string().c_str(),options.map.c_str(),cache.string().c_str()))
                throw std::runtime_error("The shared Quest decoder could not build this map; see package error above");
        }
        auto scene = ReadQuestCache(options.mesh,options.materials);
        AuthoredLightingPreview lighting;
        if (options.authoredLighting || options.actors) lighting = options.selfTest ? BuildSyntheticLightingPreview(scene) :
            BuildAuthoredLightingPreview(scene,options.gameRoot,options.map,options.mesh,options.bakedLighting,
                options.actors,options.authoredLighting,options.posePreview);
        if (options.posePreview.scriptUseResultActor) {
            if (lighting.scriptResultActor.empty())
                throw std::runtime_error("Compiled helper did not return an eligible nonnull map Actor Object");
            options.isolatedActor = lighting.scriptResultActor;
        }
        if (!options.isolatedActor.empty()) {
            const auto& records = lighting.actors.records;
            const auto actor = std::find_if(records.begin(),records.end(),[&](const auto& record) {
                return Lower(record.path) == Lower(options.isolatedActor);
            });
            if (actor == records.end() || actor->triangles == 0u || actor->chunkCount == 0u)
                throw std::runtime_error("Isolated actor must name a rendered full original object path");
            Scene isolated;
            isolated.actorTextureWidth = scene.actorTextureWidth;
            isolated.actorTextureHeight = scene.actorTextureHeight;
            isolated.actorTextureLayers = scene.actorTextureLayers;
            isolated.actorTextures = std::move(scene.actorTextures);
            for (std::size_t c = actor->firstChunk; c < actor->firstChunk+actor->chunkCount; ++c) {
                isolated.chunks.push_back(std::move(scene.chunks[c]));
                isolated.vertexLighting.push_back(std::move(scene.vertexLighting[c]));
            }
            scene = std::move(isolated);
            options.camera = FrameIsolatedActor(options.camera,actor->minimum,actor->maximum);
        }
        const auto result = Render(scene,options.camera,options.width,options.height);
        WriteBmp(options.output,result.image);
        if (options.selfTest) VerifySyntheticPipeline(scene,options,result);
        std::optional<double> difference;
        if (baseline) difference = MeanAbsoluteImageError(result.image,*baseline);
        const double coverage = static_cast<double>(result.coveredPixels)/result.depth.size();
        const bool passed = (!difference || *difference <= options.maxMeanError) &&
            (!options.minimumCoverage || coverage >= *options.minimumCoverage);
        Report(options,scene,result,difference,passed,lighting);
        std::cout << (options.selfTest ? "Synthetic renderer checks passed; no real maps tested.\n" :
            "World cache capture complete; campaign playability is unverified.\n")
            << "Triangles: " << result.inputTriangles << ", material layers: " << scene.textureLayers
            << ", coverage: " << std::fixed << std::setprecision(3) << coverage
            << ", hash: " << std::hex << result.frameHash << std::dec << '\n'
            << "Capture: " << std::filesystem::absolute(options.output).string() << '\n'
            << "Report: " << std::filesystem::absolute(options.report).string() << '\n';
        if (lighting.baked) std::cout << "Original static lightmap preview: "
            << lighting.staticLightmaps.bakedSurfaces << " surface/ambient tiles, "
            << lighting.staticLightmaps.pixelSamples << " texels, "
            << lighting.staticLightmaps.shadowedMaskSamples << " authored shadowed samples, "
            << lighting.staticLightmaps.unlitVertices << " unlit vertices; "
            << lighting.staticLightmaps.bakeStats.unsupportedTypes << " dynamic type references and "
            << lighting.staticLightmaps.bakeStats.unsupportedEffects << " dynamic effect references omitted. "
            << "No Quest GPU, campaign playability or original-renderer image equivalence verification.\n";
        else if (lighting.enabled) std::cout << (lighting.synthetic ? "Synthetic" : "Original authored")
            << " light preview: " << lighting.lightStats.total << " lights ("
            << lighting.lightStats.spotlights << " spot, " << lighting.lightStats.colored << " colored), "
            << lighting.texturedVertices << " textured vertices, gain luminance min/mean/max "
            << lighting.minimumLuminance << "/" << lighting.meanLuminance << "/" << lighting.maximumLuminance
            << ". No lightmaps, BSP shadow occlusion or Quest performance verification.\n";
        if (!passed) { std::cerr << "Configured baseline/coverage gate failed.\n"; return 2; }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Desktop visual capture failed: " << error.what() << '\n';
        return 1;
    }
}
