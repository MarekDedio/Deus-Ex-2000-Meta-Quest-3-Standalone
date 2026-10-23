// Same-root original Frob on actual surviving damage births, before normal
// SetPlayer/Possess. This is not a grant, a published pickup or controller input.
    void ValidateOriginalLootFrobPacket(const Function& function,bool base) {
        auto* node=Object(function.path);
        if(!node->script || function.nativeIndex || (function.flags&0x400u) ||
            function.source!=node->sourcePath || function.flags!=node->script->functionFlags ||
            node->script->nativeIndex ||
            LowerAscii(Qualified(Table(node->sourcePath),Table(node->sourcePath).exports.at(node->exportIndex).ObjClass))!="core.function" ||
            (base && (function.path!="Engine.Actor.Frob" || function.flags!=0x2u ||
                node->outer!=Object("Engine.Actor") || function.variables.size()!=2u)))
            throw std::runtime_error("Startup loot Frob lacks its original script declaration authority");
        std::size_t slot{};
        for(const auto& field:function.variables) {
            LifecycleStep();if(!(field.flags&0x80u))continue;
            if(slot>=2u || field.arrayDimension!=1u || field.flags!=0x80u || field.zero.kind!=Kind::Object)
                throw std::runtime_error("Startup loot Frob has a different two-Object input packet");
            auto* metadata=Object(field.key);
            if(metadata->outer!=node || !metadata->property || metadata->property->type!="ObjectProperty" ||
                metadata->property->baseField ||
                LowerAscii(Qualified(Table(metadata->sourcePath),Table(metadata->sourcePath).exports.at(metadata->exportIndex).ObjClass))!="core.objectproperty" ||
                LowerAscii(Qualified(Table(metadata->sourcePath),metadata->property->referencedType))!=
                    (slot==0u?"engine.actor":"engine.inventory"))
                throw std::runtime_error("Startup loot Frob lost its original Actor/Inventory parameter constraints");
            ++slot;
        }
        if(slot!=2u)throw std::runtime_error("Startup loot Frob lacks its complete original input packet");
        // Inherited state Frob overrides legitimately use flags2, not Event800.
    }
    void ProbeStartupLoot(RuntimeObject* player,QuestVr::Vm::Execution& execution) {
        if(!transaction_ || !worldStartupProbe_ || !worldStartupResult_ ||
            !worldStartupResult_->lootObservations.empty())
            throw std::runtime_error("Startup loot requires its one actual caller-owned startup root");
        RequireMapActor(player);
        auto* info=NativeLevelInfo(player);auto* game=ActorLink(info,"Game");
        const auto flag=[&](RuntimeObject* object,const char* name) {
            const auto value=Read(object,PropertyNamed(object,name),0u);
            if(value.kind!=Kind::Bool)throw std::runtime_error("Startup loot observation requires original Bool storage");
            return value.boolean;
        };
        const auto integer=[&](RuntimeObject* object,const char* name) {
            const auto value=Read(object,PropertyNamed(object,name),0u);
            if(value.kind!=Kind::Int && value.kind!=Kind::Byte)
                throw std::runtime_error("Startup loot observation requires original integer storage");
            return QuestVr::Vm::ToInt(value);
        };
        const auto link=[&](RuntimeObject* object,const char* name) {
            const auto value=Read(object,PropertyNamed(object,name),0u);
            if(value.kind!=Kind::Object)throw std::runtime_error("Startup loot observation requires original Object storage");
            return value.text;
        };
        if(!IsDerivedFromPath(player->cls,"Engine.PlayerPawn") || player->worldRemoved || flag(player,"bDeleteMe") ||
            !game || !IsDerivedFromPath(game->cls,"Engine.GameInfo") || NativeLevelInfo(game)!=info ||
            !flag(info,"bBegunPlay") || flag(info,"bStartup") || !link(player,"Player").empty())
            throw std::runtime_error("Startup loot lacks real begun GameInfo/unpaired Login-player authority");
        ValidateOriginalLootFrobPacket(*FunctionFor(Object("Engine.Actor.Frob")),true);
        constexpr std::size_t maximumItems=8u,maximumLinks=128u,outputBound=1u<<20u,scratchBound=64u<<10u;
        // Detached observations outlive this lexical call and remain after a
        // real later Possess refusal, so retain their entire fixed allowance.
        ValidateStateBudget(outputBound);
        nativeActorOrderObservationBytes_=QuestVr::DecalStateDetail::Add(nativeActorOrderObservationBytes_,outputBound);
        QuestVr::NativeScratchScope scratch(nativeQueryScratchBytes_,scratchBound,
            [this](auto bytes){ValidateStateBudget(bytes);});
        QuestVr::AuthoredStructLimits limits;limits.retainedBytes=outputBound;
        QuestVr::AuthoredStructDetail::Budget retained{limits};
        const auto text=[&](std::string& destination,std::string_view source) {
            if(source.size()>1024u || source.find('\0')!=std::string_view::npos)
                throw std::runtime_error("Startup loot observation identity exceeds its bound");
            retained.Retain(source.size()+1u);destination.assign(source);retained.Retain(destination.capacity()+1u);
        };
        const auto item=[&](RuntimeObject* object,PortableStartupLootItem& out) {
            LifecycleStep();RequireMapActor(object);
            if(!IsDerivedFromPath(object->cls,"Engine.Inventory") || NativeLevelInfo(object)!=info)
                throw std::runtime_error("Startup loot snapshot is not actual Inventory in the player's Level");
            text(out.actorPath,object->reflection.objectPath);text(out.classPath,object->cls->reflection.objectPath);
            text(out.statePath,CurrentDispatchContext(object).selectedPath);
            text(out.ownerPath,link(object,"Owner"));text(out.basePath,link(object,"Base"));
            text(out.inventoryPath,link(object,"Inventory"));text(out.instigatorPath,link(object,"Instigator"));
            out.physics=integer(object,"Physics");out.remoteRole=integer(object,"RemoteRole");
            if(IsDerivedFromPath(object->cls,"Engine.Pickup"))out.numCopies=integer(object,"NumCopies");
            if(IsDerivedFromPath(object->cls,"DeusEx.DeusExPickup"))out.maxCopies=integer(object,"maxCopies");
            out.removed=object->worldRemoved;out.deleteMe=flag(object,"bDeleteMe");out.heldItem=flag(object,"bHeldItem");
            out.hidden=flag(object,"bHidden");out.carriedItem=flag(object,"bCarriedItem");
            out.onlyOwnerSee=flag(object,"bOnlyOwnerSee");out.sleepTouch=flag(object,"bSleepTouch");
            out.collideActors=flag(object,"bCollideActors");
            out.collideWorld=flag(object,"bCollideWorld");out.blockActors=flag(object,"bBlockActors");
            out.blockPlayers=flag(object,"bBlockPlayers");
        };
        const auto inventory=[&](std::vector<PortableStartupLootItem>& out) {
            if(!out.empty())throw std::runtime_error("Startup loot inventory snapshot was already populated");
            retained.Retain(maximumLinks*sizeof(PortableStartupLootItem));out.reserve(maximumLinks);
            retained.Retain(out.capacity()*sizeof(PortableStartupLootItem));
            auto next=link(player,"Inventory");std::array<RuntimeObject*,maximumLinks> visited{};std::size_t count{};
            while(!next.empty()) {
                LifecycleStep();auto* object=Object(next);
                if(count==maximumLinks || std::find(visited.begin(),visited.begin()+count,object)!=visited.begin()+count)
                    throw std::runtime_error("Startup loot actual inventory chain cycles or exceeds its diagnostic bound");
                visited[count++]=object;auto& snapshot=out.emplace_back();item(object,snapshot);
                next=link(object,"Inventory");
            }
        };
        auto& output=worldStartupResult_->lootObservations;
        retained.Retain(maximumItems*sizeof(PortableStartupLootObservation));output.reserve(maximumItems);
        retained.Retain(output.capacity()*sizeof(PortableStartupLootObservation));
        std::array<RuntimeObject*,maximumItems> attempted{};std::size_t count{};
        const auto& damage=worldStartupResult_->damageObservations;
        for(std::size_t d=0u;d<damage.size();++d) {
            if(!damage[d].captured)throw std::runtime_error("Startup loot cannot consume incomplete damage birth observations");
            for(std::size_t b=0u;b<damage[d].births.size();++b) {
                const auto& birth=damage[d].births[b];
                if(!birth.pickup || birth.removed || birth.deleteMe)continue;
                LifecycleStep();auto* actor=Object(birth.actorPath);RequireMapActor(actor);
                if(!actor->runtimeBirth || actor->worldRemoved || flag(actor,"bDeleteMe") ||
                    actor->cls->reflection.objectPath!=birth.classPath ||
                    !IsDerivedFromPath(actor->cls,"Engine.Inventory") || NativeLevelInfo(actor)!=info)
                    throw std::runtime_error("Startup loot lost its actual surviving damage-birth Inventory identity");
                if(count==maximumItems || std::find(attempted.begin(),attempted.begin()+count,actor)!=attempted.begin()+count)
                    throw std::runtime_error("Startup loot repeated a damage birth or exceeded eight attempts");
                attempted[count++]=actor;
                PortableStartupLootObservation staged;staged.commandIndex=output.size();
                staged.damageCommandIndex=damage[d].commandIndex;staged.birthIndex=b;
                text(staged.actorPath,actor->reflection.objectPath);text(staged.playerPath,player->reflection.objectPath);
                text(staged.gameInfoPath,game->reflection.objectPath);text(staged.playerStatePath,CurrentDispatchContext(player).selectedPath);
                staged.worldBegun=flag(info,"bBegunPlay");staged.worldStartup=flag(info,"bStartup");
                staged.playerPaired=!link(player,"Player").empty();staged.isPawn=flag(player,"bIsPawn");
                staged.isPlayer=flag(player,"bIsPlayer");staged.playerHealth=integer(player,"Health");
                item(actor,staged.before);inventory(staged.inventoryBefore);
                const auto identity=ResolveEvent(actor->reflection.objectPath,"Frob",true);
                if(!identity)throw std::runtime_error("Startup loot has no eligible original state-aware Frob");
                const auto selected=FunctionFor(Object(identity->path));ValidateOriginalLootFrobPacket(*selected,false);
                text(staged.functionPath,selected->path);
                staged.writesBefore=staged.writesAfter=execution.WriteCount();output.push_back(std::move(staged));
                auto& row=output.back();row.entered=true;
                Value result;
                try {
                    result=execution.CallEvent(actor->reflection.objectPath,"Frob",true,
                        {{Value::Text(Kind::Object,player->reflection.objectPath),{}},{Value::Text(Kind::Object,{}),{}}});
                }catch(...) {row.writesAfter=execution.WriteCount();throw;}
                row.returned=true;row.writesAfter=execution.WriteCount();
                if(result.kind!=Kind::Nothing)throw std::runtime_error("Original loot Frob unexpectedly returned a value");
                item(actor,row.after);inventory(row.inventoryAfter);row.captured=true;
            }
        }
        // No gameplay publication or manual item grant. Normal SetPlayer and
        // original Possess follow, retaining any later genuine root failure.
    }
