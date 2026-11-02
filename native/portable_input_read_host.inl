// Actual UInput/UViewport ReadInput binding. Physical transitions remain in
// UpdateNativeQuestInput and held/scaling order remains in quest_input_read.h.
    void RequireInitializedInput(RuntimeObject* input) {
        if(!transaction_||!ActualNativeInitializedInputInstanceValid(input))
            throw std::runtime_error("Original Input ReadInput requires genuinely completed current Input.Init");
    }
    QuestVr::OriginalInputReadResult ReadNativeInput(RuntimeObject* input,float elapsed,std::string& output,
        QuestVr::Vm::Execution& execution) {
        if(!transaction_||!ActualNativeInputObjectValid(input))
            throw std::runtime_error("Original Input ReadInput has no actual native receiver/root");
        clientDependent_=true;RuntimeObject* cacheClass{};
        QuestVr::NativeScratchScope control(nativeQueryScratchBytes_,sizeof(QuestVr::OriginalInputReadQueries)+1024u,
            [this](std::size_t bytes){ValidateStateBudget(bytes);});
        const auto previous=nativeQueryScratchBytes_;std::size_t frames{};
        struct Restore {std::size_t& ledger;std::size_t before;~Restore(){ledger=before;}} restore{nativeQueryScratchBytes_,previous};
        const auto actor=[&]{RequireInitializedInput(input);auto* pawn=InputCommandActor(input);
            if(!pawn)throw std::runtime_error("Original Input ReadInput requires its current actual Viewport.Actor");
            return pawn;};
        const auto field=[&](QuestVr::InputReadId id){auto* property=reinterpret_cast<RuntimeObject*>(id);
            if(!property||!InputActualProperty(property)||!(property->property->flags&4u))
                throw std::runtime_error("Original ReadInput cache lost its actual INPUT property");
            return property;};
        const auto cache=[&](QuestVr::InputReadId id)->const InputPropertyCacheEntry&{
            if(!cacheClass||id!=reinterpret_cast<std::uintptr_t>(cacheClass))
                throw std::runtime_error("Original ReadInput cache lease does not match its entry Actor Class");
            return InputProperties(cacheClass);};
        const auto globalOutput=[&]{
            if(!ActualNativeInputObjectValid(input)||input->nativeInputInstance->logging!=PortableInputLoggingProfile::Ordinary)
                throw std::runtime_error("Original ReadInput lacks its selected ordinary global output-device profile");
            // Explicit port ordinary FString output profile, selected afresh
            // per original global load. Not a claimed PE32 GLog address.
            return reinterpret_cast<std::uintptr_t>(&output);
        };
        QuestVr::OriginalInputReadQueries q;
        q.validate=[&]{if(!transaction_||!ActualNativeInputObjectValid(input))
                throw std::runtime_error("Original ReadInput native lifetime changed during its caller root");};
        q.work=[this]{LifecycleStep();};
        q.enterFrame=[&](std::size_t bytes){const auto next=QuestVr::DecalStateDetail::Add(frames,bytes);
            nativeQueryScratchBytes_=previous;ValidateStateBudget(next);frames=next;nativeQueryScratchBytes_=previous+frames;};
        q.leaveFrame=[&](std::size_t bytes)noexcept{if(bytes>frames)std::terminate();frames-=bytes;nativeQueryScratchBytes_=previous+frames;};
        q.running=[this]{return ObserveOriginalProcessRunning();};
        q.acquireProperties=[&]{cacheClass=actor()->cls;static_cast<void>(InputProperties(cacheClass));
            return reinterpret_cast<std::uintptr_t>(cacheClass);};
        q.releaseProperties=[&](auto id)noexcept{if(id!=reinterpret_cast<std::uintptr_t>(cacheClass))std::terminate();cacheClass=nullptr;};
        q.propertyCount=[&](auto id){return cache(id).fields.size();};
        q.property=[&](auto id,std::size_t index){const auto& selected=cache(id);
            if(index>=selected.fields.size())throw std::runtime_error("Original ReadInput cache index is invalid");
            return reinterpret_cast<std::uintptr_t>(selected.fields[index].get());};
        q.isFloat=[&](auto id){return IsDerivedFromPath(field(id)->cls,"Core.FloatProperty");};
        q.key=[&](std::int32_t key){RequireInitializedInput(input);
            if(key<0||key>=255)throw std::runtime_error("Original ReadInput key index exceeds actual255 storage");
            return input->nativeInputInstance->keys[static_cast<std::size_t>(key)];};
        q.globalOutput=globalOutput;
        q.process=[&](auto device,std::int32_t key,std::int32_t action,float delta){
            if(device!=globalOutput())throw std::runtime_error("Original held Process lost its selected global output device");
            static_cast<void>(ProcessNativeInputExt(input,key,action,delta,output,execution));};
        q.readFloat=[&](auto id){auto* pawn=actor();const auto property=Describe(field(id));
            if(property.zero.kind!=Kind::Float||!property.arrayDimension||!OwnsProperty(pawn->cls,property))
                throw std::runtime_error("Original ReadInput Float address is incompatible with current Viewport.Actor");
            const auto value=Read(pawn,property,0u);if(value.kind!=Kind::Float)
                throw std::runtime_error("Original ReadInput actual axis is not Float storage");
            return value.floating;};
        q.writeFloat=[&](auto id,float value){auto* pawn=actor();const auto property=Describe(field(id));
            if(property.zero.kind!=Kind::Float||!property.arrayDimension||!OwnsProperty(pawn->cls,property))
                throw std::runtime_error("Original ReadInput store lost its current Actor/Float declaration");
            execution.WriteReference(ReferenceFor(pawn,property,0u,false),Value::Float(value));};
        return QuestVr::ReadOriginalInput(elapsed,q);
    }
    QuestVr::OriginalInputReadResult ReadNativeViewportInput(RuntimeObject* viewport,float elapsed,std::string& output,
        QuestVr::Vm::Execution& execution) {
        if(!transaction_||!viewport||NativeSessionViewport(viewport->nativeViewportIdentity)!=viewport)
            throw std::runtime_error("Original viewport ReadInput has no actual current caller/viewport");
        clientDependent_=true;QuestVr::OriginalInputReadResult result;
        QuestVr::NativeScratchScope control(nativeQueryScratchBytes_,sizeof(QuestVr::OriginalViewportReadQueries)+1024u,
            [this](std::size_t bytes){ValidateStateBudget(bytes);});
        const auto previous=nativeQueryScratchBytes_;std::size_t frames{};
        struct Restore {std::size_t& ledger;std::size_t before;~Restore(){ledger=before;}} restore{nativeQueryScratchBytes_,previous};
        QuestVr::OriginalViewportReadQueries q;
        q.validate=[&]{if(!transaction_||NativeSessionViewport(viewport->nativeViewportIdentity)!=viewport)
                throw std::runtime_error("Original viewport ReadInput lifetime was revoked");};
        q.work=[this]{LifecycleStep();};
        q.enterFrame=[&](std::size_t bytes){const auto next=QuestVr::DecalStateDetail::Add(frames,bytes);
            nativeQueryScratchBytes_=previous;ValidateStateBudget(next);frames=next;nativeQueryScratchBytes_=previous+frames;};
        q.leaveFrame=[&](std::size_t bytes)noexcept{if(bytes>frames)std::terminate();frames-=bytes;nativeQueryScratchBytes_=previous+frames;};
        q.input=[&]{auto* input=viewport->nativeViewportInput;
            if(input&&(!ActualNativeInputObjectValid(input)||input->nativeInputInstance->ownerViewport!=viewport))
                throw std::runtime_error("Original viewport ReadInput configured Input lost its actual native owner");
            return reinterpret_cast<std::uintptr_t>(input);};
        q.updateInput=[&](std::int32_t reset){if(reset)throw std::runtime_error("Original viewport ReadInput changed UpdateInput0");
            auto* input=viewport->nativeViewportInput;RequireInitializedInput(input);
            static_cast<void>(UpdateNativeQuestInput(input,false,elapsed,output,execution));};
        q.globalOutput=[&]{auto* input=viewport->nativeViewportInput;
            if(!input||!ActualNativeInputObjectValid(input)||input->nativeInputInstance->logging!=PortableInputLoggingProfile::Ordinary)
                throw std::runtime_error("Original viewport ReadInput has no selected ordinary output device");
            return reinterpret_cast<std::uintptr_t>(&output);};
        q.readInput=[&](auto input,float delta,auto device){
            if(device!=reinterpret_cast<std::uintptr_t>(&output)||input!=reinterpret_cast<std::uintptr_t>(viewport->nativeViewportInput))
                throw std::runtime_error("Original viewport ReadInput changed its reloaded Input/global output");
            result=ReadNativeInput(reinterpret_cast<RuntimeObject*>(input),delta,output,execution);};
        QuestVr::ReadOriginalViewportInput(elapsed,q);return result;
    }
