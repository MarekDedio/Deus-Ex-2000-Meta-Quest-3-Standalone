// Included inside PortableActorVmHost, after the actual retail query services
// and ZoneLevel/ZoneActor providers are declared. Ephemeral native hit storage
// only: this does not expose native266, provide movement, or publish startup.
    std::unique_ptr<QuestVr::NativeHitMarks> nativeMoveHits_;
    QuestVr::NativeHitRootStamp nativeMoveHitStamp_{};

    QuestVr::NativeHitRootStamp MoveHitRootStamp(std::uintptr_t identity) {
        if(!transaction_)
            throw std::runtime_error("Movement hit storage requires its complete outer VM transaction");
        auto* level=ZoneLevel(identity);
        if(!persistentObjectLookup || !persistentObjectLookup->active || !persistentObjectLookup->generation ||
            level->lookupControl!=persistentObjectLookup)
            throw std::runtime_error("Movement hit storage lost its actual live lookup-root authority");
        return {identity,reinterpret_cast<std::uintptr_t>(persistentObjectLookup.get()),
            level->nativeMapGeneration,persistentObjectLookup->generation};
    }
    QuestVr::NativeHitMarks& ExistingMoveHits(const QuestVr::NativeHitRootStamp& stamp) {
        if(!transaction_ || !nativeMoveHits_ || !nativeMoveHits_->Active() || !nativeMoveHits_->MarkDepth() ||
            nativeMoveHitStamp_!=stamp)
            throw std::runtime_error("Movement hit query has no current owning VM-root memory mark");
        return *nativeMoveHits_;
    }
    std::uintptr_t MoveHitMark(std::uintptr_t level) {
        const auto stamp=MoveHitRootStamp(level);
        if(!nativeMoveHits_) {
            // The eventual shared host validator includes this member's
            // RetainedBytes once published. No allocation is hidden in Mark.
            ValidateStateBudget(sizeof(QuestVr::NativeHitMarks));
            auto staged=std::make_unique<QuestVr::NativeHitMarks>();
            staged->BeginRoot(stamp);
            nativeMoveHits_=std::move(staged);
            nativeMoveHitStamp_=stamp;
        }else if(!nativeMoveHits_->Active()) {
            nativeMoveHits_->BeginRoot(stamp);nativeMoveHitStamp_=stamp;
        }
        return nativeMoveHits_->Mark(stamp);
    }
    void MoveHitPop(std::uintptr_t token)noexcept {
        // Mandatory cleanup authenticates the unique issued token internally.
        // It must not perform allocating UObject/lookup/schema validation,
        // charge work, reset budgets, or run callbacks while unwinding.
        if(!nativeMoveHits_ || !nativeMoveHits_->Pop(token))std::terminate();
    }
    void ClearMoveHits()noexcept {
        // Call BEFORE rollback retires provisional lookup/birth identities.
        // Capability serials are deliberately not part of gameplay rollback.
        if(nativeMoveHits_ && !nativeMoveHits_->Clear())std::terminate();
        nativeMoveHitStamp_={};
    }
    void RequireEmptyMoveHits() {
        if(nativeMoveHits_ && (nativeMoveHits_->MarkDepth() || nativeMoveHits_->ArenaCount() ||
            nativeMoveHits_->PayloadBytes()))
            throw std::runtime_error("Movement cannot commit with live native hit marks or retained arenas");
    }
    void MoveHitPrimitiveIdentity(std::uintptr_t identity) {
        if(!identity)return;
        if(!persistentObjectLookup || !persistentObjectLookup->active)
            throw std::runtime_error("Movement hit Primitive lost its actual UObject lookup authority");
        const auto* record=persistentObjectLookup->index.RecordForIdentity(identity);
        if(!record)throw std::runtime_error("Movement hit Primitive contains a retired UObject identity");
        auto* object=reinterpret_cast<RuntimeObject*>(identity);
        if(object->lookupControl!=persistentObjectLookup || object->objectIndex.Index()!=record->index ||
            object->defaultOwner)
            throw std::runtime_error("Movement hit Primitive belongs to another lookup generation");
        // Original providers established the selected primitive's class and
        // native authority. This is a lifetime check, not asset re-selection.
    }
    QuestVr::OriginalActorLineHit MoveHitPacket(std::uintptr_t level,std::uintptr_t handle) {
        const auto stamp=MoveHitRootStamp(level);
        auto packet=ExistingMoveHits(stamp).Packet(stamp,handle);
        auto* nativeLevel=ZoneLevel(level);
        if(packet.actor)static_cast<void>(ZoneActor(packet.actor,nativeLevel));
        MoveHitPrimitiveIdentity(packet.primitive);
        return packet; // Full raw packet, INCLUDING its already-global Next.
    }
    std::uintptr_t MoveHitActor(std::uintptr_t level,std::uintptr_t handle) {
        const auto stamp=MoveHitRootStamp(level);
        const auto actor=ExistingMoveHits(stamp).Actor(stamp,handle);
        if(actor)static_cast<void>(ZoneActor(actor,ZoneLevel(level)));
        return actor;
    }
    std::uintptr_t MoveHitNext(std::uintptr_t level,std::uintptr_t handle) {
        const auto stamp=MoveHitRootStamp(level);
        return ExistingMoveHits(stamp).Next(stamp,handle);
    }
    float MoveHitTime(std::uintptr_t level,std::uintptr_t handle) {
        const auto stamp=MoveHitRootStamp(level);
        return ExistingMoveHits(stamp).Time(stamp,handle);
    }
    std::uintptr_t AdoptMoveHits(std::uintptr_t level,QuestVr::OriginalLineHitArena&& arena) {
        const auto stamp=MoveHitRootStamp(level);
        auto& store=ExistingMoveHits(stamp);
        // Capacity, not size, is the live allocation. Keep the returned arena
        // visible to every admission until ownership is transferred. The
        // producer already accounted for its larger collector/Model peak.
        const auto incomingBytes=QuestVr::DecalStateDetail::Multiply(
            arena.records.capacity(),sizeof(QuestVr::OriginalActorLineHit));
        QuestVr::NativeScratchScope incoming(nativeQueryScratchBytes_,incomingBytes,
            [this](std::size_t extra){ValidateStateBudget(extra);});
        return store.Adopt(stamp,std::move(arena),[this,incomingBytes](std::size_t additional) {
            // The incoming reservation already counts this allocation once;
            // adding 'additional' again would impose an invented double peak.
            // Empty arenas discard their reserved collector vector on Adopt.
            if(additional && additional!=incomingBytes)
                throw std::runtime_error("Movement hit adoption disagrees with its reserved owning capacity");
            ValidateStateBudget(); // Pure/non-reentrant: no provider dispatch.
        });
        // Adoption's nonthrowing swap installs store ownership before this
        // scope releases its accounting. No callback/admission runs in between.
    }
    std::uintptr_t MoveMultiLineHits(std::uintptr_t identity,QuestVr::OriginalMoveActorSweep sweep) {
        const auto stamp=MoveHitRootStamp(identity);
        auto* level=ZoneLevel(identity);
        if(sweep.level!=identity)
            throw std::runtime_error("Movement sweep changed its retained native Level receiver");
        static_cast<void>(ExistingMoveHits(stamp));
        RuntimeObject* world{};
        if(sweep.world)world=ZoneActor(sweep.world,level);
        const bool actorHash=sweep.checkActors && level->nativeRetailHash;
        if(actorHash)TouchRetailCollision();
        const auto collect=[&] {
            // Required factored service: actual native Level authority, no
            // source/active/render/deletion anchor gate and no source exclusion.
            // The existing diagnostic RetailMultiLine wrapper stays separate.
            auto reply=RetailMultiLineLevel(level,sweep.end,sweep.start,sweep.extent,
                sweep.checkActors,world,sweep.extraFlags);
            return AdoptMoveHits(identity,std::move(reply.result.hits));
        };
        // Query AND adoption/late admission share the actual existing hash root.
        // A world-only/absent-hash call neither initializes CRT tables nor takes
        // a fresh hash snapshot just for temporary hit-list ownership.
        if(retailCollisionRoot_) {
            if(!persistentRetailCollision)
                throw std::runtime_error("Movement hit query lost its enclosing native collision root");
            return persistentRetailCollision->WithRoot([&](QuestVr::OriginalCollisionHashWorld&) {return collect();});
        }
        return collect();
    }
    std::uintptr_t MoveEncroachmentHits(std::uintptr_t hash,std::uintptr_t source,
        PortableModelVec3 proposed,std::array<std::int32_t,3u> rotation) {
        if(!transaction_)
            throw std::runtime_error("Movement encroachment requires its complete outer VM transaction");
        auto* level=RetailCollisionLevel();
        const auto identity=reinterpret_cast<std::uintptr_t>(level);
        const auto stamp=MoveHitRootStamp(identity);
        static_cast<void>(ExistingMoveHits(stamp));
        auto* actor=ZoneActor(source,level);
        if(!hash || level->nativeRetailHash!=hash || !persistentRetailCollision ||
            !persistentRetailCollision->ContainsHash(hash))
            throw std::runtime_error("Movement encroachment supplied a foreign or expired actual Level.Hash");
        TouchRetailCollision();
        return persistentRetailCollision->WithRoot([&](QuestVr::OriginalCollisionHashWorld&) {
            auto hits=RetailActorEncroachment(actor,{proposed.x,proposed.y,proposed.z},rotation,0u);
            return AdoptMoveHits(identity,std::move(hits));
        });
    }
