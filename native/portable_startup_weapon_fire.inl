// Included inside PortableActorVmHost AFTER portable_startup_weapon.inl.
// Selected original player Fire cycles and bounded main-animation/state suffix,
// still inside the genuine pre-Possess startup root. No guessed native/hit,
// synthetic attack, input Tick, direct damage or partially published session.
    void ValidateStartupWeaponFire(const std::optional<PortableStartupWeaponFireCommand>& command,
        const std::optional<PortableStartupWeaponCommand>& equip) {
        if(!command)return;
        if(!equip)throw std::runtime_error("Startup weapon Fire requires the genuine Training pickup/equip command");
        ValidateStartupWeaponCommand(*equip);
        ValidatePortableStartupWeaponFireInput(*command);
    }
    PortableStartupWeaponFireObservation* startupFireTraceRow_{};
    QuestVr::AuthoredStructDetail::Budget* startupFireTraceRetained_{};
    std::size_t startupFireTraceCount_{};
    std::optional<std::size_t> RecordStartupWeaponFireTrace(RuntimeObject* source,RuntimeObject* hitActor,
        PortableModelVec3 start,PortableModelVec3 end,PortableModelVec3 extent,bool actors,
        const QuestVr::OriginalActorLineHit& hit,std::size_t writes) {
        if(!startupFireTraceRow_)return std::nullopt;
        if(!transaction_ || !worldStartupProbe_ || !startupFireTraceRetained_ || startupFireTraceCount_>=64u)
            throw std::runtime_error("Training Fire actual-trace observations exceeded their single-root bound");
        auto& rows=startupFireTraceRow_->traces;auto& retained=*startupFireTraceRetained_;
        if(rows.size()==rows.capacity()) {
            const auto capacity=std::min<std::size_t>(64u,std::max<std::size_t>(4u,rows.capacity()*2u));
            retained.Retain(capacity*sizeof(PortableStartupWeaponFireTrace));rows.reserve(capacity);
            retained.Retain(rows.capacity()*sizeof(PortableStartupWeaponFireTrace));
        }
        PortableStartupWeaponFireTrace row;
        StartupWeaponText(row.sourcePath,source->reflection.objectPath,retained);
        StartupWeaponText(row.actorPath,hitActor?std::string_view(hitActor->reflection.objectPath):std::string_view{},retained);
        row.start={start.x,start.y,start.z};row.end={end.x,end.y,end.z};row.extent={extent.x,extent.y,extent.z};
        row.hitLocation={hit.location.x,hit.location.y,hit.location.z};row.hitNormal={hit.normal.x,hit.normal.y,hit.normal.z};
        row.traceActors=actors;row.writesBeforeOutputs=row.writesAfterOutputs=writes;
        rows.push_back(std::move(row));++startupFireTraceCount_;return rows.size()-1u;
    }
    void StartupFireCrates(std::array<PortableStartupWeaponFireCrateSnapshot,2u>& out,
        QuestVr::AuthoredStructDetail::Budget& retained) {
        constexpr std::array<const char*,2u> paths{"00_Training.CrateBreakableMedGeneral1","00_Training.CrateBreakableMedGeneral2"};
        for(std::size_t index=0u;index<paths.size();++index) {
            LifecycleStep();auto* actor=Object(paths[index]);RequireMapActor(actor);auto& row=out[index];
            if(actor->runtimeBirth || actor->sourcePath!=persistentMapSourcePath ||
                !IsDerivedFromPath(actor->cls,"DeusEx.DeusExDecoration"))
                throw std::runtime_error("Training Fire observation lost an original first-map crate");
            StartupWeaponText(row.actorPath,actor->reflection.objectPath,retained);
            StartupWeaponText(row.classPath,actor->cls->reflection.objectPath,retained);
            StartupWeaponText(row.statePath,CurrentDispatchContext(actor).selectedPath,retained);
            StartupWeaponText(row.basePath,StartupWeaponLink(actor,"Base"),retained);
            StartupWeaponText(row.contentsPath,StartupWeaponLink(actor,"contents"),retained);
            row.location=StartupWeaponRead(actor,"Location",Kind::Vector).vector;
            row.hitPoints=StartupWeaponInteger(actor,"HitPoints");row.physics=StartupWeaponInteger(actor,"Physics");
            row.removed=actor->worldRemoved;row.deleteMe=StartupWeaponRead(actor,"bDeleteMe",Kind::Bool).boolean;
        }
    }
    void StartupFireBirths(std::size_t first,std::vector<PortableStartupWeaponFireBirth>& out,
        QuestVr::AuthoredStructDetail::Budget& retained) {
        const auto& exports=persistentRuntime->get()->exports;
        if(first>exports.size() || exports.size()-first>64u)
            throw std::runtime_error("Training Fire real-birth observations exceeded their per-phase bound");
        out.clear();retained.Retain((exports.size()-first)*sizeof(PortableStartupWeaponFireBirth));
        out.reserve(exports.size()-first);retained.Retain(out.capacity()*sizeof(PortableStartupWeaponFireBirth));
        for(std::size_t index=first;index<exports.size();++index) {
            LifecycleStep();auto* actor=exports[index];
            if(!actor || !actor->runtimeBirth || !IsDerivedFromPath(actor->cls,"Engine.Actor"))continue;
            RequireMapActor(actor);auto& row=out.emplace_back();
            StartupWeaponText(row.actorPath,actor->reflection.objectPath,retained);
            StartupWeaponText(row.classPath,actor->cls->reflection.objectPath,retained);
            StartupWeaponText(row.statePath,CurrentDispatchContext(actor).selectedPath,retained);
            StartupWeaponText(row.ownerPath,StartupWeaponLink(actor,"Owner"),retained);
            StartupWeaponText(row.basePath,StartupWeaponLink(actor,"Base"),retained);
            row.location=StartupWeaponRead(actor,"Location",Kind::Vector).vector;
            row.physics=StartupWeaponInteger(actor,"Physics");row.removed=actor->worldRemoved;
            row.deleteMe=StartupWeaponRead(actor,"bDeleteMe",Kind::Bool).boolean;
            row.inventory=IsDerivedFromPath(actor->cls,"Engine.Inventory");
        }
    }
    std::shared_ptr<const Function> StartupFirePoseAction(RuntimeObject* player,bool rotation) {
        const std::string path=rotation?"Engine.Pawn.ClientSetRotation":"Engine.Actor.SetLocation";
        const auto selected=Member(player,rotation?"ClientSetRotation":"SetLocation");
        if(!selected || selected->path!=path)throw std::runtime_error("Training Fire pose lacks its actual selected original method");
        const auto function=FunctionFor(Object(selected->path));auto* method=Object(path);
        if(function->nativeIndex!=(rotation?0u:267u) || function->flags!=(rotation?0xc2u:0x401u) ||
            !method->script || method->script->logicalSize!=(rotation?45u:5u) ||
            method->script->nativeIndex!=function->nativeIndex || method->script->functionFlags!=function->flags ||
            function->bytecode!=method->script->bytecode || function->source!=method->sourcePath ||
            method->outer!=Object(rotation?"Engine.Pawn":"Engine.Actor") || function->variables.size()!=2u)
            throw std::runtime_error("Training Fire pose action differs from the original declaring UFunction");
        const auto& input=function->variables.front();const auto name=rotation?"NewRotation":"NewLocation";
        auto* field=Object(input.key);
        if(input.key!=path+'.'+name || input.name!=name || input.flags!=0x80u || input.arrayDimension!=1u ||
            input.zero.kind!=(rotation?Kind::Rotator:Kind::Vector) || field->outer!=method || !field->property ||
            field->property->flags!=0x80u || field->property->arrayDimension!=1)
            throw std::runtime_error("Training Fire pose input lacks its original required struct packet");
        if(rotation)NativeCoordsRotatorSchema(field);else NativeCoordsVectorSchema(field);
        const auto& tail=function->variables.back();
        if(rotation) {
            if(tail.key!=path+".P" || tail.flags || tail.zero.kind!=Kind::Object || tail.arrayDimension!=1u)
                throw std::runtime_error("ClientSetRotation local differs from the original Pawn storage");
        } else if(tail.key!=path+".ReturnValue" || tail.flags!=0x580u || tail.zero.kind!=Kind::Bool || tail.arrayDimension!=1u)
            throw std::runtime_error("SetLocation lacks its original Bool result packet");
        return function;
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
        ValidatePortableStartupWeaponFireInput(command);
        if(!transaction_ || !worldStartupProbe_ || !worldStartupResult_ || !output.empty() ||
            LowerAscii(persistentMapPackageName)!="00_training" || startupFireTraceRow_ || startupFireTraceRetained_)
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
        // At most262 bounded rows: four Fire/frame groups and two pose calls.
        // Keep its entire (still conservatively admitted) observation allowance
        // capped at4MiB; reserving8MiB here unnecessarily crowded the genuine
        // Trace query's scratch under the unchanged32MiB combined state limit.
        // Snapshot Budget enforces the tighter cap; no query/state budget is
        // raised and no retained allocation becomes unaccounted.
        constexpr std::size_t outputBound=4u<<20u,scratchBound=64u<<10u;
        ValidateStateBudget(outputBound);
        nativeActorOrderObservationBytes_=QuestVr::DecalStateDetail::Add(nativeActorOrderObservationBytes_,outputBound);
        QuestVr::NativeScratchScope scratch(nativeQueryScratchBytes_,scratchBound,[this](auto bytes){ValidateStateBudget(bytes);});
        QuestVr::AuthoredStructLimits limits;limits.retainedBytes=outputBound;QuestVr::AuthoredStructDetail::Budget retained{limits};
        const auto capacity=static_cast<std::size_t>(command.attacks)*(static_cast<std::size_t>(command.frames)+1u)+
            (command.pose?2u:0u);
        retained.Retain(capacity*sizeof(PortableStartupWeaponFireObservation));output.reserve(capacity);
        retained.Retain(output.capacity()*sizeof(PortableStartupWeaponFireObservation));
        struct TraceScope {
            PortableActorVmHost& host;
            ~TraceScope(){host.startupFireTraceRow_=nullptr;host.startupFireTraceRetained_=nullptr;host.startupFireTraceCount_=0u;}
        } traceScope{*this};
        startupFireTraceRetained_=&retained;startupFireTraceCount_=0u;
        std::size_t notifyEvents{},birthBoundary{};
        const auto pose=[&](PortableStartupWeaponFireObservation& row,bool after) {
            const auto location=NativeCoordsRead(player,"Engine.Actor.Location",Kind::Vector,0x23u).vector;
            const auto rotation=NativeCoordsRead(player,"Engine.Actor.Rotation",Kind::Rotator,0x23u).rotation;
            const auto view=NativeCoordsRead(player,"Engine.Pawn.ViewRotation",Kind::Rotator,0x20u).rotation;
            const auto eye=NoiseField(player,"Engine.Pawn.BaseEyeHeight",Kind::Float,1u).floating;
            if(after){row.locationAfter=location;row.rotationAfter=rotation;row.viewRotationAfter=view;
                row.baseEyeHeightAfter=eye;row.poseCaptured=true;}
            else {row.locationBefore=location;row.rotationBefore=rotation;row.viewRotationBefore=view;row.baseEyeHeightBefore=eye;}
        };
        const auto begin=[&](PortableStartupWeaponFireStage stage,std::size_t attack,std::size_t frame,const std::shared_ptr<const Function>& function) {
            LifecycleStep();if(output.size()>=capacity)throw std::runtime_error("Startup weapon Fire observations exceeded their admitted bound");
            PortableStartupWeaponFireObservation row;row.stage=stage;row.commandIndex=output.size();row.attackIndex=attack;row.frameIndex=frame;
            StartupWeaponText(row.actorPath,path,retained);StartupWeaponText(row.playerPath,player->reflection.objectPath,retained);
            StartupWeaponText(row.gameInfoPath,game->reflection.objectPath,retained);
            if(function) {StartupWeaponText(row.functionPath,function->path,retained);StartupWeaponText(row.functionSource,function->source,retained);}
            StartupWeaponPlayer(player,info,row.playerBefore,retained);StartupWeaponItem(weapon,row.weaponBefore,retained);
            pose(row,false);StartupFireCrates(row.cratesBefore,retained);
            row.writesBefore=row.writesAfter=execution.WriteCount();row.entered=true;output.push_back(std::move(row));
            birthBoundary=persistentRuntime->get()->exports.size();startupFireTraceRow_=&output.back();
        };
        const auto snapshotAfter=[&](PortableStartupWeaponFireObservation& row) {
            StartupWeaponPlayer(player,info,row.playerAfter,retained);StartupWeaponItem(weapon,row.weaponAfter,retained);
            row.ownedAfter=StartupWeaponOwned(row.playerAfter,row.weaponAfter);
            pose(row,true);StartupFireCrates(row.cratesAfter,retained);StartupFireBirths(birthBoundary,row.births,retained);
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
            if(result.kind!=(row.stage==PortableStartupWeaponFireStage::SetLocation?Kind::Bool:Kind::Nothing))
                throw std::runtime_error("Original startup weapon diagnostic action returned a different result kind");
            snapshotAfter(row);StartupWeaponFireNotifies(weapon,row.authoredNotifies,retained);
            row.totalReturnedNotifyEvents=notifyEvents;row.captured=true;
        };
        if(command.pose) {
            for(const bool rotation:{false,true}) {
                const auto selected=StartupFirePoseAction(player,rotation);
                begin(rotation?PortableStartupWeaponFireStage::ClientSetRotation:PortableStartupWeaponFireStage::SetLocation,0u,0u,selected);
                auto& row=output.back();
                try {
                    const auto input=rotation?Value::Rotator(command.pose->viewRotation):Value::Vector(command.pose->location);
                    const auto result=execution.CallFunction(*selected,player->reflection.objectPath,{{input,{}}});
                    if(!rotation)row.placementAccepted=result.kind==Kind::Bool&&result.boolean;
                    finish(row,result);
                    if(!rotation && !row.placementAccepted)
                        throw std::runtime_error("Original diagnostic SetLocation refused the supplied player location");
                }catch(...) {failed(row);throw;}
            }
        }
        for(std::uint32_t attack=0u;attack<command.attacks;++attack) {
        const auto notifyBefore=notifyEvents;bool complete{};
        const auto selected=StartupWeaponFireAction(player);begin(PortableStartupWeaponFireStage::Fire,attack,0u,selected);
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
            begin(PortableStartupWeaponFireStage::AnimationThenState,attack,frame,{});auto& row=output.back();row.deltaSeconds=command.deltaSeconds;
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
                    if(notifyEvents==notifyBefore)throw std::runtime_error("Original attack returned Idle without a returned notify phase; no hit is inferred");
                    row.attackCycleComplete=true;complete=true;break;
                }
            }catch(...) {failed(row);throw;}
        }
        if(!complete)throw std::runtime_error("Original Training attack did not complete within its bounded animation/state suffix");
        }
        // All output is final, including full reserved capacities, actual
        // traces and births. Do not refund callback-added observation charges.
        if(retained.retained>outputBound || nativeActorOrderObservationBytes_<outputBound)
            throw std::runtime_error("Completed startup weapon Fire observation accounting is inconsistent");
        nativeActorOrderObservationBytes_-=outputBound-retained.retained;
    }
