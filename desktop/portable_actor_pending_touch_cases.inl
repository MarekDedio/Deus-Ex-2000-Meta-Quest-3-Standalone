// Generated production AActor::performPhysics epilogue coverage. Included in
// the coordinate fixture's anonymous namespace after Package/Fixture/Tables.
// These authored test packages contain no commercial bytes. Their LevelInfo
// is explicitly authored begun=true; no original world flags are changed.
// Physics0 and low16-zero RotationRate need no Level/Model/hash or movement.
enum class PendingTouchGeneratedCase { Ordinary,Replacement,SelfLink,SourceAlias,FreshNone,CallbackFailure };

Code PendingTouchContext(Code receiver,Code expression) {
    Require(expression.logical<=65535u,"Generated PendingTouch Context exceeds native skip width");
    auto code=Join({Token(0x19u),std::move(receiver)});
    U16(code.raw,static_cast<std::uint16_t>(expression.logical));code.raw.push_back(0u);code.logical+=3u;
    Append(code.raw,expression.raw);code.logical+=expression.logical;return code;
}
void PendingTouchEventBody(Package& package,std::int32_t function,Code code,std::uint32_t flags) {
    package.builders.at(static_cast<std::size_t>(function-1))=[&package,function,code=std::move(code),flags]
        (std::int32_t next,std::int32_t first) {
        Bytes body{0u};Index(body,0);Index(body,next);Index(body,0);Index(body,first);
        Index(body,package.exports.at(static_cast<std::size_t>(function-1)).ObjName);
        U32(body,0);U32(body,0);U32(body,static_cast<std::uint32_t>(code.logical));Append(body,code.raw);
        U16(body,0u);body.push_back(0u);U32(body,flags);return body;
    };
}
Tables BuildPendingTouch(Fixture& fixture,PendingTouchGeneratedCase which) {
    Package core;core.stem="Core";
    const auto cp=core.ImportPackage("Core"),object=core.Export("Object"),meta=core.Export("Class",0,object);
    const auto rotator=core.Export("Rotator",core.Import("Struct",cp),0,object);
    for(const auto* axis:{"Pitch","Yaw","Roll"})core.Property(axis,"IntProperty",rotator,cp,0,1u);
    core.ClassBody(object);core.ClassBody(meta);core.StructBody(rotator);const auto ct=fixture.Write(core);

    Package engine;engine.stem="Engine";
    const auto ec=engine.ImportPackage("Core"),eo=engine.Import("Object",ec),er=engine.Import("Rotator",eo,"Struct");
    const auto actor=engine.Export("Actor",0,eo),info=engine.Export("LevelInfo",0,actor);
    engine.Native(actor);engine.Native(info);
    const auto enumeration=engine.Export("EPhysics",engine.Import("Enum",ec),0,actor,true);
    std::vector<std::int32_t> physicsNames;
    for(const auto* name:{"PHYS_None","PHYS_Walking","PHYS_Falling","PHYS_Swimming","PHYS_Flying",
        "PHYS_Rotating","PHYS_Projectile","PHYS_Rolling","PHYS_Interpolating","PHYS_MovingBrush","PHYS_Spider","PHYS_Trailer"})
        physicsNames.push_back(engine.Name(name));
    engine.builders.at(static_cast<std::size_t>(enumeration-1))=[physicsNames=std::move(physicsNames)]
        (std::int32_t next,std::int32_t) {
        Bytes body{0u};Index(body,0);Index(body,next);Index(body,static_cast<std::int32_t>(physicsNames.size()));
        for(const auto name:physicsNames)Index(body,name);
        return body;
    };
    engine.Property("Physics","ByteProperty",actor,ec,enumeration,0x23u);
    engine.Property("RotationRate","StructProperty",actor,ec,er,0x21u);
    engine.Property("PendingTouch","ObjectProperty",actor,ec,actor,0u);
    engine.Property("Level","ObjectProperty",actor,ec,info,0x2u);
    engine.Property("bDeleteMe","BoolProperty",actor,ec,0,0x2u);
    engine.Property("bBegunPlay","BoolProperty",info,ec);
    const auto post=engine.Function("PostTouch",actor,ec);
    engine.Property("Other","ObjectProperty",post,ec,actor,0x80u);
    PendingTouchEventBody(engine,post,Return(),0x800u);
    engine.ClassBody(actor);engine.ClassBody(info);const auto et=fixture.Write(engine);

    Package classes;classes.stem="PendingTouchClasses";
    const auto cc=classes.ImportPackage("Core"),ce=classes.ImportPackage("Engine"),ca=classes.Import("Actor",ce);
    const auto pending=classes.Import("PendingTouch",ca,"ObjectProperty");
    const auto handler=classes.Export("Handler",0,ca);
    const auto seen=classes.Property("Seen","ObjectProperty",handler,cc,ca);
    const auto calls=classes.Property("Calls","IntProperty",handler,cc);
    const auto replacement=classes.Property("Replacement","ObjectProperty",handler,cc,ca);
    const auto replacementNext=classes.Property("ReplacementNext","ObjectProperty",handler,cc,ca);
    const auto callback=classes.Function("PostTouch",handler,cc);
    const auto other=classes.Property("Other","ObjectProperty",callback,cc,ca,0x80u);
    const Code unknown{{0x63u,0xe7u,0x16u},3u}; // Deliberately unsupported native999, inside the selected callback.
    Code body=Join({Assign(Ref(0x01u,seen),Ref(0x00u,other)),Assign(Ref(0x01u,calls),Token(0x26u))});
    if(which==PendingTouchGeneratedCase::Replacement) {
        body=Join({body,
            Assign(PendingTouchContext(Ref(0x00u,other),Ref(0x01u,pending)),Ref(0x01u,replacement)),
            Assign(PendingTouchContext(Ref(0x01u,replacement),Ref(0x01u,pending)),Ref(0x01u,replacementNext))});
    }else if(which==PendingTouchGeneratedCase::FreshNone) {
        body=Join({body,Assign(PendingTouchContext(Ref(0x00u,other),Ref(0x01u,pending)),Token(0x2au))});
    }else if(which==PendingTouchGeneratedCase::CallbackFailure)body=Join({body,unknown});
    PendingTouchEventBody(classes,callback,Join({body,Return()}),2u); // Override need not retain FUNC_Event.
    for(const auto* name:{"SetPending","SetPendingThenFail"}) {
        const auto function=classes.Function(name,handler,cc);
        const auto input=classes.Property("Value","ObjectProperty",function,cc,ca,0x80u);
        Code setter=Assign(Ref(0x01u,pending),Ref(0x00u,input));
        if(std::string_view(name)=="SetPendingThenFail")setter=Join({setter,unknown});
        classes.FunctionBody(function,Join({setter,Return()}));
    }
    classes.ClassBody(handler);const auto st=fixture.Write(classes);

    Package map;map.stem="PendingTouchGenerated";
    const auto me=map.ImportPackage("Engine"),mc=map.ImportPackage("PendingTouchClasses");
    const auto ma=map.Import("Actor",me),mi=map.Import("LevelInfo",me),mh=map.Import("Handler",mc);
    const auto level=map.Export("Info",mi),source=map.Export("Source",mh),node=map.Export("Node",mh);
    const auto changed=map.Export("Replacement",mh),tail=map.Export("Tail",ma),end=map.Export("End",ma),second=map.Export("Second",ma);
    const auto pointer=[&](Bytes& tags,const char* name,std::int32_t target) {
        Bytes data;Index(data,target);map.Tag(tags,name,5u,data);
    };
    Bytes begun;pointer(begun,"Level",level);Index(begun,map.Name("bBegunPlay"));begun.push_back(0x83u);map.ActorBody(level,begun);
    Bytes sourceTags;pointer(sourceTags,"Level",level);
    pointer(sourceTags,"PendingTouch",which==PendingTouchGeneratedCase::SourceAlias?source:node);
    // High16 alone must NOT call physicsRotation or require native movement.
    map.RotationTag(sourceTags,"RotationRate",{65536,-65536,std::bit_cast<std::int32_t>(0x80000000u)});map.ActorBody(source,sourceTags);
    Bytes nodeTags;pointer(nodeTags,"Level",level);
    pointer(nodeTags,"PendingTouch",which==PendingTouchGeneratedCase::SelfLink?node:
        which==PendingTouchGeneratedCase::Replacement?end:tail);
    pointer(nodeTags,"Replacement",changed);pointer(nodeTags,"ReplacementNext",tail);map.ActorBody(node,nodeTags);
    Bytes changedTags;pointer(changedTags,"Level",level);
    if(which==PendingTouchGeneratedCase::Replacement)pointer(changedTags,"PendingTouch",end);
    map.ActorBody(changed,changedTags);
    Bytes tailTags;pointer(tailTags,"Level",level);pointer(tailTags,"PendingTouch",end);map.ActorBody(tail,tailTags);
    Bytes endTags;pointer(endTags,"Level",level);map.ActorBody(end,endTags);
    Bytes secondTags;pointer(secondTags,"Level",level);map.Tag(secondTags,"Physics",1u,Bytes{6u});map.ActorBody(second,secondTags);
    return {{ct,et,st},fixture.Write(map,true)};
}
std::string PendingTouchPath(std::string_view actor) {return "PendingTouchGenerated."+std::string(actor);}
using PendingTouchGraph=std::vector<std::pair<std::string,std::string>>;
PendingTouchGraph PendingTouchOriginalGraph(PendingTouchGeneratedCase which) {
    const auto p=[](std::string_view name){return PendingTouchPath(name);};
    return {{p("Info"),{}},{p("Source"),p(which==PendingTouchGeneratedCase::SourceAlias?"Source":"Node")},
        {p("Node"),p(which==PendingTouchGeneratedCase::SelfLink?"Node":which==PendingTouchGeneratedCase::Replacement?"End":"Tail")},
        {p("Replacement"),which==PendingTouchGeneratedCase::Replacement?p("End"):std::string{}},
        {p("Tail"),p("End")},{p("End"),{}},{p("Second"),{}}};
}
void PendingTouchSetEdge(PendingTouchGraph& graph,std::string_view source,std::string_view target) {
    const auto actor=PendingTouchPath(source);const auto found=std::find_if(graph.begin(),graph.end(),[&](const auto& row){return row.first==actor;});
    Require(found!=graph.end(),"Generated PendingTouch expected edge has no Actor");
    found->second=target.empty()?std::string{}:PendingTouchPath(target);
}
void PendingTouchSameGraph(PendingTouchGraph actual,PendingTouchGraph expected,const std::string& reason) {
    std::sort(actual.begin(),actual.end());std::sort(expected.begin(),expected.end());
    Require(actual==expected,reason);
}
void PendingTouchGeneratedCoverage() {
    const auto source=PendingTouchPath("Source"),node=PendingTouchPath("Node"),changed=PendingTouchPath("Replacement");
    const auto declared=[](const std::string& actor,const char* property) {
        return ReadPortableActorDeclaredScriptProperty(actor,std::string("Engine.Actor.")+property);
    };
    std::size_t complete{},writeCuts{},lateFailures{},callbackFailures{},freshFailures{};
    for(const auto which:{PendingTouchGeneratedCase::Ordinary,PendingTouchGeneratedCase::Replacement,
        PendingTouchGeneratedCase::SelfLink,PendingTouchGeneratedCase::SourceAlias,
        PendingTouchGeneratedCase::FreshNone,PendingTouchGeneratedCase::CallbackFailure}) {
        Fixture fixture;const auto tables=BuildPendingTouch(fixture,which);
        Require(InitializePortableRuntime(tables.scripts).passed&&LoadPortableRuntimeMap(tables.map).passed,
            "Cannot initialize generated production PendingTouch graph");
        Require(ResolvePortableActorFunction(which==PendingTouchGeneratedCase::SourceAlias?source:node,"None","PostTouch")==
            "PendingTouchClasses.Handler.PostTouch","PendingTouch fixture did not select its real script-class override");
        const auto before=fixture.Snapshot("PendingTouchBefore");const auto revision=GetPortableRuntimeWorldRevision();
        GC::Collect();const auto gc=GC::GetStats();
        const auto unchanged=[&] {
            GC::Collect();const auto current=GC::GetStats();
            Require(fixture.Snapshot("PendingTouchAfter")==before&&GetPortableRuntimeWorldRevision()==revision&&
                current.numObjects==gc.numObjects&&current.memoryUsage==gc.memoryUsage&&!GetPortableRuntimeUnsavedScriptState(),
                "PendingTouch probe leaked native/overlay/callback/save/revision/GC state");
            const auto graph=PendingTouchOriginalGraph(which);
            for(const auto& edge:graph)Require(declared(edge.first,"PendingTouch").text==edge.second,
                "PendingTouch rollback restored its overlay but not the actual native Object edge");
            for(const auto& path:{source,node,changed}) {
                Require(ReadPortableActorScriptProperty(path,"Calls").integer==0&&ReadPortableActorScriptProperty(path,"Seen").text.empty(),
                    "PendingTouch rollback retained selected callback fields");
            }
            fixture.UnchangedSources();
        };
        const auto full=ProbePortableActorNativePhysics({source},0.125f);
        Require(!full.execution.committed&&full.actors.size()==1u&&full.actors[0u].actorPath==source&&full.actors[0u].entered,
            "Production PendingTouch diagnostic did not enter exactly its original Actor root");
        PendingTouchSameGraph(full.actors[0u].pendingBefore,PendingTouchOriginalGraph(which),
            "PendingTouch diagnostic did not capture every original map Actor edge before dispatch");
        if(which==PendingTouchGeneratedCase::FreshNone||which==PendingTouchGeneratedCase::CallbackFailure) {
            const bool fresh=which==PendingTouchGeneratedCase::FreshNone;
            Require(!full.execution.passed()&&!full.actors[0u].returned&&full.execution.writes==(fresh?3u:2u)&&
                full.execution.error.find(fresh?"PendingTouch":"999")!=std::string::npos,
                "Fresh-None/throwing selected PostTouch was hidden or reached the native dequeue stores: "+full.execution.error);
            if(fresh)++freshFailures;else ++callbackFailures;++refusals;unchanged();
            for(std::size_t prefix=0u;prefix<(fresh?3u:2u);++prefix) {
                QuestVr::Vm::Limits limits;limits.writes=prefix;
                const auto cut=ProbePortableActorNativePhysics({source},0.125f,limits);
                Require(!cut.execution.passed()&&!cut.execution.committed&&cut.execution.status==QuestVr::Vm::Status::Budget&&
                    cut.execution.writes==prefix,"Failing PostTouch callback bypassed its shared partial-write budget");
                unchanged();++writeCuts;++refusals;
            }
            continue;
        }
        const auto expectedWrites=which==PendingTouchGeneratedCase::Replacement?6u:4u;
        const auto& row=full.actors[0u];
        Require(full.execution.passed()&&row.returned&&full.execution.writes==expectedWrites&&
            row.fallingCalls==0u&&row.rotationCalls==0u&&row.rotationMoveCalls==0u&&row.endedRotationCalls==0u&&
            row.postTouchCalls==1u&&row.pendingWrites==2u,
            "Production PendingTouch epilogue skipped its selected callback, gated high16 rotation incorrectly or changed its two ordered stores: "+full.execution.error);
        auto after=PendingTouchOriginalGraph(which);
        if(which==PendingTouchGeneratedCase::SourceAlias)PendingTouchSetEdge(after,"Source",{});
        else {
            PendingTouchSetEdge(after,"Source",which==PendingTouchGeneratedCase::SelfLink?"Node":"Tail");
            PendingTouchSetEdge(after,which==PendingTouchGeneratedCase::Replacement?"Replacement":"Node",{});
        }
        PendingTouchSameGraph(row.pendingAfter,after,
            "PostTouch dequeued cached retained/old Next, drained more than one node or lost fresh callback replacement/self alias");
        unchanged();++complete;
        for(std::size_t prefix=0u;prefix<expectedWrites;++prefix) {
            QuestVr::Vm::Limits limits;limits.writes=prefix;
            const auto cut=ProbePortableActorNativePhysics({source},0.125f,limits);
            Require(!cut.execution.passed()&&!cut.execution.committed&&cut.execution.status==QuestVr::Vm::Status::Budget&&cut.execution.writes==prefix,
                "Production PendingTouch acquired a nested write ledger or accepted a partial callback/dequeue: "+cut.execution.error);
            unchanged();++writeCuts;++refusals;
        }
        const auto late=ProbePortableActorNativePhysics({source,PendingTouchPath("Second")},0.125f);
        Require(!late.execution.passed()&&!late.execution.committed&&late.execution.writes==expectedWrites&&
            late.actors.size()==2u&&late.actors[0u].returned&&late.actors[1u].entered&&!late.actors[1u].returned,
            "Later unsupported Physics6 failed to poison and roll back the completed earlier PendingTouch epilogue");
        unchanged();++lateFailures;++refusals;
        const auto warm=ProbePortableActorNativePhysics({source},0.125f);
        Require(warm.execution.passed()&&warm.execution.nativeWork&&warm.execution.nativeWork->used>3u,
            "PendingTouch native diagnostic has no shared caller work ledger");
        const auto work=warm.execution.nativeWork->used;
        for(const auto prefix:{std::size_t{},work/2u,work-1u}) {
            QuestVr::Vm::Limits limits;limits.nativeWork=prefix;
            const auto cut=ProbePortableActorNativePhysics({source},0.125f,limits);
            Require(!cut.execution.passed()&&!cut.execution.committed&&cut.execution.nativeWork&&cut.execution.nativeWork->exhausted,
                "PendingTouch ignored a shared native-work cutoff");
            unchanged();++refusals;
        }
        unchanged();
    }
    {
        // Ordinary script Let uses the same typed ownership field. Save/load
        // must restore both explicit pointers and omitted authored/default
        // pointers, never merge this session's native pointer with old bytes.
        Fixture fixture;const auto tables=BuildPendingTouch(fixture,PendingTouchGeneratedCase::Ordinary);
        Require(InitializePortableRuntime(tables.scripts).passed&&LoadPortableRuntimeMap(tables.map).passed,
            "Cannot initialize generated PendingTouch script/save fixture");
        const auto baseline=fixture.Snapshot("PendingTouchOmittedBaseline");
        const auto baselineFile=fixture.directory/"PendingTouchOmittedBaseline.sav";
        const auto set=[&](const std::string& actor,const std::string& target,const char* function="SetPending") {
            return ExecutePortableActorFunction(actor,function,{{Value::Text(Kind::Object,target),{}}});
        };
        const auto first=set(source,changed),second=set(changed,PendingTouchPath("Tail"));
        Require(first.passed()&&first.committed&&first.writes==1u&&second.passed()&&second.committed&&second.writes==1u&&
            declared(source,"PendingTouch").text==changed&&declared(changed,"PendingTouch").text==PendingTouchPath("Tail"),
            "Script PendingTouch Let did not synchronize its original native Object pointer");
        const auto modified=fixture.Snapshot("PendingTouchExplicitPointers");
        const auto modifiedFile=fixture.directory/"PendingTouchExplicitPointers.sav";
        const auto beforeLate=modified;const auto revision=GetPortableRuntimeWorldRevision();GC::Collect();const auto gc=GC::GetStats();
        const auto late=set(source,{},"SetPendingThenFail");
        Require(!late.passed()&&!late.committed&&late.writes==1u&&late.error.find("999")!=std::string::npos&&
            fixture.Snapshot("PendingTouchScriptUndo")==beforeLate&&GetPortableRuntimeWorldRevision()==revision&&
            declared(source,"PendingTouch").text==changed,
            "Late script failure retained a PendingTouch native/overlay assignment");++refusals;
        GC::Collect();Require(GC::GetStats().numObjects==gc.numObjects&&GC::GetStats().memoryUsage==gc.memoryUsage,
            "Script PendingTouch undo leaked an ownership root");
        Require(LoadPortableRuntimeState(baselineFile.string())&&fixture.Snapshot("PendingTouchResetOmitted")==baseline&&
            declared(source,"PendingTouch").text==node&&declared(changed,"PendingTouch").text.empty(),
            "Omitted checkpoint PendingTouch failed to reset to original nonempty/None defaults");
        Require(LoadPortableRuntimeState(modifiedFile.string())&&fixture.Snapshot("PendingTouchReloadExplicit")==modified&&
            declared(source,"PendingTouch").text==changed&&declared(changed,"PendingTouch").text==PendingTouchPath("Tail"),
            "Explicit checkpoint PendingTouch failed to rebind actual typed Object ownership");
        const auto probe=ProbePortableActorNativePhysics({source},0.125f);
        Require(probe.execution.passed()&&probe.actors.size()==1u&&probe.actors[0u].returned&&probe.actors[0u].postTouchCalls==1u,
            "Restored PendingTouch pointers are not usable by the production native epilogue");
        auto after=PendingTouchOriginalGraph(PendingTouchGeneratedCase::Ordinary);
        PendingTouchSetEdge(after,"Source","Tail");PendingTouchSetEdge(after,"Replacement",{});
        PendingTouchSameGraph(probe.actors[0u].pendingAfter,after,
            "Native epilogue used stale pre-checkpoint PendingTouch identities after explicit restore");
        Require(fixture.Snapshot("PendingTouchAfterReloadProbe")==modified,
            "Rollback-only epilogue changed restored script/native PendingTouch storage");
        GC::Collect();fixture.UnchangedSources();
    }
    Require(complete==4u&&writeCuts==23u&&lateFailures==4u&&freshFailures==1u&&callbackFailures==1u,
        "Generated PendingTouch production coverage escaped its exact closed case count");
    std::cout<<"GENERATED original performPhysics PendingTouch: actual selected PostTouch/Other packet, one-node dequeue, "
        "fresh replacement/self aliases, all23write cuts, later Physics6/callback/freshNone refusals and whole-root rollback; "
        "script Let plus omitted/explicit checkpoint pointer reset. Synthetic Physics0/no-rotation only; NOT world scheduling or headset acceptance.\n";
}
