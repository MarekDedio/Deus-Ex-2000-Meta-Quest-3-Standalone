// Original native298 on actual first-Training Actors ONLY. The world remains
// dormant and every diagnostic root rolls back. This is not FindBase/support,
// automatic startup, Destroy, gravity, damage or playable crate acceptance.
void ActorBaseCoverage(const std::vector<Actor>& actors,const std::function<void()>& rollback) {
    using Value=QuestVr::Vm::Value;
    using Fields=PortableRetailBaseFields;
    const std::string lower="00_Training.CrateBreakableMedGeneral1";
    const std::string upper="00_Training.CrateBreakableMedGeneral2";
    const std::string doctor="00_Training.Doctor1",level="00_Training.LevelInfo0";
    const auto actual=[&](const std::string& path)->const Actor& {
        const auto found=std::find_if(actors.begin(),actors.end(),[&](const auto& row){return row.path==path;});
        Require(found!=actors.end(),"SetBase fixture lacks an actual first-Training Actor: "+path);return *found;
    };
    Require(actual(lower).collide&&actual(upper).collide&&!actual(lower).deleted&&!actual(upper).deleted&&
        !Declared(level,"Engine.LevelInfo.bBegunPlay",Kind::Bool).boolean,
        "SetBase fixture lost the real dormant collidable Training crate pair");
    static_cast<void>(actual(doctor));static_cast<void>(actual(level));
    std::size_t authoredBaseLinks{};
    for(const auto& actor:actors)if(!Declared(actor.path,"Engine.Actor.Base",Kind::Object).text.empty())++authoredBaseLinks;
    Require(authoredBaseLinks==0u,"SetBase first-Training source unexpectedly contains authored Base edges");
    struct ModelActor {Fields fields;std::string level;};
    std::vector<ModelActor> initial;
    for(const auto& path:{lower,upper,doctor,level}) {
        const auto count=Declared(path,"Engine.Actor.StandingCount",Kind::Byte).integer;
        Require(count>=0&&count<=255,"Actual StandingCount is not a BYTE");
        initial.push_back({{path,Declared(path,"Engine.Actor.Base",Kind::Object).text,static_cast<std::uint8_t>(count)},
            Declared(path,"Engine.Actor.Level",Kind::Object).text});
    }
    for(std::size_t index=0u;index<3u;++index)Require(initial[index].level==level,
        "Actual crate/Doctor LevelInfo-exception controls lack the real declared Level binding");
    // The model starts from declared bytes, not a reverse-list cardinality or
    // an invented map-load relink. Native BYTE arithmetic is independently
    // expressed as unsigned modulo256, including inconsistent source counts.
    const auto command=[](const std::string& source,const std::string& target) {
        Command result{Operation::ActorSetBase};result.actorPath=source;
        result.arguments={Value::Text(Kind::Object,target)};return result;
    };
    const auto order=SetterOrderQuery(actors,static_cast<std::size_t>(&actual(lower)-actors.data()));
    const auto equalFields=[](const Fields& a,const Fields& b) {
        return a.actorPath==b.actorPath&&a.basePath==b.basePath&&a.standingCount==b.standingCount;
    };
    std::size_t cases{},nativeEntries{},storedFields{},cycleControls{},equalControls{},failedRoots{};
    const auto run=[&](const std::vector<Command>& commands,const std::string& why,
        const QuestVr::Vm::Limits& limits=QuestVr::Vm::Limits{}) {
        Require(!commands.empty()&&commands.size()<=16u,"SetBase reference exceeds the closed diagnostic command bound");
        auto model=initial;
        const auto find=[&](const std::string& path)->ModelActor& {
            const auto found=std::find_if(model.begin(),model.end(),[&](const auto& row){return row.fields.actorPath==path;});
            Require(found!=model.end(),"Independent SetBase model lacks a requested identity: "+path);return *found;
        };
        const auto fields=[&](const std::string& path) {return path.empty()?Fields{}:find(path).fields;};
        struct ExpectedBase {std::size_t command;std::array<Fields,3u> before,after;};
        struct Frame {bool enabled;std::uint32_t tag;std::size_t links;std::vector<std::uint32_t> tags;};
        std::vector<ExpectedBase> expectedBases;std::vector<Frame> frames;
        std::vector<std::pair<std::vector<PortableRetailCollisionHit>,std::size_t>> hits;
        Oracle oracle(actors);bool enabled{};std::size_t writes{},entries{},cycles{},equals{};
        for(std::size_t i=0u;i<commands.size();++i) {
            const auto& input=commands[i];
            if(input.operation==Operation::Enable) {Require(!enabled,"SetBase reference unexpectedly enables twice");enabled=true;}
            else if(input.operation==Operation::LineCheck) {
                Require(enabled,"SetBase reference query lacks its actual native hash");hits.push_back(oracle.Query(input,actors));
            }else {
                Require(input.operation==Operation::ActorSetBase&&input.arguments.size()==1u&&
                    input.arguments.front().kind==Kind::Object,"SetBase success model received an unsupported command");
                const auto source=input.actorPath,target=input.arguments.front().text;
                auto& self=find(source);const auto old=self.fields.basePath;
                const std::array<std::string,3u> tracked{source,old,target};
                ExpectedBase expected;expected.command=i;
                for(std::size_t n=0u;n<tracked.size();++n)expected.before[n]=fields(tracked[n]);
                bool cycle{};std::size_t visits{};
                for(auto parent=target;!parent.empty();parent=find(parent).fields.basePath) {
                    Require(++visits<=model.size(),"Independent SetBase model encountered an unbounded prospective chain");
                    if(parent==source){cycle=true;break;}
                }
                if(cycle)++cycles;
                else if(old==target)++equals;
                else {
                    if(!old.empty()&&old!=self.level) {
                        auto& count=find(old).fields.standingCount;
                        count=static_cast<std::uint8_t>((static_cast<unsigned>(count)+255u)%256u);++writes;
                    }
                    self.fields.basePath=target;++writes;
                    if(!target.empty()&&target!=self.level) {
                        auto& count=find(target).fields.standingCount;
                        count=static_cast<std::uint8_t>((static_cast<unsigned>(count)+1u)%256u);++writes;
                    }
                }
                for(std::size_t n=0u;n<tracked.size();++n)expected.after[n]=fields(tracked[n]);
                expectedBases.push_back(std::move(expected));++entries;
            }
            frames.push_back({enabled,oracle.tag,oracle.links,oracle.tags});
        }
        const auto result=ProbePortableRuntimeRetailCollision(commands,limits);
        Require(result.available&&result.execution.passed()&&!result.execution.committed&&
            result.states.size()==commands.size()&&result.bases.size()==expectedBases.size()&&
            result.execution.writes==writes&&result.execution.instructions==entries&&
            result.queries.size()==hits.size()&&result.queryHeads.size()==hits.size(),
            "Actual dormant SetBase/count sequence failed: "+why+": "+result.execution.error+
                " writes="+std::to_string(result.execution.writes)+" expected="+std::to_string(writes));
        for(std::size_t i=0u;i<expectedBases.size();++i) {
            const auto& expected=expectedBases[i];const auto& snapshot=result.bases[i];
            Require(snapshot.commandIndex==expected.command&&snapshot.clockBefore==snapshot.clockAfter,
                "SetBase lost source command/animation-clock identity: "+why);
            for(std::size_t n=0u;n<3u;++n)Require(equalFields(snapshot.before[n],expected.before[n])&&
                equalFields(snapshot.after[n],expected.after[n]),
                "SetBase actual fixed-identity Base/BYTE trace differs from independent declared-field model: "+why+
                    " command="+std::to_string(expected.command)+" tracked="+std::to_string(n));
        }
        for(std::size_t i=0u;i<frames.size();++i) {
            const auto& state=result.states[i];const auto& expected=frames[i];
            if(!expected.enabled) {
                Require(!state.initialized&&!state.enabled&&!state.hashIdentity&&state.actors.empty()&&
                    !state.initializationDraws&&state.seed==InitialSeed,
                    "Hash-absent SetBase initialized tables/registration/shared CRT state");continue;
            }
            Require(state.initialized&&state.enabled&&state.seed==oracle.seed&&state.tag==expected.tag&&
                state.activeLinks==expected.links&&state.initializationDraws==49152u&&
                state.hashIdentity==result.states.front().hashIdentity&&
                state.mapGeneration==result.states.front().mapGeneration&&
                state.hashes==result.states.front().hashes&&state.allocatedLinks==result.states.front().allocatedLinks,
                "SetBase rebuilt/replaced collision registration or changed native query/RNG history: "+why);
            SameActors(state,actors,expected.tags);
        }
        for(std::size_t i=0u;i<hits.size();++i)SameHits(result.queries[i],result.queryHeads[i],hits[i]);
        for(std::size_t i=1u;i<result.queries.size();++i) {
            Require(result.queries[i].size()==result.queries.front().size(),"SetBase changed collision-query encounter count");
            for(std::size_t n=0u;n<result.queries[i].size();++n)Require(
                result.queries[i][n].actorPath==result.queries.front()[n].actorPath,
                "SetBase performed an unintended collision remove/prepend encounter-order change");
        }
        ++cases;nativeEntries+=entries;storedFields+=writes;cycleControls+=cycles;equalControls+=equals;
        rollback();return result;
    };

    // Establish both directions of ordinary attachment and count transfer,
    // followed by the actual source-LevelInfo exception. Query witnesses keep
    // every primitive/hash encounter independent of these direct Base fields.
    run({Command{Operation::Enable},order,command(lower,upper),order,command(lower,doctor),order,
        command(lower,{}),order,command(lower,level),order,command(lower,{}),order},
        "two real crates / ordinary target transfer / None / actual LevelInfo");
    run({Command{Operation::Enable},command(lower,upper),command(lower,upper),command(lower,lower),
        command(upper,lower),command(lower,{}),command(lower,{}),order},
        "same target / self / indirect two-Actor cycle / repeated None");
    run({Command{Operation::Enable},command(lower,upper),command(upper,doctor),command(lower,doctor),
        command(upper,{}),command(lower,{}),order},
        "two real crates with live ordinary old/new-base count transfers");
    const auto noHash=run({command(lower,upper),command(lower,{}),command(lower,level),command(lower,{})},
        "native298 without a collision hash");
    Require(noHash.execution.writes==6u,"Hash-absent native298 did not retain actual ordinary/LevelInfo store counts");

    const auto reject=[&](const std::vector<Command>& commands,const QuestVr::Vm::Limits& limits,const std::string& why) {
        const auto result=ProbePortableRuntimeRetailCollision(commands,limits);
        Require(!result.available&&!result.execution.passed()&&!result.execution.committed&&!result.execution.error.empty(),
            "Unsupported/partial SetBase root succeeded: "+why);++refusals;++failedRoots;rollback();return result;
    };
    const std::vector<Command> transfer{Command{Operation::Enable},command(lower,upper),command(lower,doctor)};
    const auto complete=run(transfer,"complete ordinary transfer write/work baseline");
    Require(complete.execution.writes==5u&&complete.execution.nativeWork&&complete.execution.nativeWork->used>1u&&
        !complete.execution.nativeWork->exhausted,"SetBase completed root lacks its exact shared write/work accounting");
    QuestVr::Vm::Limits shortWrites;shortWrites.writes=complete.execution.writes-1u;
    const auto partial=reject(transfer,shortWrites,"exact N-1 stores after successful first native298");
    Require(partial.execution.status==QuestVr::Vm::Status::Budget&&partial.execution.writes==shortWrites.writes&&
        partial.bases.size()==1u&&partial.bases.front().commandIndex==1u&&partial.states.size()==2u&&
        partial.bases.front().after[0u].basePath==upper,
        "SetBase N-1 write refusal did not retain its genuine completed prefix before complete outer rollback");
    QuestVr::Vm::Limits exactWrites;exactWrites.writes=complete.execution.writes;
    run(transfer,"exact N stores complete the same transfer",exactWrites);
    auto lateCommands=transfer;lateCommands.push_back(order);lateCommands.push_back(Command{static_cast<Operation>(999)});
    const auto late=reject(lateCommands,{},"late unknown command after genuine transfers and query");
    Require(late.bases.size()==2u&&late.states.size()==4u&&late.queries.size()==1u&&late.execution.writes==5u&&
        late.bases.back().after[0u].basePath==doctor,
        "Late SetBase refusal missed its real changed-field/count/query prefix");
    QuestVr::Vm::Limits shortWork;shortWork.nativeWork=complete.execution.nativeWork->used-1u;
    const auto workCut=reject(transfer,shortWork,"exact N-1 shared native-work allowance");
    Require(workCut.execution.status==QuestVr::Vm::Status::Budget&&workCut.execution.nativeWork&&
        workCut.execution.nativeWork->exhausted&&workCut.execution.nativeWork->used<=shortWork.nativeWork,
        "SetBase native-work exhaustion did not poison the shared enclosing root");
    QuestVr::Vm::Limits exactWork;exactWork.nativeWork=complete.execution.nativeWork->used;
    const auto enough=run(transfer,"exact N shared native-work allowance",exactWork);
    Require(enough.execution.nativeWork&&enough.execution.nativeWork->used==exactWork.nativeWork&&
        !enough.execution.nativeWork->exhausted,"SetBase exact N work differs from its identical completed root");
    const std::vector<Command> firstOnly{Command{Operation::Enable},command(lower,upper)};
    const auto first=run(firstOnly,"first native298 shared-work baseline");
    Require(first.execution.nativeWork.has_value(),"First SetBase root has no shared work ledger");
    QuestVr::Vm::Limits sharedWork;sharedWork.nativeWork=first.execution.nativeWork->used;
    const auto shared=reject(transfer,sharedWork,"second native298 may not acquire a fresh root allowance");
    Require(shared.execution.status==QuestVr::Vm::Status::Budget&&shared.execution.nativeWork&&
        shared.execution.nativeWork->exhausted&&shared.bases.size()==1u&&shared.states.size()==2u,
        "Second SetBase received a fresh native-work ledger or lost its genuine first prefix");

    std::vector<Command> invalid;
    for(const auto& value:{Value::Integer(1),Value::Text(Kind::String,upper),Value::Text(Kind::Object,"Engine.Actor"),
        Value::Text(Kind::Object,Declared(lower,"Engine.Actor.Mesh",Kind::Object).text),
        Value::Text(Kind::Object,"00_Training.NoSuchBase")}) {
        auto bad=command(lower,upper);bad.arguments={value};invalid.push_back(std::move(bad));
    }
    auto bad=command(lower,upper);bad.arguments.clear();invalid.push_back(bad);
    bad=command(lower,upper);bad.arguments.push_back(Value::Text(Kind::Object,doctor));invalid.push_back(bad);
    for(const auto& path:{std::string{},std::string("Engine.Actor"),std::string("00_Training.NoSuchActor")}) {
        bad=command(path,upper);invalid.push_back(bad);
    }
    for(std::size_t i=0u;i<invalid.size();++i)reject({Command{Operation::Enable},invalid[i]}, {},
        "bounded target/type/count/receiver case "+std::to_string(i));
    Require(cycleControls>=2u&&equalControls>=2u&&storedFields&&nativeEntries,
        "Actual SetBase controls did not exercise cycle/equal/write cohorts");
    std::cout<<"FIRST TRAINING SetBase: "<<cases<<" dormant actual native298 roots / "<<nativeEntries
        <<" native entries / "<<storedFields<<" declared Base/BYTE stores / "<<cycleControls<<" cycle and "
        <<equalControls<<" equal controls / "<<failedRoots<<" refused roots; independent live-field transfer model, "
        "real LevelInfo exception, unchanged collision order/tags/ColLocation/registration/RNG, exact write/work cuts "
        "and complete outer rollback. No authored Base relinks, forced begun world, FindBase, Destroy or gravity claim.\n";
}

// A genuine original helper installs the Doctor's full animation clock. Two
// explicit public publications (helper + final checkpoint restore) are scoped
// to this test; every SetBase diagnostic between them still rolls back.
void AnimatedActorBaseCoverage(Temporary& temporary) {
    using Value=QuestVr::Vm::Value;
    const std::string doctor="00_Training.Doctor1",crate="00_Training.CrateBreakableMedGeneral1";
    const std::string level="00_Training.LevelInfo0";
    const auto before=temporary.Save("BeforeAnimatedActorBase");
    const auto checkpoint=temporary.Write("ActorBaseAnimationReset",before);
    const auto waiting=ExecutePortableActorFunction(doctor,"PlayWaiting");
    Require(waiting.passed()&&waiting.committed&&waiting.instructions>0u,
        "SetBase clock control failed actual Doctor1.PlayWaiting: "+waiting.error);
    const auto seeded=temporary.Save("AnimatedActorBaseSeed");const Envelope envelope(seeded);
    const auto saved=std::find_if(envelope.state.objects.begin(),envelope.state.objects.end(),[&](const auto& row){return row.path==doctor;});
    Require(saved!=envelope.state.objects.end()&&saved->clock.has_value()&&!saved->clock->pose.main.sequence.empty(),
        "Actual SetBase animation control lacks its genuine native animation clock");
    QuestVr::ScriptSavedState expected;expected.mapName="00_Training";
    QuestVr::ScriptSavedObject object;object.path=doctor;object.classPath=saved->classPath;object.clock=saved->clock;
    expected.objects.push_back(object);const auto clock=QuestVr::EncodeScriptSavedState(expected);
    const auto command=[&](const std::string& target) {
        Command result{Operation::ActorSetBase};result.actorPath=doctor;
        result.arguments={Value::Text(Kind::Object,target)};return result;
    };
    const std::vector<Command> commands{Command{Operation::Enable},command(crate),command(crate),command(level),command({})};
    GC::Collect();const auto gc=GC::GetStats();const auto revision=GetPortableRuntimeWorldRevision();
    const auto lookup=ReadPortableRuntimeObjectLookup();const auto owner=ReadPortableRuntimeObjectOwnership(doctor);
    const auto native=ReadPortableRuntimeNativeModel();const auto actors=GetPortableRuntimeMapActors(true);
    const auto same=[&] {
        GC::Collect();SameGc(gc,"Animated SetBase probe leaked original runtime objects");
        const auto current=ReadPortableRuntimeObjectLookup();const auto identity=ReadPortableRuntimeObjectOwnership(doctor);
        const auto model=ReadPortableRuntimeNativeModel();const auto hash=ReadPortableRuntimeRetailCollision();
        Require(temporary.Save("AfterAnimatedActorBase")==seeded&&GetPortableRuntimeWorldRevision()==revision&&
            current.prepared==lookup.prepared&&current.generation==lookup.generation&&current.objects==lookup.objects&&
            current.retainedBytes==lookup.retainedBytes&&identity.objectIndex==owner.objectIndex&&
            identity.classIndex==owner.classIndex&&identity.outerIndex==owner.outerIndex&&
            model.mapGeneration==native.mapGeneration&&model.levelPath==native.levelPath&&model.modelPath==native.modelPath&&
            !hash.initialized&&!hash.enabled&&!hash.hashIdentity&&hash.actors.empty()&&
            hash.seed==*envelope.state.randomSeed&&!Declared(level,"Engine.LevelInfo.bBegunPlay",Kind::Bool).boolean,
            "Animated SetBase probe failed complete clock/save/revision/lookup/Level/hash/RNG rollback");
        const auto world=GetPortableRuntimeMapActors(true);Require(world.size()==actors.size(),"Animated SetBase changed actual Actor census");
        for(std::size_t i=0u;i<world.size();++i)Require(world[i].objectPath==actors[i].objectPath&&world[i].classPath==actors[i].classPath,
            "Animated SetBase changed actual first-Training Actor slot/class identity");
    };
    const auto clocks=[&](const PortableRetailCollisionProbeResult& result,std::size_t count) {
        Require(result.bases.size()==count,"Animated SetBase lost its successful in-root snapshot prefix");
        for(const auto& row:result.bases)Require(row.before[0u].actorPath==doctor&&row.after[0u].actorPath==doctor&&
            row.clockBefore==clock&&row.clockAfter==clock,
            "Native SetBase/count write reset, reconstructed or advanced the genuine full animation clock INSIDE its root");
    };
    const auto result=ProbePortableRuntimeRetailCollision(commands);
    Require(result.available&&result.execution.passed()&&!result.execution.committed&&result.execution.writes==5u,
        "Actual animated native298 sequence failed: "+result.execution.error);clocks(result,4u);same();
    // The animated Doctor is also a COUNT-store target, not only the source
    // of raw Base writes. An equal-None native call samples its live clock
    // inside the same root immediately after each other actor's count store.
    Command attach{Operation::ActorSetBase};attach.actorPath=crate;
    attach.arguments={Value::Text(Kind::Object,doctor)};
    auto detach=attach;detach.arguments={Value::Text(Kind::Object,{})};
    const auto counts=ProbePortableRuntimeRetailCollision({Command{Operation::Enable},attach,command({}),detach,command({})});
    Require(counts.available&&counts.execution.passed()&&!counts.execution.committed&&
        counts.execution.writes==4u&&counts.bases.size()==4u,
        "Animated support-count target control failed: "+counts.execution.error);
    for(const auto index:{1u,3u})Require(counts.bases[index].before[0u].actorPath==doctor&&
        counts.bases[index].clockBefore==clock&&counts.bases[index].clockAfter==clock,
        "StandingCount store reset or rebuilt the actual support actor's full clock INSIDE the same root");
    same();
    auto lateCommands=commands;lateCommands.push_back(Command{static_cast<Operation>(999)});
    const auto late=ProbePortableRuntimeRetailCollision(lateCommands);
    Require(!late.available&&!late.execution.passed()&&!late.execution.committed&&late.execution.writes==5u,
        "Animated SetBase late refusal missed its genuine field-write prefix");clocks(late,4u);++refusals;same();
    QuestVr::Vm::Limits partialWrites;partialWrites.writes=3u;
    const auto partial=ProbePortableRuntimeRetailCollision({command(crate),command({})},partialWrites);
    Require(!partial.available&&!partial.execution.passed()&&!partial.execution.committed&&
        partial.execution.status==QuestVr::Vm::Status::Budget&&partial.execution.writes==3u,
        "Animated SetBase did not refuse after genuine attach and partial detach stores");
    clocks(partial,1u);++refusals;same();
    Require(LoadPortableRuntimeState(checkpoint.string()),"Cannot restore before real animated native298 control");GC::Collect();
    Require(temporary.Save("RestoredAnimatedActorBase")==before,
        "Animated SetBase retained its PlayWaiting clock or Base/count fields after exact checkpoint restoration");
    std::cout<<"FIRST TRAINING animated SetBase: actual Doctor1.PlayWaiting, exact canonical full-clock equality INSIDE "
        "ordinary/equal/LevelInfo/None native298 calls and as a separate count-store target; successful, late and partial-write full rollback, original "
        "checkpoint restored. No invented pose, possession, begun flag, support or gravity.\n";
}
