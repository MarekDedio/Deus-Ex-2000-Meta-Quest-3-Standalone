#pragma once

#include "quest_multi_line_trace.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <limits>
#include <stdexcept>

namespace QuestVr {

// Host-private temporary authority for the actual MoveActor bridge. These are
// opaque identity words, not addresses to dereference or persistent save data.
struct NativeHitRootStamp {
    std::uintptr_t level{},lookupControl{};
    std::uint64_t mapGeneration{},lookupGeneration{};
    bool operator==(const NativeHitRootStamp&)const=default;
};
struct NativeHitMarkLimits {
    std::size_t marks{64u},arenas{256u},records{1'048'576u},payloadBytes{16u<<20u};
    std::uint32_t markSerials{1'048'576u},rootSerial{std::numeric_limits<std::uint32_t>::max()};
};
namespace NativeHitMarksDetail {
// Capability generations are deliberately outside gameplay journals. A new
// host instance must not resurrect integers issued by an earlier host/root.
inline std::atomic<std::uint32_t> rootSerial{};
inline std::uint32_t AcquireRoot(std::uint32_t maximum) {
    auto previous=rootSerial.load(std::memory_order_relaxed);
    for(;;) {
        if(previous>=maximum)throw std::runtime_error("Native hit root generation exhausted");
        const auto next=static_cast<std::uint32_t>(previous+1u);
        if(rootSerial.compare_exchange_weak(previous,next,std::memory_order_relaxed))return next;
    }
}
} // namespace NativeHitMarksDetail

class NativeHitMarks {
    static_assert(sizeof(std::uintptr_t)>=8u,"Native hit handles require the actual 64-bit host/Quest target");
    struct Arena {
        std::vector<OriginalActorLineHit> records;
        std::uint32_t first{},count{};
        std::size_t bytes{};
    };
    struct MarkEntry {std::uintptr_t token{};std::size_t arenas{},bytes{};};
    std::array<Arena,256u> arenas_{};
    std::array<MarkEntry,64u> marks_{};
    NativeHitMarkLimits limits_;
    NativeHitRootStamp stamp_{};
    std::size_t arenaCount_{},markCount_{},payloadBytes_{};
    std::uint32_t root_{},ordinal_{},markSerial_{};
    bool preflight_{},preflightViolation_{};

    [[noreturn]] static void Fail(const char* reason){throw std::runtime_error(reason);}
    void Mutable() {
        if(preflight_){preflightViolation_=true;Fail("Native hit adoption preflight attempted reentrant mutation");}
    }
    void Require(const NativeHitRootStamp& stamp)const {
        if(!root_||stamp!=stamp_)Fail("Native hit operation has expired or foreign Level/map/lookup-root authority");
    }
    std::uintptr_t Encode(std::uint32_t serial,bool mark)const noexcept {
        return static_cast<std::uintptr_t>((std::uint64_t{root_}<<32u)|(std::uint64_t{serial}<<1u)|(mark?1u:0u));
    }
    std::uint32_t Decode(std::uintptr_t handle,bool mark)const {
        const auto word=static_cast<std::uint64_t>(handle);
        if(!handle||!root_||static_cast<std::uint32_t>(word>>32u)!=root_||((word&1u)!=0u)!=mark)
            Fail("Native hit handle is null, foreign, expired or the wrong token kind");
        const auto serial=static_cast<std::uint32_t>(word)>>1u;
        if(!serial)Fail("Native hit handle has no issued ordinal");
        return serial;
    }
    const OriginalActorLineHit& Lookup(const NativeHitRootStamp& stamp,std::uintptr_t handle)const {
        Require(stamp);const auto ordinal=Decode(handle,false);
        // Active ranges remain sorted despite holes left by Pop; later adopts
        // use strictly newer ordinals. No numeric word becomes a C++ pointer.
        std::size_t first{},last=arenaCount_;
        while(first<last){const auto middle=first+(last-first)/2u;
            if(arenas_[middle].first<=ordinal)first=middle+1u;else last=middle;}
        if(!first)Fail("Native hit handle was never issued in this active mark/root");
        const auto& arena=arenas_[first-1u];const auto offset=ordinal-arena.first;
        if(offset>=arena.count)Fail("Native hit handle belongs to popped or unissued storage");
        return arena.records[offset];
    }
    static std::size_t CapacityBytes(const OriginalLineHitArena& arena) {
        if(arena.records.capacity()>std::numeric_limits<std::size_t>::max()/sizeof(OriginalActorLineHit))
            Fail("Native hit vector capacity byte overflow");
        return arena.records.capacity()*sizeof(OriginalActorLineHit);
    }
    void ReleaseSuffix(std::size_t count)noexcept {
        while(arenaCount_>count){auto& arena=arenas_[--arenaCount_];
            // Invalidate first, then release the vector without allocation.
            arena.first=arena.count=0u;arena.bytes=0u;
            std::vector<OriginalActorLineHit> empty;arena.records.swap(empty);}
    }
public:
    explicit NativeHitMarks(NativeHitMarkLimits limits={}):limits_(limits) {
        const NativeHitMarkLimits maximum;
        if(limits.marks>maximum.marks||limits.arenas>maximum.arenas||limits.records>maximum.records||
            limits.payloadBytes>maximum.payloadBytes||limits.markSerials>maximum.markSerials)
            Fail("Native hit callers may only tighten fixed storage/token bounds");
    }
    NativeHitMarks(const NativeHitMarks&)=delete;
    NativeHitMarks& operator=(const NativeHitMarks&)=delete;
    NativeHitMarks(NativeHitMarks&&)=delete;
    NativeHitMarks& operator=(NativeHitMarks&&)=delete;
    ~NativeHitMarks()noexcept{if(!Clear())std::terminate();}

    void BeginRoot(NativeHitRootStamp stamp) {
        Mutable();
        if(root_||arenaCount_||markCount_||payloadBytes_)Fail("Native hit root is already active or not empty");
        if(!stamp.level||!stamp.lookupControl||!stamp.mapGeneration||!stamp.lookupGeneration)
            Fail("Native hit root requires actual nonzero Level/map/lookup identity words");
        const auto serial=NativeHitMarksDetail::AcquireRoot(limits_.rootSerial);
        stamp_=stamp;root_=serial;ordinal_=markSerial_=0u;
    }
    std::uintptr_t Mark(const NativeHitRootStamp& stamp) {
        Mutable();Require(stamp);
        if(markCount_>=limits_.marks||markSerial_>=limits_.markSerials)
            Fail("Native hit mark depth or cumulative token allowance exhausted");
        const auto token=Encode(markSerial_+1u,true);
        marks_[markCount_++]={token,arenaCount_,payloadBytes_};++markSerial_;return token;
    }
    // The validator observes CURRENT RetainedBytes plus this exact additional
    // capacity charge; the surrounding host must account for simultaneous
    // query workspace/reservations. It must be pure/non-reentrant. Attempted
    // mutation (even caught inside the callback) invalidates this adoption.
    // All admission and the callback finish BEFORE input links/store mutate.
    template<class Preflight>
    std::uintptr_t Adopt(const NativeHitRootStamp& stamp,OriginalLineHitArena&& incoming,Preflight&& validate) {
        Mutable();Require(stamp);
        if(!markCount_)Fail("Native hit adoption requires a current memory mark");
        const auto count=incoming.records.size();
        if(incoming.first>count)Fail("Native hit local head is outside its owning arena");
        for(const auto& hit:incoming.records)if(hit.next>count)Fail("Native hit local Next is outside its owning arena");
        if(count>limits_.records-ordinal_)Fail("Native hit cumulative record ordinal allowance exhausted");
        if(count&&arenaCount_>=limits_.arenas)Fail("Native hit owning arena allowance exhausted");
        const auto bytes=count?CapacityBytes(incoming):0u;
        if(bytes>limits_.payloadBytes-payloadBytes_)Fail("Native hit retained vector capacity allowance exhausted");
        preflight_=true;preflightViolation_=false;
        struct Guard {bool& active;~Guard()noexcept{active=false;}} guard{preflight_};
        validate(bytes);
        if(preflightViolation_)Fail("Native hit adoption preflight violated its non-reentrant contract");
        preflight_=false;
        if(!count){std::vector<OriginalActorLineHit> empty;incoming.records.swap(empty);incoming.first=0u;return 0u;}
        const auto first=static_cast<std::uint32_t>(ordinal_+1u);
        const auto head=incoming.first?Encode(first+static_cast<std::uint32_t>(incoming.first)-1u,false):0u;
        for(auto& hit:incoming.records)if(hit.next)hit.next=Encode(first+static_cast<std::uint32_t>(hit.next)-1u,false);
        auto& target=arenas_[arenaCount_];target.records.swap(incoming.records);
        target.first=first;target.count=static_cast<std::uint32_t>(count);target.bytes=bytes;
        ++arenaCount_;payloadBytes_+=bytes;ordinal_+=static_cast<std::uint32_t>(count);
        incoming.first=0u;return head;
    }
    OriginalActorLineHit Packet(const NativeHitRootStamp& stamp,std::uintptr_t handle)const{return Lookup(stamp,handle);}
    std::uintptr_t Actor(const NativeHitRootStamp& stamp,std::uintptr_t handle)const{return Lookup(stamp,handle).actor;}
    std::uintptr_t Next(const NativeHitRootStamp& stamp,std::uintptr_t handle)const{return Lookup(stamp,handle).next;}
    float Time(const NativeHitRootStamp& stamp,std::uintptr_t handle)const{return Lookup(stamp,handle).time;}
    // The adapter MUST turn false into an invariant failure; it cannot silently
    // claim successful mandatory cleanup. These operations never call validators,
    // work hooks, scripts, lookup code or an allocator's allocation path.
    bool Pop(std::uintptr_t token)noexcept {
        if(preflight_){preflightViolation_=true;return false;}
        if(!root_||!markCount_||marks_[markCount_-1u].token!=token)return false;
        const auto mark=marks_[markCount_-1u];ReleaseSuffix(mark.arenas);payloadBytes_=mark.bytes;--markCount_;return true;
    }
    bool Clear()noexcept {
        if(preflight_){preflightViolation_=true;return false;}
        ReleaseSuffix(0u);markCount_=payloadBytes_=0u;root_=ordinal_=markSerial_=0u;stamp_={};return true;
    }
    std::size_t RetainedBytes()const noexcept{return sizeof(*this)+payloadBytes_;}
    std::size_t PayloadBytes()const noexcept{return payloadBytes_;}
    std::size_t ArenaCount()const noexcept{return arenaCount_;}
    std::size_t MarkDepth()const noexcept{return markCount_;}
    std::uint32_t IssuedRecords()const noexcept{return ordinal_;}
    std::uint32_t RootSerial()const noexcept{return root_;}
    bool Active()const noexcept{return root_!=0u;}
};
} // namespace QuestVr
