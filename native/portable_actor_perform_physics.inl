// Actual native per-Actor physics services. No world scheduling, fabricated
// native index, dormant-event bypass or private execution/rollback boundary.
    Property PendingTouchField(RuntimeObject* actor) {
        return RetailPredicatePointerField(actor,"Engine.Actor.PendingTouch","Engine.Actor","Engine.Actor",0u);
    }
    void BindNativePendingTouch(RuntimeObject* actor) {
        if(actor->nativePendingTouchBound||!persistentVmObjects.contains("engine.actor.pendingtouch"))return;
        const auto field=PendingTouchField(actor);const auto value=Read(actor,field,0u);
        ValidateObjectValue(value,"Engine.Actor",false);
        actor->nativePendingTouch=value.text.empty()?nullptr:Object(value.text);actor->nativePendingTouchBound=true;
    }
    void StoreNativePendingTouch(RuntimeObject* actor,const Property& field,const Value& value) {
        RequireMapActor(actor);
        const auto actual=PendingTouchField(actor);
        if(!actor->nativePendingTouchBound||field.key!=actual.key||field.arrayDimension!=1u||
            field.zero.kind!=Kind::Object||field.flags!=0u)
            throw std::runtime_error("Original PendingTouch store lacks its actual bound Actor field");
        auto next=TypedLiveValue(actual,value);
        auto* target=next.text.empty()?nullptr:Object(next.text);
        if(std::none_of(beforePendingTouch_.begin(),beforePendingTouch_.end(),[&](const auto& row){return row.actor.get()==actor;})) {
            if(beforePendingTouch_.size()>=4096u)throw std::runtime_error("Original PendingTouch root journal exceeds its bound");
            if(beforePendingTouch_.size()==beforePendingTouch_.capacity()) {
                const auto count=std::min<std::size_t>(4096u,std::max<std::size_t>(8u,beforePendingTouch_.capacity()*2u));
                ValidateStateBudget(count*sizeof(BeforePendingTouch)+2u*sizeof(GCRootNode));beforePendingTouch_.reserve(count);
            }
            beforePendingTouch_.push_back({GCRoot<RuntimeObject>(actor),GCRoot<RuntimeObject>(actor->nativePendingTouch)});
        }
        Touch(actor);
        actor->scriptValues[InstanceStorageKey(actor->cls,actual)][0u]=std::move(next);
        actor->nativePendingTouch=target;actor->committedScriptState=true;
        ValidateStateBudget(); // No unrelated animation-clock reconstruction.
    }
    void WriteNativePendingTouch(RuntimeObject* actor,RuntimeObject* value,QuestVr::Vm::Execution& execution) {
        const auto field=PendingTouchField(actor);ReserveActorPhysicsReferences(field,1u);
        auto reference=std::make_shared<Reference>();reference->zero=field.zero;
        reference->read=[this,actor,field]{return Read(actor,field,0u);};
        reference->write=[this,actor,field](const Value& next){StoreNativePendingTouch(actor,field,next);};
        execution.WriteReference(reference,Value::Text(Kind::Object,value?value->reflection.objectPath:std::string{}));
    }
    Property PhysicsRotatorField(RuntimeObject* actor,const char* key,std::uint32_t flags) {
        RequireMapActor(actor);const auto field=NativeCoordsProperty(actor,key,Kind::Rotator,flags);auto* node=Object(field.key);
        if(node->outer!=Object("Engine.Actor")||!node->property||node->property->baseField||
            LowerAscii(Qualified(Table(node->sourcePath),Table(node->sourcePath).exports.at(node->exportIndex).ObjClass))!="core.structproperty")
            throw std::runtime_error("Original physics Rotator lacks its actual Actor declaration");
        return field;
    }
    std::int32_t PhysicsRotatorChannel(RuntimeObject* actor,const char* key,std::uint32_t flags,unsigned axis) {
        if(axis>=3u)throw std::runtime_error("Original physics Rotator channel is outside its actual three INT fields");
        const auto value=Read(actor,PhysicsRotatorField(actor,key,flags),0u);
        if(value.kind!=Kind::Rotator)throw std::runtime_error("Original physics Rotator live value changed kind");
        return value.rotation[axis];
    }
    QuestVr::OriginalActorRotationStats RetailPhysicsRotation(RuntimeObject* actor,float deltaSeconds,
        QuestVr::Vm::Execution& execution,PortableActorPhysicsObservation* observation=nullptr) {
        if(!transaction_)throw std::runtime_error("Original physicsRotation requires its caller root");
        const auto source=reinterpret_cast<std::uintptr_t>(actor);LifecycleActor(source);
        const auto require=[&](std::uintptr_t identity) {
            if(identity!=source)throw std::runtime_error("Original physicsRotation source identity changed");
            return LifecycleActor(identity);
        };
        QuestVr::OriginalActorRotationQueries q;q.work=[this]{LifecycleStep();};
        q.rotationFlags=[&](auto a) {
            auto* receiver=require(a);
            const bool fixed=Read(receiver,LandingScalarProperty(receiver,"Engine.Actor.bFixedRotationDir","Engine.Actor",Kind::Bool,0x21u),0u).boolean;
            const bool desired=Read(receiver,LandingScalarProperty(receiver,"Engine.Actor.bRotateToDesired","Engine.Actor",Kind::Bool,0x21u),0u).boolean;
            return static_cast<std::uint8_t>((fixed?0x20u:0u)|(desired?0x40u:0u));
        };
        q.rotation=[&](auto a,unsigned axis){return PhysicsRotatorChannel(require(a),"Engine.Actor.Rotation",0x23u,axis);};
        q.desiredRotation=[&](auto a,unsigned axis){return PhysicsRotatorChannel(require(a),"Engine.Actor.DesiredRotation",0x21u,axis);};
        q.rotationRate=[&](auto a,unsigned axis){return PhysicsRotatorChannel(require(a),"Engine.Actor.RotationRate",0x21u,axis);};
        q.level=[&](auto a){return reinterpret_cast<std::uintptr_t>(require(a)->nativeXLevel);};
        q.moveActor=[&](auto level,auto a,auto delta,auto rotation,auto& hit,auto options) {
            require(a);
            if(delta.x!=0.0f||delta.y!=0.0f||delta.z!=0.0f||options!=std::array<std::int32_t,4u>{})
                throw std::runtime_error("Original physicsRotation changed its zero-translation/four-zero native packet");
            if(observation)++observation->rotationMoveCalls;
            return MoveLevelActor(level,a,delta,rotation,hit,options,execution)?1:0;
        };
        q.isProbingEndedRotation=[&](auto a) {
            const auto context=CurrentDispatchContext(require(a));
            return QuestVr::ScriptDispatch::IsEnabled("EndedRotation",context.classProbeMask,context.codeMasks,context.disabledNames,context.liveProbeMask);
        };
        q.endedRotation=[&](auto a) {
            auto* receiver=require(a);if(observation)++observation->endedRotationCalls;
            LifecycleEvent(receiver,LifecycleEventKind::EndedRotation,nullptr,execution);
        };
        return QuestVr::RotateOriginalActor(source,deltaSeconds,q);
    }
    QuestVr::OriginalActorPerformPhysicsStats RetailPerformActorPhysics(RuntimeObject* actor,float deltaSeconds,
        QuestVr::Vm::Execution& execution,PortableActorPhysicsObservation* observation=nullptr,
        PortableStartupFallingObservation* falling=nullptr) {
        if(!transaction_)throw std::runtime_error("Original performPhysics requires its caller root");
        const auto source=reinterpret_cast<std::uintptr_t>(actor);LifecycleActor(source);
        const auto require=[&](std::uintptr_t identity) {
            if(identity!=source)throw std::runtime_error("Original performPhysics source identity changed");
            return LifecycleActor(identity);
        };
        QuestVr::OriginalActorPerformPhysicsQueries q;q.work=[this]{LifecycleStep();};
        q.physics=[&](auto a){auto* receiver=require(a);return static_cast<std::uint8_t>(Read(receiver,ActorPhysicsProperty(receiver),0u).integer);};
        q.falling=[&](auto a,float dt,std::int32_t iterations) {
            require(a);if(iterations)throw std::runtime_error("Original Actor falling dispatch changed its zero iteration argument");
            if(observation)++observation->fallingCalls;
            static_cast<void>(RetailStepActorFalling(actor,dt,execution,falling));
        };
        q.otherPhysicsBlock=[&](auto a,QuestVr::OriginalActorPhysicsDispatch mode,float dt) {
            auto* receiver=require(a);
            if(mode==QuestVr::OriginalActorPhysicsDispatch::MovingBrush) {
                RetailPerformMovingBrush(receiver,dt,execution);return;
            }
            throw std::runtime_error("Original Actor performPhysics unsupported dispatch actor="+
                receiver->reflection.objectPath+" mode="+std::to_string(static_cast<unsigned>(mode)));
        };
        q.rotationRate=[&](auto a,unsigned axis){return PhysicsRotatorChannel(require(a),"Engine.Actor.RotationRate",0x21u,axis);};
        q.physicsRotation=[&](auto a,float dt) {
            require(a);if(observation)++observation->rotationCalls;
            static_cast<void>(RetailPhysicsRotation(actor,dt,execution,observation));
        };
        q.pendingTouch=[&](auto a) {
            auto* receiver=LifecycleActor(a);
            if(!receiver->nativePendingTouchBound)throw std::runtime_error("Original PendingTouch read lacks initialized typed Actor ownership");
            static_cast<void>(PendingTouchField(receiver));return reinterpret_cast<std::uintptr_t>(receiver->nativePendingTouch);
        };
        q.postTouch=[&](auto retained,auto other) {
            auto* receiver=LifecycleActor(retained);require(other);if(observation)++observation->postTouchCalls;
            LifecycleEvent(receiver,LifecycleEventKind::PostTouch,actor,execution);
        };
        q.writePendingTouch=[&](auto owner,auto next) {
            auto* receiver=LifecycleActor(owner);auto* target=next?LifecycleActor(next):nullptr;
            if(observation)++observation->pendingWrites;
            WriteNativePendingTouch(receiver,target,execution);
        };
        return QuestVr::PerformOriginalActorPhysics(source,deltaSeconds,q);
    }
    std::vector<std::pair<std::string,std::string>> PhysicsPendingSnapshot() {
        std::vector<std::pair<std::string,std::string>> rows;
        for(auto* actor:persistentRuntime->get()->exports)if(actor&&actor->nativePendingTouchBound) {
            LifecycleStep();if(rows.size()>=4096u)throw std::runtime_error("Original physics detached Actor-edge rows exceed their bound");
            const auto value=Read(actor,PendingTouchField(actor),0u);
            const auto bytes=2u*(sizeof(decltype(rows)::value_type)+actor->reflection.objectPath.size()+value.text.size()+2u);
            ValidateStateBudget(bytes);nativeActorOrderObservationBytes_=QuestVr::DecalStateDetail::Add(nativeActorOrderObservationBytes_,bytes);
            rows.emplace_back(actor->reflection.objectPath,value.text);
        }
        return rows;
    }
    Value ProbeNativeActorPhysics(const std::vector<std::string>& paths,float deltaSeconds,
        PortableActorPhysicsProbeResult& result,QuestVr::Vm::Execution& execution) {
        if(paths.empty()||paths.size()>8u)throw std::runtime_error("Original physics diagnostic requires one to eight actual Actor identities");
        actorPhysicsPreview_=true;result.actors.reserve(paths.size());
        for(const auto& path:paths) {
            if(path.empty()||path.size()>1024u||path.find('\0')!=std::string::npos)
                throw std::runtime_error("Original physics diagnostic Actor identity exceeds its bound");
            auto* actor=LifecycleActor(reinterpret_cast<std::uintptr_t>(Object(path)));
            result.actors.push_back({});auto& row=result.actors.back();row.actorPath=path;row.pendingBefore=PhysicsPendingSnapshot();
            row.entered=true;
            try {
                static_cast<void>(RetailPerformActorPhysics(actor,deltaSeconds,execution,&row));row.returned=true;
                row.pendingAfter=PhysicsPendingSnapshot();
            }catch(...) {row.pendingAfter=PhysicsPendingSnapshot();throw;}
        }
        return Value::Bool(true);
    }
