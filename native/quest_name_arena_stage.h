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
#include <type_traits>
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
    // The ordinary full stage keeps its existing multi-Add semantics. This
    // optional mode owns ONE original FName operation, and can publish a tiny
    // delta into genuine already-free arena capacity without cloning/traversing
    // every existing name. Genuine growth falls back before any delta exists.
    enum class Mode {Full,IncrementalOriginalName};
    struct Work {
        std::function<void(std::size_t)> step;
        // Absolute ADDITIONAL workspace beyond the unchanged captured arena.
        // Callback storage/copy admission belongs to the caller.
        std::function<void(std::size_t)> admit;
    };

    explicit NameStringArenaStage(Work work={},std::size_t reserveAdditionalNames=0u,Mode mode=Mode::Full)
        :work_(std::move(work)),reserveAdditionalNames_(reserveAdditionalNames),
        incrementalOriginal_(mode==Mode::IncrementalOriginalName) {
        if(mode!=Mode::Full&&mode!=Mode::IncrementalOriginalName)Fail("unrecognized name-arena mode");
        Capture();
        if(incrementalOriginal_)Admit(RetainedBytes());
        else PrepareFull(reserveAdditionalNames);
    }
    NameStringArenaStage(const NameStringArenaStage&)=delete;
    NameStringArenaStage& operator=(const NameStringArenaStage&)=delete;
    NameStringArenaStage(NameStringArenaStage&&)=delete;
    NameStringArenaStage& operator=(NameStringArenaStage&&)=delete;

    NameString Add(std::string_view value) {
        if(committed_||failed_)Fail("stage is already committed or refused");
        if(incrementalOriginal_||incrementalOriginalComplete_) {failed_=true;Fail("incremental original-name mode requires its single AddOriginalName operation");}
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

    // Original FName(CString,FNAME_Add1) returns the first stored spelling of
    // an existing case-insensitive identity. NameString's ordinary Add above
    // intentionally keeps its different per-spelling behavior unchanged.
    // The caller supplies a complete CString prefix; do not silently truncate
    // an embedded NUL or impose an unrelated 63-character name limit here.
    NameString AddOriginalName(std::string_view value) {
        if(committed_||failed_)Fail("stage is already committed or refused");
        if(incrementalOriginalComplete_) {failed_=true;Fail("incremental original-name operation was already supplied");}
        if(incrementalOriginal_) {
            if(incrementalOriginalUsed_) {failed_=true;Fail("incremental original-name operation was already supplied");}
            incrementalOriginalUsed_=true;
            const auto result=AddIncrementalOriginalName(value);
            incrementalOriginalComplete_=true;return result;
        }
        validated_=false;
        try {
            Step(AddBytes(value.size(),1u));CheckUnchanged();
            if(value.find('\0')!=std::string_view::npos)Fail("original name requires its complete CString prefix");
            if(value.empty())return Add("");
            bool none{};
            {
                // Complete the existing-identity lookup before invoking Add:
                // its own admitted temporaries must not hide this string.
                activeScratch_=StringBound(value.size());
                Admit(AddBytes(RetainedBytes(),activeScratch_));
                std::string comparison(value);
                for(char& c:comparison) {
                    const auto byte=static_cast<unsigned char>(c);
                    if(byte>='a'&&byte<='z')c=static_cast<char>(byte-('a'-'A'));
                }
                Step(comparison.size());
                none=comparison=="NONE";
                int compare=-1;
                if(const auto found=deltaCompare_.find(comparison);found!=deltaCompare_.end())compare=found->second;
                else if(const auto found=NameString::CompareStringToIndex.find(comparison);found!=NameString::CompareStringToIndex.end())compare=found->second;
                if(compare>=0) {
                    // The compare row itself is uppercase, NOT the first
                    // display spelling. Recover the earliest genuine spelling
                    // index from the unchanged live and detached delta maps.
                    int first=std::numeric_limits<int>::max();
                    const auto visit=[&](const SpellMap& spellings) {
                        for(const auto& entry:spellings) {
                            Step(1u);
                            if(entry.second.first==compare)first=std::min(first,entry.second.second);
                        }
                    };
                    visit(NameString::SpellStringToIndex);visit(deltaSpell_);
                    if(first<0||first==std::numeric_limits<int>::max()||static_cast<std::size_t>(first)>=names_.size())
                        Fail("original compare identity has no actual first spelling");
                    return Finish(compare,first);
                }
            }
            activeScratch_=0u;
            // Initial None must use the genuine seeded slot0, not Add's
            // intentional first-literal-None duplicate spelling behavior.
            return Add(none?std::string_view{}:value);
        }catch(...) {failed_=true;throw;}
    }

    const std::string& Spelling(const NameString& value) const {
        if(failed_)Fail("refused stage has no pending spelling authority");
        if(!committed_)CheckUnchanged();
        if(incrementalOriginal_&&!committed_) {
            const auto count=AddBytes(liveNamesSize_,names_.size());
            if(value.CompareIndex<0||value.SpelledIndex<0||static_cast<std::size_t>(value.CompareIndex)>=count||
                static_cast<std::size_t>(value.SpelledIndex)>=count)
                Fail("name is outside the incremental captured/tail arena");
            const auto index=static_cast<std::size_t>(value.SpelledIndex);
            return index<liveNamesSize_?NameString::Names[index]:names_[index-liveNamesSize_];
        }
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
    bool UsesIncrementalOriginalName() const noexcept {return incrementalOriginal_;}

    void Validate() {
        if(committed_||failed_)Fail("stage is already committed or refused");
        CheckUnchanged();
        if(incrementalOriginal_) {
            if(!incrementalOriginalUsed_)Fail("incremental original-name operation is absent");
            if(!IncrementalCapacity(names_.size(),deltaCompare_.size(),deltaSpell_.size()))
                Fail("incremental original-name publication lost its captured free capacity");
            if(!validated_)Step(deltaMergeWork_);
            CheckUnchanged();RefreshGrowth();validated_=true;return;
        }
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
        if(incrementalOriginal_) {
            // Validate proved actual free slots and initialized live buckets;
            // string moves and same-allocator node transfer cannot allocate.
            static_assert(std::is_nothrow_move_constructible_v<std::string>);
            for(auto& value:names_)NameString::Names.push_back(std::move(value));
            NameString::CompareStringToIndex.merge(deltaCompare_);
            NameString::SpellStringToIndex.merge(deltaSpell_);
            if(!deltaCompare_.empty()||!deltaSpell_.empty())std::terminate();
            deltaCompareNodeBytes_=0u;deltaSpellNodeBytes_=0u;nameStringBytes_=0u;
            for(const auto& value:names_)nameStringBytes_+=value.capacity()+1u;
            committed_=true;return;
        }
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
    std::size_t reserveAdditionalNames_{};
    bool incrementalOriginal_{},incrementalOriginalUsed_{},incrementalOriginalComplete_{};
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
    void PrepareFull(std::size_t reserveAdditionalNames) {
        baselineBytes_=LiveArenaBytes();
        const auto capacity=AddBytes(liveNamesSize_,reserveAdditionalNames);
        if(capacity>static_cast<std::size_t>(std::numeric_limits<int>::max()))Fail("reserved name index range exceeded");
        const auto clone=AddBytes(Multiply(capacity,sizeof(std::string)),liveNameStringBytes_);
        auto initial=AddBytes(RetainedBytes(),clone);
        initial=AddBytes(initial,BucketBound(NameString::CompareStringToIndex.size()));
        initial=AddBytes(initial,BucketBound(NameString::SpellStringToIndex.size()));
        Admit(initial);
        // Reserve before copying: no old whole-array buffer coexists with a
        // doubled replacement. This is the unchanged ordinary/full fallback.
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
    // Bounded actual-bucket traversal, not an unmetered whole-arena scan or an
    // assumed O(1) hash-table lookup. All hash/equality key bytes are charged.
    template<class Map>const typename Map::mapped_type* FindIncremental(const Map& map,const std::string& key,
        std::size_t* traversal=nullptr) {
        const auto charge=[&](std::size_t amount) {
            Step(amount);if(traversal)*traversal=AddBytes(*traversal,amount);
        };
        charge(AddBytes(key.size(),1u));
        const auto bucket=map.bucket(key);
        for(auto entry=map.begin(bucket);entry!=map.end(bucket);++entry) {
            charge(AddBytes(AddBytes(entry->first.size(),key.size()),1u));
            if(entry->first==key)return &entry->second;
        }
        return nullptr;
    }
    bool IncrementalCapacity(std::size_t names,std::size_t compares,std::size_t spellings) const {
        if(names>liveNamesCapacity_-liveNamesSize_)return false;
        const auto fits=[](std::size_t size,std::size_t buckets,std::size_t extra) {
            // A default single bucket has no initialized resize threshold.
            return !extra||(buckets>1u&&size<=buckets&&extra<=buckets-size);
        };
        return fits(liveCompareSize_,liveCompareBuckets_,compares)&&fits(liveSpellSize_,liveSpellBuckets_,spellings);
    }
    NameString AddIncrementalOriginalName(std::string_view value) {
        validated_=false;
        try {
            Step(AddBytes(value.size(),1u));CheckUnchanged();
            if(value.find('\0')!=std::string_view::npos)Fail("original name requires its complete CString prefix");
            if(liveNamesSize_) {
                activeScratch_=Multiply(StringBound(value.size()),2u);
                Admit(AddBytes(RetainedBytes(),activeScratch_));
                const std::string spelling(value);auto comparison=spelling;
                for(char& c:comparison) {
                    const auto byte=static_cast<unsigned char>(c);
                    if(byte>='a'&&byte<='z')c=static_cast<char>(byte-('a'-'A'));
                }
                Step(comparison.size());
                if(comparison.empty()||comparison=="NONE") {
                    // Logical None uses slot0 even if first literal Add(None)
                    // also retained a spelling at a later actual array slot.
                    if(NameString::Names[0u]!="None")Fail("captured original None slot is malformed");
                    const std::string none="NONE";
                    const auto* zero=FindIncremental(NameString::CompareStringToIndex,none);
                    if(!zero||*zero!=0)Fail("captured original None compare identity is absent");
                    return Finish(0,0);
                }
                std::size_t compareLookupWork{};
                const auto* found=FindIncremental(NameString::CompareStringToIndex,comparison,&compareLookupWork);
                if(found) {
                    const auto compare=*found;
                    if(compare<=0||static_cast<std::size_t>(compare)>=liveNamesSize_-1u)
                        Fail("original compare identity lacks its adjacent first spelling");
                    const auto first=compare+1;
                    const auto& canonical=NameString::Names[static_cast<std::size_t>(first)];
                    const auto* pair=FindIncremental(NameString::SpellStringToIndex,canonical);
                    if(!pair||pair->first!=compare||pair->second!=first||canonical.size()!=comparison.size())
                        Fail("original adjacent first spelling is not authenticated by its real spelling map");
                    Step(AddBytes(NameString::Names[static_cast<std::size_t>(compare)].size(),comparison.size()));
                    if(NameString::Names[static_cast<std::size_t>(compare)]!=comparison)
                        Fail("original compare row disagrees with its actual hash identity");
                    for(std::size_t index=0u;index<canonical.size();++index) {
                        Step(1u);auto byte=static_cast<unsigned char>(canonical[index]);
                        if(byte>='a'&&byte<='z')byte=static_cast<unsigned char>(byte-('a'-'A'));
                        if(byte!=static_cast<unsigned char>(comparison[index]))Fail("original first spelling has a foreign compare value");
                    }
                    return Finish(compare,first);
                }
                std::size_t spellingLookupWork{};
                if(FindIncremental(NameString::SpellStringToIndex,spelling,&spellingLookupWork))
                    Fail("new original name spelling has no corresponding compare identity");
                if(IncrementalCapacity(2u,1u,1u)) {
                    if(liveNamesSize_>static_cast<std::size_t>(std::numeric_limits<int>::max())-2u)
                        Fail("pending original name index range exceeded");
                    Admit(AddBytes(AddBytes(RetainedBytes(),activeScratch_),2u*sizeof(std::string)));
                    names_.reserve(2u);
                    const auto append=[&](const std::string& text) {
                        Step(AddBytes(text.size(),1u));
                        Admit(AddBytes(AddBytes(RetainedBytes(),activeScratch_),StringBound(text.size())));
                        names_.push_back(text);nameStringBytes_=AddBytes(nameStringBytes_,names_.back().capacity()+1u);
                    };
                    append(comparison);append(spelling);
                    const auto compare=static_cast<int>(liveNamesSize_),first=compare+1;
                    // Delta buckets/nodes allocate now. Live maps are NOT
                    // reserved or replaced; only actual free capacity is used.
                    EnsureBuckets(deltaCompare_,1u);EnsureBuckets(deltaSpell_,1u);
                    Step(AddBytes(comparison.size(),spelling.size()));
                    Admit(AddBytes(AddBytes(RetainedBytes(),activeScratch_),
                        AddBytes(sizeof(CompareMap::value_type)+8u*sizeof(void*),StringBound(comparison.size()))));
                    const auto compared=deltaCompare_.emplace(comparison,compare);
                    if(!compared.second)Fail("duplicate original compare delta");
                    deltaCompareNodeBytes_=NodeBytes<CompareMap>(*compared.first);
                    Admit(AddBytes(AddBytes(RetainedBytes(),activeScratch_),
                        AddBytes(sizeof(SpellMap::value_type)+8u*sizeof(void*),StringBound(spelling.size()))));
                    const auto spelled=deltaSpell_.emplace(spelling,std::pair<int,int>{compare,first});
                    if(!spelled.second)Fail("duplicate original spelling delta");
                    deltaSpellNodeBytes_=NodeBytes<SpellMap>(*spelled.first);
                    // Commit searches the populated DESTINATION buckets again.
                    // Capture their actual absent-key traversal above, then
                    // charge it at Validate while the exact arena is unchanged.
                    // Also charge source-key hashing and two tail moves.
                    deltaMergeWork_=AddBytes(AddBytes(compareLookupWork,spellingLookupWork),
                        AddBytes(AddBytes(comparison.size(),spelling.size()),2u));
                    return Finish(compare,first);
                }
            }
            // Existing-name success and tiny-delta success already returned.
            // Temporary input/folded strings are gone BEFORE full fallback.
            activeScratch_=0u;incrementalOriginal_=false;
            PrepareFull(std::max<std::size_t>(reserveAdditionalNames_,2u));
            return AddOriginalName(value);
        }catch(...) {failed_=true;throw;}
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
        if(liveNamesSize_>liveNamesCapacity_)Fail("live name array size exceeds its actual capacity");
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
        if(incrementalOriginal_) {
            growthBytes_=AddBytes(nameStringBytes_,AddBytes(deltaCompareNodeBytes_,deltaSpellNodeBytes_));
            return;
        }
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
