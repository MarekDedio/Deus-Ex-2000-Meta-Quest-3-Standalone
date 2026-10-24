// Included AFTER the pickup/equip CLI observer in the same anonymous namespace.
// Returned animation counters and authored metadata are separate from actual
// trace/hit evidence. A thrown notify callback never fabricates a returned count.
void ObserveTrainingWeaponFire(const PortableWorldStartupProbeResult& world,
    const std::vector<PortableStartupWeaponFireObservation>& rows,
    const std::optional<PortableStartupWeaponCommand>& equip,
    const std::optional<PortableStartupWeaponFireCommand>& command) {
    if(!command) {
        Require(rows.empty(),"Normal/equip-only startup ran unrequested Fire diagnostics");return;
    }
    Require(equip.has_value(),"Fire observation lacks the requested original equip command");
    Require(rows.size()<=1u+command->frames,"Training Fire observations exceed bounded phases");
    std::size_t returnedNotifies{},completeCycles{};
    for(std::size_t index=0u;index<rows.size();++index) {
        const auto& row=rows[index];const bool fire=index==0u;
        Require(row.commandIndex==index&&row.stage==(fire?PortableStartupWeaponFireStage::Fire:
            PortableStartupWeaponFireStage::AnimationThenState)&&row.actorPath==equip->actorPath&&
            row.playerPath=="00_Training.JCDentonMale0"&&row.gameInfoPath=="00_Training.DeusExGameInfo0",
            "Training Fire row changed original target/player/GameInfo or stage order");
        Require(row.entered&&row.writesBefore<=row.writesAfter&&row.writesAfter<=world.execution.writes&&
            (!row.captured||row.returned)&&(!row.partialAfterCaptured||row.failed)&&
            (!row.failed||(!world.execution.passed()&&index+1u==rows.size())),
            "Training Fire completion/failure or shared write-prefix evidence is inconsistent");
        if(fire)Require(row.functionPath=="DeusEx.DeusExPlayer.Fire"&&!row.functionSource.empty()&&
            !row.animationReturned&&!row.stateDisposition,"Fire did not retain its real selected original method identity");
        else Require(row.frameIndex==index-1u&&row.deltaSeconds==command->deltaSeconds,
            "Training attack phase changed its explicit elapsed input");
        if(row.animationReturned)returnedNotifies+=row.notifyEvents;
        else Require(!row.animationIterations&&!row.animationFloatWrites&&!row.animationFinishedWrites&&
            !row.notifyEvents&&!row.animEndEvents&&!row.missingHandlers&&!row.animationIterationCap,
            "Failed/unentered animation phase fabricated returned statistics");
        Require(row.totalReturnedNotifyEvents==returnedNotifies,"Attack notify ledger differs from returned real phases");
        Require(row.playerBefore.actorPath==row.playerPath&&row.playerBefore.playerPath.empty()&&
            row.weaponBefore.actorPath==row.actorPath,"Fire before snapshot lost the original unpaired player/weapon");
        if(fire)Require(row.playerBefore.inHandPath==row.actorPath&&row.playerBefore.weaponPath==row.actorPath&&
            !row.playerBefore.inHandTransition&&row.weaponBefore.ownerPath==row.playerPath&&
            row.weaponBefore.state.selectedPath=="DeusEx.DeusExWeapon.Idle"&&
            row.weaponBefore.weaponUp&&row.weaponBefore.readyToFire,
            "Selected Fire was entered before genuine equipped Idle readiness");
        std::cout<<"WEAPON FIRE index="<<index<<" stage="<<(fire?"Fire":"AnimationThenState")
            <<" frame="<<row.frameIndex<<" function="<<std::quoted(row.functionPath)
            <<" source="<<std::quoted(row.functionSource)<<" entered="<<row.entered
            <<" returned="<<row.returned<<" captured="<<row.captured<<" failed="<<row.failed
            <<" partialAfter="<<row.partialAfterCaptured<<" writes="<<row.writesBefore<<".."<<row.writesAfter
            <<" owned="<<row.ownedAfter<<" attackSelected="<<row.attackSelected
            <<" cycleComplete="<<row.attackCycleComplete<<" dt="<<row.deltaSeconds
            <<" animationReturned="<<row.animationReturned<<" animationIterations="<<row.animationIterations
            <<" floatWrites="<<row.animationFloatWrites<<" finishedWrites="<<row.animationFinishedWrites
            <<" returnedNotifies="<<row.notifyEvents<<" totalReturnedNotifies="<<returnedNotifies
            <<" animEnds="<<row.animEndEvents<<" missingHandlers="<<row.missingHandlers
            <<" suppressAnimEnd="<<row.suppressAnimEnd<<" capped="<<row.animationIterationCap;
        if(row.stateDisposition)std::cout<<" childPhase="<<WeaponPhaseName(*row.stateDisposition);
        std::cout<<'\n';PrintWeaponPlayer("fire-before",row.playerBefore);PrintWeaponItem("fire-before",row.weaponBefore);
        if(row.captured||row.partialAfterCaptured) {
            Require(row.playerAfter.actorPath==row.playerPath&&row.playerAfter.playerPath.empty()&&
                row.weaponAfter.actorPath==row.actorPath,"Fire after snapshot changed actual player/weapon identity");
            const auto member=std::find_if(row.playerAfter.inventory.begin(),row.playerAfter.inventory.end(),
                [&](const auto& item){return item.actorPath==row.actorPath;});
            const bool owned=member!=row.playerAfter.inventory.end()&&member->ownerPath==row.playerPath&&
                !member->removed&&!member->deleteMe&&row.weaponAfter.ownerPath==row.playerPath&&
                !row.weaponAfter.removed&&!row.weaponAfter.deleteMe;
            Require(row.ownedAfter==owned,"Fire ownership flag differs from actual Owner/inventory graph");
            if(row.captured&&index+1u<rows.size())Require(owned&&row.playerAfter.weaponPath==row.actorPath,
                "Attack continued without retaining its genuinely owned weapon");
            PrintWeaponPlayer("fire-after",row.playerAfter);PrintWeaponItem("fire-after",row.weaponAfter);
        }
        if(!row.captured)continue;
        Require(row.resultKind==Kind::Nothing,"Original player Fire/state action invented a returned value");
        const auto& sequence=row.weaponAfter.sequence;
        const bool attack=sequence=="Attack"||sequence=="Attack2"||sequence=="Attack3";
        if(fire)Require(row.attackSelected==attack,"Attack-selected flag differs from the original chosen animation");
        if(attack)Require(row.authoredNotifies.size()==1u&&row.authoredNotifies.front().function=="HandToHandAttack"&&
            row.authoredNotifies.front().time==0.5f,
            "Actual Training weapon attack lacks its authored HandToHandAttack0.5 metadata");
        for(const auto& notify:row.authoredNotifies) {
            Require(std::isfinite(notify.time)&&notify.time>=0.0f&&notify.time<=1.0f,
                "Attack notify metadata has invalid authored time");
            std::cout<<"WEAPON FIRE AUTHORED NOTIFY sequence="<<std::quoted(sequence)
                <<" function="<<std::quoted(notify.function)<<" time="<<notify.time
                <<" serializedIndex="<<notify.serializedIndex<<"; metadata only, NOT dispatch or hit proof\n";
        }
        if(row.attackCycleComplete) {
            Require(!row.failed&&row.animationReturned&&returnedNotifies>0u&&row.ownedAfter&&
                row.playerAfter.weaponPath==row.actorPath&&row.weaponAfter.weaponUp&&row.weaponAfter.readyToFire&&
                row.weaponAfter.state.selectedPath=="DeusEx.DeusExWeapon.Idle",
                "Attack-cycle completion lacks a returned notify and genuine owned Idle readiness");
            ++completeCycles;
        }
    }
    if(world.execution.error=="Unsupported runtime native 1463")
        Require(completeCycles==1u&&!rows.empty()&&rows.back().attackCycleComplete,
            "Later ordinary startup reached without completing the requested original attack diagnostic");
    std::cout<<"WEAPON FIRE OBSERVATIONS rows="<<rows.size()<<" cycles="<<completeCycles
        <<" returnedNotifies="<<returnedNotifies
        <<"; same-root original attack evidence only, NOT controller aim, hit/TakeDamage, committed combat or Quest acceptance\n";
}
