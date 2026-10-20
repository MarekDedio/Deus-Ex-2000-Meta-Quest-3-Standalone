// Actual first Training ONLY. These are rollback-only original Let stores,
// not replacement setters, hash relocations, startup or playable crate tests.
QuestVr::OriginalCollisionHashRange RawShapeSourceCylinderRange(const Actor& actor,float radius,float height) {
    const auto exactInput=[](float value) {
        return std::isfinite(value)&&(value==0.0f||std::abs(value)>=1.0f/256.0f)&&std::abs(value)<1'048'576.0f;
    };
    Require(radius>=0.0f&&(radius==0.0f||radius>=1.0f/256.0f)&&radius<256.0f&&
        height>=0.0f&&(height==0.0f||height>=1.0f/256.0f)&&height<256.0f,
        "Raw shape source dimensions exceed the exact binary64 oracle cohort");
    for(const auto component:actor.location)Require(exactInput(component),
        "Raw shape source location exceeds the exact binary64 oracle cohort");
    // These binary32 operands have at most53 significant bits when aligned.
    // Binary64 is therefore exact until each specified binary32 spill. Unlike
    // the crate-only setter oracle, this permits the real key's fractional
    // dimensions and preserves the original unspilled Y radius+1 path.
    const double retainedRadius=double(radius)+1.0;
    const float storedRadius=static_cast<float>(retainedRadius);
    const float storedHeight=static_cast<float>(double(height)+1.0);
    const std::array<double,3u> extent{double(storedRadius),retainedRadius,double(storedHeight)};
    std::array<float,3u> minimum{},maximum{};
    for(std::size_t axis=0u;axis<3u;++axis) {
        minimum[axis]=static_cast<float>(double(actor.location[axis])-extent[axis]);
        maximum[axis]=static_cast<float>(double(actor.location[axis])+extent[axis]);
    }
    const auto actual=ReadPortableRuntimeActorPrimitiveBounds(actor.path);
    SameVector(actual.minimum,minimum,"Source cylinder independent minimum differs from actual primitive");
    SameVector(actual.maximum,maximum,"Source cylinder independent maximum differs from actual primitive");
    return QuestVr::OriginalCollisionHashBoxRange({Vector(minimum),Vector(maximum)});
}
void RawShapeCoverage(const std::vector<Actor>& actors,const std::function<void()>& rollback) {
    using Value=QuestVr::Vm::Value;
    const auto selected=std::find_if(actors.begin(),actors.end(),[](const auto& actor) {
        return actor.collide&&!actor.deleted&&(actor.path=="00_Training.CrateBreakableMedGeneral1"||
            actor.path=="00_Training.CrateBreakableMedGeneral2");
    });
    Require(selected!=actors.end(),"Raw shape controls lack the actual collidable Training crate pair");
    const auto target=static_cast<std::size_t>(selected-actors.begin());
    const auto originalMesh=Declared(selected->path,"Engine.Actor.Mesh",Kind::Object).text;
    const float originalScale=Declared(selected->path,"Engine.Actor.DrawScale",Kind::Float).floating;
    const auto originalPrimitive=ReadPortableRuntimeActorPrimitive(selected->path);
    const std::string doctor="00_Training.Doctor1";
    const auto otherMesh=Declared(doctor,"Engine.Actor.Mesh",Kind::Object).text;
    const auto otherPrimitive=ReadPortableRuntimeActorPrimitive(doctor);
    const auto engine=ReadPortableRuntimeEngineCollision();
    Require(!originalMesh.empty()&&!otherMesh.empty()&&originalMesh!=otherMesh&&engine.prepared&&
        !engine.cylinderPath.empty()&&originalPrimitive.primitivePath==originalMesh&&
        otherPrimitive.primitivePath==otherMesh&&
        (otherPrimitive.nativeClassPath=="Engine.Mesh"||otherPrimitive.nativeClassPath=="Engine.LodMesh")&&
        Declared(selected->path,"Engine.Actor.Brush",Kind::Object).text.empty()&&
        Declared(doctor,"Engine.Actor.Brush",Kind::Object).text.empty()&&std::isfinite(originalScale),
        "Raw shape fixtures lack two genuinely loaded Meshes and the Engine-owned Cylinder");
    const auto order=SetterOrderQuery(actors,target);
    const auto raw=[&](Operation operation,Value value) {
        Command command{operation};command.actorPath=selected->path;
        command.arguments.push_back(std::move(value));return command;
    };
    const auto mesh=[&](const std::string& path){return raw(Operation::RawMeshWrite,Value::Text(Kind::Object,path));};
    const auto scale=[&](float value){return raw(Operation::RawDrawScaleWrite,Value::Float(value));};
    Command setter{Operation::SetCollision};setter.actorPath=selected->path;setter.arguments={Value::Bool(true)};
    const float changedScale=originalScale==1.25f?0.75f:1.25f;
    struct Shape {std::size_t command;std::string mesh,primitive,nativeClass;float scale;};
    struct Frame {std::uint32_t tag;std::size_t links;std::vector<std::uint32_t> tags;};
    const auto sequence=[&](const std::vector<Command>& commands,bool requireSetterOrder) {
        Require(commands.size()<=16u,"Raw shape sequence exceeds the closed production command bound");
        Oracle expected(actors);std::vector<Shape> shapes;std::vector<Frame> frames;
        std::vector<std::pair<std::vector<PortableRetailCollisionHit>,std::size_t>> hits;
        std::string currentMesh=originalMesh;float currentScale=originalScale;
        std::size_t setterCount{},setterQuery=std::numeric_limits<std::size_t>::max();
        for(std::size_t i=0u;i<commands.size();++i) {
            const auto& command=commands[i];
            if(command.operation==Operation::RawMeshWrite)currentMesh=command.arguments.at(0u).text;
            else if(command.operation==Operation::RawDrawScaleWrite)currentScale=command.arguments.at(0u).floating;
            else if(command.operation==Operation::SetCollision) {
                expected.Remove(target);expected.Add(target);++setterCount;setterQuery=hits.size();
            }else if(command.operation==Operation::LineCheck)hits.push_back(expected.Query(command,actors));
            if(command.operation==Operation::RawMeshWrite||command.operation==Operation::RawDrawScaleWrite) {
                const bool cylinder=currentMesh.empty(),other=currentMesh==otherMesh;
                shapes.push_back({i,currentMesh,cylinder?engine.cylinderPath:currentMesh,
                    cylinder?"Engine.Primitive":other?otherPrimitive.nativeClassPath:originalPrimitive.nativeClassPath,currentScale});
            }
            frames.push_back({expected.tag,expected.links,expected.tags});
        }
        const auto result=ProbePortableRuntimeRetailCollision(commands);
        // Each direct native262 entry consumes its own VM Step even though
        // it has no interpreted script body. Raw WriteReference stores do not.
        Require(result.available&&result.execution.passed()&&!result.execution.committed&&
            result.execution.instructions==setterCount&&result.rawShapes.size()==shapes.size()&&
            result.states.size()==commands.size()&&result.queries.size()==hits.size()&&
            result.queryHeads.size()==hits.size()&&result.mutations.size()==setterCount,
            "Actual same-root Mesh/DrawScale sequence failed: "+result.execution.error+
                " instructions="+std::to_string(result.execution.instructions)+" expected-native-entries="+std::to_string(setterCount)+
                " shapes="+std::to_string(result.rawShapes.size())+" states="+std::to_string(result.states.size())+
                " queries="+std::to_string(result.queries.size())+" mutations="+std::to_string(result.mutations.size()));
        for(std::size_t i=0u;i<shapes.size();++i) {
            const auto& wanted=shapes[i];const auto& actual=result.rawShapes[i];
            Require(actual.commandIndex==wanted.command&&actual.actorPath==selected->path&&
                actual.meshPath==wanted.mesh&&actual.primitivePath==wanted.primitive&&
                actual.nativeClassPath==wanted.nativeClass&&actual.clockBefore==actual.clockAfter,
                "Raw shape store did not immediately select its real live Mesh/Cylinder or changed an unrelated clock");
            SameFloat(actual.drawScale,wanted.scale,"Raw DrawScale Let did not preserve the exact binary32 word");
        }
        for(std::size_t i=0u;i<frames.size();++i) {
            const auto& state=result.states[i];const auto& frame=frames[i];
            Require(state.initialized&&state.enabled&&state.hashIdentity==result.states.front().hashIdentity&&
                state.seed==expected.seed&&state.tag==frame.tag&&state.activeLinks==frame.links,
                "Raw Mesh/DrawScale store rebuilt links, changed query tags/RNG or lost the actual hash lease");
            SameActors(state,actors,frame.tags);
        }
        for(std::size_t i=0u;i<hits.size();++i)SameHits(result.queries[i],result.queryHeads[i],hits[i]);
        const auto encounter=[](const auto& rows) {
            std::vector<std::string> paths;for(const auto& row:rows)paths.push_back(row.actorPath);return paths;
        };
        Require(!result.queries.empty(),"Raw shape controls lost their same-root query witnesses");
        for(std::size_t i=1u;i<result.queries.size()&&i<setterQuery;++i)
            Require(encounter(result.queries[i])==encounter(result.queries.front()),
                "Raw store unexpectedly performed a setter's remove/prepend encounter-order change");
        if(requireSetterOrder)Require(setterCount==1u&&setterQuery<result.queries.size()&&
            encounter(result.queries.front())!=encounter(result.queries[setterQuery]),
            "Actual true-to-true SetCollision did not distinguish its remove/prepend from the preceding raw stores");
        rollback();
    };
    sequence({Command{Operation::Enable},order,mesh(originalMesh),order,scale(originalScale),order,
        mesh({}),order,mesh(otherMesh),order,scale(changedScale),order,setter,order},true);
    // The host adds no positive-scale gate. Retain exact admitted finite words,
    // including -0/subnormal/negative/max, without changing primitive geometry.
    // The common VM validator separately rejects nonfinite inputs BEFORE host
    // stores; original DLL raw DWORD copies do not establish VM admission here.
    const float nan=std::bit_cast<float>(std::uint32_t{0x7fc12345u});
    sequence({Command{Operation::Enable},scale(-0.0f),order,
        scale(std::numeric_limits<float>::denorm_min()),order,scale(-1.25f),order,
        scale(std::numeric_limits<float>::max()),order},false);
    const auto noHash=ProbePortableRuntimeRetailCollision({mesh({}),scale(changedScale)});
    Require(noHash.available&&noHash.execution.passed()&&!noHash.execution.committed&&
        noHash.execution.instructions==0u&&noHash.execution.writes==2u&&noHash.rawShapes.size()==2u&&noHash.states.size()==2u,
        "Original raw shape stores incorrectly require or construct an enabled native hash: "+noHash.execution.error);
    for(const auto& state:noHash.states)Require(!state.initialized&&!state.enabled&&!state.hashIdentity&&
        state.actors.empty()&&!state.initializationDraws&&state.seed==InitialSeed,
        "Hash-absent raw Mesh/DrawScale store initialized tables, links or the native RNG stream");
    Require(noHash.rawShapes[0u].meshPath.empty()&&noHash.rawShapes[0u].primitivePath==engine.cylinderPath&&
        noHash.rawShapes[1u].meshPath.empty()&&noHash.rawShapes[1u].primitivePath==engine.cylinderPath,
        "Hash-absent Mesh=None raw store did not select the real Engine-owned Cylinder");
    SameFloat(noHash.rawShapes[1u].drawScale,changedScale,"Hash-absent raw DrawScale did not store its actual scalar");rollback();
    const auto reject=[&](std::vector<Command> commands,const std::string& why) {
        const auto result=ProbePortableRuntimeRetailCollision(commands);
        Require(!result.available&&!result.execution.passed()&&!result.execution.committed&&!result.execution.error.empty(),
            "Raw Mesh/DrawScale admitted an unsupported scalar/receiver: "+why);++refusals;rollback();return result;
    };
    std::vector<Command> invalid{
        raw(Operation::RawMeshWrite,Value::Integer(1)),
        raw(Operation::RawMeshWrite,Value::Text(Kind::String,originalMesh)),
        mesh(doctor),mesh("Engine.Mesh"),mesh(engine.cylinderPath),mesh("00_Training.NoSuchMesh"),
        raw(Operation::RawDrawScaleWrite,Value::Text(Kind::Object,originalMesh))};
    auto missing=mesh(originalMesh);missing.actorPath="00_Training.NoSuchActor";invalid.push_back(missing);
    auto wrongReceiver=mesh(originalMesh);wrongReceiver.actorPath="Engine.Actor";invalid.push_back(wrongReceiver);
    auto absent=mesh(originalMesh);absent.actorPath.clear();invalid.push_back(absent);
    auto noArgument=mesh(originalMesh);noArgument.arguments.clear();invalid.push_back(noArgument);
    auto excess=mesh(originalMesh);excess.arguments.push_back(Value::Text(Kind::Object,otherMesh));invalid.push_back(excess);
    for(std::size_t i=0u;i<invalid.size();++i)
        reject({Command{Operation::Enable},invalid[i]},"type/class/count/receiver case "+std::to_string(i));
    for(const float value:{nan,std::numeric_limits<float>::infinity(),-std::numeric_limits<float>::infinity()}) {
        const auto nonfinite=reject({Command{Operation::Enable},mesh({}),scale(changedScale),order,scale(value)},
            "shared VM nonfinite admission after successful finite raw/query prefix");
        Require(nonfinite.execution.status==QuestVr::Vm::Status::Invalid&&
            nonfinite.execution.error=="Non-finite VM float"&&nonfinite.execution.writes==3u&&
            nonfinite.rawShapes.size()==2u&&nonfinite.states.size()==4u&&nonfinite.queries.size()==1u,
            "Common VM nonfinite refusal did not occur before the host field store");
    }
    const auto late=reject({Command{Operation::Enable},mesh({}),scale(changedScale),order,setter,order,
        Command{static_cast<Operation>(999)}},"late failure after changed raw fields, query and actual setter");
    Require(late.rawShapes.size()==2u&&late.states.size()==6u&&late.queries.size()==2u&&late.mutations.size()==1u&&
        late.rawShapes[0u].meshPath.empty()&&late.rawShapes[0u].primitivePath==engine.cylinderPath,
        "Late raw-shape refusal never reached its successful changed-field/query/setter prefix");
    SameFloat(late.rawShapes[1u].drawScale,changedScale,"Late-refused raw scale prefix did not actually change the field");

    // Execute the original authored Inventory body, including its raw Mesh and
    // DrawScale stores followed by its explicit native262. No forced state or
    // invented pickup is used to cross the authentic startup boundary.
    const std::string key="00_Training.NanoKey0";
    const auto keyActor=std::find_if(actors.begin(),actors.end(),[&](const auto& a){return a.path==key;});
    Require(keyActor!=actors.end()&&!keyActor->deleted,"Original BecomePickup lacks actual first Training NanoKey0");
    const auto keyIndex=static_cast<std::size_t>(keyActor-actors.begin());
    const auto pickupMesh=Declared(key,"Engine.Inventory.PickupViewMesh",Kind::Object).text;
    const auto pickupScale=Declared(key,"Engine.Inventory.PickupViewScale",Kind::Float).floating;
    const auto keyRadius=Declared(key,"Engine.Actor.CollisionRadius",Kind::Float).floating;
    const auto keyHeight=Declared(key,"Engine.Actor.CollisionHeight",Kind::Float).floating;
    Require(!pickupMesh.empty(),"Actual original NanoKey has no authored PickupViewMesh");
    Command keyQuery{Operation::LineCheck};keyQuery.start=keyQuery.end=keyActor->location;
    keyQuery.start[0u]-=96.0f;keyQuery.end[0u]+=96.0f;
    Command keyMesh{Operation::RawMeshWrite};keyMesh.actorPath=key;keyMesh.arguments={Value::Text(Kind::Object,{})};
    Command keyScale{Operation::RawDrawScaleWrite};keyScale.actorPath=key;keyScale.arguments={Value::Float(1.25f)};
    Command become{Operation::OriginalBecomePickup};become.actorPath=key;
    Oracle keyExpected(actors);const auto beforeKey=keyExpected.Query(keyQuery,actors);
    const auto beforeTags=keyExpected.tags;const auto beforeLinks=keyExpected.links;
    if(keyActor->collide)keyExpected.Remove(keyIndex);
    Require(Declared(key,"Engine.Actor.Brush",Kind::Object).text.empty(),
        "Original key registration cannot use the cylinder oracle for a selected Model");
    keyExpected.ranges[keyIndex]=RawShapeSourceCylinderRange(*keyActor,keyRadius,keyHeight);keyExpected.Add(keyIndex);
    auto keyModel=actors;keyModel[keyIndex].collide=true;keyModel[keyIndex].colLocation=keyModel[keyIndex].location;
    const auto afterSetterTags=keyExpected.tags;
    const auto afterKey=keyExpected.Query(keyQuery,keyModel);
    Require(std::any_of(afterKey.first.begin(),afterKey.first.end(),[&](const auto& hit){return hit.actorPath==key;}),
        "Original BecomePickup query fixture never encounters its actual key cylinder");
    const auto pickup=ProbePortableRuntimeRetailCollision({Command{Operation::Enable},keyMesh,keyScale,keyQuery,become,keyQuery});
    Require(pickup.available&&pickup.execution.passed()&&!pickup.execution.committed&&pickup.execution.instructions>0u&&
        pickup.execution.writes>2u&&pickup.rawShapes.size()==3u&&pickup.states.size()==6u&&pickup.queries.size()==2u&&
        pickup.queryHeads.size()==2u,
        "Actual original NanoKey0 Engine.Inventory.BecomePickup failed: "+pickup.execution.error);
    Require(pickup.rawShapes[0u].commandIndex==1u&&pickup.rawShapes[0u].meshPath.empty()&&
        pickup.rawShapes[0u].primitivePath==engine.cylinderPath&&pickup.rawShapes[1u].meshPath.empty(),
        "Original NanoKey BecomePickup was not preceded by its genuine same-root changed raw Mesh");
    SameFloat(pickup.rawShapes[1u].drawScale,1.25f,"Original NanoKey BecomePickup control did not first write a different raw scale");
    const auto& keyShape=pickup.rawShapes.back();
    Require(keyShape.commandIndex==4u&&keyShape.actorPath==key&&keyShape.meshPath==pickupMesh&&
        keyShape.primitivePath==pickupMesh&&(keyShape.nativeClassPath=="Engine.Mesh"||keyShape.nativeClassPath=="Engine.LodMesh")&&
        keyShape.clockBefore==keyShape.clockAfter,
        "Original BecomePickup did not complete its actual Mesh store and live primitive selection");
    SameFloat(keyShape.drawScale,pickupScale,"Original BecomePickup did not store authored PickupViewScale");
    Require(pickup.states[3u].activeLinks==beforeLinks&&pickup.states[4u].activeLinks==keyExpected.links&&
        pickup.states[5u].activeLinks==keyExpected.links&&pickup.states[3u].tag==1u&&
        pickup.states[4u].tag==1u&&pickup.states[5u].tag==2u&&
        pickup.states[5u].seed==keyExpected.seed&&
        pickup.states[5u].hashIdentity==pickup.states.front().hashIdentity,
        "Original BecomePickup failed its final native262 registration or changed unrelated hash RNG/tags");
    SameActors(pickup.states[3u],actors,beforeTags);SameActors(pickup.states[4u],keyModel,afterSetterTags);
    SameActors(pickup.states[5u],keyModel,keyExpected.tags);
    SameHits(pickup.queries[0u],pickup.queryHeads[0u],beforeKey);
    SameHits(pickup.queries[1u],pickup.queryHeads[1u],afterKey);rollback();
    const auto latePickup=reject({Command{Operation::Enable},become,keyQuery,Command{static_cast<Operation>(999)}},
        "late failure after complete original NanoKey BecomePickup");
    Require(latePickup.rawShapes.size()==1u&&latePickup.rawShapes.front().meshPath==pickupMesh&&
        latePickup.queries.size()==1u&&latePickup.execution.instructions>0u,
        "Late original BecomePickup refusal did not execute its genuine script/native/query prefix");
    std::cout<<"FIRST TRAINING raw Mesh/DrawScale: actual collidable crate unchanged/changed stores, None Cylinder and second loaded Mesh, "
        "exact finite/-0/subnormal words, shared VM NaN/Inf refusal, independent same-root hash/tag/hit/order oracle, only actual native262 re-prepends; "
        "typed Mesh/receiver refusals and full late rollback; actual NanoKey0 original BecomePickup completes through native262. "
        "Not begun startup or live crate/loot/gravity acceptance.\n";
}

void AnimatedRawShapeCoverage(Temporary& temporary) {
    using Value=QuestVr::Vm::Value;
    const std::string doctor="00_Training.Doctor1",crate="00_Training.CrateBreakableMedGeneral1";
    const auto before=temporary.Save("BeforeAnimatedRawShape");
    const auto checkpoint=temporary.Write("RawShapeAnimationReset",before);
    const auto waiting=ExecutePortableActorFunction(doctor,"PlayWaiting");
    Require(waiting.passed()&&waiting.committed&&waiting.instructions>0u,
        "Raw shape clock control failed actual Doctor1.PlayWaiting: "+waiting.error);
    const auto seeded=temporary.Save("AnimatedRawShapeSeed");const Envelope envelope(seeded);
    const auto saved=std::find_if(envelope.state.objects.begin(),envelope.state.objects.end(),[&](const auto& row){return row.path==doctor;});
    Require(saved!=envelope.state.objects.end()&&saved->clock.has_value()&&!saved->clock->pose.main.sequence.empty(),
        "Actual raw-shape animation control lacks its genuine native animation clock");
    QuestVr::ScriptSavedState expected;expected.mapName="00_Training";
    QuestVr::ScriptSavedObject object;object.path=doctor;object.classPath=saved->classPath;object.clock=saved->clock;
    expected.objects.push_back(object);const auto clock=QuestVr::EncodeScriptSavedState(expected);
    const auto originalMesh=Declared(doctor,"Engine.Actor.Mesh",Kind::Object).text;
    const auto otherMesh=Declared(crate,"Engine.Actor.Mesh",Kind::Object).text;
    const float originalScale=Declared(doctor,"Engine.Actor.DrawScale",Kind::Float).floating;
    Require(!originalMesh.empty()&&!otherMesh.empty()&&originalMesh!=otherMesh,
        "Animated raw-shape control does not have two genuinely distinct loaded Meshes");
    const auto raw=[&](Operation operation,Value value) {
        Command command{operation};command.actorPath=doctor;command.arguments.push_back(std::move(value));return command;
    };
    const auto mesh=[&](const std::string& path){return raw(Operation::RawMeshWrite,Value::Text(Kind::Object,path));};
    const auto scale=[&](float value){return raw(Operation::RawDrawScaleWrite,Value::Float(value));};
    const std::vector<Command> commands{Command{Operation::Enable},mesh(originalMesh),scale(originalScale),mesh({}),
        scale(-0.0f),mesh(otherMesh),scale(1.25f),mesh(originalMesh),scale(originalScale)};
    GC::Collect();const auto gc=GC::GetStats();const auto revision=GetPortableRuntimeWorldRevision();
    const auto lookup=ReadPortableRuntimeObjectLookup();const auto owner=ReadPortableRuntimeObjectOwnership(doctor);
    const auto native=ReadPortableRuntimeNativeModel();const auto actors=GetPortableRuntimeMapActors(true);
    const auto same=[&] {
        GC::Collect();SameGc(gc,"Animated raw-shape probe leaked original runtime objects");
        const auto current=ReadPortableRuntimeObjectLookup();const auto identity=ReadPortableRuntimeObjectOwnership(doctor);
        const auto model=ReadPortableRuntimeNativeModel();const auto hash=ReadPortableRuntimeRetailCollision();
        Require(temporary.Save("AfterAnimatedRawShape")==seeded&&GetPortableRuntimeWorldRevision()==revision&&
            current.prepared==lookup.prepared&&current.generation==lookup.generation&&current.objects==lookup.objects&&
            current.retainedBytes==lookup.retainedBytes&&identity.objectIndex==owner.objectIndex&&
            identity.classIndex==owner.classIndex&&identity.outerIndex==owner.outerIndex&&
            model.mapGeneration==native.mapGeneration&&model.levelPath==native.levelPath&&model.modelPath==native.modelPath&&
            !hash.initialized&&!hash.enabled&&!hash.hashIdentity&&hash.actors.empty()&&
            hash.seed==*envelope.state.randomSeed&&!Declared("00_Training.LevelInfo0","Engine.LevelInfo.bBegunPlay",Kind::Bool).boolean,
            "Animated raw-shape probe failed complete clock/save/revision/lookup/Level/hash/RNG rollback");
        const auto world=GetPortableRuntimeMapActors(true);Require(world.size()==actors.size(),"Animated raw store changed actual Actor census");
        for(std::size_t i=0u;i<world.size();++i)Require(world[i].objectPath==actors[i].objectPath&&world[i].classPath==actors[i].classPath,
            "Animated raw store changed actual first-Training Actor slot/class identity");
    };
    const auto clocks=[&](const PortableRetailCollisionProbeResult& result,std::size_t count) {
        Require(result.rawShapes.size()==count,"Animated raw store lost its successful in-root snapshot prefix");
        for(const auto& row:result.rawShapes)Require(row.actorPath==doctor&&row.clockBefore==clock&&row.clockAfter==clock,
            "Raw Mesh/DrawScale write reset, reconstructed or advanced the genuine full animation clock INSIDE its root");
    };
    const auto result=ProbePortableRuntimeRetailCollision(commands);
    Require(result.available&&result.execution.passed()&&!result.execution.committed&&result.execution.writes>=8u,
        "Actual animated raw shape stores failed: "+result.execution.error);clocks(result,8u);same();
    auto lateCommands=commands;lateCommands.push_back(Command{static_cast<Operation>(999)});
    const auto late=ProbePortableRuntimeRetailCollision(lateCommands);
    Require(!late.available&&!late.execution.passed()&&!late.execution.committed&&late.execution.writes>=8u,
        "Animated raw-shape late refusal missed its genuine field-write prefix");clocks(late,8u);++refusals;same();
    QuestVr::Vm::Limits oneWrite;oneWrite.writes=1u;
    const auto partial=ProbePortableRuntimeRetailCollision({mesh({}),scale(originalScale)},oneWrite);
    Require(!partial.available&&!partial.execution.passed()&&!partial.execution.committed&&partial.execution.writes==1u,
        "Animated raw shape did not refuse after its first successful journaled write");
    clocks(partial,1u);++refusals;same();
    Require(LoadPortableRuntimeState(checkpoint.string()),"Cannot restore before real animated raw-shape control");GC::Collect();
    Require(temporary.Save("RestoredAnimatedRawShape")==before,
        "Animated raw-shape test retained its PlayWaiting clock or raw fields after exact checkpoint restoration");
    std::cout<<"FIRST TRAINING animated raw shape: actual Doctor1.PlayWaiting, exact canonical full-clock equality INSIDE "
        "unchanged/changed Mesh/DrawScale/None/negative-zero raw stores; successful, late and partial-write full rollback, "
        "original checkpoint restored. No invented pose, role or begun flag.\n";
}
