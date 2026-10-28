#include "quest_player_exec_route.h"
#include "quest_engine_exec_commands.h"
#include <functional>
#include <iostream>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
std::size_t checks{},refusals{};
void Require(bool value,const char* why){++checks;if(!value)throw std::runtime_error(why);}
template<class Callback>void Refuse(Callback callback) {
    bool threw{};try{callback();}catch(const std::exception&){threw=true;}
    Require(threw,"Missing native route refusal");++refusals;
}
struct Offer {
    char kind{};int receiver{},executor{};std::u16string command;
    bool operator==(const Offer&)const=default;
};
struct Operations {
    using Handle=int;
    int actor{1};
    std::map<int,int> levels{{1,101},{2,102},{3,103}},infos{{1,201},{2,202},{3,203}},
        games{{201,301},{202,302},{203,303}},auxiliary{{1,401},{2,402},{3,403}},
        inventory{{1,501},{2,601},{3,701},{501,502},{502,0},{503,0},{601,0},{701,0}},
        engines{{101,801},{102,802},{103,803}};
    std::vector<Offer> offers;
    std::function<void(Operations&,const Offer&)> callback;
    std::function<void(Operations&,int)> inventoryRead;
    std::size_t work{},maximumWork{std::numeric_limits<std::size_t>::max()},handledAt{std::numeric_limits<std::size_t>::max()},actorReads{};
    void Step(){if(work==maximumWork)throw std::runtime_error("shared work");++work;}
    int Actor(){++actorReads;return actor;}
    int XLevel(int value){return levels[value];}
    int LevelInfo(int value){return infos[value];}
    int Game(int value){return games[value];}
    int Auxiliary(int value){return auxiliary[value];}
    int Inventory(int value){const auto next=inventory[value];if(inventoryRead)inventoryRead(*this,value);return next;}
    int Engine(int value){return engines[value];}
    bool OfferTo(char kind,int receiver,int executor,std::u16string_view command) {
        offers.push_back({kind,receiver,executor,std::u16string(command)});
        // A child's own parsing pointer is independent of the outer view.
        if(!command.empty())command.remove_prefix(1u);
        if(callback)callback(*this,offers.back());
        return offers.size()-1u==handledAt;
    }
    bool ExecLevel(int value,std::u16string_view command){return OfferTo('L',value,0,command);}
    bool ScriptConsoleExec(int value,std::u16string_view command,int executor){return OfferTo('S',value,executor,command);}
    bool ExecEngine(int value,std::u16string_view command){return OfferTo('E',value,0,command);}
};
const std::u16string Command=u" \tSET InputExt T Talk";
std::vector<Offer> Normal(){return {{'L',101,0,Command},{'S',301,1,Command},{'S',401,1,Command},
    {'S',1,1,Command},{'S',501,1,Command},{'S',502,1,Command},{'E',801,0,Command}};}
void Order() {
    Operations operations;Require(!QuestVr::PlayerExecRoute::Exec(operations,Command)&&operations.offers==Normal(),
        "Original UPlayer provider order/command/executor changed");
    Require(operations.actorReads==8u,"UPlayer did not reread Actor for every stage and inventory executor");
    for(std::size_t handled=0u;handled<Normal().size();++handled) {
        Operations first;first.handledAt=handled;
        Require(QuestVr::PlayerExecRoute::Exec(first,Command),"Handled provider did not stop routing");
        const auto expected=Normal();Require(first.offers==std::vector<Offer>(expected.begin(),expected.begin()+handled+1u),
            "Handled provider invoked a later offer or changed the command");
    }
    Operations repeated;repeated.auxiliary[1]=301;repeated.inventory[1]=301;repeated.inventory[301]=0;
    Require(!QuestVr::PlayerExecRoute::Exec(repeated,Command)&&repeated.offers.size()==6u&&
        repeated.offers[1].receiver==301&&repeated.offers[2].receiver==301&&repeated.offers[4].receiver==301,
        "Repeated actual provider identities were deduplicated");
}
void Mutable() {
    Operations changed;changed.callback=[](Operations& self,const Offer& offer){if(offer.kind=='L')self.actor=2;};
    Require(!QuestVr::PlayerExecRoute::Exec(changed,Command),"Mutation fixture unexpectedly handled");
    const std::vector<Offer> expected{{'L',101,0,Command},{'S',302,2,Command},{'S',402,2,Command},
        {'S',2,2,Command},{'S',601,2,Command},{'E',802,0,Command}};
    Require(changed.offers==expected,"Viewport Actor was captured across provider callbacks");
    Operations links;
    links.inventoryRead=[](Operations& self,int object){if(object==1)self.actor=2;};
    links.callback=[](Operations& self,const Offer& offer){if(offer.receiver==501){self.inventory[501]=503;self.actor=3;}};
    Require(!QuestVr::PlayerExecRoute::Exec(links,Command),"Live inventory fixture unexpectedly handled");
    Require(links.offers.size()==7u&&links.offers[4]==Offer{'S',501,2,Command}&&
        links.offers[5]==Offer{'S',503,3,Command}&&links.offers.back()==Offer{'E',803,0,Command},
        "Inventory next/executor/final Actor were not reread after callbacks");
    Operations cleared;cleared.callback=[](Operations& self,const Offer& offer){if(offer.kind=='L')self.actor=0;};
    Require(!QuestVr::PlayerExecRoute::Exec(cleared,Command)&&cleared.offers.size()==1u,
        "A cleared Actor retained stale later providers");
    Operations nullExecutor;nullExecutor.inventoryRead=[](Operations& self,int object){if(object==1)self.actor=0;};
    Require(!QuestVr::PlayerExecRoute::Exec(nullExecutor,Command)&&nullExecutor.offers.size()==6u&&
        nullExecutor.offers[4].executor==0&&nullExecutor.offers[5].executor==0,
        "A later None Actor suppressed an already captured inventory chain or fabricated its executor");
}
void MissingAndBounds() {
    Operations none;none.actor=0;Require(!QuestVr::PlayerExecRoute::Exec(none,Command)&&none.offers.empty(),"None Actor invoked a provider");
    for(int branch=0;branch<4;++branch) {
        Operations missing;
        if(branch==0)missing.infos[1]=0;
        if(branch==1)missing.games[201]=0;
        if(branch==2)missing.auxiliary[1]=0;
        if(branch==3)missing.inventory[1]=0;
        Require(!QuestVr::PlayerExecRoute::Exec(missing,Command),"Optional native-null branch handled command");
        Require(missing.offers.size()==(branch==3?5u:6u),"Optional native-null branch altered unrelated offers");
    }
    Operations noLevel;noLevel.levels[1]=0;Refuse([&]{static_cast<void>(QuestVr::PlayerExecRoute::Exec(noLevel,Command));});
    Require(noLevel.offers.empty(),"Missing mandatory XLevel became a provider call");
    Operations noEngine;noEngine.engines[101]=0;Refuse([&]{static_cast<void>(QuestVr::PlayerExecRoute::Exec(noEngine,Command));});
    Require(noEngine.offers.size()==6u,"Missing mandatory Engine became a silent decline or skipped earlier providers");
    Operations complete;static_cast<void>(QuestVr::PlayerExecRoute::Exec(complete,Command));
    for(std::size_t work=0u;work<complete.work;++work) {
        Operations cut;cut.maximumWork=work;Refuse([&]{static_cast<void>(QuestVr::PlayerExecRoute::Exec(cut,Command));});
    }
    Operations throwing;throwing.callback=[](Operations&,const Offer&){throw std::runtime_error("provider");};
    Refuse([&]{static_cast<void>(QuestVr::PlayerExecRoute::Exec(throwing,Command));});
    Require(throwing.offers.size()==1u,"Throwing provider continued routing");
    Operations cycle;cycle.inventory[501]=501;Refuse([&]{static_cast<void>(QuestVr::PlayerExecRoute::Exec(cycle,Command));});
    Require(cycle.offers.size()==4u+QuestVr::PlayerExecRoute::MaximumInventoryOffers&&cycle.offers.back().receiver==501,
        "Live inventory traversal was deduplicated or escaped its bound");
}
struct CommandOperations {
    std::size_t work{},maximumWork{std::numeric_limits<std::size_t>::max()},matches{},next{};
    bool throwMatch{};std::u16string matched;
    void Step(){if(work==maximumWork)throw std::runtime_error("shared discriminator work");++work;}
    void Match(std::u16string_view command,std::size_t cursor) {
        ++matches;next=cursor;matched=command;if(throwMatch)throw std::runtime_error("missing command body");
    }
};
template<std::size_t Count>
void Commands(const std::array<std::u16string_view,Count>& actual,
    const std::array<std::u16string_view,Count>& expected) {
    Require(actual==expected,"Original native command discriminator list/order changed");
    for(std::size_t index=0u;index<expected.size();++index) {
        std::u16string lower(expected[index]);for(auto& unit:lower)if(unit>=u'A'&&unit<=u'Z')unit=static_cast<char16_t>(unit+(u'a'-u'A'));
        const auto text=u" \t"+lower+u" arg";CommandOperations matched;std::size_t cursor{};
        Require(QuestVr::EngineExecCommands::Exec(matched,text,cursor,actual)&&matched.matches==1u&&
            matched.matched==expected[index]&&matched.next==expected[index].size()+3u&&cursor==matched.next&&
            matched.work==text.size()+index+1u,"Original command case/order/advancing pointer changed");
        CommandOperations suffix;cursor=0u;
        Require(!QuestVr::EngineExecCommands::Exec(suffix,std::u16string(expected[index])+u"X",cursor,actual)&&!suffix.matches,
            "Native command accepted an alphanumeric suffix");
        CommandOperations underscore;cursor=0u;
        Require(QuestVr::EngineExecCommands::Exec(underscore,std::u16string(expected[index])+u"_",cursor,actual)&&
            underscore.matched==expected[index]&&cursor==expected[index].size(),"Native command invented underscore suffix rejection");
        auto terminated=std::u16string(expected[index]);terminated.push_back(0);terminated+=u"DoNotParse";
        CommandOperations nul;cursor=0u;
        Require(QuestVr::EngineExecCommands::Exec(nul,terminated,cursor,actual)&&nul.matches==1u&&
            cursor==expected[index].size()&&nul.work==expected[index].size()+index+1u,"Native command ignored CString NUL boundary");
        CommandOperations throwing;throwing.throwMatch=true;cursor=0u;
        Refuse([&]{static_cast<void>(QuestVr::EngineExecCommands::Exec(throwing,text,cursor,actual));});
        Require(throwing.matches==1u&&throwing.matched==expected[index],"Recognized unsupported command silently declined");
        for(std::size_t work=0u;work<matched.work;++work) {
            CommandOperations cut;cut.maximumWork=work;cursor=0u;
            Refuse([&]{static_cast<void>(QuestVr::EngineExecCommands::Exec(cut,text,cursor,actual));});
        }
    }
    CommandOperations decline;std::size_t cursor{};
    Require(!QuestVr::EngineExecCommands::Exec(decline,Command,cursor,actual)&&!decline.matches&&cursor==2u&&
        decline.work==Command.size()+Count,"Native command miss altered unrelated command or skipped discriminators");
    CommandOperations large;cursor=0u;
    Refuse([&]{static_cast<void>(QuestVr::EngineExecCommands::Exec(large,std::u16string(16385u,u'x'),cursor,actual));});
    Require(!large.work&&!large.matches,"Oversized native command read before admission");
}
void Discriminators() {
    Commands(QuestVr::EngineExecCommands::Level,std::array<std::u16string_view,2u>{u"DEMOREC",u"DEMOPLAY"});
    Commands(QuestVr::EngineExecCommands::DeusExGameEngine,std::array<std::u16string_view,2u>{u"SAVEGAME",u"DELETEGAME"});
    Commands(QuestVr::EngineExecCommands::GameEngine,std::array<std::u16string_view,11u>{
        u"OPEN",u"START",u"SERVERTRAVEL",u"SAY",u"DISCONNECT",u"RECONNECT",u"GETCURRENTTICKRATE",
        u"GETMAXTICKRATE",u"GSPYLITE",u"SAVEGAME",u"CANCEL"});
}
}
int main(){try{Order();Mutable();MissingAndBounds();Discriminators();std::cout<<"PASS original UPlayer/native command route: "<<checks<<" checks / "<<refusals
    <<" refusals; explicit mock providers, exact order/repeated identities, live Actor/Inventory rereads, copied command views, Level/DeusEx/GameEngine discriminators and shared work/traversal limits. Not actual field binding or ScriptConsoleExec execution.\n";
    return 0;}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
