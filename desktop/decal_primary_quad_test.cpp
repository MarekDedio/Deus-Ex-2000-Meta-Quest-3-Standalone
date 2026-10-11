#include "quest_decal_primary_quad.h"
#include "quest_decal_secondary_quad.h"

#include <cmath>
#include <cstdint>
#include <functional>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

namespace {
using Vec=PortableModelVec3;
using Path=QuestVr::DecalPrimaryAxisPath;
std::size_t checks{},refusals{};
void Require(bool condition,const std::string& message) {
    if (!condition) throw std::runtime_error(message);
    ++checks;
}
void Near(float actual,float expected,const std::string& message,float tolerance=0.000003f) {
    Require(std::isfinite(actual) && std::abs(actual-expected)<=tolerance,message);
}
void Same(Vec a,Vec b,const std::string& message,float tolerance=0.000003f) {
    Near(a.x,b.x,message+" X",tolerance); Near(a.y,b.y,message+" Y",tolerance); Near(a.z,b.z,message+" Z",tolerance);
}
void Refuse(const std::function<void()>& fn,const std::string& message) {
    bool refused{};
    try { fn(); } catch (const std::runtime_error& error) {
        refused=std::string(error.what()).rfind("Original decal primary quad: ",0)==0;
    }
    Require(refused,message); ++refusals;
}
QuestVr::DecalPreparedDirection Supplied(Vec d) {
    std::uint32_t seed=317u;
    const auto result=QuestVr::PrepareOriginalDecalDirection(d,seed);
    Require(seed==317u && !result.randomized && !result.draws && !result.attempts,"Supplied direction used RNG");
    return result;
}
void KnownGeometry() {
    const auto d=Supplied({1,0,0});
    const auto quad=QuestVr::BuildOriginalDecalPrimaryQuad({10,20,30},{2,3,30},{0,0,1},32,.5f,d);
    Require(quad.axisPath==Path::SuppliedCombined,"Supplied tangent did not take combined45-degree axes");
    Near(quad.radius,std::sqrt(128.0f),"USize-only radius");
    const std::array<Vec,4> expected{{{18,12,30},{18,28,30},{2,28,30},{2,12,30}}};
    const std::array<Vec,4> relative{{{16,9,0},{16,25,0},{0,25,0},{0,9,0}}};
    for (std::size_t i=0;i<4;++i) { Same(quad.worldCorners[i],expected[i],"Ordered world corner"); Same(quad.relativeCorners[i],relative[i],"Base-relative corner"); }
    const auto negative=QuestVr::BuildOriginalDecalPrimaryQuad({10,20,30},{2,3,30},{0,0,1},32,-.5f,d);
    for (std::size_t i=0;i<4;++i) Same(negative.worldCorners[i],expected[i],"Negative DrawScale changed geometric winding");
    const auto negativeSize=QuestVr::BuildOriginalDecalPrimaryQuad({10,20,30},{2,3,30},{0,0,1},-32,.5f,d);
    for (std::size_t i=0;i<4;++i) Same(negativeSize.worldCorners[i],expected[i],"Signed texture USize radius changed");
    const auto reverse=QuestVr::BuildOriginalDecalPrimaryQuad({10,20,30},{2,3,30},{0,0,-1},32,.5f,d);
    Same(reverse.worldCorners[0],expected[1],"Normal flip axis0"); Same(reverse.worldCorners[1],expected[0],"Normal flip axis1");
    const auto diagonal=QuestVr::BuildOriginalDecalPrimaryQuad({},{},{0,0,1},32,.5f,Supplied({.1f,.1f,0}));
    Same(diagonal.worldCorners[0],{std::sqrt(128.0f),0,0},"Shadow.Tick supplied tangent axis0");
    Same(diagonal.worldCorners[1],{0,std::sqrt(128.0f),0},"Shadow.Tick supplied tangent axis1");
    // Project against the authored normal, not an invented normalized normal:
    // with N=(0,0,2), D=(1,0,1), P=(1,0,-3), not the horizontal tangent.
    const auto nonunit=QuestVr::BuildOriginalDecalPrimaryQuad({},{},{0,0,2},32,.5f,Supplied({1,0,1}));
    Require(nonunit.worldCorners[0].z<0 && nonunit.worldCorners[1].z<0,"Authored nonunit normal was silently normalized");
    for (const auto& corner:nonunit.worldCorners) {
        const double length=std::sqrt(static_cast<double>(corner.x)*corner.x+static_cast<double>(corner.y)*corner.y+static_cast<double>(corner.z)*corner.z);
        Require(std::abs(length-nonunit.radius)<.000003,"Nonunit axes did not use common normalization");
    }
}
void DegenerateBranches() {
    const auto fallback=QuestVr::BuildOriginalDecalPrimaryQuad({2,0,0},{0,0,0},{0,0,1},32,.5f,Supplied({0,0,1}));
    const float r=std::sqrt(128.0f);
    Require(fallback.axisPath==Path::SurfaceBaseFallback,"Parallel supplied direction did not use base-center fallback");
    Same(fallback.worldCorners[0],{2-r,0,0},"Fallback direct axis0"); Same(fallback.worldCorners[1],{2,r,0},"Fallback direct cross axis1");
    Same(fallback.worldCorners[2],{2+r,0,0},"Fallback opposite0"); Same(fallback.worldCorners[3],{2,-r,0},"Fallback opposite1");
    const auto collapse=QuestVr::BuildOriginalDecalPrimaryQuad({2,3,0},{2,3,0},{0,0,1},32,.5f,Supplied({0,0,1}));
    for (const auto& corner:collapse.worldCorners) Same(corner,{2,3,0},"Degenerate quad gained invented tangent");
    for (const auto& corner:collapse.relativeCorners) Same(corner,{},"Degenerate relative quad");
    const float threshold=0.0001f;
    Require(static_cast<double>(threshold)<0.0001,"Expected threshold float is not just below native DOUBLE constant");
    const auto below=QuestVr::BuildOriginalDecalPrimaryQuad({2,0,0},{0,0,0},{0,0,1},32,.5f,Supplied({threshold,0,1}));
    const auto above=QuestVr::BuildOriginalDecalPrimaryQuad({2,0,0},{0,0,0},{0,0,1},32,.5f,
        Supplied({std::nextafter(threshold,std::numeric_limits<float>::infinity()),0,1}));
    Require(below.axisPath==Path::SurfaceBaseFallback && above.axisPath==Path::SuppliedCombined,"Adjacent binary32 tangent threshold changed branches");
    for (const int size:{0,32}) {
        const auto zero=QuestVr::BuildOriginalDecalPrimaryQuad({2,3,4},{1,1,1},{0,0,1},size,0,Supplied({1,0,0}));
        Near(zero.radius,0,"Zero DrawScale radius");
        for (const auto& corner:zero.worldCorners) Same(corner,{2,3,4},"Zero-radius native quad");
    }
    const auto zeroNormal=QuestVr::BuildOriginalDecalPrimaryQuad({2,3,4},{1,1,1},{},32,.5f,Supplied({1,0,0}));
    // Quad construction alone is not the earlier X dot N guard in Attach.
    Require(zeroNormal.axisPath==Path::SuppliedCombined,"Quad helper inserted native Attach surface acceptance");
    Same(zeroNormal.worldCorners[0],{2+r,3,4},"Zero normal degenerate axis0");
    Same(zeroNormal.worldCorners[1],{2+r,3,4},"Zero normal degenerate axis1");
}
void RandomDirection() {
    std::uint32_t seed=1u;
    const auto d=QuestVr::PrepareOriginalDecalDirection({},seed);
    Require(d.randomized && d.attempts==2 && d.draws==6,"Seed1 rejection sampling consumption differs");
    // Independent original MSVCRT first6 draws:41,18467,6334(reject),26500,
    //19169,15724(accept). Pin seed without drawing through the implementation.
    Require(seed==1030492215u,"Seed1 final MSVCRT state differs");
    const auto sample=[](std::uint32_t value) { return static_cast<float>(value*0x1.0002p-15*2.0-1.0); };
    const Vec accepted{sample(26500),sample(19169),sample(15724)};
    const double length=std::sqrt(static_cast<double>(accepted.x)*accepted.x+
        static_cast<double>(accepted.y)*accepted.y+static_cast<double>(accepted.z)*accepted.z);
    Same(d.direction,{static_cast<float>(accepted.x/length),static_cast<float>(accepted.y/length),static_cast<float>(accepted.z/length)},"Known accepted unsafe normal",0);
    const auto quad=QuestVr::BuildOriginalDecalPrimaryQuad({},{},{0,0,1},32,.5f,d);
    Require(quad.axisPath==Path::RandomDirect,"Random direction incorrectly entered supplied45-degree combination");
    // Direct branch keeps the direction's projected angle, no additional45deg.
    const double tangentLength=std::sqrt(static_cast<double>(d.direction.x)*d.direction.x+
        static_cast<double>(d.direction.y)*d.direction.y);
    Near(quad.worldCorners[0].x,static_cast<float>(d.direction.x/tangentLength*quad.radius),"Random direct X");
    Near(quad.worldCorners[0].y,static_cast<float>(d.direction.y/tangentLength*quad.radius),"Random direct Y");
    Near(quad.worldCorners[0].z,0,"Random projection Z");
    // Stored Script FRand is not the unrounded native appFrand register used
    // here. This draw demonstrably distinguishes them after*2-1 FLOATstore.
    const float unrounded=sample(19169), rounded=(19169*static_cast<float>(0x1.0002p-15))*2.0f-1.0f;
    Require(unrounded!=rounded,"Random test does not distinguish native appFrand from script FLOAT FRand");
    for (std::uint32_t initial=0;initial<1024;++initial) {
        auto a=initial,b=initial;
        const auto first=QuestVr::PrepareOriginalDecalDirection({},a),repeat=QuestVr::PrepareOriginalDecalDirection({-0.0f,0,-0.0f},b);
        Require(a==b && first.draws==repeat.draws && first.attempts>0 && first.draws==first.attempts*3,"Random sampling not deterministic/three-draw bounded");
        Same(first.direction,repeat.direction,"Signed-zero optional vector",0);
        const double square=static_cast<double>(first.direction.x)*first.direction.x+
            static_cast<double>(first.direction.y)*first.direction.y+static_cast<double>(first.direction.z)*first.direction.z;
        Require(std::abs(square-1.0)<.0000002,"Accepted random vector not unsafe-normalized");
    }
}
void ValidationAndAtomicSeed() {
    const float infinity=std::numeric_limits<float>::infinity(),nan=std::numeric_limits<float>::quiet_NaN();
    std::uint32_t seed=1u;
    Refuse([&] { QuestVr::PrepareOriginalDecalDirection({},seed,{1}); },"Rejection bound did not fail");
    Require(seed==1,"Rejection failure advanced caller RNG");
    for (const std::size_t limit:{0u,4097u}) {
        Refuse([&] { QuestVr::PrepareOriginalDecalDirection({1,0,0},seed,{limit}); },"Invalid direction limits accepted");
        Require(seed==1,"Invalid direction limit changed seed");
    }
    for (const Vec bad:std::array<Vec,4>{{{infinity,0,0},{0,nan,0},{0,0,-infinity},{nan,nan,nan}}}) {
        Refuse([&] { QuestVr::PrepareOriginalDecalDirection(bad,seed); },"Nonfinite direction accepted");
        Require(seed==1,"Nonfinite direction changed seed");
    }
    const auto d=Supplied({1,0,0});
    Refuse([&] { QuestVr::BuildOriginalDecalPrimaryQuad({nan,0,0},{},{0,0,1},32,1,d); },"Nonfinite center");
    Refuse([&] { QuestVr::BuildOriginalDecalPrimaryQuad({},{infinity,0,0},{0,0,1},32,1,d); },"Nonfinite base");
    Refuse([&] { QuestVr::BuildOriginalDecalPrimaryQuad({},{},{0,0,nan},32,1,d); },"Nonfinite normal");
    Refuse([&] { QuestVr::BuildOriginalDecalPrimaryQuad({},{},{0,0,1},32,infinity,d); },"Nonfinite scale");
    Refuse([&] { QuestVr::BuildOriginalDecalPrimaryQuad({},{},{0,0,1},INT32_MAX,std::numeric_limits<float>::max(),d); },"Radius overflow");
    Refuse([&] { QuestVr::BuildOriginalDecalPrimaryQuad({},{},{0,0,1},32,1,{{nan,0,0},true,0,0}); },"Malformed prepared direction");
    Refuse([&] { QuestVr::BuildOriginalDecalPrimaryQuad({std::numeric_limits<float>::max(),0,0},
        {-std::numeric_limits<float>::max(),0,0},{0,0,1},32,1,Supplied({0,0,1})); },"Base-center subtraction overflow");
}
void SecondaryGridAndAngles() {
    const std::array<Vec,4> corners{{{0,0,0},{8,0,0},{8,8,0},{0,8,0}}};
    for (const int level:{INT32_MIN,-1,0,1,2,3,4,5,INT32_MAX}) {
        const auto grid=QuestVr::BuildOriginalDecalSecondaryGrid(corners,{0,0,1},level);
        const int clamped=std::min(level,4),count=std::max(clamped,0);
        Require(QuestVr::ClampOriginalDecalMultiLevel(level)==clamped,"Multilevel lower-clamped or did not upper-clamp4");
        Require(grid.size()==static_cast<std::size_t>(count*count),"Secondary grid wrong sample count");
        for (std::size_t k=0;k<grid.size();++k) {
            const auto& sample=grid[k];
            const int i=static_cast<int>(k)/count,j=static_cast<int>(k)%count;
            Require(sample.outer==i && sample.inner==j,"Secondary grid changed outer/inner order");
            Same(sample.point,{8.0f*(i+1)/count,8.0f*(j+1)/count,0},"Secondary sample includes wrong fraction");
            Same(sample.start,{sample.point.x,sample.point.y,16},"Secondary trace start above floor");
            Same(sample.end,{sample.point.x,sample.point.y,-16},"Secondary trace end below floor");
        }
        if (!grid.empty()) Same(grid.back().point,{8,8,0},"Secondary grid does not include far corner");
    }
    const auto ordinary=QuestVr::BuildOriginalDecalSecondaryGrid(corners,{0,0,1},4),
        scaled=QuestVr::BuildOriginalDecalSecondaryGrid(corners,{0,0,7},4),
        reverse=QuestVr::BuildOriginalDecalSecondaryGrid(corners,{0,0,-1},4);
    for (std::size_t i=0;i<ordinary.size();++i) {
        Same(scaled[i].start,ordinary[i].start,"Grid offset did not UnsafeNormal authored normal");
        Same(reverse[i].start,ordinary[i].end,"Reverse normal grid start");
        Same(reverse[i].end,ordinary[i].start,"Reverse normal grid end");
    }
    const auto aligned=QuestVr::CheckOriginalDecalSecondaryAngle({0,0,1},{0,0,3}),
        opposite=QuestVr::CheckOriginalDecalSecondaryAngle({0,0,1},{0,0,-3}),
        perpendicular=QuestVr::CheckOriginalDecalSecondaryAngle({0,0,1},{1,0,0});
    Require(aligned.accepted && opposite.accepted && !perpendicular.accepted,"Secondary angle used facing-only or accepted perpendicular");
    Near(aligned.signedCosine,1,"Aligned signed cosine"); Near(opposite.signedCosine,-1,"Opposite signed cosine");
    const auto slope=QuestVr::CheckOriginalDecalSecondaryAngle({0,0,2},{.6f,0,.8f}),
        steep=QuestVr::CheckOriginalDecalSecondaryAngle({0,0,1},{.8f,0,.6f});
    Require(slope.accepted && !steep.accepted,"Secondary angle0.7 rejection changed");
    Near(slope.signedCosine,.8f,"Sloped angle does not normalize length");
    // Unit-ish vector with z=.7 demonstrates retained-register acceptance:
    // the stored FLOAT cosine rounds to.7, but unrounded cosine is slightly
    // greater than DOUBLE.7 (its X input is also a float). No epsilon is used.
    const auto boundary=QuestVr::CheckOriginalDecalSecondaryAngle({0,0,1},{std::sqrt(.51f),0,.7f});
    const double x=std::sqrt(.51f),z=.7f,expected=z/std::sqrt(x*x+z*z);
    Require(boundary.accepted==(std::abs(expected)>.7),"Secondary acceptance rounded signed cosine before testing");
    for (const auto invalid:std::array<Vec,3>{{{},{std::numeric_limits<float>::infinity(),0,0},{0,0,std::numeric_limits<float>::quiet_NaN()}}}) {
        Refuse([&] { QuestVr::BuildOriginalDecalSecondaryGrid(corners,invalid,4); },"Invalid grid normal accepted");
        Refuse([&] { QuestVr::CheckOriginalDecalSecondaryAngle({0,0,1},invalid); },"Invalid secondary angle accepted");
    }
}
void SecondaryCornerCorrection() {
    const std::array<Vec,4> corners{{{1,1,0},{2,1,0},{2,2,0},{1,2,0}}};
    const auto flat=QuestVr::BuildOriginalDecalSecondaryQuad(corners,{0,0,1},{0,0,0},{0,0,1},1);
    for (std::size_t i=0;i<4;++i) Same(flat.worldCorners[i],corners[i],"Parallel-plane secondary corner changed");
    const auto raised=QuestVr::BuildOriginalDecalSecondaryQuad(corners,{0,0,1},{0,0,5},{0,0,1},1);
    for (std::size_t i=0;i<4;++i) {
        Same(raised.worldCorners[i],{corners[i].x,corners[i].y,5},"Shifted parallel plane reprojection");
        Same(raised.relativeCorners[i],corners[i],"Secondary subtracts selected surface base");
    }
    const auto angle=QuestVr::CheckOriginalDecalSecondaryAngle({0,0,1},{.6f,0,.8f}),
        reverseAngle=QuestVr::CheckOriginalDecalSecondaryAngle({0,0,1},{-.6f,0,-.8f});
    const auto slope=QuestVr::BuildOriginalDecalSecondaryQuad(corners,{0,0,1},{},{.6f,0,.8f},angle.signedCosine),
        reverse=QuestVr::BuildOriginalDecalSecondaryQuad(corners,{0,0,1},{},{-.6f,0,-.8f},reverseAngle.signedCosine);
    // Hand-computed unit-normal geometry: R=(x,y,-.75x), S=(.64x,y,-.48x),
    // a=.75|x|, b=.9375|x|, D=(-.8sign(x),0,.6sign(x)). Signed negative c
    // reverses b-a; do not replace with abs(c) or a simple S projection.
    for (std::size_t i=0;i<4;++i) {
        const float x=corners[i].x,y=corners[i].y;
        Same(slope.worldCorners[i],{.79f*x,y,-.5925f*x},"Original two-line sloped correction",.000002f);
        Same(reverse.worldCorners[i],{.49f*x,y,-.3675f*x},"Original signed-cosine reverse correction",.000002f);
        Require(std::abs(slope.worldCorners[i].x-.64f*x)>.1,"Sloped correction silently replaced by simple projection");
    }
    const std::array<Vec,4> negativeCorners{{{-1,1,0},{-2,1,0},{-2,2,0},{-1,2,0}}};
    const auto negative=QuestVr::BuildOriginalDecalSecondaryQuad(negativeCorners,{0,0,1},{},{.6f,0,.8f},angle.signedCosine);
    for (std::size_t i=0;i<4;++i) Same(negative.worldCorners[i],{-.79f*corners[i].x,corners[i].y,.5925f*corners[i].x},"Negative sloped corner correction",.000002f);
    for (const float invalid:{0.0f,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()})
        Refuse([&] { QuestVr::BuildOriginalDecalSecondaryQuad(corners,{0,0,1},{},{.6f,0,.8f},invalid); },"Invalid signed cosine accepted");
    Refuse([&] { QuestVr::BuildOriginalDecalSecondaryQuad(corners,{0,0,1},{},{1,0,0},1); },"Singular secondary corner denominator accepted");
    auto bad=corners; bad[2].x=std::numeric_limits<float>::quiet_NaN();
    Refuse([&] { QuestVr::BuildOriginalDecalSecondaryQuad(bad,{0,0,1},{},{0,0,1},1); },"Nonfinite secondary corner accepted");
}
} // namespace
int main() {
    try {
        KnownGeometry(); DegenerateBranches(); RandomDirection(); ValidationAndAtomicSeed(); SecondaryGridAndAngles(); SecondaryCornerCorrection();
        std::cout<<"PASS original decal primary/secondary quad checks="<<checks<<" expected-refusals="<<refusals
            <<"; primary axes/radius/RNG, sixteen-sample grid, signed-angle/two-line correction; not Attach/render completion\n";
        return 0;
    } catch (const std::exception& error) { std::cerr<<"FAIL "<<error.what()<<'\n'; return 1; }
}
