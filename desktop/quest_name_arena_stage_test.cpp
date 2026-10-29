#include "quest_name_arena_stage.h"

#include <cstdint>
#include <iostream>
#include <type_traits>
#include <vector>

// Real NameString implementation/static arena in this independent test
// executable. No stand-in lookup catalog, numeric-name shim or runtime UObject
// authority is supplied. NameString's inline GetIndex is tested unchanged.
Array<std::string> NameString::Names;
std::unordered_map<std::string,int> NameString::CompareStringToIndex;
std::unordered_map<std::string,std::pair<int,int>> NameString::SpellStringToIndex;

namespace {
std::size_t checks{},refusals{};
void Require(bool condition,const char* why) {++checks;if(!condition)throw std::runtime_error(why);}
template<class F>void Refused(F&& action,const char* why) {
    bool refused{};try{action();}catch(const std::runtime_error&){refused=true;}
    Require(refused,why);++refusals;
}
struct Budget {
    std::size_t work{},limit=std::numeric_limits<std::size_t>::max();
    std::size_t bytes=std::numeric_limits<std::size_t>::max(),peak{};
    std::vector<std::size_t> admissions;
    NameStringArenaStage::Work Callbacks() {
        return {[this](std::size_t count) {
            if(count>limit-work)throw std::runtime_error("Test name work refusal");
            work+=count;
        },[this](std::size_t count) {
            if(count>bytes)throw std::runtime_error("Test name retained refusal");
            Require(count>peak,"Arena callback is not a growing absolute peak");peak=count;admissions.push_back(count);
        }};
    }
};
void EmptyArenaAndFirstNone() {
    // All three stages begin against the genuinely empty real static arena.
    NameStringArenaStage observer;
    NameStringArenaStage emptyFirst;const auto empty=emptyFirst.Add("");
    const auto emptyAlpha=emptyFirst.Add("StageFirstAlpha");
    Require(empty.IsNone()&&emptyFirst.Spelling(empty)=="None","First empty spelling failed native None initialization");
    Require(emptyAlpha.GetCompareIndex()==1,"Empty first spelling allocated extra native None slot");
    NameStringArenaStage noneFirst;const auto none=noneFirst.Add("None");
    const auto alpha=noneFirst.Add("StageFirstAlpha");
    Require(none.IsNone()&&noneFirst.Spelling(none)=="None","First None spelling changed native identity");
    Require(alpha.GetCompareIndex()==2,"First literal None omitted native duplicate spelling slot");
    Require(!NameString::FindCompareIndex("StageFirstAlpha"),"Detached first-name stages changed real compare table");
    observer.Validate();noneFirst.Validate();const auto growth=noneFirst.CommittedGrowthBytes();
    Require(growth>0u&&noneFirst.PeakBytes()>=noneFirst.RetainedBytes(),"First arena initialization was not metered");
    noneFirst.Commit();noneFirst.Commit();
    Require(none.ToString()=="None"&&alpha.ToString()=="StageFirstAlpha","Committed real indices resolve to wrong spellings");
    Require(NameString("StageFirstAlpha")==alpha&&NameString::FindCompareIndex("StageFirstAlpha")==alpha.GetCompareIndex(),"Commit did not install genuine NameString tables");
    Require(noneFirst.CommittedGrowthBytes()==growth,"Commit changed cached net live-arena growth");
    Refused([&]{observer.Validate();},"Observer accepted another arena's commit");
    Refused([&]{emptyFirst.Validate();},"Alternate pending index plan survived conflicting commit");
}
void ExactSpellingAndStableIds() {
    const NameString existing("ArenaExistingCase");const auto existingId=existing.GetCompareIndex();
    Budget budget;NameStringArenaStage stage(budget.Callbacks());
    const auto same=stage.Add("ArenaExistingCase"),variant=stage.Add("arenaexistingcase");
    const auto fresh=stage.Add("ArenaFreshMixedCase"),freshVariant=stage.Add("ARENAFRESHMIXEDCASE");
    Require(same==existing&&variant==existing&&fresh==freshVariant,"Staged compare identities differ from actual byte-fold contract");
    Require(stage.Spelling(same)=="ArenaExistingCase"&&stage.Spelling(variant)=="arenaexistingcase"&&
        stage.Spelling(fresh)=="ArenaFreshMixedCase"&&stage.Spelling(freshVariant)=="ARENAFRESHMIXEDCASE","Staged spelling identity was collapsed to compare text");
    const auto held=stage.RetainedBytes(),growth=stage.CommittedGrowthBytes();
    const auto duplicate=stage.Add("ArenaFreshMixedCase");
    Require(duplicate==fresh&&stage.RetainedBytes()==held&&stage.CommittedGrowthBytes()==growth,"Same-case duplicate appended names/nodes");
    const auto aliasFromStageView=stage.Add(stage.Spelling(fresh));
    Require(aliasFromStageView==fresh&&stage.Spelling(aliasFromStageView)=="ArenaFreshMixedCase","Spelling view alias became invalid during Add");
    const auto none=stage.Add(""),upperNone=stage.Add("NONE"),lowerNone=stage.Add("none");
    Require(none.IsNone()&&upperNone.IsNone()&&lowerNone.IsNone()&&stage.Spelling(none)=="None"&&
        stage.Spelling(upperNone)=="NONE"&&stage.Spelling(lowerNone)=="none","None compare/spelling rules changed");
    Require(!NameString::FindCompareIndex("ArenaFreshMixedCase")&&existing.GetCompareIndex()==existingId&&existing.ToString()=="ArenaExistingCase","Add mutated real arena or existing IDs before Commit");
    stage.Validate();stage.Commit();
    Require(existing.GetCompareIndex()==existingId&&existing.ToString()=="ArenaExistingCase"&&same==existing,"Commit invalidated existing actual NameString identities");
    Require(fresh.ToString()=="ArenaFreshMixedCase"&&freshVariant.ToString()=="ARENAFRESHMIXEDCASE"&&
        variant.ToString()=="arenaexistingcase"&&NameString("arenaexistingcase")==existing,"Committed spellings differ from real GetIndex results");
    Require(none.ToString()=="None"&&upperNone.ToString()=="NONE"&&lowerNone.ToString()=="none","Committed None spelling changed");
    Require(budget.work>0u&&budget.peak==stage.PeakBytes(),"Shared work/absolute peak callbacks were not used");
    Refused([&]{static_cast<void>(stage.Add("AfterCommit"));},"Committed stage accepted another pending name");
}
void ByteFoldAndCStringNonAssumptions() {
    const std::string highLower{static_cast<char>(0xe9u),'a'},highUpper{static_cast<char>(0xe9u),'A'};
    const std::string differentHigh{static_cast<char>(0xc9u),'A'};
    const std::string embedded{'A','r','e','n','a','\0','x'};
    NameStringArenaStage stage;
    const auto lower=stage.Add(highLower),upper=stage.Add(highUpper),different=stage.Add(differentHigh),nul=stage.Add(embedded);
    Require(lower==upper&&lower!=different,"Name byte-fold unexpectedly expanded non-ASCII case");
    Require(stage.Spelling(nul)==embedded&&stage.Spelling(nul).size()==7u,"Stage invented CString truncation absent from NameString(std::string)");
    stage.Validate();stage.Commit();
    Require(NameString(highLower)==lower&&NameString(highUpper)==upper&&NameString(differentHigh)==different&&
        NameString(embedded)==nul&&nul.ToString()==embedded,"Staged byte spelling differs from unchanged real GetIndex");
}
void RefusalLeavesRealArenaUntouched() {
    const NameString anchor("ArenaRefusalAnchor");const auto anchorId=anchor.GetCompareIndex();
    NameStringArenaStage observer;
    Budget constructorBytes;constructorBytes.bytes=0u;
    Refused([&]{NameStringArenaStage refused(constructorBytes.Callbacks());},"Zero constructor byte budget accepted cloned storage");
    observer.Validate();
    Budget constructorWork;constructorWork.limit=0u;
    Refused([&]{NameStringArenaStage refused(constructorWork.Callbacks());},"Zero constructor work budget accepted arena traversal");
    observer.Validate();
    {
        Budget budget;NameStringArenaStage stage(budget.Callbacks());
        static_cast<void>(stage.Add("ArenaPendingBeforeRefusal"));budget.bytes=0u;
        // Force a larger admitted workspace, not a callback below its prior peak.
        Refused([&]{static_cast<void>(stage.Add("ArenaRefused"+std::string(8192u,'x')));},"Add ignored shared retained refusal");
        Refused([&]{stage.Validate();},"Refused partial stage retained publication authority");
        Require(!NameString::FindCompareIndex("ArenaPendingBeforeRefusal"),"Partial detached Add leaked a real arena entry");
    }
    observer.Validate();
    {
        Budget budget;NameStringArenaStage stage(budget.Callbacks());budget.limit=budget.work;
        Refused([&]{static_cast<void>(stage.Add("ArenaWorkRefused"));},"Add ignored shared work refusal");
    }
    observer.Validate();
    Require(!NameString::FindCompareIndex("ArenaWorkRefused")&&anchor.GetCompareIndex()==anchorId&&anchor.ToString()=="ArenaRefusalAnchor","Refusal changed actual names or existing stable IDs");
}
void InvalidationIsDetected() {
    NameStringArenaStage stage;static_cast<void>(stage.Add("ArenaInvalidatedPending"));
    const NameString external("ArenaExternalActualIntern");
    Refused([&]{stage.Validate();},"Concurrent actual GetIndex mutation was not detected");
    Refused([&]{static_cast<void>(stage.Add("ArenaAfterInvalidation"));},"Add accepted changed source arena");
    Require(external.ToString()=="ArenaExternalActualIntern"&&!NameString::FindCompareIndex("ArenaInvalidatedPending"),"Invalidation exposed pending entries or rolled back somebody else's real intern");
    bool mutate{};Budget budget;auto work=budget.Callbacks();const auto admit=work.admit;
    work.admit=[&](std::size_t bytes) {admit(bytes);if(mutate)static_cast<void>(NameString("ArenaCallbackActualIntern"));};
    NameStringArenaStage callbackStage(std::move(work));mutate=true;
    Refused([&]{static_cast<void>(callbackStage.Add("ArenaCallbackPending"+std::string(4096u,'q')));},"Arena-changing budget callback was not detected");
    Require(NameString::FindCompareIndex("ArenaCallbackActualIntern").has_value()&&!NameString::FindCompareIndex("ArenaCallbackPending"),"Callback invalidation published pending names");
}
void ManyNamesAndCommitGrowth() {
    Budget budget;NameStringArenaStage stage(budget.Callbacks());std::vector<NameString> pending;
    for(unsigned i=0u;i<512u;++i)pending.push_back(stage.Add("ArenaBulk"+std::to_string(i)+std::string(64u,'z')));
    const auto growth=stage.CommittedGrowthBytes();Require(growth>0u,"Bulk net arena growth was not retained");
    Require(stage.PeakBytes()>=stage.RetainedBytes()&&stage.PeakBytes()==budget.peak,"Growth/reallocation workspace escaped absolute admission");
    stage.Validate();stage.Commit();
    for(unsigned i=0u;i<pending.size();++i) {
        const auto spelling="ArenaBulk"+std::to_string(i)+std::string(64u,'z');
        Require(pending[i].ToString()==spelling&&NameString(spelling)==pending[i],"Bulk commit changed prospective real indices");
    }
    Require(stage.CommittedGrowthBytes()==growth,"Net arena credit vanished after commit");
}
void ReservedCloneAvoidsWholeBufferGrowth() {
    // Enlarge the REAL baseline, not a mock table. Every new spelling adds
    // genuine compare/spelling rows before any detached observer is created.
    for(unsigned i=0u;i<2048u;++i)static_cast<void>(NameString("ArenaReservationSeed"+std::to_string(i)));
    NameStringArenaStage observer;
    constexpr auto candidate="ArenaReservedFirstNewName";
    Require(!NameString::FindCompareIndex(candidate),"Reservation candidate already exists in real arena");
    Budget refusedReservation;refusedReservation.bytes=0u;
    Refused([&]{NameStringArenaStage refused(refusedReservation.Callbacks(),2u);},"Reserved constructor allocated despite refusal");
    observer.Validate();
    std::size_t defaultPeak{},reservedPeak{};
    {
        NameStringArenaStage stage;
        static_cast<void>(stage.Add(candidate));defaultPeak=stage.PeakBytes();
    }
    observer.Validate();
    {
        NameStringArenaStage stage({},2u);
        static_cast<void>(stage.Add(candidate));reservedPeak=stage.PeakBytes();
    }
    observer.Validate();
    Require(reservedPeak<defaultPeak,"Reserved clone still overlaps a doubled whole Names buffer on first Add");
    const auto limit=reservedPeak+(defaultPeak-reservedPeak)/2u;
    {
        Budget budget;budget.bytes=limit;NameStringArenaStage stage(budget.Callbacks());
        Refused([&]{static_cast<void>(stage.Add(candidate));},"Unreserved first whole-buffer growth escaped the tighter workspace budget");
    }
    observer.Validate();
    Require(!NameString::FindCompareIndex(candidate),"First-growth refusal mutated the real name arena");
    Budget budget;budget.bytes=limit;NameStringArenaStage stage(budget.Callbacks(),2u);
    const auto pending=stage.Add(candidate);
    Require(stage.Spelling(pending)==candidate&&stage.PeakBytes()<=limit,"Reserved first Add did not fit its pre-admitted no-double-buffer workspace");
    Require(!NameString::FindCompareIndex(candidate),"Reserved construction/Add published actual names early");
    observer.Validate();stage.Validate();stage.Commit();
    Require(pending.ToString()==candidate&&NameString(candidate)==pending,"Reserved direct-copy constructor changed committed real IDs");
}
}

int main() {
    static_assert(noexcept(std::declval<NameStringArenaStage&>().Commit()));
    static_assert(!std::is_copy_constructible_v<NameStringArenaStage>);
    try {
        EmptyArenaAndFirstNone();ExactSpellingAndStableIds();ByteFoldAndCStringNonAssumptions();
        RefusalLeavesRealArenaUntouched();InvalidationIsDetected();ManyNamesAndCommitGrowth();ReservedCloneAvoidsWholeBufferGrowth();
        std::cout<<"PASS quest_name_arena_stage checks="<<checks<<" refusals="<<refusals<<'\n';return 0;
    }catch(const std::exception& error) {std::cerr<<"FAIL quest_name_arena_stage: "<<error.what()<<'\n';return 1;}
}
