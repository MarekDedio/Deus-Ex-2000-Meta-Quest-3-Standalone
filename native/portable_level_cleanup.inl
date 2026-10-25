// Included inside PortableActorVmHost after portable_world_actor_order.inl.
// Original ordinary-game Tick cleanup boundary, not forced/editor cleanup or
// scalar deletion. All native array changes borrow the enclosing frame root.
    void CleanupOriginalTickBoundary(RuntimeObject* level,QuestVr::Vm::Execution& execution) {
        static_cast<void>(execution); // Native array stores are not script writes.
        if(!transaction_)throw std::runtime_error("Original Tick cleanup lacks its caller-owned root");
        ActorOrderLevel(level);
        if(!level->nativeFirstDynamic||*level->nativeFirstDynamic<2u||
            *level->nativeFirstDynamic>persistentWorldActors.size()||
            persistentWorldActors.size()>QuestVr::WorldActorOrderDetail::MaximumSlots)
            throw std::runtime_error("Original CompactActors lacks the actual bounded native partition");

        // ULevel.CleanupDestroyed(force0), game mode: CompactActors virtual+7C
        // is called BEFORE FirstDeleted is read, including an empty queue.
        // CompactActors1039640A..103964F7 keeps the prefix and FirstDynamic,
        // scans the LIVE suffix/count, copies survivors stably and only then
        // removes its trailing range. It never repartitions by bStatic.
        std::size_t destination=*level->nativeFirstDynamic;
        for(std::size_t source=destination;source<persistentWorldActors.size();++source) {
            LifecycleStep();
            auto* actor=persistentWorldActors[source];
            if(!actor)continue;
            ZoneActor(reinterpret_cast<std::uintptr_t>(actor),level);
            if(actor->worldActorIndex!=static_cast<std::int32_t>(source))
                throw std::runtime_error("Original CompactActors encountered an inconsistent reverse slot");
            if(NoiseField(actor,"Engine.Actor.bDeleteMe",Kind::Bool,2u).boolean) {
                // Original diagnostic literal at1047F770 is "Undeleted %s".
                // This portable diagnostic uses the canonical object path;
                // it is not a new DestroyActor or selected script callback.
                __android_log_print(ANDROID_LOG_WARN,"quest_main","Undeleted %s",actor->reflection.objectPath.c_str());
                JournalNativeActorOrder(level);
                actor->worldActorIndex=-1;
                level->nativeActorOrderChanged=true;
                ValidateStateBudget();
                continue;
            }
            if(destination!=source) {
                JournalNativeActorOrder(level);
                persistentWorldActors[destination]=actor;
                actor->worldActorIndex=static_cast<std::int32_t>(destination);
                level->nativeActorOrderChanged=true;
                ValidateStateBudget();
            }
            ++destination;
        }
        if(destination!=persistentWorldActors.size()) {
            JournalNativeActorOrder(level);
            persistentWorldActors.resize(destination);
            level->nativeActorOrderChanged=true;
            ValidateStateBudget();
        }

        //103965DF..1039660B: count the actual native chain only after the
        // compaction prefix. No per-node bDeleteMe read belongs in this count.
        // Below128 and force0 the ORIGINAL keeps every deferred UObject alive.
        LifecycleStep();
        auto* queued=DeferredLevel(level)->nativeFirstDeleted;
        std::size_t count{};
        while(queued) {
            LifecycleStep();
            queued=DeferredActor(queued,level)->nativeDeleted;
            ++count;
        }
        if(count>=128u)
            throw std::runtime_error("Original CleanupDestroyed class-reference/scalar teardown is not implemented");
        // Intentionally no head/link/lookup/index/AI mutation on this path.
        // Queue capture/rebind support remains a separate explicit guard.
    }
