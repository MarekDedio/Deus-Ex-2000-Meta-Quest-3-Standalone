// Commercial-data-free production ExitProperties witness. Native StrProperty
// and StructProperty authority comes ONLY from the real registration phases,
// never a serialized same-name Class shim or manually installed native pointer.
namespace StructArrayTeardown {
using StructArrayGenerated::Package;
using StructArrayGenerated::RuntimeFixture;
using StructArrayGenerated::Tables;
using StructArrayGenerated::Value;
using StructArrayGenerated::Kind;
constexpr const char* Driver="StructArrayTeardown.Driver";
constexpr const char* RecordsProperty="StructArrayTeardownClasses.Probe.Records";

void TextStream(Bytes& bytes,const std::string& text) {
    Index(bytes,static_cast<std::int32_t>(text.size()+1u));
    bytes.insert(bytes.end(),text.begin(),text.end());bytes.push_back(0u);
}
std::string LeafText(std::size_t outer,std::size_t child,std::size_t text) {
    return "payload-"+std::to_string(outer)+std::to_string(child)+std::to_string(text);
}
Tables Build(RuntimeFixture& fixture) {
    // These are generated authored declarations, not replacements for original
    // metadata. The production phases still authenticate all their ordinary
    // prerequisites and allocate the audited closed native class domains.
    Package core;core.stem="Core";
    const auto object=core.Export("Object"),subsystem=core.Export("Subsystem",0,object);
    core.exports.at(static_cast<std::size_t>(object-1)).ObjFlags=static_cast<ObjectFlags>(0x04000000u);
    core.exports.at(static_cast<std::size_t>(subsystem-1)).ObjFlags=static_cast<ObjectFlags>(0x04000000u);
    core.ClassBody(object);core.ClassBody(subsystem);const auto ct=fixture.Write(core);
    Package engine;engine.stem="Engine";
    const auto ec=engine.ImportPackage("Core"),ep=engine.ImportPackage("Engine");
    const auto eo=engine.Import("Object",ec),es=engine.Import("Subsystem",ec);
    const auto actor=engine.Export("Actor",0,eo),baseEngine=engine.Export("Engine",0,es);
    for(const auto ref:{actor,baseEngine})
        engine.exports.at(static_cast<std::size_t>(ref-1)).ObjFlags=static_cast<ObjectFlags>(0x04000000u);
    engine.Property("Cylinder","ObjectProperty",baseEngine,ec,engine.Import("Primitive",ep));
    engine.ClassBody(actor);engine.ClassBody(baseEngine);const auto et=fixture.Write(engine);
    Package classes;classes.stem="StructArrayTeardownClasses";
    const auto cc=classes.ImportPackage("Core"),ce=classes.ImportPackage("Engine");
    const auto probe=classes.Export("Probe",0,classes.Import("Actor",ce));
    const auto leaf=classes.Export("Leaf",classes.Import("Struct",cc),0,probe);
    classes.Property("Text","StrProperty",leaf,cc,0,0x00400000u,2u);
    classes.Property("LeafSentinel","IntProperty",leaf,cc);
    const auto record=classes.Export("Record",classes.Import("Struct",cc),0,probe);
    classes.Property("Children","StructProperty",record,cc,leaf,0x00400000u,2u);
    classes.Property("RecordSentinel","IntProperty",record,cc);
    classes.Property("Records","StructProperty",probe,cc,record,0x00400000u,2u);
    classes.Property("ActorSentinel","IntProperty",probe,cc);
    // InitializePortableRuntime's production acceptance requires actual
    // normalized script bytes. Keep a real harmless function declaration;
    // native teardown itself does not execute or substitute this script.
    const auto witness=classes.Function("FixtureWitness",probe,cc);
    classes.FunctionBody(witness,StructArrayGenerated::Return());
    classes.ClassBody(probe);classes.StructBody(leaf);classes.StructBody(record);const auto st=fixture.Write(classes);
    Package map;map.stem="StructArrayTeardown";
    const auto mc=map.ImportPackage("StructArrayTeardownClasses"),driver=map.Export("Driver",map.Import("Probe",mc));
    Bytes tags;
    for(std::uint32_t outer=0u;outer<2u;++outer) {
        Bytes raw;
        for(std::uint32_t child=0u;child<2u;++child) {
            for(std::uint32_t text=0u;text<2u;++text)TextStream(raw,LeafText(outer,child,text));
            U32(raw,100u+10u*outer+child);
        }
        U32(raw,500u+outer);map.Tag(tags,"Records",10u,raw,"Record",outer);
    }
    Bytes sentinel;U32(sentinel,97531u);map.Tag(tags,"ActorSentinel",2u,sentinel);map.ActorBody(driver,tags);
    const std::string config="[Engine.Engine]\nGameEngine=Engine.GameEngine\n";
    const Bytes configBytes(config.begin(),config.end());const auto ini=fixture.directory/"System"/"DeusEx.ini";
    fixture.Write(ini,configBytes);fixture.sources.emplace(ini,configBytes);
    return {{ct,et,st},fixture.Write(map,true)};
}
Value ExpectedRecord(std::size_t outer,std::size_t cleared) {
    Value children;children.kind=Kind::Struct;
    for(std::size_t child=0u;child<2u;++child) {
        Value texts;texts.kind=Kind::Struct;
        for(std::size_t text=0u;text<2u;++text) {
            // Independent literal address order: root element, child element,
            // then native StrProperty's own two ascending elements.
            const auto ordinal=4u*outer+2u*child+text;
            texts.fields.emplace(std::to_string(text),Value::Text(Kind::String,ordinal<cleared?std::string{}:LeafText(outer,child,text)));
        }
        Value leaf;leaf.kind=Kind::Struct;
        leaf.fields={{"text",std::move(texts)},{"leafsentinel",Value::Integer(static_cast<std::int32_t>(100u+10u*outer+child))}};
        children.fields.emplace(std::to_string(child),std::move(leaf));
    }
    Value record;record.kind=Kind::Struct;
    record.fields={{"children",std::move(children)},{"recordsentinel",Value::Integer(static_cast<std::int32_t>(500u+outer))}};
    return record;
}
void Values(const PortablePropertyTeardownProbeResult& result,std::size_t cleared) {
    Require(result.values.size()==2u,"Generated ConstructorLink did not expose exactly Records[2]");
    for(std::size_t outer=0u;outer<2u;++outer) {
        const auto& row=result.values[outer];
        Require(row.propertyPath==RecordsProperty&&row.arrayIndex==outer,
            "Nested teardown observation changed its actual root field/outer slot");
        Require(QuestVr::Vm::Equal(row.before,ExpectedRecord(outer,0u)),
            "Nested StrProperty fixture did not start with independent nonempty authored slots");
        Require(QuestVr::Vm::Equal(row.after,ExpectedRecord(outer,cleared)),
            "Nested native destruction cleared wrong slot/order, aliased records or changed a nonowning sentinel");
    }
}
void Run() {
    const auto beforeChecks=checks,beforeRejections=rejections;
    RuntimeFixture fixture;const auto tables=StructArrayTeardown::Build(fixture);
    const auto initialized=InitializePortableRuntime(tables.scripts);
    Require(initialized.passed,"Cannot initialize generated nested teardown declarations: objects="+
        std::to_string(initialized.objects)+" classes="+std::to_string(initialized.classes)+
        " functions="+std::to_string(initialized.functions)+" properties="+std::to_string(initialized.properties)+
        " bytecode="+std::to_string(initialized.normalizedBytecodeBytes));
    const auto engine=PreparePortableRuntimeNativeEngineClasses();
    Require(engine.prepared&&engine.classes.size()==5u,"Nested teardown lacks its real native Engine registration phase");
    const auto metadata=PreparePortableRuntimeNativeMetadataClasses();
    Require(metadata.prepared&&metadata.classes.size()==13u&&metadata.classes[9u].classPath=="Core.StrProperty"&&
        metadata.classes[11u].classPath=="Core.StructProperty"&&metadata.classes[11u].nativeSize==0x64u,
        "Nested teardown lacks the actual thirteen-class native metadata registration");
    const auto stringField=ReadPortableRuntimeObjectOwnership("StructArrayTeardownClasses.Probe.Leaf.Text");
    const auto structField=ReadPortableRuntimeObjectOwnership(RecordsProperty);
    Require(stringField.classPath=="Core.StrProperty"&&stringField.classIndex==metadata.classes[9u].objectIndex&&
        structField.classPath=="Core.StructProperty"&&structField.classIndex==metadata.classes[11u].objectIndex&&
        stringField.classIndex!=0u&&structField.classIndex!=0u,
        "Constructor fields are not bound to the actual registered native class index leases");
    Require(LoadPortableRuntimeMap(tables.map).passed,"Cannot load generated nested teardown actor");
    const auto checkLive=[&] {
        for(std::uint32_t outer=0u;outer<2u;++outer)
            Require(QuestVr::Vm::Equal(ReadPortableActorDeclaredScriptProperty(Driver,RecordsProperty,outer),ExpectedRecord(outer,0u)),
                "Rolled-back native teardown retained a nested string slot mutation");
        Require(ReadPortableActorDeclaredScriptProperty(Driver,"StructArrayTeardownClasses.Probe.ActorSentinel").integer==97531,
            "Native teardown changed nonconstructor actor storage");
    };
    checkLive();const auto baseline=fixture.Snapshot("TeardownBefore");const auto revision=GetPortableRuntimeWorldRevision();
    const auto ownership=ReadPortableRuntimeObjectOwnership(Driver);GC::Collect();const auto gc=GC::GetStats();
    const auto unchanged=[&] {
        checkLive();Require(fixture.Snapshot("TeardownAfter")==baseline&&GetPortableRuntimeWorldRevision()==revision,
            "Nested native teardown changed complete checkpoint or world revision");
        const auto current=ReadPortableRuntimeObjectOwnership(Driver);
        Require(current.objectIndex==ownership.objectIndex&&current.classIndex==ownership.classIndex&&
            current.outerIndex==ownership.outerIndex&&current.objectPath==ownership.objectPath&&
            current.classPath==ownership.classPath&&current.outerPath==ownership.outerPath,
            "Nested native teardown changed actual Actor/Class/Outer identity");
        GC::Collect();const auto after=GC::GetStats();
        Require(after.numObjects==gc.numObjects&&after.memoryUsage==gc.memoryUsage,
            "Nested native teardown leaked native objects or rollback roots");
    };
    const auto full=ProbePortableActorPropertyTeardown(Driver);
    Require(full.execution.passed()&&!full.execution.committed&&full.completed&&full.execution.writes==8u,
        "Real nested native teardown did not execute eight leaf stores: "+full.execution.error);
    Values(full,8u);unchanged();
    for(std::size_t prefix=0u;prefix<8u;++prefix) {
        QuestVr::Vm::Limits limits;limits.writes=prefix;const auto cut=ProbePortableActorPropertyTeardown(Driver,limits);
        Require(!cut.execution.passed()&&!cut.execution.committed&&!cut.completed&&
            cut.execution.status==QuestVr::Vm::Status::Budget&&cut.execution.writes==prefix,
            "Nested native destruction bypassed its exact caller write prefix: "+cut.execution.error);
        Values(cut,prefix);unchanged();++rejections;
    }
    const auto warm=ProbePortableActorPropertyTeardown(Driver);
    Require(warm.execution.passed()&&!warm.execution.committed&&warm.completed&&warm.execution.writes==8u&&
        warm.execution.nativeWork&&warm.execution.nativeWork->used>1u,
        "Warm nested native destruction lost original native work or eight-store behavior");
    Values(warm,8u);unchanged();
    for(const auto prefix:{std::size_t{0u},warm.execution.nativeWork->used/2u,warm.execution.nativeWork->used-1u}) {
        QuestVr::Vm::Limits limits;limits.nativeWork=prefix;const auto cut=ProbePortableActorPropertyTeardown(Driver,limits);
        Require(!cut.execution.passed()&&!cut.execution.committed&&!cut.completed&&cut.execution.nativeWork&&
            cut.execution.nativeWork->exhausted,"Nested native teardown escaped the original caller's work allowance");
        unchanged();++rejections;
    }
    fixture.UnchangedSources();
    std::cout<<"PASS generated native nested StrProperty teardown: "<<checks-beforeChecks<<" checks, "<<rejections-beforeRejections
        <<" refusal controls; actual registered metadata Classes, Records[2]/Children[2]/Text[2], eight leaf clears, all write prefixes, native work and full checkpoint/revision/GC undo.\n";
}
} // namespace StructArrayTeardown
