// Genuine initialized first-world caller. Explicit diagnostic swept motion,
// not an automatic falling solver, fabricated support or partial startup commit.
    void ValidateStartupLandingCommands(const PortableWorldStartupProbeOptions& options) {
        if(options.landingCommands.empty())return;
        if(options.landingCommands.size()>8u || !options.playerLogin || !persistentNativeClient.HasClient() ||
            persistentNativeClient.ReadOnlyView().size()!=1u)
            throw std::runtime_error("Startup landing requires at most eight sweeps and one real Login viewport");
        for(const auto& command:options.landingCommands) {
            LifecycleStep();
            if(command.actorPath.empty() || command.actorPath.size()>1024u || command.actorPath.find('\0')!=std::string::npos)
                throw std::runtime_error("Startup landing has an invalid bounded Actor identity");
            for(const auto value:command.delta)if(!std::isfinite(value))
                throw std::runtime_error("Startup landing sweep has a nonfinite delta");
        }
    }
    void ProbeStartupLandings(RuntimeObject* player,const std::vector<PortableStartupLandingCommand>& commands,
        QuestVr::Vm::Execution& execution) {
        if(commands.empty())return;
        if(!transaction_ || !worldStartupProbe_ || !worldStartupResult_ ||
            !worldStartupResult_->landingObservations.empty())
            throw std::runtime_error("Startup landing requires its one actual startup caller root");
        auto* level=RetailCollisionLevel();ZoneActor(reinterpret_cast<std::uintptr_t>(player),level);
        auto* info=NativeLevelInfo(player);auto* game=ActorLink(info,"Game");
        if(!IsDerivedFromPath(player->cls,"Engine.PlayerPawn") || player->worldRemoved || !game ||
            !IsDerivedFromPath(game->cls,"Engine.GameInfo") || NativeLevelInfo(game)!=info ||
            !Read(info,PropertyNamed(info,"bBegunPlay"),0u).boolean || Read(info,PropertyNamed(info,"bStartup"),0u).boolean ||
            !Read(player,PropertyNamed(player,"Player"),0u).text.empty())
            throw std::runtime_error("Startup landing lacks real begun GameInfo/unpaired Login-player authority");
        constexpr std::size_t outputBound=256u<<10u;
        ValidateStateBudget(outputBound);
        nativeActorOrderObservationBytes_=QuestVr::DecalStateDetail::Add(nativeActorOrderObservationBytes_,outputBound);
        QuestVr::AuthoredStructLimits limits;limits.retainedBytes=outputBound;
        QuestVr::AuthoredStructDetail::Budget retained{limits};
        QuestVr::NativeScratchScope scratch(nativeQueryScratchBytes_,32u<<10u,
            [this](auto bytes){ValidateStateBudget(bytes);});
        auto& output=worldStartupResult_->landingObservations;
        retained.Retain(commands.size()*sizeof(PortableStartupLandingObservation));output.reserve(commands.size());
        retained.Retain(output.capacity()*sizeof(PortableStartupLandingObservation));
        const auto text=[&](std::string& out,std::string_view in) {
            if(in.size()>1024u || in.find('\0')!=std::string_view::npos)
                throw std::runtime_error("Startup landing detached identity exceeds its bound");
            retained.Retain(in.size()+1u);out.assign(in);retained.Retain(out.capacity()+1u);
        };
        const auto snapshot=[&](RuntimeObject* actor,PortableStartupLandingObservation& row,bool after) {
            auto& location=after?row.locationAfter:row.locationBefore;
            location=NativeCoordsRead(actor,"Engine.Actor.Location",Kind::Vector,0x23u).vector;
            (after?row.velocityAfter:row.velocityBefore)=ReadActorPhysicsVector(actor,
                ActorPhysicsVectorProperty(actor,"Engine.Actor.Velocity")).vector;
            (after?row.physicsAfter:row.physicsBefore)=static_cast<std::uint8_t>(Read(actor,ActorPhysicsProperty(actor),0u).integer);
            (after?row.counterAfter:row.counterBefore)=Read(actor,LandingScalarProperty(actor,
                "Engine.Decoration.numLandings","Engine.Decoration",Kind::Int,0x2u),0u).integer;
            text(after?row.baseAfter:row.baseBefore,
                RetailPredicatePointerRead(actor,"Engine.Actor.Base","Engine.Actor",0x22u).text);
            const auto region=ZoneStoredRegion(actor,ZoneRegionProperty(actor,QuestVr::OriginalActorRegionSlot::Main));
            text(after?row.zoneAfter:row.zoneBefore,region.fields.at("zone").text);
            (after?row.clockAfter:row.clockBefore)=ZoneClockSnapshot(actor);
        };
        for(const auto& command:commands) {
            auto* actor=RetailCollisionActor(reinterpret_cast<std::uintptr_t>(Object(command.actorPath)));
            if(!RetailTouchClass(reinterpret_cast<std::uintptr_t>(actor),"Engine.Decoration") || actor->worldRemoved ||
                Read(actor,ActorPhysicsProperty(actor),0u).integer!=2 ||
                Read(actor,PropertyNamed(actor,"bDeleteMe"),0u).boolean)
                throw std::runtime_error("Startup landing sweep requires an actual live Falling Decoration");
            PortableStartupLandingObservation staged;staged.commandIndex=output.size();
            text(staged.actorPath,actor->reflection.objectPath);text(staged.gameInfoPath,game->reflection.objectPath);
            snapshot(actor,staged,false);staged.writesBefore=staged.writesAfter=execution.WriteCount();
            output.push_back(std::move(staged));auto& row=output.back();row.entered=true;
            try {
                QuestVr::OriginalActorLineHit hit;
                const auto rotation=NativeCoordsRead(actor,"Engine.Actor.Rotation",Kind::Rotator,0x23u).rotation;
                row.moved=MoveLevelActor(reinterpret_cast<std::uintptr_t>(level),reinterpret_cast<std::uintptr_t>(actor),
                    {command.delta[0u],command.delta[1u],command.delta[2u]},rotation,hit,{},execution);
                row.hitTime=hit.time;row.normal={hit.normal.x,hit.normal.y,hit.normal.z};
                auto* floor=hit.actor?ZoneActor(hit.actor,level):nullptr;
                text(row.floorPath,floor?floor->reflection.objectPath:std::string_view{});
                // Same native collision gate as physFalling. No hit, deletion,
                // bounce or wall is disguised as an ordinary floor landing.
                if(!Read(actor,PropertyNamed(actor,"bDeleteMe"),0u).boolean && hit.time<1.0f && hit.normal.z>0.7) {
                    if(Read(actor,PropertyNamed(actor,"bBounce"),0u).boolean)
                        throw std::runtime_error("Startup explicit landing sweep reached unimplemented bounce dispatch");
                    const auto selected=Member(actor,"Landed");
                    if(!selected)throw std::runtime_error("Startup landing has no original selected Landed");
                    text(row.selectedLanded,selected->path);row.landingEntered=true;
                    const auto stats=RetailProcessDecorationLanded(actor,hit.normal,floor,execution);
                    row.traceCalls=stats.traceCalls;row.counterWrites=stats.counterWrites;row.velocityWords=stats.velocityWords;
                    row.landedCalls=stats.landedCalls;row.setPhysicsCalls=stats.setPhysicsCalls;
                    row.recovery=stats.recovery;row.zoneBounce=stats.zoneBounce;row.clear=stats.clear;
                }
                row.returned=true;row.writesAfter=execution.WriteCount();
                snapshot(actor,row,true);row.removed=actor->worldRemoved;
                row.deleteMe=Read(actor,PropertyNamed(actor,"bDeleteMe"),0u).boolean;row.captured=true;
            }catch(...) {row.writesAfter=execution.WriteCount();throw;}
        }
    }
