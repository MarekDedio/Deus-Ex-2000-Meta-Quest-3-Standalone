    void RequireNativeModelWritable()const {
        if(nativeModelReaders_||persistentLiveModelReaders)
            throw std::runtime_error("Native Model/tracker mutation is forbidden during a live query lease");
    }
    template<class Query>
    auto WithNativeWorldModelQuery(RuntimeObject* level,Query&& query)
        -> std::invoke_result_t<Query,const PortableModelGeometry&> {
        using Result=std::invoke_result_t<Query,const PortableModelGeometry&>;
        static_assert(!std::is_reference_v<Result> && !std::is_void_v<Result>,"Native query must return a detached value");
        const auto& source=NativeLevelTraceModel(level);auto* model=level->nativeLevelModel;
        if(!model->nativeMutableModel)return query(source);
        if(!transaction_||!level->nativeBrushTracker||!level->nativeBrushTracker->initialUpdateCompleted||level->nativeBrushTracker->mutating)
            throw std::runtime_error("Current mutable Model query requires a completed, nonmutating actual Level tracker");
        auto* payload=model->nativeMutableModel.get();auto* tracker=level->nativeBrushTracker.get();
        if(&source!=&payload->geometry||tracker->model!=model||tracker->level!=level||
            tracker->generation!=level->nativeMapGeneration||payload->generation!=level->nativeMapGeneration)
            throw std::runtime_error("Live Model query lost actual Level/Model/tracker ownership");
        QuestVr::NativeScratchScope scratch(nativeQueryScratchBytes_,4096u,[this](std::size_t extra){ValidateStateBudget(extra);});
        // Pins are lexical, NOT held by an escaped/dead proxy or a source cache.
        GCRoot<RuntimeObject> levelPin(level),modelPin(model);
        struct Control {bool active{true};};auto control=std::make_shared<Control>();
        if(nativeModelReaders_==std::numeric_limits<std::size_t>::max()||persistentLiveModelReaders==std::numeric_limits<std::size_t>::max())
            throw std::runtime_error("Native Model read depth exhausted");
        ++nativeModelReaders_;++persistentLiveModelReaders;
        struct Release {
            std::shared_ptr<Control> control;std::size_t& readers;std::size_t& shared;
            ~Release(){control->active=false;if(!readers||!shared)std::terminate();--readers;--shared;}
        } release{control,nativeModelReaders_,persistentLiveModelReaders};
        const auto root=nativeRootSerial_,generation=level->nativeMapGeneration,
            modelRevision=payload->revision,trackerRevision=tracker->revision;
        const auto* nodes=payload->geometry.nodes.data();const auto nodeCount=payload->geometry.nodes.size();
        const auto check=[this,control,level,model,payload,tracker,root,generation,modelRevision,trackerRevision,nodes,nodeCount] {
            // The dead control is read FIRST: captured host/payload addresses
            // may no longer exist after scope/root/owner replacement.
            if(!control->active)throw std::runtime_error("Native Model query lease is expired");
            if(!transaction_||nativeRootSerial_!=root||!nativeModelReaders_||!persistentLiveModelReaders||
                persistentWorldActors.empty()||persistentWorldActors.front()->nativeXLevel!=level||
                level->nativeLevelModel!=model||level->nativeMapGeneration!=generation||generation!=persistentMapGenerationCounter||
                model->nativeMutableModel.get()!=payload||level->nativeBrushTracker.get()!=tracker)
                throw std::runtime_error("Native Model query root/payload identity changed");
            if(payload->generation!=generation||tracker->generation!=generation||tracker->level!=level||tracker->model!=model||
                payload->revision!=modelRevision||tracker->revision!=trackerRevision||!tracker->initialUpdateCompleted||tracker->mutating||
                payload->geometry.nodes.data()!=nodes||payload->geometry.nodes.size()!=nodeCount||
                !model->nativeLinker||!model->nativeLinker->active)
                throw std::runtime_error("Native Model query generation/revision/owner state changed");
        };
        const auto active=[this,check,payload,tracker](std::size_t index) {
            check();LifecycleStep();const auto& pools=tracker->pools;
            if(pools.original.nodes>payload->geometry.nodes.size()||payload->geometry.nodes.size()-pools.original.nodes!=pools.nodes.size())
                throw std::runtime_error("Live Model node pool owner counts changed");
            return index<pools.original.nodes||pools.nodes[index-pools.original.nodes].owner!=nullptr;
        };
        const auto admit=[this,check,payload,tracker](std::size_t index) {
            check();LifecycleStep();static_cast<void>(ReadTrackerNode(*tracker,*payload,static_cast<std::int32_t>(index)));
            ++nativeLiveModelNodeReads_;if(index>=tracker->pools.original.nodes)++nativeLiveModelDynamicReads_;
        };
        check();const auto& g=payload->geometry;
        const RuntimeLiveModelView view{RuntimeLiveModelNodes(g.nodes,check,active,admit),g.version,g.licenseeMode,g.rootOutside,g.leafHulls,g.zones};
        static_assert(std::is_same_v<Result,std::invoke_result_t<Query,const RuntimeLiveModelView&>>,
            "Raw/live native query results must have the same detached type");
        auto result=query(view);check();return result;
    }
    Value ProbeNativeLiveModelQueries(PortableLiveModelQueryProbe& result,std::function<void()>& expired) {
        brushGeometryPreview_=true;
        ConstructNativeBrushTracker(0u,result.tracker);
        auto* level=CurrentClockLevel();auto* model=level->nativeLevelModel;auto& live=*model->nativeMutableModel;
        auto& tracker=*level->nativeBrushTracker;const auto& g=live.geometry;
        result.originalNodes=tracker.pools.original.nodes;result.modelRevision=live.revision;result.trackerRevision=tracker.revision;
        // Detached observations + deliberately escaped DEAD test callback have
        // bounded lifetime outside the query scope but within this probe owner.
        ValidateStateBudget(16384u+8u*sizeof(PortableLiveModelQuerySample));
        nativeActorOrderObservationBytes_+=16384u+8u*sizeof(PortableLiveModelQuerySample);result.samples.reserve(8u);
        const auto priorReads=nativeLiveModelNodeReads_,priorDynamic=nativeLiveModelDynamicReads_;
        std::array<RuntimeObject*,8u> sampled{};std::size_t sampledCount{};std::int32_t freeIndex=-1;
        for(std::size_t i=0;i<tracker.pools.nodes.size();++i) {
            LifecycleStep();auto* actor=tracker.pools.nodes[i].owner;
            if(!actor){++result.freeNodes;if(freeIndex==-1)freeIndex=static_cast<std::int32_t>(tracker.pools.original.nodes+i);continue;}
            if(sampledCount==sampled.size()||std::find(sampled.begin(),sampled.begin()+sampledCount,actor)!=sampled.begin()+sampledCount)continue;
            const auto node=ReadTrackerNode(tracker,live,static_cast<std::int32_t>(tracker.pools.original.nodes+i));
            if(!node.vertexCount)continue;
            const auto point=g.vertices.at(static_cast<std::size_t>(node.vertexPool)).point;
            const auto center=g.points.at(static_cast<std::size_t>(point));
            PortableLiveModelQuerySample sample;
            sample.start={center.x+node.planeX*32.0f,center.y+node.planeY*32.0f,center.z+node.planeZ*32.0f};
            sample.end={center.x-node.planeX*32.0f,center.y-node.planeY*32.0f,center.z-node.planeZ*32.0f};sample.extent={0.5f,0.5f,0.5f};
            const auto traceScratch=QuestVr::OriginalModelLineScratchBytes(g.nodes.size(),true);
            QuestVr::NativeScratchScope queryWorkspace(nativeQueryScratchBytes_,std::max(traceScratch,
                QuestVr::DecalStateDetail::Add(4096u,QuestVr::DecalStateDetail::Multiply(g.nodes.size(),64u))),
                [this](std::size_t extra){ValidateStateBudget(extra);});
            const auto remaining=[&] {
                if(fastTraceValidationWork_>=MaximumNativeModelValidationWork||fastTraceQueryWork_>=MaximumNativeModelQueryWork)
                    throw std::runtime_error("Live Model diagnostic exhausted shared query allowance");
            };
            remaining();QuestVr::ModelPointTraceLimits pointLimits;
            pointLimits.validationSteps=std::min(pointLimits.validationSteps,MaximumNativeModelValidationWork-fastTraceValidationWork_);
            pointLimits.querySteps=std::min(pointLimits.querySteps,MaximumNativeModelQueryWork-fastTraceQueryWork_);
            const auto fast=WithNativeWorldModelQuery(level,[&](const auto& current){
                return QuestVr::ModelFastLineDetail::Check(current,sample.end,sample.start,pointLimits);
            });
            sample.fastClear=fast.clear;sample.fastVisits=fast.stats.nodeVisits;
            fastTraceValidationWork_+=fast.stats.validationSteps;fastTraceQueryWork_+=fast.stats.querySteps;
            remaining();pointLimits.validationSteps=std::min(pointLimits.validationSteps,MaximumNativeModelValidationWork-fastTraceValidationWork_);
            pointLimits.querySteps=std::min(pointLimits.querySteps,MaximumNativeModelQueryWork-fastTraceQueryWork_);
            const auto capture=[]()->QuestVr::OriginalModelCoords{throw std::runtime_error("World live query requested inactive Owner.ToWorld");};
            QuestVr::OriginalActorLineHit hit{};hit.item=-1;QuestVr::ModelPointTraceStats pointStats;
            sample.pointClear=WithNativeWorldModelQuery(level,[&](const auto& current){
                return QuestVr::ModelPointLineDetail::Check(hit,current,reinterpret_cast<std::uintptr_t>(model),0u,
                    sample.end,sample.start,6u,capture,pointLimits,&pointStats);
            });
            sample.pointNode=hit.item;sample.pointTime=hit.time;sample.pointVisits=pointStats.nodeVisits;
            fastTraceValidationWork_+=pointStats.validationSteps;fastTraceQueryWork_+=pointStats.querySteps;
            remaining();QuestVr::ModelExtentTraceLimits extentLimits;
            extentLimits.validationSteps=std::min(extentLimits.validationSteps,MaximumNativeModelValidationWork-fastTraceValidationWork_);
            extentLimits.querySteps=std::min(extentLimits.querySteps,MaximumNativeModelQueryWork-fastTraceQueryWork_);
            QuestVr::ModelExtentTraceStats extentStats;
            sample.extentClear=WithNativeWorldModelQuery(level,[&](const auto& current){
                return QuestVr::ModelExtentTraceDetail::Check(hit,current,reinterpret_cast<std::uintptr_t>(model),0u,nullptr,
                    sample.end,sample.start,sample.extent,extentLimits,&extentStats);
            });
            sample.extentTime=hit.time;sample.extentVisits=extentStats.nodeVisits;
            fastTraceValidationWork_+=extentStats.validationSteps;fastTraceQueryWork_+=extentStats.querySteps;
            for(bool box:{false,true}) {
                remaining();QuestVr::ModelPointOverlapLimits overlapLimits;
                overlapLimits.validationSteps=std::min(overlapLimits.validationSteps,MaximumNativeModelValidationWork-fastTraceValidationWork_);
                overlapLimits.querySteps=std::min(overlapLimits.querySteps,MaximumNativeModelQueryWork-fastTraceQueryWork_);
                QuestVr::ModelPointOverlapStats overlapStats;
                const auto clear=WithNativeWorldModelQuery(level,[&](const auto& current){
                    return QuestVr::ModelPointOverlapDetail::Check(hit,current,reinterpret_cast<std::uintptr_t>(model),0u,
                        center,box?sample.extent:PortableModelVec3{},0u,capture,overlapLimits,&overlapStats);
                });
                if(box){sample.boxClear=clear;sample.boxVisits=overlapStats.nodeVisits;}
                else {sample.zeroClear=clear;sample.zeroVisits=overlapStats.nodeVisits;}
                fastTraceValidationWork_+=overlapStats.validationSteps;fastTraceQueryWork_+=overlapStats.querySteps;
            }
            const auto region=WithNativeWorldModelQuery(level,[&](const auto& current){
                return QuestVr::ModelPointRegionDetail::Query(current,center,65'536u,[this]{LifecycleStep();});
            });
            sample.leaf=region.leaf;sample.zone=region.zoneNumber;sample.regionVisits=region.nodeVisits;
            result.samples.push_back(sample);sampled[sampledCount++]=actor;
        }
        if(freeIndex==-1)throw std::runtime_error("Actual Training tracker has no free pool control");
        const auto refusal=[&](auto run,const char* expected) {
            try{run();}catch(const std::exception& error){return std::string(error.what()).find(expected)!=std::string::npos;}
            return false;
        };
        result.freeReadRefused=refusal([&]{WithNativeWorldModelQuery(level,[&](const auto& current){
            static_cast<void>(current.nodes.at(static_cast<std::size_t>(freeIndex)));return true;});},"free dynamic");
        result.mutationRefused=refusal([&]{WithNativeWorldModelQuery(level,[&](const auto&){
            static_cast<void>(MutableRootModel(level));return true;});},"forbidden during a live query");
        result.independentRootRefused=refusal([&]{WithNativeWorldModelQuery(level,[&](const auto&){
            PortableActorVmHost another;another.BeginRoot(QuestVr::Vm::Limits{});return true;
        });},"active live Model query lease");
        result.shutdownRefused=refusal([&]{WithNativeWorldModelQuery(level,[](const auto&){
            ShutdownPortableRuntime();return true;
        });},"active live Model query lease");
        result.unloadRefused=refusal([&]{WithNativeWorldModelQuery(level,[](const auto&){
            static_cast<void>(UnloadPortableRuntimeMap());return true;
        });},"active live Model query lease");
        result.mapLoadRefused=refusal([&]{WithNativeWorldModelQuery(level,[&](const auto&){
            static_cast<void>(LoadPortableRuntimeMap(Table(level->sourcePath)));return true;
        });},"active live Model query lease");
        {
            const auto revision=live.revision;
            struct Restore {std::uint64_t& value;std::uint64_t before;~Restore(){value=before;}} restore{live.revision,revision};
            result.modelRevisionRefused=refusal([&]{WithNativeWorldModelQuery(level,[&](const auto&){++live.revision;return true;});},"revision/owner");
        }
        {
            const auto revision=tracker.revision;
            struct Restore {std::uint64_t& value;std::uint64_t before;~Restore(){value=before;}} restore{tracker.revision,revision};
            result.trackerRevisionRefused=refusal([&]{WithNativeWorldModelQuery(level,[&](const auto&){++tracker.revision;return true;});},"revision/owner");
        }
        {
            std::unique_ptr<RuntimeNativeMutableModel> removed;
            struct Restore {RuntimeObject* model;std::unique_ptr<RuntimeNativeMutableModel>& prior;
                ~Restore(){if(prior)model->nativeMutableModel.swap(prior);}} restore{model,removed};
            result.payloadReplacementRefused=refusal([&]{WithNativeWorldModelQuery(level,[&](const auto&){
                model->nativeMutableModel.swap(removed);return true;});},"root/payload identity");
        }
        tracker.mutating=true;
        struct RestoreMutating {RuntimeNativeBrushTracker& tracker;~RestoreMutating(){tracker.mutating=false;}} restoreMutating{tracker};
        result.mutatingTrackerRefused=refusal([&]{WithNativeWorldModelQuery(level,[](const auto&){return true;});},"nonmutating");
        tracker.mutating=false;
        result.sourceProofRefused=refusal([&]{static_cast<void>(NativeWorldCollisionSource(level));},"cannot borrow an immutable");
        WithNativeWorldModelQuery(level,[&](const auto& current) {
            if constexpr(!std::is_same_v<std::decay_t<decltype(current)>,PortableModelGeometry>) {
                expired=[nodes=current.nodes]{nodes.check();};
            }
            return true;
        });
        result.allNodeReads=nativeLiveModelNodeReads_-priorReads;result.dynamicNodeReads=nativeLiveModelDynamicReads_-priorDynamic;
        ValidateStateBudget();return Value::Bool(true);
    }
