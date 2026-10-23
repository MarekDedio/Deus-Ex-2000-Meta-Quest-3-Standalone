// Explicit original physFalling on the caller's actual world/VM transaction.
// This is not performPhysics (rotation/PendingTouch) or a world scheduler.
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
        actor=RetailCollisionActor(reinterpret_cast<std::uintptr_t>(actor));
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
        q.isPawnClass=[&](auto a){requireSource(a);return RetailTouchClass(a,"Engine.Pawn");};
        q.isPlayerPawnClass=[&](auto a){ZoneActor(a,level);return RetailTouchClass(a,"Engine.PlayerPawn");};
        q.isDecorationClass=[&](auto a){requireSource(a);return RetailTouchClass(a,"Engine.Decoration");};
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
            if(!RetailTouchClass(z,"Engine.ZoneInfo"))throw std::runtime_error("Original falling gravity receiver is not ZoneInfo");
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
            if(!RetailTouchClass(z,"Engine.ZoneInfo"))throw std::runtime_error("Original falling terminal receiver is not ZoneInfo");
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
        }
        ValidateStateBudget();return stats;
    }
