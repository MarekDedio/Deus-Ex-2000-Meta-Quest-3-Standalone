#include "actor_visibility_state.h"

#include <iostream>
#include <stdexcept>

namespace {
std::size_t checks{};
void Require(bool value,const char* message) {
    if (!value) throw std::runtime_error(message);
    ++checks;
}
void OwnershipAndStaging() {
    QuestVr::ActorRemovalState removals;
    QuestVr::ActorVisibilityState active,staged;
    for (std::size_t slot=0u;slot<512u;++slot) {
        const auto path="Map.Actor"+std::to_string(slot);
        Require(active.Add(path,removals)==slot && staged.Add(path,removals)==slot,"Actor mask ordinal changed");
        Require(QuestVr::ActorVisibilityState::Matrix(slot)*16u+
            QuestVr::ActorVisibilityState::Row(slot)*4u+
            QuestVr::ActorVisibilityState::Column(slot)==slot,"Matrix scalar mapping changed");
    }
    removals.Remove("MAP.ACTOR16");
    Require(active.Hide("map.actor16") && staged.Hide("Map.Actor16"),"Removal did not reach both banks");
    Require(!active.Visible(16u) && !staged.Visible(16u) && active.Visible(15u) && active.Visible(17u),
        "Actor removal hid a neighboring actor");
    Require(!active.Hide("Map.Unknown") && !active.Slot("Map.Unknown"),"Unknown actor mutated a mask");
    QuestVr::ActorVisibilityState later;
    Require(later.Add("map.actor16",removals)==0u && !later.Visible(0u),"New stage resurrected a removal");
    removals.Reset();
    QuestVr::ActorVisibilityState restored;
    Require(restored.Add("Map.Actor16",removals)==0u && restored.Visible(0u) && !active.Visible(16u),
        "Authoritative revival flashed into an older visible bank");
    bool refused{};
    try { active.Add("Map.Excess",removals); } catch (const std::exception&) { refused=true; }
    Require(refused && active.Visible(511u),"Actor-mask limit damaged existing ownership");
    refused=false;
    try { restored.Add("MAP.ACTOR16",removals); } catch (const std::exception&) { refused=true; }
    Require(refused && restored.Visible(0u),"Duplicate alias escaped actor ownership");
}
void RefreshAndCollisionCoverage() {
    using Kind=QuestVr::ActorRefreshKind;
    Require(!QuestVr::ActorRefreshBlocksGameplay(true,true,Kind::Spatial),"Retained spatial refresh froze gameplay");
    Require(QuestVr::ActorRefreshBlocksGameplay(false,true,Kind::Spatial),"Initial partial bank allowed gameplay");
    Require(QuestVr::ActorRefreshBlocksGameplay(true,true,Kind::Authoritative),"Unknown authoritative mutation allowed stale gameplay");
    Require(!QuestVr::ActorRefreshBlocksGameplay(true,false,Kind::Authoritative),"Complete idle bank blocked gameplay");
    Require(QuestVr::RetainedActorCoverageContains(true,0,0,20,0) &&
        !QuestVr::RetainedActorCoverageContains(true,0,0,20.01f,0),"Retained collision coverage lost its boundary");
    Require(QuestVr::RetainedActorCoverageContains(false,0,0,1000,1000),"Complete unfiltered mover set was spatially blocked");
}
}

void RetainedMoverPoses() {
    struct Pose { std::string objectPath; bool mover{}; float position{}; bool moving{}; };
    QuestVr::RetainedMoverPoseSnapshots<Pose> poses;
    poses.Replace({{"Map.Door",true,0,false},{"Map.UnseenDoor",true,10,false},{"Map.Pickup",false,30,false}});
    Require(poses.Entries().size()==2u && !poses.Find("Map.Pickup"),"Cache included non-movers");
    poses.Observe({"MAP.DOOR",true,5,true});
    Require(poses.Find("map.door")->position==5 && poses.Find("Map.UnseenDoor")->position==10,
        "Moving-door delta replaced an untouched stationary mover");
    poses.Observe({"Map.Door",true,8,false});
    Require(poses.Find("MAP.DOOR")->position==8 && !poses.Find("Map.Door")->moving,
        "Final stationary pose was lost before spatial rebase");
    const auto stagedPose = *poses.Find("Map.Door");
    poses.Observe({"Map.Door",true,6,true});
    Require(stagedPose.position==8 && poses.Find("Map.Door")->position==6,
        "Live motion did not remain independent of an older staged snapshot");
    bool refused{};
    try { poses.Replace({{"Map.Door",true,1,false},{"MAP.DOOR",true,2,false}}); }
    catch (const std::exception&) { refused=true; }
    Require(refused && poses.Find("Map.Door")->position==6 && poses.Entries().size()==2u,
        "Failed authoritative pose replacement damaged retained state");
    refused=false;
    try { poses.Observe({"Map.Pickup",false,0,false}); }
    catch (const std::exception&) { refused=true; }
    Require(refused && !poses.Find("Map.Pickup"),"Non-mover delta entered the cache");
    poses.Replace({{"Map.Door",true,0,false}});
    Require(poses.Find("Map.Door")->position==0 && !poses.Find("Map.UnseenDoor"),
        "Authoritative save restoration retained stale mover identities/poses");
    poses.Clear();
    Require(poses.Entries().empty() && !poses.Find("Map.Door"),"Map teardown retained a pose");
}

int main() {
    try {
        OwnershipAndStaging(); RefreshAndCollisionCoverage(); RetainedMoverPoses();
        std::cout<<"PASS: "<<checks<<" actor visibility ownership/refresh controls; synthetic CPU mapping only, not GPU or headset acceptance.\n";
        return 0;
    } catch (const std::exception& error) { std::cerr<<"FAIL: "<<error.what()<<'\n'; return 1; }
}
