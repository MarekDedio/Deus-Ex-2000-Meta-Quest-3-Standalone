// Included inside the generated-coordinate fixture's anonymous namespace,
// after Package/Fixture/Tables helpers. All serialized packages are created by
// this test. Original game files, VM/save finite policy and runtime inputs are
// never patched. This tests native3970 raw storage, NOT a physics phase.
enum class PhysicsGeneratedVariant {Good,FunctionClass,InputByteClass,FloorObjectClass,PhysicsClass,
    PhysicsBase,VelocityClass,VelocityFlags,VelocityDimension,VectorOrder,VectorChannel,VectorOwner,VectorBase};

Tables BuildPhysics(Fixture& fixture,PhysicsGeneratedVariant variant=PhysicsGeneratedVariant::Good) {
    Package core;core.stem="Core";
    const auto cp=core.ImportPackage("Core"),object=core.Export("Object"),meta=core.Export("Class",0,object);
    const auto vector=core.Export("Vector",core.Import("Struct",cp),0,
        variant==PhysicsGeneratedVariant::VectorOwner?meta:object);
    const auto channels=variant==PhysicsGeneratedVariant::VectorOrder?
        std::array<const char*,3u>{"Y","X","Z"}:std::array<const char*,3u>{"X","Y","Z"};
    for(const auto* name:channels)core.Property(name,
        variant==PhysicsGeneratedVariant::VectorChannel&&std::string(name)=="Z"?"IntProperty":"FloatProperty",
        vector,cp,0,1u);
    core.ClassBody(object);core.ClassBody(meta);core.StructBody(vector);
    if(variant==PhysicsGeneratedVariant::VectorBase)
        core.builders.at(static_cast<std::size_t>(vector-1))=[&core,vector,object](std::int32_t next,std::int32_t first) {
            Bytes b{0u};Index(b,object);Index(b,next);Index(b,0);Index(b,first);
            Index(b,core.exports.at(static_cast<std::size_t>(vector-1)).ObjName);U32(b,0);U32(b,0);U32(b,0);return b;
        };
    const auto ct=fixture.Write(core);
    // These real, distinctly qualified classes intentionally have the same
    // leaf Names as metadata. No loader-wide rejection is assumed: the actual
    // original native adapter must authenticate its own declaration/fields.
    Package spoof;spoof.stem="PhysicsSpoof";const auto sc=spoof.ImportPackage("Core"),so=spoof.Import("Object",sc);
    for(const auto* name:{"Function","ByteProperty","ObjectProperty","StructProperty"}) {
        const auto cls=spoof.Export(name,0,so);spoof.ClassBody(cls);
    }
    const auto st=fixture.Write(spoof);
    Package engine;engine.stem="Engine";
    const auto ec=engine.ImportPackage("Core"),eo=engine.Import("Object",ec),ev=engine.Import("Vector",eo,"Struct");
    const auto es=engine.ImportPackage("PhysicsSpoof"),actor=engine.Export("Actor",0,eo);engine.Native(actor);
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
    const auto physics=engine.Property("Physics","ByteProperty",actor,ec,enumeration,0x23u);
    const auto base=engine.Property("Base","ObjectProperty",actor,ec,actor,0x22u);
    const auto velocity=engine.Property("Velocity","StructProperty",actor,ec,ev,
        variant==PhysicsGeneratedVariant::VelocityFlags?0x20u:0x21u,
        variant==PhysicsGeneratedVariant::VelocityDimension?2u:1u);
    engine.Property("Acceleration","StructProperty",actor,ec,ev,0x20u);
    const auto setter=engine.Function("SetPhysics",actor,ec);
    const auto input=engine.Property("newPhysics","ByteProperty",setter,ec,enumeration,0x80u);
    const auto floor=engine.Property("newFloor","ObjectProperty",setter,ec,actor,0x90u);
    const auto signature=Join({Ref(0x29u,input),Ref(0x29u,floor)});
    Require(signature.logical==10u,"Generated original3970 packet is not ten logical bytes");
    engine.builders.at(static_cast<std::size_t>(setter-1))=[&engine,setter,signature](std::int32_t next,std::int32_t first) {
        Bytes b{0u};Index(b,0);Index(b,next);Index(b,0);Index(b,first);
        Index(b,engine.exports.at(static_cast<std::size_t>(setter-1)).ObjName);U32(b,0);U32(b,0);
        U32(b,static_cast<std::uint32_t>(signature.logical));Append(b,signature.raw);U16(b,3970u);b.push_back(0u);U32(b,0x401u);return b;
    };
    const auto spoofClass=[&](std::int32_t field,const char* name) {
        engine.exports.at(static_cast<std::size_t>(field-1)).ObjClass=engine.Import(name,es);
    };
    if(variant==PhysicsGeneratedVariant::FunctionClass)spoofClass(setter,"Function");
    if(variant==PhysicsGeneratedVariant::InputByteClass)spoofClass(input,"ByteProperty");
    if(variant==PhysicsGeneratedVariant::FloorObjectClass)spoofClass(floor,"ObjectProperty");
    if(variant==PhysicsGeneratedVariant::PhysicsClass)spoofClass(physics,"ByteProperty");
    if(variant==PhysicsGeneratedVariant::VelocityClass)spoofClass(velocity,"StructProperty");
    if(variant==PhysicsGeneratedVariant::PhysicsBase)
        engine.builders.at(static_cast<std::size_t>(physics-1))=[base,enumeration](std::int32_t next,std::int32_t) {
            Bytes b{0u};Index(b,base);Index(b,next);U32(b,1u);U32(b,0x23u);Index(b,0);U16(b,0u);Index(b,enumeration);return b;
        };
    // A real script wrapper calls the authored native, then fails in the SAME
    // interpreter/root after all seven stores. It is not a public nested call.
    Code five{{0x24u,5u},2u};
    const auto call=Join({Ref(0x1cu,setter),five,Ref(0x01u,base),Token(0x16u)});
    const auto fail=engine.Function("ClearThenFail",actor,ec);
    engine.FunctionBody(fail,Join({call,Code{{0x63u,0xe7u,0x16u},3u},Return()}));
    engine.ClassBody(actor);const auto et=fixture.Write(engine);
    Package map;map.stem="PhysicsGenerated";const auto me=map.ImportPackage("Engine"),ma=map.Import("Actor",me);
    const auto source=map.Export("Source",ma),support=map.Export("Support",ma),other=map.Export("Other",ma);
    Bytes tags;map.Tag(tags,"Physics",1u,Bytes{3u});Bytes pointer;Index(pointer,support);map.Tag(tags,"Base",5u,pointer);
    // Literal IEEE words only: do not turn signaling NaNs into arithmetic
    // operands while constructing this authored-byte fixture.
    Bytes v,a;for(const auto word:{0x80000000u,0x7fc12345u,0x7f800000u})U32(v,word);
    for(const auto word:{0xff800000u,0x7f812345u,0x00000001u})U32(a,word);
    map.Tag(tags,"Velocity",10u,v,"Vector");map.Tag(tags,"Acceleration",10u,a,"Vector");
    map.ActorBody(source,std::move(tags));map.ActorBody(support);map.ActorBody(other);
    return {{ct,st,et},fixture.Write(map,true)};
}

void PhysicsGeneratedCoverage() {
    const std::string actor="PhysicsGenerated.Source",support="PhysicsGenerated.Support",other="PhysicsGenerated.Other";
    const auto arguments=[&](std::uint8_t mode,const std::string& floor) {
        return std::vector<Evaluation>{{Value::Byte(mode),{}},{Value::Text(Kind::Object,floor),{}}};
    };
    const auto read=[&](const char* property) {
        return ReadPortableActorDeclaredScriptProperty(actor,std::string("Engine.Actor.")+property);
    };
    const auto refusesNormalVector=[&] {
        for(const auto* field:{"Velocity","Acceleration"}) {
            bool refused{};try{static_cast<void>(read(field));}catch(const std::exception&){refused=true;}
            Require(refused,"Raw native IEEE capability leaked into an ordinary declared Vector read");++refusals;
        }
    };
    std::size_t cleared{},cutRoots{},lateRoots{},schemaRoots{};
    for(const auto mode:{std::uint8_t{0u},std::uint8_t{5u}}) {
        Fixture fixture;const auto tables=BuildPhysics(fixture);
        Require(InitializePortableRuntime(tables.scripts).passed&&LoadPortableRuntimeMap(tables.map).passed,
            "Generated original3970 raw-tag fixture did not initialize");
        Require(read("Physics").kind==Kind::Byte&&read("Physics").integer==3&&
            read("Base").kind==Kind::Object&&read("Base").text==support,
            "Generated raw-tag fixture lost its literal Physics3/current actual Base");
        refusesNormalVector();GC::Collect();
        const auto before=fixture.Snapshot("RawPhysicsBefore");const auto revision=GetPortableRuntimeWorldRevision();
        const auto gc=GC::GetStats();const auto actors=GetPortableRuntimeMapActors(true);
        std::map<std::filesystem::path,std::filesystem::file_time_type> modified;
        for(const auto& [path,bytes]:fixture.sources){static_cast<void>(bytes);modified.emplace(path,std::filesystem::last_write_time(path));}
        const auto unchanged=[&] {
            GC::Collect();const auto current=GC::GetStats();
            Require(fixture.Snapshot("RawPhysicsAfter")==before&&GetPortableRuntimeWorldRevision()==revision&&
                !GetPortableRuntimeScriptStatePresent()&&!GetPortableRuntimeUnsavedScriptState()&&
                current.numObjects==gc.numObjects&&current.memoryUsage==gc.memoryUsage,
                "Raw3970 refusal/no-op retained partial vector words/Physics/save/revision/GC state");
            const auto world=GetPortableRuntimeMapActors(true);Require(world.size()==actors.size(),"Raw3970 changed generated Actor census");
            for(std::size_t i=0u;i<world.size();++i)Require(world[i].objectPath==actors[i].objectPath&&world[i].classPath==actors[i].classPath,
                "Raw3970 changed generated Actor slot/class identities");
            Require(read("Physics").integer==3&&read("Base").text==support,"Raw3970 undo lost original Physics/Base");
            refusesNormalVector();fixture.UnchangedSources();
            for(const auto& [path,time]:modified)Require(std::filesystem::last_write_time(path)==time,
                "Raw3970 changed generated source modification time");
        };
        const auto equal=ExecutePortableActorFunction(actor,"SetPhysics",arguments(3u,other));
        Require(equal.passed()&&equal.writes==0u,
            "Same Physics/different floor inspected inactive authored nonfinite vectors or performed a store: "+equal.error);
        unchanged();
        // There are seven actual native stores: Physics plus six raw DWORDs.
        // No save codec or generic VM Vector input is used to inject old NaNs.
        for(std::size_t limit=0u;limit<7u;++limit) {
            QuestVr::Vm::Limits limits;limits.writes=limit;
            const auto cut=ExecutePortableActorFunction(actor,"SetPhysics",arguments(mode,support),limits);
            Require(!cut.passed()&&!cut.committed&&cut.status==QuestVr::Vm::Status::Budget&&cut.writes==limit,
                "Generated raw3970 partial-word prefix escaped shared write admission: "+cut.error);
            ++refusals;++cutRoots;unchanged();
        }
        const auto late=ExecutePortableActorFunction(actor,"ClearThenFail");
        Require(!late.passed()&&!late.committed&&late.writes==7u&&late.error.find("999")!=std::string::npos,
            "Generated raw3970 late failure did not reach all seven original stores: "+late.error);
        ++refusals;++lateRoots;unchanged();
        QuestVr::Vm::Limits exact;exact.writes=7u;
        const auto result=ExecutePortableActorFunction(actor,"SetPhysics",arguments(mode,support),exact);
        Require(result.passed()&&result.committed&&result.writes==7u&&read("Physics").integer==mode&&read("Base").text==support,
            "Original3970 could not clear inactive authored IEEE words with its exact seven stores: "+result.error);
        for(const auto* field:{"Velocity","Acceleration"}) {
            const auto value=read(field);Require(value.kind==Kind::Vector,"Cleared raw3970 field is not Vector");
            for(const auto channel:value.vector)Require(std::bit_cast<std::uint32_t>(channel)==0u,
                "Original3970 did not overwrite every raw Vector channel with literal positive zero");
        }
        const auto saved=fixture.Snapshot("RawPhysicsCleared");
        Require(saved!=before&&GetPortableRuntimeWorldRevision()>revision&&GetPortableRuntimeUnsavedScriptState(),
            "Successful raw3970 clear did not publish its genuine finite overlay/checkpoint state");
        const auto same=ExecutePortableActorFunction(actor,"SetPhysics",arguments(mode,other));
        Require(same.passed()&&same.writes==0u&&fixture.Snapshot("RawPhysicsStillCleared")==saved,
            "Post-clear same Physics used a different floor or rewrote finite vectors");
        fixture.UnchangedSources();++cleared;
    }
    // Native-local schema validation, not a global rule based on leaf Names.
    // Each generated package loads through the same source table/metadata
    // machinery; the malformed declaring packet or reached Vector capability
    // must refuse without publishing a pending Physics write.
    for(const auto variant:{PhysicsGeneratedVariant::FunctionClass,PhysicsGeneratedVariant::InputByteClass,
        PhysicsGeneratedVariant::FloorObjectClass,PhysicsGeneratedVariant::PhysicsClass,PhysicsGeneratedVariant::PhysicsBase,
        PhysicsGeneratedVariant::VelocityClass,PhysicsGeneratedVariant::VelocityFlags,PhysicsGeneratedVariant::VelocityDimension,
        PhysicsGeneratedVariant::VectorOrder,PhysicsGeneratedVariant::VectorChannel,PhysicsGeneratedVariant::VectorOwner,
        PhysicsGeneratedVariant::VectorBase}) {
        Fixture fixture;const auto tables=BuildPhysics(fixture,variant);
        Require(InitializePortableRuntime(tables.scripts).passed&&LoadPortableRuntimeMap(tables.map).passed,
            "Generated spoof3970 fixture refused before reaching its native-local schema validation");
        const auto before=fixture.Snapshot("SpoofPhysicsBefore");const auto revision=GetPortableRuntimeWorldRevision();
        GC::Collect();const auto gc=GC::GetStats();
        const auto result=ExecutePortableActorFunction(actor,"SetPhysics",arguments(5u,support));
        const bool inputSchema=variant==PhysicsGeneratedVariant::FunctionClass||variant==PhysicsGeneratedVariant::InputByteClass||
            variant==PhysicsGeneratedVariant::FloorObjectClass||variant==PhysicsGeneratedVariant::PhysicsClass||
            variant==PhysicsGeneratedVariant::PhysicsBase;
        Require(!result.passed()&&!result.committed&&!result.error.empty()&&result.writes==(inputSchema?0u:1u),
            "Native3970 accepted qualified metadata spoof or missed its exact pending Physics prefix: "+result.error);
        GC::Collect();const auto current=GC::GetStats();
        Require(fixture.Snapshot("SpoofPhysicsAfter")==before&&GetPortableRuntimeWorldRevision()==revision&&
            !GetPortableRuntimeScriptStatePresent()&&!GetPortableRuntimeUnsavedScriptState()&&
            current.numObjects==gc.numObjects&&current.memoryUsage==gc.memoryUsage,
            "Native3970 schema refusal retained partial stores/save/revision/GC state");
        fixture.UnchangedSources();++refusals;++schemaRoots;
    }
    Require(cleared==2u&&cutRoots==14u&&lateRoots==2u&&schemaRoots==12u,
        "Generated raw3970 case count escaped its exact closed bound");
    std::cout<<"GENERATED original3970 raw storage: "<<cleared<<" exact7-store clears / "<<cutRoots
        <<" every-word budget cuts / "<<lateRoots<<" late-root rollbacks / "<<schemaRoots
        <<" qualified metadata/schema refusals; genuine authored signed-zero/NaN/Inf/subnormal words, "
        "ordinary Vector reads remain strict, same-mode/different floor unchanged, finite saved overlay only after native overwrite. "
        "No original binaries, forced support callbacks, physics phase or save-validation relaxation.\n";
}
