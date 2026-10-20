#pragma once

#include "quest_unreal_string.h"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace QuestVr {

struct OriginalConversationBindLimits {
    // Portable finite safety policy, not original engine counters. All calls
    // must additionally borrow the enclosing root's work/write/undo authority.
    std::size_t steps{4'194'304u},links{65'536u},textUnits{8192u};
};
struct OriginalConversationBindStats {
    std::size_t steps{},purged{},visited{},accepted{},created{};
};
struct OriginalConversationBindQueries {
    std::function<std::uintptr_t(std::uintptr_t)> sourceHead,sourceNext,sourceConversation;
    std::function<std::uintptr_t(std::uintptr_t)> actorHead,nodeNext,nodeConversation,outer;
    // Conversation.conName FName text (+28), NOT UObject.Name (+20).
    std::function<std::string(std::uintptr_t)> conversationName,ownerName,bindName,barkName;
    std::function<std::int32_t(std::uintptr_t)> ownerRefCount;
    std::function<void(std::uintptr_t,std::int32_t)> writeOwnerRefCount;
    std::function<void(std::uintptr_t)> destroyNode;
    // Exact ConSys.ConListItem class construction, Name=None, template=None.
    // Actual Outer is supplied; allocation flags are0. The real allocator must
    // apply concrete class flags, original unique naming, constructor zeros,
    // object-index/lookup ownership and SAME-root rollback before returning.
    std::function<std::uintptr_t(std::uintptr_t,std::uint32_t)> createNode;
    std::function<std::uint32_t(std::uintptr_t)> flags;
    std::function<void(std::uintptr_t,std::uint32_t)> writeFlags;
    std::function<void(std::uintptr_t,std::uintptr_t)> writeNodeConversation,writeNodeNext,writeActorHead;
    std::function<void()> work;
};

namespace ConversationBindDetail {
[[noreturn]] inline void Fail(const char* reason) {
    throw std::runtime_error(std::string("Original conversation binding: ")+reason);
}
inline void Limits(const OriginalConversationBindLimits& value) {
    const OriginalConversationBindLimits maximum;
    if(!value.steps||value.steps>maximum.steps||!value.links||value.links>maximum.links||
        !value.textUnits||value.textUnits>maximum.textUnits)Fail("callers may only tighten positive safety bounds");
}
inline void Step(OriginalConversationBindStats& stats,const OriginalConversationBindLimits& limits,
    const OriginalConversationBindQueries& query) {
    if(stats.steps>=limits.steps)Fail("shared protocol work budget exhausted");
    if(query.work)query.work();
    ++stats.steps;
}
template<class F,class... Args> inline auto Call(OriginalConversationBindStats& stats,
    const OriginalConversationBindLimits& limits,const OriginalConversationBindQueries& query,const F& provider,Args... args) {
    if(!provider)Fail("missing active provider");
    Step(stats,limits,query);return provider(std::forward<Args>(args)...);
}
inline std::u16string Text(std::string text,const OriginalConversationBindLimits& limits) {
    if(text.size()>4u*limits.textUnits)Fail("text byte bound exceeded");
    auto result=UnrealString::Decode(text);
    if(result.size()>limits.textUnits)Fail("text unit bound exceeded");
    // Native FString -> TCHAR* consumers terminate at the first zero unit.
    // Keep transport validation over the entire actual field, including suffix.
    result.resize(UnrealString::CString(result).size());return result;
}
inline std::int32_t Count(std::int32_t value,bool increment) {
    return std::bit_cast<std::int32_t>(std::bit_cast<std::uint32_t>(value)+(increment?1u:0xffffffffu));
}
} // namespace ConversationBindDetail

// Complete ConSys.dll10010B70..10010D34 protocol, NOT the pinned guide's two
// owner-name passes. Handles are opaque CPU-owner identities. This helper owns
// no transaction/objects and does not enable natives2100/2101/2102 by itself.
// Deletions, allocations, class counters, counts and links must share ONE root.
inline OriginalConversationBindStats BindOriginalConversations(std::uintptr_t list,std::uintptr_t actor,
    const OriginalConversationBindQueries& query,const OriginalConversationBindLimits& limits={}) {
    using namespace ConversationBindDetail;
    Limits(limits);if(!list||!actor)Fail("nonnull list and Actor required");
    OriginalConversationBindStats stats;
    const auto call=[&](const auto& provider,auto... args){return Call(stats,limits,query,provider,args...);};
    // EBP captures the source head BEFORE purge; it is not reread afterward.
    auto source=call(query.sourceHead,list);
    auto old=call(query.actorHead,actor);
    while(old) {
        if(stats.purged>=limits.links)Fail("purge link bound exceeded");
        const auto conversation=call(query.nodeConversation,old);
        if(!conversation)Fail("bound node has null conversation");
        const auto count=call(query.ownerRefCount,conversation);
        call(query.writeOwnerRefCount,conversation,Count(count,false));
        const auto next=call(query.nodeNext,old);
        call(query.destroyNode,old); // Original scalar deleting destructor, not GC unlink.
        ++stats.purged;old=next;
    }
    call(query.writeActorHead,actor,std::uintptr_t{}); // Also stores when old head wasNone.
    while(source) {
        if(stats.visited>=limits.links)Fail("source link bound exceeded");
        ++stats.visited;
        const auto conversation=call(query.sourceConversation,source);
        if(!conversation)Fail("source item has null Conversation");
        const auto name=Text(call(query.conversationName,conversation),limits);
        const bool isBark=name.find(u"_Bark")!=std::u16string::npos;
        const auto owner=Text(call(query.ownerName,conversation),limits);
        const auto bark=Text(call(query.barkName,actor),limits);
        bool accepted=UnrealString::EqualInsensitive(bark,owner)&&isBark;
        if(!accepted) {
            const auto ordinaryOwner=Text(call(query.ownerName,conversation),limits);
            const auto bind=Text(call(query.bindName,actor),limits);
            if(UnrealString::EqualInsensitive(bind,ordinaryOwner)) {
                const auto emptyTest=Text(call(query.barkName,actor),limits);
                if(emptyTest.empty())accepted=true;
                else {
                    // The native body really rereads/recompares this field.
                    const auto repeated=Text(call(query.barkName,actor),limits);
                    accepted=!repeated.empty()&&!isBark;
                }
            }
        }
        if(accepted) {
            ++stats.accepted;
            const auto count=call(query.ownerRefCount,conversation);
            call(query.writeOwnerRefCount,conversation,Count(count,true)); // BEFORE allocation.
            const auto outer=call(query.outer,list);
            const auto node=call(query.createNode,outer,std::uint32_t{});
            if(!node)Fail("ConListItem allocation returned null");
            ++stats.created;
            const auto flags=call(query.flags,node); // Captured before con store.
            call(query.writeNodeConversation,node,conversation);
            call(query.writeFlags,node,flags|4u); // RF_Public, not RF_Transient.
            const auto head=call(query.actorHead,actor);
            call(query.writeNodeNext,node,head);
            call(query.writeActorHead,actor,node);
        }
        source=call(query.sourceNext,source); // Live Next read AFTER allocation/stores.
    }
    return stats;
}

struct OriginalConversationMissionQueries {
    std::function<std::uintptr_t(std::uintptr_t)> xlevel;
    std::function<std::size_t(std::uintptr_t)> actorCount;
    std::function<std::uintptr_t(std::uintptr_t,std::size_t)> actorAt;
    std::function<bool(std::uintptr_t)> isDeusExLevelInfo;
    std::function<void()> missingLevelInfoLog;
    std::function<std::int32_t(std::uintptr_t)> missionNumber;
    std::function<std::string(std::uintptr_t)> conversationPackage;
    // Native exact ConversationList class, Outer=None, filename=None,
    // loadFlags2, sandbox=None. Actual loader owns resolution/type/lifetime.
    std::function<std::uintptr_t(const std::string&,std::uint32_t)> loadList;
    std::function<void(std::uintptr_t,std::uintptr_t)> bind;
    std::function<void()> work;
};
struct OriginalConversationMissionResult {
    enum class Disposition { MissingLevelInfo,NegativeMission,MissingList,Bound };
    Disposition disposition{Disposition::MissingLevelInfo};
    std::size_t steps{},slots{};
    std::uintptr_t info{},list{};
    std::int32_t mission{};
    std::string path;
};
// Complete normal wrapper protocol shared by original native2100/2101/2102.
// No content list is selected by scanning ConMissionList or presentation data.
inline OriginalConversationMissionResult BindOriginalActorConversationMission(std::uintptr_t actor,
    const OriginalConversationMissionQueries& query,const OriginalConversationBindLimits& limits={}) {
    using namespace ConversationBindDetail;
    Limits(limits);if(!actor)Fail("nonnull wrapper Actor required");
    OriginalConversationMissionResult result;
    const auto call=[&](const auto& provider,auto... args) {
        if(!provider)Fail("missing active wrapper provider");
        if(result.steps>=limits.steps)Fail("wrapper work budget exhausted");
        if(query.work)query.work();
        ++result.steps;return provider(args...);
    };
    for(std::size_t slot=0;;++slot) {
        const auto level=call(query.xlevel,actor);
        if(!level)Fail("wrapper has null native XLevel");
        if(slot>=call(query.actorCount,level))break;
        if(result.slots>=limits.links)Fail("wrapper Actor slot bound exceeded");
        ++result.slots;
        const auto candidate=call(query.actorAt,level,slot);
        if(candidate&&call(query.isDeusExLevelInfo,candidate)){result.info=candidate;break;}
    }
    if(!result.info){call(query.missingLevelInfoLog);return result;}
    result.mission=call(query.missionNumber,result.info);
    if(result.mission<0){result.disposition=OriginalConversationMissionResult::Disposition::NegativeMission;return result;}
    auto prefix=Text(call(query.conversationPackage,result.info),limits);
    if(UnrealString::EqualInsensitive(prefix,u"DeusExConversations"))prefix=u"DeusExCon";
    auto number=std::to_string(result.mission);if(number.size()<2u)number.insert(number.begin(),'0');
    auto name=std::u16string(u"ConList_Mission");for(const char c:number)name.push_back(static_cast<char16_t>(c));
    auto path=prefix+u"Text."+name;
    // Original's three stack TCHAR buffers each hold128 units. Portable policy
    // refuses would-overflow input rather than reproducing memory corruption.
    if(prefix.size()>=128u||name.size()>=128u||path.size()>=128u)Fail("original wrapper TCHAR buffer bound exceeded");
    result.path=UnrealString::Encode(path);
    result.list=call(query.loadList,result.path,std::uint32_t{2u});
    if(!result.list){result.disposition=OriginalConversationMissionResult::Disposition::MissingList;return result;}
    call(query.bind,result.list,actor);result.disposition=OriginalConversationMissionResult::Disposition::Bound;
    return result;
}

} // namespace QuestVr
