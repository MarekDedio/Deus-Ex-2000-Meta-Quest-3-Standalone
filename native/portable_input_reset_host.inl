// Same-root UObject.ResetConfig for the actually prepared User-configured
// Input classes. Native544 uses this service; it is NOT UInput.ResetInput.
    void InputResetPostEditChange(RuntimeObject* object) {
        if(!ActualNativeInputInstanceValid(object)||object->reflection.flags!=0x4000u)
            throw std::runtime_error("Input ResetConfig PostEditChange lacks its actual native instance authority");
        // Input/InputExt inherit UObject.PostEditChange -> Modify. These exact
        // registered implementations and flags4000 have no transactional bit1;
        // do not generalize that proven gate to arbitrary UObject subclasses.
        if(object->reflection.flags&1u)
            throw std::runtime_error("Transactional Input ResetConfig PostEditChange is not implemented");
    }
    void ResetInputClassConfig(RuntimeObject* selected,QuestVr::Vm::Execution& execution,
        PortableInputResetResult& observed) {
        if(!transaction_||!persistentInputSchema||!persistentInputSchema->get()||
            !persistentInputSchema->get()->config||!persistentObjectLookup||!persistentObjectLookup->active)
            throw std::runtime_error("Original Input ResetConfig requires its actual configured schema/cache/root");
        auto& domain=*persistentInputSchema->get();
        if((selected!=domain.inputClass&&selected!=domain.inputExtClass)||!InputSchemaDefaultValid(selected)||
            !ActualLookupClass(*persistentObjectLookup,selected)||QuestVr::NativeInputConfigName!="User")
            throw std::runtime_error("Original ResetConfig selected Class has no audited User Input configuration authority");
        if(!domain.resetSource)
            throw std::runtime_error("Original Input ResetConfig DefUser cache source is not prepared");
        QuestVr::NativeScratchScope frame(nativeQueryScratchBytes_,
            2u*sizeof(RuntimeNativeInputConfig)+4u*sizeof(QuestVr::NativeConfig::Work)+
                4u*sizeof(std::function<void(RuntimeObject*,std::size_t,RuntimeObject*,const Value&)>) +
                selected->reflection.objectPath.size()+1u,
            [this](std::size_t bytes){ValidateStateBudget(bytes);});
        observed.classPath=selected->reflection.objectPath;
        // The immutable source/cache has its OWN read-only authority. It is
        // not a User backend alias and does not request a Flush or a file stage.
        static_cast<void>(domain.resetSource->source.ReadAdmitted(
            [this](std::size_t bytes){ValidateStateBudget(bytes);}));
        nativeInputResetSourceUsed_=true;
        const auto base=nativeQueryScratchBytes_;
        struct RestoreScratch {std::size_t& ledger;std::size_t before;~RestoreScratch(){ledger=before;}}
            restore{nativeQueryScratchBytes_,base};
        const QuestVr::NativeConfig::Work cacheWork{
            [this](std::size_t units){nativeWork_.Step(units);},
            [this](std::size_t bytes){ValidateStateBudget(bytes);}};
        const QuestVr::ResetConfigSection::Work sectionWork{
            [this](std::size_t units){nativeWork_.Step(units);},
            [this,base](std::size_t bytes) {
                nativeQueryScratchBytes_=base;ValidateStateBudget(bytes);
                nativeQueryScratchBytes_=QuestVr::DecalStateDetail::Add(base,bytes);
            },
            [this,&selected,&observed,&cacheWork](std::string_view key,std::string_view value) {
                auto& config=MutateNativeInputConfig();
                auto changed=config.document.WithString(selected->reflection.objectPath,key,value,cacheWork);
                ++observed.overlayCalls;
                if(changed) {
                    config.document=std::move(*changed);config.cacheDirty=true;observed.cacheChanged=true;
                    ValidateStateBudget();
                }
            }};
        const auto section=QuestVr::ResetConfigSection::Apply(&domain.resetSource->document,selected->reflection.objectPath,sectionWork);
        observed.sectionPresent=section.sectionPresent;
        // GetSection's packet is no longer live during class/instance reload.
        nativeQueryScratchBytes_=base;
        RuntimeNativeInputConfig profile;
        const std::function<void(std::size_t)> charge=[this](std::size_t units){nativeWork_.Step(units);};
        const std::function<void(std::size_t)> validate=[this](std::size_t bytes){ValidateStateBudget(bytes);};
        profile.charge=&charge;profile.scratchLedger=&nativeQueryScratchBytes_;profile.scratchValidate=&validate;
        const auto reload=[this,&domain,&profile,&execution](RuntimeObject* object,RuntimeObject* cls) {
            auto& values=MutateNativeInputValues(object);
            const std::function<void(RuntimeObject*,std::size_t,RuntimeObject*,const Value&)> store=
                [this,object,&execution](RuntimeObject* field,std::size_t index,RuntimeObject* member,const Value& value) {
                    WriteNativeInputCell(object,field,index,member,value,execution);
                };
            struct RestoreStore {RuntimeNativeInputConfig& profile;~RestoreStore(){profile.store=nullptr;}} restoreStore{profile};
            profile.store=&store;
            LoadInputDefaultConfig(domain,profile,cls,values,0u,&domain.config->document);
        };
        // These are TWO independent indexed passes with a refreshed bound,
        // NOT viewport order, a base-class reset or copying CDOs to instances.
        for(std::uint32_t index=1u;index<=RuntimeObjectIndices().AllocatedSlots();++index) {
            LifecycleStep();const auto* record=persistentObjectLookup->index.RecordAt(index);if(!record)continue;
            auto* cls=ActualLookupObject(*persistentObjectLookup,record->identity);
            if(cls->defaultOwner||!ActualLookupClass(*persistentObjectLookup,cls)||
                !IsDerivedFromPath(cls,selected->reflection.objectPath))continue;
            if((cls!=domain.inputClass&&cls!=domain.inputExtClass)||!InputSchemaDefaultValid(cls)||!cls->defaultObject)
                throw std::runtime_error("Input ResetConfig encountered an unsupported actual descendant Class");
            reload(cls->defaultObject,cls);++observed.defaultReloads;
        }
        for(std::uint32_t index=1u;index<=RuntimeObjectIndices().AllocatedSlots();++index) {
            LifecycleStep();const auto* record=persistentObjectLookup->index.RecordAt(index);if(!record)continue;
            auto* object=ActualLookupObject(*persistentObjectLookup,record->identity);
            if(object->defaultOwner||!object->cls||!IsDerivedFromPath(object->cls,selected->reflection.objectPath))continue;
            if(!ActualNativeInputInstanceValid(object))
                throw std::runtime_error("Input ResetConfig encountered an unsupported matching live instance");
            reload(object,object->cls);InputResetPostEditChange(object);++observed.instanceReloads;
        }
        observed.configPasses=profile.passes;observed.configImports=profile.imports;
        observed.importFailures=profile.failures;observed.importWarnings=profile.warnings;
        observed.cacheDirty=domain.config->cacheDirty;observed.completed=true;
    }
    Evaluation PlayerPawnResetKeyboardNative(RuntimeObject* pawn,const std::vector<Evaluation>& arguments,
        const Function* declaration,QuestVr::Vm::Execution& execution) {
        QuestVr::OriginalResetKeyboardQueries query;PortableInputResetResult observed;
        QuestVr::NativeScratchScope frame(nativeQueryScratchBytes_,2u*sizeof(query)+sizeof(observed)+4096u,
            [this](std::size_t bytes){ValidateStateBudget(bytes);});
        const auto authored=FunctionFor(Object("Engine.PlayerPawn.ResetKeyboard"));
        const auto check=[this](const Function& function) {
            auto* node=Object(function.path);
            if(function.path!="Engine.PlayerPawn.ResetKeyboard"||function.nativeIndex!=544u||function.flags!=0x401u||
                !function.variables.empty()||!function.bytecode.empty()||!node->script||
                node->outer!=Object("Engine.PlayerPawn")||function.source!=node->sourcePath||
                node->script->nativeIndex!=544u||node->script->functionFlags!=0x401u||node->script->children||
                node->script->baseField||node->script->logicalSize||!node->script->bytecode.empty()||!node->script->rawBytes.empty()||
                LowerAscii(Qualified(Table(node->sourcePath),Table(node->sourcePath).exports.at(node->exportIndex).ObjClass))!="core.function")
                throw std::runtime_error("ResetKeyboard is not its original native544/no-parameter declaration");
        };
        check(*authored);
        // Compiled indexed-native bytecode carries no Function argument. The
        // original metadata above is still mandatory. A FinalFunction/native
        // declaration, when supplied, must identify that same original source.
        if(declaration) {
            check(*declaration);
            if(declaration->source!=authored->source)
                throw std::runtime_error("ResetKeyboard changed its actual original declaration source");
        }
        RequireMapActor(pawn);
        if(!IsDerivedFromPath(pawn->cls,"Engine.PlayerPawn")||!arguments.empty())
            throw std::runtime_error("ResetKeyboard requires an actual PlayerPawn and no arguments");
        query.requireOuterRoot=[this]{if(!transaction_)throw std::runtime_error("ResetKeyboard lost its existing root");};
        query.work=[this]{LifecycleStep();};
        query.playerPawnPlayer=[this,pawn](std::uintptr_t identity) {
            if(identity!=reinterpret_cast<std::uintptr_t>(pawn))throw std::runtime_error("ResetKeyboard changed its PlayerPawn receiver");
            const auto property=OriginalAnimationField(pawn,"Engine.PlayerPawn","Player",Kind::Object,2u,"ObjectProperty");
            auto* field=Object(property.key);
            if(property.zero.kind!=Kind::Object||property.arrayDimension!=1u||!(property.flags&2u)||
                !OwnsProperty(pawn->cls,property)||field->outer!=Object("Engine.PlayerPawn")||!field->property||
                field->property->type!="ObjectProperty"||!field->property->referencedType||
                LowerAscii(Qualified(Table(field->sourcePath),field->property->referencedType))!="engine.player")
                throw std::runtime_error("ResetKeyboard Player field lost its actual Player constraint");
            if(pawn->nativePlayerBinding&&pawn->nativePlayer&&(!persistentNativeClient.HasClient()||
                pawn->nativePlayer->nativeClientIdentity!=persistentNativeClient.ClientIdentity()))
                throw std::runtime_error("ResetKeyboard actual Player lease is revoked or stale");
            const auto value=OriginalAnimationRead(pawn,property,Kind::Object);
            return reinterpret_cast<std::uintptr_t>(value.text.empty()?nullptr:Object(value.text));
        };
        query.isViewport=[this](std::uintptr_t identity) {
            auto* player=reinterpret_cast<RuntimeObject*>(identity);
            return player&&Object(player->reflection.objectPath)==player&&IsDerivedFromPath(player->cls,"Engine.Viewport");
        };
        query.viewportInput=[this](std::uintptr_t identity) {
            auto* viewport=reinterpret_cast<RuntimeObject*>(identity);
            if(!viewport||!viewport->nativeViewportIdentity||NativeSessionViewport(viewport->nativeViewportIdentity)!=viewport)
                throw std::runtime_error("ResetKeyboard Viewport lacks its actual session lease");
            clientDependent_=true;
            return reinterpret_cast<std::uintptr_t>(viewport->nativeViewportInput);
        };
        query.inputClass=[](std::uintptr_t identity) {
            auto* input=reinterpret_cast<RuntimeObject*>(identity);
            if(!ActualNativeInputInstanceValid(input))
                throw std::runtime_error("ResetKeyboard Input lacks its actual constructor/Class authority");
            return reinterpret_cast<std::uintptr_t>(input->cls);
        };
        query.resetConfig=[this,&execution,&observed](std::uintptr_t identity) {
            ResetInputClassConfig(reinterpret_cast<RuntimeObject*>(identity),execution,observed);
        };
        static_cast<void>(QuestVr::RunOriginalResetKeyboard(reinterpret_cast<std::uintptr_t>(pawn),query));
        return {};
    }
