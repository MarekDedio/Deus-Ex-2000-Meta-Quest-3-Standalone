#pragma once

#include <stdexcept>

namespace QuestVr {

enum class NativeWindowRelativeQuery {TopChild,BottomChild,HigherSibling,LowerSibling};
enum class NativeWindowRelativeLink {FirstChild,LastChild,PrevSibling,NextSibling};

// Original Extension.dll1004c650/1004c680/1004c6b0/1004c740. Top/bottom
// start at the actual last/first child; lower/higher follow prev/next sibling.
// Filtering inspects only each candidate's LOCAL visibility bit, never parents,
// sensitivity, initialization or layout. The wrapper's TRUE default is supplied
// by the caller, not inferred here from the declaration's false Bool zero.
//
// Visit validates the actual receiver/candidate and spends the enclosing root's
// shared work before any reads. ReadLink and LocalVisible remain live providers;
// their failures propagate. In particular a cyclic hidden chain must exhaust
// Visit's budget, not fabricate None or hit an arbitrary local traversal cap.
// This helper allocates nothing, writes nothing and supplies no rollback root.
template<class Node,class Operations>
Node* QueryNativeWindowRelative(Node* receiver,NativeWindowRelativeQuery query,
    bool visibleOnly,Operations& operations) {
    NativeWindowRelativeLink head,step;
    switch(query) {
    case NativeWindowRelativeQuery::TopChild:
        head=NativeWindowRelativeLink::LastChild;step=NativeWindowRelativeLink::PrevSibling;break;
    case NativeWindowRelativeQuery::BottomChild:
        head=NativeWindowRelativeLink::FirstChild;step=NativeWindowRelativeLink::NextSibling;break;
    case NativeWindowRelativeQuery::HigherSibling:
        head=NativeWindowRelativeLink::NextSibling;step=NativeWindowRelativeLink::NextSibling;break;
    case NativeWindowRelativeQuery::LowerSibling:
        head=NativeWindowRelativeLink::PrevSibling;step=NativeWindowRelativeLink::PrevSibling;break;
    default:
        throw std::invalid_argument("Unknown native Window relative query");
    }
    operations.Visit(receiver);
    Node* candidate=operations.ReadLink(receiver,head);
    while(candidate) {
        operations.Visit(candidate);
        if(!visibleOnly || operations.LocalVisible(candidate)) return candidate;
        candidate=operations.ReadLink(candidate,step);
    }
    return nullptr;
}

} // namespace QuestVr
