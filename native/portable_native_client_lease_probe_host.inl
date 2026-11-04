// Included in PortableActorVmHost's public section. Diagnostic only: all
// mutation uses the existing production ReferenceFor/CoreStaticExec services.
    PortableNativeClientGraphLeaseProbeResult ProbeNativeClientGraphLease(std::uint64_t identity,
        const std::function<void()>& afterMutation,const QuestVr::Vm::Limits& limits) {
        if(transaction_||persistentVmRootCount)
            throw std::runtime_error("Native Client graph lease diagnostic requires an exclusive CPU boundary");
        auto* viewport=NativeSessionViewport(identity);
        auto* canvas=viewport->nativeCanvasPeer;auto* console=viewport->nativeViewportConsole;
        auto* input=viewport->nativeViewportInput;auto* player=viewport->nativeViewportActor;
        RequireNativeViewportCanvas(canvas);RequireNativeViewportConsole(console);
        if(!input||!ActualNativeInputObjectValid(input)||!input->nativeInputInstance||
            input->nativeInputInstance->ownerViewport!=viewport)
            throw std::runtime_error("Native Client graph lease diagnostic lacks its actual Input constructor");
        if(player&&(!player->nativePlayerBinding||player->nativePlayer!=viewport||
            !IsDerivedFromPath(player->cls,"Engine.PlayerPawn")))
            throw std::runtime_error("Native Client graph lease diagnostic found an unpaired actual player");
        // These extra observation roots must survive native-root retirement at
        // the end of Rollback, including an explicit GC inside the callback.
        // Reserve before their GCRootNode allocations. Conservative duplicate
        // object admission also covers their short post-rollback lifetime.
        using QuestVr::DecalStateDetail::Add;
        auto bytes=sizeof(PortableNativeClientGraphLeaseProbeResult)+5u*(sizeof(GCRoot<RuntimeObject>)+sizeof(GCRootNode))+8192u;
        for(auto* object:{viewport->outer,viewport,canvas,console,input})bytes=Add(bytes,NativeSessionObjectBytes(object));
        QuestVr::NativeScratchScope observationScratch(nativeQueryScratchBytes_,bytes,
            [this](std::size_t extra){ValidateStateBudget(extra);});
        GCRoot<RuntimeObject> heldViewport(viewport),heldCanvas(canvas),heldConsole(console),heldInput(input),heldPlayer(player);
        const auto observe=[&]() noexcept {
            PortableNativeClientGraphLeaseObservation value;
            const auto index=[](RuntimeObject* object)noexcept{return object?object->objectIndex.Index():0u;};
            auto* v=heldViewport.get();auto* c=heldCanvas.get();auto* o=heldConsole.get();auto* i=heldInput.get();auto* p=heldPlayer.get();
            value.viewportIndex=index(v);value.canvasIndex=index(c);value.consoleIndex=index(o);value.inputIndex=index(i);value.playerIndex=index(p);
            value.playerPresent=p!=nullptr;value.playerBinding=p&&p->nativePlayerBinding;
            value.playerPointer=p&&p->nativePlayer;value.playerToViewport=p&&p->nativePlayer==v;
            value.viewportActor=v->nativeViewportActor!=nullptr;value.viewportCanvas=v->nativeCanvasPeer==c;
            value.canvasViewport=c->nativeCanvasPeer==v;value.viewportConsole=v->nativeViewportConsole==o;
            value.consoleOwner=o->nativeConsoleOwner==v;value.viewportInput=v->nativeViewportInput==i;
            if(const auto* payload=i->nativeInputInstance.get()) {
                value.inputRetired=payload->retired;value.inputOwner=payload->ownerViewport!=nullptr;
                value.inputOwnerIsViewport=payload->ownerViewport==v;value.inputViewport=payload->values.viewport!=nullptr;
                value.inputInitialized=payload->initialized;value.inputReceipt=payload->initialization.has_value();
                value.inputPhysicalDefault=payload->physical==QuestVr::QuestInputPhysicalState{};
                value.cursorSource=payload->sourceCursor.source;value.cursorSequence=payload->sourceCursor.sequence;
                value.cursorRejections=payload->sourceCursor.rejections;
                value.inputMutation=payload->values.bindings[84u]=="QuestClientLeaseProbeMutation";
            }
            return value;
        };
        PortableNativeClientGraphLeaseProbeResult result;result.before=observe();
        result.execution=QuestVr::Vm::ExecuteTransaction(*this,[&](QuestVr::Vm::Execution& execution) {
            clientDependent_=true;
            const auto write=[&](RuntimeObject* object,const char* name,const Value& value) {
                const auto field=DescribeProperty(name);
                execution.WriteReference(ReferenceFor(object,field,0u,false),value);++result.peerWrites;
            };
            // Same actual header value forces the viewport before-image without
            // forging its identity. Canvas/Console scalars deliberately differ:
            // plain same-value Console stores correctly short-circuit Touch.
            write(viewport,"Core.Object.ObjectFlags",Read(viewport,DescribeProperty("Core.Object.ObjectFlags"),0u));
            const auto space=Read(canvas,DescribeProperty("Engine.Canvas.SpaceX"),0u);
            if(space.kind!=Kind::Float)throw std::runtime_error("Graph probe Canvas SpaceX is not Float");
            write(canvas,"Engine.Canvas.SpaceX",Value::Float(space.floating==0.375f?1.375f:0.375f));
            const auto line=Read(console,DescribeProperty("Engine.Console.TopLine"),0u);
            if(line.kind!=Kind::Int)throw std::runtime_error("Graph probe Console TopLine is not Int");
            write(console,"Engine.Console.TopLine",Value::Integer(line.integer==31?32:31));
            if(player)write(player,"Engine.Actor.Tag",Read(player,DescribeProperty("Engine.Actor.Tag"),0u));
            const auto command="SET "+input->cls->reflection.objectPath+" T QuestClientLeaseProbeMutation";
            if(!CoreStaticExec(command,result.mutation,execution)||!result.mutation.handled||!result.mutation.instanceImports)
                throw std::runtime_error("Native graph lease probe did not reach actual Core SET/Input COW fanout");
            result.during=observe();result.completed=true;
            if(afterMutation){afterMutation();result.callbackCompleted=true;}
            // SET requests logical Flush. Never permit this diagnostic to reach
            // PublishNativeUserConfig, including in the no-callback control.
            throw std::runtime_error("Non-publishing native Client graph lease probe completed; rollback required");
            return Value{};
        },limits);
        result.after=observe();result.afterCaptured=true;
        return result;
    }
