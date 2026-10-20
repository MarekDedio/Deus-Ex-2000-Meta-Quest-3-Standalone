#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <exception>
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
        std::uint32_t next{};
        std::uint32_t identityNext{};
    };
    using Slots=std::map<std::uint32_t,Node>;
    struct LinkPosition {
        std::uint32_t previous{},next{};
        std::uintptr_t previousIdentity{},nextIdentity{};
    };
    static constexpr auto StoredNodeBytes=sizeof(typename Slots::value_type)+8u*sizeof(void*);

public:
    static constexpr std::uint32_t MaximumObjects=1'000'000u;
    static constexpr std::uint32_t MaximumObjectIndex=4'000'000u;
    static constexpr std::size_t BucketCount=4096u;

    // A detached owning tree node, not another weak publication. Destroying a
    // token commits that retirement; moving it transfers the sole restoration
    // right. No UObject is owned or kept alive by the token.
    class Retirement {
    public:
        Retirement() noexcept=default;
        Retirement(const Retirement&)=delete;
        Retirement& operator=(const Retirement&)=delete;
        Retirement(Retirement&&) noexcept=default;
        Retirement& operator=(Retirement&&) noexcept=default;
        [[nodiscard]] explicit operator bool() const noexcept {return !node_.empty();}
        [[nodiscard]] const Record* GetRecord() const noexcept {
            return node_.empty()?nullptr:&node_.mapped().record;
        }
        [[nodiscard]] std::size_t RetainedBytes() const noexcept {
            return sizeof(Retirement)+(node_.empty()?0u:StoredNodeBytes);
        }
    private:
        friend class Index;
        typename Slots::node_type node_;
        std::uint64_t authority_{};
        LinkPosition name_,identity_;
    };

    explicit Index(std::uint32_t maximum=MaximumObjects):maximum_(maximum) {
        if(!maximum||maximum>MaximumObjects)
            throw std::invalid_argument("Object lookup capacity exceeds portable policy");
    }
    Index(const Index& other):maximum_(other.maximum_),slots_(other.slots_),
        heads_(other.heads_),identityHeads_(other.identityHeads_) {
        // Links are bounded numeric indices, not addresses into the source
        // map. Its copied nodes therefore preserve both chains verbatim.
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
        if(this==&other)return;
        using std::swap;swap(maximum_,other.maximum_);slots_.swap(other.slots_);
        heads_.swap(other.heads_);identityHeads_.swap(other.identityHeads_);
        // Outstanding retirement rights are tied to one exclusive owner and
        // state, not transferable through copy/move/assignment/swap.
        authority_=NewAuthority();other.authority_=NewAuthority();
    }

    // Preallocate only the new records in a detached Index. Admission and
    // commit do not copy the live world or allocate map nodes. Node transfers
    // preserve actual hash order as well as stable node addresses.
    [[nodiscard]] bool CanAdopt(const Index& additions) const noexcept {
        if(this==&additions || additions.Size()>maximum_-Size()) return false;
        for(const auto& [index,node]:additions.slots_)
            if(slots_.find(index)!=slots_.end() || RecordForIdentity(node.record.identity)) return false;
        return true;
    }
    bool Adopt(Index& additions) noexcept {
        if(!CanAdopt(additions)) return false;
        // The sole map uses fixed integer std::less and equal standard
        // allocators; merge only transfers already allocated, unique nodes.
        slots_.merge(additions.slots_);
        for(std::size_t bucket=0u;bucket<BucketCount;++bucket) {
            // All staged nodes now belong to this map. Resolve their numeric
            // links here, not through the emptied additions map.
            if(const auto head=additions.heads_[bucket]) {
                auto* tail=&NodeAt(head);while(tail->next) tail=&NodeAt(tail->next);
                tail->next=heads_[bucket];
                heads_[bucket]=head;additions.heads_[bucket]=0u;
            }
            if(const auto head=additions.identityHeads_[bucket]) {
                auto* tail=&NodeAt(head);while(tail->identityNext) tail=&NodeAt(tail->identityNext);
                tail->identityNext=identityHeads_[bucket];
                identityHeads_[bucket]=head;additions.identityHeads_[bucket]=0u;
            }
        }
        return true;
    }

    // Strict new publication. Replacement is explicitly Remove(old identity)
    // then Publish(new identity); overwriting a still-live slot is not allowed.
    // Every successful publication hashes at the head. Map insertion failures
    // leave both ordering and membership unchanged.
    void Publish(Record record) {
        if(!Valid(record))throw std::invalid_argument("Invalid object lookup publication");
        if(slots_.find(record.index)!=slots_.end()||RecordForIdentity(record.identity))
            throw std::invalid_argument("Object lookup index or identity is already published");
        if(slots_.size()>=maximum_)throw std::length_error("Object lookup capacity exhausted");
        // One allocating operation with the map's strong guarantee. No weak
        // hash chain changes until its stable owning node exists.
        const auto inserted=slots_.emplace(record.index,Node{record,0u,0u});
        Prepend(inserted.first->second);PrependIdentity(inserted.first->second);
    }

    // Cleanup reads only stored publication keys. Never dereference an Outer
    // which may already have died; stale identity cannot remove a reused slot.
    bool Remove(std::uint32_t index,std::uintptr_t identity) noexcept {
        const auto found=slots_.find(index);
        if(found==slots_.end()||!identity||found->second.record.identity!=identity)return false;
        Unlink(found->second);UnlinkIdentity(found->second);slots_.erase(found);return true;
    }

    // Transactional retirement: immediately absent from EVERY lookup, while
    // retaining the existing tree allocation for a possible rollback. The
    // caller exclusively owns this Index throughout the transaction and must
    // reverse later publications/rehashes/retirements before restoring tokens
    // in LIFO order. This is not a general concurrent or arbitrary-order undo.
    // Copying/moving/swapping/assigning the Index invalidates its old tokens.
    [[nodiscard]] Retirement Extract(std::uint32_t index,std::uintptr_t identity) noexcept {
        const auto found=slots_.find(index);
        if(found==slots_.end()||!identity||found->second.record.identity!=identity)return {};
        Retirement token;
        token.authority_=authority_;
        token.name_=Position(found->second,false);
        token.identity_=Position(found->second,true);
        Unlink(found->second);UnlinkIdentity(found->second);
        token.node_=slots_.extract(found);
        return token;
    }
    // All checks precede the nonallocating map-node transfer and link writes.
    // A stale/foreign/conflicting token or unreversed neighbor mutation leaves
    // BOTH token and Index unchanged, so the legitimate owner may retry later.
    bool Restore(Retirement& token) noexcept {
        if(token.node_.empty()||token.authority_!=authority_||slots_.size()>=maximum_)return false;
        const auto& record=token.node_.mapped().record;
        if(slots_.find(record.index)!=slots_.end()||RecordForIdentity(record.identity)||
            !PositionMatches(token.name_,Bucket(record.outerIndex,record.nameIndex),false)||
            !PositionMatches(token.identity_,IdentityBucket(record.identity),true))return false;
        auto inserted=slots_.insert(std::move(token.node_));
        if(!inserted.inserted) {token.node_=std::move(inserted.node);return false;}
        auto& node=inserted.position->second;
        node.next=token.name_.next;node.identityNext=token.identity_.next;
        Relink(node.record.index,token.name_,Bucket(node.record.outerIndex,node.record.nameIndex),false);
        Relink(node.record.index,token.identity_,IdentityBucket(node.record.identity),true);
        token.authority_=0u;token.name_={};token.identity_={};
        return true;
    }
    bool Rehash(std::uint32_t index,std::uintptr_t identity,std::uintptr_t outerIdentity,
        std::uint32_t outerIndex,int nameIndex) noexcept {
        const Record replacement{identity,outerIdentity,index,outerIndex,nameIndex};
        if(!Valid(replacement))return false;
        const auto found=slots_.find(index);
        if(found==slots_.end()||found->second.record.identity!=identity)return false;
        auto& node=found->second;Unlink(node);node.record=replacement;
        Prepend(node);return true;
    }

    template<class Predicate>
    [[nodiscard]] std::uintptr_t Exact(std::uintptr_t outerIdentity,std::uint32_t outerIndex,
        int nameIndex,Predicate&& predicate) const {
        ValidateKey(outerIdentity,outerIndex,nameIndex);
        if(!nameIndex)return 0u;
        for(auto link=heads_[Bucket(outerIndex,nameIndex)];link;) {
            const auto& node=NodeAt(link);const auto& record=node.record;
            if(record.outerIdentity==outerIdentity&&record.nameIndex==nameIndex&&predicate(record.identity))
                return record.identity;
            link=node.next;
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
        if(!identity)return nullptr;
        for(auto link=identityHeads_[IdentityBucket(identity)];link;) {
            const auto& node=NodeAt(link);
            if(node.record.identity==identity)return &node.record;
            link=node.identityNext;
        }
        return nullptr;
    }
    template<class Visitor>void ForEachAscending(Visitor&& visitor) const {
        for(const auto& [index,node]:slots_) {(void)index;visitor(node.record);}
    }
    [[nodiscard]] std::vector<Record> RecordsAscending() const {
        std::vector<Record> result;result.reserve(slots_.size());
        ForEachAscending([&](const Record& record){result.push_back(record);});return result;
    }
    [[nodiscard]] std::size_t Size() const noexcept {return slots_.size();}
    // One owning ordered map, with the SAME conservative per-tree-node
    // allowance for the tree's three links, alignment/color slack and allocator
    // metadata. The name-hash chain is singly linked, so its redundant reverse
    // pointer is not retained in every live object node.
    // Both weak hash indices use bounded 32-bit object indices in that node;
    // there is no secondary identity-map allocation to charge. sizeof(Index)
    // includes both compact head arrays. Each chain hop resolves through the
    // owning map, trading a nonallocating logarithmic search for pointer space.
    // No UObject/default/name storage is owned here.
    [[nodiscard]] std::size_t RetainedBytes() const noexcept {
        return sizeof(Index)+slots_.size()*StoredNodeBytes;
    }

private:
    [[nodiscard]] static std::uint64_t NewAuthority() noexcept {
        // An address alone would accept a foreign token after owner lifetime
        // reuse. These process-local, nonallocating identities never transfer
        // with tree storage. Exhaustion must not silently reuse an authority.
        static std::atomic<std::uint64_t> next{1u};
        const auto result=next.fetch_add(1u,std::memory_order_relaxed);
        if(!result)std::terminate();
        return result;
    }
    [[nodiscard]] LinkPosition Position(const Node& node,bool identity) const noexcept {
        const auto bucket=identity?IdentityBucket(node.record.identity):Bucket(node.record.outerIndex,node.record.nameIndex);
        auto link=identity?identityHeads_[bucket]:heads_[bucket];
        std::uint32_t previous{};
        while(link!=node.record.index) {
            previous=link;const auto& current=NodeAt(link);
            link=identity?current.identityNext:current.next;
        }
        const auto next=identity?node.identityNext:node.next;
        return {previous,next,previous?NodeAt(previous).record.identity:0u,next?NodeAt(next).record.identity:0u};
    }
    [[nodiscard]] bool PositionMatches(const LinkPosition& position,std::size_t bucket,bool identity) const noexcept {
        const auto matches=[&](std::uint32_t index,std::uintptr_t opaque) {
            const auto found=slots_.find(index);
            if(found==slots_.end()||found->second.record.identity!=opaque)return false;
            const auto& record=found->second.record;
            return (identity?IdentityBucket(record.identity):Bucket(record.outerIndex,record.nameIndex))==bucket;
        };
        if(position.next&&!matches(position.next,position.nextIdentity))return false;
        if(position.previous) {
            if(!matches(position.previous,position.previousIdentity))return false;
            const auto& previous=NodeAt(position.previous);
            return (identity?previous.identityNext:previous.next)==position.next;
        }
        return (identity?identityHeads_[bucket]:heads_[bucket])==position.next;
    }
    void Relink(std::uint32_t index,const LinkPosition& position,std::size_t bucket,bool identity) noexcept {
        if(position.previous) {
            auto& previous=NodeAt(position.previous);
            (identity?previous.identityNext:previous.next)=index;
        }else (identity?identityHeads_[bucket]:heads_[bucket])=index;
    }
    // Zero is the chain sentinel; every nonzero link names a node in this
    // owning map. Publication, unlink and node transfer preserve that invariant.
    // The object-index policy is independent of actual pointer-sized identity.
    [[nodiscard]] Node& NodeAt(std::uint32_t index) noexcept {
        return slots_.find(index)->second;
    }
    [[nodiscard]] const Node& NodeAt(std::uint32_t index) const noexcept {
        return slots_.find(index)->second;
    }
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
    static std::size_t IdentityBucket(std::uintptr_t identity) noexcept {
        // Fold ALL pointer-sized bits, rather than discarding high address
        // bits or assuming that aligned object addresses use their low bits.
        auto folded=identity;
        for(unsigned shift=12u;shift<sizeof(identity)*8u;shift+=12u) folded^=identity>>shift;
        return static_cast<std::size_t>(folded)&(BucketCount-1u);
    }
    void Prepend(Node& node) noexcept {
        const auto bucket=Bucket(node.record.outerIndex,node.record.nameIndex);
        node.next=heads_[bucket];heads_[bucket]=node.record.index;
    }
    void Unlink(Node& node) noexcept {
        // Cached publication keys only: no UObject/Outer dereference. At most
        // Size() owned nodes can occur in this bucket, and no provider callback
        // runs during this nonallocating search. Keep all surviving precedence.
        const auto bucket=Bucket(node.record.outerIndex,node.record.nameIndex);
        auto* link=&heads_[bucket];
        while(*link&&*link!=node.record.index) link=&NodeAt(*link).next;
        if(*link) *link=node.next;
        node.next=0u;
    }
    void PrependIdentity(Node& node) noexcept {
        const auto bucket=IdentityBucket(node.record.identity);
        node.identityNext=identityHeads_[bucket];identityHeads_[bucket]=node.record.index;
    }
    void UnlinkIdentity(Node& node) noexcept {
        // Cached identity only. Neither the UObject nor its Outer is read.
        auto* link=&identityHeads_[IdentityBucket(node.record.identity)];
        while(*link && *link!=node.record.index) link=&NodeAt(*link).identityNext;
        if(*link) *link=node.identityNext;
        node.identityNext=0u;
    }
    std::uint64_t authority_{NewAuthority()};
    std::uint32_t maximum_;
    Slots slots_;
    std::array<std::uint32_t,BucketCount> heads_{};
    std::array<std::uint32_t,BucketCount> identityHeads_{};
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
