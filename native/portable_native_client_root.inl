// Private native Client transaction ownership. No platform providers are fabricated.
// The Begin snapshot is a root/lease inventory, NEVER the source of iteration.
    struct RootViewportGraph {
        GCRoot<RuntimeObject> viewport,actor,canvas,console,input,renderDevice;
        std::uint32_t viewportIndex{},actorIndex{},canvasIndex{},consoleIndex{},inputIndex{},renderDeviceIndex{};
        std::uint64_t identity{};
        explicit RootViewportGraph(RuntimeObject* object,RuntimeObject* device)
            :viewport(object),actor(object->nativeViewportActor),canvas(object->nativeCanvasPeer),
             console(object->nativeViewportConsole),input(object->nativeViewportInput),renderDevice(device) {
            const auto index=[](RuntimeObject* value){return value?value->objectIndex.Index():0u;};
            viewportIndex=index(object);actorIndex=index(actor.get());canvasIndex=index(canvas.get());
            consoleIndex=index(console.get());inputIndex=index(input.get());renderDeviceIndex=index(device);
            identity=object->nativeViewportIdentity;
        }
    };
    struct RootClientGraph {
        GCRoot<RuntimePlayerSession> session;
        GCRoot<RuntimeEngineCollisionDomain> engineDomain;
        GCRoot<RuntimeObject> client,engine,engineClient,level;
        std::shared_ptr<RuntimeObjectLookupControl> lookup;
        std::shared_ptr<QuestVr::NativeClientLifetime> lifetime;
        RuntimePackage* runtime{};
        RuntimeObject* clientEngineAtEntry{};
        std::uint64_t clientIdentity{},lookupGeneration{},mapGeneration{};
        std::uint32_t clientIndex{},engineIndex{},levelIndex{};
        std::vector<RootViewportGraph> viewports;
        bool engineObserved{};
    };
    std::unique_ptr<RootClientGraph> nativeClientRoot_;

    static std::size_t RootClientGraphStorageBytes(const RootClientGraph& graph) {
        using QuestVr::DecalStateDetail::Add;
        using QuestVr::DecalStateDetail::Multiply;
        auto bytes=Add(sizeof(RootClientGraph),
            Multiply(graph.viewports.capacity(),sizeof(RootViewportGraph)));
        bytes=Add(bytes,Multiply(6u+6u*graph.viewports.size(),sizeof(GCRootNode)));
        // A copied shared_ptr allocates no new control block. Still meter the
        // endpoint retained by this root if Clear removes the registry owner.
        if(graph.lifetime)bytes=Add(bytes,sizeof(QuestVr::NativeClientLifetime)+4u*sizeof(void*));
        return bytes;
    }
    std::size_t NativeClientRootRetainedBytes() const {
        if(!nativeClientRoot_)return 0u;
        using QuestVr::DecalStateDetail::Add;
        const auto& graph=*nativeClientRoot_;auto bytes=RootClientGraphStorageBytes(graph);
        const auto* current=persistentPlayerSession?persistentPlayerSession->get():nullptr;
        // Clear retires Client/Viewport out of ordinary session accounting.
        // Their captured roots must not make these old allocations unmetered.
        if(graph.client&&(!current||current->client!=graph.client.get()))
            bytes=Add(bytes,NativeSessionObjectBytes(graph.client.get()));
        for(const auto& peer:graph.viewports) {
            if(!current||std::find(current->viewports.begin(),current->viewports.end(),peer.viewport.get())==current->viewports.end())
                bytes=Add(bytes,NativeSessionObjectBytes(peer.viewport.get()));
        }
        // Native Canvas/Console/Input weak inventories already meter escaped
        // peers until GC retirement. Do not double-count their object payloads.
        if(graph.engineDomain&&(!persistentEngineCollision||persistentEngineCollision->get()!=graph.engineDomain.get()))
            bytes=Add(bytes,EngineCollisionRetainedBytes(*graph.engineDomain.get()));
        if(graph.lookup&&graph.lookup!=persistentObjectLookup)
            bytes=Add(bytes,sizeof(RuntimeObjectLookupControl)-sizeof(QuestVr::ObjectLookup::Index)+
                4u*sizeof(void*)+graph.lookup->index.RetainedBytes());
        return bytes;
    }
    std::unique_ptr<RootClientGraph> CaptureNativeClientRootForBegin() {
        if(nativeClientRoot_)throw std::runtime_error("Native Client root survived its previous VM execution");
        auto* session=persistentPlayerSession?persistentPlayerSession->get():nullptr;
        auto* domain=persistentEngineCollision?persistentEngineCollision->get():nullptr;
        if(!session&&!domain)return {};
        const auto count=session?session->viewports.size():0u;
        if(count>QuestVr::NativeClientRegistry::MaximumViewports)
            throw std::runtime_error("Native Client root array exceeds its live bound");
        using QuestVr::DecalStateDetail::Add;
        using QuestVr::DecalStateDetail::Multiply;
        auto admission=Add(sizeof(RootClientGraph),Multiply(count,sizeof(RootViewportGraph)));
        admission=Add(admission,Multiply(6u+6u*count,sizeof(GCRootNode)));
        admission=Add(admission,sizeof(QuestVr::NativeClientLifetime)+4u*sizeof(void*));
        ValidateStateBudget(admission);
        auto graph=std::make_unique<RootClientGraph>();
        graph->runtime=persistentRuntime->get();graph->session.set(session);
        graph->engineDomain.set(domain);graph->engine.set(domain?domain->engine:nullptr);
        graph->client.set(session?session->client:nullptr);
        auto* engine=graph->engine.get();
        graph->engineClient.set(engine&&engine->nativeEngineInstance?engine->nativeEngineInstance->values.client:nullptr);
        auto* level=engine&&engine->nativeEngineInstance?engine->nativeEngineInstance->currentLevel:nullptr;
        if(!level&&!persistentWorldActors.empty()&&persistentWorldActors.front())
            level=persistentWorldActors.front()->nativeXLevel;
        graph->level.set(level);graph->mapGeneration=level?level->nativeMapGeneration:0u;
        graph->clientEngineAtEntry=session?session->nativeClient.engine:nullptr;
        graph->clientIdentity=session?session->nativeClient.clientIdentity:0u;
        graph->lookup=persistentObjectLookup;graph->lookupGeneration=graph->lookup?graph->lookup->generation:0u;
        graph->lifetime=persistentNativeClient.RetainBoundLifetime();
        graph->clientIndex=graph->client?graph->client->objectIndex.Index():0u;
        graph->engineIndex=engine?engine->objectIndex.Index():0u;
        graph->levelIndex=level?level->objectIndex.Index():0u;
        graph->viewports.reserve(count);
        // A library-selected larger capacity is real storage, not free credit.
        ValidateStateBudget(Add(RootClientGraphStorageBytes(*graph),
            Multiply(count,6u*sizeof(GCRootNode))));
        if(session)for(auto* viewport:session->viewports) {
            if(!viewport)throw std::runtime_error("Native Client root has a null actual viewport");
            const auto payload=std::find_if(session->nativeViewports.begin(),session->nativeViewports.end(),
                [viewport](const auto& value){return value.viewport==viewport;});
            if(payload==session->nativeViewports.end())
                throw std::runtime_error("Native Client root viewport has no actual payload");
            // A future initialized renderer needs its concrete retained-byte
            // provider. Do not invent semantic device storage/accounting here.
            if(payload->renderDevice)
                throw std::runtime_error("Native Client root concrete render-device byte provider is not implemented");
            graph->viewports.emplace_back(viewport,payload->renderDevice);
        }
        ValidateStateBudget(RootClientGraphStorageBytes(*graph));
        return graph; // Caller installs only at the final Begin boundary.
    }
    bool RootObjectIndexedNoThrow(const RuntimeObject* object,std::uint32_t index) const noexcept {
        if(!nativeClientRoot_||!object||!index||!object->objectIndex||object->objectIndex.Index()!=index||
            object->lookupControl!=nativeClientRoot_->lookup)return false;
        const auto& lookup=nativeClientRoot_->lookup;
        if(!lookup)return true; // Explicit legacy pre-lookup constructor profile.
        if(!lookup->active||lookup->generation!=nativeClientRoot_->lookupGeneration)return false;
        const auto* record=lookup->index.RecordForIdentity(reinterpret_cast<std::uintptr_t>(object));
        return record&&record->index==index&&record->identity==reinterpret_cast<std::uintptr_t>(object)&&
            record->outerIdentity==reinterpret_cast<std::uintptr_t>(object->outer);
    }
    bool RootClientGraphStillOwnedNoThrow() const noexcept {
        if(!nativeClientRoot_)return false;
        const auto& graph=*nativeClientRoot_;
        if(!persistentRuntime||persistentRuntime->get()!=graph.runtime||persistentObjectLookup!=graph.lookup||
            (graph.lookup&&(!graph.lookup->active||graph.lookup->generation!=graph.lookupGeneration)))return false;
        auto* session=persistentPlayerSession?persistentPlayerSession->get():nullptr;
        auto* domain=persistentEngineCollision?persistentEngineCollision->get():nullptr;
        if(session!=graph.session.get()||domain!=graph.engineDomain.get())return false;
        auto* client=graph.client.get();auto* engine=graph.engine.get();
        if((session?session->client:nullptr)!=client||(domain?domain->engine:nullptr)!=engine)return false;
        if(engine) {
            if(!RootObjectIndexedNoThrow(engine,graph.engineIndex)||!engine->nativeEngineInstance||
                engine->nativeEngineInstance->values.client!=graph.engineClient.get())return false;
        } else if(graph.engineClient)return false;
        if(client) {
            // Structural provenance for noexcept UNDO, not permission to use
            // targets. Revoke alone is not Clear: restore this still-owned
            // graph exactly, while publication separately requires Alive.
            if(!session||!graph.clientIdentity||!graph.lifetime||
                persistentNativeClient.RetainBoundLifetime()!=graph.lifetime||
                session->nativeClient.clientIdentity!=graph.clientIdentity||
                session->nativeClient.engine!=graph.clientEngineAtEntry||
                client->nativeClientIdentity!=graph.clientIdentity||client->nativeViewportIdentity||
                !client->cls||client->cls->reflection.objectPath!="Engine.Client"||
                !RootObjectIndexedNoThrow(client,graph.clientIndex))return false;
            if(graph.clientEngineAtEntry) {
                if(graph.clientEngineAtEntry!=engine||graph.engineClient.get()!=client)return false;
            } else if(graph.engineClient)return false;
        } else {
            if(graph.engineClient)return false;
            if(session&&(session->nativeClient.engine||session->nativeClient.clientIdentity||!session->viewports.empty()))return false;
        }
        return true; // No viewport-count liveness condition belongs here.
    }
    const RootViewportGraph* CapturedRootViewportNoThrow(RuntimeObject* viewport) const noexcept {
        if(!nativeClientRoot_||!viewport)return nullptr;
        for(const auto& captured:nativeClientRoot_->viewports)
            if(captured.viewport.get()==viewport)return &captured;
        return nullptr;
    }
    bool RestorableRootViewportNoThrow(RuntimeObject* viewport) const noexcept {
        if(!RootClientGraphStillOwnedNoThrow())return false;
        const auto* captured=CapturedRootViewportNoThrow(viewport);
        if(!captured)return false; // Compare provenance before dereferencing.
        const auto& graph=*nativeClientRoot_;auto* session=graph.session.get();
        if(!session||std::find(session->viewports.begin(),session->viewports.end(),viewport)==session->viewports.end()||
            !RootObjectIndexedNoThrow(viewport,captured->viewportIndex)||viewport->outer!=graph.client.get()||
            viewport->nativeClientIdentity!=graph.clientIdentity||viewport->nativeViewportIdentity!=captured->identity||
            !viewport->cls||viewport->cls->reflection.objectPath!="Engine.Viewport")return false;
        std::size_t matching{};
        for(const auto& payload:session->nativeViewports)if(payload.viewport==viewport) {
            if(payload.clientIdentity!=graph.clientIdentity||payload.viewportIdentity!=captured->identity||
                !payload.renderSlotBound||payload.renderDevice!=captured->renderDevice.get())return false;
            ++matching;
        }
        if(matching!=1u)return false;
        // No render-target access in undo. Clear/unindex/replacement already
        // fail structural ownership above. Live access additionally validates
        // metadata in ValidateRootNativeClientLease/NativeSessionViewport.
        return true;
    }
    bool RestorableRootViewportActorNoThrow(RuntimeObject* viewport,RuntimeObject* actor) const noexcept {
        if(!actor)return true;
        if(!RestorableRootViewportNoThrow(viewport))return false;
        const auto* captured=CapturedRootViewportNoThrow(viewport);
        if(captured->actor.get()!=actor||!RootObjectIndexedNoThrow(actor,captured->actorIndex))return false;
        auto* level=nativeClientRoot_->level.get();
        if(!level||actor->nativeXLevel!=level||!RootObjectIndexedNoThrow(level,nativeClientRoot_->levelIndex)||
            level->nativeMapGeneration!=nativeClientRoot_->mapGeneration)return false;
        // Do not test worldRemoved/bDeleteMe or live slot presence here: those
        // before-images/removed slots may be restored LATER in the same undo.
        return true;
    }
    bool RestorableRootCanvasPeerNoThrow(RuntimeObject* owner,RuntimeObject* peer) const noexcept {
        if(!peer)return true;
        if(!nativeClientRoot_)return false;
        for(const auto& captured:nativeClientRoot_->viewports) {
            const bool viewportSide=owner==captured.viewport.get()&&peer==captured.canvas.get();
            const bool canvasSide=owner==captured.canvas.get()&&peer==captured.viewport.get();
            if(!viewportSide&&!canvasSide)continue;
            auto* canvas=captured.canvas.get();
            return RestorableRootViewportNoThrow(captured.viewport.get())&&canvas&&canvas->nativeCanvasObject&&
                canvas->nativeClientIdentity==nativeClientRoot_->clientIdentity&&
                RootObjectIndexedNoThrow(canvas,captured.canvasIndex);
        }
        return false;
    }
    bool RestorableRootConsoleNoThrow(RuntimeObject* viewport,RuntimeObject* console) const noexcept {
        if(!console)return true;
        if(!RestorableRootViewportNoThrow(viewport))return false;
        const auto* captured=CapturedRootViewportNoThrow(viewport);
        return captured->console.get()==console&&console->nativeConsoleObject&&
            console->nativeConsoleOwner==viewport&&console->nativeClientIdentity==nativeClientRoot_->clientIdentity&&
            RootObjectIndexedNoThrow(console,captured->consoleIndex);
    }
    bool RestorableRootInputNoThrow(RuntimeObject* input,RuntimeObject* viewport) const noexcept {
        if(!input||!RestorableRootViewportNoThrow(viewport))return false;
        const auto* captured=CapturedRootViewportNoThrow(viewport);
        return captured->input.get()==input&&viewport->nativeViewportInput==input&&
            input->nativeInputInstance&&input->nativeClientIdentity==nativeClientRoot_->clientIdentity&&
            input->nativeInputInstance->ownerViewport==viewport&&!input->nativeInputInstance->retired&&
            input->nativeInputInstance->clientIdentity==nativeClientRoot_->clientIdentity&&
            input->nativeInputInstance->viewportIdentity==captured->identity&&
            RootObjectIndexedNoThrow(input,captured->inputIndex);
    }
    void PreserveRetiredRootInputNoThrow(RuntimeObject* input,RuntimeObject* viewport) noexcept {
        if(!input||!input->nativeInputInstance||RestorableRootInputNoThrow(input,viewport))return;
        auto& payload=*input->nativeInputInstance;
        // Exact existing ClearNativePlayerObjects retirement policy, applied
        // AFTER COW pointer undo so an old initialized receipt cannot revive.
        payload.retired=true;payload.ownerViewport=nullptr;payload.values.viewport=nullptr;
        payload.initialized=false;payload.initialization.reset();
        payload.sourceCursor={};payload.physical={};
    }
    template<class BeforeImage>
    void RestoreRootNativeSessionEdgesNoThrow(RuntimeObject* object,const BeforeImage& before) noexcept {
        const bool sameGraph=RootClientGraphStillOwnedNoThrow();
        const bool restorePlayer=sameGraph&&(!before.player||RestorableRootViewportNoThrow(before.player));
        if(restorePlayer) {
            object->nativePlayerBinding=before.playerBinding;object->nativePlayer=before.player;
        } else {
            // Preserve an established native None after Clear. Do not restore
            // false and reopen an authored Player fallback on a retired graph.
            object->nativePlayerBinding=object->nativePlayerBinding||before.playerBinding||
                object->nativePlayer!=nullptr||before.player!=nullptr;
            object->nativePlayer=nullptr;
        }
        object->nativeViewportActor=sameGraph&&RestorableRootViewportNoThrow(object)&&
            RestorableRootViewportActorNoThrow(object,before.viewportActor)?before.viewportActor:nullptr;
        object->nativeCanvasPeer=RestorableRootCanvasPeerNoThrow(object,before.canvasPeer)?before.canvasPeer:nullptr;
        object->nativeViewportConsole=RestorableRootConsoleNoThrow(object,before.viewportConsole)?before.viewportConsole:nullptr;
    }
    void ValidateRootNativeHeader(RuntimeObject* object) {
        if(!object||!object->cls||!object->lookupName||!object->objectIndex)
            throw std::runtime_error("Native Client graph has no actual UObject header");
        const auto read=[&](const char* key,Kind kind) {
            const auto field=DescribeProperty(key);
            if(field.zero.kind!=kind||field.arrayDimension!=1u||!OwnsProperty(object->cls,field))
                throw std::runtime_error("Native Client graph header schema has wrong type/ownership");
            auto value=Read(object,field,0u);
            if(value.kind!=kind)throw std::runtime_error("Native Client graph header read changed kind");
            return value;
        };
        if(read("Core.Object.Class",Kind::Object).text!=object->cls->reflection.objectPath||
            read("Core.Object.Name",Kind::Name).text!=object->lookupName->ToString()||
            read("Core.Object.Outer",Kind::Object).text!=(object->outer?object->outer->reflection.objectPath:std::string{})||
            read("Core.Object.ObjectFlags",Kind::Int).integer!=static_cast<std::int32_t>(object->reflection.flags))
            throw std::runtime_error("Native Client graph native/reflected header mismatch");
    }
    void ValidateRootNativeClientLease() {
        if(!clientDependent_&&(!nativeClientRoot_||!nativeClientRoot_->engineObserved))return;
        if(!RootClientGraphStillOwnedNoThrow())
            throw std::runtime_error("Native client lifetime revoked, replaced or unindexed during script transaction");
        const auto& graph=*nativeClientRoot_;
        if(graph.client&&!persistentNativeClient.MatchesLifetime(graph.lifetime))
            throw std::runtime_error("Native client lifetime revoked during script transaction");
        if(clientDependent_&&!graph.client)
            throw std::runtime_error("Viewport-dependent root never owned an actual native Client");
        if(auto* client=graph.client.get()) {
            if(Object(client->reflection.objectPath)!=client)throw std::runtime_error("Native Client root lost canonical object identity");
            ValidateRootNativeHeader(client);
            const auto& viewports=graph.session->viewports;
            if(viewports.size()>QuestVr::NativeClientRegistry::MaximumViewports)
                throw std::runtime_error("Native Client root viewport array exceeds its live bound");
            for(std::size_t slot=0u;slot<viewports.size();++slot) {
                if(!RestorableRootViewportNoThrow(viewports[slot])||
                    NativeSessionViewport(viewports[slot]->nativeViewportIdentity)!=viewports[slot]||
                    std::find(viewports.begin(),viewports.begin()+slot,viewports[slot])!=viewports.begin()+slot)
                    throw std::runtime_error("Native Client root has a stale, foreign or duplicate actual viewport");
            }
            for(const auto& payload:graph.session->nativeViewports)
                if(payload.viewport&&std::find(viewports.begin(),viewports.end(),payload.viewport)==viewports.end())
                    throw std::runtime_error("Native Client root retains a payload outside its actual array");
        }
        if(graph.engineObserved) {
            auto* engine=graph.engine.get();auto* level=graph.level.get();
            if(!engine||!level||!ActualNativeCollisionObjectValid(engine)||
                Object(engine->reflection.objectPath)!=engine||level!=CurrentClockLevel()||
                level->nativeLevelEngine!=engine||engine->nativeEngineInstance->currentLevel!=level||
                level->nativeMapGeneration!=graph.mapGeneration)
                throw std::runtime_error("Native Client root lost its actual Level.Engine context");
            ValidateRootNativeHeader(engine);
            if(graph.client&&graph.clientEngineAtEntry!=engine)
                throw std::runtime_error("Engine consumer used a constructor-only uninitialized Client");
        }
    }
    RuntimeObject* RequireActualEngineClient(RuntimeObject* nativeLevel) {
        if(!transaction_)throw std::runtime_error("Actual Engine.Client access requires its caller root");
        LifecycleStep();
        if(!nativeClientRoot_||!RootClientGraphStillOwnedNoThrow())
            throw std::runtime_error("Actual Engine.Client has no retained current native owner");
        auto& graph=*nativeClientRoot_;auto* engine=graph.engine.get();
        if(!nativeLevel||nativeLevel!=graph.level.get()||nativeLevel!=CurrentClockLevel()||
            !engine||nativeLevel->nativeLevelEngine!=engine||!ActualNativeCollisionObjectValid(engine)||
            !engine->nativeEngineInstance||engine->nativeEngineInstance->currentLevel!=nativeLevel||
            !graph.lookup||!graph.lookup->active||Object(engine->reflection.objectPath)!=engine)
            throw std::runtime_error("Actual Engine.Client lacks prepared native Level/Engine/lookup authority");
        ValidateRootNativeHeader(engine);
        auto* client=engine->nativeEngineInstance->values.client;
        if(!client) {
            // Do not interpret an uninitialized detached constructor fixture
            // as a faithful initial local-player driver or a headless success.
            if(graph.client)throw std::runtime_error("Actual Engine.Client is uninitialized for the detached constructor-only session");
            graph.engineObserved=true;
            return nullptr; // Actual Engine edge is explicitly, genuinely None.
        }
        if(client!=graph.client.get()||!graph.session||graph.session->client!=client||
            graph.session->nativeClient.engine!=engine||graph.clientEngineAtEntry!=engine||
            !client->cls||client->cls->reflection.objectPath!="Engine.Client"||
            !IsDerivedFromPath(client->cls,"Core.Object")||Object(client->reflection.objectPath)!=client)
            throw std::runtime_error("Actual Client.Engine/Engine.Client pair is uninitialized, foreign or stale");
        ValidateRootNativeHeader(client);
        graph.engineObserved=true;clientDependent_=true;
        ValidateRootNativeClientLease();
        return client;
    }
    std::size_t ActualClientViewportCount(RuntimeObject* client) {
        if(!client||RequireActualEngineClient(CurrentClockLevel())!=client)
            throw std::runtime_error("Actual Client viewport count changed native Client owner");
        return nativeClientRoot_->session->viewports.size(); // Actual live array.
    }
    RuntimeObject* ActualClientViewportAt(RuntimeObject* client,std::size_t slot) {
        if(!client||RequireActualEngineClient(CurrentClockLevel())!=client)
            throw std::runtime_error("Actual Client viewport access changed native Client owner");
        const auto& array=nativeClientRoot_->session->viewports; // Never cached across callback.
        if(slot>=array.size())return nullptr;
        auto* viewport=array[slot];
        if(!RestorableRootViewportNoThrow(viewport)||Object(viewport->reflection.objectPath)!=viewport||
            !IsDerivedFromPath(viewport->cls,"Engine.Player"))
            throw std::runtime_error("Actual Client array contains a foreign, stale or unprepared viewport");
        ValidateRootNativeHeader(viewport);
        return viewport;
    }
    void RetireRootNativeClientLease() noexcept { nativeClientRoot_.reset(); }

