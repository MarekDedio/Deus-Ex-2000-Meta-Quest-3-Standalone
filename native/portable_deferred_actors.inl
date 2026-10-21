// Included inside PortableActorVmHost. Original ordinary-game deferred edges,
// not a replacement for CleanupDestroyed, scalar deletion or an archive.
    Property NativeDeletedField(RuntimeObject* actor) {
        return RetailPredicatePointerField(actor,"Engine.Actor.Deleted","Engine.Actor","Engine.Actor",0x2u);
    }
    void BindNativeDeleted(RuntimeObject* actor) {
        if(actor->nativeDeletedBound)return;
        // Metadata-only generated hosts may omit this native declaration.
        // They remain explicitly UNBOUND; destruction cannot infer a None.
        if(!persistentVmObjects.contains("engine.actor.deleted"))return;
        const auto field=NativeDeletedField(actor);const auto value=Read(actor,field,0u);
        if(value.kind!=Kind::Object)throw std::runtime_error("Initialized Actor.Deleted is not its declared Object payload");
        ValidateObjectValue(value,"Engine.Actor",false);
        actor->nativeDeleted=value.text.empty()?nullptr:Object(value.text);actor->nativeDeletedBound=true;
    }
    RuntimeObject* DeferredLevel(RuntimeObject* level) {
        if(!level||Object(level->reflection.objectPath)!=level||!level->nativeFirstDeletedBound||
            level->sourcePath!=persistentMapSourcePath||level->nativeMapGeneration!=persistentMapGenerationCounter||
            LowerAscii(CastObjectIdentity(level->reflection.objectPath).classPath)!="engine.level")
            throw std::runtime_error("Deferred deletion has no actual serialized current Level binding");
        return level;
    }
    RuntimeObject* DeferredActor(RuntimeObject* actor,RuntimeObject* level) {
        if(!actor||Object(actor->reflection.objectPath)!=actor||!actor->nativeDeletedBound||
            actor->nativeXLevel!=level||!IsDerivedFromPath(actor->cls,"Engine.Actor"))
            throw std::runtime_error("Deferred deletion link has no bound current-Level Actor identity");
        static_cast<void>(NativeDeletedField(actor));return actor;
    }
    std::uintptr_t FirstDeleted(RuntimeObject* level) {
        return reinterpret_cast<std::uintptr_t>(DeferredLevel(level)->nativeFirstDeleted);
    }
    void JournalDeferred(RuntimeObject* object) {
        if(!transaction_)throw std::runtime_error("Native deferred store is outside its caller root");
        if(std::none_of(beforeDeferred_.begin(),beforeDeferred_.end(),[&](const auto& row){return row.object==object;})) {
            if(beforeDeferred_.size()>=4096u)throw std::runtime_error("Native deferred journal exceeds caller bound");
            if(beforeDeferred_.size()==beforeDeferred_.capacity()) {
                const auto next=std::min<std::size_t>(4096u,std::max<std::size_t>(8u,beforeDeferred_.capacity()*2u));
                ValidateStateBudget(next*sizeof(BeforeDeferred));beforeDeferred_.reserve(next);
            }
            beforeDeferred_.push_back({object,object->nativeDeleted,object->nativeFirstDeleted,object->nativeDeferredChanged});
        }
    }
    void StoreDeleted(RuntimeObject* actor,RuntimeObject* value,RuntimeObject* level) {
        DeferredActor(actor,DeferredLevel(level));if(value)DeferredActor(value,level);
        JournalDeferred(actor);Touch(actor);actor->nativeDeleted=value;actor->nativeDeferredChanged=true;ValidateStateBudget();
    }
    void StoreFirstDeleted(RuntimeObject* level,RuntimeObject* value) {
        DeferredLevel(level);if(value)DeferredActor(value,level);
        JournalDeferred(level);Touch(level);level->nativeFirstDeleted=value;level->nativeDeferredChanged=true;ValidateStateBudget();
    }
    PortableDeferredOwnershipSnapshot DeferredSnapshot() {
        PortableDeferredOwnershipSnapshot result;
        if(persistentWorldActors.empty()||!persistentWorldActors.front()||!persistentWorldActors.front()->nativeXLevel)return result;
        auto* level=persistentWorldActors.front()->nativeXLevel;
        if(!level->nativeFirstDeletedBound)return result;
        DeferredLevel(level);result.available=true;result.levelPath=level->reflection.objectPath;
        if(level->nativeFirstDeleted)result.firstDeletedPath=level->nativeFirstDeleted->reflection.objectPath;
        for(std::size_t index=persistentScriptExportCount;index<persistentRuntime->get()->exports.size();++index) {
            auto* actor=persistentRuntime->get()->exports[index];
            if(actor->nativeXLevel!=level||!actor->nativeDeletedBound)continue;
            result.actorLinks.emplace_back(actor->reflection.objectPath,actor->nativeDeleted?
                actor->nativeDeleted->reflection.objectPath:std::string{});
        }
        return result;
    }
