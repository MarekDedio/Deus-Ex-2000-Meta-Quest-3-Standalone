// Actual first Training only. This helper owns its baseline because the final
// positive control intentionally commits original FindTriggerActor writes.
// Restoring its exact save also publishes a revision; neither is concealed by
// resetting a revision/counter or by forcing begun play, roles or a native hash.
void NativeWorkCoverage(const PortablePackageTables& map,Temporary& temporary) {
    const std::string info="00_Training.LevelInfo0",mover="00_Training.DeusExMover16";
    const auto order=ReadPortableLevel68ActorOrder(map);const auto count=order.size();
    Require(count>1u&&count<QuestVr::NativeWork::DefaultLimit,
        "Native work fixture requires the bounded original first-Training native Actor array");
    const auto actors=GetPortableRuntimeMapActors(true);
    std::size_t occupied{};
    for(const auto reference:order)if(reference)++occupied;
    Require(occupied==actors.size(),"Native work source slots differ from actual dormant Training actors");
    Require(Declared(info,"Engine.Actor.Base",Kind::Object).text.empty()&&
        !Declared(info,"Engine.LevelInfo.bBegunPlay",Kind::Bool).boolean&&
        !ReadPortableRuntimeRetailCollision().initialized,
        "Native work controls require genuine unbased LevelInfo/dormant/no-hash state");
    const auto before=temporary.Save("BeforeNativeWork");
    const auto checkpoint=temporary.Write("NativeWorkReset",before);
    const auto revision=GetPortableRuntimeWorldRevision();
    const bool present=GetPortableRuntimeScriptStatePresent(),dirty=GetPortableRuntimeUnsavedScriptState();
    const auto time=ReadPortableRuntimeLevelTime();
    const auto lookup=ReadPortableRuntimeObjectLookup();
    const auto native=ReadPortableRuntimeNativeModel();
    const auto session=ReadPortableRuntimePlayerSession(2802u);
    const auto ownership=ReadPortableRuntimeObjectOwnership(mover);
    const auto hash=ReadPortableRuntimeRetailCollision();
    GC::Collect();const auto gc=GC::GetStats();
    const auto stableIdentity=[&] {
        const auto current=ReadPortableRuntimeObjectLookup();
        const auto model=ReadPortableRuntimeNativeModel();
        const auto owner=ReadPortableRuntimeObjectOwnership(mover);
        const auto player=ReadPortableRuntimePlayerSession(2802u);
        const auto world=GetPortableRuntimeMapActors(true);
        Require(current.prepared==lookup.prepared&&current.generation==lookup.generation&&current.objects==lookup.objects&&
            model.mapGeneration==native.mapGeneration&&model.levelPath==native.levelPath&&model.modelPath==native.modelPath&&
            owner.objectIndex==ownership.objectIndex&&owner.classIndex==ownership.classIndex&&owner.outerIndex==ownership.outerIndex&&
            world.size()==actors.size(),"Native work control changed actual lookup/Actor/Level authority");
        for(std::size_t i=0u;i<actors.size();++i)
            Require(world[i].objectPath==actors[i].objectPath&&world[i].classPath==actors[i].classPath,
                "Native work control changed actual Actor slot/class order");
        const auto outside=ReadPortableRuntimeRetailCollision();
        Require(!outside.initialized&&!outside.enabled&&!outside.hashIdentity&&outside.actors.empty()&&
            outside.seed==hash.seed&&!outside.initializationDraws&&!outside.lifecycleOperations&&
            !Declared(info,"Engine.LevelInfo.bBegunPlay",Kind::Bool).boolean&&
            std::bit_cast<std::uint64_t>(ReadPortableRuntimeLevelTime())==std::bit_cast<std::uint64_t>(time),
            "Native work control initialized hash/RNG/work history or began/advanced the world");
        Require(HasPortableRuntimeClient()&&player.clientIdentity==session.clientIdentity&&
            player.viewportIdentity==session.viewportIdentity&&player.clientPath==session.clientPath&&
            player.viewportPath==session.viewportPath&&player.playerPath==session.playerPath&&player.bound==session.bound,
            "Native work control changed real client/viewport/possession identity");
        GC::Collect();SameGc(gc,"Native work control leaked rooted runtime objects");
    };
    const auto unchanged=[&] {
        stableIdentity();
        Require(temporary.Save("AfterNativeWorkRefusal")==before&&GetPortableRuntimeWorldRevision()==revision&&
            GetPortableRuntimeScriptStatePresent()==present&&GetPortableRuntimeUnsavedScriptState()==dirty,
            "Native work diagnostic/refusal changed complete save/clock/RNG/state/revision");
    };
    const auto usage=[](const QuestVr::Vm::Result& result,std::size_t used,std::size_t limit,bool exhausted) {
        Require(result.nativeWork.has_value()&&result.nativeWork->used==used&&result.nativeWork->limit==limit&&
            result.nativeWork->exhausted==exhausted,"Actual result lost its exact final shared native-work usage");
    };
    const auto based=[&](bool self){Command c;c.operation=Operation::IsBasedOn;c.actorPath=info;if(self)c.otherActorPath=info;return c;};
    const std::vector<Command> predicates{based(true),based(true),based(false)};
    QuestVr::Vm::Limits three;three.nativeWork=3u;
    const auto shortBatch=ProbePortableRuntimeRetailCollision(predicates,three);
    Require(!shortBatch.available&&!shortBatch.execution.passed()&&!shortBatch.execution.committed&&
        !shortBatch.execution.error.empty()&&shortBatch.predicates.size()==2u&&shortBatch.states.size()==2u&&
        shortBatch.execution.writes==0u,"Shared native budget3 failed before/after the genuine1+1+2 predicate prefix");
    usage(shortBatch.execution,3u,3u,true);++refusals;unchanged();
    QuestVr::Vm::Limits four;four.nativeWork=4u;
    const auto batch=ProbePortableRuntimeRetailCollision(predicates,four);
    Require(batch.available&&batch.execution.passed()&&!batch.execution.committed&&batch.predicates.size()==3u&&
        batch.states.size()==3u&&batch.predicates[0u].commandIndex==0u&&batch.predicates[0u].returned&&
        batch.predicates[1u].commandIndex==1u&&batch.predicates[1u].returned&&
        batch.predicates[2u].commandIndex==2u&&!batch.predicates[2u].returned&&batch.execution.writes==0u,
        "Shared native budget4 did not complete exact1+1+2 original predicates");
    usage(batch.execution,4u,4u,false);unchanged();
    const auto ordinary=ProbePortableRuntimeRetailCollision(predicates);
    Require(ordinary.available&&ordinary.execution.passed()&&!ordinary.execution.committed&&ordinary.predicates.size()==3u,
        "Default native-work predicate control failed");
    usage(ordinary.execution,4u,QuestVr::NativeWork::DefaultLimit,false);unchanged();
    QuestVr::Vm::Limits zero;zero.nativeWork=0u;
    const auto absent=ProbePortableRuntimeRetailCollision({Command{Operation::Disable}},zero);
    Require(absent.available&&absent.execution.passed()&&!absent.execution.committed&&absent.states.size()==1u&&
        !absent.states.front().initialized,"Zero-work absent-hash Disable initialized or charged a native operation");
    usage(absent.execution,0u,0u,false);unchanged();
    const auto denied=ProbePortableRuntimeRetailCollision({based(true)},zero);
    Require(!denied.available&&!denied.execution.passed()&&!denied.execution.committed&&denied.predicates.empty()&&
        denied.execution.writes==0u,"Zero-work predicate root did not refuse before its first genuine read");
    usage(denied.execution,0u,0u,true);++refusals;unchanged();

    // Predict the actual original script's selected trigger pointers and Bool
    // from immutable native slot order plus declared current fields/ancestry.
    // No production IsA/cast/iterator helper supplies the expected answers.
    std::vector<std::pair<std::string,std::vector<std::string>>> ancestry;
    const auto isA=[&](const std::string& path,const char* wanted) {
        const auto row=std::find_if(actors.begin(),actors.end(),[&](const auto& a){return a.objectPath==path;});
        Require(row!=actors.end(),"Native work expected trigger is outside actual first Training");
        auto found=std::find_if(ancestry.begin(),ancestry.end(),[&](const auto& a){return a.first==row->classPath;});
        if(found==ancestry.end()){
            std::vector<std::string> chain;auto cls=row->classPath;
            while(!cls.empty()){
                Require(chain.size()<128u&&std::find(chain.begin(),chain.end(),cls)==chain.end(),
                    "Native work expected ancestry is cyclic/excessive");chain.push_back(cls);
                const auto declared=ReadPortableRuntimeDispatchClass(cls);
                Require(declared.path==cls,"Native work expected ancestry changed actual class identity");cls=declared.parentPath;
            }
            ancestry.push_back({row->classPath,std::move(chain)});found=ancestry.end()-1;
        }
        return std::any_of(found->second.begin(),found->second.end(),[&](const auto& cls){
            return Fold(cls.substr(cls.find_last_of('.')+1u))==Fold(wanted);});
    };
    const auto name=[](const std::string& value){const auto folded=Fold(value);return folded.empty()?std::string("none"):folded;};
    const auto tag=name(Declared(mover,"Engine.Actor.Tag",Kind::Name).text);
    std::string first,second;std::size_t selectedWrites{};
    for(const auto reference:order){
        if(!reference)continue;
        Require(reference>0,"Native work source Actor slot imports an unsupported foreign actor");
        const auto path=std::string("00_Training.")+GetPortableObjectPath(map,reference);
        if(name(Declared(path,"Engine.Actor.Event",Kind::Name).text)!=tag)continue;
        if(!isA(path,"Trigger")&&!isA(path,"Mover"))continue;
        Require(!isA(path,"Counter")&&!isA(path,"Pawn"),
            "Actual FindTriggerActor fixture has an early-return actor and is not a complete-scan witness");
        if(first.empty()){first=path;++selectedWrites;}else if(second.empty()){second=path;++selectedWrites;}
    }
    bool playerOnly{};std::size_t finalWrites=1u;
    if(first.empty())playerOnly=Declared(mover,"Engine.Mover.BumpType",Kind::Byte).integer==0;
    else {
        playerOnly=isA(first,"Trigger")&&Declared(first,"Engine.Trigger.TriggerType",Kind::Byte).integer==0;
        if(playerOnly&&!second.empty()){
            ++finalWrites;
            // PC343 intentionally reads TriggerActor, not TriggerActor2. The
            // original script's apparent typo is part of this exact control.
            playerOnly=isA(second,"Trigger")&&Declared(first,"Engine.Trigger.TriggerType",Kind::Byte).integer==0;
            if(!playerOnly){std::swap(first,second);finalWrites+=3u;}
        }
    }
    const auto expectedWrites=occupied+1u+2u+selectedWrites+finalWrites;
    QuestVr::Vm::Limits shortScan;shortScan.instructions=1'000'000u;shortScan.writes=65'536u;shortScan.nativeWork=count-1u;
    const auto refused=ExecutePortableActorFunction(mover,"Engine.Mover.FindTriggerActor",{},shortScan);
    Require(!refused.passed()&&!refused.committed&&!refused.error.empty()&&refused.instructions>0u&&refused.writes>2u,
        "N-1 native work did not execute/refuse the actual original full-scan FindTriggerActor");
    usage(refused,count-1u,count-1u,true);++refusals;unchanged();

    // LAST: genuine original script commits, then exact save restoration. A
    // monotonic publication revision is not rollback state and is not reset.
    auto enough=shortScan;enough.nativeWork=count;
    const auto success=ExecutePortableActorFunction(mover,"Engine.Mover.FindTriggerActor",{},enough);
    Require(success.passed()&&success.committed&&success.instructions>0u&&success.writes==expectedWrites&&
        success.value.kind==Kind::Nothing,"N native work did not complete exact original FindTriggerActor writes: "+success.error);
    usage(success,count,count,false);
    Require(GetPortableRuntimeWorldRevision()==revision+1u,
        "Successful actual FindTriggerActor did not publish exactly one committed revision");
    Require(Declared(mover,"Engine.Mover.TriggerActor",Kind::Object).text==first&&
        Declared(mover,"Engine.Mover.TriggerActor2",Kind::Object).text==second&&
        Declared(mover,"Engine.Mover.bPlayerOnly",Kind::Bool).boolean==playerOnly,
        "Actual FindTriggerActor result differs from independent source-order/ancestry/field prediction");
    stableIdentity();
    Require(LoadPortableRuntimeState(checkpoint.string()),"Cannot restore exact state after committed native-work control");
    Require(GetPortableRuntimeWorldRevision()==revision+2u,
        "Restoring committed native-work control has an unexpected publication revision");
    stableIdentity();
    Require(temporary.Save("RestoredNativeWork")==before&&GetPortableRuntimeScriptStatePresent()==present&&
        GetPortableRuntimeUnsavedScriptState()==dirty,"Native work control failed exact save/clock/state/RNG restoration");
    std::cout<<"FIRST TRAINING native work: original Actor-array slots="<<count<<" occupied="<<occupied
        <<"; actual FindTriggerActor N-1 refuses/N completes "<<expectedWrites<<" original writes; shared1+1+2 predicate budget3/4, "
        "default65536 and zero-work roots. Complete refusal rollback; final committed control restored exact save with two explicit "
        "publication revisions. No inner reset, forced startup/role, native hash or other-map integration.\n";
}
