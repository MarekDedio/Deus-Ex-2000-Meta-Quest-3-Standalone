#pragma once

#include "quest_brush_coords.h"
#include "quest_native_work.h"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <stdexcept>

namespace QuestVr {

// Detached providers for Core229's literal UnitCoords/table reads. They are
// borrowed read-only numeric services, not native GMath identity or script
// callback authority. Only actually consumed table slots are inspected.
struct OriginalGetAxesQueries {
    std::function<OriginalModelCoords()> unitCoords;
    std::function<float(std::uint32_t)> sineTable;
    std::function<void()> work;
    std::size_t maximumSteps{32u};
};
struct OriginalGetAxesStats {
    std::size_t steps{},unitReads{},tableReads{},compositions{};
};
struct OriginalGetAxesResult {
    PortableModelVec3 xAxis{},yAxis{},zAxis{};
    OriginalGetAxesStats stats;
};

namespace OriginalGetAxesDetail {
inline void Step(const OriginalGetAxesQueries& queries,OriginalGetAxesStats& stats) {
    if(stats.steps>=queries.maximumSteps)
        throw std::runtime_error("Original GetAxes finite native work bound exceeded");
    if(queries.work)queries.work();
    ++stats.steps;
}
inline std::uint32_t Index(std::int32_t angle,std::uint32_t offset=0u) {
    // Original ADD wraps32, then SAR2/AND3FFF. This unsigned equivalent also
    // handles INT_MIN/INT_MAX without negation, clamp or signed-overflow UB.
    return ((std::bit_cast<std::uint32_t>(angle)+offset)>>2u)&0x3fffu;
}
inline float Read(const OriginalGetAxesQueries& queries,OriginalGetAxesStats& stats,
    std::int32_t angle,std::uint32_t offset=0u) {
    if(!queries.sineTable)throw std::invalid_argument("Original GetAxes lacks its reached sine-table provider");
    Step(queries,stats);++stats.tableReads;
    const auto value=queries.sineTable(Index(angle,offset));
    static_cast<void>(ModelOwnerCoordsDetail::Load(value));return value;
}
inline float Negative(float value) {
    // Native FLD/FCHS/FSTP Float on an already stored finite binary32 value.
    // Toggle the sign bit, including signed zero; no cardinal correction.
    return std::bit_cast<float>(std::bit_cast<std::uint32_t>(value)^0x80000000u);
}
inline OriginalModelCoords Compose(const OriginalGetAxesQueries& queries,OriginalGetAxesStats& stats,
    OriginalModelCoords value,const OriginalModelCoords& next) {
    Step(queries,stats);++stats.compositions;
    return ComposeOriginalModelCoords(value,next);
}
} // namespace OriginalGetAxesDetail

// Exact scalar/storage schedule of Core10139E90..1013A268: copy UnitCoords,
// compose inverse ROLL, then PITCH, then YAW. It is NOT forward Rotate(A),
// Rotate(-A), a transpose/closed-form Euler formula, or a normalized vector.
// No public output mutation occurs until the detached result is returned.
inline OriginalGetAxesResult ComputeOriginalGetAxes(OriginalBrushRotation rotation,const OriginalGetAxesQueries& queries) {
    using namespace OriginalGetAxesDetail;
    NativeWork::ValidateLimit(queries.maximumSteps);
    OriginalGetAxesResult result;
    if(!queries.unitCoords)throw std::invalid_argument("Original GetAxes lacks its reached UnitCoords provider");
    Step(queries,result.stats);++result.stats.unitReads;
    auto coords=queries.unitCoords();
    // These intentionally separate reads preserve the original load order,
    // rather than cache one sin/cos pair across matrix construction.
    const auto negativeRollSin=Negative(Read(queries,result.stats,rotation.roll));
    const auto rollCosZ=Read(queries,result.stats,rotation.roll,0x4000u);
    const auto rollCosY=Read(queries,result.stats,rotation.roll,0x4000u);
    const auto rollSinY=Read(queries,result.stats,rotation.roll);
    coords=Compose(queries,result.stats,coords,{{0,0,0},{1,0,0},{0,rollCosY,rollSinY},{0,negativeRollSin,rollCosZ}});
    const auto pitchSinZ=Read(queries,result.stats,rotation.pitch);
    const auto pitchCosZ=Read(queries,result.stats,rotation.pitch,0x4000u);
    const auto pitchCosX=Read(queries,result.stats,rotation.pitch,0x4000u);
    const auto negativePitchSin=Negative(Read(queries,result.stats,rotation.pitch));
    coords=Compose(queries,result.stats,coords,{{0,0,0},{pitchCosX,0,negativePitchSin},{0,1,0},{pitchSinZ,0,pitchCosZ}});
    const auto yawCosX=Read(queries,result.stats,rotation.yaw,0x4000u);
    const auto yawCosY=Read(queries,result.stats,rotation.yaw,0x4000u);
    const auto yawSin=Read(queries,result.stats,rotation.yaw);
    coords=Compose(queries,result.stats,coords,{{0,0,0},{yawCosX,Negative(yawSin),0},{yawSin,yawCosY,0},{0,0,1}});
    result.xAxis=coords.xAxis;result.yAxis=coords.yAxis;result.zAxis=coords.zAxis;return result;
}

// Value convenience only. The caller explicitly chooses canonical UnitCoords
// and the audited table profile; this overload invents no native GMath service.
inline OriginalGetAxesResult ComputeOriginalGetAxes(OriginalBrushRotation rotation,const OriginalModelCoords& unitCoords,
    OriginalBrushSinTable table,const std::function<void()>& work={}) {
    OriginalGetAxesQueries queries;queries.unitCoords=[&]{return unitCoords;};
    queries.sineTable=[&](std::uint32_t index){return table[index];};queries.work=work;
    return ComputeOriginalGetAxes(rotation,queries);
}
} // namespace QuestVr
