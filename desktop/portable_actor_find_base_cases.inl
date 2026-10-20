// Actual first Training only. Cold-world support callbacks remain suppressed
// by original ProcessEvent gates. These are not begun-world or gravity tests.
void ActorFindBaseCoverage(const std::vector<Actor>& actors,const std::function<void()>& rollback) {
    using Value=QuestVr::Vm::Value;
    const std::string lower="00_Training.CrateBreakableMedGeneral1",upper="00_Training.CrateBreakableMedGeneral2";
    const auto find=[](const std::string& path) {Command command{Operation::ActorFindBase};command.actorPath=path;return command;};
    const auto base=[](const std::string& source,const std::string& target) {
        Command command{Operation::ActorSetBase};command.actorPath=source;
        command.arguments={Value::Text(Kind::Object,target)};return command;
    };
    const auto actor=[&](const std::string& path)->const Actor& {
        const auto found=std::find_if(actors.begin(),actors.end(),[&](const auto& row){return row.path==path;});
        Require(found!=actors.end(),"FindBase lacks its actual first Training actor: "+path);return *found;
    };
    std::size_t hits{},misses{},attempted{},roots{},guards{};
    std::optional<Command> positiveQuery;
    std::string positiveHit;
    const auto unchangedHash=[&](const PortableRetailCollisionSnapshot& before,const PortableRetailCollisionSnapshot& after) {
        Require(before.initialized&&after.initialized&&before.enabled&&after.enabled&&
            before.hashIdentity==after.hashIdentity&&before.mapGeneration==after.mapGeneration&&
            before.seed==after.seed&&before.initializationDraws==after.initializationDraws&&
            before.activeLinks==after.activeLinks&&before.allocatedLinks==after.allocatedLinks&&
            before.actors.size()==after.actors.size(),"FindBase changed hash ownership/registration/RNG");
        for(std::size_t i=0u;i<before.actors.size();++i) {
            const auto& a=before.actors[i];const auto& b=after.actors[i];
            Require(a.path==b.path,"FindBase changed native actor encounter identity");
            SameVector(a.location,b.location,"FindBase changed native collision Location");
            SameVector(a.colLocation,b.colLocation,"FindBase changed native ColLocation");
        }
    };
    for(const auto& path:{lower,upper,std::string("00_Training.Doctor1"),std::string("00_Training.LightSwitch0")}) {
        const auto& source=actor(path);
        Command query{Operation::SingleLineCheck};query.actorPath=path;query.flags=0x17u;
        query.start=Declared(path,"Engine.Actor.Location",Kind::Vector).vector;query.end=query.start;
        query.end[2u]=static_cast<float>(double(query.start[2u])-8.0);
        const auto radius=Declared(path,"Engine.Actor.CollisionRadius",Kind::Float).floating;
        query.extent={radius,radius,Declared(path,"Engine.Actor.CollisionHeight",Kind::Float).floating};
        SameVector(source.location,query.start,"FindBase fixture is not the authentic unposed location");
        const auto result=ProbePortableRuntimeRetailCollision({Command{Operation::Enable},query,find(path)});
        Require(result.available&&result.execution.passed()&&!result.execution.committed&&
            result.states.size()==3u&&result.singles.size()==1u&&result.findBases.size()==1u,
            "Actual FindBase service failed: "+result.execution.error);
        const auto& selected=result.singles.front();const auto& row=result.findBases.front();
        Require(row.commandIndex==2u&&row.actorPath==path&&row.hitActor==selected.hit.actorPath&&
            row.clear==selected.clear&&row.baseBefore.empty()&&row.baseAtDecision.empty()&&row.baseAfter.empty()&&
            row.marksBefore==row.marksAfterQuery&&row.arenasBefore==row.arenasAfterQuery&&
            !row.marksAfterQuery&&!row.arenasAfterQuery&&row.clockBefore==row.clockAfter,
            "FindBase lost its selected hit, cold-event suppression or mark/clock lifetime");
        SameVector(row.start,query.start,"FindBase did not read original Location");
        SameVector(row.end,query.end,"FindBase downward Float store differs");
        SameVector(row.extent,query.extent,"FindBase did not read actual cylinder extent");
        Require(row.unchanged==selected.hit.actorPath.empty()&&!row.basedOnSelf&&
            row.supportAttempted==!selected.hit.actorPath.empty()&&
            row.selectedSupportFunction.empty()==selected.hit.actorPath.empty()&&result.execution.writes==0u,
            "Cold FindBase bypassed ProcessEvent or inferred an actual attachment");
        unchangedHash(result.states[1u],result.states[2u]);
        if(selected.hit.actorPath.empty())++misses;else {
            ++hits;++attempted;
            Require(selected.hit.actorPath!=path,"FindBase independent query did not exclude its source");
            static_cast<void>(actor(selected.hit.actorPath));
            if(!positiveQuery){positiveQuery=query;positiveHit=selected.hit.actorPath;}
        }
        ++roots;rollback();
    }
    Require(hits&&misses&&attempted&&positiveQuery.has_value(),"Actual unposed FindBase checks lack positive and clear witnesses");
    const auto reject=[&](std::vector<Command> commands,const QuestVr::Vm::Limits& limits={}) {
        const auto result=ProbePortableRuntimeRetailCollision(commands,limits);
        Require(!result.available&&!result.execution.passed()&&!result.execution.committed&&!result.execution.error.empty(),
            "Invalid/partial FindBase root succeeded");++refusals;rollback();return result;
    };
    // Select the support identity independently with the original SingleLine
    // diagnostic, not FindBase's own reply. SetBase changes only direct
    // relationship fields: both controls retain the real unposed map geometry.
    // Even a world LevelInfo hit is an actual current-map Actor here, not an
    // invented floor, forced begun flag or publishing startup shortcut.
    for(const bool basedOnSelf:{false,true}) {
        const auto& source=positiveQuery->actorPath;
        const auto& receiver=basedOnSelf?positiveHit:source;
        const auto& target=basedOnSelf?source:positiveHit;
        Require(Declared(receiver,"Engine.Actor.Base",Kind::Object).text.empty()&&
            Declared(target,"Engine.Actor.Base",Kind::Object).text.empty(),
            "FindBase guard fixture no longer starts from actual empty Base fields");
        const auto receiverCount=Declared(receiver,"Engine.Actor.StandingCount",Kind::Byte).integer;
        const auto targetCount=Declared(target,"Engine.Actor.StandingCount",Kind::Byte).integer;
        Require(receiverCount>=0&&receiverCount<=255&&targetCount>=0&&targetCount<=255,
            "FindBase guard fixture lost its actual StandingCount BYTE storage");
        const bool ordinaryTarget=target!=Declared(receiver,"Engine.Actor.Level",Kind::Object).text;
        const auto writes=std::size_t{1u}+(ordinaryTarget?1u:0u);
        const auto expectedTargetCount=static_cast<std::uint8_t>(static_cast<unsigned>(targetCount)+(ordinaryTarget?1u:0u));
        const std::vector<Command> commands{Command{Operation::Enable},*positiveQuery,
            base(receiver,target),find(source)};
        const auto checkGuard=[&](const PortableRetailCollisionProbeResult& result) {
            Require(result.available&&result.execution.passed()&&!result.execution.committed&&
                result.states.size()==4u&&result.singles.size()==1u&&result.bases.size()==1u&&
                result.findBases.size()==1u&&result.execution.instructions==1u&&result.execution.writes==writes&&
                result.execution.nativeWork&&result.execution.nativeWork->used>1u&&!result.execution.nativeWork->exhausted,
                "Actual FindBase relationship guard failed: "+result.execution.error);
            const auto& selection=result.singles.front();const auto& attachment=result.bases.front();
            const auto& guarded=result.findBases.front();
            Require(!selection.clear&&selection.hit.actorPath==positiveHit&&attachment.commandIndex==2u&&
                attachment.before[0u].actorPath==receiver&&attachment.before[0u].basePath.empty()&&
                attachment.after[0u].basePath==target&&
                attachment.before[0u].standingCount==static_cast<std::uint8_t>(receiverCount)&&
                attachment.after[0u].standingCount==static_cast<std::uint8_t>(receiverCount)&&
                attachment.before[2u].actorPath==target&&attachment.before[2u].basePath.empty()&&
                attachment.after[2u].basePath.empty()&&
                attachment.before[2u].standingCount==static_cast<std::uint8_t>(targetCount)&&
                attachment.after[2u].standingCount==expectedTargetCount&&
                attachment.clockBefore==attachment.clockAfter,
                "FindBase guard lost its independent hit or reversible original Base/BYTE stores");
            const auto expectedBase=basedOnSelf?std::string{}:positiveHit;
            Require(guarded.commandIndex==3u&&guarded.actorPath==source&&guarded.hitActor==positiveHit&&
                !guarded.clear&&guarded.baseBefore==expectedBase&&guarded.baseAtDecision==expectedBase&&
                guarded.baseAfter==expectedBase&&guarded.unchanged==!basedOnSelf&&guarded.basedOnSelf==basedOnSelf&&
                !guarded.supportAttempted&&guarded.selectedSupportFunction.empty()&&
                guarded.clockBefore==guarded.clockAfter&&!guarded.marksBefore&&!guarded.marksAfterQuery&&
                !guarded.arenasBefore&&!guarded.arenasAfterQuery,
                "FindBase did not preserve its exact same-hit / hit-based-on-self early return");
            SameVector(guarded.start,positiveQuery->start,"FindBase guard changed original start");
            SameVector(guarded.end,positiveQuery->end,"FindBase guard changed original end");
            SameVector(guarded.extent,positiveQuery->extent,"FindBase guard changed original cylinder extent");
            unchangedHash(result.states[1u],result.states[2u]);
            unchangedHash(result.states[2u],result.states[3u]);
            ++roots;++guards;rollback();
        };
        const auto complete=ProbePortableRuntimeRetailCollision(commands);checkGuard(complete);
        QuestVr::Vm::Limits exact;exact.nativeWork=complete.execution.nativeWork->used;
        const auto enough=ProbePortableRuntimeRetailCollision(commands,exact);checkGuard(enough);
        Require(enough.execution.nativeWork&&enough.execution.nativeWork->used==exact.nativeWork,
            "FindBase guard exact N native-work allowance changed its completed root");
        QuestVr::Vm::Limits shortWork;shortWork.nativeWork=exact.nativeWork-1u;
        const auto cut=reject(commands,shortWork);
        Require(cut.execution.status==QuestVr::Vm::Status::Budget&&cut.execution.nativeWork&&
            cut.execution.nativeWork->exhausted&&cut.execution.nativeWork->used<=shortWork.nativeWork&&
            cut.states.size()==3u&&cut.singles.size()==1u&&cut.bases.size()==1u&&cut.findBases.empty()&&
            cut.execution.writes==writes,
            "FindBase guard N-1 work refusal lost its actual completed attachment prefix");
        auto late=commands;late.push_back(Command{static_cast<Operation>(999)});
        const auto refused=reject(late);
        Require(refused.states.size()==4u&&refused.findBases.size()==1u&&refused.bases.size()==1u&&
            refused.execution.writes==writes&&refused.execution.nativeWork&&
            refused.execution.nativeWork->used==exact.nativeWork&&
            refused.findBases.front().unchanged==!basedOnSelf&&refused.findBases.front().basedOnSelf==basedOnSelf,
            "Late FindBase guard refusal lost its genuine completed decision prefix");
    }
    // The floating original upper crate's eight-unit sweep is clear. An
    // explicit actual SetBase call supplies a reversible direct relationship;
    // FindBase's None-hit branch must then detach it, even in a dormant world.
    const auto detached=ProbePortableRuntimeRetailCollision({Command{Operation::Enable},base(upper,lower),find(upper)});
    Require(detached.available&&detached.execution.passed()&&!detached.execution.committed&&
        detached.bases.size()==1u&&detached.findBases.size()==1u&&detached.execution.writes==4u,
        "FindBase clear-support detach failed: "+detached.execution.error);
    const auto& row=detached.findBases.front();
    Require(row.hitActor.empty()&&row.baseBefore==lower&&row.baseAtDecision==lower&&row.baseAfter.empty()&&
        row.supportAttempted&&!row.unchanged&&!row.basedOnSelf&&row.selectedSupportFunction.empty(),
        "FindBase did not perform its original None-hit SetBase path");
    unchangedHash(detached.states[1u],detached.states[2u]);++roots;rollback();
    auto late=std::vector<Command>{Command{Operation::Enable},base(upper,lower),find(upper),Command{static_cast<Operation>(999)}};
    const auto refused=reject(late);
    Require(refused.findBases.size()==1u&&refused.findBases.front().baseAfter.empty()&&refused.execution.writes==4u,
        "Late FindBase refusal missed its genuine completed detach prefix");
    QuestVr::Vm::Limits partial;partial.writes=3u;
    const auto cut=reject({Command{Operation::Enable},base(upper,lower),find(upper)},partial);
    Require(cut.execution.status==QuestVr::Vm::Status::Budget&&cut.bases.size()==1u&&cut.findBases.empty()&&
        cut.execution.writes==3u,"FindBase partial detach did not share VM write/undo authority");
    auto bad=find(upper);bad.arguments={Value::Integer(1)};reject({Command{Operation::Enable},bad});
    bad=find("Engine.Actor");reject({Command{Operation::Enable},bad});
    bad=find("00_Training.NoSuchActor");reject({Command{Operation::Enable},bad});
    const auto complete=ProbePortableRuntimeRetailCollision({Command{Operation::Enable},find(upper)});
    Require(complete.available&&complete.execution.passed()&&complete.execution.nativeWork&&
        complete.execution.nativeWork->used>1u,"FindBase lacks actual shared work accounting");rollback();
    QuestVr::Vm::Limits work;work.nativeWork=complete.execution.nativeWork->used-1u;
    const auto exhausted=reject({Command{Operation::Enable},find(upper)},work);
    Require(exhausted.execution.status==QuestVr::Vm::Status::Budget&&exhausted.execution.nativeWork&&
        exhausted.execution.nativeWork->exhausted,"FindBase query got a fresh root work allowance");
    std::cout<<"FIRST TRAINING FindBase: "<<roots<<" actual unposed dormant roots, "<<hits<<" hits / "<<misses
        <<" clear witnesses / "<<guards<<" same-hit or hit-based-on-self guard roots; independent prior SingleLine selections, original geometry, suppressed selected SupportActor, "
        "None-hit real SetBase detach, popped marks, exact late/partial/shared-work rollback. Not begun-world callback execution, falling or crates.\n";
}
