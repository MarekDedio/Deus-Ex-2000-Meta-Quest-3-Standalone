// Included inside PortableActorVmHost; all field writes reuse the original VM.
    PortableTextLogProbeResult* nativeTextLogProbe_{}; // Observation only; never a GC root.
    void ValidateNativeTextLogFields(RuntimeObject* window) {
        RequireNativeWindow(window);
        if(!IsDerivedFromPath(window->cls,"Extension.TextLogWindow") ||
            NativeWindowAncestor(window->cls)!="extension.textlogwindow")
            throw std::runtime_error("Native TextLog requires its actual constructed class ancestry");
        ValidateNativeWindowList(window,"Extension.TextLogWindow.Lines");
        for(const auto* name:{"bPaused","bTooTall"})
            ValidateNativeWindowScalar(window,("Extension.TextLogWindow."+std::string(name)).c_str(),Kind::Bool,"BoolProperty");
        ValidateNativeWindowScalar(window,"Extension.TextLogWindow.textTimeout",Kind::Float,"FloatProperty");
    }
    void InitializeChildTextLogWindow(RuntimeObject* window,QuestVr::Vm::Execution& execution) {
        ValidateNativeTextLogFields(window);
        if(!window->nativeWindowState || !window->nativeWindowState->textLog.initialized)
            throw std::runtime_error("Native TextLog Init lacks its constructor-owned Lines array");
        // Retail100474B0 calls Text.Init first, AFTER its native array ctor and
        // Window.Init/parent notifications. Never copy a Win32 FArray address.
        InitializeChildTextWindow(window,execution);
        NativeWindowWrite(window,"Extension.TextWindow.HAlign",Value::Byte(0u),execution);
        NativeWindowWrite(window,"Extension.TextWindow.VAlign",Value::Byte(0u),execution);
        NativeWindowWrite(window,"Extension.TextLogWindow.textTimeout",Value::Float(5.0f),execution);
        Touch(window);
        // Suffix destroys any callback-created strings and frees arrayMax too.
        // Do not assert empty, merely clear count, or alter copied bPaused.
        window->nativeWindowState->textLog.Clear();
        NativeWindowWrite(window,"Extension.TextLogWindow.bTooTall",Value::Bool(false),execution);
        ValidateStateBudget();
    }
    std::shared_ptr<const Function> TextLogScalarDeclaration(const std::uint16_t index,const Function* declaration) {
        if(index!=1572u && index!=1573u)throw std::runtime_error("Unregistered TextLog scalar declaration");
        const std::string path=index==1572u?"Extension.TextLogWindow.SetTextTimeout":"Extension.TextLogWindow.PauseLog";
        const auto authored=FunctionFor(Object(path));
        const auto validate=[&](const Function& function) {
            if(LowerAscii(function.path)!=LowerAscii(path) || function.nativeIndex!=index ||
                function.flags!=0x401u || function.variables.size()!=1u)
                throw std::runtime_error("TextLog scalar lacks its original native declaration");
            const auto& input=function.variables[0u];auto* metadata=Object(input.key);
            if(LowerAscii(input.key)!=LowerAscii(path+(index==1572u?".newTimeout":".bNewPauseState")) ||
                input.zero.kind!=(index==1572u?Kind::Float:Kind::Bool) || input.arrayDimension!=1u ||
                input.flags!=0x80u || !metadata->property ||
                metadata->property->type!=(index==1572u?"FloatProperty":"BoolProperty"))
                throw std::runtime_error("TextLog scalar lacks its actual required input schema");
        };
        validate(*authored);
        if(declaration) {
            validate(*declaration);
            if(declaration->source!=authored->source)throw std::runtime_error("TextLog scalar declaration is not its owned source");
        }
        return authored;
    }
    Evaluation TextLogScalarNative(const std::uint16_t index,RuntimeObject* window,
        const std::vector<Evaluation>& arguments,const Function* declaration,QuestVr::Vm::Execution& execution) {
        ValidateNativeTextLogFields(window);
        if(!window->nativeWindowState || !window->nativeWindowState->textLog.initialized)
            throw std::runtime_error("TextLog scalar requires its constructor-owned native Lines");
        const auto authored=TextLogScalarDeclaration(index,declaration);
        if(arguments.size()!=1u)throw std::runtime_error("TextLog scalar requires its one original input");
        auto value=arguments[0u].Load();if(value.kind==Kind::Nothing)value=authored->variables[0u].zero;
        if(value.kind!=authored->variables[0u].zero.kind)
            throw std::runtime_error("TextLog scalar argument changed its original type");
        LifecycleStep();
        //1572 stores the raw Float word;1573 only assigns the reflected paused
        // bit. Always write, even equal; no regeneration/layout/expiry updates.
        NativeWindowWrite(window,index==1572u?"Extension.TextLogWindow.textTimeout":"Extension.TextLogWindow.bPaused",value,execution);
        return {};
    }
    Value ProbeNativeTextLogWindow(const std::string& playerPath,const PortableTextLogProbeMode mode,
        PortableTextLogProbeResult& result,QuestVr::Vm::Execution& execution) {
        switch(mode) {
        case PortableTextLogProbeMode::RoundTrip:case PortableTextLogProbeMode::PrivateRead:
        case PortableTextLogProbeMode::PrivateWrite:case PortableTextLogProbeMode::WrongReceiver:
        case PortableTextLogProbeMode::WrongArgument:case PortableTextLogProbeMode::MissingArgument:
        case PortableTextLogProbeMode::LateFailure:case PortableTextLogProbeMode::WrongColor:
        case PortableTextLogProbeMode::NoColorResult:break;
        default:throw std::runtime_error("Unknown native TextLog diagnostic mode");
        }
        auto* pawn=Object(playerPath);RequireMapActor(pawn);
        if(!IsDerivedFromPath(pawn->cls,"Extension.PlayerPawnExt") || pawn->nativeRootWindow)
            throw std::runtime_error("Native TextLog diagnostic requires an uninitialized actual player");
        constexpr std::size_t snapshotBytes=64u<<10u;
        ValidateStateBudget(snapshotBytes);nativeActorOrderObservationBytes_+=snapshotBytes;
        QuestVr::AuthoredStructLimits bounds;bounds.retainedBytes=snapshotBytes;
        QuestVr::AuthoredStructDetail::Budget retained{bounds};
        retained.Retain(sizeof(result)+16u*sizeof(PortableTextLogProbeStage));result.stages.reserve(16u);
        struct ObservationScope {
            PortableTextLogProbeResult*& slot;PortableTextLogProbeResult* previous;
            ~ObservationScope() {slot=previous;}
        } observer{nativeTextLogProbe_,nativeTextLogProbe_};
        nativeTextLogProbe_=&result;
        // Explicit isolated BASE Root fixture, NOT a configured HUD replacement.
        auto* root=ConstructNativeRootWindow(pawn,DefaultClass(Object("Extension.RootWindow")),execution);
        const auto rootInit=Member(root,"InitWindow");
        if(!rootInit) throw std::runtime_error("Native TextLog fixture base Root has no original InitWindow");
        static_cast<void>(execution.CallFunction(*rootInit,root->reflection.objectPath,{}));
        const auto newChild=NewChildWindowDeclaration(nullptr);
        const auto child=execution.CallFunction(*newChild,root->reflection.objectPath,
            {{Value::Text(Kind::Object,"Extension.TextLogWindow"),{}},{Value::Bool(false),{}}});
        if(child.kind!=Kind::Object || child.text.empty())
            throw std::runtime_error("Native TextLog fixture NewChild returned None");
        auto* log=Object(child.text);ValidateNativeTextLogFields(log);
        const auto context=[&](const char* key) {
            const auto value=Read(log,DescribeProperty(key),0u);
            if(value.kind!=Kind::Object || value.text.empty())
                throw std::runtime_error("Native TextLog fixture lost its actual graphics context");
            auto* gc=Object(value.text);
            if(!gc->nativeWindowObject || !IsDerivedFromPath(gc->cls,"Extension.GC"))
                throw std::runtime_error("Native TextLog fixture context is not its constructed GC");
            return gc;
        };
        auto* windowGC=context("Extension.Window.wGc");auto* generalGC=context("Extension.Window.gGc");
        retained.Retain(root->reflection.objectPath.size()+1u+log->reflection.objectPath.size()+1u);
        result.rootPath=root->reflection.objectPath;result.logPath=log->reflection.objectPath;
        const auto read=[&](const char* key,const Kind kind) {
            const auto value=Read(log,DescribeProperty(key),0u);
            if(value.kind!=kind)throw std::runtime_error("Native TextLog diagnostic field changed scalar type");
            return value;
        };
        const auto snapshot=[&](const char* name,const bool colors=false) {
            if(result.stages.size()>=16u || !log->nativeWindowState)
                throw std::runtime_error("Native TextLog diagnostic snapshot lacks its bounded native state");
            PortableTextLogProbeStage stage;stage.name=name;retained.Retain(stage.name.capacity()+1u);
            const auto& storage=log->nativeWindowState->textLog;
            stage.writes=execution.WriteCount();stage.linesInitialized=storage.initialized;
            stage.lineCapacity=storage.lines.capacity();
            stage.paused=read("Extension.TextLogWindow.bPaused",Kind::Bool).boolean;
            stage.tooTall=read("Extension.TextLogWindow.bTooTall",Kind::Bool).boolean;
            stage.needsQuery=read("Extension.Window.bNeedsQuery",Kind::Bool).boolean;
            stage.needsReconfigure=read("Extension.Window.bNeedsReconfigure",Kind::Bool).boolean;
            stage.timeout=read("Extension.TextLogWindow.textTimeout",Kind::Float).floating;
            stage.hAlign=static_cast<std::uint8_t>(read("Extension.TextWindow.HAlign",Kind::Byte).integer);
            stage.vAlign=static_cast<std::uint8_t>(read("Extension.TextWindow.VAlign",Kind::Byte).integer);
            stage.hMargin=read("Extension.TextWindow.hMargin",Kind::Float).floating;
            stage.vMargin=read("Extension.TextWindow.vMargin",Kind::Float).floating;
            stage.minLines=read("Extension.TextWindow.minLines",Kind::Int).integer;
            stage.maxLines=read("Extension.TextWindow.MaxLines",Kind::Int).integer;
            retained.Retain(storage.lines.size()*sizeof(PortableNativeTextLogLineSnapshot));
            stage.lines.reserve(storage.lines.size());
            for(const auto& line:storage.lines) {
                retained.Retain(line.text.capacity()+1u);
                stage.lines.push_back({line.expiry,line.text,line.color});
            }
            if(colors) {
                const auto copy=[&](RuntimeObject* object,const char* key,Value& destination) {
                    destination=Read(object,DescribeProperty(key),0u);
                    QuestVr::AuthoredStructDetail::RetainedValue(destination,retained);
                };
                copy(log,"Extension.Window.TextColor",stage.textColor);
                copy(log,"Extension.Window.textPlane",stage.textPlane);
                // The detached original wGc can still be observed in the None
                // fixture; this does not pretend it remains the current wGc.
                copy(windowGC,"Extension.GC.TextColor",stage.windowGCTextColor);
                copy(windowGC,"Extension.GC.textPlane",stage.windowGCTextPlane);
                copy(generalGC,"Extension.GC.TextColor",stage.generalGCTextColor);
                copy(generalGC,"Extension.GC.textPlane",stage.generalGCTextPlane);
            }
            result.stages.push_back(std::move(stage));
        };
        const auto setter=[&](const std::uint16_t index,RuntimeObject* receiver,const std::vector<Evaluation>& arguments) {
            const auto authored=TextWindowSetterDeclaration(index,nullptr);
            static_cast<void>(execution.CallFunction(*authored,receiver->reflection.objectPath,arguments));
        };
        snapshot("constructed");
        if(mode==PortableTextLogProbeMode::PrivateRead)
            static_cast<void>(Read(log,DescribeProperty("Extension.TextLogWindow.Lines"),0u));
        else if(mode==PortableTextLogProbeMode::PrivateWrite) {
            const auto property=DescribeProperty("Extension.TextLogWindow.Lines");
            // Exercise the host store guard directly. VM WriteReference first
            // reads the old value and would hit the separate read refusal.
            ReferenceFor(log,property,0u,false)->write(property.zero);
        } else if(mode==PortableTextLogProbeMode::WrongReceiver)
            setter(1556u,root,{{Value::Byte(0u),{}},{Value::Byte(0u),{}}});
        else if(mode==PortableTextLogProbeMode::WrongArgument)
            setter(1557u,log,{{Value::Integer(1),{}},{Value::Float(0.0f),{}}});
        else if(mode==PortableTextLogProbeMode::MissingArgument)
            setter(1556u,log,{{Value::Byte(0u),{}}});
        else if(mode==PortableTextLogProbeMode::WrongColor || mode==PortableTextLogProbeMode::NoColorResult) {
            const auto authored=WindowNativeDeclaration(1476u,nullptr);
            auto color=CursorColorSeed();
            if(mode==PortableTextLogProbeMode::WrongColor)color.fields.at("a")=Value::Integer(7);
            Evaluation input{std::move(color),{}};
            if(mode==PortableTextLogProbeMode::NoColorResult)input.disposition=QuestVr::Vm::ResultDisposition::NoWrite;
            static_cast<void>(execution.CallFunction(*authored,log->reflection.objectPath,{input}));
        }
        // Any refused mode must throw from its authentic ownership/schema guard.
        if(mode!=PortableTextLogProbeMode::RoundTrip && mode!=PortableTextLogProbeMode::LateFailure)
            throw std::runtime_error("Native TextLog diagnostic guard unexpectedly accepted its invalid request");
        const auto scalar=[&](const std::uint16_t index,Evaluation value) {
            const auto authored=TextLogScalarDeclaration(index,nullptr);
            static_cast<void>(execution.CallFunction(*authored,log->reflection.objectPath,{std::move(value)}));
        };
        scalar(1573u,{Value::Bool(true),{}});
        NativeWindowWrite(log,"Extension.TextLogWindow.bTooTall",Value::Bool(true),execution);
        scalar(1572u,{Value::Float(-9.0f),{}});
        // Explicit owned-storage fixture, not a fabricated script AddLog call.
        ValidateStateBudget(37u*sizeof(QuestVr::NativeTextLogLine)+512u);Touch(log);
        auto& storage=log->nativeWindowState->textLog;
        storage.lines.reserve(37u);
        QuestVr::NativeTextLogLine line;line.expiry=-0.0f;line.text=std::string("owned\0log",9u);
        line.text.reserve(64u);line.color={7u,11u,19u,0u};storage.lines.push_back(std::move(line));
        ValidateStateBudget();snapshot("fixture-populated");
        // Exercise the actual inherited Text.Init/TextLog suffix, not a second
        // base Window.Init which would incorrectly link this child twice.
        InitializeChildTextLogWindow(log,execution);
        // Explicit clean layout-flag fixture makes the setter's own request
        // observable. These stores are NOT claimed as TextLog.Init effects.
        NativeWindowWrite(log,"Extension.Window.bNeedsQuery",Value::Bool(false),execution);
        NativeWindowWrite(log,"Extension.Window.bNeedsReconfigure",Value::Bool(false),execution);
        snapshot("reinitialized");
        setter(1556u,log,{{Value::Byte(255u),{}},{Value::Byte(254u),{}}});snapshot("aligned");
        setter(1556u,log,{{Value::Byte(255u),{}},{Value::Byte(254u),{}}});snapshot("equal-alignment");
        setter(1557u,log,{{Value::Float(1.25f),{}},{Value::Float(-2.5f),{}}});snapshot("margins");
        setter(1557u,log,{{Value::Float(1.25f),{}},{Value::Float(-2.5f),{}}});snapshot("equal-margins");
        const auto nan=std::bit_cast<float>(std::uint32_t{0x7fc01234u});
        setter(1557u,log,{{Value::Float(nan),{}},{Value::Float(nan),{}}});snapshot("nan-margins");
        setter(1558u,log,{{Value::Integer(-7),{}},{Value::Integer(-9),{}}});snapshot("negative-lines");
        setter(1558u,log,{{Value::Integer(10),{}},{Value::Integer(3),{}}});snapshot("clamped-lines");
        setter(1558u,log,{{Value::Integer(3),{}},{Value::Integer(3),{}}});snapshot("equal-lines");
        Evaluation noResult{Value::Byte(255u),{}};noResult.disposition=QuestVr::Vm::ResultDisposition::NoWrite;
        setter(1556u,log,{noResult,noResult});snapshot("no-result");
        const std::array<const char*,6u> preservedFields{
            "Extension.Window.normalFont","Extension.Window.boldFont","Extension.Window.backgroundStyle",
            "Extension.Window.bNeedsQuery","Extension.Window.bNeedsReconfigure","Extension.Window.bTextTranslucent"};
        std::array<Value,6u> beforeFields;
        for(std::size_t slot=0u;slot<preservedFields.size();++slot) {
            beforeFields[slot]=Read(log,DescribeProperty(preservedFields[slot]),0u);
            QuestVr::AuthoredStructDetail::RetainedValue(beforeFields[slot],retained);
        }
        const auto generalColor=Read(generalGC,DescribeProperty("Extension.GC.TextColor"),0u);
        const auto generalPlane=Read(generalGC,DescribeProperty("Extension.GC.textPlane"),0u);
        QuestVr::AuthoredStructDetail::RetainedValue(generalColor,retained);
        QuestVr::AuthoredStructDetail::RetainedValue(generalPlane,retained);
        const auto colorDeclaration=WindowNativeDeclaration(1476u,nullptr);
        const auto setColor=[&](const std::array<std::uint8_t,4u> channels) {
            auto value=CursorColorSeed();std::size_t slot{};
            for(const auto* name:{"r","g","b","a"})value.fields.at(name)=Value::Byte(channels[slot++]);
            static_cast<void>(execution.CallFunction(*colorDeclaration,log->reflection.objectPath,{{value,{}}}));
        };
        snapshot("color-initial",true);
        setColor({17u,101u,239u,7u});snapshot("color-changed",true);
        setColor({17u,101u,239u,7u});snapshot("equal-color",true);
        const auto beforeNone=execution.WriteCount();
        NativeWindowWrite(log,"Extension.Window.wGc",Value::Text(Kind::Object,{}),execution);
        result.colorNoneFixtureWrites=execution.WriteCount()-beforeNone;
        setColor({255u,0u,128u,255u});snapshot("color-none",true);
        // Restore the real context only as explicit fixture state for the
        // subsequent all-channel native sweep; displayed deltas stay exact.
        NativeWindowWrite(log,"Extension.Window.wGc",Value::Text(Kind::Object,windowGC->reflection.objectPath),execution);
        const auto verifyColor=[&](RuntimeObject* target,const char* base,const std::array<std::uint8_t,4u> channels) {
            const auto color=Read(target,DescribeProperty(std::string(base)+".TextColor"),0u);
            const auto plane=Read(target,DescribeProperty(std::string(base)+".textPlane"),0u);
            if(color.kind!=Kind::Struct || plane.kind!=Kind::Struct || color.fields.size()!=4u || plane.fields.size()!=4u)
                throw std::runtime_error("Native text-color diagnostic lost its canonical Color/Plane storage");
            std::size_t slot{};
            for(const auto* name:{"r","g","b","a"}) {
                const auto& channel=color.fields.at(name);
                if(channel.kind!=Kind::Byte || channel.integer!=channels[slot++])
                    throw std::runtime_error("Native text-color diagnostic changed an RGBA byte");
            }
            slot=0u;
            for(const auto* name:{"x","y","z","w"}) {
                const auto& axis=plane.fields.at(name);
                const auto expected=slot<3u?static_cast<float>(static_cast<double>(channels[slot])*(1.0/255.0)):0.0f;
                if(axis.kind!=Kind::Float || std::bit_cast<std::uint32_t>(axis.floating)!=std::bit_cast<std::uint32_t>(expected))
                    throw std::runtime_error("Native text-color diagnostic changed original RGB normalization or positive-zero W");
                ++slot;
            }
        };
        for(std::uint32_t channel=0u;channel<256u;++channel) {
            LifecycleStep();
            const std::array<std::uint8_t,4u> channels{static_cast<std::uint8_t>(channel),
                static_cast<std::uint8_t>(255u-channel),static_cast<std::uint8_t>(channel^0x55u),
                static_cast<std::uint8_t>((channel*29u)&255u)};
            const auto writes=execution.WriteCount();setColor(channels);
            if(execution.WriteCount()!=writes+4u)
                throw std::runtime_error("Native text-color diagnostic lost unconditional Window/wGc writes");
            verifyColor(log,"Extension.Window",channels);verifyColor(windowGC,"Extension.GC",channels);
            if(!QuestVr::Vm::Equal(generalColor,Read(generalGC,DescribeProperty("Extension.GC.TextColor"),0u)) ||
                !QuestVr::Vm::Equal(generalPlane,Read(generalGC,DescribeProperty("Extension.GC.textPlane"),0u)))
                throw std::runtime_error("Native text color incorrectly changed the general graphics context");
            for(std::size_t slot=0u;slot<preservedFields.size();++slot)
                if(!QuestVr::Vm::Equal(beforeFields[slot],Read(log,DescribeProperty(preservedFields[slot]),0u)))
                    throw std::runtime_error("Native text color incorrectly changed layout, fonts or style");
            ++result.colorCasesVerified;
        }
        const auto beforeScalarColor=Read(log,DescribeProperty("Extension.Window.TextColor"),0u);
        const auto beforeScalarPlane=Read(log,DescribeProperty("Extension.Window.textPlane"),0u);
        QuestVr::AuthoredStructDetail::RetainedValue(beforeScalarColor,retained);
        QuestVr::AuthoredStructDetail::RetainedValue(beforeScalarPlane,retained);
        for(const auto bits:{0u,0x80000000u,0xbf800000u,0x7f800000u,0xff800000u,0x7fc01234u,0x7f801234u}) {
            for(unsigned duplicate=0u;duplicate<2u;++duplicate) {
                const auto writes=execution.WriteCount();scalar(1572u,{Value::Float(std::bit_cast<float>(bits)),{}});
                if(execution.WriteCount()!=writes+1u || std::bit_cast<std::uint32_t>(read("Extension.TextLogWindow.textTimeout",Kind::Float).floating)!=bits)
                    throw std::runtime_error("TextLog timeout did not preserve its unconditional raw Float store");
                ++result.scalarCasesVerified;
            }
        }
        Evaluation noTimeout{Value::Float(-9.0f),{}};noTimeout.disposition=QuestVr::Vm::ResultDisposition::NoWrite;
        scalar(1572u,noTimeout);
        if(std::bit_cast<std::uint32_t>(read("Extension.TextLogWindow.textTimeout",Kind::Float).floating)!=0u)
            throw std::runtime_error("TextLog timeout no-result lost its original positive-zero seed");
        ++result.scalarCasesVerified;
        NativeWindowWrite(log,"Extension.TextLogWindow.bTooTall",Value::Bool(true),execution);
        for(const bool paused:{false,false,true,true}) {
            const auto writes=execution.WriteCount();scalar(1573u,{Value::Bool(paused),{}});
            if(execution.WriteCount()!=writes+1u || read("Extension.TextLogWindow.bPaused",Kind::Bool).boolean!=paused ||
                !read("Extension.TextLogWindow.bTooTall",Kind::Bool).boolean)
                throw std::runtime_error("TextLog pause changed another flag or skipped its unconditional store");
            ++result.scalarCasesVerified;
        }
        Evaluation noPause{Value::Bool(true),{}};noPause.disposition=QuestVr::Vm::ResultDisposition::NoWrite;
        scalar(1573u,noPause);
        if(read("Extension.TextLogWindow.bPaused",Kind::Bool).boolean || !read("Extension.TextLogWindow.bTooTall",Kind::Bool).boolean ||
            !storage.initialized || !storage.lines.empty() || storage.lines.capacity()!=0u ||
            !QuestVr::Vm::Equal(beforeScalarColor,Read(log,DescribeProperty("Extension.Window.TextColor"),0u)) ||
            !QuestVr::Vm::Equal(beforeScalarPlane,Read(log,DescribeProperty("Extension.Window.textPlane"),0u)))
            throw std::runtime_error("TextLog scalar no-result seed changed unrelated native state");
        ++result.scalarCasesVerified;
        if(mode==PortableTextLogProbeMode::LateFailure)
            throw std::runtime_error("Native TextLog diagnostic forced late failure");
        throw std::runtime_error("Native TextLog diagnostic complete; rollback required");
    }
