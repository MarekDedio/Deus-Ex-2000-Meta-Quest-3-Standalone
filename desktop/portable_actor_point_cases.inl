// Actual first-Training source/provider controls. The separate cylinder test
// supplies independent arithmetic evidence; these cases check real selection,
// qualified owner fields, packet ownership and non-mutating runtime behavior.
void ActorPointCoverage(const std::vector<Actor>& actors,const std::function<void()>& rollback) {
    std::size_t primitive{},mesh{},lodMesh{},blocked{},clear{},models{};
    std::string modelActor,positiveActor;
    const float nan=std::numeric_limits<float>::quiet_NaN();
    for(const auto& actor:actors) {
        const auto selected=ReadPortableRuntimeActorPrimitive(actor.path);
        if(selected.nativeClassPath=="Engine.Model") {
            ++models;if(modelActor.empty())modelActor=actor.path;continue;
        }
        if(selected.nativeClassPath=="Engine.Primitive")++primitive;
        else if(selected.nativeClassPath=="Engine.Mesh")++mesh;
        else {
            Require(selected.nativeClassPath=="Engine.LodMesh","Unexpected actual PointCheck primitive implementation");++lodMesh;
        }
        const float radius=Declared(actor.path,"Engine.Actor.CollisionRadius",Kind::Float).floating;
        const float height=Declared(actor.path,"Engine.Actor.CollisionHeight",Kind::Float).floating;
        Require(std::isfinite(radius)&&std::isfinite(height)&&radius>=0.0f&&height>=0.0f,
            "First Training PointCheck source dimensions are outside finite nonnegative policy");
        const QuestVr::OriginalActorCylinder cylinder{19u,Vector(actor.location),radius,height};
        std::array<Array,4u> points{actor.location,actor.location,actor.location,actor.location};
        points[1u][0u]+=radius+16.0f;
        points[2u][2u]+=height+16.0f;
        points[3u][2u]+=height>0.0f?height*.5f:0.0f;
        for(std::size_t index=0u;index<points.size();++index) {
            const Array extent{index==3u?2.0f:0.0f,nan,index==3u?2.0f:0.0f};
            const std::uint32_t flags=index==3u?0xffffffffu:0u;
            QuestVr::OriginalActorLineHit expected{37u,0u,0u,{101,102,103},{104,105,106},17.0f,73};
            const auto expectedClear=QuestVr::CheckOriginalActorCylinderPoint(expected,cylinder,Vector(points[index]),Vector(extent),flags);
            const auto result=ProbePortableRuntimeActorPrimitivePoint(actor.path,points[index],extent,flags);
            Require(result.clear==expectedClear&&result.hitOwner==(expected.actor==19u)&&result.hitPrimitiveIsNone&&
                result.untouchedCallerFields&&result.time==17.0f&&result.item==73&&
                result.pointImplementation=="UPrimitive::PointCheck"&&result.primitive.actorPath==actor.path&&
                result.primitive.primitivePath==selected.primitivePath&&result.primitive.nativeClassPath==selected.nativeClassPath,
                "Actual source PointCheck selected wrong owner/primitive or changed caller fields: "+actor.path);
            SameVector(result.location,{expected.location.x,expected.location.y,expected.location.z},
                "Actual source PointCheck Location differs from source-backed cylinder");
            SameVector(result.normal,{expected.normal.x,expected.normal.y,expected.normal.z},
                "Actual source PointCheck Normal differs from source-backed cylinder");
            if(expectedClear)++clear;else {
                ++blocked;
                if(positiveActor.empty()&&index==0u&&radius>0.0f&&height>0.0f)positiveActor=actor.path;
            }
        }
    }
    Require(primitive==227u&&lodMesh>0u&&blocked>0u&&clear>0u&&models>0u&&!positiveActor.empty(),
        "Actual first Training PointCheck did not cover constructed Cylinder, LodMesh, overlap and clear outcomes");
    const auto reject=[&](const std::string& actor,const Array& point,const Array& extent) {
        bool rejected{};try {static_cast<void>(ProbePortableRuntimeActorPrimitivePoint(actor,point,extent));}
        catch(const std::exception&) {rejected=true;}
        Require(rejected,"Unsupported actual primitive PointCheck input did not refuse: "+actor);++refusals;rollback();
    };
    const auto found=std::find_if(actors.begin(),actors.end(),[&](const auto& actor){return actor.path==positiveActor;});
    Require(found!=actors.end(),"PointCheck positive source disappeared");
    reject(positiveActor,found->location,{nan,0.0f,0.0f});
    reject(positiveActor,{found->location[0u],found->location[1u],nan},{});
    reject("00_Training.MissingPointOwner",{},{});
    reject("Engine.Actor",{},{});
    reject(modelActor,{},{}); // Explicit missing full Model service, not a clear/line substitute.
    rollback();
    std::cout<<"FIRST TRAINING actual PointCheck: "<<primitive<<" Engine.Cylinder, "<<mesh<<" Mesh, "<<lodMesh
        <<" LodMesh owners; "<<blocked<<" overlaps / "<<clear<<" clear packets; Model remains explicitly unsupported. "
        "No movement, temporary pose, encroachment or playable physics claim.\n";
}
