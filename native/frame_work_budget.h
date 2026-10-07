#pragma once

#include <cmath>
#include <cstddef>
#include <limits>

namespace QuestVr {

// Bound triangle-list scanning/copying using subtraction only after validating
// each cursor. The result is a multiple of three so chunks never split a face.
// Zero can mean an exhausted frame/chunk or malformed internal cursor; callers
// should reject malformed cursors rather than retrying forever.
inline std::size_t TriangleWorkSlice(
    const std::size_t totalVertices, const std::size_t nextVertex,
    const std::size_t chunkVertices, const std::size_t frameVertices,
    const std::size_t vertexLimit) noexcept {
    if (nextVertex > totalVertices || chunkVertices > vertexLimit ||
        frameVertices > vertexLimit || totalVertices % 3u != 0u ||
        nextVertex % 3u != 0u || chunkVertices % 3u != 0u ||
        frameVertices % 3u != 0u) return 0u;
    std::size_t count = totalVertices - nextVertex;
    const std::size_t chunkRemaining = vertexLimit - chunkVertices;
    const std::size_t frameRemaining = vertexLimit - frameVertices;
    if (count > chunkRemaining) count = chunkRemaining;
    if (count > frameRemaining) count = frameRemaining;
    return count / 3u * 3u;
}

// Cooperative budget for incremental preparation/copy/upload work. The caller
// supplies milliseconds from a monotonic frame-local clock and must split large
// operations itself: this helper cannot interrupt an operation already started.
// One operation is always permitted for valid elapsed time, even with zero or
// exceeded limits, so pending work makes progress on every frame. Consequently
// these are between-operation limits, not a hard real-time/preemption guarantee.
class FrameWorkBudget {
public:
    FrameWorkBudget(std::size_t maxOperations, std::size_t maxVertices,
                    double maxMilliseconds) noexcept
        : maxOperations_(maxOperations), maxVertices_(maxVertices),
          maxMilliseconds_(std::isfinite(maxMilliseconds) && maxMilliseconds >= 0.0
                               ? maxMilliseconds
                               : 0.0) {}

    bool CanStart(std::size_t nextVertices, double elapsedMilliseconds) const noexcept {
        if (!ValidElapsed(elapsedMilliseconds)) return false;
        if (operations_ == 0) return true;
        if (ShouldYield(elapsedMilliseconds)) return false;
        // ShouldYield checked vertices_ < maxVertices_; subtraction cannot wrap.
        return nextVertices <= maxVertices_ - vertices_;
    }

    void Consume(std::size_t vertices) noexcept {
        operations_ = SaturatingAdd(operations_, 1);
        vertices_ = SaturatingAdd(vertices_, vertices);
    }

    bool ShouldYield(double elapsedMilliseconds) const noexcept {
        if (!ValidElapsed(elapsedMilliseconds)) return true;
        if (operations_ == 0) return false;
        return operations_ >= maxOperations_ || vertices_ >= maxVertices_ ||
               elapsedMilliseconds >= maxMilliseconds_;
    }

    void Reset() noexcept {
        operations_ = 0;
        vertices_ = 0;
    }

    std::size_t Operations() const noexcept { return operations_; }
    std::size_t Vertices() const noexcept { return vertices_; }

private:
    static bool ValidElapsed(double elapsedMilliseconds) noexcept {
        return std::isfinite(elapsedMilliseconds) && elapsedMilliseconds >= 0.0;
    }

    static std::size_t SaturatingAdd(std::size_t left, std::size_t right) noexcept {
        const std::size_t maximum = std::numeric_limits<std::size_t>::max();
        return right > maximum - left ? maximum : left + right;
    }

    std::size_t maxOperations_;
    std::size_t maxVertices_;
    double maxMilliseconds_;
    std::size_t operations_ = 0;
    std::size_t vertices_ = 0;
};

} // namespace QuestVr
