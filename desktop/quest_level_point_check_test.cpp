#include "quest_level_point_check.h"
#include "quest_native_hit_marks.h"
#include <iostream>
#include <cstring>
namespace {
using namespace QuestVr;std::size_t checks{},refusals{};
void Need(bool v,const char* why){++checks;if(!v)throw std::runtime_error(why);}
template<class F>void Refuse(F&& f){bool yes{};try{f();}catch(const std::exception&){yes=true;}Need(yes,"Expected refusal");++refusals;}
bool Same(const OriginalActorLineHit& a,const OriginalActorLineHit& b){return std::memcmp(&a,&b,sizeof(a))==0;}
void Run(){
    NativeHitMarks marks;NativeHitRootStamp stamp{1,2,3,4};marks.BeginRoot(stamp);
    std::size_t scratch{},work{};OriginalLineHitArena supplied;std::uintptr_t hash=77;
    std::vector<int> order;OriginalMultiPointQueries multi;multi.scratchBytes=&scratch;
    multi.validate=[&](auto extra){Need(scratch+extra<1048576,"Scratch bound exceeded");};multi.work=[&]{++work;};
    multi.hash=[&]{order.push_back(1);return hash;};
    multi.actors=[&](auto h,auto,auto,auto flags){Need(h==77&&flags==19,"Actor query flags/hash changed");order.push_back(2);return supplied;};
    bool blocked{};multi.world=[&](auto world,auto owner,auto,auto,auto flags){
        Need(world==99&&!owner&&!flags,"World PointCheck owner/flags changed");order.push_back(3);
        OriginalWorldLineReply r;r.clear=!blocked;r.hit.location={3,4,0};r.hit.item=17;return r;};
    auto empty=CollectOriginalMultiPointHits({}, {},19,99,0,multi);
    Need(empty.records.empty()&&order==std::vector<int>{3}&&!scratch,"CheckActors0 touched Hash or leaked scratch");
    order.clear();blocked=true;OriginalActorLineHit one;one.actor=55;one.location={10,0,0};supplied.records={one};supplied.first=1;
    auto joined=CollectOriginalMultiPointHits({}, {},19,99,1,multi);
    Need(order==std::vector<int>({1,2,3})&&joined.records.size()==2&&joined.first==2&&
        joined.records[1].actor==99&&joined.records[1].next==1&&joined.records[1].item==17,"World hit did not prepend complete packet");
    Need(!scratch,"Collector retained temporary ledger after return");
    OriginalSinglePointQueries q;q.work=[&]{++work;};q.mark=[&]{return marks.Mark(stamp);};
    q.pop=[&](auto token)noexcept{if(!marks.Pop(token))std::terminate();};
    q.packet=[&](auto handle){return marks.Packet(stamp,handle);};
    q.multiPoint=[&](auto,auto,auto,auto,auto){return marks.Adopt(stamp,std::move(joined),[](auto){});};
    OriginalActorLineHit caller;caller.actor=123;caller.next=987;caller.time=.37f;caller.item=92;const auto before=caller;
    Need(CheckOriginalSinglePoint(caller,{}, {},19,99,1,q)==0&&caller.actor==99&&caller.item==17,"Nearest Location not selected");
    Need(!marks.MarkDepth()&&!marks.ArenaCount(),"Point selector leaked its mark");
    caller=before;joined={};Need(CheckOriginalSinglePoint(caller,{}, {},0,0,0,q)==1&&Same(caller,before),"Point miss must leave full caller untouched");
    one.actor=11;one.location={1,0,0};one.item=51;auto two=one;two.actor=12;two.location={-1,0,0};two.item=52;one.next=2;
    joined.records={one,two};joined.first=1;
    Need(CheckOriginalSinglePoint(caller,{}, {},0,0,0,q)==0&&caller.actor==11&&caller.item==51&&caller.next!=0,"Tie failed to keep first raw packet");
    const auto expired=caller.next;Refuse([&]{marks.Packet(stamp,expired);});
    caller=before;one.next=1;joined.records={one};joined.first=1;
    Refuse([&]{CheckOriginalSinglePoint(caller,{}, {},0,0,0,q,3);});
    Need(Same(caller,before)&&!marks.MarkDepth()&&!marks.ArenaCount(),"Cyclic refusal failed cleanup/unchanged caller");
    q.multiPoint=[](auto,auto,auto,auto,auto)->std::uintptr_t{throw std::runtime_error("provider");};
    Refuse([&]{CheckOriginalSinglePoint(caller,{}, {},0,0,0,q);});Need(!marks.MarkDepth(),"Provider failure leaked mark");
    multi.actors={};Refuse([&]{CollectOriginalMultiPointHits({}, {},19,0,1,multi);});
    hash=0;Need(CollectOriginalMultiPointHits({}, {},19,0,1,multi).records.empty(),"Missing native hash must skip actor provider");
    Need(work>0,"Point kernels omitted shared work");
}
}
int main(){try{Run();std::cout<<"Original Level point checks: "<<checks<<" checks / "<<refusals<<" refusals; native mark/arena lifetime controls\n";return 0;}
catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
