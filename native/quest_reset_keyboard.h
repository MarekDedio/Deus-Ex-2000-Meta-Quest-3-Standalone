#pragma once

#include <cstdint>
#include <functional>
#include <stdexcept>
#include <utility>

namespace QuestVr {

using ResetKeyboardNativeId=std::uintptr_t;
enum class OriginalResetKeyboardOutcome {NoPlayer,NotViewport,NoInput,ResetConfigInvoked};

// Original native544, Engine103021B2 ->103B96D0..103B9756. Providers
// authenticate actual native PlayerPawn.Player / Viewport.Input / Input.Class
// identities. Missing providers are NOT evidence of a None pointer. The caller
// authenticates the original native544 declaration and already consumed its
// EndFunctionParms. All reached work/reset mutations join its EXISTING root.
struct OriginalResetKeyboardQueries {
    std::function<void()> requireOuterRoot;
    std::function<void()> work;
    std::function<ResetKeyboardNativeId(ResetKeyboardNativeId)> playerPawnPlayer;
    std::function<bool(ResetKeyboardNativeId)> isViewport;
    std::function<ResetKeyboardNativeId(ResetKeyboardNativeId)> viewportInput;
    std::function<ResetKeyboardNativeId(ResetKeyboardNativeId)> inputClass;
    // Actual UObject.ResetConfig(selectedInput.Class), not ResetInput,
    // Input.Init, a default-copy substitute or a fresh nested transaction.
    std::function<void(ResetKeyboardNativeId)> resetConfig;
};

namespace ResetKeyboardDetail {
template<class Function,class... Arguments>
inline auto Call(const OriginalResetKeyboardQueries& query,const Function& function,Arguments&&... arguments) {
    if(!function)throw std::invalid_argument("Original ResetKeyboard lacks a reached provider");
    if(!query.work)throw std::invalid_argument("Original ResetKeyboard requires borrowed root work");
    query.work();return function(std::forward<Arguments>(arguments)...);
}
} // namespace ResetKeyboardDetail

inline OriginalResetKeyboardOutcome RunOriginalResetKeyboard(ResetKeyboardNativeId playerPawn,
    const OriginalResetKeyboardQueries& query) {
    using ResetKeyboardDetail::Call;
    if(!playerPawn)throw std::invalid_argument("Original ResetKeyboard requires an actual PlayerPawn receiver");
    Call(query,query.requireOuterRoot);
    const auto player=Call(query,query.playerPawnPlayer,playerPawn); //103B9702
    if(!player)return OriginalResetKeyboardOutcome::NoPlayer; //103B9708..70A
    if(!Call(query,query.isViewport,player))return OriginalResetKeyboardOutcome::NotViewport; //103B970C..72C
    const auto input=Call(query,query.viewportInput,player); //103B9732
    if(!input)return OriginalResetKeyboardOutcome::NoInput; //103B9735..737
    const auto cls=Call(query,query.inputClass,input); //103B9739
    if(!cls)
        // The original immediately passes Class into ResetConfig's native
        // dereference. Refuse invalid storage instead of inventing a None gate.
        throw std::runtime_error("Original ResetKeyboard Input has no actual Class");
    Call(query,query.resetConfig,cls); //103B973C..743
    return OriginalResetKeyboardOutcome::ResetConfigInvoked;
}

} // namespace QuestVr
