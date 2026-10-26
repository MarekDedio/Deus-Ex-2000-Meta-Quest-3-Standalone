// APawn.physicsRotation, within the enclosing Level Execution. This is not
// the ordinary Actor rotation coordinator or a renderer/controller pose edit.
    std::array<Property,2u> PawnRotationFlagFields(RuntimeObject* actor) {
        return {OriginalAnimationField(actor,"Engine.Actor","bFixedRotationDir",Kind::Bool,0x21u,"BoolProperty"),
            OriginalAnimationField(actor,"Engine.Actor","bRotateToDesired",Kind::Bool,0x21u,"BoolProperty")};
    }
    std::uint32_t PawnRotationFlags(RuntimeObject* actor,const std::array<Property,2u>& fields) {
        const bool fixed=OriginalAnimationRead(actor,fields[0u],Kind::Bool).boolean;
        const bool desired=OriginalAnimationRead(actor,fields[1u],Kind::Bool).boolean;
        return (fixed?0x20u:0u)|(desired?0x40u:0u);
    }
    void WritePawnRotationFlags(RuntimeObject* actor,std::uint32_t value,QuestVr::Vm::Execution& execution) {
        const auto fields=PawnRotationFlagFields(actor);
        // A relevant-bit lens, NOT a fabricated full native DWORD: update
        // only these two authenticated Bools, leaving every sibling untouched.
        // One unconditional native word store consumes ONE caller write slot.
        if(value&~0x60u)throw std::runtime_error("Original Pawn rotation flag lens received unrelated bits");
        for(const auto& field:fields)ReserveActorPhysicsReferences(field,1u);
        auto reference=std::make_shared<Reference>();reference->zero=Value::Integer(0);
        reference->read=[this,actor,fields]{return Value::Integer(static_cast<std::int32_t>(PawnRotationFlags(actor,fields)));};
        reference->write=[this,actor,fields](const Value& next) {
            if(next.kind!=Kind::Int||(static_cast<std::uint32_t>(next.integer)&~0x60u))
                throw std::runtime_error("Original Pawn rotation flag journal lost its relevant-bit word");
            const auto word=static_cast<std::uint32_t>(next.integer);Touch(actor);
            actor->scriptValues[InstanceStorageKey(actor->cls,fields[0u])][0u]=Value::Bool((word&0x20u)!=0u);
            actor->scriptValues[InstanceStorageKey(actor->cls,fields[1u])][0u]=Value::Bool((word&0x40u)!=0u);
            actor->committedScriptState=true;ValidateStateBudget(); // No animation rebuild.
        };
        execution.WriteReference(reference,Value::Integer(static_cast<std::int32_t>(value)));
    }
    QuestVr::OriginalPawnRotationStats RetailPawnPhysicsRotation(RuntimeObject* actor,float elapsed,
        const std::array<float,3u>& oldVelocity,QuestVr::Vm::Execution& execution) {
        if(!transaction_)throw std::runtime_error("Original Pawn rotation lacks its caller root");
        const auto source=reinterpret_cast<std::uintptr_t>(actor);actor=LifecycleActor(source);
        const auto require=[&](auto identity) {
            if(identity!=source)throw std::runtime_error("Original Pawn rotation changed its retained source");
            return LifecycleActor(identity);
        };
        QuestVr::OriginalPawnRotationQueries q;q.work=[this]{LifecycleStep();};
        q.rotationFlags=[&](auto a){auto* r=require(a);return PawnRotationFlags(r,PawnRotationFlagFields(r));};
        q.writeRotationFlags=[&](auto a,std::uint32_t value){WritePawnRotationFlags(require(a),value,execution);};
        q.rotation=[&](auto a,unsigned axis){return PhysicsRotatorChannel(require(a),"Engine.Actor.Rotation",0x23u,axis);};
        q.desiredRotation=[&](auto a,unsigned axis){return PhysicsRotatorChannel(require(a),"Engine.Actor.DesiredRotation",0x21u,axis);};
        q.rotationRate=[&](auto a,unsigned axis){return PhysicsRotatorChannel(require(a),"Engine.Actor.RotationRate",0x21u,axis);};
        q.isPlayerPawnClass=[&](auto a){return OriginalPawnPlayerCast(require(a))!=0u;};
        q.isPawnClass=[&](auto a){return FallingNativeClass(require(a),"Engine.Pawn");};
        q.physics=[&](auto a){auto* r=require(a);return static_cast<std::uint8_t>(OriginalAnimationRead(r,ActorPhysicsProperty(r),Kind::Byte).integer);};
        q.level=[&](auto a){return reinterpret_cast<std::uintptr_t>(require(a)->nativeXLevel);};
        q.moveActor=[&](auto l,auto a,auto delta,auto rotation,auto& hit,auto options) {
            require(a);
            if(delta.x!=0.0f||delta.y!=0.0f||delta.z!=0.0f||options!=std::array<std::int32_t,4u>{})
                throw std::runtime_error("Original Pawn rotation changed its zero-translation/four-zero movement packet");
            return MoveLevelActor(l,a,delta,rotation,hit,options,execution)?1:0;
        };
        return QuestVr::RotateOriginalPawn(source,elapsed,oldVelocity,q);
    }
