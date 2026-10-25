// Rollback-only original first-Training InterpolateTo commands, immediately
// before the SAME Execution's complete, unfiltered dynamic-actor phase.
    void ValidateStartupMoverCommands(const PortableWorldStartupProbeOptions& options) {
        if(options.moverCommands.empty())return;
        if(!transaction_ || options.moverCommands.size()>8u || LowerAscii(persistentMapPackageName)!="00_training" ||
            !options.playerLogin || !persistentNativeClient.HasClient() || persistentNativeClient.ReadOnlyView().size()!=1u ||
            !options.observeDynamicActorDelta || !std::isfinite(*options.observeDynamicActorDelta) ||
            *options.observeDynamicActorDelta<=0.0f || *options.observeDynamicActorDelta>1.0f ||
            !options.damageCommands.empty() || !options.landingCommands.empty() || !options.fallingCommands.empty() ||
            options.physicsCohort || options.collectDamageLoot || options.collectDamageLootViaPlayer || options.weapon)
            throw std::runtime_error("Startup movers require one real first-Training Login viewport and an isolated positive full actor phase");
        for(const auto& command:options.moverCommands) {
            LifecycleStep();
            if((command.actorPath!="00_Training.DeusExMover16" && command.actorPath!="00_Training.DeusExMover17") ||
                command.keyNum>7u || !std::isfinite(command.seconds) || command.seconds<=0.0f || command.seconds>60.0f)
                throw std::runtime_error("Startup mover command requires an authored first-door panel, key0..7 and finite seconds in(0,60]");
        }
    }
    std::shared_ptr<const Function> StartupMoverInterpolateDeclaration() {
        auto* method=Object("Engine.Mover.InterpolateTo");const auto function=FunctionFor(method);
        if(function->path!="Engine.Mover.InterpolateTo" || function->nativeIndex || function->flags!=3u ||
            !method->script || method->outer!=Object("Engine.Mover") || function->source!=method->sourcePath ||
            method->script->nativeIndex || method->script->functionFlags!=3u ||
            LowerAscii(Qualified(Table(method->sourcePath),Table(method->sourcePath).exports.at(method->exportIndex).ObjClass))!="core.function")
            throw std::runtime_error("Startup mover InterpolateTo lacks its exact original final script declaration");
        std::size_t inputs{};
        for(const auto& field:function->variables) {
            LifecycleStep();if(!(field.flags&0x80u))continue;
            if(inputs>=2u)throw std::runtime_error("Startup mover InterpolateTo has an additional input/result slot");
            const auto key=inputs==0u?"engine.mover.interpolateto.newkeynum":"engine.mover.interpolateto.seconds";
            const auto type=inputs==0u?"ByteProperty":"FloatProperty";auto* node=Object(field.key);
            if(LowerAscii(field.key)!=key || field.zero.kind!=(inputs==0u?Kind::Byte:Kind::Float) ||
                field.flags!=0x80u || field.arrayDimension!=1u || node->outer!=method || !node->property ||
                node->property->type!=type || node->property->baseField || node->property->referencedType ||
                LowerAscii(Qualified(Table(node->sourcePath),Table(node->sourcePath).exports.at(node->exportIndex).ObjClass))!=
                    (inputs==0u?"core.byteproperty":"core.floatproperty"))
                throw std::runtime_error("Startup mover InterpolateTo lacks its original required Byte/Float packet");
            ++inputs;
        }
        if(inputs!=2u)throw std::runtime_error("Startup mover InterpolateTo is missing its two required inputs");
        return function;
    }
    PortableStartupMoverState StartupMoverSnapshot(RuntimeObject* actor) {
        LifecycleActor(reinterpret_cast<std::uintptr_t>(actor));
        PortableStartupMoverState state;
        state.location=NativeCoordsRead(actor,"Engine.Actor.Location",Kind::Vector,0x23u).vector;
        state.rotation=NativeCoordsRead(actor,"Engine.Actor.Rotation",Kind::Rotator,0x23u).rotation;
        state.oldLocation=ReadFallingOldLocation(actor,FallingOldLocationProperty(actor)).vector;
        state.velocity=ReadActorPhysicsVector(actor,ActorPhysicsVectorProperty(actor,"Engine.Actor.Velocity")).vector;
        state.physics=static_cast<std::uint8_t>(Read(actor,ActorPhysicsProperty(actor),0u).integer);
        const auto scalar=[&](const char* key,const char* owner,Kind kind,std::uint32_t flags,const char* constraint="") {
            return Read(actor,MovingBrushScalarField(actor,key,owner,kind,flags,constraint),0u);
        };
        state.keyNum=static_cast<std::uint8_t>(scalar("Engine.Mover.KeyNum","Engine.Mover",Kind::Byte,1u).integer);
        state.glideType=static_cast<std::uint8_t>(scalar("Engine.Mover.MoverGlideType","Engine.Mover",Kind::Byte,1u,
            "Engine.Mover.EMoverGlideType").integer);
        state.bInterpolating=scalar("Engine.Actor.bInterpolating","Engine.Actor",Kind::Bool,0u).boolean;
        state.physAlpha=scalar("Engine.Actor.PhysAlpha","Engine.Actor",Kind::Float,0u).floating;
        state.physRate=scalar("Engine.Actor.PhysRate","Engine.Actor",Kind::Float,0u).floating;
        return state;
    }
    void ProbeStartupMovers(std::uint64_t viewportIdentity,RuntimeObject* player,
        const std::vector<PortableStartupMoverCommand>& commands,QuestVr::Vm::Execution& execution) {
        if(commands.empty())return;
        if(!transaction_ || !startupMoverPreview_ || !worldStartupProbe_ || !worldStartupResult_ ||
            !worldStartupResult_->movers.empty() || !worldStartupDynamicActorDelta_ ||
            worldStartupMoverCommands_!=&commands || commands.size()>8u ||
            LowerAscii(persistentMapPackageName)!="00_training")
            throw std::runtime_error("Startup mover commands require their one preview-only caller-owned startup root");
        auto* level=RetailCollisionLevel();ZoneActor(reinterpret_cast<std::uintptr_t>(player),level);
        auto* viewport=NativeSessionViewport(viewportIdentity);const auto views=persistentNativeClient.ReadOnlyView();
        auto* info=NativeLevelInfo(player);auto* game=ActorLink(info,"Game");
        if(!persistentNativeClient.HasClient() || views.size()!=1u || views.front().identity!=viewportIdentity ||
            viewport->nativeViewportActor!=player || player->nativePlayer!=viewport || !player->nativePlayerBinding ||
            !IsDerivedFromPath(player->cls,"Engine.PlayerPawn") || !game || !IsDerivedFromPath(game->cls,"Engine.GameInfo") ||
            NativeLevelInfo(game)!=info || !Read(info,PropertyNamed(info,"bBegunPlay"),0u).boolean ||
            Read(info,PropertyNamed(info,"bStartup"),0u).boolean || !level->nativeFirstDynamic)
            throw std::runtime_error("Startup mover commands lack real begun GameInfo/paired Login-player authority");
        constexpr std::size_t outputBound=64u<<10u;
        ValidateStateBudget(outputBound);
        nativeActorOrderObservationBytes_=QuestVr::DecalStateDetail::Add(nativeActorOrderObservationBytes_,outputBound);
        QuestVr::AuthoredStructLimits limits;limits.retainedBytes=outputBound;
        QuestVr::AuthoredStructDetail::Budget retained{limits};
        QuestVr::NativeScratchScope scratch(nativeQueryScratchBytes_,16u<<10u,[this](auto bytes){ValidateStateBudget(bytes);});
        auto& output=worldStartupResult_->movers;
        retained.Retain(commands.size()*sizeof(PortableStartupMoverObservation));output.reserve(commands.size());
        retained.Retain(output.capacity()*sizeof(PortableStartupMoverObservation));
        const auto text=[&](std::string& destination,std::string_view source) {
            if(source.size()>1024u || source.find('\0')!=std::string_view::npos)
                throw std::runtime_error("Startup mover detached identity exceeds its bounded output");
            retained.Retain(source.size()+1u);destination.assign(source);retained.Retain(destination.capacity()+1u);
        };
        const auto function=StartupMoverInterpolateDeclaration();
        for(std::size_t index=0u;index<commands.size();++index) {
            const auto& command=commands[index];
            auto* actor=LifecycleActor(reinterpret_cast<std::uintptr_t>(Object(command.actorPath)));
            const auto slot=actor->worldActorIndex;
            if(actor->runtimeBirth || actor->sourcePath!=persistentMapSourcePath || actor->cls->reflection.objectPath!="DeusEx.DeusExMover" ||
                !IsDerivedFromPath(actor->cls,"Engine.Mover") || actor->nativeXLevel!=level || slot<0 ||
                static_cast<std::size_t>(slot)<*level->nativeFirstDynamic || static_cast<std::size_t>(slot)>=persistentWorldActors.size() ||
                persistentWorldActors[static_cast<std::size_t>(slot)]!=actor ||
                Qualified(Table(actor->sourcePath),static_cast<std::int32_t>(actor->exportIndex+1u))!=command.actorPath)
                throw std::runtime_error("Startup mover target is not its actual authored live first-door dynamic actor");
            RequireOriginalActorTickPhysicsImplementation(actor);
            PortableStartupMoverObservation staged;staged.commandIndex=index;staged.keyNum=command.keyNum;staged.seconds=command.seconds;
            text(staged.actorPath,actor->reflection.objectPath);text(staged.classPath,actor->cls->reflection.objectPath);
            text(staged.functionPath,function->path);staged.before=StartupMoverSnapshot(actor);
            staged.writesBefore=staged.writesAfterCall=execution.WriteCount();staged.entered=true;
            output.push_back(std::move(staged));auto& row=output.back();
            try {
                const auto value=execution.CallFunction(*function,actor->reflection.objectPath,
                    {{Value::Byte(command.keyNum),{}},{Value::Float(command.seconds),{}}});
                row.returned=true;row.writesAfterCall=execution.WriteCount();
                if(value.kind!=Kind::Nothing)throw std::runtime_error("Original mover InterpolateTo unexpectedly returned a value");
                row.afterCall=StartupMoverSnapshot(actor);row.callCaptured=true;
            }catch(...) {
                row.writesAfterCall=execution.WriteCount();
                // Diagnostic admission can fail too. It must never replace the
                // original method/native/budget refusal being unwound.
                try {row.afterCall=StartupMoverSnapshot(actor);row.callCaptured=true;}catch(...) {}
                throw;
            }
        }
    }
    void BeginStartupMoverFrame() {
        if(!worldStartupMoverCommands_ || worldStartupMoverCommands_->empty())return;
        if(!transaction_ || !startupMoverPreview_ || !worldStartupProbe_ || !worldStartupResult_ ||
            worldStartupResult_->movers.size()!=worldStartupMoverCommands_->size())
            throw std::runtime_error("Startup mover frame lost its preview-only original command root");
        for(auto& row:worldStartupResult_->movers) {
            if(!row.returned || !row.callCaptured || row.frameEntered)
                throw std::runtime_error("Startup mover frame preceded a completed original InterpolateTo command");
            row.frameEntered=true;
        }
    }
    void CaptureStartupMoverFrame(bool returned,QuestVr::Vm::Execution& execution) {
        if(!worldStartupMoverCommands_ || worldStartupMoverCommands_->empty())return;
        if(!transaction_ || !startupMoverPreview_ || !worldStartupProbe_ || !worldStartupResult_)
            throw std::runtime_error("Startup mover frame snapshot lost its preview-only caller root");
        for(auto& row:worldStartupResult_->movers) {
            row.writesAfterFrame=execution.WriteCount();row.frameReturned=returned;
            row.afterFrame=StartupMoverSnapshot(Object(row.actorPath));row.frameCaptured=true;
        }
    }
