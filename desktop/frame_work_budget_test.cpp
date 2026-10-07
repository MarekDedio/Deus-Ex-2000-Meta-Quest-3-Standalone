#include "frame_work_budget.h"

#include <iostream>
#include <limits>
#include <stdexcept>

namespace {

void Require(bool value, const char* description) {
    if (!value) throw std::runtime_error(description);
}

void OperationCap() {
    QuestVr::FrameWorkBudget budget(2, 100, 3.0);
    Require(budget.CanStart(10, 0.0), "first operation was refused");
    budget.Consume(10);
    Require(budget.CanStart(10, 0.1), "operation below cap was refused");
    budget.Consume(10);
    Require(budget.Operations() == 2 && budget.Vertices() == 20,
            "consumed counts are incorrect");
    Require(budget.ShouldYield(0.2) && !budget.CanStart(0, 0.2),
            "operation cap did not stop zero-cost work");
}

void VertexCap() {
    QuestVr::FrameWorkBudget budget(8, 6144, 3.0);
    budget.Consume(3000);
    Require(budget.CanStart(3144, 0.1), "exact remaining vertex capacity was refused");
    Require(!budget.CanStart(3145, 0.1), "work larger than remaining capacity was allowed");
    budget.Consume(3144);
    Require(budget.ShouldYield(0.2) && !budget.CanStart(0, 0.2),
            "exact vertex cap did not yield");
}

void TimeCap() {
    QuestVr::FrameWorkBudget budget(8, 6144, 3.0);
    budget.Consume(3);
    Require(budget.CanStart(3, 2.999), "work before time cap was refused");
    Require(budget.ShouldYield(3.0) && !budget.CanStart(3, 3.0),
            "exact elapsed-time cap did not yield");
    Require(!budget.CanStart(3, 40.0), "expired time cap allowed work");
}

void FirstOperationProgress() {
    QuestVr::FrameWorkBudget budget(0, 0, 0.0);
    Require(!budget.ShouldYield(50.0) && budget.CanStart(9000, 50.0),
            "zero/exceeded caps prevented first-operation progress");
    budget.Consume(9000);
    Require(budget.ShouldYield(50.0) && !budget.CanStart(1, 50.0),
            "first-operation exception allowed a second operation");

    QuestVr::FrameWorkBudget oversized(8, 6144, 3.0);
    Require(oversized.CanStart(9000, 0.0), "oversized first operation was refused");
    oversized.Consume(9000);
    Require(!oversized.CanStart(1, 0.0), "oversized first operation wrapped capacity");
}

void CounterOverflow() {
    const auto maximum = std::numeric_limits<std::size_t>::max();
    QuestVr::FrameWorkBudget budget(maximum, maximum, 3.0);
    budget.Consume(maximum - 1);
    Require(!budget.CanStart(maximum, 0.0), "remaining-capacity addition overflowed");
    Require(budget.CanStart(1, 0.0), "last representable vertex was refused");
    budget.Consume(10);
    Require(budget.Vertices() == maximum && budget.Operations() == 2,
            "vertex consumption overflowed instead of saturating");
    budget.Consume(maximum);
    Require(budget.Vertices() == maximum && !budget.CanStart(0, 0.0),
            "further consumption resurrected an exhausted budget");
}

void ResetAndNewFrame() {
    QuestVr::FrameWorkBudget budget(1, 12, 3.0);
    budget.Consume(12);
    budget.Reset();
    Require(budget.Operations() == 0 && budget.Vertices() == 0,
            "reset retained consumed work");
    Require(budget.CanStart(12, 0.0), "reset did not restore first-operation progress");
    budget.Consume(12);
    Require(!budget.CanStart(1, 0.0), "reset changed the configured limits");

    QuestVr::FrameWorkBudget nextFrame(1, 12, 3.0);
    Require(nextFrame.CanStart(12, 0.0) && nextFrame.Operations() == 0,
            "new frame inherited another budget's consumed work");
}

void InvalidClock() {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double infinity = std::numeric_limits<double>::infinity();
    QuestVr::FrameWorkBudget budget(8, 6144, 3.0);
    for (double elapsed : {-1.0, nan, infinity}) {
        Require(!budget.CanStart(3, elapsed) && budget.ShouldYield(elapsed),
                "invalid elapsed time allowed unchecked work");
    }
    for (double limit : {-1.0, nan, infinity}) {
        QuestVr::FrameWorkBudget invalidLimit(8, 6144, limit);
        Require(invalidLimit.CanStart(3, 0.0), "invalid time limit prevented progress");
        invalidLimit.Consume(3);
        Require(invalidLimit.ShouldYield(0.0), "invalid time limit became unlimited");
    }
}

void TriangleSlices() {
    using QuestVr::TriangleWorkSlice;
    Require(TriangleWorkSlice(9000, 0, 0, 0, 6144) == 6144,
            "large triangle list was not capped");
    Require(TriangleWorkSlice(9000, 6144, 0, 0, 6144) == 2856,
            "tail triangle list was not completed");
    Require(TriangleWorkSlice(9000, 0, 6000, 0, 6144) == 144,
            "partial chunk capacity was ignored");
    Require(TriangleWorkSlice(9000, 0, 0, 6102, 6144) == 42,
            "remaining frame capacity was ignored");
    Require(TriangleWorkSlice(9, 0, 0, 0, 8) == 6,
            "non-aligned cap split a triangle");
    Require(TriangleWorkSlice(9000, 0, 6144, 0, 6144) == 0 &&
            TriangleWorkSlice(9000, 0, 0, 6144, 6144) == 0 &&
            TriangleWorkSlice(9000, 9000, 0, 0, 6144) == 0,
            "exhausted chunk/frame/mesh still supplied work");
    Require(TriangleWorkSlice(9, 12, 0, 0, 6144) == 0 &&
            TriangleWorkSlice(9, 0, 6147, 0, 6144) == 0 &&
            TriangleWorkSlice(9, 0, 0, 6147, 6144) == 0 &&
            TriangleWorkSlice(10, 0, 0, 0, 6144) == 0 &&
            TriangleWorkSlice(9, 1, 0, 0, 6144) == 0 &&
            TriangleWorkSlice(9, 0, 1, 0, 6144) == 0,
            "malformed cursors underflowed or split a triangle");
    const auto maximum = std::numeric_limits<std::size_t>::max();
    const auto alignedMaximum = maximum / 3u * 3u;
    Require(TriangleWorkSlice(alignedMaximum, alignedMaximum - 3u, 0, 0, maximum) == 3,
            "large cursor arithmetic overflowed");
}

// Real caller-like incremental slices: one frame's completed work cannot exceed
// the count/vertex caps, and cancellation means simply starting no further work.
void IncrementalSlicesAndCancellation() {
    constexpr std::size_t totalVertices = 6144 * 7 + 1536;
    constexpr std::size_t sliceVertices = 1536;
    std::size_t remaining = totalVertices;
    std::size_t completed = 0;
    std::size_t frames = 0;
    while (remaining != 0) {
        QuestVr::FrameWorkBudget budget(8, 6144, 3.0);
        double elapsed = 0.0;
        while (remaining != 0) {
            const std::size_t next = remaining < sliceVertices ? remaining : sliceVertices;
            if (!budget.CanStart(next, elapsed)) break;
            remaining -= next;
            completed += next;
            budget.Consume(next);
            elapsed += 0.25;
        }
        Require(budget.Operations() > 0 && budget.Operations() <= 8 &&
                    budget.Vertices() <= 6144,
                "incremental work failed progress or crossed caps");
        Require(completed + remaining == totalVertices, "incremental work lost vertices");
        ++frames;
    }
    Require(frames == 8 && completed == totalVertices,
            "incremental slices did not complete across fresh frame budgets");

    QuestVr::FrameWorkBudget cancelled(8, 6144, 3.0);
    cancelled.Consume(1536);
    const auto operationsAtCancel = cancelled.Operations();
    const auto verticesAtCancel = cancelled.Vertices();
    // Diagnostic reads cannot consume work or revive exhausted budgets.
    for (int i = 0; i < 100; ++i) {
        (void)cancelled.CanStart(1536, 0.25);
        (void)cancelled.ShouldYield(0.25);
    }
    Require(cancelled.Operations() == operationsAtCancel &&
                cancelled.Vertices() == verticesAtCancel,
            "budget queries consumed work after cancellation");
}

} // namespace

int main() {
    try {
        OperationCap();
        VertexCap();
        TimeCap();
        FirstOperationProgress();
        CounterOverflow();
        ResetAndNewFrame();
        InvalidClock();
        TriangleSlices();
        IncrementalSlicesAndCancellation();
        std::cout << "Frame work budget: 9 regression groups passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Frame work budget regression failed: " << error.what() << '\n';
        return 1;
    }
}
