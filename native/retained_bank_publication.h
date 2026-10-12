#pragma once

#include <cstdint>
#include <limits>
#include <memory>
#include <stdexcept>

namespace QuestVr {

struct RetainedBankKey {
    std::uint64_t scene{},revision{},atlas{};
    friend bool operator==(const RetainedBankKey& a,const RetainedBankKey& b) {
        return a.scene==b.scene && a.revision==b.revision && a.atlas==b.atlas;
    }
};
struct RetainedBankToken {
    std::uint64_t serial{};
    RetainedBankKey key;
};

// One immutable visible generation and at most one bounded replacement.
// Bank owns its renderer storage and shared atlas references: moving the bank
// pointer never moves GL command/uniform addresses. Failure/cancellation must
// discard only staging; a map/session teardown explicitly clears both.
template <typename Bank>
class RetainedBankPublication {
public:
    RetainedBankToken Begin(std::unique_ptr<Bank> bank,RetainedBankKey key) {
        if (!bank || key.scene==0u || key.atlas==0u || serial_==std::numeric_limits<std::uint64_t>::max())
            throw std::runtime_error("Invalid/bounded retained bank publication");
        const RetainedBankToken token{serial_+1u,key};
        staged_=std::move(bank);
        token_=token;
        serial_=token.serial;
        ready_=false;
        return token;
    }
    Bank* Active() const { return active_.get(); }
    Bank* Staged() const { return staged_.get(); }
    bool Current(RetainedBankToken token) const {
        return staged_ && token.serial==token_.serial && token.key==token_.key;
    }
    bool MarkReady(RetainedBankToken token) {
        if (!Current(token)) return false;
        ready_=true;
        return true;
    }
    bool Commit(RetainedBankToken token,RetainedBankKey authoritative) {
        if (!ready_ || !Current(token) || !(token.key==authoritative)) return false;
        active_.swap(staged_);
        staged_.reset();
        ready_=false;
        return true;
    }
    void Cancel() { staged_.reset(); ready_=false; }
    void Clear() { Cancel(); active_.reset(); }
private:
    std::unique_ptr<Bank> active_,staged_;
    RetainedBankToken token_;
    std::uint64_t serial_{};
    bool ready_{};
};

} // namespace QuestVr
