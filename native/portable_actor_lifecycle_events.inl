// Included inside PortableActorVmHost. Original selected lifecycle packets and
// narrow raw fields use the SAME Execution/journal, without animation updates.
    RuntimeObject* LifecycleActor(std::uintptr_t identity) {
        if(!identity)throw std::runtime_error("Original lifecycle dereferenced a null Actor");
        auto* actor=reinterpret_cast<RuntimeObject*>(identity);
        RequireMapActor(actor);
        if(Object(actor->reflection.objectPath)!=actor)
            throw std::runtime_error("Original lifecycle retained Actor identity is unavailable");
        return actor; // No pending-delete, active, slot0 or Engine gate.
    }
    enum class LifecycleEventKind { EndState,Destroyed,LostChild,GainedChild,PostTouch,EndedRotation,Expired };
    const char* LifecycleEventName(LifecycleEventKind kind) {
        switch(kind) {
        case LifecycleEventKind::EndState:return "EndState";
        case LifecycleEventKind::Destroyed:return "Destroyed";
        case LifecycleEventKind::LostChild:return "LostChild";
        case LifecycleEventKind::GainedChild:return "GainedChild";
        case LifecycleEventKind::PostTouch:return "PostTouch";
        case LifecycleEventKind::EndedRotation:return "EndedRotation";
        case LifecycleEventKind::Expired:return "Expired";
        }
        throw std::runtime_error("Unknown original lifecycle event");
    }
    void LifecycleEventDeclaration(RuntimeObject* receiver,const Function& function,LifecycleEventKind kind,bool base) {
        const auto expected=std::string(kind==LifecycleEventKind::EndState?"Core.Object.":"Engine.Actor.")+LifecycleEventName(kind);
        auto* method=Object(function.path);auto* owner=method->outer;
        if(LowerAscii(function.path.substr(function.path.find_last_of('.')+1u))!=LowerAscii(LifecycleEventName(kind)))
            throw std::runtime_error("Selected lifecycle Function has a different event identity");
        if(!method->script||!owner||(!owner->classDescriptor&&!owner->stateDescriptor))
            throw std::runtime_error("Selected lifecycle event has no actual Function/Class/State ownership");
        auto* ownerClass=owner->stateDescriptor?owner->outer:owner;
        if(!ownerClass||!ownerClass->classDescriptor||!IsDerivedFromPath(receiver->cls,ownerClass->reflection.objectPath))
            throw std::runtime_error("Selected lifecycle event belongs to another receiver ancestry");
        const bool actorParameter=kind==LifecycleEventKind::LostChild||kind==LifecycleEventKind::GainedChild||kind==LifecycleEventKind::PostTouch;
        if(base&&(function.path!=expected||function.nativeIndex||function.flags!=0x800u||
            function.variables.size()!=(actorParameter?1u:0u)))
            throw std::runtime_error("Original lifecycle base event has a different native packet");
        unsigned parameters{};
        for(const auto& parameter:function.variables) {
            LifecycleStep();if(!(parameter.flags&0x80u))continue;
            if(!actorParameter||parameters++)
                throw std::runtime_error("Selected lifecycle event has extra native parameters/results");
            auto* field=Object(parameter.key);
            if((base&&LowerAscii(parameter.key)!=LowerAscii(expected)+".other")||parameter.zero.kind!=Kind::Object||
                parameter.arrayDimension!=1u||parameter.flags!=0x80u||!field->property||field->outer!=method||
                field->property->type!="ObjectProperty"||field->property->baseField||!field->property->referencedType||
                LowerAscii(Qualified(Table(field->sourcePath),field->property->referencedType))!="engine.actor"||
                LowerAscii(Qualified(Table(field->sourcePath),Table(field->sourcePath).exports.at(field->exportIndex).ObjClass))!="core.objectproperty")
                throw std::runtime_error("Selected lifecycle event lacks its exact Actor Other packet");
        }
        if(parameters!=(actorParameter?1u:0u))
            throw std::runtime_error("Selected lifecycle event is missing its native Actor parameter");
    }
    RuntimeObject* LifecycleFindEvent(RuntimeObject* receiver,LifecycleEventKind kind) {
        LifecycleActor(reinterpret_cast<std::uintptr_t>(receiver));
        const auto base=std::string(kind==LifecycleEventKind::EndState?"Core.Object.":"Engine.Actor.")+LifecycleEventName(kind);
        LifecycleEventDeclaration(receiver,*FunctionFor(Object(base)),kind,true);
        const auto selected=Member(receiver,LifecycleEventName(kind));
        if(!selected)throw std::runtime_error("Original lifecycle FindFunctionChecked found no selected event");
        auto* function=Object(selected->path);
        LifecycleEventDeclaration(receiver,*FunctionFor(function),kind,false);
        return function;
    }
    void LifecycleProcessEvent(RuntimeObject* receiver,RuntimeObject* function,LifecycleEventKind kind,
        RuntimeObject* other,QuestVr::Vm::Execution& execution) {
        if(!transaction_)throw std::runtime_error("Original lifecycle event has no caller transaction");
        LifecycleActor(reinterpret_cast<std::uintptr_t>(receiver));
        LifecycleEventDeclaration(receiver,*FunctionFor(function),kind,false);
        const bool actorParameter=kind==LifecycleEventKind::LostChild||kind==LifecycleEventKind::GainedChild||kind==LifecycleEventKind::PostTouch;
        if(actorParameter&&other)LifecycleActor(reinterpret_cast<std::uintptr_t>(other));
        if(!actorParameter&&other)throw std::runtime_error("Original lifecycle no-parameter event received an Actor");
        const auto bytes=QuestVr::DecalStateDetail::Add(1024u+sizeof(Evaluation),
            QuestVr::DecalStateDetail::Multiply(other?other->reflection.objectPath.size()+1u:1u,4u));
        QuestVr::NativeScratchScope scratch(nativeQueryScratchBytes_,bytes,
            [this](std::size_t extra){ValidateStateBudget(extra);});
        std::vector<Evaluation> packet;
        if(actorParameter)packet.push_back({Value::Text(Kind::Object,other?other->reflection.objectPath:std::string{}),{}});
        static_cast<void>(execution.CallEvent(receiver->reflection.objectPath,LifecycleEventName(kind),true,packet));
    }
    void LifecycleEvent(RuntimeObject* receiver,LifecycleEventKind kind,RuntimeObject* other,
        QuestVr::Vm::Execution& execution) {
        // Original FindFunctionChecked precedes ordinary ProcessEvent's begun/
        // probe suppression, including before-begun SetOwner calls in Spawn.
        auto* function=LifecycleFindEvent(receiver,kind);
        LifecycleProcessEvent(receiver,function,kind,other,execution);
    }
    void LifecycleRawActorStore(RuntimeObject* actor,const Property& property,const Value& value,
        QuestVr::Vm::Execution& execution) {
        const bool owner=property.key=="Engine.Actor.Owner";
        const bool pending=property.key=="Engine.Actor.bDeleteMe";
        if((!owner&&!pending)||property.arrayDimension!=1u||property.flags!=(owner?0x22u:0x2u)||
            property.zero.kind!=(owner?Kind::Object:Kind::Bool))
            throw std::runtime_error("Original lifecycle raw store has no supported field authority");
        auto reference=std::make_shared<Reference>();reference->zero=property.zero;
        reference->read=[this,actor,property]{return Read(actor,property,0u);};
        reference->write=[this,actor,property](const Value& next) {
            auto normalized=TypedLiveValue(property,next);Touch(actor);
            actor->scriptValues[InstanceStorageKey(actor->cls,property)][0u]=std::move(normalized);
            actor->committedScriptState=true;
        };
        execution.WriteReference(reference,value);
        ValidateStateBudget(); // No UpdateClockProperty or generic shape guard.
    }
