// Actual first Training only. PlayWaiting, Move and all pose/clock reads use
// the original indexed Doctor1; no begun flag, role, pose or clock is injected.
// This includes one COMMITTED native Move, then restores the exact checkpoint.
// The current public surface does not expose the derived legacy registry's
// records. Publication is witnessed by successful Commit, its one revision and
// the durable/render-consumer pose, not a direct registry-content assertion.
void CommittedAnimatedActorMove(Temporary& temporary) {
    using Value=QuestVr::Vm::Value;
    const std::string doctor="00_Training.Doctor1",info="00_Training.LevelInfo0";
    const auto before=temporary.Save("BeforeAnimatedMove");
    const auto checkpoint=temporary.Write("AnimatedMoveReset",before);
    const auto ownership=ReadPortableRuntimeObjectOwnership(doctor);
    const auto native=ReadPortableRuntimeNativeModel();
    const auto world=GetPortableRuntimeMapActors(true);
    Require(native.available&&ownership.objectIndex&&world.size()>1u&&
        !ReadPortableRuntimeRetailCollision().initialized,
        "Animated Move requires actual dormant first Training with no native hash");
    Require(!Declared(info,"Engine.LevelInfo.bBegunPlay",Kind::Bool).boolean&&
        !Declared(doctor,"Engine.Actor.bStatic",Kind::Bool).boolean&&
        Declared(doctor,"Engine.Actor.bMovable",Kind::Bool).boolean&&
        Declared(doctor,"Engine.Actor.bCollideWorld",Kind::Bool).boolean,
        "Actual Doctor1 is not a dormant, movable world-colliding animation control");

    const auto waiting=ExecutePortableActorFunction(doctor,"PlayWaiting");
    Require(waiting.passed()&&waiting.committed&&waiting.instructions>0u,
        "Move clock control failed actual Doctor1.PlayWaiting: "+waiting.error);
    const auto seeded=temporary.Save("AnimatedMoveSeed");
    const auto canonicalClock=[&](const Bytes& bytes) {
        const Envelope envelope(bytes);
        const auto row=std::find_if(envelope.state.objects.begin(),envelope.state.objects.end(),
            [&](const auto& object){return object.path==doctor;});
        Require(row!=envelope.state.objects.end()&&row->clock.has_value()&&
            !row->clock->pose.main.sequence.empty(),
            "Actual animated Move control lost its genuine saved animation clock");
        QuestVr::ScriptSavedState state;state.mapName="00_Training";
        QuestVr::ScriptSavedObject object;object.path=doctor;object.classPath=row->classPath;
        object.clock=row->clock;state.objects.push_back(std::move(object));
        return QuestVr::EncodeScriptSavedState(state);
    };
    const auto clock=canonicalClock(seeded);
    const auto origin=Declared(doctor,"Engine.Actor.Location",Kind::Vector).vector;
    const auto rotation=Declared(doctor,"Engine.Actor.Rotation",Kind::Rotator).rotation;
    const auto colLocation=Declared(doctor,"Engine.Actor.ColLocation",Kind::Vector).vector;
    for(const auto component:origin)
        Require(std::isfinite(component)&&std::abs(component)<8'388'608.0f,
            "Actual Doctor1 location is outside the small exact-add test domain");
    const Envelope seedEnvelope(seeded);
    GC::Collect();const auto gc=GC::GetStats();
    const auto lookup=ReadPortableRuntimeObjectLookup();
    const auto revision=GetPortableRuntimeWorldRevision();
    const auto sameIdentity=[&] {
        const auto current=ReadPortableRuntimeObjectLookup();
        const auto actor=ReadPortableRuntimeObjectOwnership(doctor);
        const auto model=ReadPortableRuntimeNativeModel();
        Require(current.generation==lookup.generation&&current.objects==lookup.objects&&
            actor.objectPath==ownership.objectPath&&actor.classPath==ownership.classPath&&
            actor.outerPath==ownership.outerPath&&actor.objectIndex==ownership.objectIndex&&
            actor.classIndex==ownership.classIndex&&actor.outerIndex==ownership.outerIndex&&
            model.mapGeneration==native.mapGeneration&&model.levelPath==native.levelPath&&
            model.modelPath==native.modelPath,
            "Animated Move changed real Actor/lookup/current-Level identity");
        const auto actors=GetPortableRuntimeMapActors(true);
        Require(actors.size()==world.size(),"Animated Move changed actual Training Actor census");
        for(std::size_t i=0u;i<world.size();++i)
            Require(actors[i].objectPath==world[i].objectPath&&actors[i].classPath==world[i].classPath,
                "Animated Move changed actual Training Actor slot/class ordering");
        const auto hash=ReadPortableRuntimeRetailCollision();
        Require(!hash.initialized&&!hash.enabled&&!hash.hashIdentity&&hash.actors.empty()&&
            hash.seed==*seedEnvelope.state.randomSeed&&!hash.initializationDraws&&
            !Declared(info,"Engine.LevelInfo.bBegunPlay",Kind::Bool).boolean,
            "World-only animated Move initialized a native hash/RNG domain or began Training");
    };
    const auto unchanged=[&] {
        GC::Collect();SameGc(gc,"Animated Move preview/refusal leaked runtime objects");
        sameIdentity();
        Require(temporary.Save("AfterAnimatedMovePreview")==seeded&&
            GetPortableRuntimeWorldRevision()==revision,
            "Animated Move preview/refusal changed complete clock/save/revision state");
        SameVector(Declared(doctor,"Engine.Actor.Location",Kind::Vector).vector,origin,
            "Animated Move preview/refusal retained a partial Location");
    };

    // Find an actually unobstructed NONZERO upward move using the complete
    // Level service and its caller-visible hit. No collision flags/options are
    // disabled. Every attempted query must execute successfully; an unsupported
    // provider is a test failure, not a reason to silently choose another case.
    std::optional<Command> selected;
    Array destination{};std::size_t attempts{};
    for(const float rise:std::array<float,4u>{0.25f,1.0f,2.0f,4.0f}) {
        Command command;command.operation=Operation::LevelMoveActor;command.actorPath=doctor;
        command.end={0.0f,0.0f,rise};command.rotation=rotation;
        auto expected=origin;
        // Two binary32 operands add exactly in binary64 in this bounded source
        // coordinate domain; only the independent final binary32 store rounds.
        expected[2u]=static_cast<float>(double(origin[2u])+double(rise));
        const auto result=ProbePortableRuntimeRetailCollision({command});++attempts;
        Require(result.available&&result.execution.passed()&&!result.execution.committed&&
            result.moves.size()==1u,
            "Actual animated nonzero Level.MoveActor probe failed: "+result.execution.error);
        const auto& move=result.moves.front();
        Require(move.commandIndex==0u&&move.actorPath==doctor&&move.hitAvailable&&
            !move.marksAfter&&!move.arenasAfter&&move.clockBefore==clock&&move.clockAfter==clock,
            "Actual animated Level.MoveActor changed clock in-root or retained native hit storage");
        SameVector(move.locationBefore,origin,"Animated Level.MoveActor sampled a different source pose");
        SameVector(move.colLocationBefore,colLocation,"Animated Level.MoveActor sampled a different source ColLocation");
        SameVector(move.colLocationAfter,colLocation,"Absent-hash Level.MoveActor invented ColLocation registration");
        Require(move.rotationBefore==rotation&&move.rotationAfter==rotation,
            "Translation-only animated Level.MoveActor changed Rotation");
        if(move.returned==1&&move.hit.time==1.0f&&move.hit.actorPath.empty()&&
            move.hit.primitivePath.empty()&&!move.hitNextPresent) {
            SameVector(move.locationAfter,expected,"Unobstructed animated Move has a wrong stored destination");
            Require(std::bit_cast<std::uint32_t>(expected[2u])!=std::bit_cast<std::uint32_t>(origin[2u]),
                "Animated Move fixture accidentally selected a zero/rounded-away displacement");
            command.operation=Operation::ActorMove;selected=command;destination=expected;
        }
        unchanged();if(selected)break;
    }
    Require(selected.has_value(),
        "Actual Doctor1 has no unobstructed upward Move among .25/1/2/4 source units; no zero-only substitute accepted");

    // Actual native266 runs and its stores are visible before the late failure.
    // The SAME root must then undo pose/zone/clock state and any retained hits.
    const auto late=ProbePortableRuntimeRetailCollision({*selected,Command{static_cast<Operation>(999)}});
    Require(!late.available&&!late.execution.passed()&&!late.execution.committed&&
        late.execution.writes>=6u&&late.moves.size()==1u&&late.moves.front().returned==1&&
        late.moves.front().clockBefore==clock&&late.moves.front().clockAfter==clock,
        "Animated native Move late refusal missed its successful full pose-write prefix");
    SameVector(late.moves.front().locationAfter,destination,
        "Late-refused animated native Move never reached the genuine nonzero destination");
    ++refusals;unchanged();
    QuestVr::Vm::Limits firstWrite;firstWrite.writes=1u;
    const auto partial=ProbePortableRuntimeRetailCollision({*selected},firstWrite);
    Require(!partial.available&&!partial.execution.passed()&&!partial.execution.committed&&
        partial.execution.writes==1u,
        "Animated native Move did not refuse after its first journaled scalar store");
    ++refusals;unchanged();

    const auto moved=ExecutePortableActorFunction(doctor,"Move",{{Value::Vector(selected->end),{}}});
    Require(moved.passed()&&moved.committed&&moved.value.kind==Kind::Bool&&moved.value.boolean,
        "Actual committed animated Engine.Actor.Move failed: "+moved.error);
    Require(GetPortableRuntimeWorldRevision()==revision+1u,
        "Committed animated Move failed to publish exactly one shared world revision");
    SameVector(Declared(doctor,"Engine.Actor.Location",Kind::Vector).vector,destination,
        "Committed animated Move did not retain its genuine nonzero Location");
    SameVector(Declared(doctor,"Engine.Actor.ColLocation",Kind::Vector).vector,colLocation,
        "Committed absent-hash animated Move invented native ColLocation history");
    Require(Declared(doctor,"Engine.Actor.Rotation",Kind::Rotator).rotation==rotation,
        "Committed translation-only animated Move changed original Rotation");
    const auto committed=temporary.Save("CommittedAnimatedMove");
    Require(committed!=seeded&&canonicalClock(committed)==clock,
        "Committed nonzero native Move lost its pose or reconstructed/advanced the genuine native animation clock");
    const Envelope committedEnvelope(committed);
    Require(committedEnvelope.state.randomSeed==seedEnvelope.state.randomSeed,
        "Committed world-only native Move consumed the shared random stream");
    const auto rendered=GetPortableRuntimeMapActors(true);
    const auto shown=std::find_if(rendered.begin(),rendered.end(),[&](const auto& actor){return actor.objectPath==doctor;});
    Require(shown!=rendered.end()&&shown->hasLocation,
        "Committed animated Move disappeared from the actual render-consumer Actor snapshot");
    SameVector({shown->x,shown->y,shown->z},destination,
        "Committed native Move published a stale render-consumer pose");
    sameIdentity();GC::Collect();SameGc(gc,"Committed animated Move retained new runtime UObjects");

    Require(LoadPortableRuntimeState(checkpoint.string()),"Cannot restore before real animated Move control");
    GC::Collect();SameGc(gc,"Restoring animated Move control leaked actual runtime objects");sameIdentity();
    Require(temporary.Save("RestoredAnimatedMove")==before,
        "Animated Move test retained its committed Location, PlayWaiting clock or other checkpoint fields");
    std::cout<<"FIRST TRAINING animated Move: actual Doctor1.PlayWaiting, "<<attempts
        <<" collision-enabled nonzero upward fixture probe(s), in-root canonical clock preservation, "
        "late/partial-write rollback, then one committed Engine.Actor.Move with exact durable/render pose, "
        "unchanged canonical native clock/RNG/lookup/GC and one revision; exact checkpoint restored. "
        "No native hash/forced startup. Commit's derived-cache path is exercised, but legacy-registry contents "
        "are not publicly observable and are not independently asserted.\n";
}
