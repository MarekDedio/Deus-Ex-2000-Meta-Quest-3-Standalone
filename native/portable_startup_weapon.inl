// Included inside PortableActorVmHost, after startup_loot_input and the original
// animation/FinishAnim bindings. Only a bounded same-Execution pickup/equip
// diagnostic after genuine Login, BEFORE ordinary SetPlayer/Possess. No Fire,
// damage, forced state/flags, presentation grant or complete Tick is supplied.
    void ValidateStartupWeaponCommand(const PortableStartupWeaponCommand& command) {
        if((command.actorPath!="00_Training.WeaponCombatKnife0" && command.actorPath!="00_Training.WeaponCrowbar1") ||
            !std::isfinite(command.deltaSeconds) || command.deltaSeconds<=0.0f || command.deltaSeconds>0.25f ||
            !command.frames || command.frames>64u)
            throw std::runtime_error("Startup weapon requires one actual first-Training knife/crowbar and at most sixty-four bounded phases");
    }
    void ValidateStartupWeapon(const PortableWorldStartupProbeOptions& options,
        const std::optional<PortableStartupWeaponCommand>& command) {
        if(!command)return;
        ValidateStartupWeaponCommand(*command);
        if(LowerAscii(persistentMapPackageName)!="00_training" || !options.playerLogin ||
            !persistentNativeClient.HasClient() || persistentNativeClient.ReadOnlyView().size()!=1u ||
            !options.damageCommands.empty() || !options.landingCommands.empty() || !options.fallingCommands.empty() ||
            options.physicsCohort || options.collectDamageLoot || options.collectDamageLootViaPlayer)
            throw std::runtime_error("Startup weapon requires one genuine Login viewport and an isolated pickup/equip diagnostic");
    }
    void StartupWeaponText(std::string& out,std::string_view value,QuestVr::AuthoredStructDetail::Budget& retained) {
        if(value.size()>1024u || value.find('\0')!=std::string_view::npos)
            throw std::runtime_error("Startup weapon detached identity exceeds its bound");
        retained.Retain(value.size()+1u);out.assign(value);retained.Retain(out.capacity()+1u);
    }
    Value StartupWeaponRead(RuntimeObject* object,const char* name,Kind kind) {
        const auto field=PropertyNamed(object,name);
        if(!OwnsProperty(object->cls,field) || field.arrayDimension!=1u || field.zero.kind!=kind)
            throw std::runtime_error(std::string("Startup weapon observation lacks its actual scalar field: ")+name);
        const auto value=Read(object,field,0u);
        if(value.kind!=kind)throw std::runtime_error("Startup weapon observation has different live scalar storage");
        return value;
    }
    std::string StartupWeaponLink(RuntimeObject* object,const char* name) {
        return StartupWeaponRead(object,name,Kind::Object).text;
    }
    std::int32_t StartupWeaponInteger(RuntimeObject* object,const char* name) {
        const auto field=PropertyNamed(object,name);
        if(!OwnsProperty(object->cls,field) || field.arrayDimension!=1u ||
            (field.zero.kind!=Kind::Byte && field.zero.kind!=Kind::Int))
            throw std::runtime_error("Startup weapon integer observation lacks an actual Byte/Int scalar");
        const auto value=Read(object,field,0u);
        if(value.kind!=field.zero.kind)throw std::runtime_error("Startup weapon integer observation changed storage kind");
        return QuestVr::Vm::ToInt(value);
    }
    void StartupWeaponState(RuntimeObject* actor,PortableStartupWeaponStateSnapshot& out,
        QuestVr::AuthoredStructDetail::Budget& retained) {
        StartupWeaponText(out.selectedPath,CurrentDispatchContext(actor).selectedPath,retained);
        out.localsRevision=StateLocalRevision(actor->reflection.objectPath);
        out.latentTimeLeft=actor->latentTimeLeft;
        if(actor->serializedStack) {
            const auto& raw=*actor->serializedStack;out.hasSerializedStack=true;
            out.rawFunctionReference=raw.functionReference;out.rawStateReference=raw.stateReference;
            out.rawProbeMask=raw.probeMask;out.rawLatentAction=raw.latentAction;out.rawLogicalOffset=raw.logicalOffset;
        }
        // Do not call ReadState here: observing an immutable dormant frame must
        // not materialize a live override or infer native latent0 from Code.
        if(!actor->stateObject)return;
        const auto& state=*actor->stateObject;out.hasStateObject=true;out.hasStack=state.hasStack;
        out.frameOverride=state.frameOverride;out.hasFrame=state.frame.has_value();
        if(!state.frame)return;
        const auto& frame=*state.frame;
        StartupWeaponText(out.codePath,frame.codePath,retained);StartupWeaponText(out.localsCodePath,frame.localsCodePath,retained);
        out.statementIndex=frame.statementIndex;out.portableLatent=frame.latent;out.hasControl=frame.control.has_value();
        if(frame.control) {out.codePresent=frame.control->codePresent;out.liveProbeMask=frame.control->liveProbeMask;}
    }
    void StartupWeaponInventory(RuntimeObject* player,RuntimeObject* info,
        std::vector<PortableStartupWeaponInventoryLink>& out,QuestVr::AuthoredStructDetail::Budget& retained) {
        constexpr std::size_t maximumLinks=128u;
        std::array<RuntimeObject*,maximumLinks> visited{};std::size_t count{};
        auto next=StartupWeaponLink(player,"Inventory");
        while(!next.empty()) {
            LifecycleStep();auto* actor=Object(next);RequireMapActor(actor);
            if(count==maximumLinks || std::find(visited.begin(),visited.begin()+count,actor)!=visited.begin()+count ||
                !IsDerivedFromPath(actor->cls,"Engine.Inventory") || NativeLevelInfo(actor)!=info)
                throw std::runtime_error("Startup weapon actual inventory chain cycles, exceeds its bound or leaves the player's Level");
            visited[count++]=actor;next=StartupWeaponLink(actor,"Inventory");
        }
        // The fixed pointer walk retains the full safety bound without
        // allocating128 detached records for every three-item snapshot. No
        // VM callback occurs between counting and copying the live chain.
        retained.Retain(count*sizeof(PortableStartupWeaponInventoryLink));out.reserve(count);
        retained.Retain(out.capacity()*sizeof(PortableStartupWeaponInventoryLink));
        for(std::size_t slot=0u;slot<count;++slot) {
            LifecycleStep();auto* actor=visited[slot];auto& row=out.emplace_back();
            StartupWeaponText(row.actorPath,actor->reflection.objectPath,retained);
            StartupWeaponText(row.classPath,actor->cls->reflection.objectPath,retained);
            StartupWeaponText(row.ownerPath,StartupWeaponLink(actor,"Owner"),retained);
            StartupWeaponText(row.inventoryPath,StartupWeaponLink(actor,"Inventory"),retained);
            StartupWeaponText(row.statePath,CurrentDispatchContext(actor).selectedPath,retained);
            row.runtimeBirth=actor->runtimeBirth;row.removed=actor->worldRemoved;
            row.deleteMe=StartupWeaponRead(actor,"bDeleteMe",Kind::Bool).boolean;
            const auto expected=slot+1u<count?std::string_view(visited[slot+1u]->reflection.objectPath):std::string_view{};
            if(row.inventoryPath!=expected)
                throw std::runtime_error("Startup weapon actual inventory chain changed during its read-only snapshot");
        }
    }
    void StartupWeaponPlayer(RuntimeObject* player,RuntimeObject* info,PortableStartupWeaponPlayerSnapshot& out,
        QuestVr::AuthoredStructDetail::Budget& retained) {
        static_cast<void>(MoveEventActor(player));
        StartupWeaponText(out.actorPath,player->reflection.objectPath,retained);
        StartupWeaponText(out.classPath,player->cls->reflection.objectPath,retained);
        const auto link=[&](std::string& value,const char* name){StartupWeaponText(value,StartupWeaponLink(player,name),retained);};
        link(out.playerPath,"Player");link(out.frobTargetPath,"FrobTarget");link(out.inHandPath,"inHand");
        link(out.inHandPendingPath,"inHandPending");link(out.clientInHandPendingPath,"ClientinHandPending");
        link(out.selectedItemPath,"SelectedItem");link(out.weaponPath,"Weapon");link(out.pendingWeaponPath,"PendingWeapon");
        out.role=StartupWeaponInteger(player,"Role");out.health=StartupWeaponInteger(player,"Health");
        out.isPawn=StartupWeaponRead(player,"bIsPawn",Kind::Bool).boolean;
        out.isPlayer=StartupWeaponRead(player,"bIsPlayer",Kind::Bool).boolean;
        out.inHandTransition=StartupWeaponRead(player,"bInHandTransition",Kind::Bool).boolean;
        StartupWeaponInventory(player,info,out.inventory,retained);
        auto grid=StartupLootInputGrid(player);out.gridRows=grid.rows;out.gridColumns=grid.columns;
        retained.Retain(grid.cells.capacity());out.grid=std::move(grid.cells);
        StartupWeaponState(player,out.state,retained);
        out.clock=ZoneClockSnapshot(player);retained.Retain(out.clock.capacity());
    }
    void StartupWeaponItem(RuntimeObject* weapon,PortableStartupWeaponItemSnapshot& out,
        QuestVr::AuthoredStructDetail::Budget& retained) {
        static_cast<void>(MoveEventActor(weapon));
        StartupWeaponText(out.actorPath,weapon->reflection.objectPath,retained);
        StartupWeaponText(out.classPath,weapon->cls->reflection.objectPath,retained);
        const auto link=[&](std::string& value,const char* name){StartupWeaponText(value,StartupWeaponLink(weapon,name),retained);};
        link(out.ownerPath,"Owner");link(out.inventoryPath,"Inventory");link(out.instigatorPath,"Instigator");
        link(out.meshPath,"Mesh");link(out.playerViewMeshPath,"PlayerViewMesh");link(out.ammoTypePath,"AmmoType");
        const auto sequence=OriginalAnimationField(weapon,"Engine.Actor","AnimSequence",Kind::Name,0x21u,"NameProperty");
        StartupWeaponText(out.sequence,OriginalAnimationRead(weapon,sequence,Kind::Name).text,retained);
        const auto number=[&](QuestVr::OriginalActorAnimationFloat selected) {
            return OriginalAnimationRead(weapon,OriginalAnimationFloatField(weapon,selected),Kind::Float).floating;
        };
        out.frame=number(QuestVr::OriginalActorAnimationFloat::Frame);out.rate=number(QuestVr::OriginalActorAnimationFloat::Rate);
        out.tweenRate=number(QuestVr::OriginalActorAnimationFloat::TweenRate);out.last=number(QuestVr::OriginalActorAnimationFloat::Last);
        out.drawScale=StartupWeaponRead(weapon,"DrawScale",Kind::Float).floating;
        out.physics=StartupWeaponInteger(weapon,"Physics");out.inventoryGroup=StartupWeaponInteger(weapon,"InventoryGroup");
        out.invPosX=StartupWeaponInteger(weapon,"invPosX");out.invPosY=StartupWeaponInteger(weapon,"invPosY");
        out.invSlotsX=StartupWeaponInteger(weapon,"invSlotsX");out.invSlotsY=StartupWeaponInteger(weapon,"invSlotsY");
        const auto flag=[&](const char* name){return StartupWeaponRead(weapon,name,Kind::Bool).boolean;};
        out.removed=weapon->worldRemoved;out.deleteMe=flag("bDeleteMe");out.heldItem=flag("bHeldItem");
        out.hidden=flag("bHidden");out.carriedItem=flag("bCarriedItem");out.onlyOwnerSee=flag("bOnlyOwnerSee");
        out.animLoop=flag("bAnimLoop");out.animFinished=flag("bAnimFinished");out.weaponUp=flag("bWeaponUp");
        out.readyToFire=flag("bReadyToFire");StartupWeaponState(weapon,out.state,retained);
        out.clock=ZoneClockSnapshot(weapon);retained.Retain(out.clock.capacity());
    }
    std::shared_ptr<const Function> StartupWeaponAction(RuntimeObject* player,bool putInHand) {
        const auto selected=Member(player,putInHand?"PutInHand":"UpdateInHand");
        if(!selected)throw std::runtime_error("Startup weapon player has no eligible selected equip action");
        const auto function=FunctionFor(Object(selected->path));auto* method=Object(function->path);
        const auto expected=std::string("DeusEx.DeusExPlayer.")+(putInHand?"PutInHand":"UpdateInHand");
        if(function->path!=expected || function->nativeIndex || function->flags!=(putInHand?0x2c2u:0x2u) ||
            !method->script || method->script->nativeIndex || method->script->functionFlags!=function->flags ||
            method->script->logicalSize!=(putInHand?180u:660u) || function->bytecode!=method->script->bytecode ||
            method->outer!=Object("DeusEx.DeusExPlayer") || function->source!=method->sourcePath ||
            LowerAscii(Qualified(Table(method->sourcePath),Table(method->sourcePath).exports.at(method->exportIndex).ObjClass))!="core.function")
            throw std::runtime_error("Startup weapon selected player action lacks its original declaring UFunction");
        std::size_t inputs{};
        for(const auto& field:function->variables) {
            LifecycleStep();if(!(field.flags&0x80u))continue;
            auto* metadata=Object(field.key);
            if(!putInHand || inputs || field.key!="DeusEx.DeusExPlayer.PutInHand.Inv" || field.zero.kind!=Kind::Object ||
                field.arrayDimension!=1u || field.flags!=0x90u || !metadata->property || metadata->outer!=method ||
                metadata->property->type!="ObjectProperty" || metadata->property->baseField ||
                metadata->property->arrayDimension!=1 || metadata->property->flags!=0x90u ||
                LowerAscii(Qualified(Table(metadata->sourcePath),metadata->property->referencedType))!="engine.inventory" ||
                LowerAscii(Qualified(Table(metadata->sourcePath),Table(metadata->sourcePath).exports.at(metadata->exportIndex).ObjClass))!="core.objectproperty")
                throw std::runtime_error("Startup weapon player action has a different original optional Inventory packet");
            ++inputs;
        }
        if(inputs!=(putInHand?1u:0u))throw std::runtime_error("Startup weapon player action has incomplete original input storage");
        return function;
    }
    bool StartupWeaponOwned(const PortableStartupWeaponPlayerSnapshot& player,const PortableStartupWeaponItemSnapshot& weapon) {
        if(weapon.removed || weapon.deleteMe || weapon.ownerPath!=player.actorPath)return false;
        return std::any_of(player.inventory.begin(),player.inventory.end(),[&](const auto& link) {
            return link.actorPath==weapon.actorPath && link.ownerPath==player.actorPath && !link.removed && !link.deleteMe;
        });
    }
    bool StartupWeaponEquipComplete(const PortableStartupWeaponObservation& row) {
        const auto& player=row.playerAfter;const auto& weapon=row.weaponAfter;
        return row.ownedAfter && player.inHandPath==weapon.actorPath && player.inHandPendingPath==weapon.actorPath &&
            player.selectedItemPath==weapon.actorPath && player.weaponPath==weapon.actorPath && player.pendingWeaponPath.empty() &&
            !player.inHandTransition && weapon.weaponUp && weapon.readyToFire &&
            weapon.state.selectedPath=="DeusEx.DeusExWeapon.Idle";
    }
    void ProbeStartupWeapon(RuntimeObject* player,const PortableStartupWeaponCommand& command,QuestVr::Vm::Execution& execution) {
        ValidateStartupWeaponCommand(command);
        if(!transaction_ || !worldStartupProbe_ || !worldStartupResult_ || !worldStartupResult_->weaponObservations.empty() ||
            LowerAscii(persistentMapPackageName)!="00_training")
            throw std::runtime_error("Startup weapon requires its one actual first-Training caller-owned startup root");
        static_cast<void>(MoveEventActor(player));RequireMapActor(player);
        auto* info=NativeLevelInfo(player);auto* game=ActorLink(info,"Game");
        if(!IsDerivedFromPath(player->cls,"DeusEx.DeusExPlayer") || player->worldRemoved ||
            StartupWeaponRead(player,"bDeleteMe",Kind::Bool).boolean || !game || !IsDerivedFromPath(game->cls,"Engine.GameInfo") ||
            NativeLevelInfo(game)!=info || !StartupWeaponRead(info,"bBegunPlay",Kind::Bool).boolean ||
            StartupWeaponRead(info,"bStartup",Kind::Bool).boolean || !StartupWeaponLink(player,"Player").empty())
            throw std::runtime_error("Startup weapon lacks real begun GameInfo/unpaired Login-player authority");
        auto* weapon=Object(command.actorPath);static_cast<void>(MoveEventActor(weapon));RequireMapActor(weapon);
        const char* expectedClass=command.actorPath=="00_Training.WeaponCombatKnife0"?"DeusEx.WeaponCombatKnife":"DeusEx.WeaponCrowbar";
        if(weapon->runtimeBirth || weapon->sourcePath!=persistentMapSourcePath || weapon->worldRemoved ||
            weapon->cls->reflection.objectPath!=expectedClass || !IsDerivedFromPath(weapon->cls,"Engine.Weapon") ||
            NativeLevelInfo(weapon)!=info || StartupWeaponRead(weapon,"bDeleteMe",Kind::Bool).boolean ||
            !StartupWeaponLink(weapon,"Owner").empty() ||
            Qualified(Table(weapon->sourcePath),static_cast<std::int32_t>(weapon->exportIndex+1u))!=command.actorPath)
            throw std::runtime_error("Startup weapon target is not its actual unowned authored first-Training weapon");
        constexpr std::size_t outputBound=8u<<20u,scratchBound=64u<<10u;
        ValidateStateBudget(outputBound);
        nativeActorOrderObservationBytes_=QuestVr::DecalStateDetail::Add(nativeActorOrderObservationBytes_,outputBound);
        QuestVr::NativeScratchScope scratch(nativeQueryScratchBytes_,scratchBound,[this](auto bytes){ValidateStateBudget(bytes);});
        QuestVr::AuthoredStructLimits limits;limits.retainedBytes=outputBound;QuestVr::AuthoredStructDetail::Budget retained{limits};
        auto& output=worldStartupResult_->weaponObservations;const auto capacity=static_cast<std::size_t>(command.frames)+4u;
        retained.Retain(capacity*sizeof(PortableStartupWeaponObservation));output.reserve(capacity);
        retained.Retain(output.capacity()*sizeof(PortableStartupWeaponObservation));
        const auto before=[&](PortableStartupWeaponStage stage,std::size_t frame,const std::shared_ptr<const Function>& function) {
            LifecycleStep();
            if(output.size()>=capacity)throw std::runtime_error("Startup weapon observations exceeded the admitted stage bound");
            PortableStartupWeaponObservation row;row.stage=stage;row.commandIndex=output.size();row.frameIndex=frame;
            StartupWeaponText(row.actorPath,weapon->reflection.objectPath,retained);StartupWeaponText(row.playerPath,player->reflection.objectPath,retained);
            StartupWeaponText(row.gameInfoPath,game->reflection.objectPath,retained);
            if(function) {StartupWeaponText(row.functionPath,function->path,retained);StartupWeaponText(row.functionSource,function->source,retained);}
            StartupWeaponPlayer(player,info,row.playerBefore,retained);StartupWeaponItem(weapon,row.weaponBefore,retained);
            row.writesBefore=row.writesAfter=execution.WriteCount();row.entered=true;output.push_back(std::move(row));
        };
        const auto after=[&](PortableStartupWeaponObservation& row,Value result) {
            row.returned=true;row.resultKind=result.kind;row.writesAfter=execution.WriteCount();
            if(result.kind!=Kind::Nothing)throw std::runtime_error("Startup weapon original void action unexpectedly returned a value");
            StartupWeaponPlayer(player,info,row.playerAfter,retained);StartupWeaponItem(weapon,row.weaponAfter,retained);
            row.ownedAfter=StartupWeaponOwned(row.playerAfter,row.weaponAfter);
            row.equipComplete=StartupWeaponEquipComplete(row);row.captured=true;
        };
        const auto call=[&](PortableStartupWeaponStage stage,const std::shared_ptr<const Function>& function,
            const std::vector<Evaluation>& arguments) {
            before(stage,0u,function);auto& row=output.back();
            try {after(row,execution.CallFunction(*function,player->reflection.objectPath,arguments));}
            catch(...) {row.writesAfter=execution.WriteCount();throw;}
        };
        const auto pickup=StartupLootInputFunction(player);
        const auto target=StartupLootInputProperty(player,"DeusEx.DeusExPlayer.FrobTarget",Kind::Object,0x20u,1u,"Engine.Actor");
        before(PortableStartupWeaponStage::Pickup,0u,pickup);
        {
            auto& row=output.back();
            try {
                const auto referenceBytes=2u*(sizeof(Property)+target.key.capacity()+target.name.capacity()+target.zero.text.capacity()+
                    4u*sizeof(void*))+sizeof(Reference)+256u;
                ValidateStateBudget(referenceBytes);
                nativeActorOrderObservationBytes_=QuestVr::DecalStateDetail::Add(nativeActorOrderObservationBytes_,referenceBytes);
                execution.WriteReference(ReferenceFor(player,target,0u,false),Value::Text(Kind::Object,weapon->reflection.objectPath));
                after(row,execution.CallFunction(*pickup,player->reflection.objectPath,{}));
            }catch(...) {row.writesAfter=execution.WriteCount();throw;}
            if(!row.ownedAfter)throw std::runtime_error("Original Training pickup did not produce actual player ownership/inventory membership");
        }
        call(PortableStartupWeaponStage::PutInHand,StartupWeaponAction(player,true),{{Value::Text(Kind::Object,weapon->reflection.objectPath),{}}});
        if(!output.back().ownedAfter)throw std::runtime_error("Original PutInHand lost the actual collected weapon");
        call(PortableStartupWeaponStage::UpdateInHand,StartupWeaponAction(player,false),{});
        if(!output.back().ownedAfter || output.back().playerAfter.weaponPath!=weapon->reflection.objectPath)
            throw std::runtime_error("Original pending equip did not select this actual weapon; advancing a previous weapon transition is outside this bounded diagnostic");
        for(std::uint32_t frame=0u;frame<command.frames;++frame) {
            before(PortableStartupWeaponStage::AnimationThenState,frame,{});auto& row=output.back();row.deltaSeconds=command.deltaSeconds;
            try {
                // Capture the original preceding Tick gate once, BEFORE any
                // animation callback. This is not the surrounding Actor Tick.
                const auto pawnFlag=OriginalAnimationField(weapon,"Engine.Actor","bIsPawn",Kind::Bool,2u,"BoolProperty");
                row.suppressAnimEnd=OriginalAnimationRead(weapon,pawnFlag,Kind::Bool).boolean &&
                    IsDerivedFromPath(weapon->cls,"Engine.Pawn") && OriginalAnimationRead(weapon,
                        OriginalAnimationField(weapon,"Engine.Actor","Role",Kind::Byte,0x20u,"ByteProperty"),Kind::Byte).integer==2;
                const auto stats=AdvanceOriginalMainAnimation(weapon,command.deltaSeconds,row.suppressAnimEnd,execution);
                row.animationIterations=stats.iterations;row.animationFloatWrites=stats.floatWrites;
                row.animationFinishedWrites=stats.finishedWrites;row.notifyEvents=stats.notifyEvents;
                row.animEndEvents=stats.animEndEvents;row.animationIterationCap=stats.iterationCap;
                const auto phase=execution.RunStatePhase(weapon->reflection.objectPath,command.deltaSeconds);
                row.stateDisposition=phase.status;after(row,phase.value);
            }catch(...) {row.writesAfter=execution.WriteCount();throw;}
            if(!row.ownedAfter || row.playerAfter.weaponPath!=weapon->reflection.objectPath)
                throw std::runtime_error("Original equip phases replaced or lost the requested actual weapon");
            if(row.weaponAfter.state.selectedPath=="DeusEx.DeusExWeapon.Idle" && row.weaponAfter.weaponUp && row.weaponAfter.readyToFire) {
                // A separate selected original method call, not a fabricated
                // transition clear or a claim about its unknown live caller.
                call(PortableStartupWeaponStage::UpdateInHand,StartupWeaponAction(player,false),{});
                if(!output.back().equipComplete)
                    throw std::runtime_error("Original final UpdateInHand did not complete this genuine pending equip");
                return;
            }
        }
        throw std::runtime_error("Original Training equip did not complete within the bounded animation/state phases");
        // Success returns to normal SetPlayer/Possess. Its genuine later
        // startup failure must still roll back ALL pickup/equip/state effects.
    }
