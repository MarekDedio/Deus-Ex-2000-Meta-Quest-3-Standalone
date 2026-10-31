    RuntimeObject* BrushPolygonObject(const PortablePackageTables& table,std::int32_t reference,const char* base) {
        if(!reference)return nullptr;
        const auto path=ResolvePortableValueObjectReference(table,reference,[&](const std::string& value,const std::string& type) {
            auto* target=Object(value);const auto identity=CastObjectIdentity(target->reflection.objectPath);
            return !identity.classObject&&ClassDerives(identity.classPath,base)&&
                LowerAscii(identity.classPath.substr(identity.classPath.find_last_of('.')+1u))==LowerAscii(type);
        });
        auto* target=Object(path);const auto identity=CastObjectIdentity(target->reflection.objectPath);
        if(target->runtimeBirth||target->defaultOwner||identity.classObject||!ClassDerives(identity.classPath,base)||
            target->exportIndex>=Table(target->sourcePath).exports.size()||!target->nativeLinker||!target->nativeLinker->active)
            throw std::runtime_error("Native polygon reference lacks its actual indexed source object/linker");
        return target;
    }
    RuntimeNativeMutablePolys& MutableBrushPolys(RuntimeObject* model,RuntimeObject* level) {
        if(!transaction_||level!=CurrentClockLevel()||model->sourcePath!=level->sourcePath||model->runtimeBirth)
            throw std::runtime_error("Native Polys mutation lacks current source Model/Level ownership");
        const auto selected=NativePrimitiveAsset(model);
        if(selected.kind!=NativePrimitiveKind::Model)throw std::runtime_error("Native Brush.Polys owner is not Model");
        const auto& geometry=NativeSelectedModel(persistentWorldActors.front(),selected);
        const auto& table=Table(model->sourcePath);const auto ref=geometry.polysReference;
        if(ref<=0||static_cast<std::size_t>(ref)>table.exports.size())
            throw std::runtime_error("Native Brush Model.Polys requires its actual source-local export");
        auto* polys=Object(Qualified(table,ref));const auto identity=CastObjectIdentity(polys->reflection.objectPath);
        if(polys->sourcePath!=model->sourcePath||polys->exportIndex!=static_cast<std::size_t>(ref-1)||
            polys->runtimeBirth||polys->defaultOwner||identity.classObject||LowerAscii(identity.classPath)!="engine.polys"||
            !polys->nativeLinker||polys->nativeLinker!=model->nativeLinker)
            throw std::runtime_error("Native Polys source/export/class/linker identity is unavailable");
        if(model->nativeBrushPolys&&model->nativeBrushPolys!=polys)
            throw std::runtime_error("Native Brush Model.Polys binding changed from its authored object");
        if(std::none_of(beforeBrushModels_.begin(),beforeBrushModels_.end(),[&](const auto& row){return row.model.get()==model;})) {
            if(beforeBrushModels_.size()>=4096u)throw std::runtime_error("Native Brush model journal count exceeded");
            ValidateStateBudget((beforeBrushModels_.size()+1u)*sizeof(BeforeBrushModel)+2u*sizeof(GCRootNode));
            beforeBrushModels_.reserve(beforeBrushModels_.size()+1u);
            beforeBrushModels_.emplace_back(model);
        }
        if(std::any_of(beforeMutablePolys_.begin(),beforeMutablePolys_.end(),[&](const auto& row){return row.polys.get()==polys;})) {
            if(!polys->nativeMutablePolys||polys->nativeMutablePolys->generation!=level->nativeMapGeneration)
                throw std::runtime_error("Native Polys journal lost its current mutable payload");
            model->nativeBrushPolys=polys;return *polys->nativeMutablePolys;
        }
        std::unique_ptr<RuntimeNativeMutablePolys> prepared;
        std::vector<GCRoot<RuntimeObject>> oldRoots;
        // Detached payloads and old-edge roots are not yet reachable through
        // the owning object/journal. Keep them charged across every nested
        // object/class resolver, not just at isolated preflight checks.
        const auto previousScratch=nativeQueryScratchBytes_;
        struct RestoreScratch {std::size_t& value;std::size_t previous;
            ~RestoreScratch(){value=previous;}} restoreScratch{nativeQueryScratchBytes_,previousScratch};
        const auto hold=[&](std::size_t bytes) {
            nativeQueryScratchBytes_=previousScratch;
            ValidateStateBudget(bytes);
            nativeQueryScratchBytes_=QuestVr::DecalStateDetail::Add(previousScratch,bytes);
        };
        const auto workspace=QuestVr::DecalStateDetail::Add(4u*65536u+16384u,
            QuestVr::DecalStateDetail::Multiply(table.sourcePath.size()+1u,128u));
        const auto rootsBytes=[&] {
            return QuestVr::DecalStateDetail::Add(
                QuestVr::DecalStateDetail::Multiply(oldRoots.capacity(),sizeof(GCRoot<RuntimeObject>)),
                QuestVr::DecalStateDetail::Multiply(oldRoots.size(),sizeof(GCRootNode)));
        };
        if(polys->nativeMutablePolys) {
            const auto& old=*polys->nativeMutablePolys;
            if(old.generation!=level->nativeMapGeneration)throw std::runtime_error("Native Polys payload has stale generation");
            const auto rootCount=QuestVr::DecalStateDetail::Multiply(old.references.size(),2u);
            hold(QuestVr::DecalStateDetail::Add(NativeMutablePolysBytes(old),workspace)+
                QuestVr::DecalStateDetail::Multiply(rootCount,sizeof(GCRoot<RuntimeObject>)+sizeof(GCRootNode)));
            oldRoots.reserve(rootCount);
            for(const auto& refs:old.references){oldRoots.emplace_back(refs.actor);oldRoots.emplace_back(refs.texture);}
            prepared=std::make_unique<RuntimeNativeMutablePolys>(old);
        }else {
            const auto available=RemainingNativeStateBytes();
            const auto overhead=sizeof(RuntimeNativeMutablePolys)+sizeof(BeforeMutablePolys)+workspace;
            if(available<=overhead)throw std::runtime_error("Native Polys has no shared decode allowance");
            const auto bytes=std::min<std::size_t>(32u<<20u,available-overhead);
            hold(bytes+overhead);
            prepared=std::make_unique<RuntimeNativeMutablePolys>();
            prepared->data=LoadPortableBrushPolys68(table,polys->exportIndex,bytes);
            prepared->generation=level->nativeMapGeneration;
            const auto count=prepared->data.polygons.size();nativeWork_.Step(count);
            const auto edges=QuestVr::DecalStateDetail::Multiply(count,sizeof(RuntimeNativePolygonReferences)+sizeof(NameString));
            hold(NativeMutablePolysBytes(*prepared)+edges+workspace);
            prepared->references.reserve(count);prepared->itemNames.reserve(count);
            hold(NativeMutablePolysBytes(*prepared)+workspace);
            for(const auto& poly:prepared->data.polygons) {
                LifecycleStep();
                prepared->references.push_back({BrushPolygonObject(table,poly.actorReference,"Engine.Actor"),
                    BrushPolygonObject(table,poly.textureReference,"Engine.Texture")});
                if(poly.itemNameReference<0||static_cast<std::size_t>(poly.itemNameReference)>=table.names.size())
                    throw std::runtime_error("Native Polys ItemName lost its source FName identity");
                prepared->itemNames.push_back(table.names[static_cast<std::size_t>(poly.itemNameReference)].Name);
            }
        }
        hold(NativeMutablePolysBytes(*prepared)+rootsBytes()+workspace);
        if(beforeMutablePolys_.size()>=4096u)throw std::runtime_error("Native Polys journal count exceeded");
        // The scratch ledger already includes prepared and oldRoots. Admit
        // only the overlapping journal allocation and its new root node here.
        const auto journal=(beforeMutablePolys_.size()+1u)*sizeof(BeforeMutablePolys)+sizeof(GCRootNode);
        ValidateStateBudget(journal);
        beforeMutablePolys_.reserve(beforeMutablePolys_.size()+1u);
        auto& before=beforeMutablePolys_.emplace_back(polys);
        before.references=std::move(oldRoots);before.state=std::move(polys->nativeMutablePolys);
        polys->nativeMutablePolys=std::move(prepared);model->nativeBrushPolys=polys;
        return *polys->nativeMutablePolys;
    }
    template<class T> void SetupAppendZero(std::vector<T>& values) {
        if(values.size()>=static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()))
            throw std::runtime_error("Native SetupActor array exceeds signed index space");
        // Source FArray growth while adding, not merely a capacity hint. This
        // portable owner retains exact active Num and zeros the new record.
        if(values.size()==values.capacity()) {
            const auto count=values.size()+1u;
            const auto capacity=count+(3u*count)/8u+32u;
            ValidateStateBudget(QuestVr::DecalStateDetail::Multiply(capacity,sizeof(T)));
            values.reserve(capacity);ValidateStateBudget();
        }
        values.emplace_back();
    }
    void OriginalBrushSetupActor(RuntimeObject* actor,RuntimeObject* model,PortableBrushSetupProbe& result,
        std::size_t failAfterPolygons) {
        if(!IsDerivedFromPath(actor->cls,"Engine.Mover"))
            throw std::runtime_error("Original dynamic Brush checked-cast to Mover failed");
        auto* level=CurrentClockLevel();
        if(actor->runtimeBirth||actor->sourcePath!=level->sourcePath||model->sourcePath!=level->sourcePath)
            throw std::runtime_error("Native SetupActor dynamic/source reference rebinding requires its original provider");
        auto& source=MutableBrushPolys(model,level);auto& world=MutableRootModel(level);
        if(source.references.size()!=source.data.polygons.size()||source.itemNames.size()!=source.data.polygons.size()||
            world.sourceSurfaceCount+world.dynamicSurfaceReferences.size()!=world.geometry.surfaces.size())
            throw std::runtime_error("Native SetupActor source/typed edge correspondence differs");
        if(source.revision==std::numeric_limits<std::uint64_t>::max()||world.revision==std::numeric_limits<std::uint64_t>::max())
            throw std::runtime_error("Native SetupActor geometry revision exhausted");
        const auto oldPos=MovingBrushStructField(actor,"Engine.Mover.OldPos",Kind::Vector,1u);
        const auto oldRot=MovingBrushStructField(actor,"Engine.Mover.OldRot",Kind::Rotator,1u);
        const auto assimilated=MovingBrushScalarField(actor,"Engine.Actor.bAssimilated","Engine.Actor",Kind::Bool,0x2002u);
        const auto special=MovingBrushScalarField(actor,"Engine.Actor.bSpecialLit","Engine.Actor",Kind::Bool,0x21u);
        model->nativeBrushRoot=-1;
        ReferenceFor(actor,oldPos,0u,false)->write(Value::Vector({-1,-1,-1}));
        ReferenceFor(actor,oldRot,0u,false)->write(Value::Rotator({123,456,789}));
        ReferenceFor(actor,assimilated,0u,false)->write(Value::Bool(false));
        const auto first=world.geometry.surfaces.size();
        for(std::size_t i=0;i<source.data.polygons.size();++i) {
            LifecycleStep();auto& poly=source.data.polygons[i];
            const auto surfaceIndex=world.geometry.surfaces.size();
            SetupAppendZero(world.geometry.surfaces);
            const auto normal=world.geometry.vectors.size();SetupAppendZero(world.geometry.vectors);
            const auto u=world.geometry.vectors.size();SetupAppendZero(world.geometry.vectors);
            const auto v=world.geometry.vectors.size();SetupAppendZero(world.geometry.vectors);
            const auto point=world.geometry.points.size();SetupAppendZero(world.geometry.points);
            SetupAppendZero(world.dynamicSurfaceReferences);
            auto& surf=world.geometry.surfaces.back();
            surf.normalVector=static_cast<std::int32_t>(normal);surf.textureU=static_cast<std::int32_t>(u);
            surf.textureV=static_cast<std::int32_t>(v);surf.basePoint=static_cast<std::int32_t>(point);
            surf.lightMap=poly.brushPoly;surf.brushPoly=static_cast<std::int32_t>(i);
            surf.materialReference=poly.textureReference;surf.panU=poly.panU;surf.panV=poly.panV;
            surf.brushActorReference=static_cast<std::int32_t>(actor->exportIndex+1u);
            const bool specialLit=Read(actor,special,0u).boolean;
            surf.polyFlags=(poly.polyFlags&0x3cffffffu)|(specialLit?0x100000u:0u);
            world.dynamicSurfaceReferences.back()={actor,source.references[i].texture};
            poly.link=static_cast<std::int32_t>(surfaceIndex);++result.completedPolygons;
            if(failAfterPolygons&&result.completedPolygons==failAfterPolygons)
                throw std::runtime_error("Injected SetupActor refusal after native surface/polygon link writes");
            const auto& texture=source.references[i].texture;
            const auto outputBytes=sizeof(PortableBrushSetupSurface)+actor->reflection.objectPath.capacity()+
                model->nativeBrushPolys->reflection.objectPath.capacity()+(texture?texture->reflection.objectPath.capacity():0u)+3u;
            ValidateStateBudget(outputBytes+(result.surfaces.size()+1u)*sizeof(PortableBrushSetupSurface));
            result.surfaces.reserve(result.surfaces.size()+1u);
            result.surfaces.push_back({actor->reflection.objectPath,model->nativeBrushPolys->reflection.objectPath,
                texture?texture->reflection.objectPath:std::string{},i,surfaceIndex,poly.link,surf,
                {world.geometry.points[point],world.geometry.vectors[normal],world.geometry.vectors[u],world.geometry.vectors[v]}});
            nativeActorOrderObservationBytes_+=outputBytes;
        }
        ++source.revision;++world.revision;
        ValidateStateBudget(sizeof(PortableBrushSetupMover)+actor->reflection.objectPath.capacity()+model->reflection.objectPath.capacity()+
            model->nativeBrushPolys->reflection.objectPath.capacity()+3u+(result.movers.size()+1u)*sizeof(PortableBrushSetupMover));
        result.movers.reserve(result.movers.size()+1u);
        const auto p=Read(actor,oldPos,0u);const auto r=Read(actor,oldRot,0u);
        result.movers.push_back({actor->reflection.objectPath,model->reflection.objectPath,model->nativeBrushPolys->reflection.objectPath,
            source.data.polygons.size(),first,p.vector,r.rotation,*model->nativeBrushRoot,
            Read(actor,assimilated,0u).boolean,Read(actor,special,0u).boolean});
        nativeActorOrderObservationBytes_+=sizeof(PortableBrushSetupMover)+actor->reflection.objectPath.capacity()+
            model->reflection.objectPath.capacity()+model->nativeBrushPolys->reflection.objectPath.capacity()+3u;
    }
    Value ProbeNativeBrushSetup(std::size_t failAfterPolygons,PortableBrushSetupProbe& result) {
        brushGeometryPreview_=true;auto* level=CurrentClockLevel();auto& world=MutableRootModel(level);
        result.originalSurfaces=world.geometry.surfaces.size();result.originalVectors=world.geometry.vectors.size();
        result.originalPoints=world.geometry.points.size();
        for(std::size_t slot=0u;slot<persistentWorldActors.size();++slot) {
            LifecycleStep();auto* actor=persistentWorldActors[slot];if(!actor)continue;
            const auto selected=NativePrimitiveField(actor,"Engine.Actor.Brush","Engine.Model",0x2au);
            if(!selected||!IsDerivedFromPath(actor->cls,"Engine.Brush")||NativeActorStatic(actor))continue;
            OriginalBrushSetupActor(actor,selected->object,result,failAfterPolygons);
        }
        // Exercise the real GC visit path while the journal and source/native
        // edges are live. No permanent diagnostic roots conceal missing edges.
        const auto objects=GC::GetStats().numObjects;GC::Collect();
        if(GC::GetStats().numObjects!=objects)throw std::runtime_error("SetupActor GC lost a source/native object");
        result.gcOwnershipChecked=true;result.modelPolysEdges=true;result.textureEdges=true;result.actorEdges=true;
        for(const auto& row:result.surfaces) {
            LifecycleStep();const auto& edges=world.dynamicSurfaceReferences.at(row.surface-world.sourceSurfaceCount);
            if(!edges.actor||Object(row.moverPath)!=edges.actor||(!row.texturePath.empty()&&Object(row.texturePath)!=edges.texture))
                throw std::runtime_error("SetupActor dynamic surface typed GC edge lost object identity");
        }
        ValidateStateBudget();return Value::Bool(true);
    }
