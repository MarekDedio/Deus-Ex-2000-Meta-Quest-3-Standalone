// Included in PortableActorVmHost: actual runtime identities/providers and the
// SAME VM root, not a second scene/renderer registry. Rollback-only entries are
// in portable_retail_collision.h; publishing startup/checkpoint work is separate.
    RuntimeObject* RetailCollisionLevel() {
        if(persistentWorldActors.empty() || !persistentWorldActors.front() || !persistentEngineCollision)
            throw std::runtime_error("Retail collision requires actual current Level/Engine/Cylinder preparation");
        auto* info=NativeLevelInfo(persistentWorldActors.front());auto* level=info->nativeXLevel;
        auto* engine=persistentEngineCollision->get()->engine;
        if(!level || level->nativeMapGeneration!=persistentMapGenerationCounter || !level->nativeLevelModel ||
            level->nativeLevelEngine!=engine || !engine->nativeEngineInstance || engine->nativeEngineInstance->currentLevel!=level)
            throw std::runtime_error("Retail collision has no actual current-generation Level.Engine binding");
        return level;
    }
    RuntimeObject* RetailCollisionActor(std::uintptr_t identity) {
        if(!identity || !persistentObjectLookup || !persistentObjectLookup->active)
            throw std::runtime_error("Retail collision actor requires actual object lookup authority");
        const auto* record=persistentObjectLookup->index.RecordForIdentity(identity);
        if(!record) throw std::runtime_error("Retail collision actor storage has a retired UObject identity");
        auto* actor=reinterpret_cast<RuntimeObject*>(identity);
        if(actor->lookupControl!=persistentObjectLookup || actor->objectIndex.Index()!=record->index ||
            actor->defaultOwner || !actor->cls || !IsDerivedFromPath(actor->cls,"Engine.Actor") ||
            actor->nativeXLevel!=RetailCollisionLevel())
            throw std::runtime_error("Retail collision actor storage belongs to another native Level generation");
        return actor;
    }
    Property RetailCollisionTagProperty(RuntimeObject* actor) {
        const auto property=DescribeProperty("Engine.Actor.CollisionTag");auto* field=Object(property.key);
        if(property.key!="Engine.Actor.CollisionTag" || !OwnsProperty(actor->cls,property) ||
            property.zero.kind!=Kind::Int || property.arrayDimension!=1u || property.flags!=0x2002u ||
            !field->property || field->property->type!="IntProperty" || field->property->baseField ||
            field->property->referencedType || field->outer!=Object("Engine.Actor") ||
            LowerAscii(Qualified(Table(field->sourcePath),Table(field->sourcePath).exports.at(field->exportIndex).ObjClass))!="core.intproperty")
            throw std::runtime_error("Retail collision tag lacks its original declared scalar Int schema");
        return property;
    }
    bool RetailCollisionFlag(RuntimeObject* actor,const char* key) {
        const auto property=DescribeProperty(key);auto* field=Object(property.key);
        if(property.key!=key || !OwnsProperty(actor->cls,property) || property.zero.kind!=Kind::Bool ||
            property.arrayDimension!=1u || !field->property || field->property->type!="BoolProperty" ||
            field->property->baseField || field->outer!=Object("Engine.Actor"))
            throw std::runtime_error("Retail collision flag lacks its original Actor Bool declaration");
        const auto value=Read(actor,property,0u);
        if(value.kind!=Kind::Bool) throw std::runtime_error("Retail collision live flag is not Bool");
        return value.boolean;
    }
    void TouchRetailCollision() {
        if(!transaction_) throw std::runtime_error("Retail collision requires its complete outer VM transaction");
        if(retailCollisionRoot_) return;
        if(!persistentRetailCollision) {
            // appRand uses the existing shared CRT stream; absent means its
            // established unconsumed seed1 baseline, never a new per-map seed.
            auto seed=persistentScriptRandomSeed.value_or(1u);
            for(std::size_t i=0u;i<QuestVr::OriginalCollisionHashInitializationDraws;++i) LifecycleStep();
            auto tables=QuestVr::InitializeOriginalCollisionHashTables(seed,QuestVr::OriginalCollisionHashInitializationDraws);
            auto staged=std::make_unique<QuestVr::OriginalCollisionHashWorld>(std::move(tables));
            ValidateStateBudget(QuestVr::DecalStateDetail::Add(staged->RetainedBytes(),staged->AdditionalRootBytes()));
            auto root=staged->BeginRoot();
            persistentRetailCollision=std::move(staged);retailCollisionRoot_=std::move(root);
            createdRetailCollision_=true;persistentScriptRandomSeed=seed;
        }else {
            ValidateStateBudget(persistentRetailCollision->AdditionalRootBytes());
            retailCollisionRoot_=persistentRetailCollision->BeginRoot();
        }
        ValidateStateBudget();
    }
    void DefineRetailCollisionActor(RuntimeObject* actor) {
        const auto identity=reinterpret_cast<std::uintptr_t>(actor);
        static_cast<void>(RetailCollisionActor(identity));
        if(persistentRetailCollision->ContainsActor(identity)) return;
        const auto tagProperty=RetailCollisionTagProperty(actor);
        // Native transient query history cannot be supplied by a generic saved
        // actor/default/frozen overlay. Persistent ColLocation is separate.
        const auto tagKey=InstanceStorageKey(actor->cls,tagProperty);
        for(const auto* values:{&actor->scriptValues,&actor->frozenBirthDefaults})
            if(values->count(tagKey)) throw std::runtime_error("Retail collision refuses generic transient CollisionTag overlays");
        for(auto* cls=actor->cls;cls;cls=cls->base)
            if(cls->classDefaultValues.count(LowerAscii(tagProperty.name)) &&
                ClassProperty(cls,tagProperty.name).key==tagProperty.key)
                throw std::runtime_error("Retail collision refuses mutable class-default CollisionTag history");
        NativeAuthoredReadOrigin origin;
        const auto tag=Read(actor,tagProperty,0u,false,false,&origin);
        if(origin.tag)
            throw std::runtime_error("Retail collision refuses serialized transient CollisionTag history");
        const auto location=NativeCoordsRead(actor,"Engine.Actor.Location",Kind::Vector,0x23u);
        const auto colLocation=NativeCoordsRead(actor,"Engine.Actor.ColLocation",Kind::Vector,0x2u);
        if(tag.kind!=Kind::Int) throw std::runtime_error("Retail collision initial native tag is not Int");
        persistentRetailCollision->DefineActor({identity,
            {location.vector[0u],location.vector[1u],location.vector[2u]},
            {colLocation.vector[0u],colLocation.vector[1u],colLocation.vector[2u]},
            RetailCollisionFlag(actor,"Engine.Actor.bCollideActors"),RetailCollisionFlag(actor,"Engine.Actor.bDeleteMe"),
            std::bit_cast<std::uint32_t>(tag.integer)});
    }
    void RefreshRetailCollisionActor(RuntimeObject* actor) {
        DefineRetailCollisionActor(actor);
        const auto location=NativeCoordsRead(actor,"Engine.Actor.Location",Kind::Vector,0x23u);
        persistentRetailCollision->SetActorLive(reinterpret_cast<std::uintptr_t>(actor),
            {location.vector[0u],location.vector[1u],location.vector[2u]},
            RetailCollisionFlag(actor,"Engine.Actor.bCollideActors"),RetailCollisionFlag(actor,"Engine.Actor.bDeleteMe"));
    }
    void RequireNoActiveRetailMutation(RuntimeObject* actor,const char* operation) {
        if(actor && actor->nativeXLevel && actor->nativeXLevel->nativeRetailHash)
            throw std::runtime_error(std::string("Unsupported original active-hash ")+operation+" contract");
    }
    void ReserveRetailDiagnosticOutput(std::size_t bytes,std::size_t scratch=0u) {
        constexpr std::size_t maximum=8u<<20u;
        if(bytes>maximum || retailCollisionOutputBytes_>maximum-bytes)
            throw std::runtime_error("Retail collision detached diagnostic output exceeds8MiB");
        ValidateStateBudget(QuestVr::DecalStateDetail::Add(bytes,scratch));
        retailCollisionOutputBytes_+=bytes;
    }
    QuestVr::OriginalHashQueries RetailCollisionQueries() {
        return {[this](std::uintptr_t identity) {
            LifecycleStep();return NativeActorPrimitiveBounds(RetailCollisionActor(identity));
        },[this](QuestVr::OriginalActorLineHit& hit,std::uintptr_t identity,const PortableModelVec3& end,
            const PortableModelVec3& start,const PortableModelVec3& extent,std::uint32_t flags) {
            LifecycleStep();return NativeActorPrimitiveLineCheck(RetailCollisionActor(identity),hit,end,start,extent,flags);
        }};
    }
    void SetRetailActorCollision(bool enabled) {
        auto* level=RetailCollisionLevel();
        if(level->nativeRetailHash && (!persistentRetailCollision ||
            !persistentRetailCollision->ContainsHash(level->nativeRetailHash)))
            throw std::runtime_error("Retail collision Level.Hash lost its actual owned storage");
        if(enabled==bool(level->nativeRetailHash)) return; // Original two no-op cases.
        TouchRetailCollision();Touch(level);
        if(enabled) level->nativeRetailHash=persistentRetailCollision->CreateHash();
        if(!level->nativeRetailHash || !persistentRetailCollision->ContainsHash(level->nativeRetailHash))
            throw std::runtime_error("Retail collision Level.Hash lost its actual owned storage");
        const auto queries=RetailCollisionQueries();
        // Reread live count and each ascending slot. No block/hidden/class
        // filter replaces the original nonnull + bCollideActors predicate.
        for(std::size_t slot=0u;slot<persistentWorldActors.size();++slot) {
            LifecycleStep();auto* actor=persistentWorldActors[slot];if(!actor) continue;
            DefineRetailCollisionActor(actor);
            if(!RetailCollisionFlag(actor,"Engine.Actor.bCollideActors")) continue;
            RefreshRetailCollisionActor(actor);
            if(enabled) persistentRetailCollision->Add(level->nativeRetailHash,reinterpret_cast<std::uintptr_t>(actor),queries);
            else persistentRetailCollision->Remove(level->nativeRetailHash,reinterpret_cast<std::uintptr_t>(actor),queries);
        }
        if(!enabled) {
            persistentRetailCollision->DestroyHash(level->nativeRetailHash);level->nativeRetailHash=0u;
        }
        ValidateStateBudget();
    }
    PortableRetailCollisionSnapshot RetailCollisionSnapshot() {
        PortableRetailCollisionSnapshot result;result.seed=persistentScriptRandomSeed.value_or(1u);
        if(!persistentRetailCollision) return result;
        auto* level=RetailCollisionLevel();const auto usage=persistentRetailCollision->ReadUsage();
        result.initialized=true;result.initializedHere=createdRetailCollision_;result.enabled=level->nativeRetailHash!=0u;
        result.levelPath=level->reflection.objectPath;result.mapGeneration=level->nativeMapGeneration;
        result.hashIdentity=level->nativeRetailHash;result.tag=usage.tag;result.hashes=usage.hashes;
        result.allocatedLinks=usage.allocatedLinks;result.activeLinks=usage.activeLinks;
        result.retainedBytes=persistentRetailCollision->RetainedBytes();
        auto bytes=QuestVr::DecalStateDetail::Add(sizeof(result)+level->reflection.objectPath.size()+1u,
            QuestVr::DecalStateDetail::Multiply(usage.actors,sizeof(PortableRetailCollisionActor)));
        for(auto* actor:persistentWorldActors) if(actor &&
            persistentRetailCollision->ContainsActor(reinterpret_cast<std::uintptr_t>(actor)))
            bytes=QuestVr::DecalStateDetail::Add(bytes,actor->reflection.objectPath.size()+1u+4u*sizeof(void*));
        ReserveRetailDiagnosticOutput(bytes);
        result.actors.reserve(usage.actors);
        for(auto* actor:persistentWorldActors) {
            if(!actor || !persistentRetailCollision->ContainsActor(reinterpret_cast<std::uintptr_t>(actor))) continue;
            const auto value=persistentRetailCollision->ReadActor(reinterpret_cast<std::uintptr_t>(actor));
            const auto declaredTag=Read(actor,RetailCollisionTagProperty(actor),0u);
            const auto declaredCol=NativeCoordsRead(actor,"Engine.Actor.ColLocation",Kind::Vector,0x2u);
            const bool mirrors=declaredTag.kind==Kind::Int &&
                std::bit_cast<std::uint32_t>(declaredTag.integer)==value.collisionTag &&
                declaredCol.vector==std::array<float,3u>{value.colLocation.x,value.colLocation.y,value.colLocation.z};
            result.actors.push_back({actor->reflection.objectPath,actor->objectIndex.Index(),value.collisionTag,
                {value.location.x,value.location.y,value.location.z},{value.colLocation.x,value.colLocation.y,value.colLocation.z},
                value.collideActors,value.pendingDelete,mirrors});
        }
        return result;
    }
    Value ProbeRetailCollision(const std::vector<PortableRetailCollisionCommand>& commands,
        PortableRetailCollisionProbeResult& result) {
        if(commands.empty() || commands.size()>16u) throw std::runtime_error("Retail collision diagnostic requires one to sixteen commands");
        retailCollisionPreview_=true;
        ReserveRetailDiagnosticOutput(QuestVr::DecalStateDetail::Multiply(commands.size(),
            sizeof(PortableRetailCollisionSnapshot)+sizeof(std::vector<PortableRetailCollisionHit>)+sizeof(std::size_t)));
        result.states.reserve(commands.size());result.queries.reserve(commands.size());result.queryHeads.reserve(commands.size());
        for(const auto& command:commands) {
            if(command.operation==PortableRetailCollisionOperation::Enable) SetRetailActorCollision(true);
            else if(command.operation==PortableRetailCollisionOperation::Disable) SetRetailActorCollision(false);
            else if(command.operation==PortableRetailCollisionOperation::LineCheck) {
                auto* level=RetailCollisionLevel();
                if(!level->nativeRetailHash || !persistentRetailCollision)
                    throw std::runtime_error("Retail collision query requires an actual enabled Level.Hash");
                TouchRetailCollision();
                // Genuine global tags deduplicate each actor in this query.
                // Preflight vector growth before producing the native arena.
                ValidateStateBudget(QuestVr::DecalStateDetail::Multiply(persistentRetailCollision->ReadUsage().actors,
                    2u*sizeof(QuestVr::OriginalActorLineHit)));
                const auto vector=[](const auto& value) {return PortableModelVec3{value[0u],value[1u],value[2u]};};
                const auto hits=persistentRetailCollision->ActorLineCheck(level->nativeRetailHash,vector(command.end),
                    vector(command.start),vector(command.extent),command.flags,RetailCollisionQueries());
                const auto path=[&](std::uintptr_t identity) {
                    if(!identity) return std::string{};
                    if(!persistentObjectLookup || !persistentObjectLookup->index.RecordForIdentity(identity))
                        throw std::runtime_error("Retail collision result contains an unowned actual object identity");
                    return reinterpret_cast<RuntimeObject*>(identity)->reflection.objectPath;
                };
                auto bytes=QuestVr::DecalStateDetail::Multiply(hits.records.size(),sizeof(PortableRetailCollisionHit));
                for(const auto& hit:hits.records) for(const auto identity:{hit.actor,hit.primitive}) {
                    if(!identity) continue;
                    if(!persistentObjectLookup || !persistentObjectLookup->index.RecordForIdentity(identity))
                        throw std::runtime_error("Retail collision output contains a retired identity");
                    bytes=QuestVr::DecalStateDetail::Add(bytes,
                        reinterpret_cast<RuntimeObject*>(identity)->reflection.objectPath.size()+1u+4u*sizeof(void*));
                }
                ReserveRetailDiagnosticOutput(bytes,QuestVr::DecalStateDetail::Multiply(
                    hits.records.capacity(),sizeof(QuestVr::OriginalActorLineHit)));
                std::vector<PortableRetailCollisionHit> converted;converted.reserve(hits.records.size());
                for(const auto& hit:hits.records) converted.push_back({path(hit.actor),path(hit.primitive),hit.next,
                    {hit.location.x,hit.location.y,hit.location.z},{hit.normal.x,hit.normal.y,hit.normal.z},hit.time,hit.item});
                result.queries.push_back(std::move(converted));result.queryHeads.push_back(hits.first);
            }else throw std::runtime_error("Unknown retail collision diagnostic operation");
            ValidateStateBudget();result.states.push_back(RetailCollisionSnapshot());
        }
        return Value::Bool(true);
    }
