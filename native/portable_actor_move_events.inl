// Included inside PortableActorVmHost. Authenticated native movement packets
// join the SAME caller Execution/root. This does not implement MoveActor or
// bypass dormant-world/probe/deletion suppression in ordinary ProcessEvent.
// Owned static evidence: training-move-actor-20261019/original-bump-schemas.log,
// training-move-smooth-20261019/original-hitwall-schema.log and
// training-level-encroachment-20261019/original-callback-schemas.log.
    enum class MoveSelectedEventKind { BumpWall,Bump,HitWall,EncroachingOn,EncroachedBy };
    RuntimeObject* MoveEventActor(RuntimeObject* actor) {
        const auto identity=reinterpret_cast<std::uintptr_t>(actor);
        if(!identity||!persistentObjectLookup||!persistentObjectLookup->active)
            throw std::runtime_error("Original movement event requires actual Actor lookup authority");
        const auto* record=persistentObjectLookup->index.RecordForIdentity(identity);
        if(!record)throw std::runtime_error("Original movement event retained Actor identity is retired");
        // Authenticate before dereferencing nativeXLevel. This is a lifetime/
        // current-generation safety boundary, NOT bDeleteMe/world-slot logic.
        if(actor->lookupControl!=persistentObjectLookup||actor->objectIndex.Index()!=record->index||
            actor->classDescriptor||actor->defaultOwner||!actor->cls)
            throw std::runtime_error("Original movement event identity is not its actual Actor instance");
        auto* level=ZoneLevel(reinterpret_cast<std::uintptr_t>(actor->nativeXLevel));
        return ZoneActor(identity,level); // No implicit Actors[0]/Hash read.
    }
    void MoveEventDeclaration(RuntimeObject* receiver,const Function& function,
        MoveSelectedEventKind selected,bool base) {
        const char* name{};std::array<const char*,3u> names{};
        std::array<Kind,3u> kinds{};unsigned count{};
        switch(selected){
        case MoveSelectedEventKind::BumpWall:
            name="BumpWall";names={"hitlocation","hitnormal",nullptr};kinds={Kind::Vector,Kind::Vector,Kind::Nothing};count=2u;break;
        case MoveSelectedEventKind::Bump:
            name="Bump";names={"other",nullptr,nullptr};kinds={Kind::Object,Kind::Nothing,Kind::Nothing};count=1u;break;
        case MoveSelectedEventKind::HitWall:
            name="HitWall";names={"hitnormal","hitwall",nullptr};kinds={Kind::Vector,Kind::Object,Kind::Nothing};count=2u;break;
        case MoveSelectedEventKind::EncroachingOn:
            name="EncroachingOn";names={"other","returnvalue",nullptr};kinds={Kind::Object,Kind::Bool,Kind::Nothing};count=2u;break;
        case MoveSelectedEventKind::EncroachedBy:
            name="EncroachedBy";names={"other",nullptr,nullptr};kinds={Kind::Object,Kind::Nothing,Kind::Nothing};count=1u;break;
        default:throw std::runtime_error("Unknown original movement event declaration");
        }
        const auto expected=LowerAscii(std::string("Engine.Actor.")+name);
        auto* method=Object(function.path);
        if(!method->script||!method->outer||function.source!=method->sourcePath||
            LowerAscii(Qualified(Table(method->sourcePath),Table(method->sourcePath).exports.at(method->exportIndex).ObjClass))!="core.function")
            throw std::runtime_error("Original movement event has no actual authored UFunction identity");
        auto* owner=method->outer;
        if(owner->stateDescriptor)owner=owner->outer;
        if(!owner||!owner->classDescriptor||owner->defaultOwner||
            !IsDerivedFromPath(receiver->cls,owner->reflection.objectPath))
            throw std::runtime_error("Selected movement callback is not owned by the receiver's actual class/state ancestry");
        if(base&&(LowerAscii(function.path)!=expected||method->outer!=Object("Engine.Actor")||
            function.nativeIndex!=0u||function.flags!=0x800u||function.variables.size()!=count))
            throw std::runtime_error("Original Actor movement event has a different base native packet");
        unsigned slot{};
        for(const auto& parameter:function.variables){
            LifecycleStep();
            // Derived/state overrides may have arbitrary genuine locals and
            // different parameter spelling, but NOT extra/out/optional parms.
            if(!(parameter.flags&0x80u))continue;
            if(slot==count)throw std::runtime_error("Selected movement callback has extra native parameter/result fields");
            const auto kind=kinds[slot];const bool result=kind==Kind::Bool;
            const auto flags=result?0x580u:0x80u;
            auto* field=Object(parameter.key);
            const char* type=kind==Kind::Vector?"StructProperty":kind==Kind::Object?"ObjectProperty":"BoolProperty";
            if((base&&LowerAscii(parameter.key)!=expected+'.'+names[slot])||parameter.zero.kind!=kind||
                parameter.arrayDimension!=1u||parameter.flags!=flags||!field->property||field->outer!=method||
                field->property->type!=type||field->property->baseField||
                field->property->arrayDimension!=1||field->property->flags!=flags||
                LowerAscii(Qualified(Table(field->sourcePath),Table(field->sourcePath).exports.at(field->exportIndex).ObjClass))!=
                    std::string("core.")+LowerAscii(type))
                throw std::runtime_error("Selected movement callback parameter/result does not match its original scalar packet");
            if(kind==Kind::Vector){
                NativeCoordsVectorSchema(field); // Actual canonical Vector12, not a shape guess.
                if(parameter.resultLayout!=QuestVr::Vm::ResultLayout::CoreVector12)
                    throw std::runtime_error("Movement callback Vector has no authenticated original12-byte result layout");
            }else if(kind==Kind::Object){
                if(!field->property->referencedType||
                    LowerAscii(Qualified(Table(field->sourcePath),field->property->referencedType))!="engine.actor")
                    throw std::runtime_error("Movement callback Object parameter is not constrained to actual Engine.Actor");
            }else if(field->property->referencedType||parameter.zero.boolean){
                throw std::runtime_error("EncroachingOn ReturnValue lacks its original zero-initialized scalar Bool layout");
            }
            ++slot;
        }
        if(slot!=count)throw std::runtime_error("Selected movement callback is missing an original native parameter/result");
    }
    Value MoveSelectedEvent(RuntimeObject* receiver,MoveSelectedEventKind selected,
        PortableModelVec3 first,PortableModelVec3 second,RuntimeObject* other,
        QuestVr::Vm::Execution& execution) {
        if(!transaction_)throw std::runtime_error("Original movement callback requires its existing complete caller VM transaction");
        LifecycleStep();receiver=MoveEventActor(receiver);
        const char* name{};unsigned arguments{};bool objectArgument{};
        switch(selected){
        case MoveSelectedEventKind::BumpWall:name="BumpWall";arguments=2u;break;
        case MoveSelectedEventKind::Bump:name="Bump";arguments=1u;objectArgument=true;break;
        case MoveSelectedEventKind::HitWall:name="HitWall";arguments=2u;objectArgument=true;break;
        case MoveSelectedEventKind::EncroachingOn:name="EncroachingOn";arguments=1u;objectArgument=true;break;
        case MoveSelectedEventKind::EncroachedBy:name="EncroachedBy";arguments=1u;objectArgument=true;break;
        default:throw std::runtime_error("Unknown original selected movement event");
        }
        // Raw packet None is legitimate (notably moveSmooth HitWall). Do not
        // invent an Other!=None, Other!=receiver, active or deletion gate.
        if(objectArgument&&other)other=MoveEventActor(other);
        const auto pathBytes=QuestVr::DecalStateDetail::Add(receiver->reflection.objectPath.size()+1u,
            other&&objectArgument?other->reflection.objectPath.size()+1u:1u);
        const auto scratch=QuestVr::DecalStateDetail::Add(2048u+3u*sizeof(Evaluation),
            QuestVr::DecalStateDetail::Multiply(pathBytes,4u));
        QuestVr::NativeScratchScope reservation(nativeQueryScratchBytes_,scratch,
            [this](std::size_t extra){ValidateStateBudget(extra);});
        const auto base=FunctionFor(Object(std::string("Engine.Actor.")+name));
        MoveEventDeclaration(receiver,*base,selected,true);
        // Original FindFunctionChecked precedes ProcessEvent suppression.
        // Resolve/authenticate the real selected class/state override even
        // when CallEvent subsequently returns Nothing in a dormant world.
        const auto identity=Member(receiver,name);
        if(!identity)throw std::runtime_error("Original movement FindFunctionChecked found no selected event");
        const auto method=FunctionFor(Object(identity->path));
        MoveEventDeclaration(receiver,*method,selected,false);
        const auto vector=[](const PortableModelVec3& raw){
            static_assert(sizeof(raw)==3u*sizeof(float));
            Value value;value.kind=Kind::Vector;
            std::memcpy(value.vector.data(),&raw,sizeof(raw));return value;
        };
        std::vector<Evaluation> packet;packet.reserve(arguments);
        if(selected==MoveSelectedEventKind::BumpWall){
            packet.push_back({vector(first),{}});
            packet.push_back({vector(second),{}});
        }else {
            if(selected==MoveSelectedEventKind::HitWall)packet.push_back({vector(first),{}});
            packet.push_back({Value::Text(Kind::Object,other?other->reflection.objectPath:std::string{}),{}});
        }
        // Copied native inputs: no aliases back to the caller's hit packet.
        // Use the SAME Execution: no private Machine, budget, or acceptance.
        return execution.CallEvent(receiver->reflection.objectPath,name,true,packet);
    }
    void MoveBumpWall(RuntimeObject* receiver,PortableModelVec3 location,PortableModelVec3 normal,
        QuestVr::Vm::Execution& execution) {
        static_cast<void>(MoveSelectedEvent(receiver,MoveSelectedEventKind::BumpWall,location,normal,nullptr,execution));
    }
    void MoveBump(RuntimeObject* receiver,RuntimeObject* other,QuestVr::Vm::Execution& execution) {
        static_cast<void>(MoveSelectedEvent(receiver,MoveSelectedEventKind::Bump,{},{},other,execution));
    }
    void MoveHitWall(RuntimeObject* receiver,PortableModelVec3 normal,RuntimeObject* wall,
        QuestVr::Vm::Execution& execution) {
        static_cast<void>(MoveSelectedEvent(receiver,MoveSelectedEventKind::HitWall,normal,{},wall,execution));
    }
    std::int32_t MoveEncroachingOn(RuntimeObject* receiver,RuntimeObject* other,QuestVr::Vm::Execution& execution) {
        const auto result=MoveSelectedEvent(receiver,MoveSelectedEventKind::EncroachingOn,{},{},other,execution);
        // Native caller seeds its Bool DWORD to0. ProcessEvent NoWrite, a
        // dormant/probe-suppressed event, and Return Nothing retain that seed.
        // The VM canonicalizes Bool; only zero/nonzero is consumed by retail.
        if(result.kind==Kind::Nothing)return 0;
        if(result.kind!=Kind::Bool)
            throw std::runtime_error("Original EncroachingOn callback produced a non-Bool native result");
        return result.boolean?1:0;
    }
    void MoveEncroachedBy(RuntimeObject* receiver,RuntimeObject* other,QuestVr::Vm::Execution& execution) {
        static_cast<void>(MoveSelectedEvent(receiver,MoveSelectedEventKind::EncroachedBy,{},{},other,execution));
    }
