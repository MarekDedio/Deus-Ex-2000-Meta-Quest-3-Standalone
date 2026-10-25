// Actual query replies, reflected HP changes and births, not notify metadata.
const char* FireStageName(PortableStartupWeaponFireStage stage) {
    switch(stage) {
    case PortableStartupWeaponFireStage::SetLocation:return "SetLocation";
    case PortableStartupWeaponFireStage::ClientSetRotation:return "ClientSetRotation";
    case PortableStartupWeaponFireStage::Fire:return "Fire";
    case PortableStartupWeaponFireStage::AnimationThenState:return "AnimationThenState";
    }
    throw std::runtime_error("Unknown Training Fire observation stage");
}
template<class T> void FireTriple(const std::array<T,3u>& value) {
    std::cout<<'('<<value[0u]<<','<<value[1u]<<','<<value[2u]<<')';
}
void FireCrate(const char* stage,const PortableStartupWeaponFireCrateSnapshot& row) {
    std::cout<<"WEAPON FIRE CRATE stage="<<stage<<" actor="<<std::quoted(row.actorPath)
        <<" class="<<std::quoted(row.classPath)<<" state="<<std::quoted(row.statePath)
        <<" base="<<std::quoted(row.basePath)<<" contents="<<std::quoted(row.contentsPath)
        <<" hp="<<row.hitPoints<<" physics="<<row.physics<<" removed="<<row.removed
        <<" deleteMe="<<row.deleteMe<<" location=";FireTriple(row.location);std::cout<<'\n';
}
void ObserveTrainingWeaponFire(const PortableWorldStartupProbeResult& world,
    const std::vector<PortableStartupWeaponFireObservation>& rows,
    const std::optional<PortableStartupWeaponCommand>& equip,
    const std::optional<PortableStartupWeaponFireCommand>& command) {
    if(!command) {Require(rows.empty(),"Unrequested Fire diagnostic observations");return;}
    Require(equip.has_value(),"Fire observation lacks its actual equip command");
    const auto capacity=static_cast<std::size_t>(command->attacks)*(command->frames+1u)+(command->pose?2u:0u);
    Require(rows.size()<=capacity&&capacity<=262u,"Fire phase capacity exceeded");
    std::size_t returnedNotifies{},completeCycles{},attack{},frame{},cycleStartNotifies{},traces{},changedCrates{},births{};
    bool nextFire=true;
    for(std::size_t index=0u;index<rows.size();++index) {
        const auto& row=rows[index];const bool pose=command->pose&&index<2u;
        const auto expected=pose?(index?PortableStartupWeaponFireStage::ClientSetRotation:PortableStartupWeaponFireStage::SetLocation):
            nextFire?PortableStartupWeaponFireStage::Fire:PortableStartupWeaponFireStage::AnimationThenState;
        const bool fire=expected==PortableStartupWeaponFireStage::Fire;
        const bool animation=expected==PortableStartupWeaponFireStage::AnimationThenState;
        Require(row.commandIndex==index&&row.attackIndex==(pose?0u:attack)&&row.stage==expected&&
            row.actorPath==equip->actorPath&&row.playerPath=="00_Training.JCDentonMale0"&&
            row.gameInfoPath=="00_Training.DeusExGameInfo0","Fire identity/stage/attack order changed");
        Require(row.entered&&row.writesBefore<=row.writesAfter&&row.writesAfter<=world.execution.writes&&
            (!row.captured||row.returned)&&(!row.partialAfterCaptured||row.failed)&&
            (!row.failed||(!world.execution.passed()&&index+1u==rows.size())),"Fire completion/write prefix inconsistent");
        Require(row.frameIndex==(animation?frame:0u),"Fire elapsed-frame index changed");
        if(animation)Require(row.deltaSeconds==command->deltaSeconds,"Attack elapsed input changed");
        else {
            const char* method=fire?"DeusEx.DeusExPlayer.Fire":
                expected==PortableStartupWeaponFireStage::SetLocation?"Engine.Actor.SetLocation":"Engine.Pawn.ClientSetRotation";
            Require(row.functionPath==method&&!row.functionSource.empty()&&!row.animationReturned&&!row.stateDisposition,
                "Selected original input method identity changed");
        }
        if(fire)cycleStartNotifies=returnedNotifies;
        if(row.animationReturned)returnedNotifies+=row.notifyEvents;
        else Require(!row.animationIterations&&!row.animationFloatWrites&&!row.animationFinishedWrites&&
            !row.notifyEvents&&!row.animEndEvents&&!row.missingHandlers&&!row.animationIterationCap,
            "Unreturned animation phase fabricated returned statistics");
        Require(row.totalReturnedNotifyEvents==returnedNotifies,"Cumulative returned notify count differs");
        Require(row.playerBefore.actorPath==row.playerPath&&row.playerBefore.playerPath.empty()&&
            row.weaponBefore.actorPath==row.actorPath,"Fire before snapshot lost genuine unpaired identities");
        if(fire)Require(row.playerBefore.inHandPath==row.actorPath&&row.playerBefore.weaponPath==row.actorPath&&
            !row.playerBefore.inHandTransition&&row.weaponBefore.ownerPath==row.playerPath&&
            row.weaponBefore.state.selectedPath=="DeusEx.DeusExWeapon.Idle"&&
            row.weaponBefore.weaponUp&&row.weaponBefore.readyToFire,"Fire entered before original equipped Idle");
        std::cout<<"WEAPON FIRE index="<<index<<" attack="<<row.attackIndex<<" stage="<<FireStageName(row.stage)
            <<" frame="<<row.frameIndex<<" function="<<std::quoted(row.functionPath)
            <<" entered="<<row.entered<<" returned="<<row.returned<<" captured="<<row.captured<<" failed="<<row.failed
            <<" partialAfter="<<row.partialAfterCaptured<<" writes="<<row.writesBefore<<".."<<row.writesAfter
            <<" owned="<<row.ownedAfter<<" cycleComplete="<<row.attackCycleComplete<<" placementAccepted="<<row.placementAccepted
            <<" animationReturned="<<row.animationReturned<<" returnedNotifies="<<row.notifyEvents
            <<" totalReturnedNotifies="<<row.totalReturnedNotifyEvents<<'\n';
        std::cout<<"WEAPON FIRE POSE before location=";FireTriple(row.locationBefore);
        std::cout<<" rotation=";FireTriple(row.rotationBefore);std::cout<<" view=";FireTriple(row.viewRotationBefore);
        std::cout<<" eyeHeight="<<row.baseEyeHeightBefore<<'\n';
        PrintWeaponPlayer("fire-before",row.playerBefore);PrintWeaponItem("fire-before",row.weaponBefore);
        for(const auto& crate:row.cratesBefore){Require(!crate.actorPath.empty(),"Missing actual before-crate snapshot");FireCrate("before",crate);}
        if(row.poseCaptured) {
            std::cout<<"WEAPON FIRE POSE after location=";FireTriple(row.locationAfter);
            std::cout<<" rotation=";FireTriple(row.rotationAfter);std::cout<<" view=";FireTriple(row.viewRotationAfter);
            std::cout<<" eyeHeight="<<row.baseEyeHeightAfter<<'\n';
        }
        if(row.captured||row.partialAfterCaptured) {
            Require(row.poseCaptured&&row.playerAfter.actorPath==row.playerPath&&row.playerAfter.playerPath.empty()&&
                row.weaponAfter.actorPath==row.actorPath,"After snapshot lost original pose/identity");
            const bool owned=row.weaponAfter.ownerPath==row.playerPath&&!row.weaponAfter.removed&&!row.weaponAfter.deleteMe&&
                std::any_of(row.playerAfter.inventory.begin(),row.playerAfter.inventory.end(),[&](const auto& link){
                    return link.actorPath==row.actorPath&&link.ownerPath==row.playerPath&&!link.removed&&!link.deleteMe;});
            Require(row.ownedAfter==owned,"Observed owned flag differs from actual inventory graph");
            PrintWeaponPlayer("fire-after",row.playerAfter);PrintWeaponItem("fire-after",row.weaponAfter);
            for(std::size_t c=0u;c<row.cratesAfter.size();++c) {
                Require(row.cratesBefore[c].actorPath==row.cratesAfter[c].actorPath,"After-crate identity changed");
                FireCrate("after",row.cratesAfter[c]);
                if(row.cratesBefore[c].hitPoints!=row.cratesAfter[c].hitPoints)++changedCrates;
            }
            for(const auto& born:row.births) {
                Require(!born.actorPath.empty()&&!born.classPath.empty(),"Birth lacks an actual runtime identity");++births;
                std::cout<<"WEAPON FIRE BIRTH actor="<<std::quoted(born.actorPath)<<" class="<<std::quoted(born.classPath)
                    <<" state="<<std::quoted(born.statePath)<<" owner="<<std::quoted(born.ownerPath)<<" base="<<std::quoted(born.basePath)
                    <<" physics="<<born.physics<<" removed="<<born.removed<<" deleteMe="<<born.deleteMe<<" inventory="<<born.inventory
                    <<" location=";FireTriple(born.location);std::cout<<'\n';
            }
        }
        for(const auto& trace:row.traces) {
            Require(++traces<=64u&&!trace.sourcePath.empty()&&trace.writesBeforeOutputs>=row.writesBefore&&
                trace.writesAfterOutputs>=trace.writesBeforeOutputs&&trace.writesAfterOutputs<=world.execution.writes,
                "Actual trace observation/write prefix inconsistent");
            if(row.captured&&!row.failed)Require(trace.outputsComplete,"Successful phase retained an incomplete trace");
            std::cout<<"WEAPON FIRE TRACE source="<<std::quoted(trace.sourcePath)<<" hit="<<std::quoted(trace.actorPath)
                <<" actors="<<trace.traceActors<<" outputsComplete="<<trace.outputsComplete<<" writes="
                <<trace.writesBeforeOutputs<<".."<<trace.writesAfterOutputs<<" start=";FireTriple(trace.start);
            std::cout<<" end=";FireTriple(trace.end);std::cout<<" hitLocation=";FireTriple(trace.hitLocation);
            std::cout<<" normal=";FireTriple(trace.hitNormal);std::cout<<'\n';
        }
        if(!row.captured)continue;
        const auto& sequence=row.weaponAfter.sequence;
        if(sequence=="Attack"||sequence=="Attack2"||sequence=="Attack3")
            Require(row.authoredNotifies.size()==1u&&row.authoredNotifies.front().function=="HandToHandAttack"&&
                row.authoredNotifies.front().time==0.5f,"Attack lost its actual original notify metadata");
        for(const auto& notify:row.authoredNotifies)
            Require(std::isfinite(notify.time)&&notify.time>=0.0f&&notify.time<=1.0f,"Invalid authored notify time");
        Require(row.resultKind==(expected==PortableStartupWeaponFireStage::SetLocation?Kind::Bool:Kind::Nothing),
            "Original action result kind changed");
        if(expected==PortableStartupWeaponFireStage::SetLocation) {
            if(index+1u<rows.size())Require(row.placementAccepted,"Continued after original SetLocation refusal");
        } else Require(!row.placementAccepted,"Nonplacement stage fabricated accepted placement");
        if(expected==PortableStartupWeaponFireStage::ClientSetRotation)
            Require(row.viewRotationAfter==command->pose->viewRotation,"ClientSetRotation did not retain supplied view");
        if(fire) {
            Require(row.attackSelected&&(sequence=="Attack"||sequence=="Attack2"||sequence=="Attack3"),"Original Fire did not select attack");
            nextFire=false;frame=0u;
        } else if(animation)++frame;
        if(row.attackCycleComplete) {
            Require(animation&&!row.failed&&row.animationReturned&&returnedNotifies>cycleStartNotifies&&row.ownedAfter&&
                row.playerAfter.weaponPath==row.actorPath&&row.weaponAfter.weaponUp&&row.weaponAfter.readyToFire&&
                row.weaponAfter.state.selectedPath=="DeusEx.DeusExWeapon.Idle","Cycle lacks returned notify/original owned Idle");
            ++completeCycles;++attack;frame=0u;nextFire=true;
        }
    }
    if(world.execution.error=="Unsupported runtime native 1463" ||
        world.execution.error=="Native preferred-size virtual override is not implemented" ||
        world.execution.error=="Bound PlayerPawn ConsoleCommand native FExec is not implemented")
        Require(completeCycles==command->attacks&&!rows.empty()&&rows.back().attackCycleComplete,
            "Ordinary startup reached without all requested attacks completing");
    std::cout<<"WEAPON FIRE OBSERVATIONS rows="<<rows.size()<<" cycles="<<completeCycles<<" returnedNotifies="<<returnedNotifies
        <<" actualTraces="<<traces<<" crateChanges="<<changedCrates<<" births="<<births
        <<"; original same-root diagnostic only, NOT live controller, published gameplay or Quest acceptance\n";
}
