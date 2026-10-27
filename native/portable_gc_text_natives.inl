// Included inside PortableActorVmHost after its genuine GC/font measurement
// provider. These are original native packets, not presentation estimates.
    std::shared_ptr<const Function> NativeGcTextDeclaration(std::uint16_t index,const Function* supplied) {
        struct Parameter {const char* name;Kind kind;std::uint32_t flags;const char* type;const char* constraint;};
        const char* path{};std::vector<Parameter> parameters;
        switch(index) {
        case 1242u:path="Extension.GC.SetFont";
            parameters={{"NewFont",Kind::Object,0x80u,"ObjectProperty","Engine.Font"}};break;
        case 1280u:path="Extension.GC.GetTextExtent";
            parameters={{"destWidth",Kind::Float,0x80u,"FloatProperty",""},
                {"xExtent",Kind::Float,0x180u,"FloatProperty",""},{"yExtent",Kind::Float,0x180u,"FloatProperty",""},
                {"textStr",Kind::String,0x400880u,"StrProperty",""}};break;
        case 1281u:path="Extension.GC.GetFontHeight";
            parameters={{"bIncludeSpace",Kind::Bool,0x90u,"BoolProperty",""},
                {"ReturnValue",Kind::Float,0x580u,"FloatProperty",""}};break;
        case 1469u:path="Extension.Window.ConvertScriptString";
            parameters={{"oldStr",Kind::String,0x400080u,"StrProperty",""},
                {"ReturnValue",Kind::String,0x400580u,"StrProperty",""}};break;
        default:throw std::runtime_error("Unregistered GC/text native declaration");
        }
        const auto authored=FunctionFor(Object(path));
        const auto validate=[&](const Function& function) {
            if(LowerAscii(function.path)!=LowerAscii(path)||function.nativeIndex!=index||
                function.flags!=0x401u||function.variables.size()!=parameters.size())
                throw std::runtime_error("GC/text method lacks its original native declaration");
            for(std::size_t slot=0u;slot<parameters.size();++slot) {
                const auto& expected=parameters[slot];const auto& field=function.variables[slot];auto* metadata=Object(field.key);
                if(LowerAscii(field.key)!=LowerAscii(std::string(path)+'.'+expected.name)||field.zero.kind!=expected.kind||
                    field.flags!=expected.flags||field.arrayDimension!=1u||!metadata->property||metadata->property->type!=expected.type||
                    (*expected.constraint&&LowerAscii(Qualified(Table(metadata->sourcePath),metadata->property->referencedType))!=
                        LowerAscii(expected.constraint)))
                    throw std::runtime_error("GC/text method packet differs from its original scalar schema");
            }
        };
        validate(*authored);
        if(supplied){validate(*supplied);if(supplied->source!=authored->source)
            throw std::runtime_error("GC/text method declaration is not its owned source");}
        return authored;
    }
    Evaluation NativeGcTextMethod(std::uint16_t index,RuntimeObject* receiver,const std::vector<Evaluation>& arguments,
        const Function* declaration,QuestVr::Vm::Execution& execution) {
        const auto authored=NativeGcTextDeclaration(index,declaration);
        if(index==1469u)RequireNativeWindow(receiver);else ValidateNativeGcContextFields(receiver);
        if(index==1242u) {
            if(arguments.size()!=1u)throw std::runtime_error("GC.SetFont requires one Font input");
            const auto& input=arguments.front();
            if(input.reference||input.disposition!=QuestVr::Vm::ResultDisposition::Written)
                throw std::runtime_error("GC.SetFont requires its copied Object temporary");
            const auto value=TypedLiveValue(authored->variables.front(),input.value.kind==Kind::Nothing?
                Value::Text(Kind::Object,{}):input.value);
            if(!value.text.empty())RequireNativeFontAsset(Object(value.text));
            //10002DC0/10027640: both stores are unconditional, including None.
            NativeGcWrite(receiver,"normalFont",value,execution);
            NativeGcWrite(receiver,"boldFont",value,execution);return {};
        }
        if(index==1281u) {
            if(arguments.size()>1u)throw std::runtime_error("GC.GetFontHeight accepts one optional Bool");
            const auto value=arguments.empty()?Value::Bool(false):arguments.front().value;
            if(!arguments.empty()&&(arguments.front().reference||
                arguments.front().disposition!=QuestVr::Vm::ResultDisposition::Written))
                throw std::runtime_error("GC.GetFontHeight requires its copied Bool temporary");
            if(value.kind!=Kind::Bool&&value.kind!=Kind::Nothing)
                throw std::runtime_error("GC.GetFontHeight spacing input is not Bool");
            NativeTextGcOperations operations{*this,receiver};
            return {Value::Float(QuestVr::NativeTextMeasurement::GetFontHeight(operations,value.kind==Kind::Bool&&value.boolean)),{}};
        }
        if(arguments.size()!=(index==1280u?4u:1u))throw std::runtime_error("GC/text native argument count is invalid");
        const auto stringSlot=index==1280u?3u:0u;
        const auto& input=arguments[stringSlot];
        if(input.reference||input.disposition!=QuestVr::Vm::ResultDisposition::Written||
            (input.value.kind!=Kind::String&&input.value.kind!=Kind::Nothing))
            throw std::runtime_error("GC/text native requires its copied String temporary");
        float width{};
        if(index==1280u) {
            if(arguments[0u].reference||arguments[0u].disposition!=QuestVr::Vm::ResultDisposition::Written||
                (arguments[0u].value.kind!=Kind::Float&&arguments[0u].value.kind!=Kind::Nothing))
                throw std::runtime_error("GC.GetTextExtent requires its copied Float destination width");
            width=arguments[0u].value.kind==Kind::Float?arguments[0u].value.floating:0.0f;
            for(const auto slot:{1u,2u})if(arguments[slot].disposition!=QuestVr::Vm::ResultDisposition::Written||
                (arguments[slot].value.kind!=Kind::Float&&arguments[slot].value.kind!=Kind::Nothing)||
                (arguments[slot].reference&&(arguments[slot].reference->zero.kind!=Kind::Float||!arguments[slot].reference->write)))
                throw std::runtime_error("GC.GetTextExtent output is not Float storage");
        }
        const auto bytes=QuestVr::DecalStateDetail::Add(sizeof(NativeTextGcOperations)+sizeof(std::u16string),
            QuestVr::DecalStateDetail::Multiply(QuestVr::DecalStateDetail::Add(input.value.text.size(),1u),16u));
        QuestVr::NativeScratchScope scratch(nativeQueryScratchBytes_,bytes,[this](std::size_t extra){ValidateStateBudget(extra);});
        for(std::size_t position=0u;position<input.value.text.size();++position)LifecycleStep();
        auto text=QuestVr::UnrealString::Decode(input.value.text);
        if(index==1469u) {
            QuestVr::NativeTextContent::ConvertScriptString(text,[this]{LifecycleStep();});
            return {Value::Text(Kind::String,QuestVr::UnrealString::Encode(text)),{}};
        }
        NativeTextGcOperations operations{*this,receiver};
        const auto output=[&](std::size_t slot,float value) {
            // Machine already guards captured native OUT aliases. Wrapping
            // again in WriteReference would double-journal/budget the store.
            if(arguments[slot].reference)arguments[slot].reference->write(Value::Float(value));
        };
        QuestVr::NativeTextMeasurement::GetTextExtentTo(operations,text,width,
            [&](float value){output(1u,value);},[&](float value){output(2u,value);});
        return {};
    }
