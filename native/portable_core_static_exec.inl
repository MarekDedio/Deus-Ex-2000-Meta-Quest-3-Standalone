// PortableActorVmHost: original Core.StaticExec SET and its real Input payloads.
// This service does not stand in for the missing viewport FExec provider chain.
    struct CoreSetOperations {
        PortableActorVmHost& host;
        void Step() {host.LifecycleStep();}
        void Admit(std::size_t bytes) {host.ValidateStateBudget(bytes);}
    };
    static std::size_t NativeInputBindingSlot(const RuntimeNativeInputDomain& domain,RuntimeObject* field) {
        if(!ActualNativeInputSchemaObjectValid(field)||!field->nativeProperty||field->outer!=domain.inputClass||
            !field->cls||field->cls->reflection.objectPath!="Core.StrProperty")
            throw std::runtime_error("SET Input binding lacks its actual StrProperty authority");
        const auto& property=*field->nativeProperty;
        if(property.dimension!=1u||property.elementSize!=12u||property.offset<0x2acu||
            (property.offset-0x2acu)%12u||property.referencedStruct||
            (property.offset-0x2acu)/12u>=255u)
            throw std::runtime_error("SET Input binding has an unsupported native storage offset");
        return (property.offset-0x2acu)/12u;
    }
    void WriteNativeInputCell(RuntimeObject* object,RuntimeObject* field,std::size_t index,
        RuntimeObject* member,const Value& value,QuestVr::Vm::Execution& execution) {
        if(!persistentInputSchema||!persistentInputSchema->get())
            throw std::runtime_error("Native Input write has no schema owner");
        auto& domain=*persistentInputSchema->get();
        auto& values=MutateNativeInputValues(object);
        std::string* text{};NameString* name{};
        if(field==domain.fields[2u]) {
            if(!ActualNativeInputSchemaObjectValid(field)||!field->nativeProperty||
                field->nativeProperty->referencedStruct!=domain.aliasStruct||index>=values.aliases.size()||
                !ActualNativeInputSchemaObjectValid(member))
                throw std::runtime_error("Native Input Alias write lost its Struct/slot/member authority");
            if(member==domain.fields[0u])name=&values.aliases[index].alias;
            else if(member==domain.fields[1u])text=&values.aliases[index].command;
            else throw std::runtime_error("Native Input Alias write selected an unaudited member");
        }else {
            if(index||member)throw std::runtime_error("Scalar native Input binding has a fabricated array/member offset");
            text=&values.bindings[NativeInputBindingSlot(domain,field)];
        }
        if(value.kind!=(name?Kind::Name:Kind::String))
            throw std::runtime_error("Native Input cell write has the wrong typed value");
        // Reuse only the stable physical cell capability, NOT its VM writes
        // or beforeimages. Repeated config reloads still call WriteReference
        // below with every distinct old Value in original import order.
        for(const auto& cached:nativeInputCellReferences_) {
            LifecycleStep();
            if(cached.owner==object&&cached.field==field&&cached.member==member&&cached.index==index) {
                if(!cached.reference)throw std::runtime_error("Native Input cached cell has no actual reference");
                execution.WriteReference(cached.reference,value);return;
            }
        }
        if(nativeInputCellReferences_.size()==NativeInputCellReferenceLimit)
            throw std::runtime_error("Native Input physical-cell reference inventory exceeds its actual owner/field bounds");
        constexpr std::size_t referenceBytes=2u*sizeof(Reference)+8u*sizeof(void*)+256u;
        auto growthBytes=std::size_t{};
        auto capacity=nativeInputCellReferences_.capacity();
        if(nativeInputCellReferences_.size()==capacity) {
            capacity=std::min(NativeInputCellReferenceLimit,capacity?capacity*2u:32u);
            growthBytes=QuestVr::DecalStateDetail::Multiply(capacity,sizeof(NativeInputCellReference));
        }
        // Existing cache capacity is already retained. New allocation may
        // overlap that old vector while reserve relocates its shared_ptrs.
        ValidateStateBudget(QuestVr::DecalStateDetail::Add(referenceBytes,growthBytes));
        if(growthBytes) {
            nativeInputCellReferences_.reserve(capacity);
            if(nativeInputCellReferences_.capacity()>capacity)
                throw std::runtime_error("Native Input reference-vector growth exceeds its admitted bundled-library profile");
        }
        auto reference=std::make_shared<Reference>();
        reference->zero=Value::Text(name?Kind::Name:Kind::String,{});
        // Capture the actual stable working storage. Undo may run after lease
        // revocation; it must not allocate a second payload or reauthenticate a
        // now-retired viewport. Host COW rollback follows reference undo.
        reference->read=[text,name] {return Value::Text(name?Kind::Name:Kind::String,name?name->ToString():*text);};
        reference->write=[text,name](const Value& incoming) {
            if(incoming.kind!=(name?Kind::Name:Kind::String))
                throw std::runtime_error("Native Input reference undo/write has the wrong typed storage");
            if(name)*name=NameString(incoming.text);else *text=incoming.text;
        };
        nativeInputCellReferences_.push_back({object,field,member,index,reference});
        nativeInputReferenceBytes_=QuestVr::DecalStateDetail::Add(nativeInputReferenceBytes_,referenceBytes);
        execution.WriteReference(reference,value);
    }
    void ImportCoreInputProperty(RuntimeObject* object,RuntimeObject* field,std::u16string_view value,
        RuntimeNativeInputConfig& profile,QuestVr::Vm::Execution& execution) {
        auto& domain=*persistentInputSchema->get();auto& values=MutateNativeInputValues(object);
        const std::function<void(RuntimeObject*,std::size_t,RuntimeObject*,const Value&)> writer=
            [&](RuntimeObject* property,std::size_t index,RuntimeObject* member,const Value& incoming) {
                WriteNativeInputCell(object,property,index,member,incoming,execution);
            };
        struct StoreScope {
            RuntimeNativeInputConfig& profile;
            decltype(RuntimeNativeInputConfig::store) previous;
            ~StoreScope() {profile.store=previous;}
        } store{profile,profile.store};profile.store=&writer;
        // Parsed command storage is retained by CoreStaticExec's scope. The
        // ImportText UTF16/encoded temporary is separate and must stay visible
        // while the nested COW/reference/undo admissions run.
        InputConfigScratch importScratch(profile,QuestVr::DecalStateDetail::Add(8192u,
            QuestVr::DecalStateDetail::Multiply(QuestVr::DecalStateDetail::Add(value.size(),1u),16u)));
        QuestVr::NativeConfigImport::Result imported;
        if(field==domain.fields[2u])
            imported=ImportNativeInputAlias(domain,profile,values.aliases[0u],0u,field,value,1u);
        else {
            static_cast<void>(NativeInputBindingSlot(domain,field));std::u16string parsed;
            imported=QuestVr::NativeConfigImport::String(value,1u,parsed);
            if(imported.returned)WriteNativeInputCell(object,field,0u,nullptr,
                Value::Text(Kind::String,QuestVr::UnrealString::Encode(parsed)),execution);
        }
        if(!imported.returned)++profile.failures;
    }
    std::string ExportCoreInputAlias(const RuntimeNativeInputDomain& domain,const RuntimeNativeInputAlias& value) {
        std::string result="(";std::size_t count{};
        for(auto* field=domain.aliasStruct->nativeChildren;field;field=field->nativeProperty->next) {
            LifecycleStep();
            if(++count>2u||!ActualNativeInputSchemaObjectValid(field)||!field->nativeProperty||
                field->nativeProperty->dimension!=1u||!field->lookupName)
                throw std::runtime_error("Native Alias export has a foreign/cyclic member list");
            if(count>1u)result+=',';
            result+=field->lookupName->ToString();result+='=';
            if(field==domain.fields[0u])result+=value.alias.ToString();
            else if(field==domain.fields[1u]) {
                result+='"';result+=QuestVr::UnrealString::Encode(QuestVr::UnrealString::CString(
                    QuestVr::UnrealString::Decode(value.command)));result+='"';
            }else throw std::runtime_error("Native Alias export selected an unaudited member");
        }
        if(count!=2u)throw std::runtime_error("Native Alias export omitted an actual member");
        result+=')';return result;
    }
    void RequireCoreUserConfigSnapshot() {
        if(nativeConfigExpected_)return;
        if(!persistentUserConfigStorage)throw std::runtime_error("Core SET requires an explicit app-owned User config backend");
        // Backend admission is a data-sized absolute peak, including the
        // returned snapshot/control and sequential seed/target read buffers.
        const auto previous=nativeQueryScratchBytes_;
        struct RestoreScratch {std::size_t& ledger;std::size_t previous;~RestoreScratch(){ledger=previous;}}
            restore{nativeQueryScratchBytes_,previous};
        auto snapshot=persistentUserConfigStorage->backend.ReadAdmitted([this,previous](std::size_t peak) {
            nativeQueryScratchBytes_=previous;ValidateStateBudget(peak);
            nativeQueryScratchBytes_=QuestVr::DecalStateDetail::Add(previous,peak);
        });
        if(snapshot.Data()!=persistentUserConfigStorage->baseline)
            throw std::runtime_error("App-owned config changed outside the current native cache generation");
        nativeQueryScratchBytes_=previous;
        ValidateStateBudget(snapshot.RetainedBytes());nativeConfigExpected_.emplace(std::move(snapshot));
    }
    void SaveCoreInputConfig(RuntimeObject* cls,PortableCoreExecResult& result) {
        auto& domain=*persistentInputSchema->get();
        auto& config=MutateNativeInputConfig();
        const auto* defaults=cls->defaultObject?cls->defaultObject->preparedNativeDefaults.get():nullptr;
        if(!defaults||!defaults->input||!cls->nativeConstructorLinksPrepared)
            throw std::runtime_error("Native Input SaveConfig has no actual linked CDO storage");
        const QuestVr::NativeConfig::Work work{
            [this](std::size_t units){nativeWork_.Step(units);},
            [this](std::size_t extra){ValidateStateBudget(extra);}};
        std::size_t fields{};
        for(auto* field=cls->nativePropertyHeads[1u];field;field=field->nativeProperty->configNext) {
            LifecycleStep();
            if(++fields>8192u||!ActualNativeInputSchemaObjectValid(field)||!field->nativeProperty||
                !(field->nativeProperty->flags&0x4000u)||!field->lookupName)
                throw std::runtime_error("Native Input SaveConfig has a foreign/cyclic Config property list");
            const auto& property=*field->nativeProperty;
            const auto* section=(property.flags&0x40000u)?field->outer:cls;
            for(std::size_t index=0u;index<property.dimension;++index) {
                // Export allocates BEFORE WithString's own admission. Account
                // its UTF16 decode/encode and overlapping native/raw strings
                // while the returned document clone is prepared as well.
                std::size_t sourceBytes{};
                if(field==domain.fields[2u]&&index<defaults->input->aliases.size())
                    sourceBytes=defaults->input->aliases[index].command.capacity()+1024u;
                else sourceBytes=defaults->input->bindings[NativeInputBindingSlot(domain,field)].capacity();
                QuestVr::NativeScratchScope exportScratch(nativeQueryScratchBytes_,
                    QuestVr::DecalStateDetail::Add(QuestVr::DecalStateDetail::Multiply(sourceBytes+1u,16u),8192u),
                    [this](std::size_t extra){ValidateStateBudget(extra);});
                LifecycleStep();auto key=field->lookupName->ToString();
                if(property.dimension>1u)key+='['+std::to_string(index)+']';
                std::string exported;
                if(field==domain.fields[2u]&&index<defaults->input->aliases.size())
                    exported=ExportCoreInputAlias(domain,defaults->input->aliases[index]);
                else {
                    if(index)throw std::runtime_error("Scalar Input config export fabricated an array offset");
                    exported=defaults->input->bindings[NativeInputBindingSlot(domain,field)];
                }
                auto changed=config.document.WithString(section->reflection.objectPath,key,exported,work);
                if(changed) {config.document=std::move(*changed);config.cacheDirty=true;result.cacheChanged=true;}
                ++result.configExports;
            }
        }
        result.flushRequested=true;
        if(config.cacheDirty) {
            auto serialized=config.document.Serialize(work);
            ValidateStateBudget(serialized.capacity()+1u+serialized.size());
            nativeConfigBytes_.assign(serialized.begin(),serialized.end());
            config.sourcePath=persistentUserConfigStorage->targetPath;
            // This is the logical Flush snapshot. A later ResetConfig in this
            // root may dirty the cache again, but must not alter these bytes.
            nativeConfigFlushPending_=true;config.cacheDirty=false;
        }
    }
    void ReloadCoreInputDefaults(RuntimeNativeInputConfig& profile,QuestVr::Vm::Execution& execution,
        PortableCoreExecResult& result) {
        auto& domain=*persistentInputSchema->get();
        for(std::uint32_t index=1u;index<=RuntimeObjectIndices().AllocatedSlots();++index) {
            LifecycleStep();const auto* record=persistentObjectLookup->index.RecordAt(index);if(!record)continue;
            auto* cls=ActualLookupObject(*persistentObjectLookup,record->identity);
            if(cls->defaultOwner||!ActualLookupClass(*persistentObjectLookup,cls)||
                !IsDerivedFromPath(cls,domain.inputClass->reflection.objectPath))continue;
            if(cls!=domain.inputClass&&cls!=domain.inputExtClass)
                throw std::runtime_error("Core Input default reload encountered an unsupported actual descendant class");
            auto* object=cls->defaultObject;
            if(!InputSchemaDefaultValid(cls)||!object)throw std::runtime_error("Core Input reload lacks its actual CDO");
            auto& values=MutateNativeInputValues(object);
            const std::function<void(RuntimeObject*,std::size_t,RuntimeObject*,const Value&)> writer=
                [&](RuntimeObject* field,std::size_t arrayIndex,RuntimeObject* member,const Value& incoming) {
                    WriteNativeInputCell(object,field,arrayIndex,member,incoming,execution);
                };
            profile.store=&writer;
            try {LoadInputDefaultConfig(domain,profile,cls,values,0u,&domain.config->document);}
            catch(...) {profile.store=nullptr;throw;}
            profile.store=nullptr;++result.defaultReloads;
        }
    }
    bool CoreStaticExec(const std::string& command,PortableCoreExecResult& result,
        QuestVr::Vm::Execution& execution) {
        nativeCoreExecResult_=&result;
        if(command.size()>QuestVr::NativeInputKeyQueries::MaximumCommandUnits*3u)
            throw std::runtime_error("Core SET command transport exceeds its bounded FString profile");
        // UTF16 command, persistent packet tokens/value, encoded lookup keys
        // and small descriptors. Input imports/default copies/config/file
        // staging have their own data-sized scoped or retained accounting.
        QuestVr::NativeScratchScope scratch(nativeQueryScratchBytes_,QuestVr::DecalStateDetail::Add(8192u,
            QuestVr::DecalStateDetail::Multiply(QuestVr::DecalStateDetail::Add(command.size(),1u),16u)),
            [this](std::size_t extra){ValidateStateBudget(extra);});
        const auto text=QuestVr::UnrealString::Decode(command);CoreSetOperations operations{*this};
        auto parsed=QuestVr::CoreSetCommand::Begin(text,operations);
        if(!parsed.recognized)return false;
        result.handled=true;auto& packet=*parsed.packet;
        if(!packet.classTokenPresent) {result.output="Unrecognized class ";return true;}
        if(!persistentObjectLookup||!persistentObjectLookup->active)
            throw std::runtime_error("Core SET requires the actual prepared object lookup generation");
        auto* meta=Object("Core.Class");
        const auto selected=FindPortableRuntimeObject({QuestVr::UnrealString::Encode(packet.className),
            meta->objectIndex.Index(),0u,true,false});
        if(!selected) {
            result.output="Unrecognized class "+QuestVr::UnrealString::Encode(packet.className);return true;
        }
        auto* cls=Object(selected->objectPath);
        QuestVr::CoreSetCommand::ReadProperty(text,packet,operations);
        const auto propertyName=QuestVr::UnrealString::Encode(packet.propertyName);
        if(!packet.propertyTokenPresent) {result.output="Unrecognized property ";return true;}
        std::optional<Property> property;
        try {property=ClassProperty(cls,propertyName);}
        catch(const std::runtime_error& error) {
            if(std::string(error.what())!="Runtime property metadata unavailable: "+propertyName)throw;
        }
        if(!property) {result.output="Unrecognized property "+propertyName;return true;}
        QuestVr::CoreSetCommand::ReadValue(text,packet,operations);
        if(!persistentInputSchema||!persistentInputSchema->get()||!persistentInputSchema->get()->config)
            throw std::runtime_error("Core SET Input config service is not prepared");
        auto& domain=*persistentInputSchema->get();
        if(cls!=domain.inputClass&&cls!=domain.inputExtClass)
            throw std::runtime_error("Core SET selected an unsupported actual Class storage service");
        auto* field=Object(property->key);
        if(field!=domain.fields[2u])static_cast<void>(NativeInputBindingSlot(domain,field));
        RequireCoreUserConfigSnapshot();
        RuntimeNativeInputConfig profile;
        const std::function<void(std::size_t)> charge=[this](std::size_t units){nativeWork_.Step(units);};
        const std::function<void(std::size_t)> scratchValidate=[this](std::size_t bytes){ValidateStateBudget(bytes);};
        profile.charge=&charge;profile.scratchLedger=&nativeQueryScratchBytes_;profile.scratchValidate=&scratchValidate;
        // Original indexed-object traversal refreshes its upper bound after
        // each import/PostEditChange, not a weak viewport-vector snapshot.
        for(std::uint32_t index=1u;index<=RuntimeObjectIndices().AllocatedSlots();++index) {
            LifecycleStep();const auto* record=persistentObjectLookup->index.RecordAt(index);if(!record)continue;
            auto* object=ActualLookupObject(*persistentObjectLookup,record->identity);
            if(object->defaultOwner||!object->cls||!IsDerivedFromPath(object->cls,cls->reflection.objectPath))continue;
            if(!ActualNativeInputInstanceValid(object))
                throw std::runtime_error("Core SET encountered an unsupported matching indexed Input lifecycle");
            ImportCoreInputProperty(object,field,packet.value,profile,execution);++result.instanceImports;
            // Actual Input/PostEditChange inherits UObject::Modify. The
            // RF_Transactional bit is false for these authenticated flags4000.
            if(object->reflection.flags&1u)
                throw std::runtime_error("Transactional Input PostEditChange undo backend is not implemented");
        }
        if(!InputSchemaDefaultValid(cls))throw std::runtime_error("Core SET target has no actual Input CDO");
        ImportCoreInputProperty(cls->defaultObject,field,packet.value,profile,execution);++result.defaultImports;
        SaveCoreInputConfig(cls,result);
        ReloadCoreInputDefaults(profile,execution,result);
        result.importFailures=profile.failures;result.importWarnings=profile.warnings;
        return true;
    }
    void PublishNativeUserConfig() {
        if(!nativeConfigFlushPending_)return;
        if(!persistentUserConfigStorage||!nativeConfigExpected_)
            throw std::runtime_error("Native config publication lost its app-owned snapshot authority");
        auto& storage=*persistentUserConfigStorage;
        QuestVr::NativeScratchScope scratch(nativeQueryScratchBytes_,
            storage.backend.PreparePeakBytes(nativeConfigBytes_.size(),*nativeConfigExpected_),
            [this](std::size_t extra){ValidateStateBudget(extra);});
        auto prepared=storage.backend.Prepare(nativeConfigBytes_,*nativeConfigExpected_,QuestVr::ConfigFile::PublicationMode::CommitAllowed);
        // File preparation may block long enough for a platform lifetime to
        // be revoked. Refuse BEFORE rename and let Prepared cancel its stage.
        ValidateNativeInputMutationLeases();
        const auto result=prepared.Publish();
        if(!result.Published())throw std::runtime_error("Native app-owned config was not published");
        // Rename has happened. Only noexcept state publication follows: NEVER
        // roll old memory back over an already-published settings file.
        storage.baseline.swap(nativeConfigBytes_);storage.owned=true;storage.generation=result.generation;
        storage.publication=result.disposition==QuestVr::ConfigFile::Disposition::Durable?
            PortableConfigPublication::Durable:PortableConfigPublication::DurabilityUnknown;
        if(nativeCoreExecResult_)nativeCoreExecResult_->publication=storage.publication;
    }
