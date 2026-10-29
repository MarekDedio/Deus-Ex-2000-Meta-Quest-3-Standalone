// Included inside PortableActorVmHost. This continues the captured original
// Login pawn in the SAME startup Execution, not a fabricated account/player UI.
    void StartupPlayerScalar(RuntimeObject* pawn,const char* owner,const char* name,Kind kind,
        std::uint32_t flags,const char* type,const Value& value,QuestVr::Vm::Execution& execution) {
        LifecycleActor(reinterpret_cast<std::uintptr_t>(pawn));
        if(!IsDerivedFromPath(pawn->cls,"Engine.PlayerPawn"))
            throw std::runtime_error("SpawnPlayActor tail store lost the captured PlayerPawn");
        const auto property=OriginalAnimationField(pawn,owner,name,kind,flags,type);
        if(kind==Kind::Byte) {
            auto* field=Object(property.key);
            if(!field->property->referencedType||
                LowerAscii(Qualified(Table(field->sourcePath),field->property->referencedType))!="engine.actor.enetrole")
                throw std::runtime_error("SpawnPlayActor tail role lost the original ENetRole declaration");
        }
        execution.WriteReference(ReferenceFor(pawn,property,0u,false),value);
    }
    void StartupTravelEventPacket(RuntimeObject* receiver,const Function& function,const char* name,
        const char* parameterClass,bool original) {
        auto* method=Object(function.path);auto* owner=method->outer;
        const bool game=parameterClass!=nullptr;
        const std::string base=(game?"Engine.GameInfo.":"Engine.Actor.")+std::string(name);
        if(!method->script||!owner||!owner->classDescriptor||owner->defaultOwner||
            !IsDerivedFromPath(receiver->cls,owner->reflection.objectPath)||function.nativeIndex||
            !(function.flags&0x800u)||LowerAscii(function.path.substr(function.path.find_last_of('.')+1u))!=LowerAscii(name)||
            (original&&(function.path!=base||function.flags!=(game?0x802u:0x800u))))
            throw std::runtime_error("SpawnPlayActor tail event lacks its original checked declaration");
        std::size_t parameters{};
        for(const auto& property:function.variables) {
            LifecycleStep();if(!(property.flags&0x80u))continue;
            auto* field=Object(property.key);
            if(!game||parameters++||property.zero.kind!=Kind::Object||property.arrayDimension!=1u||property.flags!=0x80u||
                field->outer!=method||!field->property||field->property->type!="ObjectProperty"||field->property->baseField||
                !field->property->referencedType||
                LowerAscii(Qualified(Table(field->sourcePath),field->property->referencedType))!=LowerAscii(parameterClass))
                throw std::runtime_error("SpawnPlayActor tail event has a different native argument/result packet");
        }
        if(parameters!=(game?1u:0u))
            throw std::runtime_error("SpawnPlayActor tail event is missing its original native argument");
    }
    void StartupTravelPawnEvent(RuntimeObject* pawn,const char* name,QuestVr::Vm::Execution& execution) {
        QuestVr::NativeScratchScope metadata(nativeQueryScratchBytes_,
            8u*(std::char_traits<char>::length(name)+64u)+4u*sizeof(std::string),
            [this](std::size_t bytes){ValidateStateBudget(bytes);});
        const auto base=FunctionFor(Object(std::string("Engine.Actor.")+name));
        StartupTravelEventPacket(pawn,*base,name,nullptr,true);
        // This existing native FindFunctionChecked/ProcessEvent adapter keeps
        // scriptability, begun/probe/native/pending-kill gates in source order.
        OriginalNativeActorCallback(pawn,name,std::nullopt,execution);
    }
    RuntimeObject* StartupTravelGame(RuntimeObject* level) {
        // Native AcceptInventory and PostLogin independently read Actors[0]
        // THREE times, then its current Game; no Login/Game cache is authority.
        LifecycleStep();
        if(CurrentClockLevel()!=level||persistentWorldActors.empty()||!persistentWorldActors.front())
            throw std::runtime_error("SpawnPlayActor tail assertion: Actors(0)");
        LifecycleStep();auto* checked=persistentWorldActors.front();
        if(!checked||!IsDerivedFromPath(checked->cls,"Engine.LevelInfo"))
            throw std::runtime_error("SpawnPlayActor tail assertion: Actors(0)->IsA(ALevelInfo)");
        LifecycleStep();auto* info=persistentWorldActors.front();
        if(!info)throw std::runtime_error("SpawnPlayActor tail slot zero vanished before Game read");
        const auto property=OriginalAnimationField(info,"Engine.LevelInfo","Game",Kind::Object,0u,"ObjectProperty");
        auto* metadata=Object(property.key);
        if(!metadata->property->referencedType||
            LowerAscii(Qualified(Table(metadata->sourcePath),metadata->property->referencedType))!="engine.gameinfo")
            throw std::runtime_error("SpawnPlayActor tail Game lacks its original GameInfo constraint");
        const auto game=OriginalAnimationRead(info,property,Kind::Object);
        if(game.text.empty())throw std::runtime_error("SpawnPlayActor tail current LevelInfo.Game is None");
        auto* receiver=Object(game.text);RequireMapActor(receiver);
        if(!IsDerivedFromPath(receiver->cls,"Engine.GameInfo"))
            throw std::runtime_error("SpawnPlayActor tail current Game is not GameInfo");
        return receiver;
    }
    void StartupTravelGameEvent(RuntimeObject* level,RuntimeObject* pawn,const char* name,const char* parameterClass,
        QuestVr::Vm::Execution& execution) {
        QuestVr::NativeScratchScope metadata(nativeQueryScratchBytes_,
            8u*(std::char_traits<char>::length(name)+64u)+4u*sizeof(std::string),
            [this](std::size_t bytes){ValidateStateBudget(bytes);});
        auto* game=StartupTravelGame(level);
        const auto base=FunctionFor(Object(std::string("Engine.GameInfo.")+name));
        StartupTravelEventPacket(game,*base,name,parameterClass,true);
        QuestVr::NativeScratchScope packet(nativeQueryScratchBytes_,
            sizeof(Value)+pawn->reflection.objectPath.size()+1u,
            [this](std::size_t bytes){ValidateStateBudget(bytes);});
        // Same checked lookup/gates as the pawn events. In particular, do not
        // use generic CallEvent, which re-resolves and has different gates.
        OriginalNativeActorCallback(game,name,Value::Text(Kind::Object,pawn->reflection.objectPath),execution,parameterClass);
    }
    void StartupPlayerTail(RuntimeObject* level,RuntimeObject* pawn,const PortablePlayerLoginSource& source,
        const QuestVr::OriginalSpawnPlayActorUrl& url,PortablePlayerLoginObservation& observed,
        QuestVr::Vm::Execution& execution) {
        QuestVr::OriginalSpawnPlayActorTailQueries query;
        QuestVr::OriginalSpawnPlayActorTravelQueries travelQuery;
        // The second set of provider headers conservatively admits heap
        // captures for these fixed, bounded callbacks before std::function
        // assignment. Most captures fit inline; the combined owned captures
        // of these fixed callbacks fit within the combined extra headers.
        QuestVr::NativeScratchScope scope(nativeQueryScratchBytes_,2u*(sizeof(query)+sizeof(travelQuery)),
            [this](std::size_t bytes){ValidateStateBudget(bytes);});
        query.requireOuterRoot=[this]{if(!transaction_)throw std::runtime_error("SpawnPlayActor tail lost its existing root");};
        query.work=[this]{LifecycleStep();};
        const auto requirePawn=[this,pawn](std::uintptr_t identity) {
            if(identity!=reinterpret_cast<std::uintptr_t>(pawn)||LifecycleActor(identity)!=pawn)
                throw std::runtime_error("SpawnPlayActor tail substituted the captured Login pawn");
        };
        query.writeRole=[&](auto identity,auto value) {
            requirePawn(identity);StartupPlayerScalar(pawn,"Engine.Actor","Role",Kind::Byte,0x20u,"ByteProperty",Value::Byte(value),execution);
        };
        query.writeRemoteRole=[&](auto identity,auto value) {
            requirePawn(identity);StartupPlayerScalar(pawn,"Engine.Actor","RemoteRole",Kind::Byte,0x21u,"ByteProperty",Value::Byte(value),execution);
        };
        query.writeShowFlags=[&](auto identity,auto value) {
            requirePawn(identity);StartupPlayerScalar(pawn,"Engine.PlayerPawn","ShowFlags",Kind::Int,0u,"IntProperty",Value::Integer(value),execution);
        };
        query.writeRendMap=[&](auto identity,auto value) {
            requirePawn(identity);StartupPlayerScalar(pawn,"Engine.PlayerPawn","RendMap",Kind::Int,0u,"IntProperty",Value::Integer(value),execution);
            observed.rolesApplied=true;
        };
        query.appCmdLine=[&]()->std::u16string_view {
            if(!source.tail)throw std::runtime_error("Original SpawnPlayActor appCmdLine service is not prepared");
            if(source.tail->commandLine.size()>65536u)throw std::runtime_error("SpawnPlayActor command line exceeds bound");
            return source.tail->commandLine;
        };
        query.parseParam=[&](auto text,auto parameter){return QuestVr::OriginalParseParam(text,parameter,query.work);};
        query.levelNetDriver=[&](std::uintptr_t identity)->std::uintptr_t {
            if(identity!=reinterpret_cast<std::uintptr_t>(level)||CurrentClockLevel()!=level||!level->nativeStandaloneLevelServices)
                throw std::runtime_error("SpawnPlayActor tail has no actual native standalone NetDriver service");
            return 0u; // Actual loader's explicitly established native driver absence.
        };
        query.setAdminTrue=[&](auto identity) {
            requirePawn(identity);StartupPlayerScalar(pawn,"Engine.PlayerPawn","bAdmin",Kind::Bool,0u,"BoolProperty",Value::Bool(true),execution);
        };
        query.invokeTravelPreAccept=[&](auto identity) {
            requirePawn(identity);StartupTravelPawnEvent(pawn,"TravelPreAccept",execution);observed.preTravelReturned=true;
        };
        QuestVr::RunOriginalSpawnPlayActorPreTravel(reinterpret_cast<std::uintptr_t>(level),
            reinterpret_cast<std::uintptr_t>(pawn),2u,query); // Actual standalone caller role, not URL/config.
        const auto* state=level->nativeLevelTickContext.get();
        if(!state||!state->travel||!state->travel->incomingEstablished)
            throw std::runtime_error("Original SpawnPlayActor native travel map/incoming source is not prepared");
        const auto& travel=*state->travel;
        observed.travelMapCount=travel.entries.size();observed.travelMapOffset=travel.serializedOffset;
        observed.travelIncomingEstablished=travel.incomingEstablished;
        observed.travelIncomingReplacement=travel.incomingReplacement;
        const auto viewBytes=QuestVr::DecalStateDetail::Multiply(travel.entries.size(),sizeof(QuestVr::OriginalSpawnPlayActorTravelEntryView));
        QuestVr::NativeScratchScope viewsScope(nativeQueryScratchBytes_,sizeof(std::vector<QuestVr::OriginalSpawnPlayActorTravelEntryView>)+viewBytes,
            [this](std::size_t bytes){ValidateStateBudget(bytes);});
        std::vector<QuestVr::OriginalSpawnPlayActorTravelEntryView> entries;entries.reserve(travel.entries.size());
        if(entries.capacity()!=travel.entries.size())
            throw std::runtime_error("SpawnPlayActor travel view capacity exceeds admitted storage");
        for(const auto& entry:travel.entries) {LifecycleStep();entries.push_back({entry.key,entry.value});}
        travelQuery.requireOuterRoot=query.requireOuterRoot;travelQuery.work=query.work;
        travelQuery.defaultName=[&]()->std::u16string_view {
            if(!source.tail)throw std::runtime_error("Original SpawnPlayActor FURL.DefaultName service is not prepared");
            if(source.tail->urlDefaultName.size()>1023u)throw std::runtime_error("SpawnPlayActor URL.Name exceeds original GetStr1024 bound");
            return source.tail->urlDefaultName;
        };
        travelQuery.slotZero=[&](std::uintptr_t identity)->std::uintptr_t {
            if(identity!=reinterpret_cast<std::uintptr_t>(level)||CurrentClockLevel()!=level)
                throw std::runtime_error("SpawnPlayActor travel selection lost its actual Level");
            return reinterpret_cast<std::uintptr_t>(persistentWorldActors.empty()?nullptr:persistentWorldActors.front());
        };
        travelQuery.isLevelInfo=[this](auto identity) {
            auto* actor=reinterpret_cast<RuntimeObject*>(identity);
            return actor&&Object(actor->reflection.objectPath)==actor&&IsDerivedFromPath(actor->cls,"Engine.LevelInfo");
        };
        travelQuery.levelInfoNetMode=[this](auto identity) {
            auto* info=LifecycleActor(identity);
            const auto field=OriginalAnimationField(info,"Engine.LevelInfo","NetMode",Kind::Byte,0u,"ByteProperty");
            auto* metadata=Object(field.key);
            if(!metadata->property->referencedType||
                LowerAscii(Qualified(Table(metadata->sourcePath),metadata->property->referencedType))!="engine.levelinfo.enetmode")
                throw std::runtime_error("SpawnPlayActor travel fallback lost original ENetMode declaration");
            return static_cast<std::uint8_t>(OriginalAnimationRead(info,field,Kind::Byte).integer);
        };
        const auto selection=QuestVr::OriginalSpawnPlayActorSelectTravel(reinterpret_cast<std::uintptr_t>(level),url,entries,travelQuery);
        observed.travelLoadOnly=selection.loadOnly;observed.travelPointerPresent=selection.selected.has_value();
        QuestVr::RequireOriginalSpawnPlayActorEmptyTravellers(selection);observed.travelSelected=true;
        StartupTravelGameEvent(level,pawn,"AcceptInventory","Engine.Pawn",execution);observed.acceptInventoryReturned=true;
        observed.postTravelEntered=true;
        StartupTravelPawnEvent(pawn,"TravelPostAccept",execution);observed.postTravelReturned=true;
        StartupTravelGameEvent(level,pawn,"PostLogin","Engine.PlayerPawn",execution);observed.postLoginReturned=true;
        observed.tailReturned=true; // Caller retains the captured pawn; never reads/re-pairs viewport.Actor here.
    }
