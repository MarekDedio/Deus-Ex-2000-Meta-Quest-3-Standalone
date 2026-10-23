#pragma once

#include <cstddef>
#include <cstdint>

namespace QuestVr {
namespace NativeWindowToggleDetail {
// The adapter shares this depth with reentrant native calls in the SAME VM/root
// transaction. EnterToggle may refuse the port's explicit native stack limit;
// LeaveToggle must be noexcept, unmetered and restore the depth on exceptions.
// Neither operation changes original Window or RadioBox native state.
template<class Operations>
class Scope {
public:
    explicit Scope(Operations& operations):operations_(operations) {
        operations_.Step();operations_.EnterToggle();
    }
    ~Scope() noexcept {operations_.LeaveToggle();}
    Scope(const Scope&)=delete;
    Scope& operator=(const Scope&)=delete;
private:
    Operations& operations_;
};
} // namespace NativeWindowToggleDetail

template<class Operations>
void SetNativeWindowToggle(Operations& operations,typename Operations::Window source,std::int32_t value);

// The adapter must select the actual native virtual before IsRadioBox returns
// true, refusing unaudited native overrides rather than silently bypassing them.
// ScriptToggleChanged uses most-derived/state-aware same-VM event dispatch with
// its original initialized-false result. A Radio's script event is NOT another
// native virtual call (which would recursively re-enter its native override).
template<class Operations>
bool DispatchNativeWindowToggleChanged(Operations& operations,
    typename Operations::Window receiver,typename Operations::Window source,bool value) {
    operations.Step();
    if(operations.IsRadioBox(receiver)) {
        operations.Step();
        if(operations.IsToggle(source)) {
            if(value) {
                // Original native registered list, not the reflected Window
                // children. Reads stay live; no frozen membership copy or
                // synthetic registration of an otherwise absent source.
                for(std::size_t index=0u;;++index) {
                    operations.Step();
                    if(index>=operations.ToggleButtonCount(receiver)) break;
                    operations.Step();
                    if(operations.ToggleButtonAt(receiver,index)!=source) continue;
                    operations.Step();
                    const auto old=operations.ReadSelection(receiver);
                    if(old==source) return true;
                    operations.Step();operations.WriteSelection(receiver,source);
                    // Publish the replacement BEFORE old-selection callbacks.
                    if(old!=typename Operations::Window{}) SetNativeWindowToggle(operations,old,false);
                    break;
                }
            } else {
                operations.Step();
                if(operations.ReadOneCheck(receiver)) {
                    operations.Step();
                    if(operations.ReadSelection(receiver)==source) SetNativeWindowToggle(operations,source,true);
                    return true;
                }
                operations.Step();
                if(operations.ReadSelection(receiver)!=source) return true;
                operations.Step();operations.WriteSelection(receiver,typename Operations::Window{});
            }
        }
    }
    // Use the incoming native callback Bool here. Only ChangeToggle rereads the
    // original source bit before dispatch to its NEXT receiver.
    operations.Step();return operations.ScriptToggleChanged(receiver,source,value);
}

// Extension.dll10049910: source first, then each receiver's LIVE parent after
// an unhandled callback, with the source's CURRENT pressed bit before each
// receiver. No visited-set/ancestor snapshot is legitimate across callbacks.
// Every loop iteration spends shared native work, including parent cycles.
template<class Operations>
void ChangeNativeWindowToggle(Operations& operations,typename Operations::Window source) {
    auto receiver=source;
    while(receiver!=typename Operations::Window{}) {
        operations.Step();const bool value=operations.ReadPressed(source);
        if(DispatchNativeWindowToggleChanged(operations,receiver,source,value)) return;
        operations.Step();receiver=operations.ReadParent(receiver);
    }
}

// Extension.dll100499B0. Equality compares the current canonical bit with the
// RAW incoming native INT before replacement masks bit0. Thus incoming2 can
// refresh an already-unpressed toggle and incoming-1 an already-pressed one.
// Typed script Bool ingress normally supplies0/1, with the native thunk's
// audited initialized-false seed when argument evaluation has no result.
// The adapter journals only the bButtonPressed bit, preserving packed siblings,
// and performs original appearance stores text -> texture -> tile. No sound.
// All effects/callbacks belong to the caller's transaction; this coordinator
// deliberately does not create a new VM/root, restart budgets or catch failure.
template<class Operations>
void SetNativeWindowToggle(Operations& operations,typename Operations::Window source,std::int32_t value) {
    NativeWindowToggleDetail::Scope<Operations> scope(operations);
    operations.Step();
    if(static_cast<std::int32_t>(operations.ReadPressed(source))==value) return;
    operations.Step();operations.WritePressed(source,(static_cast<std::uint32_t>(value)&1u)!=0u);
    operations.Step();operations.ChangeButtonAppearance(source);
    ChangeNativeWindowToggle(operations,source);
}
} // namespace QuestVr
