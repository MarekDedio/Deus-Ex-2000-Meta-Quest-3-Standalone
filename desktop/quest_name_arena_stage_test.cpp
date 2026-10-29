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
    NameStringArenaStage originalFirst;
    const auto originalNone=originalFirst.AddOriginalName("nOnE"),originalEmpty=originalFirst.AddOriginalName("");
    const auto originalAlpha=originalFirst.AddOriginalName("OriginalFirstAlpha");
    Require(originalNone.IsNone()&&originalEmpty.IsNone()&&originalFirst.Spelling(originalNone)=="None"&&
        originalFirst.Spelling(originalEmpty)=="None","Original first None/empty did not select genuine canonical slot0");
    Require(originalAlpha.GetCompareIndex()==1,"Original first None added a case-variant spelling row");
    NameStringArenaStage incrementalEmpty({},2u,NameStringArenaStage::Mode::IncrementalOriginalName);
    const auto incrementalNone=incrementalEmpty.AddOriginalName("NONE");
    Require(!incrementalEmpty.UsesIncrementalOriginalName()&&incrementalNone.IsNone()&&
        incrementalEmpty.Spelling(incrementalNone)=="None","Uninitialized original arena did not use genuine full bootstrap");
    incrementalEmpty.Validate();observer.Validate();
    originalFirst.Validate();observer.Validate();
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
void OriginalFirstSpellingAndNameLength() {
    const NameString existing("OriginalArenaFirstSpelling");
    static_cast<void>(NameString("ORIGINALARENAFIRSTSPELLING"));
    static_cast<void>(NameString("originalarenafirstspelling"));
    Budget budget;NameStringArenaStage stage(budget.Callbacks(),8u);
    const auto held=stage.RetainedBytes(),growth=stage.CommittedGrowthBytes();
    const auto variant=stage.AddOriginalName("oRIGINALaRENAfIRSTsPELLING");
    Require(variant==existing&&stage.Spelling(variant)=="OriginalArenaFirstSpelling",
        "Original existing identity did not retain earliest actual spelling");
    Require(stage.RetainedBytes()==held&&stage.CommittedGrowthBytes()==growth,
        "Original existing lookup appended a case-variant spelling");
    const auto fresh=stage.AddOriginalName("OriginalArenaFresh_Mixed-42");
    const auto freshHeld=stage.RetainedBytes(),freshGrowth=stage.CommittedGrowthBytes();
    const auto freshVariant=stage.AddOriginalName("ORIGINALARENAFRESH_MIXED-42");
    Require(freshVariant==fresh&&stage.Spelling(freshVariant)=="OriginalArenaFresh_Mixed-42",
        "Original pending identity did not preserve first spelling");
    Require(stage.RetainedBytes()==freshHeld&&stage.CommittedGrowthBytes()==freshGrowth,
        "Original pending duplicate appended spelling/storage");
    const auto next=stage.AddOriginalName("OriginalArenaNext");
    Require(next.GetCompareIndex()==fresh.GetCompareIndex()+2,
        "Original pending case variant consumed an actual name index");
    const auto ordinaryVariant=stage.Add("ORIGINALARENAFRESH_MIXED-42");
    Require(ordinaryVariant==fresh&&stage.Spelling(ordinaryVariant)=="ORIGINALARENAFRESH_MIXED-42"&&
        stage.Spelling(stage.AddOriginalName("originalarenafresh_mixed-42"))=="OriginalArenaFresh_Mixed-42",
        "Original canonical lookup changed ordinary Add or selected its later spelling");
    const auto none=stage.AddOriginalName("NONE"),lowerNone=stage.AddOriginalName("none"),empty=stage.AddOriginalName("");
    Require(none.IsNone()&&lowerNone.IsNone()&&empty.IsNone()&&stage.Spelling(none)=="None"&&
        stage.Spelling(lowerNone)=="None"&&stage.Spelling(empty)=="None","Original None variants lost canonical slot0 spelling");
    const auto longName="OriginalArenaLong_"+std::string(4096u,'q');
    auto longVariant=longName;longVariant[0u]='o';
    const auto longValue=stage.AddOriginalName(longName),longAgain=stage.AddOriginalName(longVariant);
    Require(longValue==longAgain&&stage.Spelling(longAgain)==longName&&stage.Spelling(longValue).size()>63u,
        "Original name insertion truncated a variable-length name");
    const std::string_view view=stage.Spelling(longValue);
    Require(stage.AddOriginalName(view)==longValue,"Original name lookup invalidated a supplied staged spelling view");
    Require(!NameString::FindCompareIndex("OriginalArenaFresh_Mixed-42")&&!NameString::FindCompareIndex(longName),
        "Original name insertion published real arena state before Commit");
    stage.Validate();stage.Commit();
    Require(variant.ToString()==existing.ToString()&&freshVariant.ToString()=="OriginalArenaFresh_Mixed-42"&&
        longAgain.ToString()==longName,"Committed original identities resolved to noncanonical spellings");
    Require(NameString(longName)==longValue&&NameString("OriginalArenaFresh_Mixed-42")==fresh,
        "Original names were not inserted into the actual shared NameString arena");
    Refused([&]{static_cast<void>(stage.AddOriginalName("AfterOriginalCommit"));},
        "Committed stage accepted another original name");
    Require(budget.work>0u&&budget.peak==stage.PeakBytes(),"Original lookup omitted shared work/peak admission");
}
void OriginalNameRefusals() {
    NameStringArenaStage observer;
    {
        NameStringArenaStage stage;
        const std::string embedded{'O','r','i','g','i','n','a','l','\0','x'};
        Refused([&]{static_cast<void>(stage.AddOriginalName(embedded));},"Original name accepted an untrimmed embedded NUL");
        Refused([&]{stage.Validate();},"Refused original CString retained publication authority");
    }
    observer.Validate();
    {
        Budget budget;NameStringArenaStage stage(budget.Callbacks());budget.limit=budget.work;
        Refused([&]{static_cast<void>(stage.AddOriginalName("OriginalArenaFirstSpelling"));},
            "Original existing-identity lookup ignored shared work refusal");
    }
    observer.Validate();
    const auto large="OriginalArenaBytesRefusal_"+std::string(32768u,'x');
    {
        Budget budget;NameStringArenaStage stage(budget.Callbacks());budget.bytes=0u;
        Refused([&]{static_cast<void>(stage.AddOriginalName(large));},
            "Original lookup allocated comparison scratch after retained refusal");
        Refused([&]{stage.Validate();},"Refused original scratch retained publication authority");
    }
    observer.Validate();
    Require(!NameString::FindCompareIndex(large),"Original refusal leaked a new real arena entry");
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
void IncrementalOriginalNames() {
    using Mode=NameStringArenaStage::Mode;
    constexpr const char* fresh="IncrementalOriginalFreshMixed";
    Require(!NameString::FindCompareIndex(fresh),"Incremental fresh fixture already exists");
    // Obtain genuine free Names capacity via unchanged GetIndex growth, and
    // genuine map slack via real inserts. Some exact-prime bucket counts can
    // require one further insertion; never mutate private table capacities.
    bool ready{};
    for(unsigned index=0u;index<32u&&!ready;++index) {
        static_cast<void>(NameString("IncrementalCapacitySeed"+std::to_string(index)));
        NameStringArenaStage probe({},2u,Mode::IncrementalOriginalName);
        static_cast<void>(probe.AddOriginalName(fresh));ready=probe.UsesIncrementalOriginalName();
    }
    Require(ready,"Could not prepare a real free-capacity incremental arena profile");
    const NameString anchor("OriginalArenaFirstSpelling");const auto* address=&anchor.ToString();
    NameStringArenaStage observer;
    Budget budget;budget.limit=8192u;budget.bytes=16384u;
    NameStringArenaStage stage(budget.Callbacks(),2u,Mode::IncrementalOriginalName);
    const auto pending=stage.AddOriginalName(fresh);
    Require(stage.UsesIncrementalOriginalName()&&stage.Spelling(pending)==fresh,
        "Free-capacity original insertion cloned the whole arena or lost tail spelling");
    Require(!NameString::FindCompareIndex(fresh)&& &anchor.ToString()==address,"Incremental staging changed real arena state early");
    observer.Validate();stage.Validate();const auto growth=stage.CommittedGrowthBytes();
    Require(growth>0u&&budget.work<8192u&&budget.peak<=16384u,
        "Tiny original delta traversed/cloned the full arena or omitted growth credit");
    stage.Commit();stage.Commit();
    Require(pending.ToString()==fresh&&NameString(fresh)==pending&& &anchor.ToString()==address,
        "Incremental commit replaced global Names storage or published different real identity");
    Require(stage.CommittedGrowthBytes()==growth,"Incremental commit discarded process-lifetime growth credit");
    Refused([&]{observer.Validate();},"Incremental publication did not invalidate an older captured arena size");
    for(const auto* input:{"incrementaloriginalfreshmixed","INCREMENTALORIGINALFRESHMIXED","None","NONE","none",""}) {
        NameStringArenaStage unchanged;
        Budget exact;exact.limit=8192u;exact.bytes=16384u;
        NameStringArenaStage duplicate(exact.Callbacks(),2u,Mode::IncrementalOriginalName);
        const auto found=duplicate.AddOriginalName(input);
        const bool none=std::string(input).empty()||std::string(input)=="None"||std::string(input)=="NONE"||std::string(input)=="none";
        Require(duplicate.UsesIncrementalOriginalName()&&duplicate.Spelling(found)==(none?"None":fresh)&&
            (none?found.IsNone():found==pending),"Incremental existing-name lookup lost canonical first spelling/None");
        Require(duplicate.CommittedGrowthBytes()==0u&&exact.work<8192u,"Existing original name added storage or scanned whole arena");
        duplicate.Validate();duplicate.Commit();unchanged.Validate();
    }
    {
        NameStringArenaStage unchanged;
        NameStringArenaStage once({},2u,Mode::IncrementalOriginalName);
        static_cast<void>(once.AddOriginalName(fresh));
        Refused([&]{static_cast<void>(once.AddOriginalName("IncrementalForbiddenSecond"));},
            "Incremental single-operation mode overwrote its first plan");
        Refused([&]{once.Validate();},"Refused second original operation retained commit authority");unchanged.Validate();
    }
    {
        NameStringArenaStage unchanged;
        NameStringArenaStage onlyOriginal({},2u,Mode::IncrementalOriginalName);
        Refused([&]{static_cast<void>(onlyOriginal.Add("IncrementalOrdinaryAdd"));},
            "Incremental mode admitted an unrelated ordinary Add plan");unchanged.Validate();
    }
    {
        NameStringArenaStage unchanged;Budget cut;cut.limit=0u;
        NameStringArenaStage refused(cut.Callbacks(),2u,Mode::IncrementalOriginalName);
        Refused([&]{static_cast<void>(refused.AddOriginalName("IncrementalWorkCut"));},
            "Incremental key lookup ignored work refusal");unchanged.Validate();
    }
    {
        NameStringArenaStage unchanged;Budget cut;cut.bytes=0u;
        Refused([&]{NameStringArenaStage refused(cut.Callbacks(),2u,Mode::IncrementalOriginalName);},
            "Incremental stage ignored header/bucket admission");unchanged.Validate();
    }
    {
        NameStringArenaStage unchanged;Budget cut;
        NameStringArenaStage refused(cut.Callbacks(),2u,Mode::IncrementalOriginalName);cut.bytes=cut.peak;
        Refused([&]{static_cast<void>(refused.AddOriginalName("IncrementalBytesCut"+std::string(8192u,'q')));},
            "Incremental staging allocated strings after byte refusal");unchanged.Validate();
    }
    {
        NameStringArenaStage unchanged;Budget cut;
        NameStringArenaStage refused(cut.Callbacks(),2u,Mode::IncrementalOriginalName);
        static_cast<void>(refused.AddOriginalName("IncrementalValidateWorkCut"));cut.limit=cut.work;
        Refused([&]{refused.Validate();},"Incremental publication omitted merge/hash work admission");unchanged.Validate();
        Require(!NameString::FindCompareIndex("IncrementalValidateWorkCut"),"Publication work cut leaked pending original name");
    }
    {
        NameStringArenaStage invalidated({},2u,Mode::IncrementalOriginalName);
        static_cast<void>(invalidated.AddOriginalName(fresh));static_cast<void>(NameString("IncrementalExternalActualIntern"));
        Refused([&]{invalidated.Validate();},"Incremental unchanged-capture check missed another real interner");
    }
    // An exact-size ordinary clone leaves NO free Names slots, establishing
    // genuine growth rather than a mocked capacity condition.
    {NameStringArenaStage tight;tight.Validate();tight.Commit();}
    {
        NameStringArenaStage unchanged;NameStringArenaStage fallback({},2u,Mode::IncrementalOriginalName);
        const auto name=fallback.AddOriginalName("IncrementalGenuineGrowthFallback");
        Require(!fallback.UsesIncrementalOriginalName()&&fallback.Spelling(name)=="IncrementalGenuineGrowthFallback"&&
            !NameString::FindCompareIndex("IncrementalGenuineGrowthFallback"),"True capacity growth did not stage the real full fallback");
        unchanged.Validate();fallback.Validate();fallback.Commit();
        Require(name.ToString()=="IncrementalGenuineGrowthFallback","Full fallback did not preserve prospective real name identity");
    }
    {NameStringArenaStage tight;tight.Validate();tight.Commit();}
    {
        NameStringArenaStage unchanged;NameStringArenaStage fallback({},2u,Mode::IncrementalOriginalName);
        static_cast<void>(fallback.AddOriginalName("IncrementalFallbackSecondRefusal"));
        Require(!fallback.UsesIncrementalOriginalName(),"Tight Names fixture unexpectedly retained free slots");
        Refused([&]{static_cast<void>(fallback.AddOriginalName("IncrementalForbiddenAfterFallback"));},
            "Full fallback lost its configured single-operation restriction");unchanged.Validate();
    }
}
}

int main() {
    static_assert(noexcept(std::declval<NameStringArenaStage&>().Commit()));
    static_assert(!std::is_copy_constructible_v<NameStringArenaStage>);
    try {
        EmptyArenaAndFirstNone();ExactSpellingAndStableIds();ByteFoldAndCStringNonAssumptions();
        OriginalFirstSpellingAndNameLength();OriginalNameRefusals();
        RefusalLeavesRealArenaUntouched();InvalidationIsDetected();ManyNamesAndCommitGrowth();ReservedCloneAvoidsWholeBufferGrowth();
        IncrementalOriginalNames();
        std::cout<<"PASS quest_name_arena_stage checks="<<checks<<" refusals="<<refusals<<'\n';return 0;
    }catch(const std::exception& error) {std::cerr<<"FAIL quest_name_arena_stage: "<<error.what()<<'\n';return 1;}
}
