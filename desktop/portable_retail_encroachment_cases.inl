// First-Training-only native encroachment adapter evidence. The independent
// vector-bucket/CRT oracle supplies candidate encounter order and tag writes;
// separately verified scalar kernels supply source-pose bounds and PointCheck.
// This is NOT Move, callback/carry logic, published startup or playable physics.
void EncroachmentCoverage(const PortablePackageTables& map,const std::vector<Actor>& actors,
    const std::function<void()>& rollback) {
    using Rotation=std::array<std::int32_t,3u>;
    using Coords=QuestVr::OriginalModelCoords;
    using PlaneModel=PortableModelGeometry;
    const auto array=[](PortableModelVec3 value)->Array{return {value.x,value.y,value.z};};
    const auto snapshots=GetPortableRuntimeMapActors(true);
    std::vector<bool> movingBrush(actors.size());
    std::size_t movingBrushCount{},cases{},totalHits{},selfTags{},movingTags{},modelCalls{},cylinderCalls{};
    const auto derives=[&](std::string path,const std::string& base) {
        std::set<std::string> visited;
        for(std::size_t depth=0u;!path.empty()&&depth<128u;++depth) {
            const auto folded=Fold(path);
            Require(visited.insert(folded).second,"Encroachment source class ancestry cycles");
            if(folded==Fold(base))return true;
            path=ReadPortableRuntimeDispatchClass(path).parentPath;
        }
        Require(path.empty(),"Encroachment source class ancestry exceeded fixed bound");return false;
    };
    for(std::size_t index=0u;index<actors.size();++index) {
        const auto& actor=actors[index];
        const auto brush=Declared(actor.path,"Engine.Actor.Brush",Kind::Object).text;
        if(brush.empty())continue;
        const auto selected=std::find_if(snapshots.begin(),snapshots.end(),[&](const auto& item){return item.objectPath==actor.path;});
        Require(selected!=snapshots.end(),"Encroachment source actor lacks current snapshot class identity");
        movingBrush[index]=derives(selected->classPath,"Engine.Brush")&&
            !Declared(actor.path,"Engine.Actor.bStatic",Kind::Bool).boolean;
        if(movingBrush[index])++movingBrushCount;
    }
    struct Source {
        std::size_t actor{};
        PortableActorPrimitiveSnapshot primitive;
        Rotation rotation{};
        float radius{},height{};
        bool brush{};
        PortableModelVec3 prePivot{};
        QuestVr::OriginalBrushScale post{},main{};
        std::optional<PlaneModel> model;
    };
    const auto scale=[&](const std::string& path,const char* name) {
        const auto value=Declared(path,name,Kind::Struct);
        Require(value.fields.size()==3u&&value.fields.at("scale").kind==Kind::Vector&&
            value.fields.at("sheerrate").kind==Kind::Float&&value.fields.at("sheeraxis").kind==Kind::Byte,
            "Encroachment expected ToWorld lacks exact qualified Scale members");
        const auto& vector=value.fields.at("scale").vector;
        return QuestVr::OriginalBrushScale{{vector[0u],vector[1u],vector[2u]},
            value.fields.at("sheerrate").floating,static_cast<std::uint8_t>(value.fields.at("sheeraxis").integer)};
    };
    const auto loadSource=[&](const std::string& path) {
        Source source;
        const auto found=std::find_if(actors.begin(),actors.end(),[&](const auto& actor){return actor.path==path;});
        Require(found!=actors.end(),"First Training encroachment source is absent");
        source.actor=static_cast<std::size_t>(found-actors.begin());
        source.primitive=ReadPortableRuntimeActorPrimitive(path);
        source.rotation=Declared(path,"Engine.Actor.Rotation",Kind::Rotator).rotation;
        source.radius=Declared(path,"Engine.Actor.CollisionRadius",Kind::Float).floating;
        source.height=Declared(path,"Engine.Actor.CollisionHeight",Kind::Float).floating;
        const auto snapshot=ReadPortableRuntimeActorToWorld(path);
        source.brush=snapshot.nativeImplementation=="ABrush::ToWorld";
        Require(source.brush||snapshot.nativeImplementation=="AActor::ToWorld",
            "Encroachment source has unaudited native coordinate implementation");
        if(source.brush) {
            const auto pivot=Declared(path,"Engine.Actor.PrePivot",Kind::Vector).vector;
            source.prePivot=Vector(pivot);source.post=scale(path,"Engine.Brush.PostScale");
            source.main=scale(path,"Engine.Brush.MainScale");
        }
        if(source.primitive.nativeClassPath=="Engine.Model") {
            const auto dot=source.primitive.primitivePath.find('.');
            Require(dot!=std::string::npos&&Fold(source.primitive.primitivePath.substr(0u,dot))=="00_training",
                "Encroachment fixture selected a Model outside first Training");
            const auto exportIndex=FindPortableExport(map,source.primitive.primitivePath.substr(dot+1u));
            Require(exportIndex<map.exports.size(),"Encroachment source selected Model lacks its original export");
            source.model=LoadPortableCollisionModel68(map,exportIndex);
        }else Require(source.primitive.nativeClassPath=="Engine.Mesh"||source.primitive.nativeClassPath=="Engine.LodMesh"||
            source.primitive.nativeClassPath=="Engine.Primitive","Encroachment source selected an unknown primitive implementation");
        return source;
    };
    const auto coords=[&](const Source& source,const Array& location,const Rotation& rotation) {
        const QuestVr::OriginalBrushRotation angles{rotation[0u],rotation[1u],rotation[2u]};
        return source.brush?QuestVr::BuildOriginalBrushCoords(Vector(location),angles,source.prePivot,
            source.post,source.main,QuestVr::OriginalSineProfile::Table()):
            QuestVr::BuildOriginalActorCoords(Vector(location),angles,QuestVr::OriginalSineProfile::Table());
    };
    const auto currentCoordsMatch=[&](const Source& source) {
        const auto expected=coords(source,actors[source.actor].location,source.rotation);
        const auto actual=ReadPortableRuntimeActorToWorld(actors[source.actor].path);
        SameVector(actual.origin,array(expected.origin),"Encroachment source current native Origin differs from declared-field builder");
        SameVector(actual.xAxis,array(expected.xAxis),"Encroachment source current native XAxis differs from declared-field builder");
        SameVector(actual.yAxis,array(expected.yAxis),"Encroachment source current native YAxis differs from declared-field builder");
        SameVector(actual.zAxis,array(expected.zAxis),"Encroachment source current native ZAxis differs from declared-field builder");
    };
    const auto lower=loadSource("00_Training.CrateBreakableMedGeneral1");
    const auto upper=loadSource("00_Training.CrateBreakableMedGeneral2");
    const auto mover=loadSource("00_Training.DeusExMover16");
    Require(lower.primitive.nativeClassPath=="Engine.LodMesh"&&upper.primitive.nativeClassPath=="Engine.LodMesh"&&
        mover.primitive.nativeClassPath=="Engine.Model"&&mover.brush&&movingBrush[mover.actor]&&
        !actors[lower.actor].deleted&&!actors[upper.actor].deleted&&actors[lower.actor].collide&&actors[upper.actor].collide,
        "Encroachment fixture lost original crate pair, moving brush or selected native implementations");
    for(const auto* source:{&lower,&upper,&mover})currentCoordsMatch(*source);
    const auto sourceOracle=std::make_unique<Oracle>(actors);
    struct Expected {
        std::pair<std::vector<PortableRetailCollisionHit>,std::size_t> packets;
        QuestVr::OriginalCollisionHashRange range;
        std::size_t self{},moving{},model{},cylinder{};
    };
    const auto predict=[&](const Source& source,const Command& command,Oracle& oracle) {
        Expected expected;const auto proposed=coords(source,command.end,command.rotation);
        QuestVr::OriginalCollisionHashBox bounds;
        if(source.model) {
            const auto transformed=QuestVr::TransformOriginalModelBounds(source.model->bounds.minimum,source.model->bounds.maximum,proposed);
            bounds={transformed.minimum,transformed.maximum};
        }else bounds=QuestVr::OriginalPrimitiveCollisionBounds(Vector(command.end),source.radius,source.height);
        expected.range=QuestVr::OriginalCollisionHashBoxRange(bounds);
        ++oracle.tag;
        oracle.Cells(expected.range,[&](std::uint32_t bucket,std::uint32_t cell) {
            for(auto entry=oracle.buckets[bucket].rbegin();entry!=oracle.buckets[bucket].rend();++entry) {
                const auto index=entry->actor;
                if(oracle.tags[index]==oracle.tag||entry->cell!=cell)continue;
                oracle.tags[index]=oracle.tag;
                if(movingBrush[index]) {++expected.moving;continue;}
                if(index==source.actor) {++expected.self;continue;}
                const auto& candidate=actors[index];
                const float radius=Declared(candidate.path,"Engine.Actor.CollisionRadius",Kind::Float).floating,
                    height=Declared(candidate.path,"Engine.Actor.CollisionHeight",Kind::Float).floating;
                QuestVr::OriginalActorLineHit hit;bool clear;
                if(source.model) {
                    ++expected.model;
                    clear=QuestVr::CheckOriginalModelPointOverlap(hit,*source.model,29u,31u,Vector(candidate.location),
                        {radius,radius,height},0u,[&]()->Coords{return proposed;});
                }else {
                    ++expected.cylinder;
                    clear=QuestVr::CheckOriginalActorCylinderPoint(hit,
                        QuestVr::OriginalActorCylinder{31u,Vector(command.end),source.radius,source.height},
                        Vector(candidate.location),{radius,radius,height},0u);
                }
                if(clear)continue;
                // The hash overwrites provider Actor with the SAME candidate,
                // clears Primitive, and appends in encounter order. No reverse
                // list/prepend or Time-based sort is appropriate here.
                const auto next=expected.packets.first.size()+2u;
                expected.packets.first.push_back({candidate.path,{},next,array(hit.location),array(hit.normal),hit.time,hit.item});
            }
        });
        if(!expected.packets.first.empty()) {expected.packets.first.back().next=0u;expected.packets.second=1u;}
        return expected;
    };
    const auto command=[&](const Source& source,const Array& proposed,const Rotation& rotation,std::uint32_t flags=0u) {
        Command result{Operation::EncroachmentCheck};result.actorPath=actors[source.actor].path;
        result.end=proposed;result.rotation=rotation;result.ignoredEncroachmentFlags=flags;return result;
    };
    const auto run=[&](const Source& source,const Command& query,const std::string& label,
        const std::string& requiredHit=std::string{},bool requireSelf=false,bool repeat=false) {
        auto oracle=std::make_unique<Oracle>(*sourceOracle);
        const auto expected=predict(source,query,*oracle);const auto firstTags=oracle->tags;
        std::optional<Expected> second;
        std::vector<Command> commands{Command{Operation::Enable},query};
        if(repeat) {auto ignored=query;ignored.ignoredEncroachmentFlags=~query.ignoredEncroachmentFlags;
            commands.push_back(ignored);second=predict(source,ignored,*oracle);}
        const auto result=ProbePortableRuntimeRetailCollision(commands);
        Require(result.available&&result.execution.passed()&&!result.execution.committed&&result.states.size()==commands.size()&&
            result.queries.size()==commands.size()-1u&&result.queryHeads.size()==commands.size()-1u,
            "Actual first Training temporary-pose encroachment refused "+label+": "+result.execution.error);
        SameHits(result.queries[0u],result.queryHeads[0u],expected.packets);
        if(second) {
            SameHits(result.queries[1u],result.queryHeads[1u],second->packets);
            SameHits(result.queries[1u],result.queryHeads[1u],expected.packets);
        }
        if(!requiredHit.empty())Require(std::any_of(expected.packets.first.begin(),expected.packets.first.end(),
            [&](const auto& packet){return packet.actorPath==requiredHit;}),
            "Encroachment positive source case did not independently overlap its required real candidate: "+label);
        if(requireSelf)Require(expected.self>0u,"Encroachment source self-tag gate was not exercised");
        std::vector<std::uint32_t> authoredTags;for(const auto& actor:actors)authoredTags.push_back(actor.tag);
        for(std::size_t index=0u;index<result.states.size();++index) {
            const auto& state=result.states[index];
            const auto& tags=index==0u?authoredTags:index==1u?firstTags:oracle->tags;
            SameActors(state,actors,tags);
            Require(state.initialized&&state.enabled&&state.seed==sourceOracle->seed&&state.initializationDraws==49152u&&
                state.tag==index&&state.hashIdentity==result.states[0u].hashIdentity&&
                state.activeLinks==result.states[0u].activeLinks&&state.allocatedLinks==result.states[0u].allocatedLinks,
                "Encroachment changed hash membership/ColLocation/table RNG or reset query tags: "+label);
        }
        ++cases;totalHits+=expected.packets.first.size();selfTags+=expected.self;movingTags+=expected.moving;
        modelCalls+=expected.model;cylinderCalls+=expected.cylinder;
        rollback();currentCoordsMatch(source);
        Require(Declared(actors[source.actor].path,"Engine.Actor.Rotation",Kind::Rotator).rotation==source.rotation,
            "Temporary source Rotation survived successful encroachment scope");
        SameVector(Declared(actors[source.actor].path,"Engine.Actor.Location",Kind::Vector).vector,
            actors[source.actor].location,"Temporary source Location survived successful encroachment scope");
    };
    run(lower,command(lower,actors[lower.actor].location,lower.rotation),"authored lower crate/self tag",{},true,true);
    run(lower,command(lower,actors[upper.actor].location,lower.rotation),"lower crate posed at upper crate",
        actors[upper.actor].path);
    run(upper,command(upper,actors[lower.actor].location,upper.rotation,0xffffffffu),"upper crate posed at lower crate",
        actors[lower.actor].path);
    const Array farLocation{100000.0f,100000.0f,100000.0f};
    run(lower,command(lower,farLocation,Rotation{8192,-16384,4096}),"far translated and rotated crate clear");
    run(mover,command(mover,actors[mover.actor].location,mover.rotation),"authored moving-brush source",{},false,true);

    // Position actual mover bounds at the source crate stack, not its editor
    // pivot (which can be far outside local geometry). Every bounded proposed
    // transform is tested, including misses; a genuine crate overlap is then
    // required before this fixture may claim positive Model provider coverage.
    std::size_t modelPositive{};
    for(const std::int32_t yawOffset:{0,16384}) {
        auto rotation=mover.rotation;rotation[1u]=std::bit_cast<std::int32_t>(
            std::bit_cast<std::uint32_t>(rotation[1u])+std::bit_cast<std::uint32_t>(yawOffset));
        const auto centered=coords(mover,Array{},rotation);
        const auto box=QuestVr::TransformOriginalModelBounds(mover.model->bounds.minimum,mover.model->bounds.maximum,centered);
        const Array middle{(box.minimum.x+box.maximum.x)*.5f,(box.minimum.y+box.maximum.y)*.5f,
            (box.minimum.z+box.maximum.z)*.5f};
        for(const float shift:{0.0f,16.0f,-16.0f}) {
            Array proposed=actors[lower.actor].location;
            for(std::size_t axis=0u;axis<3u;++axis)proposed[axis]-=middle[axis];
            proposed[0u]+=shift;
            const auto query=command(mover,proposed,rotation,0x81abcdefu);
            auto oracle=std::make_unique<Oracle>(*sourceOracle);const auto expected=predict(mover,query,*oracle);
            const bool positive=std::any_of(expected.packets.first.begin(),expected.packets.first.end(),[&](const auto& packet) {
                return packet.actorPath==actors[lower.actor].path||packet.actorPath==actors[upper.actor].path;
            });
            modelPositive+=positive?1u:0u;
            run(mover,query,"translated/rotated actual mover at crate stack");
        }
    }
    Require(modelPositive>0u,"Bounded actual mover poses never overlapped either source Training crate");
    const auto valid=command(lower,actors[upper.actor].location,lower.rotation);
    const auto reject=[&](Command bad,const std::string& label) {
        const auto result=ProbePortableRuntimeRetailCollision({Command{Operation::Enable},valid,bad});
        Require(!result.available&&!result.execution.passed()&&!result.execution.committed&&!result.execution.error.empty(),
            "Late invalid encroachment retained prior hash/query/pose prefix: "+label);
        ++refusals;rollback();currentCoordsMatch(lower);currentCoordsMatch(mover);
    };
    auto invalid=valid;invalid.end[0u]=std::numeric_limits<float>::quiet_NaN();reject(invalid,"active proposed source Location NaN");
    invalid=valid;invalid.actorPath="00_Training.MissingEncroachmentSource";reject(invalid,"missing actual source");
    invalid=valid;invalid.actorPath="Engine.Actor";reject(invalid,"source Class instead of indexed Actor");
    invalid=valid;invalid.actorPath=mover.primitive.primitivePath;reject(invalid,"source Model asset instead of Actor");
    const auto absent=ProbePortableRuntimeRetailCollision({valid});
    Require(!absent.available&&!absent.execution.passed()&&!absent.execution.committed&&!absent.execution.error.empty(),
        "Encroachment without actual enabled Level.Hash silently succeeded");++refusals;rollback();
    Require(cases>=11u&&totalHits>0u&&selfTags>0u&&movingBrushCount>0u&&movingTags>0u&&modelCalls>0u&&cylinderCalls>0u,
        "Encroachment adapter did not cover real source pose, candidate skips, Model and cylinder paths");
    std::cout<<"FIRST TRAINING Encroachment: "<<cases<<" rollback-only source poses, "<<totalHits<<" ordered native hits, "
        <<modelCalls<<" Model/"<<cylinderCalls<<" cylinder source PointChecks; "<<selfTags<<" self tags/"<<movingTags
        <<" moving-brush tags. Actual declared source pose, source primitive, candidate cylinder, hash order/tags, "
        "ignored DWORD flags and complete success/failure restore. NOT Move, startup or playable crate physics.\n";
}
