// First Training adapter evidence only. The independent arithmetic/hull tests
// own numerical proof; these compare the actual selected Model, native owner
// coordinates, declared fields and complete caller-seeded provider packet.
void ModelPointCoverage(const PortablePackageTables& map,const std::vector<Actor>& actors,
    const std::function<void()>& rollback) {
    const auto array=[](const PortableModelVec3& value)->Array {return {value.x,value.y,value.z};};
    const auto samePrimitive=[](const PortableActorPrimitiveSnapshot& left,const PortableActorPrimitiveSnapshot& right) {
        return left.actorPath==right.actorPath&&left.primitivePath==right.primitivePath&&
            left.nativeClassPath==right.nativeClassPath&&left.lineImplementation==right.lineImplementation&&
            left.boundsImplementation==right.boundsImplementation;
    };
    const auto native=ReadPortableRuntimeNativeModel();
    Require(native.available&&native.mapGeneration&&map.sourcePath.find("00_Training.dx")!=std::string::npos,
        "Model PointCheck coverage requires the actual first Training native map");
    struct SourceModel {std::string actor;PortableActorPrimitiveSnapshot selected;std::size_t exportIndex;};
    std::vector<SourceModel> sourceModels;
    for(const auto& actor:actors) {
        const auto selected=ReadPortableRuntimeActorPrimitive(actor.path);
        if(selected.nativeClassPath!="Engine.Model")continue;
        Require(selected.actorPath==actor.path&&
            Declared(actor.path,"Engine.Actor.Brush",Kind::Object).text==selected.primitivePath,
            "Actual Model PointCheck selection did not retain declared Brush precedence");
        std::optional<std::size_t> source;
        for(std::size_t index=0u;index<map.exports.size();++index)
            if(selected.primitivePath=="00_Training."+GetPortableObjectPath(map,static_cast<std::int32_t>(index+1u))) {
                Require(!source.has_value(),"Training Model provider has duplicate exact export identities");source=index;
            }
        Require(source.has_value(),"Selected first Training Brush is not its actual source Model export: "+selected.primitivePath);
        sourceModels.push_back({actor.path,selected,*source});
    }
    Require(!sourceModels.empty(),"First Training Model PointCheck census has no actual Brush Model owner");
    std::size_t queried{},nonempty{},empty{},calls{},predictions{},zeroClear{},zeroBlocked{},boxClear{},boxBlocked{};
    std::set<std::string> queriedModels;
    for(const auto& source:sourceModels) {
        if(queried==8u||(queried>=3u&&zeroClear&&zeroBlocked&&boxClear&&boxBlocked))break;
        if(queriedModels.count(source.selected.primitivePath))continue;
        // Decode the selected source export, never the loader root Model or a
        // rendered mesh. Cache/provider authority is tested separately below.
        const auto model=LoadPortableCollisionModel68(map,source.exportIndex);
        if(model.nodes.empty()) {++empty;continue;}
        ++nonempty;
        const auto ownership=ReadPortableRuntimeObjectOwnership(source.actor);
        const auto modelOwnership=ReadPortableRuntimeObjectOwnership(source.selected.primitivePath);
        const auto revision=GetPortableRuntimeWorldRevision();
        const auto location=Declared(source.actor,"Engine.Actor.Location",Kind::Vector);
        const auto rotation=Declared(source.actor,"Engine.Actor.Rotation",Kind::Rotator);
        const auto colLocation=Declared(source.actor,"Engine.Actor.ColLocation",Kind::Vector);
        const auto nativeCoords=ReadPortableRuntimeActorToWorld(source.actor);
        Require(nativeCoords.actorPath==source.actor&&!nativeCoords.nativeImplementation.empty(),
            "Actual Model PointCheck owner lacks its native ToWorld dispatch");
        const QuestVr::OriginalModelCoords coords{Vector(nativeCoords.origin),Vector(nativeCoords.xAxis),
            Vector(nativeCoords.yAxis),Vector(nativeCoords.zAxis)};
        const auto bounds=QuestVr::TransformOriginalModelBounds(model.bounds.minimum,model.bounds.maximum,coords);
        const Array minimum=array(bounds.minimum),maximum=array(bounds.maximum);
        const auto expected=[&](const Array& point,const Array& extent,std::uint32_t flags) {
            QuestVr::OriginalActorLineHit packet{37u,0u,0u,{101.0f,102.0f,103.0f},{104.0f,105.0f,106.0f},17.0f,73};
            std::size_t captures{};
            const auto clear=QuestVr::CheckOriginalModelPointOverlap(packet,model,19u,23u,Vector(point),Vector(extent),flags,
                [&] {++captures;return coords;});
            Require(captures==1u,"Nonempty Model PointCheck expected path did not capture owner coordinates exactly once");
            ++predictions;return std::pair{clear,packet};
        };
        const auto run=[&](const Array& point,const Array& extent,std::uint32_t flags,const std::string& label) {
            const auto predicted=expected(point,extent,flags);
            const auto actual=ProbePortableRuntimeActorPrimitivePoint(source.actor,point,extent,flags);
            const auto& hit=predicted.second;
            Require(samePrimitive(actual.primitive,source.selected)&&actual.pointImplementation=="UModel::PointCheck"&&
                actual.clear==predicted.first&&actual.hitOwner==(hit.actor==23u)&&
                actual.hitPrimitiveIsNone==(hit.primitive==0u)&&actual.hitSelectedPrimitive==(hit.primitive==19u)&&
                actual.nextPreserved&&hit.next==37u&&!actual.untouchedCallerFields&&actual.item==hit.item,
                "Actual first Training Model PointCheck packet/selection differs: "+source.actor+" "+label);
            SameFloat(actual.time,hit.time,"Actual Model PointCheck defined Time differs");
            SameVector(actual.location,array(hit.location),"Actual Model PointCheck Location differs");
            SameVector(actual.normal,array(hit.normal),"Actual Model PointCheck Normal differs");
            const auto now=ReadPortableRuntimeObjectOwnership(source.actor);
            const auto nowModel=ReadPortableRuntimeObjectOwnership(source.selected.primitivePath);
            const auto afterCoords=ReadPortableRuntimeActorToWorld(source.actor);
            Require(now.objectIndex==ownership.objectIndex&&nowModel.objectIndex==modelOwnership.objectIndex&&
                GetPortableRuntimeWorldRevision()==revision&&
                QuestVr::Vm::Equal(Declared(source.actor,"Engine.Actor.Location",Kind::Vector),location)&&
                QuestVr::Vm::Equal(Declared(source.actor,"Engine.Actor.Rotation",Kind::Rotator),rotation)&&
                QuestVr::Vm::Equal(Declared(source.actor,"Engine.Actor.ColLocation",Kind::Vector),colLocation)&&
                afterCoords.actorPath==nativeCoords.actorPath&&afterCoords.nativeClassPath==nativeCoords.nativeClassPath&&
                afterCoords.nativeImplementation==nativeCoords.nativeImplementation&&
                samePrimitive(ReadPortableRuntimeActorPrimitive(source.actor),source.selected),
                "Model PointCheck provider changed source pose, native identity, selection or revision");
            SameVector(afterCoords.origin,nativeCoords.origin,"Model PointCheck changed native ToWorld origin");
            SameVector(afterCoords.xAxis,nativeCoords.xAxis,"Model PointCheck changed native ToWorld X axis");
            SameVector(afterCoords.yAxis,nativeCoords.yAxis,"Model PointCheck changed native ToWorld Y axis");
            SameVector(afterCoords.zAxis,nativeCoords.zAxis,"Model PointCheck changed native ToWorld Z axis");
            ++calls;rollback();return predicted.first;
        };
        // All candidates derive from the selected source's transformed box.
        // No production result chooses an expected outcome. A genuine source
        // refusal propagates; it is never skipped as an apparently clear hit.
        std::vector<Array> points;
        Array center{};
        for(std::size_t axis=0u;axis<3u;++axis)center[axis]=static_cast<float>((double(minimum[axis])+double(maximum[axis]))*.5);
        points.push_back(center);
        for(const float x:{.25f,.5f,.75f})for(const float y:{.25f,.5f,.75f})for(const float z:{.25f,.5f,.75f}) {
            const Array fraction{x,y,z};Array point{};
            for(std::size_t axis=0u;axis<3u;++axis)point[axis]=static_cast<float>(double(minimum[axis])+
                (double(maximum[axis])-double(minimum[axis]))*double(fraction[axis]));
            points.push_back(point);
        }
        for(std::size_t axis=0u;axis<3u;++axis)for(const float side:{-1.0f,1.0f}) {
            auto point=center;point[axis]=side<0.0f?minimum[axis]-512.0f:maximum[axis]+512.0f;points.push_back(point);
        }
        std::array<std::array<std::optional<Array>,2u>,2u> cases;
        for(std::size_t branch=0u;branch<2u;++branch) {
            const Array extent=branch?Array{2.0f,2.0f,2.0f}:Array{};
            for(const auto& point:points) {
                const auto prediction=expected(point,extent,0u);const auto outcome=prediction.first?1u:0u;
                if(!cases[branch][outcome])cases[branch][outcome]=point;
                if(cases[branch][0u]&&cases[branch][1u])break;
            }
            for(std::size_t outcome=0u;outcome<2u;++outcome) {
                // A real owner can be a shell or have no solid collision hull.
                // Absence of one geometric outcome is not a query refusal;
                // record all available outcomes and require both across the
                // bounded set of actual owners below. Errors still propagate.
                if(!cases[branch][outcome])continue;
                const auto clear=run(*cases[branch][outcome],extent,0u,branch?"box":"zero");
                Require(clear==(outcome==1u),"Selected Model source control changed its predicted outcome");
                if(branch) {if(clear)++boxClear;else ++boxBlocked;}
                else {if(clear)++zeroClear;else ++zeroBlocked;}
            }
        }
        const auto point=cases[0u][0u]?*cases[0u][0u]:*cases[0u][1u];
        const float nan=std::numeric_limits<float>::quiet_NaN();
        run(point,{nan,-0.0f,nan},0xffffffffu,"zero-or-unordered extent and ignored zero-branch flags");
        run(cases[1u][0u]?*cases[1u][0u]:*cases[1u][1u],{2.0f,2.0f,2.0f},0xffffffffu,"full DWORD box flags");
        bool refused{};
        try {static_cast<void>(ProbePortableRuntimeActorPrimitivePoint(source.actor,point,{2.0f,nan,2.0f}));}
        catch(const std::exception&) {refused=true;}
        Require(refused,"Active nonfinite Model box extent was accepted as zero/clear");++refusals;rollback();
        queriedModels.insert(source.selected.primitivePath);++queried;
        const auto afterModel=ReadPortableRuntimeNativeModel();
        Require(afterModel.available&&afterModel.mapGeneration==native.mapGeneration&&afterModel.levelPath==native.levelPath&&
            afterModel.modelPath==native.modelPath&&afterModel.modelExportIndex==native.modelExportIndex,
            "Selected Model PointCheck replaced the native root Level/Model binding");
        std::cout<<"MODEL POINT SOURCE owner="<<source.actor<<" primitive="<<source.selected.primitivePath
            <<" native="<<nativeCoords.nativeImplementation<<" nodes="<<model.nodes.size()<<" sourceExport="<<source.exportIndex
            <<" zeroOverlap="<<cases[0u][0u].has_value()<<" zeroClear="<<cases[0u][1u].has_value()
            <<" boxOverlap="<<cases[1u][0u].has_value()<<" boxClear="<<cases[1u][1u].has_value()<<'\n';
    }
    Require(queried>=1u&&nonempty==queried&&zeroClear&&zeroBlocked&&boxClear&&boxBlocked,
        "Actual first Training Model PointCheck missed nonempty zero/box clear and overlap provider coverage");
    rollback();
    std::cout<<"FIRST TRAINING Model PointCheck: "<<sourceModels.size()<<" actual Brush owners in census; "<<queried
        <<" distinct nonempty source Models, "<<empty<<" empty selections before bound, "<<calls<<" actual packet checks / "
        <<predictions<<" source/ToWorld kernel predictions, exact source-pose/identity/save/revision/RNG rollback. "
        "Adapter wiring evidence, not independent numerical proof or encroachment/movement completion.\n";
}
