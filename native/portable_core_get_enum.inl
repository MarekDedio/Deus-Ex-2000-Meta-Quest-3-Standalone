// Included inside PortableActorVmHost. Original static UObject.GetEnum is
// declaration-specific native0; no key-name list or lazy Enum construction.
    std::shared_ptr<const Function> CoreGetEnumDeclaration(const Function* supplied) {
        const std::string path="Core.Object.GetEnum";
        auto* method=Object(path);const auto authored=FunctionFor(method);
        const auto validate=[&](const Function& function) {
            if(function.path!=path||function.source!=method->sourcePath||function.flags!=0x2401u||
                function.nativeIndex!=0u||function.variables.size()!=3u||!method->script||
                method->outer!=Object("Core.Object")||method->script->baseField||
                method->script->nativeIndex!=0u||method->script->functionFlags!=0x2401u||
                method->script->logicalSize!=10u||method->script->bytecode.size()!=10u||
                function.bytecode!=method->script->bytecode||
                LowerAscii(Qualified(Table(method->sourcePath),Table(method->sourcePath).exports.at(method->exportIndex).ObjClass))!="core.function")
                throw std::runtime_error("GetEnum lacks its actual original Core UFunction");
            constexpr std::array<const char*,3u> names{"E","i","ReturnValue"};
            constexpr std::array<Kind,3u> kinds{Kind::Object,Kind::Int,Kind::Name};
            constexpr std::array<const char*,3u> types{"ObjectProperty","IntProperty","NameProperty"};
            RuntimeBytecodeReader code(function.bytecode);
            for(std::size_t slot=0u;slot<names.size();++slot) {
                const auto& parameter=function.variables[slot];auto* field=Object(parameter.key);
                const auto flags=slot==2u?0x580u:0x80u;
                if(parameter.key!=path+'.'+names[slot]||parameter.name!=names[slot]||parameter.flags!=flags||
                    parameter.arrayDimension!=1u||parameter.zero.kind!=kinds[slot]||!field->property||
                    field->outer!=method||field->property->flags!=flags||field->property->type!=types[slot]||
                    (slot==0u&&Qualified(Table(field->sourcePath),field->property->referencedType)!="Core.Object")||
                    (slot<2u&&(code.Byte()!=0x29u||
                        Qualified(Table(method->sourcePath),static_cast<std::int32_t>(code.Dword()))!=parameter.key)))
                    throw std::runtime_error("GetEnum differs from its original NativeParm/result packet");
            }
        };
        validate(*authored);
        if(supplied) {
            if(supplied->path!=path||supplied->source!=authored->source||supplied->flags!=authored->flags||supplied->nativeIndex!=0u)
                throw std::runtime_error("GetEnum has a foreign native-zero declaration identity");
            if(!supplied->variables.empty()||!supplied->bytecode.empty())validate(*supplied);
        }
        return authored;
    }
    const PortableEnumDescriptor& BorrowNativeEnum(RuntimeObject* object) {
        if(!persistentRuntime||!persistentMetadataClasses)
            throw std::runtime_error("Runtime Enum query requires the actual registered Core.Enum class");
        auto* cls=Object("Core.Enum");auto* within=Object("Core.Struct");
        const auto imported=object->nativeMetadataClassImport;
        if(!imported||imported>QuestVr::NativeMetadataClasses.size()||
            QuestVr::NativeMetadataClasses[imported-1u].path!="Core.Enum"||
            cls->nativeMetadataClass!=&QuestVr::NativeMetadataClasses[imported-1u]||
            object->cls!=cls||cls->nativeClassWithin!=within||Object(object->reflection.objectPath)!=object||
            !object->inventoryTableMetadata||!object->inventoryClassReferenceValid||
            object->defaultOwner||object->runtimeBirth||!object->enumDescriptor||
            !object->outer||!IsDerivedFromPath(object->outer->cls,"Core.Struct")||
            !object->objectIndex||!object->outer->objectIndex||!cls->objectIndex)
            throw std::runtime_error("Runtime Enum query lacks genuine source/Class/UStruct Outer authority");
        return *object->enumDescriptor;
    }
    Evaluation CoreGetEnumNative(const std::vector<Evaluation>& arguments,const Function* declaration) {
        static_cast<void>(CoreGetEnumDeclaration(declaration));
        if(arguments.size()!=2u)throw std::runtime_error("GetEnum requires its two original expressions");
        for(const auto& argument:arguments)
            if(argument.reference||argument.disposition!=QuestVr::Vm::ResultDisposition::Written)
                throw std::runtime_error("GetEnum requires independently copied initialized inputs");
        const auto& enumeration=arguments[0u].value;const auto& index=arguments[1u].value;
        if((enumeration.kind!=Kind::Object&&enumeration.kind!=Kind::Nothing)||
            (index.kind!=Kind::Int&&index.kind!=Kind::Nothing))
            throw std::runtime_error("GetEnum requires Object and signed Int inputs");
        LifecycleStep();
        if(enumeration.kind==Kind::Nothing||enumeration.text.empty())return {Value::Text(Kind::Name,"None"),{}};
        auto* object=Object(enumeration.text);
        // Retail IsA walks the actual current UObject.Class chain. An indexed
        // map Enum need not exist in a newly reopened file's older export table
        // (the owned late-linker fixture deliberately supplies newer tables).
        // Only the not-yet-bound source identity needs the existing resolver.
        bool isEnum{};
        if(object->cls)isEnum=IsDerivedFromPath(object->cls,"Core.Enum");
        else {
            const auto identity=CastObjectIdentity(object->reflection.objectPath);
            isEnum=!identity.classObject&&ClassDerives(identity.classPath,"Core.Enum");
        }
        if(!isEnum)return {Value::Text(Kind::Name,"None"),{}};
        const auto selected=index.kind==Kind::Int?index.integer:0;
        LifecycleStep();
        if(selected<0)return {Value::Text(Kind::Name,"None"),{}};
        const auto& names=BorrowNativeEnum(object).names;
        if(static_cast<std::size_t>(selected)>=names.size())return {Value::Text(Kind::Name,"None"),{}};
        const auto& name=names[static_cast<std::size_t>(selected)].ToString();
        const auto bytes=QuestVr::DecalStateDetail::Add(sizeof(Value),
            QuestVr::DecalStateDetail::Multiply(QuestVr::DecalStateDetail::Add(name.size(),1u),2u));
        QuestVr::NativeScratchScope scratch(nativeQueryScratchBytes_,bytes,[this](std::size_t extra){ValidateStateBudget(extra);});
        for(std::size_t position=0u;position<name.size();++position)LifecycleStep();
        return {Value::Text(Kind::Name,name),{}};
    }
