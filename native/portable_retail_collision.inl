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
            // Fixed, input-independent table construction has its own draw
            // budget, not49152 actor/iterator traversal operations. Reserve
            // its entire allowance once in this outer root; never reset it
            // between native calls or on a failed creation attempt.
            LifecycleStep();
            if(retailCollisionInitializationReserved_)
                throw std::runtime_error("Retail collision initialization allowance already consumed in this root");
            retailCollisionInitializationReserved_=true;
            auto tables=QuestVr::InitializeOriginalCollisionHashTables(seed,QuestVr::OriginalCollisionHashInitializationDraws);
            retailCollisionInitializationDraws_=QuestVr::OriginalCollisionHashInitializationDraws;
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
    std::shared_ptr<const Function> RetailCollisionSetterDeclaration(std::uint16_t index,const Function* declaration) {
        if(index!=262u && index!=283u) throw std::runtime_error("Unknown original collision setter");
        const std::string path=index==262u?"Engine.Actor.SetCollision":"Engine.Actor.SetCollisionSize";
        const auto source=FunctionFor(Object(path));
        const auto validate=[&](const Function& function) {
            if(function.path!=path || function.nativeIndex!=index || function.flags!=0x401u || function.variables.size()!=3u)
                throw std::runtime_error("Collision setter lacks its actual original native declaration");
            const std::array<const char*,3u> names=index==262u?
                std::array<const char*,3u>{"NewColActors","NewBlockActors","NewBlockPlayers"}:
                std::array<const char*,3u>{"NewRadius","NewHeight","ReturnValue"};
            for(std::size_t slot=0u;slot<3u;++slot) {
                const auto& property=function.variables[slot];
                const auto kind=index==262u || slot==2u?Kind::Bool:Kind::Float;
                const auto flags=index==262u?0x90u:slot==2u?0x580u:0x80u;
                if(property.key!=path+'.'+names[slot] || property.name!=names[slot] || property.arrayDimension!=1u ||
                    property.zero.kind!=kind || property.flags!=flags)
                    throw std::runtime_error("Collision setter has a different original scalar parameter schema");
            }
        };
        validate(*source);if(declaration) validate(*declaration);return source;
    }
    Property RetailCollisionSetterField(RuntimeObject* actor,const char* key) {
        const auto property=DescribeProperty(key);auto* field=Object(property.key);
        const bool dimension=std::strcmp(key,"Engine.Actor.CollisionRadius")==0 || std::strcmp(key,"Engine.Actor.CollisionHeight")==0;
        const auto kind=dimension?Kind::Float:Kind::Bool;
        const auto flags=dimension || std::strcmp(key,"Engine.Actor.bCollideActors")==0?0x23u:0x21u;
        const auto type=dimension?"FloatProperty":"BoolProperty";
        if(property.key!=key || !OwnsProperty(actor->cls,property) || property.zero.kind!=kind ||
            property.arrayDimension!=1u || property.flags!=flags || !field->property || field->property->type!=type ||
            field->property->baseField || field->property->referencedType || field->outer!=Object("Engine.Actor") ||
            LowerAscii(Qualified(Table(field->sourcePath),Table(field->sourcePath).exports.at(field->exportIndex).ObjClass))!=
                std::string("core.")+LowerAscii(type))
            throw std::runtime_error("Collision setter field lacks its original Actor scalar declaration");
        return property;
    }
    void WriteRetailCollisionSetterField(RuntimeObject* actor,const char* key,const Value& value) {
        // Narrow original-native authority only; ordinary bytecode retains the
        // active-hash shape-write guard. Hash Remove/write/Add share this root.
        const auto property=RetailCollisionSetterField(actor,key);
        auto normalized=TypedLiveValue(property,value);Touch(actor);
        actor->scriptValues[InstanceStorageKey(actor->cls,property)][0u]=std::move(normalized);
        actor->committedScriptState=true;
    }
    Evaluation RetailCollisionSetterNative(std::uint16_t index,RuntimeObject* actor,
        const std::vector<Evaluation>& arguments,const Function* declaration) {
        static_cast<void>(RetailCollisionSetterDeclaration(index,declaration));
        static_cast<void>(RetailCollisionActor(reinterpret_cast<std::uintptr_t>(actor)));
        if((index==262u && arguments.size()>3u) || (index==283u && arguments.size()!=2u))
            throw std::runtime_error("Collision setter original argument count is invalid");
        std::array<Value,3u> values;
        static constexpr std::array<const char*,3u> keys{
            "Engine.Actor.bCollideActors","Engine.Actor.bBlockActors","Engine.Actor.bBlockPlayers"};
        for(std::size_t slot=0u;slot<(index==262u?3u:2u);++slot) {
            auto value=slot<arguments.size()?arguments[slot].Load():Value{};
            if(index==262u) {
                if(value.kind==Kind::Nothing) value=Value::Bool(RetailCollisionFlag(actor,keys[slot]));
                if(value.kind!=Kind::Bool) throw std::runtime_error("SetCollision input is not its original Bool");
            }else {
                if(value.kind!=Kind::Float && value.kind!=Kind::Byte && value.kind!=Kind::Int)
                    throw std::runtime_error("SetCollisionSize input is not numeric");
                value=Value::Float(QuestVr::Vm::ToFloat(value));
                if(!std::isfinite(value.floating))
                    throw std::runtime_error("SetCollisionSize unsupported portable nonfinite dimension");
            }
            values[slot]=value;
        }
        TouchRetailCollision();DefineRetailCollisionActor(actor);
        auto* level=RetailCollisionLevel();const auto identity=reinterpret_cast<std::uintptr_t>(actor);
        if(level->nativeRetailHash && RetailCollisionFlag(actor,"Engine.Actor.bCollideActors")) {
            RefreshRetailCollisionActor(actor);persistentRetailCollision->Remove(level->nativeRetailHash,identity,RetailCollisionQueries());
        }
        // No equality shortcut: unchanged setters still remove and prepend.
        // Size samples its collide bit after removal, before the field writes.
        const bool sizeCollides=index==283u && RetailCollisionFlag(actor,"Engine.Actor.bCollideActors");
        if(index==262u) for(std::size_t slot=0u;slot<3u;++slot) WriteRetailCollisionSetterField(actor,keys[slot],values[slot]);
        else {
            WriteRetailCollisionSetterField(actor,"Engine.Actor.CollisionRadius",values[0u]);
            WriteRetailCollisionSetterField(actor,"Engine.Actor.CollisionHeight",values[1u]);
        }
        RefreshRetailCollisionActor(actor);
        if(level->nativeRetailHash && (index==262u?RetailCollisionFlag(actor,"Engine.Actor.bCollideActors"):sizeCollides))
            persistentRetailCollision->Add(level->nativeRetailHash,identity,RetailCollisionQueries());
        ValidateStateBudget();
        return index==283u?Evaluation{Value::Bool(true),{}}:
            Evaluation{Value{},{},QuestVr::Vm::ResultDisposition::NoWrite};
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
        result.initializationDraws=retailCollisionInitializationDraws_;
        result.lifecycleOperations=nativeWork_.Read().used;
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
        PortableRetailCollisionProbeResult& result,QuestVr::Vm::Execution& execution) {
        if(commands.empty() || commands.size()>16u) throw std::runtime_error("Retail collision diagnostic requires one to sixteen commands");
        retailCollisionPreview_=true;
        ReserveRetailDiagnosticOutput(QuestVr::DecalStateDetail::Multiply(commands.size(),
            sizeof(PortableRetailCollisionSnapshot)+sizeof(std::vector<PortableRetailCollisionHit>)+sizeof(std::size_t)+
                sizeof(PortableRetailCollisionMutation)+sizeof(PortableRetailMultiLineMetadata)+sizeof(PortableRetailActorPredicate)+
                sizeof(PortableRetailTouchSnapshot)+sizeof(PortableRetailZoneSnapshot)+sizeof(PortableRetailMoveSnapshot)+
                sizeof(PortableRetailRawShapeSnapshot)+sizeof(PortableRetailSingleLineSnapshot)+sizeof(PortableRetailBaseSnapshot)+
                sizeof(PortableRetailFindBaseSnapshot)));
        result.states.reserve(commands.size());result.queries.reserve(commands.size());result.queryHeads.reserve(commands.size());
        result.mutations.reserve(commands.size());result.multiLine.reserve(commands.size());
        result.predicates.reserve(commands.size());
        result.touches.reserve(commands.size());
        result.zones.reserve(commands.size());
        result.moves.reserve(commands.size());
        result.rawShapes.reserve(commands.size());
        result.singles.reserve(commands.size());
        result.bases.reserve(commands.size());
        result.findBases.reserve(commands.size());
        for(const auto& command:commands) {
            if(command.operation==PortableRetailCollisionOperation::Enable) SetRetailActorCollision(true);
            else if(command.operation==PortableRetailCollisionOperation::Disable) SetRetailActorCollision(false);
            else if(command.operation==PortableRetailCollisionOperation::SingleLineCheck) {
                if(command.actorPath.size()>4096u||command.actorPath.find('\0')!=std::string::npos||!command.arguments.empty())
                    throw std::runtime_error("SingleLine diagnostic requires a bounded optional Actor source and no arguments");
                auto* level=RetailCollisionLevel();
                auto* source=command.actorPath.empty()?nullptr:RetailCollisionActor(reinterpret_cast<std::uintptr_t>(Object(command.actorPath)));
                QuestVr::OriginalActorLineHit packet{0u,0u,0u,{11.0f,12.0f,13.0f},{-1.0f,0.0f,0.0f},0.375f,127};
                const auto vector=[](const auto& value){return PortableModelVec3{value[0u],value[1u],value[2u]};};
                const auto reply=RetailSingleLineLevel(level,source,packet,vector(command.end),vector(command.start),
                    vector(command.extent),command.flags,command.singleExtraFlags);
                const auto path=[&](std::uintptr_t identity)->std::string {
                    if(!identity)return {};
                    if(!persistentObjectLookup||!persistentObjectLookup->index.RecordForIdentity(identity))
                        throw std::runtime_error("SingleLine diagnostic selected a retired native object");
                    return reinterpret_cast<RuntimeObject*>(identity)->reflection.objectPath;
                };
                // Both identities are authenticated by the same-root adapter;
                // reserve detached string storage before their owning copies.
                std::size_t bytes=2u+8u*sizeof(void*);
                for(const auto identity:{packet.actor,packet.primitive}) {
                    if(!identity)continue;
                    if(!persistentObjectLookup||!persistentObjectLookup->index.RecordForIdentity(identity))
                        throw std::runtime_error("SingleLine diagnostic lost its selected native object");
                    bytes=QuestVr::DecalStateDetail::Add(bytes,reinterpret_cast<RuntimeObject*>(identity)->reflection.objectPath.size());
                }
                ReserveRetailDiagnosticOutput(bytes);
                result.singles.push_back({result.states.size(),reply.clear,packet.next!=0u,reply.worldItemKnown,
                    {path(packet.actor),path(packet.primitive),0u,{packet.location.x,packet.location.y,packet.location.z},
                        {packet.normal.x,packet.normal.y,packet.normal.z},packet.time,packet.item},
                    nativeMoveHits_?nativeMoveHits_->MarkDepth():0u,nativeMoveHits_?nativeMoveHits_->ArenaCount():0u,reply.stats});
            }
            else if(command.operation==PortableRetailCollisionOperation::ActorFindBase) {
                if(command.actorPath.empty() || command.actorPath.size()>4096u ||
                    command.actorPath.find('\0')!=std::string::npos || !command.arguments.empty())
                    throw std::runtime_error("Original FindBase diagnostic requires one bounded Actor receiver without arguments");
                auto* actor=RetailCollisionActor(reinterpret_cast<std::uintptr_t>(Object(command.actorPath)));
                PortableRetailFindBaseSnapshot snapshot;snapshot.commandIndex=result.states.size();
                const auto before=RetailPredicatePointerRead(actor,"Engine.Actor.Base","Engine.Actor",0x22u);
                ReserveRetailDiagnosticOutput(actor->reflection.objectPath.size()+before.text.size()+2u);
                snapshot.actorPath=actor->reflection.objectPath;snapshot.baseBefore=before.text;
                snapshot.clockBefore=ZoneClockSnapshot(actor);
                const auto reply=RetailFindBase(actor,execution);
                const auto after=RetailPredicatePointerRead(actor,"Engine.Actor.Base","Engine.Actor",0x22u);
                std::size_t bytes=after.text.size()+5u;
                for(auto* object:{reply.hitActor,reply.baseAtDecision,reply.selectedSupportFunction})
                    if(object)bytes=QuestVr::DecalStateDetail::Add(bytes,object->reflection.objectPath.size());
                ReserveRetailDiagnosticOutput(bytes);
                snapshot.baseAfter=after.text;
                if(reply.hitActor)snapshot.hitActor=reply.hitActor->reflection.objectPath;
                if(reply.baseAtDecision)snapshot.baseAtDecision=reply.baseAtDecision->reflection.objectPath;
                if(reply.selectedSupportFunction)snapshot.selectedSupportFunction=reply.selectedSupportFunction->reflection.objectPath;
                const auto vector=[](PortableModelVec3 value){return std::array<float,3u>{value.x,value.y,value.z};};
                snapshot.start=vector(reply.start);snapshot.end=vector(reply.end);snapshot.extent=vector(reply.extent);
                snapshot.clear=reply.line.clear;snapshot.unchanged=reply.unchanged;snapshot.basedOnSelf=reply.basedOnSelf;
                snapshot.supportAttempted=reply.supportAttempted;snapshot.marksBefore=reply.marksBefore;
                snapshot.marksAfterQuery=reply.marksAfterQuery;snapshot.arenasBefore=reply.arenasBefore;
                snapshot.arenasAfterQuery=reply.arenasAfterQuery;snapshot.clockAfter=ZoneClockSnapshot(actor);
                result.findBases.push_back(std::move(snapshot));
            }
            else if(command.operation==PortableRetailCollisionOperation::ActorSetBase) {
                if(command.actorPath.empty() || command.actorPath.size()>4096u ||
                    command.actorPath.find('\0')!=std::string::npos || command.arguments.size()!=1u)
                    throw std::runtime_error("Original SetBase diagnostic requires one bounded Actor receiver and target");
                const auto& value=command.arguments.front();
                if(value.kind!=Kind::Object || value.text.size()>4096u || value.text.find('\0')!=std::string::npos ||
                    !value.intArray.empty() || !value.fields.empty())
                    throw std::runtime_error("Original SetBase diagnostic target is not a bounded Actor or None");
                auto* actor=RetailCollisionActor(reinterpret_cast<std::uintptr_t>(Object(command.actorPath)));
                auto* target=value.text.empty()?nullptr:RetailCollisionActor(reinterpret_cast<std::uintptr_t>(Object(value.text)));
                const auto original=RetailPredicatePointerRead(actor,"Engine.Actor.Base","Engine.Actor",0x22u);
                auto* old=original.text.empty()?nullptr:RetailCollisionActor(reinterpret_cast<std::uintptr_t>(Object(original.text)));
                const std::array<RuntimeObject*,3u> tracked{actor,old,target};
                const auto fields=[this](RuntimeObject* object) {
                    PortableRetailBaseFields snapshot;
                    if(!object)return snapshot;
                    const auto base=RetailPredicatePointerRead(object,"Engine.Actor.Base","Engine.Actor",0x22u);
                    ReserveRetailDiagnosticOutput(object->reflection.objectPath.size()+base.text.size()+2u+8u*sizeof(void*));
                    snapshot.actorPath=object->reflection.objectPath;snapshot.basePath=base.text;
                    snapshot.standingCount=ActorBaseCount(object);return snapshot;
                };
                PortableRetailBaseSnapshot snapshot;snapshot.commandIndex=result.states.size();
                for(std::size_t i=0u;i<tracked.size();++i)snapshot.before[i]=fields(tracked[i]);
                snapshot.clockBefore=ZoneClockSnapshot(actor);
                const auto function=FunctionFor(Object("Engine.Actor.SetBase"));
                static_cast<void>(execution.CallFunction(*function,command.actorPath,{{value,{}}}));
                for(std::size_t i=0u;i<tracked.size();++i)snapshot.after[i]=fields(tracked[i]);
                snapshot.clockAfter=ZoneClockSnapshot(actor);
                result.bases.push_back(std::move(snapshot));
            }
            else if(command.operation==PortableRetailCollisionOperation::RawMeshWrite ||
                command.operation==PortableRetailCollisionOperation::RawDrawScaleWrite ||
                command.operation==PortableRetailCollisionOperation::OriginalBecomePickup) {
                const bool pickup=command.operation==PortableRetailCollisionOperation::OriginalBecomePickup;
                if(command.actorPath.empty()||command.actorPath.size()>4096u||command.actorPath.find('\0')!=std::string::npos||
                    command.arguments.size()!=(pickup?0u:1u))
                    throw std::runtime_error("Raw Actor shape diagnostic requires bounded original inputs");
                auto* actor=RetailCollisionActor(reinterpret_cast<std::uintptr_t>(Object(command.actorPath)));
                if(!pickup) {
                    const auto& value=command.arguments.front();
                    if(value.text.size()>4096u||value.text.find('\0')!=std::string::npos||
                        !value.intArray.empty()||!value.fields.empty())
                        throw std::runtime_error("Raw Actor shape diagnostic input is not a bounded scalar");
                }
                auto clockBefore=ZoneClockSnapshot(actor);
                if(pickup) {
                    if(!IsDerivedFromPath(actor->cls,"Engine.Inventory"))
                        throw std::runtime_error("Original BecomePickup requires an actual Inventory receiver");
                    const auto function=FunctionFor(Object("Engine.Inventory.BecomePickup"));
                    static_cast<void>(execution.CallFunction(*function,command.actorPath,{}));
                }else {
                    const auto property=DescribeProperty(command.operation==PortableRetailCollisionOperation::RawMeshWrite?
                        "Engine.Actor.Mesh":"Engine.Actor.DrawScale");
                    execution.WriteReference(ReferenceFor(actor,property,0u,false),command.arguments.front());
                }
                const auto mesh=Read(actor,DescribeProperty("Engine.Actor.Mesh"),0u);
                const auto scale=Read(actor,DescribeProperty("Engine.Actor.DrawScale"),0u);
                if(mesh.kind!=Kind::Object||scale.kind!=Kind::Float)
                    throw std::runtime_error("Raw Actor shape diagnostic lost its scalar field schema");
                const auto selected=NativeActorPrimitive(actor);
                std::size_t bytes=actor->reflection.objectPath.size()+mesh.text.size()+
                    selected.object->reflection.objectPath.size()+selected.nativeClassPath.size()+4u+16u*sizeof(void*);
                ReserveRetailDiagnosticOutput(bytes);
                result.rawShapes.push_back({result.states.size(),actor->reflection.objectPath,mesh.text,
                    selected.object->reflection.objectPath,selected.nativeClassPath,scale.floating,
                    std::move(clockBefore),ZoneClockSnapshot(actor)});
            }
            else if(command.operation==PortableRetailCollisionOperation::LevelMoveActor||
                command.operation==PortableRetailCollisionOperation::ActorMove||
                command.operation==PortableRetailCollisionOperation::ActorSetRotation||
                command.operation==PortableRetailCollisionOperation::ActorMoveSmooth||
                command.operation==PortableRetailCollisionOperation::LevelCheckEncroachment) {
                ProbeNativeMoveCommand(command,result.states.size(),result,execution);
            }
            else if(command.operation==PortableRetailCollisionOperation::SetActorZone) {
                if(command.actorPath.empty()||command.actorPath.size()>8192u||command.actorPath.find('\0')!=std::string::npos)
                    throw std::runtime_error("Original zone diagnostic requires one bounded actor path");
                auto* level=RetailCollisionLevel();auto* actor=ZoneActor(reinterpret_cast<std::uintptr_t>(Object(command.actorPath)),level);
                auto clockBefore=ZoneClockSnapshot(actor);
                const auto run=[&]{QuestVr::SetOriginalActorZone(reinterpret_cast<std::uintptr_t>(level),reinterpret_cast<std::uintptr_t>(actor),
                    command.zoneTest,command.zoneForce,ActorZoneQueries(level,execution));};
                if(command.zoneUsePose){
                    const auto begun=Read(NativeLevelInfo(actor),DescribeProperty("Engine.LevelInfo.bBegunPlay"),0u);
                    if(begun.kind!=Kind::Bool||begun.boolean)
                        throw std::runtime_error("Zone posed-input fixture is restricted to an authentic dormant world");
                    static_cast<void>(NativeCoordsProperty(actor,"Engine.Actor.Location",Kind::Vector,0x23u));
                    const auto rotation=NativeCoordsRead(actor,"Engine.Actor.Rotation",Kind::Rotator,0x23u).rotation;
                    // Fixed native pose slots are charged once by the host.
                    QuestVr::NativeActorPoseStack::Scope pose(nativeActorPoses_,reinterpret_cast<std::uintptr_t>(actor),{command.end,rotation});
                    run();
                }else run(); // Scope is restored BEFORE output/hash snapshots.
                auto snapshot=ZoneSnapshot(actor,result.states.size());
                snapshot.clockBefore=std::move(clockBefore);snapshot.clockAfter=ZoneClockSnapshot(actor);
                result.zones.push_back(std::move(snapshot));
            }
            else if(command.operation==PortableRetailCollisionOperation::BeginTouch ||
                command.operation==PortableRetailCollisionOperation::EndTouch ||
                command.operation==PortableRetailCollisionOperation::TouchSide ||
                command.operation==PortableRetailCollisionOperation::IsOverlapping) {
                const auto valid=[](const std::string& path) {
                    return !path.empty()&&path.size()<=8192u&&path.find('\0')==std::string::npos;
                };
                if(!valid(command.actorPath)||!valid(command.otherActorPath))
                    throw std::runtime_error("Original native touching diagnostic requires two bounded actor paths");
                auto* source=RetailCollisionActor(reinterpret_cast<std::uintptr_t>(Object(command.actorPath)));
                auto* other=RetailCollisionActor(reinterpret_cast<std::uintptr_t>(Object(command.otherActorPath)));
                const auto a=reinterpret_cast<std::uintptr_t>(source),b=reinterpret_cast<std::uintptr_t>(other);
                if(command.operation==PortableRetailCollisionOperation::IsOverlapping) {
                    const bool returned=QuestVr::IsOriginalActorOverlapping(a,b,RetailActorOverlapQueries());
                    ReserveRetailDiagnosticOutput(1u+4u*sizeof(void*));
                    result.predicates.push_back({result.states.size(),returned,{}});
                }else {
                    auto queries=RetailActorTouchQueries(execution);bool accepted{};
                    if(command.operation==PortableRetailCollisionOperation::BeginTouch)
                        QuestVr::BeginOriginalActorTouch(a,b,queries);
                    else if(command.operation==PortableRetailCollisionOperation::EndTouch)
                        QuestVr::EndOriginalActorTouch(a,b,command.touchSuppressSelf,queries);
                    else accepted=QuestVr::AddOriginalActorTouchSide(a,b,queries);
                    result.touches.push_back(RetailTouchSnapshot(source,other,result.states.size(),accepted));
                }
            }
            else if(command.operation==PortableRetailCollisionOperation::IsBlockedBy ||
                command.operation==PortableRetailCollisionOperation::IsBasedOn ||
                command.operation==PortableRetailCollisionOperation::LocalPlayerPawn) {
                const auto valid=[](const std::string& path) {
                    return path.size()<=8192u&&path.find('\0')==std::string::npos;
                };
                if(command.actorPath.empty() || !valid(command.actorPath) || !valid(command.otherActorPath) ||
                    (command.operation==PortableRetailCollisionOperation::IsBlockedBy && command.otherActorPath.empty()))
                    throw std::runtime_error("Original actor predicate diagnostic has invalid bounded actor input");
                auto* source=Object(command.actorPath);
                static_cast<void>(RetailCollisionActor(reinterpret_cast<std::uintptr_t>(source)));
                bool returned{};std::string playerPath;
                if(command.operation==PortableRetailCollisionOperation::LocalPlayerPawn) {
                    const auto player=QuestVr::GetOriginalActorLocalPlayerPawn(reinterpret_cast<std::uintptr_t>(source),
                        RetailActorBlockingQueries());
                    returned=player!=0u;
                    // Admit the returned string BEFORE copying it. Predicate
                    // evaluation itself does not allocate a detached path.
                    ReserveRetailDiagnosticOutput((player?source->reflection.objectPath.size():0u)+1u+4u*sizeof(void*));
                    if(player)playerPath=source->reflection.objectPath;
                }else {
                    auto* other=command.otherActorPath.empty()?nullptr:Object(command.otherActorPath);
                    // IsBasedOn reads no target fields, including on self hit.
                    returned=command.operation==PortableRetailCollisionOperation::IsBlockedBy?
                        RetailIsBlockedBy(source,other):RetailIsBasedOn(source,other);
                }
                if(command.operation!=PortableRetailCollisionOperation::LocalPlayerPawn)
                    ReserveRetailDiagnosticOutput(1u+4u*sizeof(void*));
                result.predicates.push_back({result.states.size(),returned,std::move(playerPath)});
            }
            else if(command.operation==PortableRetailCollisionOperation::LineCheck ||
                command.operation==PortableRetailCollisionOperation::MultiLineCheck ||
                command.operation==PortableRetailCollisionOperation::EncroachmentCheck) {
                auto* level=RetailCollisionLevel();
                const auto vector=[](const auto& value) {return PortableModelVec3{value[0u],value[1u],value[2u]};};
                QuestVr::OriginalLineHitArena hits;
                if(command.operation==PortableRetailCollisionOperation::LineCheck) {
                    if(!level->nativeRetailHash || !persistentRetailCollision)
                        throw std::runtime_error("Retail collision query requires an actual enabled Level.Hash");
                    TouchRetailCollision();
                    // Genuine global tags deduplicate each actor in this query.
                    // Preflight vector growth before producing the native arena.
                    ValidateStateBudget(QuestVr::DecalStateDetail::Multiply(persistentRetailCollision->ReadUsage().actors,
                        2u*sizeof(QuestVr::OriginalActorLineHit)));
                    hits=persistentRetailCollision->ActorLineCheck(level->nativeRetailHash,vector(command.end),
                        vector(command.start),vector(command.extent),command.flags,RetailCollisionQueries());
                }else if(command.operation==PortableRetailCollisionOperation::EncroachmentCheck) {
                    if(command.actorPath.empty() || command.actorPath.size()>8192u || command.actorPath.find('\0')!=std::string::npos)
                        throw std::runtime_error("ActorEncroachmentCheck diagnostic source is invalid");
                    hits=RetailActorEncroachment(Object(command.actorPath),command.end,command.rotation,command.ignoredEncroachmentFlags);
                }else {
                    if(command.actorPath.size()>8192u || command.actorPath.find('\0')!=std::string::npos)
                        throw std::runtime_error("Retail MultiLineCheck diagnostic anchor is invalid");
                    auto* anchor=command.actorPath.empty()?persistentWorldActors.front():Object(command.actorPath);
                    auto reply=RetailMultiLine(anchor,vector(command.end),vector(command.start),vector(command.extent),
                        command.checkActors?1:0,command.checkWorld?persistentWorldActors.front():nullptr,command.flags);
                    const auto& trace=reply.result;const auto& stats=trace.stats;
                    result.multiLine.push_back({result.queries.size(),stats.steps,stats.actorVisits,stats.moverTests,
                        stats.skipped,stats.corrected,{trace.actorQueryEnd.x,trace.actorQueryEnd.y,trace.actorQueryEnd.z},
                        trace.scale,trace.hasWorldHit,trace.nearWorldHit,reply.worldItemKnown});
                    hits=std::move(reply.result.hits);
                }
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
            }else if(command.operation==PortableRetailCollisionOperation::SetCollision ||
                command.operation==PortableRetailCollisionOperation::SetCollisionSize) {
                if(command.actorPath.empty() || command.actorPath.size()>8192u || command.arguments.size()>3u)
                    throw std::runtime_error("Collision setter diagnostic input is unbounded");
                auto* actor=Object(command.actorPath);
                static_cast<void>(RetailCollisionActor(reinterpret_cast<std::uintptr_t>(actor)));
                if(!persistentRetailCollision) throw std::runtime_error("Collision setter diagnostic requires an initialized native domain");
                const auto index=std::uint16_t(command.operation==PortableRetailCollisionOperation::SetCollision?262u:283u);
                const auto function=RetailCollisionSetterDeclaration(index,nullptr);
                for(const auto& value:command.arguments) {
                    const bool scalar=index==262u?(value.kind==Kind::Bool || value.kind==Kind::Nothing):
                        (value.kind==Kind::Float || value.kind==Kind::Byte || value.kind==Kind::Int || value.kind==Kind::Nothing);
                    if(!scalar || !value.text.empty() || !value.intArray.empty() || !value.fields.empty())
                        throw std::runtime_error("Collision setter diagnostic requires bounded original scalar inputs");
                }
                ValidateStateBudget(QuestVr::DecalStateDetail::Multiply(command.arguments.size(),sizeof(Evaluation)+sizeof(Value)));
                const auto returned=[&] {
                    std::vector<Evaluation> inputs;inputs.reserve(command.arguments.size());
                    for(const auto& value:command.arguments) inputs.push_back({value,{}});
                    return execution.CallFunction(*function,command.actorPath,inputs);
                }(); // Copied scalar input scratch dies before output/snapshot retention.
                const auto radius=Read(actor,RetailCollisionSetterField(actor,"Engine.Actor.CollisionRadius"),0u);
                const auto height=Read(actor,RetailCollisionSetterField(actor,"Engine.Actor.CollisionHeight"),0u);
                ReserveRetailDiagnosticOutput(command.actorPath.size()+1u+4u*sizeof(void*));
                result.mutations.push_back({command.actorPath,returned,radius.floating,height.floating,
                    RetailCollisionFlag(actor,"Engine.Actor.bCollideActors"),RetailCollisionFlag(actor,"Engine.Actor.bBlockActors"),
                    RetailCollisionFlag(actor,"Engine.Actor.bBlockPlayers"),RetailCollisionFlag(actor,"Engine.Actor.bCollideWorld")});
            }else throw std::runtime_error("Unknown retail collision diagnostic operation");
            ValidateStateBudget();result.states.push_back(RetailCollisionSnapshot());
        }
        return Value::Bool(true);
    }
