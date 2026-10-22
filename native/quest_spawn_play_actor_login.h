#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <span>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace QuestVr {

// Source-matched REACHED PREFIX only: Engine.dll10396BD0..103970ED.
// This does not implement SpawnPlayActor's SetPlayer/Possess/role/travel tail,
// accept world startup, create a transaction, or manufacture a player.
using SpawnPlayActorNativeId=std::uintptr_t;
struct OriginalSpawnPlayActorUrl {
    // Borrowed actual const FURL fields. Providers may not mutate or invalidate
    // their storage before invokeLogin returns (the exclusive owner retains it).
    std::u16string_view portal;
    std::span<const std::u16string_view> options;
};
struct OriginalSpawnPlayActorLoginQueries {
    // All providers join the caller's SAME native/VM root and error storage.
    // Providers that allocate must admit their own CURRENT retained/scratch
    // peak before allocation; callbacks must not leave an unbalanced ledger.
    std::function<void()> requireOuterRoot;
    std::function<void()> work;
    std::function<void(std::size_t)> admitTemporaryBytes;
    std::function<void(std::u16string_view)> assignOutError;
    // Original10590C40 is UNetConnection, NOT UViewport. A standalone
    // Viewport fails this actual class predicate and retains sandbox None.
    std::function<bool(SpawnPlayActorNativeId)> playerIsNetConnection;
    std::function<SpawnPlayActorNativeId(SpawnPlayActorNativeId)> netConnectionSandbox;
    // The actual registered APlayerPawn UClass, not a guessed selected class.
    std::function<SpawnPlayActorNativeId()> playerPawnClass;
    // Native FURL.GetOption("CLASS=",nullptr). A borrowed result must be the
    // FIRST case-insensitive prefix match's suffix, including present-empty.
    // OriginalSpawnPlayActorUrlClass below supplies that exact selection seam.
    std::function<std::optional<std::u16string_view>(const OriginalSpawnPlayActorUrl&)> optionalUrlClass;
    // Original UObject.StaticLoadClass(base,nullptr,token,nullptr,2,sandbox).
    // This provider owns real class/config/package authority and verifies the
    // actual returned UClass is base-derived; failure returns native None.
    std::function<SpawnPlayActorNativeId(SpawnPlayActorNativeId,std::u16string_view,SpawnPlayActorNativeId)> loadPlayerClass;
    // LocalizeError("LoadPlayerClass",L"Engine",nullptr), then GError.Logf
    // (L"%s",localized). MUST be fatal; an unexpected return also refuses.
    std::function<void()> fatalLoadPlayerClass;
    std::function<SpawnPlayActorNativeId(SpawnPlayActorNativeId)> slotZero;
    std::function<bool(SpawnPlayActorNativeId)> isLevelInfo;
    std::function<SpawnPlayActorNativeId(SpawnPlayActorNativeId)> levelInfoGame;
    std::function<void(std::u16string_view)> logError;
    // Dynamic most-derived Game.Login(Portal,Options,OUT Error,SpawnClass).
    // OUT Error is the SAME caller-owned storage used by assignOutError;
    // the provider performs genuine script dispatch/OUT copyback in this root.
    std::function<SpawnPlayActorNativeId(SpawnPlayActorNativeId,std::u16string_view,
        std::u16string_view,SpawnPlayActorNativeId)> invokeLogin;
};
enum class OriginalSpawnPlayActorLoginOutcome {LoginReturned,NoLevelInfo,NoGame};
struct OriginalSpawnPlayActorLoginResult {
    OriginalSpawnPlayActorLoginOutcome outcome{};
    SpawnPlayActorNativeId player{}; // An actual Login None is retained as None.
};

namespace SpawnPlayActorLoginDetail {
template<class Function,class... Arguments>
inline auto Call(const OriginalSpawnPlayActorLoginQueries& query,const Function& function,Arguments&&... arguments) {
    if(!function)throw std::invalid_argument("Original SpawnPlayActor prefix lacks a reached provider");
    if(!query.work)throw std::invalid_argument("Original SpawnPlayActor prefix requires borrowed root work");
    query.work();return function(std::forward<Arguments>(arguments)...);
}
inline void Step(const OriginalSpawnPlayActorLoginQueries& query) {
    if(!query.work)throw std::invalid_argument("Original SpawnPlayActor prefix requires borrowed root work");
    query.work();
}
inline std::u16string_view CString(std::u16string_view value,const OriginalSpawnPlayActorLoginQueries& query) {
    // Original *FString, appStrlen/appStrcat stop at the first UTF16 NUL.
    std::size_t length{};
    for(const auto unit:value) {Step(query);if(!unit)break;++length;}
    return value.substr(0u,length);
}
inline char16_t Fold(char16_t value) {return value>=u'A'&&value<=u'Z'?static_cast<char16_t>(value+(u'a'-u'A')):value;}
struct Scratch final {
    std::size_t& ledger;const std::size_t before;
    Scratch(std::size_t& value,const OriginalSpawnPlayActorLoginQueries& query,std::size_t bytes):ledger(value),before(value) {
        if(bytes>std::numeric_limits<std::size_t>::max()-before)
            throw std::runtime_error("Original SpawnPlayActor borrowed scratch overflows");
        Call(query,query.admitTemporaryBytes,bytes);ledger=before+bytes;
    }
    ~Scratch()noexcept{ledger=before;}
    Scratch(const Scratch&)=delete;
    Scratch& operator=(const Scratch&)=delete;
};
inline void CheckLevelInfo(SpawnPlayActorNativeId level,const OriginalSpawnPlayActorLoginQueries& query) {
    // Original checkSlow Actors(0), then Actors(0)->IsA(ALevelInfo), each
    // rereading the native slot. Refuse invalid storage instead of suppressing
    // appFailAssert or continuing into an unsafe original dereference.
    if(!Call(query,query.slotZero,level))throw std::runtime_error("Original SpawnPlayActor assertion: Actors(0)");
    const auto actor=Call(query,query.slotZero,level);
    if(!actor||!Call(query,query.isLevelInfo,actor))
        throw std::runtime_error("Original SpawnPlayActor assertion: Actors(0)->IsA(ALevelInfo)");
}
} // namespace SpawnPlayActorLoginDetail

// FURL.GetOption thunk10303ECC ->103F9EE0: ascending Op, appStrnicmp
// against literal CLASS=, return suffix of FIRST match, else nullptr. Core's
// appStrnicmp delegates to MSVCRT._wcsnicmp; unestablished non-ASCII locale
// equivalences in a still-potential matching prefix explicitly refuse.
inline std::optional<std::u16string_view> OriginalSpawnPlayActorUrlClass(
    const OriginalSpawnPlayActorUrl& url,const OriginalSpawnPlayActorLoginQueries& query) {
    constexpr std::u16string_view prefix=u"CLASS=";
    for(const auto raw:url.options) {
        SpawnPlayActorLoginDetail::Step(query);
        const auto option=SpawnPlayActorLoginDetail::CString(raw,query);
        if(option.size()<prefix.size())continue;
        bool matches=true;
        for(std::size_t i=0u;i<prefix.size();++i) {
            SpawnPlayActorLoginDetail::Step(query);
            if(option[i]>127u)
                throw std::runtime_error("Original SpawnPlayActor non-ASCII CLASS prefix locale is unestablished");
            if(SpawnPlayActorLoginDetail::Fold(option[i])!=SpawnPlayActorLoginDetail::Fold(prefix[i])) {matches=false;break;}
        }
        if(matches)return option.substr(prefix.size());
    }
    return std::nullopt;
}

inline OriginalSpawnPlayActorLoginResult RunOriginalSpawnPlayActorLoginPrefix(
    SpawnPlayActorNativeId level,SpawnPlayActorNativeId player,const OriginalSpawnPlayActorUrl& url,
    const OriginalSpawnPlayActorLoginQueries& query,std::size_t& borrowedScratchLedger) {
    using SpawnPlayActorLoginDetail::Call;
    using SpawnPlayActorLoginDetail::CString;
    if(!level)throw std::invalid_argument("Original SpawnPlayActor prefix requires an actual native Level");
    Call(query,query.requireOuterRoot);
    Call(query,query.assignOutError,std::u16string_view{}); //10396BFF..10396C67
    SpawnPlayActorNativeId sandbox{};
    if(player&&Call(query,query.playerIsNetConnection,player))sandbox=Call(query,query.netConnectionSandbox,player);
    const auto selectedOption=Call(query,query.optionalUrlClass,url);
    const auto base=Call(query,query.playerPawnClass);
    if(!base)throw std::runtime_error("Original SpawnPlayActor lacks actual PlayerPawn UClass");
    SpawnPlayActorNativeId spawnClass{};
    if(selectedOption)spawnClass=Call(query,query.loadPlayerClass,base,*selectedOption,sandbox);
    if(!spawnClass)spawnClass=Call(query,query.loadPlayerClass,base,std::u16string_view{u"usr:DefaultPlayer.Class"},sandbox);
    if(!spawnClass)spawnClass=Call(query,query.loadPlayerClass,base,std::u16string_view{u"ini:URL.Class"},sandbox);
    if(!spawnClass) {
        Call(query,query.fatalLoadPlayerClass);
        throw std::runtime_error("Original SpawnPlayActor fatal class failure unexpectedly returned");
    }
    // Native10396D34..10396DA6 uses unbounded appStrcat into1024 WCHAR.
    // Portable safety refuses overflow BEFORE copying, never truncates or
    // silently omits an option. This buffer is borrowed only until Login returns.
    SpawnPlayActorLoginDetail::Scratch scratch(borrowedScratchLedger,query,sizeof(std::array<char16_t,1024u>));
    std::array<char16_t,1024u> options{};std::size_t size{};
    for(const auto raw:url.options) {
        SpawnPlayActorLoginDetail::Step(query);
        const auto option=CString(raw,query);
        if(option.size()>options.size()-2u||size>options.size()-2u-option.size())
            throw std::runtime_error("Original SpawnPlayActor options exceed the native1024-WCHAR buffer");
        options[size++]=u'?';
        for(const auto unit:option) {SpawnPlayActorLoginDetail::Step(query);options[size++]=unit;}
        options[size]=0;
    }
    SpawnPlayActorLoginDetail::CheckLevelInfo(level,query); //10396DA8..10396DFD
    if(!Call(query,query.slotZero,level)) { //10396E00..10396E9F
        Call(query,query.assignOutError,std::u16string_view{u"Couldn't spawn Player Actor: No LevelInfo"});
        Call(query,query.logError,std::u16string_view{u"Login failed, no LevelInfo"});
        return {OriginalSpawnPlayActorLoginOutcome::NoLevelInfo,0u};
    }
    const auto checked=Call(query,query.slotZero,level); //10396EA2..10396EC7
    if(!checked||!Call(query,query.isLevelInfo,checked))
        throw std::runtime_error("Original SpawnPlayActor assertion: Actors(0)->IsA(ALevelInfo)");
    const auto info=Call(query,query.slotZero,level);
    if(!info)throw std::runtime_error("Original SpawnPlayActor slot zero vanished before Game read");
    if(!Call(query,query.levelInfoGame,info)) { //10396ECA..10396F52
        Call(query,query.assignOutError,std::u16string_view{u"Couldn't spawn Player Actor: No Game"});
        Call(query,query.logError,std::u16string_view{u"No GameInfo"});
        return {OriginalSpawnPlayActorLoginOutcome::NoGame,0u};
    }
    const auto portal=CString(url.portal,query); //10396FBE..10397024
    SpawnPlayActorLoginDetail::CheckLevelInfo(level,query); //10397027..1039706C
    const auto finalInfo=Call(query,query.slotZero,level);
    if(!finalInfo)throw std::runtime_error("Original SpawnPlayActor slot zero vanished before Login");
    const auto game=Call(query,query.levelInfoGame,finalInfo); //1039706F..1039707A
    if(!game)throw std::runtime_error("Original SpawnPlayActor Game vanished before Login");
    // Retail frame+00 Portal FString,+0C Options FString,+18 OUT Error FString,
    // +24 SpawnClass,+28 ReturnValue, then ProcessEvent at103970ED.
    return {OriginalSpawnPlayActorLoginOutcome::LoginReturned,
        Call(query,query.invokeLogin,game,portal,std::u16string_view{options.data(),size},spawnClass)};
}

} // namespace QuestVr
