// Actual surviving original damage births, advanced by explicit same-root
// physFalling commands. This observer is NOT a world scheduler, committed
// inventory/weapon bridge, rendered loot or on-headset gravity acceptance.
void ObserveTrainingLootFalling(const PortableWorldStartupProbeResult& world,bool requested) {
    const auto& rows=world.fallingObservations;
    if(!requested){Require(rows.empty(),"Startup manufactured unrequested loot falling commands");return;}
    Require(rows.size()<=16u,"Training loot falling exceeded two requested eight-step command bounds");
    struct Birth {std::size_t damage,index;const PortableStartupDamageBirth* item;};
    std::vector<Birth> births;
    for(std::size_t damage=0u;damage<world.damageObservations.size();++damage) {
        const auto& row=world.damageObservations[damage];if(!row.captured)continue;
        for(std::size_t index=0u;index<row.births.size();++index) {
            const auto& item=row.births[index];
            if(item.classPath=="DeusEx.Lockpick"&&item.pickup&&!item.removed&&!item.deleteMe)
                births.push_back({damage,index,&item});
        }
    }
    Require(births.size()<=2u,"Training loot falling found more than two genuine surviving Lockpick births");
    const auto finite=[](const auto& vector) {
        return std::all_of(vector.begin(),vector.end(),[](float value){return std::isfinite(value);});
    };
    const auto clock=[](const Bytes& bytes,const std::string& actor) {
        Require(bytes.size()<=64u*1024u,"Training loot falling clock exceeds its bounded canonical packet");
        if(bytes.empty())return;
        const auto state=QuestVr::DecodeScriptSavedState(bytes);
        Require(QuestVr::EncodeScriptSavedState(state)==bytes&&state.mapName=="00_Training"&&state.objects.size()==1u&&
            state.objects.front().path==actor&&state.objects.front().clock.has_value(),
            "Training loot falling clock is not the actual actor's complete canonical animation packet");
    };
    const auto stopped=[](const PortableStartupFallingObservation& row) {
        return row.captured&&row.landingEntered&&!row.recovery&&!row.zoneBounce&&row.landedCalls==1u&&
            row.setPhysicsCalls==1u&&row.physicsAfter==0u&&!row.removed&&!row.deleteMe&&
            !row.floorPath.empty()&&row.baseAfter==row.floorPath&&
            row.velocityAfter==std::array<float,3u>{}&&row.accelerationAfter==std::array<float,3u>{};
    };
    std::array<const PortableStartupFallingObservation*,2u> first{},last{};
    const PortableStartupFallingObservation* previous{};
    std::size_t captured{},settled{},moves{},apex{},previousWrites{};
    if(!world.damageObservations.empty())previousWrites=world.damageObservations.back().writesAfter;
    for(std::size_t i=0u;i<rows.size();++i) {
        const auto& row=rows[i];
        Require(row.commandIndex<2u&&row.commandIndex<births.size(),
            "Training loot falling command lacks a captured actual surviving damage birth");
        const auto& birth=births[row.commandIndex];
        Require(world.damageObservations.size()==4u&&std::all_of(world.damageObservations.begin(),world.damageObservations.end(),
            [](const auto& value){return value.captured;}),
            "Training loot falling entered before its four genuine crate damage commands completed");
        Require(row.stepIndex<8u&&row.deltaSeconds==0.1f&&row.afterDamage&&row.actorPath==birth.item->actorPath&&
            row.classPath==birth.item->classPath&&row.classPath=="DeusEx.Lockpick"&&!row.hasLandingCounter&&
            row.gameInfoPath==world.gameInfoPath&&row.entered&&(!row.returned||row.entered)&&(!row.captured||row.returned)&&
            row.physicsBefore==2u&&row.writesBefore>=previousWrites&&row.writesBefore<=row.writesAfter&&
            row.writesAfter<=world.execution.writes,
            "Training loot falling lost genuine birth/class/root/phase/timestep/write-prefix identity");
        Require(CanonicalIdentity(row.actorPath)&&row.actorPath.starts_with("00_Training.")&&CanonicalIdentity(row.classPath)&&
            CanonicalIdentity(row.gameInfoPath)&&CanonicalIdentity(row.zoneBefore)&&CanonicalIdentity(row.zoneAfter,!row.captured)&&
            CanonicalIdentity(row.baseBefore,true)&&CanonicalIdentity(row.baseAfter,true)&&CanonicalIdentity(row.floorPath,true)&&
            CanonicalIdentity(row.selectedLanded,true),"Training loot falling has an invalid/unbounded canonical identity");
        Require(finite(row.locationBefore)&&finite(row.velocityBefore)&&finite(row.accelerationBefore)&&finite(row.oldLocationBefore),
            "Training loot falling before snapshot contains nonfinite actual native vectors");
        clock(row.clockBefore,row.actorPath);
        if(previous&&row.commandIndex==previous->commandIndex) {
            Require(previous->captured&&previous->physicsAfter==2u&&!previous->removed&&!previous->deleteMe&&
                row.stepIndex==previous->stepIndex+1u&&row.locationBefore==previous->locationAfter&&
                row.velocityBefore==previous->velocityAfter&&row.accelerationBefore==previous->accelerationAfter&&
                row.oldLocationBefore==previous->oldLocationAfter&&row.baseBefore==previous->baseAfter&&
                row.zoneBefore==previous->zoneAfter&&row.clockBefore==previous->clockAfter&&
                row.teleportedBefore==previous->teleportedAfter&&row.writesBefore==previous->writesAfter,
                "Training loot falling lost same-command full native/clock/write continuity");
        }else {
            Require(row.commandIndex==(previous?previous->commandIndex+1u:0u)&&row.stepIndex==0u&&
                row.locationBefore==birth.item->location&&row.velocityBefore==birth.item->velocity&&
                row.baseBefore==birth.item->basePath&&birth.item->physics==2&&birth.item->velocity[2u]>0.0f,
                "Training loot falling skipped a command or replaced its genuine captured birth launch state");
            if(previous)Require(previous->captured&&(previous->physicsAfter!=2u||previous->stepIndex==7u||
                previous->removed||previous->deleteMe),"Training loot falling skipped requested remaining live timesteps");
            first[row.commandIndex]=&row;
        }
        if(row.captured) {
            Require(finite(row.locationAfter)&&finite(row.velocityAfter)&&finite(row.accelerationAfter)&&
                finite(row.oldLocationAfter)&&finite(row.normal)&&std::isfinite(row.hitTime),
                "Training loot falling captured nonfinite actual pose, motion or swept contact data");
            clock(row.clockAfter,row.actorPath);
            Require(row.substeps>=1u&&row.substeps<=8u&&row.moveCalls==row.substeps&&
                row.oldLocationWords==3u*row.substeps&&row.teleportWrites==row.substeps&&row.apexSplits<=row.substeps&&
                row.velocityWords>=3u*(row.substeps+row.apexSplits)&&row.velocityWords%3u==0u&&
                row.counterWrites==0u&&row.traceCalls==0u&&!row.recovery,
                "Training loot falling lost actual substep/raw-store counters or executed inactive Decoration fit/counter work");
            // No axis lock, forced downward profile, assumed teleport flag or
            // unchanged-clock assertion: genuine pickup callbacks own them.
            if(row.landingEntered) {
                Require(row.hitTime>=0.0f&&row.hitTime<1.0f&&row.normal[2u]>0.7&&!row.floorPath.empty()&&
                    row.selectedLanded=="DeusEx.DeusExPickup.Pickup.Landed"&&row.accelerationWords==0u,
                    "Training loot falling manufactured a floor/selected pickup Landed or restored acceleration on its early return");
                if(!row.zoneBounce) {
                    Require(stopped(row),"Original pickup Landed did not stop and establish actual swept floor support through SetPhysics0");
                    ++settled;
                }
            }else {
                Require(row.selectedLanded.empty()&&row.landedCalls==0u&&row.setPhysicsCalls==0u,
                    "Training loot falling fabricated selected event/support work without an actual landing handoff");
                if(!row.removed&&!row.deleteMe)Require(row.physicsAfter==2u&&row.accelerationWords==3u&&
                    row.accelerationAfter==row.accelerationBefore,
                    "Continuing original loot falling lost Falling mode or loop-completion raw acceleration restore");
            }
            moves+=row.moveCalls;apex+=row.apexSplits;++captured;
        }else Require(i+1u==rows.size()&&!world.execution.passed()&&!world.execution.committed,
            "Training continued after an incomplete loot timestep or fabricated startup acceptance");
        std::cout<<std::setprecision(9)<<"TRAINING LOOT FALLING command="<<row.commandIndex<<" step="<<row.stepIndex
            <<" damageCommand="<<birth.damage<<" birth="<<birth.index<<" actor="<<std::quoted(row.actorPath)
            <<" class="<<std::quoted(row.classPath)<<" hasLandingCounter="<<row.hasLandingCounter
            <<" dt="<<row.deltaSeconds<<" afterDamage="<<row.afterDamage<<" entered="<<row.entered
            <<" returned="<<row.returned<<" captured="<<row.captured<<" moved="<<row.moved
            <<" substeps="<<row.substeps<<" moves="<<row.moveCalls<<" apexSplits="<<row.apexSplits
            <<" before=("<<row.locationBefore[0u]<<','<<row.locationBefore[1u]<<','<<row.locationBefore[2u]
            <<") after=("<<row.locationAfter[0u]<<','<<row.locationAfter[1u]<<','<<row.locationAfter[2u]
            <<") velocityBefore=("<<row.velocityBefore[0u]<<','<<row.velocityBefore[1u]<<','<<row.velocityBefore[2u]
            <<") velocityAfter=("<<row.velocityAfter[0u]<<','<<row.velocityAfter[1u]<<','<<row.velocityAfter[2u]<<')'
            <<" physicsBefore="<<unsigned(row.physicsBefore)<<" physicsAfter="<<unsigned(row.physicsAfter)
            <<" baseAfter="<<std::quoted(row.baseAfter)<<" floor="<<std::quoted(row.floorPath)
            <<" hitTime="<<row.hitTime<<" normalZ="<<row.normal[2u]<<" landed="<<row.landingEntered
            <<" selected="<<std::quoted(row.selectedLanded)<<" oldLocationWords="<<row.oldLocationWords
            <<" velocityWords="<<row.velocityWords<<" accelerationWords="<<row.accelerationWords
            <<" teleportWrites="<<row.teleportWrites<<" teleportedBefore="<<row.teleportedBefore
            <<" teleportedAfter="<<row.teleportedAfter<<" traces="<<row.traceCalls<<" counterWrites="<<row.counterWrites
            <<" landedCalls="<<row.landedCalls<<" setterCalls="<<row.setPhysicsCalls<<" recovery="<<row.recovery
            <<" zoneBounce="<<row.zoneBounce<<" writesBefore="<<row.writesBefore<<" writesAfter="<<row.writesAfter<<'\n';
        previousWrites=row.writesAfter;previous=&row;last[row.commandIndex]=&row;
    }
    if(ReachedTrainingPossess(world)) {
        Require(births.size()==2u,"Original Possess was reached without both genuine surviving Training loot births");
        for(std::size_t command=0u;command<2u;++command)
            Require(first[command]&&last[command]&&stopped(*last[command]),
                "Original Possess was reached without both requested original loot groups captured and settled on actual support");
    }else if(!previous||!previous->captured||previous->commandIndex!=1u||
        (previous->physicsAfter==2u&&previous->stepIndex!=7u&&!previous->removed&&!previous->deleteMe))
        Require(!world.execution.passed()&&!world.execution.committed,
            "Training startup skipped requested loot falling timesteps and fabricated completion");
    std::cout<<"TRAINING LOOT FALLING OBSERVATIONS rows="<<rows.size()<<" captured="<<captured<<" moves="<<moves
        <<" apexSplits="<<apex<<" settled="<<settled
        <<"; explicit original born-item physFalling timesteps only, NOT a world scheduler or published headset gameplay.\n";
}
