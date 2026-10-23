// Detached actual native timestep observations. These are not acceptance of a
// world scheduler, an equipped weapon path or committed/on-headset gameplay.
void ObserveTrainingFalling(const PortableWorldStartupProbeResult& world,bool requested,bool afterLowerBreak=false) {
    const auto& rows=world.fallingObservations;
    Require(!afterLowerBreak||requested,"Post-destruction falling requires the initial gravity commands");
    if(!requested){Require(rows.empty(),"Normal startup manufactured falling commands");return;}
    const std::size_t requestedCount=afterLowerBreak?3u:2u;
    Require(rows.size()<=8u*requestedCount,"Training falling exceeded its requested eight-step command bounds");
    std::size_t captured{},landed{},moves{},previousWrites{};
    std::array<const PortableStartupFallingObservation*,3u> first{},last{};
    const PortableStartupFallingObservation* previous{};
    for(std::size_t i=0u;i<rows.size();++i) {
        const auto& row=rows[i];
        Require(row.commandIndex<requestedCount&&row.stepIndex<8u&&row.deltaSeconds==0.1f&&row.afterDamage==(row.commandIndex==2u)&&
            row.actorPath==std::string("00_Training.CrateBreakableMedGeneral")+std::to_string(row.commandIndex==0u?1u:2u)&&
            row.classPath=="DeusEx.CrateBreakableMedGeneral"&&row.hasLandingCounter&&
            row.gameInfoPath==world.gameInfoPath&&row.entered&&(!row.returned||row.entered)&&(!row.captured||row.returned)&&
            row.writesBefore>=previousWrites&&row.writesBefore<=row.writesAfter&&row.writesAfter<=world.execution.writes,
            "Training falling lost actual crate/root/timestep/write-prefix identity");
        Require(CanonicalIdentity(row.actorPath)&&CanonicalIdentity(row.gameInfoPath)&&
            CanonicalIdentity(row.zoneBefore)&&CanonicalIdentity(row.zoneAfter,!row.captured)&&
            CanonicalIdentity(row.baseBefore,true)&&CanonicalIdentity(row.baseAfter,true)&&
            CanonicalIdentity(row.floorPath,true)&&CanonicalIdentity(row.selectedLanded,true),
            "Training falling contains invalid/unbounded canonical identities");
        if(previous && row.commandIndex==previous->commandIndex) {
            Require(previous->captured&&previous->physicsAfter==2u&&!previous->removed&&!previous->deleteMe&&
                row.stepIndex==previous->stepIndex+1u&&row.locationBefore==previous->locationAfter&&
                row.velocityBefore==previous->velocityAfter&&row.accelerationBefore==previous->accelerationAfter&&
                row.oldLocationBefore==previous->oldLocationAfter&&row.baseBefore==previous->baseAfter&&
                row.zoneBefore==previous->zoneAfter&&row.clockBefore==previous->clockAfter&&
                row.teleportedBefore==previous->teleportedAfter&&row.writesBefore==previous->writesAfter,
                "Training falling lost continuity between genuine timesteps");
        }else {
            Require(row.stepIndex==0u&&row.commandIndex==(previous?previous->commandIndex+1u:0u)&&
                row.baseBefore.empty()&&row.velocityBefore==std::array<float,3u>{}&&
                row.accelerationBefore==std::array<float,3u>{},
                "Training falling changed the original first-step profile or skipped a command");
            if(previous)Require(previous->captured&&(previous->physicsAfter!=2u||previous->stepIndex==7u||
                previous->removed||previous->deleteMe),"Training falling skipped remaining live Falling steps");
            first[row.commandIndex]=&row;
            if(row.commandIndex==2u) {
                Require(last[0u]&&last[1u]&&last[0u]->captured&&last[1u]->captured&&
                    last[0u]->landingEntered&&!last[0u]->recovery&&!last[0u]->zoneBounce&&last[0u]->physicsAfter==0u&&
                    last[1u]->landingEntered&&!last[1u]->recovery&&!last[1u]->zoneBounce&&last[1u]->physicsAfter==0u&&
                    last[1u]->baseAfter==TrainingDamageActors[0u]&&
                    world.damageObservations.size()==2u&&world.damageObservations[0u].captured&&world.damageObservations[1u].captured,
                    "Post-destruction falling lacks completed original initial support and lower-crate damage");
                const auto& nonfatal=world.damageObservations[0u];const auto& fatal=world.damageObservations[1u];
                Require(nonfatal.actorPath==TrainingDamageActors[0u]&&nonfatal.physicsBefore==0u&&
                    nonfatal.baseBefore==last[0u]->baseAfter&&nonfatal.writesBefore>=last[1u]->writesAfter&&
                    fatal.actorPath==TrainingDamageActors[0u]&&fatal.removed&&fatal.deleteMe&&fatal.hitPointsAfter==-2&&
                    row.writesBefore>=fatal.writesAfter&&row.physicsBefore==2u&&row.baseBefore.empty()&&
                    row.locationBefore==last[1u]->locationAfter&&row.oldLocationBefore==last[1u]->oldLocationAfter&&
                    row.velocityBefore==last[1u]->velocityAfter&&row.accelerationBefore==last[1u]->accelerationAfter&&
                    row.zoneBefore==last[1u]->zoneAfter&&!row.teleportedBefore,
                    "Post-destruction upper crate did not genuinely lose its destroyed support while preserving its settled pose");
            }
        }
        Require(row.physicsBefore==2u&&row.counterBefore==0,
            "Training falling did not enter the actual original Falling crate");
        if(row.captured) {
            Require(!row.removed&&!row.deleteMe&&row.substeps>0u&&row.substeps<=8u&&row.moveCalls>0u&&
                row.moveCalls==row.substeps&&row.oldLocationWords==3u*row.substeps&&
                row.teleportWrites==row.substeps&&!row.teleportedAfter&&row.apexSplits==0u&&
                row.locationAfter[0u]==row.locationBefore[0u]&&row.locationAfter[1u]==row.locationBefore[1u]&&
                row.locationAfter[2u]<=row.locationBefore[2u]&&row.clockAfter==row.clockBefore,
                "Training ordinary falling lost native motion/store order or changed the unrelated animation clock");
            if(row.landingEntered) {
                Require(row.hitTime>=0.0f&&row.hitTime<1.0f&&row.normal[2u]>0.7&&
                    row.selectedLanded=="DeusEx.DeusExDecoration.Landed"&&!row.floorPath.empty(),
                    "Training falling manufactured floor contact or selected Landed");
                if(!row.recovery&&!row.zoneBounce) {
                    Require(row.landedCalls==1u&&row.setPhysicsCalls==1u&&row.counterAfter==0&&row.physicsAfter==0u&&
                        row.baseAfter==row.floorPath&&row.velocityAfter==std::array<float,3u>{},
                        "Training falling did not stop and establish original floor support through SetPhysics");
                    ++landed;
                }
            }else Require(row.physicsAfter==2u&&row.baseAfter.empty()&&row.velocityAfter[2u]<0.0f,
                "Training ordinary free fall did not retain actual downward Falling motion");
            moves+=row.moveCalls;++captured;
        }else Require(i+1u==rows.size()&&!world.execution.passed()&&!world.execution.committed,
            "Training continued after an incomplete falling timestep");
        std::cout<<std::setprecision(9)<<"TRAINING FALLING command="<<row.commandIndex<<" step="<<row.stepIndex
            <<" actor="<<std::quoted(row.actorPath)<<" class="<<std::quoted(row.classPath)
            <<" hasLandingCounter="<<row.hasLandingCounter<<" dt="<<row.deltaSeconds<<" afterDamage="<<row.afterDamage<<" entered="<<row.entered
            <<" returned="<<row.returned<<" captured="<<row.captured<<" moved="<<row.moved
            <<" substeps="<<row.substeps<<" moves="<<row.moveCalls<<" apexSplits="<<row.apexSplits
            <<" zBefore="<<row.locationBefore[2u]<<" zAfter="<<row.locationAfter[2u]
            <<" vzBefore="<<row.velocityBefore[2u]<<" vzAfter="<<row.velocityAfter[2u]
            <<" physicsBefore="<<unsigned(row.physicsBefore)<<" physicsAfter="<<unsigned(row.physicsAfter)
            <<" baseAfter="<<std::quoted(row.baseAfter)<<" floor="<<std::quoted(row.floorPath)
            <<" hitTime="<<row.hitTime<<" normalZ="<<row.normal[2u]<<" landed="<<row.landingEntered
            <<" selected="<<std::quoted(row.selectedLanded)<<" oldLocationWords="<<row.oldLocationWords
            <<" velocityWords="<<row.velocityWords<<" accelerationWords="<<row.accelerationWords
            <<" teleportWrites="<<row.teleportWrites<<" teleportedBefore="<<row.teleportedBefore
            <<" teleportedAfter="<<row.teleportedAfter<<" traces="<<row.traceCalls
            <<" landedCalls="<<row.landedCalls<<" setterCalls="<<row.setPhysicsCalls
            <<" recovery="<<row.recovery<<" zoneBounce="<<row.zoneBounce
            <<" writesBefore="<<row.writesBefore<<" writesAfter="<<row.writesAfter<<'\n';
        previousWrites=row.writesAfter;previous=&row;last[row.commandIndex]=&row;
    }
    if(ReachedTrainingPossess(world)) {
        for(std::size_t command=0u;command<requestedCount;++command)
            Require(first[command]&&last[command]&&last[command]->captured&&last[command]->landingEntered&&
                !last[command]->recovery&&!last[command]->zoneBounce&&last[command]->physicsAfter==0u&&
                !last[command]->removed&&!last[command]->deleteMe,
                "Original Possess was reached without every requested Training gravity command settling on real support");
        if(afterLowerBreak)Require(last[2u]->locationAfter[2u]<first[2u]->locationBefore[2u]&&
            last[2u]->baseAfter==last[2u]->floorPath&&!last[2u]->floorPath.empty()&&last[2u]->floorPath!=TrainingDamageActors[0u],
            "Released upper crate did not actually fall and settle on a surviving real collision support");
    }else if(!previous || !previous->captured || previous->commandIndex+1u!=requestedCount ||
        (previous->physicsAfter==2u&&previous->stepIndex!=7u&&!previous->removed&&!previous->deleteMe))
        Require(!world.execution.passed()&&!world.execution.committed,
            "Training startup skipped requested falling timesteps and fabricated success");
    std::cout<<"TRAINING FALLING OBSERVATIONS rows="<<rows.size()<<" captured="<<captured<<" moves="<<moves
        <<" ordinaryLanded="<<landed
        <<"; explicit original physFalling timesteps only, NOT a world scheduler or published headset gameplay.\n";
}
