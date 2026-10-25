// Included inside PortableActorVmHost. Borrow the caller's root and actual
// loader-owned native edges, not a renderer visibility/Region facade.
    void StoreOriginalActorLastRenderTime(RuntimeObject* actor,float value) {
        // Native assignment and script Let share one actual Float field. Keep
        // its existing saved-property mirror, without inventing a wire field
        // or charging a native raw store as a bytecode Let instruction.
        const auto field=OriginalAnimationField(actor,"Engine.Actor","LastRenderTime",Kind::Float,0u,"FloatProperty");
        ReferenceFor(actor,field,0u,false)->write(Value::Float(value));
    }
    QuestVr::OriginalActorStasisStats OriginalActorStasis(RuntimeObject* actor) {
        if(!transaction_)throw std::runtime_error("Original InStasis requires its caller transaction");
        actor=LifecycleActor(reinterpret_cast<std::uintptr_t>(actor));
        auto* const level=ZoneLevel(reinterpret_cast<std::uintptr_t>(actor->nativeXLevel));
        const auto source=reinterpret_cast<std::uintptr_t>(actor);
        const auto receiver=[&](std::uintptr_t identity) {
            if(identity!=source)throw std::runtime_error("Original InStasis changed its retained Actor");
            auto* live=LifecycleActor(identity);
            if(live->nativeXLevel!=level)throw std::runtime_error("Original InStasis changed native XLevel");
            static_cast<void>(ZoneLevel(reinterpret_cast<std::uintptr_t>(level)));
            return live;
        };
        const auto scalar=[&](RuntimeObject* live,const char* owner,const char* name,
            Kind kind,std::uint32_t flags,const char* type) {
            return OriginalAnimationRead(live,OriginalAnimationField(live,owner,name,kind,flags,type),kind);
        };
        QuestVr::OriginalActorStasisQueries q;q.work=[this]{LifecycleStep();};
        q.lastRenderTime=[&](auto identity) {
            auto* live=receiver(identity);
            static_cast<void>(OriginalAnimationField(live,"Engine.Actor","LastRenderTime",Kind::Float,0u,"FloatProperty"));
            if(!live->lastRenderTime)
                throw std::runtime_error("Original InStasis native Actor.LastRenderTime is not initialized");
            return *live->lastRenderTime;
        };
        q.nativeTime=[&](auto identity) {
            static_cast<void>(receiver(identity));
            if(!level->nativeTimeSeconds)throw std::runtime_error("Original InStasis has no native ULevel Double clock");
            return *level->nativeTimeSeconds;
        };
        q.stasisFlags=[&](auto identity) {
            auto* live=receiver(identity);
            const bool stasis=scalar(live,"Engine.Actor","bStasis",Kind::Bool,1u,"BoolProperty").boolean;
            const bool force=scalar(live,"Engine.Actor","bForceStasis",Kind::Bool,1u,"BoolProperty").boolean;
            return (stasis?0x00200000u:0u)|(force?0x00400000u:0u);
        };
        q.physics=[&](auto identity) {
            auto* live=receiver(identity);const auto field=ActorPhysicsProperty(live);
            const auto value=Read(live,field,0u);
            if(value.kind!=Kind::Byte)throw std::runtime_error("Original InStasis Physics has changed storage kind");
            return static_cast<std::uint8_t>(value.integer);
        };
        q.zoneNumber=[&](auto identity) {
            auto* live=receiver(identity);
            const auto region=ZoneStoredRegion(live,ZoneRegionProperty(live,QuestVr::OriginalActorRegionSlot::Main));
            return static_cast<std::uint8_t>(region.fields.at("zonenumber").integer);
        };
        q.zoneLastRenderTime=[&](auto identity,std::uint8_t zone) {
            static_cast<void>(receiver(identity));
            auto* model=level->nativeLevelModel;
            ValidateNativeLevelModel(level,model);
            if(!model->nativeModelZoneRenderTimes||zone>=model->nativeModelZoneRenderTimes->size())
                throw std::runtime_error("Original InStasis has no bound native Model zone timestamp");
            return (*model->nativeModelZoneRenderTimes)[zone];
        };
        q.distanceFromPlayer=[&](auto identity) {
            return scalar(receiver(identity),"Engine.Actor","DistanceFromPlayer",Kind::Float,0u,"FloatProperty").floating;
        };
        q.netMode=[&](auto identity) {
            auto* live=receiver(identity);
            const auto pointer=OriginalAnimationField(live,"Engine.Actor","Level",Kind::Object,2u,"ObjectProperty");
            auto* field=Object(pointer.key);
            if(!field->property->referencedType||
                Qualified(Table(field->sourcePath),field->property->referencedType)!="Engine.LevelInfo")
                throw std::runtime_error("Original InStasis Actor.Level has a different LevelInfo constraint");
            const auto value=OriginalAnimationRead(live,pointer,Kind::Object);
            if(value.text.empty())throw std::runtime_error("Original InStasis reached a null actual Actor.Level");
            auto* info=Object(value.text);
            if(!IsDerivedFromPath(info->cls,"Engine.LevelInfo"))
                throw std::runtime_error("Original InStasis actual Actor.Level is not LevelInfo");
            const auto mode=OriginalAnimationField(info,"Engine.LevelInfo","NetMode",Kind::Byte,0u,"ByteProperty");
            auto* modeField=Object(mode.key);
            if(!modeField->property->referencedType||
                Qualified(Table(modeField->sourcePath),modeField->property->referencedType)!="Engine.LevelInfo.ENetMode")
                throw std::runtime_error("Original InStasis NetMode has a different native enum constraint");
            return static_cast<std::uint8_t>(OriginalAnimationRead(info,mode,Kind::Byte).integer);
        };
        return QuestVr::QueryOriginalActorStasis(source,q);
    }
    PortableActorStasisObservation ObserveOriginalActorStasis(const std::string& path) {
        auto* actor=Object(path);
        const auto stats=OriginalActorStasis(actor);
        return {actor->reflection.objectPath,true,stats.inStasis,stats.physicsRead,stats.zoneRead,
            stats.distanceRead,stats.netModeRead,stats.steps,stats.nativeTimeReads};
    }
