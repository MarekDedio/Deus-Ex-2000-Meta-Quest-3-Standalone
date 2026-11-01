    void ProbeInitialInputResetPrefix(RuntimeObject* viewport,
        PortableWorldStartupProbeResult::InputResetPrefixObservation& observed,QuestVr::Vm::Execution& execution) {
        // Explicit initial-GameEngine primitive diagnostic after real Spawn,
        // not an Init silently inserted into the later LoadMap.ResetInput site.
        if(!transaction_||!viewport||NativeSessionViewport(viewport->nativeViewportIdentity)!=viewport||
            !viewport->nativeViewportActor||!viewport->nativeViewportActor->nativePlayerBinding||
            viewport->nativeViewportActor->nativePlayer!=viewport||!viewport->nativeViewportInput||
            !ActualNativeInputInstanceValid(viewport->nativeViewportInput))
            throw std::runtime_error("Original Input.Init requires actual spawned Pawn/Viewport/Input ownership");
        auto* input=viewport->nativeViewportInput;
        if(input->nativeInputInstance->initialized||input->nativeInputInstance->values.viewport)
            throw std::runtime_error("Initial Input diagnostic requires its actual constructor-only instance");
        if(!viewport->nativeViewportConsole||!viewport->nativeViewportConsole->nativeConsoleObject)
            throw std::runtime_error("Initial Input diagnostic lacks its actual initialized Console");
        RequireNativeViewportConsole(viewport->nativeViewportConsole);
        observed.attempted=true;static_cast<void>(MutateNativeInputValues(input));
        auto& protocol=*input->nativeInputInstance;
        // Explicit diagnostic stress values in the real COW instance, NOT an
        // original constructor claim or a fabricated Pawn/input provider.
        protocol.keys.fill(0xa5u);protocol.action=7;protocol.delta=-3.0f;
        const auto previous=nativeQueryScratchBytes_;std::size_t frames{};
        struct Restore {std::size_t& ledger;std::size_t before;~Restore(){ledger=before;}} restore{nativeQueryScratchBytes_,previous};
        const auto check=[&] {
            if(!transaction_||input->nativeInputInstance.get()!=&protocol||protocol.retired||protocol.initialized||
                protocol.ownerViewport!=viewport||viewport->nativeViewportInput!=input||
                NativeSessionViewport(protocol.viewportIdentity)!=viewport||
                protocol.clientIdentity!=persistentNativeClient.ClientIdentity()||
                protocol.lookupGeneration!=persistentObjectLookup->generation)
                throw std::runtime_error("Initial Input private root/payload lease changed");
        };
        if(lexicalCommandInput_||lexicalCommandCheck_)
            throw std::runtime_error("Initial Input command lease cannot nest another initialization");
        const std::function<void()> commandCheck=check;
        lexicalCommandInput_=input;lexicalCommandCheck_=&commandCheck;
        struct EndCommandLease {
            RuntimeObject*& input;const std::function<void()>*& check;
            ~EndCommandLease(){input=nullptr;check=nullptr;}
        } endCommandLease{lexicalCommandInput_,lexicalCommandCheck_};
        const auto native=[&](std::uintptr_t identity)->RuntimeObject* {
            check();if(!identity)throw std::runtime_error("Original input native receiver is None");
            return reinterpret_cast<RuntimeObject*>(identity);
        };
        const auto actualInput=[&](std::uintptr_t identity) {
            if(native(identity)!=input)throw std::runtime_error("Original input operation changed its actual receiver");
        };
        const auto actor=[&](std::uintptr_t identity) {
            if(native(identity)!=viewport||protocol.values.viewport!=viewport)
                throw std::runtime_error("Original Input reset lost its actual native Viewport");
            auto* pawn=viewport->nativeViewportActor;
            if(!pawn||!pawn->nativePlayerBinding||pawn->nativePlayer!=viewport||pawn->nativeXLevel!=CurrentClockLevel())
                throw std::runtime_error("Original Input reset has no current actual Pawn/Viewport backedge");
            RequireMapActor(pawn);return pawn;
        };
        const auto cls=[&](std::uintptr_t identity) {
            auto* value=native(identity);
            if(!value->classDescriptor||!value->classFieldLinksValid||value->reflection.metaClass!="Class")
                throw std::runtime_error("Original Input property iterator lacks actual linked Class authority");
            return value;
        };
        const auto linked=[&](RuntimeObject* owner,std::int32_t reference)->RuntimeObject* {
            if(!reference)return nullptr;
            auto* field=Object(Qualified(Table(owner->sourcePath),reference));
            if(field->outer!=owner||(field->property&&field->linkedClassOwner!=owner))
                throw std::runtime_error("Original Input Children/Next declaration ownership changed");
            return field;
        };
        QuestVr::OriginalInputInitializeQueries q;
        q.requireOuterRoot=check;q.work=[this]{LifecycleStep();};
        q.enterScratch=[&](std::size_t bytes){const auto next=QuestVr::DecalStateDetail::Add(frames,bytes);
            nativeQueryScratchBytes_=previous;ValidateStateBudget(next);
            frames=next;nativeQueryScratchBytes_=QuestVr::DecalStateDetail::Add(previous,frames);};
        q.leaveScratch=[&](std::size_t bytes)noexcept{if(bytes>frames)std::terminate();frames-=bytes;nativeQueryScratchBytes_=previous+frames;};
        q.inputViewport=[&](std::uintptr_t identity){actualInput(identity);return reinterpret_cast<std::uintptr_t>(protocol.values.viewport);};
        q.writeInputViewport=[&](std::uintptr_t identity,std::uintptr_t target){actualInput(identity);
            if(native(target)!=viewport)throw std::runtime_error("Original Input.Init supplied another Viewport");
            protocol.values.viewport=viewport;observed.viewportStored=true;};
        q.writeKey=[&](std::uintptr_t identity,std::size_t index,std::uint8_t value){actualInput(identity);
            if(index>=protocol.keys.size()||value)throw std::runtime_error("Original Input reset changed its key byte store");
            protocol.keys[index]=value;++observed.keysCleared;};
        q.viewportActor=[&](std::uintptr_t identity){return reinterpret_cast<std::uintptr_t>(actor(identity));};
        q.actorClass=[&](std::uintptr_t identity){auto* pawn=native(identity);RequireMapActor(pawn);
            return reinterpret_cast<std::uintptr_t>(cls(reinterpret_cast<std::uintptr_t>(pawn->cls)));};
        q.classChildren=[&](std::uintptr_t identity){auto* owner=cls(identity);
            return reinterpret_cast<std::uintptr_t>(linked(owner,owner->classDescriptor->state.children));};
        q.classSuper=[&](std::uintptr_t identity){auto* owner=cls(identity);
            if(owner->base)static_cast<void>(cls(reinterpret_cast<std::uintptr_t>(owner->base)));
            return reinterpret_cast<std::uintptr_t>(owner->base);};
        q.fieldNext=[&](std::uintptr_t identity){auto* field=native(identity);auto* owner=cls(reinterpret_cast<std::uintptr_t>(field->outer));
            std::int32_t next{};
            if(field->property)next=field->property->nextField;
            else if(field->script)next=field->script->nextField;
            else if(const auto* metadata=StateMetadata(field))next=metadata->nextField;
            else {if(!field->commonFieldLinks)field->commonFieldLinks=LoadPortableFieldLinks(Table(field->sourcePath),field->exportIndex);
                next=field->commonFieldLinks->nextField;}
            return reinterpret_cast<std::uintptr_t>(linked(owner,next));};
        q.propertyIsA=[&](std::uintptr_t identity,QuestVr::OriginalInputResetPropertyKind kind){auto* field=native(identity);
            if(!field->property)return false;
            // An unresolved property Class is not a negative IsA result. In
            // particular, missing Core.ByteProperty used to skip all thirteen
            // real Pawn input bytes while allowing the Float pass to finish.
            const auto& table=Table(field->sourcePath);
            if(!field->cls||!field->inventoryClassReferenceValid||field->exportIndex>=table.exports.size()||
                Object(Qualified(table,table.exports[field->exportIndex].ObjClass))!=field->cls||
                !IsDerivedFromPath(field->cls,"Core.Property"))
                throw std::runtime_error("Original Input property iterator lacks its actual registered Property Class");
            return IsDerivedFromPath(field->cls,kind==QuestVr::OriginalInputResetPropertyKind::Byte?"Core.ByteProperty":"Core.FloatProperty");};
        q.propertyFlags=[&](std::uintptr_t identity)->std::uint64_t{auto* field=native(identity);
            if(!field->property||field->linkedClassOwner!=field->outer)throw std::runtime_error("Input reset property lacks original declaration authority");
            return field->property->flags;};
        q.clearProperty=[&](std::uintptr_t receiving,std::uintptr_t identity,QuestVr::OriginalInputResetPropertyKind kind){
            auto* pawn=native(receiving);auto* field=native(identity);const auto property=Describe(field);
            const auto expected=kind==QuestVr::OriginalInputResetPropertyKind::Byte?Kind::Byte:Kind::Float;
            if(pawn!=actor(reinterpret_cast<std::uintptr_t>(protocol.values.viewport))||!OwnsProperty(pawn->cls,property)||
                !field->property||field->linkedClassOwner!=field->outer||!(property.flags&4u)||property.zero.kind!=expected||!property.arrayDimension)
                throw std::runtime_error("Original Input selected store lost exact Byte/Float/flags/Actor declaration");
            execution.WriteReference(ReferenceFor(pawn,property,0u,false),expected==Kind::Byte?Value::Byte(0u):Value::Float(0.0f));
            const auto cleared=Read(pawn,property,0u);
            if(cleared.kind!=expected||(expected==Kind::Byte?cleared.integer!=0:
                std::bit_cast<std::uint32_t>(cleared.floating)!=0u))
                throw std::runtime_error("Original Input reset did not clear its actual qualified Actor field");
            if(expected==Kind::Byte)++observed.byteProperties;else ++observed.floatProperties;};
        q.writeAction=[&](std::uintptr_t identity,std::int32_t value){actualInput(identity);protocol.action=value;};
        q.writeDelta=[&](std::uintptr_t identity,float value){actualInput(identity);protocol.delta=value;};
        q.updateInput=[&](std::uintptr_t target,std::int32_t reset){static_cast<void>(actor(target));
            if(reset!=1)throw std::runtime_error("Original Input reset changed its literal UpdateInput argument");
            observed.platformReached=true;
            if(std::any_of(protocol.keys.begin(),protocol.keys.end(),[](auto value){return value!=0u;})||
                protocol.action||std::bit_cast<std::uint32_t>(protocol.delta)!=0u)
                throw std::runtime_error("Original Input platform reset reached before native key/accumulator clearing");
            // Explicit primitive checks at the real Init-prefix authority.
            // They do not replace UpdateInput or consume a physical event.
            ProbeOriginalInputActionCells(input,observed,execution);
            QuestVr::NativeScratchScope batchStorage(nativeQueryScratchBytes_,sizeof(QuestVr::QuestInputSource::Batch),
                [this](std::size_t bytes){ValidateStateBudget(bytes);});
            const auto& source=CurrentQuestInputSource(protocol.viewportIdentity);const auto batch=source->Stage(protocol.sourceCursor,true);
            if(batch.status!=QuestVr::QuestInputReadStatus::Ready||!batch.reset||batch.count!=1u)
                throw std::runtime_error("Original Quest UpdateInput reset requires a current valid physical observation");
            observed.sourceObserved=true;
            // A coherent baseline is NOT key reconciliation, Root.Process or
            // ExecInputCommands. Preserve the actual unsupported continuation;
            // do not advance cursor or label native Init complete from Stage.
            throw std::runtime_error("Original Quest viewport UpdateInput dispatch/reconciliation is not prepared");
        };
        q.initializedLog=[&](std::uintptr_t target){if(native(target)!=viewport||!viewport->lookupName)
                throw std::runtime_error("Original Input.Init log lost its actual Viewport Name");
            __android_log_print(ANDROID_LOG_INFO,"quest_main","Input system initialized for %s",viewport->lookupName->ToString().c_str());};
        QuestVr::InitializeOriginalInput(reinterpret_cast<std::uintptr_t>(input),reinterpret_cast<std::uintptr_t>(viewport),q);
    }
