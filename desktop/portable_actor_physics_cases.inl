// Original native3970 on actual first-Training Actors ONLY. Every collision
// diagnostic is rollback-only. These setter controls do not advance a native
// physics phase, begin the world, settle crates, or prove playable gravity.
void ActorPhysicsCoverage(const std::vector<Actor>& actors,const std::function<void()>& rollback) {
    using Value=QuestVr::Vm::Value;
    const std::string lower="00_Training.CrateBreakableMedGeneral1",upper="00_Training.CrateBreakableMedGeneral2";
    const std::string doctor="00_Training.Doctor1",level="00_Training.LevelInfo0";
    const auto source=std::find_if(actors.begin(),actors.end(),[&](const auto& actor){return actor.path==upper;});
    Require(source!=actors.end()&&source->collide&&!source->deleted&&
        !Declared(level,"Engine.LevelInfo.bBegunPlay",Kind::Bool).boolean,
        "SetPhysics lost its genuine dormant first-Training upper crate");
    const auto mode=static_cast<std::uint8_t>(Declared(upper,"Engine.Actor.Physics",Kind::Byte).integer);
    const auto originalVelocity=Declared(upper,"Engine.Actor.Velocity",Kind::Vector).vector;
    const auto originalAcceleration=Declared(upper,"Engine.Actor.Acceleration",Kind::Vector).vector;
    const auto originalCount=static_cast<std::uint8_t>(Declared(lower,"Engine.Actor.StandingCount",Kind::Byte).integer);
    Require(Declared(upper,"Engine.Actor.Base",Kind::Object).text.empty()&&
        Declared(lower,"Engine.Actor.Base",Kind::Object).text.empty(),
        "SetPhysics fixture would replace an original authored Base relationship");
    Require(mode==2u,"SetPhysics original upper crate no longer starts in its authored Falling2 mode");
    const auto physics=[](const std::string& actor,Value requested,std::optional<Value> floor=std::nullopt) {
        Command command{Operation::ActorSetPhysics};command.actorPath=actor;
        command.arguments.push_back(std::move(requested));if(floor)command.arguments.push_back(std::move(*floor));return command;
    };
    const auto base=[](const std::string& actor,const std::string& support) {
        Command command{Operation::ActorSetBase};command.actorPath=actor;
        command.arguments={Value::Text(Kind::Object,support)};return command;
    };
    const auto none=Value::Text(Kind::Object,{}),floor=Value::Text(Kind::Object,doctor);
    std::size_t roots{},setters{},failedRoots{},zeroing{},writeCuts{};
    const auto complete=[&](const std::vector<Command>& commands,const std::string& why,
        const QuestVr::Vm::Limits& limits=QuestVr::Vm::Limits{}) {
        const auto result=ProbePortableRuntimeRetailCollision(commands,limits);
        Require(result.available&&result.execution.passed()&&!result.execution.committed&&
            result.states.size()==commands.size(),"Original SetPhysics root failed: "+why+": "+result.execution.error);
        Require(result.execution.nativeWork&&result.execution.nativeWork->used&&!result.execution.nativeWork->exhausted,
            "SetPhysics has no actual enclosing native-work ledger");
        for(const auto& row:result.physics) {
            Require(row.commandIndex<commands.size()&&commands[row.commandIndex].operation==Operation::ActorSetPhysics&&
                row.actorPath==commands[row.commandIndex].actorPath&&row.writesBefore<=row.writesAfter&&
                row.writesAfter<=result.execution.writes,
                "SetPhysics detached row lost the successful native command/write identity");
        }
        ++roots;setters+=result.physics.size();rollback();return result;
    };
    const auto failed=[&](const std::vector<Command>& commands,const QuestVr::Vm::Limits& limits,const std::string& why) {
        const auto result=ProbePortableRuntimeRetailCollision(commands,limits);
        Require(!result.available&&!result.execution.passed()&&!result.execution.committed&&!result.execution.error.empty(),
            "Invalid/partial SetPhysics root succeeded: "+why);++refusals;++failedRoots;rollback();return result;
    };
    const auto vectors=[&](const auto& row,const Array& velocity,const Array& acceleration,const std::string& why) {
        SameVector(row.velocityAfter,velocity,"SetPhysics Velocity differs: "+why);
        SameVector(row.accelerationAfter,acceleration,"SetPhysics Acceleration differs: "+why);
    };
    const auto noHash=[](const auto& result) {
        for(const auto& state:result.states)Require(!state.initialized&&!state.enabled&&!state.hashIdentity&&
            !state.initializationDraws&&state.seed==InitialSeed&&state.actors.empty(),
            "Hash-inactive SetPhysics created a query/hash/RNG side effect");
    };
    const auto sameHash=[&](const PortableRetailCollisionSnapshot& before,const PortableRetailCollisionSnapshot& after,
        bool queried) {
        Require(before.initialized&&after.initialized&&before.enabled&&after.enabled&&
            before.hashIdentity==after.hashIdentity&&before.mapGeneration==after.mapGeneration&&
            before.levelPath==after.levelPath&&before.seed==after.seed&&
            before.initializationDraws==after.initializationDraws&&before.lifecycleOperations<=after.lifecycleOperations&&
            before.hashes==after.hashes&&before.activeLinks==after.activeLinks&&before.allocatedLinks==after.allocatedLinks&&
            before.actors.size()==after.actors.size()&&(!queried||after.tag==before.tag+1u)&&
            (queried||after.tag==before.tag),
            "SetPhysics changed hash ownership/registration/RNG or missed its real support-query tag");
        for(std::size_t i=0u;i<before.actors.size();++i) {
            const auto& a=before.actors[i];const auto& b=after.actors[i];
            Require(a.path==b.path&&a.objectIndex==b.objectIndex&&a.collideActors==b.collideActors&&
                a.pendingDelete==b.pendingDelete&&a.declaredMirrorsMatch&&b.declaredMirrorsMatch&&
                (queried||a.tag==b.tag),"SetPhysics changed hash Actor identity/flags/declared mirrors");
            SameVector(a.location,b.location,"SetPhysics moved actual hash Location");
            SameVector(a.colLocation,b.colLocation,"SetPhysics changed actual ColLocation");
        }
    };

    // Equal Physics ignores a genuinely DIFFERENT floor and retains an actual
    // prior Base/count relationship. With no hash there is nowhere to hide an
    // accidental FindBase query. The trailing equal SetBase samples live fields.
    const auto equal=complete({base(upper,lower),physics(upper,Value::Byte(mode),floor),base(upper,lower)},
        "same mode / different explicit floor / no native hash");
    Require(equal.physics.size()==1u&&equal.bases.size()==2u&&equal.execution.writes==2u&&
        equal.physics.front().physicsBefore==mode&&equal.physics.front().physicsAfter==mode&&
        equal.physics.front().baseBefore==lower&&equal.physics.front().baseAfter==lower&&
        equal.physics.front().writesBefore==2u&&equal.physics.front().writesAfter==2u&&
        equal.bases.back().after[2u].standingCount==static_cast<std::uint8_t>(originalCount+1u),
        "Equal SetPhysics wrote fields, inspected a different floor, or changed actual StandingCount");
    SameVector(equal.physics.front().velocityBefore,originalVelocity,"Equal SetPhysics lost source Velocity bits");
    SameVector(equal.physics.front().accelerationBefore,originalAcceleration,"Equal SetPhysics lost source Acceleration bits");
    vectors(equal.physics.front(),originalVelocity,originalAcceleration,"same mode");noHash(equal);

    // Real cold BaseChange is suppressed by the original ProcessEvent gates,
    // but native SetBase still decrements the lower crate's live BYTE counter.
    // A literal unknown byte255 is not clamped to a named enum value.
    const auto detach=complete({Command{Operation::Enable},base(upper,lower),
        physics(upper,Value::Integer(255)),base(upper,{}),base(upper,lower),
        physics(upper,Value::Byte(2u)),base(upper,{}),base(upper,lower)},"raw255 then Falling2 detach actual based crate");
    Require(detach.physics.size()==2u&&detach.bases.size()==5u&&detach.execution.writes==12u&&
        detach.execution.instructions==7u,"Cold Falling/raw255 dispatched a script callback or lost exact stores");
    for(std::size_t i=0u;i<detach.physics.size();++i) {
        const auto& row=detach.physics[i];
        Require(row.baseBefore==lower&&row.baseAfter.empty()&&row.physicsAfter==(i==0u?255u:2u)&&
            row.writesAfter-row.writesBefore==3u,"Falling/raw255 did not perform Physics + count + Base stores");
        vectors(row,originalVelocity,originalAcceleration,"nonzeroing detach");
        const auto& count=detach.bases[i==0u?1u:3u];
        Require(count.after[0u].basePath.empty()&&count.after[0u].standingCount==
            static_cast<std::uint8_t>(Declared(upper,"Engine.Actor.StandingCount",Kind::Byte).integer),
            "Falling/raw255 changed the source StandingCount");
    }
    Require(detach.bases[2u].before[2u].standingCount==originalCount,
        "Raw255 did not decrement the actual lower crate StandingCount before reattachment");
    Require(detach.bases[4u].before[2u].standingCount==originalCount&&
        detach.bases[4u].after[2u].standingCount==static_cast<std::uint8_t>(originalCount+1u),
        "Falling2 did not decrement the actual lower crate StandingCount before the next real attachment");
    for(std::size_t i=1u;i<detach.states.size();++i)sameHash(detach.states.front(),detach.states[i],false);

    // Each exact support mode uses an authentic explicit Doctor floor. Its
    // dynamically selected SupportActor remains suppressed in this cold world;
    // the setter must NOT fake attachment with direct SetBase. No hash needed.
    for(const auto requested:{0u,1u,5u,7u,10u}) {
        const auto result=complete({physics(upper,Value::Byte(3u)),
            physics(upper,Value::Byte(static_cast<std::uint8_t>(requested)),floor),base(upper,{})},
            "explicit original support floor mode "+std::to_string(requested));
        const bool zero=requested==0u||requested==5u;
        Require(result.physics.size()==2u&&result.bases.size()==1u&&result.execution.writes==(zero?8u:2u)&&
            result.execution.instructions==3u&&result.physics.back().physicsBefore==3u&&
            result.physics.back().physicsAfter==requested&&result.physics.back().baseBefore.empty()&&
            result.physics.back().baseAfter.empty()&&result.physics.back().writesAfter-result.physics.back().writesBefore==(zero?7u:1u),
            "Explicit support mode bypassed cold callback eligibility, skipped literal zero words or changed raw mode");
        vectors(result.physics.front(),originalVelocity,originalAcceleration,"changed non-support mode3");
        vectors(result.physics.back(),zero?Array{}:originalVelocity,zero?Array{}:originalAcceleration,"explicit support");
        noHash(result);zeroing+=zero?1u:0u;
    }
    // Incoming finite numeric forms are converted through the authored Byte
    // declaration, not treated as unrelated Int/Float native field storage.
    for(const auto& requested:{Value::Integer(3),Value::Float(3.0f)}) {
        const auto result=complete({physics(upper,requested)},"numeric input canonical Byte coercion");
        Require(result.physics.size()==1u&&result.physics.front().physicsAfter==3u&&result.execution.writes==1u,
            "SetPhysics numeric argument did not become its actual declared Physics BYTE");noHash(result);
    }

    // Independently query the actual upper crate's unposed eight-unit sweep.
    // None floor must call FindBase, not clear Base without querying. The real
    // clear witness then detaches the genuine preceding native SetBase edge.
    Command query{Operation::SingleLineCheck};query.actorPath=upper;query.flags=0x17u;
    query.start=Declared(upper,"Engine.Actor.Location",Kind::Vector).vector;query.end=query.start;
    query.end[2u]=static_cast<float>(double(query.start[2u])-8.0);
    const auto radius=Declared(upper,"Engine.Actor.CollisionRadius",Kind::Float).floating;
    query.extent={radius,radius,Declared(upper,"Engine.Actor.CollisionHeight",Kind::Float).floating};
    SameVector(query.start,source->location,"SetPhysics query replaced original unposed crate Location");
    const auto found=complete({Command{Operation::Enable},query,base(upper,lower),
        physics(upper,Value::Byte(3u)),base(upper,lower),physics(upper,Value::Byte(0u),none),base(upper,{})},
        "None-floor original FindBase query and actual clear-support detach");
    Require(found.physics.size()==2u&&found.singles.size()==1u&&found.singles.front().clear&&
        found.singles.front().hit.actorPath.empty()&&found.bases.size()==3u&&found.execution.writes==16u,
        "None-floor SetPhysics lacks its independent actual clear-support witness or exact store count");
    const auto& settled=found.physics.back();
    Require(settled.physicsBefore==3u&&settled.physicsAfter==0u&&settled.baseBefore==lower&&settled.baseAfter.empty()&&
        settled.writesAfter-settled.writesBefore==9u,
        "None-floor SetPhysics did not call actual FindBase/SetBase before six zero stores");
    vectors(settled,Array{},Array{},"None-floor clear support");++zeroing;
    sameHash(found.states[0u],found.states[1u],true);
    for(const auto i:{2u,3u,4u})sameHash(found.states[i-1u],found.states[i],false);
    sameHash(found.states[4u],found.states[5u],true);sameHash(found.states[5u],found.states[6u],false);

    // Every individual shared-write prefix of two zero blocks and one real
    // detach must poison the enclosing root. Only completed native operations
    // may leave detached observations; all actual world/save/client/GC state
    // is checked by the outer rollback witness after EVERY refusal.
    const std::vector<Command> commands{base(upper,lower),physics(upper,Value::Byte(3u)),
        physics(upper,Value::Byte(5u),floor),physics(upper,Value::Byte(0u),floor),base(upper,{})};
    const auto baseline=complete(commands,"full shared write/work baseline");
    Require(baseline.execution.writes==19u&&baseline.physics.size()==3u&&baseline.bases.size()==2u,
        "SetPhysics baseline lost exact attach/detach/six-word accounting");
    for(std::size_t limit=0u;limit<baseline.execution.writes;++limit) {
        QuestVr::Vm::Limits bounds;bounds.writes=limit;
        const auto cut=failed(commands,bounds,"each shared write prefix "+std::to_string(limit));
        Require(cut.execution.status==QuestVr::Vm::Status::Budget&&cut.execution.writes==limit,
            "SetPhysics partial stores escaped shared write admission");
        const auto successful=static_cast<std::size_t>(limit>=5u)+static_cast<std::size_t>(limit>=12u)+
            static_cast<std::size_t>(limit>=19u);
        Require(cut.physics.size()==successful&&cut.states.size()==(limit<2u?0u:1u)+successful,
            "Refused SetPhysics exposed an incomplete native snapshot or lost a completed write prefix");++writeCuts;
    }
    QuestVr::Vm::Limits exactWrites;exactWrites.writes=baseline.execution.writes;
    complete(commands,"exact N shared write budget",exactWrites);
    QuestVr::Vm::Limits exactWork;exactWork.nativeWork=baseline.execution.nativeWork->used;
    const auto enough=complete(commands,"exact N shared native work",exactWork);
    Require(enough.execution.nativeWork&&enough.execution.nativeWork->used==exactWork.nativeWork,
        "SetPhysics exact native-work root changed its authority or usage");
    QuestVr::Vm::Limits shortWork;shortWork.nativeWork=exactWork.nativeWork-1u;
    const auto exhausted=failed(commands,shortWork,"N-1 shared native work");
    Require(exhausted.execution.status==QuestVr::Vm::Status::Budget&&exhausted.execution.nativeWork&&
        exhausted.execution.nativeWork->exhausted&&exhausted.execution.nativeWork->used<=shortWork.nativeWork,
        "SetPhysics did not poison the caller's shared native-work ledger");
    const auto prefix=complete({commands[0u],commands[1u]},"completed first physics native-work prefix");
    QuestVr::Vm::Limits shared;shared.nativeWork=prefix.execution.nativeWork->used;
    const auto sharedCut=failed(commands,shared,"later native cannot acquire fresh work allowance");
    Require(sharedCut.execution.status==QuestVr::Vm::Status::Budget&&sharedCut.execution.nativeWork&&
        sharedCut.execution.nativeWork->exhausted&&sharedCut.physics.size()==1u&&sharedCut.states.size()==2u&&
        sharedCut.execution.writes==5u,"Later SetPhysics reset native work or lost its complete original prefix");
    auto late=commands;late.push_back(Command{static_cast<Operation>(999)});
    const auto refused=failed(late,{},"late malformed operation after all original stores");
    Require(refused.physics.size()==3u&&refused.bases.size()==2u&&refused.states.size()==5u&&
        refused.execution.writes==19u,"Late refusal discarded completed native observations before full undo");

    std::vector<Command> invalid;
    for(const auto& value:{Value::Bool(true),Value::Text(Kind::String,"2"),Value::Text(Kind::Name,"PHYS_Falling"),
        Value::Text(Kind::Object,lower),Value::Vector({})})invalid.push_back(physics(upper,value));
    auto bad=physics(upper,Value::Byte(2u));bad.arguments.clear();invalid.push_back(bad);
    bad=physics(upper,Value::Byte(2u),floor);bad.arguments.push_back(none);invalid.push_back(bad);
    for(const auto& value:{Value::Integer(1),Value::Text(Kind::Object,"Engine.Actor"),
        Value::Text(Kind::Object,"00_Training.NoSuchFloor"),
        Value::Text(Kind::Object,Declared(upper,"Engine.Actor.Mesh",Kind::Object).text)})
        invalid.push_back(physics(upper,Value::Byte(2u),value));
    for(const auto& path:{std::string{},std::string("Engine.Actor"),std::string("00_Training.NoSuchActor")})
        invalid.push_back(physics(path,Value::Byte(2u)));
    for(std::size_t i=0u;i<invalid.size();++i)failed({invalid[i]}, {},"bounded argument/receiver/schema case "+std::to_string(i));
    // Original MultiLine/SingleLine deliberately permit Level.Hash=None: the
    // native Model world query still runs, while actor hashing/RNG/tag work is
    // absent. This is not missing authority and must NOT manufacture a refusal.
    // Keep the actual unposed query as an independent witness, then sample the
    // same original FindBase service after the native setter's genuine detach.
    Command find{Operation::ActorFindBase};find.actorPath=upper;
    const auto worldOnly=complete({query,base(upper,lower),physics(upper,Value::Byte(3u)),base(upper,lower),
        physics(upper,Value::Byte(0u)),base(upper,{}),find},
        "None-floor real Model support query with native Hash=None");
    Require(worldOnly.physics.size()==2u&&worldOnly.singles.size()==1u&&worldOnly.singles.front().clear&&
        worldOnly.singles.front().hit.actorPath.empty()&&worldOnly.bases.size()==3u&&worldOnly.findBases.size()==1u&&
        worldOnly.execution.writes==16u,"Hash-absent original support query lost its world witness or actual store accounting");
    const auto& cleared=worldOnly.physics.back();const auto& supportCheck=worldOnly.findBases.front();
    Require(cleared.physicsBefore==3u&&cleared.physicsAfter==0u&&cleared.baseBefore==lower&&cleared.baseAfter.empty()&&
        cleared.writesAfter-cleared.writesBefore==9u&&supportCheck.actorPath==upper&&supportCheck.clear&&
        supportCheck.hitActor.empty()&&supportCheck.baseBefore.empty()&&supportCheck.baseAfter.empty()&&
        supportCheck.unchanged&&!supportCheck.supportAttempted&&!supportCheck.basedOnSelf&&
        supportCheck.selectedSupportFunction.empty()&&supportCheck.clockBefore==supportCheck.clockAfter&&
        !supportCheck.marksBefore&&!supportCheck.marksAfterQuery&&!supportCheck.arenasBefore&&!supportCheck.arenasAfterQuery,
        "Hash-absent SetPhysics skipped original None-floor FindBase/SetBase or leaked native query lifetime");
    SameVector(supportCheck.start,query.start,"Hash-absent support changed actual unposed query start");
    SameVector(supportCheck.end,query.end,"Hash-absent support changed the exact downward query end");
    SameVector(supportCheck.extent,query.extent,"Hash-absent support changed original cylinder extent");
    vectors(cleared,Array{},Array{},"Hash-absent None-floor clear support");noHash(worldOnly);++zeroing;
    Require(writeCuts==19u&&zeroing==4u&&roots<=24u&&setters<=48u&&failedRoots<=40u,
        "SetPhysics focused first-Training case count escaped its explicit bound");
    std::cout<<"FIRST TRAINING SetPhysics: authored upper Physics="<<unsigned(mode)<<"; "<<roots<<" actual dormant roots / "<<setters<<" native3970 snapshots / "
        <<writeCuts<<" exact partial-write cuts / "<<failedRoots<<" refused roots; equal/different floor, real Base/count detach, "
        "all five support modes, literal255, independent None-floor SingleLine witness, six exact zero words, unchanged "
        "pose/hash/ColLocation/RNG, shared work and full rollback. NOT falling simulation, settled crates or playable gravity.\n";
}

void AnimatedActorPhysicsCoverage(Temporary& temporary) {
    using Value=QuestVr::Vm::Value;
    const std::string doctor="00_Training.Doctor1",crate="00_Training.CrateBreakableMedGeneral1";
    const auto before=temporary.Save("BeforeAnimatedActorPhysics");
    const auto checkpoint=temporary.Write("ActorPhysicsAnimationReset",before);
    const auto waiting=ExecutePortableActorFunction(doctor,"PlayWaiting");
    Require(waiting.passed()&&waiting.committed&&waiting.instructions>0u,
        "SetPhysics clock control failed actual Doctor1.PlayWaiting: "+waiting.error);
    const auto seeded=temporary.Save("AnimatedActorPhysicsSeed");const Envelope envelope(seeded);
    const auto saved=std::find_if(envelope.state.objects.begin(),envelope.state.objects.end(),[&](const auto& row){return row.path==doctor;});
    Require(saved!=envelope.state.objects.end()&&saved->clock.has_value()&&!saved->clock->pose.main.sequence.empty(),
        "SetPhysics clock control lacks its genuine native animation clock");
    QuestVr::ScriptSavedState expected;expected.mapName="00_Training";
    QuestVr::ScriptSavedObject object;object.path=doctor;object.classPath=saved->classPath;object.clock=saved->clock;
    expected.objects.push_back(object);const auto clock=QuestVr::EncodeScriptSavedState(expected);
    const auto physics=[&](std::uint8_t mode) {
        Command command{Operation::ActorSetPhysics};command.actorPath=doctor;
        command.arguments={Value::Byte(mode),Value::Text(Kind::Object,crate)};return command;
    };
    Command witness{Operation::ActorSetBase};witness.actorPath=doctor;witness.arguments={Value::Text(Kind::Object,{})};
    const std::vector<Command> commands{physics(2u),witness,physics(5u),witness,physics(0u),witness};
    GC::Collect();const auto gc=GC::GetStats();const auto revision=GetPortableRuntimeWorldRevision();
    const auto lookup=ReadPortableRuntimeObjectLookup();const auto owner=ReadPortableRuntimeObjectOwnership(doctor);
    const auto model=ReadPortableRuntimeNativeModel();const auto actors=GetPortableRuntimeMapActors(true);
    const auto same=[&] {
        GC::Collect();SameGc(gc,"Animated SetPhysics leaked original runtime objects");
        const auto current=ReadPortableRuntimeObjectLookup();const auto identity=ReadPortableRuntimeObjectOwnership(doctor);
        const auto native=ReadPortableRuntimeNativeModel();const auto hash=ReadPortableRuntimeRetailCollision();
        Require(temporary.Save("AfterAnimatedActorPhysics")==seeded&&GetPortableRuntimeWorldRevision()==revision&&
            current.prepared==lookup.prepared&&current.generation==lookup.generation&&current.objects==lookup.objects&&
            current.retainedBytes==lookup.retainedBytes&&identity.objectIndex==owner.objectIndex&&
            identity.classIndex==owner.classIndex&&identity.outerIndex==owner.outerIndex&&
            model.mapGeneration==native.mapGeneration&&model.levelPath==native.levelPath&&model.modelPath==native.modelPath&&
            !hash.initialized&&!hash.enabled&&!hash.hashIdentity&&hash.actors.empty()&&
            hash.seed==*envelope.state.randomSeed&&!Declared("00_Training.LevelInfo0","Engine.LevelInfo.bBegunPlay",Kind::Bool).boolean&&
            ReadPortableRuntimeLevelTime()==0.0&&HasPortableRuntimeClient()&&ReadPortableRuntimeClientView().size()==1u,
            "Animated SetPhysics lost complete clock/save/revision/lookup/Level/client/hash/RNG rollback");
        const auto world=GetPortableRuntimeMapActors(true);Require(world.size()==actors.size(),"Animated SetPhysics changed Actor census");
        for(std::size_t i=0u;i<world.size();++i)Require(world[i].objectPath==actors[i].objectPath&&world[i].classPath==actors[i].classPath,
            "Animated SetPhysics changed original first-Training Actor slot/class identity");
    };
    const auto clocks=[&](const auto& result,std::size_t count) {
        Require(result.bases.size()==count,"Animated SetPhysics lost its completed in-root clock witnesses");
        for(const auto& row:result.bases)Require(row.before[0u].actorPath==doctor&&row.after[0u].actorPath==doctor&&
            row.clockBefore==clock&&row.clockAfter==clock,
            "SetPhysics reset/reconstructed/advanced the genuine native clock inside its SAME root");
    };
    const auto complete=ProbePortableRuntimeRetailCollision(commands);
    Require(complete.available&&complete.execution.passed()&&!complete.execution.committed&&
        complete.physics.size()==3u&&complete.execution.writes==15u,
        "Animated original3970 sequence failed: "+complete.execution.error);clocks(complete,3u);same();
    auto late=commands;late.push_back(Command{static_cast<Operation>(999)});
    const auto refused=ProbePortableRuntimeRetailCollision(late);
    Require(!refused.available&&!refused.execution.passed()&&!refused.execution.committed&&refused.physics.size()==3u&&
        refused.execution.writes==15u,"Animated SetPhysics late refusal lost its real zero-store prefix");
    clocks(refused,3u);++refusals;same();
    QuestVr::Vm::Limits partial;partial.writes=7u;
    const auto cut=ProbePortableRuntimeRetailCollision(commands,partial);
    Require(!cut.available&&!cut.execution.passed()&&!cut.execution.committed&&cut.execution.status==QuestVr::Vm::Status::Budget&&
        cut.execution.writes==7u&&cut.physics.size()==1u,
        "Animated SetPhysics accepted a partial six-word block or exposed an incomplete native snapshot");
    clocks(cut,1u);++refusals;same();
    Require(LoadPortableRuntimeState(checkpoint.string()),"Cannot restore before animated native3970 clock control");GC::Collect();
    Require(temporary.Save("RestoredAnimatedActorPhysics")==before,
        "Animated SetPhysics retained PlayWaiting clock/Physics/vector fields after exact checkpoint restoration");
    std::cout<<"FIRST TRAINING animated SetPhysics: actual Doctor1.PlayWaiting; unchanged canonical full clock witnessed "
        "inside the same root after Falling2 and both six-word zero blocks; successful/late/partial full rollback, original "
        "checkpoint restored. No invented pose, begun flag, possession or physics-phase claim.\n";
}
