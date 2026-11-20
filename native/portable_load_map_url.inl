// FURL assignment and User-cache updates join the actual LoadMap root. No
// additional Flush is requested by the original SaveURLConfig call sites.
    void ValidateNativeLoadMapUrlLease() {
        if(!beforeLoadMapUrl_)return;
        auto* engine=beforeLoadMapUrl_->engine.get();auto* level=beforeLoadMapUrl_->level.get();
        if(!transaction_||!persistentObjectLookup||!persistentObjectLookup->active||
            persistentObjectLookup->generation!=beforeLoadMapUrl_->lookupGeneration||
            !level||level!=CurrentClockLevel()||level->nativeMapGeneration!=beforeLoadMapUrl_->mapGeneration||
            !engine||level->nativeLevelEngine!=engine||!ActualNativeCollisionObjectValid(engine)||
            !engine->nativeEngineInstance||engine->nativeEngineInstance->currentLevel!=level||
            Object(engine->reflection.objectPath)!=engine)
            throw std::runtime_error("Original LoadMap URL lost its actual Engine/current Level lease");
    }
    Value FinishOriginalLoadMapUrl(const QuestVr::OriginalLoadMapUrl& incoming,
        PortableWorldStartupProbeResult::LoadMapContinuationObservation& observed) {
        using QuestVr::DecalStateDetail::Add;
        using QuestVr::DecalStateDetail::Multiply;
        auto* level=CurrentClockLevel();auto* engine=level->nativeLevelEngine;
        if(!transaction_||beforeLoadMapUrl_||!engine||!ActualNativeCollisionObjectValid(engine)||
            !engine->nativeEngineInstance||engine->nativeEngineInstance->currentLevel!=level||
            !persistentObjectLookup||!persistentObjectLookup->active)
            throw std::runtime_error("Original LoadMap URL requires its actual Engine/current Level root");
        const auto role=ReadPortableRuntimeLoadProfile();
        if(!role)throw std::runtime_error("Original LoadMap URL tail lacks explicit process GIsClient authority");
        // Copy only FURL. Engine Client, Audio and reserved Level fields retain
        // separate ownership. The old FURL remains live through the root.
        constexpr auto journalBytes=sizeof(LoadMapUrlJournal)+2u*sizeof(GCRootNode);
        ValidateStateBudget(Add(journalBytes,NativeLoadMapUrlPayloadBytes(incoming)));
        auto journal=std::make_unique<LoadMapUrlJournal>(engine,level,persistentObjectLookup->generation);
        journal->url=incoming;
        ValidateStateBudget(Add(journalBytes,NativeLoadMapUrlPayloadBytes(journal->url)));
        using std::swap;
        swap(engine->nativeEngineInstance->values.url,journal->url);
        beforeLoadMapUrl_=std::move(journal);
        ValidateNativeLoadMapUrlLease();
        observed.urlCopied=engine->nativeEngineInstance->values.url==incoming;
        if(!observed.urlCopied)throw std::runtime_error("Original LoadMap FURL copy changed the supplied URL");
        if(role->client) {
            const QuestVr::NativeConfig::Work work{
                [this](std::size_t units){nativeWork_.Step(units);},
                [this](std::size_t bytes){ValidateStateBudget(bytes);}};
            observed.urlConfigMatches=QuestVr::VisitOriginalLoadMapUserOptions(incoming.options,work.step,
                [&](std::u16string_view key,std::u16string_view value) {
                    if(!persistentInputSchema||!persistentInputSchema->get()->config||!persistentUserConfigStorage)
                        throw std::runtime_error("Original FURL.SaveURLConfig lacks its owned User cache");
                    const auto& source=*persistentInputSchema->get()->config;
                    const auto& storage=*persistentUserConfigStorage;
                    const auto& currentPath=storage.owned?storage.targetPath:storage.seedPath;
                    const bool alreadyStaged=nativeConfigFlushPending_&&nativeConfigExpected_&&source.sourcePath==storage.targetPath;
                    if(source.seedPath.empty()||source.seedPath!=storage.seedPath||
                        (source.sourcePath!=currentPath&&!alreadyStaged))
                        throw std::runtime_error("Original FURL.SaveURLConfig cache is not bound to its User source");
                    QuestVr::NativeScratchScope text(nativeQueryScratchBytes_,Add(2u*sizeof(std::string),
                        Multiply(Add(Add(key.size(),value.size()),2u),6u)),[this](std::size_t bytes){ValidateStateBudget(bytes);});
                    const auto encodedKey=QuestVr::UnrealString::Encode(key),encodedValue=QuestVr::UnrealString::Encode(value);
                    auto& config=MutateNativeInputConfig();
                    if(auto changed=config.document.WithString("DefaultPlayer",encodedKey,encodedValue,work)) {
                        if(changed->Scalar("DefaultPlayer",encodedKey)!=std::optional<std::string_view>{encodedValue})
                            throw std::runtime_error("Original FURL.SaveURLConfig staged a different User value");
                        config.document=std::move(*changed);config.cacheDirty=true;
                        ++observed.urlConfigChanges;
                    }
                    observed.urlConfigCacheDirty=config.cacheDirty;
                    ValidateStateBudget();
                });
        }
        ValidateNativeLoadMapUrlLease();
        observed.urlReturnedLevel=true;
        return Value::Text(Kind::Object,level->reflection.objectPath);
    }
