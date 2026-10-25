// Included inside PortableActorVmHost; all writes borrow its actual Execution.
    void AdvanceOriginalPlayerDistance(PortableLevelPlayerDistanceObservation& result,QuestVr::Vm::Execution& execution) {
        if(!transaction_)throw std::runtime_error("Original distance prepass requires its caller transaction");
        auto* level=ZoneLevel(reinterpret_cast<std::uintptr_t>(CurrentClockLevel()));
        const auto identity=reinterpret_cast<std::uintptr_t>(level);
        const auto owner=[&](std::uintptr_t value) {
            if(value!=identity)throw std::runtime_error("Original distance prepass changed native Level");
            return ZoneLevel(value);
        };
        const auto actor=[&](std::uintptr_t value){return ZoneActor(value,level);};
        QuestVr::OriginalLevelPlayerDistanceQueries q;q.work=[this]{LifecycleStep();};
        q.netMode=[&](auto value) {
            static_cast<void>(owner(value));
            if(persistentWorldActors.empty()||!persistentWorldActors.front())
                throw std::runtime_error("Original distance prepass lacks actual Level slot0");
            auto* info=actor(reinterpret_cast<std::uintptr_t>(persistentWorldActors.front()));
            const auto field=OriginalAnimationField(info,"Engine.LevelInfo","NetMode",Kind::Byte,0u,"ByteProperty");
            auto* metadata=Object(field.key);
            if(!metadata->property->referencedType||
                LowerAscii(Qualified(Table(metadata->sourcePath),metadata->property->referencedType))!="engine.levelinfo.enetmode")
                throw std::runtime_error("Original distance NetMode does not reference actual ENetMode");
            return static_cast<std::uint8_t>(OriginalAnimationRead(info,field,Kind::Byte).integer);
        };
        q.firstViewportActor=[&](auto value) {
            auto* nativeLevel=owner(value);auto* engine=nativeLevel->nativeLevelEngine;
            if(!persistentEngineCollision||engine!=persistentEngineCollision->get()->engine||!engine||
                !engine->nativeEngineInstance||engine->nativeEngineInstance->currentLevel!=nativeLevel)
                throw std::runtime_error("Original distance prepass lacks actual native Level.Engine");
            auto* client=engine->nativeEngineInstance->values.client;
            if(!client||!persistentPlayerSession||client!=persistentPlayerSession->get()->client||
                !persistentNativeClient.HasClient()||client->nativeClientIdentity!=persistentNativeClient.ClientIdentity()||
                Object(client->reflection.objectPath)!=client)
                throw std::runtime_error("Original distance prepass lacks a live actual Engine.Client");
            const auto viewports=persistentNativeClient.ReadOnlyView();
            if(viewports.empty())throw std::runtime_error("Original distance prepass cannot dereference empty native Viewports[0]");
            auto* viewport=NativeSessionViewport(viewports.front().identity);
            if(viewport->outer!=client)throw std::runtime_error("Original distance first viewport has another Client owner");
            clientDependent_=true;
            return reinterpret_cast<std::uintptr_t>(viewport->nativeViewportActor);
        };
        q.isPlayerPawn=[&](auto value){return IsDerivedFromPath(actor(value)->cls,"Engine.PlayerPawn");};
        q.hasPlayer=[&](auto value) {
            auto* player=actor(value);
            const auto field=OriginalAnimationField(player,"Engine.PlayerPawn","Player",Kind::Object,2u,"ObjectProperty");
            auto* metadata=Object(field.key);
            if(!metadata->property->referencedType||
                LowerAscii(Qualified(Table(metadata->sourcePath),metadata->property->referencedType))!="engine.player")
                throw std::runtime_error("Original distance PlayerPawn.Player has another native constraint");
            if(!player->nativePlayerBinding)
                throw std::runtime_error("Original distance PlayerPawn.Player native storage is unbound");
            if(player->nativePlayer&&(!persistentNativeClient.HasClient()||
                player->nativePlayer->nativeClientIdentity!=persistentNativeClient.ClientIdentity()))
                throw std::runtime_error("Original distance Player pointer is revoked or stale");
            return player->nativePlayer!=nullptr;
        };
        q.firstDynamic=[&](auto value) {
            auto* native=owner(value);
            if(!native->nativeFirstDynamic)throw std::runtime_error("Original distance prepass lacks the actual LoadMap partition");
            return static_cast<std::int32_t>(*native->nativeFirstDynamic);
        };
        q.actorCount=[&](auto value) {
            static_cast<void>(owner(value));
            if(persistentWorldActors.size()>65'536u)throw std::runtime_error("Original distance Actor array exceeds policy");
            return static_cast<std::int32_t>(persistentWorldActors.size());
        };
        q.actorSlot=[&](auto value,std::int32_t index) {
            static_cast<void>(owner(value));
            if(index<0||static_cast<std::size_t>(index)>=persistentWorldActors.size())
                throw std::runtime_error("Original distance slot is outside the live native array");
            return reinterpret_cast<std::uintptr_t>(persistentWorldActors[static_cast<std::size_t>(index)]);
        };
        q.location=[&](auto value,unsigned channel) {
            if(channel>=3u)throw std::runtime_error("Original distance Location channel is invalid");
            return NativeCoordsRead(actor(value),"Engine.Actor.Location",Kind::Vector,0x23u).vector[channel];
        };
        q.writeDistance=[&](auto value,float distance) {
            auto* target=actor(value);
            const auto field=OriginalAnimationField(target,"Engine.Actor","DistanceFromPlayer",Kind::Float,0u,"FloatProperty");
            ReserveActorPhysicsReferences(field,1u);
            execution.WriteReference(ReferenceFor(target,field,0u,false),Value::Float(distance));
        };
        const auto stats=QuestVr::AdvanceOriginalLevelPlayerDistance(identity,q);
        result.completed=true;result.networkSkipped=stats.networkSkipped;result.noActor=stats.noActor;
        result.notPlayerPawn=stats.notPlayerPawn;result.noPlayer=stats.noPlayer;
        result.steps=stats.steps;result.visits=stats.visits;result.nullSlots=stats.nullSlots;result.writes=stats.writes;
        result.firstDynamic=stats.firstDynamic;result.capturedLocation=stats.capturedLocation;
        result.playerPath=stats.player?actor(stats.player)->reflection.objectPath:std::string{};
    }
