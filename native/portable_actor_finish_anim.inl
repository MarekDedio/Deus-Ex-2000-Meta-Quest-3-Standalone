// Included inside PortableActorVmHost, after portable_actor_animation_phase.inl.
// Original zero-input FinishAnim261 and native latent poll385 use the SAME
// caller Execution/root. Neither helper owns a frame, timer or animation clock.
// Evidence: docs/ORIGINAL-FINISHANIM-LATENT.md, Engine.dll103DFF60/103E00A0.
    std::shared_ptr<const Function> OriginalFinishAnimDeclaration(const Function* supplied) {
        const auto original=FunctionFor(Object("Engine.Actor.FinishAnim"));
        const auto validate=[this](const Function& function) {
            auto* node=Object(function.path);
            if(function.path!="Engine.Actor.FinishAnim"||function.nativeIndex!=QuestVr::OriginalActorFinishAnimNative||
                function.flags!=QuestVr::OriginalActorFinishAnimFlags||!function.variables.empty()||!function.bytecode.empty()||
                !node->script||node->outer!=Object("Engine.Actor")||function.source!=node->sourcePath||
                node->script->nativeIndex!=QuestVr::OriginalActorFinishAnimNative||
                node->script->functionFlags!=QuestVr::OriginalActorFinishAnimFlags||node->script->children||
                node->script->baseField||node->script->logicalSize||!node->script->bytecode.empty()||
                !node->script->rawBytes.empty()||
                LowerAscii(Qualified(Table(node->sourcePath),Table(node->sourcePath).exports.at(node->exportIndex).ObjClass))!="core.function")
                throw std::runtime_error("FinishAnim261 lacks its actual zero-input native409 Actor declaration");
        };
        validate(*original);
        if(supplied) {
            validate(*supplied);
            if(supplied->source!=original->source)
                throw std::runtime_error("FinishAnim261 declaration changed its original source identity");
        }
        return original;
    }
    QuestVr::StateFrame* OriginalFinishAnimFrame(RuntimeObject* actor) {
        static_cast<void>(MoveEventActor(actor));
        // Do not manufacture a frame, code position or selected state. A raw
        // loader FStateFrame cannot yet be mutated by this bounded adapter.
        // This check is deliberately reached only by a real native slot store.
        if(!actor->stateObject||!actor->stateObject->hasStack||!actor->stateObject->frameOverride||
            !actor->stateObject->frame||!actor->stateObject->frame->control)
            throw std::runtime_error("FinishAnim native latent store needs the actual existing modern actor StateFrame");
        return &*actor->stateObject->frame;
    }
    std::shared_ptr<Reference> OriginalFinishAnimLatentReference(RuntimeObject* actor,std::uintptr_t identity) {
        auto* frame=OriginalFinishAnimFrame(actor);
        if(reinterpret_cast<std::uintptr_t>(frame)!=identity)
            throw std::runtime_error("FinishAnim native latent store lost its retained StateFrame identity");
        const auto revision=actor->stateLocalsRevision;
        const auto current=[this,actor,identity,revision] {
            auto* retained=OriginalFinishAnimFrame(actor);
            if(actor->stateLocalsRevision!=revision||reinterpret_cast<std::uintptr_t>(retained)!=identity)
                throw std::runtime_error("FinishAnim latent reference outlived its actual actor frame storage");
            return retained;
        };
        auto reference=std::make_shared<Reference>();reference->zero=Value::Byte(0u);reference->dimension=1u;
        // INTERNAL typed undo payload: these Byte ordinals are not native
        // LatentAction IDs. In particular Continue does not prove native zero.
        // Only writeNativeLatent below maps known native385/0 to the model.
        reference->read=[current] {return Value::Byte(static_cast<std::uint8_t>(current()->latent));};
        reference->write=[this,actor,current](const Value& value) {
            if(value.kind!=Kind::Byte||value.integer<0||
                value.integer>static_cast<std::int32_t>(QuestVr::StateLatent::WaitForLanding))
                throw std::runtime_error("FinishAnim latent undo payload is not a supported portable enum Byte");
            auto* retained=current();Touch(actor);
            retained->latent=static_cast<QuestVr::StateLatent>(value.integer);actor->committedScriptState=true;
            ValidateStateBudget();
        };
        return reference;
    }
    QuestVr::OriginalActorFinishAnimQueries OriginalFinishAnimQueries(RuntimeObject* actor,
        QuestVr::Vm::Execution& execution) {
        using B=QuestVr::OriginalActorFinishAnimBool;
        using F=QuestVr::OriginalActorFinishAnimFloat;
        const auto receiver=[this,actor](std::uintptr_t identity) {
            if(identity!=reinterpret_cast<std::uintptr_t>(actor))
                throw std::runtime_error("FinishAnim changed its actual retained Actor receiver");
            return MoveEventActor(actor);
        };
        QuestVr::OriginalActorFinishAnimQueries queries;
        queries.work=[this]{LifecycleStep();};
        queries.readBool=[this,receiver](std::uintptr_t identity,B selected) {
            auto* current=receiver(identity);const char* name{};
            switch(selected) {
            case B::Loop:name="bAnimLoop";break;
            case B::Finished:name="bAnimFinished";break;
            default:throw std::runtime_error("FinishAnim has an unknown native Bool selector");
            }
            return OriginalAnimationRead(current,OriginalAnimationField(current,"Engine.Actor",name,
                Kind::Bool,0u,"BoolProperty"),Kind::Bool).boolean;
        };
        queries.readFloat=[this,receiver](std::uintptr_t identity,F selected) {
            QuestVr::OriginalActorAnimationFloat field;
            switch(selected) {
            case F::Frame:field=QuestVr::OriginalActorAnimationFloat::Frame;break;
            case F::Rate:field=QuestVr::OriginalActorAnimationFloat::Rate;break;
            case F::TweenRate:field=QuestVr::OriginalActorAnimationFloat::TweenRate;break;
            case F::Last:field=QuestVr::OriginalActorAnimationFloat::Last;break;
            default:throw std::runtime_error("FinishAnim has an unknown native Float selector");
            }
            auto* current=receiver(identity);
            return OriginalAnimationRead(current,OriginalAnimationFloatField(current,field),Kind::Float).floating;
        };
        queries.sequenceIsNone=[this,receiver](std::uintptr_t identity) {
            auto* current=receiver(identity);
            const auto value=OriginalAnimationRead(current,OriginalAnimationField(current,"Engine.Actor","AnimSequence",
                Kind::Name,0x21u,"NameProperty"),Kind::Name);
            return QuestVr::MeshAnimationNamesEqual(value.text,"None");
        };
        queries.clearLoopAndFinished=[this,receiver,&execution](std::uintptr_t identity) {
            auto* current=receiver(identity);
            const auto loop=OriginalAnimationField(current,"Engine.Actor","bAnimLoop",Kind::Bool,0u,"BoolProperty");
            const auto finished=OriginalAnimationField(current,"Engine.Actor","bAnimFinished",Kind::Bool,0u,"BoolProperty");
            auto loopReference=OriginalAnimationReference(current,loop,{});
            const auto store=loopReference->write;
            loopReference->write=[current,store](const Value& value) {
                store(value);
                if(current->animationClock)current->animationClock->main.loop=value.boolean;
            };
            auto finishedReference=OriginalAnimationReference(current,finished,{});
            // Retail one packed AND becomes two typed journal leaves, with no
            // event, state continuation, publication or callback in between.
            // A second-write refusal poisons/undoes this complete outer root.
            execution.WriteReference(loopReference,Value::Bool(false));
            execution.WriteReference(finishedReference,Value::Bool(false));
        };
        queries.nativeFrame=[this,receiver](std::uintptr_t identity) {
            return reinterpret_cast<std::uintptr_t>(OriginalFinishAnimFrame(receiver(identity)));
        };
        queries.writeNativeLatent=[this,receiver,&execution](std::uintptr_t identity,std::uintptr_t frame,std::uint32_t native) {
            QuestVr::StateLatent modeled;
            if(native==QuestVr::OriginalActorFinishAnimPollNative)modeled=QuestVr::StateLatent::FinishAnim;
            else if(native==0u)modeled=QuestVr::StateLatent::Continue;
            else throw std::runtime_error("FinishAnim attempted an unaudited native latent action store");
            auto* current=receiver(identity);
            execution.WriteReference(OriginalFinishAnimLatentReference(current,frame),
                Value::Byte(static_cast<std::uint8_t>(modeled)));
        };
        return queries;
    }
    void OriginalFinishAnimNative(RuntimeObject* actor,const Function* declaration,QuestVr::Vm::Execution& execution) {
        if(!transaction_)throw std::runtime_error("FinishAnim261 requires its existing caller Execution transaction");
        static_cast<void>(OriginalFinishAnimDeclaration(declaration));
        actor=MoveEventActor(actor);
        // The outer raw/declaration wrapper must reject a nonempty packet.
        // Actual FinishAnim has no expression Step/default/result destination.
        const auto queries=OriginalFinishAnimQueries(actor,execution);
        static_cast<void>(QuestVr::ExecuteOriginalActorFinishAnim(reinterpret_cast<std::uintptr_t>(actor),queries));
    }
    bool PollOriginalFinishAnim(RuntimeObject* actor,QuestVr::Vm::Execution& execution) {
        if(!transaction_)throw std::runtime_error("PollFinishAnim385 requires its existing ProcessState transaction");
        actor=MoveEventActor(actor);
        const auto queries=OriginalFinishAnimQueries(actor,execution);
        return QuestVr::PollOriginalActorFinishAnim(reinterpret_cast<std::uintptr_t>(actor),queries).released;
    }
