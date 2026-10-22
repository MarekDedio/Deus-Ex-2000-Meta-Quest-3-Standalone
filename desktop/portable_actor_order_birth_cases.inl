// Included inside portable_actor_spawn_test's unnamed namespace after its
// existing fixture/tests. Generated native278/279 world and portable v17
// continuation coverage, NOT original startup, surviving crate loot or gravity.
void ActorOrderBirthTests(Fixture& fixture,const Tables& tables) {
    using Slot=QuestVr::ScriptSavedNativeActorSlot;
    using Tag=QuestVr::ScriptSavedNativeActorSlotKind;
    fixture.Reset();
    const auto first=Spawn();
    const auto second=Spawn("Make",{Class("Probe")});
    const auto removed=Spawn();
    Require(QuestVr::Vm::ToBool(Call(removed,"Die").value)&&!Published(removed),
        "Explicit-order control did not genuinely remove a spawned Actor");
    auto script=Decode(fixture.Snapshot("BeforeExplicitBirthOrder"));
    const auto original=ReadPortableLevel68ActorOrder(tables.map);
    Require(original.size()==7u&&original[0]&&original[1]&&original[3]&&original[4]&&original[5],
        "Explicit birth-order fixture lacks authenticated original slots");
    std::string level;
    for(std::size_t i=0u;i<tables.map.exports.size();++i)
        if(Fold(GetPortableObjectPath(tables.map,tables.map.exports[i].ObjClass))=="engine.level") {
            Require(level.empty(),"Explicit birth-order fixture has multiple Levels");
            level="SpawnFixture."+GetPortableObjectPath(tables.map,static_cast<std::int32_t>(i+1u));
        }
    Require(!level.empty(),"Explicit birth-order fixture has no actual ULevel");
    const auto authored=[](std::int32_t ref) {
        return Slot{Tag::AuthoredExport,static_cast<std::uint32_t>(ref-1),{}};
    };
    const auto born=[](const std::string& path) {return Slot{Tag::Birth,0u,path};};
    QuestVr::ScriptSavedNativeActorOrder order;
    order.levelPath=level;order.firstDynamic=4u;
    order.slots={authored(original[0]),authored(original[1]),born(second),authored(original[3]),{},
        born(first),authored(original[4]),authored(original[5]),{}};
    script.nativeActorOrder=order;
    Birth(script,first).worldActorIndex=5u;
    Birth(script,second).worldActorIndex=2u;
    Birth(script,removed).worldActorIndex=std::numeric_limits<std::uint32_t>::max();
    Require(Record(script,removed).lifecycle&&Record(script,removed).lifecycle->worldRemoved,
        "Nonmember birth lacks its real DestroyActor lifecycle");
    Record(script,first).properties.push_back({"Engine.Actor.bStatic","bStatic",0u,Value::Bool(true)});
    const auto generated=Envelope(fixture.legacy,script,17u);
    const auto path=fixture.directory/"ExplicitBirthOrder.sav";WriteBytes(path,generated);
    const auto prefix=[](const Bytes& bytes) {
        std::size_t cursor{};
        Require(Read32(bytes,cursor)==0x53515844u&&Read32(bytes,cursor)==17u,
            "Explicit native birth ordering was not saved in runtime v17");
        const auto string=[&] {
            const auto size=Read32(bytes,cursor);
            Require(size<=bytes.size()-cursor,"Native-order checkpoint string exceeds bound");cursor+=size;
        };
        const auto strings=[&] {
            const auto count=Read32(bytes,cursor);
            Require(count<100'000u&&count<=(bytes.size()-cursor)/4u,"Native-order checkpoint list exceeds bound");
            for(std::uint32_t i=0u;i<count;++i)string();
        };
        strings();strings();strings();static_cast<void>(Read32(bytes,cursor));
        const auto damaged=Read32(bytes,cursor);
        Require(damaged<100'000u&&damaged<=(bytes.size()-cursor)/8u,"Native-order damage list exceeds bound");
        for(std::uint32_t i=0u;i<damaged;++i){string();static_cast<void>(Read32(bytes,cursor));}
        static_cast<void>(Read32(bytes,cursor));static_cast<void>(Read32(bytes,cursor));
        for(unsigned i=0u;i<4u;++i)strings();
        return cursor;
    };
    const auto decode=[&](const Bytes& bytes) {
        auto cursor=prefix(bytes);const auto size=Read32(bytes,cursor);
        Require(size==bytes.size()-cursor,"Explicit-order checkpoint trailer differs from independent wire length");
        return QuestVr::DecodeScriptSavedState(Bytes(bytes.begin()+static_cast<std::ptrdiff_t>(cursor),bytes.end()));
    };
    const auto expected=[&] {
        PortableWorldActorOrderSnapshot result;result.available=true;result.levelPath=level;result.firstDynamic=4u;
        for(const auto& slot:order.slots) {
            std::string name;
            if(slot.kind==Tag::Birth)name=slot.birthPath;
            else if(slot.kind==Tag::AuthoredExport)
                name="SpawnFixture."+GetPortableObjectPath(tables.map,static_cast<std::int32_t>(slot.authoredExport+1u));
            result.actorIndices.push_back(name.empty()?0u:ReadPortableRuntimeObjectOwnership(name).objectIndex);
            result.staticFlags.push_back(name.empty()?0u:
                ReadPortableRuntimeDeclaredScriptProperty(name,"Engine.Actor.bStatic").boolean?1u:0u);
        }
        return result;
    };
    const auto restore=[&] {
        const auto before=fixture.Snapshot("BeforeExplicitOrderValidate");const auto revision=GetPortableRuntimeWorldRevision();
        const auto gc=GC::GetStats();
        Require(ValidatePortableRuntimeState(path.string(),"SpawnFixture"),"Valid explicit native birth order refused preflight");
        const auto after=GC::GetStats();
        Require(gc.numObjects==after.numObjects&&gc.memoryUsage==after.memoryUsage&&
            fixture.Snapshot("AfterExplicitOrderValidate")==before&&GetPortableRuntimeWorldRevision()==revision,
            "Explicit birth preflight changed native state or retained allocations");
        Require(LoadPortableRuntimeState(path.string()),"Valid explicit native birth order refused materialization");
        Require(ReadPortableWorldActorOrder()==expected()&&Published(first)&&Published(second)&&!Published(removed),
            "Explicit birth order/nulls/nonmember identity did not publish exactly");
        Require(Integer(first,"Trace")==123456&&Integer(removed,"Counter")==20,
            "Explicit native-order restore replayed or lost genuine birth/deletion callbacks");
    };
    restore();
    const auto canonical=fixture.Snapshot("ExplicitBirthCanonical");
    auto captured=decode(canonical);
    Require(captured.nativeActorOrder&&captured.nativeActorOrder->slots.size()==order.slots.size()&&
        captured.nativeActorOrder->firstDynamic==order.firstDynamic&&
        Birth(captured,removed).worldActorIndex==std::numeric_limits<std::uint32_t>::max(),
        "Saved native birth order lost split or detached birth sentinel");
    const auto stableOrder=ReadPortableWorldActorOrder();
    const auto probe=ProbePortableWorldActorPartition();
    Require(probe.execution.passed()&&!probe.execution.committed&&probe.before==stableOrder&&
        probe.after.actorIndices.size()==7u&&probe.after.firstDynamic==3u&&
        probe.after.actorIndices[2]==ReadPortableRuntimeObjectOwnership(first).objectIndex&&
        ReadPortableWorldActorOrder()==stableOrder&&fixture.Snapshot("BirthPartitionRollback")==canonical,
        "Actual generated partition failed to move static birth ahead of authored dynamics or restore exact order");
    fixture.Reset();restore();Require(fixture.Snapshot("ExplicitBirthWarm")==canonical,"Warm explicit birth restore is not canonical");
    ShutdownPortableRuntime();
    Require(InitializePortableRuntime({tables.core,tables.engine,tables.classes}).passed&&LoadPortableRuntimeMap(tables.map).passed,
        "Cannot cold-load generated source for explicit native birth restoration");
    restore();Require(fixture.Snapshot("ExplicitBirthCold")==canonical,"Cold explicit birth restore is not exact/canonical");
    const auto rejected=[&](QuestVr::ScriptSavedState bad,const char* label,bool structural=false) {
        const auto before=fixture.Snapshot("BeforeInvalidExplicitBirth");const auto orderBefore=ReadPortableWorldActorOrder();
        const auto revision=GetPortableRuntimeWorldRevision();const auto gc=GC::GetStats();
        Bytes bytes;
        try {bytes=Envelope(fixture.legacy,bad,17u);}catch(const std::exception&) {
            Require(structural,"Semantic malformed birth order failed only structural codec");++refusals;return;
        }
        const auto invalid=fixture.directory/(std::string(label)+".sav");WriteBytes(invalid,bytes);
        Require(!ValidatePortableRuntimeState(invalid.string(),"SpawnFixture")&&!LoadPortableRuntimeState(invalid.string()),
            std::string("Invalid explicit native birth order accepted: ")+label);
        const auto after=GC::GetStats();
        Require(gc.numObjects==after.numObjects&&gc.memoryUsage==after.memoryUsage&&
            fixture.Snapshot("AfterInvalidExplicitBirth")==before&&ReadPortableWorldActorOrder()==orderBefore&&
            GetPortableRuntimeWorldRevision()==revision&&ReadBytes(invalid)==bytes&&ReadBytes(path)==generated,
            std::string("Invalid explicit birth restore changed world/source/checkpoint: ")+label);++refusals;
    };
    auto bad=captured;Birth(bad,removed).worldActorIndex=8u;rejected(bad,"NonmemberBirthIndex",true);
    bad=captured;Record(bad,removed).lifecycle->worldRemoved=false;rejected(bad,"NonmemberBirthNotRemoved");
    bad=captured;bad.nativeActorOrder->slots[5]={};Birth(bad,first).worldActorIndex=std::numeric_limits<std::uint32_t>::max();
    rejected(bad,"OmittedLiveBirth");
    bad=captured;bad.nativeActorOrder->slots[4]=born(first);rejected(bad,"DuplicateNativeBirth",true);
    bad=captured;bad.nativeActorOrder->slots[4]=born("SpawnFixture.MissingBirth");rejected(bad,"UnregisteredNativeBirth",true);
    Require(Spawn()==Path("Leaf3"),"Explicit cold restore lost per-class allocator continuation across removed birth");
    fixture.Reset();Require(ReadPortableWorldActorOrder().actorIndices.size()==original.size(),
        "Legacy restore retained explicit native birth order instead of immutable source slots");
    fixture.UnchangedSources();
    std::cout<<"Generated explicit native Actor order: real spawned/static/removed births, exact null slots/sentinel, warm/cold replacement, partition rollback, allocator continuation and malformed controls; NOT crate loot/gravity acceptance\n";
}
