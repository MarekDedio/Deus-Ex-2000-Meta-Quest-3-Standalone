// Included inside PortableActorVmHost. This is a committing per-Actor native
// phase service, like AdvanceTimer/RunStatePhase, NOT an automatic world Tick.
// No startup/animation/parity/owner prefix is fabricated by this adapter.
    void RequireOriginalActorTickPhysicsImplementation(RuntimeObject* actor) {
        std::size_t depth{};
        for(auto* cls=actor->cls;cls&&depth++<64u;cls=cls->base) {
            LifecycleStep();
            if(!cls->classDescriptor)throw std::runtime_error("Original Actor Tick native ancestry lacks authored Class metadata");
            if(!(cls->reflection.flags&0x04000000u))continue;
            const auto& path=cls->reflection.objectPath;
            // Owned native vtables/IAT, ORIGINAL-ACTOR-TICK-PHYSICS-PHASE.md
            // and ORIGINAL-TRIGGER-TICK-PREREQUISITES.md. Exact nearest native
            // boundaries only: Trigger does not authorize native Triggers or
            // an unknown nearer native subclass, nor scalar teardown.
            if(path=="DeusEx.DeusExDecoration"||path=="Engine.Projectile"||path=="Engine.Inventory"||
                path=="Engine.Actor"||path=="Engine.Decoration"||path=="Engine.Mover"||path=="Engine.Trigger")return;
            throw std::runtime_error("Original Actor Tick reached an unimplemented native virtual override: "+path);
        }
        throw std::runtime_error("Original Actor Tick has no authenticated native implementation");
    }
    void OriginalTickEventDeclaration(RuntimeObject* actor,const Function& function,bool base) {
        auto* method=Object(function.path);auto* owner=method->outer;
        if(LowerAscii(function.path.substr(function.path.find_last_of('.')+1u))!="tick"||
            !method->script||!owner||(!owner->classDescriptor&&!owner->stateDescriptor))
            throw std::runtime_error("Original Tick event has no actual selected Function/Class/State identity");
        auto* ownerClass=owner->stateDescriptor?owner->outer:owner;
        if(!ownerClass||!ownerClass->classDescriptor||!IsDerivedFromPath(actor->cls,ownerClass->reflection.objectPath))
            throw std::runtime_error("Original Tick event belongs to another Actor ancestry");
        if(base&&(function.path!="Engine.Actor.Tick"||function.nativeIndex||function.flags!=0x800u||function.variables.size()!=1u))
            throw std::runtime_error("Original base Tick has a different native event packet");
        std::size_t parameters{};
        for(const auto& field:function.variables) {
            LifecycleStep();if(!(field.flags&0x80u))continue;
            auto* node=Object(field.key);
            if(parameters++||field.zero.kind!=Kind::Float||field.arrayDimension!=1u||field.flags!=0x80u||
                (base&&field.key!="Engine.Actor.Tick.DeltaTime")||node->outer!=method||!node->property||
                node->property->type!="FloatProperty"||node->property->baseField||node->property->referencedType||
                LowerAscii(Qualified(Table(node->sourcePath),Table(node->sourcePath).exports.at(node->exportIndex).ObjClass))!="core.floatproperty")
                throw std::runtime_error("Original Tick does not have its one scalar Float packet");
        }
        if(parameters!=1u)throw std::runtime_error("Original Tick is missing DeltaTime");
    }
    QuestVr::OriginalNonPawnTickTailStats AdvanceOriginalTickTail(RuntimeObject* actor,float elapsed,
        std::int32_t tickType,QuestVr::Vm::Execution& execution) {
        if(!transaction_)throw std::runtime_error("Original Tick tail has no caller transaction");
        actor=LifecycleActor(reinterpret_cast<std::uintptr_t>(actor));
        if(IsDerivedFromPath(actor->cls,"Engine.Pawn"))
            throw std::runtime_error("Original non-Pawn Tick tail cannot substitute for Pawn Tick");
        RequireOriginalActorTickPhysicsImplementation(actor);
        const auto source=reinterpret_cast<std::uintptr_t>(actor);
        const auto require=[&](std::uintptr_t identity) {
            if(identity!=source)throw std::runtime_error("Original Tick tail changed its retained Actor");
            return LifecycleActor(identity);
        };
        QuestVr::OriginalNonPawnTickTailQueries q;q.work=[this]{LifecycleStep();};
        const auto role=[&](std::uintptr_t identity,bool remote) {
            auto* receiver=require(identity);
            const auto field=NoiseProperty(receiver,remote?"Engine.Actor.RemoteRole":"Engine.Actor.Role",
                Kind::Byte,remote?0x21u:0x20u,"Engine.Actor.ENetRole");
            auto* metadata=Object(field.key);
            if(metadata->outer!=Object("Engine.Actor")||metadata->property->baseField||
                LowerAscii(Qualified(Table(metadata->sourcePath),Table(metadata->sourcePath).exports.at(metadata->exportIndex).ObjClass))!="core.byteproperty")
                throw std::runtime_error("Original Tick Role lacks its actual Actor Byte declaration");
            const auto value=Read(receiver,field,0u);
            if(value.kind!=Kind::Byte)throw std::runtime_error("Original Tick Role has changed storage kind");
            return static_cast<std::uint8_t>(value.integer);
        };
        q.remoteRole=[&](auto a){return role(a,true);};q.role=[&](auto a){return role(a,false);};
        q.physics=[&](auto a){return static_cast<std::uint8_t>(Read(require(a),ActorPhysicsProperty(require(a)),0u).integer);};
        q.isProbingTick=[&](auto a) {
            const auto context=CurrentDispatchContext(require(a));
            return QuestVr::ScriptDispatch::IsEnabled("Tick",context.classProbeMask,context.codeMasks,context.disabledNames,context.liveProbeMask);
        };
        q.tick=[&](auto a,float delta) {
            auto* receiver=require(a);
            OriginalTickEventDeclaration(receiver,*FunctionFor(Object("Engine.Actor.Tick")),true);
            const auto selected=Member(receiver,"Tick");
            if(!selected)throw std::runtime_error("Original Tick FindFunctionChecked found no selected event");
            OriginalTickEventDeclaration(receiver,*FunctionFor(Object(selected->path)),false);
            QuestVr::NativeScratchScope scratch(nativeQueryScratchBytes_,1024u+sizeof(Evaluation),
                [this](std::size_t extra){ValidateStateBudget(extra);});
            static_cast<void>(execution.CallEvent(receiver->reflection.objectPath,"Tick",true,{{Value::Float(delta),{}}}));
        };
        q.processState=[&](auto a,float delta){static_cast<void>(execution.RunStatePhase(require(a)->reflection.objectPath,delta));};
        q.timer=[&](auto a,float delta){static_cast<void>(AdvanceTimer(require(a),delta,execution));};
        const auto lifeField=[&](RuntimeObject* receiver) {
            return OriginalAnimationField(receiver,"Engine.Actor","LifeSpan",Kind::Float,1u,"FloatProperty");
        };
        q.lifeSpan=[&](auto a) {
            auto* receiver=require(a);const auto value=Read(receiver,lifeField(receiver),0u);
            if(value.kind!=Kind::Float)throw std::runtime_error("Original LifeSpan has changed storage kind");
            return value.floating;
        };
        q.writeLifeSpan=[&](auto a,float next) {
            auto* receiver=require(a);const auto field=lifeField(receiver);
            ReserveActorPhysicsReferences(field,1u);
            auto reference=std::make_shared<Reference>();reference->zero=field.zero;
            reference->read=[this,receiver,field]{return Read(receiver,field,0u);};
            reference->write=[this,receiver,field](const Value& value) {
                LifecycleActor(reinterpret_cast<std::uintptr_t>(receiver));
                if(value.kind!=Kind::Float)throw std::runtime_error("Original LifeSpan native store changed kind");
                Touch(receiver);receiver->scriptValues[InstanceStorageKey(receiver->cls,field)][0u]=value;
                receiver->committedScriptState=true;ValidateStateBudget();
            };
            execution.WriteReference(reference,Value::Float(next));
        };
        q.expired=[&](auto a){LifecycleEvent(require(a),LifecycleEventKind::Expired,nullptr,execution);};
        q.destroy=[&](auto a,std::int32_t force) {
            auto* receiver=require(a);
            static_cast<void>(DestroyNativeActor(receiver->nativeXLevel,receiver,force,execution));
        };
        q.performPhysics=[&](auto a,float delta){static_cast<void>(RetailPerformActorPhysics(require(a),delta,execution));};
        return QuestVr::AdvanceOriginalNonPawnTickTail(source,elapsed,tickType,q);
    }
