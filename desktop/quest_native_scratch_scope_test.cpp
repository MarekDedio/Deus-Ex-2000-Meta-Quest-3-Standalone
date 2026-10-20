#include "quest_native_scratch_scope.h"
#include "quest_model_line_scratch.h"

#include <iostream>
#include <new>
#include <optional>

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
    // Model/world work precedes allocation of the separate actor hit arena.
    // Each full peak fits; retaining both inactive peaks incorrectly refuses.
    {
        QuestVr::NativeScratchScope collector(ledger,10u,validate);
        std::optional<QuestVr::NativeScratchScope> actor;
        Require(ledger==10u,"Fixed collector did not precede both providers");
        {
            QuestVr::NativeScratchScope world(ledger,50u,validate);
            Require(ledger==60u,"World query was charged for an inactive actor arena");
            Refuse([&]{QuestVr::NativeScratchScope inactive(ledger,50u,validate);});
        }
        Require(ledger==10u,"World workspace survived into the actor query");
        actor.emplace(ledger,40u,validate);
        {
            QuestVr::NativeScratchScope primitive(ledger,10u,validate);
            Require(ledger==60u,"Actor primitive lost its caller's full arena envelope");
        }
        Require(ledger==50u,"Actor arena did not survive primitive/collector processing");
    }
    Require(!ledger,"Sequential provider storage did not unwind in LIFO order");
    try {
        QuestVr::NativeScratchScope collector(ledger,10u,validate);
        std::optional<QuestVr::NativeScratchScope> actor;
        actor.emplace(ledger,40u,validate);
        QuestVr::NativeScratchScope primitive(ledger,10u,validate);
        throw std::runtime_error("late actor primitive refusal");
    }catch(const std::runtime_error&) {}
    Require(!ledger,"Late sequential-provider refusal retained an arena reservation");
    // Independent 64-bit supported-host layouts: point DFS16B/frame and
    // pending36B/segment retain conservative vector growth. Extent uses
    // fixed packed4B frames/colors and a fixed12B/visit array of N+1.
    static_assert(sizeof(std::size_t)==8u);
    for(const std::size_t nodes:{0u,1u,64u,9524u,1'000'000u}) {
        Require(QuestVr::OriginalModelLineScratchBytes(nodes,true)==2048u+nodes*108u,
            "Point query scratch lost validation/traversal growth envelope");
        Require(QuestVr::OriginalModelLineScratchBytes(nodes,false)==2048u+(nodes+1u)*12u,
            "Extent query scratch incorrectly charges inactive point storage");
    }
    base=16'000'000u;
    const auto memory=[&](std::size_t extra) {
        if(extra>16'777'216u-base-ledger)throw std::runtime_error("Fixed retained cap exceeded");
    };
    Refuse([&]{QuestVr::NativeScratchScope old(ledger,QuestVr::OriginalModelLineScratchBytes(9524u,true),memory);});
    {
        QuestVr::NativeScratchScope actual(ledger,QuestVr::OriginalModelLineScratchBytes(9524u,false),memory);
        Require(ledger==116'348u,"Extent branch admission does not match its actual conservative peak");
    }
    Require(!ledger,"Model scratch scope survived its query");
    for(bool point:{false,true})Refuse([&]{
        static_cast<void>(QuestVr::OriginalModelLineScratchBytes(std::numeric_limits<std::size_t>::max(),point));
    });
    Require(QuestVr::ModelLineScratchDetail::ExtentPendingCapacity(9524u,1'000'000u)==9525u,
        "Extent traversal capacity does not bound the actual maximum DFS frontier");
    Require(QuestVr::ModelLineScratchDetail::ExtentPendingCapacity(9524u,100u)==100u,
        "Extent traversal ignored a tighter pending limit");
    Refuse([&]{static_cast<void>(QuestVr::ModelLineScratchDetail::ExtentPendingCapacity(
        std::numeric_limits<std::size_t>::max(),1u));});
}
}
int main() {
    try {Run();std::cout<<"PASS composed native scratch scopes "<<checks<<" checks / "<<refusals<<" refusals.\n";return 0;}
    catch(const std::exception& error) {std::cerr<<"Native scratch scopes failed: "<<error.what()<<'\n';return 1;}
}
