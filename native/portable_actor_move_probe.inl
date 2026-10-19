// Included inside PortableActorVmHost. Actual movement services in a diagnostic
// root that ALWAYS rolls back. No fabricated pose, begun flag, role or player.
    void ProbeNativeMoveCommand(const PortableRetailCollisionCommand& command,std::size_t index,
        PortableRetailCollisionProbeResult& result,QuestVr::Vm::Execution& execution) {
        using Operation=PortableRetailCollisionOperation;
        if(command.actorPath.empty()||command.actorPath.size()>8192u||command.actorPath.find('\0')!=std::string::npos)
            throw std::runtime_error("Original movement diagnostic requires one bounded actual Actor path");
        auto* actor=RetailCollisionActor(reinterpret_cast<std::uintptr_t>(Object(command.actorPath)));
        const auto level=reinterpret_cast<std::uintptr_t>(actor->nativeXLevel),source=reinterpret_cast<std::uintptr_t>(actor);
        ReserveRetailDiagnosticOutput(command.actorPath.size()+1u+4u*sizeof(void*));
        PortableRetailMoveSnapshot snapshot;snapshot.commandIndex=index;snapshot.actorPath=command.actorPath;
        snapshot.locationBefore=NativeCoordsRead(actor,"Engine.Actor.Location",Kind::Vector,0x23u).vector;
        snapshot.rotationBefore=NativeCoordsRead(actor,"Engine.Actor.Rotation",Kind::Rotator,0x23u).rotation;
        snapshot.colLocationBefore=NativeCoordsRead(actor,"Engine.Actor.ColLocation",Kind::Vector,2u).vector;
        snapshot.clockBefore=ZoneClockSnapshot(actor);
        const PortableModelVec3 delta{command.end[0u],command.end[1u],command.end[2u]};
        if(command.operation==Operation::LevelMoveActor){
            QuestVr::OriginalActorLineHit hit;hit.time=.375f;hit.item=127;hit.location={11,12,13};hit.normal={-1,0,0};
            snapshot.returned=MoveLevelActor(level,source,delta,command.rotation,hit,command.moveOptions,execution)?1:0;
            snapshot.hitAvailable=true;snapshot.hitNextPresent=hit.next!=0u;
            snapshot.hit.location={hit.location.x,hit.location.y,hit.location.z};
            snapshot.hit.normal={hit.normal.x,hit.normal.y,hit.normal.z};snapshot.hit.time=hit.time;snapshot.hit.item=hit.item;
            if(hit.actor){auto* owner=ZoneActor(hit.actor,ZoneLevel(level));
                ReserveRetailDiagnosticOutput(owner->reflection.objectPath.size()+1u+4u*sizeof(void*));snapshot.hit.actorPath=owner->reflection.objectPath;}
            if(hit.primitive){MoveHitPrimitiveIdentity(hit.primitive);auto* primitive=reinterpret_cast<RuntimeObject*>(hit.primitive);
                ReserveRetailDiagnosticOutput(primitive->reflection.objectPath.size()+1u+4u*sizeof(void*));snapshot.hit.primitivePath=primitive->reflection.objectPath;}
        }else if(command.operation==Operation::ActorMove||command.operation==Operation::ActorSetRotation){
            const auto path=command.operation==Operation::ActorMove?"Engine.Actor.Move":"Engine.Actor.SetRotation";
            const auto declaration=FunctionFor(Object(path));
            const auto input=command.operation==Operation::ActorMove?Value::Vector(command.end):Value::Rotator(command.rotation);
            const auto value=execution.CallFunction(*declaration,actor->reflection.objectPath,{{input,{}}});
            if(value.kind!=Kind::Bool)throw std::runtime_error("Original Move/SetRotation wrapper did not return Bool");
            snapshot.returned=value.boolean?1:0;
        }else if(command.operation==Operation::ActorMoveSmooth){
            snapshot.returned=MoveActorSmooth(source,delta,execution);
        }else if(command.operation==Operation::LevelCheckEncroachment){
            snapshot.returned=MoveLevelEncroachment(level,source,delta,command.rotation,command.moveTouch,execution)?1:0;
        }else throw std::runtime_error("Unknown original movement diagnostic operation");
        snapshot.locationAfter=NativeCoordsRead(actor,"Engine.Actor.Location",Kind::Vector,0x23u).vector;
        snapshot.rotationAfter=NativeCoordsRead(actor,"Engine.Actor.Rotation",Kind::Rotator,0x23u).rotation;
        snapshot.colLocationAfter=NativeCoordsRead(actor,"Engine.Actor.ColLocation",Kind::Vector,2u).vector;
        snapshot.clockAfter=ZoneClockSnapshot(actor);
        if(nativeMoveHits_){snapshot.marksAfter=nativeMoveHits_->MarkDepth();snapshot.arenasAfter=nativeMoveHits_->ArenaCount();}
        result.moves.push_back(std::move(snapshot));
    }
