#pragma once

#include <cstddef>
#include <stdexcept>

namespace QuestVr::NativeWork {

// Portable safety policy, not a recovered original-engine counter. Ordinary
// roots retain the old allowance. Larger finite roots require explicit opt-in;
// this ceiling bounds even hostile configuration and subtraction arithmetic.
inline constexpr std::size_t DefaultLimit=65'536u;
inline constexpr std::size_t MaximumLimit=16'777'216u;

struct Usage {
    std::size_t used{},limit{DefaultLimit};
    bool exhausted{};
    bool operator==(const Usage&) const = default;
};

class Exhausted final : public std::runtime_error {
public:
    Exhausted(const char* message,std::size_t used,std::size_t limit,std::size_t requested)
        :std::runtime_error(message),used_(used),limit_(limit),requested_(requested){}
    std::size_t Used()const noexcept{return used_;}
    std::size_t Limit()const noexcept{return limit_;}
    std::size_t Requested()const noexcept{return requested_;}
private:
    std::size_t used_,limit_,requested_;
};

inline void ValidateLimit(std::size_t limit){
    if(limit>MaximumLimit)throw std::invalid_argument("Native work limit exceeds the portable maximum");
}

class Budget final {
public:
    Budget()=default;
    explicit Budget(std::size_t limit){Reset(limit);}
    // Owner-only outer-root boundary. No child/native/iterator gets reset
    // authority through its borrowed AI work view.
    void Reset(std::size_t limit){ValidateLimit(limit);usage_={0u,limit,false};}
    void Step(std::size_t count=1u){
        if(usage_.exhausted || usage_.used>usage_.limit || count>usage_.limit-usage_.used){
            usage_.exhausted=true;
            throw Exhausted("Native actor lifecycle work budget exceeded",usage_.used,usage_.limit,count);
        }
        usage_.used+=count;
    }
    void Check()const{
        if(usage_.exhausted)throw Exhausted("Native actor lifecycle work budget exceeded",usage_.used,usage_.limit,0u);
    }
    Usage Read()const noexcept{return usage_;}
private:
    Usage usage_;
};

} // namespace QuestVr::NativeWork
