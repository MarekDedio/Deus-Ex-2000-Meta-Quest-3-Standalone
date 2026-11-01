    RuntimeNativeBrushTracker& MutableNativeBrushTracker(RuntimeObject* level) {
        RequireNativeModelWritable();
        auto* model=level?level->nativeLevelModel:nullptr;
        auto* current=level?level->nativeBrushTracker.get():nullptr;
        if(!transaction_||level!=CurrentClockLevel()||!model||!model->nativeMutableModel||!current||
            current->level!=level||current->model!=model||current->generation!=level->nativeMapGeneration||
            current->generation!=model->nativeMutableModel->generation||!current->initialUpdateCompleted||current->mutating)
            throw std::runtime_error("Native brush Update lost its completed current Level/Model/tracker");
        if(beforeBrushTracker_) {
            if(beforeBrushTracker_->level.get()!=level)throw std::runtime_error("Brush tracker journal belongs to another Level");
            return *current; // Earliest beforeimage, including absent-before-construction, wins.
        }
        const auto previous=nativeQueryScratchBytes_;
        struct Restore {std::size_t& value;std::size_t prior;~Restore(){value=prior;}} restore{nativeQueryScratchBytes_,previous};
        const auto count=QuestVr::DecalStateDetail::Add(1u,QuestVr::DecalStateDetail::Add(current->pools.nodes.size(),
            QuestVr::DecalStateDetail::Add(current->pools.vertices.size(),current->pools.points.size())));
        const auto rootBytes=QuestVr::DecalStateDetail::Multiply(count,sizeof(GCRoot<RuntimeObject>)+sizeof(GCRootNode));
        const auto hold=[&](std::size_t bytes){nativeQueryScratchBytes_=previous;ValidateStateBudget(bytes);
            nativeQueryScratchBytes_=QuestVr::DecalStateDetail::Add(previous,bytes);};
        hold(QuestVr::DecalStateDetail::Add(NativeBrushTrackerBytes(*current),rootBytes));
        nativeWork_.Step(count+NativeBrushTrackerBytes(*current)/64u+1u);
        std::vector<GCRoot<RuntimeObject>> roots;roots.reserve(count);roots.emplace_back(current->model);
        for(const auto& row:current->pools.nodes)if(row.owner)roots.emplace_back(row.owner);
        for(auto* owner:current->pools.vertices)if(owner)roots.emplace_back(owner);
        for(const auto& row:current->pools.points)if(row.owner)roots.emplace_back(row.owner);
        auto prepared=std::make_unique<RuntimeNativeBrushTracker>(*current);
        hold(NativeBrushTrackerBytes(*prepared)+roots.capacity()*sizeof(GCRoot<RuntimeObject>)+roots.size()*sizeof(GCRootNode));
        ValidateStateBudget(sizeof(BeforeBrushTracker)+sizeof(GCRootNode));
        beforeBrushTracker_.emplace(level);beforeBrushTracker_->references=std::move(roots);
        beforeBrushTracker_->state=std::move(level->nativeBrushTracker);level->nativeBrushTracker=std::move(prepared);
        return *level->nativeBrushTracker;
    }
    RuntimeObject* NativeUpdateBrushModel(RuntimeObject* actor,RuntimeObject* level) {
        const auto selected=NativePrimitiveField(actor,"Engine.Actor.Brush","Engine.Model",0x2au);
        auto* model=selected?selected->object:nullptr;
        if(!model||actor->nativeXLevel!=level||actor->worldRemoved||!actor->active||actor->worldActorIndex<0||
            static_cast<std::size_t>(actor->worldActorIndex)>=persistentWorldActors.size()||
            persistentWorldActors[static_cast<std::size_t>(actor->worldActorIndex)]!=actor||
            model->sourcePath!=level->sourcePath||!model->nativeBrushRoot||
            !model->nativeBrushPolys||!model->nativeBrushPolys->nativeMutablePolys||
            model->nativeBrushPolys->nativeMutablePolys->generation!=level->nativeMapGeneration||
            model->nativeBrushPolys->sourcePath!=model->sourcePath||!model->nativeLinker||!model->nativeLinker->active||
            model->nativeBrushPolys->nativeLinker!=model->nativeLinker)
            throw std::runtime_error("Native brush Update lost its current Actor/Brush/Polys binding");
        return model;
    }
    template<class Measure> void RebuildNativeBrushMover(RuntimeNativeBrushTracker& tracker,RuntimeNativeMutableModel& live,
        RuntimeObject* actor,std::size_t& moverScratch,Measure&& measure,PortableBrushTrackerProbe& result,
        const QuestVr::OriginalBrushPoolQueries& allocation,std::size_t failAfterAttachments) {
        LifecycleStep();moverScratch=65536u;measure(0u);auto& g=live.geometry;
        auto* brushModel=NativeUpdateBrushModel(actor,tracker.level);TouchNativeBrushModel(brushModel);
        const auto& source=brushModel->nativeBrushPolys->nativeMutablePolys->data;
        if(live.revision==std::numeric_limits<std::uint64_t>::max()||tracker.revision==std::numeric_limits<std::uint64_t>::max())
            throw std::runtime_error("Native brush geometry revision exhausted");
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
        const auto read=[&](std::int32_t index){return ReadTrackerNode(tracker,live,index);};
        QuestVr::OriginalBrushSphereFilterQueries sphereQueries{read,[&](std::int32_t index,std::uint8_t flags){
            static_cast<void>(read(index));g.nodes[static_cast<std::size_t>(index)].nodeFlags=flags;},[this]{LifecycleStep();}};
        QuestVr::PrecomputeOriginalBrushSphereFilter(g.nodes.size(),sphere,sphereQueries);
        QuestVr::OriginalBrushRootDestination destination;
        QuestVr::OriginalBrushInsertionQueries insertion{allocation,read,[&](std::int32_t root){brushModel->nativeBrushRoot=root;},
            [&](std::int32_t){++result.attachments;if(failAfterAttachments&&result.attachments==failAfterAttachments)
                throw std::runtime_error("Injected brush tracker refusal after actual BSP/root attachment");}};
        QuestVr::OriginalBrushPolyFilterQueries filter{read,
            [&](std::int32_t surf,std::int32_t node,std::int32_t coplanar,std::int32_t place,const QuestVr::OriginalBrushClipPolygon& poly){
                QuestVr::InsertOriginalBrushPolygon(g,tracker.pools,actor,destination,surf,node,coplanar,place,poly,insertion);},
            [this]{LifecycleStep();},[&](std::size_t bytes){moverScratch=QuestVr::DecalStateDetail::Add(moverScratch,bytes);measure(0u);}};
        for(const auto& poly:transformed) {
            LifecycleStep();
            if(poly.link<0||static_cast<std::size_t>(poly.link)<live.sourceSurfaceCount||static_cast<std::size_t>(poly.link)>=g.surfaces.size()||
                live.dynamicSurfaceReferences[static_cast<std::size_t>(poly.link)-live.sourceSurfaceCount].actor!=actor)
                throw std::runtime_error("Update polygon LinkIndex lost its typed surface owner");
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
            if(static_cast<std::size_t>(node)<tracker.pools.original.nodes||
                tracker.pools.nodes[static_cast<std::size_t>(node)-tracker.pools.original.nodes].owner!=actor)
                throw std::runtime_error("Update final root chain owner assertion failed");
            g.nodes[static_cast<std::size_t>(node)].nodeFlags&=static_cast<std::uint8_t>(~0x20u);
            ++observed.nodes;observed.vertices+=current.vertexCount;node=current.renderBound;
        }
        ++live.revision;++tracker.revision;
        const auto observationBytes=sizeof(PortableBrushTrackerMover)+observed.actorPath.capacity()+1u;
        measure(observationBytes+(result.movers.size()+1u)*sizeof(PortableBrushTrackerMover));
        result.movers.reserve(result.movers.size()+1u);result.movers.push_back(std::move(observed));
        nativeActorOrderObservationBytes_+=observationBytes;
    }
    bool NativeBrushPoseChanged(RuntimeObject* actor) {
        const auto current=NativeCoordsRead(actor,"Engine.Actor.Location",Kind::Vector,0x23u);
        const auto previous=Read(actor,MovingBrushStructField(actor,"Engine.Mover.OldPos",Kind::Vector,1u),0u);
        for(std::size_t c=0;c<3u;++c) {
            if(!std::isfinite(current.vector[c])||!std::isfinite(previous.vector[c]))
                throw std::runtime_error("Original brush Update requires finite active pose comparisons");
            if(current.vector[c]!=previous.vector[c])return true;
        }
        return NativeCoordsRead(actor,"Engine.Actor.Rotation",Kind::Rotator,0x23u).rotation!=
            Read(actor,MovingBrushStructField(actor,"Engine.Mover.OldRot",Kind::Rotator,1u),0u).rotation;
    }
    void UpdateNativeBrushTracker(RuntimeObject* supplied,PortableBrushUpdateProbe& result,std::size_t failureStage=0u) {
        RequireNativeModelWritable();auto* level=CurrentClockLevel();auto* model=level->nativeLevelModel;
        auto* existing=level->nativeBrushTracker.get();
        if(!transaction_||!model||!model->nativeMutableModel||!existing||existing->level!=level||existing->model!=model||
            existing->generation!=level->nativeMapGeneration||existing->generation!=persistentMapGenerationCounter||
            model->nativeMutableModel->generation!=existing->generation||!existing->initialUpdateCompleted||existing->mutating)
            throw std::runtime_error("Original noninitial Update requires its actual completed tracker");
        if(supplied&&!IsDerivedFromPath(supplied->cls,"Engine.Mover"))supplied=nullptr;
        if(supplied) {
            static_cast<void>(NativeUpdateBrushModel(supplied,level));
            if(!NativeBrushPoseChanged(supplied)){result.unchangedSkipped=true;return;}
            if(Read(supplied,MovingBrushScalarField(supplied,"Engine.Actor.bAssimilated","Engine.Actor",Kind::Bool,0x2002u),0u).boolean) {
                result.assimilatedSkipped=true;return;
            }
        }
        if(!supplied) {
            bool any{};
            // Read-only early admission, not a snapshot replacing the later
            // live Actor walk. A genuinely empty/all-assimilated selection must
            // not create geometry journals and spuriously advance world revision.
            for(std::size_t slot=0;slot<persistentWorldActors.size();++slot) {
                LifecycleStep();auto* actor=persistentWorldActors[slot];if(!actor)continue;
                if(NativePrimitiveField(actor,"Engine.Actor.Brush","Engine.Model",0x2au)&&IsDerivedFromPath(actor->cls,"Engine.Brush")&&!NativeActorStatic(actor)&&
                    !Read(actor,MovingBrushScalarField(actor,"Engine.Actor.bAssimilated","Engine.Actor",Kind::Bool,0x2002u),0u).boolean){any=true;break;}
            }
            if(!any){result.emptySkipped=true;return;}
        }
        auto& live=MutableRootModel(level);auto& tracker=MutableNativeBrushTracker(level);auto& g=live.geometry;
        tracker.mutating=true;struct EndMutation {RuntimeNativeBrushTracker& value;~EndMutation(){value.mutating=false;}} endMutation{tracker};
        struct Pending {GCRoot<RuntimeObject> actor;std::size_t next;Pending(RuntimeObject* a,std::size_t n):actor(a),next(n){}};
        constexpr auto none=std::numeric_limits<std::size_t>::max();std::vector<Pending> pending;std::size_t head=none;
        std::vector<std::uint8_t> released;std::size_t moverScratch{},frameScratch{};
        const auto previous=nativeQueryScratchBytes_;
        struct Restore {std::size_t& value;std::size_t prior;~Restore(){value=prior;}} restore{nativeQueryScratchBytes_,previous};
        const auto measure=[&](std::size_t extra){nativeQueryScratchBytes_=previous;
            const auto bytes=pending.capacity()*sizeof(Pending)+pending.size()*sizeof(GCRootNode)+released.capacity()+moverScratch+frameScratch+8192u;
            ValidateStateBudget(QuestVr::DecalStateDetail::Add(bytes,extra));
            nativeQueryScratchBytes_=QuestVr::DecalStateDetail::Add(previous,QuestVr::DecalStateDetail::Add(bytes,extra));};
        measure(tracker.pools.nodes.size());released.resize(tracker.pools.nodes.size());measure(0u);
        const auto check=[&]{if(!transaction_||level!=CurrentClockLevel()||level->nativeLevelModel!=model||
            model->nativeMutableModel.get()!=&live||level->nativeBrushTracker.get()!=&tracker||!tracker.mutating||
            live.generation!=level->nativeMapGeneration||tracker.generation!=live.generation)
                throw std::runtime_error("Brush mutation scope lost current owning payloads");};
        const auto enqueue=[&](RuntimeObject* actor) {
            check();static_cast<void>(NativeUpdateBrushModel(actor,level));
            const auto assimilated=MovingBrushScalarField(actor,"Engine.Actor.bAssimilated","Engine.Actor",Kind::Bool,0x2002u);
            if(Read(actor,assimilated,0u).boolean)return;
            ReferenceFor(actor,assimilated,0u,false)->write(Value::Bool(true));
            if(pending.size()>=persistentWorldActors.size())throw std::runtime_error("Brush pending list exceeds actual Actor cohort");
            measure((pending.size()+1u)*sizeof(Pending)+sizeof(GCRootNode));
            pending.reserve(pending.size()+1u);pending.emplace_back(actor,head);head=pending.size()-1u;measure(0u);
            ReferenceFor(actor,MovingBrushStructField(actor,"Engine.Mover.OldPos",Kind::Vector,1u),0u,false)->write(
                NativeCoordsRead(actor,"Engine.Actor.Location",Kind::Vector,0x23u));
            ReferenceFor(actor,MovingBrushStructField(actor,"Engine.Mover.OldRot",Kind::Rotator,1u),0u,false)->write(
                NativeCoordsRead(actor,"Engine.Actor.Rotation",Kind::Rotator,0x23u));
        };
        if(supplied)enqueue(supplied);
        else for(std::size_t slot=0;slot<persistentWorldActors.size();++slot) {
            LifecycleStep();auto* actor=persistentWorldActors[slot];if(!actor)continue;
            if(NativePrimitiveField(actor,"Engine.Actor.Brush","Engine.Model",0x2au)&&IsDerivedFromPath(actor->cls,"Engine.Brush")&&!NativeActorStatic(actor))enqueue(actor);
        }
        QuestVr::OriginalBrushDependencyQueries<RuntimeObject> dependency;
        dependency.readOwnedNode=[&](std::int32_t index){check();return ReadTrackerNode(tracker,live,index);};
        dependency.enqueueIfUnassimilated=enqueue;dependency.work=[this]{LifecycleStep();};
        dependency.enterFrame=[&](std::size_t bytes){frameScratch=QuestVr::DecalStateDetail::Add(frameScratch,bytes);measure(0u);};
        dependency.leaveFrame=[&](std::size_t bytes)noexcept{frameScratch-=bytes;};
        const auto pass1Head=head;
        for(auto entry=pass1Head;entry!=none;entry=pending.at(entry).next) {
            LifecycleStep();++result.pass1Movers;auto* actor=pending.at(entry).actor.get();auto* brush=NativeUpdateBrushModel(actor,level);
            for(auto node=*brush->nativeBrushRoot;node!=-1;) {
                LifecycleStep();QuestVr::PropagateOriginalBrushDependency(g,tracker.pools,node,dependency);
                node=ReadTrackerNode(tracker,live,node).renderBound;
            }
        }
        result.pending=pending.size();if(failureStage==1u)throw std::runtime_error("Injected noninitial brush failure after dependency/cache writes");
        QuestVr::OriginalBrushFlushQueries<RuntimeObject> flush;
        flush.pool={ [&,this]{LifecycleStep();if(failureStage==4u&&tracker.pools.freePoint!=-1)
                throw std::runtime_error("Injected noninitial brush failure after partial point free");},measure};
        flush.readOwnedNode=dependency.readOwnedNode;
        flush.readBrushRoot=[&](RuntimeObject* actor){return *NativeUpdateBrushModel(actor,level)->nativeBrushRoot;};
        flush.writeBrushRoot=[&](RuntimeObject* actor,std::int32_t root){check();auto* brush=NativeUpdateBrushModel(actor,level);TouchNativeBrushModel(brush);brush->nativeBrushRoot=root;};
        flush.admitRecordedParent=[&](std::int32_t parent,std::int32_t child){check();LifecycleStep();
            if(child<0||static_cast<std::size_t>(child)<tracker.pools.original.nodes||
                static_cast<std::size_t>(child)-tracker.pools.original.nodes>=tracker.pools.nodes.size()||
                tracker.pools.nodes[static_cast<std::size_t>(child)-tracker.pools.original.nodes].parent!=parent||
                parent<0||static_cast<std::size_t>(parent)>=g.nodes.size())throw std::runtime_error("Brush flush lost recorded parent identity");
            if(static_cast<std::size_t>(parent)<tracker.pools.original.nodes||tracker.pools.nodes[static_cast<std::size_t>(parent)-tracker.pools.original.nodes].owner)
                static_cast<void>(ReadTrackerNode(tracker,live,parent));
            else if(!released.at(static_cast<std::size_t>(parent)-tracker.pools.original.nodes))
                throw std::runtime_error("Brush flush selected an unrelated free parent");
        };
        flush.recordReleasedNode=[&](std::int32_t index){check();const auto offset=static_cast<std::size_t>(index)-tracker.pools.original.nodes;
            if(index<0||static_cast<std::size_t>(index)<tracker.pools.original.nodes||offset>=released.size()||released[offset]||
                tracker.pools.nodes[offset].owner||tracker.pools.nodes[offset].parent!=0)
                throw std::runtime_error("Brush flush release lost same-pass active identity");
            released[offset]=1u;++result.releasedNodes;result.releasedPoints+=g.nodes[static_cast<std::size_t>(index)].vertexCount;};
        flush.admitReleasedNode=[&](std::int32_t index){check();if(index<0||static_cast<std::size_t>(index)<tracker.pools.original.nodes||
            static_cast<std::size_t>(index)-tracker.pools.original.nodes>=released.size()||!released[static_cast<std::size_t>(index)-tracker.pools.original.nodes])
                throw std::runtime_error("Brush flush lacks same-pass released-node authority");};
        for(auto entry=head;entry!=none;entry=pending.at(entry).next) {
            LifecycleStep();auto* actor=pending.at(entry).actor.get();TouchNativeBrushModel(NativeUpdateBrushModel(actor,level));
            QuestVr::FlushOriginalBrushMover(g,tracker.pools,actor,flush);++result.flushedMovers;
            if(failureStage==2u)throw std::runtime_error("Injected noninitial brush failure after ordered flush");
        }
        const QuestVr::OriginalBrushPoolQueries allocation{[this]{LifecycleStep();},measure};
        for(auto entry=head;entry!=none;entry=pending.at(entry).next)
            RebuildNativeBrushMover(tracker,live,pending.at(entry).actor.get(),moverScratch,measure,result.rebuilt,allocation,failureStage==3u?1u:0u);
        moverScratch=65536u;measure(0u);
        for(std::size_t i=0;i<g.nodes.size();++i) {
            LifecycleStep();if(i>=tracker.pools.original.nodes&&!tracker.pools.nodes[i-tracker.pools.original.nodes].owner)continue;
            const auto node=ReadTrackerNode(tracker,live,static_cast<std::int32_t>(i));
            if(i>=tracker.pools.original.nodes)++result.rebuilt.activeNodes;
            for(auto child:{node.front,node.back,node.plane})if(child!=-1)static_cast<void>(ReadTrackerNode(tracker,live,child));
        }
        for(auto* owner:tracker.pools.vertices)if(owner)++result.rebuilt.activeVertices;
        result.rebuilt.dynamicPoints=tracker.pools.points.size();result.rebuilt.finalCounts=QuestVr::OriginalBrushCounts(g);
        result.topologyChecked=result.rebuilt.topologyChecked=true;
        result.afterModelRevision=live.revision;result.afterTrackerRevision=tracker.revision;check();ValidateStateBudget();
    }
    Value ProbeNativeBrushUpdate(bool full,std::size_t failureStage,PortableBrushUpdateProbe& result) {
        brushGeometryPreview_=true;ConstructNativeBrushTracker(0u,result.initial);
        auto* level=CurrentClockLevel();auto* model=level->nativeLevelModel;auto& tracker=*level->nativeBrushTracker;
        RuntimeObject* actor{};for(const auto& row:tracker.pools.nodes)if(row.owner){actor=row.owner;break;}
        if(!actor)throw std::runtime_error("Actual Training has no initialized mover to update");
        auto* originalPayload=model->nativeMutableModel.get();auto* originalTracker=level->nativeBrushTracker.get();
        const auto initialModelRevision=originalPayload->revision,initialTrackerRevision=tracker.revision;
        PortableBrushUpdateProbe unchanged;UpdateNativeBrushTracker(actor,unchanged);result.unchangedSkipped=unchanged.unchangedSkipped;
        if(model->nativeMutableModel.get()!=originalPayload||level->nativeBrushTracker.get()!=originalTracker||
            originalPayload->revision!=initialModelRevision||tracker.revision!=initialTrackerRevision)
            throw std::runtime_error("Unchanged brush Update mutated payload/revision");
        for(const auto& row:result.initial.movers) {
            auto* current=Object(row.actorPath);const auto field=MovingBrushScalarField(current,"Engine.Actor.bAssimilated","Engine.Actor",Kind::Bool,0x2002u);
            if(Read(current,field,0u).boolean)throw std::runtime_error("Initial tracker left an assimilated mover");
            ReferenceFor(current,field,0u,false)->write(Value::Bool(true));
        }
        PortableBrushUpdateProbe empty;UpdateNativeBrushTracker(nullptr,empty);result.emptySkipped=empty.emptySkipped;
        if(model->nativeMutableModel.get()!=originalPayload||level->nativeBrushTracker.get()!=originalTracker||
            originalPayload->revision!=initialModelRevision||tracker.revision!=initialTrackerRevision)
            throw std::runtime_error("Empty full brush Update mutated payload/revision");
        for(const auto& row:result.initial.movers) {
            auto* current=Object(row.actorPath);
            ReferenceFor(current,MovingBrushScalarField(current,"Engine.Actor.bAssimilated","Engine.Actor",Kind::Bool,0x2002u),0u,false)->write(Value::Bool(false));
        }
        auto location=NativeCoordsRead(actor,"Engine.Actor.Location",Kind::Vector,0x23u);location.vector[0]+=16.0f;
        ReferenceFor(actor,MovingBrushLocationField(actor),0u,false)->write(location);
        const auto assimilated=MovingBrushScalarField(actor,"Engine.Actor.bAssimilated","Engine.Actor",Kind::Bool,0x2002u);
        ReferenceFor(actor,assimilated,0u,false)->write(Value::Bool(true));
        PortableBrushUpdateProbe already;UpdateNativeBrushTracker(actor,already);result.assimilatedSkipped=already.assimilatedSkipped;
        if(model->nativeMutableModel.get()!=originalPayload||level->nativeBrushTracker.get()!=originalTracker||
            originalPayload->revision!=initialModelRevision||tracker.revision!=initialTrackerRevision)
            throw std::runtime_error("Assimilated brush Update mutated payload/revision");
        ReferenceFor(actor,assimilated,0u,false)->write(Value::Bool(false));
        const auto oldRoot=*NativeUpdateBrushModel(actor,level)->nativeBrushRoot;
        result.beforeModelRevision=model->nativeMutableModel->revision;result.beforeTrackerRevision=tracker.revision;
        UpdateNativeBrushTracker(full?nullptr:actor,result,failureStage);
        const auto prior=nativeLiveModelNodeReads_;
        WithNativeWorldModelQuery(level,[&](const auto& current){
            return QuestVr::ModelPointRegionDetail::Query(current,{location.vector[0],location.vector[1],location.vector[2]},65536u,[this]{LifecycleStep();});});
        result.currentQueryObserved=nativeLiveModelNodeReads_>prior;
        try {WithNativeWorldModelQuery(level,[&](const auto& current){static_cast<void>(current.nodes.at(static_cast<std::size_t>(oldRoot)));return true;});}
        catch(const std::exception& error){result.releasedReadRefused=std::string(error.what()).find("free dynamic")!=std::string::npos;}
        for(unsigned pass=0;pass<2u;++pass) {
            RuntimeObject* selected=actor;
            if(pass)for(const auto& row:result.initial.movers){auto* candidate=Object(row.actorPath);if(candidate!=actor){selected=candidate;break;}}
            if(pass&&selected==actor)throw std::runtime_error("Training repeat update lacks another actual mover");
            const auto retired=*NativeUpdateBrushModel(selected,level)->nativeBrushRoot;
            auto next=NativeCoordsRead(selected,"Engine.Actor.Location",Kind::Vector,0x23u);next.vector[0]+=8.0f;
            ReferenceFor(selected,MovingBrushLocationField(selected),0u,false)->write(next);
            PortableBrushUpdateProbe repeated;UpdateNativeBrushTracker(pass?nullptr:selected,repeated);
            if(!repeated.topologyChecked||!repeated.releasedNodes||!repeated.rebuilt.attachments)
                throw std::runtime_error("Repeated Training Update omitted real release/rebuild");
            bool refused{};
            try{WithNativeWorldModelQuery(level,[&](const auto& current){static_cast<void>(current.nodes.at(static_cast<std::size_t>(retired)));return true;});}
            catch(const std::exception& error){refused=std::string(error.what()).find("free dynamic")!=std::string::npos;}
            if(!refused)throw std::runtime_error("Repeated Update retained free-node query authority");
            const auto dynamicReads=nativeLiveModelDynamicReads_;
            QuestVr::NativeScratchScope workspace(nativeQueryScratchBytes_,
                QuestVr::OriginalModelLineScratchBytes(model->nativeMutableModel->geometry.nodes.size(),true),
                [this](std::size_t extra){ValidateStateBudget(extra);});
            static_cast<void>(NativeFastLine(selected,{next.vector[0],next.vector[1],next.vector[2]+32.0f},
                {next.vector[0],next.vector[1],next.vector[2]-32.0f}));
            if(nativeLiveModelDynamicReads_<=dynamicReads)throw std::runtime_error("Repeated Update query omitted current dynamic nodes");
            ++result.repeatedUpdates;result.repeatedReleasedNodes+=repeated.releasedNodes;result.repeatedAttachments+=repeated.rebuilt.attachments;
        }
        return Value::Bool(true);
    }
