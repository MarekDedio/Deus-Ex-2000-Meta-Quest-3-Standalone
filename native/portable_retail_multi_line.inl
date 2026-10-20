// Included inside PortableActorVmHost after portable_retail_collision.inl.
// This is the actual current-Level service seam, not MoveActor, a render ray,
// or permission to publish an incompletely initialized Training world.
    struct RetailMultiLineReply {
        QuestVr::OriginalMultiLineResult result;
        // Retail's world FCheckResult is initially uninitialized. A point hit
        // writes Item; an extent hit does not. We deterministically initialize
        // that otherwise undefined word to -1, but MUST NOT expose it as an
        // authored node/surface index. No admitted world hit also leaves this
        // true because no undefined world Item is present in the output.
        bool worldItemKnown{true};
    };
    RetailMultiLineReply RetailMultiLine(RuntimeObject* levelActor,
        PortableModelVec3 end,PortableModelVec3 start,PortableModelVec3 extent,
        std::int32_t checkActors,RuntimeObject* suppliedLevelInfo,std::uint8_t extraNodeFlags) {
        if(!transaction_)
            throw std::runtime_error("Retail MultiLineCheck requires its complete outer VM transaction");
        auto* anchor=RetailCollisionActor(reinterpret_cast<std::uintptr_t>(levelActor));
        auto* level=RetailCollisionLevel();
        // This first-Training embedding accepts only the live current native
        // domain. The anchor identifies the receiving ULevel; it is NEVER a
        // source actor to exclude from either provider's returned hits.
        if(!anchor->active || anchor->worldRemoved || anchor->nativeXLevel!=level)
            throw std::runtime_error("Retail MultiLineCheck has no live current-Level anchor");
        if(suppliedLevelInfo&&(!suppliedLevelInfo->active||suppliedLevelInfo->worldRemoved))
            throw std::runtime_error("Retail MultiLineCheck diagnostic world anchor is inactive");
        return RetailMultiLineLevel(level,end,start,extent,checkActors,suppliedLevelInfo,extraNodeFlags);
    }
    // Actual ULevel service: unlike the public dormant diagnostic wrapper,
    // MoveActor has no source-anchor/render-active/deletion admission here.
    // It also has no source argument to filter out of the result list.
    RetailMultiLineReply RetailMultiLineLevel(RuntimeObject* level,
        PortableModelVec3 end,PortableModelVec3 start,PortableModelVec3 extent,
        std::int32_t checkActors,RuntimeObject* suppliedLevelInfo,std::uint8_t extraNodeFlags) {
        if(!transaction_||level!=RetailCollisionLevel())
            throw std::runtime_error("Retail MultiLineCheck requires the same actual native Level/VM root");
        if(suppliedLevelInfo) {
            static_cast<void>(RetailCollisionActor(reinterpret_cast<std::uintptr_t>(suppliedLevelInfo)));
            if(suppliedLevelInfo!=persistentWorldActors.front() ||
                !IsDerivedFromPath(suppliedLevelInfo->cls,"Engine.LevelInfo") ||
                suppliedLevelInfo->nativeXLevel!=level)
                throw std::runtime_error("Retail MultiLineCheck world is not actual current native slot-zero LevelInfo");
        }
        // Preserve the native short circuit: CheckActors==0 does not consult
        // Level.Hash, allocate its root snapshot, or consume a collision tag.
        const bool queryActors=checkActors && level->nativeRetailHash;
        if(queryActors) {
            if(!persistentRetailCollision || !persistentRetailCollision->ContainsHash(level->nativeRetailHash))
                throw std::runtime_error("Retail MultiLineCheck Level.Hash lost its owned native storage");
            TouchRetailCollision();
        }
        using QuestVr::DecalStateDetail::Multiply;
        constexpr std::size_t collectorCapacity=64u;
        // The collector's fixed64 packets coexist with either provider. The
        // actor arena is allocated only AFTER the world query has returned,
        // so it must not be charged against that already-dead Model workspace.
        const auto temporaryBytes=Multiply(collectorCapacity,sizeof(QuestVr::OriginalActorLineHit));
        QuestVr::NativeScratchScope reservation(nativeQueryScratchBytes_,temporaryBytes,
            [this](std::size_t extra){ValidateStateBudget(extra);});
        // Declared before query/result owners: this full arena envelope stays
        // live across actor primitives, the collector's arena traversal and
        // late admission, and unwinds before the fixed collector reservation.
        std::optional<QuestVr::NativeScratchScope> actorArenaReservation;
        bool worldItemKnown=true;
        QuestVr::OriginalMultiLineQueries queries;
        queries.work=[this] {
            if(fastTraceQueryWork_>=MaximumNativeModelQueryWork)
                throw std::runtime_error("Retail MultiLineCheck shared collector/query work budget exhausted");
            ++fastTraceQueryWork_;
        };
        queries.world=[this,level,suppliedLevelInfo,&worldItemKnown](std::uintptr_t identity,
            const PortableModelVec3& worldEnd,const PortableModelVec3& worldStart,
            const PortableModelVec3& worldExtent,std::uint32_t flags) {
            if(!suppliedLevelInfo || identity!=reinterpret_cast<std::uintptr_t>(suppliedLevelInfo) ||
                suppliedLevelInfo!=persistentWorldActors.front() || suppliedLevelInfo->nativeXLevel!=level ||
                level!=RetailCollisionLevel())
                throw std::runtime_error("Retail MultiLineCheck supplied native world identity changed");
            const auto& model=NativeTraceModel(suppliedLevelInfo);
            auto* nativeModel=level->nativeLevelModel;
            if(!nativeModel)
                throw std::runtime_error("Retail MultiLineCheck native Level.Model disappeared");
            // Both null-owner trace kernels validate the BSP and then release
            // that validation workspace before allocating traversal storage.
            // Bound the larger peak including geometric vector-growth overlap.
            const bool point=!model.nodes.empty() &&
                QuestVr::MeshLineTraceDetail::ZeroOrUnordered(worldExtent.x) &&
                QuestVr::MeshLineTraceDetail::ZeroOrUnordered(worldExtent.y) &&
                QuestVr::MeshLineTraceDetail::ZeroOrUnordered(worldExtent.z);
            const auto workspace=QuestVr::OriginalModelLineScratchBytes(model.nodes.size(),point);
            QuestVr::NativeScratchScope workspaceReservation(nativeQueryScratchBytes_,workspace,
                [this](std::size_t extra){ValidateStateBudget(extra);});
            if(fastTraceValidationWork_>=MaximumNativeModelValidationWork || fastTraceQueryWork_>=MaximumNativeModelQueryWork)
                throw std::runtime_error("Retail MultiLineCheck shared Model query budget exhausted");
            QuestVr::OriginalWorldLineReply reply;
            // This deterministic initialization is a portable undefined-field
            // policy, NOT an original Time1/Item-1 constructor. Only defined
            // Model outputs may be consumed below; the collector replaces Next.
            reply.hit={};
            try {
                if(point) {
                    QuestVr::ModelPointTraceLimits limits;
                    limits.validationSteps=std::min(limits.validationSteps,MaximumNativeModelValidationWork-fastTraceValidationWork_);
                    limits.querySteps=std::min(limits.querySteps,MaximumNativeModelQueryWork-fastTraceQueryWork_);
                    QuestVr::ModelPointTraceStats stats;
                    reply.clear=QuestVr::CheckOriginalModelPoint(reply.hit,model,reinterpret_cast<std::uintptr_t>(nativeModel),
                        0u,worldEnd,worldStart,flags,[]()->QuestVr::OriginalModelCoords {
                            throw std::runtime_error("Null-owner world Model unexpectedly requested Owner.ToWorld");
                        },limits,&stats);
                    fastTraceValidationWork_+=stats.validationSteps;fastTraceQueryWork_+=stats.querySteps;
                }else {
                    QuestVr::ModelExtentTraceLimits limits;
                    limits.validationSteps=std::min(limits.validationSteps,MaximumNativeModelValidationWork-fastTraceValidationWork_);
                    limits.querySteps=std::min(limits.querySteps,MaximumNativeModelQueryWork-fastTraceQueryWork_);
                    QuestVr::ModelExtentTraceStats stats;
                    reply.clear=QuestVr::CheckOriginalModelExtent(reply.hit,model,reinterpret_cast<std::uintptr_t>(nativeModel),
                        0u,nullptr,worldEnd,worldStart,worldExtent,limits,&stats);
                    fastTraceValidationWork_+=stats.validationSteps;fastTraceQueryWork_+=stats.querySteps;
                    if(!reply.clear) {
                        if(model.nodes.empty())
                            throw std::runtime_error("Retail MultiLineCheck empty-inside world packet has undefined Time/Location");
                        worldItemKnown=false;
                    }
                }
            }catch(...) {
                // Kernels publish statistics only on success. A refused native
                // query cannot be retried for a fresh allowance by a catching
                // coordinator: conservatively consume the remaining ledgers.
                fastTraceValidationWork_=MaximumNativeModelValidationWork;
                fastTraceQueryWork_=MaximumNativeModelQueryWork;
                throw;
            }
            return reply;
        };
        if(queryActors) {
            queries.actors=[this,level,&actorArenaReservation](const PortableModelVec3& actorEnd,const PortableModelVec3& actorStart,
                const PortableModelVec3& actorExtent,std::uint8_t flags) {
                if(level!=RetailCollisionLevel() || !level->nativeRetailHash || !persistentRetailCollision ||
                    !persistentRetailCollision->ContainsHash(level->nativeRetailHash))
                    throw std::runtime_error("Retail MultiLineCheck receiving native hash changed");
                try {
                    if(actorArenaReservation)
                        throw std::runtime_error("Retail MultiLineCheck repeated its one owning actor arena query");
                    // Still permit one hit per CURRENT actual native actor,
                    // including vector capacity growth's old/new overlap.
                    // Do not cap candidates or substitute collidable/link counts.
                    const auto arenaBytes=Multiply(persistentRetailCollision->ReadUsage().actors,
                        3u*sizeof(QuestVr::OriginalActorLineHit));
                    actorArenaReservation.emplace(nativeQueryScratchBytes_,arenaBytes,
                        [this](std::size_t extra){ValidateStateBudget(extra);});
                    return persistentRetailCollision->ActorLineCheck(level->nativeRetailHash,
                        actorEnd,actorStart,actorExtent,flags,RetailCollisionQueries());
                }catch(...) {
                    // Model providers inside the actor hash also return stats
                    // only after success; preserve no reusable failed allowance.
                    fastTraceValidationWork_=MaximumNativeModelValidationWork;
                    fastTraceQueryWork_=MaximumNativeModelQueryWork;
                    throw;
                }
            };
        }
        queries.isMover=[this](std::uintptr_t identity) {
            return IsDerivedFromPath(RetailCollisionActor(identity)->cls,"Engine.Mover");
        };
        const auto collect=[&] {
            auto result=QuestVr::CollectOriginalMultiLineHits(end,start,extent,checkActors,
                reinterpret_cast<std::uintptr_t>(suppliedLevelInfo),extraNodeFlags,queries);
            // Provider arenas/workspaces have died here. The retained peak
            // already includes the fixed64-packet returned collector. Do not
            // charge that same vector again while this reservation is active.
            ValidateStateBudget();
            return RetailMultiLineReply{std::move(result),worldItemKnown};
        };
        // A late collector/sort/output-admission failure must poison the SAME
        // hash root, including already consumed actor collision tags. No child
        // query may commit independently before the host transaction commits.
        if(queryActors) return persistentRetailCollision->WithRoot([&](QuestVr::OriginalCollisionHashWorld&) {
            return collect();
        });
        return collect();
    }
