#include "quest_flag_base.h"
#include <iostream>
#include <limits>
#include <string>
#include <vector>
namespace {
std::size_t checks{},refusals{};
void Require(bool v,const char* why){++checks;if(!v)throw std::runtime_error(why);}
template<class F>void Refused(F&& f,const char* why){bool refused{};try{f();}catch(const std::runtime_error&){refused=true;}Require(refused,why);++refusals;}
void Tests(){
    using namespace QuestVr::FlagBase;
    Require(CrcTable[0]==0u&&CrcTable[1]==0x04c11db7u&&CrcTable[255]==0xb1f740b4u,"Original unreflected CRC table changed");
    Require(Hash(u"Mixed_Name-42")==Hash(u"MIXED_NAME-42"),"ASCII case-fold changed hash identity");
    Require(Hash(std::u16string_view(u"ABC\0ignored",11u))==Hash(u"ABC"),"Hash ignored CString termination");
    Require(Hash(u"\u00e9")!=Hash(u"\u00c9"),"Hash invented nonASCII folding");
    std::vector<Node> nodes;const auto hash=Hash(u"Test");std::size_t work{};
    Queries query{[&](std::size_t n){work+=n;},[&](std::size_t bucket){Require(bucket==(hash&63u),"Wrong original bucket");return nodes.empty()?0u:1u;},
        [&](std::uintptr_t id){return nodes.at(id-1u);}};
    const auto empty=Find(71,u"Test",2u,query);Require(!empty.node&&!empty.previous&&!empty.visited,"Empty real bucket failed lookup");
    for(unsigned count=0u;count<129u;++count)nodes.push_back({count+1u,count==128u?0u:count+2u,static_cast<int>(count),std::bit_cast<std::int32_t>(hash),2u});
    auto hit=Find(128,u"Test",2u,query);Require(hit.node==129u&&hit.previous==128u&&hit.visited==129u,"64 buckets treated as64 flag capacity");
    const auto miss=Find(999,u"Test",2u,query);Require(!miss.node&&miss.previous==129u,"Missing name lost native insertion-link predecessor");
    nodes={{1u,2u,71,std::bit_cast<std::int32_t>(hash),1u},{2u,3u,71,std::bit_cast<std::int32_t>(hash),2u},{3u,0u,71,std::bit_cast<std::int32_t>(hash),4u}};
    Require(Find(71,u"Test",2u,query).node==2u,"Exact Int type match failed");
    const auto early=Find(71,u"Test",3u,query);Require(!early.node&&early.previous==2u&&early.visited==3u,"Unsigned type early-exit changed insertion link");
    nodes={{1u,0u,999,std::numeric_limits<std::int32_t>::max(),2u}};
    Require(!Find(71,u"Test",2u,query).node&&!Find(71,u"Test",2u,query).previous,"Signed hash ordering changed early exit");
    auto missing=query;missing.node={};Refused([&]{Find(71,u"Test",2u,missing);},"Missing native provider fabricated a miss");
    auto cut=query;cut.work=[](std::size_t){throw std::runtime_error("Work exhausted");};Refused([&]{Find(71,u"Test",2u,cut);},"Native work cut accepted lookup");
    auto stale=query;stale.node=[](std::uintptr_t){return Node{};};Refused([&]{Find(71,u"Test",2u,stale);},"Changed native identity accepted");
    nodes={{1u,1u,999,std::numeric_limits<std::int32_t>::min(),2u}};
    Refused([&]{Find(71,u"Test",2u,query);},"Cyclic native chain escaped its finite work bound");
    Require(work>128u,"Native graph work was not charged");
}
}
int main(){try{Tests();std::cout<<"PASS quest_flag_base checks="<<checks<<" refusals="<<refusals<<'\n';return 0;}
catch(const std::exception& e){std::cerr<<"FAIL quest_flag_base: "<<e.what()<<'\n';return 1;}}
