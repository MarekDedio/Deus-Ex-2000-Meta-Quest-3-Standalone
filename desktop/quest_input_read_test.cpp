#include "quest_input_read.h"
#include <array>
#include <bit>
#include <iostream>
#include <limits>
#include <vector>

namespace {
using namespace QuestVr;
std::size_t checks{},refusals{};
void Require(bool okay,const char* why){++checks;if(!okay)throw std::runtime_error(why);}
template<class F> void Refuse(F f){bool refused{};try{f();}catch(const std::exception&){refused=true;}Require(refused,"Expected ReadInput refusal");++refusals;}
struct Fixture {
    std::array<std::uint8_t,255u> keys{};std::array<float,4u> axes{1.0f,-2.0f,3.0f,4.0f};
    std::vector<int> events;std::size_t work{},limit{1000000u},frame{},releases{},writes{},globalLoads{};
    bool running{true},clearLater{},throwProcess{},revoked{};std::uintptr_t selectedOutput{11u};
    OriginalInputReadQueries Queries(){OriginalInputReadQueries q;
        q.validate=[&]{if(revoked)throw std::runtime_error("lease revoked");};
        q.work=[&]{if(++work>limit)throw std::runtime_error("shared work exhausted");};
        q.enterFrame=[&](auto bytes){frame+=bytes;};q.leaveFrame=[&](auto bytes)noexcept{frame-=bytes;};
        q.running=[&]{events.push_back(-1);return running;};
        q.acquireProperties=[&]{events.push_back(-2);return InputReadId{4u};};
        q.releaseProperties=[&](auto id)noexcept{if(id!=4u)std::terminate();++releases;};
        q.propertyCount=[](auto){return std::size_t{5u};};
        q.property=[](auto,std::size_t slot){return slot==4u?InputReadId{}:InputReadId(slot+1u);};
        q.isFloat=[](auto id){return id!=3u;};
        q.key=[&](std::int32_t key){return keys.at(static_cast<std::size_t>(key));};
        q.globalOutput=[&]{++globalLoads;return selectedOutput;};
        q.process=[&](auto output,std::int32_t key,std::int32_t action,float delta){
            Require(output==selectedOutput&&action==2&&delta==.02f,"Held Process lost fresh device/Action/delta");
            events.push_back(key);if(clearLater)keys[254u]=0u;
            if(throwProcess)throw std::runtime_error("selected Process failed");
            ++selectedOutput;
        };
        q.readFloat=[&](auto id){return axes.at(static_cast<std::size_t>(id)-1u);};
        q.writeFloat=[&](auto id,float value){axes.at(static_cast<std::size_t>(id)-1u)=value;++writes;};return q;}
};
void OrderAndFailure(){
    {Fixture f;f.running=false;auto q=f.Queries();q.acquireProperties={};const auto r=ReadOriginalInput(.02f,q);
        Require(!r.running&&!r.cacheAcquired&&f.events==std::vector<int>{-1}&&!f.writes&&!f.frame,"Stopped process inspected Actor/cache");}
    {Fixture f;f.keys[0]=f.keys[7]=f.keys[254]=1u;f.clearLater=true;const auto r=ReadOriginalInput(.02f,f.Queries());
        Require(r.holds==2u&&r.scaled==3u&&f.globalLoads==2u&&f.events==std::vector<int>{-1,-2,0,7}&&f.releases==1u&&!f.frame,
            "Live ascending keys/fresh global output/cache release order differs");
        Require(f.axes[0]==1000&&f.axes[1]==-2000&&f.axes[2]==3&&f.axes[3]==4000,"Float-only20/elapsed scaling differs");}
    {Fixture f;f.keys[1]=1u;f.throwProcess=true;Refuse([&]{ReadOriginalInput(.02f,f.Queries());});
        Require(f.releases==1u&&!f.frame&&!f.writes,"Process failure lost cache/frame cleanup or scaled suffix");}
    {Fixture f;auto q=f.Queries();q.releaseProperties={};Refuse([&]{ReadOriginalInput(.02f,q);});
        Require(f.events==std::vector<int>{-1}&&!f.frame&&!f.releases,"Missing release provider acquired an unreleasable cache");}
    {Fixture f;f.keys[1]=1u;auto q=f.Queries();q.process=[&](auto,auto,auto,float){f.revoked=true;};
        Refuse([&]{ReadOriginalInput(.02f,q);});Require(f.releases==1u&&!f.frame&&!f.writes,"Lifetime revocation retained cache or ran scaled suffix");}
    {Fixture f;f.keys[1]=1u;auto q=f.Queries();q.process=[&](auto,auto,auto,float){f.axes[0]=7.0f;};
        const auto r=ReadOriginalInput(.02f,q);Require(r.holds==1u&&f.axes[0]==7000.0f,"Scaling did not read post-Hold live Float value");}
    for(std::size_t cut=0u;cut<290u;++cut){Fixture f;f.limit=cut;bool failed{};
        try{ReadOriginalInput(.02f,f.Queries());}catch(const std::exception&){failed=true;}
        const bool acquired=f.events.size()>1u&&f.events[1u]==-2;
        Require(!f.frame&&f.releases==static_cast<std::size_t>(acquired),"Budget cutoff retained lexical frame/cache use");if(failed)++refusals;}
    {Fixture f;auto q=f.Queries();q.writeFloat=[&](auto,float){throw std::runtime_error("actual store failed");};
        Refuse([&]{ReadOriginalInput(-1.0f,q);});Require(f.releases==1u&&!f.frame,"Post-input store failure lost cache release");}
}
float HardwareScale(float elapsed,float value){
#if defined(__GNUC__) && (defined(__x86_64__) || defined(__i386__))
    unsigned short oldWord{},word{};asm volatile("fnstcw %0":"=m"(oldWord));word=static_cast<unsigned short>((oldWord&~0x0f00u)|0x0300u);
    asm volatile("fldcw %0"::"m"(word));const double numerator=20.0,zero=0.0;float result{};
    const auto bits=std::bit_cast<std::uint32_t>(elapsed);
    const bool special=(bits&0x7fffffffu)>0x7f800000u||bits==0xbf800000u;
    if(special)asm volatile("fldl %1; fmuls %2; fstps %0":"=m"(result):"m"(zero),"m"(value):"st");
    else asm volatile("fldl %1; fdivs %2; fmuls %3; fstps %0":"=m"(result):"m"(numerator),"m"(elapsed),"m"(value):"st");
    asm volatile("fldcw %0"::"m"(oldWord));return result;
#else
    // Fixed finite/sentinel goldens are checked below on non-x87 hosts. The
    // independent generated oracle is specifically the desktop hardware x87.
    return NativeScalar::StoreFloat(NativeScalar::Multiply(InputReadDetail::Polls(elapsed)?
        NativeScalar::Divide(NativeScalar::Load(20.0),NativeScalar::Load(elapsed)):NativeScalar::Load(0.0),NativeScalar::Memory(value)));
#endif
}
void Numeric(){
    for(float elapsed:{.02f,.01f}){Fixture f;const float delta=elapsed==.02f?240.0f:120.0f;
        f.axes[0]=NativeScalar::StoreFloat(NativeScalar::Multiply(
            NativeScalar::Multiply(NativeScalar::Load(delta),NativeScalar::Memory(2.0f)),
            NativeScalar::Memory(std::bit_cast<double>(0x3f847ae147ae147bull))));
        Require(std::bit_cast<std::uint32_t>(HardwareScale(elapsed,f.axes[0]))==0x45960001u,
            "Owned Speed2/Action4 Float accumulation has an incorrect independent rate golden");
        ReadOriginalInput(elapsed,f.Queries());Require(std::bit_cast<std::uint32_t>(f.axes[0])==0x45960001u,
            "Original axis/read pipeline lost exact rate-invariant Float rounding");}
    constexpr std::array<std::uint32_t,17u> words{0u,0x80000000u,0x3f800000u,0xbf800000u,0x3c23d70au,0x3ca3d70au,
        1u,0x80000001u,0x007fffffu,0x00800000u,0x7f7fffffu,0x7f800000u,0xff800000u,0x7fc12345u,0x7f812345u,0xffc43210u,0x3eaaaaabu};
    for(auto time:words)for(auto value:words){Fixture f;f.axes[0]=std::bit_cast<float>(value);
        const auto elapsed=std::bit_cast<float>(time);const auto expected=HardwareScale(elapsed,f.axes[0]);
        const auto r=ReadOriginalInput(elapsed,f.Queries());
        Require(std::bit_cast<std::uint32_t>(f.axes[0])==std::bit_cast<std::uint32_t>(expected),"ReadInput differs from independent PC64 x87 scaling");
        Require(r.scaled==3u&&!r.holds&&!f.globalLoads&&f.releases==1u&&!f.frame,"Numeric/sentinel read changed dispatch/release profile");}
    std::uint32_t seed=0x23456789u;
    for(std::size_t i=0u;i<2000u;++i){seed=seed*1664525u+1013904223u;const auto value=std::bit_cast<float>(seed);
        seed=seed*1664525u+1013904223u;const auto elapsed=std::bit_cast<float>(seed);Fixture f;f.axes[0]=value;
        ReadOriginalInput(elapsed,f.Queries());Require(std::bit_cast<std::uint32_t>(f.axes[0])==std::bit_cast<std::uint32_t>(HardwareScale(elapsed,value)),
            "Generated ReadInput extended factor/Float store differs from hardware");}
    {Fixture f;f.axes[0]=-2;const auto r=ReadOriginalInput(-1,f.Queries());Require(r.scaled==3u&&std::bit_cast<std::uint32_t>(f.axes[0])==0x80000000u,
        "Post-input multiply0 became assignment positive0");}
}
void Viewport(){
    for(float elapsed:{.02f,-1.0f,std::numeric_limits<float>::quiet_NaN()}){
        std::vector<int> calls;std::uintptr_t input=1u;std::size_t frame{};OriginalViewportReadQueries q;
        q.validate=[]{};q.work=[]{};q.enterFrame=[&](auto bytes){frame+=bytes;};q.leaveFrame=[&](auto bytes)noexcept{frame-=bytes;};
        q.input=[&]{calls.push_back(static_cast<int>(input));return input;};
        q.updateInput=[&](auto reset){Require(!reset,"Viewport repoll changed reset0");calls.push_back(8);input=2u;};
        q.globalOutput=[&]{calls.push_back(9);return InputReadId{3u};};
        q.readInput=[&](auto receiver,float delta,auto device){Require(receiver==input&&device==3u&&
            (delta==elapsed||(std::isnan(delta)&&std::isnan(elapsed))),"Viewport did not reload Input after platform update");calls.push_back(10);};
        ReadOriginalViewportInput(elapsed,q);Require(!frame&&calls==(elapsed==.02f?std::vector<int>{1,8,2,9,10}:
            std::vector<int>{1,1,9,10}),"Viewport pre/post/unordered input order differs");
    }
}
}
int main(){try{OrderAndFailure();Numeric();Viewport();std::cout<<"PASS original ReadInput coordinator checks="<<checks<<" refusals="<<refusals
    <<"; x87 scaling/live keys/global output/cache/frame ordering. NOT actual initialized owner, startup or headset acceptance\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL ReadInput coordinator: "<<e.what()<<'\n';return 1;}}
