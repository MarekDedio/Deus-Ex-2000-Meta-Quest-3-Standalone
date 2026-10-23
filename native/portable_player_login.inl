// Included inside PortableActorVmHost. The selected original standalone
// LoadMap continuation calls real GameInfo.Login in its EXISTING startup root.
// This is not a public nested Execute, synthetic Pawn or accepted travel tail.
    std::pair<std::shared_ptr<Value>,std::shared_ptr<Reference>> StartupOutString() {
        if(!transaction_ || nativeStartupOut_.size()>=QuestVr::NativeClientRegistry::MaximumViewports+1u)
            throw std::runtime_error("Startup OUT storage requires a bounded existing root");
        if(nativeStartupOut_.empty()) {
            ValidateStateBudget((QuestVr::NativeClientRegistry::MaximumViewports+1u)*sizeof(StartupOutStorage));
            nativeStartupOut_.reserve(QuestVr::NativeClientRegistry::MaximumViewports+1u);
        }
        ValidateStateBudget(sizeof(Value)+sizeof(Reference)+8u*sizeof(void*)+32u);
        auto error=std::make_shared<Value>(Value::Text(Kind::String,{}));
        auto out=std::make_shared<Reference>();out->zero=*error;
        // Only owned data is captured. These callbacks survive native-stack
        // unwinding and remain safe during outer VM reference rollback.
        out->read=[error]{return *error;};
        out->write=[this,error](const Value& value) {
            if(value.kind!=Kind::String || !value.fields.empty() || !value.intArray.empty())
                throw std::runtime_error("Startup OUT Error requires original scalar String storage");
            if(value.text.size()<=error->text.capacity()) {
                // Never shrink the allocation: every prior journal value fits
                // during undo, which must not perform admission/work checks.
                error->text.assign(value.text);return;
            }
            ValidateStateBudget(value.text.size()+1u);std::string staged(value.text);
            ValidateStateBudget(staged.capacity()+1u);error->text.swap(staged);
        };
        nativeStartupOut_.push_back({error,out});ValidateStateBudget();return {error,out};
    }
    void StorePlayerLoginObservation(std::string& destination,std::string_view source) {
        // Each row's initial capacities are retained at creation. A staged
        // replacement admits the old+new peak without mistaking old inline
        // capacity for an existing allocation that can fund the first spill.
        ValidateStateBudget(source.size()+1u);std::string staged(source);
        ValidateStateBudget(staged.capacity()+1u);const auto before=destination.capacity();
        destination.swap(staged);
        if(destination.capacity()>=before)nativeActorOrderObservationBytes_+=destination.capacity()-before;
        else nativeActorOrderObservationBytes_-=before-destination.capacity();
    }
    std::shared_ptr<const Function> OriginalPlayerLoginDeclaration(RuntimeObject* game) {
        RequireMapActor(game);
        if(!IsDerivedFromPath(game->cls,"Engine.GameInfo"))
            throw std::runtime_error("SpawnPlayActor Login receiver is not actual GameInfo");
        const auto identity=Member(game,"Login");
        auto declaration=FunctionFor(Object(identity->path));
        auto* metadata=Object(declaration->path);
        if(!metadata->outer || !metadata->outer->classDescriptor ||
            !IsDerivedFromPath(metadata->outer,"Engine.GameInfo") ||
            declaration->nativeIndex!=0u || declaration->flags!=0x802u || declaration->variables.size()<5u)
            throw std::runtime_error("SpawnPlayActor Login has no authenticated original event declaration");
        const std::array<const char*,5u> names{"portal","options","error","spawnclass","returnvalue"};
        const std::array<Kind,5u> kinds{Kind::String,Kind::String,Kind::String,Kind::Object,Kind::Object};
        const std::array<std::uint32_t,5u> flags{0x400080u,0x400080u,0x400180u,0x80u,0x580u};
        for(std::size_t i=0u;i<names.size();++i) {
            LifecycleStep();const auto& field=declaration->variables[i];auto* property=Object(field.key);
            if(LowerAscii(field.key)!=LowerAscii(declaration->path)+'.'+names[i] ||
                field.zero.kind!=kinds[i] || field.arrayDimension!=1u || field.flags!=flags[i] ||
                property->outer!=metadata || !property->property || property->property->type!=
                    (i<3u?"StrProperty":i==3u?"ClassProperty":"ObjectProperty"))
                throw std::runtime_error("SpawnPlayActor Login parameter is not its original declared field");
            if(i>=3u) {
                const auto& table=Table(property->sourcePath);
                if(LowerAscii(Qualified(table,property->property->referencedType))!=
                    (i==3u?"core.class":"engine.playerpawn") ||
                    (i==3u && LowerAscii(Qualified(table,property->property->secondaryType))!="engine.playerpawn"))
                    throw std::runtime_error("SpawnPlayActor Login class/result constraints are not PlayerPawn");
            }
        }
        return declaration;
    }
    RuntimeObject* LoadStartupPlayerClass(const PortablePlayerLoginSource& source,std::u16string_view token,
        RuntimeObject* base,std::uintptr_t sandbox) {
        if(base!=Object("Engine.PlayerPawn") || sandbox)
            throw std::runtime_error("SpawnPlayActor class loader lacks actual standalone PlayerPawn/None sandbox authority");
        const std::optional<std::u16string>* setting{};
        if(QuestVr::UnrealString::EqualInsensitive(token,u"usr:DefaultPlayer.Class"))setting=&source.userDefaultPlayerClass;
        else if(QuestVr::UnrealString::EqualInsensitive(token,u"ini:URL.Class"))setting=&source.iniUrlClass;
        if(setting) {
            if(!*setting)return nullptr;
            token=QuestVr::UnrealString::CString(**setting);
        }
        if(token.empty())return nullptr;
        ValidateStateBudget((token.size()+1u)*3u);
        const auto path=QuestVr::UnrealString::Encode(token);
        const auto found=persistentVmObjects.find(LowerAscii(path));
        if(found==persistentVmObjects.end())
            // An unmaterialized class/package is not evidence of a failed
            // original StaticLoadClass. Never fall through on unknown service.
            throw std::runtime_error("SpawnPlayActor StaticLoadClass of an unmaterialized class/package is not implemented: "+path);
        auto* cls=found->second;
        if(!cls || cls->reflection.metaClass!="Class")return nullptr;
        static_cast<void>(CastClassPath(cls->reflection.objectPath));
        if(!IsDerivedFromPath(cls,base->reflection.objectPath))return nullptr;
        return cls;
    }
    void StartupPlayerLogin(std::uint64_t viewportIdentity,const PortablePlayerLoginSource& source,
        PortablePlayerLoginObservation& observation,const std::vector<PortableStartupLandingCommand>& landingCommands,
        const std::vector<PortableStartupFallingCommand>& fallingCommands,
        const std::vector<PortableStartupDamageCommand>& damageCommands,
        bool collectDamageLoot,bool collectDamageLootViaPlayer,QuestVr::Vm::Execution& execution) {
        if(!transaction_ || !worldStartupProbe_)
            throw std::runtime_error("SpawnPlayActor continuation requires its actual startup root");
        auto* level=CurrentClockLevel();auto* viewport=NativeSessionViewport(viewportIdentity);
        if(!level->nativeStandaloneLevelServices)
            throw std::runtime_error("SpawnPlayActor native IsServer driver state is not established");
        clientDependent_=true;
        const auto previous=nativeQueryScratchBytes_;
        struct RestoreScratch {std::size_t& value;std::size_t before;~RestoreScratch(){value=before;}} scratch{nativeQueryScratchBytes_,previous};
        if(source.urlOptions.size()>1024u || source.portal.size()>4096u)
            throw std::runtime_error("SpawnPlayActor URL source exceeds portable input bounds");
        for(const auto* alias:{&source.userDefaultPlayerClass,&source.iniUrlClass})
            if(*alias && (**alias).size()>4096u)throw std::runtime_error("SpawnPlayActor config class source exceeds portable bounds");
        ValidateStateBudget(sizeof(std::vector<std::u16string_view>)+source.urlOptions.size()*sizeof(std::u16string_view));
        std::vector<std::u16string_view> options;options.reserve(source.urlOptions.size());
        const auto optionBytes=sizeof(options)+options.capacity()*sizeof(std::u16string_view);
        ValidateStateBudget(optionBytes);nativeQueryScratchBytes_+=optionBytes;
        for(const auto& option:source.urlOptions) {
            LifecycleStep();if(option.size()>4096u)throw std::runtime_error("SpawnPlayActor URL option exceeds portable input bound");
            options.push_back(option);
        }
        const auto [error,out]=StartupOutString();
        struct Context {
            PortableActorVmHost* host;RuntimeObject* level;RuntimeObject* viewport;
            const PortablePlayerLoginSource* source;QuestVr::Vm::Execution* execution;
            PortablePlayerLoginObservation* observation;std::shared_ptr<Value> error;std::shared_ptr<Reference> out;
        } context{this,level,viewport,&source,&execution,&observation,error,out};
        QuestVr::OriginalSpawnPlayActorLoginQueries q;
        ValidateStateBudget(sizeof(context)+sizeof(q));nativeQueryScratchBytes_+=sizeof(context)+sizeof(q);
        q.requireOuterRoot=[&context] {
            if(!context.host->transaction_)throw std::runtime_error("SpawnPlayActor lost its caller transaction");
        };
        q.work=[this]{LifecycleStep();};
        q.admitTemporaryBytes=[this](std::size_t bytes){ValidateStateBudget(bytes);};
        q.assignOutError=[&context](std::u16string_view text) {
            context.host->ValidateStateBudget((text.size()+1u)*3u);
            context.execution->WriteReference(context.out,Value::Text(Kind::String,QuestVr::UnrealString::Encode(text)));
        };
        q.playerIsNetConnection=[&context](std::uintptr_t identity) {
            if(identity!=reinterpret_cast<std::uintptr_t>(context.viewport) ||
                NativeSessionViewport(context.observation->viewportIdentity)!=context.viewport ||
                context.host->CastClassPath(context.viewport->cls->reflection.objectPath)!="Engine.Viewport" ||
                context.viewport->cls->base!=context.host->Object("Engine.Player"))
                throw std::runtime_error("SpawnPlayActor UPlayer is not the actual prepared native Viewport");
            // Original10590C40 is UNetConnection, NOT UViewport10581068.
            // Our actual native Viewport->Player hierarchy fails that class
            // predicate. The original local sandbox therefore stays None.
            return false;
        };
        q.netConnectionSandbox=[](std::uintptr_t)->std::uintptr_t {
            throw std::runtime_error("SpawnPlayActor NetConnection sandbox is not implemented");
        };
        q.playerPawnClass=[this]{return reinterpret_cast<std::uintptr_t>(Object("Engine.PlayerPawn"));};
        q.optionalUrlClass=[&q](const auto& url){return QuestVr::OriginalSpawnPlayActorUrlClass(url,q);};
        q.loadPlayerClass=[&context](std::uintptr_t base,std::u16string_view token,std::uintptr_t sandbox) {
            auto* cls=context.host->LoadStartupPlayerClass(*context.source,token,reinterpret_cast<RuntimeObject*>(base),sandbox);
            if(cls)context.host->StorePlayerLoginObservation(context.observation->selectedClass,cls->reflection.objectPath);
            return reinterpret_cast<std::uintptr_t>(cls);
        };
        q.fatalLoadPlayerClass=[] {throw std::runtime_error("Original SpawnPlayActor fatal LoadPlayerClass: no valid PlayerPawn class");};
        q.slotZero=[&context](std::uintptr_t identity) {
            if(identity!=reinterpret_cast<std::uintptr_t>(context.level) || context.host->CurrentClockLevel()!=context.level)
                throw std::runtime_error("SpawnPlayActor ULevel identity changed");
            return reinterpret_cast<std::uintptr_t>(persistentWorldActors.empty()?nullptr:persistentWorldActors.front());
        };
        q.isLevelInfo=[this](std::uintptr_t identity) {
            auto* actor=reinterpret_cast<RuntimeObject*>(identity);
            return actor && Object(actor->reflection.objectPath)==actor && IsDerivedFromPath(actor->cls,"Engine.LevelInfo");
        };
        q.levelInfoGame=[this](std::uintptr_t identity) {
            auto* info=reinterpret_cast<RuntimeObject*>(identity);
            const auto property=DescribeProperty("Engine.LevelInfo.Game");auto* metadata=Object(property.key);
            if(property.arrayDimension!=1u || property.zero.kind!=Kind::Object || !OwnsProperty(info->cls,property) ||
                metadata->outer!=Object("Engine.LevelInfo") || !metadata->property ||
                metadata->property->type!="ObjectProperty" ||
                LowerAscii(Qualified(Table(metadata->sourcePath),metadata->property->referencedType))!="engine.gameinfo")
                throw std::runtime_error("SpawnPlayActor Game field lacks original LevelInfo authority");
            const auto value=Read(info,property,0u);
            if(value.kind!=Kind::Object)throw std::runtime_error("SpawnPlayActor Game field is not an Object");
            return reinterpret_cast<std::uintptr_t>(value.text.empty()?nullptr:Object(value.text));
        };
        q.logError=[](std::u16string_view message) {
            __android_log_print(ANDROID_LOG_WARN,"quest_main","SpawnPlayActor: %s",QuestVr::UnrealString::Encode(message).c_str());
        };
        q.invokeLogin=[&context](std::uintptr_t game,std::u16string_view portal,std::u16string_view options,std::uintptr_t cls) {
            auto* actor=reinterpret_cast<RuntimeObject*>(game);auto* spawnClass=reinterpret_cast<RuntimeObject*>(cls);
            const auto declaration=context.host->OriginalPlayerLoginDeclaration(actor);
            const auto before=context.host->nativeQueryScratchBytes_;
            RestoreScratch frame{context.host->nativeQueryScratchBytes_,before};
            // Initializer-list and owned vector arguments coexist through the
            // call; retain both, encoded payloads and class/OUT copies.
            const auto frameBytes=8u*sizeof(Evaluation)+(portal.size()+options.size()+2u)*6u+
                2u*(spawnClass->reflection.objectPath.size()+context.error->text.size()+2u);
            context.host->ValidateStateBudget(frameBytes);context.host->nativeQueryScratchBytes_+=frameBytes;
            context.host->StorePlayerLoginObservation(context.observation->gamePath,actor->reflection.objectPath);
            context.host->StorePlayerLoginObservation(context.observation->functionPath,declaration->path);
            context.observation->loginEntered=true;
            const auto returned=context.execution->CallFunction(*declaration,actor->reflection.objectPath,{
                {Value::Text(Kind::String,QuestVr::UnrealString::Encode(portal)),{}},
                {Value::Text(Kind::String,QuestVr::UnrealString::Encode(options)),{}},
                {*context.error,context.out},{Value::Text(Kind::Object,spawnClass->reflection.objectPath),{}}});
            if(returned.kind!=Kind::Object)throw std::runtime_error("Original GameInfo.Login returned non-PlayerPawn storage");
            auto* player=returned.text.empty()?nullptr:context.host->Object(returned.text);
            if(player && !IsDerivedFromPath(player->cls,"Engine.PlayerPawn"))
                throw std::runtime_error("Original GameInfo.Login result is not an actual PlayerPawn");
            context.host->StorePlayerLoginObservation(context.observation->playerPath,returned.text);
            context.observation->loginReturned=true;
            return reinterpret_cast<std::uintptr_t>(player);
        };
        observation.attempted=true;
        const auto retainObservation=[&] {
            if(error->kind!=Kind::String)throw std::runtime_error("SpawnPlayActor OUT Error lost original String storage");
            StorePlayerLoginObservation(observation.error,error->text);
        };
        try {
            const auto result=QuestVr::RunOriginalSpawnPlayActorLoginPrefix(reinterpret_cast<std::uintptr_t>(level),
                reinterpret_cast<std::uintptr_t>(viewport),{source.portal,options},q,nativeQueryScratchBytes_);
            if(result.player) {
                auto* player=reinterpret_cast<RuntimeObject*>(result.player);
                ProbeStartupLandings(player,landingCommands,execution);
                ProbeStartupFalling(player,fallingCommands,false,execution);
                ProbeStartupDamage(player,damageCommands,execution);
                ProbeStartupFalling(player,fallingCommands,true,execution);
                if(collectDamageLoot)ProbeStartupLoot(player,collectDamageLootViaPlayer,execution);
                static_cast<void>(SetSessionPlayer(viewportIdentity,player->reflection.objectPath,execution));
                observation.paired=true;
                throw std::runtime_error("Original SpawnPlayActor post-Possess role/travel continuation is not implemented");
            }
        }catch(...){retainObservation();throw;}
        retainObservation();
    }
