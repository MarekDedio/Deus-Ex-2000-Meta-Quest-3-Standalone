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
    bool ConsoleCommandPrefix(RuntimeObject* console,const std::string& command,RuntimeObject* executor) {
        clientDependent_=true;
        RequireNativeViewportConsole(console);
        if(executor&&(!executor->cls||!IsDerivedFromPath(executor->cls,"Engine.Actor")))
            throw std::runtime_error("Console command executor lacks actual Actor identity");
        if(command.size()>QuestVr::NativeInputKeyQueries::MaximumCommandUnits*3u)
            throw std::runtime_error("Console command transport exceeds its bounded FString profile");
        QuestVr::NativeScratchScope scratch(nativeQueryScratchBytes_,8u*(command.size()+1u)+4096u,
            [this](std::size_t bytes){ValidateStateBudget(bytes);});
        const auto text=QuestVr::UnrealString::Decode(command);
        ScriptConsoleOperations operations{*this,console,executor};
        return QuestVr::ProcessConsoleExec::Exec(operations,text);
    }
    struct ViewportCommandOperations {
        PortableActorVmHost& host;
        void Step(){host.LifecycleStep();}
        [[noreturn]] void Match(std::u16string_view command,std::size_t) {
            throw std::runtime_error("Original viewport native command service is not implemented: "+
                QuestVr::UnrealString::Encode(command));
        }
    };
    std::string ViewportExecAfterInput(RuntimeObject* viewport,const std::string& command) {
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
        throw std::runtime_error("Original UPlayer FExec routing is not implemented: "+forwarded);
    }
