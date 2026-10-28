// PortableActorVmHost: genuine pre-window native viewport / Console prefix.
    struct ScriptConsoleOperations {
        struct Name {
            int index{};std::u16string_view spelling;
            bool operator==(const Name& other) const{return index==other.index;}
        };
        PortableActorVmHost& host;
        RuntimeObject* receiver;
        RuntimeObject* executor;
        void Step(){host.LifecycleStep();}
        void Admit(std::size_t bytes){host.ValidateStateBudget(bytes);}
        bool IsScriptable(){return host.OriginalGameScriptsEnabled();}
        Name FindName(std::u16string_view text) {
            return {NameString::FindCompareIndex(QuestVr::UnrealString::Encode(text)).value_or(0),text};
        }
        RuntimeObject* FindFunction(Name name,bool global) {
            if(global)throw std::runtime_error("Console command lookup requires original Global0");
            const auto spelling=QuestVr::UnrealString::Encode(name.spelling);
            if(NameString::FindCompareIndex(spelling)!=std::optional<int>{name.index})
                throw std::runtime_error("Console lookup lost its actual non-interned FName identity");
            const auto context=CurrentDispatchContext(receiver);
            const auto selected=context.exactSelected?
                QuestVr::ScriptDispatch::ResolveFunctionAtSelectedNode(DispatchGraph(),
                    receiver->cls->reflection.objectPath,context.selectedPath,spelling):
                QuestVr::ScriptDispatch::ResolveFunction(DispatchGraph(),
                    receiver->cls->reflection.objectPath,context.stateName,spelling);
            return selected?host.Object(*selected):nullptr;
        }
        bool IsFunction(RuntimeObject* function) {
            if(function&&function->script&&!function->cls)
                throw std::runtime_error("ScriptConsoleExec requires the actual registered UFunction class binding");
            return function&&function->cls&&IsDerivedFromPath(function->cls,"Core.Function")&&function->script;
        }
        std::uint32_t FunctionFlags(RuntimeObject* function){return function->script->functionFlags;}
        void Match(RuntimeObject* function,std::size_t) {
            static_cast<void>(executor);
            throw std::runtime_error("Original ScriptConsoleExec command parameter/call service is not implemented: "+
                function->reflection.objectPath);
        }
    };
    bool ScriptConsoleCommandPrefix(RuntimeObject* receiver,const std::string& command,RuntimeObject* executor) {
        clientDependent_=true;
        if(receiver&&receiver->nativeConsoleObject)RequireNativeViewportConsole(receiver);
        else {
            RequireMapActor(receiver);
            // Independent +48 audit of all56 native Actor vtable exports
            // resolves to Core.ScriptConsoleExec. Reuse only the validated
            // nearest-native ancestry selector, not ToWorld's +88 authority.
            static_cast<void>(NativeToWorldClass(receiver));
        }
        if(executor&&(!executor->cls||!IsDerivedFromPath(executor->cls,"Engine.Actor")))
            throw std::runtime_error("Console command executor lacks actual Actor identity");
        if(command.size()>QuestVr::NativeInputKeyQueries::MaximumCommandUnits*3u)
            throw std::runtime_error("Console command transport exceeds its bounded FString profile");
        QuestVr::NativeScratchScope scratch(nativeQueryScratchBytes_,8u*(command.size()+1u)+4096u,
            [this](std::size_t bytes){ValidateStateBudget(bytes);});
        const auto text=QuestVr::UnrealString::Decode(command);
        ScriptConsoleOperations operations{*this,receiver,executor};
        return QuestVr::ProcessConsoleExec::Exec(operations,text);
    }
    bool ConsoleCommandPrefix(RuntimeObject* console,const std::string& command,RuntimeObject* executor) {
        RequireNativeViewportConsole(console);return ScriptConsoleCommandPrefix(console,command,executor);
    }
    RuntimeObject* CommandObjectField(RuntimeObject* owner,const char* declaration,const char* constraint) {
        RequireMapActor(owner);const auto property=DescribeProperty(declaration);auto* field=Object(property.key);
        if(property.zero.kind!=Kind::Object||property.arrayDimension!=1u||!OwnsProperty(owner->cls,property)||
            !field->property||field->property->type!="ObjectProperty"||
            LowerAscii(Qualified(Table(field->sourcePath),field->property->referencedType))!=LowerAscii(constraint))
            throw std::runtime_error("Command provider field lacks its actual ObjectProperty/Class authority: "+std::string(declaration));
        const auto value=Read(owner,property,0u);
        if(value.kind!=Kind::Object)throw std::runtime_error("Command provider field is not an actual Object");
        if(value.text.empty())return nullptr;
        auto* target=Object(value.text);RequireMapActor(target);
        if(!IsDerivedFromPath(target->cls,constraint))throw std::runtime_error("Command provider field violates its actual Class constraint");
        return target;
    }
    void RequireCommandLevel(RuntimeObject* level) {
        if(!level||!persistentEngineCollision||!persistentEngineCollision->get()||persistentWorldActors.empty()||
            !persistentWorldActors.front()||persistentWorldActors.front()->nativeXLevel!=level||
            level->sourcePath!=persistentMapSourcePath||Object(level->reflection.objectPath)!=level)
            throw std::runtime_error("Native command Level is not the current actual linked world");
        static_cast<void>(NativeLevelInfo(persistentWorldActors.front()));
        if(!level->nativeStandaloneLevelServices)
            throw std::runtime_error("Native Level command has no established network/demo provider profile");
    }
    struct NativeCommandOperations {
        PortableActorVmHost& host;
        const char* provider;
        void Step(){host.LifecycleStep();}
        [[noreturn]] void Match(std::u16string_view command,std::size_t) {
            throw std::runtime_error(std::string("Original ")+provider+" native command service is not implemented: "+
                QuestVr::UnrealString::Encode(command));
        }
    };
    bool LevelCommandExec(RuntimeObject* level,std::u16string_view command) {
        RequireCommandLevel(level);
        // This loader-owned standalone profile explicitly creates neither
        // NetDriver nor DemoRecDriver, the two preceding native offers.
        NativeCommandOperations operations{*this,"Level"};std::size_t cursor{};
        return QuestVr::EngineExecCommands::Exec(operations,command,cursor,QuestVr::EngineExecCommands::Level);
    }
    bool GameEngineCommandExec(RuntimeObject* engine,std::u16string_view command,QuestVr::Vm::Execution& execution) {
        static_cast<void>(execution);
        if(!engine||!persistentEngineCollision||engine!=persistentEngineCollision->get()->engine||
            !ActualNativeCollisionObjectValid(engine)||!engine->nativeEngineInstance||!engine->nativeEngineInstance->execInstalled)
            throw std::runtime_error("Command has no actual configured native Engine FExec provider");
        const auto* registration=engine->cls->nativeEngineClass;
        if(registration!=&QuestVr::NativeEngineClasses[2u]&&registration!=&QuestVr::NativeEngineClasses[3u]&&
            registration!=&QuestVr::NativeEngineClasses[4u])
            throw std::runtime_error("Native Engine command override is not the audited GameEngine chain");
        if(registration==&QuestVr::NativeEngineClasses[4u]) {
            NativeCommandOperations selected{*this,"DeusExGameEngine"};std::size_t local{};
            // DeusEx forwards its original argument, not the local pointer
            // advanced by the discriminator checks, to UGameEngine.Exec.
            if(QuestVr::EngineExecCommands::Exec(selected,command,local,QuestVr::EngineExecCommands::DeusExGameEngine))return true;
        }
        NativeCommandOperations base{*this,"GameEngine"};std::size_t cursor{};
        if(QuestVr::EngineExecCommands::Exec(base,command,cursor,QuestVr::EngineExecCommands::GameEngine))return true;
        const auto forwarded=command.substr(cursor);
        auto* current=engine->nativeEngineInstance->currentLevel;
        if(current&&LevelCommandExec(current,forwarded))return true;
        // Original GameEngine rereads currentLevel and its actual Actors[0]
        // after Level.Exec; the earlier UPlayer offers are NOT deduplicated.
        current=engine->nativeEngineInstance->currentLevel;
        if(current) {
            RequireCommandLevel(current);auto* info=persistentWorldActors.front();
            if(auto* game=CommandObjectField(info,"Engine.LevelInfo.Game","Engine.GameInfo"))
                if(ScriptConsoleCommandPrefix(game,QuestVr::UnrealString::Encode(forwarded),nullptr))return true;
        }
        // Fresh import resolution corrects the old GExec label: the provider
        // BEFORE Core.StaticExec is GSys (USystem.FExec), not later GExec.
        throw std::runtime_error("Original UEngine GSys command provider is not prepared: "+QuestVr::UnrealString::Encode(forwarded));
    }
    struct PlayerCommandOperations {
        using Handle=RuntimeObject*;
        PortableActorVmHost& host;RuntimeObject* viewport;QuestVr::Vm::Execution& execution;
        void Step(){host.LifecycleStep();}
        Handle Actor() {
            static_cast<void>(RequireQuestViewportPayload(viewport));
            auto* actor=viewport->nativeViewportActor;
            if(actor) {
                host.RequireMapActor(actor);
                if(!IsDerivedFromPath(actor->cls,"Engine.PlayerPawn")||!actor->nativePlayerBinding||actor->nativePlayer!=viewport)
                    throw std::runtime_error("UPlayer command lost its actual PlayerPawn/Player backedge");
            }
            return actor;
        }
        Handle XLevel(Handle actor) {host.RequireMapActor(actor);host.RequireCommandLevel(actor->nativeXLevel);return actor->nativeXLevel;}
        Handle LevelInfo(Handle actor){return host.CommandObjectField(actor,"Engine.Actor.Level","Engine.LevelInfo");}
        Handle Game(Handle info){return host.CommandObjectField(info,"Engine.LevelInfo.Game","Engine.GameInfo");}
        Handle Auxiliary(Handle actor){return host.CommandObjectField(actor,"Engine.PlayerPawn.myHUD","Engine.HUD");}
        Handle Inventory(Handle actor){return host.CommandObjectField(actor,"Engine.Actor.Inventory","Engine.Inventory");}
        Handle Engine(Handle level) {
            host.RequireCommandLevel(level);auto* engine=level->nativeLevelEngine;
            if(!engine||engine!=persistentEngineCollision->get()->engine)
                throw std::runtime_error("UPlayer command actual XLevel.Engine belongs to another native process");
            return engine;
        }
        bool ExecLevel(Handle level,std::u16string_view command){return host.LevelCommandExec(level,command);}
        bool ScriptConsoleExec(Handle receiver,std::u16string_view command,Handle executor) {
            return host.ScriptConsoleCommandPrefix(receiver,QuestVr::UnrealString::Encode(command),executor);
        }
        bool ExecEngine(Handle engine,std::u16string_view command){return host.GameEngineCommandExec(engine,command,execution);}
    };
    struct ViewportCommandOperations {
        PortableActorVmHost& host;
        void Step(){host.LifecycleStep();}
        [[noreturn]] void Match(std::u16string_view command,std::size_t) {
            throw std::runtime_error("Original viewport native command service is not implemented: "+
                QuestVr::UnrealString::Encode(command));
        }
    };
    std::string ViewportExecAfterInput(RuntimeObject* viewport,const std::string& command,QuestVr::Vm::Execution& execution) {
        clientDependent_=true;
        const auto& native=RequireQuestViewportPayload(viewport);
        if(native.renderDevice)
            throw std::runtime_error("Viewport has no implemented authenticated render-device Exec service");
        if(native.windowOpened)
            throw std::runtime_error("Opened Quest viewport lacks its required render-device provider");
        // A real pre-open native-null RenDev skips this offer. Missing payload
        // or a nonnull unsupported provider must never become that decline.
        QuestVr::NativeScratchScope scratch(nativeQueryScratchBytes_,8u*(command.size()+1u)+4096u,
            [this](std::size_t bytes){ValidateStateBudget(bytes);});
        const auto text=QuestVr::UnrealString::Decode(command);std::size_t cursor{};
        ViewportCommandOperations operations{*this};
        static_cast<void>(QuestVr::ViewportExecCommands::Exec(operations,text,cursor));
        const auto forwarded=QuestVr::UnrealString::Encode(text.substr(cursor));
        // Original viewport+34 may be null. A present Console must run its
        // actual ScriptConsoleExec, with Actor as executor, before UPlayer.
        if(viewport->nativeViewportConsole&&ConsoleCommandPrefix(viewport->nativeViewportConsole,
            forwarded,viewport->nativeViewportActor))return {};
        PlayerCommandOperations player{*this,viewport,execution};
        static_cast<void>(QuestVr::PlayerExecRoute::Exec(player,text.substr(cursor)));
        return {};
    }
