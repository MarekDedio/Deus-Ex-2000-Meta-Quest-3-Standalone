    void InitializeNativeViewportInput(RuntimeObject* viewport,QuestVr::Vm::Execution& execution,
        PortableWorldStartupProbeResult::InputResetPrefixObservation* diagnostic=nullptr) {
        PortableWorldStartupProbeResult::InputResetPrefixObservation ordinary;
        auto& observed=diagnostic?*diagnostic:ordinary;
        // Genuine initial Init service. Its caller must own the original initial
        // GameEngine call site; this is NOT an Init inserted at LoadMap.ResetInput.
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
        if(diagnostic){protocol.keys.fill(0xa5u);protocol.action=7;protocol.delta=-3.0f;}
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
            if(diagnostic)ProbeOriginalInputActionCells(input,observed,execution);
            std::string output;const auto reconciled=UpdateNativeQuestInput(input,true,std::nullopt,output,execution);
            observed.sourceObserved=reconciled.rows!=0u;observed.sourceReconciled=true;
            observed.sourceRows=reconciled.rows;observed.sourcePressed=reconciled.pressed;
            observed.sourceReleased=reconciled.released;observed.sourceAxes=reconciled.axes;
            observed.sourceSequence=protocol.sourceCursor.sequence;observed.physicalMappingVersion=protocol.physical.version;
            if(!diagnostic)return;
            auto* pawn=actor(target);
            auto* strafe=FindInputProperty(input,pawn,u"aStrafe",Kind::Float);
            auto* forward=FindInputProperty(input,pawn,u"aBaseY",Kind::Float);
            if(!strafe||!forward)throw std::runtime_error("Quest diagnostic lacks actual original joystick INPUT Float declarations");
            observed.sourceStrafe=Read(pawn,Describe(strafe),0u).floating;
            observed.sourceForward=Read(pawn,Describe(forward),0u).floating;
            observed.sourceStrafeLevel=protocol.physical.strafeLevel;observed.sourceForwardLevel=protocol.physical.forwardLevel;
            const auto mapping=NativeQuestInputMapping(input);
            observed.originalScriptCommands=scriptCommandPackets_.size();
            observed.physicalTriggerVerified=protocol.physical.held[static_cast<std::size_t>(mapping.fire)]==1u&&
                protocol.keys[static_cast<std::size_t>(mapping.fire)]==1u&&
                Read(pawn,DescribeProperty("Engine.Pawn.bFire"),0u).integer==1&&
                Read(pawn,DescribeProperty("Engine.PlayerPawn.bJustFired"),0u).boolean;
            // Explicit second CPU-poll diagnostic, no new physical row. The
            // actual source/owners/current bindings remain unchanged.
            QuestVr::NativeScratchScope retainedObservation(nativeQueryScratchBytes_,
                sizeof(QuestVr::QuestInputPhysicalState)+sizeof(QuestVr::QuestInputCursor)+256u,
                [this](std::size_t bytes){ValidateStateBudget(bytes);});
            const auto physical=protocol.physical;const auto cursor=protocol.sourceCursor;
            const auto empty=UpdateNativeQuestInput(input,false,0.02f,output,execution);
            observed.emptyPollVerified=empty.empty&&!empty.rows&&empty.axes==2u&&
                protocol.physical==physical&&QuestVr::QuestInputReconcileDetail::Same(protocol.sourceCursor,cursor)&&
                std::fabs(Read(pawn,Describe(strafe),0u).floating-5.4f)<0.00001f&&
                std::fabs(Read(pawn,Describe(forward),0u).floating-7.2f)<0.00001f;
            const auto resetAgain=UpdateNativeQuestInput(input,true,0.01f,output,execution);
            observed.sameSampleResetVerified=resetAgain.reset&&resetAgain.rows==1u&&resetAgain.pressed==1u&&resetAgain.released==1u&&
                protocol.physical==physical&&QuestVr::QuestInputReconcileDetail::Same(protocol.sourceCursor,cursor)&&
                protocol.keys[static_cast<std::size_t>(mapping.fire)]==1u&&
                Read(pawn,DescribeProperty("Engine.Pawn.bFire"),0u).integer==1;
        };
        q.initializedLog=[&](std::uintptr_t target){if(native(target)!=viewport||!viewport->lookupName)
                throw std::runtime_error("Original Input.Init log lost its actual Viewport Name");
            __android_log_print(ANDROID_LOG_INFO,"quest_main","Input system initialized for %s",viewport->lookupName->ToString().c_str());};
        QuestVr::InitializeOriginalInput(reinterpret_cast<std::uintptr_t>(input),reinterpret_cast<std::uintptr_t>(viewport),q);
        observed.initReturned=true;
        // This completion record is staged ONLY after the real Init body,
        // platform reconciliation and final log returned in this same COW root.
        // It is transient native/session state, never actor save provenance.
        if(protocol.initialized||protocol.initialization||protocol.values.viewport!=viewport||!protocol.sourceCursor.source||
            !protocol.sourceCursor.sequence||protocol.physical.version!=QuestVr::QuestInputReconcileDetail::Version)
            throw std::runtime_error("Original completed Init lost its actual reconciled native payload");
        protocol.initialization.emplace(RuntimeNativeInputInstance::Initialization{protocol.clientIdentity,protocol.viewportIdentity,
            protocol.lookupGeneration,protocol.sourceCursor.source,input->objectIndex.Index(),viewport->objectIndex.Index(),
            protocol.values.keyEnum->objectIndex.Index()});
        protocol.initialized=true;
        if(ActualNativeInputInstanceValid(input)||!ActualNativeInitializedInputInstanceValid(input))
            throw std::runtime_error("Original completed Init did not establish distinct initialized-state authority");
        observed.initializedAuthenticated=true;
    }
    void ProbeInitialInputResetPrefix(RuntimeObject* viewport,
        PortableWorldStartupProbeResult::InputResetPrefixObservation& observed,QuestVr::Vm::Execution& execution) {
        InitializeNativeViewportInput(viewport,execution,&observed);
        auto* input=viewport->nativeViewportInput;auto* pawn=InputCommandActor(input);
        auto& protocol=*input->nativeInputInstance;
        QuestVr::NativeScratchScope ownershipControls(nativeQueryScratchBytes_,
            sizeof(RuntimeNativeInputInstance::Initialization)+128u,[this](std::size_t bytes){ValidateStateBudget(bytes);});
        const auto receipt=*protocol.initialization;
        const auto rejectChangedAuthority=[&]{LifecycleStep();
            if(ActualNativeInputInstanceValid(input)||ActualNativeInitializedInputInstanceValid(input))
                throw std::runtime_error("Initialized Input diagnostic admitted altered native authority");
            ++observed.initializationRefusals;};
        // Actual native-owner negative controls in this rollback-only caller,
        // never manufactured providers or published initialization records.
        protocol.initialized=false;rejectChangedAuthority();protocol.initialized=true;
        protocol.initialization.reset();rejectChangedAuthority();protocol.initialization=receipt;
        protocol.values.viewport=nullptr;rejectChangedAuthority();protocol.values.viewport=viewport;
        protocol.initialization->client^=1u;rejectChangedAuthority();protocol.initialization=receipt;
        protocol.initialization->lookup^=1u;rejectChangedAuthority();protocol.initialization=receipt;
        protocol.initialization->inputIndex^=1u;rejectChangedAuthority();protocol.initialization=receipt;
        protocol.initialization->source^=1u;rejectChangedAuthority();protocol.initialization=receipt;
        protocol.initialization->source^=1u;protocol.sourceCursor.source^=1u;rejectChangedAuthority();
        protocol.initialization=receipt;protocol.sourceCursor.source=receipt.source;
        protocol.physical.version^=1u;rejectChangedAuthority();protocol.physical.version^=1u;
        if(!ActualNativeInitializedInputInstanceValid(input)||observed.initializationRefusals!=9u)
            throw std::runtime_error("Initialized Input controls did not restore their actual current owner");
        auto* strafe=FindInputProperty(input,pawn,u"aStrafe",Kind::Float);
        auto* forward=FindInputProperty(input,pawn,u"aBaseY",Kind::Float);
        if(!strafe||!forward)throw std::runtime_error("Initialized ReadInput diagnostic lacks actual player axes");
        std::string output;
        const auto axesZero=[&]{return std::bit_cast<std::uint32_t>(Read(pawn,Describe(strafe),0u).floating)==0u&&
            std::bit_cast<std::uint32_t>(Read(pawn,Describe(forward),0u).floating)==0u;};
        const auto initialPost=ReadNativeViewportInput(viewport,-1.0f,output,execution);
        observed.readInputPostVerified=initialPost.running&&!initialPost.holds&&initialPost.scaled==15u&&axesZero();
        const auto before=input->nativeInputInstance->sourceCursor;
        const auto first=ReadNativeViewportInput(viewport,0.02f,output,execution);
        observed.readInputHolds=first.holds;observed.readInputFloats=first.scaled;
        observed.readInputStrafe=Read(pawn,Describe(strafe),0u).floating;
        observed.readInputForward=Read(pawn,Describe(forward),0u).floating;
        // Float4.8 times extended20/Float.02 stores4800+one Float ULP,
        // independently verified by the generated hardware-x87 oracle.
        observed.readInputPreVerified=first.running&&first.holds==1u&&first.scaled==15u&&
            observed.readInputStrafe==3600.0f&&std::bit_cast<std::uint32_t>(observed.readInputForward)==0x45960001u&&
            QuestVr::QuestInputReconcileDetail::Same(input->nativeInputInstance->sourceCursor,before);
        const auto post=ReadNativeViewportInput(viewport,-1.0f,output,execution);
        observed.readInputPostVerified=observed.readInputPostVerified&&post.running&&!post.holds&&post.scaled==15u&&axesZero();
        const auto second=ReadNativeViewportInput(viewport,0.01f,output,execution);
        observed.readInputRateVerified=second.running&&second.holds==1u&&second.scaled==15u&&
            Read(pawn,Describe(strafe),0u).floating==observed.readInputStrafe&&
            Read(pawn,Describe(forward),0u).floating==observed.readInputForward;
        // Return to the REAL remaining startup path; no artificial input
        // boundary or early partial-world/initialized-state commit is inserted.
    }
