// Generated, commercial-data-free production native3970 reentry. Included in
// the same anonymous namespace as Package/Fixture/Tables. Its tiny synthetic
// LevelInfo is AUTHORED begun=true; no original map or runtime flag is edited.
// No native Level/Model/hash/query service is needed on the reached branches.
Tables BuildPhysicsReentry(Fixture& fixture,bool callbackFailure=false) {
    Package core;core.stem="Core";
    const auto cp=core.ImportPackage("Core"),object=core.Export("Object"),meta=core.Export("Class",0,object);
    const auto vector=core.Export("Vector",core.Import("Struct",cp),0,object);
    for(const auto* channel:{"X","Y","Z"})core.Property(channel,"FloatProperty",vector,cp,0,1u);
    core.ClassBody(object);core.ClassBody(meta);core.StructBody(vector);const auto ct=fixture.Write(core);
    Package engine;engine.stem="Engine";
    const auto ec=engine.ImportPackage("Core"),eo=engine.Import("Object",ec),ev=engine.Import("Vector",eo,"Struct");
    const auto actor=engine.Export("Actor",0,eo),info=engine.Export("LevelInfo",0,actor);engine.Native(actor);engine.Native(info);
    const auto enumeration=engine.Export("EPhysics",engine.Import("Enum",ec),0,actor,true);
    std::vector<std::int32_t> enumNames;
    for(const auto* name:{"PHYS_None","PHYS_Walking","PHYS_Falling","PHYS_Swimming","PHYS_Flying",
        "PHYS_Rotating","PHYS_Projectile","PHYS_Rolling","PHYS_Interpolating","PHYS_MovingBrush","PHYS_Spider"})
        enumNames.push_back(engine.Name(name));
    engine.builders.at(static_cast<std::size_t>(enumeration-1))=[enumNames=std::move(enumNames)](std::int32_t next,std::int32_t) {
        Bytes b{0u};Index(b,0);Index(b,next);Index(b,static_cast<std::int32_t>(enumNames.size()));
        for(const auto name:enumNames)Index(b,name);
        return b;
    };
    engine.Property("Physics","ByteProperty",actor,ec,enumeration,0x23u);
    engine.Property("Base","ObjectProperty",actor,ec,actor,0x22u);
    engine.Property("StandingCount","ByteProperty",actor,ec,0,0x2u);
    engine.Property("Level","ObjectProperty",actor,ec,info,0x2u);
    engine.Property("bDeleteMe","BoolProperty",actor,ec,0,0x2u);
    engine.Property("Velocity","StructProperty",actor,ec,ev,0x21u);
    engine.Property("Acceleration","StructProperty",actor,ec,ev,0x20u);
    engine.Property("bBegunPlay","BoolProperty",info,ec);
    const auto setter=engine.Function("SetPhysics",actor,ec);
    const auto input=engine.Property("newPhysics","ByteProperty",setter,ec,enumeration,0x80u);
    const auto floor=engine.Property("newFloor","ObjectProperty",setter,ec,actor,0x90u);
    const auto signature=Join({Ref(0x29u,input),Ref(0x29u,floor)});
    Require(signature.logical==10u,"Reentrant original3970 packet is not ten logical bytes");
    engine.builders.at(static_cast<std::size_t>(setter-1))=[&engine,setter,signature](std::int32_t next,std::int32_t first) {
        Bytes b{0u};Index(b,0);Index(b,next);Index(b,0);Index(b,first);
        Index(b,engine.exports.at(static_cast<std::size_t>(setter-1)).ObjName);U32(b,0);U32(b,0);
        U32(b,static_cast<std::uint32_t>(signature.logical));Append(b,signature.raw);U16(b,3970u);b.push_back(0u);U32(b,0x401u);return b;
    };
    const auto outerFail=engine.Function("PhysicsThenFail",actor,ec);
    const auto outerCall=Join({Ref(0x1cu,setter),Code{{0x24u,2u},2u},Token(0x16u)});
    const Code unknown{{0x63u,0xe7u,0x16u},3u};
    engine.FunctionBody(outerFail,Join({outerCall,unknown,Return()}));
    engine.ClassBody(actor);engine.ClassBody(info);const auto et=fixture.Write(engine);
    Package classes;classes.stem="PhysicsReentryClasses";
    const auto cc=classes.ImportPackage("Core"),ce=classes.ImportPackage("Engine"),ca=classes.Import("Actor",ce);
    const auto support=classes.Export("Support",0,ca),detach=classes.Function("Detach",support,cc);
    const auto other=classes.Property("Other","ObjectProperty",detach,cc,ca,0x80u);
    // Make Detach an actual event Function, not a direct helper substituting
    // for ProcessEvent. It is selected on the support's real script class.
    const auto nested=Join({Code{{0x6fu,0x82u},2u},Code{{0x24u,5u},2u},Token(0x17u),Token(0x16u)});
    Code context=Join({Token(0x19u),Ref(0x00u,other)});
    Require(nested.logical<=65535u,"Nested original3970 Context exceeds its logical skip packet");
    U16(context.raw,static_cast<std::uint16_t>(nested.logical));context.raw.push_back(0u);context.logical+=3u;
    Append(context.raw,nested.raw);context.logical+=nested.logical;
    const auto body=callbackFailure?Join({context,unknown,Return()}):Join({context,Return()});
    classes.builders.at(static_cast<std::size_t>(detach-1))=[&classes,detach,body](std::int32_t next,std::int32_t first) {
        Bytes b{0u};Index(b,0);Index(b,next);Index(b,0);Index(b,first);
        Index(b,classes.exports.at(static_cast<std::size_t>(detach-1)).ObjName);U32(b,0);U32(b,0);
        U32(b,static_cast<std::uint32_t>(body.logical));Append(b,body.raw);U16(b,0u);b.push_back(0u);U32(b,0x800u);return b;
    };
    classes.ClassBody(support);const auto st=fixture.Write(classes);
    Package map;map.stem="PhysicsReentry";
    const auto me=map.ImportPackage("Engine"),mc=map.ImportPackage("PhysicsReentryClasses");
    const auto ma=map.Import("Actor",me),mi=map.Import("LevelInfo",me),ms=map.Import("Support",mc);
    const auto level=map.Export("Info",mi),source=map.Export("Source",ma),base=map.Export("Support",ms);
    const auto objectTag=[&](Bytes& tags,const char* name,std::int32_t target) {
        Bytes data;Index(data,target);map.Tag(tags,name,5u,data);
    };
    Bytes begun;objectTag(begun,"Level",level);Index(begun,map.Name("bBegunPlay"));begun.push_back(0x83u);map.ActorBody(level,begun);
    Bytes supportTags;objectTag(supportTags,"Level",level);map.Tag(supportTags,"StandingCount",1u,Bytes{1u});map.ActorBody(base,supportTags);
    Bytes sourceTags;objectTag(sourceTags,"Level",level);objectTag(sourceTags,"Base",base);map.Tag(sourceTags,"Physics",1u,Bytes{3u});
    Bytes velocity,acceleration;
    for(const auto word:{0x80000000u,0x7fc12345u,0x7f800000u})U32(velocity,word);
    for(const auto word:{0xff800000u,0x7f812345u,0x00000001u})U32(acceleration,word);
    map.Tag(sourceTags,"Velocity",10u,velocity,"Vector");map.Tag(sourceTags,"Acceleration",10u,acceleration,"Vector");
    map.ActorBody(source,sourceTags);return {{ct,et,st},fixture.Write(map,true)};
}

void PhysicsReentryCoverage() {
    const std::string actor="PhysicsReentry.Source",support="PhysicsReentry.Support",info="PhysicsReentry.Info";
    const auto property=[](const std::string& object,const char* field) {
        return ReadPortableActorDeclaredScriptProperty(object,std::string("Engine.Actor.")+field);
    };
    const auto arguments=std::vector<Evaluation>{{Value::Byte(2u),{}}};
    std::size_t completeRoots{},cuts{},lateRoots{},callbackRefusals{};
    {
        Fixture fixture;const auto tables=BuildPhysicsReentry(fixture);
        Require(InitializePortableRuntime(tables.scripts).passed&&LoadPortableRuntimeMap(tables.map).passed,
            "Generated original3970 reentry fixture failed initialization");
        Require(property(actor,"Physics").integer==3&&property(actor,"Base").text==support&&
            property(support,"StandingCount").integer==1&&
            ReadPortableActorDeclaredScriptProperty(info,"Engine.LevelInfo.bBegunPlay").boolean,
            "Generated reentry lost its authentic authored Physics/Base/count/event eligibility");
        Require(ResolvePortableActorFunction(support,"None","Detach")=="PhysicsReentryClasses.Support.Detach",
            "Reentry fixture did not select the actual support-class Detach event");
        GC::Collect();const auto gc=GC::GetStats();const auto revision=GetPortableRuntimeWorldRevision();
        const auto before=fixture.Snapshot("PhysicsReentryBefore");const auto actors=GetPortableRuntimeMapActors(true);
        const auto unchanged=[&] {
            GC::Collect();const auto current=GC::GetStats();
            Require(fixture.Snapshot("PhysicsReentryAfter")==before&&GetPortableRuntimeWorldRevision()==revision&&
                !GetPortableRuntimeUnsavedScriptState()&&current.numObjects==gc.numObjects&&current.memoryUsage==gc.memoryUsage&&
                property(actor,"Physics").integer==3&&property(actor,"Base").text==support&&
                property(support,"StandingCount").integer==1,
                "Nested original3970 failure retained Physics/Base/count/raw words/save/revision/GC state");
            const auto world=GetPortableRuntimeMapActors(true);Require(world.size()==actors.size(),"Reentry changed Actor census");
            for(std::size_t i=0u;i<world.size();++i)Require(world[i].objectPath==actors[i].objectPath&&world[i].classPath==actors[i].classPath,
                "Reentry changed generated Actor slot/class identity");
            for(const auto* field:{"Velocity","Acceleration"}) {
                bool refused{};try{static_cast<void>(property(actor,field));}catch(const std::exception&){refused=true;}
                Require(refused,"Nested3970 undo did not restore nonfinite source tag authority");++refusals;
            }
            fixture.UnchangedSources();
        };
        // Outer Falling2 stores Physics, then countDEC; Detach calls inner5
        // while Base STILL names this support. Its explicit floor equals that
        // live Base, legitimately skipping query/support dispatch. Inner stores
        // Physics5+sixzeros; outer then stores BaseNone and MUST reread5, not
        // its original requested2, and perform the sixzero block AGAIN.
        for(std::size_t limit=0u;limit<16u;++limit) {
            QuestVr::Vm::Limits limits;limits.writes=limit;
            const auto cut=ExecutePortableActorFunction(actor,"SetPhysics",arguments,limits);
            Require(!cut.passed()&&!cut.committed&&cut.status==QuestVr::Vm::Status::Budget&&cut.writes==limit,
                "Production nested3970 acquired a fresh write budget or bypassed post-callback zero stores: "+cut.error);
            ++cuts;++refusals;unchanged();
        }
        const auto late=ExecutePortableActorFunction(actor,"PhysicsThenFail");
        Require(!late.passed()&&!late.committed&&late.writes==16u&&late.error.find("999")!=std::string::npos,
            "Reentrant original3970 late failure did not reach both native zero blocks: "+late.error);
        ++lateRoots;++refusals;unchanged();
        QuestVr::Vm::Limits exact;exact.writes=16u;
        const auto result=ExecutePortableActorFunction(actor,"SetPhysics",arguments,exact);
        Require(result.passed()&&result.committed&&result.writes==16u&&property(actor,"Physics").integer==5&&
            property(actor,"Base").text.empty()&&property(support,"StandingCount").integer==0&&
            result.nativeWork&&result.nativeWork->used&&!result.nativeWork->exhausted,
            "Production original3970 did not preserve nested Physics5/countdetach or outer final reread: "+result.error);
        for(const auto* field:{"Velocity","Acceleration"}) {
            const auto value=property(actor,field);Require(value.kind==Kind::Vector,"Nested3970 final raw field is not Vector");
            for(const auto word:value.vector)Require(std::bit_cast<std::uint32_t>(word)==0u,
                "Production nested/outer3970 did not produce six literal positive-zero words");
        }
        Require(fixture.Snapshot("PhysicsReentryCommitted")!=before&&GetPortableRuntimeWorldRevision()>revision&&
            GetPortableRuntimeUnsavedScriptState(),"Successful nested3970 did not publish finite final state");
        fixture.UnchangedSources();++completeRoots;
    }
    {
        Fixture fixture;const auto tables=BuildPhysicsReentry(fixture,true);
        Require(InitializePortableRuntime(tables.scripts).passed&&LoadPortableRuntimeMap(tables.map).passed,
            "Generated nested3970 callback-failure fixture failed initialization");
        const auto before=fixture.Snapshot("PhysicsReentryCallbackBefore");const auto revision=GetPortableRuntimeWorldRevision();
        GC::Collect();const auto gc=GC::GetStats();
        const auto result=ExecutePortableActorFunction(actor,"SetPhysics",arguments);
        Require(!result.passed()&&!result.committed&&result.writes==9u&&result.error.find("999")!=std::string::npos,
            "Nested3970 callback refusal did not preserve its original completed inner zero-block prefix: "+result.error);
        GC::Collect();const auto current=GC::GetStats();
        Require(fixture.Snapshot("PhysicsReentryCallbackAfter")==before&&GetPortableRuntimeWorldRevision()==revision&&
            !GetPortableRuntimeUnsavedScriptState()&&current.numObjects==gc.numObjects&&current.memoryUsage==gc.memoryUsage&&
            property(actor,"Physics").integer==3&&property(actor,"Base").text==support&&property(support,"StandingCount").integer==1,
            "Throwing Detach failed to undo its nested native3970/outer Physics/count stores");
        fixture.UnchangedSources();++callbackRefusals;++refusals;
    }
    Require(completeRoots==1u&&cuts==16u&&lateRoots==1u&&callbackRefusals==1u,
        "Production nested3970 case count escaped its exact closed bound");
    std::cout<<"GENERATED original3970 reentry: actual class-selected Detach ProcessEvent, innerPhysics5 before BaseNone, "
        "outer livePhysics reread and repeated six-wordzero block; exact16stores, all16partialwrite prefixes, late-root and "
        "throwing-callback full rollback. Synthetic authored event eligibility only; no original map mutation, nativequery bypass, "
        "falling phase or playable gravity claim.\n";
}
