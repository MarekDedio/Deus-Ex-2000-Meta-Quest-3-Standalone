#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

namespace QuestVr::ObjectLookup {

// Weak, process-local authority only. The owner must remove a record before its
// actual object/index lease dies; none of these integers keeps a UObject alive.
// Name and Outer identity are independent of diagnostic/canonical path strings.
struct Record {
    std::uintptr_t identity{},outerIdentity{};
    std::uint32_t index{},outerIndex{};
    int nameIndex{};
    bool operator==(const Record&) const = default;
};

class Index {
    struct Node {
        Record record;
        Node* previous{};
        Node* next{};
        std::size_t bucket{};
    };
    using Slots=std::map<std::uint32_t,Node>;
    using Identities=std::map<std::uintptr_t,std::uint32_t>;

public:
    static constexpr std::uint32_t MaximumObjects=1'000'000u;
    static constexpr std::uint32_t MaximumObjectIndex=4'000'000u;
    static constexpr std::size_t BucketCount=4096u;

    explicit Index(std::uint32_t maximum=MaximumObjects):maximum_(maximum) {
        if(!maximum||maximum>MaximumObjects)
            throw std::invalid_argument("Object lookup capacity exceeds portable policy");
    }
    Index(const Index& other):maximum_(other.maximum_),slots_(other.slots_),identities_(other.identities_) {
        // std::map node addresses differ after a copy. Reconstruct links from
        // the old chains, NOT ascending slot order or object allocation order.
        for(std::size_t bucket=0u;bucket<BucketCount;++bucket) {
            Node* previous{};
            for(auto* old=other.heads_[bucket];old;old=old->next) {
                auto* current=&slots_.find(old->record.index)->second;
                current->previous=previous;current->next=nullptr;
                if(previous)previous->next=current;else heads_[bucket]=current;
                previous=current;
            }
        }
    }
    Index& operator=(const Index& other) {
        if(this!=&other) {Index replacement(other);Swap(replacement);}
        return *this;
    }
    Index(Index&& other) noexcept:maximum_(other.maximum_) {Swap(other);}
    Index& operator=(Index&& other) noexcept {
        if(this!=&other) {Index replacement(std::move(other));Swap(replacement);}
        return *this;
    }
    void Swap(Index& other) noexcept {
        using std::swap;swap(maximum_,other.maximum_);slots_.swap(other.slots_);
        identities_.swap(other.identities_);heads_.swap(other.heads_);
    }

    // Preallocate only the new records in a detached Index. Admission and
    // commit do not copy the live world or allocate map nodes. Node transfers
    // preserve actual hash order as well as stable node addresses.
    [[nodiscard]] bool CanAdopt(const Index& additions) const noexcept {
        if(this==&additions || additions.Size()>maximum_-Size()) return false;
        for(const auto& [index,node]:additions.slots_)
            if(slots_.find(index)!=slots_.end() || identities_.find(node.record.identity)!=identities_.end()) return false;
        return true;
    }
    bool Adopt(Index& additions) noexcept {
        if(!CanAdopt(additions)) return false;
        // Both maps use fixed integer std::less comparators and equal standard
        // allocators; merge only transfers already allocated, unique nodes.
        slots_.merge(additions.slots_);identities_.merge(additions.identities_);
        for(std::size_t bucket=0u;bucket<BucketCount;++bucket) {
            auto* head=additions.heads_[bucket];if(!head) continue;
            auto* tail=head;while(tail->next) tail=tail->next;
            tail->next=heads_[bucket];if(tail->next) tail->next->previous=tail;
            heads_[bucket]=head;additions.heads_[bucket]=nullptr;
        }
        return true;
    }

    // Strict new publication. Replacement is explicitly Remove(old identity)
    // then Publish(new identity); overwriting a still-live slot is not allowed.
    // Every successful publication hashes at the head. Map insertion failures
    // leave both ordering and membership unchanged.
    void Publish(Record record) {
        if(!Valid(record))throw std::invalid_argument("Invalid object lookup publication");
        if(slots_.find(record.index)!=slots_.end()||identities_.find(record.identity)!=identities_.end())
            throw std::invalid_argument("Object lookup index or identity is already published");
        if(slots_.size()>=maximum_)throw std::length_error("Object lookup capacity exhausted");
        const auto identity=identities_.emplace(record.identity,record.index);
        try {
            const auto inserted=slots_.emplace(record.index,Node{record,nullptr,nullptr,Bucket(record.outerIndex,record.nameIndex)});
            Prepend(inserted.first->second);
        }catch(...) {identities_.erase(identity.first);throw;}
    }

    // Cleanup reads only stored publication keys. Never dereference an Outer
    // which may already have died; stale identity cannot remove a reused slot.
    bool Remove(std::uint32_t index,std::uintptr_t identity) noexcept {
        const auto found=slots_.find(index);
        if(found==slots_.end()||!identity||found->second.record.identity!=identity)return false;
        Unlink(found->second);identities_.erase(identity);slots_.erase(found);return true;
    }
    bool Rehash(std::uint32_t index,std::uintptr_t identity,std::uintptr_t outerIdentity,
        std::uint32_t outerIndex,int nameIndex) noexcept {
        const Record replacement{identity,outerIdentity,index,outerIndex,nameIndex};
        if(!Valid(replacement))return false;
        const auto found=slots_.find(index);
        if(found==slots_.end()||found->second.record.identity!=identity)return false;
        auto& node=found->second;Unlink(node);node.record=replacement;
        node.bucket=Bucket(outerIndex,nameIndex);Prepend(node);return true;
    }

    template<class Predicate>
    [[nodiscard]] std::uintptr_t Exact(std::uintptr_t outerIdentity,std::uint32_t outerIndex,
        int nameIndex,Predicate&& predicate) const {
        ValidateKey(outerIdentity,outerIndex,nameIndex);
        if(!nameIndex)return 0u;
        for(auto* node=heads_[Bucket(outerIndex,nameIndex)];node;node=node->next) {
            const auto& record=node->record;
            if(record.outerIdentity==outerIdentity&&record.nameIndex==nameIndex&&predicate(record.identity))
                return record.identity;
        }
        return 0u;
    }

    template<class Predicate>
    [[nodiscard]] std::uintptr_t Any(int nameIndex,Predicate&& predicate) const {
        if(nameIndex<0)throw std::invalid_argument("Negative object lookup name index");
        if(!nameIndex)return 0u;
        for(const auto& [index,node]:slots_) {
            (void)index;
            // Name-first is a bounded pure lookup optimization, not a claim
            // about retail iterator callback traces. Irrelevant names cannot
            // affect selection and need no portable Class metadata admission.
            if(node.record.nameIndex==nameIndex&&predicate(node.record.identity))
                return node.record.identity;
        }
        return 0u;
    }

    [[nodiscard]] const Record* RecordAt(std::uint32_t index) const noexcept {
        const auto found=slots_.find(index);return found==slots_.end()?nullptr:&found->second.record;
    }
    [[nodiscard]] const Record* RecordForIdentity(std::uintptr_t identity) const noexcept {
        const auto found=identities_.find(identity);return found==identities_.end()?nullptr:RecordAt(found->second);
    }
    template<class Visitor>void ForEachAscending(Visitor&& visitor) const {
        for(const auto& [index,node]:slots_) {(void)index;visitor(node.record);}
    }
    [[nodiscard]] std::vector<Record> RecordsAscending() const {
        std::vector<Record> result;result.reserve(slots_.size());
        ForEachAscending([&](const Record& record){result.push_back(record);});return result;
    }
    [[nodiscard]] std::size_t Size() const noexcept {return slots_.size();}
    // Conservative portable map-node accounting: stored pair, three tree
    // links, alignment/color slack and allocator metadata for both trees. No
    // UObject/default/name storage is owned here. sizeof(Index) includes heads.
    [[nodiscard]] std::size_t RetainedBytes() const noexcept {
        constexpr auto nodeOverhead=8u*sizeof(void*);
        return sizeof(Index)+slots_.size()*(sizeof(typename Slots::value_type)+
            sizeof(typename Identities::value_type)+2u*nodeOverhead);
    }

private:
    [[nodiscard]] bool Valid(const Record& record) const noexcept {
        return record.identity&&record.index&&record.index<=MaximumObjectIndex&&record.outerIndex<=MaximumObjectIndex&&
            record.nameIndex>=0&&((record.outerIdentity==0u)==(record.outerIndex==0u))&&
            record.identity!=record.outerIdentity&&(!record.outerIdentity||record.index!=record.outerIndex);
    }
    void ValidateKey(std::uintptr_t outerIdentity,std::uint32_t outerIndex,int nameIndex) const {
        if(outerIndex>MaximumObjectIndex||nameIndex<0||((outerIdentity==0u)!=(outerIndex==0u)))
            throw std::invalid_argument("Invalid object lookup search key");
    }
    static std::size_t Bucket(std::uint32_t outerIndex,int nameIndex) noexcept {
        return (outerIndex^static_cast<std::uint32_t>(nameIndex))&(BucketCount-1u);
    }
    void Prepend(Node& node) noexcept {
        node.previous=nullptr;node.next=heads_[node.bucket];
        if(node.next)node.next->previous=&node;
        heads_[node.bucket]=&node;
    }
    void Unlink(Node& node) noexcept {
        if(node.previous)node.previous->next=node.next;else heads_[node.bucket]=node.next;
        if(node.next)node.next->previous=node.previous;
        node.previous=nullptr;node.next=nullptr;
    }
    std::uint32_t maximum_;
    Slots slots_;
    Identities identities_;
    std::array<Node*,BucketCount> heads_{};
};

struct Query {
    std::uintptr_t requestedClass{},outerIdentity{};
    std::uint32_t outerIndex{};
    bool anyPackage{},exact{};
    std::string_view name;
};
inline constexpr std::size_t MaximumInputBytes=16384u,MaximumSegments=256u;

// This is ResolveName(Create0,Throw0) plus StaticFindObject's two search
// phases, not a loader/constructor or canonical-string-path lookup. findName
// must ONLY find an existing FName comparison index; matcher authenticates
// actual Class/Super pointers. Neither callback may mutate this weak index.
template<class FindExistingName,class ClassMatcher>
[[nodiscard]] std::uintptr_t Find(const Index& index,const Query& query,
    std::uintptr_t objectClass,std::uintptr_t packageClass,FindExistingName&& findName,ClassMatcher&& matcher) {
    if(query.name.empty()||query.name.size()>MaximumInputBytes||query.name.find('\0')!=std::string_view::npos)
        throw std::invalid_argument("Object lookup input is outside bounded name profile");
    if(!objectClass||!packageClass||query.outerIndex>Index::MaximumObjectIndex||
        ((query.outerIdentity==0u)!=(query.outerIndex==0u))||
        (query.anyPackage&&(query.outerIdentity||query.outerIndex)))
        throw std::invalid_argument("Object lookup requires actual class/Outer authority");
    auto lower=[](char c){return c>='A'&&c<='Z'?static_cast<char>(c-'A'+'a'):c;};
    if(query.name.size()>=4u&&lower(query.name[0])=='i'&&lower(query.name[1])=='n'&&
        lower(query.name[2])=='i'&&query.name[3]==':')
        throw std::invalid_argument("Object lookup ini: resolution is not implemented");
    if(query.name.size()>=4u&&lower(query.name[0])=='u'&&lower(query.name[1])=='s'&&
        lower(query.name[2])=='r'&&query.name[3]==':')
        throw std::invalid_argument("Object lookup usr: resolution is not implemented");
    std::size_t segments=1u;
    for(std::size_t i=0u;i<query.name.size();++i)if(query.name[i]=='.') {
        if(!i||i+1u==query.name.size()||query.name[i-1u]=='.'||++segments>MaximumSegments)
            throw std::invalid_argument("Object lookup dotted name exceeds bounded profile");
    }
    auto outerIdentity=query.outerIdentity;auto outerIndex=query.outerIndex;
    std::size_t cursor{};
    for(;;) {
        const auto dot=query.name.find('.',cursor);
        const auto segment=query.name.substr(cursor,dot==std::string_view::npos?dot:dot-cursor);
        const std::optional<int> nameIndex=findName(segment);
        if(!nameIndex||!*nameIndex)return 0u;
        if(*nameIndex<0)throw std::invalid_argument("Name resolver returned a negative comparison index");
        if(dot==std::string_view::npos) {
            const auto found=index.Exact(outerIdentity,outerIndex,*nameIndex,[&](std::uintptr_t candidate){
                return !query.requestedClass||matcher(candidate,query.requestedClass,query.exact);
            });
            if(found||!query.anyPackage)return found;
            return index.Any(*nameIndex,[&](std::uintptr_t candidate){
                return matcher(candidate,objectClass,false)&&
                    (!query.requestedClass||matcher(candidate,query.requestedClass,query.exact));
            });
        }
        auto prefix=index.Exact(outerIdentity,outerIndex,*nameIndex,[&](std::uintptr_t candidate){
            return matcher(candidate,packageClass,false);
        });
        if(!prefix)prefix=index.Exact(outerIdentity,outerIndex,*nameIndex,[&](std::uintptr_t candidate){
            return matcher(candidate,objectClass,false);
        });
        if(!prefix)return 0u;
        const auto* actual=index.RecordForIdentity(prefix);
        if(!actual)throw std::logic_error("Resolved prefix lost weak object authority");
        outerIdentity=prefix;outerIndex=actual->index;cursor=dot+1u;
    }
}

} // namespace QuestVr::ObjectLookup
