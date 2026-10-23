// Included inside PortableActorVmHost. These are optional diagnostic commands
// in the existing genuine startup root, not forced begun/state fixtures and
// not an accepted startup, paired player, equipped weapon or physics update.
    void ValidateOriginalDamagePacket(const Function& function,const bool base) {
        if(function.nativeIndex || (function.flags&0x400u) ||
            (base && (function.path!="Engine.Actor.TakeDamage" || function.flags!=0x800u)))
            throw std::runtime_error("TakeDamage lacks its original script event authority");
        const std::array<const char*,5u> names{"damage","eventinstigator","hitlocation","momentum","damagetype"};
        const std::array<Kind,5u> kinds{Kind::Int,Kind::Object,Kind::Vector,Kind::Vector,Kind::Name};
        const std::array<const char*,5u> types{"IntProperty","ObjectProperty","StructProperty","StructProperty","NameProperty"};
        auto* owner=Object(function.path);std::size_t slot{};
        for(const auto& field:function.variables) {
            LifecycleStep();
            if(!(field.flags&0x80u))continue;
            if(slot>=names.size() || field.arrayDimension!=1u || field.flags!=0x80u ||
                field.zero.kind!=kinds[slot] || LowerAscii(field.name)!=names[slot])
                throw std::runtime_error("TakeDamage has a different original five-parameter packet");
            auto* metadata=Object(field.key);
            if(metadata->outer!=owner || !metadata->property || metadata->property->type!=types[slot])
                throw std::runtime_error("TakeDamage parameter lacks its original declaring schema");
            if(slot==1u && LowerAscii(Qualified(Table(metadata->sourcePath),metadata->property->referencedType))!="engine.pawn")
                throw std::runtime_error("TakeDamage instigator is not constrained to Engine.Pawn");
            if((slot==2u || slot==3u) && (field.resultLayout!=QuestVr::Vm::ResultLayout::CoreVector12 ||
                LowerAscii(Qualified(Table(metadata->sourcePath),metadata->property->referencedType))!="core.object.vector"))
                throw std::runtime_error("TakeDamage vectors lack the canonical original Vector layout");
            ++slot;
        }
        if(slot!=names.size())throw std::runtime_error("TakeDamage lacks its complete original packet");
        // A state override legitimately has flags0x2, NOT FUNC_Event0x800.
        // The actual state-aware ResolveEvent identity supplies dispatch.
    }
    void ValidateStartupDamageCommands(const PortableWorldStartupProbeOptions& options) {
        if(options.collectDamageLootViaPlayer && !options.collectDamageLoot)
            throw std::runtime_error("Startup loot player input requires actual loot collection commands");
        if(options.collectDamageLoot && options.damageCommands.empty())
            throw std::runtime_error("Startup loot diagnostic requires actual damage-birth commands");
        if(options.damageCommands.empty())return;
        if(options.damageCommands.size()>8u || !options.playerLogin ||
            !persistentNativeClient.HasClient() || persistentNativeClient.ReadOnlyView().size()!=1u)
            throw std::runtime_error("Startup damage diagnostic requires at most eight commands and one real Login viewport");
        const auto valid=[](const std::string& value,bool empty) {
            return (empty || !value.empty()) && value.size()<=1024u && value.find('\0')==std::string::npos;
        };
        for(const auto& command:options.damageCommands) {
            LifecycleStep();
            if(!valid(command.actorPath,false) || !valid(command.damageType,true))
                throw std::runtime_error("Startup damage command has an invalid bounded identity/name");
            for(const auto* vector:{command.hitLocation?&*command.hitLocation:nullptr,&command.momentum})
                if(vector)for(const auto value:*vector)if(!std::isfinite(value))
                    throw std::runtime_error("Startup damage command has a nonfinite vector");
        }
    }
    void ProbeStartupDamage(RuntimeObject* player,const std::vector<PortableStartupDamageCommand>& commands,
        QuestVr::Vm::Execution& execution) {
        if(commands.empty())return;
        if(!transaction_ || !worldStartupProbe_ || !worldStartupResult_ ||
            !worldStartupResult_->damageObservations.empty())
            throw std::runtime_error("Startup damage requires its one actual caller-owned startup root");
        RequireMapActor(player);
        if(!IsDerivedFromPath(player->cls,"Engine.PlayerPawn") || player->worldRemoved)
            throw std::runtime_error("Startup damage has no genuine returned PlayerPawn");
        auto* info=NativeLevelInfo(player);auto* game=ActorLink(info,"Game");
        if(!game || !IsDerivedFromPath(game->cls,"Engine.GameInfo") || NativeLevelInfo(game)!=info ||
            !QuestVr::Vm::ToBool(Read(info,PropertyNamed(info,"bBegunPlay"),0u)) ||
            QuestVr::Vm::ToBool(Read(info,PropertyNamed(info,"bStartup"),0u)) ||
            !Read(player,PropertyNamed(player,"Player"),0u).text.empty())
            throw std::runtime_error("Startup damage lacks real begun GameInfo/unpaired Login-player authority");
        const auto base=FunctionFor(Object("Engine.Actor.TakeDamage"));ValidateOriginalDamagePacket(*base,true);
        // Admit detached output for the ENTIRE root, not only this lexical
        // call. It remains alive after a later real Possess/HUD refusal.
        constexpr std::size_t outputBound=512u<<10u,scratchBound=64u<<10u;
        ValidateStateBudget(outputBound);nativeActorOrderObservationBytes_+=outputBound;
        const auto beforeScratch=nativeQueryScratchBytes_;
        struct ScratchScope {std::size_t& value;std::size_t before;~ScratchScope(){value=before;}} scratch{nativeQueryScratchBytes_,beforeScratch};
        ValidateStateBudget(scratchBound);nativeQueryScratchBytes_+=scratchBound;
        QuestVr::AuthoredStructLimits bounds;bounds.retainedBytes=outputBound;
        QuestVr::AuthoredStructDetail::Budget retained{bounds};
        auto& output=worldStartupResult_->damageObservations;
        retained.Retain(commands.size()*sizeof(PortableStartupDamageObservation));output.reserve(commands.size());
        retained.Retain(output.capacity()*sizeof(PortableStartupDamageObservation));
        const auto text=[&](std::string& destination,std::string_view source) {
            if(source.size()>1024u || source.find('\0')!=std::string_view::npos)
                throw std::runtime_error("Startup damage observation identity exceeds its bound");
            retained.Retain(source.size()+1u);destination.assign(source);
            retained.Retain(destination.capacity()+1u);
        };
        const auto integer=[&](RuntimeObject* object,const char* name) {
            const auto value=Read(object,PropertyNamed(object,name),0u);
            if(value.kind!=Kind::Int && value.kind!=Kind::Byte)
                throw std::runtime_error("Startup damage observation requires original integer storage");
            return QuestVr::Vm::ToInt(value);
        };
        const auto flag=[&](RuntimeObject* object,const char* name) {
            const auto value=Read(object,PropertyNamed(object,name),0u);
            if(value.kind!=Kind::Bool)throw std::runtime_error("Startup damage observation requires original Bool storage");
            return value.boolean;
        };
        const auto vector=[&](RuntimeObject* object,const char* name) {
            const auto value=Read(object,PropertyNamed(object,name),0u);
            if(value.kind!=Kind::Vector)throw std::runtime_error("Startup damage observation requires original Vector storage");
            return value.vector;
        };
        const auto link=[&](RuntimeObject* object,const char* name) {
            const auto value=Read(object,PropertyNamed(object,name),0u);
            if(value.kind!=Kind::Object)throw std::runtime_error("Startup damage observation requires original Object storage");
            return value.text;
        };
        for(std::size_t index=0u;index<commands.size();++index) {
            LifecycleStep();const auto& command=commands[index];auto* actor=Object(command.actorPath);RequireMapActor(actor);
            if(!IsDerivedFromPath(actor->cls,"DeusEx.DeusExDecoration") || NativeLevelInfo(actor)!=info ||
                actor->worldRemoved || flag(actor,"bDeleteMe"))
                throw std::runtime_error("Startup damage target is not a live original decoration in the player's Level");
            // Do not expose a half-initialized row if preparation itself
            // exhausts shared work. Only a real event attempt owns a row.
            PortableStartupDamageObservation staged;staged.commandIndex=index;
            text(staged.actorPath,actor->reflection.objectPath);text(staged.playerPath,player->reflection.objectPath);
            text(staged.gameInfoPath,game->reflection.objectPath);
            text(staged.stateBefore,CurrentDispatchContext(actor).selectedPath);
            text(staged.contentsBefore,link(actor,"contents"));
            staged.worldBegun=flag(info,"bBegunPlay");staged.worldStartup=flag(info,"bStartup");
            staged.playerPaired=!link(player,"Player").empty();
            staged.hitPointsBefore=integer(actor,"HitPoints");staged.hitPointsAfter=staged.hitPointsBefore;
            const auto identity=ResolveEvent(actor->reflection.objectPath,"TakeDamage",true);
            if(!identity)throw std::runtime_error("Startup damage has no eligible original TakeDamage event");
            const auto selected=FunctionFor(Object(identity->path));ValidateOriginalDamagePacket(*selected,false);
            text(staged.functionPath,selected->path);
            const auto birthBoundary=persistentRuntime->get()->exports.size();
            const auto hit=command.hitLocation.value_or(vector(actor,"Location"));
            staged.writesBefore=staged.writesAfter=execution.WriteCount();
            output.push_back(std::move(staged));auto& row=output.back();
            row.entered=true;
            const auto result=execution.CallEvent(actor->reflection.objectPath,"TakeDamage",true,
                {{Value::Integer(command.damage),{}},{Value::Text(Kind::Object,player->reflection.objectPath),{}},
                 {Value::Vector(hit),{}},{Value::Vector(command.momentum),{}},{Value::Text(Kind::Name,command.damageType),{}}});
            row.returned=true;row.writesAfter=execution.WriteCount();
            if(result.kind!=Kind::Nothing)throw std::runtime_error("Original TakeDamage unexpectedly returned a value");
            row.hitPointsAfter=integer(actor,"HitPoints");row.removed=actor->worldRemoved;row.deleteMe=flag(actor,"bDeleteMe");
            text(row.stateAfter,CurrentDispatchContext(actor).selectedPath);
            const auto& exports=persistentRuntime->get()->exports;
            if(exports.size()<birthBoundary || exports.size()-birthBoundary>64u)
                throw std::runtime_error("Startup damage birth observation exceeds its bounded prefix");
            retained.Retain((exports.size()-birthBoundary)*sizeof(PortableStartupDamageBirth));
            row.births.reserve(exports.size()-birthBoundary);
            retained.Retain(row.births.capacity()*sizeof(PortableStartupDamageBirth));
            for(std::size_t born=birthBoundary;born<exports.size();++born) {
                LifecycleStep();auto* object=exports[born];
                if(!object || !object->runtimeBirth || !IsDerivedFromPath(object->cls,"Engine.Actor"))continue;
                RequireMapActor(object);auto& snapshot=row.births.emplace_back();
                text(snapshot.actorPath,object->reflection.objectPath);text(snapshot.classPath,object->cls->reflection.objectPath);
                text(snapshot.statePath,CurrentDispatchContext(object).selectedPath);
                text(snapshot.ownerPath,link(object,"Owner"));text(snapshot.basePath,link(object,"Base"));
                snapshot.location=vector(object,"Location");snapshot.velocity=vector(object,"Velocity");
                snapshot.physics=integer(object,"Physics");snapshot.remoteRole=integer(object,"RemoteRole");
                snapshot.removed=object->worldRemoved;snapshot.deleteMe=flag(object,"bDeleteMe");
                snapshot.pickup=IsDerivedFromPath(object->cls,"Engine.Inventory");
            }
            row.captured=true;
            // No tick, state slice, pose setter, inventory grant or callback
            // after a failure. Continue original SetPlayer/Possess unchanged.
        }
    }
