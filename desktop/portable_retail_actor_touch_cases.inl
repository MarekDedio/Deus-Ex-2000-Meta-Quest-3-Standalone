// Actual first-Training, cold-world integration only. The slot interpreter
// below uses declared source fields/classes, not the production touch helpers.
// Original event bodies remain suppressed by authentic bBegunPlay=false.
void ActorTouchCases(const std::vector<Actor>& actors,const std::function<void()>& rollback) {
    using Slots=std::array<std::string,4u>;
    struct Row {
        std::string path,cls,level;Slots slots;
        std::uint8_t role{};bool pawn{},player{},brush{};
        Array location{},colLocation{};std::array<std::int32_t,3u> rotation{};
        float radius{},height{};
    };
    const auto none=[](std::string text) {if(Fold(text)=="none")text.clear();return text;};
    const auto snapshots=GetPortableRuntimeMapActors(true);
    Require(snapshots.size()==actors.size(),"Touch fixture lost actual first-Training actor census");
    std::vector<std::pair<std::string,std::vector<std::string>>> ancestry;
    const auto derives=[&](const std::string& path,const char* wanted) {
        auto found=std::find_if(ancestry.begin(),ancestry.end(),[&](const auto& entry){return entry.first==Fold(path);});
        if(found==ancestry.end()) {
            std::vector<std::string> chain;auto current=path;
            while(!current.empty()) {
                const auto folded=Fold(current);
                Require(chain.size()<128u&&std::find(chain.begin(),chain.end(),folded)==chain.end(),
                    "Touch fixture native class ancestry is cyclic or excessive");
                chain.push_back(folded);const auto cls=ReadPortableRuntimeDispatchClass(current);
                Require(Fold(cls.path)==folded,"Touch fixture class identity changed");current=cls.parentPath;
            }
            ancestry.push_back({Fold(path),std::move(chain)});found=ancestry.end()-1;
        }
        return std::find(found->second.begin(),found->second.end(),Fold(wanted))!=found->second.end();
    };
    std::vector<Row> rows;rows.reserve(actors.size());
    std::vector<std::size_t> empty,crates,pawns,players,brushes,roleZero;
    for(const auto& actor:actors) {
        const auto snapshot=std::find_if(snapshots.begin(),snapshots.end(),[&](const auto& item){return item.objectPath==actor.path;});
        Require(snapshot!=snapshots.end(),"Touch fixture actor has no actual class snapshot");
        Row row;row.path=actor.path;row.cls=snapshot->classPath;
        row.level=none(Declared(row.path,"Engine.Actor.Level",Kind::Object).text);
        const auto role=Declared(row.path,"Engine.Actor.Role",Kind::Byte).integer;
        Require(role>=0&&role<=255,"Touch fixture original Role is not a raw Byte");row.role=static_cast<std::uint8_t>(role);
        row.pawn=derives(row.cls,"Engine.Pawn");
        row.player=row.pawn&&Declared(row.path,"Engine.Pawn.bIsPlayer",Kind::Bool).boolean;
        row.brush=!none(Declared(row.path,"Engine.Actor.Brush",Kind::Object).text).empty()&&derives(row.cls,"Engine.Brush");
        row.location=Declared(row.path,"Engine.Actor.Location",Kind::Vector).vector;
        row.colLocation=Declared(row.path,"Engine.Actor.ColLocation",Kind::Vector).vector;
        row.rotation=Declared(row.path,"Engine.Actor.Rotation",Kind::Rotator).rotation;
        row.radius=Declared(row.path,"Engine.Actor.CollisionRadius",Kind::Float).floating;
        row.height=Declared(row.path,"Engine.Actor.CollisionHeight",Kind::Float).floating;
        for(std::uint32_t slot=0u;slot<4u;++slot) {
            const auto value=ReadPortableActorDeclaredScriptProperty(row.path,"Engine.Actor.Touching",slot);
            Require(value.kind==Kind::Object,"Touch fixture original Touching is not Object[4]");row.slots[slot]=none(value.text);
        }
        const auto index=rows.size();
        if(!actor.deleted&&std::all_of(row.slots.begin(),row.slots.end(),[](const auto& peer){return peer.empty();}))empty.push_back(index);
        if(Fold(row.path).find(".cratebreakable")!=std::string::npos)crates.push_back(index);
        if(row.pawn)pawns.push_back(index);
        if(row.player)players.push_back(index);
        if(row.brush)brushes.push_back(index);
        if(!row.role)roleZero.push_back(index);
        rows.push_back(std::move(row));
    }
    const auto locate=[&](const std::string& path) {
        const auto found=std::find_if(rows.begin(),rows.end(),[&](const auto& row){return Fold(row.path)==Fold(path);});
        Require(found!=rows.end(),"Touch fixture slot/pointer is outside actual Training census: "+path);
        return static_cast<std::size_t>(found-rows.begin());
    };
    Require(!crates.empty()&&empty.size()>=6u&&!brushes.empty(),"Cold Training lacks fresh crate/five-peer/brush controls");
    const auto sourceFound=std::find_if(crates.begin(),crates.end(),[&](auto index){return std::find(empty.begin(),empty.end(),index)!=empty.end();});
    Require(sourceFound!=crates.end(),"No authentic cold crate has empty Touching slots");const auto source=*sourceFound;
    const auto level=locate(rows[source].level);
    Require(!Declared(rows[level].path,"Engine.LevelInfo.bBegunPlay",Kind::Bool).boolean,
        "Touch integration must not force or rely on begun Training gameplay");
    for(auto& row:rows)for(auto& peer:row.slots)if(!peer.empty())peer=rows[locate(peer)].path;
    std::vector<std::size_t> peers;for(const auto index:empty)if(index!=source)peers.push_back(index);
    const auto command=[&](Operation operation,std::size_t a,std::size_t b,std::int32_t suppress=0) {
        Command result;result.operation=operation;result.actorPath=rows[a].path;result.otherActorPath=rows[b].path;
        result.touchSuppressSelf=suppress;return result;
    };
    struct Model {
        std::vector<Slots> slots;
        std::size_t touches{},untouches{},roleVictims{},pawnVictims{},playerVictims{},fullNoInsert{};
    };
    const auto initial=[&] {Model result;for(const auto& row:rows)result.slots.push_back(row.slots);return result;};
    const auto end=[&](Model& model,std::size_t a,std::size_t b,std::int32_t suppress) {
        const auto first=std::find(model.slots[a].begin(),model.slots[a].end(),rows[b].path);
        if(first!=model.slots[a].end()) {first->clear();if(!suppress)++model.untouches;}
        const auto reciprocal=std::find(model.slots[b].begin(),model.slots[b].end(),rows[a].path);
        if(reciprocal!=model.slots[b].end()) {reciprocal->clear();++model.untouches;}
    };
    const auto side=[&](Model& model,std::size_t a,std::size_t b) {
        // No event changes slots in this genuine dormant map. This separate
        // string-array interpreter predicts replacement from authentic fields.
        auto& slots=model.slots[a];
        if(std::find(slots.begin(),slots.end(),rows[b].path)!=slots.end())return true;
        int selected=-1;
        for(unsigned slot=0u;slot<4u;++slot)if(slots[slot].empty())selected=static_cast<int>(slot);
        if(selected<0) {
            for(unsigned slot=0u;slot<4u;++slot) {
                const auto old=locate(slots[slot]);
                if(rows[old].role==0u) {end(model,a,old,0);selected=static_cast<int>(slot);++model.roleVictims;}
            }
            if(selected<0&&rows[b].pawn) {
                for(unsigned slot=0u;slot<4u;++slot)if(!rows[locate(slots[slot])].pawn) {
                    end(model,a,locate(slots[slot]),0);selected=static_cast<int>(slot);++model.pawnVictims;break;
                }
                if(selected<0&&rows[b].player)for(unsigned slot=0u;slot<4u;++slot) {
                    const auto old=locate(slots[slot]);
                    if(!rows[old].pawn||!rows[old].player) {end(model,a,old,0);selected=static_cast<int>(slot);++model.playerVictims;break;}
                }
            }
        }
        if(selected<0) {++model.fullNoInsert;return true;}
        slots[static_cast<std::size_t>(selected)]=rows[b].path;++model.touches;return true;
    };
    // Exact integer-only overlap predictor. Source/other brush and receiver
    // Level gates are predicted separately, never via native overlap helper.
    // Integral source fields keep every sum/product exact in signed64 below.
    const auto integer=[](float value) {return std::isfinite(value)&&std::floor(value)==value&&std::abs(value)<=1'000'000.0f;};
    const auto integral=[&](std::size_t a) {const auto& r=rows[a];return integer(r.radius)&&integer(r.height)&&
        std::all_of(r.location.begin(),r.location.end(),integer);};
    const auto overlap=[&](std::size_t a,std::size_t b) {
        if(rows[a].brush||rows[b].brush||Fold(rows[b].path)==Fold(rows[a].level))return false;
        // For self, all retained differences are exactly zero regardless of
        // nonintegral authored coordinates. Signed finite sums square >0 iff
        // each dimension is nonzero. Original has no self exclusion.
        if(a==b) {
            Require(std::isfinite(rows[a].radius)&&std::isfinite(rows[a].height)&&
                std::all_of(rows[a].location.begin(),rows[a].location.end(),[](float value){return std::isfinite(value);}),
                "Self overlap fixture has unsafe active authored Float fields");
            return rows[a].radius!=0.0f&&rows[a].height!=0.0f;
        }
        Require(integral(a)&&integral(b),"Numeric overlap fixture is not exact bounded integer geometry");
        const auto d=[&](unsigned axis){return static_cast<std::int64_t>(rows[a].location[axis])-static_cast<std::int64_t>(rows[b].location[axis]);};
        const auto radius=static_cast<std::int64_t>(rows[a].radius)+static_cast<std::int64_t>(rows[b].radius),
            height=static_cast<std::int64_t>(rows[a].height)+static_cast<std::int64_t>(rows[b].height);
        return d(0u)*d(0u)+d(1u)*d(1u)<radius*radius&&d(2u)*d(2u)<height*height;
    };
    std::size_t batches{},touchCommands{},overlapCommands{},positiveOverlaps{},negativeOverlaps{},
        touchEvents{},untouchEvents{},roleVictims{},pawnVictims{},playerVictims{},fullNoInsert{};
    const auto run=[&](const std::vector<Command>& commands,const std::string& label) {
        Require(!commands.empty()&&commands.size()<=16u,"Touch integration batch exceeded sixteen commands");
        auto model=initial();const auto result=ProbePortableRuntimeRetailCollision(commands);
        Require(result.available&&result.execution.passed()&&!result.execution.committed&&result.states.size()==commands.size()&&
            result.execution.instructions==0u&&result.queries.empty()&&result.queryHeads.empty()&&result.mutations.empty()&&result.multiLine.empty(),
            "Cold first-Training touch diagnostic refused, executed gameplay bytecode or emitted unrelated outputs: "+label+" / "+result.execution.error);
        std::size_t ti{},pi{},work{};bool hash{};
        for(std::size_t index=0u;index<commands.size();++index) {
            const auto& cmd=commands[index];
            if(cmd.operation==Operation::Enable)hash=true;
            else {
                const auto a=locate(cmd.actorPath),b=locate(cmd.otherActorPath);
                if(cmd.operation==Operation::IsOverlapping) {
                    const bool expected=overlap(a,b);Require(pi<result.predicates.size(),"Overlap result omitted an actual command");
                    const auto& actual=result.predicates[pi++];
                    Require(actual.commandIndex==index&&actual.returned==expected&&actual.localPlayerPath.empty(),
                        "Actual overlap differs from source brush/Level gates or exact integer geometry");
                    ++overlapCommands;if(expected)++positiveOverlaps;else ++negativeOverlaps;
                }else {
                    bool accepted{};
                    if(cmd.operation==Operation::TouchSide)accepted=side(model,a,b);
                    else if(cmd.operation==Operation::BeginTouch) {if(side(model,a,b))static_cast<void>(side(model,b,a));}
                    else if(cmd.operation==Operation::EndTouch)end(model,a,b,cmd.touchSuppressSelf);
                    else Require(false,"Unexpected operation in touch reference interpreter");
                    Require(ti<result.touches.size(),"Touch diagnostic omitted its postcommand slot snapshot");const auto& actual=result.touches[ti++];
                    Require(actual.commandIndex==index&&actual.sideAccepted==accepted&&actual.source==model.slots[a]&&actual.other==model.slots[b],
                        "Actual first-Training touch slots/void result differ from independent suppressed-event interpreter: "+label);
                    ++touchCommands;
                }
            }
            const auto& state=result.states[index];
            Require(state.lifecycleOperations>work,"Touch commands reset or bypassed their shared same-root work ledger");work=state.lifecycleOperations;
            if(!hash)Require(!state.initialized&&!state.enabled&&!state.hashIdentity&&state.actors.empty()&&state.tag==0u&&
                state.seed==InitialSeed&&!state.initializationDraws,"Touch/overlap initialized a private hash or consumed RNG");
            else {std::vector<std::uint32_t> tags;for(const auto& actor:actors)tags.push_back(actor.tag);
                SameActors(state,actors,tags);Require(state.initialized&&state.enabled&&state.tag==0u&&state.initializationDraws==49152u,
                    "Touch modified enabled-hash membership, tags, declared mirrors or initialization history");}
        }
        Require(ti==result.touches.size()&&pi==result.predicates.size(),"Touch diagnostic emitted unrequested touch/predicate outputs");
        touchEvents+=model.touches;untouchEvents+=model.untouches;roleVictims+=model.roleVictims;pawnVictims+=model.pawnVictims;
        playerVictims+=model.playerVictims;fullNoInsert+=model.fullNoInsert;++batches;rollback();
    };
    const auto p=peers[0u];
    run({command(Operation::BeginTouch,source,p),command(Operation::BeginTouch,source,p),command(Operation::EndTouch,source,p),
        command(Operation::BeginTouch,source,p),command(Operation::EndTouch,source,p,std::numeric_limits<std::int32_t>::min()),
        command(Operation::TouchSide,source,p),command(Operation::EndTouch,source,p),command(Operation::EndTouch,source,p)},
        "fresh/duplicate/symmetric/suppressed/one-sided/absent");
    const auto full=[&](std::array<std::size_t,4u> occupants,std::size_t incoming,const std::string& label) {
        std::vector<Command> commands;for(const auto old:occupants)commands.push_back(command(Operation::TouchSide,source,old));
        commands.push_back(command(Operation::TouchSide,source,incoming));commands.push_back(command(Operation::BeginTouch,source,incoming));
        commands.push_back(command(Operation::EndTouch,source,incoming,1));run(commands,label);
    };
    full({peers[0u],peers[1u],peers[2u],peers[3u]},peers[4u],"five authentic peers/full replacement");
    const auto choose=[&](const std::function<bool(const Row&)>& predicate) {
        std::vector<std::size_t> result;for(const auto index:peers)if(predicate(rows[index]))result.push_back(index);return result;
    };
    const auto zeros=choose([](const Row& row){return row.role==0u;}),
        stableNonPawn=choose([](const Row& row){return row.role!=0u&&!row.pawn;}),
        stablePawn=choose([](const Row& row){return row.role!=0u&&row.pawn;}),
        stableNonplayerPawn=choose([](const Row& row){return row.role!=0u&&row.pawn&&!row.player;}),
        incomingPlayer=choose([](const Row& row){return row.pawn&&row.player;});
    if(!zeros.empty()) {std::array<std::size_t,4u> occupants{zeros.front(),0u,0u,0u};std::size_t used=1u;
        for(const auto index:peers)if(index!=zeros.front()&&used<4u)occupants[used++]=index;
        const auto incoming=std::find_if(peers.begin(),peers.end(),[&](auto index){return std::find(occupants.begin(),occupants.end(),index)==occupants.end();});
        Require(incoming!=peers.end(),"Role0 scenario lacks a distinct authentic fifth peer");full(occupants,*incoming,"authentic Role0 eviction");}
    if(stableNonPawn.size()>=4u&&!stablePawn.empty())
        full({stableNonPawn[0u],stableNonPawn[1u],stableNonPawn[2u],stableNonPawn[3u]},stablePawn.front(),"authentic incoming Pawn priority");
    if(stableNonplayerPawn.size()>=4u&&!incomingPlayer.empty())
        full({stableNonplayerPawn[0u],stableNonplayerPawn[1u],stableNonplayerPawn[2u],stableNonplayerPawn[3u]},incomingPlayer.front(),
            "authentic incoming bIsPlayer priority (not possession)");
    std::vector<Command> overlaps;
    overlaps.push_back(command(Operation::IsOverlapping,brushes.front(),source));
    overlaps.push_back(command(Operation::IsOverlapping,source,brushes.front()));
    overlaps.push_back(command(Operation::IsOverlapping,source,level));
    overlaps.push_back(command(Operation::IsOverlapping,source,source));
    std::vector<std::size_t> numeric;
    if(integral(source)&&!rows[source].brush)numeric.push_back(source);
    for(const auto index:empty)if(!rows[index].brush&&integral(index)&&numeric.size()<3u&&
        std::find(numeric.begin(),numeric.end(),index)==numeric.end())numeric.push_back(index);
    for(const auto a:numeric)for(const auto b:numeric)if(a!=source||b!=source)
        overlaps.push_back(command(Operation::IsOverlapping,a,b));
    run(overlaps,"native brush/Level fast gates and exact integer cylinder comparisons");
    run({command(Operation::TouchSide,source,p),command(Operation::IsOverlapping,source,level),
        command(Operation::BeginTouch,source,peers[1u]),command(Operation::EndTouch,source,p)},"interspersed overlap command indices");
    run({Command{Operation::Enable},command(Operation::BeginTouch,source,p),command(Operation::EndTouch,source,p)},"same root with existing native hash");
    const auto reject=[&](const std::vector<Command>& commands,const std::string& label,const QuestVr::Vm::Limits& limits={}) {
        const auto result=ProbePortableRuntimeRetailCollision(commands,limits);
        Require(!result.available&&!result.execution.passed()&&!result.execution.committed&&!result.execution.error.empty(),
            "Touch diagnostic accepted unsafe authority/budget or published earlier slot prefix: "+label);++refusals;rollback();
    };
    const auto witness=command(Operation::BeginTouch,source,p);
    auto bad=witness;bad.otherActorPath.clear();reject({bad},"None peer");bad=witness;bad.actorPath.clear();reject({bad},"None source");
    bad=witness;bad.otherActorPath=bad.actorPath;reject({bad},"self Touch pair");
    bad=witness;bad.otherActorPath="Engine.Actor";reject({bad},"class peer");
    bad=witness;bad.otherActorPath="00_Training.__MissingNativeTouch";reject({witness,bad},"late unknown peer");
    bad=witness;bad.actorPath.push_back('\0');bad.actorPath+="tail";reject({bad},"embedded source NUL");
    bad=witness;bad.otherActorPath.assign(8193u,'x');reject({bad},"oversized peer");
    reject({Command{Operation::Enable},witness,Command{static_cast<Operation>(999)}},"late unknown after Enable and inserted slots");
    QuestVr::Vm::Limits writes;writes.writes=1u;
    reject({command(Operation::TouchSide,source,p),command(Operation::TouchSide,source,peers[1u])},"second native slot write budget",writes);
    // One native slot write consumes entry1; even a DORMANT Touch dispatch
    // must attempt entry2. This distinguishes actual suppressed CallEvent
    // from an adapter silently omitting the original event after insertion.
    QuestVr::Vm::Limits eventEntry;eventEntry.operations=1u;
    reject({command(Operation::TouchSide,source,p)},"suppressed native Touch still charges a real event entry after its write",eventEntry);
    QuestVr::Vm::Limits retained;retained.retainedBytes=1u;reject({witness},"shared detached-output retained budget",retained);
    for(const auto& row:rows) {
        SameVector(Declared(row.path,"Engine.Actor.Location",Kind::Vector).vector,row.location,"Touch changed actor Location");
        SameVector(Declared(row.path,"Engine.Actor.ColLocation",Kind::Vector).vector,row.colLocation,"Touch changed actor ColLocation");
        Require(Declared(row.path,"Engine.Actor.Rotation",Kind::Rotator).rotation==row.rotation&&
            none(Declared(row.path,"Engine.Actor.Level",Kind::Object).text)==row.level&&
            Declared(row.path,"Engine.Actor.Role",Kind::Byte).integer==row.role,"Touch changed actor Rotation/Level/Role");
        for(std::uint32_t slot=0u;slot<4u;++slot) {
            const auto value=ReadPortableActorDeclaredScriptProperty(row.path,"Engine.Actor.Touching",slot);
            Require(value.kind==Kind::Object&&none(value.text)==row.slots[slot],"Successful/refused touch retained a live slot after rollback");
        }
    }
    Require(touchCommands>=20u&&overlapCommands>=3u&&negativeOverlaps>=3u&&touchEvents&&untouchEvents,
        "Actual first-Training touch cohort missed slot insertion/removal or original fast gates");
    rollback();
    std::cout<<"FIRST TRAINING touching: "<<batches<<" rollback-only batches, "<<touchCommands<<" live touch commands, "
        <<overlapCommands<<" overlaps ("<<positiveOverlaps<<" true/"<<negativeOverlaps<<" false; integer/self/fast-gate expectations), "
        <<touchEvents<<" Touch/"<<untouchEvents<<" UnTouch dispatch attempts with dormant gameplay bytecode suppressed; "
        <<roleVictims<<" Role0/"<<pawnVictims<<" Pawn/"<<playerVictims<<" bIsPlayer evictions, "<<fullNoInsert<<" full-no-insert returns. "
        <<roleZero.size()<<" actual Role0 actors, "<<pawns.size()<<" actual Pawns/"<<players.size()<<" authored bIsPlayer; no roles, possession or begun-play forced. "
        "Not published movement, startup or crate physics.\n";
}

// A real command creates this clock; no saved clock, begun flag, possession or
// actor role is injected. Full post-root equality tests rollback with live
// animation state, not clock preservation DURING a successful preview.
void AnimatedActorTouchRollback(Temporary& temporary,const std::string& crate) {
    const auto before=temporary.Save("BeforeAnimatedTouch");
    const auto checkpoint=temporary.Write("AnimatedTouchReset",before);
    const std::string doctor="00_Training.Doctor1";
    const auto waiting=ExecutePortableActorFunction(doctor,"PlayWaiting");
    Require(waiting.passed()&&waiting.committed&&waiting.instructions>0u,
        "Animated touching control failed actual Doctor1.PlayWaiting: "+waiting.error);
    const auto seeded=temporary.Save("AnimatedTouchSeed");Envelope state(seeded);
    const auto record=std::find_if(state.state.objects.begin(),state.state.objects.end(),
        [&](const auto& object){return object.path==doctor;});
    Require(record!=state.state.objects.end()&&record->clock.has_value()&&
        !record->clock->pose.main.sequence.empty(),"Actual PlayWaiting did not create a real saved animation clock");
    Require(!Declared("00_Training.LevelInfo0","Engine.LevelInfo.bBegunPlay",Kind::Bool).boolean,
        "PlayWaiting control silently began the Training world");
    const auto revision=GetPortableRuntimeWorldRevision();GC::Collect();const auto gc=GC::GetStats();
    const auto lookup=ReadPortableRuntimeObjectLookup();
    const auto same=[&] {
        GC::Collect();SameGc(gc,"Animated touch preview leaked real runtime objects");
        const auto current=ReadPortableRuntimeObjectLookup();
        Require(temporary.Save("AfterAnimatedTouch")==seeded&&GetPortableRuntimeWorldRevision()==revision&&
            current.generation==lookup.generation&&current.objects==lookup.objects&&
            !ReadPortableRuntimeRetailCollision().initialized&&
            !Declared("00_Training.LevelInfo0","Engine.LevelInfo.bBegunPlay",Kind::Bool).boolean,
            "Touch preview/refusal failed complete rollback of a real animation clock and shared world state");
    };
    Command begin;begin.operation=Operation::BeginTouch;begin.actorPath=doctor;begin.otherActorPath=crate;
    Command end=begin;end.operation=Operation::EndTouch;
    const auto success=ProbePortableRuntimeRetailCollision({begin,end});
    Require(success.available&&success.execution.passed()&&!success.execution.committed&&
        success.execution.instructions==0u&&success.touches.size()==2u,
        "Animated actual actor touch/untouch preview failed or executed dormant callbacks: "+success.execution.error);
    same();
    const auto late=ProbePortableRuntimeRetailCollision({begin,Command{static_cast<Operation>(999)}});
    Require(!late.available&&!late.execution.passed()&&!late.execution.committed&&late.execution.writes>=2u,
        "Late invalid command failed to exercise a real animated two-sided touching write prefix");++refusals;same();
    QuestVr::Vm::Limits limits;limits.writes=1u;
    const auto partial=ProbePortableRuntimeRetailCollision({begin},limits);
    Require(!partial.available&&!partial.execution.passed()&&!partial.execution.committed&&partial.execution.writes==1u,
        "Animated touch did not refuse and roll back its first-side-only write prefix");++refusals;same();
    Require(LoadPortableRuntimeState(checkpoint.string()),"Cannot restore preanimation Training checkpoint");
    GC::Collect();Require(temporary.Save("RestoredAnimatedTouch")==before,
        "Actual animation/touch regression retained its authored PlayWaiting changes");
    std::cout<<"FIRST TRAINING animated touching: actual Doctor1.PlayWaiting clock, one successful dormant preview and two "
        "late/partial-write refusals preserve complete clock/save/lookup/revision/GC state after rollback. "
        "Not an in-root clock-preservation or begun-world callback witness.\n";
}
