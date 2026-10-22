#pragma once

#include "portable_model_geometry.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>

namespace QuestVr {
namespace ModelLineScratchDetail {
struct ExtentVisit {std::int32_t node,parent;bool outside;};
// An acyclic binary DFS has at most one deferred sibling for each ancestor,
// plus the current leaf. Shared DAG children repeat visits, not path depth.
inline std::size_t ExtentPendingCapacity(std::size_t nodes,std::size_t limit) {
    if(nodes==std::numeric_limits<std::size_t>::max())
        throw std::runtime_error("Model extent pending byte overflow");
    return std::min(nodes+1u,limit);
}
} // namespace ModelLineScratchDetail
// Peak envelope for Model line kernels. Structural validation dies before
// traversal; point and extent traversals are mutually exclusive. Point graph
// validation still includes vector growth's overlap, but point traversal uses
// one fixed Segment per node. Extent validation uses one packed
// uint32 frame and one color byte per node, and traversal one fixed Visit array.
// Fixed headroom covers hull/plane scratch alongside those arrays. The host
// must compose this with its other live scopes, not release it before querying.
inline std::size_t OriginalModelLineScratchBytes(std::size_t nodes,bool point) {
    struct ValidationFrame {std::size_t node;unsigned next;};
    struct PointSegment {std::int32_t node,last;PortableModelVec3 start,end;bool outside;};
    constexpr std::size_t headroom=2048u;
    const auto validation=point?3u*sizeof(ValidationFrame)+sizeof(std::uint8_t):
        sizeof(std::uint32_t)+sizeof(std::uint8_t);
    const auto traversal=point?sizeof(PointSegment):sizeof(ModelLineScratchDetail::ExtentVisit);
    const auto width=std::max(validation,traversal);
    const auto extra=point?0u:sizeof(ModelLineScratchDetail::ExtentVisit);
    if(nodes>(std::numeric_limits<std::size_t>::max()-headroom-extra)/width)
        throw std::runtime_error("Model line scratch byte overflow");
    return headroom+extra+nodes*width;
}
} // namespace QuestVr
