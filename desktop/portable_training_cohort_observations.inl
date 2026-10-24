// First-Training frame-major original physics SUFFIX observations. This does
// not assert Actor/Level Tick, state/lifespan/timer execution or headset play.
void ObserveTrainingPhysicsCohort(const PortableWorldStartupProbeResult& world,bool requested) {
    const auto& rows=world.physicsCohortObservations;
    if(!requested){Require(rows.empty(),"Normal startup manufactured an unrequested physics cohort");return;}
    constexpr std::size_t Members=8u,Frames=8u;
    constexpr const char* Upper="00_Training.CrateBreakableMedGeneral2";
    Require(rows.size()<=Members*Frames,"Training cohort exceeded its eight-member/eight-frame bound");
    std::array<const PortableStartupDamageBirth*,Members> births{};
    std::array<std::string,Members> paths{};paths[0u]=Upper;
    const PortableStartupDamageObservation* fatal{};
    if(world.damageObservations.size()==2u&&world.damageObservations[1u].captured) {
        fatal=&world.damageObservations[1u];
        Require(fatal->commandIndex==1u&&fatal->actorPath==TrainingDamageActors[0u]&&fatal->removed&&fatal->deleteMe&&
            fatal->hitPointsAfter==-2,"Cohort birth authority is not genuine original lower-crate destruction");
        std::size_t member=1u,fragments{},lockpicks{};
        for(const auto& birth:fatal->births)if(!birth.removed&&!birth.deleteMe) {
            Require(member<Members&&CanonicalIdentity(birth.actorPath)&&birth.actorPath.starts_with("00_Training.")&&
                std::find(paths.begin(),paths.begin()+static_cast<std::ptrdiff_t>(member),birth.actorPath)==paths.begin()+static_cast<std::ptrdiff_t>(member),
                "Cohort discarded/duplicated a genuine surviving Actor birth or exceeded all-birth membership");
            if(birth.classPath=="DeusEx.WoodFragment") {
                ++fragments;Require(!birth.pickup&&member<=6u,"Fragment membership changed original damage-birth order");
            }else {
                Require(birth.classPath=="DeusEx.Lockpick"&&birth.pickup&&member==7u,
                    "Cohort contains a fabricated/substituted original surviving birth");++lockpicks;
            }
            births[member]=&birth;paths[member++]=birth.actorPath;
        }
        Require(member==Members&&fragments==6u&&lockpicks==1u,
            "Cohort did not retain upper crate plus all six genuine fragments and actual Lockpick");
    }
    Require(rows.empty()||fatal,"Cohort ran without a fully captured original lower-crate fatal event");
    const PortableStartupFallingObservation* settledUpper{};
    for(const auto& step:world.fallingObservations)if(step.actorPath==Upper&&step.captured)settledUpper=&step;
    if(!rows.empty())Require(settledUpper&&settledUpper->physicsAfter==0u&&settledUpper->baseAfter==TrainingDamageActors[0u],
        "Cohort lacks genuine upper-crate stacking on the real lower support before destruction");
    std::optional<std::size_t> upperSlot;
    if(!rows.empty()) {
        const auto upperIdentity=ReadPortableRuntimeObjectOwnership(Upper);
        const auto& slots=world.afterActorOrder.actorIndices;
        const auto found=std::find(slots.begin(),slots.end(),upperIdentity.objectIndex);
        Require(world.afterActorOrder.available&&upperIdentity.objectIndex&&found!=slots.end(),
            "Cohort upper crate has no original startup-partition slot authority");
        upperSlot=static_cast<std::size_t>(found-slots.begin());
    }
    const auto stateIdentity=[&](const auto& state,std::size_t member) {
        Require(state.actorPath==paths[member]&&CanonicalIdentity(state.classPath)&&CanonicalIdentity(state.basePath,true)&&
            CanonicalIdentity(state.statePath,true)&&state.role<=4u&&state.remoteRole<=4u,
            "Cohort all-state snapshot changed canonical retained member/Actor-state identity");
        Require(state.classPath==(member?births[member]->classPath:"DeusEx.CrateBreakableMedGeneral"),
            "Cohort all-state snapshot substituted an actual member class");
        for(unsigned channel=0u;channel<3u;++channel)Require(std::isfinite(state.location[channel])&&std::isfinite(state.velocity[channel]),
            "Cohort snapshot has a nonfinite actual Location/Velocity");
    };
    const auto matchesPhysicsBefore=[](const auto& state,const auto& physics) {
        return state.actorPath==physics.actorPath&&state.classPath==physics.classPath&&state.location==physics.locationBefore&&
            state.velocity==physics.velocityBefore&&state.rotation==physics.rotationBefore&&state.basePath==physics.baseBefore&&
            state.statePath==physics.stateBefore&&state.physics==physics.physicsBefore;
    };
    const auto matchesPhysicsAfter=[](const auto& state,const auto& physics) {
        return state.location==physics.locationAfter&&state.velocity==physics.velocityAfter&&state.rotation==physics.rotationAfter&&
            state.basePath==physics.baseAfter&&state.statePath==physics.stateAfter&&state.physics==physics.physicsAfter&&
            state.removed==physics.removed&&state.deleteMe==physics.deleteMe;
    };
    const PortableStartupPhysicsCohortObservation* previous{};
    std::array<std::optional<std::size_t>,Members> memberSlots{};
    std::size_t captured{},eligible{},skipped{},sweeps{},rotations{},rotationMoves{},hits{},landings{},carries{},carryMoves{},postTouches{};
    std::size_t previousWrites=fatal?fatal->writesAfter:0u;
    for(std::size_t i=0u;i<rows.size();++i) {
        const auto& row=rows[i];const auto& step=row.physics;const auto member=step.commandIndex;
        Require(member<Members&&row.frameIndex<Frames&&step.stepIndex==row.frameIndex&&
            step.actorPath==paths[member]&&step.afterDamage&&step.performPhysics&&step.deltaSeconds==0.4f&&
            step.gameInfoPath==world.gameInfoPath&&row.before.size()==Members&&
            step.writesBefore>=previousWrites&&step.writesBefore<=step.writesAfter&&step.writesAfter<=world.execution.writes,
            "Cohort lost frame-major member/root/delta/all-state/write-prefix identity");
        if(memberSlots[member])Require(row.actorSlot==*memberSlots[member],"Cohort live Actor slot changed between frames");
        else memberSlots[member]=row.actorSlot;
        if(member==0u)Require(upperSlot&&row.actorSlot==*upperSlot,"Cohort upper crate slot differs from actual native startup order");
        if(previous&&previous->frameIndex==row.frameIndex)Require(row.actorSlot>previous->actorSlot,
            "Cohort update order is not ascending authentic live native Actor slots");
        for(std::size_t m=0u;m<Members;++m)stateIdentity(row.before[m],m);
        const auto& before=row.before[member];
        Require(matchesPhysicsBefore(before,step)&&row.role==before.role&&row.remoteRole==before.remoteRole&&
            step.hasLandingCounter==(member==0u),"Cohort per-Actor input disagrees with the same-root all-member before snapshot");
        const bool expected=before.remoteRole!=3u&&(before.role>=2u?(before.physics!=0u&&before.role!=3u):before.physics==2u);
        Require(row.eligible==expected,"Cohort eligibility differs from independent original role/remote-role/Physics suffix oracle");
        if(previous) {
            Require(previous->captured&&row.before==previous->after&&step.writesBefore==previous->physics.writesAfter,
                "Cohort lost exact all-member state/write continuity between adjacent ordered updates");
            const auto previousMember=previous->physics.commandIndex;
            if(row.frameIndex==previous->frameIndex) {
                Require(member>previousMember,"Cohort repeated or reversed a member within its original ordered frame");
                for(auto m=previousMember+1u;m<member;++m)Require(row.before[m].removed,
                    "Cohort skipped a retained nonremoved member within an ordered frame");
            }else {
                Require(row.frameIndex==previous->frameIndex+1u,"Cohort skipped or repeated an original physics frame");
                for(auto m=previousMember+1u;m<Members;++m)Require(row.before[m].removed,
                    "Cohort ended a frame before updating every retained nonremoved member");
                for(std::size_t m=0u;m<member;++m)Require(row.before[m].removed,
                    "Cohort began the next frame after skipping a retained nonremoved member");
            }
        }else {
            Require(member==0u&&row.frameIndex==0u&&before.physics==2u&&before.basePath.empty()&&!before.removed&&!before.deleteMe&&
                before.location==settledUpper->locationAfter&&before.velocity==settledUpper->velocityAfter&&
                before.rotation==settledUpper->rotationAfter&&step.writesBefore>=fatal->writesAfter,
                "First cohort update did not preserve actual upper-crate pose after genuine support loss");
            for(std::size_t m=1u;m<Members;++m) {
                const auto& state=row.before[m];const auto& birth=*births[m];
                Require(state.location==birth.location&&state.velocity==birth.velocity&&state.basePath==birth.basePath&&
                    state.statePath==birth.statePath&&state.physics==birth.physics&&state.remoteRole==birth.remoteRole&&
                    !state.removed&&!state.deleteMe,"Cohort did not begin with every unmodified genuine surviving damage birth");
            }
        }
        if(row.captured) {
            Require(row.after.size()==Members&&step.captured&&step.entered==row.eligible&&step.returned==row.eligible,
                "Cohort captured an incomplete physics suffix or manufactured a call for a skipped member");
            for(std::size_t m=0u;m<Members;++m)stateIdentity(row.after[m],m);
            Require(matchesPhysicsAfter(row.after[member],step),"Cohort source after-state disagrees with its actual per-Actor native output");
            if(row.eligible) {
                ++eligible;
                Require(step.substeps<=8u&&step.moveCalls==step.substeps&&step.oldLocationWords==3u*step.substeps&&
                    step.teleportWrites==step.substeps&&step.counterWrites<=step.substeps&&
                    (step.hasLandingCounter||step.counterWrites==0u)&&step.physicsRotationCalls<=1u&&
                    step.rotationMoveCalls<=step.physicsRotationCalls&&step.endedRotationCalls<=step.physicsRotationCalls&&
                    step.postTouchCalls<=1u&&step.pendingWrites<=2u,
                    "Cohort altered original finite falling-store bounds or repeated a per-Actor epilogue operation");
                if(before.physics==2u)Require(step.substeps>0u,"Eligible live Falling member omitted its original gravity timestep");
                else Require(step.substeps==0u,"Non-Falling member was sent through a fabricated gravity timestep");
                if(step.hitWallCalls)Require(member>0u&&member<7u&&step.selectedHitWall=="DeusEx.DeusExFragment.HitWall",
                    "Cohort bounce changed the actual original fragment HitWall selection");
                else Require(step.selectedHitWall.empty(),"Cohort fabricated a selected bounce without a real callback");
                if(step.stoppedByBounce)Require(step.hitWallCalls>0u&&step.physicsAfter==0u&&step.accelerationWords==0u&&!step.landingEntered,
                    "Cohort bounce stop lost its original Physics0 early-return semantics");
                sweeps+=step.moveCalls;hits+=step.hitWallCalls;rotations+=step.physicsRotationCalls;
                rotationMoves+=step.rotationMoveCalls;landings+=step.landedCalls;postTouches+=step.postTouchCalls;
            }else {
                ++skipped;
                Require(row.after==row.before&&step.writesAfter==step.writesBefore&&row.carries.empty()&&step.substeps==0u&&
                    step.moveCalls==0u&&step.hitWallCalls==0u&&step.landedCalls==0u&&step.setPhysicsCalls==0u&&
                    step.oldLocationWords==0u&&step.velocityWords==0u&&step.accelerationWords==0u&&step.teleportWrites==0u&&
                    step.counterWrites==0u&&step.physicsRotationCalls==0u&&step.rotationMoveCalls==0u&&
                    step.endedRotationCalls==0u&&step.postTouchCalls==0u&&step.pendingWrites==0u,
                    "Ineligible cohort member produced native movement, callbacks, carries, writes or collateral state changes");
            }
            ++captured;
        }else Require(i+1u==rows.size()&&!world.execution.passed()&&!world.execution.committed&&!ReachedTrainingPossess(world),
            "Cohort continued after an uncaptured original physics boundary or fabricated successful startup");
        Require(row.carries.size()<=64u,"Cohort carry observations exceed their detached packet bound");
        for(std::size_t n=0u;n<row.carries.size();++n) {
            const auto& carry=row.carries[n];
            Require(CanonicalIdentity(carry.sourcePath)&&CanonicalIdentity(carry.childPath)&&
                carry.sourcePath.starts_with("00_Training.")&&carry.childPath.starts_with("00_Training.")&&
                CanonicalIdentity(carry.baseBefore,true)&&CanonicalIdentity(carry.baseAfter,true)&&carry.entered&&
                row.eligible&&carry.baseBefore==carry.sourcePath&&carry.options==std::array<std::int32_t,4u>{0,0,1,0}&&
                (!carry.returned||carry.entered)&&(!carry.captured||carry.returned)&&
                carry.writesBefore>=step.writesBefore&&carry.writesBefore<=carry.writesAfter&&carry.writesAfter<=step.writesAfter,
                "Cohort child carry lost actual source/child/baseline/root write packet identity");
            if(!carry.captured)Require(!row.captured&&!world.execution.passed()&&!world.execution.committed,
                "Incomplete nested native carry was published as completed parent physics");
            for(unsigned channel=0u;channel<3u;++channel)Require(std::isfinite(carry.locationBefore[channel])&&std::isfinite(carry.delta[channel])&&
                (!carry.captured||std::isfinite(carry.locationAfter[channel])),"Cohort carry contains nonfinite active pose/delta data");
            ++carries;if(carry.captured&&carry.moved)++carryMoves;
            std::cout<<std::setprecision(9)<<"TRAINING COHORT CARRY row="<<i<<" nested="<<n
                <<" source="<<std::quoted(carry.sourcePath)<<" child="<<std::quoted(carry.childPath)
                <<" entered="<<carry.entered<<" returned="<<carry.returned<<" captured="<<carry.captured<<" moved="<<carry.moved
                <<" baseBefore="<<std::quoted(carry.baseBefore)<<" baseAfter="<<std::quoted(carry.baseAfter)
                <<" locationBefore=("<<carry.locationBefore[0u]<<','<<carry.locationBefore[1u]<<','<<carry.locationBefore[2u]
                <<") locationAfter=("<<carry.locationAfter[0u]<<','<<carry.locationAfter[1u]<<','<<carry.locationAfter[2u]
                <<") delta=("<<carry.delta[0u]<<','<<carry.delta[1u]<<','<<carry.delta[2u]
                <<") rotation=("<<carry.rotation[0u]<<','<<carry.rotation[1u]<<','<<carry.rotation[2u]
                <<") options=("<<carry.options[0u]<<','<<carry.options[1u]<<','<<carry.options[2u]<<','<<carry.options[3u]
                <<") writesBefore="<<carry.writesBefore<<" writesAfter="<<carry.writesAfter<<'\n';
        }
        std::cout<<std::setprecision(9)<<"TRAINING COHORT frame="<<row.frameIndex<<" member="<<member<<" slot="<<row.actorSlot
            <<" actor="<<std::quoted(step.actorPath)<<" role="<<unsigned(row.role)<<" remoteRole="<<unsigned(row.remoteRole)
            <<" eligible="<<row.eligible<<" entered="<<step.entered<<" returned="<<step.returned<<" captured="<<row.captured
            <<" zBefore="<<step.locationBefore[2u]<<" zAfter="<<step.locationAfter[2u]
            <<" physicsBefore="<<unsigned(step.physicsBefore)<<" physicsAfter="<<unsigned(step.physicsAfter)
            <<" baseBefore="<<std::quoted(step.baseBefore)<<" baseAfter="<<std::quoted(step.baseAfter)
            <<" stateAfter="<<std::quoted(step.stateAfter)<<" substeps="<<step.substeps<<" sweeps="<<step.moveCalls
            <<" rotations="<<step.physicsRotationCalls<<" rotationMoves="<<step.rotationMoveCalls<<" hits="<<step.hitWallCalls
            <<" landed="<<step.landedCalls<<" carries="<<row.carries.size()
            <<" writesBefore="<<step.writesBefore<<" writesAfter="<<step.writesAfter<<'\n';
        if(row.captured)for(std::size_t m=0u;m<Members;++m) {
            const auto& state=row.after[m];
            std::cout<<std::setprecision(9)<<"TRAINING COHORT STATE row="<<i<<" member="<<m<<" actor="<<std::quoted(state.actorPath)
                <<" class="<<std::quoted(state.classPath)<<" base="<<std::quoted(state.basePath)<<" state="<<std::quoted(state.statePath)
                <<" location=("<<state.location[0u]<<','<<state.location[1u]<<','<<state.location[2u]
                <<") velocity=("<<state.velocity[0u]<<','<<state.velocity[1u]<<','<<state.velocity[2u]
                <<") rotation=("<<state.rotation[0u]<<','<<state.rotation[1u]<<','<<state.rotation[2u]
                <<") physics="<<unsigned(state.physics)<<" role="<<unsigned(state.role)<<" remoteRole="<<unsigned(state.remoteRole)
                <<" removed="<<state.removed<<" deleteMe="<<state.deleteMe<<'\n';
        }
        previous=&row;previousWrites=step.writesAfter;
    }
    if(ReachedTrainingPossess(world)) {
        Require(previous&&previous->captured&&captured==rows.size(),
            "Original Possess reached without a captured ordered cohort physics-suffix prefix");
        for(auto m=previous->physics.commandIndex+1u;m<Members;++m)Require(previous->after[m].removed,
            "Original Possess reached before the final physics frame updated every retained member");
        if(previous->frameIndex+1u<Frames)for(const auto& state:previous->after)Require(state.removed,
            "Original Possess reached while requested cohort frames still had retained nonremoved members");
    }
    std::cout<<"TRAINING COHORT OBSERVATIONS rows="<<rows.size()<<" captured="<<captured<<" eligible="<<eligible<<" skipped="<<skipped
        <<" sweeps="<<sweeps<<" hits="<<hits<<" landed="<<landings<<" rotations="<<rotations<<" rotationMoves="<<rotationMoves
        <<" carries="<<carries<<" carryMoves="<<carryMoves<<" postTouches="<<postTouches
        <<"; frame-major original physics suffix only, NOT full Actor/Level Tick, timers/lifespan/state Tick, world publication or headset acceptance.\n";
}
