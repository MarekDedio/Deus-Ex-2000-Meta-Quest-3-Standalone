#pragma once

namespace QuestVr {

// Original XWindow::SetChildVisibility (Extension1004ee20). Effective state is
// captured on each side of the local store; callbacks may subsequently mutate
// the tree, but must not change the captured value sent down this invocation.
template<class Operations>
void SetNativeWindowChildVisibility(Operations& operations,bool visible) {
    if(operations.ReadLocalVisibility()==visible) return;
    const bool before=operations.ReadEffectiveVisibility();
    operations.WriteLocalVisibility(visible);
    const bool after=operations.ReadEffectiveVisibility();
    if(before==after) return;
    operations.CheckFocus();
    operations.CheckGrab();
    operations.MarkNeedsReconfigure();
    operations.RecomputeInitTree(after);
    operations.InvokeVisibilityTree(after);
    operations.ReconfigureTree();
    operations.DirtyModalAccelerators();
}

} // namespace QuestVr
