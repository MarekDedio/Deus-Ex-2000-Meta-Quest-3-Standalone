#include "quest_spawn_play_actor_tail.h"
#include "quest_native_work.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iostream>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

// PROVIDER MOCKS ONLY. No original DLL, script, real Level/Player or startup is
// executed/certified by this fixture. Runtime authority is the integration's job.
namespace {
using Id=QuestVr::SpawnPlayActorNativeId;
using Queries=QuestVr::OriginalSpawnPlayActorTailQueries;
constexpr Id Level=100u,Pawn=200u,Driver=300u;
constexpr std::size_t Unlimited=std::numeric_limits<std::size_t>::max();
std::size_t checks{},cases{};
void Require(bool value,const char* why){++checks;if(!value)throw std::runtime_error(why);}
struct Injected {};
struct Mock {
    QuestVr::NativeWork::Budget work{QuestVr::NativeWork::MaximumLimit};
    bool outer{true},allAdmin{},travelCalled{};
    Id driver{};
    std::uint8_t role{9u},remoteRole{8u};
    std::int32_t showFlags{7},rendMap{6};
    std::uint32_t packedAdmin{0x5a5a5a5au};
    std::u16string commandLine{u"actual command line"};
    std::size_t failAt{Unlimited},calls{};
    std::vector<std::string> events;
    std::function<void(Mock&,const std::string&)> after;
    void Enter(const std::string& event){events.push_back(event);if(calls++==failAt)throw Injected{};}
    void After(const std::string& event){if(after)after(*this,event);}
    void ActualPawn(Id pawn){Require(pawn==Pawn,"Store/event substituted a mutable viewport actor for captured Login pawn");}
    Queries Bind(){
        Queries q;
        q.work=[this]{work.Step();};
        q.requireOuterRoot=[this]{Enter("root");if(!outer)throw Injected{};After("root");};
        q.writeRole=[this](Id pawn,std::uint8_t value){ActualPawn(pawn);Enter("role");role=value;After("role");};
        q.writeRemoteRole=[this](Id pawn,std::uint8_t value){ActualPawn(pawn);Enter("remote-role");remoteRole=value;After("remote-role");};
        q.writeShowFlags=[this](Id pawn,std::int32_t value){ActualPawn(pawn);Enter("show-flags");showFlags=value;After("show-flags");};
        q.writeRendMap=[this](Id pawn,std::int32_t value){ActualPawn(pawn);Enter("rend-map");rendMap=value;After("rend-map");};
        q.appCmdLine=[this]()->std::u16string_view{Enter("command-line");After("command-line");return commandLine;};
        q.parseParam=[this](std::u16string_view text,std::u16string_view token){Enter("parse-param");
            Require(text.data()==commandLine.data()&&text.size()==commandLine.size(),"ParseParam did not borrow actual appCmdLine storage");
            Require(token==u"alladmin","ParseParam did not receive literal alladmin");After("parse-param");return allAdmin;};
        q.levelNetDriver=[this](Id level){Require(level==Level,"NetDriver came from another native Level");Enter("driver");After("driver");return driver;};
        q.setAdminTrue=[this](Id pawn){ActualPawn(pawn);Enter("admin-or");packedAdmin|=1u;After("admin-or");};
        q.invokeTravelPreAccept=[this](Id pawn){ActualPawn(pawn);Enter("travel-pre-accept");travelCalled=true;After("travel-pre-accept");};
        return q;
    }
    void Run(std::uint8_t remote=2u){const auto q=Bind();QuestVr::RunOriginalSpawnPlayActorPreTravel(Level,Pawn,remote,q);}
};
template<class Function> bool Refuses(Function&& function){try{function();}catch(const std::exception&){return true;}catch(const Injected&){return true;}return false;}
const std::vector<std::string> StandaloneOrder{"root","role","remote-role","show-flags","rend-map",
    "command-line","parse-param","driver","admin-or","travel-pre-accept"};
void ExactOrderAndValues(){
    ++cases;Mock m;m.Run();
    Require(m.events==StandaloneOrder,"Independent literal post-Possess provider order differs from native source");
    Require(m.role==4u&&m.remoteRole==2u&&m.showFlags==0x480c&&m.rendMap==5,"Original role/display scalar values differ");
    Require(m.packedAdmin==0x5a5a5a5bu&&m.travelCalled,"Standalone admin OR or checked TravelPreAccept was omitted");
    for(const std::uint8_t remote:{std::uint8_t{0u},std::uint8_t{2u},std::uint8_t{255u}}){
        ++cases;Mock x;x.Run(remote);Require(x.remoteRole==remote,"Supplied native remote-role byte was normalized/substituted");
    }
    // Every callback receives the captured Pawn even if another event changes
    // unrelated identities. No viewport.Actor re-query belongs to this prefix.
    ++cases;Mock changed;Id mutableViewportActor=Pawn;changed.after=[&](Mock&,const std::string& event){if(event=="role")mutableViewportActor=999u;};
    changed.Run();Require(mutableViewportActor==999u&&changed.travelCalled,"Captured-pawn dispatch depended on mutable viewport actor");
}
void AdminBranchesAndLaziness(){
    ++cases;Mock parsed;parsed.allAdmin=true;parsed.driver=Driver;auto q=parsed.Bind();q.levelNetDriver={};
    QuestVr::RunOriginalSpawnPlayActorPreTravel(Level,Pawn,2u,q);
    auto parsedOrder=StandaloneOrder;parsedOrder.erase(std::find(parsedOrder.begin(),parsedOrder.end(),"driver"));
    Require(parsed.events==parsedOrder&&parsed.packedAdmin==0x5a5a5a5bu,"alladmin did not short-circuit only the native driver read");
    for(const auto initial:{std::uint32_t{0x5a5a5a5au},std::uint32_t{0x5a5a5a5bu}}){
        ++cases;Mock network;network.driver=Driver;network.packedAdmin=initial;auto n=network.Bind();n.setAdminTrue={};
        QuestVr::RunOriginalSpawnPlayActorPreTravel(Level,Pawn,2u,n);
        auto networkOrder=StandaloneOrder;networkOrder.erase(std::find(networkOrder.begin(),networkOrder.end(),"admin-or"));
        Require(network.events==networkOrder&&network.packedAdmin==initial&&network.travelCalled,
            "False admin condition wrote false/adjacent bits or omitted later TravelPreAccept");
    }
    ++cases;Mock mustQuery;auto missing=mustQuery.Bind();missing.appCmdLine={};
    Require(Refuses([&]{QuestVr::RunOriginalSpawnPlayActorPreTravel(Level,Pawn,2u,missing);})&&!mustQuery.travelCalled&&
        mustQuery.packedAdmin==0x5a5a5a5au,"Standalone None bypassed required earlier appCmdLine authority");
    ++cases;Mock mustParse;auto noParse=mustParse.Bind();noParse.parseParam={};
    Require(Refuses([&]{QuestVr::RunOriginalSpawnPlayActorPreTravel(Level,Pawn,2u,noParse);})&&!mustParse.travelCalled&&
        std::find(mustParse.events.begin(),mustParse.events.end(),"driver")==mustParse.events.end(),
        "Standalone None bypassed required earlier ParseParam authority");
    ++cases;Mock mustHaveDriver;auto noDriver=mustHaveDriver.Bind();noDriver.levelNetDriver={};
    Require(Refuses([&]{QuestVr::RunOriginalSpawnPlayActorPreTravel(Level,Pawn,2u,noDriver);})&&!mustHaveDriver.travelCalled&&
        mustHaveDriver.packedAdmin==0x5a5a5a5au,"Missing native driver provider became fabricated None");
}
void RefusalAndBorrowedRoot(){
    ++cases;Mock root;root.outer=false;
    Require(Refuses([&]{root.Run();})&&root.events==std::vector<std::string>{"root"}&&root.role==9u&&!root.travelCalled,
        "Prefix bypassed required caller root or created an independent transaction");
    for(const Id level:{Id{0u},Level})for(const Id pawn:{Id{0u},Pawn})if(!level||!pawn){
        ++cases;Mock missing;const auto q=missing.Bind();
        Require(Refuses([&]{QuestVr::RunOriginalSpawnPlayActorPreTravel(level,pawn,2u,q);})&&missing.events.empty(),
            "Invalid actual Level/captured Login pawn reached providers");
    }
    const std::vector<std::function<void(Queries&)>> clearers{
        [](Queries& q){q.requireOuterRoot={};},[](Queries& q){q.writeRole={};},
        [](Queries& q){q.writeRemoteRole={};},[](Queries& q){q.writeShowFlags={};},
        [](Queries& q){q.writeRendMap={};},[](Queries& q){q.appCmdLine={};},
        [](Queries& q){q.parseParam={};},[](Queries& q){q.levelNetDriver={};},
        [](Queries& q){q.setAdminTrue={};},[](Queries& q){q.invokeTravelPreAccept={};}};
    for(std::size_t i=0u;i<clearers.size();++i){
        ++cases;Mock m;auto q=m.Bind();clearers[i](q);
        Require(Refuses([&]{QuestVr::RunOriginalSpawnPlayActorPreTravel(Level,Pawn,2u,q);})&&!m.travelCalled&&m.events.size()==i,
            "Missing reached provider was swallowed, eagerly validated, or treated as a successful no-op");
        Require(std::equal(m.events.begin(),m.events.end(),StandaloneOrder.begin()),"Missing-provider path reordered prior native effects");
    }
    for(std::size_t failure=0u;failure<StandaloneOrder.size();++failure){
        ++cases;Mock m;m.failAt=failure;
        Require(Refuses([&]{m.Run();})&&!m.travelCalled&&m.events.size()==failure+1u,
            "Reached provider exception was swallowed or later events ran");
        Require(std::equal(m.events.begin(),m.events.end(),StandaloneOrder.begin()),"Provider failure reordered reached prefix");
        if(failure>1u)Require(m.role==4u,"Helper locally rolled back stores instead of leaving outer VM journal responsible");
    }
    ++cases;Mock noWork;auto q=noWork.Bind();q.work={};
    Require(Refuses([&]{QuestVr::RunOriginalSpawnPlayActorPreTravel(Level,Pawn,2u,q);})&&noWork.events.empty(),
        "Prefix executed outside borrowed native work accounting");
    ++cases;Mock measured;measured.work.Step(17u);measured.Run();
    Require(measured.work.Read().used==17u+StandaloneOrder.size(),"Prefix reset or failed to charge caller native work");
    for(std::size_t limit=0u;limit<StandaloneOrder.size();++limit){
        ++cases;Mock cut;cut.work.Reset(limit);
        Require(Refuses([&]{cut.Run();})&&!cut.travelCalled&&cut.events.size()==limit,
            "Work cut did not stop before exactly the next reached provider");
        Require(std::equal(cut.events.begin(),cut.events.end(),StandaloneOrder.begin()),"Work cut reordered original effects");
    }
    ++cases;Mock exact;exact.work.Reset(StandaloneOrder.size());exact.Run();
    Require(exact.travelCalled&&exact.work.Read().used==StandaloneOrder.size(),"Exact borrowed work boundary refused valid prefix");
}
struct TravelMock {
    QuestVr::NativeWork::Budget work{QuestVr::NativeWork::MaximumLimit};
    std::u16string defaultName{u"ActualURLName"};
    std::vector<QuestVr::OriginalSpawnPlayActorTravelEntryView> travel;
    std::vector<std::string> events;
    std::size_t reads{},noneAt{Unlimited},failAt{Unlimited},calls{};
    bool outer{true},validInfo{true},differentSlots{};
    std::uint8_t netMode{};
    Id checkedInfo{},modeInfo{};
    void Enter(const std::string& event){events.push_back(event);if(calls++==failAt)throw Injected{};}
    QuestVr::OriginalSpawnPlayActorTravelQueries Bind(){
        QuestVr::OriginalSpawnPlayActorTravelQueries q;
        q.work=[this]{work.Step();};
        q.requireOuterRoot=[this]{Enter("root");if(!outer)throw Injected{};};
        q.defaultName=[this]()->std::u16string_view{Enter("default-name");return defaultName;};
        q.slotZero=[this](Id level){Require(level==Level,"Travel fallback read another native Level");Enter("slot");
            ++reads;return reads==noneAt?Id{0u}:(differentSlots?Id{500u+reads}:Id{500u});};
        q.isLevelInfo=[this](Id info){Enter("is-info");checkedInfo=info;return validInfo;};
        q.levelInfoNetMode=[this](Id info){Enter("net-mode");modeInfo=info;return netMode;};
        return q;
    }
    QuestVr::OriginalSpawnPlayActorTravelSelection Run(const std::vector<std::u16string_view>& options={}){
        const auto q=Bind();return QuestVr::OriginalSpawnPlayActorSelectTravel(Level,{u"",options},travel,q);
    }
};
const std::vector<std::string> TravelFallbackOrder{"root","default-name","slot","slot","is-info","slot","net-mode"};
void TravelOptionAndMapSelection(){
    ++cases;TravelMock empty;const auto none=empty.Run();
    Require(!none.loadOnly&&!none.selected&&empty.events==TravelFallbackOrder&&empty.reads==3u,
        "Actual empty travel map omitted default/fresh slot/NetMode services or fabricated a selected CString");
    // Source HasOption uses full appStricmp, unlike GetOption's prefix check.
    for(const auto option:{u"loadonly",u"LOADONLY",u"LoAdOnLy"}){
        ++cases;TravelMock load;load.travel={{u"ActualURLName",u"unsupported nonempty"}};auto q=load.Bind();
        q.defaultName={};q.slotZero={};q.isLevelInfo={};q.levelInfoNetMode={};
        const std::vector<std::u16string_view> options{option};
        const auto selected=QuestVr::OriginalSpawnPlayActorSelectTravel(Level,{u"",options},load.travel,q);
        QuestVr::RequireOriginalSpawnPlayActorEmptyTravellers(selected);
        Require(selected.loadOnly&&!selected.selected&&load.events==std::vector<std::string>{"root"},
            "Exact loadonly did not skip ONLY travel selection/import or consulted later missing providers");
    }
    for(const auto option:{u"loadonly=1",u"xloadonly",u"loadonlyx",u" loadonly"}){
        ++cases;TravelMock mismatch;const auto selected=mismatch.Run({option});
        Require(!selected.loadOnly&&!selected.selected&&mismatch.events==TravelFallbackOrder,
            "HasOption was incorrectly treated as prefix/whitespace matching");
    }
    ++cases;TravelMock fallback;fallback.travel={{u"ActualURLName",u"default-selected"},{u"JC",u"explicit-selected"}};
    const auto byDefault=fallback.Run();
    Require(byDefault.selected==std::optional<std::u16string_view>{u"default-selected"}&&
        fallback.events==std::vector<std::string>{"root","default-name"},"Actual FURL.DefaultName was replaced or lookup match still fell back");
    ++cases;TravelMock explicitName;explicitName.travel=fallback.travel;
    const auto first=explicitName.Run({u"xNAME=No",u"nAmE=JC",u"NAME=ActualURLName"});
    Require(first.selected==std::optional<std::u16string_view>{u"explicit-selected"}&&
        explicitName.events==std::vector<std::string>{"root","default-name"},"GetOption did not use FIRST case-insensitive NAME= suffix after evaluating genuine default");
    ++cases;TravelMock emptyName;emptyName.travel={{u"ActualURLName",u"wrong-default"},{u"",u"empty-name"}};
    const auto byEmpty=emptyName.Run({u"NAME=",u"NAME=ActualURLName"});
    Require(byEmpty.selected==std::optional<std::u16string_view>{u"empty-name"},"Present-empty NAME= was mistaken for absent option/default");
    ++cases;TravelMock duplicate;duplicate.travel={{u"JC",u"first-nonempty"},{u"jc",u""}};
    const auto last=duplicate.Run({u"NAME=JC"});
    QuestVr::RequireOriginalSpawnPlayActorEmptyTravellers(last);
    Require(last.selected&&last.selected->empty()&&duplicate.events==std::vector<std::string>{"root","default-name"},
        "Native rehashed duplicate-last lookup or nonnull empty suppression of fallback was lost");
    ++cases;TravelMock noMatch;noMatch.travel={{u"Other",u"first-serialized"},{u"Another",u"last-serialized"}};
    const auto firstFallback=noMatch.Run({u"NAME=JC"});
    Require(firstFallback.selected==std::optional<std::u16string_view>{u"first-serialized"}&&noMatch.events==TravelFallbackOrder,
        "Standalone miss did not select FIRST serialized travel entry after fresh NetMode check");
    ++cases;TravelMock network;network.travel=noMatch.travel;network.netMode=1u;
    const auto networkMiss=network.Run({u"NAME=JC"});
    Require(!networkMiss.selected&&network.events==TravelFallbackOrder,"Non-standalone NetMode invented first-entry fallback");
    ++cases;TravelMock refreshed;refreshed.differentSlots=true;refreshed.Run();
    Require(refreshed.checkedInfo==502u&&refreshed.modeInfo==503u,"Fallback cached slot0 across native check/IsA/NetMode reads");
    for(const auto selection:{byDefault,first,byEmpty,firstFallback}){
        ++cases;Require(Refuses([&]{QuestVr::RequireOriginalSpawnPlayActorEmptyTravellers(selection);}),
            "Unsupported nonempty selected import was accepted as empty travellers");
    }
    ++cases;QuestVr::RequireOriginalSpawnPlayActorEmptyTravellers(none);
}
void TravelCStringsAndRefusals(){
    ++cases;TravelMock nul;
    // Explicit array extents retain the embedded suffix in borrowed storage.
    constexpr char16_t rawKey[]=u"JC\0ignored",rawValue[]=u"\0hidden",rawOption[]=u"NAME=JC\0NAME=Wrong";
    nul.travel={{std::u16string_view{rawKey,std::size(rawKey)-1u},std::u16string_view{rawValue,std::size(rawValue)-1u}}};
    const auto selected=nul.Run({std::u16string_view{rawOption,std::size(rawOption)-1u}});
    Require(selected.selected&&selected.selected->empty(),"Native key/value/option CString first-NUL semantics changed");
    QuestVr::RequireOriginalSpawnPlayActorEmptyTravellers(selected);
    ++cases;TravelMock loadNul;constexpr char16_t rawLoad[]=u"loadonly\0=not-an-option";
    Require(loadNul.Run({std::u16string_view{rawLoad,std::size(rawLoad)-1u}}).loadOnly,"HasOption compared past native CString terminator");
    ++cases;TravelMock defaultNul;constexpr char16_t rawDefault[]=u"JC\0Wrong";
    defaultNul.defaultName.assign(rawDefault,std::size(rawDefault)-1u);defaultNul.travel={{u"JC",u"selected"}};
    Require(defaultNul.Run().selected==std::optional<std::u16string_view>{u"selected"},"DefaultName native CString was not retained");
    ++cases;TravelMock missingDefault;auto q=missingDefault.Bind();q.defaultName={};
    const std::vector<std::u16string_view> explicitOptions{u"NAME=JC"};
    Require(Refuses([&]{QuestVr::OriginalSpawnPlayActorSelectTravel(Level,{u"",explicitOptions},missingDefault.travel,q);})&&
        missingDefault.events==std::vector<std::string>{"root"},"Explicit NAME= bypassed earlier genuine DefaultName service");
    for(const std::size_t read:{1u,2u,3u}){
        ++cases;TravelMock vanished;vanished.noneAt=read;
        Require(Refuses([&]{vanished.Run();})&&vanished.reads==read&&vanished.modeInfo==0u,
            "Vanishing fallback slot0 bypassed checked/safe refusal");
    }
    ++cases;TravelMock wrong;wrong.validInfo=false;
    Require(Refuses([&]{wrong.Run();})&&wrong.reads==2u&&wrong.modeInfo==0u,"Fallback wrong LevelInfo ancestry reached NetMode");
    ++cases;TravelMock locale;
    Require(Refuses([&]{locale.Run({u"loa\u017fonly"});}),"HasOption invented a non-ASCII CRT case equivalence");
    ++cases;TravelMock mapLocale;mapLocale.travel={{u"J\u0106",u""}};
    Require(Refuses([&]{mapLocale.Run({u"NAME=JC"});}),"Travel map lookup invented non-ASCII CRT/hash equivalence");
    ++cases;TravelMock prefixLocale;
    Require(Refuses([&]{prefixLocale.Run({u"NAM\u017f=JC"});}),"GetOption invented non-ASCII prefix equivalence");
    ++cases;TravelMock unicodeValue;
    const auto noMap=unicodeValue.Run({u"NAME=\u0141\u00f3d\u017a"});
    Require(!noMap.selected&&unicodeValue.events==TravelFallbackOrder,"Non-compared UTF16 option value was rejected or folded");
    ++cases;TravelMock root;root.outer=false;
    Require(Refuses([&]{root.Run();})&&root.events==std::vector<std::string>{"root"},"Travel selector bypassed existing root");
}
void TravelBorrowedWorkAndProviderFailures(){
    const std::vector<std::function<void(QuestVr::OriginalSpawnPlayActorTravelQueries&)>> clearers{
        [](auto& q){q.requireOuterRoot={};},[](auto& q){q.defaultName={};},
        [](auto& q){q.slotZero={};},[](auto& q){q.isLevelInfo={};},[](auto& q){q.levelInfoNetMode={};}};
    const std::vector<std::size_t> reached{0u,1u,2u,4u,6u};
    for(std::size_t i=0u;i<clearers.size();++i){
        ++cases;TravelMock m;auto q=m.Bind();clearers[i](q);
        Require(Refuses([&]{QuestVr::OriginalSpawnPlayActorSelectTravel(Level,{u"",{}},m.travel,q);})&&m.events.size()==reached[i],
            "Missing reached travel provider was swallowed, eagerly validated, or replaced with None");
        Require(std::equal(m.events.begin(),m.events.end(),TravelFallbackOrder.begin()),"Missing travel provider changed earlier source order");
    }
    for(std::size_t fail=0u;fail<TravelFallbackOrder.size();++fail){
        ++cases;TravelMock m;m.failAt=fail;
        Require(Refuses([&]{m.Run();})&&m.events.size()==fail+1u,"Travel provider failure was swallowed");
        Require(std::equal(m.events.begin(),m.events.end(),TravelFallbackOrder.begin()),"Travel failure changed reached provider order");
    }
    ++cases;TravelMock noWork;auto q=noWork.Bind();q.work={};
    Require(Refuses([&]{QuestVr::OriginalSpawnPlayActorSelectTravel(Level,{u"",{}},noWork.travel,q);})&&noWork.events.empty(),
        "Travel selection bypassed borrowed root work");
    ++cases;TravelMock noLevel;auto noLevelQuery=noLevel.Bind();
    Require(Refuses([&]{QuestVr::OriginalSpawnPlayActorSelectTravel(0u,{u"",{}},noLevel.travel,noLevelQuery);})&&noLevel.events.empty(),
        "Travel selector accepted missing actual Level");
    TravelMock measured;measured.work.Step(17u);measured.travel={{u"Other",u""},{u"JC",u""}};
    measured.Run({u"Name=JC"});const auto used=measured.work.Read().used-17u;
    Require(used>0u&&measured.work.Read().used>17u,"Travel selection reset caller's work");
    for(std::size_t limit=0u;limit<used;++limit){
        ++cases;TravelMock cut;cut.travel=measured.travel;cut.work.Reset(limit);bool returned{};
        Require(Refuses([&]{cut.Run({u"Name=JC"});returned=true;})&&!returned,"Travel work cut reached an uncharged successful result");
    }
    ++cases;TravelMock exact;exact.travel=measured.travel;exact.work.Reset(used);
    Require(exact.Run({u"Name=JC"}).selected==std::optional<std::u16string_view>{u""}&&exact.work.Read().used==used,
        "Exact borrowed travel work boundary changed result or accounting");
}
} // namespace

int main(){try{ExactOrderAndValues();AdminBranchesAndLaziness();RefusalAndBorrowedRoot();
    TravelOptionAndMapSelection();TravelCStringsAndRefusals();TravelBorrowedWorkAndProviderFailures();
    std::cout<<"SpawnPlayActor post-Possess pre-travel provider mocks: "<<checks<<" checks / "<<cases
        <<" cases; not original runtime/startup/Training acceptance\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
