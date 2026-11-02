#pragma once
#include "quest_input_source.h"
#include "quest_vr_input.h"
#include <functional>

namespace QuestVr {
// Versioned PORT adaptation, not Windows joystick packets. The host resolves
// these five slots against the actual Engine.Actor.EInputKey declaration.
struct QuestInputMappingKeys {
    std::int32_t fire{-1},frob{-1},inventory{-1},strafe{-1},forward{-1};
    bool operator==(const QuestInputMappingKeys&) const = default;
};
struct QuestInputPhysicalState {
    std::uint32_t version{};
    QuestInputMappingKeys mapping;
    // NOT Input.Keys: a Root grab may clear gameplay keys while hardware holds.
    std::array<std::uint8_t,255u> held{};
    float strafeLevel{},forwardLevel{}; // Last admitted gated NORMALIZED levels.
    bool operator==(const QuestInputPhysicalState&) const = default;
};
struct QuestInputReconcileQueries {
    std::function<void()> validate,work;
    std::function<void(std::size_t)> enterFrame,leaveFrame;
    std::function<bool(std::int32_t,std::int32_t,float)> process;
};
struct QuestInputReconcileResult {std::size_t rows{},pressed{},released{},axes{},handled{};bool reset{},empty{};};
namespace QuestInputReconcileDetail {
inline constexpr std::uint32_t Version=1u;
[[noreturn]] inline void Fail(const char* text){throw std::runtime_error(std::string("Quest input reconciliation: ")+text);}
inline bool Same(const QuestInputCursor& a,const QuestInputCursor& b) noexcept {
    return a.source==b.source&&a.sequence==b.sequence&&a.rejections==b.rejections;
}
inline void Keys(const QuestInputMappingKeys& map) {
    const std::array<std::int32_t,5u> keys{map.fire,map.frob,map.inventory,map.strafe,map.forward};
    for(std::size_t i=0u;i<keys.size();++i){
        if(keys[i]<=0||keys[i]>=255)Fail("mapping key exceeds the non-None original key domain");
        for(std::size_t j=0u;j<i;++j)if(keys[i]==keys[j])Fail("mapping keys overlap");
    }
}
struct Mapped {std::array<std::uint8_t,255u> held{};float strafe{},forward{},strafeLevel{},forwardLevel{};};
inline float AxisUnits(float value,float pollSeconds) {
    const double scaled=double(value)*15000.0*double(pollSeconds);
    if(std::fabs(scaled)>std::numeric_limits<float>::max())Fail("port joystick contribution exceeds Float storage");
    return static_cast<float>(scaled);
}
inline Mapped Map(const QuestInputObservation& row,const QuestInputMappingKeys& keys,float pollSeconds) {
    if(!std::isfinite(pollSeconds)||pollSeconds<0.0f)Fail("invalid actual input-poll elapsed");
    if(!std::isfinite(row.displayTime)||row.displayTime<0||!std::isfinite(row.deltaSeconds)||row.deltaSeconds<0||
        row.buttons>127u||(row.headTracked&&!row.headValid))Fail("invalid observation metadata");
    for(const auto& hand:row.hands)if(!std::isfinite(hand.stickX)||!std::isfinite(hand.stickY)||
        std::fabs(hand.stickX)>1.001f||std::fabs(hand.stickY)>1.001f||!std::isfinite(hand.trigger)||
        hand.trigger<0||hand.trigger>1.001f||!std::isfinite(hand.grip)||hand.grip<0||hand.grip>1.001f)
        Fail("invalid observation controls");
    Mapped result;
    if(!row.focused||!row.mounted||!row.headValid||!row.headTracked)return result;
    if(row.hands[1u].tracked){
        result.held[static_cast<std::size_t>(keys.fire)]=row.hands[1u].trigger>0.75f;
        result.held[static_cast<std::size_t>(keys.frob)]=(row.buttons&1u)!=0u; // A
    }
    if(row.hands[0u].tracked){
        result.held[static_cast<std::size_t>(keys.inventory)]=(row.buttons&16u)!=0u; // Menu
        const auto stick=ApplyRadialDeadzone(row.hands[0u].stickX,row.hands[0u].stickY);
        // Port rate, NOT WinDrv units. Action4*.01 and owned JoyX/Y Speed2
        // yield300*elapsed; original ReadInput's20/elapsed matches digital
        // full-stick magnitude6000, independently of the polling frame rate.
        result.strafeLevel=stick.x;result.forwardLevel=stick.y;
        result.strafe=AxisUnits(stick.x,pollSeconds);result.forward=AxisUnits(stick.y,pollSeconds);
    }
    // B, grips, X/Y, stick clicks and right stick retain their accepted VR roles.
    // Poses need a separate reference-space/world-aim handoff, not raw copying.
    return result;
}
struct Frame {
    const QuestInputReconcileQueries& q;
    static constexpr std::size_t Bytes=3u*sizeof(QuestInputPhysicalState)+3u*sizeof(Mapped)+1024u;
    explicit Frame(const QuestInputReconcileQueries& source):q(source){
        if(!q.work||!q.validate||!q.enterFrame||!q.leaveFrame||!q.process)Fail("missing actual root/work/dispatch service");
        q.enterFrame(Bytes);
    }
    ~Frame()noexcept{q.leaveFrame(Bytes);}
};
} // namespace QuestInputReconcileDetail

// The caller MUST own a real transaction/COW beforeimage for native and script
// mutations. This helper only publishes its physical baseline/cursor AFTER all
// dispatch succeeds; it does not repair prefix mutations or commit a world.
inline QuestInputReconcileResult ReconcileQuestInput(const QuestInputSource::Batch& batch,
    QuestInputPhysicalState& state,QuestInputCursor& cursor,const QuestInputMappingKeys& mapping,
    float pollSeconds,const QuestInputReconcileQueries& q) {
    using namespace QuestInputReconcileDetail;Frame frame(q);q.work();q.validate();Keys(mapping);
    if(!std::isfinite(pollSeconds)||pollSeconds<0.0f)Fail("invalid actual input-poll elapsed");
    if(!Same(batch.before,cursor))Fail("batch does not start at the actual native cursor");
    if((!state.version)!=(!cursor.source)||(!cursor.source&&(cursor.sequence||cursor.rejections))||
        (cursor.source&&!cursor.sequence))Fail("physical baseline and source cursor disagree");
    if(state.version&&state.version!=Version)Fail("unsupported prior physical mapping version");
    if(!std::isfinite(state.strafeLevel)||!std::isfinite(state.forwardLevel)||
        std::hypot(double(state.strafeLevel),double(state.forwardLevel))>1.001||
        (!state.version&&(state.strafeLevel!=0.0f||state.forwardLevel!=0.0f)))
        Fail("invalid prior admitted analog level");
    for(auto held:state.held){q.work();if(held>1u||(!state.version&&held))Fail("invalid prior physical held baseline");}
    if(state.version){Keys(state.mapping);for(std::size_t key=0u;key<state.held.size();++key){q.work();
        if(state.held[key]&&key!=static_cast<std::size_t>(state.mapping.fire)&&
            key!=static_cast<std::size_t>(state.mapping.frob)&&key!=static_cast<std::size_t>(state.mapping.inventory))
            Fail("prior physical baseline contains an unmapped control");}}
    if(!batch.reset&&(!state.version||state.mapping!=mapping))Fail("mapping requires an explicit reset");
    if(batch.status==QuestInputReadStatus::NeedsReset)Fail("source history requires an explicit valid reset");
    QuestInputReconcileResult result;result.reset=batch.reset;
    const auto dispatch=[&](std::int32_t key,std::int32_t action,float delta){
        q.work();q.validate();if(q.process(key,action,delta))++result.handled;q.validate();
    };
    if(batch.status==QuestInputReadStatus::Empty){
        if(batch.reset||batch.count||!Same(batch.before,batch.after))Fail("reset has no valid physical baseline");
        // No new row is NOT tracking loss. Resample the last admitted level
        // at the actual CPU elapsed without inventing a row/cursor advance.
        dispatch(mapping.strafe,4,AxisUnits(state.strafeLevel,pollSeconds));
        dispatch(mapping.forward,4,AxisUnits(state.forwardLevel,pollSeconds));result.axes=2u;
        q.work();q.validate();result.empty=true;return result;
    }
    if(batch.status!=QuestInputReadStatus::Ready||!batch.count||batch.count>QuestInputSource::Capacity||
        !batch.after.source||(cursor.source&&cursor.source!=batch.after.source)||
        (batch.reset&&batch.count!=1u)||(!batch.reset&&cursor.rejections!=batch.after.rejections))
        Fail("invalid staged source batch");
    auto sequence=cursor.sequence;
    for(std::size_t i=0u;i<batch.count;++i){q.work();const auto next=batch.rows[i].sequence;
        if(!next||(batch.reset?next<sequence:
            sequence==std::numeric_limits<std::uint64_t>::max()||next!=sequence+1u))
            Fail("source rows are not the admitted ordered history");
        sequence=next;
    }
    if(sequence!=batch.after.sequence)Fail("source cursor does not end at the staged history");
    QuestInputPhysicalState working=state;
    if(batch.reset)for(std::size_t key=0u;key<working.held.size();++key){q.work();if(!working.held[key])continue;
        dispatch(static_cast<std::int32_t>(key),3,0.0f);++result.released;working.held[key]=0u;}
    Mapped latest;
    for(std::size_t i=0u;i<batch.count;++i){q.work();latest=Map(batch.rows[i].observation,mapping,pollSeconds);
        // A frame's releases precede its presses. Do not compare to Input.Keys.
        for(std::size_t key=0u;key<working.held.size();++key){q.work();if(working.held[key]&&!latest.held[key]){
            dispatch(static_cast<std::int32_t>(key),3,0.0f);++result.released;}}
        for(std::size_t key=0u;key<working.held.size();++key){q.work();if(!working.held[key]&&latest.held[key]){
            dispatch(static_cast<std::int32_t>(key),1,0.0f);++result.pressed;}}
        working.held=latest.held;++result.rows;
    }
    // Axes are sampled levels, not button history: deliver only the newest
    // admitted state once per poll, never N accumulated render-frame axes.
    dispatch(mapping.strafe,4,latest.strafe);dispatch(mapping.forward,4,latest.forward);result.axes=2u;
    working.strafeLevel=latest.strafeLevel;working.forwardLevel=latest.forwardLevel;
    working.mapping=mapping;working.version=Version;q.work();q.validate();
    state=working;cursor=batch.after; // POD, same outer native Input COW
    return result;
}
} // namespace QuestVr
