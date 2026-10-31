    PortableModelNode ReadTrackerNode(RuntimeNativeBrushTracker& tracker,RuntimeNativeMutableModel& live,std::int32_t index) {
        auto* level=CurrentClockLevel();
        if(tracker.level!=level||tracker.model!=level->nativeLevelModel||tracker.generation!=level->nativeMapGeneration||
            tracker.model->nativeMutableModel.get()!=&live||live.generation!=tracker.generation||
            index<0||static_cast<std::size_t>(index)>=live.geometry.nodes.size())
            throw std::runtime_error("Live brush node lost actual Level/Model/generation ownership");
        const auto& g=live.geometry;const auto& pool=tracker.pools;const auto i=static_cast<std::size_t>(index);
        if(i>=pool.original.nodes) {
            const auto offset=i-pool.original.nodes;
            if(offset>=pool.nodes.size()||!pool.nodes[offset].owner)
                throw std::runtime_error("Live brush traversal selected a free dynamic node");
            const auto* owner=pool.nodes[offset].owner;const auto& n=g.nodes[i];
            if(n.surface<0||static_cast<std::size_t>(n.surface)<live.sourceSurfaceCount||
                static_cast<std::size_t>(n.surface)-live.sourceSurfaceCount>=live.dynamicSurfaceReferences.size()||
                live.dynamicSurfaceReferences[static_cast<std::size_t>(n.surface)-live.sourceSurfaceCount].actor!=owner||
                n.vertexPool<0||static_cast<std::size_t>(n.vertexPool)<pool.original.vertices||
                static_cast<std::size_t>(n.vertexPool)>g.vertices.size()||n.vertexCount>g.vertices.size()-static_cast<std::size_t>(n.vertexPool))
                throw std::runtime_error("Live brush node lost its typed surface/vertex identity");
            for(std::size_t v=0;v<n.vertexCount;++v) {
                LifecycleStep();const auto vertex=static_cast<std::size_t>(n.vertexPool)+v;
                if(vertex-pool.original.vertices>=pool.vertices.size()||pool.vertices[vertex-pool.original.vertices]!=owner)
                    throw std::runtime_error("Live brush node selected another dynamic vertex owner");
                const auto point=g.vertices[vertex].point;
                if(point<0||static_cast<std::size_t>(point)<pool.postSetupPoints||static_cast<std::size_t>(point)>=g.points.size()||
                    static_cast<std::size_t>(point)-pool.postSetupPoints>=pool.points.size()||
                    pool.points[static_cast<std::size_t>(point)-pool.postSetupPoints].owner!=owner)
                    throw std::runtime_error("Live brush vertex selected a free/foreign point");
            }
        }
        return g.nodes[i];
    }
    Value ConstructNativeBrushTracker(std::size_t failAfterAttachments,PortableBrushTrackerProbe& result) {
        auto* level=CurrentClockLevel();auto* model=level->nativeLevelModel;
        if(level->nativeBrushTracker||beforeBrushTracker_)
            throw std::runtime_error("Original fresh brush constructor requires an absent prior tracker");
        auto& live=MutableRootModel(level);auto& g=live.geometry;
        result.original=QuestVr::OriginalBrushCounts(g);
        // Exact original setup pass and native/reflected journal, not a second
        // transaction or renderer mesh reconstruction.
        SetupNativeBrushCohort(0u,result.setup);
        const auto previousScratch=nativeQueryScratchBytes_;
        struct Restore {std::size_t& value;std::size_t before;~Restore(){value=before;}} restore{nativeQueryScratchBytes_,previousScratch};
        std::unique_ptr<RuntimeNativeBrushTracker> prepared;
        std::vector<GCRoot<RuntimeObject>> pending;
        std::size_t moverScratch{};
        const auto measure=[&](std::size_t extra) {
            nativeQueryScratchBytes_=previousScratch;
            const auto local=(prepared?NativeBrushTrackerBytes(*prepared):sizeof(RuntimeNativeBrushTracker))+
                pending.capacity()*sizeof(GCRoot<RuntimeObject>)+pending.size()*sizeof(GCRootNode)+moverScratch;
            ValidateStateBudget(QuestVr::DecalStateDetail::Add(local,extra));
            // Growth overlap remains visible to nested admission until the
            // next exact measurement. Never lend the same allowance twice.
            nativeQueryScratchBytes_=QuestVr::DecalStateDetail::Add(previousScratch,QuestVr::DecalStateDetail::Add(local,extra));
        };
        measure(0u);prepared=std::make_unique<RuntimeNativeBrushTracker>();
        prepared->level=level;prepared->model=model;prepared->generation=level->nativeMapGeneration;
        prepared->pools.original=result.original;
        QuestVr::OriginalBrushPoolQueries allocation{[this]{LifecycleStep();},measure};
        QuestVr::InitializeOriginalBrushPools(g,prepared->pools,allocation);measure(0u);
        // Update(nullptr) first walks current Actors ascending, prepending each
        // unassimilated candidate. Reversed iteration below is that same list.
        for(std::size_t slot=0;slot<persistentWorldActors.size();++slot) {
            LifecycleStep();auto* actor=persistentWorldActors[slot];if(!actor)continue;
            const auto selected=NativePrimitiveField(actor,"Engine.Actor.Brush","Engine.Model",0x2au);
            if(!selected||!IsDerivedFromPath(actor->cls,"Engine.Brush")||NativeActorStatic(actor))continue;
            if(!IsDerivedFromPath(actor->cls,"Engine.Mover"))throw std::runtime_error("Original Update checked Mover cast failed");
            const auto assimilated=MovingBrushScalarField(actor,"Engine.Actor.bAssimilated","Engine.Actor",Kind::Bool,0x2002u);
            if(Read(actor,assimilated,0u).boolean)continue;
            ReferenceFor(actor,assimilated,0u,false)->write(Value::Bool(true));
            measure((pending.size()+1u)*sizeof(GCRoot<RuntimeObject>)+sizeof(GCRootNode));
            pending.reserve(pending.size()+1u);pending.emplace_back(actor);measure(0u);
            ReferenceFor(actor,MovingBrushStructField(actor,"Engine.Mover.OldPos",Kind::Vector,1u),0u,false)->write(
                NativeCoordsRead(actor,"Engine.Actor.Location",Kind::Vector,0x23u));
            ReferenceFor(actor,MovingBrushStructField(actor,"Engine.Mover.OldRot",Kind::Rotator,1u),0u,false)->write(
                NativeCoordsRead(actor,"Engine.Actor.Rotation",Kind::Rotator,0x23u));
        }
        // Original pass1 dependency and pass2 flush really select the -1 short
        // paths for this FRESH constructor. Noninitial roots explicitly refuse;
        // these are not empty providers for later moving-object updates.
        for(unsigned pass=0;pass<2u;++pass)for(auto it=pending.rbegin();it!=pending.rend();++it) {
            LifecycleStep();const auto brush=NativePrimitiveField(it->get(),"Engine.Actor.Brush","Engine.Model",0x2au);
            if(!brush||!brush->object->nativeBrushRoot||*brush->object->nativeBrushRoot!=-1)
                throw std::runtime_error("Noninitial brush dependency/flush requires its original provider");
            if(pass==1u)brush->object->nativeBrushRoot=-1;
        }
        for(auto it=pending.rbegin();it!=pending.rend();++it) {
            LifecycleStep();auto* actor=it->get();moverScratch=65536u;measure(0u);
            const auto brush=NativePrimitiveField(actor,"Engine.Actor.Brush","Engine.Model",0x2au);
            if(!brush||!brush->object->nativeBrushPolys||!brush->object->nativeBrushPolys->nativeMutablePolys)
                throw std::runtime_error("Initial brush Update lost native Model/Polys binding");
            auto* brushModel=brush->object;const auto& source=brushModel->nativeBrushPolys->nativeMutablePolys->data;
            ReferenceFor(actor,MovingBrushScalarField(actor,"Engine.Actor.bAssimilated","Engine.Actor",Kind::Bool,0x2002u),0u,false)->write(Value::Bool(false));
            const auto coords=NativeBrushBuildCoords(actor);
            const auto pivotValue=NativeCoordsRead(actor,"Engine.Actor.PrePivot",Kind::Vector,0x20u);
            const auto locationValue=NativeCoordsRead(actor,"Engine.Actor.Location",Kind::Vector,0x23u);
            const PortableModelVec3 pivot{pivotValue.vector[0],pivotValue.vector[1],pivotValue.vector[2]},
                location{locationValue.vector[0],locationValue.vector[1],locationValue.vector[2]};
            const auto polyBytes=QuestVr::DecalStateDetail::Multiply(source.polygons.size(),sizeof(PortableBrushPoly)+16u*sizeof(PortableModelVec3));
            moverScratch=QuestVr::DecalStateDetail::Add(moverScratch,polyBytes);measure(0u);
            std::vector<PortableBrushPoly> transformed;std::vector<PortableModelVec3> vertices;
            transformed.reserve(source.polygons.size());vertices.reserve(source.polygons.size()*16u);
            for(const auto& poly:source.polygons) {
                LifecycleStep();transformed.push_back(QuestVr::TransformOriginalBrushPoly(poly,coords,pivot,location));
                const auto& current=transformed.back();
                for(std::int32_t v=0;v<current.vertexCount;++v){LifecycleStep();vertices.push_back(current.vertices[static_cast<std::size_t>(v)]);}
            }
            const auto sphere=QuestVr::BuildOriginalBrushSphere(vertices);
            const auto read=[&](std::int32_t index){return ReadTrackerNode(*prepared,live,index);};
            QuestVr::OriginalBrushSphereFilterQueries sphereQueries{read,[&](std::int32_t index,std::uint8_t flags){
                static_cast<void>(read(index));g.nodes[static_cast<std::size_t>(index)].nodeFlags=flags;},[this]{LifecycleStep();}};
            QuestVr::PrecomputeOriginalBrushSphereFilter(g.nodes.size(),sphere,sphereQueries);
            QuestVr::OriginalBrushRootDestination destination;
            QuestVr::OriginalBrushInsertionQueries insertion{allocation,read,[&](std::int32_t root){brushModel->nativeBrushRoot=root;},
                [&](std::int32_t){++result.attachments;if(failAfterAttachments&&result.attachments==failAfterAttachments)
                    throw std::runtime_error("Injected brush tracker refusal after actual BSP/root attachment");}};
            QuestVr::OriginalBrushPolyFilterQueries filter{read,
                [&](std::int32_t surf,std::int32_t node,std::int32_t coplanar,std::int32_t place,const QuestVr::OriginalBrushClipPolygon& poly){
                    QuestVr::InsertOriginalBrushPolygon(g,prepared->pools,actor,destination,surf,node,coplanar,place,poly,insertion);},
                [this]{LifecycleStep();},[&](std::size_t bytes){moverScratch=QuestVr::DecalStateDetail::Add(moverScratch,bytes);measure(0u);}};
            for(const auto& poly:transformed) {
                LifecycleStep();
                if(poly.link<0||static_cast<std::size_t>(poly.link)<live.sourceSurfaceCount||static_cast<std::size_t>(poly.link)>=g.surfaces.size()||
                    live.dynamicSurfaceReferences[static_cast<std::size_t>(poly.link)-live.sourceSurfaceCount].actor!=actor)
                    throw std::runtime_error("Initial Update polygon LinkIndex lost its typed surface owner");
                const auto& surf=g.surfaces[static_cast<std::size_t>(poly.link)];
                g.points.at(static_cast<std::size_t>(surf.basePoint))=poly.base;
                g.vectors.at(static_cast<std::size_t>(surf.normalVector))=poly.normal;
                g.vectors.at(static_cast<std::size_t>(surf.textureU))=poly.textureU;
                g.vectors.at(static_cast<std::size_t>(surf.textureV))=poly.textureV;
                if(!g.nodes.empty())QuestVr::FilterOriginalBrushPolygon(poly.link,0,-1,
                    QuestVr::MakeOriginalBrushClipPolygon(poly),true,poly.normal,g.nodes.size(),filter);
            }
            if(destination.tail==-1)brushModel->nativeBrushRoot=-1;
            else g.nodes[static_cast<std::size_t>(destination.tail)].renderBound=-1;
            PortableBrushTrackerMover observed;observed.actorPath=actor->reflection.objectPath;observed.root=*brushModel->nativeBrushRoot;
            for(auto node=observed.root;node!=-1;) {
                LifecycleStep();const auto current=read(node);
                if(static_cast<std::size_t>(node)<prepared->pools.original.nodes||
                    prepared->pools.nodes[static_cast<std::size_t>(node)-prepared->pools.original.nodes].owner!=actor)
                    throw std::runtime_error("Initial Update final root chain owner assertion failed");
                g.nodes[static_cast<std::size_t>(node)].nodeFlags&=static_cast<std::uint8_t>(~0x20u);
                ++observed.nodes;observed.vertices+=current.vertexCount;node=current.renderBound;
            }
            ++live.revision;++prepared->revision;
            const auto observationBytes=sizeof(PortableBrushTrackerMover)+observed.actorPath.capacity()+1u;
            measure(observationBytes+(result.movers.size()+1u)*sizeof(PortableBrushTrackerMover));
            result.movers.reserve(result.movers.size()+1u);result.movers.push_back(std::move(observed));
            nativeActorOrderObservationBytes_+=observationBytes;
        }
        moverScratch=65536u;measure(0u);
        // Validate the current index-preserving selected graph/ownership domain.
        // Do not pass zero free records to unchanged raw-source kernels.
        for(std::size_t i=0;i<g.nodes.size();++i) {
            LifecycleStep();if(i>=prepared->pools.original.nodes&&!prepared->pools.nodes[i-prepared->pools.original.nodes].owner)continue;
            const auto node=ReadTrackerNode(*prepared,live,static_cast<std::int32_t>(i));
            if(i>=prepared->pools.original.nodes)++result.activeNodes;
            for(const auto child:{node.back,node.front,node.plane})if(child!=-1)static_cast<void>(ReadTrackerNode(*prepared,live,child));
        }
        for(auto* owner:prepared->pools.vertices)if(owner)++result.activeVertices;
        result.dynamicPoints=prepared->pools.points.size();result.finalCounts=QuestVr::OriginalBrushCounts(g);result.topologyChecked=true;
        prepared->initialUpdateCompleted=true;
        measure(sizeof(BeforeBrushTracker)+sizeof(GCRootNode));beforeBrushTracker_.emplace(level);
        beforeBrushTracker_->state=std::move(level->nativeBrushTracker);level->nativeBrushTracker=std::move(prepared);
        nativeQueryScratchBytes_=QuestVr::DecalStateDetail::Add(previousScratch,
            pending.capacity()*sizeof(GCRoot<RuntimeObject>)+pending.size()*sizeof(GCRootNode));
        result.levelOwned=level->nativeBrushTracker&&level->nativeBrushTracker->model==model&&level->nativeBrushTracker->initialUpdateCompleted;
        if(!result.levelOwned)throw std::runtime_error("Level-owned tracker publication identity failed");
        ValidateStateBudget();return Value::Bool(true);
    }
    Value ProbeNativeBrushTracker(std::size_t failAfterAttachments,PortableBrushTrackerProbe& result) {
        brushGeometryPreview_=true;const auto value=ConstructNativeBrushTracker(failAfterAttachments,result);
        // Probe-only GC; original constructor does not manufacture a GC pass.
        const auto gc=GC::GetStats();GC::Collect();
        if(GC::GetStats().numObjects!=gc.numObjects)throw std::runtime_error("Level-owned tracker GC identity failed");
        result.gcChecked=true;return value;
    }
