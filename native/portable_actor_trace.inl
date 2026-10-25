// Included inside PortableActorVmHost after coordinate/schema/query helpers.
// Original Actor.Trace277 wrapper, not aiming policy, a hit/damage shortcut,
// a new collision world, or another Execute root.
    std::shared_ptr<const Function> ActorTraceDeclaration(const Function* supplied) {
        const std::string path="Engine.Actor.Trace";auto* method=Object(path);const auto authored=FunctionFor(method);
        const auto validate=[&](const Function& function) {
            if(function.path!=path || function.source!=method->sourcePath || function.nativeIndex!=277u ||
                function.flags!=0x401u || function.variables.size()!=7u || !method->script ||
                method->outer!=Object("Engine.Actor") || method->script->baseField ||
                method->script->nativeIndex!=277u || method->script->functionFlags!=0x401u ||
                method->script->logicalSize!=30u || method->script->bytecode.size()!=30u ||
                function.bytecode!=method->script->bytecode ||
                LowerAscii(Qualified(Table(method->sourcePath),Table(method->sourcePath).exports.at(method->exportIndex).ObjClass))!="core.function")
                throw std::runtime_error("Trace277 lacks its actual original Actor UFunction");
            constexpr std::array<const char*,7u> names{"HitLocation","HitNormal","TraceEnd","TraceStart","bTraceActors","Extent","ReturnValue"};
            RuntimeBytecodeReader code(function.bytecode);
            for(std::size_t slot=0u;slot<names.size();++slot) {
                const auto& p=function.variables[slot];auto* field=Object(p.key);
                const auto flags=slot<2u?0x180u:slot==2u?0x80u:slot==6u?0x580u:0x90u;
                const auto kind=slot==4u?Kind::Bool:slot==6u?Kind::Object:Kind::Vector;
                if(p.key!=path+'.'+names[slot] || p.name!=names[slot] || p.flags!=flags || p.zero.kind!=kind ||
                    p.arrayDimension!=1u || !field->property || field->outer!=method || field->property->flags!=flags ||
                    field->property->arrayDimension!=1 || field->property->baseField ||
                    (slot<6u && (code.Byte()!=0x29u || Qualified(Table(method->sourcePath),static_cast<std::int32_t>(code.Dword()))!=p.key)))
                    throw std::runtime_error("Trace277 differs from its original ordered NativeParm packet");
                if(kind==Kind::Vector) {
                    NativeCoordsVectorSchema(field);
                    if(p.resultLayout!=QuestVr::Vm::ResultLayout::CoreVector12)
                        throw std::runtime_error("Trace277 Vector lacks its original twelve-byte layout");
                } else {
                    const auto type=slot==4u?"BoolProperty":"ObjectProperty";
                    if(field->property->type!=type || p.resultLayout!=QuestVr::Vm::ResultLayout::Unknown ||
                        LowerAscii(Qualified(Table(field->sourcePath),Table(field->sourcePath).exports.at(field->exportIndex).ObjClass))!=
                            std::string("core.")+LowerAscii(type) ||
                        (slot==4u?field->property->referencedType!=0:
                            Qualified(Table(field->sourcePath),field->property->referencedType)!="Engine.Actor"))
                        throw std::runtime_error("Trace277 optional Bool/Actor result has a foreign native schema");
                }
            }
        };
        validate(*authored);
        if(supplied) {
            if(supplied->path!=path || supplied->source!=authored->source || supplied->nativeIndex!=277u || supplied->flags!=0x401u)
                throw std::runtime_error("Trace277 has a foreign declaration identity");
            if(!supplied->variables.empty() || !supplied->bytecode.empty())validate(*supplied);
        }
        return authored;
    }
    RuntimeObject* ActorTraceSource(const std::string& receiver) {
        return RetailCollisionActor(reinterpret_cast<std::uintptr_t>(Object(receiver)));
    }
    Value ActorTraceSeed(RuntimeObject* actor,std::size_t slot) {
        if(slot==3u)return NativeCoordsRead(actor,"Engine.Actor.Location",Kind::Vector,0x23u);
        if(slot==4u) {
            // Independent Trace/SetCollision static audit: Actor+1DC mask1.
            const auto property=RetailCollisionSetterField(actor,"Engine.Actor.bCollideActors");
            const auto value=Read(actor,property,0u);
            if(value.kind!=Kind::Bool)throw std::runtime_error("Trace277 live bCollideActors seed is not Bool");
            return value;
        }
        if(slot==5u)return Value::Vector({});
        throw std::runtime_error("Trace277 has no initialized temporary at that slot");
    }
    Evaluation ActorTraceNative(const std::string& receiver,const std::vector<Evaluation>& arguments,const Function* declaration,
        QuestVr::Vm::Execution& execution) {
        static_cast<void>(ActorTraceDeclaration(declaration));auto* source=ActorTraceSource(receiver);
        if(arguments.size()<3u || arguments.size()>6u || arguments[2u].reference ||
            arguments[2u].disposition!=QuestVr::Vm::ResultDisposition::Written || arguments[2u].value.kind!=Kind::Vector)
            throw std::runtime_error("Trace277 requires its original two OUT expressions and fully copied TraceEnd");
        const auto& end=arguments[2u].value;
        std::array<Value,3u> optional;
        for(std::size_t slot=3u;slot<6u;++slot) {
            // Supplied EX_Nothing already consumed its seed at its own Step.
            // Only missing trailing placeholders reach this fallback; with no
            // intervening expression, these live seed reads retain native order.
            auto value=slot<arguments.size()?arguments[slot].value:Value{};
            if(slot<arguments.size() && arguments[slot].reference)
                throw std::runtime_error("Trace277 optional input was not copied at its own Step");
            if(slot<arguments.size() && value.kind!=Kind::Nothing &&
                arguments[slot].disposition!=QuestVr::Vm::ResultDisposition::Written)
                throw std::runtime_error("Trace277 optional input lacks its complete materialized result");
            if(value.kind==Kind::Nothing)value=ActorTraceSeed(source,slot);
            if(value.kind!=(slot==4u?Kind::Bool:Kind::Vector))
                throw std::runtime_error("Trace277 optional input differs from its initialized native destination");
            optional[slot-3u]=std::move(value);
        }
        const auto vector=[](const Value& v){return PortableModelVec3{v.vector[0u],v.vector[1u],v.vector[2u]};};
        auto* level=RetailCollisionLevel();static_cast<void>(ZoneActor(reinterpret_cast<std::uintptr_t>(source),level));
        QuestVr::OriginalActorLineHit hit{};
        static_cast<void>(RetailSingleLineLevel(level,source,hit,vector(end),vector(optional[0u]),vector(optional[2u]),
            optional[1u].boolean?0x37u:0x06u,0u));
        auto* actor=hit.actor?ZoneActor(hit.actor,level):nullptr;
        auto result=Value::Text(Kind::Object,actor?actor->reflection.objectPath:std::string{});
        ValidateObjectValue(result,"Engine.Actor",false); // Actor instance, not a UClass value
        const auto observation=RecordStartupWeaponFireTrace(source,actor,vector(optional[0u]),vector(end),
            vector(optional[2u]),optional[1u].boolean,hit,execution.WriteCount());
        const std::array<PortableModelVec3,2u> outputs{hit.location,hit.normal};
        try { for(std::size_t slot=0u;slot<outputs.size();++slot) {
            const auto& argument=arguments[slot];if(!argument.reference)continue;
            if(argument.disposition!=QuestVr::Vm::ResultDisposition::Written ||
                argument.reference->resultLayout!=QuestVr::Vm::ResultLayout::CoreVector12 || !argument.reference->write)
                throw std::runtime_error("Trace277 output lacks its typed writable destination");
            const auto& v=outputs[slot];
            argument.reference->write(Value::Vector({v.x,v.y,v.z})); // already guarded/journaled, Location then Normal
        }} catch(...) {
            if(observation)startupFireTraceRow_->traces.at(*observation).writesAfterOutputs=execution.WriteCount();
            throw;
        }
        if(observation) {
            auto& recorded=startupFireTraceRow_->traces.at(*observation);
            recorded.writesAfterOutputs=execution.WriteCount();recorded.outputsComplete=true;
        }
        return {std::move(result),{}};
    }
