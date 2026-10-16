#include "quest_brush_coords.h"
#include "quest_model_line_hit.h"

#include <array>
#include <bit>
#include <iostream>
#include <limits>

// Wiring/integration controls with EXPLICIT test table values. These are not
// evidence for native LUT initialization, live Actor fields or class identity;
// independent arithmetic/rotation oracles live in the two separate tests.
namespace {
using Vec=PortableModelVec3;
using Coords=QuestVr::OriginalModelCoords;
using Scale=QuestVr::OriginalBrushScale;
using Rotation=QuestVr::OriginalBrushRotation;
std::size_t checks{},refusals{};
void Require(bool value,const char* reason) {++checks;if(!value) throw std::runtime_error(reason);}
bool Same(float a,float b) {return std::bit_cast<std::uint32_t>(a)==std::bit_cast<std::uint32_t>(b);}
bool Same(Vec a,Vec b) {return Same(a.x,b.x)&&Same(a.y,b.y)&&Same(a.z,b.z);}
bool Same(const Coords& a,const Coords& b) {
    return Same(a.origin,b.origin)&&Same(a.xAxis,b.xAxis)&&Same(a.yAxis,b.yAxis)&&Same(a.zAxis,b.zAxis);
}
template<class F> void Refuses(F operation,const char* reason) {
    bool refused{};try {operation();} catch(const std::exception&) {refused=true;}
    Require(refused,reason);++refusals;
}
std::array<float,16384> Fixture() {
    std::array<float,16384> table;
    table.fill(std::numeric_limits<float>::quiet_NaN());
    table[0]=0;table[4096]=1;table[8192]=0;table[12288]=-1;return table;
}
void ClosedForm() {
    const auto table=Fixture();
    const Scale first{{2,3,4},0,0},second{{4,3,2},0,0};
    auto result=QuestVr::BuildOriginalBrushCoords({8,18,32},{0,0,0},{1,2,3},first,second,table);
    Require(Same(result.origin,{0,0,-1})&&Same(result.xAxis,{8,0,0})&&Same(result.yAxis,{0,9,0})&&Same(result.zAxis,{0,0,8}),
        "Both scale blocks or final PrePivot are absent from closed-form brush transform");
    result=QuestVr::BuildOriginalBrushCoords({8,18,32},{0,16384,0},{1,2,3},first,second,table);
    Require(Same(result.origin,{-0.5f,3.33333349f,-1})&&Same(result.xAxis,{0,-6,0})&&
        Same(result.yAxis,{12,0,0})&&Same(result.zAxis,{0,0,8}),"Brush scale-rotation-scale order collapsed or transposed");
    const auto actor=QuestVr::BuildOriginalActorCoords({8,18,32},{0,16384,0},table);
    Require(Same(actor.origin,{-18,8,-32})&&Same(actor.xAxis,{0,-1,0})&&Same(actor.yAxis,{1,0,0})&&Same(actor.zAxis,{0,0,1}),
        "Generic Actor acquired brush-specific scale or pivot behavior");
    const auto swapped=QuestVr::BuildOriginalBrushCoords({8,18,32},{0,16384,0},{1,2,3},second,first,table);
    Require(!Same(result,swapped),"Noncommuting scale340/scale32C fixture cannot distinguish swapped native blocks");
}
void Chain() {
    const auto table=Fixture();const Vec location{13.125f,-2.5f,9},pivot{2,-4,1};
    const std::array<Scale,4> scales{{{{2,0.5f,-1},0.6f,1},{{0.25f,-2,3},-0.4f,4},
        {{-1,2,0.75f},1.25f,6},{{1,1,1},std::numeric_limits<float>::quiet_NaN(),255}}};
    for(const auto& first:scales) for(const auto& second:scales) {
        const auto actual=QuestVr::BuildOriginalBrushCoords(location,{0,16384,0},pivot,first,second,table);
        auto expected=QuestVr::IdentityOriginalModelCoords();expected.origin={-location.x,-location.y,-location.z};
        expected=QuestVr::ScaleOriginalModelCoords(expected,first);
        expected=QuestVr::ComposeOriginalModelCoords(expected,{{},{0,1,0},{-1,0,0},{0,0,1}});
        expected=QuestVr::ComposeOriginalModelCoords(expected,QuestVr::IdentityOriginalModelCoords());
        expected=QuestVr::ComposeOriginalModelCoords(expected,QuestVr::IdentityOriginalModelCoords());
        expected=QuestVr::ScaleOriginalModelCoords(expected,second);
        expected.origin={expected.origin.x+pivot.x,expected.origin.y+pivot.y,expected.origin.z+pivot.z};
        Require(Same(actual,expected),"Full brush wiring differs from verified staged components");
        const auto bounds=QuestVr::TransformOriginalModelBounds({-10,-5,-2},{0,5,2},actual);
        Require(bounds.isValid&&bounds.minimum.x<=bounds.maximum.x&&bounds.minimum.y<=bounds.maximum.y&&bounds.minimum.z<=bounds.maximum.z,
            "Built brush coordinates did not feed eight-corner registration bounds");
    }
    const float nan=std::numeric_limits<float>::quiet_NaN();const Scale unit;
    Refuses([&] {QuestVr::BuildOriginalBrushCoords({},{},{nan,0,0},unit,unit,table);},"Active PrePivot was ignored");
    Refuses([&] {QuestVr::BuildOriginalBrushCoords({},{},{},{{0,1,1},0,0},unit,table);},"Initial brush scale division by zero was ignored");
    Refuses([&] {QuestVr::BuildOriginalBrushCoords({},{},{},unit,{{1,0,1},0,0},table);},"Final brush scale division by zero was ignored");
}
PortableModelGeometry Halfspace() {
    PortableModelGeometry m;m.rootOutside=1;PortableModelNode n;
    n.planeX=1;n.vertexCount=3;n.front=n.back=n.plane=n.renderBound=n.leaf0=n.leaf1=-1;n.collisionBound=0;
    m.nodes.push_back(n);m.leafHulls={0,-1};
    for(const float f:{-10.0f,-10.0f,-10.0f,0.0f,10.0f,10.0f}) m.leafHulls.push_back(std::bit_cast<std::int32_t>(f));
    return m;
}
void NarrowPhase() {
    const auto table=Fixture();const auto coords=QuestVr::BuildOriginalBrushCoords({10,0,0},{},{},{},{},table);
    const auto model=Halfspace();QuestVr::OriginalActorLineHit packet;packet.next=42;packet.item=17;
    Require(!QuestVr::CheckOriginalModelExtent(packet,model,77,78,&coords,{-10,0,0},{20,0,0},{1,1,1}),
        "Constructed brush coordinates failed real Model narrow-phase integration");
    Require(packet.actor==78&&packet.primitive==77&&packet.next==42&&packet.item==17&&Same(packet.normal,{1,0,0}),
        "Constructed brush sweep lost native identity/packet fields");
    Require(Same(packet.location,{14,0,0}),"Brush extent sweep localized extents or applied world transform twice");
}
} // namespace
int main() {
    try {
        ClosedForm();Chain();NarrowPhase();
        std::cout<<"PASS original brush builder wiring: "<<checks<<" checks, "<<refusals
            <<" explicit refusals; supplied fixture table, both native scales, PrePivot, bounds and Model hit integration. No runtime/physics claim.\n";
        return 0;
    } catch(const std::exception& error) {std::cerr<<"Original brush builder failed: "<<error.what()<<'\n';return 1;}
}
