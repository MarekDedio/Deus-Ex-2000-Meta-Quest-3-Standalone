// Included inside PortableActorVmHost. Original normal DestroyActor provider
// binding; every callback/field/hash/slot write belongs to this same root.
    void DestroyClearSlot(RuntimeObject* level,std::int32_t index) {
        static_cast<void>(ZoneLevel(reinterpret_cast<std::uintptr_t>(level)));
        if(!transaction_||index<0||static_cast<std::size_t>(index)>=persistentWorldActors.size())
            throw std::runtime_error("Original DestroyActor cannot clear an invalid native Level item");
        const auto slot=static_cast<std::size_t>(index);
        if(slot<beforeWorldSlots_&&std::none_of(beforeWorldSlotItems_.begin(),beforeWorldSlotItems_.end(),
            [slot](const auto& item){return item.first==slot;})) {
            if(beforeWorldSlotItems_.size()>=4096u)throw std::runtime_error("DestroyActor slot journal exceeds caller bound");
            if(beforeWorldSlotItems_.size()==beforeWorldSlotItems_.capacity()) {
                const auto old=beforeWorldSlotItems_.capacity();
                const auto next=std::min<std::size_t>(4096u,std::max<std::size_t>(8u,old*2u));
                // Existing capacity is already retained; reserve briefly owns
                // old+new buffers. No new transaction/work allowance here.
                ValidateStateBudget(next*sizeof(decltype(beforeWorldSlotItems_)::value_type));
                beforeWorldSlotItems_.reserve(next);
            }
            beforeWorldSlotItems_.emplace_back(slot,persistentWorldActors[slot]);
        }
        auto* actor=persistentWorldActors[slot];
        if(!actor)throw std::runtime_error("Original DestroyActor clearing a vanished saved Actor item");
        TouchLifecycle(actor);TouchCollision();
        persistentWorldActors[slot]=nullptr;actor->worldRemoved=true;
        persistentActorCollision.Remove(reinterpret_cast<std::uintptr_t>(actor));
        ValidateStateBudget();
    }
    std::int32_t DestroyNativeActor(RuntimeObject* level,RuntimeObject* actor,std::int32_t force,
        QuestVr::Vm::Execution& execution) {
        const auto levelId=reinterpret_cast<std::uintptr_t>(level),source=reinterpret_cast<std::uintptr_t>(actor);
        static_cast<void>(ZoneLevel(levelId));static_cast<void>(ZoneActor(source,level));
        const auto resolve=[this,level](std::uintptr_t identity){return ZoneActor(identity,level);};
        const auto retainLevel=[this,level,levelId](std::uintptr_t identity) {
            if(identity!=levelId)throw std::runtime_error("DestroyActor changed its retained native Level");
            return ZoneLevel(reinterpret_cast<std::uintptr_t>(level));
        };
        const auto standalone=[level] {
            if(!level->nativeStandaloneLevelServices)
                throw std::runtime_error("DestroyActor has no established standalone Level services");
        };
        QuestVr::OriginalDestroyActorQueries q;
        q.isValid=[resolve](std::uintptr_t identity){static_cast<void>(resolve(identity));return 1;};
        q.editor=[] {return 0;}; // This native host is a game executor, not an editor.
        q.actorFlags=[this,resolve](std::uintptr_t identity) {
            auto* current=resolve(identity);std::uint32_t flags{};
            if(NoiseField(current,"Engine.Actor.bStatic",Kind::Bool,3u).boolean)flags|=1u;
            if(NoiseField(current,"Engine.Actor.bNoDelete",Kind::Bool,3u).boolean)flags|=4u;
            if(NoiseField(current,"Engine.Actor.bDeleteMe",Kind::Bool,2u).boolean)flags|=0x80u;
            if(NoiseField(current,"Engine.Actor.bNetTemporary",Kind::Bool,3u).boolean)flags|=0x01000000u;
            return flags;
        };
        q.role=[this](std::uintptr_t identity){return RetailTouchRole(identity);};
        q.playerPawn=[this](std::uintptr_t identity){return RetailTouchClass(identity,"Engine.PlayerPawn")?1:0;};
        q.player=[this,resolve](std::uintptr_t identity) {
            auto* current=resolve(identity);
            const auto property=RetailPredicatePointerField(current,"Engine.PlayerPawn.Player",
                "Engine.PlayerPawn","Engine.Player",0x2u);
            // Native binding is an explicit authority, not a default for every
            // authored PlayerPawn. Its raw pointer must not become None just
            // because Read's public view hides a revoked native client lease.
            if(current->nativePlayerBinding)
                return reinterpret_cast<std::uintptr_t>(current->nativePlayer);
            const auto value=Read(current,property,0u);
            if(value.kind!=Kind::Object)
                throw std::runtime_error("DestroyActor declared Player is not an Object");
            ValidateObjectValue(value,"Engine.Player",false);
            return value.text.empty()?std::uintptr_t{}:reinterpret_cast<std::uintptr_t>(Object(value.text));
        };
        q.netConnection=[this](std::uintptr_t identity) {
            auto* player=reinterpret_cast<RuntimeObject*>(identity);
            if(!player||Object(player->reflection.objectPath)!=player||!player->nativeViewportIdentity)
                throw std::runtime_error("DestroyActor native Player has no authenticated Viewport/NetConnection service");
            if(!IsDerivedFromPath(player->cls,"Engine.Viewport"))
                throw std::runtime_error("DestroyActor Player is not the established native Viewport");
            return 0; // Real Viewport class is NOT a NetConnection.
        };
        q.getActorIndex=[this,retainLevel](std::uintptr_t identity,std::uintptr_t target) {
            static_cast<void>(retainLevel(identity));
            for(std::size_t slot=0u;slot<persistentWorldActors.size();++slot) {
                LifecycleStep();
                if(reinterpret_cast<std::uintptr_t>(persistentWorldActors[slot])==target) {
                    if(slot>static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()))
                        throw std::runtime_error("DestroyActor native slot exceeds signed index");
                    return static_cast<std::int32_t>(slot);
                }
            }
            return std::int32_t{-1};
        };
        q.undo=[] {return std::uintptr_t{};}; // No editor/GUndo service is installed by this host.
        q.modify=[resolve](std::uintptr_t identity) {
            static_cast<void>(resolve(identity));
            // Core10150170..1C2: with actual GUndo=None Modify writes nothing.
        };
        q.stateFrame=[resolve](std::uintptr_t identity) {
            auto* current=resolve(identity);
            if(current->stateObject&&current->stateObject->frameOverride)
                return current->stateObject->frame?reinterpret_cast<std::uintptr_t>(&*current->stateObject->frame):std::uintptr_t{};
            if(current->serializedStack)
                throw std::runtime_error("DestroyActor raw serialized state-frame service is not implemented");
            return std::uintptr_t{};
        };
        q.stateCode=[actor](std::uintptr_t frame) {
            if(!actor->stateObject||!actor->stateObject->frame||!actor->stateObject->frame->control||
                reinterpret_cast<std::uintptr_t>(&*actor->stateObject->frame)!=frame)
                throw std::runtime_error("DestroyActor state-frame Code has no live modern native authority");
            // A capability into the REAL owned running frame, not fabricated
            // executable bytes. The coordinator consumes only Code!=None.
            return actor->stateObject->frame->control->codePresent?
                reinterpret_cast<std::uintptr_t>(&actor->stateObject->frame->statementIndex):std::uintptr_t{};
        };
        q.isProbing=[resolve](std::uintptr_t identity,std::uint32_t name) {
            if(name!=0x13du)throw std::runtime_error("DestroyActor changed original EndState FName13D");
            const auto current=CurrentDispatchContext(resolve(identity));
            return QuestVr::ScriptDispatch::IsEnabled("EndState",current.classProbeMask,current.codeMasks,
                current.disabledNames,current.liveProbeMask)?1:0;
        };
        const auto eventKind=[](QuestVr::OriginalDestroyActorEvent event) {
            switch(event) {
            case QuestVr::OriginalDestroyActorEvent::EndState:return LifecycleEventKind::EndState;
            case QuestVr::OriginalDestroyActorEvent::Destroyed:return LifecycleEventKind::Destroyed;
            case QuestVr::OriginalDestroyActorEvent::LostChild:return LifecycleEventKind::LostChild;
            }
            throw std::runtime_error("DestroyActor requested an unknown selected event");
        };
        q.findFunctionChecked=[this,resolve,eventKind](std::uintptr_t identity,QuestVr::OriginalDestroyActorEvent event) {
            return reinterpret_cast<std::uintptr_t>(LifecycleFindEvent(resolve(identity),eventKind(event)));
        };
        q.processEvent=[this,resolve,&execution](std::uintptr_t identity,std::uintptr_t function,std::uintptr_t other) {
            auto* method=reinterpret_cast<RuntimeObject*>(function);
            if(!method||Object(method->reflection.objectPath)!=method)
                throw std::runtime_error("DestroyActor ProcessEvent retained a retired Function");
            const auto name=LowerAscii(method->reflection.objectPath.substr(method->reflection.objectPath.find_last_of('.')+1u));
            const auto kind=name=="endstate"?LifecycleEventKind::EndState:name=="destroyed"?
                LifecycleEventKind::Destroyed:name=="lostchild"?LifecycleEventKind::LostChild:
                throw std::runtime_error("DestroyActor ProcessEvent has a different Function identity");
            LifecycleProcessEvent(resolve(identity),method,kind,other?resolve(other):nullptr,execution);
        };
        q.pendingDelete=[this,resolve](std::uintptr_t identity) {
            return NoiseField(resolve(identity),"Engine.Actor.bDeleteMe",Kind::Bool,2u).boolean?1:0;
        };
        q.base=[this,resolve](std::uintptr_t identity) {
            const auto value=RetailPredicatePointerRead(resolve(identity),"Engine.Actor.Base","Engine.Actor",0x22u);
            return value.text.empty()?std::uintptr_t{}:reinterpret_cast<std::uintptr_t>(Object(value.text));
        };
        q.setBase=[this,resolve,&execution](std::uintptr_t identity,std::uintptr_t target,std::int32_t notify) {
            SetBase(resolve(identity),target?resolve(target):nullptr,notify!=0,execution);
        };
        q.standingCount=[this,resolve](std::uintptr_t identity){return ActorBaseCount(resolve(identity));};
        q.actorCount=[retainLevel](std::uintptr_t identity) {
            static_cast<void>(retainLevel(identity));
            if(persistentWorldActors.size()>static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()))
                throw std::runtime_error("DestroyActor native array exceeds signed count");
            return static_cast<std::int32_t>(persistentWorldActors.size());
        };
        q.actorSlot=[retainLevel](std::uintptr_t identity,std::int32_t slot) {
            static_cast<void>(retainLevel(identity));
            if(slot<0||static_cast<std::size_t>(slot)>=persistentWorldActors.size())
                throw std::runtime_error("DestroyActor native item is outside its live array");
            return reinterpret_cast<std::uintptr_t>(persistentWorldActors[static_cast<std::size_t>(slot)]);
        };
        q.levelHash=[retainLevel](std::uintptr_t identity){return retainLevel(identity)->nativeRetailHash;};
        q.collideActors=[this,resolve](std::uintptr_t identity){return RetailCollisionFlag(resolve(identity),"Engine.Actor.bCollideActors")?1:0;};
        q.removeFromHash=[this,level](std::uintptr_t hash,std::uintptr_t identity) {
            if(!hash||level->nativeRetailHash!=hash||!persistentRetailCollision||!persistentRetailCollision->ContainsHash(hash))
                throw std::runtime_error("DestroyActor RemoveActor has no actual retained retail Hash");
            auto* current=ZoneActor(identity,level);TouchRetailCollision();RefreshRetailCollisionActor(current);
            persistentRetailCollision->Remove(hash,identity,RetailCollisionQueries());
        };
        q.checkActorNotReferenced=[this,level](std::uintptr_t hash,std::uintptr_t identity) {
            static_cast<void>(ZoneActor(identity,level));
            if(!hash||hash!=level->nativeRetailHash||!persistentRetailCollision||!persistentRetailCollision->ContainsHash(hash))
                throw std::runtime_error("DestroyActor CheckActorNotReferenced lost original retail Hash identity");
            // Audited retail virtual+20 body is genuinely empty. Do not scan/
            // repair links or invent a success on an unsupported hash class.
        };
        q.owner=[this,resolve](std::uintptr_t identity) {
            const auto value=RetailPredicatePointerRead(resolve(identity),"Engine.Actor.Owner","Engine.Actor",0x22u);
            return value.text.empty()?std::uintptr_t{}:reinterpret_cast<std::uintptr_t>(Object(value.text));
        };
        q.setOwner=[this,resolve,&execution](std::uintptr_t identity,std::uintptr_t target) {
            SetOwner(resolve(identity),target?resolve(target):nullptr,execution);
        };
        q.touching=[this](std::uintptr_t identity,unsigned slot){return RetailTouchReadSlot(identity,slot);};
        q.endTouch=[this,&execution](std::uintptr_t identity,std::uintptr_t other,std::int32_t suppress) {
            QuestVr::EndOriginalActorTouch(identity,other,suppress,RetailActorTouchQueries(execution));
        };
        q.netDriver=[retainLevel,standalone](std::uintptr_t identity) {static_cast<void>(retainLevel(identity));standalone();return std::uintptr_t{};};
        q.demoRecDriver=q.netDriver;
        q.clearActorSlot=[this,retainLevel](std::uintptr_t identity,std::int32_t slot){DestroyClearSlot(retainLevel(identity),slot);};
        q.setPendingDelete=[this,resolve,&execution](std::uintptr_t identity) {
            auto* current=resolve(identity);const auto field=NoiseProperty(current,"Engine.Actor.bDeleteMe",Kind::Bool,2u);
            LifecycleRawActorStore(current,field,Value::Bool(true),execution);
            if(persistentRetailCollision&&persistentRetailCollision->ContainsActor(identity)) {
                TouchRetailCollision();const auto raw=persistentRetailCollision->ReadActor(identity);
                persistentRetailCollision->SetActorLive(identity,raw.location,raw.collideActors,true);
            }
        };
        q.engine=[retainLevel](std::uintptr_t identity){return reinterpret_cast<std::uintptr_t>(retainLevel(identity)->nativeLevelEngine);};
        q.audio=[](std::uintptr_t identity) {
            auto* engine=reinterpret_cast<RuntimeObject*>(identity);
            if(!engine||!ActualNativeCollisionObjectValid(engine)||!engine->nativeEngineInstance)
                throw std::runtime_error("DestroyActor Audio has no actual native Engine constructor storage");
            return reinterpret_cast<std::uintptr_t>(engine->nativeEngineInstance->values.audio);
        };
        q.audioActorDestroyed=[](std::uintptr_t,std::uintptr_t) {
            throw std::runtime_error("Original DestroyActor nonnull Audio notification is not implemented");
        };
        q.conditionalDestroy=[](std::uintptr_t)->std::int32_t {
            throw std::runtime_error("Original DestroyActor ConditionalDestroy/property/linker teardown is not implemented");
        };
        q.firstDeleted=[](std::uintptr_t)->std::uintptr_t {
            throw std::runtime_error("Original DestroyActor deferred deletion ownership is not implemented");
        };
        q.work=[this]{LifecycleStep();};
        return QuestVr::DestroyOriginalLevelActor(levelId,source,force,q);
    }
