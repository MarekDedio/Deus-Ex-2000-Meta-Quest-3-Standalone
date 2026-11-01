// Included inside PortableActorVmHost. This is the real alias-command route on
// a constructed native Input. It does not fabricate Init/Viewport/Action state.
    PortableInputQueryResult QueryOriginalInput(RuntimeObject* input,const std::string& command,
        QuestVr::Vm::Execution& execution,std::string* output=nullptr) {
        if(!transaction_)throw std::runtime_error("Original Input commands require their actual outer VM root");
        // An Input-handled query never reaches ViewportExecAfterInput, whose
        // normal client lease pin therefore cannot protect this root for us.
        clientDependent_=true;
        const auto previous=nativeQueryScratchBytes_;
        struct Restore {std::size_t& ledger;std::size_t before;~Restore(){ledger=before;}} restore{nativeQueryScratchBytes_,previous};
        const RuntimeInputCommandDispatcher dispatch=[&](RuntimeObject* selected,std::size_t slot,std::string& device) {
            ExecuteOriginalInputAlias(selected,slot,device,execution);
        };
        return ExecuteNativeInputQuery(input,command,[this]{LifecycleStep();},[&](std::size_t bytes) {
            nativeQueryScratchBytes_=previous;ValidateStateBudget(bytes);
            nativeQueryScratchBytes_=QuestVr::DecalStateDetail::Add(previous,bytes);
        },dispatch,output);
    }
    void ExecuteOriginalInputAlias(RuntimeObject* input,std::size_t slot,std::string& output,
        QuestVr::Vm::Execution& execution) {
        if(!transaction_||!ActualNativeInputInstanceValid(input)||slot>=40u)
            throw std::runtime_error("Original Input alias dispatcher lacks its current native Input/root");
        const auto& command=input->nativeInputInstance->values.aliases[slot].command;
        if(command.size()>3u*QuestVr::NativeInputKeyQueries::MaximumCommandUnits)
            throw std::runtime_error("Original alias command exceeds admitted transport storage");
        QuestVr::NativeScratchScope capture(nativeQueryScratchBytes_,
            8u*(command.size()+1u)+sizeof(QuestVr::OriginalInputExecQueries)+256u,
            [this](std::size_t bytes){ValidateStateBudget(bytes);});
        const auto captured=command;
        const auto source=QuestVr::UnrealString::Decode(captured);
        if(source.size()>QuestVr::NativeInputKeyQueries::MaximumCommandUnits)
            throw std::runtime_error("Original alias command exceeds admitted UTF16 storage");
        const auto inputId=reinterpret_cast<std::uintptr_t>(input),outputId=reinterpret_cast<std::uintptr_t>(&output);
        const auto validate=[&] {
            if(!transaction_||!ActualNativeInputInstanceValid(input)||
                input->nativeInputInstance->values.aliases[slot].command!=captured)
                throw std::runtime_error("Original Input alias native owner/source changed during dispatch");
        };
        const auto identities=[&](std::uintptr_t receiver,std::uintptr_t device) {
            validate();if(receiver!=inputId||device!=outputId)
                throw std::runtime_error("Original Input alias changed its native Input/output device");
        };
        std::size_t frames{};const auto frameBase=nativeQueryScratchBytes_;
        struct Restore {std::size_t& ledger;std::size_t before;~Restore(){ledger=before;}} restore{nativeQueryScratchBytes_,frameBase};
        QuestVr::OriginalInputExecQueries q;
        q.requireOuterRoot=identities;q.work=[this]{LifecycleStep();};
        q.enterScratch=[&](std::size_t bytes) {
            const auto next=QuestVr::DecalStateDetail::Add(frames,bytes);
            nativeQueryScratchBytes_=frameBase;ValidateStateBudget(next);
            frames=next;nativeQueryScratchBytes_=QuestVr::DecalStateDetail::Add(frameBase,frames);
        };
        q.leaveScratch=[&](std::size_t bytes)noexcept{if(bytes>frames)std::terminate();frames-=bytes;nativeQueryScratchBytes_=frameBase+frames;};
        q.running=[&]{validate();return ObserveOriginalProcessRunning();};
        q.action=[&](std::uintptr_t receiver){identities(receiver,outputId);return input->nativeInputInstance->action;};
        q.inputViewport=[&](std::uintptr_t receiver){identities(receiver,outputId);
            auto* viewport=input->nativeInputInstance->values.viewport;
            if(!viewport||NativeSessionViewport(viewport->nativeViewportIdentity)!=viewport||viewport->nativeViewportInput!=input)
                throw std::runtime_error("Original Input viewport dispatch requires its genuine native Init back-pointer");
            return reinterpret_cast<std::uintptr_t>(viewport);};
        q.inputExec=[&](std::uintptr_t receiver,std::uintptr_t device,std::u16string_view line) {
            identities(receiver,device);
            const auto result=QueryOriginalInput(input,QuestVr::UnrealString::Encode(line),execution,&output);
            validate();return result.handled;
        };
        q.viewportExec=[&](std::uintptr_t receiver,std::uintptr_t device,std::u16string_view line) {
            validate();if(device!=outputId)throw std::runtime_error("Original Viewport alias changed its output device");
            auto* viewport=input->nativeInputInstance->values.viewport;
            if(!viewport||receiver!=reinterpret_cast<std::uintptr_t>(viewport)||
                NativeSessionViewport(viewport->nativeViewportIdentity)!=viewport||viewport->nativeViewportInput!=input)
                throw std::runtime_error("Original alias Viewport FExec lost its actual native Input backedge");
            // This is FULL viewport entry: first offer actual Input, then the
            // existing native renderer/viewport/Console/UPlayer provider chain.
            const auto encoded=QuestVr::UnrealString::Encode(line);
            const auto queried=QueryOriginalInput(input,encoded,execution,&output);
            if(queried.handled)return true;
            const auto tail=ViewportExecAfterInput(viewport,encoded,execution);
            if(tail.size()>49152u||output.size()>49152u-tail.size())
                throw std::runtime_error("Original Viewport command output exceeds admitted collector storage");
            ValidateStateBudget(6u*(output.size()+tail.size()+1u));output+=tail;
            validate();return true;
        };
        QuestVr::ExecuteOriginalInputCommands(inputId,outputId,source,q);
        validate();
    }
