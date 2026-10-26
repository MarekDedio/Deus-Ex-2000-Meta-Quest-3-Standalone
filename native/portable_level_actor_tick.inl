// Included in PortableActorVmHost after the original Actor phases and native
// Level ordering/deferred/cleanup services. One caller Execution owns all of
// these operations; no public per-Actor transaction is opened by the loop.
    RuntimeObject* OriginalTickLevel() {
        auto* level=ActorOrderLevel(CurrentClockLevel());
        if(!level->nativeLevelTickContext)
            throw std::runtime_error("Original Level Tick has no constructed native context");
        const auto& tick=*level->nativeLevelTickContext;
        if((tick.parity!=0&&tick.parity!=1)||(tick.inTick!=0&&tick.inTick!=1))
            throw std::runtime_error("Original Level Tick context has an unsupported continuation");
        return level;
    }
    void StoreOriginalTickContext(RuntimeObject* level,std::int32_t parity,std::int32_t inTick) {
        if(!transaction_||level!=OriginalTickLevel()||(parity!=0&&parity!=1)||(inTick!=0&&inTick!=1))
            throw std::runtime_error("Original Level Tick context store lacks its caller authority");
        if(beforeLevelTickContext_&&beforeLevelTickContext_->first!=level)
            throw std::runtime_error("Original Level Tick root crossed native Levels");
        if(!beforeLevelTickContext_) {
            ValidateStateBudget(sizeof(*beforeLevelTickContext_));
            beforeLevelTickContext_=std::pair{level,*level->nativeLevelTickContext};
        }
        *level->nativeLevelTickContext={parity,inTick};
        ValidateStateBudget();
    }
    void RetireOriginalTickLease() noexcept {
        nativePendingTickHead_=NoPendingTick;
        activeTickStats_=nullptr;activeTickLevel_=nullptr;
        nativePendingTicks_.clear(); // Includes roots for detached batches.
    }
    void EnqueueOriginalTickActor(RuntimeObject* level,RuntimeObject* actor) {
        LifecycleStep();
        if(!transaction_||level!=activeTickLevel_||!level->nativeLevelTickContext||
            !level->nativeLevelTickContext->inTick||!activeTickStats_)
            throw std::runtime_error("Original Tick pending insertion lacks its active native Level lease");
        actor=LifecycleActor(reinterpret_cast<std::uintptr_t>(actor));
        if(actor->nativeXLevel!=level)
            throw std::runtime_error("Original Tick pending insertion crossed Actor.Level ownership");
        const auto count=nativePendingTicks_.size();
        if(count>=QuestVr::WorldActorOrderDetail::MaximumSlots)
            throw std::runtime_error("Original Tick pending arena exceeds its bounded caller policy");
        if(count==nativePendingTicks_.capacity()) {
            const auto capacity=nativePendingTicks_.capacity();
            const auto next=std::min<std::size_t>(QuestVr::WorldActorOrderDetail::MaximumSlots,
                std::max<std::size_t>(8u,capacity*2u));
            // reserve briefly owns BOTH buffers. The old capacity is already
            // retained by ValidateStateBudget; admit the entire new buffer.
            ValidateStateBudget(next*sizeof(NativePendingTick)+sizeof(GCRootNode));
            nativePendingTicks_.reserve(next);
        }
        ValidateStateBudget(sizeof(GCRootNode));
        nativePendingTicks_.emplace_back(actor,nativePendingTickHead_);
        nativePendingTickHead_=count;
        ++activeTickStats_->deferredPushes;
        ValidateStateBudget();
    }
    bool OriginalTickMarker(RuntimeObject* actor) {
        const auto property=OriginalAnimationField(actor,"Engine.Actor","bTicked",Kind::Bool,0x2002u,"BoolProperty");
        return OriginalAnimationRead(actor,property,Kind::Bool).boolean;
    }
    QuestVr::OriginalActorPawnContext OriginalTickPawnContext(RuntimeObject* actor) {
        const auto source=reinterpret_cast<std::uintptr_t>(actor);
        const auto require=[&](std::uintptr_t identity) {
            if(identity!=source)throw std::runtime_error("Original Tick Pawn context changed its retained Actor");
            return LifecycleActor(identity);
        };
        QuestVr::OriginalActorPawnContextQueries q;q.work=[this]{LifecycleStep();};
        q.pawnFlag=[&](auto a) {
            auto* receiver=require(a);return OriginalAnimationRead(receiver,OriginalAnimationField(receiver,
                "Engine.Actor","bIsPawn",Kind::Bool,2u,"BoolProperty"),Kind::Bool).boolean;
        };
        q.actorClass=[&](auto a){return reinterpret_cast<std::uintptr_t>(require(a)->cls);};
        q.nativePawnClass=[&] {
            auto* target=Object("Engine.Pawn");
            if(!target->classDescriptor||target->defaultOwner||!(target->reflection.flags&0x04000000u)||
                target->base!=Object("Engine.Actor"))
                throw std::runtime_error("Original Tick Pawn cast lost its actual native Class ancestry");
            return reinterpret_cast<std::uintptr_t>(target);
        };
        q.superClass=[&](auto identity) {
            auto* cls=reinterpret_cast<RuntimeObject*>(identity);
            if(!cls||!cls->classDescriptor||cls->defaultOwner)
                throw std::runtime_error("Original Tick Pawn cast lacks actual Class/Super metadata");
            return reinterpret_cast<std::uintptr_t>(cls->base);
        };
        q.role=[&](auto a) {
            auto* receiver=require(a);const auto field=NoiseProperty(receiver,"Engine.Actor.Role",Kind::Byte,
                0x20u,"Engine.Actor.ENetRole");
            return static_cast<std::uint8_t>(OriginalAnimationRead(receiver,field,Kind::Byte).integer);
        };
        return QuestVr::CaptureOriginalActorPawnContext(source,q);
    }
    std::int32_t AdvanceOriginalActorTick(RuntimeObject* actor,float elapsed,std::int32_t tickType,
        QuestVr::Vm::Execution& execution) {
        LifecycleStep();
        actor=LifecycleActor(reinterpret_cast<std::uintptr_t>(actor));
        if(!activeTickLevel_||actor->nativeXLevel!=activeTickLevel_||!activeTickStats_)
            throw std::runtime_error("Original Actor Tick has no enclosing native Level phase");
        // Select the real virtual BEFORE base stasis, owner or parity work.
        // ScriptedPawn owns a custom prefix; its qualified AActor call below
        // is not a new virtual dispatch and does not authorize Pawn physics.
        auto* selected=OriginalActorTickNativeClass(actor);
        if(selected->reflection.objectPath=="DeusEx.ScriptedPawn") {
            ObserveOriginalActorTickBoundary(actor,selected->reflection.objectPath);
            ++activeTickStats_->actorCalls;
            return AdvanceOriginalScriptedPawnTick(actor,elapsed,tickType,execution);
        }
        RequireOriginalActorTickPhysicsImplementation(actor);
        ++activeTickStats_->actorCalls;
        return AdvanceOriginalActorBaseTick(actor,elapsed,tickType,execution);
    }
    std::int32_t AdvanceOriginalActorBaseTick(RuntimeObject* actor,float elapsed,std::int32_t tickType,
        QuestVr::Vm::Execution& execution) {
        // Private qualified AActor::Tick entry, used ONLY by the authenticated
        // native dispatcher and ScriptedPawn wrapper in the same Level root.
        actor=LifecycleActor(reinterpret_cast<std::uintptr_t>(actor));
        if(!transaction_||!activeTickLevel_||actor->nativeXLevel!=activeTickLevel_||!activeTickStats_)
            throw std::runtime_error("Qualified original Actor Tick lost its enclosing native Level phase");
        if(OriginalActorStasis(actor).inStasis) {
            const bool transient=OriginalAnimationRead(actor,OriginalAnimationField(actor,
                "Engine.Actor","bTransient",Kind::Bool,1u,"BoolProperty"),Kind::Bool).boolean;
            if(transient&&OriginalAnimationRead(actor,OriginalAnimationField(actor,
                "Engine.Actor","Role",Kind::Byte,0x20u,"ByteProperty"),Kind::Byte).integer>=2)
                static_cast<void>(DestroyNativeActor(activeTickLevel_,actor,0,execution));
            ++activeTickStats_->stasisReturns;
            return 1; // No bTicked assignment belongs on this return.
        }
        const auto owner=RetailPredicatePointerRead(actor,"Engine.Actor.Owner","Engine.Actor",0x22u);
        if(!owner.text.empty()) {
            auto* parent=LifecycleActor(reinterpret_cast<std::uintptr_t>(Object(owner.text)));
            if(static_cast<std::int32_t>(OriginalTickMarker(parent))!=activeTickLevel_->nativeLevelTickContext->parity) {
                EnqueueOriginalTickActor(activeTickLevel_,actor);
                return 0;
            }
        }
        const auto marker=OriginalAnimationField(actor,"Engine.Actor","bTicked",Kind::Bool,0x2002u,"BoolProperty");
        ReferenceFor(actor,marker,0u,false)->write(Value::Bool(activeTickLevel_->nativeLevelTickContext->parity!=0));
        const auto pawn=OriginalTickPawnContext(actor);
        const auto main=AdvanceOriginalMainAnimation(actor,elapsed,pawn.suppressAnimEnd,execution);
        static_cast<void>(AdvanceOriginalBlendAnimation(actor,main,execution));
        if(pawn.pawn||IsDerivedFromPath(actor->cls,"Engine.Pawn"))
            throw std::runtime_error("Original Actor Tick Pawn continuation is not implemented");
        static_cast<void>(AdvanceOriginalTickTail(actor,elapsed,tickType,execution));
        return 1;
    }
    Value AdvanceOriginalLevelDynamicActors(float elapsed,std::int32_t tickType,
        PortableLevelDynamicTickResult& result,QuestVr::Vm::Execution& execution) {
        if(!transaction_)throw std::runtime_error("Original Level actor phase requires its caller transaction");
        auto* level=OriginalTickLevel();
        if(level->nativeLevelTickContext->inTick||activeTickLevel_||!nativePendingTicks_.empty())
            throw std::runtime_error("Original Level actor phase cannot reenter an active frame");
        if(!level->nativeFirstDynamic||*level->nativeFirstDynamic<2u||
            *level->nativeFirstDynamic>persistentWorldActors.size())
            throw std::runtime_error("Original Level actor phase lacks the actual native FirstDynamic partition");
        result.parityBefore=level->nativeLevelTickContext->parity;
        // Before any callbacks/births, retain the original slot/reverse-index
        // continuation for later cleanup. It shares this root, not a snapshot
        // used for iteration (the loop below still reads live slots/count).
        JournalNativeActorOrder(level);
        StoreOriginalTickContext(level,result.parityBefore,1);
        activeTickLevel_=level;activeTickStats_=&result;
        nativePendingTickHead_=NoPendingTick;
        struct LeaseScope {
            PortableActorVmHost& host;
            ~LeaseScope(){host.RetireOriginalTickLease();}
        } lease{*this};
        std::uint32_t returns{};
        for(std::size_t slot=*level->nativeFirstDynamic;slot<persistentWorldActors.size();++slot) {
            LifecycleStep();++result.slotsVisited;
            auto* actor=persistentWorldActors[slot];
            if(actor)returns+=static_cast<std::uint32_t>(AdvanceOriginalActorTick(actor,elapsed,tickType,execution));
        }
        while(nativePendingTickHead_!=NoPendingTick&&returns!=0u) {
            LifecycleStep();++result.pendingPasses;
            auto node=nativePendingTickHead_;
            nativePendingTickHead_=NoPendingTick;returns=0u;
            while(node!=NoPendingTick) {
                LifecycleStep();
                if(node>=nativePendingTicks_.size())
                    throw std::runtime_error("Original Tick pending batch lost its retained scratch node");
                // Copy both before a callback can grow/relocate the arena.
                auto* actor=nativePendingTicks_[node].actor.get();
                const auto next=nativePendingTicks_[node].next;
                if(static_cast<std::int32_t>(OriginalTickMarker(actor))!=level->nativeLevelTickContext->parity)
                    returns+=static_cast<std::uint32_t>(AdvanceOriginalActorTick(actor,elapsed,tickType,execution));
                else ++result.pendingSkips;
                node=next;
            }
        }
        result.parityAfter=level->nativeLevelTickContext->parity==0?1:0;
        StoreOriginalTickContext(level,result.parityAfter,0);
        RetireOriginalTickLease(); // Source restores scratch before cleanup.
        CleanupOriginalTickBoundary(level,execution);
        result.completed=true;
        return Value::Bool(true);
    }
