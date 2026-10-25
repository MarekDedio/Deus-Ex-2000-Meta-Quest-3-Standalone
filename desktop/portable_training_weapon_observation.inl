// Included in the first-Training observation CLI's anonymous namespace.
// Reports actual same-root pickup/equip observations; never grants inventory,
// supplies a weapon state, or turns a later startup refusal into acceptance.
const char* WeaponStageName(PortableStartupWeaponStage stage) {
    switch(stage) {
    case PortableStartupWeaponStage::Pickup:return "Pickup";
    case PortableStartupWeaponStage::PutInHand:return "PutInHand";
    case PortableStartupWeaponStage::UpdateInHand:return "UpdateInHand";
    case PortableStartupWeaponStage::AnimationThenState:return "AnimationThenState";
    }
    throw std::runtime_error("Unknown Training weapon observation stage");
}
const char* WeaponPhaseName(QuestVr::Vm::StatePhaseStatus phase) {
    switch(phase) {
    case QuestVr::Vm::StatePhaseStatus::Returned:return "Returned";
    case QuestVr::Vm::StatePhaseStatus::Stopped:return "Stopped";
    case QuestVr::Vm::StatePhaseStatus::Waiting:return "Waiting";
    }
    throw std::runtime_error("Unknown Training weapon child-state disposition");
}
void PrintWeaponState(const PortableStartupWeaponStateSnapshot& state) {
    std::cout<<" selected="<<std::quoted(state.selectedPath)<<" code="<<std::quoted(state.codePath)
        <<" locals="<<std::quoted(state.localsCodePath)<<" pc="<<state.statementIndex
        <<" object="<<state.hasStateObject<<" stack="<<state.hasStack<<" override="<<state.frameOverride
        <<" frame="<<state.hasFrame<<" control="<<state.hasControl<<" codePresent="<<state.codePresent
        <<" probeMask="<<state.liveProbeMask<<" localsRevision="<<state.localsRevision;
    if(state.portableLatent)std::cout<<" portableLatent="<<static_cast<unsigned>(*state.portableLatent);
    else std::cout<<" portableLatent=absent";
    std::cout<<" serializedStack="<<state.hasSerializedStack;
    if(state.hasSerializedStack) {
        std::cout<<" rawFunction="<<state.rawFunctionReference<<" rawState="<<state.rawStateReference
            <<" rawProbeMask="<<state.rawProbeMask<<" rawNativeLatent="<<state.rawLatentAction;
        if(state.rawLogicalOffset)std::cout<<" rawCode="<<*state.rawLogicalOffset;
        else std::cout<<" rawCode=None";
    }
    if(state.latentTimeLeft)std::cout<<" actorTimer="<<*state.latentTimeLeft;
    else std::cout<<" actorTimer=absent";
}
void PrintWeaponPlayer(const char* when,const PortableStartupWeaponPlayerSnapshot& player) {
    std::cout<<"WEAPON PLAYER "<<when<<" actor="<<std::quoted(player.actorPath)
        <<" class="<<std::quoted(player.classPath)<<" paired="<<std::quoted(player.playerPath)
        <<" target="<<std::quoted(player.frobTargetPath)<<" inHand="<<std::quoted(player.inHandPath)
        <<" pending="<<std::quoted(player.inHandPendingPath)
        <<" clientPending="<<std::quoted(player.clientInHandPendingPath)
        <<" selectedItem="<<std::quoted(player.selectedItemPath)<<" weapon="<<std::quoted(player.weaponPath)
        <<" pendingWeapon="<<std::quoted(player.pendingWeaponPath)<<" transition="<<player.inHandTransition
        <<" role="<<player.role<<" health="<<player.health<<" isPawn="<<player.isPawn
        <<" isPlayer="<<player.isPlayer<<" inventoryCount="<<player.inventory.size()
        <<" grid="<<player.gridRows<<'x'<<player.gridColumns<<" cells=";
    for(const auto cell:player.grid)std::cout<<static_cast<unsigned>(cell)<<',';
    PrintWeaponState(player.state);std::cout<<'\n';
    for(const auto& item:player.inventory)
        std::cout<<"WEAPON INVENTORY "<<when<<" actor="<<std::quoted(item.actorPath)
            <<" class="<<std::quoted(item.classPath)<<" owner="<<std::quoted(item.ownerPath)
            <<" next="<<std::quoted(item.inventoryPath)<<" state="<<std::quoted(item.statePath)
            <<" birth="<<item.runtimeBirth<<" removed="<<item.removed<<" deleteMe="<<item.deleteMe<<'\n';
}
void PrintWeaponItem(const char* when,const PortableStartupWeaponItemSnapshot& item) {
    std::cout<<"WEAPON ITEM "<<when<<" actor="<<std::quoted(item.actorPath)
        <<" class="<<std::quoted(item.classPath)<<" owner="<<std::quoted(item.ownerPath)
        <<" next="<<std::quoted(item.inventoryPath)<<" instigator="<<std::quoted(item.instigatorPath)
        <<" mesh="<<std::quoted(item.meshPath)<<" playerViewMesh="<<std::quoted(item.playerViewMeshPath)
        <<" ammo="<<std::quoted(item.ammoTypePath)<<" sequence="<<std::quoted(item.sequence)
        <<" frame="<<item.frame<<" rate="<<item.rate<<" tween="<<item.tweenRate<<" last="<<item.last
        <<" drawScale="<<item.drawScale<<" physics="<<item.physics<<" group="<<item.inventoryGroup
        <<" grid="<<item.invPosX<<','<<item.invPosY<<'/'<<item.invSlotsX<<'x'<<item.invSlotsY
        <<" removed="<<item.removed<<" deleteMe="<<item.deleteMe<<" held="<<item.heldItem
        <<" hidden="<<item.hidden<<" carried="<<item.carriedItem<<" ownerOnly="<<item.onlyOwnerSee
        <<" loop="<<item.animLoop<<" finished="<<item.animFinished
        <<" weaponUp="<<item.weaponUp<<" readyToFire="<<item.readyToFire;
    PrintWeaponState(item.state);std::cout<<'\n';
}
void ObserveTrainingWeapon(const PortableWorldStartupProbeResult& world,
    const std::optional<PortableStartupWeaponCommand>& command) {
    const auto& rows=world.weaponObservations;
    if(!command) {
        Require(rows.empty(),"Normal startup ran an unrequested weapon diagnostic");return;
    }
    Require(rows.size()<=4u+command->frames,"Training weapon observations exceed the bounded command");
    const std::string expectedClass=command->actorPath=="00_Training.WeaponCrowbar1"?
        "DeusEx.WeaponCrowbar":"DeusEx.WeaponCombatKnife";
    std::size_t completed{};
    for(std::size_t index=0u;index<rows.size();++index) {
        const auto& row=rows[index];
        const bool finalUpdate=index>=3u&&index+1u==rows.size()&&row.stage==PortableStartupWeaponStage::UpdateInHand;
        const auto stage=index==0u?PortableStartupWeaponStage::Pickup:
            index==1u?PortableStartupWeaponStage::PutInHand:
            index==2u||finalUpdate?PortableStartupWeaponStage::UpdateInHand:PortableStartupWeaponStage::AnimationThenState;
        Require(row.stage==stage&&row.commandIndex==index&&row.actorPath==command->actorPath&&
            row.playerPath=="00_Training.JCDentonMale0"&&row.gameInfoPath=="00_Training.DeusExGameInfo0",
            "Training weapon row lost its original command/player/GameInfo/order");
        Require(row.writesBefore<=row.writesAfter&&row.writesAfter<=world.execution.writes&&
            (!row.captured||row.returned)&&(!row.returned||row.entered),
            "Training weapon row has invalid write bounds or completion flags");
        if(stage==PortableStartupWeaponStage::AnimationThenState)
            Require(row.frameIndex==index-3u&&row.deltaSeconds==command->deltaSeconds,
            "Training weapon animation/state phase changed its explicit step input");
        std::cout<<"WEAPON STAGE index="<<index<<" stage="<<WeaponStageName(row.stage)
            <<" frame="<<row.frameIndex<<" function="<<std::quoted(row.functionPath)
            <<" source="<<std::quoted(row.functionSource)<<" entered="<<row.entered
            <<" returned="<<row.returned<<" captured="<<row.captured<<" writes="<<row.writesBefore
            <<".."<<row.writesAfter<<" owned="<<row.ownedAfter<<" equipped="<<row.equipComplete
            <<" dt="<<row.deltaSeconds<<" suppressAnimEnd="<<row.suppressAnimEnd
            <<" animationIterations="<<row.animationIterations<<" capped="<<row.animationIterationCap
            <<" floatWrites="<<row.animationFloatWrites<<" finishedWrites="<<row.animationFinishedWrites
            <<" notifies="<<row.notifyEvents<<" animEnds="<<row.animEndEvents;
        if(row.stateDisposition)std::cout<<" childPhase="<<WeaponPhaseName(*row.stateDisposition);
        std::cout<<'\n';
        if(row.entered) {
            Require(row.playerBefore.actorPath==row.playerPath&&row.playerBefore.playerPath.empty()&&
                row.weaponBefore.actorPath==row.actorPath&&row.weaponBefore.classPath==expectedClass,
                "Training weapon before snapshot lacks actual unpaired JC/authored item");
            PrintWeaponPlayer("before",row.playerBefore);PrintWeaponItem("before",row.weaponBefore);
        }
        if(!row.captured)continue;
        Require(row.resultKind==Kind::Nothing&&row.playerAfter.actorPath==row.playerPath&&
            row.playerAfter.playerPath.empty()&&row.weaponAfter.actorPath==row.actorPath&&
            row.weaponAfter.classPath==expectedClass,"Completed weapon action changed player/item identity or invented a result");
        const auto member=std::find_if(row.playerAfter.inventory.begin(),row.playerAfter.inventory.end(),
            [&](const auto& item){return item.actorPath==row.actorPath;});
        const bool owned=member!=row.playerAfter.inventory.end()&&member->ownerPath==row.playerPath&&
            !member->removed&&!member->deleteMe&&row.weaponAfter.ownerPath==row.playerPath&&
            !row.weaponAfter.removed&&!row.weaponAfter.deleteMe;
        Require(row.ownedAfter==owned,"Weapon ownership flag disagrees with actual inventory membership/Owner");
        if(!owned)Require(index==0u&&index+1u==rows.size()&&!world.execution.passed()&&
            world.execution.error=="Original Training pickup did not produce actual player ownership/inventory membership",
            "Equip continued without actual original pickup ownership");
        const bool complete=owned&&row.playerAfter.inHandPath==row.actorPath&&
            row.playerAfter.inHandPendingPath==row.actorPath&&row.playerAfter.selectedItemPath==row.actorPath&&
            row.playerAfter.weaponPath==row.actorPath&&row.playerAfter.pendingWeaponPath.empty()&&
            !row.playerAfter.inHandTransition&&row.weaponAfter.weaponUp&&row.weaponAfter.readyToFire&&
            row.weaponAfter.state.selectedPath=="DeusEx.DeusExWeapon.Idle";
        Require(row.equipComplete==complete,"Weapon completion flag disagrees with actual ownership/links/transition/readiness/state");
        if(row.equipComplete) {
            Require(row.playerAfter.weaponPath==row.actorPath&&row.playerAfter.inHandPath==row.actorPath&&
                row.playerAfter.selectedItemPath==row.actorPath&&
                row.weaponAfter.state.selectedPath=="DeusEx.DeusExWeapon.Idle"&&
                row.weaponAfter.state.hasStack&&row.weaponAfter.state.frameOverride&&
                row.weaponAfter.state.hasFrame&&row.weaponAfter.state.hasControl&&
                row.weaponAfter.meshPath==row.weaponAfter.playerViewMeshPath&&!row.weaponAfter.meshPath.empty(),
                "Weapon completion lacks original owned player links, first-person mesh and selected Idle state");
            ++completed;
        }
        PrintWeaponPlayer("after",row.playerAfter);PrintWeaponItem("after",row.weaponAfter);
    }
    if(world.execution.error=="Unsupported runtime native 1463" ||
        world.execution.error=="Native preferred-size virtual override is not implemented")
        Require(completed>=1u&&!rows.empty()&&rows.back().equipComplete,
            "Later HUD boundary was reached without completing the requested real weapon equip");
    std::cout<<"WEAPON OBSERVATIONS rows="<<rows.size()<<" completeSnapshots="<<completed
        <<"; same-root original pickup/equip evidence only, NOT committed ownership, Fire/trace/crate damage or Quest acceptance\n";
}
