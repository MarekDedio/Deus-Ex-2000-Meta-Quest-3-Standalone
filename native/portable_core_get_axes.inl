// Included inside PortableActorVmHost. Core229 is static UObject math, not an
// Actor/Level/player service. OUT references have already been guarded by VM.
    std::shared_ptr<const Function> CoreGetAxesDeclaration(const Function* supplied) {
        const std::string path="Core.Object.GetAxes";
        auto* method=Object(path);const auto authored=FunctionFor(method);
        const auto validate=[&](const Function& function) {
            if(function.path!=path || function.source!=method->sourcePath || function.flags!=0x2401u ||
                function.nativeIndex!=229u || function.variables.size()!=4u || !method->script ||
                method->outer!=Object("Core.Object") || method->script->baseField ||
                method->script->nativeIndex!=229u || method->script->functionFlags!=0x2401u ||
                method->script->logicalSize!=20u || method->script->bytecode.size()!=20u ||
                function.bytecode!=method->script->bytecode ||
                LowerAscii(Qualified(Table(method->sourcePath),Table(method->sourcePath).exports.at(method->exportIndex).ObjClass))!="core.function")
                throw std::runtime_error("GetAxes229 lacks its actual original Core UFunction");
            constexpr std::array<const char*,4u> names{"A","X","Y","Z"};
            RuntimeBytecodeReader code(function.bytecode);
            for(std::size_t slot=0u;slot<names.size();++slot) {
                const auto& parameter=function.variables[slot];auto* field=Object(parameter.key);
                const auto flags=slot==0u?0x80u:0x180u;
                const auto layout=slot==0u?QuestVr::Vm::ResultLayout::CoreRotator12:QuestVr::Vm::ResultLayout::CoreVector12;
                if(parameter.key!=path+'.'+names[slot] || parameter.name!=names[slot] ||
                    parameter.flags!=flags || parameter.arrayDimension!=1u ||
                    parameter.zero.kind!=(slot==0u?Kind::Rotator:Kind::Vector) || parameter.resultLayout!=layout ||
                    !field->property || field->outer!=method || field->property->flags!=flags ||
                    code.Byte()!=0x29u || Qualified(Table(method->sourcePath),static_cast<std::int32_t>(code.Dword()))!=parameter.key)
                    throw std::runtime_error("GetAxes229 differs from its original ordered NativeParm packet");
                if(slot==0u)NativeCoordsRotatorSchema(field);else NativeCoordsVectorSchema(field);
            }
        };
        validate(*authored);
        if(supplied) {
            if(supplied->path!=path || supplied->source!=authored->source || supplied->flags!=authored->flags ||
                supplied->nativeIndex!=229u)
                throw std::runtime_error("GetAxes229 has a foreign declaration identity");
            // Resolution supplies a minimal immutable identity before Prepare.
            // A materialized declaration must match the complete source packet.
            if(!supplied->variables.empty() || !supplied->bytecode.empty())validate(*supplied);
        }
        return authored;
    }
    Evaluation CoreGetAxesNative(const std::vector<Evaluation>& arguments,const Function* declaration) {
        static_cast<void>(CoreGetAxesDeclaration(declaration));
        if(arguments.size()!=4u)throw std::runtime_error("GetAxes229 requires all four original expressions");
        if(arguments[0u].reference || arguments[0u].disposition!=QuestVr::Vm::ResultDisposition::Written ||
            arguments[0u].value.kind!=Kind::Rotator)
            throw std::runtime_error("GetAxes229 requires its fully copied Rotator input");
        const auto& a=arguments[0u].value.rotation;
        // Canonical original constructor values and the explicitly audited
        // sine profile; no libm, forward Rotate(A), normalization or VR aim.
        const QuestVr::OriginalModelCoords unit{{0,0,0},{1,0,0},{0,1,0},{0,0,1}};
        const auto axes=QuestVr::ComputeOriginalGetAxes({a[0u],a[1u],a[2u]},unit,
            QuestVr::OriginalSineProfile::Table(),[this]{LifecycleStep();});
        const std::array<PortableModelVec3,3u> values{axes.xAxis,axes.yAxis,axes.zAxis};
        for(std::size_t slot=0u;slot<values.size();++slot) {
            const auto& argument=arguments[slot+1u];
            if(!argument.reference)continue; // separate disposable local output
            if(argument.disposition!=QuestVr::Vm::ResultDisposition::Written ||
                argument.reference->resultLayout!=QuestVr::Vm::ResultLayout::CoreVector12 || !argument.reference->write)
                throw std::runtime_error("GetAxes229 output lacks its original typed writable destination");
            const auto& value=values[slot];
            // GuardReference already journals/counts/poisons this write. Do NOT
            // double-wrap it in execution.WriteReference. Aliases: X then Y then Z.
            argument.reference->write(Value::Vector({value.x,value.y,value.z}));
        }
        return {};
    }
