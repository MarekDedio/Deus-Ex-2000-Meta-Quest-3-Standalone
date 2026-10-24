// Included inside PortableActorVmHost AFTER portable_startup_weapon.inl.
// One original selected player Fire and bounded main-animation/state suffix,
// still inside the genuine pre-Possess startup root. No guessed native/hit,
// synthetic attack, input Tick, direct damage or partially published session.
    void ValidateStartupWeaponFire(const std::optional<PortableStartupWeaponFireCommand>& command,
        const std::optional<PortableStartupWeaponCommand>& equip) {
        if(!command)return;
        if(!equip)throw std::runtime_error("Startup weapon Fire requires the genuine Training pickup/equip command");
        ValidateStartupWeaponCommand(*equip);
        if(!std::isfinite(command->deltaSeconds) || command->deltaSeconds<=0.0f || command->deltaSeconds>0.1f ||
            !command->frames || command->frames>64u)
            throw std::runtime_error("Startup weapon Fire requires at most sixty-four bounded animation/state phases");
    }
    std::shared_ptr<const Function> StartupWeaponFireAction(RuntimeObject* player) {
        // InvalidState has only PlayerMove/PlayerTick; it does not override
        // Fire. Still resolve the REAL current selection before authenticating
        // the expected class entry, rather than forcing that class function.
        const auto selected=Member(player,"Fire");
        if(!selected)throw std::runtime_error("Startup weapon player has no eligible selected Fire action");
        const auto function=FunctionFor(Object(selected->path));auto* method=Object(function->path);
        if(function->path!="DeusEx.DeusExPlayer.Fire" || function->nativeIndex || function->flags!=0x202u ||
            !method->script || method->script->nativeIndex || method->script->functionFlags!=0x202u ||
            method->script->logicalSize!=39u || function->bytecode!=method->script->bytecode ||
            method->outer!=Object("DeusEx.DeusExPlayer") || function->source!=method->sourcePath ||
            LowerAscii(Qualified(Table(method->sourcePath),Table(method->sourcePath).exports.at(method->exportIndex).ObjClass))!="core.function" ||
            function->variables.size()!=1u)
            throw std::runtime_error("Startup weapon Fire selection is not the actual original DeusExPlayer Fire entry");
        const auto& field=function->variables[0u];auto* metadata=Object(field.key);
        if(field.key!="DeusEx.DeusExPlayer.Fire.F" || field.zero.kind!=Kind::Float || field.arrayDimension!=1u ||
            field.flags!=0x90u || !metadata->property || metadata->outer!=method ||
            metadata->property->type!="FloatProperty" || metadata->property->baseField || metadata->property->referencedType ||
            metadata->property->arrayDimension!=1 || metadata->property->flags!=0x90u || metadata->property->nextField ||
            LowerAscii(Qualified(Table(metadata->sourcePath),Table(metadata->sourcePath).exports.at(metadata->exportIndex).ObjClass))!="core.floatproperty")
            throw std::runtime_error("Startup weapon Fire lacks its original sole optional Float input packet");
        return function;
    }
    void StartupWeaponFireNotifies(RuntimeObject* weapon,std::vector<PortableStartupWeaponFireNotify>& out,
        QuestVr::AuthoredStructDetail::Budget& retained) {
        const auto meshValue=StartupWeaponRead(weapon,"Mesh",Kind::Object);
        ValidateObjectValue(meshValue,"Engine.Mesh",false);
        if(meshValue.text.empty())return;
        auto* mesh=Object(meshValue.text);
        if(!mesh->lodMesh)mesh->lodMesh=LoadCompleteRuntimeLodMesh(Table(mesh->sourcePath),mesh->exportIndex,mesh->sourcePath);
        if(!mesh->lodMesh || !mesh->lodMesh->animation)return;
        const auto sequence=OriginalAnimationRead(weapon,OriginalAnimationField(weapon,"Engine.Actor","AnimSequence",
            Kind::Name,0x21u,"NameProperty"),Kind::Name).text;
        for(const auto& authored:mesh->lodMesh->animation->sequences) {
            LifecycleStep();if(!QuestVr::MeshAnimationNamesEqual(authored.name,sequence))continue;
            if(authored.notifies.size()>64u)throw std::runtime_error("Startup weapon Fire selected notify metadata exceeds its observation bound");
            retained.Retain(authored.notifies.size()*sizeof(PortableStartupWeaponFireNotify));out.reserve(authored.notifies.size());
            retained.Retain(out.capacity()*sizeof(PortableStartupWeaponFireNotify));
            for(const auto& notify:authored.notifies) {
                LifecycleStep();auto& row=out.emplace_back();StartupWeaponText(row.function,notify.function,retained);
                row.time=notify.time;row.serializedIndex=notify.serializedIndex;
            }
            return;
        }
    }
    void ProbeStartupWeaponFire(RuntimeObject* player,RuntimeObject* weapon,const PortableStartupWeaponFireCommand& command,
        std::vector<PortableStartupWeaponFireObservation>& output,QuestVr::Vm::Execution& execution) {
        if(!transaction_ || !worldStartupProbe_ || !worldStartupResult_ || !output.empty() ||
            LowerAscii(persistentMapPackageName)!="00_training" || !std::isfinite(command.deltaSeconds) ||
            command.deltaSeconds<=0.0f || command.deltaSeconds>0.1f || !command.frames || command.frames>64u)
            throw std::runtime_error("Startup weapon Fire requires its single bounded first-Training startup root");
        static_cast<void>(MoveEventActor(player));static_cast<void>(MoveEventActor(weapon));
        RequireMapActor(player);RequireMapActor(weapon);
        auto* info=NativeLevelInfo(player);auto* game=ActorLink(info,"Game");
        const auto& path=weapon->reflection.objectPath;
        const char* expectedClass=path=="00_Training.WeaponCombatKnife0"?"DeusEx.WeaponCombatKnife":
            path=="00_Training.WeaponCrowbar1"?"DeusEx.WeaponCrowbar":nullptr;
        if(!expectedClass || weapon->runtimeBirth || weapon->sourcePath!=persistentMapSourcePath ||
            weapon->cls->reflection.objectPath!=expectedClass ||
            Qualified(Table(weapon->sourcePath),static_cast<std::int32_t>(weapon->exportIndex+1u))!=path ||
            !IsDerivedFromPath(player->cls,"DeusEx.DeusExPlayer") || player->worldRemoved || weapon->worldRemoved ||
            StartupWeaponRead(player,"bDeleteMe",Kind::Bool).boolean || StartupWeaponRead(weapon,"bDeleteMe",Kind::Bool).boolean ||
            !game || !IsDerivedFromPath(game->cls,"Engine.GameInfo") || NativeLevelInfo(game)!=info || NativeLevelInfo(weapon)!=info ||
            !StartupWeaponRead(info,"bBegunPlay",Kind::Bool).boolean || StartupWeaponRead(info,"bStartup",Kind::Bool).boolean ||
            !StartupWeaponLink(player,"Player").empty())
            throw std::runtime_error("Startup weapon Fire lacks the actual authored weapon and genuine begun unpaired Login-player authority");
        constexpr std::size_t outputBound=8u<<20u,scratchBound=64u<<10u;
        ValidateStateBudget(outputBound);
        nativeActorOrderObservationBytes_=QuestVr::DecalStateDetail::Add(nativeActorOrderObservationBytes_,outputBound);
        QuestVr::NativeScratchScope scratch(nativeQueryScratchBytes_,scratchBound,[this](auto bytes){ValidateStateBudget(bytes);});
        QuestVr::AuthoredStructLimits limits;limits.retainedBytes=outputBound;QuestVr::AuthoredStructDetail::Budget retained{limits};
        const auto capacity=static_cast<std::size_t>(command.frames)+1u;
        retained.Retain(capacity*sizeof(PortableStartupWeaponFireObservation));output.reserve(capacity);
        retained.Retain(output.capacity()*sizeof(PortableStartupWeaponFireObservation));
        std::size_t notifyEvents{};
        const auto begin=[&](PortableStartupWeaponFireStage stage,std::size_t frame,const std::shared_ptr<const Function>& function) {
            LifecycleStep();if(output.size()>=capacity)throw std::runtime_error("Startup weapon Fire observations exceeded their admitted bound");
            PortableStartupWeaponFireObservation row;row.stage=stage;row.commandIndex=output.size();row.frameIndex=frame;
            StartupWeaponText(row.actorPath,path,retained);StartupWeaponText(row.playerPath,player->reflection.objectPath,retained);
            StartupWeaponText(row.gameInfoPath,game->reflection.objectPath,retained);
            if(function) {StartupWeaponText(row.functionPath,function->path,retained);StartupWeaponText(row.functionSource,function->source,retained);}
            StartupWeaponPlayer(player,info,row.playerBefore,retained);StartupWeaponItem(weapon,row.weaponBefore,retained);
            row.writesBefore=row.writesAfter=execution.WriteCount();row.entered=true;output.push_back(std::move(row));
        };
        const auto snapshotAfter=[&](PortableStartupWeaponFireObservation& row) {
            StartupWeaponPlayer(player,info,row.playerAfter,retained);StartupWeaponItem(weapon,row.weaponAfter,retained);
            row.ownedAfter=StartupWeaponOwned(row.playerAfter,row.weaponAfter);
        };
        const auto failed=[&](PortableStartupWeaponFireObservation& row) {
            row.failed=true;row.writesAfter=execution.WriteCount();row.totalReturnedNotifyEvents=notifyEvents;
            // Preserve the genuine first VM/native failure. A refused partial
            // observation is explicitly marked, never converted into success.
            // These are read-only snapshots, not further VM operations.
            try {snapshotAfter(row);row.partialAfterCaptured=true;}catch(...) {}
        };
        const auto finish=[&](PortableStartupWeaponFireObservation& row,const Value& result) {
            row.returned=true;row.resultKind=result.kind;row.writesAfter=execution.WriteCount();
            if(result.kind!=Kind::Nothing)throw std::runtime_error("Original startup weapon Fire/state void action returned a value");
            snapshotAfter(row);StartupWeaponFireNotifies(weapon,row.authoredNotifies,retained);
            row.totalReturnedNotifyEvents=notifyEvents;row.captured=true;
        };
        const auto selected=StartupWeaponFireAction(player);begin(PortableStartupWeaponFireStage::Fire,0u,selected);
        {
            auto& row=output.back();
            const auto& p=row.playerBefore;const auto& w=row.weaponBefore;
            try {
                if(!StartupWeaponOwned(p,w) || p.inHandPath!=path || p.inHandPendingPath!=path || p.selectedItemPath!=path ||
                    p.weaponPath!=path || !p.pendingWeaponPath.empty() || p.inHandTransition ||
                    w.state.selectedPath!="DeusEx.DeusExWeapon.Idle" || !w.weaponUp || !w.readyToFire)
                    throw std::runtime_error("Startup weapon Fire requires the genuine completed owned Idle equip, not a forced ready state");
                finish(row,execution.CallFunction(*selected,player->reflection.objectPath,{{Value::Float(0.0f),{}}}));
                const auto& sequence=row.weaponAfter.sequence;
                row.attackSelected=sequence=="Attack" || sequence=="Attack2" || sequence=="Attack3";
                if(!row.ownedAfter || row.playerAfter.weaponPath!=path || !row.attackSelected ||
                    row.weaponAfter.state.selectedPath!="DeusEx.DeusExWeapon.NormalFire")
                    throw std::runtime_error("Selected original player Fire did not enter a real owned Training attack");
            }catch(...) {failed(row);throw;}
        }
        for(std::uint32_t frame=0u;frame<command.frames;++frame) {
            begin(PortableStartupWeaponFireStage::AnimationThenState,frame,{});auto& row=output.back();row.deltaSeconds=command.deltaSeconds;
            try {
                const auto pawnFlag=OriginalAnimationField(weapon,"Engine.Actor","bIsPawn",Kind::Bool,2u,"BoolProperty");
                row.suppressAnimEnd=OriginalAnimationRead(weapon,pawnFlag,Kind::Bool).boolean &&
                    IsDerivedFromPath(weapon->cls,"Engine.Pawn") && OriginalAnimationRead(weapon,
                        OriginalAnimationField(weapon,"Engine.Actor","Role",Kind::Byte,0x20u,"ByteProperty"),Kind::Byte).integer==2;
                const auto stats=AdvanceOriginalMainAnimation(weapon,command.deltaSeconds,row.suppressAnimEnd,execution);
                row.animationReturned=true;row.animationIterations=stats.iterations;row.animationFloatWrites=stats.floatWrites;
                row.animationFinishedWrites=stats.finishedWrites;row.notifyEvents=stats.notifyEvents;row.animEndEvents=stats.animEndEvents;
                row.missingHandlers=stats.missingHandlers;row.animationIterationCap=stats.iterationCap;
                notifyEvents=QuestVr::DecalStateDetail::Add(notifyEvents,stats.notifyEvents);
                const auto phase=execution.RunStatePhase(path,command.deltaSeconds);row.stateDisposition=phase.status;
                finish(row,phase.value);
                if(!row.ownedAfter || row.playerAfter.weaponPath!=path)
                    throw std::runtime_error("Original startup attack phases lost or replaced the actual owned weapon");
                if(row.weaponAfter.state.selectedPath=="DeusEx.DeusExWeapon.Idle" && row.weaponAfter.weaponUp && row.weaponAfter.readyToFire) {
                    if(!notifyEvents)throw std::runtime_error("Original attack returned Idle without a returned notify phase; no hit is inferred");
                    row.attackCycleComplete=true;return;
                }
            }catch(...) {failed(row);throw;}
        }
        throw std::runtime_error("Original Training attack did not complete within its bounded animation/state suffix");
    }
