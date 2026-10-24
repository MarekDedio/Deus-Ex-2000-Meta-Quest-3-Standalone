#include "quest_actor_finish_anim.h"

#include <array>
#include <bit>
#include <iostream>
#include <limits>
#include <map>
#include <vector>

namespace {
using FloatField=QuestVr::OriginalActorFinishAnimFloat;
using BoolField=QuestVr::OriginalActorFinishAnimBool;
using Queries=QuestVr::OriginalActorFinishAnimQueries;
using Stats=QuestVr::OriginalActorFinishAnimStats;
constexpr std::uintptr_t Actor=7u,FirstFrame=11u,SecondFrame=22u;
constexpr std::uint32_t Loop=0x10u,Finished=0x08u,Unrelated=0xa5a50020u;
std::size_t checks{},refusals{},matrixCases{};
void Require(bool value,const char* label) {++checks;if(!value)throw std::runtime_error(label);}
std::uint32_t Raw(float value) {return std::bit_cast<std::uint32_t>(value);}
std::size_t Index(FloatField field) {return static_cast<std::size_t>(field);}
const char* Label(FloatField field) {
    constexpr std::array names{"Frame","Rate","TweenRate","Last"};
    return names.at(Index(field));
}
struct Frame {
    std::uint32_t latent{0xabcdefffu};
    float timer{-123.5f};
    std::string selected{"Selected"},running{"Running"};
    std::uint32_t pc{0x421u};
    std::vector<std::uint8_t> locals{1u,2u,3u,4u};
    bool nullCode{};
};
struct State {
    std::uint32_t flags{Unrelated};
    bool sequenceNone{};
    std::array<float,4u> floats{0.25f,1.0f,0.5f,0.875f};
    std::uintptr_t frame{FirstFrame};
    std::map<std::uintptr_t,Frame> frames{{FirstFrame,{}},{SecondFrame,{}}};
    float& At(FloatField field) {return floats.at(Index(field));}
    float At(FloatField field) const {return floats.at(Index(field));}
};
bool Same(const Frame& a,const Frame& b) {
    return a.latent==b.latent && Raw(a.timer)==Raw(b.timer) && a.selected==b.selected &&
        a.running==b.running && a.pc==b.pc && a.locals==b.locals && a.nullCode==b.nullCode;
}
bool Same(const State& a,const State& b) {
    if(a.flags!=b.flags || a.sequenceNone!=b.sequenceNone || a.frame!=b.frame || a.frames.size()!=b.frames.size())return false;
    for(std::size_t i=0u;i<a.floats.size();++i)if(Raw(a.floats[i])!=Raw(b.floats[i]))return false;
    for(const auto& [identity,frame]:a.frames) {
        const auto other=b.frames.find(identity);
        if(other==b.frames.end() || !Same(frame,other->second))return false;
    }
    return true;
}
template<class Action> void Refuses(Action action,const char* label) {
    bool failed{};
    try {action();}catch(const std::runtime_error&) {failed=true;}
    ++refusals;Require(failed,label);
}
struct Fixture {
    State state;
    Queries queries;
    std::vector<std::string> calls;
    std::size_t writes{},work{},writeCut{std::numeric_limits<std::size_t>::max()},
        workCut{std::numeric_limits<std::size_t>::max()};
    bool inPair{};
    Fixture() {
        queries.work=[&] {
            Require(!inPair,"Work callback observed half-cleared packed flag pair");
            if(work==workCut)throw std::runtime_error("injected native work cutoff");
            ++work;
        };
        queries.readBool=[&](std::uintptr_t actor,BoolField field) {
            Identity(actor);calls.emplace_back(field==BoolField::Loop?"read:Loop":"read:Finished");
            return (state.flags&(field==BoolField::Loop?Loop:Finished))!=0u;
        };
        queries.readFloat=[&](std::uintptr_t actor,FloatField field) {
            Identity(actor);calls.emplace_back(std::string("read:")+Label(field));return state.At(field);
        };
        queries.sequenceIsNone=[&](std::uintptr_t actor) {
            Identity(actor);calls.emplace_back("read:SequenceNone");return state.sequenceNone;
        };
        queries.clearLoopAndFinished=[&](std::uintptr_t actor) {
            Identity(actor);calls.emplace_back("clear:LoopAndFinished");
            // A typed backend can journal two leaves. Deliberately inject
            // the second leaf's cutoff after the first mutation, with NO
            // intervening callback/publication. Only whole-root undo handles
            // that failure; no half-pair is ever a successful result.
            inPair=true;
            try {
                BeforeWrite();state.flags&=~Loop;
                BeforeWrite();state.flags&=~Finished;
            }catch(...) {inPair=false;throw;}
            inPair=false;
        };
        queries.nativeFrame=[&](std::uintptr_t actor) {
            Identity(actor);calls.emplace_back("read:NativeFrame");return state.frame;
        };
        queries.writeNativeLatent=[&](std::uintptr_t actor,std::uintptr_t frame,std::uint32_t nativeAction) {
            Identity(actor);
            Require(nativeAction==385u || nativeAction==0u,"Native latent action was guessed from an enum ordinal");
            Require(state.frames.count(frame)==1u,"Native writer received a fabricated/unretained frame");
            calls.emplace_back("write:Latent:"+std::to_string(nativeAction));
            BeforeWrite();state.frames.at(frame).latent=nativeAction;
        };
    }
    void Identity(std::uintptr_t actor) {
        Require(actor==Actor,"Command/poll changed actual source identity");
        Require(!inPair,"Provider observed half-cleared packed flag pair");
    }
    void BeforeWrite() {
        if(writes==writeCut)throw std::runtime_error("injected native write cutoff");
        ++writes;
    }
    Stats Command() {return QuestVr::ExecuteOriginalActorFinishAnim(Actor,queries);}
    Stats Poll() {return QuestVr::PollOriginalActorFinishAnim(Actor,queries);}
    // This is explicitly a test journal, not proof of production Execution.
    template<class Action> Stats Root(Action action) {
        const auto before=state;
        try {return action();}catch(...) {state=before;throw;}
    }
    Stats CommandRoot() {return Root([&] {return Command();});}
    Stats PollRoot() {return Root([&] {return Poll();});}
};

void LazyCommandGates() {
    Queries none;
    none.readBool=[](std::uintptr_t actor,BoolField field) {
        Require(actor==Actor && field==BoolField::Loop,"Inactive command read another flag/receiver");return false;
    };
    none.sequenceIsNone=[](std::uintptr_t actor) {Require(actor==Actor,"None gate changed actor");return true;};
    auto result=QuestVr::ExecuteOriginalActorFinishAnim(Actor,none);
    Require(result.sequenceInactive && result.steps==2u && result.floatReads==0u && result.frameReads==0u,
        "None command required inactive Float/frame/write providers");

    const auto nan=std::numeric_limits<float>::quiet_NaN();
    Fixture looping;
    looping.state.flags=Unrelated|Loop|Finished;looping.state.sequenceNone=true;
    looping.state.floats={nan,nan,nan,nan};looping.state.frame=0u;
    looping.queries.readFloat={};looping.queries.nativeFrame={};looping.queries.writeNativeLatent={};
    const auto loopBefore=looping.state;
    result=looping.Command();
    auto loopExpected=loopBefore;loopExpected.flags=Unrelated;
    Require(Same(loopExpected,looping.state) && result.sequenceInactive && result.packedFlagClears==1u &&
        looping.writes==2u && looping.calls==std::vector<std::string>{"read:Loop","clear:LoopAndFinished","read:SequenceNone"},
        "Loop pair was not cleared atomically before lazy None return");

    for(const float frame:{0.25f,-0.25f,0.0f,-0.0f})for(const float zero:{0.0f,-0.0f}) {
        Fixture frozen;
        frozen.state.flags=Unrelated|Finished;frozen.state.At(FloatField::Frame)=frame;
        frozen.state.At(FloatField::Rate)=frame>=0.0f?zero:nan;
        frozen.state.At(FloatField::TweenRate)=frame<0.0f?zero:nan;
        frozen.state.At(FloatField::Last)=nan;frozen.state.frame=0u;
        frozen.queries.nativeFrame={};frozen.queries.writeNativeLatent={};
        const auto original=frozen.state;
        result=frozen.Command();
        Require(Same(original,frozen.state) && result.rateInactive && result.floatReads==2u &&
            result.frameReads==0u && result.packedFlagClears==0u,
            "Signed-zero inactive rate read opposite rate/Last/frame or cleared nonloop finished flag");
        Require(frozen.calls[3u]==(frame>=0.0f?"read:Rate":"read:TweenRate"),
            "Signed-zero Frame followed the tween branch");
    }
    for(const float frame:{0.875f,1.0f}) {
        Fixture ended;
        ended.state.At(FloatField::Frame)=frame;ended.state.frame=0u;
        ended.queries.nativeFrame={};ended.queries.writeNativeLatent={};
        const auto original=ended.state;
        result=ended.Command();
        Require(Same(original,ended.state) && result.atOrPastLast && result.frameReads==0u && result.floatReads==4u,
            "At/past Last incorrectly required a native frame or installed a wait");
    }
    Fixture reload;
    unsigned frameReads{};
    const auto ordinaryRead=reload.queries.readFloat;
    reload.queries.readFloat=[&](std::uintptr_t actor,FloatField field) {
        const auto value=ordinaryRead(actor,field);
        if(field==FloatField::Frame && ++frameReads==2u)return 1.0f;
        return value;
    };
    reload.queries.nativeFrame={};
    result=reload.Command();
    Require(result.atOrPastLast && frameReads==2u,"AnimFrame was not reloaded at the final original Last gate");
}

void InstallAndPoll() {
    for(const float frame:{0.25f,-0.25f,0.0f,-0.0f})for(const float rate:{-1.0f,1.0f})
        for(const auto old:{0u,384u,385u,528u,0xffffffffu}) {
            Fixture fixture;
            fixture.state.flags=Unrelated|Finished;fixture.state.At(FloatField::Frame)=frame;
            fixture.state.At(frame>=0.0f?FloatField::Rate:FloatField::TweenRate)=rate;
            fixture.state.frames.at(FirstFrame).latent=old;
            const auto original=fixture.state;
            const auto result=fixture.Command();
            auto expected=original;expected.frames.at(FirstFrame).latent=385u;
            Require(result.installed && result.frameReads==1u && result.nativeLatentWrites==1u &&
                result.packedFlagClears==0u && Same(expected,fixture.state),
                "Reached FinishAnim failed to overwrite actual latent385 only or cleared nonloop finished flag");
            Require(fixture.calls==std::vector<std::string>{"read:Loop","read:SequenceNone","read:Frame",
                frame>=0.0f?"read:Rate":"read:TweenRate","read:Frame","read:Last","read:NativeFrame","write:Latent:385"},
                "FinishAnim reached provider order diverged from retail");
        }

    Queries inactive;
    inactive.readBool=[](std::uintptr_t actor,BoolField field) {
        Require(actor==Actor && field==BoolField::Finished,"Poll read a different flag/receiver");return false;
    };
    auto result=QuestVr::PollOriginalActorFinishAnim(Actor,inactive);
    Require(result.steps==1u && result.boolReads==1u && result.frameReads==0u && !result.released,
        "Unfinished poll required native frame or unrelated providers");

    Fixture poll;
    const auto nan=std::numeric_limits<float>::quiet_NaN();
    poll.state.flags=Unrelated|Loop|Finished;poll.state.floats={nan,nan,nan,nan};
    poll.state.sequenceNone=true;poll.state.frames.at(FirstFrame).timer=nan;
    poll.queries.readFloat={};poll.queries.sequenceIsNone={};poll.queries.clearLoopAndFinished={};
    const auto original=poll.state;
    result=poll.Poll();
    auto expected=original;expected.frames.at(FirstFrame).latent=0u;
    Require(result.released && result.nativeLatentWrites==1u && result.floatReads==0u &&
        result.sequenceReads==0u && result.packedFlagClears==0u && Same(expected,poll.state),
        "Finished poll did not clear only native latent while preserving flags/timer/frame/code/locals");
    Require(poll.calls==std::vector<std::string>{"read:Finished","read:NativeFrame","write:Latent:0"},
        "Poll included animation advance/timer/sequence/loop work");

    Fixture replaced;
    replaced.Command();
    // Stand-in for the already-completed, synchronous selected AnimEnd: a new
    // frame and new unfinished animation must win over prior command stats.
    replaced.state.frame=SecondFrame;
    replaced.state.frames.at(SecondFrame).latent=385u;
    replaced.state.frames.at(SecondFrame).pc=0x912u;
    replaced.state.flags&=~Finished;
    auto beforePoll=replaced.state;
    result=replaced.Poll();
    Require(!result.released && result.frameReads==0u && Same(beforePoll,replaced.state),
        "Poll reused captured finish/frame status after animation callback replacement");
    replaced.state.flags|=Finished;
    beforePoll=replaced.state;
    result=replaced.Poll();
    auto afterPoll=beforePoll;afterPoll.frames.at(SecondFrame).latent=0u;
    Require(result.released && Same(afterPoll,replaced.state) && replaced.state.frames.at(FirstFrame).latent==385u,
        "Poll cleared old captured frame rather than freshly selected actual native frame");
}

void RefusalsAndWholeRootUndo() {
    for(const auto bad:{std::numeric_limits<float>::infinity(),-std::numeric_limits<float>::infinity(),
        std::numeric_limits<float>::quiet_NaN()})for(unsigned path=0u;path<4u;++path) {
        Fixture fixture;fixture.state.flags=Unrelated|Loop|Finished;
        if(path==0u)fixture.state.At(FloatField::Frame)=bad;
        if(path==1u)fixture.state.At(FloatField::Rate)=bad;
        if(path==2u) {fixture.state.At(FloatField::Frame)=-0.25f;fixture.state.At(FloatField::TweenRate)=bad;}
        if(path==3u)fixture.state.At(FloatField::Last)=bad;
        const auto original=fixture.state;
        Refuses([&] {fixture.CommandRoot();},"Reached unsupported nonfinite comparison was accepted");
        Require(Same(original,fixture.state),"Finite-subset refusal retained earlier packed flag clear");
    }
    for(const bool poll:{false,true}) {
        Fixture fixture;fixture.state.flags=Unrelated|Loop|Finished;fixture.state.frame=0u;
        const auto original=fixture.state;
        Refuses([&] {if(poll)fixture.PollRoot();else fixture.CommandRoot();},
            "Reached native frame absence was replaced with a fabricated frame");
        Require(Same(original,fixture.state),"Native frame refusal retained early command writes");
    }
    for(unsigned missing=0u;missing<6u;++missing) {
        Fixture fixture;fixture.state.flags=Unrelated|Loop|Finished;
        if(missing==0u)fixture.queries.readBool={};
        if(missing==1u)fixture.queries.clearLoopAndFinished={};
        if(missing==2u)fixture.queries.sequenceIsNone={};
        if(missing==3u)fixture.queries.readFloat={};
        if(missing==4u)fixture.queries.nativeFrame={};
        if(missing==5u)fixture.queries.writeNativeLatent={};
        const auto original=fixture.state;
        Refuses([&] {fixture.CommandRoot();},"Reached required native provider absence was accepted");
        Require(Same(original,fixture.state),"Provider refusal escaped complete command root undo");
    }

    Fixture complete;complete.state.flags=Unrelated|Loop|Finished;complete.CommandRoot();
    Require(complete.writes==3u && complete.work==9u,"Write/work fixture missed clear pair or latent install");
    for(std::size_t cut=0u;cut<complete.writes;++cut) {
        Fixture fixture;fixture.state.flags=Unrelated|Loop|Finished;fixture.writeCut=cut;
        const auto original=fixture.state;
        Refuses([&] {fixture.CommandRoot();},"Actual FinishAnim write-prefix cutoff did not refuse");
        Require(Same(original,fixture.state) && !fixture.inPair,
            "FinishAnim write-prefix rollback published a half-cleared pair or latent install");
    }
    for(std::size_t cut=0u;cut<complete.work;++cut) {
        Fixture fixture;fixture.state.flags=Unrelated|Loop|Finished;fixture.workCut=cut;
        const auto original=fixture.state;
        Refuses([&] {fixture.CommandRoot();},"Actual FinishAnim work-prefix cutoff did not refuse");
        Require(Same(original,fixture.state),"FinishAnim work-prefix rollback retained packed/latent writes");
    }
    Fixture completedThenFailed;completedThenFailed.state.flags=Unrelated|Loop|Finished;
    const auto original=completedThenFailed.state;
    Refuses([&] {
        completedThenFailed.Root([&]()->Stats {
            completedThenFailed.Command();
            throw std::runtime_error("later enclosing statement refused");
        });
    },"Completed command escaped a later enclosing-root failure");
    Require(Same(original,completedThenFailed.state),"Later root failure retained completed FinishAnim writes");

    Fixture pollComplete;pollComplete.state.flags|=Finished;pollComplete.PollRoot();
    Require(pollComplete.writes==1u && pollComplete.work==3u,"Poll cutoff fixture did not reach only native clear");
    for(std::size_t cut=0u;cut<pollComplete.work;++cut) {
        Fixture fixture;fixture.state.flags|=Finished;fixture.workCut=cut;
        const auto before=fixture.state;
        Refuses([&] {fixture.PollRoot();},"Actual poll work-prefix cutoff did not refuse");
        Require(Same(before,fixture.state),"Poll work-prefix rollback changed latent/timer/frame state");
    }
    Fixture pollWrite;pollWrite.state.flags|=Finished;pollWrite.writeCut=0u;
    const auto pollBefore=pollWrite.state;
    Refuses([&] {pollWrite.PollRoot();},"Actual poll native-slot write cutoff did not refuse");
    Require(Same(pollBefore,pollWrite.state),"Poll write cutoff escaped complete root undo");
}

void FiniteMatrix() {
    // Independent direct finite comparisons/predicate oracle. No animation
    // policy, portable latent enum, pure helper, or original DLL is invoked
    // to derive expected changes. Binary32 loads compare exactly in x87 too.
    const auto tiny=std::numeric_limits<float>::denorm_min();
    for(const float frame:{-1.0f,-tiny,-0.0f,0.0f,tiny,0.25f,0.875f,1.0f})
        for(const float rate:{-1.0f,-tiny,-0.0f,0.0f,tiny,1.0f})
            for(const float tween:{-1.0f,-0.0f,0.0f,tiny,1.0f})
                for(const float last:{-0.5f,0.0f,0.25f,0.875f})
                    for(unsigned flags=0u;flags<8u;++flags) {
                        Fixture fixture;
                        fixture.state.flags=Unrelated|((flags&1u)?Loop:0u)|((flags&2u)?Finished:0u);
                        fixture.state.sequenceNone=(flags&4u)!=0u;
                        fixture.state.floats={frame,rate,tween,last};
                        auto expected=fixture.state;
                        if((expected.flags&Loop)!=0u)expected.flags&=~(Loop|Finished);
                        const bool install=!expected.sequenceNone && (frame>=0.0f?rate!=0.0f:tween!=0.0f) && frame<last;
                        if(install)expected.frames.at(FirstFrame).latent=385u;
                        const auto result=fixture.Command();
                        Require(Same(expected,fixture.state) && result.installed==install,
                            "Finite original FinishAnim comparison matrix mismatch");
                        ++matrixCases;
                    }
}
} // namespace

int main() {
    try {
        static_assert(QuestVr::OriginalActorFinishAnimNative==261u && QuestVr::OriginalActorFinishAnimFlags==0x409u &&
            QuestVr::OriginalActorFinishAnimPollNative==385u);
        LazyCommandGates();InstallAndPoll();RefusalsAndWholeRootUndo();FiniteMatrix();
        std::cout<<"Original Actor FinishAnim tests passed: "<<checks<<" checks / "<<refusals
            <<" refusals / "<<matrixCases<<" finite comparison matrix cases\n";
        return 0;
    }catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
