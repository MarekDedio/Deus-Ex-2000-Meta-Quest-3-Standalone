// Explicit actor-major physFalling on all six genuine lower-crate fragments.
// This is not performPhysics, rotation, state Tick or interleaved world physics.
void ObserveTrainingFragments(const PortableWorldStartupProbeResult& world) {
    const auto& rows=world.fallingObservations;
    Require(rows.size()<=48u,"Fragment diagnostic exceeded six eight-step commands");
    std::array<const PortableStartupDamageBirth*,6u> births{};
    if(world.damageObservations.size()==2u&&world.damageObservations[1u].captured) {
        unsigned n{};
        for(const auto& birth:world.damageObservations[1u].births)if(birth.classPath=="DeusEx.WoodFragment") {
            Require(n<births.size()&&!birth.removed&&!birth.deleteMe&&!birth.pickup&&birth.physics==2,
                "Fragment command source is not a genuine surviving falling birth");
            births[n++]=&birth;
        }
        Require(n==births.size(),"Original lower crate did not leave six genuine fragments");
    }
    const PortableStartupFallingObservation* previous{};
    std::array<const PortableStartupFallingObservation*,6u> last{};
    std::size_t captured{},bounces{},stopped{},moves{};
    for(std::size_t i=0u;i<rows.size();++i) {
        const auto& row=rows[i];
        Require(row.commandIndex<6u&&births[row.commandIndex],"Fragment command has no captured original birth");
        const auto& birth=*births[row.commandIndex];
        Require(row.actorPath==birth.actorPath&&row.classPath==birth.classPath&&!row.hasLandingCounter&&row.afterDamage&&
            row.stepIndex<8u&&row.deltaSeconds==0.1f&&row.gameInfoPath==world.gameInfoPath&&row.physicsBefore==2u&&
            row.entered&&(!row.returned||row.entered)&&(!row.captured||row.returned)&&
            row.writesBefore<=row.writesAfter&&row.writesAfter<=world.execution.writes,
            "Fragment timestep lost actual birth/class/root/phase/write identity");
        if(previous&&previous->commandIndex==row.commandIndex) {
            Require(previous->captured&&previous->physicsAfter==2u&&!previous->removed&&!previous->deleteMe&&
                row.stepIndex==previous->stepIndex+1u&&row.locationBefore==previous->locationAfter&&
                row.velocityBefore==previous->velocityAfter&&row.accelerationBefore==previous->accelerationAfter&&
                row.oldLocationBefore==previous->oldLocationAfter&&row.rotationBefore==previous->rotationAfter&&
                row.rotationRateBefore==previous->rotationRateAfter&&row.stateBefore==previous->stateAfter&&
                row.zoneBefore==previous->zoneAfter&&row.baseBefore==previous->baseAfter&&row.clockBefore==previous->clockAfter&&
                row.bounceBefore==previous->bounceAfter&&row.teleportedBefore==previous->teleportedAfter&&
                row.writesBefore==previous->writesAfter,"Fragment timestep lost genuine same-actor continuity");
        }else {
            Require(row.commandIndex==(previous?previous->commandIndex+1u:0u)&&row.stepIndex==0u&&
                row.locationBefore==birth.location&&row.velocityBefore==birth.velocity&&row.baseBefore==birth.basePath&&
                row.writesBefore>=(previous?previous->writesAfter:world.damageObservations[1u].writesAfter),
                "Fragment command did not start from actual original launch state");
            if(previous)Require(previous->captured&&(previous->physicsAfter!=2u||previous->removed||previous->deleteMe||previous->stepIndex==7u),
                "Fragment diagnostic skipped requested live falling steps");
        }
        if(row.captured) {
            Require(row.substeps>0u&&row.substeps<=8u&&row.moveCalls==row.substeps&&
                row.oldLocationWords==3u*row.substeps&&row.teleportWrites==row.substeps&&row.counterWrites==0u,
                "Fragment timestep altered original loop/store/Decoration counter behavior");
            if(row.hitWallCalls)Require(row.selectedHitWall=="DeusEx.DeusExFragment.HitWall",
                "Fragment bounce did not select its original DeusEx override");
            else Require(row.selectedHitWall.empty(),"Fragment diagnostic fabricated a selected bounce");
            if(row.stoppedByBounce) {
                Require(row.hitWallCalls>0u&&row.physicsAfter==0u&&row.accelerationWords==0u&&!row.landingEntered,
                    "Original bounce stop lost fresh Physics0/early return semantics");
                ++stopped;
            }
            moves+=row.moveCalls;bounces+=row.hitWallCalls;++captured;
        }else Require(i+1u==rows.size()&&!ReachedTrainingPossess(world)&&!world.execution.committed,
            "Fragment diagnostic continued after an uncaptured original step");
        std::cout<<std::setprecision(9)<<"TRAINING FRAGMENT command="<<row.commandIndex<<" step="<<row.stepIndex
            <<" actor="<<std::quoted(row.actorPath)<<" entered="<<row.entered<<" returned="<<row.returned<<" captured="<<row.captured
            <<" zBefore="<<row.locationBefore[2u]<<" zAfter="<<row.locationAfter[2u]
            <<" physicsBefore="<<unsigned(row.physicsBefore)<<" physicsAfter="<<unsigned(row.physicsAfter)
            <<" bounceBefore="<<row.bounceBefore<<" bounceAfter="<<row.bounceAfter<<" hits="<<row.hitWallCalls
            <<" selected="<<std::quoted(row.selectedHitWall)<<" stateAfter="<<std::quoted(row.stateAfter)
            <<" stoppedByBounce="<<row.stoppedByBounce<<" moves="<<row.moveCalls<<" baseAfter="<<std::quoted(row.baseAfter)
            <<" writesBefore="<<row.writesBefore<<" writesAfter="<<row.writesAfter<<'\n';
        last[row.commandIndex]=&row;previous=&row;
    }
    if(ReachedTrainingPossess(world))for(const auto* row:last)
        Require(row&&row->captured&&(row->physicsAfter!=2u||row->removed||row->deleteMe||row->stepIndex==7u),
            "Original Possess reached without completing every requested fragment command");
    std::cout<<"TRAINING FRAGMENT OBSERVATIONS rows="<<rows.size()<<" captured="<<captured<<" moves="<<moves
        <<" hits="<<bounces<<" stopped="<<stopped
        <<"; explicit physFalling only, NOT rotation/PendingTouch/state Tick/interleaved scene or headset acceptance.\n";
}
