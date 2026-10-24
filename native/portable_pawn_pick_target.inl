// Included inside PortableActorVmHost. Original Pawn.PickTarget531 selection
// prefix; a qualifying visibility query still explicitly refuses until the
// internal retail Pawn.LineOfSightTo(candidate,0,false) service is bound.
    std::shared_ptr<const Function> PawnPickTargetDeclaration(const Function* supplied) {
        const std::string path="Engine.Pawn.PickTarget";auto* method=Object(path);const auto authored=FunctionFor(method);
        const auto validate=[&](const Function& function) {
            if(function.path!=path || function.source!=method->sourcePath || function.nativeIndex!=531u ||
                function.flags!=0x401u || function.variables.size()!=5u || !method->script ||
                method->outer!=Object("Engine.Pawn") || method->script->baseField ||
                method->script->nativeIndex!=531u || method->script->functionFlags!=0x401u ||
                method->script->logicalSize!=20u || method->script->bytecode.size()!=20u ||
                function.bytecode!=method->script->bytecode ||
                LowerAscii(Qualified(Table(method->sourcePath),Table(method->sourcePath).exports.at(method->exportIndex).ObjClass))!="core.function")
                throw std::runtime_error("PickTarget531 lacks its actual original Pawn UFunction");
            constexpr std::array<const char*,5u> names{"bestAim","bestDist","FireDir","projStart","ReturnValue"};
            RuntimeBytecodeReader code(function.bytecode);
            for(std::size_t slot=0u;slot<names.size();++slot) {
                const auto& p=function.variables[slot];auto* field=Object(p.key);
                const auto flags=slot<2u?0x180u:slot<4u?0x80u:0x580u;
                const auto kind=slot<2u?Kind::Float:slot<4u?Kind::Vector:Kind::Object;
                if(p.key!=path+'.'+names[slot] || p.name!=names[slot] || p.flags!=flags || p.zero.kind!=kind ||
                    p.arrayDimension!=1u || !field->property || field->outer!=method ||
                    field->property->flags!=flags || field->property->arrayDimension!=1 || field->property->baseField ||
                    (slot<4u && (code.Byte()!=0x29u || Qualified(Table(method->sourcePath),static_cast<std::int32_t>(code.Dword()))!=p.key)))
                    throw std::runtime_error("PickTarget531 differs from its original ordered NativeParm packet");
                if(kind==Kind::Vector) {
                    NativeCoordsVectorSchema(field);
                    if(p.resultLayout!=QuestVr::Vm::ResultLayout::CoreVector12)
                        throw std::runtime_error("PickTarget531 Vector lacks its original twelve-byte layout");
                } else {
                    const auto type=slot<2u?"FloatProperty":"ObjectProperty";
                    if(field->property->type!=type || p.resultLayout!=QuestVr::Vm::ResultLayout::Unknown ||
                        LowerAscii(Qualified(Table(field->sourcePath),Table(field->sourcePath).exports.at(field->exportIndex).ObjClass))!=
                            std::string("core.")+LowerAscii(type) ||
                        (slot<2u?field->property->referencedType!=0:
                            Qualified(Table(field->sourcePath),field->property->referencedType)!="Engine.Pawn"))
                        throw std::runtime_error("PickTarget531 Float/Pawn result has a foreign native schema");
                }
            }
        };
        validate(*authored);
        if(supplied) {
            if(supplied->path!=path || supplied->source!=authored->source || supplied->nativeIndex!=531u || supplied->flags!=0x401u)
                throw std::runtime_error("PickTarget531 has a foreign declaration identity");
            if(!supplied->variables.empty() || !supplied->bytecode.empty())validate(*supplied);
        }
        return authored;
    }
    RuntimeObject* PawnPickTargetSource(const std::string& receiver) {
        auto* pawn=Object(receiver);RequireMapActor(pawn);
        if(!IsDerivedFromPath(pawn->cls,"Engine.Pawn"))
            throw std::runtime_error("PickTarget531 receiver is not an actual map Pawn");
        static_cast<void>(NativeLevelInfo(pawn));return pawn;
    }
    RuntimeObject* PawnPickTargetLink(RuntimeObject* owner,const char* key,RuntimeObject* source) {
        auto* pawn=NoiseObjectField(owner,key,2u,"Engine.Pawn");
        if(pawn) {
            RequireMapActor(pawn);
            if(pawn->nativeXLevel!=source->nativeXLevel)
                throw std::runtime_error("PickTarget531 PawnList link leaves its actual native Level");
        }
        return pawn;
    }
    std::int32_t PawnPickTargetHealth(RuntimeObject* pawn) {
        const auto property=DescribeProperty("Engine.Pawn.Health");auto* field=Object(property.key);
        if(property.key!="Engine.Pawn.Health" || !OwnsProperty(pawn->cls,property) ||
            property.zero.kind!=Kind::Int || property.arrayDimension!=1u || property.flags!=0x10021u ||
            !field->property || field->property->type!="IntProperty" || field->property->baseField ||
            field->property->referencedType || field->outer!=Object("Engine.Pawn") ||
            LowerAscii(Qualified(Table(field->sourcePath),Table(field->sourcePath).exports.at(field->exportIndex).ObjClass))!="core.intproperty")
            throw std::runtime_error("PickTarget531 Health lacks its actual declared Int schema");
        const auto value=Read(pawn,property,0u);
        if(value.kind!=Kind::Int)throw std::runtime_error("PickTarget531 live Health is not Int");
        return value.integer;
    }
    bool PawnPickTargetLineOfSight(RuntimeObject* source,RuntimeObject* candidate) {
        static_cast<void>(source);static_cast<void>(candidate);
        // This is the internal C++ helper, NOT selected script/native514.
        // It has visibility/range/eye/enemy/corner rays and possible writes.
        // A simple FastTrace or false result would silently change autoaim.
        throw std::runtime_error("Original PickTarget531 requires internal Pawn.LineOfSightTo(candidate,0,false)");
    }
    Evaluation PawnPickTargetNative(const std::string& receiver,const std::vector<Evaluation>& arguments,
        const Function* declaration) {
        static_cast<void>(PawnPickTargetDeclaration(declaration));auto* source=PawnPickTargetSource(receiver);
        if(arguments.size()!=4u)throw std::runtime_error("PickTarget531 requires all four original expressions");
        std::array<float,2u> temporary{};
        for(std::size_t slot=0u;slot<arguments.size();++slot) {
            const auto& argument=arguments[slot];
            if(argument.disposition!=QuestVr::Vm::ResultDisposition::Written ||
                argument.value.kind!=(slot<2u?Kind::Float:Kind::Vector) ||
                (slot>=2u && argument.reference) ||
                (slot<2u && argument.reference &&
                    (argument.reference->zero.kind!=Kind::Float || !argument.reference->read || !argument.reference->write)))
                throw std::runtime_error("PickTarget531 lacks its copied Float temporary/live OUT or complete copied Vector");
            if(slot<2u)temporary[slot]=argument.value.floating;
        }
        const auto vector=[](const Value& value){return PortableModelVec3{value.vector[0u],value.vector[1u],value.vector[2u]};};
        const auto direction=vector(arguments[2u].value),start=vector(arguments[3u].value);
        const auto readAim=[&] {
            const auto value=arguments[0u].reference?arguments[0u].reference->read():Value::Float(temporary[0u]);
            if(value.kind!=Kind::Float)throw std::runtime_error("PickTarget531 live bestAim is not Float");
            return value.floating;
        };
        const auto write=[&](std::size_t slot,float value) {
            if(arguments[slot].reference)arguments[slot].reference->write(Value::Float(value));
            else temporary[slot]=value; // own initialized native fallback, not a VM property
        };
        constexpr std::size_t maximumPawns=4096u;
        QuestVr::NativeScratchScope scratch(nativeQueryScratchBytes_,maximumPawns*sizeof(RuntimeObject*),
            [this](auto extra){ValidateStateBudget(extra);});
        std::array<RuntimeObject*,maximumPawns> visited{};std::size_t count{};RuntimeObject* best{};
        auto* info=NativeLevelInfo(source);
        for(auto* pawn=PawnPickTargetLink(info,"Engine.LevelInfo.PawnList",source);pawn;
            pawn=PawnPickTargetLink(pawn,"Engine.Pawn.nextPawn",source)) {
            LifecycleStep();
            if(count==maximumPawns || std::find(visited.begin(),visited.begin()+count,pawn)!=visited.begin()+count)
                throw std::runtime_error("PickTarget531 PawnList cycles or exceeds its bounded traversal");
            visited[count++]=pawn;
            if(pawn==source || PawnPickTargetHealth(pawn)<=0 ||
                !RetailSingleLineFlag(pawn,QuestVr::OriginalSingleLineFlag::ProjTarget))continue;
            auto* game=NoiseObjectField(NativeLevelInfo(source),"Engine.LevelInfo.Game",0u,"Engine.GameInfo");
            if(!game)continue;
            auto* ours=NoiseObjectField(source,"Engine.Pawn.PlayerReplicationInfo",0x20u,"Engine.PlayerReplicationInfo");
            if(ours) {
                auto* theirs=NoiseObjectField(pawn,"Engine.Pawn.PlayerReplicationInfo",0x20u,"Engine.PlayerReplicationInfo");
                if(theirs && NoiseField(game,"Engine.GameInfo.bTeamGame",Kind::Bool,1u).boolean &&
                    NoiseField(ours,"Engine.PlayerReplicationInfo.Team",Kind::Byte,0x20u).integer==
                    NoiseField(theirs,"Engine.PlayerReplicationInfo.Team",Kind::Byte,0x20u).integer)continue;
            }
            const auto candidate=QuestVr::EvaluateOriginalPawnPickTargetCandidate(NoiseLocation(pawn),start,direction,readAim);
            if(!candidate || !PawnPickTargetLineOfSight(source,pawn))continue;
            best=pawn;write(0u,candidate->aim);write(1u,candidate->distance);
        }
        auto result=Value::Text(Kind::Object,best?best->reflection.objectPath:std::string{});
        ValidateObjectValue(result,"Engine.Pawn",false);return {std::move(result),{}};
    }
