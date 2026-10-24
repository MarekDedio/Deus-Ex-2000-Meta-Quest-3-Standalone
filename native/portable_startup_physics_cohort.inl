// Explicit ordered physics SUFFIX, not Actor/Level Tick. No state/animation,
// timer/lifespan/stasis/owner queue, parity or partially published startup.
    PortableStartupPhysicsCohortObservation* activePhysicsCohortObservation_{};
    QuestVr::AuthoredStructDetail::Budget* activePhysicsCohortBudget_{};
    void CohortText(std::string& out,std::string_view value,QuestVr::AuthoredStructDetail::Budget& retained) {
        if(value.size()>1024u||value.find('\0')!=std::string_view::npos)
            throw std::runtime_error("Physics cohort detached identity exceeds its bound");
        retained.Retain(value.size()+1u);out.assign(value);retained.Retain(out.capacity()+1u);
    }
    std::uint8_t CohortRole(RuntimeObject* actor,bool remote) {
        const auto field=NoiseProperty(actor,remote?"Engine.Actor.RemoteRole":"Engine.Actor.Role",
            Kind::Byte,remote?0x21u:0x20u,"Engine.Actor.ENetRole");
        auto* metadata=Object(field.key);
        if(metadata->outer!=Object("Engine.Actor")||metadata->property->baseField||
            LowerAscii(Qualified(Table(metadata->sourcePath),Table(metadata->sourcePath).exports.at(metadata->exportIndex).ObjClass))!="core.byteproperty")
            throw std::runtime_error("Physics cohort role lacks its original declaring Actor Byte");
        return static_cast<std::uint8_t>(Read(actor,field,0u).integer);
    }
    bool CohortFlag(RuntimeObject* actor,const char* key,std::uint32_t flags=0x2u) {
        return Read(actor,LandingScalarProperty(actor,key,"Engine.Actor",Kind::Bool,flags),0u).boolean;
    }
    void RequireCohortActorPhysicsImplementation(RuntimeObject* actor) {
        // Owned metadata RF_Native, not inherited ClassFlags or a broad IsA.
        // Static original vtable/IAT evidence for these three native classes
        // is recorded in ORIGINAL-ACTOR-TICK-PHYSICS-PHASE.md. Unproven native
        // overrides must not silently receive a qualified Actor base call.
        auto* cls=actor->cls;
        for(std::size_t depth=0u;cls&&depth<64u;++depth,cls=cls->base) {
            LifecycleStep();
            if(!cls->classDescriptor)throw std::runtime_error("Physics cohort native ancestry lacks authored Class metadata");
            if(!(cls->reflection.flags&0x04000000u))continue;
            const auto& path=cls->reflection.objectPath;
            if(path=="DeusEx.DeusExDecoration"||path=="Engine.Projectile"||path=="Engine.Inventory")return;
            throw std::runtime_error("Physics cohort reached an unproven original native physics override: "+path);
        }
        throw std::runtime_error("Physics cohort has no bounded authentic native Actor implementation");
    }
    void ValidateStartupPhysicsCohort(const PortableWorldStartupProbeOptions& options) {
        if(!options.physicsCohort)return;
        const auto& command=*options.physicsCohort;
        if(!options.playerLogin||!persistentNativeClient.HasClient()||persistentNativeClient.ReadOnlyView().size()!=1u||
            command.placedActors.size()>8u||command.damageCommandIndex>=options.damageCommands.size()||
            !std::isfinite(command.deltaSeconds)||command.deltaSeconds<=0.0f||command.deltaSeconds>0.4f||
            !command.frames||command.frames>8u)
            throw std::runtime_error("Physics cohort requires one real viewport and bounded damage source/frame inputs");
        for(const auto& path:command.placedActors) {
            LifecycleStep();
            if(path.empty()||path.size()>1024u||path.find('\0')!=std::string::npos)
                throw std::runtime_error("Physics cohort placed identity exceeds its bound");
        }
        for(const auto& falling:options.fallingCommands)if(falling.afterDamage)
            throw std::runtime_error("Physics cohort cannot follow a separate actor-major after-damage update");
    }
    void CohortFallingSnapshot(RuntimeObject* actor,PortableStartupFallingObservation& row,bool after,
        QuestVr::AuthoredStructDetail::Budget& retained) {
        (after?row.locationAfter:row.locationBefore)=NativeCoordsRead(actor,"Engine.Actor.Location",Kind::Vector,0x23u).vector;
        (after?row.velocityAfter:row.velocityBefore)=ReadActorPhysicsVector(actor,ActorPhysicsVectorProperty(actor,"Engine.Actor.Velocity")).vector;
        (after?row.accelerationAfter:row.accelerationBefore)=ReadActorPhysicsVector(actor,ActorPhysicsVectorProperty(actor,"Engine.Actor.Acceleration")).vector;
        (after?row.oldLocationAfter:row.oldLocationBefore)=ReadFallingOldLocation(actor,FallingOldLocationProperty(actor)).vector;
        (after?row.physicsAfter:row.physicsBefore)=static_cast<std::uint8_t>(Read(actor,ActorPhysicsProperty(actor),0u).integer);
        if(row.hasLandingCounter)(after?row.counterAfter:row.counterBefore)=Read(actor,LandingScalarProperty(actor,
            "Engine.Decoration.numLandings","Engine.Decoration",Kind::Int,0x2u),0u).integer;
        (after?row.teleportedAfter:row.teleportedBefore)=CohortFlag(actor,"Engine.Actor.bJustTeleported");
        (after?row.bounceAfter:row.bounceBefore)=CohortFlag(actor,"Engine.Actor.bBounce",0x21u);
        (after?row.rotationAfter:row.rotationBefore)=NativeCoordsRead(actor,"Engine.Actor.Rotation",Kind::Rotator,0x23u).rotation;
        (after?row.rotationRateAfter:row.rotationRateBefore)=NativeCoordsRead(actor,"Engine.Actor.RotationRate",Kind::Rotator,0x21u).rotation;
        CohortText(after?row.stateAfter:row.stateBefore,CurrentDispatchContext(actor).selectedPath,retained);
        CohortText(after?row.baseAfter:row.baseBefore,RetailPredicatePointerRead(actor,"Engine.Actor.Base","Engine.Actor",0x22u).text,retained);
        const auto region=ZoneStoredRegion(actor,ZoneRegionProperty(actor,QuestVr::OriginalActorRegionSlot::Main));
        CohortText(after?row.zoneAfter:row.zoneBefore,region.fields.at("zone").text,retained);
        auto& clock=after?row.clockAfter:row.clockBefore;
        // ZoneClockSnapshot premeasures/admit its encoded allocation through
        // the shared host ledger; account its retained capacity here too.
        clock=ZoneClockSnapshot(actor);retained.Retain(clock.capacity());
    }
    void CohortStateSnapshot(const std::vector<RuntimeObject*>& actors,
        std::vector<PortableStartupPhysicsCohortState>& output,QuestVr::AuthoredStructDetail::Budget& retained) {
        retained.Retain(actors.size()*sizeof(PortableStartupPhysicsCohortState));output.reserve(actors.size());
        retained.Retain(output.capacity()*sizeof(PortableStartupPhysicsCohortState));
        for(auto* actor:actors) {
            LifecycleStep();LifecycleActor(reinterpret_cast<std::uintptr_t>(actor));auto& row=output.emplace_back();
            CohortText(row.actorPath,actor->reflection.objectPath,retained);CohortText(row.classPath,actor->cls->reflection.objectPath,retained);
            CohortText(row.basePath,RetailPredicatePointerRead(actor,"Engine.Actor.Base","Engine.Actor",0x22u).text,retained);
            CohortText(row.statePath,CurrentDispatchContext(actor).selectedPath,retained);
            row.location=NativeCoordsRead(actor,"Engine.Actor.Location",Kind::Vector,0x23u).vector;
            row.velocity=ReadActorPhysicsVector(actor,ActorPhysicsVectorProperty(actor,"Engine.Actor.Velocity")).vector;
            row.rotation=NativeCoordsRead(actor,"Engine.Actor.Rotation",Kind::Rotator,0x23u).rotation;
            row.physics=static_cast<std::uint8_t>(Read(actor,ActorPhysicsProperty(actor),0u).integer);
            row.role=CohortRole(actor,false);row.remoteRole=CohortRole(actor,true);
            row.removed=actor->worldRemoved;row.deleteMe=CohortFlag(actor,"Engine.Actor.bDeleteMe");
        }
    }
    std::int32_t CohortObserveChildMove(std::uintptr_t source,QuestVr::OriginalActorChildMove move,
        QuestVr::OriginalActorLineHit& hit,QuestVr::Vm::Execution& execution) {
        if(!activePhysicsCohortObservation_)
            return MoveLevelActor(move.level,move.actor,move.delta,move.rotation,hit,move.options,execution)?1:0;
        if(!activePhysicsCohortBudget_)throw std::runtime_error("Physics cohort carry observation lost its bounded owner");
        auto& retained=*activePhysicsCohortBudget_;auto& output=activePhysicsCohortObservation_->carries;
        if(output.size()>=64u)throw std::runtime_error("Physics cohort carry observations exceed sixty-four recursive moves");
        if(output.size()==output.capacity()) {
            const auto capacity=std::min<std::size_t>(64u,std::max<std::size_t>(4u,output.capacity()*2u));
            retained.Retain(capacity*sizeof(PortableStartupPhysicsCarryObservation));output.reserve(capacity);
            retained.Retain(output.capacity()*sizeof(PortableStartupPhysicsCarryObservation));
        }
        auto* child=LifecycleActor(move.actor);PortableStartupPhysicsCarryObservation row;
        CohortText(row.sourcePath,LifecycleActor(source)->reflection.objectPath,retained);
        CohortText(row.childPath,child->reflection.objectPath,retained);
        CohortText(row.baseBefore,RetailPredicatePointerRead(child,"Engine.Actor.Base","Engine.Actor",0x22u).text,retained);
        row.locationBefore=NativeCoordsRead(child,"Engine.Actor.Location",Kind::Vector,0x23u).vector;
        row.delta={move.delta.x,move.delta.y,move.delta.z};row.rotation=move.rotation;row.options=move.options;
        row.writesBefore=row.writesAfter=execution.WriteCount();row.entered=true;
        const auto index=output.size();output.push_back(std::move(row));
        try {
            const auto moved=MoveLevelActor(move.level,move.actor,move.delta,move.rotation,hit,move.options,execution)?1:0;
            auto& completed=output[index];completed.returned=true;completed.moved=moved!=0;completed.writesAfter=execution.WriteCount();
            completed.locationAfter=NativeCoordsRead(child,"Engine.Actor.Location",Kind::Vector,0x23u).vector;
            CohortText(completed.baseAfter,RetailPredicatePointerRead(child,"Engine.Actor.Base","Engine.Actor",0x22u).text,retained);
            completed.captured=true;return moved;
        }catch(...){output[index].writesAfter=execution.WriteCount();throw;}
    }
    void ProbeStartupPhysicsCohort(RuntimeObject* player,const PortableStartupPhysicsCohortCommand& command,
        QuestVr::Vm::Execution& execution) {
        if(!transaction_||!worldStartupProbe_||!worldStartupResult_||!worldStartupResult_->physicsCohortObservations.empty()||
            activePhysicsCohortObservation_||activePhysicsCohortBudget_)
            throw std::runtime_error("Physics cohort requires its one original caller-owned startup root");
        auto* level=ActorOrderLevel(RetailCollisionLevel());ZoneActor(reinterpret_cast<std::uintptr_t>(player),level);
        auto* info=NativeLevelInfo(player);auto* game=ActorLink(info,"Game");
        if(!IsDerivedFromPath(player->cls,"Engine.PlayerPawn")||player->worldRemoved||!game||
            !IsDerivedFromPath(game->cls,"Engine.GameInfo")||NativeLevelInfo(game)!=info||
            !Read(info,PropertyNamed(info,"bBegunPlay"),0u).boolean||Read(info,PropertyNamed(info,"bStartup"),0u).boolean||
            !Read(player,PropertyNamed(player,"Player"),0u).text.empty()||!level->nativeFirstDynamic)
            throw std::runtime_error("Physics cohort lacks real begun GameInfo/player/native dynamic partition");
        constexpr std::size_t outputBound=2u<<20u;
        ValidateStateBudget(outputBound);nativeActorOrderObservationBytes_=QuestVr::DecalStateDetail::Add(nativeActorOrderObservationBytes_,outputBound);
        QuestVr::AuthoredStructLimits limits;limits.retainedBytes=outputBound;QuestVr::AuthoredStructDetail::Budget retained{limits};
        QuestVr::NativeScratchScope scratch(nativeQueryScratchBytes_,64u<<10u,[this](auto bytes){ValidateStateBudget(bytes);});
        std::vector<RuntimeObject*> members;members.reserve(8u);
        const auto append=[&](RuntimeObject* actor,bool birth) {
            LifecycleStep();LifecycleActor(reinterpret_cast<std::uintptr_t>(actor));const auto identity=reinterpret_cast<std::uintptr_t>(actor);
            if(actor->runtimeBirth!=birth||actor->nativeXLevel!=level||actor->worldRemoved||CohortFlag(actor,"Engine.Actor.bDeleteMe")||
                (!RetailTouchClass(identity,"Engine.Decoration")&&!RetailTouchClass(identity,"Engine.Inventory")&&!RetailTouchClass(identity,"Engine.Fragment"))||
                RetailTouchClass(identity,"Engine.Pawn")||std::find(members.begin(),members.end(),actor)!=members.end()||members.size()>=8u)
                throw std::runtime_error("Physics cohort member is not an actual distinct supported live survivor");
            members.push_back(actor);
            RequireCohortActorPhysicsImplementation(actor);
        };
        for(const auto& path:command.placedActors)append(Object(path),false);
        const auto& damage=worldStartupResult_->damageObservations;
        if(command.damageCommandIndex>=damage.size()||!damage[command.damageCommandIndex].captured)
            throw std::runtime_error("Physics cohort has no captured genuine damage birth source");
        for(const auto& birth:damage[command.damageCommandIndex].births) {
            LifecycleStep();if(birth.removed||birth.deleteMe)continue;
            auto* actor=Object(birth.actorPath);
            if(actor->cls->reflection.objectPath!=birth.classPath)
                throw std::runtime_error("Physics cohort genuine birth class identity changed");
            append(actor,true);
        }
        if(members.empty())throw std::runtime_error("Physics cohort has no genuine retained members");
        auto& output=worldStartupResult_->physicsCohortObservations;
        retained.Retain(64u*sizeof(PortableStartupPhysicsCohortObservation));output.reserve(64u);
        retained.Retain(output.capacity()*sizeof(PortableStartupPhysicsCohortObservation));
        struct Scope {
            PortableStartupPhysicsCohortObservation*& row;QuestVr::AuthoredStructDetail::Budget*& budget;
            ~Scope()noexcept{row=nullptr;budget=nullptr;}
        } scope{activePhysicsCohortObservation_,activePhysicsCohortBudget_};
        activePhysicsCohortBudget_=&retained;
        for(std::size_t frame=0u;frame<command.frames;++frame) {
            // This is the original live ascending dynamic slot traversal for
            // the explicit cohort only. New noncohort births are NOT Tick-ed.
            for(std::size_t slot=*level->nativeFirstDynamic;slot<persistentWorldActors.size();++slot) {
                LifecycleStep();if(persistentWorldActors.size()>65536u)throw std::runtime_error("Physics cohort live native slots exceed their bound");
                auto* actor=persistentWorldActors[slot];if(!actor)continue;
                const auto found=std::find(members.begin(),members.end(),actor);if(found==members.end())continue;
                if(actor->worldActorIndex!=static_cast<std::int32_t>(slot)||actor->nativeXLevel!=level)
                    throw std::runtime_error("Physics cohort member lost its actual native slot identity");
                PortableStartupPhysicsCohortObservation staged;staged.frameIndex=frame;staged.actorSlot=slot;
                auto& physics=staged.physics;physics.commandIndex=static_cast<std::size_t>(found-members.begin());
                physics.stepIndex=frame;physics.deltaSeconds=command.deltaSeconds;physics.afterDamage=true;physics.performPhysics=true;
                physics.hasLandingCounter=RetailTouchClass(reinterpret_cast<std::uintptr_t>(actor),"Engine.Decoration");
                CohortText(physics.actorPath,actor->reflection.objectPath,retained);CohortText(physics.classPath,actor->cls->reflection.objectPath,retained);
                CohortText(physics.gameInfoPath,game->reflection.objectPath,retained);CohortFallingSnapshot(actor,physics,false,retained);
                CohortStateSnapshot(members,staged.before,retained);staged.role=CohortRole(actor,false);staged.remoteRole=CohortRole(actor,true);
                staged.eligible=QuestVr::OriginalNonPawnPhysicsPhaseEligible(staged.remoteRole,staged.role,physics.physicsBefore);
                physics.writesBefore=physics.writesAfter=execution.WriteCount();
                if(output.size()>=64u)throw std::runtime_error("Physics cohort detached rows exceed sixty-four updates");
                output.push_back(std::move(staged));auto& row=output.back();activePhysicsCohortObservation_=&row;
                auto& observation=row.physics;
                try {
                    if(row.eligible) {
                        observation.entered=true;PortableActorPhysicsObservation native;
                        const auto capture=[&] {
                            observation.physicsRotationCalls=native.rotationCalls;observation.rotationMoveCalls=native.rotationMoveCalls;
                            observation.endedRotationCalls=native.endedRotationCalls;observation.postTouchCalls=native.postTouchCalls;
                            observation.pendingWrites=native.pendingWrites;
                        };
                        try{static_cast<void>(RetailPerformActorPhysics(actor,command.deltaSeconds,execution,&native,&observation));capture();}
                        catch(...){capture();throw;}
                        observation.returned=true;
                    }
                    observation.writesAfter=execution.WriteCount();CohortFallingSnapshot(actor,observation,true,retained);
                    observation.removed=actor->worldRemoved;observation.deleteMe=CohortFlag(actor,"Engine.Actor.bDeleteMe");
                    observation.captured=true;CohortStateSnapshot(members,row.after,retained);row.captured=true;
                }catch(...){observation.writesAfter=execution.WriteCount();throw;}
                activePhysicsCohortObservation_=nullptr;
            }
        }
    }
