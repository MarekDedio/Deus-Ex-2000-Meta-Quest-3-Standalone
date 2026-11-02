// Included inside PortableActorVmHost. This is the real alias-command route on
// a constructed native Input. It does not fabricate Init/Viewport/Action state.
    PortableInputQueryResult QueryOriginalInput(RuntimeObject* input,const std::string& command,
        QuestVr::Vm::Execution& execution,std::string* output=nullptr) {
        if(!transaction_)throw std::runtime_error("Original Input commands require their actual outer VM root");
        // An Input-handled query never reaches ViewportExecAfterInput, whose
        // normal client lease pin therefore cannot protect this root for us.
        clientDependent_=true;
        const auto action=QueryOriginalInputActions(input,command,execution,output);
        if(action.handled)return action;
        const auto previous=nativeQueryScratchBytes_;
        struct Restore {std::size_t& ledger;std::size_t before;~Restore(){ledger=before;}} restore{nativeQueryScratchBytes_,previous};
        const RuntimeInputCommandDispatcher dispatch=[&](RuntimeObject* selected,std::size_t slot,std::string& device) {
            ExecuteOriginalInputAlias(selected,slot,device,execution);
        };
        auto result=ExecuteNativeInputQuery(input,command,[this]{LifecycleStep();},[&](std::size_t bytes) {
            nativeQueryScratchBytes_=previous;ValidateStateBudget(bytes);
            nativeQueryScratchBytes_=QuestVr::DecalStateDetail::Add(previous,bytes);
        },dispatch,output,[&]{RequireOriginalCommandInput(input);});
        result.work=QuestVr::DecalStateDetail::Add(result.work,action.work);return result;
    }
    void ExecuteOriginalInputAlias(RuntimeObject* input,std::size_t slot,std::string& output,
        QuestVr::Vm::Execution& execution) {
        ExecuteOriginalInputSource(input,slot,false,output,execution);
    }
    void ExecuteOriginalInputSource(RuntimeObject* input,std::size_t slot,bool binding,std::string& output,
        QuestVr::Vm::Execution& execution) {
        RequireOriginalCommandInput(input);
        if(slot>=(binding?255u:40u))
            throw std::runtime_error("Original Input command source exceeds its actual native fixed storage");
        const auto& command=binding?input->nativeInputInstance->values.bindings[slot]:input->nativeInputInstance->values.aliases[slot].command;
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
            RequireOriginalCommandInput(input);
            const auto& live=binding?input->nativeInputInstance->values.bindings[slot]:input->nativeInputInstance->values.aliases[slot].command;
            if(live!=captured)
                throw std::runtime_error("Original Input command native owner/source changed during dispatch");
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
    RuntimeObject* NativeInputExtRoot(RuntimeObject* input) {
        auto* pawn=InputCommandActor(input);
        if(!pawn)return nullptr;
        if(!IsDerivedFromPath(pawn->cls,"Extension.PlayerPawnExt"))
            throw std::runtime_error("Original InputExt Process requires its actual PlayerPawnExt actor");
        const auto property=DescribeProperty("Extension.PlayerPawnExt.RootWindow");auto* field=Object(property.key);
        if(property.zero.kind!=Kind::Object||property.arrayDimension!=1u||property.flags!=0x2002u||
            !OwnsProperty(pawn->cls,property)||!field->property||field->property->type!="ObjectProperty"||
            LowerAscii(Qualified(Table(field->sourcePath),field->property->referencedType))!="extension.rootwindow")
            throw std::runtime_error("Original InputExt root lookup lost its actual const RootWindow declaration");
        const auto value=Read(pawn,property,0u);
        if(value.kind!=Kind::Object)throw std::runtime_error("Original InputExt actual RootWindow value is not Object");
        if(value.text.empty())return nullptr;
        auto* root=Object(value.text);RequireRootInputOwner(root);
        if(root!=pawn->nativeRootWindow||RootInputParentPawn(root)!=pawn)
            throw std::runtime_error("Original InputExt Process lost its actual Root/parentPawn reciprocal owner");
        return root;
    }
    bool ProcessNativeInputExt(RuntimeObject* input,std::int32_t key,std::int32_t action,float delta,
        std::string& output,QuestVr::Vm::Execution& execution) {
        RequireOriginalCommandInput(input);clientDependent_=true;
        if(!persistentInputSchema||!persistentInputSchema->get()||input->cls!=persistentInputSchema->get()->inputExtClass)
            throw std::runtime_error("Original InputExt Process is not the configured actual Extension.InputExt class");
        static_cast<void>(MutateNativeInputValues(input));
        QuestVr::NativeScratchScope control(nativeQueryScratchBytes_,sizeof(QuestVr::OriginalInputProcessQueries)+256u,
            [this](std::size_t bytes){ValidateStateBudget(bytes);});
        const auto previous=nativeQueryScratchBytes_;std::size_t frames{};
        struct Restore {std::size_t& ledger;std::size_t before;~Restore(){ledger=before;}} restore{nativeQueryScratchBytes_,previous};
        const auto protocol=[&]()->RuntimeNativeInputInstance&{RequireOriginalCommandInput(input);return *input->nativeInputInstance;};
        const auto selected=[&](std::int32_t index)->std::size_t {
            if(index<0||index>=255)throw std::runtime_error("Original InputExt Process selected an invalid native key slot");
            return static_cast<std::size_t>(index);};
        QuestVr::OriginalInputProcessQueries q;q.work=[this]{LifecycleStep();};
        q.enterFrame=[&](std::size_t bytes){const auto next=QuestVr::DecalStateDetail::Add(frames,bytes);
            nativeQueryScratchBytes_=previous;ValidateStateBudget(next);frames=next;nativeQueryScratchBytes_=previous+frames;};
        q.leaveFrame=[&](std::size_t bytes)noexcept{if(bytes>frames)std::terminate();frames-=bytes;nativeQueryScratchBytes_=previous+frames;};
        q.hasRoot=[&]{return NativeInputExtRoot(input)!=nullptr;};
        q.rootProcess=[&](std::int32_t value,std::int32_t state,float amount){auto* root=NativeInputExtRoot(input);
            if(!root)throw std::runtime_error("Original InputExt selected Root disappeared before Process");
            return ProcessNativeRootInput(root,value,state,amount,execution);};
        q.key=[&](std::int32_t index){return protocol().keys[selected(index)];};
        q.writeKey=[&](std::int32_t index,std::uint8_t value){protocol().keys[selected(index)]=value;};
        q.bindingNonempty=[&](std::int32_t index){return !protocol().values.bindings[selected(index)].empty();};
        q.writeAction=[&](std::int32_t value){protocol().action=value;};
        q.writeDelta=[&](float value){protocol().delta=value;};
        q.execBinding=[&](std::int32_t index){ExecuteOriginalInputSource(input,selected(index),true,output,execution);};
        return QuestVr::ProcessOriginalInputExt(key,action,delta,q);
    }
