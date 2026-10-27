// Included inside PortableActorVmHost. These nonvirtual original TextWindow
// methods use the actual receiver and the caller's same-root Execution.
    void RequireNativeTextWindowSetters(RuntimeObject* window) {
        RequireNativeWindow(window);
        auto* text=Object("Extension.TextWindow");
        auto* base=Object("Extension.Window");
        if(!text->classDescriptor || !base->classDescriptor || text->base!=base ||
            !(text->reflection.flags&0x04000000u) ||
            !IsDerivedFromPath(window->cls,"Extension.TextWindow"))
            throw std::runtime_error("TextWindow setter requires its actual constructed native superclass");
    }
    std::shared_ptr<const Function> TextWindowSetterDeclaration(const std::uint16_t index,
        const Function* declaration) {
        const char* path{};
        std::array<const char*,2u> names{};
        Kind kind{};
        const char* type{};
        switch(index) {
        case 1556u:path="Extension.TextWindow.SetTextAlignments";
            names={"newHAlign","newVAlign"};kind=Kind::Byte;type="ByteProperty";break;
        case 1557u:path="Extension.TextWindow.SetTextMargins";
            names={"newHMargin","newVMargin"};kind=Kind::Float;type="FloatProperty";break;
        case 1558u:path="Extension.TextWindow.SetLines";
            names={"newMinLines","newMaxLines"};kind=Kind::Int;type="IntProperty";break;
        default:throw std::runtime_error("Unregistered original TextWindow setter declaration");
        }
        const auto authored=FunctionFor(Object(path));
        const auto validate=[&](const Function& function) {
            if(LowerAscii(function.path)!=LowerAscii(path) || function.nativeIndex!=index ||
                function.flags!=0x401u || function.variables.size()!=names.size())
                throw std::runtime_error("TextWindow setter lacks its actual native declaration");
            for(std::size_t slot=0u;slot<names.size();++slot) {
                const auto& field=function.variables[slot];
                const auto key=LowerAscii(std::string(path)+'.'+names[slot]);
                auto* metadata=Object(field.key);
                if(LowerAscii(field.key)!=key || field.zero.kind!=kind || field.arrayDimension!=1u ||
                    field.flags!=0x80u || !metadata->property || metadata->property->type!=type)
                    throw std::runtime_error("TextWindow setter input lacks its original required scalar schema");
                if(index==1556u) {
                    const auto expected=slot==0u?"extension.extensionobject.ehalign":"extension.extensionobject.evalign";
                    if(LowerAscii(Qualified(Table(metadata->sourcePath),metadata->property->referencedType))!=expected)
                        throw std::runtime_error("TextWindow setter input lacks its actual alignment Enum identity");
                }
            }
        };
        validate(*authored);
        if(declaration) {
            validate(*declaration);
            if(declaration->source!=authored->source)
                throw std::runtime_error("TextWindow setter declaration is not the owned source");
        }
        return authored;
    }
    struct NativeTextWindowSetterOperations {
        PortableActorVmHost& host;
        RuntimeObject* window;
        QuestVr::Vm::Execution& execution;
        void Step() {host.LifecycleStep();}
        Value Read(const char* key,const Kind kind) {
            const auto value=host.Read(window,host.DescribeProperty(key),0u);
            if(value.kind!=kind) throw std::runtime_error("Native TextWindow setter storage changed scalar type");
            return value;
        }
        void WriteHAlignment(std::uint8_t value) {
            host.NativeWindowWrite(window,"Extension.TextWindow.HAlign",Value::Byte(value),execution);
        }
        void WriteVAlignment(std::uint8_t value) {
            host.NativeWindowWrite(window,"Extension.TextWindow.VAlign",Value::Byte(value),execution);
        }
        float ReadHMargin() {return Read("Extension.TextWindow.hMargin",Kind::Float).floating;}
        float ReadVMargin() {return Read("Extension.TextWindow.vMargin",Kind::Float).floating;}
        void WriteHMargin(float value) {
            host.NativeWindowWrite(window,"Extension.TextWindow.hMargin",Value::Float(value),execution);
        }
        void WriteVMargin(float value) {
            host.NativeWindowWrite(window,"Extension.TextWindow.vMargin",Value::Float(value),execution);
        }
        std::int32_t ReadMinimumLines() {return Read("Extension.TextWindow.minLines",Kind::Int).integer;}
        std::int32_t ReadMaximumLines() {return Read("Extension.TextWindow.MaxLines",Kind::Int).integer;}
        void WriteMinimumLines(std::int32_t value) {
            host.NativeWindowWrite(window,"Extension.TextWindow.minLines",Value::Integer(value),execution);
        }
        void WriteMaximumLines(std::int32_t value) {
            host.NativeWindowWrite(window,"Extension.TextWindow.MaxLines",Value::Integer(value),execution);
        }
        void AskForReconfigure() {host.AskHiddenWindowForReconfigure(window,execution);}
    };
    Evaluation TextWindowSetterNative(const std::uint16_t index,RuntimeObject* window,
        const std::vector<Evaluation>& arguments,const Function* declaration,QuestVr::Vm::Execution& execution) {
        RequireNativeTextWindowSetters(window);
        const auto authored=TextWindowSetterDeclaration(index,declaration);
        if(arguments.size()!=2u) throw std::runtime_error("TextWindow setter requires its two original inputs");
        if(index==1556u) {
            ValidateNativeWindowScalar(window,"Extension.TextWindow.HAlign",Kind::Byte,"ByteProperty",
                "Extension.ExtensionObject.EHAlign",2u);
            ValidateNativeWindowScalar(window,"Extension.TextWindow.VAlign",Kind::Byte,"ByteProperty",
                "Extension.ExtensionObject.EVAlign",2u);
        } else if(index==1557u) {
            for(const auto* name:{"hMargin","vMargin"})
                ValidateNativeWindowScalar(window,("Extension.TextWindow."+std::string(name)).c_str(),
                    Kind::Float,"FloatProperty","",2u);
        } else {
            for(const auto* name:{"minLines","MaxLines"})
                ValidateNativeWindowScalar(window,("Extension.TextWindow."+std::string(name)).c_str(),
                    Kind::Int,"IntProperty","",2u);
        }
        std::array<Value,2u> values;
        for(std::size_t slot=0u;slot<values.size();++slot) {
            values[slot]=arguments[slot].Load();
            if(values[slot].kind==Kind::Nothing) values[slot]=authored->variables[slot].zero;
            if(values[slot].kind!=authored->variables[slot].zero.kind)
                throw std::runtime_error("TextWindow setter argument changed its original scalar type");
        }
        NativeTextWindowSetterOperations operations{*this,window,execution};
        if(index==1556u)
            QuestVr::NativeTextWindowSetters::SetAlignments(operations,
                static_cast<std::uint8_t>(values[0u].integer),static_cast<std::uint8_t>(values[1u].integer));
        else if(index==1557u)
            QuestVr::NativeTextWindowSetters::SetMargins(operations,values[0u].floating,values[1u].floating);
        else QuestVr::NativeTextWindowSetters::SetLines(operations,values[0u].integer,values[1u].integer);
        return {};
    }
