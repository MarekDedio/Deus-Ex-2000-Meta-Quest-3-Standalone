#include "quest_native_scratch_scope.h"

#include <iostream>
#include <new>

namespace {
std::size_t checks{},refusals{};
void Require(bool ok,const char* message) {++checks;if(!ok)throw std::runtime_error(message);}
template<class F>void Refuse(F run) {
    bool refused{};try {run();}catch(const std::exception&) {refused=true;}
    Require(refused,"Unsupported composed native scratch admission succeeded");++refusals;
}
void Run() {
    std::size_t ledger{},base=40u,calls{};
    const auto validate=[&](std::size_t extra) {
        ++calls;
        if(ledger>100u-base || extra>100u-base-ledger)throw std::runtime_error("Combined peak exceeds fixed limit");
    };
    // Both separate preflights pass, but their overlapping live lifetimes do
    // not fit. The outer reservation MUST remain visible during child checks.
    validate(30u);validate(40u);
    {
        QuestVr::NativeScratchScope arena(ledger,30u,validate);
        Require(ledger==30u,"Caller scratch reservation was not retained");
        Refuse([&]{QuestVr::NativeScratchScope workspace(ledger,40u,validate);});
        Require(ledger==30u,"Refused workspace modified its caller's live reservation");
        {
            QuestVr::NativeScratchScope workspace(ledger,30u,validate);
            Require(ledger==60u,"Exact combined peak was not admitted");
            Refuse([&]{validate(1u);});
        }
        Require(ledger==30u,"Normal nested scratch release lost its caller's reservation");
        try {QuestVr::NativeScratchScope nested(ledger,20u,validate);throw std::runtime_error("late provider failure");}
        catch(const std::runtime_error&) {}
        Require(ledger==30u,"Exception scratch release lost its caller's reservation");
    }
    Require(ledger==0u,"Native temporary reservation survived its query scope");
    {
        QuestVr::NativeScratchScope first(ledger,0u,validate);
        QuestVr::NativeScratchScope second(ledger,60u,validate);
        Require(ledger==60u,"Zero-sized caller or exact-cap child broke composed admission");
    }
    Require(ledger==0u,"Zero-sized scope failed exact restoration");
    bool validated{};
    ledger=std::numeric_limits<std::size_t>::max()-1u;
    Refuse([&]{QuestVr::NativeScratchScope overflow(ledger,2u,[&](std::size_t){validated=true;});});
    Require(!validated&&ledger==std::numeric_limits<std::size_t>::max()-1u,"Reservation overflow called validator or wrapped the live ledger");
    ledger=0u;
    Refuse([&]{QuestVr::NativeScratchScope rejected(ledger,5u,[](std::size_t){throw std::bad_alloc();});});
    Require(ledger==0u,"Throwing admission callback published a native reservation");
    Require(calls>=8u,"Composed scratch controls skipped nested admissions");
}
}
int main() {
    try {Run();std::cout<<"PASS composed native scratch scopes "<<checks<<" checks / "<<refusals<<" refusals.\n";return 0;}
    catch(const std::exception& error) {std::cerr<<"Native scratch scopes failed: "<<error.what()<<'\n';return 1;}
}
