// Included inside PortableActorVmHost. Original LoadMap's native partition,
// not the later dynamic-only CompactActors or a renderer iteration shortcut.
    RuntimeObject* ActorOrderLevel(RuntimeObject* level) {
        if(level!=CurrentClockLevel()||!level->nativeMapGeneration||
            level->nativeMapGeneration!=persistentMapGenerationCounter)
            throw std::runtime_error("Native Actor ordering lacks current loader-owned Level authority");
        return level;
    }
    bool NativeActorStatic(RuntimeObject* actor) {
        if(!actor||!actor->objectIndex||Object(actor->reflection.objectPath)!=actor||
            actor->nativeXLevel!=CurrentClockLevel())
            throw std::runtime_error("Native Actor ordering encountered a different live map identity");
        const auto field=DescribeProperty("Engine.Actor.bStatic");auto* metadata=Object(field.key);
        if(field.zero.kind!=Kind::Bool||field.arrayDimension!=1u||field.flags!=3u||
            !OwnsProperty(actor->cls,field)||metadata->outer!=Object("Engine.Actor")||
            !metadata->property||metadata->property->type!="BoolProperty")
            throw std::runtime_error("Native Actor ordering lacks exact Engine.Actor.bStatic authority");
        const auto value=Read(actor,field,0u);
        if(value.kind!=Kind::Bool)throw std::runtime_error("Native bStatic storage is not Bool");
        return value.boolean;
    }
    PortableWorldActorOrderSnapshot CaptureNativeActorOrder(RuntimeObject* level,bool charge) {
        if(charge)LifecycleStep();
        ActorOrderLevel(level);PortableWorldActorOrderSnapshot result;
        const auto count=persistentWorldActors.size();
        if(count>QuestVr::WorldActorOrderDetail::MaximumSlots)
            throw std::runtime_error("Native Actor order observation exceeds slot policy");
        const auto estimate=sizeof(result)+level->reflection.objectPath.size()+1u+count*5u;
        if(charge)ValidateStateBudget(estimate);
        result.levelPath=level->reflection.objectPath;result.firstDynamic=level->nativeFirstDynamic;
        result.actorIndices.reserve(count);result.staticFlags.reserve(count);
        const auto actual=sizeof(result)+result.levelPath.capacity()+1u+
            result.actorIndices.capacity()*sizeof(std::uint32_t)+result.staticFlags.capacity();
        if(charge){ValidateStateBudget(actual);nativeActorOrderObservationBytes_+=actual;}
        for(std::size_t index=0u;index<count;++index) {
            if(charge)LifecycleStep();
            auto* actor=persistentWorldActors[index];
            if(actor&&actor->worldActorIndex!=static_cast<std::int32_t>(index))
                throw std::runtime_error("Native Actor slot/cache identities disagree");
            result.actorIndices.push_back(actor?actor->objectIndex.Index():0u);
            result.staticFlags.push_back(actor&&NativeActorStatic(actor)?1u:0u);
        }
        result.available=true;return result;
    }
    void JournalNativeActorOrder(RuntimeObject* level) {
        if(!transaction_)throw std::runtime_error("Native Actor order mutation lacks its caller-owned root");
        ActorOrderLevel(level);
        if(beforeActorOrder_) {
            if(beforeActorOrder_->level!=level)throw std::runtime_error("Native Actor order root crossed Levels");
            return;
        }
        BeforeActorOrder before;before.level=level;before.firstDynamic=level->nativeFirstDynamic;
        before.changed=level->nativeActorOrderChanged;
        std::size_t actors{};
        const auto& exports=persistentRuntime->get()->exports;
        if(persistentScriptExportCount>exports.size())throw std::runtime_error("Native Actor journal lacks its map export suffix");
        for(std::size_t i=persistentScriptExportCount;i<exports.size();++i) {
            LifecycleStep();if(exports[i]&&exports[i]->nativeXLevel==level)++actors;
        }
        if(actors>QuestVr::WorldActorOrderDetail::MaximumSlots)
            throw std::runtime_error("Native Actor reverse-index journal exceeds policy");
        ValidateStateBudget(sizeof(before)+persistentWorldActors.size()*sizeof(RuntimeObject*)+
            actors*sizeof(decltype(before.indices)::value_type));
        before.slots.reserve(persistentWorldActors.size());before.indices.reserve(actors);
        ValidateStateBudget(sizeof(before)+before.slots.capacity()*sizeof(RuntimeObject*)+
            before.indices.capacity()*sizeof(decltype(before.indices)::value_type));
        for(auto* actor:persistentWorldActors){LifecycleStep();before.slots.push_back(actor);}
        for(std::size_t i=persistentScriptExportCount;i<exports.size();++i) {
            LifecycleStep();auto* actor=exports[i];
            if(actor&&actor->nativeXLevel==level)before.indices.emplace_back(actor,actor->worldActorIndex);
        }
        beforeActorOrder_.emplace(std::move(before));
    }
    void ReorderNativeWorldActors(RuntimeObject* level) {
        ActorOrderLevel(level);
        QuestVr::OriginalWorldActorOrderQueries q;
        const auto previousScratch=nativeQueryScratchBytes_;
        struct ScratchScope {std::size_t& bytes;std::size_t previous;~ScratchScope(){bytes=previous;}} scratch{nativeQueryScratchBytes_,previousScratch};
        ValidateStateBudget(sizeof(q));nativeQueryScratchBytes_+=sizeof(q);
        const auto owner=[this,level](std::uintptr_t identity) {
            if(identity!=reinterpret_cast<std::uintptr_t>(level))
                throw std::runtime_error("Native Actor partition changed its retained Level");
            return ActorOrderLevel(level);
        };
        q.actorCount=[owner](std::uintptr_t identity) {
            static_cast<void>(owner(identity));return static_cast<std::int32_t>(persistentWorldActors.size());
        };
        q.actorSlot=[owner](std::uintptr_t identity,std::int32_t index) {
            static_cast<void>(owner(identity));
            if(index<0||static_cast<std::size_t>(index)>=persistentWorldActors.size())
                throw std::runtime_error("Native Actor partition slot is outside its live array");
            return reinterpret_cast<std::uintptr_t>(persistentWorldActors[static_cast<std::size_t>(index)]);
        };
        q.isStatic=[this](std::uintptr_t identity){return NativeActorStatic(reinterpret_cast<RuntimeObject*>(identity));};
        q.writeFirstDynamic=[this,level](std::uintptr_t identity,std::int32_t index) {
            if(identity!=reinterpret_cast<std::uintptr_t>(level))throw std::runtime_error("Native partition split has a different Level");
            auto* target=ActorOrderLevel(level);JournalNativeActorOrder(target);
            if(index<2||static_cast<std::size_t>(index)>QuestVr::WorldActorOrderDetail::MaximumSlots)
                throw std::runtime_error("Native first-dynamic index is outside its bounded partition");
            target->nativeFirstDynamic=static_cast<std::uint32_t>(index);target->nativeActorOrderChanged=true;
            ValidateStateBudget();
        };
        q.replaceSlots=[this,level](std::uintptr_t identity,std::span<const std::uintptr_t> values) {
            if(identity!=reinterpret_cast<std::uintptr_t>(level))throw std::runtime_error("Native partition replacement has a different Level");
            auto* target=ActorOrderLevel(level);JournalNativeActorOrder(target);
            if(values.size()<2u||values.size()>QuestVr::WorldActorOrderDetail::MaximumSlots||
                !target->nativeFirstDynamic||*target->nativeFirstDynamic>values.size())
                throw std::runtime_error("Native Actor replacement lacks its actual partition split");
            ValidateStateBudget(sizeof(std::vector<RuntimeObject*>)+values.size()*sizeof(RuntimeObject*));
            std::vector<RuntimeObject*> next;next.reserve(values.size());
            ValidateStateBudget(sizeof(next)+next.capacity()*sizeof(RuntimeObject*));
            for(const auto value:values){LifecycleStep();next.push_back(reinterpret_cast<RuntimeObject*>(value));}
            // Cached adapter indices mirror native membership; they are not
            // UObject indices, source export ordinals or lookup/hash ordering.
            for(const auto& row:beforeActorOrder_->indices){LifecycleStep();row.first->worldActorIndex=-1;}
            for(std::size_t index=0u;index<next.size();++index) {
                LifecycleStep();if(next[index])next[index]->worldActorIndex=static_cast<std::int32_t>(index);
            }
            persistentWorldActors.swap(next);ValidateStateBudget();
        };
        q.admitTemporaryBytes=[this](std::size_t bytes){ValidateStateBudget(bytes);};
        q.work=[this]{LifecycleStep();};
        QuestVr::ReorderOriginalWorldActors(reinterpret_cast<std::uintptr_t>(level),q,nativeQueryScratchBytes_);
    }
