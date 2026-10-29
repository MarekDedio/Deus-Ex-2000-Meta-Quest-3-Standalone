#pragma once

#include "quest_spawn_play_actor_login.h"

#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace QuestVr {

// Source-matched successful SpawnPlayActor prefix AFTER SetPlayer/Possess:
// retail Engine.dll10397193..103971F4. This is not the travel import/callback
// tail, successful startup, a transaction, or permission to publish a player.
struct OriginalSpawnPlayActorTailQueries {
    // All providers borrow the caller's EXISTING native/VM root. Typed stores
    // must authenticate the original reflected fields and use its journal;
    // no provider may create a nested Execute or commit on this helper's behalf.
    std::function<void()> requireOuterRoot;
    std::function<void()> work;
    std::function<void(SpawnPlayActorNativeId,std::uint8_t)> writeRole;
    std::function<void(SpawnPlayActorNativeId,std::uint8_t)> writeRemoteRole;
    std::function<void(SpawnPlayActorNativeId,std::int32_t)> writeShowFlags;
    std::function<void(SpawnPlayActorNativeId,std::int32_t)> writeRendMap;
    // Borrow the actual appCmdLine storage until parseParam returns. Its
    // provider must implement the original ParseParam semantics, not URL
    // options or an inferred standalone/admin policy. The literal is alladmin.
    std::function<std::u16string_view()> appCmdLine;
    std::function<bool(std::u16string_view,std::u16string_view)> parseParam;
    // The actual supplied native Level+3C NetDriver, NOT LevelInfo.NetMode.
    // Native None is meaningful only after the embedding established this
    // concrete service. A missing provider is never equivalent to None.
    std::function<SpawnPlayActorNativeId(SpawnPlayActorNativeId)> levelNetDriver;
    // OR only bAdmin's original packed bit. A false condition does not store
    // false: existing bAdmin and all neighboring packed bits must survive.
    std::function<void(SpawnPlayActorNativeId)> setAdminTrue;
    // Checked most-derived TravelPreAccept event on the ORIGINAL captured
    // Login-returned pawn, no arguments/result, even if Actor's body is empty.
    std::function<void(SpawnPlayActorNativeId)> invokeTravelPreAccept;
};

namespace SpawnPlayActorTailDetail {
template<class Function,class... Arguments>
inline auto Call(const OriginalSpawnPlayActorTailQueries& query,const Function& function,Arguments&&... arguments) {
    if(!function)throw std::invalid_argument("Original SpawnPlayActor pre-travel prefix lacks a reached provider");
    if(!query.work)throw std::invalid_argument("Original SpawnPlayActor pre-travel prefix requires borrowed root work");
    query.work();return function(std::forward<Arguments>(arguments)...);
}
} // namespace SpawnPlayActorTailDetail

inline void RunOriginalSpawnPlayActorPreTravel(SpawnPlayActorNativeId level,
    SpawnPlayActorNativeId capturedLoginPawn,std::uint8_t suppliedRemoteRole,
    const OriginalSpawnPlayActorTailQueries& query) {
    using SpawnPlayActorTailDetail::Call;
    if(!level||!capturedLoginPawn)
        throw std::invalid_argument("Original SpawnPlayActor pre-travel prefix requires the actual Level and captured Login pawn");
    Call(query,query.requireOuterRoot);
    Call(query,query.writeRole,capturedLoginPawn,std::uint8_t{4u}); //10397196
    Call(query,query.writeRemoteRole,capturedLoginPawn,suppliedRemoteRole); //1039719A
    Call(query,query.writeShowFlags,capturedLoginPawn,std::int32_t{0x480c}); //1039719D
    Call(query,query.writeRendMap,capturedLoginPawn,std::int32_t{5}); //103971A7
    // The command-line query precedes the native NetDriver read even on the
    // real standalone path. Preserve the original short-circuit and do not
    // write bAdmin=false when neither source authorizes its true bit.
    const auto commandLine=Call(query,query.appCmdLine); //103971AD
    const bool allAdmin=Call(query,query.parseParam,commandLine,std::u16string_view{u"alladmin"}); //103971B9
    if(allAdmin||!Call(query,query.levelNetDriver,level)) //103971C9
        Call(query,query.setAdminTrue,capturedLoginPawn); //103971D0..D8
    Call(query,query.invokeTravelPreAccept,capturedLoginPawn); //103971DE..F4
}

// Borrowed views into the native Level's real serialized-or-incoming travel
// map, in serialized order, retaining duplicate/empty entries. The caller MUST
// first establish actual loader ownership and the explicit incoming-map policy;
// an omitted/missing service is never authority for passing an empty span.
struct OriginalSpawnPlayActorTravelEntryView {std::u16string_view key,value;};
struct OriginalSpawnPlayActorTravelQueries {
    std::function<void()> requireOuterRoot;
    std::function<void()> work;
    // Actual FURL.DefaultName, initialized from URL.Name, not GameInfo's
    // DefaultPlayerName. Its storage and all map/URL views remain owned and
    // stable through selection. No helper-owned allocation is performed.
    std::function<std::u16string_view()> defaultName;
    std::function<SpawnPlayActorNativeId(SpawnPlayActorNativeId)> slotZero;
    std::function<bool(SpawnPlayActorNativeId)> isLevelInfo;
    std::function<std::uint8_t(SpawnPlayActorNativeId)> levelInfoNetMode;
};
struct OriginalSpawnPlayActorTravelSelection {
    bool loadOnly{};
    // None means no selected pointer; present-empty is the original nonnull
    // pointer to an empty FString CString and MUST NOT trigger fallback.
    std::optional<std::u16string_view> selected;
};

namespace SpawnPlayActorTravelDetail {
template<class Function,class... Arguments>
inline auto Call(const OriginalSpawnPlayActorTravelQueries& query,const Function& function,Arguments&&... arguments) {
    if(!function)throw std::invalid_argument("Original SpawnPlayActor travel selection lacks a reached provider");
    if(!query.work)throw std::invalid_argument("Original SpawnPlayActor travel selection requires borrowed root work");
    query.work();return function(std::forward<Arguments>(arguments)...);
}
inline void Step(const OriginalSpawnPlayActorTravelQueries& query) {
    if(!query.work)throw std::invalid_argument("Original SpawnPlayActor travel selection requires borrowed root work");
    query.work();
}
inline std::u16string_view CString(std::u16string_view value,const OriginalSpawnPlayActorTravelQueries& query) {
    std::size_t length{};
    for(const auto unit:value){Step(query);if(!unit)break;++length;}
    return value.substr(0u,length);
}
inline bool EqualInsensitive(std::u16string_view a,std::u16string_view b,const OriginalSpawnPlayActorTravelQueries& query) {
    const auto length=a.size()<b.size()?a.size():b.size();
    for(std::size_t i=0u;i<length;++i){
        Step(query);
        // Core appStricmp/appStrnicmp delegate to the original CRT's wide
        // locale. Do not invent non-ASCII equivalences for a reached compare.
        if(a[i]>127u||b[i]>127u)
            throw std::runtime_error("Original SpawnPlayActor travel non-ASCII case locale is unestablished");
        if(SpawnPlayActorLoginDetail::Fold(a[i])!=SpawnPlayActorLoginDetail::Fold(b[i]))return false;
    }
    return a.size()==b.size();
}
} // namespace SpawnPlayActorTravelDetail

// HasOption thunk10303CEC ->103F9DF0 compares entire option CStrings with
// appStricmp. In particular loadonly=1 and xloadonly do NOT select loadonly.
inline bool OriginalSpawnPlayActorUrlHasLoadOnly(const OriginalSpawnPlayActorUrl& url,
    const OriginalSpawnPlayActorTravelQueries& query) {
    for(const auto raw:url.options){
        SpawnPlayActorTravelDetail::Step(query);
        if(SpawnPlayActorTravelDetail::EqualInsensitive(SpawnPlayActorTravelDetail::CString(raw,query),u"loadonly",query))return true;
    }
    return false;
}

// GetOption thunk10303ECC ->103F9EE0 selects the FIRST appStrnicmp NAME=
// prefix and returns its suffix, including present-empty. The fallback remains
// the genuine DefaultName CString; values after the prefix are not folded.
inline std::u16string_view OriginalSpawnPlayActorUrlTravelName(const OriginalSpawnPlayActorUrl& url,
    std::u16string_view defaultName,const OriginalSpawnPlayActorTravelQueries& query) {
    constexpr std::u16string_view prefix=u"NAME=";
    for(const auto raw:url.options){
        SpawnPlayActorTravelDetail::Step(query);
        const auto option=SpawnPlayActorTravelDetail::CString(raw,query);
        if(option.size()<prefix.size())continue;
        if(SpawnPlayActorTravelDetail::EqualInsensitive(option.substr(0u,prefix.size()),prefix,query))return option.substr(prefix.size());
    }
    return SpawnPlayActorTravelDetail::CString(defaultName,query);
}

inline OriginalSpawnPlayActorTravelSelection OriginalSpawnPlayActorSelectTravel(SpawnPlayActorNativeId level,
    const OriginalSpawnPlayActorUrl& url,std::span<const OriginalSpawnPlayActorTravelEntryView> travel,
    const OriginalSpawnPlayActorTravelQueries& query) {
    using SpawnPlayActorTravelDetail::Call;
    using SpawnPlayActorTravelDetail::CString;
    if(!level)throw std::invalid_argument("Original SpawnPlayActor travel selection requires the actual native Level");
    Call(query,query.requireOuterRoot);
    if(OriginalSpawnPlayActorUrlHasLoadOnly(url,query))return {true,std::nullopt}; //10397208
    // The native default FString is evaluated BEFORE GetOption even when an
    // explicit NAME= wins. Do not turn a missing default service into empty.
    const auto defaultName=Call(query,query.defaultName); //10397215..28
    const auto name=OriginalSpawnPlayActorUrlTravelName(url,defaultName,query); //10397230
    // Native loader rehash10346660 inserts ascending serialized entries at
    // each bucket's HEAD; equal ASCII keys have equal hashes, so lookup reaches
    // their LAST serialized pair first. Do not discard duplicate cells.
    for(std::size_t i=travel.size();i>0u;--i){
        SpawnPlayActorTravelDetail::Step(query);
        if(SpawnPlayActorTravelDetail::EqualInsensitive(CString(travel[i-1u].key,query),name,query))
            return {false,CString(travel[i-1u].value,query)}; //1039733A..51
    }
    // Lookup miss always checks fresh slot0 and NetMode BEFORE testing count,
    // even when the actual loader-owned map is empty. Three separate reads
    // reproduce10397361/80/AC; mutable relationships are not cached.
    if(!Call(query,query.slotZero,level))throw std::runtime_error("Original SpawnPlayActor travel assertion: Actors(0)");
    const auto checked=Call(query,query.slotZero,level);
    if(!checked||!Call(query,query.isLevelInfo,checked))
        throw std::runtime_error("Original SpawnPlayActor travel assertion: Actors(0)->IsA(ALevelInfo)");
    const auto info=Call(query,query.slotZero,level);
    if(!info)throw std::runtime_error("Original SpawnPlayActor travel slot zero vanished before NetMode read");
    const auto mode=Call(query,query.levelInfoNetMode,info);
    if(!mode&&!travel.empty())return {false,CString(travel.front().value,query)}; //103973BB..DF
    return {false,std::nullopt};
}

// This initial first-Training seam does not implement actor creation/import,
// travel-property reference fixups or the reverse traveller callback loops.
// Call this BEFORE any empty-traveller callback route. loadonly legitimately
// skips import only; it does not authorize skipping later pawn/Game callbacks.
inline void RequireOriginalSpawnPlayActorEmptyTravellers(const OriginalSpawnPlayActorTravelSelection& selection) {
    if(selection.selected&&!selection.selected->empty())
        throw std::runtime_error("Original SpawnPlayActor nonempty selected travel import is not implemented");
}

} // namespace QuestVr
