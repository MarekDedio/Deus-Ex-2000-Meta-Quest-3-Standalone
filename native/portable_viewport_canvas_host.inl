    void StageNativeCanvasInventory() {
        if(beforeCanvasInventory_)return;
        // Reserve the bounded viewport maximum in a temporary allocation. Only
        // after admission/copy succeeds do noexcept swaps retain the exact old
        // inventory allocation for rollback. No append can reallocate in-root.
        constexpr auto capacity=QuestVr::NativeClientRegistry::MaximumViewports;
        ValidateStateBudget(QuestVr::DecalStateDetail::Multiply(capacity,sizeof(RuntimeObject*)));
        std::vector<RuntimeObject*> staged;staged.reserve(capacity);
        staged.insert(staged.end(),persistentNativeCanvases.begin(),persistentNativeCanvases.end());
        if(staged.capacity()>capacity)ValidateStateBudget(staged.capacity()*sizeof(RuntimeObject*));
        beforeCanvasInventory_.emplace();beforeCanvasInventory_->swap(persistentNativeCanvases);
        persistentNativeCanvases.swap(staged);
    }
    void RequireNativeViewportCanvas(RuntimeObject* canvas) {
        if(!canvas||!canvas->nativeCanvasObject||!canvas->nativeCanvasPeer||!canvas->cls||
            canvas->cls->reflection.objectPath!="Engine.Canvas"||!canvas->cls->classDescriptor||
            canvas->cls->classDescriptor->classFlags!=0x112u||canvas->defaultOwner||canvas->runtimeBirth||
            canvas->nativeClientIdentity!=persistentNativeClient.ClientIdentity()||!persistentObjectLookup||
            canvas->lookupControl!=persistentObjectLookup||!canvas->objectIndex||!canvas->lookupName||
            canvas->reflection.flags||!persistentEngineCollision||canvas->outer!=persistentEngineCollision->get()->transient)
            throw std::runtime_error("Native Canvas has no actual current constructor/viewport authority");
        auto* viewport=canvas->nativeCanvasPeer;
        if(NativeSessionViewport(viewport->nativeViewportIdentity)!=viewport||viewport->nativeCanvasPeer!=canvas)
            throw std::runtime_error("Native Canvas lost its reciprocal viewport ownership");
    }
    PortableViewportCanvasSnapshot CaptureNativeViewportCanvas(RuntimeObject* canvas) {
        RequireNativeViewportCanvas(canvas);
        QuestVr::NativeScratchScope scratch(nativeQueryScratchBytes_,64u*1024u,
            [this](std::size_t extra){ValidateStateBudget(extra);});
        PortableViewportCanvasSnapshot result;
        result.prepared=result.initialized=true;result.objectPath=canvas->reflection.objectPath;
        result.classPath=canvas->cls->reflection.objectPath;result.outerPath=canvas->outer->reflection.objectPath;
        result.viewportPath=canvas->nativeCanvasPeer->reflection.objectPath;result.objectFlags=canvas->reflection.flags;
        result.objectIndex=canvas->objectIndex.Index();result.classIndex=canvas->cls->objectIndex.Index();
        result.outerIndex=canvas->outer->objectIndex.Index();result.viewportIndex=canvas->nativeCanvasPeer->objectIndex.Index();
        result.uniqueCounter=canvas->cls->uniqueObjectCounter;
        result.inventoryCapacity=persistentNativeCanvases.capacity();result.inventoryBytes=NativeCanvasInventoryBytes();
        result.propertyRootBytes=NativeObjectPropertyRootBytes(canvas->nativeWindowPropertyRoots);
        for(const auto* name:{"Font","SpaceX","SpaceY","OrgX","OrgY","ClipX","ClipY","CurX","CurY","Z","Style",
            "CurYL","DrawColor","bCenter","bNoSmooth","SizeX","SizeY","SmallFont","MedFont","BigFont","LargeFont","Viewport"}) {
            LifecycleStep();const auto key="Engine.Canvas."+std::string(name);const auto property=DescribeProperty(key);
            if(property.arrayDimension!=1u||!OwnsProperty(canvas->cls,property))
                throw std::runtime_error("Canvas snapshot lost its original scalar field authority");
            result.fields.push_back({key,Read(canvas,property,0u)});
        }
        return result;
    }
    PortableViewportCanvasSnapshot ConstructNativeViewportCanvas(std::uint64_t identity,const std::string& selected,
        QuestVr::Vm::Execution& execution) {
        auto* viewport=NativeSessionViewport(identity);clientDependent_=true;
        if(viewport->nativeCanvasPeer) {
            if(LowerAscii(viewport->nativeCanvasPeer->cls->reflection.objectPath)!=LowerAscii(selected))
                throw std::runtime_error("Configured Canvas cannot replace the live viewport's class");
            return CaptureNativeViewportCanvas(viewport->nativeCanvasPeer);
        }
        if(viewport->nativeViewportInput)
            throw std::runtime_error("Original Canvas construction/Init must precede viewport Input construction");
        auto* cls=Object(selected);auto* base=Object("Engine.Canvas");
        if(cls!=base||!cls->classDescriptor||!cls->classFieldLinksValid||cls->classDescriptor->classFlags!=0x112u||
            cls->nativeMetadataClass||cls->nativeEngineClass||cls->base!=Object("Core.Object"))
            throw std::runtime_error("Configured Canvas native constructor override is not implemented for this actual class");
        if(!persistentEngineCollision||!persistentObjectLookup||!persistentObjectLookup->active)
            throw std::runtime_error("Native Canvas needs actual Transient/package/lookup services");
        if(persistentNativeCanvases.size()>=QuestVr::NativeClientRegistry::MaximumViewports)
            throw std::runtime_error("Canvas inventory exceeds the actual viewport bound");
        const auto within=cls->classDescriptor->classWithinReference;
        if(!within||ResolveCastReference(Table(cls->sourcePath),*within)!="Core.Object")
            throw std::runtime_error("Configured Canvas lacks its original Object Within authority");
        StageNativeCanvasInventory();
        auto* outer=persistentEngineCollision->get()->transient;
        Touch(cls);const auto className=CastClassIdentity(cls->reflection.objectPath).name;
        const auto name=QuestVr::ObjectNames::MakeUnique(className,cls->uniqueObjectCounter,[&](std::string_view candidate) {
            LifecycleStep();return persistentVmObjects.count(LowerAscii(outer->reflection.objectPath+'.'+std::string(candidate)))!=0u;
        });
        const auto path=outer->reflection.objectPath+'.'+name;
        ValidateStateBudget(sizeof(RuntimeObject)+sizeof(GCAllocation)+path.size()+64u*1024u,cls);
        if(conversationIndices_)conversationIndices_->PrepareNextEntryCapacity();
        pendingBirthGarbage=true;provisionalCanvases_.emplace_back(GC::Alloc<RuntimeObject>(PortableReflectionObject{
            path,className,outer->reflection.objectPath,{},0u,0},nullptr));
        auto* canvas=provisionalCanvases_.back().get();canvas->cls=cls;canvas->outer=outer;canvas->lookupName=NameString(name);
        canvas->nativeCanvasObject=true;canvas->nativeClientIdentity=persistentNativeClient.ClientIdentity();
        canvas->sourcePath=cls->sourcePath;canvas->birthVmKey=LowerAscii(path);
        canvas->exportIndex=std::numeric_limits<std::size_t>::max();canvas->references={cls,outer};
        canvas->frozenBirthDefaults=cls->classDefaultValues;
        if(!persistentQualifiedObjects.emplace(path,canvas).second||!persistentVmObjects.emplace(LowerAscii(path),canvas).second)
            throw std::runtime_error("Canvas constructor collided with an indexed object");
        PublishTransactionalLookupObject(canvas);persistentNativeCanvases.push_back(canvas);
        for(const auto& [key,value]:std::array<std::pair<const char*,Value>,4u>{{
            {"Core.Object.Class",Value::Text(Kind::Object,cls->reflection.objectPath)},
            {"Core.Object.Name",Value::Text(Kind::Name,name)},
            {"Core.Object.Outer",Value::Text(Kind::Object,outer->reflection.objectPath)},
            {"Core.Object.ObjectFlags",Value::Integer(0)}}})
            execution.WriteReference(ReferenceFor(canvas,DescribeProperty(key),0u,false),value);
        // Static allocation copies defaults; native UCanvas10348120 only sets
        // its vtable. Init10377540 stores Viewport+78, nothing else. In
        // particular do not call Canvas.Reset/Update or set frame sizes here.
        const auto pointer=[&](RuntimeObject* object,RuntimeObject* target) {
            auto reference=std::make_shared<Reference>();reference->zero=Value::Text(Kind::Object,{});
            reference->read=[object]{return Value::Text(Kind::Object,object->nativeCanvasPeer?object->nativeCanvasPeer->reflection.objectPath:std::string{});};
            reference->write=[this,object](const Value& value) {
                if(value.kind!=Kind::Object)throw std::runtime_error("Native Canvas peer store is not Object");
                Touch(object);object->nativeCanvasPeer=value.text.empty()?nullptr:Object(value.text);
            };
            execution.WriteReference(reference,Value::Text(Kind::Object,target->reflection.objectPath));
        };
        pointer(viewport,canvas);pointer(canvas,viewport);
        return CaptureNativeViewportCanvas(canvas);
    }
