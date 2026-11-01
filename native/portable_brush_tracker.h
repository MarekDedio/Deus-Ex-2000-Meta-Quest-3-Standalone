#pragma once
#include "portable_brush_setup.h"
#include "quest_brush_tracker_pools.h"
#include <string>
#include <vector>

struct PortableBrushTrackerMover {
    std::string actorPath;
    std::int32_t root{-1};
    std::size_t nodes{},vertices{};
};
struct PortableBrushTrackerProbe {
    QuestVr::Vm::Result execution;
    PortableBrushSetupProbe setup;
    QuestVr::OriginalBrushModelCounts original,finalCounts;
    std::vector<PortableBrushTrackerMover> movers;
    std::size_t attachments{},dynamicPoints{},activeNodes{},activeVertices{};
    bool levelOwned{},gcChecked{},topologyChecked{};
};
// Real fresh constructor Setup/pools/mandatory initial Update and insertion,
// observed under one root then rolled back. Not later runtime mover updates,
// travel teardown, query proof, committed startup or playable APK acceptance.
PortableBrushTrackerProbe ProbePortableRuntimeBrushTracker(std::size_t failAfterAttachments=0u,
    const QuestVr::Vm::Limits& limits={});

struct PortableLiveModelQuerySample {
    PortableModelVec3 start,end,extent;
    bool fastClear{},pointClear{},extentClear{},zeroClear{},boxClear{};
    std::int32_t pointNode{-1},leaf{-1};
    std::uint8_t zone{};
    float pointTime{},extentTime{};
    std::size_t fastVisits{},pointVisits{},extentVisits{},zeroVisits{},boxVisits{},regionVisits{};
};
struct PortableLiveModelQueryProbe {
    QuestVr::Vm::Result execution;
    PortableBrushTrackerProbe tracker;
    std::vector<PortableLiveModelQuerySample> samples;
    std::size_t freeNodes{},originalNodes{},dynamicNodeReads{},allNodeReads{};
    std::uint64_t modelRevision{},trackerRevision{};
    bool mutationRefused{},freeReadRefused{},modelRevisionRefused{},trackerRevisionRefused{},
        payloadReplacementRefused{},mutatingTrackerRefused{},expiredAfterRollback{},sourceProofRefused{},
        independentRootRefused{},shutdownRefused{},unloadRefused{},mapLoadRefused{};
};
// Real Training tracker/current-world capability and kernel integration,
// observed under one caller root then completely rolled back. Not committed
// collision gameplay, noninitial mover update, renderer or headset acceptance.
PortableLiveModelQueryProbe ProbePortableRuntimeLiveModelQueries(const QuestVr::Vm::Limits& limits={});

struct PortableBrushUpdateProbe {
    QuestVr::Vm::Result execution;
    PortableBrushTrackerProbe initial,rebuilt;
    std::size_t pending{},pass1Movers{},flushedMovers{},releasedNodes{},releasedPoints{};
    std::size_t repeatedUpdates{},repeatedReleasedNodes{},repeatedAttachments{};
    std::uint64_t beforeModelRevision{},afterModelRevision{},beforeTrackerRevision{},afterTrackerRevision{};
    bool unchangedSkipped{},assimilatedSkipped{},emptySkipped{},topologyChecked{},releasedReadRefused{},currentQueryObserved{};
};
// Actual first-Training constructor then noninitial Update in ONE outer root,
// always rolled back. Does not manufacture a committed startup solely to test
// first-write COW of a preexisting tracker; that separate acceptance stays open.
PortableBrushUpdateProbe ProbePortableRuntimeBrushUpdate(bool fullUpdate=false,std::size_t failureStage=0u,
    const QuestVr::Vm::Limits& limits={});
