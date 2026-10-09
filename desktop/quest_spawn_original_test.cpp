#include "Precomp.h"
#include "quest_spawn_placement.h"
#include "surreal_portable_package_tables.h"

// Load the REAL vendor types before the two narrowly scoped substitutions.
// We do not construct a UModel/UObject, invent its vtable, or link gameplay.
#include "Packages/Engine/Resources/Level/UModel.h"
#include "Packages/Engine/Resources/Level/ULevel.h"

struct QuestSpawnPinnedModelData {
    Array<BspNode> Nodes;
    Array<std::int32_t> LeafHulls;
};
static_assert(sizeof(vec3)==3u*sizeof(std::int32_t),"Pin's LeafHulls bbox vec3 size changed");
static_assert(alignof(vec3)<=alignof(std::int32_t),"Pin's LeafHulls bbox alignment is unsafe");
#define UModel QuestSpawnPinnedModelData
#define OverlapAABBModel QuestSpawnPinnedOverlap
// The pinned hull query is compiled verbatim, not copied into an oracle.
// Its Model data access uses the above carrier; math, nodes and hit list are
// the real vendor types. This is not a UObject/lifecycle differential.
#include "Collision/BottomLevel/OverlapAABBModel.cpp"
#undef OverlapAABBModel
#undef UModel

#include <filesystem>
#include <iomanip>
#include <iostream>
#include <map>
#include <set>
#include <sstream>

namespace {
using Vec = PortableModelVec3;
static_assert(sizeof(Vec)==3u*sizeof(float),"Authored Location Vector layout changed");
std::size_t checks{}, placementCases{}, overlapCases{};

void Require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
    ++checks;
}
bool Same(const Vec& a, const Vec& b) {
    // Exact float results, including sign of zero. Candidate arithmetic is not
    // compared with a loose epsilon that could hide a wrong grid offset.
    return std::memcmp(&a.x, &b.x, sizeof(float)) == 0 &&
           std::memcmp(&a.y, &b.y, sizeof(float)) == 0 &&
           std::memcmp(&a.z, &b.z, sizeof(float)) == 0;
}
vec3 PinnedVec(const Vec& value) { return {value.x, value.y, value.z}; }
Vec PortableVec(const vec3& value) { return {value.x, value.y, value.z}; }
std::string Position(const Vec& value) {
    std::ostringstream output;
    output << std::setprecision(9) << '(' << value.x << ',' << value.y << ',' << value.z << ')';
    return output.str();
}

// The upstream recursive query is unsafe for malformed arrays/cycles and has
// no work/depth limit. Validate first and cap its WORST unpruned root traversal,
// including repeated hull scans/classification for a complete 27-query group,
// before invoking it. Invalid-fixture refusal remains the separate pure test.
QuestSpawnPinnedModelData PreparePinned(const PortableModelGeometry& model) {
    QuestVr::SpawnPlacementStats stats;
    QuestVr::SpawnPlacementDetail::ValidateModel(model, {}, stats);
    constexpr std::size_t maximumGroupWork=16'000'000u;
    constexpr std::size_t maximumQueryWork=maximumGroupWork/27u;
    std::vector<std::size_t> ownWork(model.nodes.size(),1u), subtreeWork(model.nodes.size());
    std::size_t addressedWords{};
    for (std::size_t i=0u; i<model.nodes.size(); ++i) if (model.nodes[i].collisionBound>=0) {
        const auto hull=QuestVr::SpawnPlacementDetail::ReadHull(model,model.nodes[i].collisionBound,
                                                               addressedWords,maximumGroupWork);
        const auto planes=hull.end-hull.first;
        // One recursive node visit, one terminator + six bbox words, the
        // terminator scan and a possible second full plane classification.
        Require(planes<=(maximumQueryWork-8u)/2u,"Pinned oracle addressed hull work exceeds group bound");
        ownWork[i]+=2u*planes+7u;
    }
    struct Frame { std::size_t index; unsigned next{}; };
    std::vector<Frame> stack{{0u, 0u}};
    std::vector<std::size_t> depth(model.nodes.size()), visits(model.nodes.size());
    while (!stack.empty()) {
        auto& frame = stack.back();
        const auto& node = model.nodes[frame.index];
        if (frame.next < 2u) {
            const auto child = frame.next++ == 0u ? node.front : node.back;
            if (child >= 0 && depth[static_cast<std::size_t>(child)] == 0u)
                stack.push_back({static_cast<std::size_t>(child), 0u});
            continue;
        }
        std::size_t count = 1u, height = 1u, work=ownWork[frame.index];
        for (const auto child : {node.front, node.back}) if (child >= 0) {
            const auto index = static_cast<std::size_t>(child);
            Require(visits[index] <= 1'000'000u - count, "Pinned oracle traversal exceeds bounded work");
            count += visits[index];
            height = std::max(height, depth[index] + 1u);
            Require(subtreeWork[index]<=maximumQueryWork-work,
                    "Pinned oracle repeated hull/plane work exceeds 27-query group bound");
            work+=subtreeWork[index];
        }
        Require(height <= 1024u, "Pinned recursive oracle depth exceeds test safety limit");
        depth[frame.index] = height;
        visits[frame.index] = count;
        subtreeWork[frame.index]=work;
        stack.pop_back();
    }
    QuestSpawnPinnedModelData result;
    result.Nodes.resize(model.nodes.size());
    for (std::size_t i = 0u; i < model.nodes.size(); ++i) {
        const auto& source = model.nodes[i];
        auto& target = result.Nodes[i];
        target.PlaneX = source.planeX; target.PlaneY = source.planeY;
        target.PlaneZ = source.planeZ; target.PlaneW = source.planeW;
        target.ZoneMask = source.zoneMask; target.NodeFlags = source.nodeFlags;
        target.VertPool = source.vertexPool; target.Surf = source.surface;
        target.Back = source.back; target.Front = source.front; target.Plane = source.plane;
        target.CollisionBound = source.collisionBound; target.RenderBound = source.renderBound;
        target.Zone0 = source.zone0; target.Zone1 = source.zone1;
        target.NumVertices = source.vertexCount; target.Leaf0 = source.leaf0; target.Leaf1 = source.leaf1;
    }
    result.LeafHulls.assign(model.leafHulls.begin(), model.leafHulls.end());
    return result;
}
bool PinnedOverlap(QuestSpawnPinnedModelData& model, const Vec& center, const Vec& extent) {
    // Audited OverlapTester world-only bridge: all-zero extents skip the Model.
    // The full actor hash, ULevel and OverlapTester are not linked or simulated.
    if (extent.x == 0.0f && extent.y == 0.0f && extent.z == 0.0f) return false;
    QuestSpawnPinnedOverlap query;
    return !query.TestOverlap(&model, PinnedVec(center), PinnedVec(extent), false).empty();
}
struct PinnedPlacement { bool found{}; Vec location; std::size_t probes{}; };
PinnedPlacement PinnedGrid(QuestSpawnPinnedModelData& model, Vec location,
                           float radius, float height, bool check) {
    // This small harness follows UActor_Phys.cpp::CheckLocation, but that
    // function is NOT compiled/linked. All 27 first-free choices below are
    // independently constrained by six-box fixtures, not assumed from a
    // second implementation agreeing with the port's loop.
    PinnedPlacement result{false, location, 0u};
    if (!check) { result.found = true; return result; }
    const int offset[]{0, 1, -1};
    const float scale = std::max(radius, height);
    for (int z = 0; z < 3; ++z) for (int y = 0; y < 3; ++y) for (int x = 0; x < 3; ++x) {
        const vec3 candidate = PinnedVec(location) + vec3(offset[x] * scale, offset[y] * scale, offset[z] * scale);
        ++result.probes;
        if (!PinnedOverlap(model, PortableVec(candidate), {radius, radius, height})) {
            result.found = true; result.location = PortableVec(candidate); return result;
        }
    }
    return result;
}
void Compare(const PortableModelGeometry& model, QuestSpawnPinnedModelData& pinned,
             const Vec& requested, float radius, float height, bool collideWorld,
             bool collideWhenPlacing, const std::string& label, bool allProbes) {
    const auto actual = QuestVr::FindSpawnPlacement(model, requested, radius, height,
                                                   collideWorld, collideWhenPlacing);
    const auto expected = PinnedGrid(pinned, requested, radius, height, collideWorld || collideWhenPlacing);
    Require(actual.found == expected.found && actual.probes == expected.probes &&
            Same(actual.location, expected.location),
            label + " placement mismatch at " + Position(requested) + " actual=" +
            Position(actual.location) + " pin=" + Position(expected.location));
    ++placementCases;
    if (!allProbes) return;
    // Check EVERY candidate's hull result, including candidates after the
    // first feasible location. Reuse validated data, with the same public
    // helper query algorithm and fixed aggregate query bounds.
    const float scale = std::max(radius, height);
    QuestVr::SpawnPlacementStats stats;
    for (int z : {0, 1, -1}) for (int y : {0, 1, -1}) for (int x : {0, 1, -1}) {
        const Vec candidate{requested.x + x * scale, requested.y + y * scale, requested.z + z * scale};
        const bool portable = (radius == 0.0f && height == 0.0f) ? false :
            QuestVr::SpawnPlacementDetail::QueryValidatedWorld(model, candidate,
                {radius, radius, height}, {}, stats);
        Require(portable == PinnedOverlap(pinned, candidate, {radius, radius, height}),
                label + " hull mismatch at " + Position(candidate));
        ++overlapCases;
    }
}

PortableModelNode Node() {
    PortableModelNode node;
    node.front = node.back = node.plane = node.collisionBound = -1;
    return node;
}
std::int32_t Word(float value) { std::int32_t result; std::memcpy(&result, &value, sizeof(result)); return result; }
struct Boxes {
    PortableModelGeometry model;
    std::size_t last{};
    Boxes() { model.nodes.push_back(Node()); }
    void Add(Vec low, Vec high) {
        const auto index = model.nodes.size();
        model.nodes.push_back(Node());
        model.nodes[last].front = static_cast<std::int32_t>(index);
        last = index;
        model.nodes[index].collisionBound = static_cast<std::int32_t>(model.leafHulls.size());
        const float minima[]{low.x, low.y, low.z}, maxima[]{high.x, high.y, high.z};
        for (unsigned axis = 0u; axis < 3u; ++axis) for (bool flip : {false, true}) {
            auto node = Node();
            if (axis == 0u) node.planeX = 1.0f;
            if (axis == 1u) node.planeY = 1.0f;
            if (axis == 2u) node.planeZ = 1.0f;
            node.planeW = flip ? minima[axis] : maxima[axis];
            const auto reference = static_cast<std::uint32_t>(model.nodes.size());
            model.nodes.push_back(node);
            model.leafHulls.push_back(static_cast<std::int32_t>(reference | (flip ? 0x4000'0000u : 0u)));
        }
        model.leafHulls.push_back(-1);
        for (float value : {low.x, low.y, low.z, high.x, high.y, high.z}) model.leafHulls.push_back(Word(value));
    }
};
void Synthetic() {
    {
        // A shallow valid DAG has only 12 distinct nodes and < 2,100 root
        // visits, but repeatedly scans/classifies the same 1,000-plane hull.
        // Refuse in preflight WITHOUT executing the recursive vendor oracle.
        PortableModelGeometry amplified;
        amplified.nodes.assign(2u,Node());
        amplified.nodes[1].collisionBound=0;
        amplified.leafHulls.assign(1000u,1);
        amplified.leafHulls.push_back(-1);
        for (float value : {-1.0f,-1.0f,-1.0f,1.0f,1.0f,1.0f})
            amplified.leafHulls.push_back(Word(value));
        std::size_t last=1u;
        for (unsigned layer=0u; layer<10u; ++layer) {
            auto node=Node(); node.front=node.back=static_cast<std::int32_t>(last);
            last=amplified.nodes.size(); amplified.nodes.push_back(node);
        }
        amplified.nodes[0].front=static_cast<std::int32_t>(last);
        bool refused{};
        try { (void)PreparePinned(amplified); }
        catch (const std::runtime_error& error) {
            refused=std::string(error.what()).find("repeated hull/plane work")!=std::string::npos;
        }
        Require(refused,"Unsafe oracle hull-work amplification escaped preflight");
    }
    for (const Vec dimensions : {Vec{1,1,1}, Vec{0.25f,0.25f,2}, Vec{2,2,0.25f}}) {
        const Vec requested{7,-3,5};
        const float scale = std::max(dimensions.x, dimensions.z);
        std::size_t ordinal{};
        for (int z : {0,1,-1}) for (int y : {0,1,-1}) for (int x : {0,1,-1}) {
            const Vec desired{requested.x+x*scale, requested.y+y*scale, requested.z+z*scale};
            Boxes fixture;
            const float c[]{desired.x, desired.y, desired.z};
            const float e[]{dimensions.x, dimensions.y, dimensions.z};
            for (unsigned axis=0u; axis<3u; ++axis) for (bool positive : {false,true}) {
                Vec low{-100,-100,-100}, high{100,100,100};
                const float boundary=c[axis]+(positive ? 1.0f : -1.0f)*(e[axis]+0.125f);
                if (axis==0u) (positive ? low.x : high.x)=boundary;
                if (axis==1u) (positive ? low.y : high.y)=boundary;
                if (axis==2u) (positive ? low.z : high.z)=boundary;
                fixture.Add(low,high);
            }
            auto pinned=PreparePinned(fixture.model);
            const auto known=PinnedGrid(pinned,requested,dimensions.x,dimensions.z,true);
            Require(known.found && Same(known.location,desired) && known.probes==ordinal+1u,
                    "Pinned harness did not choose the independently constrained grid cell");
            for (const auto& flags : {std::pair{true,false},std::pair{false,true},std::pair{true,true}})
                Compare(fixture.model,pinned,requested,dimensions.x,dimensions.z,flags.first,flags.second,
                        "synthetic gap",true);
            ++ordinal;
        }
        Require(ordinal==27u,"Synthetic controls missed a grid candidate");
    }
    Boxes solid; solid.Add({-100,-100,-100},{100,100,100});
    auto pinned=PreparePinned(solid.model);
    Compare(solid.model,pinned,{7,-3,5},1,2,true,false,"solid exhaustion",true);
    Require(!PinnedGrid(pinned,{7,-3,5},1,2,true).found,"Solid fixture was not blocked");
    Compare(solid.model,pinned,{7,-3,5},1,2,false,false,"collision flag bypass",false);
    Compare(solid.model,pinned,{7,-3,5},0,0,true,true,"zero extents",true);
    // Independent inclusive-contact controls exercise both bbox and planes.
    Require(PinnedOverlap(pinned,{101,0,0},{1,1,2}),"Pin dropped inclusive exact contact");
    Require(!PinnedOverlap(pinned,{101.25f,0,0},{1,1,2}),"Pin overlapped separated box");
    std::cout << "PASS synthetic: every 27-probe first-free choice, asymmetric dimensions, flags, "
              << "exhaustion, zero extents, inclusive contact; compiled pinned hull query.\n";
}

float PropertyFloat(const PortableTaggedProperty& property) {
    Require(property.type==4u && property.value.size()==4u && property.arrayIndex==0u,
            "Collision class default is not a scalar float");
    float value; std::memcpy(&value,property.value.data(),sizeof(value));
    Require(std::isfinite(value) && value>=0.0f,"Collision class default is nonfinite/negative");
    return value;
}
struct Defaults { std::string path; float radius{},height{}; bool world{},placing{}; };
class Classes {
public:
    explicit Classes(std::filesystem::path gameRoot) : root(std::move(gameRoot)) {}
    Defaults Read(const std::string& path) {
        std::vector<PortableClassDescriptor> chain;
        std::set<std::string> seen;
        std::string current=path;
        bool actor{};
        while (!current.empty()) {
            Require(chain.size()<128u && seen.insert(current).second,"Class default ancestry cycle/depth");
            const auto split=current.find('.');
            Require(split!=std::string::npos,"Class default identity has no package");
            auto& package=Package(current.substr(0u,split));
            const auto packageName=current.substr(0u,split);
            const auto index=FindPortableExport(package,current.substr(split+1u));
            const auto& row=package.exports.at(index);
            Require(row.ObjClass==0,"Spawn prospective type is not an authored UClass export");
            if (row.ObjBase<0) {
                const auto& imported=package.imports.at(static_cast<std::size_t>(-static_cast<std::int64_t>(row.ObjBase)-1));
                Require(package.names.at(static_cast<std::size_t>(imported.ClassName)).Name=="Class",
                        "Class default base import does not constrain an actual UClass");
            }
            chain.push_back(LoadPortableClassDescriptor(package,index));
            if (current=="Engine.Actor") actor=true;
            current=row.ObjBase==0 ? std::string{} : GetPortableObjectPath(package,row.ObjBase);
            if (row.ObjBase>0) current=packageName+'.'+current;
        }
        Require(actor,"Spawn prospective class is not derived from Engine.Actor");
        Defaults result; result.path=path;
        bool radius{},height{};
        // UE property storage starts at zero; absent serialized bool tags retain
        // false. Radius/height MUST occur in the actual authored ancestor chain.
        for (auto i=chain.rbegin(); i!=chain.rend(); ++i) for (const auto& property : i->defaults) {
            const auto name=property.name.ToString();
            if (name=="CollisionRadius") { result.radius=PropertyFloat(property); radius=true; }
            if (name=="CollisionHeight") { result.height=PropertyFloat(property); height=true; }
            if (name=="bCollideWorld" || name=="bCollideWhenPlacing") {
                Require(property.type==3u && property.arrayIndex==0u,"Collision flag default is not scalar bool");
                (name=="bCollideWorld" ? result.world : result.placing)=property.boolValue;
            }
        }
        Require(radius && height,"Actual collision radius/height defaults were not found");
        std::cout << "class-default " << path << " radius=" << result.radius << " height=" << result.height
                  << " collideWorld=" << result.world << " collideWhenPlacing=" << result.placing
                  << " ancestors=" << chain.size() << '\n';
        return result;
    }
private:
    const PortablePackageTables& Package(const std::string& name) {
        const auto found=packages.find(name);
        if (found!=packages.end()) return found->second;
        Require(packages.size()<16u,"Collision default package cache exceeds bounded test scope");
        return packages.emplace(name,LoadPortablePackageTables((root/"System"/(name+".u")).string())).first->second;
    }
    std::filesystem::path root;
    std::map<std::string,PortablePackageTables> packages;
};
std::vector<Vec> MapPositions(const PortablePackageTables& package, const PortableModelGeometry& model,
                              std::size_t& authoredCount, bool& pointBounds) {
    std::vector<Vec> positions;
    // Exact ULevel actor order, not all arbitrary exports/brush Model objects.
    for (const auto reference : ReadPortableLevel68ActorOrder(package)) {
        if (reference<=0) continue;
        for (const auto& property : LoadPortableExportProperties(package,static_cast<std::size_t>(reference-1)).properties) {
            if (property.name.ToString()!="Location") continue;
            Require(property.type==10u && property.structName.ToString()=="Vector" &&
                    property.value.size()==12u && property.arrayIndex==0u,"Authored actor Location is not a Vector");
            Vec position; std::memcpy(&position,property.value.data(),sizeof(position));
            Require(std::isfinite(position.x) && std::isfinite(position.y) && std::isfinite(position.z),
                    "Authored actor position is nonfinite");
            positions.push_back(position);
            break;
        }
        if (positions.size()==64u) break;
    }
    authoredCount=positions.size();
    Require(authoredCount>=16u,"Original map has insufficient explicit authored actor positions");
    Vec low=model.bounds.minimum, high=model.bounds.maximum;
    pointBounds=!model.bounds.valid;
    if (pointBounds) {
        // Real Training's UPrimitive bbox has IsValid=false. Do not invent its
        // flag or select a brush Model. Root Model's authored points provide
        // sample bounds only; they NEVER enter either collision oracle.
        Require(!model.points.empty(),"Invalid primitive bbox has no root Model points for sampling");
        low=high=model.points.front();
        for (const auto& point : model.points) {
            Require(std::isfinite(point.x) && std::isfinite(point.y) && std::isfinite(point.z),
                    "Root Model sample point is nonfinite");
            low.x=std::min(low.x,point.x); low.y=std::min(low.y,point.y); low.z=std::min(low.z,point.z);
            high.x=std::max(high.x,point.x); high.y=std::max(high.y,point.y); high.z=std::max(high.z,point.z);
        }
    }
    Require(std::isfinite(low.x) && std::isfinite(low.y) && std::isfinite(low.z) &&
            std::isfinite(high.x) && std::isfinite(high.y) && std::isfinite(high.z) &&
            low.x<=high.x && low.y<=high.y && low.z<=high.z,"Root Model sample bbox is invalid");
    const float minimum[]{low.x,low.y,low.z};
    const float maximum[]{high.x,high.y,high.z};
    for (float z : {0.0f,0.5f,1.0f}) for (float y : {0.0f,0.5f,1.0f}) for (float x : {0.0f,0.5f,1.0f})
        positions.push_back({minimum[0]+(maximum[0]-minimum[0])*x,
                             minimum[1]+(maximum[1]-minimum[1])*y,
                             minimum[2]+(maximum[2]-minimum[2])*z});
    return positions;
}
void Original(const std::filesystem::path& root) {
    Classes classes(root);
    std::vector<Defaults> defaults;
    for (const std::string path : {"DeusEx.WeaponPistol","DeusEx.Ammo10mm","DeusEx.Doctor","Engine.PlayerStart"})
        defaults.push_back(classes.Read(path));
    std::size_t found{},exhausted{},shifted{};
    for (const std::string mapName : {"00_Training","01_NYC_UNATCOIsland","00_Intro"}) {
        const auto path=root/"Maps"/(mapName+".dx");
        const auto sizeBefore=std::filesystem::file_size(path);
        const auto writeBefore=std::filesystem::last_write_time(path);
        const auto package=LoadPortablePackageTables(path.string());
        const auto model=LoadPortableRootModel68(package);
        auto pinned=PreparePinned(model);
        std::size_t authored{}; bool pointBounds{};
        const auto positions=MapPositions(package,model,authored,pointBounds);
        for (const auto& item : defaults) for (const auto& position : positions) {
            Compare(model,pinned,position,item.radius,item.height,item.world,item.placing,
                    mapName+":"+item.path,true);
            const auto result=QuestVr::FindSpawnPlacement(model,position,item.radius,item.height,item.world,item.placing);
            if (!result.found) ++exhausted;
            else { ++found; if (!Same(result.location,position)) ++shifted; }
        }
        Require(std::filesystem::file_size(path)==sizeBefore && std::filesystem::last_write_time(path)==writeBefore,
                "Read-only original map size/write time changed during inspection");
        std::cout << "PASS original-map " << mapName << " root=" << model.objectPath
                  << " rootExport=" << model.exportIndex << " nodes=" << model.nodes.size()
                  << " hullWords=" << model.leafHulls.size() << " actorPositions=" << authored
                  << " bboxGrid=27 bboxSource=" << (pointBounds ? "root-model-points" : "valid-primitive-bbox")
                  << " prospectiveClasses=" << defaults.size() << '\n';
    }
    Require(found>0u && exhausted>0u && shifted>0u,"Original sample lacked success/exhaustion/shift outcome coverage");
    std::cout << "Original placement outcomes: found=" << found << " exhausted=" << exhausted
              << " shifted=" << shifted << ". Original files opened read-only; no gameplay/saves/device changes.\n";
}
}

int main(int argc,char** argv) {
    try {
        Require(argc<=2,"Usage: quest_spawn_original_test [original-game-root]");
        Synthetic();
        if (argc==2) Original(std::filesystem::path(argv[1]));
        std::cout << "PASS: " << checks << " checks, " << placementCases << " placement cases, "
                  << overlapCases << " all-candidate hull comparisons; "
                  << (argc==2 ? "original-map mode" : "synthetic-only mode")
                  << ". Pinned OverlapAABBModel compiled; CheckLocation uses audited harness. "
                  << "No native 278/UObject/lifecycle/stereo/performance equivalence claim.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
