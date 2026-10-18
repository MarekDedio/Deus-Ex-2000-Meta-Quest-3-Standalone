#pragma once

#include <cstddef>
#include <limits>
#include <stdexcept>

namespace QuestVr {
// A host's validator includes its CURRENT ledger plus the requested extra.
// Keep a caller's temporary reservation visible to every nested admission,
// then restore it without allocation even when a provider throws. Scopes are
// lexical/LIFO and the owning ledger must outlive them; no saved state exists.
class NativeScratchScope {
    std::size_t& ledger_;
    std::size_t previous_;
public:
    template<class Validate>
    NativeScratchScope(std::size_t& ledger,std::size_t bytes,Validate&& validate):ledger_(ledger),previous_(ledger) {
        if(bytes>std::numeric_limits<std::size_t>::max()-previous_)
            throw std::runtime_error("Native temporary scratch reservation overflow");
        validate(bytes); // The old ledger is still authoritative here.
        ledger_=previous_+bytes;
    }
    ~NativeScratchScope() noexcept {ledger_=previous_;}
    NativeScratchScope(const NativeScratchScope&)=delete;
    NativeScratchScope& operator=(const NativeScratchScope&)=delete;
    NativeScratchScope(NativeScratchScope&&)=delete;
    NativeScratchScope& operator=(NativeScratchScope&&)=delete;
};
} // namespace QuestVr
