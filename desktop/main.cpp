#include "visual_renderer.h"
#include "surreal_gc_probe.h"
#include "surreal_portable_package_tables.h"
#include "persona_preview.h"
#include "persona_ui_canvas.h"

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
    std::filesystem::path gameRoot, cacheRoot, mesh, materials, output, report, baseline;
    std::string map;
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
    for (const auto& input : {options.baseline,options.mesh,options.materials}) {
        if (SameFile(options.output,input) || SameFile(options.report,input))
            throw std::runtime_error("Output/report must not alias baseline or source cache files");
    }
    if (SameFile(options.baseline,options.mesh) || SameFile(options.baseline,options.materials))
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
        "  --inspect-textures PACKAGE FILTER  List matching export/class/properties for diagnosis\n"
        "  --persona-preview --game-root PATH Original shared Persona artwork/icons on checkerboard\n"
        "  --persona-icon NAME                Repeat for each original icon asset (preview fixture)\n"
        "  --persona-selected INDEX           Select an icon in the fixture inventory grid\n"
        "  --persona-page PAGE                Inventory, Health, GoalsNotes or Logs\n"
        "Camera uses Quest-cache meters; default (0,1.65,0) looks -Z. Positive yaw turns right.\n"
        "Captures show world BSP and material albedo. Actors, baked map lights, UI,\n"
        "OpenXR tracking, stereo, campaign scripts, and Quest GPU performance are not covered.\n";
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
    const bool hasCache = !options.mesh.empty() || !options.materials.empty();
    const bool hasGame = !options.gameRoot.empty();
    if (options.personaPreview) {
        if (!hasGame || hasCache || options.selfTest || !options.cacheRoot.empty() || options.minimumCoverage)
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
    return options;
}
void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(std::string("Synthetic pipeline regression: ")+message);
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
}
void Report(const Options& options, const Scene& scene, const RenderResult& result,
            const std::optional<double>& difference, bool passed) {
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
        << ",\n  \"scope\": \"software world BSP/material albedo; no actor meshes, map lighting, UI, OpenXR, stereo or Quest performance\""
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
        << ",\n  \"scope\": \"Shared Quest CPU Persona page background/border composition; Health includes original neutral body/overlays, Inventory includes grid and icon asset fixture rather than saved inventory. Checkerboard reveals transparency. Text, fonts, tabs, live menus, VR geometry and interaction unverified.\""
        << ",\n  \"campaignPlayabilityVerified\": false"
        << ",\n  \"fontsAndTextVerified\": false"
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
    report << "],\n  \"baseline\": " << Quote(options.baseline.generic_string())
        << ",\n  \"maxMeanError\": " << options.maxMeanError
        << ",\n  \"baselineMeanAbsoluteError\": ";
    if (difference) report << *difference; else report << "null";
    report << "\n}\n";
    if (!report) throw std::runtime_error("Failed writing Persona preview report");
    std::cout << "Original Persona artwork preview: " << std::filesystem::absolute(options.output).string()
        << "\n" << DesktopPersonaPageName(options.personaPage) << ": "
        << preview.artworkPaths.size() << " original page artwork assets; " << preview.iconPaths.size()
        << " icon fixture assets; " << preview.transparentPixels << " transparent pixels.\n"
        << "Fonts, text and live VR interaction are unverified.\n";
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
            std::cout << "PASS: shared Persona masks, clipping, tints, padding, grid and icon aspect checks.\n";
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
        const auto scene = ReadQuestCache(options.mesh,options.materials);
        const auto result = Render(scene,options.camera,options.width,options.height);
        WriteBmp(options.output,result.image);
        if (options.selfTest) VerifySyntheticPipeline(scene,options,result);
        std::optional<double> difference;
        if (baseline) difference = MeanAbsoluteImageError(result.image,*baseline);
        const double coverage = static_cast<double>(result.coveredPixels)/result.depth.size();
        const bool passed = (!difference || *difference <= options.maxMeanError) &&
            (!options.minimumCoverage || coverage >= *options.minimumCoverage);
        Report(options,scene,result,difference,passed);
        std::cout << (options.selfTest ? "Synthetic renderer checks passed; no real maps tested.\n" :
            "World cache capture complete; campaign playability is unverified.\n")
            << "Triangles: " << result.inputTriangles << ", material layers: " << scene.textureLayers
            << ", coverage: " << std::fixed << std::setprecision(3) << coverage
            << ", hash: " << std::hex << result.frameHash << std::dec << '\n'
            << "Capture: " << std::filesystem::absolute(options.output).string() << '\n'
            << "Report: " << std::filesystem::absolute(options.report).string() << '\n';
        if (!passed) { std::cerr << "Configured baseline/coverage gate failed.\n"; return 2; }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Desktop visual capture failed: " << error.what() << '\n';
        return 1;
    }
}
