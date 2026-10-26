// Included inside PortableActorVmHost. This is a committing per-Actor native
// phase service, like AdvanceTimer/RunStatePhase, NOT an automatic world Tick.
// No startup/animation/parity/owner prefix is fabricated by this adapter.
    void ObserveOriginalActorTickBoundary(RuntimeObject* actor,const std::string& nativeClassPath) {
        if(!activeTickStats_)return;
        const auto& name=actor->reflection.objectPath;
        const auto& cls=actor->cls->reflection.objectPath;
        if(name.size()>1024u||cls.size()>1024u||nativeClassPath.size()>1024u)
            throw std::runtime_error("Original Tick boundary diagnostic identity exceeds its bound");
        const auto bytes=2u*(name.size()+cls.size()+nativeClassPath.size()+3u);
        ValidateStateBudget(bytes);
        nativeActorOrderObservationBytes_=QuestVr::DecalStateDetail::Add(nativeActorOrderObservationBytes_,bytes);
        activeTickStats_->nativeBoundary.emplace();auto& out=*activeTickStats_->nativeBoundary;
        out.actorPath=name;out.classPath=cls;out.nativeClassPath=nativeClassPath;
        if(nativeClassPath=="DeusEx.ScriptedPawn") {
            const auto scalar=[&](const char* field,Kind kind,std::uint32_t flags,const char* type) {
                LifecycleStep();return OriginalAnimationRead(actor,
                    OriginalAnimationField(actor,"Engine.Actor",field,kind,flags,type),kind);
            };
            out.remoteRole=static_cast<std::uint8_t>(scalar("RemoteRole",Kind::Byte,0x21u,"ByteProperty").integer);
            out.role=static_cast<std::uint8_t>(scalar("Role",Kind::Byte,0x20u,"ByteProperty").integer);
            LifecycleStep();const auto physics=Read(actor,ActorPhysicsProperty(actor),0u);
            if(physics.kind!=Kind::Byte)throw std::runtime_error("Original Tick boundary Physics snapshot changed its declared Byte kind");
            out.physics=static_cast<std::uint8_t>(physics.integer);
            out.pawnFlag=scalar("bIsPawn",Kind::Bool,2u,"BoolProperty").boolean;
            out.alwaysTick=scalar("bAlwaysTick",Kind::Bool,2u,"BoolProperty").boolean;
            out.simulatedPawn=scalar("bSimulatedPawn",Kind::Bool,2u,"BoolProperty").boolean;
            out.stasis=scalar("bStasis",Kind::Bool,1u,"BoolProperty").boolean;
        }
        ValidateStateBudget();
    }
    RuntimeObject* OriginalActorTickNativeClass(RuntimeObject* actor) {
        std::array<RuntimeObject*,64u> visited{};std::size_t depth{};
        RuntimeObject* selected{};bool actualActor{};
        for(auto* cls=actor->cls;cls;cls=cls->base) {
            LifecycleStep();
            if(depth==visited.size()||std::find(visited.begin(),visited.begin()+depth,cls)!=visited.begin()+depth||
                !cls->classDescriptor||cls->defaultOwner||!cls->classFieldLinksValid||cls->reflection.metaClass!="Class")
                throw std::runtime_error("Original Actor Tick native ancestry is malformed, cyclic or unbounded");
            visited[depth++]=cls;
            const auto identity=CastClassIdentity(cls->reflection.objectPath);
            actualActor=actualActor||(LowerAscii(identity.path)=="engine.actor"&&cls==Object("Engine.Actor"));
            if(!selected&&(cls->reflection.flags&0x04000000u))selected=cls;
        }
        if(selected&&actualActor&&!CastObjectIdentity(actor->reflection.objectPath).classObject)return selected;
        throw std::runtime_error("Original Actor Tick has no authenticated native implementation");
    }
    void RequireOriginalInfoRulesAncestry(RuntimeObject* selected,const char* canonicalClass,const char* refusal) {
        LifecycleStep();auto* info=Object("Engine.Info");
        // Owned GameInfo ref13 / Mutator ref46 -> Info ref91 -> Actor ref2,
        // including actual UClass export/payload immediate superclass links.
        // Only the two separately audited wrappers below call this helper.
        // No other native Info descendant or same-name noncanonical class
        // may borrow this proof.
        if(selected!=Object(canonicalClass)||!selected->classDescriptor||selected->defaultOwner||
            !selected->classFieldLinksValid||selected->reflection.metaClass!="Class"||
            !(selected->reflection.flags&0x04000000u)||selected->base!=info||
            !info->classDescriptor||info->defaultOwner||!info->classFieldLinksValid||
            info->reflection.metaClass!="Class"||!(info->reflection.flags&0x04000000u)||
            info->base!=Object("Engine.Actor"))
            throw std::runtime_error(refusal);
    }
    void RequireOriginalGameInfoAncestry(RuntimeObject* selected) {
        RequireOriginalInfoRulesAncestry(selected,"Engine.GameInfo",
            "Original GameInfo Tick lost its actual native GameInfo/Info/Actor ancestry");
    }
    void RequireOriginalMutatorAncestry(RuntimeObject* selected) {
        RequireOriginalInfoRulesAncestry(selected,"Engine.Mutator",
            "Original Mutator Tick lost its actual native Mutator/Info/Actor ancestry");
    }
    void RequireOriginalDecalAncestry(RuntimeObject* selected) {
        LifecycleStep();
        // Owned Engine.u native Decal ref809 -> canonical Actor ref2;
        // the original vtable10426F74 inherits the five audited Actor slots.
        // A Decal is not Info-derived and cannot borrow the rule-class proof.
        if(selected!=Object("Engine.Decal")||!selected->classDescriptor||selected->defaultOwner||
            !selected->classFieldLinksValid||selected->reflection.metaClass!="Class"||
            !(selected->reflection.flags&0x04000000u)||selected->base!=Object("Engine.Actor"))
            throw std::runtime_error("Original Decal Tick lost its actual native Decal/Actor ancestry");
    }
    void RequireOriginalActorTickPhysicsImplementation(RuntimeObject* actor) {
        auto* selected=OriginalActorTickNativeClass(actor);const auto& path=selected->reflection.objectPath;
        // Owned native vtables/IAT, ORIGINAL-ACTOR-TICK-PHYSICS-PHASE.md
        // plus ORIGINAL-TRIGGER-TICK-PREREQUISITES.md and
        // ORIGINAL-DEUSEX-LEVELINFO-TICK-PREREQUISITES.md and
        // ORIGINAL-LIGHT-TICK-PREREQUISITES.md. Exact nearest native
        // boundaries only: neither parent Info/Triggers, unknown nearer
        // native children nor scalar teardown are authorized here. Native
        // ScriptedPawn Tick's separate wrapper does NOT admit Pawn physics.
        if(path=="Engine.Decal") {
            // Exact native Decal +14/+64/+8C are Actor state/Tick/physics.
            // Preserve actual selected Shadow script and later dependencies;
            // this is not permission for arbitrary native Decal descendants.
            RequireOriginalDecalAncestry(selected);return;
        }
        if(path=="Engine.GameInfo") {
            // GameInfo10427AE0 slots+14/+64/+8C are the original Actor
            // ProcessState/Tick/performPhysics, not a new guessed override.
            // See ORIGINAL-TRAINING-GAMEINFO-TICK-GAP.md. No scalar teardown
            // or blanket native Info permission follows from these slots.
            RequireOriginalGameInfoAncestry(selected);return;
        }
        if(path=="Engine.Mutator") {
            // Mutator10427B8C has the same audited +14/+64/+8C Actor slots.
            // This actual next Training object is admitted independently of
            // GameInfo; neither base Info nor arbitrary native children are.
            RequireOriginalMutatorAncestry(selected);return;
        }
        if(path=="Engine.Weapon") {
            // Exact original Weapon104287A4 slots+14/+64/+8C inherit
            // Actor.ProcessState/Tick/performPhysics. This is NOT permission
            // to admit an arbitrary nearer native Inventory/Weapon child.
            // See ORIGINAL-TRAINING-WEAPON-TICK-GAP.md for independent bytes.
            LifecycleStep();auto* inventory=Object("Engine.Inventory");
            if(selected!=Object("Engine.Weapon")||selected->base!=inventory||
                !inventory->classDescriptor||inventory->defaultOwner||!inventory->classFieldLinksValid||
                inventory->reflection.metaClass!="Class"||!(inventory->reflection.flags&0x04000000u)||
                inventory->base!=Object("Engine.Actor"))
                throw std::runtime_error("Original Weapon Tick lost its actual native Weapon/Inventory/Actor ancestry");
            return;
        }
        if(path=="DeusEx.DeusExDecoration"||path=="DeusEx.DeusExLevelInfo"||path=="Engine.Projectile"||path=="Engine.Inventory"||
            path=="Engine.Actor"||path=="Engine.Decoration"||path=="Engine.Mover"||path=="Engine.Trigger"||path=="Engine.Light")return;
        ObserveOriginalActorTickBoundary(actor,path);
        throw std::runtime_error("Original Actor Tick reached an unimplemented native virtual override: "+path);
    }
    void OriginalTickEventDeclaration(RuntimeObject* actor,const Function& function,bool base) {
        auto* method=Object(function.path);auto* owner=method->outer;
        if(LowerAscii(function.path.substr(function.path.find_last_of('.')+1u))!="tick"||
            !method->script||!owner||(!owner->classDescriptor&&!owner->stateDescriptor))
            throw std::runtime_error("Original Tick event has no actual selected Function/Class/State identity");
        auto* ownerClass=owner->stateDescriptor?owner->outer:owner;
        if(!ownerClass||!ownerClass->classDescriptor||!IsDerivedFromPath(actor->cls,ownerClass->reflection.objectPath))
            throw std::runtime_error("Original Tick event belongs to another Actor ancestry");
        if(base&&(function.path!="Engine.Actor.Tick"||function.nativeIndex||function.flags!=0x800u||function.variables.size()!=1u))
            throw std::runtime_error("Original base Tick has a different native event packet");
        std::size_t parameters{};
        for(const auto& field:function.variables) {
            LifecycleStep();if(!(field.flags&0x80u))continue;
            auto* node=Object(field.key);
            if(parameters++||field.zero.kind!=Kind::Float||field.arrayDimension!=1u||field.flags!=0x80u||
                (base&&field.key!="Engine.Actor.Tick.DeltaTime")||node->outer!=method||!node->property||
                node->property->type!="FloatProperty"||node->property->baseField||node->property->referencedType||
                LowerAscii(Qualified(Table(node->sourcePath),Table(node->sourcePath).exports.at(node->exportIndex).ObjClass))!="core.floatproperty")
                throw std::runtime_error("Original Tick does not have its one scalar Float packet");
        }
        if(parameters!=1u)throw std::runtime_error("Original Tick is missing DeltaTime");
    }
    QuestVr::OriginalNonPawnTickTailStats AdvanceOriginalTickTail(RuntimeObject* actor,float elapsed,
        std::int32_t tickType,QuestVr::Vm::Execution& execution) {
        if(!transaction_)throw std::runtime_error("Original Tick tail has no caller transaction");
        actor=LifecycleActor(reinterpret_cast<std::uintptr_t>(actor));
        if(IsDerivedFromPath(actor->cls,"Engine.Pawn"))
            throw std::runtime_error("Original non-Pawn Tick tail cannot substitute for Pawn Tick");
        RequireOriginalActorTickPhysicsImplementation(actor);
        const auto source=reinterpret_cast<std::uintptr_t>(actor);
        const auto require=[&](std::uintptr_t identity) {
            if(identity!=source)throw std::runtime_error("Original Tick tail changed its retained Actor");
            return LifecycleActor(identity);
        };
        QuestVr::OriginalNonPawnTickTailQueries q;q.work=[this]{LifecycleStep();};
        const auto role=[&](std::uintptr_t identity,bool remote) {
            auto* receiver=require(identity);
            const auto field=NoiseProperty(receiver,remote?"Engine.Actor.RemoteRole":"Engine.Actor.Role",
                Kind::Byte,remote?0x21u:0x20u,"Engine.Actor.ENetRole");
            auto* metadata=Object(field.key);
            if(metadata->outer!=Object("Engine.Actor")||metadata->property->baseField||
                LowerAscii(Qualified(Table(metadata->sourcePath),Table(metadata->sourcePath).exports.at(metadata->exportIndex).ObjClass))!="core.byteproperty")
                throw std::runtime_error("Original Tick Role lacks its actual Actor Byte declaration");
            const auto value=Read(receiver,field,0u);
            if(value.kind!=Kind::Byte)throw std::runtime_error("Original Tick Role has changed storage kind");
            return static_cast<std::uint8_t>(value.integer);
        };
        q.remoteRole=[&](auto a){return role(a,true);};q.role=[&](auto a){return role(a,false);};
        q.physics=[&](auto a){return static_cast<std::uint8_t>(Read(require(a),ActorPhysicsProperty(require(a)),0u).integer);};
        q.isProbingTick=[&](auto a) {
            const auto context=CurrentDispatchContext(require(a));
            return QuestVr::ScriptDispatch::IsEnabled("Tick",context.classProbeMask,context.codeMasks,context.disabledNames,context.liveProbeMask);
        };
        q.tick=[&](auto a,float delta) {
            auto* receiver=require(a);
            OriginalTickEventDeclaration(receiver,*FunctionFor(Object("Engine.Actor.Tick")),true);
            const auto selected=Member(receiver,"Tick");
            if(!selected)throw std::runtime_error("Original Tick FindFunctionChecked found no selected event");
            OriginalTickEventDeclaration(receiver,*FunctionFor(Object(selected->path)),false);
            QuestVr::NativeScratchScope scratch(nativeQueryScratchBytes_,1024u+sizeof(Evaluation),
                [this](std::size_t extra){ValidateStateBudget(extra);});
            static_cast<void>(execution.CallEvent(receiver->reflection.objectPath,"Tick",true,{{Value::Float(delta),{}}}));
        };
        q.processState=[&](auto a,float delta){static_cast<void>(execution.RunStatePhase(require(a)->reflection.objectPath,delta));};
        q.timer=[&](auto a,float delta){static_cast<void>(AdvanceTimer(require(a),delta,execution));};
        const auto lifeField=[&](RuntimeObject* receiver) {
            return OriginalAnimationField(receiver,"Engine.Actor","LifeSpan",Kind::Float,1u,"FloatProperty");
        };
        q.lifeSpan=[&](auto a) {
            auto* receiver=require(a);const auto value=Read(receiver,lifeField(receiver),0u);
            if(value.kind!=Kind::Float)throw std::runtime_error("Original LifeSpan has changed storage kind");
            return value.floating;
        };
        q.writeLifeSpan=[&](auto a,float next) {
            auto* receiver=require(a);const auto field=lifeField(receiver);
            ReserveActorPhysicsReferences(field,1u);
            auto reference=std::make_shared<Reference>();reference->zero=field.zero;
            reference->read=[this,receiver,field]{return Read(receiver,field,0u);};
            reference->write=[this,receiver,field](const Value& value) {
                LifecycleActor(reinterpret_cast<std::uintptr_t>(receiver));
                if(value.kind!=Kind::Float)throw std::runtime_error("Original LifeSpan native store changed kind");
                Touch(receiver);receiver->scriptValues[InstanceStorageKey(receiver->cls,field)][0u]=value;
                receiver->committedScriptState=true;ValidateStateBudget();
            };
            execution.WriteReference(reference,Value::Float(next));
        };
        q.expired=[&](auto a){LifecycleEvent(require(a),LifecycleEventKind::Expired,nullptr,execution);};
        q.destroy=[&](auto a,std::int32_t force) {
            auto* receiver=require(a);
            static_cast<void>(DestroyNativeActor(receiver->nativeXLevel,receiver,force,execution));
        };
        q.performPhysics=[&](auto a,float delta){static_cast<void>(RetailPerformActorPhysics(require(a),delta,execution));};
        return QuestVr::AdvanceOriginalNonPawnTickTail(source,elapsed,tickType,q);
    }
