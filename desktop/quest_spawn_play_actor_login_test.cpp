#include "quest_spawn_play_actor_login.h"
#include "quest_native_work.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iostream>
#include <limits>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

// PROVIDER MOCKS ONLY. This does not execute original DLLs or original scripts,
// initialize a real FURL/client/map, certify Login, or complete SpawnPlayActor.
namespace {
using Id=QuestVr::SpawnPlayActorNativeId;
constexpr Id Level=100u,Player=200u,Sandbox=300u,Base=400u,Info=500u,Game=600u,Class=700u,Pawn=800u;
constexpr std::size_t Unlimited=std::numeric_limits<std::size_t>::max();
std::size_t checks{},cases{};
void Require(bool value,const char* why){++checks;if(!value)throw std::runtime_error(why);}
struct Injected {};
struct Fatal {};
struct Load {Id base{},sandbox{};std::u16string token;bool operator==(const Load&)const=default;};
struct Login {Id game{},cls{};std::u16string portal,options,error;bool operator==(const Login&)const=default;};
struct Mock {
    QuestVr::NativeWork::Budget work{QuestVr::NativeWork::MaximumLimit};
    std::size_t scratch{71u},baseline{71u},peak{},byteLimit{Unlimited};
    std::size_t calls{},failAt{Unlimited},slotReads{},noneAt{Unlimited},wrongAt{Unlimited};
    bool outer{true},netConnection{},fatalReturns{},gamePresent{true};
    bool calledLogin{};Id returned{Pawn};
    std::u16string error{u"previous error"};
    std::vector<std::string> events;
    std::vector<Load> loads;
    std::vector<std::u16string> assigned,logged;
    std::vector<Login> logins;
    std::map<std::u16string,Id> classes{{u"usr:DefaultPlayer.Class",Class}};
    std::function<void(Mock&,const std::string&)> after;
    void Enter(const std::string& event){events.push_back(event);if(calls++==failAt)throw Injected{};}
    void After(const std::string& event){if(after)after(*this,event);}
    QuestVr::OriginalSpawnPlayActorLoginQueries Bind(){
        QuestVr::OriginalSpawnPlayActorLoginQueries q;
        q.work=[this]{work.Step();};
        q.requireOuterRoot=[this]{Enter("root");if(!outer)throw Injected{};After("root");};
        q.admitTemporaryBytes=[this](std::size_t extra){Enter("admit");
            Require(extra<=Unlimited-scratch,"Scratch addition overflowed before mock admission");peak=std::max(peak,scratch+extra);
            if(scratch>byteLimit||extra>byteLimit-scratch)throw Injected{};
            After("admit");};
        q.assignOutError=[this](std::u16string_view value){Enter(value.empty()?"clear":"error");error=value;assigned.emplace_back(value);After("error");};
        q.playerIsNetConnection=[this](Id player){Require(player==Player,"NetConnection class check used another UPlayer");Enter("netconnection?");After("netconnection?");return netConnection;};
        q.netConnectionSandbox=[this](Id player){Require(player==Player&&netConnection,"Sandbox read preceded actual NetConnection class proof");Enter("sandbox");After("sandbox");return Sandbox;};
        q.playerPawnClass=[this]{Enter("base");After("base");return Base;};
        q.optionalUrlClass=[this](const QuestVr::OriginalSpawnPlayActorUrl& url){Enter("option");
            QuestVr::OriginalSpawnPlayActorLoginQueries workOnly;workOnly.work=[this]{work.Step();};
            const auto result=QuestVr::OriginalSpawnPlayActorUrlClass(url,workOnly);After("option");return result;};
        q.loadPlayerClass=[this](Id base,std::u16string_view token,Id sandbox){
            Require(base==Base,"Class load was not constrained to the original PlayerPawn UClass");
            Require(sandbox==(netConnection?Sandbox:0u),"Class load did not retain the authenticated NetConnection sandbox/standalone None");
            Enter("load");loads.push_back({base,sandbox,std::u16string(token)});After("load");
            const auto found=classes.find(std::u16string(token));return found==classes.end()?0u:found->second;};
        q.fatalLoadPlayerClass=[this]{Enter("fatal");After("fatal");if(!fatalReturns)throw Fatal{};};
        q.slotZero=[this](Id level){Require(level==Level,"Actor array read used a different native Level");Enter("slot");
            ++slotReads;After("slot");return slotReads==noneAt?0u:Info;};
        q.isLevelInfo=[this](Id info){Require(info==Info,"LevelInfo IsA checked a foreign/None identity");Enter("levelinfo?");
            After("levelinfo?");return slotReads!=wrongAt;};
        q.levelInfoGame=[this](Id info){Require(info==Info,"Game came from a non-LevelInfo actor");Enter("game");After("game");return gamePresent?Game:0u;};
        q.logError=[this](std::u16string_view value){Enter("log");logged.emplace_back(value);After("log");};
        q.invokeLogin=[this](Id game,std::u16string_view portal,std::u16string_view options,Id cls){Enter("login");
            Require(game==Game&&cls==Class,"Login dispatch receiver or actual loaded class was substituted");
            Require(scratch==baseline+2048u,"Fixed native options buffer was not charged through Login");
            calledLogin=true;logins.push_back({game,cls,std::u16string(portal),std::u16string(options),error});
            error=u"Login OUT result";After("login");return returned;};
        return q;
    }
    QuestVr::OriginalSpawnPlayActorLoginResult Run(const std::vector<std::u16string_view>& options={},
        std::u16string_view portal=u"TrainingEntry",Id player=Player){
        auto q=Bind();
        // Native None UPlayer does not select a NetConnection sandbox either.
        if(!player)q.loadPlayerClass=[this](Id base,std::u16string_view token,Id sandbox){
            Require(base==Base&&sandbox==0u,"None UPlayer acquired a NetConnection sandbox");Enter("load");
            loads.push_back({base,sandbox,std::u16string(token)});const auto it=classes.find(std::u16string(token));
            return it==classes.end()?0u:it->second;};
        return QuestVr::RunOriginalSpawnPlayActorLoginPrefix(Level,player,{portal,options},q,scratch);
    }
};
template<class Function> bool Refuses(Function&& function){try{function();}catch(const std::exception&){return true;}
    catch(const Injected&){return true;}catch(const Fatal&){return true;}return false;}
void Preserved(const Mock& mock){Require(mock.scratch==mock.baseline,"Borrowed scratch was not restored exactly");}
void Selection(){
    ++cases;Mock m;const auto result=m.Run({u"Name=JC",u"Team=0",u""});
    Require(result.outcome==QuestVr::OriginalSpawnPlayActorLoginOutcome::LoginReturned&&result.player==Pawn,"Actual Login return was not retained");
    Require(m.loads==std::vector<Load>{{Base,0u,u"usr:DefaultPlayer.Class"}},"Standalone Viewport CLASS fallback did not retain sandbox None");
    Require(m.logins==std::vector<Login>{{Game,Class,u"TrainingEntry",u"?Name=JC?Team=0?",u""}},"Literal Portal/Options/cleared OUT/Class frame differs from original order");
    Require(m.error==u"Login OUT result"&&m.slotReads==8u,"Login OUT copyback or repeated native slot reads were lost");
    const std::vector<std::string> expected{"root","clear","netconnection?","option","base","load","admit",
        "slot","slot","levelinfo?","slot","slot","levelinfo?","slot","game",
        "slot","slot","levelinfo?","slot","game","login"};
    Require(m.events==expected,"Independent literal provider ordering differs from original reached prefix");Preserved(m);
    for(const auto token:{u"Class=Chosen",u"cLaSs=Chosen",u"CLASS=Chosen"}){
        ++cases;Mock x;x.classes.clear();x.classes[u"Chosen"]=Class;
        x.Run({u"notCLASS=Wrong",token,u"CLASS=Later"});
        Require(x.loads.size()==1u&&x.loads.front().token==u"Chosen","GetOption did not select first case-insensitive CLASS prefix");Preserved(x);
    }
    ++cases;Mock empty;empty.classes[u""]=Class;empty.Run({u"CLASS=",u"CLASS=Later"});
    Require(empty.loads.size()==1u&&empty.loads.front().token.empty(),"Present-empty CLASS was confused with absent CLASS");Preserved(empty);
    ++cases;Mock fallback;fallback.classes.clear();fallback.classes[u"ini:URL.Class"]=Class;
    fallback.Run({u"CLASS=Bad",u"Name=X"});
    Require(fallback.loads==std::vector<Load>{{Base,0u,u"Bad"},{Base,0u,u"usr:DefaultPlayer.Class"},{Base,0u,u"ini:URL.Class"}},
        "Failed explicit/user class did not reach exact ordered ini fallback");Preserved(fallback);
    ++cases;Mock user;user.Run({u"CLASS=Bad"});
    Require(user.loads.size()==2u&&user.loads.back().token==u"usr:DefaultPlayer.Class","Failed explicit class bypassed user fallback");Preserved(user);
    ++cases;Mock viewport;viewport.Run();
    Require(std::find(viewport.events.begin(),viewport.events.end(),"sandbox")==viewport.events.end(),"Standalone Viewport read native NetConnection+A8");Preserved(viewport);
    ++cases;Mock connection;connection.netConnection=true;connection.Run();
    Require(connection.loads==std::vector<Load>{{Base,Sandbox,u"usr:DefaultPlayer.Class"}}&&
        std::find(connection.events.begin(),connection.events.end(),"sandbox")!=connection.events.end(),
        "Actual NetConnection predicate did not retain its native+A8 sandbox");Preserved(connection);
    ++cases;Mock none;none.Run({},u"",0u);
    Require(std::find(none.events.begin(),none.events.end(),"netconnection?")==none.events.end(),"None UPlayer was dereferenced");Preserved(none);
    ++cases;Mock loginNone;loginNone.returned=0u;const auto nullResult=loginNone.Run();
    Require(nullResult.outcome==QuestVr::OriginalSpawnPlayActorLoginOutcome::LoginReturned&&!nullResult.player&&loginNone.calledLogin,
        "Real Login None was invented into a pawn or prefix validation error");Preserved(loginNone);
}
void CStringsAndBounds(){
    ++cases;Mock nul;const std::u16string option{u"CLASS=Chosen\0CLASS=Hidden",25u};
    const std::u16string portal{u"Actual\0Hidden",13u};nul.classes.clear();nul.classes[u"Chosen"]=Class;
    nul.Run({option},portal);
    Require(nul.loads.front().token==u"Chosen"&&nul.logins.front().options==u"?CLASS=Chosen"&&nul.logins.front().portal==u"Actual",
        "Original CString first-NUL semantics changed");Preserved(nul);
    ++cases;Mock boundary;const std::u16string exact(1022u,u'x');boundary.Run({exact});
    Require(boundary.logins.front().options.size()==1023u,"Largest native options buffer payload was not retained");Preserved(boundary);
    ++cases;Mock overflow;const std::u16string excess(1023u,u'x');
    Require(Refuses([&]{overflow.Run({excess});})&&!overflow.calledLogin,"Native buffer overflow was truncated or dispatched");Preserved(overflow);
    ++cases;Mock split;const std::u16string a(500u,u'a'),b(521u,u'b');split.Run({a,b});
    Require(split.logins.front().options.size()==1023u,"Independent multi-option buffer arithmetic differs");Preserved(split);
    ++cases;Mock extraEmpty;Require(Refuses([&]{extraEmpty.Run({a,b,u""});})&&!extraEmpty.calledLogin,
        "Trailing empty option bypassed its mandatory question mark budget");Preserved(extraEmpty);
    ++cases;Mock many;const std::vector<std::u16string_view> emptyOptions(1023u);many.Run(emptyOptions);
    Require(many.logins.front().options==std::u16string(1023u,u'?'),"Empty options lost original order or separators");Preserved(many);
    ++cases;Mock locale;Require(Refuses([&]{locale.Run({u"CLA\u017fS=Chosen"});})&&!locale.calledLogin,
        "Unestablished CRT non-ASCII prefix case mapping was guessed");Preserved(locale);
    ++cases;Mock unicodeValue;unicodeValue.Run({u"Name=\u0141\u00f3d\u017a"});
    Require(unicodeValue.logins.front().options==u"?Name=\u0141\u00f3d\u017a","Unrelated UTF16 option value was rejected or folded");Preserved(unicodeValue);
}
void Failures(){
    for(const bool fatalReturns:{false,true}){
        ++cases;Mock m;m.classes.clear();m.fatalReturns=fatalReturns;
        Require(Refuses([&]{m.Run({u"CLASS=Bad"});})&&!m.calledLogin&&m.loads.size()==3u,
            "All class loads failing returned normal None or omitted fatal error");
        Require(m.error.empty()&&m.peak==0u,"Fatal class failure skipped OUT clear or allocated later options");Preserved(m);
    }
    ++cases;Mock noGame;noGame.gamePresent=false;const auto result=noGame.Run();
    Require(result.outcome==QuestVr::OriginalSpawnPlayActorLoginOutcome::NoGame&&!result.player&&!noGame.calledLogin&&
        noGame.error==u"Couldn't spawn Player Actor: No Game"&&noGame.logged==std::vector<std::u16string>{u"No GameInfo"},
        "Authoritative no-Game failure changed its OUT error/log/None result");Preserved(noGame);
    ++cases;Mock disappearing;disappearing.noneAt=3u;const auto none=disappearing.Run();
    Require(none.outcome==QuestVr::OriginalSpawnPlayActorLoginOutcome::NoLevelInfo&&!none.player&&!disappearing.calledLogin&&
        disappearing.error==u"Couldn't spawn Player Actor: No LevelInfo"&&
        disappearing.logged==std::vector<std::u16string>{u"Login failed, no LevelInfo"},"Native no-LevelInfo recheck did not retain exact failure");Preserved(disappearing);
    for(const std::size_t read:{1u,2u,4u,5u,6u,7u,8u}){
        ++cases;Mock m;m.noneAt=read;Require(Refuses([&]{m.Run();})&&!m.calledLogin,"Invalid native slot-zero storage bypassed assertion/safety refusal");Preserved(m);
    }
    for(const std::size_t read:{2u,4u,7u}){
        ++cases;Mock m;m.wrongAt=read;Require(Refuses([&]{m.Run();})&&!m.calledLogin,"Wrong actual slot-zero class bypassed original LevelInfo IsA");Preserved(m);
    }
    ++cases;Mock root;root.outer=false;Require(Refuses([&]{root.Run();})&&root.error==u"previous error"&&root.loads.empty(),
        "Prefix created or bypassed the outer root");Preserved(root);
    // Missing UNREACHED providers do not alter reached original branches.
    ++cases;Mock reached;auto q=reached.Bind();q.netConnectionSandbox={};
    QuestVr::RunOriginalSpawnPlayActorLoginPrefix(Level,Player,{u"",{}},q,reached.scratch);Preserved(reached);
    ++cases;Mock missing;auto bad=missing.Bind();bad.invokeLogin={};
    Require(Refuses([&]{QuestVr::RunOriginalSpawnPlayActorLoginPrefix(Level,Player,{u"",{}},bad,missing.scratch);})&&
        missing.error.empty()&&!missing.calledLogin,"Missing reached Login provider became success or internally undid OUT clear");Preserved(missing);
}
void BorrowedWorkAndAdmission(){
    Mock reference;reference.work.Step(17u);reference.Run({u"Name=Player",u"CLASS=Bad"});
    const auto totalUsed=reference.work.Read().used;Require(totalUsed>17u,"Native prefix reset its borrowed work");
    const auto used=totalUsed-17u;
    for(std::size_t limit=0u;limit<used;++limit){
        ++cases;Mock m;m.work.Reset(limit);Require(Refuses([&]{m.Run({u"Name=Player",u"CLASS=Bad"});}),"Work exhaustion did not interrupt the reached prefix");Preserved(m);
    }
    for(const auto limit:{reference.baseline+2047u,reference.baseline+2048u}){
        ++cases;Mock m;m.byteLimit=limit;const bool refused=Refuses([&]{m.Run();});
        Require(refused==(limit==m.baseline+2047u),"Fixed buffer admission boundary is not exact");Preserved(m);
    }
    for(std::size_t failure=0u;failure<reference.calls;++failure){
        ++cases;Mock m;m.failAt=failure;Require(Refuses([&]{m.Run({u"Name=Player",u"CLASS=Bad"});}),"Provider failure was swallowed");Preserved(m);
    }
    ++cases;Mock m;auto q=m.Bind();q.admitTemporaryBytes={};
    Require(Refuses([&]{QuestVr::RunOriginalSpawnPlayActorLoginPrefix(Level,Player,{u"",{}},q,m.scratch);})&&!m.calledLogin,
        "Unadmitted fixed buffer reached Login");Preserved(m);
    ++cases;Mock overflow;overflow.scratch=Unlimited-100u;overflow.baseline=overflow.scratch;
    Require(Refuses([&]{overflow.Run();})&&!overflow.calledLogin,"Borrowed scratch overflow bypassed admission arithmetic");Preserved(overflow);
}
void IndependentOrderedOracle(){
    // Independent explicit outputs, not a second copy of the selection loop.
    const std::array<std::vector<std::u16string_view>,6u> inputs{{
        {u"Name=Player"},{u"class=One",u"CLASS=Two"},{u"xCLASS=No",u"CLASS=Two"},
        {u"CLASS",u"Class=One",u""},{u"CLASS=",u"CLASS=Two"},{u"CLASS=One=Tail",u"CLASS=Two"}}};
    const std::array<std::optional<std::u16string_view>,6u> selected{{std::nullopt,u"One",u"Two",u"One",u"",u"One=Tail"}};
    const std::array<std::u16string_view,6u> assembled{{u"?Name=Player",u"?class=One?CLASS=Two",u"?xCLASS=No?CLASS=Two",
        u"?CLASS?Class=One?",u"?CLASS=?CLASS=Two",u"?CLASS=One=Tail?CLASS=Two"}};
    for(std::size_t i=0u;i<inputs.size();++i){
        ++cases;Mock m;auto q=m.Bind();const auto actual=QuestVr::OriginalSpawnPlayActorUrlClass({u"",inputs[i]},q);
        Require(actual==selected[i],"Independent literal GetOption oracle mismatch");
        if(selected[i])m.classes[std::u16string(*selected[i])]=Class;
        m.Run(inputs[i]);Require(m.logins.front().options==assembled[i],"Independent ordered Options oracle mismatch");Preserved(m);
    }
}
} // namespace
int main(){try{Selection();CStringsAndBounds();Failures();BorrowedWorkAndAdmission();IndependentOrderedOracle();
    std::cout<<"SpawnPlayActor pre-Login provider-mock controls: "<<checks<<" checks / "<<cases
        <<" cases; not original script/runtime/Training acceptance\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
