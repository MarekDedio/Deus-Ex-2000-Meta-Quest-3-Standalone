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
        if(suppliedLevelInfo) {
            static_cast<void>(RetailCollisionActor(reinterpret_cast<std::uintptr_t>(suppliedLevelInfo)));
            if(suppliedLevelInfo!=persistentWorldActors.front() ||
                !IsDerivedFromPath(suppliedLevelInfo->cls,"Engine.LevelInfo") ||
                suppliedLevelInfo->nativeXLevel!=level || !suppliedLevelInfo->active || suppliedLevelInfo->worldRemoved)
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
        using QuestVr::DecalStateDetail::Add;
        using QuestVr::DecalStateDetail::Multiply;
        constexpr std::size_t collectorCapacity=64u;
        // The collector always reserves its fixed64 packets. The actor arena
        // owns a separate growable vector: at most one hit per actual native
        // actor, with headroom for capacity growth and its old allocation.
        // Primitive-query workspaces remain subject to their own kernel caps.
        auto temporaryBytes=Multiply(collectorCapacity,sizeof(QuestVr::OriginalActorLineHit));
        if(queryActors) temporaryBytes=Add(temporaryBytes,Multiply(
            persistentRetailCollision->ReadUsage().actors,3u*sizeof(QuestVr::OriginalActorLineHit)));
        ValidateStateBudget(temporaryBytes);
        bool worldItemKnown=true;
        QuestVr::OriginalMultiLineQueries queries;
        queries.work=[this] {
            if(fastTraceQueryWork_>=MaximumNativeModelQueryWork)
                throw std::runtime_error("Retail MultiLineCheck shared collector/query work budget exhausted");
            ++fastTraceQueryWork_;
        };
        queries.world=[this,level,suppliedLevelInfo,temporaryBytes,&worldItemKnown](std::uintptr_t identity,
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
            struct ValidationFrame {std::size_t node;unsigned next;};
            struct PointSegment {std::int32_t node,last;PortableModelVec3 start,end;bool outside;};
            struct ExtentVisit {std::int32_t node,parent;bool outside;};
            const auto validationWidth=3u*sizeof(ValidationFrame)+sizeof(std::uint8_t);
            const auto traversalWidth=3u*std::max(sizeof(PointSegment),sizeof(ExtentVisit));
            const auto workspace=Add(128u,Multiply(model.nodes.size(),std::max(validationWidth,traversalWidth)));
            ValidateStateBudget(Add(temporaryBytes,workspace));
            if(fastTraceValidationWork_>=MaximumNativeModelValidationWork || fastTraceQueryWork_>=MaximumNativeModelQueryWork)
                throw std::runtime_error("Retail MultiLineCheck shared Model query budget exhausted");
            QuestVr::OriginalWorldLineReply reply;
            // This deterministic initialization is a portable undefined-field
            // policy, NOT an original Time1/Item-1 constructor. Only defined
            // Model outputs may be consumed below; the collector replaces Next.
            reply.hit={};
            try {
                const bool point=!model.nodes.empty() &&
                    QuestVr::MeshLineTraceDetail::ZeroOrUnordered(worldExtent.x) &&
                    QuestVr::MeshLineTraceDetail::ZeroOrUnordered(worldExtent.y) &&
                    QuestVr::MeshLineTraceDetail::ZeroOrUnordered(worldExtent.z);
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
            queries.actors=[this,level](const PortableModelVec3& actorEnd,const PortableModelVec3& actorStart,
                const PortableModelVec3& actorExtent,std::uint8_t flags) {
                if(level!=RetailCollisionLevel() || !level->nativeRetailHash || !persistentRetailCollision ||
                    !persistentRetailCollision->ContainsHash(level->nativeRetailHash))
                    throw std::runtime_error("Retail MultiLineCheck receiving native hash changed");
                try {
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
            // Provider arenas/workspaces have died here. Charge the one live
            // returned collector vector, not the preflight peak a second time.
            ValidateStateBudget(Multiply(result.hits.records.capacity(),sizeof(QuestVr::OriginalActorLineHit)));
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
