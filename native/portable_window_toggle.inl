// Included inside PortableActorVmHost. All writes/events reuse the caller VM.
    void RequireNativeToggleWindow(RuntimeObject* window) {
        RequireNativeWindow(window);
        if(!IsDerivedFromPath(window->cls,"Extension.ToggleWindow") ||
            NativeWindowAncestor(window->cls)!="extension.togglewindow")
            throw std::runtime_error("SetToggle requires an audited constructed ToggleWindow");
        ValidateNativeWindowScalar(window,"Extension.ButtonWindow.bButtonPressed",Kind::Bool,"BoolProperty","",2u);
    }
    bool NativeToggleScriptEvent(RuntimeObject* receiver,RuntimeObject* source,const bool value,
        QuestVr::Vm::Execution& execution) {
        const auto identity=OptionalNativeWindowEvent(receiver,"ToggleChanged");
        if(!identity) return false; // ProcessScript optional event, initialized Bool0.
        const auto callback=FunctionFor(Object(identity->path));
        const auto base=FunctionFor(Object("Extension.Window.ToggleChanged"));
        const auto validate=[&](const Function& function) {
            if(function.nativeIndex || (function.flags&0x400u) || !(function.flags&0x800u))
                throw std::runtime_error("ToggleChanged is not its original script event");
            std::size_t slot{};
            for(const auto& field:function.variables) {
                if(!(field.flags&0x80u)) continue; // Script locals are not the native packet.
                if(slot>=3u || field.arrayDimension!=1u ||
                    field.zero.kind!=(slot==0u?Kind::Object:Kind::Bool) ||
                    field.flags!=(slot==2u?0x580u:0x80u))
                    throw std::runtime_error("ToggleChanged has a different declared packet");
                auto* metadata=Object(field.key);
                if(!metadata->property || metadata->property->type!=(slot==0u?"ObjectProperty":"BoolProperty") ||
                    (slot==0u && LowerAscii(Qualified(Table(metadata->sourcePath),metadata->property->referencedType))!="extension.window"))
                    throw std::runtime_error("ToggleChanged packet lacks its original Window/Bool schema");
                ++slot;
            }
            if(slot!=3u) throw std::runtime_error("ToggleChanged lacks its complete native event packet");
        };
        validate(*base);validate(*callback);
        const auto returned=execution.CallFunction(*callback,receiver->reflection.objectPath,
            {{Value::Text(Kind::Object,source?source->reflection.objectPath:std::string{}),{}},{Value::Bool(value),{}}});
        if(returned.kind==Kind::Nothing) return false; // Original initialized return survives no-result.
        if(returned.kind!=Kind::Bool) throw std::runtime_error("ToggleChanged returned a non-Bool result");
        return returned.boolean;
    }
    struct NativeToggleOperations {
        using Window=RuntimeObject*;
        PortableActorVmHost& host;
        QuestVr::Vm::Execution& execution;
        void Step() {host.LifecycleStep();}
        void EnterToggle() {
            if(host.nativeToggleDepth_>=128u) throw std::runtime_error("Native toggle recursion exceeds its bound");
            ++host.nativeToggleDepth_;
        }
        void LeaveToggle() noexcept {--host.nativeToggleDepth_;}
        bool ReadPressed(Window window) {
            host.RequireNativeToggleWindow(window);
            const auto value=host.Read(window,host.DescribeProperty("Extension.ButtonWindow.bButtonPressed"),0u);
            if(value.kind!=Kind::Bool) throw std::runtime_error("Native toggle pressed storage is not Bool");
            return value.boolean;
        }
        void WritePressed(Window window,bool value) {
            host.RequireNativeToggleWindow(window);
            host.NativeWindowWrite(window,"Extension.ButtonWindow.bButtonPressed",Value::Bool(value),execution);
        }
        void ChangeButtonAppearance(Window window) {host.ChangeNativeButtonAppearance(window,execution);}
        Window ReadParent(Window window) {host.RequireNativeWindow(window);return host.WindowLink(window,"winParent");}
        bool IsToggle(Window window) {return window && IsDerivedFromPath(window->cls,"Extension.ToggleWindow");}
        bool IsRadioBox(Window window) {
            host.RequireNativeWindow(window);
            const auto native=host.NativeWindowAncestor(window->cls);
            if(native!="extension.window" && native!="extension.tabgroupwindow" && native!="extension.modalwindow" &&
                native!="extension.rootwindow" && native!="extension.radioboxwindow" && native!="extension.tilewindow" &&
                native!="extension.textwindow" && native!="extension.buttonwindow" && native!="extension.togglewindow")
                throw std::runtime_error("Native ToggleChanged virtual override is not implemented: "+native);
            return native=="extension.radioboxwindow";
        }
        std::size_t ToggleButtonCount(Window radio) {
            host.ValidateNativeWindowList(radio,"Extension.RadioBoxWindow.toggleButtons");
            if(!radio->nativeWindowState) throw std::runtime_error("Native Radio has no owned toggle list");
            if(radio->nativeWindowState->toggleButtons.size()>4096u)
                throw std::runtime_error("Native Radio toggle list exceeds its bound");
            return radio->nativeWindowState->toggleButtons.size();
        }
        Window ToggleButtonAt(Window radio,std::size_t index) {
            if(index>=ToggleButtonCount(radio)) throw std::runtime_error("Native Radio toggle index exceeds its live list");
            return radio->nativeWindowState->toggleButtons[index];
        }
        Window ReadSelection(Window radio) {
            host.ValidateNativeWindowScalar(radio,"Extension.RadioBoxWindow.currentSelection",Kind::Object,"ObjectProperty","Extension.ToggleWindow");
            const auto value=host.Read(radio,host.DescribeProperty("Extension.RadioBoxWindow.currentSelection"),0u);
            if(value.kind!=Kind::Object) throw std::runtime_error("Native Radio selection is not Object");
            auto* selected=value.text.empty()?nullptr:host.Object(value.text);
            if(selected) host.RequireNativeToggleWindow(selected);
            return selected;
        }
        void WriteSelection(Window radio,Window selected) {
            host.ValidateNativeWindowScalar(radio,"Extension.RadioBoxWindow.currentSelection",Kind::Object,"ObjectProperty","Extension.ToggleWindow");
            if(selected) host.RequireNativeToggleWindow(selected);
            host.NativeWindowWrite(radio,"Extension.RadioBoxWindow.currentSelection",
                Value::Text(Kind::Object,selected?selected->reflection.objectPath:std::string{}),execution);
        }
        bool ReadOneCheck(Window radio) {
            host.ValidateNativeWindowScalar(radio,"Extension.RadioBoxWindow.bOneCheck",Kind::Bool,"BoolProperty");
            const auto value=host.Read(radio,host.DescribeProperty("Extension.RadioBoxWindow.bOneCheck"),0u);
            if(value.kind!=Kind::Bool) throw std::runtime_error("Native Radio bOneCheck is not Bool");
            return value.boolean;
        }
        bool ScriptToggleChanged(Window receiver,Window source,bool value) {
            return host.NativeToggleScriptEvent(receiver,source,value,execution);
        }
    };
    Evaluation WindowToggleNative(RuntimeObject* window,const std::vector<Evaluation>& arguments,
        const Function* declaration,QuestVr::Vm::Execution& execution) {
        RequireNativeToggleWindow(window);
        static_cast<void>(WindowNativeDeclaration(1611u,declaration));
        if(arguments.size()!=1u) throw std::runtime_error("SetToggle requires its single Bool input");
        auto value=arguments.front().Load();
        if(value.kind==Kind::Nothing) value=Value::Bool(false);
        if(value.kind!=Kind::Bool) throw std::runtime_error("SetToggle requires its authored Bool input");
        NativeToggleOperations operations{*this,execution};
        QuestVr::SetNativeWindowToggle(operations,window,static_cast<std::int32_t>(value.boolean));
        return {};
    }
    Value ProbeNativeWindowToggle(const std::string& playerPath,PortableToggleProbeMode mode,
        PortableToggleProbeResult& result,QuestVr::Vm::Execution& execution) {
        switch(mode) {
        case PortableToggleProbeMode::RoundTrip:case PortableToggleProbeMode::OneCheck:
        case PortableToggleProbeMode::EmptySlot:case PortableToggleProbeMode::NoResult:
        case PortableToggleProbeMode::WrongReceiver:case PortableToggleProbeMode::WrongArgument:break;
        default:throw std::runtime_error("Unknown native Toggle diagnostic mode");
        }
        auto* pawn=Object(playerPath);RequireMapActor(pawn);
        if(!IsDerivedFromPath(pawn->cls,"Extension.PlayerPawnExt") || pawn->nativeRootWindow)
            throw std::runtime_error("Native Toggle diagnostic requires an uninitialized actual player");
        // Detached observations are bounded and admitted for the ENTIRE VM
        // root, including rollback. No callbacks run after a poisoned failure.
        constexpr std::size_t snapshotBytes=128u<<10u;
        ValidateStateBudget(snapshotBytes);nativeActorOrderObservationBytes_+=snapshotBytes;
        QuestVr::AuthoredStructLimits bounds;bounds.retainedBytes=snapshotBytes;
        QuestVr::AuthoredStructDetail::Budget retained{bounds};
        result.stages.reserve(6u);result.registeredToggles.reserve(2u);
        retained.Retain(sizeof(result)+6u*sizeof(PortableToggleProbeStage)+2u*sizeof(std::string));
        // Explicit isolated BASE Root fixture; this is not a replacement for
        // the configured DeusExRootWindow or a claimed completed player/HUD.
        // Reuse the same constructor as native1052, then the real base script.
        auto* root=ConstructNativeRootWindow(pawn,DefaultClass(Object("Extension.RootWindow")),execution);
        const auto rootInit=Member(root,"InitWindow");
        if(!rootInit) throw std::runtime_error("Native Toggle fixture base Root has no original InitWindow");
        static_cast<void>(execution.CallFunction(*rootInit,root->reflection.objectPath,{}));
        const auto newChild=NewChildWindowDeclaration(nullptr);
        const auto child=[&](RuntimeObject* parent,const char* cls) {
            const auto value=execution.CallFunction(*newChild,parent->reflection.objectPath,
                {{Value::Text(Kind::Object,cls),{}},{Value::Bool(false),{}}});
            if(value.kind!=Kind::Object || value.text.empty())
                throw std::runtime_error("Native Toggle fixture NewChild returned None");
            return Object(value.text);
        };
        auto* radio=child(root,"Extension.RadioBoxWindow");
        auto* first=child(radio,mode==PortableToggleProbeMode::EmptySlot?"DeusEx.HUDObjectSlot":"Extension.ToggleWindow");
        auto* second=child(radio,"Extension.ToggleWindow");
        NativeWindowWrite(radio,"Extension.RadioBoxWindow.bOneCheck",Value::Bool(mode==PortableToggleProbeMode::OneCheck),execution);
        // Explicit generated test colors in the real typed Info[6] block make
        // normal/pressed appearance observable. No authored package is edited.
        for(auto* window:{first,second}) {
            const auto info=NativeButtonInfoProperty(window);
            for(std::uint32_t index=0u;index<6u;++index) {
                auto record=Read(window,info,index);
                auto color=CursorColorSeed();const auto base=(index&1u)?21u:11u;
                color.fields.at("r")=Value::Byte(static_cast<std::uint8_t>(base));
                color.fields.at("g")=Value::Byte(static_cast<std::uint8_t>(base+1u));
                color.fields.at("b")=Value::Byte(static_cast<std::uint8_t>(base+2u));
                color.fields.at("a")=Value::Byte(0u);
                record.fields.at("tx")=Value::Text(Kind::Object,{});
                record.fields.at("txcolor")=color;record.fields.at("tlcolor")=color;
                execution.WriteReference(ReferenceFor(window,info,index,false),record);
            }
            ChangeNativeButtonAppearance(window,execution);
        }
        for(const auto& [target,window]:{
            std::pair{&result.rootPath,root},std::pair{&result.radioPath,radio},
            std::pair{&result.firstPath,first},std::pair{&result.secondPath,second}}) {
            retained.Retain(window->reflection.objectPath.size()+1u);*target=window->reflection.objectPath;
        }
        NativeToggleOperations operations{*this,execution};
        if(operations.ToggleButtonCount(radio)!=2u) throw std::runtime_error("Native Toggle fixture lost original Radio registration");
        for(std::size_t index=0u;index<2u;++index) {
            const auto* window=operations.ToggleButtonAt(radio,index);
            retained.Retain(window->reflection.objectPath.size()+1u);
            result.registeredToggles.push_back(window->reflection.objectPath);
        }
        const auto snapshot=[&](const char* name) {
            if(result.stages.size()>=6u) throw std::runtime_error("Native Toggle diagnostic snapshot count exceeded");
            PortableToggleProbeStage stage;stage.name=name;retained.Retain(stage.name.size()+1u);
            stage.writes=execution.WriteCount();stage.firstPressed=operations.ReadPressed(first);
            stage.secondPressed=operations.ReadPressed(second);stage.oneCheck=operations.ReadOneCheck(radio);
            if(const auto* selection=operations.ReadSelection(radio)) {
                retained.Retain(selection->reflection.objectPath.size()+1u);stage.selection=selection->reflection.objectPath;
            }
            const std::array<const char*,3u> fields{
                "Extension.ButtonWindow.curTextColor","Extension.ButtonWindow.curTexture","Extension.ButtonWindow.curTileColor"};
            for(const auto& [window,appearance]:{
                std::pair{first,&stage.firstAppearance},std::pair{second,&stage.secondAppearance}})
                for(std::size_t index=0u;index<fields.size();++index) {
                    const auto value=Read(window,DescribeProperty(fields[index]),0u);
                    QuestVr::AuthoredStructDetail::RetainedValue(value,retained);(*appearance)[index]=value;
                }
            result.stages.push_back(std::move(stage));
        };
        const auto setter=WindowNativeDeclaration(1611u,nullptr);
        const auto set=[&](RuntimeObject* window,bool pressed) {
            static_cast<void>(execution.CallFunction(*setter,window->reflection.objectPath,{{Value::Bool(pressed),{}}}));
        };
        snapshot("initial");
        if(mode==PortableToggleProbeMode::WrongReceiver) set(radio,true);
        else if(mode==PortableToggleProbeMode::WrongArgument)
            static_cast<void>(execution.CallFunction(*setter,first->reflection.objectPath,{{Value::Integer(1),{}}}));
        else if(mode==PortableToggleProbeMode::NoResult) {
            set(first,true);snapshot("first-on");
            Evaluation argument{Value::Bool(true),{}};argument.disposition=QuestVr::Vm::ResultDisposition::NoWrite;
            static_cast<void>(execution.CallFunction(*setter,first->reflection.objectPath,{argument}));snapshot("no-result");
        } else {
            set(first,false);snapshot("equal-false");
            set(first,true);
            if(mode==PortableToggleProbeMode::EmptySlot) snapshot("rejected-empty");
            else {
                snapshot("first-on");set(first,true);snapshot("equal-true");
                set(second,true);snapshot("second-on");set(second,false);snapshot("second-off");
            }
        }
        result.forwardNativeWork=nativeWork_.Read().used;
        throw std::runtime_error("Native toggle diagnostic complete; rollback required");
    }
