// Explicit genuine physFalling steps inside the original startup journal.
// This is not a world scheduler, a forced physics profile, or a partial startup
// publication path. The later ordinary Possess/HUD continuation still runs.
    void ValidateStartupFallingCommands(const PortableWorldStartupProbeOptions& options) {
        if(options.fallingCommands.empty())return;
        if(options.fallingCommands.size()>8u || !options.playerLogin || !persistentNativeClient.HasClient() ||
            persistentNativeClient.ReadOnlyView().size()!=1u)
            throw std::runtime_error("Startup falling requires at most eight commands and one real Login viewport");
        std::size_t steps{};
        for(const auto& command:options.fallingCommands) {
            LifecycleStep();
            if(command.actorPath.empty() || command.actorPath.size()>1024u || command.actorPath.find('\0')!=std::string::npos ||
                !std::isfinite(command.deltaSeconds) || command.deltaSeconds<=0.0f || command.deltaSeconds>1.0f ||
                !command.steps || command.steps>16u)
                throw std::runtime_error("Startup falling has an invalid bounded identity, timestep or step count");
            steps+=command.steps;
            if(steps>64u)throw std::runtime_error("Startup falling exceeds sixty-four requested steps");
        }
    }
    void ProbeStartupFalling(RuntimeObject* player,const std::vector<PortableStartupFallingCommand>& commands,
        bool afterDamage,QuestVr::Vm::Execution& execution) {
        if(std::none_of(commands.begin(),commands.end(),[&](const auto& command){return command.afterDamage==afterDamage;}))return;
        if(!transaction_ || !worldStartupProbe_ || !worldStartupResult_)
            throw std::runtime_error("Startup falling requires its actual caller-owned startup root");
        auto& output=worldStartupResult_->fallingObservations;
        if((!afterDamage && !output.empty()) || std::any_of(output.begin(),output.end(),[](const auto& row){return row.afterDamage;}))
            throw std::runtime_error("Startup falling repeated an already-observed phase");
        auto* level=RetailCollisionLevel();ZoneActor(reinterpret_cast<std::uintptr_t>(player),level);
        auto* info=NativeLevelInfo(player);auto* game=ActorLink(info,"Game");
        if(!IsDerivedFromPath(player->cls,"Engine.PlayerPawn") || player->worldRemoved || !game ||
            !IsDerivedFromPath(game->cls,"Engine.GameInfo") || NativeLevelInfo(game)!=info ||
            !Read(info,PropertyNamed(info,"bBegunPlay"),0u).boolean || Read(info,PropertyNamed(info,"bStartup"),0u).boolean ||
            !Read(player,PropertyNamed(player,"Player"),0u).text.empty())
            throw std::runtime_error("Startup falling lacks real begun GameInfo/unpaired Login-player authority");
        constexpr std::size_t outputBound=512u<<10u;
        ValidateStateBudget(outputBound);
        nativeActorOrderObservationBytes_=QuestVr::DecalStateDetail::Add(nativeActorOrderObservationBytes_,outputBound);
        QuestVr::AuthoredStructLimits limits;limits.retainedBytes=outputBound;
        QuestVr::AuthoredStructDetail::Budget retained{limits};
        QuestVr::NativeScratchScope scratch(nativeQueryScratchBytes_,32u<<10u,
            [this](auto bytes){ValidateStateBudget(bytes);});
        retained.Retain(64u*sizeof(PortableStartupFallingObservation));output.reserve(64u);
        retained.Retain(output.capacity()*sizeof(PortableStartupFallingObservation));
        const auto text=[&](std::string& destination,std::string_view source) {
            if(source.size()>1024u || source.find('\0')!=std::string_view::npos)
                throw std::runtime_error("Startup falling detached identity exceeds its bound");
            retained.Retain(source.size()+1u);destination.assign(source);retained.Retain(destination.capacity()+1u);
        };
        const auto flag=[&](RuntimeObject* actor,const char* key) {
            return Read(actor,LandingScalarProperty(actor,key,"Engine.Actor",Kind::Bool,0x2u),0u).boolean;
        };
        const auto snapshot=[&](RuntimeObject* actor,PortableStartupFallingObservation& row,bool after) {
            (after?row.locationAfter:row.locationBefore)=NativeCoordsRead(actor,"Engine.Actor.Location",Kind::Vector,0x23u).vector;
            (after?row.velocityAfter:row.velocityBefore)=ReadActorPhysicsVector(actor,
                ActorPhysicsVectorProperty(actor,"Engine.Actor.Velocity")).vector;
            (after?row.accelerationAfter:row.accelerationBefore)=ReadActorPhysicsVector(actor,
                ActorPhysicsVectorProperty(actor,"Engine.Actor.Acceleration")).vector;
            (after?row.oldLocationAfter:row.oldLocationBefore)=ReadFallingOldLocation(actor,FallingOldLocationProperty(actor)).vector;
            (after?row.physicsAfter:row.physicsBefore)=static_cast<std::uint8_t>(Read(actor,ActorPhysicsProperty(actor),0u).integer);
            if(row.hasLandingCounter)
                (after?row.counterAfter:row.counterBefore)=Read(actor,LandingScalarProperty(actor,
                    "Engine.Decoration.numLandings","Engine.Decoration",Kind::Int,0x2u),0u).integer;
            (after?row.teleportedAfter:row.teleportedBefore)=flag(actor,"Engine.Actor.bJustTeleported");
            (after?row.bounceAfter:row.bounceBefore)=Read(actor,LandingScalarProperty(actor,
                "Engine.Actor.bBounce","Engine.Actor",Kind::Bool,0x21u),0u).boolean;
            (after?row.rotationAfter:row.rotationBefore)=NativeCoordsRead(actor,"Engine.Actor.Rotation",Kind::Rotator,0x23u).rotation;
            (after?row.rotationRateAfter:row.rotationRateBefore)=NativeCoordsRead(actor,"Engine.Actor.RotationRate",Kind::Rotator,0x21u).rotation;
            text(after?row.stateAfter:row.stateBefore,CurrentDispatchContext(actor).selectedPath);
            text(after?row.baseAfter:row.baseBefore,
                RetailPredicatePointerRead(actor,"Engine.Actor.Base","Engine.Actor",0x22u).text);
            const auto region=ZoneStoredRegion(actor,ZoneRegionProperty(actor,QuestVr::OriginalActorRegionSlot::Main));
            text(after?row.zoneAfter:row.zoneBefore,region.fields.at("zone").text);
            (after?row.clockAfter:row.clockBefore)=ZoneClockSnapshot(actor);
        };
        for(std::size_t commandIndex=0u;commandIndex<commands.size();++commandIndex) {
            const auto& command=commands[commandIndex];if(command.afterDamage!=afterDamage)continue;
            auto* actor=RetailCollisionActor(reinterpret_cast<std::uintptr_t>(Object(command.actorPath)));
            const auto identity=reinterpret_cast<std::uintptr_t>(actor);
            const bool decoration=RetailTouchClass(identity,"Engine.Decoration");
            const bool inventory=RetailTouchClass(identity,"Engine.Inventory");
            const bool fragment=RetailTouchClass(identity,"Engine.Fragment");
            if((!decoration && !inventory && !fragment) || actor->worldRemoved ||
                flag(actor,"Engine.Actor.bDeleteMe") || Read(actor,ActorPhysicsProperty(actor),0u).integer!=2)
                throw std::runtime_error("Startup falling requires an actual live Falling Decoration, Inventory or Fragment");
            if(inventory || fragment) {
                bool born{};
                for(const auto& damage:worldStartupResult_->damageObservations) {
                    LifecycleStep();if(!damage.captured)continue;
                    for(const auto& birth:damage.births) {
                        LifecycleStep();
                        if(birth.actorPath==actor->reflection.objectPath && birth.classPath==actor->cls->reflection.objectPath &&
                            (!inventory || birth.pickup) && !birth.removed && !birth.deleteMe)born=true;
                    }
                }
                if(!afterDamage || !actor->runtimeBirth || !born)
                    throw std::runtime_error("Startup falling Inventory/Fragment is not a captured genuine surviving damage birth");
            }
            for(std::size_t step=0u;step<command.steps;++step) {
                // Only a successfully observed earlier native step may make a
                // later requested step unnecessary by stopping/deleting itself.
                if(step && (actor->worldRemoved || flag(actor,"Engine.Actor.bDeleteMe") ||
                    Read(actor,ActorPhysicsProperty(actor),0u).integer!=2))break;
                PortableStartupFallingObservation staged;staged.commandIndex=commandIndex;staged.stepIndex=step;
                staged.deltaSeconds=command.deltaSeconds;staged.afterDamage=afterDamage;
                staged.hasLandingCounter=decoration;text(staged.classPath,actor->cls->reflection.objectPath);
                text(staged.actorPath,actor->reflection.objectPath);text(staged.gameInfoPath,game->reflection.objectPath);
                snapshot(actor,staged,false);staged.writesBefore=staged.writesAfter=execution.WriteCount();
                if(output.size()>=64u)throw std::runtime_error("Startup falling detached rows exceeded the admitted bound");
                output.push_back(std::move(staged));auto& row=output.back();row.entered=true;
                try {
                    static_cast<void>(RetailStepActorFalling(actor,command.deltaSeconds,execution,&row));
                    row.returned=true;row.writesAfter=execution.WriteCount();snapshot(actor,row,true);
                    row.removed=actor->worldRemoved;row.deleteMe=flag(actor,"Engine.Actor.bDeleteMe");row.captured=true;
                }catch(...) {row.writesAfter=execution.WriteCount();throw;}
            }
        }
    }
