// Actual first Training only. Expected tuples come from source BSP data and
// hardware x87, NOT the production zone/region helpers. Script callbacks stay
// authentically suppressed by the dormant world; none is forced begun/possessed.
void ActorZoneCases(const PortablePackageTables& map,const std::vector<Actor>& actors,const std::function<void()>& rollback) {
#if (defined(__i386__) || defined(__x86_64__)) && defined(__GNUC__)
    struct Precision{unsigned short previous{};Precision(){asm volatile("fnstcw %0":"=m"(previous));
        const auto mode=static_cast<unsigned short>((previous&~0x0f00u)|0x033fu);asm volatile("fldcw %0"::"m"(mode));}
        ~Precision(){asm volatile("fldcw %0"::"m"(previous));}} precision;
    const auto native=ReadPortableRuntimeNativeModel();const auto model=LoadPortableModel68(map,native.modelExportIndex);
    const auto snapshots=GetPortableRuntimeMapActors(true);Require(snapshots.size()==actors.size(),"Zone census lost actual Training actors");
    const auto expected=[&](Array point){
        PortableRetailActorRegion out{actors.front().path,-1,0};if(model.nodes.empty())return out;
        std::size_t index{},steps{};
        while(true){Require(index<model.nodes.size()&&++steps<=model.nodes.size(),"Independent zone BSP path is malformed/cyclic");
            const auto& n=model.nodes[index];long double side;
            asm volatile("flds %[pz]; fmuls %[nz]; flds %[py]; fmuls %[ny]; faddp; "
                "flds %[px]; fmuls %[nx]; faddp; fsubs %[nw]; fstpt %[out]"
                :[out]"=m"(side):[pz]"m"(point[2u]),[nz]"m"(n.planeZ),[py]"m"(point[1u]),[ny]"m"(n.planeY),
                [px]"m"(point[0u]),[nx]"m"(n.planeX),[nw]"m"(n.planeW):"st");
            const bool front=side>=0;const auto child=front?n.front:n.back;
            if(child!=-1){Require(child>=0,"Independent zone BSP child is a negative nonterminal");index=static_cast<std::size_t>(child);continue;}
            out.leaf=front?n.leaf1:n.leaf0;const auto zone=model.zones.empty()?0:(front?n.zone1:n.zone0);
            Require(zone>=0&&zone<64,"Independent zone BSP terminal has an invalid zone");out.number=static_cast<std::uint8_t>(zone);
            const auto ref=static_cast<std::size_t>(zone)<model.zones.size()?model.zones[static_cast<std::size_t>(zone)].actorReference:0;
            if(ref)out.zonePath=(ref>0?"00_Training.":"")+GetPortableObjectPath(map,ref);
            return out;
        }
    };
    struct Row{std::string path;bool pawn{};Array location{};std::array<PortableRetailActorRegion,3u> regions;};
    std::vector<Row> selected;std::optional<Row> changed;std::string crate,crateClass;
    for(const auto& a:actors){
        const auto snapshot=std::find_if(snapshots.begin(),snapshots.end(),[&](const auto& s){return s.objectPath==a.path;});
        Require(snapshot!=snapshots.end(),"Zone source has no actual Actor class snapshot");
        Row row;row.path=a.path;row.pawn=snapshot->pawn;row.location=a.location;
        row.regions[0u]=a.path==actors.front().path?PortableRetailActorRegion{a.path,-1,0}:expected(a.location);
        if(row.pawn){const float height=Declared(a.path,"Engine.Actor.CollisionHeight",Kind::Float).floating;
            const float eyes=Declared(a.path,"Engine.Pawn.EyeHeight",Kind::Float).floating;Array foot=a.location,head{};
            asm volatile("flds %1; fsubs %2; fstps %0":"=m"(foot[2u]):"m"(a.location[2u]),"m"(height):"st");
            const float zero=0;
            for(unsigned i=0u;i<3u;++i){const float offset=i==2u?eyes:zero;
                asm volatile("flds %1; fadds %2; fstps %0":"=m"(head[i]):"m"(offset),"m"(a.location[i]):"st");}
            row.regions[1u]=expected(foot);row.regions[2u]=expected(head);
        }
        if(!a.deleted&&row.regions[0u].zonePath!=actors.front().path&&!changed)changed=row;
        if(a.path=="00_Training.CrateBreakableMedGeneral1"){crate=a.path;crateClass=snapshot->classPath;}
        if(!a.deleted&&(row.pawn||a.path==actors.front().path||a.path.find(".CrateBreakable")!=std::string::npos))selected.push_back(std::move(row));
    }
    Require(changed.has_value()&&!crate.empty()&&selected.size()>=24u,"Zone coverage lacks Pawn/crate/changed-zone controls");
    if(std::none_of(selected.begin(),selected.end(),[&](const auto& r){return r.path==changed->path;}))selected.push_back(*changed);
    const auto command=[](const std::string& path,int test,int force){Command c;c.operation=Operation::SetActorZone;c.actorPath=path;c.zoneTest=test;c.zoneForce=force;return c;};
    std::size_t batches{},commands{},pawns{},tuples{},enabled{};
    for(std::size_t first=0u;first<selected.size();first+=4u){
        const auto count=std::min<std::size_t>(4u,selected.size()-first);std::vector<Command> input;
        const bool withHash=(batches&1u)!=0u;if(withHash)input.push_back(Command{Operation::Enable});
        for(std::size_t i=0u;i<count;++i){const auto& row=selected[first+i];
            input.push_back(command(row.path,1,1));input.push_back(command(row.path,0,0));input.push_back(command(row.path,0,-1));}
        const auto result=ProbePortableRuntimeRetailCollision(input);
        Require(result.available&&result.execution.passed()&&!result.execution.committed&&result.zones.size()==count*3u&&
            result.execution.instructions==0u,"Actual original SetActorZone failed or executed dormant callback bodies: "+result.execution.error);
        for(std::size_t i=0u;i<result.zones.size();++i){const auto& actual=result.zones[i];const auto& row=selected[first+i/3u];
            Require(actual.actorPath==row.path&&actual.pawn==row.pawn&&actual.commandIndex==i+(withHash?1u:0u),"Actual native zone output lost actor/slot identity");
            Require(actual.clockBefore==actual.clockAfter,"Actual native zone write reconstructed an unrelated live animation clock");
            for(unsigned slot=0u;slot<(row.pawn?3u:1u);++slot){const auto& a=actual.regions[slot];const auto& b=row.regions[slot];
                Require(a.zonePath==b.zonePath&&a.leaf==b.leaf&&a.number==b.number,"Actual native stored zone tuple differs from independent source/x87 oracle: "+row.path);++tuples;}
            if(row.pawn)++pawns;
        }
        if(withHash){Require(result.states.front().enabled&&result.states.back().enabled,"Zone update destroyed a live original hash");
            Require(result.states.back().seed==result.states.front().seed&&result.states.back().tag==result.states.front().tag&&
                result.states.back().activeLinks==result.states.front().activeLinks,"Zone-only updates changed hash membership/tags/RNG");++enabled;}
        commands+=result.zones.size();++batches;rollback();
    }
    const auto reject=[&](std::vector<Command> input,const std::string& why,QuestVr::Vm::Limits limits={}){
        const auto result=ProbePortableRuntimeRetailCollision(input,limits);
        Require(!result.available&&!result.execution.passed()&&!result.execution.committed&&!result.execution.error.empty(),
            "Unsafe/late zone operation did not refuse: "+why);++refusals;rollback();return result;
    };
    // The real crate has an AUTHORED region, not None. Decode that source tag
    // independently of the runtime facade; do not change source or force a
    // different old Zone to make the test pass.
    const auto source=LoadPortableExportProperties(map,FindPortableExport(map,"CrateBreakableMedGeneral1"));
    const auto authored=std::find_if(source.properties.begin(),source.properties.end(),[](const auto& p){return p.name=="Region";});
    Require(authored!=source.properties.end(),"Actual crate lacks its source-authored region tag");
    QuestVr::AuthoredStructSchema schema;schema.path="Engine.Actor.PointRegion";schema.name="PointRegion";
    const auto zero=ReadPortableClassDefault(crateClass,"Region");
    for(const auto* name:{"Zone","iLeaf","ZoneNumber"}){QuestVr::AuthoredStructField field;field.key=schema.path+'.'+name;
        field.name=name;field.zero=zero.fields.at(Fold(name));if(std::string_view(name)=="Zone")field.referenceClassPath="Engine.ZoneInfo";
        schema.fields.push_back(std::move(field));}
    QuestVr::AuthoredStructResolvers resolvers;resolvers.resolveObject=[&](std::int32_t ref,const QuestVr::AuthoredStructField&){
        return ref?(ref>0?"00_Training.":"")+GetPortableObjectPath(map,ref):std::string{};};
    const auto old=QuestVr::DecodeAuthoredStructValue(schema,authored->value,map.version,resolvers);
    Require(old.fields.at("zone").text==actors.front().path&&old.fields.at("zone").text!=changed->regions[0u].zonePath,
        "Actual source crate and chosen source position do not have different zones");
    // Explicit posed-input fixture, never committed Move or hash relocation.
    // The actual old tag must survive when PointRegion samples a different
    // temporary Location. Budget1 admits ActorLeaving but refuses ZoneChange
    // BEFORE any store. A facade-as-old-Zone would instead write one member
    // and refuse the next, so a mere refusal is not the distinguishing check.
    auto posed=command(crate,0,0);posed.zoneUsePose=true;posed.end=changed->location;
    QuestVr::Vm::Limits oldEntry;oldEntry.operations=1u;
    const auto rawOld=reject({posed},"authored old Zone, not calculated posed-input facade",oldEntry);
    Require(rawOld.execution.writes==0u,"Authored old-zone event must be attempted before unforced main publication");
    posed.zoneTest=1;const auto posedSuccess=ProbePortableRuntimeRetailCollision({posed,command(crate,1,0)});
    const auto original=std::find_if(selected.begin(),selected.end(),[&](const auto& row){return row.path==crate;});
    Require(original!=selected.end(),"Actual posed-input crate is outside the selected source cohort");
    Require(posedSuccess.available&&posedSuccess.execution.passed()&&!posedSuccess.execution.committed&&posedSuccess.zones.size()==2u&&
        posedSuccess.zones.front().regions[0u].zonePath==changed->regions[0u].zonePath&&
        posedSuccess.zones.front().regions[0u].leaf==changed->regions[0u].leaf&&
        posedSuccess.zones.front().regions[0u].number==changed->regions[0u].number,
        "Actual zone query did not observe the declared temporary posed input");
    Require(posedSuccess.zones[1u].regions[0u].zonePath==original->regions[0u].zonePath&&
        posedSuccess.zones[1u].regions[0u].leaf==original->regions[0u].leaf&&
        posedSuccess.zones[1u].regions[0u].number==original->regions[0u].number,
        "Posed-input scope leaked into a subsequent ordinary zone operation inside the SAME root");rollback();
    posed.end[0u]=std::numeric_limits<float>::quiet_NaN();posed.zoneForce=1;
    const auto badPose=reject({posed},"active nonfinite posed point restores scope and prior force writes");
    Require(badPose.execution.writes>=3u,"Nonfinite posed point did not follow the genuine force-reset write prefix");
    reject({command(crate,1,1),Command{static_cast<Operation>(999)}},"late invalid command after complete zone writes");
    QuestVr::Vm::Limits oneWrite;oneWrite.writes=1u;
    const auto partial=reject({command(crate,1,1)},"partial first member of a real force-reset tuple",oneWrite);
    Require(partial.execution.writes==1u,"Zone write-budget failure did not follow its first journaled member write");
    QuestVr::Vm::Limits entry;entry.operations=changed->pawn?9u:3u;
    const auto event=reject({command(changed->path,0,1)},"dormant ActorLeaving must still attempt actual VM event entry",entry);
    Require(event.execution.writes==entry.operations&&event.execution.error=="VM operation entry limit",
        "Suppressed zone event entry failure missed the exact real force-reset prefix");
    for(const auto& path:{std::string{},std::string("Engine.Actor"),std::string("00_Training.NoSuchZoneActor")})reject({command(path,1,1)},"invalid actual actor identity");
    std::cout<<"FIRST TRAINING SetActorZone: "<<selected.size()<<" actual Actor/Pawn/LevelInfo representatives, "<<commands<<" native operations in "
        <<batches<<" rollback-only batches ("<<enabled<<" with active original hash), "<<tuples<<" independent source/x87 tuples, "<<pawns
        <<" Pawn operations; actual authored old-zone / explicit dormant posed-input controls, suppressed real event entry, complete/partial late rollback. "
        "No forced startup/role/possession or published movement/physics.\n";
#else
    (void)map;(void)actors;(void)rollback;std::cout<<"SKIP owned zone source/hardware oracle on non-x86 compiler\n";
#endif
}

void AnimatedActorZoneRollback(Temporary& temporary){
    const auto before=temporary.Save("BeforeAnimatedZone");const auto checkpoint=temporary.Write("ZoneAnimationReset",before);
    const std::string doctor="00_Training.Doctor1";const auto waiting=ExecutePortableActorFunction(doctor,"PlayWaiting");
    Require(waiting.passed()&&waiting.committed&&waiting.instructions>0u,"Zone clock control failed real Doctor1.PlayWaiting: "+waiting.error);
    const auto seeded=temporary.Save("AnimatedZoneSeed");Envelope envelope(seeded);
    const auto saved=std::find_if(envelope.state.objects.begin(),envelope.state.objects.end(),[&](const auto& row){return row.path==doctor;});
    Require(saved!=envelope.state.objects.end()&&saved->clock.has_value(),"Real zone clock control has no native animation state");
    QuestVr::ScriptSavedState expected;expected.mapName="00_Training";
    QuestVr::ScriptSavedObject object;object.path=doctor;object.classPath=saved->classPath;object.clock=saved->clock;expected.objects.push_back(object);
    const auto original=QuestVr::EncodeScriptSavedState(expected);
    GC::Collect();const auto gc=GC::GetStats();const auto revision=GetPortableRuntimeWorldRevision();const auto lookup=ReadPortableRuntimeObjectLookup();
    Command c;c.operation=Operation::SetActorZone;c.actorPath=doctor;c.zoneTest=1;c.zoneForce=1;
    const auto result=ProbePortableRuntimeRetailCollision({c});
    Require(result.available&&result.execution.passed()&&!result.execution.committed&&result.zones.size()==1u&&
        result.zones.front().clockBefore==original&&result.zones.front().clockAfter==original,
        "Zone writes changed the real native clock DURING the successful root, or did not capture it through the full codec");
    const auto same=[&]{GC::Collect();SameGc(gc,"Animated zone probe leaked runtime objects");const auto current=ReadPortableRuntimeObjectLookup();
        Require(temporary.Save("AfterAnimatedZone")==seeded&&GetPortableRuntimeWorldRevision()==revision&&current.generation==lookup.generation&&
            current.objects==lookup.objects&&!ReadPortableRuntimeRetailCollision().initialized&&
            !Declared("00_Training.LevelInfo0","Engine.LevelInfo.bBegunPlay",Kind::Bool).boolean,
            "Animated zone probe failed full clock/save/lookup/revision/hash/GC rollback");};
    same();const auto late=ProbePortableRuntimeRetailCollision({c,Command{static_cast<Operation>(999)}});
    Require(!late.available&&!late.execution.passed()&&!late.execution.committed&&late.execution.writes>=9u,
        "Animated zone late refusal did not follow genuine Pawn region writes");++refusals;same();
    Require(LoadPortableRuntimeState(checkpoint.string()),"Cannot restore before real animated zone control");GC::Collect();
    Require(temporary.Save("RestoredAnimatedZone")==before,"Animated zone test retained its PlayWaiting clock/fields");
    std::cout<<"FIRST TRAINING animated zone: actual Doctor1.PlayWaiting clock is byte-identical INSIDE successful native zone root via "
        "canonical full-clock codec snapshots; late refusal/save/revision/lookup/hash/GC rollback. No invented idle, role or begun flag.\n";
}
