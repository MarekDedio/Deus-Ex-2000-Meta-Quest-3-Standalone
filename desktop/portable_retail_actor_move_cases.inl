// First 00_Training only. Real current-Level/native266/299 adapters, not a
// mocked Move provider or a gravity tick. Cap/backoff expectations below use
// source data and hardware x87, never production movement helpers. Zone STORE
// counts are covered here; region VALUES have separate original-zone tests.
void ActorMoveCoverage(const PortablePackageTables& map,const std::vector<Actor>& actors,
    const std::function<void()>& rollback) {
    using Rotation=std::array<std::int32_t,3u>;
    const auto find=[&](const char* path)->const Actor& {
        const auto it=std::find_if(actors.begin(),actors.end(),[&](const auto& a){return a.path==path;});
        Require(it!=actors.end(),std::string("Move fixture lacks actual source Actor: ")+path);return *it;
    };
    const auto& lower=find("00_Training.CrateBreakableMedGeneral1");
    const auto& upper=find("00_Training.CrateBreakableMedGeneral2");
    const auto rotation=Declared(upper.path,"Engine.Actor.Rotation",Kind::Rotator).rotation;
    const float radius=Declared(upper.path,"Engine.Actor.CollisionRadius",Kind::Float).floating;
    const float height=Declared(upper.path,"Engine.Actor.CollisionHeight",Kind::Float).floating;
    const float lowerRadius=Declared(lower.path,"Engine.Actor.CollisionRadius",Kind::Float).floating;
    const float lowerHeight=Declared(lower.path,"Engine.Actor.CollisionHeight",Kind::Float).floating;
    Require(upper.collide&&lower.collide&&!upper.deleted&&!lower.deleted&&
        !Declared(upper.path,"Engine.Actor.bStatic",Kind::Bool).boolean&&
        Declared(upper.path,"Engine.Actor.bMovable",Kind::Bool).boolean&&
        Declared(upper.path,"Engine.Actor.StandingCount",Kind::Byte).integer==0&&
        Declared(upper.path,"Engine.Actor.Brush",Kind::Object).text.empty()&&
        ReadPortableRuntimeActorPrimitive(upper.path).nativeClassPath=="Engine.LodMesh"&&
        ReadPortableRuntimeActorPrimitive(lower.path).nativeClassPath=="Engine.LodMesh"&&
        radius==34.0f&&lowerRadius==34.0f&&height==24.0f&&lowerHeight==24.0f,
        "Move fixture lost authentic movable, unbased-child-free Training cylinder crate pair");
    Require(Declared(upper.path,"Engine.Actor.bBlockActors",Kind::Bool).boolean&&
        Declared(lower.path,"Engine.Actor.bBlockActors",Kind::Bool).boolean&&
        Declared(upper.path,"Engine.Actor.bCollideWorld",Kind::Bool).boolean,
        "Move fixture cannot independently select the ordinary crate blocking branch");
    const auto command=[&](Operation operation,Array delta,std::int32_t test=0) {
        Command c;c.operation=operation;c.actorPath=upper.path;c.end=delta;c.rotation=rotation;c.moveOptions[0u]=test;return c;
    };
    const auto upperIndex=static_cast<std::size_t>(&upper-actors.data());
    const auto row=[&](const PortableRetailCollisionSnapshot& s,const std::string& path)->const PortableRetailCollisionActor& {
        const auto i=std::find_if(s.actors.begin(),s.actors.end(),[&](const auto& a){return a.path==path;});
        Require(i!=s.actors.end(),"Move snapshot lost actual hash actor: "+path);return *i;
    };
    std::size_t cases{},numericCases{},failedRoots{};
    const auto run=[&](const std::vector<Command>& input,const std::string& why) {
        auto out=ProbePortableRuntimeRetailCollision(input);
        Require(out.available&&out.execution.passed()&&!out.execution.committed&&out.states.size()==input.size(),
            "Actual native movement failed: "+why+": "+out.execution.error);
        for(const auto& m:out.moves) {
            Require(m.marksAfter==0u&&m.arenasAfter==0u,"Successful native Move retained a mark/arena: "+why);
            Require(m.clockBefore==m.clockAfter,"Native pose/zone update reconstructed its original animation clock: "+why);
            std::cout<<std::setprecision(9)<<"MOVE SOURCE "<<why<<" actor="<<m.actorPath<<" returned="<<m.returned
                <<" after=("<<m.locationAfter[0u]<<','<<m.locationAfter[1u]<<','<<m.locationAfter[2u]
                <<") hit="<<m.hit.actorPath<<" time="<<m.hit.time<<" writes="<<out.execution.writes<<'\n';
        }
        ++cases;rollback();return out;
    };
    const auto seedHit=[&](const PortableRetailMoveSnapshot& m) {
        Require(m.hitAvailable&&!m.hitNextPresent&&m.hit.actorPath.empty()&&m.hit.primitivePath.empty()&&m.hit.item==127,
            "Early native Move changed inactive caller seed identities/Item/Next");
        SameFloat(m.hit.time,.375f,"Early native Move overwrote caller seed Time");
        SameVector(m.hit.location,{11,12,13},"Early native Move overwrote caller seed Location");
        SameVector(m.hit.normal,{-1,0,0},"Early native Move overwrote caller seed Normal");
    };
    const auto pose=[&](const PortableRetailMoveSnapshot& m,const Array& after,const Rotation& angles,bool added) {
        Require(m.actorPath==upper.path&&m.rotationBefore==rotation&&m.rotationAfter==angles,
            "Actual native Move lost requested/current Rotation authority");
        SameVector(m.locationBefore,upper.location,"Actual native Move did not start at source Location");
        SameVector(m.locationAfter,after,"Actual native Move did not publish independently expected Location");
        SameVector(m.colLocationAfter,added?after:m.colLocationBefore,"Move copied ColLocation outside original Add, or skipped Add copy");
    };
    const auto hashMoved=[&](const PortableRetailCollisionProbeResult& out,const Array& after,std::size_t last) {
        const auto& before=out.states.front();const auto& state=out.states.at(last);
        Require(before.enabled&&state.enabled&&before.hashIdentity==state.hashIdentity&&before.seed==state.seed&&
            before.initializationDraws==state.initializationDraws&&before.mapGeneration==state.mapGeneration,
            "Move replaced original hash authority or redrew CRT tables");
        Oracle expected(actors);expected.Remove(upperIndex);auto changed=upper;changed.location=after;
        expected.ranges[upperIndex]=CylinderRange(changed,radius,height);expected.Add(upperIndex);
        Require(state.activeLinks==expected.links,"Move failed authentic Remove/new-bounds/Add registration count");
        for(const auto& original:before.actors) {
            const auto& actual=row(state,original.path);
            Require(actual.declaredMirrorsMatch&&actual.collideActors==original.collideActors&&
                actual.pendingDelete==original.pendingDelete&&actual.objectIndex==original.objectIndex,
                "Move lost declared tag/ColLocation or actor identity/flags");
            SameVector(actual.location,original.path==upper.path?after:original.location,
                "Move changed another hash actor Location or failed source re-registration");
            SameVector(actual.colLocation,original.path==upper.path?after:original.colLocation,
                "Move changed another actor ColLocation or failed source Add copy");
        }
    };

    // Eligibility precedes all active Delta arithmetic. The real static source
    // is not made movable simply to get a success, and its NaN stays inactive.
    Command fixed;fixed.operation=Operation::LevelMoveActor;fixed.actorPath=actors.front().path;
    fixed.end={std::bit_cast<float>(std::uint32_t{0x7fc12345u}),0,0};fixed.rotation={17,19,23};
    Require(Declared(fixed.actorPath,"Engine.Actor.bStatic",Kind::Bool).boolean,
        "Move eligibility control is not the actual static LevelInfo");
    const auto rejected=run({fixed},"static ineligible / inactive NaN Delta");
    Require(rejected.moves.size()==1u&&!rejected.moves[0u].returned&&rejected.execution.writes==0u&&
        !rejected.states[0u].initialized,"Static ineligible Move reached pose/hash publication");
    const auto& staticMove=rejected.moves[0u];seedHit(staticMove);
    SameVector(staticMove.locationBefore,staticMove.locationAfter,"Static ineligible Move changed Location");
    SameVector(staticMove.colLocationBefore,staticMove.colLocationAfter,"Static ineligible Move changed ColLocation");
    Require(staticMove.rotationBefore==staticMove.rotationAfter,"Static ineligible Move changed Rotation");

    auto requested=rotation;requested[1u]=std::bit_cast<std::int32_t>(std::bit_cast<std::uint32_t>(rotation[1u])+4096u);
    auto turn=command(Operation::LevelMoveActor,{});turn.rotation=requested;
    const auto turned=run({Command{Operation::Enable},turn},"rotation-only untouched caller Hit");
    Require(turned.moves.size()==1u&&turned.moves[0u].returned&&turned.execution.writes==3u,
        "Rotation-only Move did not use exactly three original Rotation stores");
    seedHit(turned.moves[0u]);pose(turned.moves[0u],upper.location,requested,false);
    Require(turned.states[0u].tag==turned.states[1u].tag&&turned.states[0u].activeLinks==turned.states[1u].activeLinks&&
        turned.states[0u].hashIdentity==turned.states[1u].hashIdentity&&turned.states[0u].seed==turned.states[1u].seed,
        "Rotation-only fast path queried/re-registered original hash");
    turn.operation=Operation::ActorSetRotation;
    const auto wrapped=run({Command{Operation::Enable},turn},"actual native299 rotation-only wrapper");
    Require(wrapped.moves.size()==1u&&wrapped.moves[0u].returned&&!wrapped.moves[0u].hitAvailable&&wrapped.execution.writes==3u,
        "Original native299 wrapper returned wrong Bool or bypassed scalar Rotation journal");
    pose(wrapped.moves[0u],upper.location,requested,false);

#if (defined(__i386__) || defined(__x86_64__)) && defined(__GNUC__)
    struct Precision {
        unsigned short previous{};
        Precision(){static_assert(std::numeric_limits<long double>::digits==64);asm volatile("fnstcw %0":"=m"(previous));
            const auto mode=static_cast<unsigned short>((previous&~0x0f00u)|0x033fu);asm volatile("fldcw %0"::"m"(mode));}
        ~Precision(){asm volatile("fldcw %0"::"m"(previous));}
    } precision;
    // Volatile long-double operands are the host x87 PC64 reference. Each
    // explicit Float store is independent of all production arithmetic code.
    using Real=long double;
    const auto add=[](Real a,Real b){volatile Real x=a,y=b,z=x+y;return z;};
    const auto sub=[](Real a,Real b){volatile Real x=a,y=b,z=x-y;return z;};
    const auto mul=[](Real a,Real b){volatile Real x=a,y=b,z=x*y;return z;};
    const auto divide=[](Real a,Real b){volatile Real x=a,y=b,z=x/y;return z;};
    const auto store=[](Real a){volatile float b=static_cast<float>(a);Require(std::isfinite(b),"Independent Move fixture Float overflow");return b;};
    Array above=upper.location;above[2u]=store(add(upper.location[2u],8.0L));
    const auto clearHit=[&](const PortableRetailMoveSnapshot& m) {
        Require(m.returned&&m.hitAvailable&&!m.hitNextPresent&&m.hit.actorPath.empty()&&m.hit.primitivePath.empty()&&m.hit.item==-1,
            "Unobstructed full Move did not return its initialized clear packet");
        SameFloat(m.hit.time,1.0f,"Unobstructed full Move changed clear Time");
        SameVector(m.hit.location,{},"Unobstructed full Move changed clear Location");
        SameVector(m.hit.normal,{},"Unobstructed full Move changed clear Normal");
    };
    const auto up=command(Operation::LevelMoveActor,{0,0,8},-1);
    const auto clear=run({Command{Operation::Enable},up},"nonzero upper crate upward bTest=-1");
    Require(clear.moves.size()==1u&&clear.execution.writes==9u,
        "bTest was treated as preview, or full Move skipped its six pose/three region stores");
    clearHit(clear.moves[0u]);pose(clear.moves[0u],above,rotation,true);hashMoved(clear,above,1u);++numericCases;
    const auto noHash=run({up},"nonzero upper crate upward without original hash");
    Require(noHash.moves.size()==1u&&noHash.execution.writes==9u&&!noHash.states[0u].initialized&&
        !noHash.states[0u].enabled&&noHash.states[0u].seed==InitialSeed,
        "World-only Move fabricated an actor hash or drew CRT random values");
    clearHit(noHash.moves[0u]);pose(noHash.moves[0u],above,rotation,false);++numericCases;
    for(const auto operation:{Operation::ActorMove,Operation::ActorMoveSmooth}) {
        const auto actual=run({Command{Operation::Enable},command(operation,{0,0,8})},
            operation==Operation::ActorMove?"actual native266 full nonzero Move":"actual MoveSmooth first clear result");
        Require(actual.moves.size()==1u&&actual.moves[0u].returned&&!actual.moves[0u].hitAvailable&&actual.execution.writes==9u,
            "Original Move/MoveSmooth clear wrapper bypassed complete native movement");
        pose(actual.moves[0u],above,rotation,true);hashMoved(actual,above,1u);++numericCases;
    }

    // The source gap is39 units: the expanded lower cylinder's upper cap is
    // crossed by this66-unit extended downward query. Small XY source offsets
    // are strictly inside the radial cylinder; no quadratic root is involved.
    const float expandedHeight=store(add(height,lowerHeight));
    const float top=store(add(lower.location[2u],expandedHeight));
    const float endZ=store(add(upper.location[2u],-66.0L));
    const auto px=sub(upper.location[0u],lower.location[0u]),py=sub(upper.location[1u],lower.location[1u]);
    Require(upper.location[2u]>top&&endZ<top&&sub(upper.location[2u],top)==39.0L&&
        add(mul(px,px),mul(py,py))<mul(add(radius,lowerRadius),add(radius,lowerRadius)),
        "Source crate pair cannot independently exercise the upper-cap-only blocked sweep");
    const float entry=store(divide(sub(top,upper.location[2u]),sub(endZ,upper.location[2u])));
    const double cylinderBackoff=0.001;
    const Real cylinderTime=sub(entry,static_cast<Real>(cylinderBackoff));
    const float packetTime=store(cylinderTime);
    Array contact=upper.location;contact[2u]=store(add(mul(store(sub(endZ,upper.location[2u])),cylinderTime),upper.location[2u]));
    const float storedDistance=store(mul(66.0L,packetTime));
    const float movedDelta=store(sub(store(mul(-66.0L,packetTime)),-2.0L));
    const float correctedTime=store(divide(sub(storedDistance,2.0L),64.0L));
    Array resting=upper.location;resting[2u]=store(add(upper.location[2u],movedDelta));
    Require(movedDelta<0.0f&&movedDelta>-64.0f&&resting[2u]>top&&correctedTime>0.0f&&correctedTime<1.0f,
        "Independent blocked Move witness does not actually stop short of contact");
    auto down=command(Operation::LevelMoveActor,{0,0,-64},-1);
    const auto blockedHit=[&](const PortableRetailMoveSnapshot& m) {
        Require(m.returned&&m.hitAvailable&&m.hit.actorPath==lower.path&&m.hit.primitivePath.empty()&&m.hit.item==-1,
            "Real crate sweep did not select the source lower cylinder cap");
        SameFloat(m.hit.time,correctedTime,"Real crate Move lost original corrected Float Time");
        SameVector(m.hit.location,contact,"Real crate Move recomputed raw cylinder contact after two-unit backoff");
        SameVector(m.hit.normal,{0,0,1},"Real crate cap sweep returned wrong world-space Normal");
        pose(m,resting,rotation,true);
    };
    for(const std::int32_t test:{-1,0}) {
        down.moveOptions[0u]=test;
        const auto blocked=run({Command{Operation::Enable},down},test?"real upper/lower cap sweep bTest":"real upper/lower cap sweep ordinary callbacks");
        Require(blocked.moves.size()==1u&&blocked.execution.writes==9u,
            "Real crate blocked movement skipped full pose/zone stores or ran dormant callback bodies");
        blockedHit(blocked.moves[0u]);hashMoved(blocked,resting,1u);++numericCases;
    }

    // ColLocation follows Add ONLY. Disabling this actual actor does not
    // change its block/world flags; the upward move still runs world collision.
    Command off;off.operation=Operation::SetCollision;off.actorPath=upper.path;off.arguments={QuestVr::Vm::Value::Bool(false)};
    auto on=off;on.arguments={QuestVr::Vm::Value::Bool(true)};
    const auto readd=run({Command{Operation::Enable},off,up,on},"ColLocation stays old until original re-Add");
    Require(readd.moves.size()==1u&&readd.mutations.size()==2u,"ColLocation witness lost real SetCollision/Move entries");
    clearHit(readd.moves[0u]);pose(readd.moves[0u],above,rotation,false);
    SameVector(readd.moves[0u].colLocationBefore,upper.location,"Initial Enable failed actual source Add copy");
    SameVector(row(readd.states[2u],upper.path).colLocation,upper.location,"Noncolliding Move wrote ColLocation without Add");
    hashMoved(readd,above,3u);++numericCases;

    const auto reject=[&](const std::vector<Command>& input,const QuestVr::Vm::Limits& limits,const std::string& why) {
        auto out=ProbePortableRuntimeRetailCollision(input,limits);
        Require(!out.available&&!out.execution.passed()&&!out.execution.committed&&!out.execution.error.empty(),
            "Native movement failure became success: "+why);++refusals;++failedRoots;rollback();return out;
    };
    down.moveOptions[0u]=-1;
    const auto late=reject({Command{Operation::Enable},down,Command{static_cast<Operation>(999)}},{},"bad command after completed blocked Move");
    Require(late.moves.size()==1u&&late.states.size()==2u&&late.execution.writes==9u,
        "Late failure did not occur AFTER genuine nonzero pose/hash/zone movement");
    blockedHit(late.moves[0u]);
    auto poisoned=up;poisoned.end[0u]=std::bit_cast<float>(std::uint32_t{0x7fc12345u});
    const auto unsafe=reject({Command{Operation::Enable},up,poisoned},{},"active NaN after completed nonzero Move");
    Require(unsafe.moves.size()==1u&&unsafe.execution.writes==9u,"Active numeric refusal lost prior shared-root Move prefix");
    QuestVr::Vm::Limits limited;limited.writes=3u;
    const auto partial=reject({Command{Operation::Enable},up},limited,"budget after source XYZ, before Rotation X");
    Require(partial.execution.status==QuestVr::Vm::Status::Budget&&partial.execution.writes==3u&&partial.moves.empty()&&partial.states.size()==1u,
        "Write-limit witness failed before real hash Remove/XYZ mutation or escaped the shared root");
    std::cout<<std::setprecision(9)<<"FIRST TRAINING native Move: "<<cases<<" actual roots, "<<numericCases
        <<" independent nonzero cohorts, "<<failedRoots<<" late/partial refusals; cap source gap=39 extended=-66 correctedTime="
        <<correctedTime<<" movedZ="<<resting[2u]<<"; real266/299/Smooth, bTest mutation, Add-only ColLocation and empty marks. "
        "Rollback diagnostics only: dormant callbacks remain suppressed; NOT live gravity/destruction or published startup.\n";
#else
    (void)map;(void)radius;(void)height;(void)lowerRadius;(void)lowerHeight;(void)numericCases;(void)failedRoots;(void)hashMoved;
    std::cout<<"SKIP independent native Move cap/backoff hardware-x87 cohorts on non-x86 compiler\n";
#endif
    (void)map;
}

// A REAL original PlayWaiting clock, never a fabricated idle clock/role/begun
// flag. bNoFail keeps this clock-only pose witness independent of path blocking;
// it does NOT bypass the real query, stores, Add or full Pawn zone operation.
void AnimatedActorMoveRollback(Temporary& temporary) {
    const auto before=temporary.Save("BeforeAnimatedMove");const auto checkpoint=temporary.Write("MoveAnimationReset",before);
    const std::string doctor="00_Training.Doctor1";
    const auto waiting=ExecutePortableActorFunction(doctor,"PlayWaiting");
    Require(waiting.passed()&&waiting.committed&&waiting.instructions>0u,
        "Move clock positive control failed real Doctor1.PlayWaiting: "+waiting.error);
    const auto seeded=temporary.Save("AnimatedMoveSeed");Envelope envelope(seeded);
    const auto saved=std::find_if(envelope.state.objects.begin(),envelope.state.objects.end(),[&](const auto& o){return o.path==doctor;});
    Require(saved!=envelope.state.objects.end()&&saved->clock.has_value(),"Real Move clock control lacks native animation state");
    QuestVr::ScriptSavedState expected;expected.mapName="00_Training";
    QuestVr::ScriptSavedObject object;object.path=doctor;object.classPath=saved->classPath;object.clock=saved->clock;expected.objects.push_back(object);
    const auto original=QuestVr::EncodeScriptSavedState(expected);
    const auto location=Declared(doctor,"Engine.Actor.Location",Kind::Vector).vector;
    const auto rotation=Declared(doctor,"Engine.Actor.Rotation",Kind::Rotator).rotation;
    auto moved=location;moved[2u]=static_cast<float>(static_cast<double>(location[2u])+1.0);
    GC::Collect();const auto gc=GC::GetStats();const auto revision=GetPortableRuntimeWorldRevision();
    const auto lookup=ReadPortableRuntimeObjectLookup();
    const auto same=[&] {
        GC::Collect();SameGc(gc,"Animated Move root leaked actual objects");const auto current=ReadPortableRuntimeObjectLookup();
        Require(temporary.Save("AfterAnimatedMove")==seeded&&GetPortableRuntimeWorldRevision()==revision&&
            current.generation==lookup.generation&&current.objects==lookup.objects&&!ReadPortableRuntimeRetailCollision().initialized&&
            !Declared("00_Training.LevelInfo0","Engine.LevelInfo.bBegunPlay",Kind::Bool).boolean,
            "Animated Move changed full clock/save/revision/lookup/GC/hash state after rollback");
    };
    Command c;c.operation=Operation::LevelMoveActor;c.actorPath=doctor;c.end={0,0,1};c.rotation=rotation;c.moveOptions={-1,0,0,1};
    const auto result=ProbePortableRuntimeRetailCollision({Command{Operation::Enable},c});
    Require(result.available&&result.execution.passed()&&!result.execution.committed&&result.moves.size()==1u&&result.execution.writes>=15u,
        "Real animated Pawn Move did not execute six pose plus nine region stores: "+result.execution.error);
    const auto& actual=result.moves[0u];
    SameVector(actual.locationBefore,location,"Animated Move lost actual initial Pawn Location");
    SameVector(actual.locationAfter,moved,"Animated bNoFail Move failed requested nonzero Pawn displacement");
    SameVector(actual.colLocationAfter,moved,"Animated Move failed original hash re-Add ColLocation copy");
    Require(actual.rotationAfter==rotation&&actual.clockBefore==original&&actual.clockAfter==original&&
        actual.marksAfter==0u&&actual.arenasAfter==0u,
        "Real full Move reinitialized native animation clock or retained hit marks/arenas");same();
    const auto late=ProbePortableRuntimeRetailCollision({Command{Operation::Enable},c,Command{static_cast<Operation>(999)}});
    Require(!late.available&&!late.execution.passed()&&!late.execution.committed&&late.moves.size()==1u&&late.execution.writes>=15u&&
        late.moves[0u].clockBefore==original&&late.moves[0u].clockAfter==original,
        "Animated Move late refusal did not follow genuine full Pawn movement/clock preservation");++refusals;same();
    Require(LoadPortableRuntimeState(checkpoint.string()),"Cannot restore original dormant checkpoint after Move clock control");GC::Collect();
    Require(temporary.Save("RestoredAnimatedMove")==before,"Animated Move fixture retained its real PlayWaiting clock");
    std::cout<<"FIRST TRAINING animated Move: actual Doctor1.PlayWaiting full-clock bytes unchanged INSIDE nonzero native Move, "
        "plus fifteen-or-more pose/zone stores and late full-root rollback; no fake clock, role or begun-world override.\n";
}
