#include "quest_reset_config_section.h"

#include <algorithm>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
namespace NC=QuestVr::NativeConfig;
namespace RS=QuestVr::ResetConfigSection;
std::size_t checks{},refusals{};
void Require(bool condition,const char* why) {++checks;if(!condition)throw std::runtime_error(why);}
template<class F>void Refused(F&& action,const char* why) {
    bool refused{};try{action();}catch(const std::runtime_error&){refused=true;}
    Require(refused,why);++refusals;
}
using Pair=std::pair<std::string,std::string>;
struct Operations {
    // Test-only observer. Production supplies its same-root mutable cache and
    // shared lifecycle/work/scratch callbacks, not this vector as authority.
    std::vector<Pair> calls;
    std::vector<std::size_t> admissions,setSteps,setPeaks;
    std::size_t steps{},peak{};
    std::size_t workLimit=std::numeric_limits<std::size_t>::max();
    std::size_t byteLimit=std::numeric_limits<std::size_t>::max();
    std::size_t throwOnSet=std::numeric_limits<std::size_t>::max();
    RS::Work Callbacks() {
        return {
            [&](std::size_t amount) {
                if(amount>workLimit-steps)throw std::runtime_error("Mock shared work refused");
                steps+=amount;
            },
            [&](std::size_t bytes) {
                if(bytes>byteLimit)throw std::runtime_error("Mock shared scratch refused");
                Require(bytes>peak,"Helper admission was not an increasing absolute peak");
                admissions.push_back(bytes);peak=bytes;
            },
            [&](std::string_view key,std::string_view value) {
                if(calls.size()==throwOnSet)throw std::runtime_error("Mock real cache provider refused");
                setSteps.push_back(steps);setPeaks.push_back(peak);calls.emplace_back(key,value);
            }};
    }
};
std::string WideFile(std::u16string_view units) {
    std::string bytes;bytes.reserve(2u+2u*units.size());
    bytes.push_back(static_cast<char>(255u));bytes.push_back(static_cast<char>(254u));
    for(char16_t unit:units) {
        bytes.push_back(static_cast<char>(unit&255u));bytes.push_back(static_cast<char>((unit>>8u)&255u));
    }
    return bytes;
}
struct ContinueVisitor {bool operator()(std::string_view,std::string_view)const {return true;}};
template<class T>concept TemporaryVisit=requires(T value) {std::move(value).VisitSectionPairs("S",ContinueVisitor{});};
static_assert(!TemporaryVisit<NC::OrderedIni>);

void BorrowedPairs() {
    const auto document=NC::OrderedIni::Parse("[Selected]\nD=one\n=x\nD=\n[Other]\nD=foreign\n[SELECTED]\nD=two\n=\n[Empty]\n");
    std::vector<Pair> pairs;const char* borrowed{};std::size_t queryWork{},queryPeak{};
    const NC::Work work{[&](std::size_t n){queryWork+=n;},[&](std::size_t n){queryPeak=std::max(queryPeak,n);}};
    const auto present=document.VisitSectionPairs("sElEcTeD",[&](std::string_view key,std::string_view value) {
        if(value=="two")borrowed=value.data();
        pairs.emplace_back(key,value);return true;
    },work);
    Require(present&&pairs==std::vector<Pair>{{"D","one"},{"","x"},{"D",""},{"D","two"},{"",""}},"Borrowed visit changed pair order, duplicates, empty cells or selected section");
    Require(borrowed==document.Scalar("Selected","D")->data(),"Visitor copied instead of borrowing value storage");
    Require(queryWork>document.EntryCount()&&queryPeak>0u,"Borrowed query skipped shared work/scratch admission");
    std::size_t visited{};
    Require(document.VisitSectionPairs("Selected",[&](auto,auto){++visited;return false;})&&visited==1u,"False visitor did not preserve present-prefix semantics");
    Require(document.VisitSectionPairs("Empty",[&](auto,auto){++visited;return true;})&&visited==1u,"Present empty section was absent or visited invented pairs");
    Require(!document.VisitSectionPairs("Missing",[&](auto,auto){++visited;return true;})&&visited==1u,"Absent section visited pairs or reported present");
    const std::string nulSection("Selected\0ignored",16u);
    Require(document.VisitSectionPairs(nulSection,ContinueVisitor{}),"Section query did not use native CString boundary");
    Refused([&]{static_cast<void>(document.VisitSectionPairs(std::string(1u,static_cast<char>(0x80u)),ContinueVisitor{}));},"Malformed lookup transport was accepted");
    Refused([&]{static_cast<void>(document.VisitSectionPairs("Selected",ContinueVisitor{},NC::Work{{},[](auto){throw std::runtime_error("No scratch");}}));},"Visitor lookup bypassed scratch admission");
    Require(document.Scalar("Selected","D")=="two","Visitor/refusal changed immutable source");
}
void MissingAndEmpty() {
    Operations missingFile;const auto absent=RS::Apply(nullptr,"Extension.InputExt",missingFile.Callbacks());
    Require(!absent.sourcePresent&&!absent.sectionPresent&&!absent.keyPrefixStopped&&absent.pairs==0u&&absent.packetUnits==0u,"Absent file state was invented as a present section");
    Require(missingFile.calls.empty(),"Absent source performed cache writes");
    const auto document=NC::OrderedIni::Parse("[Engine.Input]\nA=original binding\n[Empty]\n");
    Operations missingSection;const auto noSection=RS::Apply(&document,"Extension.InputExt",missingSection.Callbacks());
    Require(noSection.sourcePresent&&!noSection.sectionPresent&&noSection.pairs==0u&&noSection.packetUnits==0u,"Actual InputExt missing-section profile differs");
    Require(missingSection.calls.empty()&&document.Scalar("Engine.Input","A")=="original binding","Reset invented ancestor-section copying or modified source");
    Operations empty;const auto present=RS::Apply(&document,"Empty",empty.Callbacks());
    Require(present.sourcePresent&&present.sectionPresent&&!present.keyPrefixStopped&&present.pairs==0u&&present.packetUnits==1u,"Present-empty packet did not retain one final NUL");
    Require(empty.calls.empty(),"Empty section cleared or invented destination cells");
}
void ReplayOrderAndLiteralText() {
    auto source=NC::OrderedIni::Parse("[S]\nD=one\n=x\nD=\n[Other]\nD=ignored\n[s]\nD=two\n=\n");
    auto changed=source.WithString("S","A=B","C=D");Require(changed.has_value(),"Fixture failed to create equals-containing key");source=std::move(*changed);
    changed=source.WithString("S","Literal","\"quoted\"\r\nsecond line");Require(changed.has_value(),"Fixture failed to create literal packet value");source=std::move(*changed);
    Operations operations;const auto result=RS::Apply(&source,"s",operations.Callbacks());
    Require(result.sectionPresent&&!result.keyPrefixStopped&&result.pairs==7u,"Replay omitted or invented selected source pairs");
    Require(operations.calls==std::vector<Pair>{{"D","one"},{"","x"},{"D",""},{"D","two"},{"",""},{"A","B=C=D"},{"Literal","\"quoted\"\r\nsecond line"}},"Replay sorted/deduplicated, skipped empties, split all equals or re-parsed literal text");
    Require(source.Scalar("S","A=B")=="C=D"&&source.Scalar("S","D")=="two","Reset changed pinned source keys/values");
    std::size_t units=1u;
    for(const auto& [key,value]:operations.calls)units+=QuestVr::UnrealString::Decode(key).size()+QuestVr::UnrealString::Decode(value).size()+2u;
    Require(result.packetUnits==units,"Packed record or final NUL accounting differs");
    Require(!operations.setPeaks.empty()&&operations.setPeaks.front()>=2u*result.packetUnits,"Live packet was not admitted through nested cache calls");

    const auto defaults=NC::OrderedIni::Parse("[S]\nD=one\nD=two\n");
    auto destination=NC::OrderedIni::Parse("[S]\nD=old1\nD=old2\nKeep=untouched\n");
    Operations destinationOps;auto callbacks=destinationOps.Callbacks();const auto observe=callbacks.setString;
    callbacks.setString=[&](auto key,auto value) {
        observe(key,value);auto replacement=destination.WithString("S",key,value);
        if(replacement)destination=std::move(*replacement);
    };
    static_cast<void>(RS::Apply(&defaults,"S",callbacks));
    Require(destination.StringArray("S","D")==std::vector<std::string>{"old1","two"},"Reset replaced all duplicates instead of sequential native SetString newest-cell updates");
    Require(destination.Scalar("S","Keep")=="untouched","Reset cleared unmentioned destination keys");
}
void PacketBounds() {
    const auto value=std::string(16380u,'x');
    const auto exact=NC::OrderedIni::Parse("[S]\na="+value+"\nb="+value+"\n");
    Operations allowed;const auto fit=RS::Apply(&exact,"S",allowed.Callbacks());
    Require(fit.pairs==2u&&fit.packetUnits==RS::PacketCapacityUnits&&!fit.keyPrefixStopped,"Exact 32767-unit packet was refused or miscounted");
    Require(allowed.calls==std::vector<Pair>{{"a",value},{"b",value}},"Exact-fit values were truncated");
    Operations packetBudget;packetBudget.byteLimit=1024u;
    Refused([&]{static_cast<void>(RS::Apply(&exact,"S",packetBudget.Callbacks()));},"Large packet allocation bypassed shared scratch admission");
    Require(packetBudget.calls.empty()&&packetBudget.steps>exact.EntryCount(),"Packet admission was not refused after bounded preflight and before destination writes");
    const auto withTail=NC::OrderedIni::Parse("[S]\na="+value+"\nb="+value+"\nc=\n");
    Operations prefix;const auto stopped=RS::Apply(&withTail,"S",prefix.Callbacks());
    Require(stopped.sectionPresent&&stopped.keyPrefixStopped&&stopped.pairs==2u&&stopped.packetUnits==RS::PacketCapacityUnits,"Native keyguard exhaustion was not successful prefix output");
    Require(prefix.calls==allowed.calls,"Guard exhaustion admitted extra tail cells or removed its prefix");
    const auto unsafe=NC::OrderedIni::Parse("[S]\na="+value+"\nb="+std::string(16381u,'y')+"\n");
    Operations refused;
    Refused([&]{static_cast<void>(RS::Apply(&unsafe,"S",refused.Callbacks()));},"Native's unchecked formatted value overflow was emulated or truncated");
    Require(refused.calls.empty(),"Unsafe later entry was detected only after earlier cache writes");
    Require(unsafe.Scalar("S","a")==value&&unsafe.Scalar("S","b")==std::string(16381u,'y'),"Overflow refusal modified immutable source");
    const auto key=std::string(16383u,'k');
    const auto guard=NC::OrderedIni::Parse("[S]\na="+value+"\n"+key+"=\nz=\n");
    Operations early;const auto earlyStop=RS::Apply(&guard,"S",early.Callbacks());
    Require(earlyStop.keyPrefixStopped&&earlyStop.pairs==1u&&earlyStop.packetUnits==16384u,"Strict native keyguard equality or prefix length differs");
    Require(early.calls==std::vector<Pair>{{"a",value}},"Keyguard skipped one pair then continued to a later fitting pair");
}
void Utf16AndCStringBoundaries() {
    std::u16string value;value.reserve(16380u);
    for(std::size_t i=0u;i<8190u;++i) {value.push_back(static_cast<char16_t>(0xd83du));value.push_back(static_cast<char16_t>(0xde00u));}
    const auto wide=NC::OrderedIni::Parse(WideFile(u"[S]\na="+value+u"\nb="+value+u"\n"));
    Operations operations;const auto exact=RS::Apply(&wide,"S",operations.Callbacks());
    Require(exact.packetUnits==RS::PacketCapacityUnits&&exact.pairs==2u,"Packet counted transport bytes/codepoints instead of UTF16 units");
    Require(operations.calls.size()==2u&&QuestVr::UnrealString::Decode(operations.calls[0u].second)==value,"Supplementary UTF16 packet changed during replay");
    const std::u16string special{u'[',u'S',u']',u'\n',static_cast<char16_t>(0xd800u),u'=',static_cast<char16_t>(0xdc00u),u'\n'};
    const auto isolated=NC::OrderedIni::Parse(WideFile(special));Operations lone;
    const auto result=RS::Apply(&isolated,"S",lone.Callbacks());
    Require(result.pairs==1u&&result.packetUnits==5u,"Isolated UTF16 surrogate unit count differs");
    Require(lone.calls==std::vector<Pair>{{QuestVr::UnrealString::Encode(std::u16string(1u,static_cast<char16_t>(0xd800u))),QuestVr::UnrealString::Encode(std::u16string(1u,static_cast<char16_t>(0xdc00u)))}},"Isolated surrogates were rejected or replaced");
    auto mutated=NC::OrderedIni::Parse("[S]\n");
    const std::string nulKey("K\0not part of key",17u),nulValue("V\0not part of value",19u);
    auto changed=mutated.WithString("S",nulKey,nulValue);Require(changed.has_value(),"Fixture CString mutation missing");mutated=std::move(*changed);
    Operations cstring;const std::string nulSection("S\0not a section",15u);
    static_cast<void>(RS::Apply(&mutated,nulSection,cstring.Callbacks()));
    Require(cstring.calls==std::vector<Pair>{{"K","V"}},"Native CString boundaries were not preserved");
}
void ProviderAndBudgetFailures() {
    const auto source=NC::OrderedIni::Parse("[S]\nOne=first value\nTwo=second value\nThree=last value\n");
    Operations baseline;static_cast<void>(RS::Apply(&source,"S",baseline.Callbacks()));
    Require(baseline.calls.size()==3u&&baseline.setSteps.size()==3u,"Failure fixture did not replay all cells");
    Operations noBytes;noBytes.byteLimit=0u;
    Refused([&]{static_cast<void>(RS::Apply(&source,"S",noBytes.Callbacks()));},"Initial scratch refusal was ignored");
    Require(noBytes.calls.empty()&&noBytes.steps==0u,"Memory refusal occurred after helper work/cache effects");
    Operations noWork;noWork.workLimit=0u;
    Refused([&]{static_cast<void>(RS::Apply(&source,"S",noWork.Callbacks()));},"Shared work refusal was ignored");
    Require(noWork.calls.empty(),"No-work operation performed cache writes");
    Operations preWrite;preWrite.workLimit=baseline.setSteps.front()-1u;
    Refused([&]{static_cast<void>(RS::Apply(&source,"S",preWrite.Callbacks()));},"Pre-write shared work cutoff was ignored");
    Require(preWrite.calls.empty(),"Packet/conversion work was charged after first cache write");
    Operations afterFirst;afterFirst.workLimit=baseline.setSteps.front()+1u;
    Refused([&]{static_cast<void>(RS::Apply(&source,"S",afterFirst.Callbacks()));},"Later shared work cutoff was ignored");
    Require(afterFirst.calls==std::vector<Pair>{{"One","first value"}},"Helper invented rollback or dropped completed SetString prefix");
    Operations setFailure;setFailure.throwOnSet=1u;
    Refused([&]{static_cast<void>(RS::Apply(&source,"S",setFailure.Callbacks()));},"Cache provider failure was swallowed");
    Require(setFailure.calls==std::vector<Pair>{{"One","first value"}},"Cache failure did not preserve exactly the completed callback prefix");
    for(unsigned missing=0u;missing<3u;++missing) {
        Operations local;auto callbacks=local.Callbacks();
        if(missing==0u)callbacks.step={};else if(missing==1u)callbacks.admit={};else callbacks.setString={};
        Refused([&]{static_cast<void>(RS::Apply(&source,"S",callbacks));},"Missing actual provider was silently skipped");
        Require(local.calls.empty()&&local.steps==0u&&local.peak==0u,"Missing provider was detected after effects");
    }
    Require(source.Scalar("S","One")=="first value"&&source.Scalar("S","Three")=="last value","Failure paths mutated immutable source");
}
}

int main() {
    try {
        BorrowedPairs();MissingAndEmpty();ReplayOrderAndLiteralText();PacketBounds();
        Utf16AndCStringBoundaries();ProviderAndBudgetFailures();
        std::cout<<"PASS quest_reset_config_section checks="<<checks<<" refusals="<<refusals<<'\n';return 0;
    }catch(const std::exception& error) {std::cerr<<"FAIL quest_reset_config_section: "<<error.what()<<'\n';return 1;}
}
