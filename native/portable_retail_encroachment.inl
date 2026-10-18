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
        // Retain the live caller reservation through every nested admission,
        // including Model workspace checks. Separate preflights alone would
        // miss an overlapping arena+workspace peak. Nested scopes reuse the
        // same fixed pose slots, but own separate hit-vector growth envelopes.
        const auto scratchBytes=QuestVr::DecalStateDetail::Add(nativeActorPoses_.Depth()?0u:sizeof(nativeActorPoses_),
            Multiply(persistentRetailCollision->ReadUsage().actors,3u*sizeof(QuestVr::OriginalActorLineHit)));
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
            // Both scopes join the existing outer hash/VM root. Restore the
            // raw pose BEFORE this query's result/admission/publication tail.
            auto hits=[&] {
                QuestVr::NativeScratchScope scratch(nativeQueryScratchBytes_,scratchBytes,
                    [this](std::size_t extra){ValidateStateBudget(extra);});
                QuestVr::NativeActorPoseStack::Scope pose(nativeActorPoses_,reinterpret_cast<std::uintptr_t>(source),
                    {location,rotation});
                return domain.ActorEncroachmentCheck(level->nativeRetailHash,reinterpret_cast<std::uintptr_t>(source),queries);
            }();
            ValidateStateBudget(Multiply(hits.records.capacity(),sizeof(QuestVr::OriginalActorLineHit)));
            return hits;
        });
    }
