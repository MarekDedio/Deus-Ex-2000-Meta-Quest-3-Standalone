// Explicit original physFalling on the caller's actual world/VM transaction.
// This is not performPhysics (rotation/PendingTouch) or a world scheduler.
    bool FallingNativeClass(RuntimeObject* actor,const char* path) {
        actor=LifecycleActor(reinterpret_cast<std::uintptr_t>(actor));auto* target=Object(path);
        if(!target->classDescriptor||target->defaultOwner||!target->classFieldLinksValid||
            target->reflection.metaClass!="Class"||!(target->reflection.flags&0x04000000u))
            throw std::runtime_error("Original falling class target lacks its genuine native UClass identity");
        std::array<RuntimeObject*,128u> visited{};std::size_t depth{};
        for(auto* cls=actor->cls;cls;cls=cls->base) {
            LifecycleStep();
            if(depth==visited.size()||std::find(visited.begin(),visited.begin()+depth,cls)!=visited.begin()+depth||
                !cls->classDescriptor||cls->defaultOwner||!cls->classFieldLinksValid)
                throw std::runtime_error("Original falling Class/Super walk is malformed or cyclic");
            visited[depth++]=cls;if(cls==target)return true;
        }
        return false;
    }
    Property FallingOldLocationProperty(RuntimeObject* actor) {
        RequireMapActor(actor);
        const auto field=NativeCoordsProperty(actor,"Engine.Actor.OldLocation",Kind::Vector,0x2u);
        auto* node=Object(field.key);
        if(node->outer!=Object("Engine.Actor") || !node->property || node->property->baseField ||
            LowerAscii(Qualified(Table(node->sourcePath),Table(node->sourcePath).exports.at(node->exportIndex).ObjClass))!="core.structproperty")
            throw std::runtime_error("Original falling OldLocation lacks its declaring Actor Vector");
        return field;
    }
    Value ReadFallingOldLocation(RuntimeObject* actor,const Property& field) {
        const auto value=Read(actor,field,0u,false,false,nullptr,false,false,false,true);
        if(value.kind!=Kind::Vector || !value.text.empty() || !value.fields.empty() || !value.intArray.empty())
            throw std::runtime_error("Original falling raw OldLocation is not a canonical Vector");
        return value;
    }
    void WriteFallingVectorWord(RuntimeObject* actor,const Property& field,unsigned channel,std::uint32_t bits,
        QuestVr::Vm::Execution& execution) {
        if(field.key!="Engine.Actor.OldLocation") {
            WriteActorPhysicsVectorWord(actor,field,channel,bits,execution);return;
        }
        if(channel>=3u)throw std::runtime_error("Original falling OldLocation channel is out of bounds");
        auto reference=std::make_shared<Reference>();reference->zero=Value::Integer(0);
        reference->read=[this,actor,field,channel] {
            return Value::Integer(std::bit_cast<std::int32_t>(ReadFallingOldLocation(actor,field).vector[channel]));
        };
        reference->write=[this,actor,field,channel](const Value& next) {
            if(next.kind!=Kind::Int)throw std::runtime_error("Original falling OldLocation journal is not a raw DWORD");
            auto value=ReadFallingOldLocation(actor,field);
            const auto word=std::bit_cast<std::uint32_t>(next.integer);
            std::memcpy(&value.vector[channel],&word,sizeof(word));Touch(actor);
            actor->scriptValues[InstanceStorageKey(actor->cls,field)][0u]=std::move(value);
            actor->committedScriptState=true; // Native stores never restart animation.
        };
        execution.WriteReference(reference,Value::Integer(std::bit_cast<std::int32_t>(bits)));
    }
    void WriteFallingScalar(RuntimeObject* actor,const Property& field,const Value& value,
        QuestVr::Vm::Execution& execution) {
        ReserveActorPhysicsReferences(field,1u);
        auto reference=std::make_shared<Reference>();reference->zero=field.zero;
        reference->read=[this,actor,field]{return Read(actor,field,0u);};
        reference->write=[this,actor,field](const Value& next) {
            if(next.kind!=field.zero.kind)throw std::runtime_error("Original falling scalar journal changed its declared type");
            Touch(actor);actor->scriptValues[InstanceStorageKey(actor->cls,field)][0u]=next;
            actor->committedScriptState=true;
        };
        execution.WriteReference(reference,value);
    }
    void FallingOutOfWorldDeclaration(RuntimeObject* actor,const Function& function,bool base) {
        auto* node=Object(function.path);auto* owner=node->outer;
        if(owner && owner->stateDescriptor)owner=owner->outer;
        if(!node->script || function.nativeIndex || (function.flags&0x400u) ||
            function.source!=node->sourcePath || function.flags!=node->script->functionFlags ||
            node->script->nativeIndex || !owner || !owner->classDescriptor ||
            !IsDerivedFromPath(actor->cls,owner->reflection.objectPath) ||
            LowerAscii(Qualified(Table(node->sourcePath),Table(node->sourcePath).exports.at(node->exportIndex).ObjClass))!="core.function" ||
            (base && (function.path!="Engine.Actor.FellOutOfWorld" || node->outer!=Object("Engine.Actor") ||
                function.flags!=0x802u || !function.variables.empty())))
            throw std::runtime_error("Original falling FellOutOfWorld lacks its actual script declaration");
        for(const auto& field:function.variables) {
            LifecycleStep();
            if(field.flags&0x80u)throw std::runtime_error("Selected FellOutOfWorld has an unexpected native parameter/result");
        }
    }
    QuestVr::OriginalActorFallingStats RetailStepActorFalling(RuntimeObject* actor,float deltaSeconds,
        QuestVr::Vm::Execution& execution,PortableStartupFallingObservation* observation=nullptr) {
        if(!transaction_)throw std::runtime_error("Original falling requires its caller VM root");
        // Original entry/class/field reads do not call a collision service.
        // Keep real Engine/Cylinder admission at reached sweep/query providers.
        actor=LifecycleActor(reinterpret_cast<std::uintptr_t>(actor));
        auto* level=ZoneLevel(reinterpret_cast<std::uintptr_t>(actor->nativeXLevel));
        QuestVr::NativeScratchScope scratch(nativeQueryScratchBytes_,32u<<10u,
            [this](auto bytes){ValidateStateBudget(bytes);});
        const auto source=reinterpret_cast<std::uintptr_t>(actor);
        const auto requireSource=[&](std::uintptr_t identity) {
            if(identity!=source)throw std::runtime_error("Original falling source identity changed");
            return ZoneActor(identity,level);
        };
        const auto flag=[&](auto identity,const char* key,const char* owner,std::uint32_t flags) {
            auto* receiver=ZoneActor(identity,level);
            return Read(receiver,LandingScalarProperty(receiver,key,owner,Kind::Bool,flags),0u).boolean;
        };
        const auto region=[&](auto identity) {
            auto* receiver=requireSource(identity);
            return ZoneStoredRegion(receiver,ZoneRegionProperty(receiver,QuestVr::OriginalActorRegionSlot::Main));
        };
        const auto vectorField=[&](const char* key) {
            auto field=std::string_view(key)=="Engine.Actor.OldLocation"?FallingOldLocationProperty(actor):ActorPhysicsVectorProperty(actor,key);
            auto* node=Object(field.key);
            if(node->outer!=Object("Engine.Actor") || !node->property || node->property->baseField ||
                LowerAscii(Qualified(Table(node->sourcePath),Table(node->sourcePath).exports.at(node->exportIndex).ObjClass))!="core.structproperty")
                throw std::runtime_error("Original falling vector lacks its declaring Actor field");
            return field;
        };
        const auto readVector=[&](auto identity,unsigned channel,const char* key) {
            requireSource(identity);if(channel>=3u)throw std::runtime_error("Original falling vector channel is out of bounds");
            const auto field=vectorField(key);
            return (field.key=="Engine.Actor.OldLocation"?ReadFallingOldLocation(actor,field):ReadActorPhysicsVector(actor,field)).vector[channel];
        };
        const auto writeVector=[&](auto identity,unsigned channel,std::uint32_t bits,const char* key) {
            requireSource(identity);const auto field=vectorField(key);
            if(channel==0u)ReserveActorPhysicsReferences(field,3u); // Admit every original X/Y/Z group before raw intermediate words.
            WriteFallingVectorWord(actor,field,channel,bits,execution);
        };
        const auto text=[&](std::string& destination,std::string_view value) {
            if(value.size()>1024u || value.find('\0')!=std::string_view::npos)
                throw std::runtime_error("Original falling detached observation identity exceeds its bound");
            const auto bytes=2u*(value.size()+1u);
            ValidateStateBudget(bytes);
            nativeActorOrderObservationBytes_=QuestVr::DecalStateDetail::Add(nativeActorOrderObservationBytes_,bytes);
            destination.assign(value);
        };
        QuestVr::OriginalActorFallingQueries q;q.work=[this]{LifecycleStep();};
        QuestVr::OriginalPawnFallingSetupQueries pawnQueries;pawnQueries.work=q.work;
        const auto pawnScalar=[&](auto a,const char* name) {
            auto* r=requireSource(a);
            return OriginalAnimationRead(r,OriginalAnimationField(r,"Engine.Pawn",name,Kind::Float,0x21u,"FloatProperty"),Kind::Float).floating;
        };
        pawnQueries.airControl=[&](auto a){return pawnScalar(a,"AirControl");};
        pawnQueries.accelRate=[&](auto a){return pawnScalar(a,"AccelRate");};
        pawnQueries.groundSpeed=[&](auto a){return pawnScalar(a,"GroundSpeed");};
        pawnQueries.radius=[&](auto a){return RetailFindBaseDimension(requireSource(a),"Engine.Actor.CollisionRadius");};
        pawnQueries.height=[&](auto a){return RetailFindBaseDimension(requireSource(a),"Engine.Actor.CollisionHeight");};
        pawnQueries.acceleration=[&](auto a,unsigned c){return readVector(a,c,"Engine.Actor.Acceleration");};
        pawnQueries.velocity=[&](auto a,unsigned c){return readVector(a,c,"Engine.Actor.Velocity");};
        pawnQueries.location=[&](auto a,unsigned c){requireSource(a);return MoveLocation(a,c);};
        pawnQueries.level=[&](auto a){requireSource(a);return reinterpret_cast<std::uintptr_t>(level);};
        pawnQueries.safeNormal=[](auto value){return QuestVr::ActorLineTraceDetail::Normal(value,true);};
        pawnQueries.writeAccelerationWord=[&](auto a,unsigned c,std::uint32_t bits) {
            requireSource(a);const auto field=vectorField("Engine.Actor.Acceleration");
            // Setup begins with one Z store, then may write two XYZ groups.
            // Admit each real reference rather than assuming channel0 first.
            ReserveActorPhysicsReferences(field,1u);WriteFallingVectorWord(actor,field,c,bits,execution);
        };
        pawnQueries.writeVelocityWord=[&](auto a,unsigned c,std::uint32_t bits){writeVector(a,c,bits,"Engine.Actor.Velocity");};
        pawnQueries.singleLineCheck=[&](auto l,QuestVr::OriginalActorLineHit& hit,auto a,auto end,auto start,
            std::uint32_t flags,auto extent,std::int32_t extra) {
            requireSource(a);
            if(l!=reinterpret_cast<std::uintptr_t>(level)||flags!=6u||extra)
                throw std::runtime_error("Original Pawn falling changed its predictive native Level trace packet");
            return RetailSingleLineLevel(level,actor,hit,end,start,extent,static_cast<std::uint8_t>(flags),0u).clear?1:0;
        };
        q.pawnPreprocess=[&](auto a,float,std::int32_t) {
            if(OriginalPawnPlayerCast(requireSource(a)))
                throw std::runtime_error("Original PlayerPawn physFalling preprocessing is not implemented");
            // Complete original103CC450 negative PlayerPawn cast arm returns
            // AL0 without world stores/callbacks, not an empty Pawn override.
            return false;
        };
        q.pawnSetup=[&](auto a,float dt){requireSource(a);return QuestVr::SetupOriginalPawnFalling(a,dt,pawnQueries);};
        q.pawnVelocityCap=[&](auto a,float cap){requireSource(a);return QuestVr::CapOriginalPawnFallingVelocity(a,cap,pawnQueries);};
        q.startSwimming=[&](auto a,auto,float,float,std::int32_t) {
            requireSource(a);throw std::runtime_error("Original Pawn startSwimming is not implemented");
        };
        q.isPawnClass=[&](auto a){return FallingNativeClass(requireSource(a),"Engine.Pawn");};
        q.isPlayerPawnClass=[&](auto a){return FallingNativeClass(ZoneActor(a,level),"Engine.PlayerPawn");};
        q.isDecorationClass=[&](auto a){return FallingNativeClass(requireSource(a),"Engine.Decoration");};
        q.pawnFlag=[&](auto a){requireSource(a);return flag(a,"Engine.Actor.bIsPawn","Engine.Actor",0x2u);};
        q.bounce=[&](auto a){requireSource(a);return flag(a,"Engine.Actor.bBounce","Engine.Actor",0x21u);};
        q.justTeleported=[&](auto a){requireSource(a);return flag(a,"Engine.Actor.bJustTeleported","Engine.Actor",0x2u);};
        q.deleted=[&](auto a){requireSource(a);return flag(a,"Engine.Actor.bDeleteMe","Engine.Actor",0x2u);};
        q.bobbing=[&](auto a){requireSource(a);return flag(a,"Engine.Decoration.bBobbing","Engine.Decoration",0u);};
        q.zoneWater=[&](auto z){return flag(z,"Engine.ZoneInfo.bWaterZone","Engine.ZoneInfo",0x1u);};
        q.zoneNumber=[&](auto a){return static_cast<std::uint8_t>(region(a).fields.at("zonenumber").integer);};
        q.zone=[&](auto a) {
            const auto current=region(a);const auto& path=current.fields.at("zone").text;
            return path.empty()?std::uintptr_t{}:reinterpret_cast<std::uintptr_t>(ZoneActor(reinterpret_cast<std::uintptr_t>(Object(path)),level));
        };
        q.level=[&](auto a){requireSource(a);return reinterpret_cast<std::uintptr_t>(level);};
        q.location=[&](auto a,unsigned c){requireSource(a);return MoveLocation(a,c);};
        q.oldLocation=[&](auto a,unsigned c){return readVector(a,c,"Engine.Actor.OldLocation");};
        q.velocity=[&](auto a,unsigned c){return readVector(a,c,"Engine.Actor.Velocity");};
        q.acceleration=[&](auto a,unsigned c){return readVector(a,c,"Engine.Actor.Acceleration");};
        q.zoneGravity=[&](auto z,unsigned c) {
            auto* zone=ZoneActor(z,level);if(c>=3u)throw std::runtime_error("Original falling gravity channel is out of bounds");
            if(!FallingNativeClass(zone,"Engine.ZoneInfo"))throw std::runtime_error("Original falling gravity receiver is not ZoneInfo");
            const auto field=NativeCoordsProperty(zone,"Engine.ZoneInfo.ZoneGravity",Kind::Vector,0x21u);
            auto* node=Object(field.key);
            if(node->outer!=Object("Engine.ZoneInfo") || !node->property || node->property->baseField ||
                LowerAscii(Qualified(Table(node->sourcePath),Table(node->sourcePath).exports.at(node->exportIndex).ObjClass))!="core.structproperty")
                throw std::runtime_error("Original falling ZoneGravity lacks its declaring field");
            return Read(zone,field,0u).vector[c];
        };
        q.zoneVelocity=[&](auto z,unsigned c) {
            auto* zone=ZoneActor(z,level);if(c>=3u)throw std::runtime_error("Original falling drift channel is out of bounds");
            return Read(zone,LandingZoneVelocityProperty(zone),0u,false,false,nullptr,false,false,true).vector[c];
        };
        q.terminal=[&](auto z) {
            auto* zone=ZoneActor(z,level);
            if(!FallingNativeClass(zone,"Engine.ZoneInfo"))throw std::runtime_error("Original falling terminal receiver is not ZoneInfo");
            const auto field=NoiseProperty(zone,"Engine.ZoneInfo.ZoneTerminalVelocity",Kind::Float,0x1u);
            auto* node=Object(field.key);
            if(node->outer!=Object("Engine.ZoneInfo") || !node->property || node->property->baseField || node->property->referencedType ||
                LowerAscii(Qualified(Table(node->sourcePath),Table(node->sourcePath).exports.at(node->exportIndex).ObjClass))!="core.floatproperty")
                throw std::runtime_error("Original falling terminal speed lacks its declaring Float field");
            return Read(zone,field,0u).floating;
        };
        q.writeOldLocationWord=[&](auto a,unsigned c,std::uint32_t word){writeVector(a,c,word,"Engine.Actor.OldLocation");};
        q.writeVelocityWord=[&](auto a,unsigned c,std::uint32_t word){writeVector(a,c,word,"Engine.Actor.Velocity");};
        q.writeAccelerationWord=[&](auto a,unsigned c,std::uint32_t word){writeVector(a,c,word,"Engine.Actor.Acceleration");};
        q.writeJustTeleported=[&](auto a,bool value) {
            requireSource(a);WriteFallingScalar(actor,LandingScalarProperty(actor,"Engine.Actor.bJustTeleported",
                "Engine.Actor",Kind::Bool,0x2u),Value::Bool(value),execution);
        };
        const auto counter=[&]{return LandingScalarProperty(actor,"Engine.Decoration.numLandings","Engine.Decoration",Kind::Int,0x2u);};
        q.numLandings=[&](auto a){requireSource(a);return Read(actor,counter(),0u).integer;};
        q.writeNumLandings=[&](auto a,std::int32_t value){requireSource(a);WriteFallingScalar(actor,counter(),Value::Integer(value),execution);};
        q.currentRotation=[&](auto a){requireSource(a);return NativeCoordsRead(actor,"Engine.Actor.Rotation",Kind::Rotator,0x23u).rotation;};
        q.physics=[&](auto a){requireSource(a);return static_cast<std::uint8_t>(Read(actor,ActorPhysicsProperty(actor),0u).integer);};
        q.hitWall=[&](auto a,PortableModelVec3 normal,auto wall) {
            requireSource(a);
            if(observation) {
                const auto selected=Member(actor,"HitWall");
                if(!selected)throw std::runtime_error("Original bouncing actor has no selected HitWall");
                text(observation->selectedHitWall,selected->path);
            }
            static_cast<void>(MoveSelectedEvent(actor,MoveSelectedEventKind::HitWall,normal,{},
                wall?ZoneActor(wall,level):nullptr,execution));
        };
        q.moveActor=[&](auto l,auto a,auto delta,auto rotation,auto& hit,auto options) {
            requireSource(a);if(l!=reinterpret_cast<std::uintptr_t>(level) || options!=std::array<std::int32_t,4u>{})
                throw std::runtime_error("Original falling changed its actual MoveActor packet");
            const bool moved=MoveLevelActor(l,a,delta,rotation,hit,options,execution);
            if(observation) {
                observation->moved|=moved;observation->hitTime=hit.time;
                observation->normal={hit.normal.x,hit.normal.y,hit.normal.z};
                text(observation->floorPath,hit.actor?ZoneActor(hit.actor,level)->reflection.objectPath:std::string_view{});
            }
            return moved?1:0;
        };
        q.processLanded=[&](auto a,auto normal,auto floor,float,std::int32_t) {
            requireSource(a);
            if(observation) {
                observation->landingEntered=true;
                const auto selected=Member(actor,"Landed");
                if(!selected)throw std::runtime_error("Original falling selected Landed is absent");
                text(observation->selectedLanded,selected->path);
            }
            const auto stats=RetailProcessDecorationLanded(actor,normal,floor?ZoneActor(floor,level):nullptr,execution);
            if(observation) {
                observation->traceCalls=stats.traceCalls;observation->counterWrites+=stats.counterWrites;
                observation->velocityWords+=stats.velocityWords;observation->landedCalls=stats.landedCalls;
                observation->setPhysicsCalls=stats.setPhysicsCalls;observation->recovery=stats.recovery;
                observation->zoneBounce=stats.zoneBounce;observation->clear=stats.clear;
            }
        };
        // The selected callback is authenticated by original Engine exports;
        // its script may itself reach an unsupported service, which propagates.
        q.fellOutOfWorld=[&](auto a) {
            requireSource(a);FallingOutOfWorldDeclaration(actor,*FunctionFor(Object("Engine.Actor.FellOutOfWorld")),true);
            const auto selected=Member(actor,"FellOutOfWorld");
            if(!selected)throw std::runtime_error("Original falling selected FellOutOfWorld is absent");
            FallingOutOfWorldDeclaration(actor,*FunctionFor(Object(selected->path)),false);
            static_cast<void>(execution.CallEvent(actor->reflection.objectPath,"FellOutOfWorld",true,{}));
        };
        const auto stats=QuestVr::StepOriginalActorFalling(source,deltaSeconds,0,q);
        if(observation) {
            observation->substeps=stats.substeps;observation->moveCalls=stats.moveCalls;observation->apexSplits=stats.apexSplits;
            observation->oldLocationWords=stats.oldLocationWords;observation->velocityWords+=stats.velocityWords;
            observation->accelerationWords=stats.accelerationWords;observation->teleportWrites=stats.teleportWrites;
            observation->counterWrites+=stats.counterWrites;
            observation->hitWallCalls=stats.hitWallCalls;observation->stoppedByBounce=stats.stoppedByBounce;
        }
        ValidateStateBudget();return stats;
    }
