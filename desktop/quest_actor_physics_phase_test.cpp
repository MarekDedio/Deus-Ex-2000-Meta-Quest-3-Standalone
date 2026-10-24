#include "quest_actor_physics_phase.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <stdexcept>

namespace {
std::size_t checks{};
void Require(bool value,const char* text) {
    if(!value)throw std::logic_error(text);
    ++checks;
}

// Independent transcription of the audited native branch destinations. Use
// the comparisons in their original order rather than the helper's combined
// Boolean expression. This models just the reached non-Pawn physics suffix,
// not earlier callback/tick-type eligibility or a world scheduler.
bool NativeBranchOracle(unsigned remoteRole,unsigned role,unsigned physics) {
    if(remoteRole==3u)goto remoteRoleTail;
    if(role>=2u)goto ordinaryRole;
    if(physics==2u)goto performPhysics;
    goto nonPawnTail;
ordinaryRole:
    if(physics==0u)goto nonPawnTail;
    if(role==3u)goto nonPawnTail;
performPhysics:
    return true;
remoteRoleTail:
nonPawnTail:
    return false;
}

void AllNativeByteTriples() {
    std::size_t eligible{};
    for(unsigned remoteRole=0u;remoteRole<256u;++remoteRole) {
        for(unsigned role=0u;role<256u;++role) {
            for(unsigned physics=0u;physics<256u;++physics) {
                const bool actual=QuestVr::OriginalNonPawnPhysicsPhaseEligible(
                    static_cast<std::uint8_t>(remoteRole),static_cast<std::uint8_t>(role),
                    static_cast<std::uint8_t>(physics));
                Require(actual==NativeBranchOracle(remoteRole,role,physics),
                    "Physics suffix differs from original unsigned native branches");
                if(actual)++eligible;
            }
        }
    }
    // 255 non-excluded RemoteRoles; each has two low roles with one active
    // Physics byte and 253 other non-autonomous roles with 255 active bytes.
    Require(eligible==255u*(2u+253u*255u),
        "Exhaustive physics suffix admission count lost a native byte path");
}

void AuthoredModesAndBoundaryBytes() {
    constexpr std::array<std::uint8_t,7u> remoteRoles{0u,1u,2u,3u,4u,128u,255u};
    constexpr std::array<std::uint8_t,8u> roles{0u,1u,2u,3u,4u,5u,128u,255u};
    constexpr std::array<std::uint8_t,8u> modes{0u,1u,2u,3u,4u,11u,128u,255u};
    for(const auto remoteRole:remoteRoles) {
        for(const auto role:roles) {
            for(const auto physics:modes) {
                const bool actual=QuestVr::OriginalNonPawnPhysicsPhaseEligible(remoteRole,role,physics);
                if(remoteRole==3u)Require(!actual,"RemoteRole3 reached ordinary physics, including Authority");
                else if(role==0u||role==1u)Require(actual==(physics==2u),"Low native Role ran a non-falling mode");
                else if(role==3u)Require(!actual,"Autonomous Role reached ordinary non-Pawn physics");
                else Require(actual==(physics!=0u),"Non-autonomous simulated/authority byte lost a nonzero mode");
            }
        }
    }
    Require(QuestVr::OriginalNonPawnPhysicsPhaseEligible(0u,4u,2u),
        "Standalone Authority falling was incorrectly excluded");
    Require(!QuestVr::OriginalNonPawnPhysicsPhaseEligible(0u,4u,0u),
        "Stopped debris incorrectly reentered the whole performPhysics epilogue");
    Require(!QuestVr::OriginalNonPawnPhysicsPhaseEligible(3u,4u,2u),
        "RemoteRole3 Authority state/timer branch incorrectly entered physics");
}
} // namespace

static_assert(QuestVr::OriginalNonPawnPhysicsPhaseEligible(0u,4u,2u));
static_assert(!QuestVr::OriginalNonPawnPhysicsPhaseEligible(0u,4u,0u));
static_assert(noexcept(QuestVr::OriginalNonPawnPhysicsPhaseEligible(0u,4u,2u)));

int main() {
    try {
        AllNativeByteTriples();AuthoredModesAndBoundaryBytes();
        std::cout<<"Original non-Pawn physics phase: "<<checks<<" checks\n";
        return 0;
    }catch(const std::exception& error) {
        std::cerr<<"Original non-Pawn physics phase test failed: "<<error.what()<<'\n';return 1;
    }
}
