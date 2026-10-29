// Included inside PortableActorVmHost. Original Extension Window operations;
// no commercial native code is loaded or executed.
    struct NativeWindowNavigationOperations {
        PortableActorVmHost& host;
        void Visit(RuntimeObject* window) {
            host.LifecycleStep();host.RequireNativeWindow(window);
        }
        RuntimeObject* ReadLink(RuntimeObject* window,QuestVr::NativeWindowRelativeLink link) {
            const char* name{};
            switch(link) {
            case QuestVr::NativeWindowRelativeLink::FirstChild:name="firstChild";break;
            case QuestVr::NativeWindowRelativeLink::LastChild:name="lastChild";break;
            case QuestVr::NativeWindowRelativeLink::PrevSibling:name="prevSibling";break;
            case QuestVr::NativeWindowRelativeLink::NextSibling:name="nextSibling";break;
            default:throw std::runtime_error("Native Window navigation link is invalid");
            }
            const auto key="Extension.Window."+std::string(name);
            host.ValidateNativeWindowScalar(window,key.c_str(),Kind::Object,"ObjectProperty","Extension.Window",2u);
            return host.WindowLink(window,name);
        }
        bool LocalVisible(RuntimeObject* window) {
            host.ValidateNativeWindowScalar(window,"Extension.Window.bIsVisible",Kind::Bool,"BoolProperty","",2u);
            return host.NativeWindowBool(window,"bIsVisible");
        }
    };
    Evaluation WindowRelativeQueryNative(const std::uint16_t index,RuntimeObject* window,
        const std::vector<Evaluation>& arguments,const Function* supplied) {
        static_cast<void>(WindowNativeDeclaration(index,supplied));RequireNativeWindow(window);
        if(arguments.size()>1u)throw std::runtime_error("Window relative query accepts only its optional Bool input");
        bool visibleOnly=true;
        if(!arguments.empty()) {
            const auto& input=arguments.front();
            if(input.reference||input.disposition!=QuestVr::Vm::ResultDisposition::Written||
                (input.value.kind!=Kind::Bool&&input.value.kind!=Kind::Nothing))
                throw std::runtime_error("Window relative query requires a copied initialized Bool input");
            // A direct/declared optional call appends a detached Nothing for
            // its absent trailing input after seed capture. It is the same
            // original wrapper omission as an indexed call with no arguments.
            if(input.value.kind==Kind::Bool)visibleOnly=input.value.boolean;
        }
        using Query=QuestVr::NativeWindowRelativeQuery;
        const auto query=index==1455u?Query::TopChild:index==1456u?Query::BottomChild:
            index==1457u?Query::HigherSibling:Query::LowerSibling;
        NativeWindowNavigationOperations operations{*this};
        auto* selected=QuestVr::QueryNativeWindowRelative(window,query,visibleOnly,operations);
        return {Value::Text(Kind::Object,selected?selected->reflection.objectPath:std::string{}),{}};
    }
    Evaluation WindowSetClientObjectNative(RuntimeObject* window,const std::vector<Evaluation>& arguments,
        const Function* supplied,QuestVr::Vm::Execution& execution) {
        const auto authored=WindowNativeDeclaration(1487u,supplied);RequireNativeWindow(window);
        if(arguments.size()!=1u||arguments[0u].reference||
            arguments[0u].disposition!=QuestVr::Vm::ResultDisposition::Written||arguments[0u].value.kind!=Kind::Object)
            throw std::runtime_error("SetClientObject requires its single copied initialized Object expression");
        const auto value=TypedLiveValue(authored->variables[0u],arguments[0u].value);
        ValidateNativeWindowScalar(window,"Extension.Window.clientObject",Kind::Object,"ObjectProperty","Core.Object",2u);
        //10052618 is a direct pointer store. The existing typed native Window
        // property journal owns its GC edge and beforeimage; no callback/layout.
        NativeWindowWrite(window,"Extension.Window.clientObject",value,execution);return {};
    }
    Value ProbeNativeWindowNavigation(const std::string& outerPath,PortableWindowNavigationProbeMode mode,
        PortableWindowNavigationProbeResult& result,QuestVr::Vm::Execution& execution) {
        using Mode=PortableWindowNavigationProbeMode;
        switch(mode) {
        case Mode::RoundTrip:case Mode::LateFailure:case Mode::WrongReceiver:case Mode::WrongArgument:
        case Mode::MissingClientArgument:case Mode::CyclicHiddenChain:break;
        default:throw std::runtime_error("Unknown native Window navigation diagnostic mode");
        }
        auto* outer=Object(outerPath);RequireMapActor(outer);
        // Explicit isolated Window constructors/links, not a substitute player
        // or configured game HUD. No pawn birth, flag graph or begun-world edit.
        constexpr std::size_t observationBytes=64u<<10u;
        ValidateStateBudget(observationBytes);nativeActorOrderObservationBytes_+=observationBytes;
        QuestVr::AuthoredStructLimits observationLimits;observationLimits.retainedBytes=observationBytes;
        QuestVr::AuthoredStructDetail::Budget retained{observationLimits};retained.Retain(sizeof(result));
        auto* parent=ConstructWindowObject(DefaultClass(Object("Extension.Window")),outer,execution);
        InitializeChildNativeWindowStorage(parent,"extension.window");InitializeBaseWindow(parent,nullptr,execution);
        const auto init=Member(parent,"InitWindow");
        if(!init)throw std::runtime_error("Window navigation fixture lacks its original InitWindow");
        static_cast<void>(execution.CallFunction(*init,parent->reflection.objectPath,{}));
        std::array<RuntimeObject*,4u> windows{parent,nullptr,nullptr,nullptr};
        const auto newChild=NewChildWindowDeclaration(nullptr);
        for(std::size_t slot=1u;slot<windows.size();++slot) {
            const auto born=execution.CallFunction(*newChild,parent->reflection.objectPath,
                {{Value::Text(Kind::Object,"Extension.Window"),{}},{Value::Bool(false),{}}});
            if(born.kind!=Kind::Object||born.text.empty())throw std::runtime_error("Window navigation NewChild returned None");
            windows[slot]=Object(born.text);
        }
        const auto observe=[&](std::string& target,const std::string& value) {
            if(value.size()>4096u)throw std::runtime_error("Window navigation observation string exceeds its bound");
            retained.Retain(2u*(value.size()+1u));target=value;
        };
        for(std::size_t slot=0u;slot<windows.size();++slot)observe(result.windows[slot],windows[slot]->reflection.objectPath);
        const auto query=[&](RuntimeObject* receiver,std::uint16_t index,std::vector<Evaluation> arguments={}) {
            const auto method=WindowNativeDeclaration(index,nullptr);
            const auto selected=execution.CallFunction(*method,receiver->reflection.objectPath,arguments);
            if(selected.kind!=Kind::Object||result.queryCount>=result.selected.size())
                throw std::runtime_error("Window navigation query result/snapshot is invalid");
            observe(result.selected[result.queryCount++],selected.text);return selected.text;
        };
        if(mode==Mode::WrongReceiver)query(outer,1455u);
        if(mode==Mode::WrongArgument)query(parent,1455u,{{Value::Integer(1),{}}});
        const auto setter=WindowNativeDeclaration(1487u,nullptr);
        if(mode==Mode::MissingClientArgument)
            static_cast<void>(execution.CallFunction(*setter,parent->reflection.objectPath,{}));
        for(std::size_t slot=1u;slot<windows.size();++slot)
            NativeWindowWrite(windows[slot],"Extension.Window.bIsVisible",Value::Bool(slot!=2u),execution);
        // Parent stays hidden. Only candidate LOCAL visibility filters queries.
        for(const bool filtered:{true,false}) {
            const auto argument=std::vector<Evaluation>{{Value::Bool(filtered),{}}};
            query(parent,1455u,argument);query(parent,1456u,argument);
            query(windows[1u],1457u,argument);query(windows[3u],1458u,argument);
        }
        NativeWindowWrite(windows[3u],"Extension.Window.bIsVisible",Value::Bool(false),execution);
        query(parent,1455u); // Fresh live tail must now skip C and B.
        auto stale=std::make_shared<Reference>();stale->zero=Value::Bool(false);
        stale->read=[]()->Value {throw std::runtime_error("Window navigation read a stale NoWrite alias");};
        query(parent,1455u,{{Value::Bool(false),stale,QuestVr::Vm::ResultDisposition::NoWrite}});
        query(parent,1455u,{{Value::Bool(true),stale,QuestVr::Vm::ResultDisposition::ZeroFill,4u}});
        const auto setClient=[&](RuntimeObject* window,const std::string& path) {
            static_cast<void>(execution.CallFunction(*setter,window->reflection.objectPath,{{Value::Text(Kind::Object,path),{}}}));
        };
        setClient(windows[1u],outerPath);
        const auto roots=windows[1u]->nativeWindowPropertyRoots.find("extension.window.clientobject");
        result.clientRooted=roots!=windows[1u]->nativeWindowPropertyRoots.end()&&roots->second.count(0u)&&
            roots->second.at(0u).size()==1u&&roots->second.at(0u).front()==outer;
        observe(result.clients[0u],Read(windows[1u],DescribeProperty("Extension.Window.clientObject"),0u).text);
        setClient(windows[3u],parent->reflection.objectPath);
        observe(result.clients[1u],Read(windows[3u],DescribeProperty("Extension.Window.clientObject"),0u).text);
        if(mode==Mode::CyclicHiddenChain) {
            for(auto* window:windows)NativeWindowWrite(window,"Extension.Window.bIsVisible",Value::Bool(false),execution);
            WindowLinkWrite(windows[1u],"prevSibling",windows[2u],execution);
            WindowLinkWrite(windows[2u],"prevSibling",windows[1u],execution);
            query(parent,1455u); // True shared work-budget refusal, never fake None.
        }
        NativeWindowWrite(windows[3u],"Extension.Window.bIsVisible",Value::Bool(true),execution);
        auto current=query(parent,1455u);
        const auto hide=WindowNativeDeclaration(1414u,nullptr);
        while(!current.empty()) {
            if(result.walkCount>=result.liveWalk.size())throw std::runtime_error("Window navigation live walk repeated a candidate");
            observe(result.liveWalk[result.walkCount++],current);
            static_cast<void>(execution.CallFunction(*hide,current,{}));
            setClient(Object(current),{});
            // Exactly the original ordering: Hide and client None first, then
            // read this window's live lower sibling. Never capture a child list.
            current=query(Object(current),1458u);
        }
        observe(result.clients[2u],Read(windows[1u],DescribeProperty("Extension.Window.clientObject"),0u).text);
        result.clientCleared=windows[1u]->nativeWindowPropertyRoots.count("extension.window.clientobject")==0u&&
            windows[3u]->nativeWindowPropertyRoots.count("extension.window.clientobject")==0u;
        query(parent,1455u);query(parent,1456u);query(windows[1u],1458u);query(windows[3u],1457u);
        result.topologyPreserved=WindowLink(parent,"firstChild")==windows[1u]&&WindowLink(parent,"lastChild")==windows[3u]&&
            WindowLink(windows[1u],"nextSibling")==windows[2u]&&WindowLink(windows[2u],"nextSibling")==windows[3u]&&
            WindowLink(windows[2u],"prevSibling")==windows[1u]&&WindowLink(windows[3u],"prevSibling")==windows[2u]&&
            WindowLink(windows[3u],"winParent")==parent&&windows[3u]->outer==parent;
        result.completed=true;
        if(mode==Mode::LateFailure)throw std::runtime_error("Window navigation late failure after actual live walk");
        throw std::runtime_error("Window navigation diagnostic complete; rollback required");
    }
