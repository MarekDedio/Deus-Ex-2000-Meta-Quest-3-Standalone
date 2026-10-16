#include "quest_collision_hit_sort.h"

#include <algorithm>
#include <bit>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <numeric>
#include <span>

namespace {
std::size_t checks{},cases{},refusals{};
void Require(bool value,const char* reason) {++checks;if (!value) throw std::runtime_error(reason);}
struct Packet {
    std::uint32_t identity{};
    std::array<std::uint32_t,8> opaque{};
    float time{};
    std::uint32_t tail{};
};
static_assert(sizeof(Packet)==44 && offsetof(Packet,time)==36);
using Bytes=std::vector<std::byte>;
Bytes Encode(const std::vector<Packet>& packets) {
    Bytes bytes(packets.size()*44u);
    if (!bytes.empty()) std::memcpy(bytes.data(),packets.data(),bytes.size());
    return bytes;
}
std::vector<Packet> Pack(std::span<const float> times) {
    std::vector<Packet> result(times.size());
    for (std::size_t i=0;i<times.size();++i) {
        auto& p=result[i];p.identity=static_cast<std::uint32_t>(i);p.time=times[i];
        for (std::size_t field=0;field<p.opaque.size();++field)
            p.opaque[field]=0xa5f00000u^(static_cast<std::uint32_t>(i)*0x19e37u)^static_cast<std::uint32_t>(field*0x10203u);
        p.tail=0xbeef1234u^static_cast<std::uint32_t>(i*0x271u);
    }
    return result;
}

// Independent instruction-shaped byte machine for the STATICALLY inspected
// bundled MSVCRT 7802543d..78025666; this does not execute original binaries or
// call production helpers. Byte addresses, 44-byte swaps and signed one-BYTE
// partition-size decrement deliberately remain unlike the index production API.
// Comparator is retail Engine 1039c060: Float at +36, no identity tie-break.
void ByteOracle(Bytes& memory) {
    constexpr std::ptrdiff_t width=44;
    const auto count=static_cast<std::ptrdiff_t>(memory.size()/width);
    if (count<2) return;
    const auto compare=[&](std::ptrdiff_t a,std::ptrdiff_t b) {
        float first{},second{};
        std::memcpy(&first,memory.data()+a+36,4);
        std::memcpy(&second,memory.data()+b+36,4);
        return first<second ? -1 : first>second ? 1 : 0;
    };
    const auto exchange=[&](std::ptrdiff_t a,std::ptrdiff_t b) {
        if (a==b) return;
        for (std::ptrdiff_t n=0;n<width;++n) {
            const auto saved=memory[a+n];memory[a+n]=memory[b+n];memory[b+n]=saved;
        }
    };
    std::array<std::ptrdiff_t,32> lowStack{},highStack{};
    std::ptrdiff_t low=0,high=(count-1)*width,number{},middle{},forward{},backward{},maximum{},cursor{},end{};
    std::size_t stack{};
dispatch:
    number=(high-low)/width+1;
    if (number>8) goto partition;
    end=high;
small_pass:
    if (end<=low) goto restore;
    maximum=low;cursor=low+width;
small_candidate:
    if (cursor>end) goto small_exchange;
    if (compare(cursor,maximum)>0) maximum=cursor;
    cursor+=width;goto small_candidate;
small_exchange:
    exchange(maximum,end);end-=width;goto small_pass;
partition:
    middle=low+(number>>1)*width;exchange(middle,low);
    forward=low;backward=high+width;
forward_scan:
    forward+=width;
    if (forward<=high && compare(forward,low)<=0) goto forward_scan;
backward_scan:
    backward-=width;
    if (backward>low && compare(backward,low)>=0) goto backward_scan;
    if (backward<forward) goto pivot_exchange;
    exchange(forward,backward);goto forward_scan;
pivot_exchange:
    exchange(backward,low);
    if ((backward-low-1)<(high-forward)) goto push_right;
    if (low+width<backward) {lowStack[stack]=low;highStack[stack++]=backward-width;}
    if (forward<high) {low=forward;goto dispatch;}
    goto restore;
push_right:
    if (forward<high) {lowStack[stack]=forward;highStack[stack++]=high;}
    if (low+width<backward) {high=backward-width;goto dispatch;}
restore:
    if (!stack) return;
    --stack;low=lowStack[stack];high=highStack[stack];goto dispatch;
}

void Check(std::span<const float> times) {
    auto actual=Pack(times);auto expected=Encode(actual);ByteOracle(expected);
    QuestVr::SortOriginalCollisionHits(actual);
    Require(Encode(actual)==expected,"Whole packet permutation differs from original byte-qsort oracle");
    for (std::size_t i=1;i<actual.size();++i)
        Require(!(actual[i].time<actual[i-1].time),"Result Times are not ordered");
    std::vector<bool> seen(times.size());
    const auto original=Pack(times);
    for (const auto& p:actual) {
        Require(p.identity<seen.size()&&!seen[p.identity],"Sort lost or duplicated a packet");
        seen[p.identity]=true;
        Require(std::memcmp(&p,&original[p.identity],44)==0,"Sort separated Time or opaque payload from its packet");
    }
    ++cases;
}
void Fixture(std::size_t count,std::span<const std::uint32_t> identities) {
    auto packets=Pack(std::vector<float>(count,1.0f));QuestVr::SortOriginalCollisionHits(packets);
    Require(packets.size()==identities.size(),"Bad fixture size");
    for (std::size_t i=0;i<count;++i)
        Require(packets[i].identity==identities[i],"Bundled CRT equal-Time fixture differs");
}
void KnownTies() {
    const std::array<std::uint32_t,2> two{1,0};Fixture(2,two);
    const std::array<std::uint32_t,8> eight{1,2,3,4,5,6,7,0};Fixture(8,eight);
    const std::array<std::uint32_t,9> nine{4,1,2,3,0,5,6,7,8};Fixture(9,nine);
    std::array<std::uint32_t,64> sixtyFour{};std::iota(sixtyFour.begin(),sixtyFour.end(),0u);
    sixtyFour[0]=32;sixtyFour[32]=0;Fixture(64,sixtyFour);
}
void Exhaustive() {
    // Every ternary tie pattern through both sides of the 8/9 qsort threshold.
    for (std::size_t length=0,patterns=1;length<=10;++length,patterns*=3u) {
        for (std::size_t code=0;code<patterns;++code) {
            auto remaining=code;std::vector<float> times(length);
            for (auto& t:times) {t=static_cast<float>(remaining%3u)-1.0f;remaining/=3u;}
            Check(times);
        }
    }
    // Full unique permutations through smallsort's maximum length.
    for (std::size_t length=0;length<=8;++length) {
        std::vector<float> times(length);std::iota(times.begin(),times.end(),0.0f);
        do {Check(times);} while (std::next_permutation(times.begin(),times.end()));
    }
    for (std::size_t length=0;length<=10;++length)
        for (std::size_t mask=0;mask<(std::size_t{1}<<length);++mask) {
            std::vector<float> times(length);
            for (std::size_t i=0;i<length;++i) times[i]=(mask&(std::size_t{1}<<i)) ? -0.0f : 0.0f;
            Check(times);
        }
}
std::uint32_t Random(std::uint32_t& state) {
    state^=state<<13;state^=state>>17;state^=state<<5;return state;
}
void BroadFinite() {
    const std::array<float,14> edges{0.0f,-0.0f,1.0f,-1.0f,
        std::numeric_limits<float>::denorm_min(),-std::numeric_limits<float>::denorm_min(),
        std::numeric_limits<float>::min(),-std::numeric_limits<float>::min(),
        std::numeric_limits<float>::max(),-std::numeric_limits<float>::max(),
        std::bit_cast<float>(0x3f7fffffu),std::bit_cast<float>(0x3f800001u),
        std::bit_cast<float>(0x00800001u),std::bit_cast<float>(0x007fffffu)};
    std::uint32_t random=0x19a76c35u;
    for (std::size_t count=0;count<=64;++count) {
        for (const unsigned pattern:{0u,1u,2u,3u,4u}) {
            std::vector<float> times(count);
            for (std::size_t i=0;i<count;++i) {
                if (pattern==0) times[i]=static_cast<float>(i);
                if (pattern==1) times[i]=static_cast<float>(count-i);
                if (pattern==2) times[i]=static_cast<float>(i%2);
                if (pattern==3) times[i]=static_cast<float>(i<=count/2 ? i : count-i);
                if (pattern==4) times[i]=edges[i%edges.size()];
            }
            Check(times);
        }
    }
    for (std::size_t trial=0;trial<6000;++trial) {
        const auto count=trial%3u==0 ? 64u : Random(random)%65u;
        std::vector<float> times(count);
        for (auto& t:times) {
            const auto word=Random(random);
            if (trial%3u==0) t=static_cast<float>(word%7u)-3.0f;
            else if (trial%3u==1) t=edges[word%edges.size()];
            else {
                // Preserve arbitrary finite Float signs, mantissas and normal/
                // subnormal exponents without relying on distribution libraries.
                const auto bits=(word&0x7f800000u)==0x7f800000u ? word^0x00800000u : word;
                t=std::bit_cast<float>(bits);
            }
        }
        Check(times);
    }
}
template<class F> void Refuse(F run) {
    bool failed{};try {run();} catch (const std::invalid_argument&) {failed=true;}
    Require(failed,"Unsafe collision-hit sort data was accepted");++refusals;
}
void Policy() {
    const auto nan=std::numeric_limits<float>::quiet_NaN();
    for (const std::size_t count:{0u,1u}) {
        auto packets=Pack(std::vector<float>(count,nan));const auto before=Encode(packets);
        QuestVr::SortOriginalCollisionHits(packets,[](const Packet&)->float {
            throw std::runtime_error("Inactive Time getter was called");
        });
        Require(Encode(packets)==before,"Zero/one-hit sort changed inactive packet");
        QuestVr::SortOriginalCollisionHits(packets);
        Require(Encode(packets)==before,"Zero/one-hit default getter validated inactive Time");
    }
    for (const auto bad:{nan,std::numeric_limits<float>::infinity(),-std::numeric_limits<float>::infinity()})
        for (const std::size_t count:{2u,8u,9u,64u})
            for (std::size_t at=0;at<count;++at) {
                auto packets=Pack(std::vector<float>(count,1.0f));packets[at].time=bad;
                const auto before=Encode(packets);
                Refuse([&]{QuestVr::SortOriginalCollisionHits(packets);});
                Require(Encode(packets)==before,"Nonfinite refusal partially reordered hit packets");
            }
    auto tooMany=Pack(std::vector<float>(65,1.0f));const auto before=Encode(tooMany);
    Refuse([&]{QuestVr::SortOriginalCollisionHits(tooMany,[](const Packet&)->float {
        throw std::runtime_error("Oversized sort read Time before cap rejection");
    });});
    Require(Encode(tooMany)==before,"Oversized refusal changed packets");
    auto packets=Pack(std::array{3.0f,2.0f,1.0f,0.0f});std::array<unsigned,4> visits{};
    QuestVr::SortOriginalCollisionHits(packets,[&](const Packet& p)->const float& {
        ++visits[p.identity];return p.time;
    });
    for (const auto n:visits) Require(n==1,"Finite getter was not cached exactly once per packet");
    for (std::size_t i=0;i<packets.size();++i)
        Require(packets[i].identity==3u-i,"Custom Float-reference getter sorted incorrectly");
    // The explicit getter controls the key; no implicit .time or payload read.
    packets=Pack(std::array{nan,nan,nan});
    QuestVr::SortOriginalCollisionHits(packets,[](const Packet& p)->float {return static_cast<float>(p.identity);});
    Require(packets[0].identity==0&&packets[1].identity==1&&packets[2].identity==2,
        "Explicit getter did not control comparator Time");
}
} // namespace

int main() {
    try {
        KnownTies();Exhaustive();BroadFinite();Policy();
        std::cout<<"Original bundled-MSVCRT collision-hit sort: "<<checks<<" checks, "
            <<cases<<" independent byte-oracle permutations, "<<refusals<<" explicit refusals\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr<<"Collision-hit sort test failed: "<<error.what()<<'\n';return 1;
    }
}
