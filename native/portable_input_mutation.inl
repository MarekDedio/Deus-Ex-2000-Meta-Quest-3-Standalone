// PortableActorVmHost private native Input/default/config transaction journal.
// Beforeimages retain the ORIGINAL owning payload; the live slot receives a
// deep working copy. Reference undo precedes these pointer-swap restorations.
    struct NativeInputInstanceBefore {
        GCRoot<RuntimeObject> owner,viewport,keyEnum,nativeViewport;
        std::unique_ptr<RuntimeNativeInputInstance> payload;
    };
    struct NativeInputDefaultBefore {
        GCRoot<RuntimeObject> owner,parent,keyEnum,nativeViewport;
        std::unique_ptr<const RuntimePreparedNativeDefaults> payload;
    };
    std::vector<NativeInputInstanceBefore> beforeNativeInputInstances_;
    std::vector<NativeInputDefaultBefore> beforeNativeInputDefaults_;
    std::unique_ptr<const RuntimeNativeInputConfig> beforeNativeInputConfig_;
    std::size_t nativeInputReferenceBytes_{};
    std::optional<QuestVr::ConfigFile::Snapshot> nativeConfigExpected_;
    QuestVr::ConfigFile::Bytes nativeConfigBytes_;
    bool nativeConfigDirty_{};
    PortableCoreExecResult* nativeCoreExecResult_{};

    static std::size_t NativeInputInstancePayloadBytes(const RuntimeNativeInputInstance& input) {
        return QuestVr::DecalStateDetail::Add(sizeof(input)-sizeof(input.values),NativeInputValuesBytes(input.values));
    }
    static std::size_t NativeInputDefaultPayloadBytes(const RuntimePreparedNativeDefaults& input) {
        if(!input.input) throw std::runtime_error("Native Input beforeimage has no input payload");
        // Input defaults have no admitted native Engine payload. Copying that
        // zero-valued parent tail is not permission to omit nonempty strings.
        const auto& engine=input.engine;
        if(!engine.protocol.empty()||!engine.host.empty()||!engine.map.empty()||!engine.portal.empty()||
            !engine.options.empty()||!engine.serverActors.empty()||!engine.serverPackages.empty())
            throw std::runtime_error("Native Input default has an unsupported nonempty Engine tail");
        return QuestVr::DecalStateDetail::Add(sizeof(input),NativeInputValuesBytes(*input.input));
    }
    void RequireEmptyNativeInputJournal() const {
        if(!beforeNativeInputInstances_.empty()||!beforeNativeInputDefaults_.empty()||beforeNativeInputConfig_||nativeInputReferenceBytes_||
            nativeConfigExpected_||!nativeConfigBytes_.empty()||nativeConfigDirty_||nativeCoreExecResult_)
            throw std::runtime_error("Native Input journal survived its previous root transaction");
    }
    std::size_t NativeInputJournalBytes() const {
        using QuestVr::DecalStateDetail::Add;using QuestVr::DecalStateDetail::Multiply;
        auto bytes=Add(nativeInputReferenceBytes_,Multiply(beforeNativeInputInstances_.capacity(),sizeof(NativeInputInstanceBefore)));
        bytes=Add(bytes,Multiply(beforeNativeInputDefaults_.capacity(),sizeof(NativeInputDefaultBefore)));
        for(const auto& entry:beforeNativeInputInstances_) {
            bytes=Add(bytes,4u*sizeof(GCRootNode));
            if(entry.payload)bytes=Add(bytes,NativeInputInstancePayloadBytes(*entry.payload));
        }
        for(const auto& entry:beforeNativeInputDefaults_) {
            bytes=Add(bytes,4u*sizeof(GCRootNode));
            if(entry.payload)bytes=Add(bytes,NativeInputDefaultPayloadBytes(*entry.payload));
        }
        if(beforeNativeInputConfig_)bytes=Add(bytes,NativeInputConfigBytes(*beforeNativeInputConfig_));
        if(nativeConfigExpected_)bytes=Add(bytes,nativeConfigExpected_->RetainedBytes());
        bytes=Add(bytes,nativeConfigBytes_.capacity());
        return bytes;
    }
    RuntimeNativeInputValues& MutateNativeInputValues(RuntimeObject* object) {
        if(!transaction_||!object)throw std::runtime_error("Native Input mutation requires its active root owner");
        if(object->nativeInputInstance) {
            for(auto& entry:beforeNativeInputInstances_)if(entry.owner.get()==object)return object->nativeInputInstance->values;
            if(!ActualNativeInputInstanceValid(object))
                throw std::runtime_error("Native Input mutation encountered an unsupported or stale indexed instance");
            if(beforeNativeInputInstances_.size()>=QuestVr::NativeClientRegistry::MaximumViewports)
                throw std::runtime_error("Native Input beforeimage inventory exceeds the actual viewport bound");
            const auto bytes=NativeInputInstancePayloadBytes(*object->nativeInputInstance);
            ValidateStateBudget(QuestVr::DecalStateDetail::Add(bytes,
                (beforeNativeInputInstances_.size()+2u)*sizeof(NativeInputInstanceBefore)+4u*sizeof(GCRootNode)));
            auto replacement=std::make_unique<RuntimeNativeInputInstance>(*object->nativeInputInstance);
            auto* working=replacement.get();
            const auto& current=*object->nativeInputInstance;
            NativeInputInstanceBefore before{GCRoot<RuntimeObject>(object),GCRoot<RuntimeObject>(current.ownerViewport),
                GCRoot<RuntimeObject>(current.values.keyEnum),GCRoot<RuntimeObject>(current.values.viewport),{}};
            beforeNativeInputInstances_.reserve(beforeNativeInputInstances_.size()+1u);
            // All allocation/root-node preparation finishes before ownership
            // changes. No throwing work lies between the following moves.
            beforeNativeInputInstances_.push_back(std::move(before));
            beforeNativeInputInstances_.back().payload=std::move(object->nativeInputInstance);
            object->nativeInputInstance=std::move(replacement);
            ValidateStateBudget();return working->values;
        }
        if(!object->defaultOwner||!InputSchemaDefaultValid(object->defaultOwner)||
            object->defaultOwner->defaultObject!=object||!object->preparedNativeDefaults||!object->preparedNativeDefaults->input)
            throw std::runtime_error("Native Input default mutation lacks its actual prepared CDO authority");
        for(auto& entry:beforeNativeInputDefaults_)if(entry.owner.get()==object)
            return *const_cast<RuntimePreparedNativeDefaults*>(object->preparedNativeDefaults.get())->input;
        if(beforeNativeInputDefaults_.size()>=2u)
            throw std::runtime_error("Native Input default journal exceeds its two admitted classes");
        const auto bytes=NativeInputDefaultPayloadBytes(*object->preparedNativeDefaults);
        ValidateStateBudget(QuestVr::DecalStateDetail::Add(bytes,
            (beforeNativeInputDefaults_.size()+2u)*sizeof(NativeInputDefaultBefore)+4u*sizeof(GCRootNode)));
        auto replacement=CopyInputDefault(*object->preparedNativeDefaults);auto* working=replacement.get();
        const auto& current=*object->preparedNativeDefaults;
        NativeInputDefaultBefore before{GCRoot<RuntimeObject>(object),GCRoot<RuntimeObject>(current.superDefault),
            GCRoot<RuntimeObject>(current.input->keyEnum),GCRoot<RuntimeObject>(current.input->viewport),{}};
        beforeNativeInputDefaults_.reserve(beforeNativeInputDefaults_.size()+1u);
        beforeNativeInputDefaults_.push_back(std::move(before));
        beforeNativeInputDefaults_.back().payload=std::move(object->preparedNativeDefaults);
        object->preparedNativeDefaults=std::move(replacement);
        ValidateStateBudget();return *working->input;
    }
    RuntimeNativeInputConfig& MutateNativeInputConfig() {
        if(!transaction_||!persistentInputSchema||!persistentInputSchema->get()||!persistentInputSchema->get()->config)
            throw std::runtime_error("Native config mutation lacks its active root/document authority");
        auto& domain=*persistentInputSchema->get();
        if(beforeNativeInputConfig_)return *const_cast<RuntimeNativeInputConfig*>(domain.config.get());
        if(domain.config->charge||domain.config->store)throw std::runtime_error("Persistent native config retained a borrowed import callback");
        ValidateStateBudget(NativeInputConfigBytes(*domain.config));
        auto replacement=std::make_unique<RuntimeNativeInputConfig>(*domain.config);auto* working=replacement.get();
        beforeNativeInputConfig_=std::move(domain.config);domain.config=std::move(replacement);
        ValidateStateBudget();return *working;
    }
    void ClearNativeInputJournal() noexcept {
        beforeNativeInputInstances_.clear();beforeNativeInputDefaults_.clear();beforeNativeInputConfig_.reset();
        nativeInputReferenceBytes_=0u;
        nativeConfigExpected_.reset();nativeConfigBytes_.clear();nativeConfigDirty_=false;nativeCoreExecResult_=nullptr;
    }
    void ValidateNativeInputMutationLeases() const {
        for(const auto& entry:beforeNativeInputInstances_)
            if(!ActualNativeInputInstanceValid(entry.owner.get()))
                throw std::runtime_error("Native Input lease was revoked before config/state publication");
    }
    void RollbackNativeInputJournal() noexcept {
        for(auto& entry:beforeNativeInputInstances_)entry.owner.get()->nativeInputInstance.swap(entry.payload);
        for(auto& entry:beforeNativeInputDefaults_)entry.owner.get()->preparedNativeDefaults.swap(entry.payload);
        if(beforeNativeInputConfig_) {
            if(!persistentInputSchema||!persistentInputSchema->get())std::terminate();
            persistentInputSchema->get()->config.swap(beforeNativeInputConfig_);
        }
        ClearNativeInputJournal();
    }
