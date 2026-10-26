// Native FindFunctionChecked + AActor::ProcessEvent, on the caller's existing
// root. Input packets are deliberately limited to the authenticated scalar
// Float/Bool or no-parameter forms reached by these original native phases.
    void OriginalNativeActorCallback(RuntimeObject* actor,const char* name,std::optional<Value> input,
        QuestVr::Vm::Execution& execution) {
        if(!transaction_)throw std::runtime_error("Original native Actor callback lacks its caller root");
        actor=LifecycleActor(reinterpret_cast<std::uintptr_t>(actor));
        if(input&&input->kind!=Kind::Bool&&input->kind!=Kind::Float)
            throw std::runtime_error("Original native Actor callback received an unaudited input packet");
        QuestVr::NativeScratchScope scratch(nativeQueryScratchBytes_,4096u,
            [this](std::size_t bytes){ValidateStateBudget(bytes);});
        std::shared_ptr<const Function> function;
        // Checked lookup is scriptability-gated BEFORE selected resolution.
        // It precedes Actor.ProcessEvent's begun gate even for Timer/Expired.
        if(OriginalGameScriptsEnabled()) {
            const auto context=CurrentDispatchContext(actor,true);
            const auto path=context.exactSelected?QuestVr::ScriptDispatch::ResolveFunctionAtSelectedNode(DispatchGraph(),
                actor->cls->reflection.objectPath,context.selectedPath,name):
                QuestVr::ScriptDispatch::ResolveFunction(DispatchGraph(),actor->cls->reflection.objectPath,context.stateName,name);
            if(!path)throw std::runtime_error(std::string("Original native Actor FindFunctionChecked found no declaration: ")+name);
            function=FunctionIdentity(Object(*path));auto* method=Object(function->path);auto* owner=method->outer;
            if(owner&&owner->stateDescriptor)owner=owner->outer;
            if(!method->script||function->source!=method->sourcePath||!owner||!owner->classDescriptor||owner->defaultOwner||
                !IsDerivedFromPath(actor->cls,owner->reflection.objectPath)||
                LowerAscii(Qualified(Table(method->sourcePath),Table(method->sourcePath).exports.at(method->exportIndex).ObjClass))!="core.function")
                throw std::runtime_error("Original native Actor callback has no genuine selected UFunction identity");
        }
        OriginalAnimationActorVirtuals(actor);
        const auto levelField=OriginalAnimationField(actor,"Engine.Actor","Level",Kind::Object,2u,"ObjectProperty");
        auto* node=Object(levelField.key);
        if(!node->property->referencedType||
            LowerAscii(Qualified(Table(node->sourcePath),node->property->referencedType))!="engine.levelinfo")
            throw std::runtime_error("Original native Actor callback lost Actor.Level's actual LevelInfo constraint");
        const auto levelValue=OriginalAnimationRead(actor,levelField,Kind::Object);
        if(levelValue.text.empty())throw std::runtime_error("Original native Actor callback has no actual Actor.Level");
        ValidateObjectValue(levelValue,"Engine.LevelInfo",false);auto* level=Object(levelValue.text);
        const auto begun=OriginalAnimationField(level,"Engine.LevelInfo","bBegunPlay",Kind::Bool,0u,"BoolProperty");
        if(!OriginalAnimationRead(level,begun,Kind::Bool).boolean||!OriginalGameScriptsEnabled())return;
        if(!function)throw std::runtime_error("Enabled original native Actor callback has no retained selected UFunction");
        if(!CanCall(*function,actor->reflection.objectPath)||OriginalAnimationPendingKill(actor)||function->nativeIndex)return;
        if(function->flags&0x400u)throw std::runtime_error("Original native Actor callback RPC virtual branch is not represented");
        const auto declaration=FunctionFor(Object(function->path));auto* method=Object(function->path);
        if(declaration->source!=function->source||declaration->nativeIndex!=function->nativeIndex||declaration->flags!=function->flags)
            throw std::runtime_error("Original native Actor callback changed its retained declaration");
        std::vector<Evaluation> arguments;std::size_t parameters{};
        for(const auto& field:declaration->variables) {
            LifecycleStep();if(!(field.flags&0x80u))continue;
            auto* parameter=Object(field.key);
            const auto type=input&&input->kind==Kind::Bool?"BoolProperty":"FloatProperty";
            if(!input||parameters++||field.zero.kind!=input->kind||field.arrayDimension!=1u||field.flags!=0x80u||
                parameter->outer!=method||!parameter->property||parameter->property->type!=type||
                parameter->property->baseField||parameter->property->referencedType||
                LowerAscii(Qualified(Table(parameter->sourcePath),Table(parameter->sourcePath).exports.at(parameter->exportIndex).ObjClass))!=
                    std::string("core.")+LowerAscii(type))
                throw std::runtime_error("Original native Actor callback has a different native parameter/result packet");
            arguments.push_back({*input,{}});
        }
        if(parameters!=(input?1u:0u))throw std::runtime_error("Original native Actor callback is missing its actual scalar packet");
        static_cast<void>(execution.CallFunction(*function,actor->reflection.objectPath,arguments));
    }
