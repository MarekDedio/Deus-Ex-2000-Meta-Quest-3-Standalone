// Pawn-aware AActor post-animation continuation, still within the original
// enclosing Level Execution. Complex player/simulated/network/Pawn-physics
// services remain explicit reached refusals, never generic Actor aliases.
    std::int32_t OriginalTimerConvertPeriods(const QuestVr::NativeScalar::Real& value) {
        // The original calls imported CRT _ftol and retains EAX32. For the
        // authenticated finite signed32 domain, integer truncation below is
        // exact independently of the host FPU. Wider/exceptional CRT results
        // are NOT inferred from an unchecked host cast or saturation rule.
        if(value.kind!=QuestVr::NativeScalar::Kind::Finite)
            throw std::runtime_error("Original Actor timer _ftol exceptional conversion is not implemented");
        const auto& input=value.finite;QuestVr::ActorTimerDetail::Wide whole{};
        if(input.significand) {
            if(input.exponent>=0) {
                if(static_cast<int>(QuestVr::ActorTimerDetail::Bits(input.significand))+input.exponent>32)
                    throw std::runtime_error("Original Actor timer _ftol wider integer conversion is not implemented");
                whole=input.significand<<static_cast<unsigned>(input.exponent);
            }else if(input.exponent>-128)whole=input.significand>>static_cast<unsigned>(-input.exponent);
        }
        const std::uint32_t limit=input.negative?0x80000000u:0x7fffffffu;
        if(whole>limit)throw std::runtime_error("Original Actor timer _ftol wider integer conversion is not implemented");
        auto word=static_cast<std::uint32_t>(whole);if(input.negative)word=0u-word;
        return std::bit_cast<std::int32_t>(word);
    }
    void AdvanceOriginalNativeActorTimer(RuntimeObject* actor,float elapsed,QuestVr::Vm::Execution& execution) {
        const auto source=reinterpret_cast<std::uintptr_t>(actor);
        const auto require=[&](auto identity) {
            if(identity!=source)throw std::runtime_error("Original native Actor timer changed its retained Actor");
            return LifecycleActor(identity);
        };
        const auto scalar=[&](RuntimeObject* receiver,const char* name,Kind kind,std::uint32_t flags) {
            return OriginalAnimationField(receiver,"Engine.Actor",name,kind,flags,kind==Kind::Bool?"BoolProperty":"FloatProperty");
        };
        QuestVr::OriginalActorTimerPhaseQueries q;q.work=[this]{LifecycleStep();};
        q.rate=[&](auto a){auto* r=require(a);return OriginalAnimationRead(r,scalar(r,"TimerRate",Kind::Float,0u),Kind::Float).floating;};
        q.counter=[&](auto a){auto* r=require(a);return OriginalAnimationRead(r,scalar(r,"TimerCounter",Kind::Float,2u),Kind::Float).floating;};
        q.loop=[&](auto a){auto* r=require(a);return OriginalAnimationRead(r,scalar(r,"bTimerLoop",Kind::Bool,0u),Kind::Bool).boolean;};
        q.writeRate=[&](auto a,float v){auto* r=require(a);WriteFallingScalar(r,scalar(r,"TimerRate",Kind::Float,0u),Value::Float(v),execution);};
        q.writeCounter=[&](auto a,float v){auto* r=require(a);WriteFallingScalar(r,scalar(r,"TimerCounter",Kind::Float,2u),Value::Float(v),execution);};
        q.convertPeriods=[this](auto value){return OriginalTimerConvertPeriods(value);};
        q.timer=[&](auto a){OriginalNativeActorCallback(require(a),"Timer",{},execution);};
        static_cast<void>(QuestVr::AdvanceOriginalActorTimerPhase(source,elapsed,q));
    }
    void OriginalPawnCopyViewRotation(RuntimeObject* actor,RuntimeObject* pawn,QuestVr::Vm::Execution& execution) {
        const auto source=NativeCoordsProperty(actor,"Engine.Actor.Rotation",Kind::Rotator,0x23u);
        const auto target=NativeCoordsProperty(pawn,"Engine.Pawn.ViewRotation",Kind::Rotator,0x20u);
        auto* node=Object(target.key);
        if(node->outer!=Object("Engine.Pawn")||!node->property||node->property->baseField||
            node->property->type!="StructProperty"||node->property->arrayDimension!=1||node->property->flags!=0x20u)
            throw std::runtime_error("Original Pawn ViewRotation lost its exact declaring Rotator");
        const auto copied=OriginalAnimationRead(actor,source,Kind::Rotator);
        ReserveActorPhysicsReferences(target,3u);
        for(unsigned axis=0u;axis<3u;++axis) {
            auto reference=std::make_shared<Reference>();reference->zero=Value::Integer(0);
            reference->read=[this,pawn,target,axis]{return Value::Integer(OriginalAnimationRead(pawn,target,Kind::Rotator).rotation[axis]);};
            reference->write=[this,pawn,target,axis](const Value& next) {
                if(next.kind!=Kind::Int)throw std::runtime_error("Original Pawn view copy lost its raw Int word");
                auto value=OriginalAnimationRead(pawn,target,Kind::Rotator);value.rotation[axis]=next.integer;
                Touch(pawn);pawn->scriptValues[InstanceStorageKey(pawn->cls,target)][0u]=std::move(value);
                pawn->committedScriptState=true;ValidateStateBudget();
            };
            execution.WriteReference(reference,Value::Integer(copied.rotation[axis]));
        }
    }
    std::int32_t AdvanceOriginalPawnTickContinuation(RuntimeObject* actor,const QuestVr::OriginalActorPawnContext& captured,
        float elapsed,std::int32_t tickType,QuestVr::Vm::Execution& execution) {
        const auto source=reinterpret_cast<std::uintptr_t>(actor);
        const auto require=[&](auto identity) {
            if(identity!=source)throw std::runtime_error("Original Pawn Tick changed its retained source");
            auto* receiver=LifecycleActor(identity);
            if(!transaction_||!activeTickLevel_||receiver->nativeXLevel!=activeTickLevel_||!activeTickStats_)
                throw std::runtime_error("Original Pawn Tick lost its enclosing native Level root");
            return receiver;
        };
        const auto role=[&](auto a,bool remote) {
            auto* r=require(a);const auto field=ScriptedPawnByteField(r,remote?
                QuestVr::OriginalScriptedPawnTickByte::RemoteRole:QuestVr::OriginalScriptedPawnTickByte::Role);
            return static_cast<std::uint8_t>(OriginalAnimationRead(r,field,Kind::Byte).integer);
        };
        QuestVr::OriginalActorPawnTickQueries q;q.work=[this]{LifecycleStep();};
        q.remoteRole=[&](auto a){return role(a,true);};q.role=[&](auto a){return role(a,false);};
        q.physics=[&](auto a){auto* r=require(a);return static_cast<std::uint8_t>(OriginalAnimationRead(r,ActorPhysicsProperty(r),Kind::Byte).integer);};
        q.playerPawnCast=[&](auto a) {
            return OriginalPawnPlayerCast(require(a));
        };
        q.player=[&](auto a) {
            auto* r=require(a);const auto field=RetailPredicatePointerField(r,"Engine.PlayerPawn.Player","Engine.PlayerPawn","Engine.Player",2u);
            if(r->nativePlayerBinding)return reinterpret_cast<std::uintptr_t>(r->nativePlayer);
            const auto value=OriginalAnimationRead(r,field,Kind::Object);ValidateObjectValue(value,"Engine.Player",false);
            return value.text.empty()?std::uintptr_t{}:reinterpret_cast<std::uintptr_t>(Object(value.text));
        };
        q.simulatedPawnMovement=[&](auto a,auto pawn,float) {
            static_cast<void>(require(a));static_cast<void>(require(pawn));
            throw std::runtime_error("Original simulated Pawn gravity/moveSmooth continuation is not implemented");
        };
        q.remotePlayerViewCopy=[&](auto a,auto playerPawn) {
            static_cast<void>(require(a));static_cast<void>(require(playerPawn));
            throw std::runtime_error("Original remote PlayerPawn view-copy continuation is not implemented");
        };
        q.localPlayerTick=[&](auto a,auto playerPawn,auto initialPlayer,float delta,std::int32_t)->QuestVr::OriginalPawnLocalTickResult {
            static_cast<void>(require(a));static_cast<void>(require(playerPawn));
            QuestVr::OriginalPlayerLocalTickQueries local;local.work=[this]{LifecycleStep();};
            local.cameraCast=[&](auto identity) {
                auto* receiver=require(identity);auto* camera=Object("Engine.Camera");
                // Actual ACamera Class1057B510, native Engine.u ref5595 ->
                // canonical PlayerPawn5. Neither name similarity nor a Bool
                // bUpdatePosition field proves the native +5FC ShowFlags gate.
                if(!camera->classDescriptor||camera->defaultOwner||!camera->classFieldLinksValid||
                    camera->reflection.metaClass!="Class"||!(camera->reflection.flags&0x04000000u)||
                    camera->base!=Object("Engine.PlayerPawn"))
                    throw std::runtime_error("Original local Player Tick lost its actual native Camera cast target");
                return !CastObject("Engine.Camera",Value::Text(Kind::Object,receiver->reflection.objectPath),false).text.empty();
            };
            local.cameraShowFlags=[&](auto identity) {
                auto* receiver=require(identity);
                const auto flags=OriginalAnimationRead(receiver,OriginalAnimationField(receiver,
                    "Engine.PlayerPawn","ShowFlags",Kind::Int,0u,"IntProperty"),Kind::Int);
                return static_cast<std::uint32_t>(flags.integer);
            };
            // The original rereads Player+5D4 before BOTH virtual calls.
            // Callbacks may replace the pointer; never reuse initialPlayer.
            local.player=[&](auto identity){static_cast<void>(require(identity));return q.player(identity);};
            local.readInput=[&](auto player,float) {
                auto* receiver=require(a);
                if(!receiver->nativePlayerBinding||!receiver->nativePlayer||
                    player!=reinterpret_cast<std::uintptr_t>(receiver->nativePlayer))
                    throw std::runtime_error("Original local Player ReadInput lacks its actual native player binding");
                auto* viewport=receiver->nativePlayer;
                clientDependent_=true;
                if(!viewport->nativeViewportIdentity||NativeSessionViewport(viewport->nativeViewportIdentity)!=viewport||
                    viewport->nativeViewportActor!=receiver)
                    throw std::runtime_error("Original local Player ReadInput has a revoked, stale or unpaired viewport");
                // Actual UViewport+54 requires an owned configured Input at+60
                // before UpdateInput/Input.ReadInput. The wrapper registry is
                // not that native Input instance, and is not a no-op input hook.
                throw std::runtime_error("Original viewport ReadInput configured Input service is not prepared");
            };
            local.playerInput=[&](auto identity,float elapsedInput) {
                OriginalNativeActorCallback(require(identity),"PlayerInput",Value::Float(elapsedInput),execution);
            };
            local.playerTick=[&](auto identity,float elapsedTick) {
                OriginalNativeActorCallback(require(identity),"PlayerTick",Value::Float(elapsedTick),execution);
            };
            local.hasNetDriver=[&](auto identity) {
                auto* receiver=require(identity);
                if(!receiver->nativeXLevel->nativeStandaloneLevelServices)
                    throw std::runtime_error("Original local Player Tick has no authenticated native NetDriver identity");
                return false; // Loader-owned actual absent standalone NetDriver.
            };
            local.networkTail=[&](auto identity) {
                static_cast<void>(require(identity));
                throw std::runtime_error("Original local Player network-copy continuation is not implemented");
            };
            return QuestVr::AdvanceOriginalPlayerLocalTick(a,playerPawn,initialPlayer,delta,local).result;
        };
        q.isProbingTick=[&](auto a) {
            const auto context=CurrentDispatchContext(require(a),true);
            return QuestVr::ScriptDispatch::IsEnabled("Tick",context.classProbeMask,context.codeMasks,context.disabledNames,context.liveProbeMask);
        };
        q.tick=[&](auto a,float delta){OriginalNativeActorCallback(require(a),"Tick",Value::Float(delta),execution);};
        q.processState=[&](auto a,float delta) {
            auto* r=require(a);auto* selected=OriginalActorTickNativeClass(r);const auto& native=selected->reflection.objectPath;
            // Independently authenticated +14: ScriptedPawn1001AF92 imports
            // AActor.ProcessState; native Pawn inherits that same slot.
            // Exact DeusExPlayer1002757C uses the same imported1001AF92 slot,
            // authenticated through its own complete native class chain.
            if(native=="DeusEx.DeusExPlayer")RequireOriginalDeusExPlayerAncestry(selected);
            else if(native!="DeusEx.ScriptedPawn"&&native!="Engine.Pawn")
                throw std::runtime_error("Original Pawn ProcessState native virtual is not audited");
            static_cast<void>(execution.RunStatePhase(r->reflection.objectPath,delta));
        };
        q.timer=[&](auto a,float delta){AdvanceOriginalNativeActorTimer(require(a),delta,execution);};
        const auto life=[&](RuntimeObject* r){return OriginalAnimationField(r,"Engine.Actor","LifeSpan",Kind::Float,1u,"FloatProperty");};
        q.lifeSpan=[&](auto a){auto* r=require(a);return OriginalAnimationRead(r,life(r),Kind::Float).floating;};
        q.writeLifeSpan=[&](auto a,float v){auto* r=require(a);WriteFallingScalar(r,life(r),Value::Float(v),execution);};
        q.expired=[&](auto a){OriginalNativeActorCallback(require(a),"Expired",{},execution);};
        q.destroy=[&](auto a,std::int32_t force){auto* r=require(a);static_cast<void>(DestroyNativeActor(r->nativeXLevel,r,force,execution));};
        q.performPhysics=[&](auto a,float delta) {
            static_cast<void>(RetailPerformPawnPhysics(require(a),delta,execution));
        };
        q.networkTail=[&](auto a,auto pawn) {
            auto* r=require(a);if(pawn)static_cast<void>(require(pawn));
            if(!r->nativeXLevel->nativeStandaloneLevelServices)
                throw std::runtime_error("Original Pawn network tail has no authenticated absent native NetDriver");
            // The loader owns the actual null NetDriver/DemoRecDriver edges;
            // the source's +88-null branch skips this network/view-copy block.
        };
        q.pawnIsPlayer=[&](auto pawn) {
            auto* r=require(pawn);return OriginalAnimationRead(r,OriginalAnimationField(r,
                "Engine.Pawn","bIsPlayer",Kind::Bool,0x20u,"BoolProperty"),Kind::Bool).boolean;
        };
        q.pawnViewTarget=[&](auto pawn) {
            auto* r=require(pawn);return OriginalAnimationRead(r,OriginalAnimationField(r,
                "Engine.Pawn","bViewTarget",Kind::Bool,0u,"BoolProperty"),Kind::Bool).boolean;
        };
        q.updateEyeHeight=[&](auto a,float delta){OriginalNativeActorCallback(require(a),"UpdateEyeHeight",Value::Float(delta),execution);};
        q.copyRotationToView=[&](auto a,auto pawn){OriginalPawnCopyViewRotation(require(a),require(pawn),execution);};
        q.weapon=[&](auto pawn) {
            auto* r=require(pawn);const auto field=RetailPredicatePointerField(r,"Engine.Pawn.Weapon","Engine.Pawn","Engine.Weapon",0x10020u);
            const auto value=OriginalAnimationRead(r,field,Kind::Object);ValidateObjectValue(value,"Engine.Weapon",false);
            return value.text.empty()?std::uintptr_t{}:reinterpret_cast<std::uintptr_t>(Object(value.text));
        };
        q.farMoveWeapon=[&](auto a,auto weapon) {
            auto* r=require(a);auto* item=LifecycleActor(weapon);ValidateObjectValue(Value::Text(Kind::Object,item->reflection.objectPath),"Engine.Weapon",false);
            const PortableModelVec3 location{MoveLocation(a,0u),MoveLocation(a,1u),MoveLocation(a,2u)};
            static_cast<void>(FarMoveLevelActor(r->nativeXLevel,item,location,0,0,execution));
        };
        q.longPawnTail=[&](auto a,auto pawn,float) {
            static_cast<void>(require(a));static_cast<void>(require(pawn));
            throw std::runtime_error("Original Pawn sight/pain/speech/tactics Tick-type2 tail is not implemented");
        };
        return QuestVr::AdvanceOriginalActorPawnTick(source,captured,elapsed,tickType,q).returned;
    }
