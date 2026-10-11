#include "quest_decal_state.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <functional>
#include <iostream>
#include <limits>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

// Independently specified surface/owner arrays exercise the audited FDecal
// insertion and reverse SurfList detach rules. They do not execute AttachDecal,
// reflect dynamic script arrays, render geometry, save it, or make GC roots.
namespace {
using State = QuestVr::DecalState;
using Record = QuestVr::DecalSurfaceRecord;
using Snapshot = QuestVr::DecalStateSnapshot;
using Limits = QuestVr::DecalStateLimits;
using TextureMap = std::map<std::string, std::optional<std::string>>;
std::size_t checks{}, refusals{};

void Require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
    ++checks;
}
std::uint32_t Bits(float value) {
    std::uint32_t bits{};
    static_assert(sizeof(bits) == sizeof(value), "float fixture requires 32-bit floats");
    std::memcpy(&bits, &value, sizeof(bits));
    return bits;
}
bool SameSnapshot(const Snapshot& a, const Snapshot& b) {
    if (a.surfaces.size() != b.surfaces.size() || a.owners.size() != b.owners.size()) return false;
    for (std::size_t surface = 0; surface < a.surfaces.size(); ++surface) {
        const auto& left = a.surfaces[surface];
        const auto& right = b.surfaces[surface];
        if (left.size() != right.size()) return false;
        for (std::size_t index = 0; index < left.size(); ++index) {
            const auto& x = left[index]; const auto& y = right[index];
            if (x.ownerIdentity != y.ownerIdentity || x.nodeMembership != y.nodeMembership ||
                x.moverRotation.pitch != y.moverRotation.pitch ||
                x.moverRotation.yaw != y.moverRotation.yaw ||
                x.moverRotation.roll != y.moverRotation.roll) return false;
            for (std::size_t corner = 0; corner < 4; ++corner) {
                const auto& u = x.relativeCorners[corner]; const auto& v = y.relativeCorners[corner];
                if (Bits(u.x) != Bits(v.x) || Bits(u.y) != Bits(v.y) || Bits(u.z) != Bits(v.z)) return false;
            }
        }
    }
    for (std::size_t owner = 0; owner < a.owners.size(); ++owner)
        if (a.owners[owner].ownerIdentity != b.owners[owner].ownerIdentity ||
            a.owners[owner].surfaces != b.owners[owner].surfaces) return false;
    return true;
}
bool SameState(const State& a, const State& b) {
    const auto& x = a.Usage(); const auto& y = b.Usage();
    const auto& u = a.Limits(); const auto& v = b.Limits();
    return a.NodeCount() == b.NodeCount() && SameSnapshot(a.ReadOnlySnapshot(), b.ReadOnlySnapshot()) &&
        x.surfaces == y.surfaces && x.owners == y.owners && x.records == y.records &&
        x.nodeMembership == y.nodeMembership && x.surfListEntries == y.surfListEntries &&
        x.identityBytes == y.identityBytes && x.storageBytes == y.storageBytes &&
        u.surfaces == v.surfaces && u.nodes == v.nodes && u.owners == v.owners &&
        u.records == v.records && u.nodeMembership == v.nodeMembership &&
        u.surfListEntries == v.surfListEntries && u.identityBytes == v.identityBytes &&
        u.storageBytes == v.storageBytes && u.stagingBytes == v.stagingBytes &&
        u.operationSteps == v.operationSteps;
}
void Refused(const std::function<void()>& operation, const std::string& message) {
    bool refused{};
    try { operation(); }
    catch (const std::runtime_error& error) {
        refused = std::string(error.what()).rfind("Native decal state: ", 0u) == 0u;
    }
    Require(refused, message); ++refusals;
}
void RefusedUnchanged(State& state, const std::function<void()>& operation,
                      const std::string& message) {
    const State before(state);
    Refused(operation, message);
    Require(SameState(state, before), message + " changed published state/usage/limits");
}
Record MakeRecord(const std::string& owner, float marker,
                  std::vector<std::int32_t> nodes = {}) {
    Record record;
    record.ownerIdentity = owner; record.nodeMembership = std::move(nodes);
    record.relativeCorners = {{{marker, 2.0f, -3.0f}, {marker + 1.0f, 4.0f, -5.0f},
        {marker + 2.0f, 6.0f, -7.0f}, {marker + 3.0f, 8.0f, -9.0f}}};
    record.moverRotation = {123, -456, std::numeric_limits<std::int32_t>::min()};
    return record;
}
State::LiveTextureResolver Resolve(const TextureMap& textures,
                                  std::vector<std::string>* visits = nullptr) {
    return [&textures, visits](const std::string& owner) {
        if (visits) visits->push_back(owner);
        return textures.at(owner);
    };
}
void Order(const State& state, std::size_t surface,
           const std::vector<std::string>& expected, const std::string& message) {
    const auto& records = state.ReadOnlySnapshot().surfaces.at(surface);
    Require(records.size() == expected.size(), message + " size");
    for (std::size_t index = 0; index < expected.size(); ++index)
        Require(records[index].ownerIdentity == expected[index], message + " record " + std::to_string(index));
}
const QuestVr::DecalOwnerSurfList& Owner(const State& state, const std::string& identity) {
    for (const auto& owner : state.ReadOnlySnapshot().owners)
        if (owner.ownerIdentity == identity) return owner;
    throw std::runtime_error("Test expected owner was absent: " + identity);
}

void LiveTextureOrdering() {
    State state(3, 10);
    TextureMap textures{{"A", "stone"}, {"B", "metal"}, {"C", "stone"},
        {"D", "stone"}, {"E", "stone"}, {"F", "glass"}};
    std::vector<std::string> visits;
    const auto resolver = Resolve(textures, &visits);
    auto stats = state.AttachPrimary(0, MakeRecord("A", 1, {3, 2, 3}), "stone", resolver);
    Require(stats.resolverCalls == 0 && stats.recordsInserted == 1, "Empty surface consulted cached texture");
    stats = state.AttachPrimary(0, MakeRecord("B", 2), "metal", resolver);
    Require(visits == std::vector<std::string>{"A"} && stats.resolverCalls == 1,
        "Append did not resolve the live existing owner");
    visits.clear();
    state.AttachPrimary(0, MakeRecord("C", 3), "stone", resolver);
    Require(visits == std::vector<std::string>{"A"}, "Insertion scanned past first matching texture");
    visits.clear();
    state.AttachPrimary(0, MakeRecord("D", 4), "stone", resolver);
    Require(visits == std::vector<std::string>{"C"}, "Insertion grouped against an older cached texture");
    Order(state, 0, {"D", "C", "A", "B"}, "First live match order");
    textures["D"] = std::nullopt; textures["C"] = "metal";
    visits.clear();
    stats = state.AttachPrimary(0, MakeRecord("E", 5), "stone", resolver);
    Order(state, 0, {"D", "C", "E", "A", "B"}, "Live mutation and None order");
    Require(visits == std::vector<std::string>({"D", "C", "A"}) && stats.resolverCalls == 3,
        "Live texture mutation was not consulted in order");
    visits.clear();
    state.AttachPrimary(0, MakeRecord("F", 6), "glass", resolver);
    Order(state, 0, {"D", "C", "E", "A", "B", "F"}, "Unmatched live texture append");
    Require(visits == std::vector<std::string>({"D", "C", "E", "A", "B"}),
        "Append did not visit each preceding record");
    const auto& a = state.ReadOnlySnapshot().surfaces[0][3];
    Require(a.nodeMembership == std::vector<std::int32_t>({3, 2, 3}),
        "Node order/duplicates were sorted or dropped");
    Require(a.moverRotation.pitch == 123 && a.moverRotation.yaw == -456 &&
        a.moverRotation.roll == std::numeric_limits<std::int32_t>::min(),
        "Stored mover rotation was changed");
    Require(state.ReadOnlySnapshot().surfaces[0].front().nodeMembership.empty(),
        "Empty unrestricted membership was replaced with fabricated nodes");
    Require(Owner(state, "A").surfaces == std::vector<std::int32_t>{0} &&
        Owner(state, "F").surfaces == std::vector<std::int32_t>{0}, "SurfList did not mirror attachment");
}

void DuplicatesAndDetach() {
    State state(3, 1);
    TextureMap textures{{"A", "tex"}, {"B", "other"}};
    const auto resolver = Resolve(textures);
    for (int marker = 1; marker <= 3; ++marker)
        state.AttachPrimary(0, MakeRecord("A", static_cast<float>(marker)), "tex", resolver);
    state.AttachPrimary(1, MakeRecord("A", 4), "tex", resolver);
    Require(Owner(state, "A").surfaces == std::vector<std::int32_t>({0, 0, 0, 1}),
        "Repeated primary attachment replaced/deduplicated SurfList");
    const auto& records = state.ReadOnlySnapshot().surfaces[0];
    Require(records.size() == 3 && records[0].relativeCorners[0].x == 3 &&
        records[1].relativeCorners[0].x == 2 && records[2].relativeCorners[0].x == 1,
        "Repeated primary attachments did not retain reverse same-texture insertion");
    auto stats = state.Detach("A");
    Require(stats.recordsRemoved == 4 && stats.surfListEntriesPopped == 4,
        "Reverse duplicate detach count differed");
    Require(state.ReadOnlySnapshot().surfaces[0].empty() && state.ReadOnlySnapshot().surfaces[1].empty() &&
        Owner(state, "A").surfaces.empty(), "Detach left duplicate records/list entries");
    Require(state.Usage().owners == 1 && state.Usage().records == 0,
        "Detach discarded stable owner mirror or retained logical records");
    stats = state.Detach("A");
    Require(stats.recordsRemoved == 0 && stats.surfListEntriesPopped == 0,
        "Already-empty detach invented work");
    const State before(state);
    stats = state.Detach("Unknown");
    Require(stats.stagedBytes == 0 && SameState(state, before), "Unknown detach added an owner");

    state.AttachPrimary(0, MakeRecord("A", 11), "tex", resolver);
    state.AttachPrimary(0, MakeRecord("B", 22), "other", resolver);
    state.AttachPrimary(0, MakeRecord("A", 33), "tex", resolver);
    state.ReplaceOwnerSurfList("A", {0});
    stats = state.Detach("A");
    Require(stats.recordsRemoved == 1 && stats.surfListEntriesPopped == 1, "Single mirror detach count");
    Order(state, 0, {"A", "B"}, "Detach FIRST matching owner only");
    Require(state.ReadOnlySnapshot().surfaces[0][0].relativeCorners[0].x == 11,
        "Detach removed last matching owner instead of first");
    // The mirror is authoritative input even when no corresponding native
    // record exists. It must pop missing/foreign references, not detach B.
    state.ReplaceOwnerSurfList("A", {2, 0, 2, 0});
    stats = state.Detach("A");
    Require(stats.recordsRemoved == 1 && stats.surfListEntriesPopped == 4,
        "Missing-record reverse detach did not pop every integer");
    Order(state, 0, {"B"}, "Foreign owner survived missing-record detach");
    Require(Owner(state, "B").surfaces == std::vector<std::int32_t>{0}, "Detach changed foreign SurfList");
}

void MirrorAndOpaqueOwnership() {
    State state(4, 2);
    TextureMap textures{{"A", "Tex"}, {"a", "tex"}, {std::string(1, '\xff'), "opaque"}};
    const auto resolver = Resolve(textures);
    state.AttachPrimary(0, MakeRecord("A", 1), "Tex", resolver);
    state.AttachPrimary(0, MakeRecord("a", 2), "tex", resolver);
    state.AttachPrimary(0, MakeRecord(std::string(1, '\xff'), 3), "opaque", resolver);
    Order(state, 0, {"A", "a", std::string(1, '\xff')}, "Opaque case-sensitive byte identities");
    const auto original = state.ReadOnlySnapshot().surfaces;
    state.ReplaceOwnerSurfList("A", {3, 2, 2, 1});
    Require(Owner(state, "A").surfaces == std::vector<std::int32_t>({3, 2, 2, 1}),
        "Mirror synchronization sorted/deduplicated authored entries");
    Snapshot geometryOnly = state.ReadOnlySnapshot(); geometryOnly.owners.clear();
    Snapshot expectedGeometry; expectedGeometry.surfaces = original;
    Require(SameSnapshot(geometryOnly, expectedGeometry), "Mirror replacement changed native records");
    state.ReplaceOwnerSurfList("Foreign", {0, 1});
    state.Detach("Foreign");
    Order(state, 0, {"A", "a", std::string(1, '\xff')}, "Foreign mirror cannot remove other actors");
    state.ReplaceOwnerSurfList("Empty", {});
    Require(Owner(state, "Empty").surfaces.empty(), "Empty mirror was not represented");
    // Aliases into a read-only snapshot remain valid until atomic publication.
    state.ReplaceOwnerSurfList("a", Owner(state, "a").surfaces);
    state.Detach(Owner(state, "a").ownerIdentity);
    Order(state, 0, {"A", std::string(1, '\xff')}, "Aliased identity/list input");
    RefusedUnchanged(state, [&] { state.ReplaceOwnerSurfList("A", {-1}); }, "Negative mirror surface");
    RefusedUnchanged(state, [&] { state.ReplaceOwnerSurfList("A", {4}); }, "One-past mirror surface");
}

void InvalidDataAndResolverFailure() {
    State state(2, 3);
    TextureMap textures{{"A", "tex"}, {"B", "tex"}};
    const auto resolver = Resolve(textures);
    state.AttachPrimary(0, MakeRecord("A", 1), "tex", resolver);
    for (const auto surface : {-1, 2, std::numeric_limits<std::int32_t>::max()})
        RefusedUnchanged(state, [&] { state.AttachPrimary(surface, MakeRecord("B", 2), "tex", resolver); },
            "Malformed attachment surface");
    for (const auto node : {-1, 3, std::numeric_limits<std::int32_t>::max()})
        RefusedUnchanged(state, [&] { state.AttachPrimary(0, MakeRecord("B", 2, {node}), "tex", resolver); },
            "Malformed node membership");
    const std::array<float QuestVr::DecalStateVec3::*, 3> components{
        &QuestVr::DecalStateVec3::x, &QuestVr::DecalStateVec3::y, &QuestVr::DecalStateVec3::z};
    for (std::size_t corner = 0; corner < 4; ++corner)
        for (const auto component : components)
            for (const float invalid : {std::numeric_limits<float>::infinity(),
                    -std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()}) {
                auto record = MakeRecord("B", 2); record.relativeCorners[corner].*component = invalid;
                RefusedUnchanged(state, [&] { state.AttachPrimary(0, record, "tex", resolver); },
                    "Nonfinite relative corner");
            }
    for (const auto& identity : {std::string{}, std::string("A\0B", 3), std::string(1025, 'x')}) {
        RefusedUnchanged(state, [&] { state.AttachPrimary(0, MakeRecord(identity, 2), "tex", resolver); },
            "Malformed owner identity");
        RefusedUnchanged(state, [&] { state.AttachPrimary(0, MakeRecord("B", 2), identity, resolver); },
            "Malformed receiver texture identity");
        RefusedUnchanged(state, [&] { state.Detach(identity); }, "Malformed detach identity");
        RefusedUnchanged(state, [&] { state.ReplaceOwnerSurfList(identity, {}); }, "Malformed mirror identity");
        RefusedUnchanged(state, [&] {
            state.AttachPrimary(0, MakeRecord("B", 2), "tex",
                [identity](const std::string&) { return std::optional<std::string>{identity}; });
        }, "Malformed resolved live texture identity");
    }
    RefusedUnchanged(state, [&] { state.AttachPrimary(0, MakeRecord("B", 2), "tex", {}); },
        "Missing resolver");
    const State before(state);
    bool hostException{};
    try {
        state.AttachPrimary(0, MakeRecord("B", 2), "tex", [](const std::string&) -> std::optional<std::string> {
            throw std::logic_error("host texture lookup failed");
        });
    } catch (const std::logic_error& error) { hostException = std::string(error.what()) == "host texture lookup failed"; }
    Require(hostException && SameState(state, before), "Throwing host resolver changed state or exception");
    state.AttachPrimary(1, MakeRecord("B", 2), "tex", resolver);
    Require(state.Usage().records == 2, "Resolver exception did not release mutation guard");

    State zeroNodes(1, 0);
    auto finite = MakeRecord("A", 1);
    finite.relativeCorners[0] = {-0.0f, std::numeric_limits<float>::max(), -std::numeric_limits<float>::max()};
    zeroNodes.AttachPrimary(0, finite, "tex", resolver);
    Require(Bits(zeroNodes.ReadOnlySnapshot().surfaces[0][0].relativeCorners[0].x) == Bits(-0.0f),
        "Finite signed zero geometry was altered");
    RefusedUnchanged(zeroNodes, [&] { zeroNodes.AttachPrimary(0, MakeRecord("A", 1, {0}), "tex", resolver); },
        "Zero-node model accepted fabricated membership");
    State empty(0, 0);
    RefusedUnchanged(empty, [&] { empty.AttachPrimary(0, finite, "tex", resolver); }, "Empty model surface");
}

void BoundsAndUsage() {
    using Member = std::size_t Limits::*;
    const std::array<Member, 10> members{&Limits::surfaces, &Limits::nodes, &Limits::owners,
        &Limits::records, &Limits::nodeMembership, &Limits::surfListEntries, &Limits::identityBytes,
        &Limits::storageBytes, &Limits::stagingBytes, &Limits::operationSteps};
    for (const auto member : members) {
        Limits limits; limits.*member = 0;
        Refused([&] { State state(0, 0, limits); }, "Zero fixed limit");
        limits = Limits{}; ++(limits.*member);
        Refused([&] { State state(0, 0, limits); }, "Raised fixed limit");
    }
    Limits limits; limits.surfaces = 1;
    Refused([&] { State state(2, 1, limits); }, "Model surface bound");
    limits = {}; limits.nodes = 1;
    Refused([&] { State state(1, 2, limits); }, "Model node bound");
    limits = {}; limits.storageBytes = sizeof(Snapshot) - 1;
    Refused([&] { State state(0, 0, limits); }, "Initial structural storage bound");
    TextureMap textures{{"A", "tex"}, {"B", "tex"}};
    const auto resolver = Resolve(textures);
    for (const auto member : {&Limits::records, &Limits::owners, &Limits::surfListEntries}) {
        limits = {}; limits.*member = 1;
        State state(2, 3, limits);
        state.AttachPrimary(0, MakeRecord("A", 1), "tex", resolver);
        RefusedUnchanged(state, [&] { state.AttachPrimary(1, MakeRecord("B", 2), "tex", resolver); },
            "Aggregate count bound");
    }
    limits = {}; limits.nodeMembership = 1;
    State nodes(1, 3, limits);
    RefusedUnchanged(nodes, [&] { nodes.AttachPrimary(0, MakeRecord("A", 1, {0, 1}), "tex", resolver); },
        "Per-record membership bound");
    nodes.AttachPrimary(0, MakeRecord("A", 1, {0}), "tex", resolver);
    RefusedUnchanged(nodes, [&] { nodes.AttachPrimary(0, MakeRecord("B", 2, {1}), "tex", resolver); },
        "Aggregate membership bound");
    limits = {}; limits.surfListEntries = 2;
    State mirror(2, 1, limits);
    mirror.ReplaceOwnerSurfList("A", {0, 0});
    RefusedUnchanged(mirror, [&] { mirror.ReplaceOwnerSurfList("B", {1}); }, "Aggregate mirror bound");
    RefusedUnchanged(mirror, [&] { mirror.ReplaceOwnerSurfList("A", {0, 0, 0}); }, "Per-owner mirror bound");
    mirror.ReplaceOwnerSurfList("A", {1});
    mirror.ReplaceOwnerSurfList("B", {0});
    Require(mirror.Usage().surfListEntries == 2, "Shrinking mirror did not release logical entry budget");
    limits = {}; limits.identityBytes = 3;
    State identities(1, 1, limits);
    identities.AttachPrimary(0, MakeRecord("A", 1), "tex", resolver);
    RefusedUnchanged(identities, [&] { identities.AttachPrimary(0, MakeRecord("AAAA", 2), "tex", resolver); },
        "Tightened owner byte bound");
    RefusedUnchanged(identities, [&] { identities.AttachPrimary(0, MakeRecord("B", 2), "long", resolver); },
        "Tightened texture byte bound");

    State usage(2, 4);
    const auto initial = sizeof(Snapshot) + 2 * sizeof(std::vector<Record>);
    Require(usage.Usage().storageBytes == initial && usage.Usage().surfaces == 2 &&
        usage.Usage().owners == 0 && usage.Usage().records == 0, "Initial deterministic byte charge");
    const auto record = MakeRecord("A", 1, {2, 1, 2});
    const auto stats = usage.AttachPrimary(0, record, "tex", resolver);
    const auto projected = initial + sizeof(Record) + sizeof(QuestVr::DecalOwnerSurfList) +
        4 /*two one-byte identities with terminators*/ + 3 * sizeof(std::int32_t) + sizeof(std::int32_t);
    const auto incoming = sizeof(Record) + 2 + 3 * sizeof(std::int32_t);
    Require(usage.Usage().storageBytes == projected && usage.Usage().identityBytes == 4 &&
        usage.Usage().nodeMembership == 3 && usage.Usage().surfListEntries == 1, "Attached byte charge");
    Require(stats.stagedBytes == initial * 2 + projected + incoming,
        "Staging omitted old copy, replaced-buffer bound or incoming node payload");
    usage.Detach("A");
    Require(usage.Usage().storageBytes == initial + sizeof(QuestVr::DecalOwnerSurfList) + 2 &&
        usage.Usage().identityBytes == 2 && usage.Usage().nodeMembership == 0 &&
        usage.Usage().surfListEntries == 0, "Detach logical byte charge");

    limits = {}; limits.storageBytes = projected - 1;
    State byteBound(2, 4, limits);
    RefusedUnchanged(byteBound, [&] { byteBound.AttachPrimary(0, record, "tex", resolver); },
        "Projected storage byte bound");
    limits = {}; limits.stagingBytes = stats.stagedBytes - 1;
    State stagingBound(2, 4, limits);
    std::size_t calls{};
    RefusedUnchanged(stagingBound, [&] {
        stagingBound.AttachPrimary(0, record, "tex", [&calls](const std::string&) {
            ++calls; return std::optional<std::string>{"tex"};
        });
    }, "Temporary candidate byte bound");
    Require(calls == 0, "Insufficient staging budget consulted host resolver");
    limits = {}; limits.operationSteps = 1;
    State workBound(1, 1, limits);
    RefusedUnchanged(workBound, [&] { workBound.AttachPrimary(0, MakeRecord("A", 1), "tex", resolver); },
        "Validation work bound");
    Refused([&] { QuestVr::DecalStateDetail::Add(std::numeric_limits<std::size_t>::max(), 1); },
        "Size addition overflow");
    Refused([&] { QuestVr::DecalStateDetail::Multiply(std::numeric_limits<std::size_t>::max(), 2); },
        "Size multiplication overflow");
}

void WorkRollbackAndReentrancy() {
    TextureMap textures{{"A", std::string(100, 'a')}, {"B", std::string(100, 'b')}, {"C", "c"}};
    const auto resolver = Resolve(textures);
    State measured(1, 1);
    measured.AttachPrimary(0, MakeRecord("A", 1), *textures["A"], resolver);
    measured.AttachPrimary(0, MakeRecord("B", 2), *textures["B"], resolver);
    const auto stats = measured.AttachPrimary(0, MakeRecord("C", 3), "c", resolver);
    Limits limits; limits.operationSteps = stats.steps - 1;
    State bounded(1, 1, limits);
    bounded.AttachPrimary(0, MakeRecord("A", 1), *textures["A"], resolver);
    bounded.AttachPrimary(0, MakeRecord("B", 2), *textures["B"], resolver);
    std::vector<std::string> visits;
    RefusedUnchanged(bounded, [&] { bounded.AttachPrimary(0, MakeRecord("C", 3), "c", Resolve(textures, &visits)); },
        "Post-resolver work exhaustion");
    Require(visits == std::vector<std::string>({"A", "B"}), "Work exhaustion did not exercise visited resolver path");

    State state(1, 1), peer(2, 0);
    state.AttachPrimary(0, MakeRecord("A", 1), "tex", [](const std::string&) { return std::optional<std::string>{}; });
    RefusedUnchanged(state, [&] {
        state.AttachPrimary(0, MakeRecord("B", 2), "tex", [&state](const std::string&) {
            state.Detach("A"); return std::optional<std::string>{};
        });
    }, "Reentrant detach");
    RefusedUnchanged(state, [&] {
        state.AttachPrimary(0, MakeRecord("B", 2), "tex", [&state, &peer](const std::string&) {
            state = peer; return std::optional<std::string>{};
        });
    }, "Reentrant assignment");
    const State peerBefore(peer);
    state.AttachPrimary(0, MakeRecord("B", 2), "tex", [&state, &peer](const std::string&) {
        Require(!state.Swap(peer), "Reentrant swap was accepted");
        return std::optional<std::string>{};
    });
    Require(SameState(peer, peerBefore), "Refused reentrant swap modified peer");
    state.Detach("A");
    Order(state, 0, {"B"}, "Mutation guard released after resolver failures");
}

void CopyAndSwapJournal() {
    static_assert(std::is_same<decltype(std::declval<const State&>().ReadOnlySnapshot()),
        const Snapshot&>::value, "Snapshot must be read-only");
    static_assert(std::is_same<decltype(std::declval<const State&>().Usage()),
        const QuestVr::DecalStateUsage&>::value, "Usage must be read-only");
    State state(3, 7);
    TextureMap textures{{"A", "tex"}, {"B", "tex"}, {"C", "other"}};
    const auto resolver = Resolve(textures);
    auto record = MakeRecord("A", 1, {6, 0, 6});
    state.AttachPrimary(1, record, "tex", resolver);
    record.ownerIdentity = "mutated caller"; record.nodeMembership.clear(); record.relativeCorners[0].x = 900;
    Require(state.ReadOnlySnapshot().surfaces[1][0].ownerIdentity == "A" &&
        state.ReadOnlySnapshot().surfaces[1][0].nodeMembership == std::vector<std::int32_t>({6, 0, 6}) &&
        state.ReadOnlySnapshot().surfaces[1][0].relativeCorners[0].x == 1,
        "Attachment retained mutable caller-owned payload");
    state.AttachPrimary(1, MakeRecord("A", 2), "tex", resolver);
    state.AttachPrimary(2, MakeRecord("B", 3), "tex", resolver);
    const State before(state);
    State journal(state);
    state.AttachPrimary(1, MakeRecord("C", 4), "other", resolver);
    state.ReplaceOwnerSurfList("A", {0, 1, 1});
    state.Detach("A");
    const State modified(state);
    Require(!SameState(state, before) && SameState(journal, before), "Journal copy was not independent");
    Require(state.Swap(journal), "Idle rollback swap failed");
    Require(SameState(state, before) && SameState(journal, modified), "Whole-state rollback lost data or mirror order");
    Require(state.Swap(state) && SameState(state, before), "Self swap changed state");
    State assigned(0, 0); assigned = state;
    Require(SameState(assigned, before), "Copy assignment omitted geometry/counts/limits");
    const State& selfAlias = assigned;
    assigned = selfAlias;
    Require(SameState(assigned, before), "Self assignment changed state");
    Limits tightened; tightened.identityBytes = 8;
    State alternate(1, 2, tightened);
    alternate.ReplaceOwnerSurfList("Ghost", {0, 0});
    const State alternativeBefore(alternate);
    Require(state.Swap(alternate), "Different bound/model journal swap failed");
    Require(SameState(state, alternativeBefore) && SameState(alternate, before),
        "Swap omitted model bounds, limits or duplicate mirror entries");
}
} // namespace

int main() {
    try {
        LiveTextureOrdering(); DuplicatesAndDetach(); MirrorAndOpaqueOwnership();
        InvalidDataAndResolverFailure(); BoundsAndUsage(); WorkRollbackAndReentrancy(); CopyAndSwapJournal();
        std::cout << "Native decal surface state: " << checks << " checks, " << refusals
                  << " expected refusals; insertion, mirror, detach and rollback passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Native decal surface state test failed: " << error.what() << '\n';
        return 1;
    }
}
