// Included inside PortableActorVmHost. Authentic1550 input/virtual dispatch,
// original UTF16 conversion/content/accelerator sequence and root ownership.
    std::shared_ptr<const Function> NativeTextContentDeclaration(const Function* supplied) {
        const auto authored=FunctionFor(Object("Extension.TextWindow.SetText"));
        const auto validate=[&](const Function& function) {
            if(LowerAscii(function.path)!="extension.textwindow.settext"||function.nativeIndex!=1550u||
                function.flags!=0x401u||function.variables.size()!=1u)
                throw std::runtime_error("TextWindow.SetText lacks its original native declaration");
            const auto& input=function.variables.front();auto* metadata=Object(input.key);
            if(LowerAscii(input.key)!="extension.textwindow.settext.newtext"||input.zero.kind!=Kind::String||
                input.arrayDimension!=1u||input.flags!=0x400880u||!metadata->property||metadata->property->type!="StrProperty")
                throw std::runtime_error("TextWindow.SetText input lacks its original string packet");
        };
        validate(*authored);
        if(supplied) {validate(*supplied);if(supplied->source!=authored->source)
            throw std::runtime_error("TextWindow.SetText declaration is not its owned source");}
        return authored;
    }
    struct NativeTextContentOperations {
        PortableActorVmHost& host;RuntimeObject* window;QuestVr::Vm::Execution& execution;
        std::u16string previous;
        void Step() {host.LifecycleStep();}
        std::u16string_view ReadText() {return previous;}
        void WriteText(std::u16string_view value) {
            host.NativeWindowWrite(window,"Extension.TextWindow.Text",
                Value::Text(Kind::String,QuestVr::UnrealString::Encode(value)),execution);
        }
        bool TextIsAccelerator() {
            const auto value=host.Read(window,host.DescribeProperty("Extension.TextWindow.bTextIsAccelerator"),0u);
            if(value.kind!=Kind::Bool)throw std::runtime_error("TextWindow accelerator flag lost its Bool storage");
            return value.boolean;
        }
        std::int32_t ReadAcceleratorKey() {
            const auto value=host.Read(window,host.DescribeProperty("Extension.Window.acceleratorKey"),0u);
            if(value.kind!=Kind::Int)throw std::runtime_error("Window accelerator lost its Int storage");return value.integer;
        }
        void WriteAcceleratorKey(std::int32_t key) {
            host.NativeWindowWrite(window,"Extension.Window.acceleratorKey",Value::Integer(key),execution);
        }
        void DirtyAccelerators() {
            if(auto* modal=host.NativeWindowModal(window)) {
                host.ValidateNativeWindowScalar(modal,"Extension.ModalWindow.bDirtyAccelerators",Kind::Bool,"BoolProperty");
                host.NativeWindowWrite(modal,"Extension.ModalWindow.bDirtyAccelerators",Value::Bool(true),execution);
            }
        }
        void AskForReconfigure() {host.AskHiddenWindowForReconfigure(window,execution);}
    };
    Evaluation NativeTextContentMethod(RuntimeObject* window,const std::vector<Evaluation>& arguments,
        const Function* declaration,QuestVr::Vm::Execution& execution) {
        RequireNativeTextWindowSetters(window);static_cast<void>(NativeTextContentDeclaration(declaration));
        const auto native=NativeWindowAncestor(window->cls);
        // Original+11C: Text5A574,TextLog5B758,Button5A440,Toggle5A8E4
        // all point to10045D50. Edit/LargeText have different native overrides.
        if(native!="extension.textwindow"&&native!="extension.textlogwindow"&&
            native!="extension.buttonwindow"&&native!="extension.togglewindow")
            throw std::runtime_error("TextWindow.SetText native virtual override is not implemented: "+native);
        if(arguments.size()!=1u)throw std::runtime_error("TextWindow.SetText requires one String argument");
        const auto property=DescribeProperty("Extension.TextWindow.Text");
        ValidateNativeWindowScalar(window,property.key.c_str(),Kind::String,"StrProperty","",0x1000u);
        ValidateNativeWindowScalar(window,"Extension.TextWindow.bTextIsAccelerator",Kind::Bool,"BoolProperty","",2u);
        ValidateNativeWindowScalar(window,"Extension.Window.acceleratorKey",Kind::Int,"IntProperty");
        if(ClassProperty(window->cls,"Text").key!=property.key)
            throw std::runtime_error("Native TextWindow.SetText cannot alias a shadowed private Text field");
        const auto* previous=FindScriptOverlay(window,"Text",0u);if(!previous)previous=&property.zero;
        if(previous->kind!=Kind::String)throw std::runtime_error("Native TextWindow.Text is not FString");
        // VM1550 snapshots ordinary inputs before entry. Native ingress
        // still needs its copied input and all decode/encode/reallocation
        // overlap admitted before Load allocates. No original DLL heap/global
        // pointer escapes this synchronous call's bounded owned conversion.
        const auto& argument=arguments.front();
        if(argument.reference||argument.disposition!=QuestVr::Vm::ResultDisposition::Written)
            throw std::runtime_error("TextWindow.SetText requires its materialized copied input");
        const auto& borrowed=argument.value;
        if(borrowed.kind!=Kind::String&&borrowed.kind!=Kind::Nothing)
            throw std::runtime_error("TextWindow.SetText input is not String");
        const auto sum=QuestVr::DecalStateDetail::Add(borrowed.text.size(),previous->text.size());
        const auto bytes=QuestVr::DecalStateDetail::Add(sizeof(NativeTextContentOperations)+sizeof(Value),
            QuestVr::DecalStateDetail::Multiply(QuestVr::DecalStateDetail::Add(sum,2u),16u));
        QuestVr::NativeScratchScope scratch(nativeQueryScratchBytes_,bytes,
            [this](std::size_t extra){ValidateStateBudget(extra);});
        auto input=argument.Load();if(input.kind==Kind::Nothing)input=Value::Text(Kind::String,{});
        if(input.kind!=Kind::String)throw std::runtime_error("TextWindow.SetText input is not String");
        for(std::size_t index=0u;index<input.text.size();++index)LifecycleStep();
        auto converted=QuestVr::UnrealString::Decode(input.text);
        QuestVr::NativeTextContent::ConvertScriptString(converted,[this]{LifecycleStep();});
        for(std::size_t index=0u;index<previous->text.size();++index)LifecycleStep();
        NativeTextContentOperations operations{*this,window,execution,QuestVr::UnrealString::Decode(previous->text)};
        QuestVr::NativeTextContent::SetText(operations,converted);
        return {};
    }
