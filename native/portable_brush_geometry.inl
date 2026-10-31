    RuntimeNativeMutableModel& MutableRootModel(RuntimeObject* level) {
        if(!transaction_)throw std::runtime_error("Native Model mutation requires its outer VM root");
        auto* model=level?level->nativeLevelModel:nullptr;
        ValidateNativeLevelModel(level,model);
        if(!level->nativeMapGeneration||level->nativeMapGeneration!=persistentMapGenerationCounter)
            throw std::runtime_error("Native Model mutation has no current map generation");
        if(beforeMutableModel_) {
            if(beforeMutableModel_->model.get()!=model||!model->nativeMutableModel)
                throw std::runtime_error("Native Model journal belongs to another root Model");
            return *model->nativeMutableModel;
        }
        const auto selected=NativePrimitiveAsset(model);
        if(selected.kind!=NativePrimitiveKind::Model)
            throw std::runtime_error("Native mutable geometry is not its actual Model export");
        std::unique_ptr<RuntimeNativeMutableModel> prepared;
        std::vector<GCRoot<RuntimeObject>> oldReferences;
        if(model->nativeMutableModel) {
            const auto& source=*model->nativeMutableModel;
            if(source.generation!=level->nativeMapGeneration)
                throw std::runtime_error("Native mutable geometry clone has a stale map generation");
            // Source capacities admit the pinned library's size-based vector
            // copies; charge overlap BEFORE copying, then verify actual bytes.
            ValidateStateBudget(QuestVr::DecalStateDetail::Add(NativeMutableModelBytes(source),65536u));
            const auto roots=QuestVr::DecalStateDetail::Multiply(source.dynamicSurfaceReferences.size(),2u);
            ValidateStateBudget(NativeMutableModelBytes(source)+65536u+roots*(sizeof(GCRoot<RuntimeObject>)+sizeof(GCRootNode)));
            oldReferences.reserve(roots);
            for(const auto& refs:source.dynamicSurfaceReferences) {
                oldReferences.emplace_back(refs.actor);oldReferences.emplace_back(refs.texture);
            }
            prepared=std::make_unique<RuntimeNativeMutableModel>(source);
        }else {
            const auto& table=Table(model->sourcePath);const auto& entry=table.exports.at(model->exportIndex);
            if(entry.ObjSize<=0)throw std::runtime_error("Native mutable Model source payload is absent");
            const auto scratch=QuestVr::DecalStateDetail::Add(1u<<20u,
                QuestVr::DecalStateDetail::Multiply(static_cast<std::size_t>(entry.ObjSize),2u));
            const auto available=RemainingNativeStateBytes();
            if(scratch>=available||sizeof(RuntimeNativeMutableModel)>=available-scratch)
                throw std::runtime_error("Native mutable Model decode exceeds shared retained-byte budget");
            // Decoder output and property/raw/native-body scratch may overlap.
            // They may not each borrow the full caller allowance.
            const auto decoded=available-scratch-sizeof(RuntimeNativeMutableModel);
            ValidateStateBudget(available);
            nativeWork_.Step(static_cast<std::size_t>(entry.ObjSize)/64u+1u);
            prepared=std::make_unique<RuntimeNativeMutableModel>();
            prepared->geometry=LoadPortableCollisionModel68(table,model->exportIndex,scratch,decoded);
            prepared->sourceSurfaceCount=prepared->geometry.surfaces.size();
            prepared->generation=level->nativeMapGeneration;
        }
        if(prepared->geometry.exportIndex!=model->exportIndex||
            LowerAscii(prepared->geometry.objectPath)!=LowerAscii(GetPortableObjectPath(Table(model->sourcePath),
                static_cast<std::int32_t>(model->exportIndex+1u))))
            throw std::runtime_error("Native mutable Model payload/export identity differs");
        ValidateStateBudget(QuestVr::DecalStateDetail::Add(NativeMutableModelBytes(*prepared),sizeof(BeforeMutableModel)+sizeof(GCRootNode))+
            oldReferences.capacity()*sizeof(GCRoot<RuntimeObject>)+oldReferences.size()*sizeof(GCRootNode));
        // No fallible operation follows ownership publication. The journal roots
        // the exact Model and retains its complete prior owning payload. Rollback
        // swaps owners before any object retirement; no source cache is mutated.
        beforeMutableModel_.emplace(model);
        beforeMutableModel_->references=std::move(oldReferences);
        beforeMutableModel_->state=std::move(model->nativeMutableModel);
        model->nativeMutableModel=std::move(prepared);
        return *model->nativeMutableModel;
    }
    std::uint64_t ModelFlagFingerprint(const PortableModelGeometry& geometry) {
        nativeWork_.Step(geometry.nodes.size());
        std::uint64_t value=14695981039346656037ull;
        for(const auto& node:geometry.nodes){value^=node.nodeFlags;value*=1099511628211ull;}
        return value;
    }
    Value ProbeNativeBrushSphereFilter(const PortableModelSphere& sphere,std::size_t failAfterWrites,
        PortableBrushGeometryProbe& result) {
        brushGeometryPreview_=true;
        auto* level=CurrentClockLevel();auto* model=level->nativeLevelModel;
        // Copied output and callbacks have bounded lifetime in this same host.
        ValidateStateBudget(4096u);nativeQueryScratchBytes_+=4096u;
        auto& live=MutableRootModel(level);auto& geometry=live.geometry;
        result.modelPath=model->reflection.objectPath;
        result.nodes=geometry.nodes.size();result.surfaces=geometry.surfaces.size();
        result.retainedBytes=NativeMutableModelBytes(live);result.generation=live.generation;
        result.beforeFlags=ModelFlagFingerprint(geometry);result.mutableOwnerObserved=true;
        const auto finish=[&]{result.afterFlags=ModelFlagFingerprint(geometry);result.revision=live.revision;};
        QuestVr::OriginalBrushSphereFilterQueries queries;
        queries.work=[this]{LifecycleStep();};
        queries.readNode=[&](std::int32_t index) {
            if(model->nativeMutableModel.get()!=&live||live.generation!=level->nativeMapGeneration||
                index<0||static_cast<std::size_t>(index)>=geometry.nodes.size())
                throw std::runtime_error("Sphere prefilter lost actual mutable Model/node ownership");
            // This initial payload has ONLY original source nodes, no dynamic
            // free pool. A future tracker must authenticate active pool owners
            // here, not pass unowned zero entries to raw serialized validators.
            return geometry.nodes[static_cast<std::size_t>(index)];
        };
        queries.writeFlags=[&](std::int32_t index,std::uint8_t flags) {
            static_cast<void>(queries.readNode(index));
            geometry.nodes[static_cast<std::size_t>(index)].nodeFlags=flags;++result.writes;
            if(failAfterWrites&&result.writes==failAfterWrites)
                throw std::runtime_error("Injected native Model journal refusal after flag write");
        };
        try {
            if(live.revision==std::numeric_limits<std::uint64_t>::max())
                throw std::runtime_error("Native Model revision exhausted");
            QuestVr::PrecomputeOriginalBrushSphereFilter(geometry.nodes.size(),sphere,queries);
            ++live.revision;
            result.queryUsesMutable=&NativeLevelTraceModel(level)==&geometry;
            try{static_cast<void>(NativeWorldCollisionSource(level));}
            catch(const std::exception& error){result.immutableProofRefused=std::string(error.what())==
                "Mutable Model cannot borrow an immutable authored-source collision proof";}
            if(!result.queryUsesMutable||!result.immutableProofRefused)
                throw std::runtime_error("Native mutable Model query/proof separation failed");
            ValidateStateBudget();finish();
        }catch(...){result.revision=live.revision;throw;}
        return Value::Bool(true);
    }
