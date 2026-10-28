    void StageNativeConsoleInventory() {
        if(beforeConsoleInventory_)return;
        constexpr auto capacity=QuestVr::NativeClientRegistry::MaximumViewports;
        ValidateStateBudget(capacity*sizeof(RuntimeObject*));
        std::vector<RuntimeObject*> staged;staged.reserve(capacity);
        staged.insert(staged.end(),persistentNativeConsoles.begin(),persistentNativeConsoles.end());
        if(staged.capacity()>capacity)ValidateStateBudget(staged.capacity()*sizeof(RuntimeObject*));
        beforeConsoleInventory_.emplace();beforeConsoleInventory_->swap(persistentNativeConsoles);
        persistentNativeConsoles.swap(staged);
    }
    void RequireNativeViewportConsole(RuntimeObject* console) {
        if(!console||!console->nativeConsoleObject||!console->nativeConsoleOwner||!console->cls||
            console->cls->reflection.objectPath!="Engine.Console"||!console->cls->classDescriptor||
            console->cls->classDescriptor->classFlags!=0x13au||console->defaultOwner||console->runtimeBirth||
            console->nativeClientIdentity!=persistentNativeClient.ClientIdentity()||!persistentObjectLookup||
            console->lookupControl!=persistentObjectLookup||!console->objectIndex||!console->lookupName||
            !persistentEngineCollision||console->outer!=persistentEngineCollision->get()->transient||
            !persistentPlayerSession||!persistentPlayerSession->get())
            throw std::runtime_error("Native Console has no actual current constructor/viewport authority");
        const auto& viewports=persistentPlayerSession->get()->viewports;
        auto* owner=console->nativeConsoleOwner;
        // Authenticate provenance before dereferencing this non-owning pointer.
        if(std::find(viewports.begin(),viewports.end(),owner)==viewports.end()||
            NativeSessionViewport(owner->nativeViewportIdentity)!=owner||owner->nativeViewportConsole!=console)
            throw std::runtime_error("Native Console lost its actual viewport owner");
        // Console.Viewport is mutable script storage, NOT constructor provenance.
    }
    template<class Callback> void ForEachNativeConsoleField(RuntimeObject* cls,Callback callback) {
        if(!cls||!cls->classDescriptor||!cls->classFieldLinksValid)
            throw std::runtime_error("Console fields lack actual Children/Next authority");
        auto child=cls->classDescriptor->state.children;std::size_t traversed{},fields{},slots{};
        while(child) {
            LifecycleStep();if(++traversed>1024u)throw std::runtime_error("Console field chain exceeds its bound");
            auto* node=Object(Qualified(Table(cls->sourcePath),child));
            if(node->outer!=cls)throw std::runtime_error("Console field chain escaped its declaring class");
            if(node->property) {
                const auto property=Describe(node);
                if(!OwnsProperty(cls,property)||!property.arrayDimension||property.arrayDimension>64u)
                    throw std::runtime_error("Console field has no actual bounded indexed schema");
                ++fields;slots+=property.arrayDimension;
                callback(property);child=node->property->nextField;
            }else if(node->script)child=node->script->nextField;
            else if(node->stateDescriptor)child=node->stateDescriptor->nextField;
            else if(node->commonFieldLinks)child=node->commonFieldLinks->nextField;
            else throw std::runtime_error("Console field chain lacks retained original links");
        }
        if(fields!=57u||slots!=324u)throw std::runtime_error("Console original property/slot inventory differs");
    }
    PortableViewportConsoleSnapshot CaptureNativeViewportConsole(RuntimeObject* console) {
        RequireNativeViewportConsole(console);
        QuestVr::NativeScratchScope scratch(nativeQueryScratchBytes_,256u*1024u,
            [this](std::size_t extra){ValidateStateBudget(extra);});
        PortableViewportConsoleSnapshot result;
        result.prepared=result.initialized=result.outputDeviceBound=true;
        result.objectPath=console->reflection.objectPath;result.classPath=console->cls->reflection.objectPath;
        result.outerPath=console->outer->reflection.objectPath;result.ownerViewportPath=console->nativeConsoleOwner->reflection.objectPath;
        result.viewportPath=Read(console,DescribeProperty("Engine.Console.Viewport"),0u).text;
        if(std::bit_cast<std::uint32_t>(QuestVr::Vm::ToInt(Read(console,DescribeProperty("Core.Object.ObjectFlags"),0u)))!=console->reflection.flags)
            throw std::runtime_error("Native Console header and reflected ObjectFlags disagree");
        result.objectFlags=console->reflection.flags;result.objectIndex=console->objectIndex.Index();
        result.classIndex=console->cls->objectIndex.Index();result.outerIndex=console->outer->objectIndex.Index();
        result.ownerViewportIndex=console->nativeConsoleOwner->objectIndex.Index();
        if(!result.viewportPath.empty())result.viewportIndex=Object(result.viewportPath)->objectIndex.Index();
        result.uniqueCounter=console->cls->uniqueObjectCounter;
        result.inventoryCapacity=persistentNativeConsoles.capacity();result.inventoryBytes=NativeConsoleInventoryBytes();
        result.propertyRootBytes=NativeObjectPropertyRootBytes(console->nativeWindowPropertyRoots);
        for(const auto& [key,slots]:console->frozenBirthDefaults) {
            static_cast<void>(key);result.frozenDefaultOverrides+=slots.size();
        }
        result.stateLocalsRevision=console->stateLocalsRevision;
        if(const auto* state=console->stateObject.get()) {
            result.hasStack=state->hasStack;result.frameOverride=state->frameOverride;
            if(state->frame) {
                const auto& frame=*state->frame;result.nodePath=frame.codePath;
                if(!frame.codePath.empty())result.nodeIndex=Object(frame.codePath)->objectIndex.Index();
                if(frame.control) {result.stateNodePath=frame.control->selectedPath;result.codePresent=frame.control->codePresent;
                    result.probeMask=frame.control->liveProbeMask;
                    if(!result.stateNodePath.empty())result.stateNodeIndex=Object(result.stateNodePath)->objectIndex.Index();}
                if(!frame.locals.empty()||!frame.localsCodePath.empty())
                    throw std::runtime_error("Console initialized snapshot does not represent command-local execution");
            }
        }
        ForEachNativeConsoleField(console->cls,[&](const Property& property) {
            if(LowerAscii(property.key)=="engine.console.vtblout")return;
            for(std::uint32_t index=0u;index<property.arrayDimension;++index) {
                LifecycleStep();result.fields.push_back({property.key,index,Read(console,property,index)});
            }
        });
        return result;
    }
    PortableViewportConsoleSnapshot ConstructNativeViewportConsole(std::uint64_t identity,const std::string& selected,
        QuestVr::Vm::Execution& execution) {
        auto* viewport=NativeSessionViewport(identity);clientDependent_=true;
        if(viewport->nativeViewportConsole) {
            if(LowerAscii(viewport->nativeViewportConsole->cls->reflection.objectPath)!=LowerAscii(selected))
                throw std::runtime_error("Configured Console cannot replace the live viewport's class");
            return CaptureNativeViewportConsole(viewport->nativeViewportConsole);
        }
        if(!viewport->nativeCanvasPeer||!viewport->nativeViewportInput||!ActualNativeInputInstanceValid(viewport->nativeViewportInput))
            throw std::runtime_error("Original Console construction requires prior Canvas and Input constructors");
        RequireNativeViewportCanvas(viewport->nativeCanvasPeer);
        auto* cls=Object(selected);auto* base=Object("Engine.Console");auto* objectClass=Object("Core.Object");
        if(cls!=base||!cls->classDescriptor||!cls->classFieldLinksValid||cls->classDescriptor->classFlags!=0x13au||
            cls->reflection.flags!=0x040f0004u||cls->nativeMetadataClass||cls->nativeEngineClass||cls->base!=objectClass||
            cls->cls!=Object("Core.Class")||cls->nativeClassWithin!=objectClass||
            !cls->classDescriptor->configName||*cls->classDescriptor->configName!="System")
            throw std::runtime_error("Configured Console native constructor override lacks actual original class authority");
        if(!persistentEngineCollision||!persistentObjectLookup||!persistentObjectLookup->active)
            throw std::runtime_error("Native Console needs actual Transient/package/lookup services");
        const auto within=cls->classDescriptor->classWithinReference;
        if(!within||ResolveCastReference(Table(cls->sourcePath),*within)!="Core.Object")
            throw std::runtime_error("Console requires its original Object Within, not Viewport Outer");
        if(persistentNativeConsoles.size()>=QuestVr::NativeClientRegistry::MaximumViewports)
            throw std::runtime_error("Console inventory exceeds the actual viewport bound");
        // Admit complete bounded field-copy/GC/snapshot/index scratch before
        // constructing. The live allocations are also metered as they publish.
        QuestVr::NativeScratchScope scratch(nativeQueryScratchBytes_,2u*1024u*1024u,
            [this](std::size_t extra){ValidateStateBudget(extra);});
        StageNativeConsoleInventory();Touch(cls);
        const auto name=QuestVr::ObjectNames::MakeUnique("Console",cls->uniqueObjectCounter,[&](std::string_view candidate) {
            LifecycleStep();return persistentVmObjects.count(LowerAscii("Transient."+std::string(candidate)))!=0u;
        });
        auto* outer=persistentEngineCollision->get()->transient;const auto path=outer->reflection.objectPath+'.'+name;
        ValidateStateBudget(sizeof(RuntimeObject)+sizeof(GCAllocation)+path.size()+65536u,cls);
        if(conversationIndices_)conversationIndices_->PrepareNextEntryCapacity();
        pendingBirthGarbage=true;provisionalConsoles_.emplace_back(GC::Alloc<RuntimeObject>(PortableReflectionObject{
            path,"Console",outer->reflection.objectPath,{},0x4000u,0},nullptr));
        auto* console=provisionalConsoles_.back().get();console->cls=cls;console->outer=outer;console->lookupName=NameString(name);
        console->nativeConsoleObject=true;console->nativeConsoleOwner=viewport;console->nativeClientIdentity=persistentNativeClient.ClientIdentity();
        console->sourcePath=cls->sourcePath;console->birthVmKey=LowerAscii(path);
        console->exportIndex=std::numeric_limits<std::size_t>::max();console->references={cls,outer};
        // Copy the effective birth value of every slot. Immutable tagged/zero
        // defaults are already retained through cls: instance reads never
        // consult later mutable CDO patches. Store only bit-exact differences,
        // including explicit zero/None overrides of authored nonzero tags.
        // Native FString/FName NoInit constructors do NOT clear either tier.
        std::size_t copiedText{};
        ForEachNativeConsoleField(cls,[&](const Property& property) {
            if(LowerAscii(property.key)=="engine.console.vtblout")return;
            for(std::uint32_t index=0u;index<property.arrayDimension;++index) {
                LifecycleStep();auto value=Read(cls,property,index,true);
                if(value.text.size()>8192u||copiedText>512u*1024u-value.text.size())
                    throw std::runtime_error("Console copied default text exceeds its admitted constructor bound");
                copiedText+=value.text.size();
                if(value.kind==Kind::Object&&!value.text.empty())
                    console->nativeWindowPropertyRoots[LowerAscii(property.key)][index].push_back(Object(value.text));
                const auto baseline=Read(console,property,index,false,true);
                if(!SameStoredValue(value,baseline))
                    console->frozenBirthDefaults[InstanceStorageKey(cls,property)][index]=std::move(value);
            }
        });
        if(!persistentQualifiedObjects.emplace(path,console).second||!persistentVmObjects.emplace(LowerAscii(path),console).second)
            throw std::runtime_error("Console constructor collided with an indexed object");
        PublishTransactionalLookupObject(console);persistentNativeConsoles.push_back(console);
        for(const auto& [key,value]:std::array<std::pair<const char*,Value>,4u>{{
            {"Core.Object.Class",Value::Text(Kind::Object,cls->reflection.objectPath)},
            {"Core.Object.Name",Value::Text(Kind::Name,name)},
            {"Core.Object.Outer",Value::Text(Kind::Object,outer->reflection.objectPath)},
            {"Core.Object.ObjectFlags",Value::Integer(0x4000)}}})
            execution.WriteReference(ReferenceFor(console,DescribeProperty(key),0u,false),value);
        auto owning=std::make_shared<Reference>();owning->zero=Value::Text(Kind::Object,{});
        owning->read=[viewport]{return Value::Text(Kind::Object,viewport->nativeViewportConsole?
            viewport->nativeViewportConsole->reflection.objectPath:std::string{});};
        owning->write=[this,viewport](const Value& value) {
            if(value.kind!=Kind::Object)throw std::runtime_error("Viewport.Console store is not Object");
            Touch(viewport);viewport->nativeViewportConsole=value.text.empty()?nullptr:Object(value.text);
        };
        execution.WriteReference(owning,Value::Text(Kind::Object,path));
        execution.WriteReference(ReferenceFor(console,DescribeProperty("Engine.Console.Viewport"),0u,false),
            Value::Text(Kind::Object,viewport->reflection.objectPath));
        execution.WriteReference(ReferenceFor(console,DescribeProperty("Engine.Console.TopLine"),0u,false),Value::Integer(63));
        execution.WriteReference(ReferenceFor(console,DescribeProperty("Engine.Console.BorderSize"),0u,false),Value::Integer(1));
        // Actual Core.InitExecution: Class Node/StateNode, null Code/Locals/
        // Previous, all-one probes. No auto-state, BeginState or Input.Init.
        auto frame=std::make_shared<Reference>();frame->zero=Value::Bool(false);
        frame->read=[console]{return Value::Bool(console->stateObject!=nullptr);};
        frame->write=[this,console](const Value& value) {
            if(value.kind!=Kind::Bool)throw std::runtime_error("Console InitExecution frame store is not Bool authority");
            if(!value.boolean){Touch(console);console->stateObject.reset();return;}
            if(console->stateObject||console->stateLocalsRevision==std::numeric_limits<std::uint64_t>::max())
                throw std::runtime_error("Console frame replacement requires its separate initialized-state service");
            ValidateStateBudget(sizeof(QuestVr::StateObject)+2u*console->cls->reflection.objectPath.size()+4096u);
            auto state=std::make_unique<QuestVr::StateObject>();QuestVr::StateFrame actual;
            actual.codePath=console->cls->reflection.objectPath;
            actual.control=QuestVr::StateControl{actual.codePath,false,~std::uint64_t{0}};
            state->hasStack=state->frameOverride=true;state->frame=std::move(actual);
            Touch(console);console->stateObject=std::move(state);++console->stateLocalsRevision;
            console->stateTransitionFlags=0u;console->committedScriptState=true;
            // Latent Continue is portable policy while Code=null, not proof of
            // a native initialization store to the original unwritten slot.
        };
        execution.WriteReference(frame,Value::Bool(true));
        const auto flags=DescribeProperty("Core.Object.ObjectFlags");
        execution.WriteReference(ReferenceFor(console,flags,0u,false),
            Value::Integer(QuestVr::Vm::ToInt(Read(console,flags,0u))|0x02000000));
        ValidateStateBudget();return CaptureNativeViewportConsole(console);
    }
