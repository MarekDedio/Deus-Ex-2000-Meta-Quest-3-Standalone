// Included inside PortableActorVmHost. Original native hash-query seam only;
// higher Level encroachment/push/touch/Move contracts are not supplied here.
    QuestVr::NativeActorPoseStack nativeActorPoses_;
    bool RetailMovingBrush(RuntimeObject* actor) {
        const auto property=DescribeProperty("Engine.Actor.Brush");auto* field=Object(property.key);
        if(property.key!="Engine.Actor.Brush" || !OwnsProperty(actor->cls,property) ||
            property.zero.kind!=Kind::Object || property.arrayDimension!=1u || property.flags!=0x2au ||
            !field->property || field->property->type!="ObjectProperty" || field->property->baseField ||
            field->outer!=Object("Engine.Actor") || !field->property->referencedType ||
            LowerAscii(Qualified(Table(field->sourcePath),field->property->referencedType))!="engine.model")
            throw std::runtime_error("Moving-brush predicate lacks its original declared Brush pointer schema");
        const auto brush=Read(actor,property,0u);
        if(brush.kind!=Kind::Object)throw std::runtime_error("Moving-brush live Brush is not an Object pointer");
        // This gate only observes whether Brush is nonnull. It never calls
        // candidate.GetPrimitive or dereferences that Brush's Model payload.
        if(brush.text.empty()||LowerAscii(brush.text)=="none")return false;
        if(!IsDerivedFromPath(actor->cls,"Engine.Brush"))return false;
        return !RetailCollisionFlag(actor,"Engine.Actor.bStatic");
    }
    QuestVr::OriginalLineHitArena RetailActorEncroachment(RuntimeObject* source,
        const std::array<float,3u>& location,const std::array<std::int32_t,3u>& rotation,
        std::uint32_t ignoredFlags) {
        static_cast<void>(ignoredFlags); // Original final DWORD is never read.
        if(!transaction_)throw std::runtime_error("ActorEncroachmentCheck requires its complete outer VM transaction");
        static_cast<void>(RetailCollisionActor(reinterpret_cast<std::uintptr_t>(source)));
        auto* level=RetailCollisionLevel();
        if(!level->nativeRetailHash || !persistentRetailCollision ||
            !persistentRetailCollision->ContainsHash(level->nativeRetailHash))
            throw std::runtime_error("ActorEncroachmentCheck requires the actual enabled native Level.Hash");
        TouchRetailCollision();DefineRetailCollisionActor(source);
        // Validate the exact native fields and existing typed live values
        // BEFORE introducing the scoped view. No ordinary script write guard
        // is relaxed, and no authored/frozen/default/property map is patched.
        static_cast<void>(NativeCoordsRead(source,"Engine.Actor.Location",Kind::Vector,0x23u));
        static_cast<void>(NativeCoordsRead(source,"Engine.Actor.Rotation",Kind::Rotator,0x23u));
        using QuestVr::DecalStateDetail::Multiply;
        QuestVr::OriginalHashEncroachmentQueries queries;
        queries.sourceBounds=[this](std::uintptr_t identity) {
            LifecycleStep();return NativeActorPrimitiveBounds(RetailCollisionActor(identity));
        };
        queries.movingBrush=[this](std::uintptr_t identity) {
            LifecycleStep();return RetailMovingBrush(RetailCollisionActor(identity));
        };
        queries.candidateCylinder=[this](std::uintptr_t identity) {
            LifecycleStep();return NativePrimitiveOwnerCylinder(RetailCollisionActor(identity));
        };
        queries.sourcePoint=[this](QuestVr::OriginalActorLineHit& hit,std::uintptr_t identity,
            const PortableModelVec3& point,const PortableModelVec3& extent,std::uint32_t flags) {
            LifecycleStep();return NativeActorPrimitivePointCheck(RetailCollisionActor(identity),hit,point,extent,flags);
        };
        return persistentRetailCollision->WithRoot([&](QuestVr::OriginalCollisionHashWorld& domain) {
            // Track the actual collector allocation, not three hit records for
            // every Actor in the Level. Its current capacity remains visible
            // during every nested primitive/Model admission; only reserve has
            // an overlapping old+new allocation peak. No hit/work cap changes.
            QuestVr::NativeScratchScope scratch(nativeQueryScratchBytes_,0u,
                [this](std::size_t extra){ValidateStateBudget(extra);});
            const auto baseline=nativeQueryScratchBytes_;
            std::size_t retainedCapacity{},requestedCapacity{};
            queries.hitStorage=[&](QuestVr::OriginalHashHitStoragePhase phase,std::size_t oldCapacity,std::size_t newCapacity) {
                const auto oldBytes=Multiply(oldCapacity,sizeof(QuestVr::OriginalActorLineHit));
                const auto newBytes=Multiply(newCapacity,sizeof(QuestVr::OriginalActorLineHit));
                if(oldCapacity!=retainedCapacity || newCapacity<=oldCapacity ||
                    nativeQueryScratchBytes_!=QuestVr::DecalStateDetail::Add(baseline,oldBytes))
                    throw std::runtime_error("Encroachment collector lost its actual owning capacity/lexical scratch scope");
                if(phase==QuestVr::OriginalHashHitStoragePhase::BeforeReserve) {
                    if(requestedCapacity)throw std::runtime_error("Encroachment collector nested its pure reserve admission");
                    ValidateStateBudget(newBytes); // Existing ledger still owns oldBytes.
                    requestedCapacity=newCapacity;
                } else if(phase==QuestVr::OriginalHashHitStoragePhase::AfterReserve) {
                    if(!requestedCapacity || newCapacity<requestedCapacity)
                        throw std::runtime_error("Encroachment collector reported capacity without its reserve admission");
                    // Also check the ACTUAL old+new peak if the STL supplied
                    // more capacity than requested. No provider runs between
                    // reserve and this admission/publication of new ownership.
                    ValidateStateBudget(newBytes);
                    nativeQueryScratchBytes_=QuestVr::DecalStateDetail::Add(baseline,newBytes);
                    retainedCapacity=newCapacity;requestedCapacity=0u;
                } else throw std::runtime_error("Encroachment collector used an unknown storage phase");
            };
            // Both scopes join the existing outer hash/VM root. Restore the
            // raw pose BEFORE this query's result/admission/publication tail.
            auto hits=[&] {
                QuestVr::NativeActorPoseStack::Scope pose(nativeActorPoses_,reinterpret_cast<std::uintptr_t>(source),
                    {location,rotation});
                return domain.ActorEncroachmentCheck(level->nativeRetailHash,reinterpret_cast<std::uintptr_t>(source),queries);
            }();
            if(requestedCapacity || retainedCapacity!=hits.records.capacity())
                throw std::runtime_error("Encroachment collector returned unaccounted owning storage");
            ValidateStateBudget();
            return hits;
        });
    }
