#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <functional>
#include <limits>
#include <new>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

#include "Utils/Array.h"
#include "Package/NameString.h"

// Detached staging for the REAL NameString arena on its exclusive CPU owner.
// Pending NameString values contain their eventual real compare/spelling
// indices, but MUST NOT be published or use NameString::ToString before the
// same nonthrowing publication suffix calls Commit. Use Spelling meanwhile.
// No fake replacement arena is installed during parsing or budget callbacks.
class NameStringArenaStage {
public:
    struct Work {
        std::function<void(std::size_t)> step;
        // Absolute ADDITIONAL workspace beyond the unchanged captured arena.
        // Callback storage/copy admission belongs to the caller.
        std::function<void(std::size_t)> admit;
    };

    explicit NameStringArenaStage(Work work={},std::size_t reserveAdditionalNames=0u) : work_(std::move(work)) {
        Capture();
        baselineBytes_=LiveArenaBytes();
        const auto capacity=AddBytes(liveNamesSize_,reserveAdditionalNames);
        if(capacity>static_cast<std::size_t>(std::numeric_limits<int>::max()))Fail("reserved name index range exceeded");
        const auto clone=AddBytes(Multiply(capacity,sizeof(std::string)),liveNameStringBytes_);
        auto initial=AddBytes(RetainedBytes(),clone);
        initial=AddBytes(initial,BucketBound(NameString::CompareStringToIndex.size()));
        initial=AddBytes(initial,BucketBound(NameString::SpellStringToIndex.size()));
        Admit(initial);
        // Reserve on an EMPTY array before copying existing strings. Copying
        // an exact-size Array first and then reserving would transiently hold
        // both whole buffers; first Add would also double that whole clone.
        // The caller's additional row bound avoids either staging peak.
        names_.reserve(capacity);
        for(const auto& name:NameString::Names) {
            Step(AddBytes(name.size(),1u));
            names_.push_back(name);
            nameStringBytes_=AddBytes(nameStringBytes_,AddBytes(names_.back().capacity(),1u));
        }
        EnsureBuckets(replacementCompare_,NameString::CompareStringToIndex.size());
        EnsureBuckets(replacementSpell_,NameString::SpellStringToIndex.size());
        CheckUnchanged();RefreshGrowth();Admit(RetainedBytes());
    }
    NameStringArenaStage(const NameStringArenaStage&)=delete;
    NameStringArenaStage& operator=(const NameStringArenaStage&)=delete;
    NameStringArenaStage(NameStringArenaStage&&)=delete;
    NameStringArenaStage& operator=(NameStringArenaStage&&)=delete;

    NameString Add(std::string_view value) {
        if(committed_||failed_)Fail("stage is already committed or refused");
        validated_=false;
        try {
            Step(AddBytes(value.size(),1u));CheckUnchanged();
            // Own the spelling before any staged Names relocation: callers
            // may pass a view returned by this stage's earlier Spelling call.
            activeScratch_=Multiply(StringBound(value.size()),2u);
            Admit(AddBytes(RetainedBytes(),activeScratch_));
            const std::string spelling(value);
            if(const auto found=deltaSpell_.find(spelling);found!=deltaSpell_.end())
                return Finish(found->second.first,found->second.second);
            if(const auto found=NameString::SpellStringToIndex.find(spelling);found!=NameString::SpellStringToIndex.end())
                return Finish(found->second.first,found->second.second);

            // Exactly GetIndex order: the first spelling lookup precedes
            // initialization. First Add("None") therefore appends a second
            // None spelling, whereas first Add("") only initializes slot0.
            if(names_.empty()) {
                Append("None");InsertCompare("NONE",0);InsertSpell("None",{0,0});
            }
            if(spelling.empty())return Finish(0,0);
            auto comparison=spelling;
            for(char& c:comparison) {
                const auto byte=static_cast<unsigned char>(c);
                if(byte>='a'&&byte<='z')c=static_cast<char>(byte-('a'-'A'));
            }
            Step(comparison.size());
            int compare{};
            if(const auto found=deltaCompare_.find(comparison);found!=deltaCompare_.end())compare=found->second;
            else if(const auto found=NameString::CompareStringToIndex.find(comparison);found!=NameString::CompareStringToIndex.end())compare=found->second;
            else {compare=Append(comparison);InsertCompare(comparison,compare);}
            const auto spelled=Append(spelling);InsertSpell(spelling,{compare,spelled});
            return Finish(compare,spelled);
        }catch(...) {failed_=true;throw;}
    }

    const std::string& Spelling(const NameString& value) const {
        if(failed_)Fail("refused stage has no pending spelling authority");
        if(!committed_)CheckUnchanged();
        const auto& source=committed_?NameString::Names:names_;
        if(value.CompareIndex<0||value.SpelledIndex<0||
            static_cast<std::size_t>(value.CompareIndex)>=source.size()||
            static_cast<std::size_t>(value.SpelledIndex)>=source.size())
            Fail("name is outside the staged/captured arena");
        return source[static_cast<std::size_t>(value.SpelledIndex)];
    }

    // Stage-held bytes: complete Names clone/string capacities, delta nodes
    // and buckets, and EMPTY replacement buckets. The unchanged live arena
    // remains separately owned; its nodes are never copied into these maps.
    std::size_t RetainedBytes() const {
        auto bytes=AddBytes(sizeof(*this),Multiply(names_.capacity(),sizeof(std::string)));
        bytes=AddBytes(bytes,nameStringBytes_);
        bytes=AddBytes(bytes,deltaCompareNodeBytes_);bytes=AddBytes(bytes,deltaSpellNodeBytes_);
        for(const auto count:{deltaCompare_.bucket_count(),deltaSpell_.bucket_count(),
            replacementCompare_.bucket_count(),replacementSpell_.bucket_count()})
            bytes=AddBytes(bytes,Multiply(count,sizeof(void*)));
        return bytes;
    }
    // High-water additional workspace, including temporary strings and old+
    // new staged buffers during growth. It excludes captured already-live
    // storage; live, delta and replacement bucket arrays coexist until Commit.
    std::size_t PeakBytes() const noexcept {return peakBytes_;}
    // Nonnegative NET complete live arena growth, not the temporary clone.
    // Keep this process-lifetime credit after the originating domain retires.
    std::size_t CommittedGrowthBytes() const noexcept {return growthBytes_;}

    void Validate() {
        if(committed_||failed_)Fail("stage is already committed or refused");
        CheckUnchanged();
        // Charge hashes/traversal for the allocation-free suffix BEFORE it
        // begins. No callback (including work) is allowed inside Commit.
        if(!validated_)Step(AddBytes(mergeWork_,deltaMergeWork_));
        CheckUnchanged();
        if(replacementCompare_.bucket_count()<AddBytes(liveCompareSize_,deltaCompare_.size())||
            replacementSpell_.bucket_count()<AddBytes(liveSpellSize_,deltaSpell_.size()))
            Fail("replacement buckets cannot hold the complete committed maps");
        RefreshGrowth();validated_=true;
    }

    // Requires Validate immediately before the caller's exclusive nonthrowing
    // publication suffix. All allocations/admissions happened earlier. The
    // default string hash/equality and same allocators allow node transfers
    // into preallocated empty buckets with no node/key copy or rehash.
    void Commit() noexcept {
        if(committed_)return;
        if(failed_||!validated_||!Unchanged())std::terminate();
        replacementCompare_.merge(NameString::CompareStringToIndex);
        replacementCompare_.merge(deltaCompare_);
        replacementSpell_.merge(NameString::SpellStringToIndex);
        replacementSpell_.merge(deltaSpell_);
        if(!NameString::CompareStringToIndex.empty()||!deltaCompare_.empty()||
            !NameString::SpellStringToIndex.empty()||!deltaSpell_.empty())std::terminate();
        names_.swap(NameString::Names);
        NameString::CompareStringToIndex.swap(replacementCompare_);
        NameString::SpellStringToIndex.swap(replacementSpell_);
        nameStringBytes_=liveNameStringBytes_;deltaCompareNodeBytes_=0u;deltaSpellNodeBytes_=0u;
        committed_=true;
    }

private:
    using CompareMap=decltype(NameString::CompareStringToIndex);
    using SpellMap=decltype(NameString::SpellStringToIndex);
    Work work_;
    Array<std::string> names_;
    CompareMap deltaCompare_,replacementCompare_;
    SpellMap deltaSpell_,replacementSpell_;
    const std::string* liveNamesData_{};
    std::size_t liveNamesSize_{},liveNamesCapacity_{},liveCompareSize_{},liveCompareBuckets_{},liveSpellSize_{},liveSpellBuckets_{};
    std::size_t liveNameStringBytes_{},liveCompareNodeBytes_{},liveSpellNodeBytes_{},baselineBytes_{};
    std::size_t nameStringBytes_{},deltaCompareNodeBytes_{},deltaSpellNodeBytes_{},mergeWork_{},deltaMergeWork_{};
    std::size_t activeScratch_{},peakBytes_{},growthBytes_{};
    bool failed_{},validated_{},committed_{};

    [[noreturn]] static void Fail(const char* why) {throw std::runtime_error(std::string("Name arena stage: ")+why);}
    static std::size_t AddBytes(std::size_t left,std::size_t right) {
        if(right>std::numeric_limits<std::size_t>::max()-left)Fail("byte/work count overflow");
        return left+right;
    }
    static std::size_t Multiply(std::size_t count,std::size_t bytes) {
        if(bytes&&count>std::numeric_limits<std::size_t>::max()/bytes)Fail("byte count overflow");
        return count*bytes;
    }
    static std::size_t StringBound(std::size_t bytes) {
        return AddBytes(sizeof(std::string)+32u,Multiply(AddBytes(bytes,1u),2u));
    }
    static std::size_t BucketBound(std::size_t count) {
        // Pinned libstdc++ prime/libc++ prime-or-power2 policies, default load1.
        // Bertrand/power2 bounds include the smallest nonempty bucket arrays.
        return count?Multiply(Multiply(AddBytes(count,1u),2u),sizeof(void*)):0u;
    }
    template<class Map>static std::size_t NodeBytes(const typename Map::value_type& node) {
        return AddBytes(sizeof(typename Map::value_type)+8u*sizeof(void*),AddBytes(node.first.capacity(),1u));
    }
    void Step(std::size_t amount) {if(work_.step&&amount)work_.step(amount);CheckUnchanged();}
    void Admit(std::size_t bytes) {
        if(bytes>peakBytes_) {if(work_.admit)work_.admit(bytes);peakBytes_=bytes;}
        CheckUnchanged();
    }
    void Capture() {
        liveNamesData_=NameString::Names.data();liveNamesSize_=NameString::Names.size();liveNamesCapacity_=NameString::Names.capacity();
        liveCompareSize_=NameString::CompareStringToIndex.size();liveCompareBuckets_=NameString::CompareStringToIndex.bucket_count();
        liveSpellSize_=NameString::SpellStringToIndex.size();liveSpellBuckets_=NameString::SpellStringToIndex.bucket_count();
        if(NameString::CompareStringToIndex.max_load_factor()!=1.0f||NameString::SpellStringToIndex.max_load_factor()!=1.0f)
            Fail("live map load factor is outside the audited default profile");
        if(liveNamesSize_>static_cast<std::size_t>(std::numeric_limits<int>::max()))Fail("live name index range exceeded");
    }
    bool Unchanged() const noexcept {
        return NameString::Names.data()==liveNamesData_&&NameString::Names.size()==liveNamesSize_&&NameString::Names.capacity()==liveNamesCapacity_&&
            NameString::CompareStringToIndex.size()==liveCompareSize_&&NameString::CompareStringToIndex.bucket_count()==liveCompareBuckets_&&
            NameString::SpellStringToIndex.size()==liveSpellSize_&&NameString::SpellStringToIndex.bucket_count()==liveSpellBuckets_&&
            NameString::CompareStringToIndex.max_load_factor()==1.0f&&NameString::SpellStringToIndex.max_load_factor()==1.0f;
    }
    void CheckUnchanged() const {if(!Unchanged())Fail("live arena changed while detached staging was active");}
    std::size_t LiveArenaBytes() {
        auto bytes=sizeof(NameString::Names)+sizeof(CompareMap)+sizeof(SpellMap);
        bytes=AddBytes(bytes,Multiply(liveNamesCapacity_,sizeof(std::string)));
        for(const auto& name:NameString::Names) {
            Step(AddBytes(name.size(),1u));CheckUnchanged();
            liveNameStringBytes_=AddBytes(liveNameStringBytes_,AddBytes(name.capacity(),1u));
        }
        bytes=AddBytes(bytes,liveNameStringBytes_);
        for(const auto& node:NameString::CompareStringToIndex) {
            Step(AddBytes(node.first.size(),1u));CheckUnchanged();
            liveCompareNodeBytes_=AddBytes(liveCompareNodeBytes_,NodeBytes<CompareMap>(node));
            mergeWork_=AddBytes(mergeWork_,AddBytes(node.first.size(),1u));
        }
        for(const auto& node:NameString::SpellStringToIndex) {
            Step(AddBytes(node.first.size(),1u));CheckUnchanged();
            liveSpellNodeBytes_=AddBytes(liveSpellNodeBytes_,NodeBytes<SpellMap>(node));
            mergeWork_=AddBytes(mergeWork_,AddBytes(node.first.size(),1u));
        }
        bytes=AddBytes(bytes,liveCompareNodeBytes_);bytes=AddBytes(bytes,liveSpellNodeBytes_);
        return AddBytes(bytes,Multiply(AddBytes(liveCompareBuckets_,liveSpellBuckets_),sizeof(void*)));
    }
    template<class Map>void EnsureBuckets(Map& map,std::size_t required) {
        // The default single static bucket has no initialized resize threshold:
        // even its FIRST insertion can allocate/rehash. Force reserve for it.
        if(!required||(required<=map.bucket_count()&&map.bucket_count()>1u))return;
        const auto bound=BucketBound(required);
        Admit(AddBytes(AddBytes(RetainedBytes(),activeScratch_),bound));
        map.reserve(required);
        if(map.bucket_count()>bound/sizeof(void*))Fail("standard library bucket growth exceeded admitted profile");
    }
    int Append(const std::string& value) {
        if(names_.size()>=static_cast<std::size_t>(std::numeric_limits<int>::max()))Fail("pending name index range exceeded");
        Step(AddBytes(value.size(),1u));CheckUnchanged();
        if(names_.size()==names_.capacity()) {
            const auto capacity=Multiply(AddBytes(names_.capacity(),1u),2u);
            Admit(AddBytes(AddBytes(RetainedBytes(),activeScratch_),Multiply(capacity,sizeof(std::string))));
            names_.reserve(capacity);
        }
        Admit(AddBytes(AddBytes(RetainedBytes(),activeScratch_),StringBound(value.size())));
        const auto index=static_cast<int>(names_.size());names_.push_back(value);
        nameStringBytes_=AddBytes(nameStringBytes_,AddBytes(names_.back().capacity(),1u));return index;
    }
    void InsertCompare(const std::string& key,int value) {
        EnsureBuckets(deltaCompare_,AddBytes(deltaCompare_.size(),1u));
        EnsureBuckets(replacementCompare_,AddBytes(AddBytes(liveCompareSize_,deltaCompare_.size()),1u));
        Step(AddBytes(key.size(),1u));
        Admit(AddBytes(AddBytes(RetainedBytes(),activeScratch_),AddBytes(sizeof(CompareMap::value_type)+8u*sizeof(void*),StringBound(key.size()))));
        const auto [found,inserted]=deltaCompare_.emplace(key,value);
        if(!inserted)Fail("duplicate staged compare insertion");
        deltaCompareNodeBytes_=AddBytes(deltaCompareNodeBytes_,NodeBytes<CompareMap>(*found));
        deltaMergeWork_=AddBytes(deltaMergeWork_,AddBytes(key.size(),1u));
    }
    void InsertSpell(const std::string& key,std::pair<int,int> value) {
        if(const auto found=deltaSpell_.find(key);found!=deltaSpell_.end()) {found->second=value;return;}
        EnsureBuckets(deltaSpell_,AddBytes(deltaSpell_.size(),1u));
        EnsureBuckets(replacementSpell_,AddBytes(AddBytes(liveSpellSize_,deltaSpell_.size()),1u));
        Step(AddBytes(key.size(),1u));
        Admit(AddBytes(AddBytes(RetainedBytes(),activeScratch_),AddBytes(sizeof(SpellMap::value_type)+8u*sizeof(void*),StringBound(key.size()))));
        const auto [found,inserted]=deltaSpell_.emplace(key,value);
        if(!inserted)Fail("duplicate staged spelling insertion");
        deltaSpellNodeBytes_=AddBytes(deltaSpellNodeBytes_,NodeBytes<SpellMap>(*found));
        deltaMergeWork_=AddBytes(deltaMergeWork_,AddBytes(key.size(),1u));
    }
    void RefreshGrowth() {
        auto final=sizeof(NameString::Names)+sizeof(CompareMap)+sizeof(SpellMap);
        final=AddBytes(final,Multiply(names_.capacity(),sizeof(std::string)));final=AddBytes(final,nameStringBytes_);
        final=AddBytes(final,liveCompareNodeBytes_);final=AddBytes(final,deltaCompareNodeBytes_);
        final=AddBytes(final,liveSpellNodeBytes_);final=AddBytes(final,deltaSpellNodeBytes_);
        final=AddBytes(final,Multiply(AddBytes(replacementCompare_.bucket_count(),replacementSpell_.bucket_count()),sizeof(void*)));
        growthBytes_=final>baselineBytes_?final-baselineBytes_:0u;
    }
    NameString Finish(int compare,int spelling) {
        CheckUnchanged();RefreshGrowth();activeScratch_=0u;Admit(RetainedBytes());
        NameString result;result.CompareIndex=compare;result.SpelledIndex=spelling;return result;
    }
};
