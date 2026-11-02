// Included inside PortableActorVmHost. Raw source -> actual native InputExt,
// never direct Pawn input stores or fabricated platform/initialized authority.
    QuestVr::QuestInputMappingKeys NativeQuestInputMapping(RuntimeObject* input) {
        RequireOriginalCommandInput(input);
        auto* enumeration=input->nativeInputInstance->values.keyEnum;
        if(!persistentInputSchema||!persistentInputSchema->get()||
            enumeration!=persistentInputSchema->get()->keyEnum||!enumeration||
            !enumeration->enumDescriptor||enumeration->enumDescriptor->names.size()!=255u||
            enumeration!=Object("Engine.Actor.EInputKey"))
            throw std::runtime_error("Quest mapping requires the actual owned EInputKey Enum");
        const auto key=[&](const char* name) {
            std::int32_t found=-1;
            for(std::size_t i=0u;i<enumeration->enumDescriptor->names.size();++i){LifecycleStep();
                if(enumeration->enumDescriptor->names[i].ToString()!=name)continue;
                if(found!=-1)throw std::runtime_error("Quest mapping encountered duplicate actual Enum names");
                found=static_cast<std::int32_t>(i);
            }
            if(found<0)throw std::runtime_error("Quest mapping lacks an actual original key: "+std::string(name));
            return found;
        };
        return {key("IK_LeftMouse"),key("IK_RightMouse"),key("IK_I"),key("IK_JoyX"),key("IK_JoyY")};
    }
    QuestVr::QuestInputReconcileResult UpdateNativeQuestInput(RuntimeObject* input,bool reset,std::optional<float> pollSeconds,
        std::string& output,QuestVr::Vm::Execution& execution) {
        RequireOriginalCommandInput(input);clientDependent_=true;
        auto* pawn=InputCommandActor(input);
        if(!pawn)throw std::runtime_error("Quest input dispatch requires its actual current player Actor");
        static_cast<void>(MutateNativeInputValues(input));
        auto* protocol=input->nativeInputInstance.get();auto* viewport=protocol->values.viewport;
        QuestVr::NativeScratchScope storage(nativeQueryScratchBytes_,sizeof(QuestVr::QuestInputSource::Batch)+
            sizeof(QuestVr::QuestInputReconcileQueries)+4096u,[this](std::size_t bytes){ValidateStateBudget(bytes);});
        const auto source=CurrentQuestInputSource(protocol->viewportIdentity);
        const auto check=[&]{
            RequireOriginalCommandInput(input);
            if(input->nativeInputInstance.get()!=protocol||protocol->retired||protocol->ownerViewport!=viewport||
                protocol->values.viewport!=viewport||InputCommandActor(input)!=pawn||
                NativeSessionViewport(protocol->viewportIdentity)!=viewport||viewport->nativeViewportInput!=input||
                CurrentQuestInputSource(protocol->viewportIdentity)!=source||
                !source->Matches(persistentNativeClient.ClientIdentity(),protocol->viewportIdentity))
                throw std::runtime_error("Quest input dispatch lost its exact native/Input/Pawn/source lifetime");
        };
        check();const auto mapping=NativeQuestInputMapping(input);
        const auto batch=source->Stage(protocol->sourceCursor,reset);
        // Regular ReadInput supplies its real CPU input-frame elapsed. Init's
        // reset-only UpdateInput(1) has no such argument: explicitly use the
        // newest physical period there, never a guessed72Hz/stale regular dt.
        if(!pollSeconds){
            if(!reset||batch.status!=QuestVr::QuestInputReadStatus::Ready||!batch.count)
                throw std::runtime_error("Quest input poll requires actual elapsed or a valid initial reset sample");
            pollSeconds=batch.rows[batch.count-1u].observation.deltaSeconds;
        }
        const auto previous=nativeQueryScratchBytes_;std::size_t frames{};
        struct Restore {std::size_t& ledger;std::size_t before;~Restore(){ledger=before;}} restore{nativeQueryScratchBytes_,previous};
        QuestVr::QuestInputReconcileQueries q;q.validate=check;q.work=[this]{LifecycleStep();};
        q.enterFrame=[&](std::size_t bytes){const auto next=QuestVr::DecalStateDetail::Add(frames,bytes);
            nativeQueryScratchBytes_=previous;ValidateStateBudget(next);frames=next;nativeQueryScratchBytes_=previous+frames;};
        q.leaveFrame=[&](std::size_t bytes)noexcept{if(bytes>frames)std::terminate();frames-=bytes;nativeQueryScratchBytes_=previous+frames;};
        q.process=[&](std::int32_t key,std::int32_t action,float delta){
            return ProcessNativeInputExt(input,key,action,delta,output,execution);};
        return QuestVr::ReconcileQuestInput(batch,protocol->physical,protocol->sourceCursor,mapping,*pollSeconds,q);
    }
