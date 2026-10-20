// Actual first Training only. Expected packets come from the separately
// exercised MultiLine diagnostic; selection is independently modeled below
// from qualified fields and detached class ancestry, never the new selector.
void SingleLineCoverage(const PortablePackageTables& map,const std::vector<Actor>& actors,
    const std::function<void()>& rollback) {
    Require(!actors.empty()&&map.exports.size()>actors.size(),"SingleLine lacks the actual first Training census");
    const auto snapshots=GetPortableRuntimeMapActors(true);
    std::map<std::string,std::vector<std::string>> ancestry;
    const auto derives=[&](const std::string& path,const char* wanted) {
        const auto actor=std::find_if(snapshots.begin(),snapshots.end(),[&](const auto& row){return row.objectPath==path;});
        Require(actor!=snapshots.end(),"SingleLine expected actor is outside the actual Level slots");
        auto found=ancestry.find(actor->classPath);
        if(found==ancestry.end()) {
            std::vector<std::string> chain;auto cls=actor->classPath;
            while(!cls.empty()) {
                Require(chain.size()<128u&&std::find(chain.begin(),chain.end(),Fold(cls))==chain.end(),
                    "SingleLine expected ancestry cycles or exceeds its bound");
                chain.push_back(Fold(cls));const auto declaration=ReadPortableRuntimeDispatchClass(cls);
                Require(Fold(declaration.path)==Fold(cls),"SingleLine expected class identity changed");
                cls=declaration.parentPath;
            }
            found=ancestry.emplace(actor->classPath,std::move(chain)).first;
        }
        return std::find(found->second.begin(),found->second.end(),Fold(wanted))!=found->second.end();
    };
    struct Expected {bool clear{true},next{},itemKnown{true};PortableRetailCollisionHit hit;
        std::size_t excluded{},filtered{},visits{},ownerReads{},ownerVisits{},classTests{},flagReads{};};
    const auto predict=[&](const Command& command,const PortableRetailCollisionProbeResult& baseline) {
        Require(baseline.queries.size()==1u&&baseline.queryHeads.size()==1u&&baseline.multiLine.size()==1u,
            "SingleLine reference MultiLine lost its owning result");
        Expected expected;expected.hit.location={11.0f,12.0f,13.0f};expected.hit.normal={-1.0f,0.0f,0.0f};
        expected.hit.time=1.0f;expected.hit.item=127;
        if(command.flags&4u)++expected.classTests; // Actual slot-zero class assertion.
        std::size_t cursor=baseline.queryHeads.front();
        while(cursor) {
            ++expected.visits;Require(cursor<=baseline.queries.front().size(),"Expected SingleLine active link exceeds its arena");
            const auto& hit=baseline.queries.front()[cursor-1u];bool excluded{};auto parent=command.actorPath;
            while(!parent.empty()) {
                Require(expected.ownerVisits<65536u,"Expected source Owner traversal exceeded its bound");
                ++expected.ownerVisits;
                if(parent==hit.actorPath){excluded=true;break;}
                ++expected.ownerReads;parent=Declared(parent,"Engine.Actor.Owner",Kind::Object).text;
                if(Fold(parent)=="none")parent.clear();
            }
            if(excluded){++expected.excluded;cursor=hit.next;continue;}
            bool accepted{};
            const auto isA=[&](const char* cls){++expected.classTests;return derives(hit.actorPath,cls);};
            const auto flag=[&](const char* field){++expected.flagReads;return Declared(hit.actorPath,field,Kind::Bool).boolean;};
            if(isA("Engine.LevelInfo"))accepted=(command.flags&4u)!=0u;
            else if(isA("Engine.Pawn"))accepted=(command.flags&1u)!=0u;
            else if(isA("Engine.Mover"))accepted=(command.flags&2u)!=0u;
            else if(isA("Engine.ZoneInfo"))accepted=(command.flags&8u)!=0u;
            else if(command.flags&0x10u)accepted=!(command.flags&0x20u)||flag("Engine.Actor.bProjTarget")||
                (flag("Engine.Actor.bBlockActors")&&flag("Engine.Actor.bBlockPlayers"));
            if(accepted){expected.clear=false;expected.next=hit.next!=0u;expected.hit=hit;expected.hit.next=0u;
                expected.itemKnown=hit.actorPath!=actors.front().path||baseline.multiLine.front().worldItemKnown;break;}
            ++expected.filtered;cursor=hit.next;
        }
        return expected;
    };
    std::size_t cases{},hits{},misses{},selfExclusions{},otherExclusions{},worldHits{},unknownWorldItems{},authoredOwners{};
    for(const auto& actor:actors)if(!Declared(actor.path,"Engine.Actor.Owner",Kind::Object).text.empty())++authoredOwners;
    const auto run=[&](Command command,bool enabled,const std::string& label) {
        command.operation=Operation::SingleLineCheck;
        Command multi=command;multi.operation=Operation::MultiLineCheck;multi.actorPath.clear();
        multi.flags=command.singleExtraFlags;multi.checkActors=(command.flags&0x17u)!=0u;multi.checkWorld=(command.flags&4u)!=0u;
        std::vector<Command> reference;if(enabled)reference.push_back(Command{Operation::Enable});reference.push_back(multi);
        const auto baseline=ProbePortableRuntimeRetailCollision(reference);
        Require(baseline.available&&baseline.execution.passed()&&!baseline.execution.committed,
            "SingleLine reference MultiLine failed: "+label+" "+baseline.execution.error);
        const auto expected=predict(command,baseline);rollback();
        std::vector<Command> commands;if(enabled)commands.push_back(Command{Operation::Enable});commands.push_back(command);
        const auto result=ProbePortableRuntimeRetailCollision(commands);
        Require(result.available&&result.execution.passed()&&!result.execution.committed&&result.singles.size()==1u&&
            result.states.size()==commands.size()&&result.execution.instructions==0u&&result.execution.writes==0u,
            "Actual SingleLine native adapter failed: "+label+" "+result.execution.error);
        const auto& actual=result.singles.front();const auto& hit=actual.hit;const auto& stats=actual.stats;
        Require(actual.commandIndex==commands.size()-1u&&actual.clear==expected.clear&&
            actual.hitNextPresent==expected.next&&actual.worldItemKnown==expected.itemKnown&&!hit.next&&
            hit.actorPath==expected.hit.actorPath&&hit.primitivePath==expected.hit.primitivePath&&
            !actual.marksAfter&&!actual.arenasAfter,
            "SingleLine selected wrong actual identity/order or retained a native hit arena: "+label);
        SameVector(hit.location,expected.hit.location,"SingleLine copied wrong whole-packet Location: "+label);
        SameVector(hit.normal,expected.hit.normal,"SingleLine copied wrong whole-packet Normal: "+label);
        SameFloat(hit.time,expected.hit.time,"SingleLine copied wrong Time/miss seed: "+label);
        if(expected.itemKnown)Require(hit.item==expected.hit.item,"SingleLine changed defined Item/miss seed: "+label);
        Require(stats.collectorCalls==1u&&stats.levelReads==((command.flags&4u)?1u:0u)&&
            stats.hitVisits==expected.visits&&stats.excluded==expected.excluded&&stats.filtered==expected.filtered&&
            stats.ownerVisits==expected.ownerVisits&&stats.ownerReads==expected.ownerReads&&
            stats.classTests==expected.classTests&&stats.flagReads==expected.flagReads,
            "SingleLine lazy class/Owner/flag path differs from independent declared-data model: "+label);
        const auto& state=result.states.back();const auto& old=baseline.states.back();
        Require(state.initialized==old.initialized&&state.enabled==old.enabled&&state.seed==old.seed&&state.tag==old.tag&&
            state.activeLinks==old.activeLinks&&state.initializationDraws==old.initializationDraws&&
            state.actors.size()==old.actors.size(),"SingleLine filter unexpectedly changed native hash registration/tags/RNG");
        for(std::size_t i=0u;i<state.actors.size();++i) {
            const auto& a=state.actors[i];const auto& b=old.actors[i];
            Require(a.path==b.path&&a.objectIndex==b.objectIndex&&a.tag==b.tag&&a.collideActors==b.collideActors&&
                a.pendingDelete==b.pendingDelete&&a.declaredMirrorsMatch,"SingleLine filter changed actual hash actor state");
            SameVector(a.colLocation,b.colLocation,"SingleLine moved a hash ColLocation");
        }
        if(expected.clear)++misses;else {++hits;if(hit.actorPath==actors.front().path){++worldHits;if(!actual.worldItemKnown)++unknownWorldItems;}}
        if(!command.actorPath.empty()){selfExclusions+=expected.excluded;otherExclusions+=expected.ownerReads;}
        ++cases;rollback();return result;
    };
    const auto crate=std::find_if(actors.begin(),actors.end(),[](const auto& a){return a.path=="00_Training.CrateBreakableMedGeneral1";});
    Require(crate!=actors.end()&&crate->collide&&!crate->deleted,"SingleLine fixture lacks its actual Training crate");
    const auto target=static_cast<std::size_t>(crate-actors.begin());
    auto ordered=SetterOrderQuery(actors,target);ordered.flags=0x10u;ordered.operation=Operation::SingleLineCheck;
    const auto noSource=run(ordered,true,"real crate pair actor-only None source");
    Require(!noSource.singles.front().clear&&noSource.singles.front().hitNextPresent,
        "SingleLine actual pair query did not expose a selected packet with nonzero qualified Next");
    ordered.actorPath=noSource.singles.front().hit.actorPath;
    const auto self=run(ordered,true,"actual nearest crate self exclusion");
    Require(!self.singles.front().clear&&self.singles.front().stats.excluded>=1u&&
        self.singles.front().hit.actorPath!=ordered.actorPath,
        "SingleLine source fixture did not exclude the first real crate and select the other");
    ordered.actorPath="00_Training.Doctor1";run(ordered,true,"actual Doctor source Owner ancestry");
    for(const std::uint8_t flags:{std::uint8_t{0x01u},std::uint8_t{0x02u},std::uint8_t{0x08u},std::uint8_t{0x13u},std::uint8_t{0x30u}}) {
        auto command=ordered;command.flags=flags;command.actorPath.clear();run(command,true,"actual crate categories "+std::to_string(flags));
    }
    // Exact no-hash/no-world path still calls the collector, but never creates
    // collision tables or touches their CRT stream. Absent actor hash is clear.
    auto absent=ordered;absent.flags=0x10u;absent.actorPath.clear();run(absent,false,"absent native hash actor request");
    absent.flags=0x08u;run(absent,false,"zone-only flags8 has no actor/world collection");
    for(const std::string& path:{std::string("00_Training.Doctor1"),std::string("00_Training.DeusExMover16")}) {
        const auto source=std::find_if(actors.begin(),actors.end(),[&](const auto& a){return a.path==path;});
        Require(source!=actors.end(),"SingleLine lost an actual NPC/Mover representative");
        const auto bounds=ReadPortableRuntimeActorPrimitiveBounds(path);std::optional<Command> witness;
        for(std::size_t axis=0u;axis<3u;++axis) {
            Command candidate{Operation::SingleLineCheck};candidate.flags=path.find("Doctor")!=std::string::npos?1u:2u;
            for(std::size_t i=0u;i<3u;++i)candidate.start[i]=candidate.end[i]=
                static_cast<float>((double(bounds.minimum[i])+double(bounds.maximum[i]))*0.5);
            candidate.start[axis]=bounds.minimum[axis]-16.0f;candidate.end[axis]=bounds.maximum[axis]+16.0f;
            const auto primitive=ProbePortableRuntimeRetailCollisionPrimitiveLine(path,candidate.end,candidate.start,candidate.extent);
            if(!primitive.clear){witness=candidate;break;}
        }
        Require(witness.has_value(),"SingleLine bounded bounds-center rays have no genuine NPC/Mover primitive hit");
        auto ray=*witness;
        const auto first=run(ray,true,"actual native NPC/Mover class category");
        Require(!first.singles.front().clear&&derives(first.singles.front().hit.actorPath,ray.flags==1u?"Engine.Pawn":"Engine.Mover"),
            "SingleLine accepted a mismatched actual class category");
        ray.actorPath=first.singles.front().hit.actorPath;
        const auto excluded=run(ray,true,"actual native NPC/Mover source exclusion");
        Require(excluded.singles.front().stats.excluded>=1u&&excluded.singles.front().hit.actorPath!=ray.actorPath,
            "SingleLine NPC/Mover case never excluded its genuinely selected source");
    }
    const auto native=ReadPortableRuntimeNativeModel();const auto model=LoadPortableModel68(map,native.modelExportIndex);
    for(const bool extent:{false,true}) {
        Command world{Operation::SingleLineCheck};world.flags=4u;world.start=world.end=crate->location;world.start[2u]+=128.0f;
        world.end=world.start;world.end[2u]-=512.0f;if(extent)world.extent={4.0f,4.0f,4.0f};
        const auto result=run(world,false,extent?"actual null-owner world extent":"actual null-owner world point");
        Require(!result.singles.front().clear&&result.singles.front().hit.actorPath==actors.front().path&&
            result.singles.front().hit.primitivePath==native.modelPath,"SingleLine actual world ray did not reach its loader Model");
        if(!extent)Require(result.singles.front().hit.item>=0&&static_cast<std::size_t>(result.singles.front().hit.item)<model.nodes.size(),
            "SingleLine point world Item is not its defined original node");
        world.actorPath=actors.front().path;run(world,false,"actual LevelInfo self exclusion preserves seeded miss");
        world.actorPath.clear();world.end=world.start;world.end[2u]+=8.0f;run(world,false,"actual clear world retains seeded packet");
    }
    const auto reject=[&](const std::vector<Command>& commands,const QuestVr::Vm::Limits& limits,const std::string& label) {
        const auto result=ProbePortableRuntimeRetailCollision(commands,limits);
        Require(!result.available&&!result.execution.passed()&&!result.execution.committed&&!result.execution.error.empty(),
            "SingleLine unsupported/late request succeeded: "+label);++refusals;rollback();return result;
    };
    auto successful=ordered;successful.actorPath.clear();successful.flags=0x10u;
    const auto late=reject({Command{Operation::Enable},successful,Command{static_cast<Operation>(999)}},{},"late failure after actual selected packet/pop");
    Require(late.singles.size()==1u&&!late.singles.front().clear&&!late.singles.front().marksAfter&&!late.singles.front().arenasAfter,
        "SingleLine late refusal missed its successful selected-packet/arena-pop prefix");
    auto bad=successful;bad.extent[0u]=std::numeric_limits<float>::quiet_NaN();
    const auto failed=reject({Command{Operation::Enable},successful,bad},{},"late active-geometry refusal");
    Require(failed.singles.size()==1u&&failed.states.size()==2u,"SingleLine geometry refusal lost its successful prefix");
    for(const auto& path:{std::string("00_Training.NoSuchActor"),std::string("Engine.Actor")}) {
        bad=successful;bad.actorPath=path;reject({bad},{},"non-Actor/stale source");
    }
    const auto complete=ProbePortableRuntimeRetailCollision({Command{Operation::Enable},successful});
    Require(complete.available&&complete.execution.nativeWork&&complete.execution.nativeWork->used>1u,
        "SingleLine complete root lacks its final shared native-work ledger");rollback();
    QuestVr::Vm::Limits low;low.nativeWork=complete.execution.nativeWork->used-1u;
    const auto cut=reject({Command{Operation::Enable},successful},low,"exact N-1 native-work refusal");
    Require(cut.execution.status==QuestVr::Vm::Status::Budget&&cut.execution.nativeWork&&cut.execution.nativeWork->exhausted,
        "SingleLine did not propagate typed shared-root work exhaustion");
    QuestVr::Vm::Limits exact;exact.nativeWork=complete.execution.nativeWork->used;
    const auto sufficient=ProbePortableRuntimeRetailCollision({Command{Operation::Enable},successful},exact);
    Require(sufficient.available&&sufficient.execution.passed()&&sufficient.execution.nativeWork&&
        sufficient.execution.nativeWork->used==exact.nativeWork&&!sufficient.execution.nativeWork->exhausted,
        "SingleLine exact N native-work control does not match the same completed root");rollback();
    const auto shared=reject({Command{Operation::Enable},successful,successful},exact,"second query shares first query's root allowance");
    Require(shared.execution.status==QuestVr::Vm::Status::Budget&&shared.execution.nativeWork&&
        shared.execution.nativeWork->exhausted&&shared.singles.size()==1u&&shared.states.size()>=2u&&
        !shared.singles.front().marksAfter&&!shared.singles.front().arenasAfter,
        "Second SingleLine query acquired a fresh root ledger or lost its successful first prefix");
    Require(hits&&misses&&selfExclusions&&worldHits==2u&&unknownWorldItems==1u,
        "Actual SingleLine cases did not establish hit/miss/self/world/undefined-Item cohorts");
    std::cout<<"FIRST TRAINING SingleLine: "<<cases<<" actual-Level rollback roots, "<<hits<<" selected / "<<misses
        <<" clear, "<<selfExclusions<<" source/self exclusions, "<<otherExclusions<<" actual Owner reads ("
        <<authoredOwners<<" authored nonempty Owner fields); independent class/Owner/flag selection over prior MultiLine packets, "
        "seeded misses, whole hits, Model Item provenance, empty hit marks, late geometry/typed work rollback. "
        "Not FindBase/support callbacks, startup, live gravity or crates.\n";
}

// LAST in its caller: two explicit checkpoint Load publications are expected.
// These Owner fields are a NONAUTHORED dormant first-Training property fixture,
// not SetOwner gameplay, a begun/possessed world or another-map integration.
void SingleLineOwnerFixture(Temporary& temporary,const std::vector<Actor>& actors) {
    using Value=QuestVr::Vm::Value;
    const std::string doctor="00_Training.Doctor1";
    const auto crate=std::find_if(actors.begin(),actors.end(),[](const auto& row){return row.path=="00_Training.CrateBreakableMedGeneral1";});
    Require(crate!=actors.end(),"SingleLine Owner fixture lacks its actual first-Training crate");
    auto query=SetterOrderQuery(actors,static_cast<std::size_t>(crate-actors.begin()));
    query.operation=Operation::MultiLineCheck;query.actorPath.clear();query.flags=0u;
    query.checkActors=true;query.checkWorld=false;
    const auto original=temporary.Save("BeforeSingleLineOwnerFixture");const Envelope envelope(original);
    Require(envelope.state.randomSeed.has_value(),"SingleLine Owner fixture lacks its original shared RNG state");
    const auto checkpoint=temporary.Write("SingleLineOwnerFixtureReset",original);
    GC::Collect();const auto originalGc=GC::GetStats();
    const auto originalLookup=ReadPortableRuntimeObjectLookup();const auto originalRevision=GetPortableRuntimeWorldRevision();
    const auto originalWorld=GetPortableRuntimeMapActors(true);
    const auto originalDirty=GetPortableRuntimeUnsavedScriptState();const auto originalPresent=GetPortableRuntimeScriptStatePresent();
    const auto originalTime=ReadPortableRuntimeLevelTime();
    const auto reference=ProbePortableRuntimeRetailCollision({Command{Operation::Enable},query});
    Require(reference.available&&reference.execution.passed()&&!reference.execution.committed&&
        reference.queries.size()==1u&&reference.queryHeads.size()==1u&&reference.queries.front().size()==2u&&
        reference.queryHeads.front()>0u&&reference.queryHeads.front()<=2u,
        "SingleLine Owner fixture requires an authentic actual two-crate MultiLine arena");
    const auto& packets=reference.queries.front();const auto& first=packets[reference.queryHeads.front()-1u];
    Require(first.next>0u&&first.next<=packets.size(),"SingleLine Owner fixture first crate has no actual linked successor");
    const auto& second=packets[first.next-1u];
    const auto cratePath=[](const std::string& path){return path=="00_Training.CrateBreakableMedGeneral1"||path=="00_Training.CrateBreakableMedGeneral2";};
    Require(cratePath(first.actorPath)&&cratePath(second.actorPath)&&first.actorPath!=second.actorPath&&!second.next,
        "SingleLine Owner fixture baseline does not contain exactly the two actual linked crates");
    const std::array<std::string,3u> paths{doctor,first.actorPath,second.actorPath};
    const std::array<std::string,3u> owners{first.actorPath,second.actorPath,std::string{}};
    std::array<std::string,3u> originalOwners;
    std::array<PortableRuntimeObjectOwnershipSnapshot,3u> identities;
    for(std::size_t i=0u;i<paths.size();++i){
        originalOwners[i]=Declared(paths[i],"Engine.Actor.Owner",Kind::Object).text;
        identities[i]=ReadPortableRuntimeObjectOwnership(paths[i]);
    }
    const auto identityStable=[&] {
        const auto current=ReadPortableRuntimeObjectLookup();
        Require(current.prepared==originalLookup.prepared&&current.generation==originalLookup.generation&&
            current.objects==originalLookup.objects&&current.retainedBytes==originalLookup.retainedBytes,
            "SingleLine Owner fixture changed actual UObject lookup storage/generation");
        for(std::size_t i=0u;i<paths.size();++i){
            const auto row=ReadPortableRuntimeObjectOwnership(paths[i]);
            Require(row.objectIndex==identities[i].objectIndex&&row.classIndex==identities[i].classIndex&&
                row.outerIndex==identities[i].outerIndex,"SingleLine Owner fixture replaced actual Actor/Class/Outer identity");
        }
        const auto world=GetPortableRuntimeMapActors(true);
        Require(world.size()==originalWorld.size()&&world.size()==actors.size(),"SingleLine Owner fixture changed actual native Actor census");
        for(std::size_t i=0u;i<world.size();++i){
            Require(world[i].objectPath==originalWorld[i].objectPath&&world[i].classPath==originalWorld[i].classPath&&
                world[i].activated==originalWorld[i].activated&&world[i].hidden==originalWorld[i].hidden&&
                world[i].hasLocation==originalWorld[i].hasLocation,"SingleLine Owner fixture changed Actor identity/activity instead of only Owner fields");
            SameVector({world[i].x,world[i].y,world[i].z},{originalWorld[i].x,originalWorld[i].y,originalWorld[i].z},
                "SingleLine Owner fixture changed an authored actor position");
        }
    };
    identityStable();SameGc(originalGc,"SingleLine Owner reference query leaked native UObjects");
    Require(temporary.Save("AfterSingleLineOwnerReference")==original&&GetPortableRuntimeWorldRevision()==originalRevision,
        "SingleLine Owner reference MultiLine failed original-state rollback");
    auto fixture=envelope.state;
    for(std::size_t i=0u;i<paths.size();++i){
        const auto actual=std::find_if(originalWorld.begin(),originalWorld.end(),[&](const auto& row){return row.objectPath==paths[i];});
        Require(actual!=originalWorld.end()&&!actual->classPath.empty(),"SingleLine Owner fixture cannot authenticate actual Actor UClass");
        auto saved=std::find_if(fixture.objects.begin(),fixture.objects.end(),[&](const auto& row){return row.path==paths[i];});
        if(saved==fixture.objects.end()){
            QuestVr::ScriptSavedObject object;object.path=paths[i];object.classPath=actual->classPath;
            fixture.objects.push_back(std::move(object));saved=std::prev(fixture.objects.end());
        }
        Require(saved->classPath==actual->classPath,"SingleLine Owner fixture existing checkpoint object has a different actual UClass");
        auto property=std::find_if(saved->properties.begin(),saved->properties.end(),[](const auto& row){
            return Fold(row.key)=="engine.actor.owner"&&row.index==0u;
        });
        QuestVr::ScriptSavedProperty value{"Engine.Actor.Owner","Owner",0u,Value::Text(Kind::Object,owners[i])};
        if(property==saved->properties.end())saved->properties.push_back(std::move(value));else *property=std::move(value);
    }
    const auto fixturePath=temporary.Write("DormantSingleLineNonAuthoredOwners",envelope.Wrap(fixture));
    Require(ValidatePortableRuntimeState(fixturePath.string(),"00_Training")&&LoadPortableRuntimeState(fixturePath.string()),
        "Actual qualified Owner checkpoint fixture was rejected by original schema validation");
    Require(GetPortableRuntimeWorldRevision()==originalRevision+1u,
        "SingleLine Owner fixture Load did not publish exactly one explicit checkpoint revision");
    GC::Collect();const auto fixtureGc=GC::GetStats();const auto seeded=temporary.Save("SingleLineOwnerFixtureSeed");
    const auto fixtureRevision=GetPortableRuntimeWorldRevision();const auto fixtureDirty=GetPortableRuntimeUnsavedScriptState();
    const auto fixturePresent=GetPortableRuntimeScriptStatePresent();
    const auto same=[&] {
        GC::Collect();SameGc(fixtureGc,"SingleLine Owner diagnostic leaked native Actor/GC state");identityStable();
        const auto collision=ReadPortableRuntimeRetailCollision();
        Require(!collision.initialized&&!collision.enabled&&!collision.hashIdentity&&collision.actors.empty()&&
            collision.seed==*envelope.state.randomSeed&&!collision.initializationDraws&&!collision.lifecycleOperations&&
            GetPortableRuntimeWorldRevision()==fixtureRevision&&temporary.Save("AfterSingleLineOwnerFixtureProbe")==seeded&&
            GetPortableRuntimeUnsavedScriptState()==fixtureDirty&&GetPortableRuntimeScriptStatePresent()==fixturePresent&&
            ReadPortableRuntimeLevelTime()==originalTime&&
            !Declared("00_Training.LevelInfo0","Engine.LevelInfo.bBegunPlay",Kind::Bool).boolean&&
            Declared("00_Training.LevelInfo0","Engine.LevelInfo.Game",Kind::Object).text.empty(),
            "SingleLine Owner diagnostic failed complete dormant fixture/save/hash/RNG/revision rollback");
        for(std::size_t i=0u;i<paths.size();++i)
            Require(Declared(paths[i],"Engine.Actor.Owner",Kind::Object).text==owners[i],
                "SingleLine Owner diagnostic changed its qualified live Owner chain");
    };
    same();
    query.operation=Operation::SingleLineCheck;query.flags=0x10u;query.actorPath.clear();
    const auto probe=[&](const std::string& source) {
        auto command=query;command.actorPath=source;
        const auto result=ProbePortableRuntimeRetailCollision({Command{Operation::Enable},command});
        Require(result.available&&result.execution.passed()&&!result.execution.committed&&result.singles.size()==1u&&
            result.execution.instructions==0u&&result.execution.writes==0u&&!result.singles.front().marksAfter&&
            !result.singles.front().arenasAfter,"Actual nonnull Owner SingleLine fixture query failed: "+result.execution.error);
        same();return result;
    };
    const auto unfiltered=probe({});
    Require(!unfiltered.singles.front().clear&&unfiltered.singles.front().hit.actorPath==first.actorPath&&
        unfiltered.singles.front().hitNextPresent&&unfiltered.singles.front().stats.ownerReads==0u,
        "SingleLine nonauthored Owner fields incorrectly changed a None-source linked crate selection");
    const auto inherited=probe(doctor);const auto& inheritedStats=inherited.singles.front().stats;
    Require(inherited.singles.front().clear&&inherited.singles.front().hit.actorPath.empty()&&
        inheritedStats.hitVisits==2u&&inheritedStats.excluded==2u&&inheritedStats.ownerReads==3u&&
        inheritedStats.ownerVisits==5u&&inheritedStats.classTests==0u&&inheritedStats.flagReads==0u,
        "SingleLine actual two-hop source Owner providers did not exclude both genuine crate ancestors before class/flag reads");
    const auto nearest=probe(first.actorPath);const auto& nearestStats=nearest.singles.front().stats;
    Require(nearest.singles.front().clear&&nearestStats.excluded==2u&&nearestStats.ownerReads==1u&&
        nearestStats.ownerVisits==3u&&nearestStats.classTests==0u,
        "SingleLine actual source self/Owner exclusions read the wrong direction or inactive Owner on equality");
    const auto last=probe(second.actorPath);
    Require(!last.singles.front().clear&&last.singles.front().hit.actorPath==first.actorPath&&
        last.singles.front().stats.ownerReads==1u&&last.singles.front().stats.excluded==0u,
        "SingleLine actual qualified Owner=None did not terminate the live source chain");
    auto owned=query;owned.actorPath=doctor;
    const auto late=ProbePortableRuntimeRetailCollision({Command{Operation::Enable},owned,Command{static_cast<Operation>(999)}});
    Require(!late.available&&!late.execution.passed()&&!late.execution.committed&&late.singles.size()==1u&&
        late.singles.front().clear&&late.singles.front().stats.excluded==2u&&!late.singles.front().marksAfter&&
        !late.singles.front().arenasAfter,"SingleLine Owner late refusal lost its actual nonnull-ancestor/pop prefix");++refusals;same();
    Require(inherited.execution.nativeWork&&inherited.execution.nativeWork->used>1u,
        "SingleLine Owner fixture lacks its final enclosing native-work accounting");
    QuestVr::Vm::Limits shortWork;shortWork.nativeWork=inherited.execution.nativeWork->used-1u;
    const auto cut=ProbePortableRuntimeRetailCollision({Command{Operation::Enable},owned},shortWork);
    Require(!cut.available&&!cut.execution.passed()&&!cut.execution.committed&&cut.execution.status==QuestVr::Vm::Status::Budget&&
        cut.execution.nativeWork&&cut.execution.nativeWork->exhausted,
        "SingleLine nonnull Owner provider path did not propagate shared typed work exhaustion");++refusals;same();
    Require(LoadPortableRuntimeState(checkpoint.string()),"Cannot restore original state after nonauthored SingleLine Owner fixture");
    GC::Collect();SameGc(originalGc,"Restored SingleLine Owner fixture retained native objects");identityStable();
    Require(GetPortableRuntimeWorldRevision()==originalRevision+2u&&temporary.Save("RestoredSingleLineOwnerFixture")==original&&
        GetPortableRuntimeUnsavedScriptState()==originalDirty&&GetPortableRuntimeScriptStatePresent()==originalPresent&&
        ReadPortableRuntimeLevelTime()==originalTime,"SingleLine Owner fixture did not restore exact original save with two explicit Load publications");
    for(std::size_t i=0u;i<paths.size();++i)
        Require(Declared(paths[i],"Engine.Actor.Owner",Kind::Object).text==originalOwners[i],
            "SingleLine Owner fixture retained a nonauthored qualified Owner property");
    const auto restoredHash=ReadPortableRuntimeRetailCollision();
    Require(!restoredHash.initialized&&!restoredHash.enabled&&!restoredHash.hashIdentity&&restoredHash.actors.empty()&&
        restoredHash.seed==*envelope.state.randomSeed,"SingleLine Owner restoration retained native hash/RNG effects");
    std::cout<<"FIRST TRAINING SingleLine NONAUTHORED dormant Owner fixture: Doctor1 -> "<<first.actorPath<<" -> "<<second.actorPath
        <<" -> None; actual qualified schema/lookup/provider two-hop exclusions, None/self controls, popped marks, late/typed-work rollback. "
        "Exact original checkpoint restored; two explicit Load publication revisions. Not authored ownership, SetOwner gameplay, startup or physics.\n";
}
