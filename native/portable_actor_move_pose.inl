// Included inside PortableActorVmHost. Narrow original-native pose authority,
// not a relaxation of generic ReferenceFor's active-hash mutation guards.
// Each original scalar store consumes ONE write in the SAME VM journal.
    void MoveWriteLocation(RuntimeObject* actor,unsigned channel,float value,QuestVr::Vm::Execution& execution) {
        if(channel>=3u)throw std::runtime_error("Original Move Location channel is invalid");
        const auto identity=reinterpret_cast<std::uintptr_t>(actor);
        static_cast<void>(RetailCollisionActor(identity));
        const auto property=NativeCoordsProperty(actor,"Engine.Actor.Location",Kind::Vector,0x23u);
        const auto token=nativeActorPoses_.CaptureLocationWrite(identity);
        auto ref=std::make_shared<Reference>();ref->zero=Value::Float(0.0f);
        if(token){
            ref->read=[this,identity,token,channel]{
                nativeActorPoses_.ValidateLocationWrite(token);
                return Value::Float(nativeActorPoses_.FindLocation(identity)->at(channel));
            };
            // The VM reuses this closure during delayed undo. It must never
            // redispatch into a reused scope or into durable actor storage.
            ref->write=[this,token,channel](const Value& v){
                if(v.kind!=Kind::Float)throw std::runtime_error("Original temporary Move Location store is not Float");
                static_cast<void>(nativeActorPoses_.UndoLocation(token,channel,v.floating));
            };
            nativeActorPoses_.ValidateLocationWrite(token); // EACH ordinary write, immediately before VM entry.
        }else{
            ref->read=[this,actor,property,channel]{return Value::Float(Read(actor,property,0u).vector.at(channel));};
            ref->write=[this,actor,property,channel](const Value& v){
                if(v.kind!=Kind::Float)throw std::runtime_error("Original durable Move Location store is not Float");
                auto merged=Read(actor,property,0u);
                if(merged.kind!=Kind::Vector)throw std::runtime_error("Original durable Move Location lost its Vector storage");
                merged.vector.at(channel)=v.floating;
                auto normalized=TypedLiveValue(property,merged);Touch(actor);
                actor->scriptValues[InstanceStorageKey(actor->cls,property)][0u]=std::move(normalized);
                actor->committedScriptState=true;
            };
        }
        execution.WriteReference(ref,Value::Float(value));
        if(!token)MarkNativeDurableMove(actor);
        ValidateStateBudget();
        // No UpdateClockProperty, hash, ColLocation or legacy collider write.
        // Those authorities belong to full movement/root publication, not a
        // scalar store (and a reverse-scope Location will be discarded).
    }
    void MoveWriteRotation(RuntimeObject* actor,unsigned channel,std::int32_t value,
        QuestVr::Vm::Execution& execution,bool view=false) {
        if(channel>=3u)throw std::runtime_error("Original Move Rotation channel is invalid");
        const auto identity=reinterpret_cast<std::uintptr_t>(actor);
        static_cast<void>(RetailCollisionActor(identity));
        const char* key=view?"Engine.Pawn.ViewRotation":"Engine.Actor.Rotation";
        const auto property=NativeCoordsProperty(actor,key,Kind::Rotator,view?0x20u:0x23u);
        if(!view)nativeActorPoses_.RequireDurableRotationWrite(identity);
        auto ref=std::make_shared<Reference>();ref->zero=Value::Integer(0);
        ref->read=[this,actor,property,channel]{return Value::Integer(Read(actor,property,0u).rotation.at(channel));};
        ref->write=[this,actor,property,channel](const Value& v){
            if(v.kind!=Kind::Int)throw std::runtime_error("Original durable Move Rotation store is not Int");
            auto merged=Read(actor,property,0u);
            if(merged.kind!=Kind::Rotator)throw std::runtime_error("Original durable Move Rotation lost its Rotator storage");
            merged.rotation.at(channel)=v.integer;
            auto normalized=TypedLiveValue(property,merged);Touch(actor);
            actor->scriptValues[InstanceStorageKey(actor->cls,property)][0u]=std::move(normalized);
            actor->committedScriptState=true;
        };
        // No callbacks/pose-scope changes occur between this admission and the
        // synchronous VM write. Undo must not rerun ordinary scope admission.
        if(!view)nativeActorPoses_.RequireDurableRotationWrite(identity);
        execution.WriteReference(ref,Value::Integer(value));
        if(!view)MarkNativeDurableMove(actor);
        ValidateStateBudget();
    }
