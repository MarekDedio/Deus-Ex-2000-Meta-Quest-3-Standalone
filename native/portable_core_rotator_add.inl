// Included inside PortableActorVmHost. Core316 is a UObject operator: no
// Actor-only/current-Level/hash/clock/startup gate belongs to its input ABI.
    std::shared_ptr<const Function> CoreRotatorAddDeclaration(const Function* supplied) {
        const std::string path="Core.Object.Add_RotatorRotator";
        auto* method=Object(path);const auto authored=FunctionFor(method);
        const auto validate=[&](const Function& function){
            if(function.path!=path || function.source!=authored->source || function.flags!=0x3401u ||
                function.nativeIndex!=316u || function.variables.size()!=3u || !method->script ||
                method->outer!=Object("Core.Object") ||
                LowerAscii(Qualified(Table(method->sourcePath),Table(method->sourcePath).exports.at(method->exportIndex).ObjClass))!="core.function")
                throw std::runtime_error("Rotator addition lacks its actual original Core UFunction");
            constexpr std::array<const char*,3u> names{"A","B","ReturnValue"};
            for(std::size_t slot=0u;slot<names.size();++slot){
                const auto& parameter=function.variables[slot];auto* field=Object(parameter.key);
                const auto flags=slot==2u?0x580u:0x80u;
                if(parameter.key!=path+'.'+names[slot] || parameter.name!=names[slot] ||
                    parameter.flags!=flags || parameter.arrayDimension!=1u || parameter.zero.kind!=Kind::Rotator ||
                    parameter.resultLayout!=QuestVr::Vm::ResultLayout::CoreRotator12 ||
                    !field->property || field->outer!=method || field->property->flags!=flags)
                    throw std::runtime_error("Rotator addition parameter/result differs from its original scalar packet");
                NativeCoordsRotatorSchema(field);
            }
        };
        validate(*authored);if(supplied)validate(*supplied);
        return authored;
    }
