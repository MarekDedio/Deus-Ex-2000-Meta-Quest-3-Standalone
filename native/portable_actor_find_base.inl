// Included inside PortableActorVmHost after portable_retail_single_line.inl.
// Actual current-Level FindBase/SetSupportBase, on the caller's SAME Execution
// and journal. This is not falling physics, world publication or a new root.
    struct RetailFindBaseReply {
        // Private, synchronous CPU-owner identities only. A future public
        // diagnostic must detach qualified paths before returning its result.
        RuntimeObject *hitActor{},*baseAtDecision{},*selectedSupportFunction{};
        RetailSingleLineReply line;
        PortableModelVec3 start{},end{},extent{};
        bool unchanged{},basedOnSelf{},supportAttempted{};
        std::size_t marksBefore{},marksAfterQuery{},arenasBefore{},arenasAfterQuery{};
    };
    float RetailFindBaseDimension(RuntimeObject* actor,const char* key) {
        LifecycleStep();
        const auto property=DescribeProperty(key);auto* field=Object(property.key);
        if((std::string_view(key)!="Engine.Actor.CollisionRadius" &&
                std::string_view(key)!="Engine.Actor.CollisionHeight") ||
            property.key!=key || !OwnsProperty(actor->cls,property) ||
            property.zero.kind!=Kind::Float || property.arrayDimension!=1u || property.flags!=0x23u ||
            !field->property || field->property->type!="FloatProperty" || field->property->baseField ||
            field->property->referencedType || field->outer!=Object("Engine.Actor") ||
            LowerAscii(Qualified(Table(field->sourcePath),Table(field->sourcePath).exports.at(field->exportIndex).ObjClass))!="core.floatproperty")
            throw std::runtime_error("Original FindBase dimension lacks its exact Actor scalar Float declaration");
        const auto value=Read(actor,property,0u);
        if(value.kind!=Kind::Float)
            throw std::runtime_error("Original FindBase dimension live storage is not Float");
        return value.floating; // Query admission, not a clamp or geometric repair.
    }
    void RetailSupportActorDeclaration(const Function& function,bool original) {
        constexpr std::string_view expected="Engine.Actor.SupportActor";
        auto* node=Object(function.path);
        if(!node->script || function.source!=node->sourcePath ||
            function.nativeIndex!=node->script->nativeIndex || function.flags!=node->script->functionFlags ||
            LowerAscii(Qualified(Table(node->sourcePath),Table(node->sourcePath).exports.at(node->exportIndex).ObjClass))!="core.function")
            throw std::runtime_error("Original SupportActor selection lost its actual compiled Function identity");
        if(original && (function.path!=expected || node->outer!=Object("Engine.Actor") ||
            function.nativeIndex || function.flags!=0x802u || node->script->logicalSize!=15u ||
            node->script->bytecode.size()!=15u || function.bytecode.size()!=15u || function.variables.size()!=1u))
            throw std::runtime_error("Original Actor.SupportActor is not its 15-byte script declaration");
        const Property* parameter{};
        // Selected derived/state overrides may contain locals and use a
        // different input name. They still receive exactly one Actor packet,
        // with no OUT/return/optional field invented by this native caller.
        for(const auto& member:function.variables)if(member.flags&0x80u) {
            if(parameter)throw std::runtime_error("Original SupportActor override has extra input/result fields");
            parameter=&member;
        }
        if(!parameter)throw std::runtime_error("Original SupportActor override has no Actor input");
        auto* field=Object(parameter->key);
        if((original && parameter->key!="Engine.Actor.SupportActor.StandingActor") ||
            parameter->zero.kind!=Kind::Object || parameter->arrayDimension!=1u || parameter->flags!=0x80u ||
            !field->property || field->property->type!="ObjectProperty" || field->property->baseField ||
            field->outer!=node || !field->property->referencedType ||
            LowerAscii(Qualified(Table(field->sourcePath),field->property->referencedType))!="engine.actor" ||
            LowerAscii(Qualified(Table(field->sourcePath),Table(field->sourcePath).exports.at(field->exportIndex).ObjClass))!="core.objectproperty")
            throw std::runtime_error("Original SupportActor lacks its declared scalar Actor input packet");
    }
    RuntimeObject* RetailSetSupportBase(RuntimeObject* actor,RuntimeObject* target,
        QuestVr::Vm::Execution& execution) {
        if(!transaction_)throw std::runtime_error("Original SetSupportBase requires its caller VM root");
        actor=RetailCollisionActor(reinterpret_cast<std::uintptr_t>(actor));
        auto* level=ZoneLevel(reinterpret_cast<std::uintptr_t>(actor->nativeXLevel));
        LifecycleStep();
        if(!target) {
            SetBase(actor,nullptr,true,execution);
            return nullptr;
        }
        target=ZoneActor(reinterpret_cast<std::uintptr_t>(target),level);
        RetailSupportActorDeclaration(*FunctionFor(Object("Engine.Actor.SupportActor")),true);
        // The original FindFunctionChecked precedes ProcessEvent suppression.
        // A cached base implementation or direct SetBase would bypass actual
        // class/state overrides. Member uses the current selected StateNode.
        const auto identity=Member(target,"SupportActor");
        if(!identity)throw std::runtime_error("Original SupportActor FindFunctionChecked found no selected function");
        auto* selected=Object(identity->path);
        RetailSupportActorDeclaration(*FunctionFor(selected),false);
        const auto scratch=QuestVr::DecalStateDetail::Add(1024u+2u*sizeof(Evaluation),
            QuestVr::DecalStateDetail::Multiply(actor->reflection.objectPath.size()+1u,4u));
        QuestVr::NativeScratchScope reservation(nativeQueryScratchBytes_,scratch,
            [this](std::size_t extra){ValidateStateBudget(extra);});
        // IMPORTANT: bare CallFunction is not Actor.ProcessEvent. The original
        // begun-play/deletion gates still apply, even for this nonprobe name.
        // No forced begun flag, new Execution or independently committed call.
        static_cast<void>(execution.CallEvent(target->reflection.objectPath,"SupportActor",true,
            {{Value::Text(Kind::Object,actor->reflection.objectPath),{}}}));
        return selected; // Internal metadata only; callback suppression is not execution proof.
    }
    RetailFindBaseReply RetailFindBase(RuntimeObject* actor,QuestVr::Vm::Execution& execution) {
        if(!transaction_)throw std::runtime_error("Original FindBase requires its caller VM root");
        actor=RetailCollisionActor(reinterpret_cast<std::uintptr_t>(actor));
        auto* level=ZoneLevel(reinterpret_cast<std::uintptr_t>(actor->nativeXLevel));
        actor=ZoneActor(reinterpret_cast<std::uintptr_t>(actor),level);
        QuestVr::NativeScratchScope frame(nativeQueryScratchBytes_,
            sizeof(RetailFindBaseReply)+sizeof(QuestVr::OriginalActorLineHit)+512u,
            [this](std::size_t extra){ValidateStateBudget(extra);});
        LifecycleStep();
        // Native 103C9495..94B1 loads height, then radius for Y and its raw X
        // copy. No callback can run between these ordinary storage reads.
        const auto height=RetailFindBaseDimension(actor,"Engine.Actor.CollisionHeight");
        const auto radiusY=RetailFindBaseDimension(actor,"Engine.Actor.CollisionRadius");
        const auto radiusX=RetailFindBaseDimension(actor,"Engine.Actor.CollisionRadius");
        const auto location=NativeCoordsRead(actor,"Engine.Actor.Location",Kind::Vector,0x23u);
        const PortableModelVec3 start{location.vector[0u],location.vector[1u],location.vector[2u]};
        if(!std::isfinite(start.z))
            throw std::runtime_error("Original FindBase downward arithmetic requires finite active Z storage");
        // FLD32 Z / FSUB32 float8 / FSTP32: retained nearest-even PC64, then
        // binary32 storage. Host long double width/FMA modes are not authority.
        const PortableModelVec3 end{start.x,start.y,
            QuestVr::NativeWindowClipDetail::AddAndStore(start.z,-8.0f)};
        const PortableModelVec3 extent{radiusX,radiusY,height};
        QuestVr::OriginalActorLineHit hit{}; // Zero words, Time1 and Item-1.
        RetailFindBaseReply reply;
        reply.start=start;reply.end=end;reply.extent=extent;
        reply.marksBefore=nativeMoveHits_?nativeMoveHits_->MarkDepth():0u;
        reply.arenasBefore=nativeMoveHits_?nativeMoveHits_->ArenaCount():0u;
        const auto payloadBefore=nativeMoveHits_?nativeMoveHits_->PayloadBytes():0u;
        reply.line=RetailSingleLineLevel(level,actor,hit,end,start,extent,0x17u,0u);
        reply.marksAfterQuery=nativeMoveHits_?nativeMoveHits_->MarkDepth():0u;
        reply.arenasAfterQuery=nativeMoveHits_?nativeMoveHits_->ArenaCount():0u;
        if(reply.marksAfterQuery!=reply.marksBefore || reply.arenasAfterQuery!=reply.arenasBefore ||
            (nativeMoveHits_?nativeMoveHits_->PayloadBytes():0u)!=payloadBefore)
            throw std::runtime_error("Original FindBase SingleLine mark survives into its support callback");
        // The query's own mark has popped, INCLUDING before IsBasedOn or any
        // script dispatch. Legitimate enclosing movement marks remain alive.
        // Hit.Next is expired: never traverse it. Undefined world Item is not
        // inspected either; original FindBase consumes only Hit.Actor.
        reply.hitActor=hit.actor?ZoneActor(hit.actor,level):nullptr;
        const auto base=RetailPredicatePointerRead(actor,"Engine.Actor.Base","Engine.Actor",0x22u);
        reply.baseAtDecision=base.text.empty()?nullptr:
            ZoneActor(reinterpret_cast<std::uintptr_t>(Object(base.text)),level);
        if(reply.baseAtDecision==reply.hitActor) {
            reply.unchanged=true;
            return reply;
        }
        // The Bool SingleLine return is deliberately NOT the decision gate.
        // Direction is Hit.IsBasedOn(Self), including its initial self test.
        if(reply.hitActor && RetailIsBasedOn(reply.hitActor,actor)) {
            reply.basedOnSelf=true;
            return reply;
        }
        reply.supportAttempted=true;
        reply.selectedSupportFunction=RetailSetSupportBase(actor,reply.hitActor,execution);
        // No imposed final Base: an authentic override may choose otherwise.
        // LoadMap-only LevelInfo clearing and Spawn-before-PPP ordering belong
        // to their callers, not to this shared native FindBase body.
        return reply;
    }
