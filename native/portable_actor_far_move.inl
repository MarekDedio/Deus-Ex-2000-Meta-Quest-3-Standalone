// Included inside PortableActorVmHost after actual movement providers. Native
// SetLocation267 uses original FarMove, not direct Location or Move266(delta).
    QuestVr::OriginalWorldLineReply FarWorldPoint(RuntimeObject* level,RuntimeObject* info,
        PortableModelVec3 location,PortableModelVec3 extent) {
        if(!transaction_ || level!=RetailCollisionLevel() || persistentWorldActors.empty() ||
            info!=persistentWorldActors.front() || !info || info->nativeXLevel!=level || !IsDerivedFromPath(info->cls,"Engine.LevelInfo"))
            throw std::runtime_error("Original world PointCheck lacks actual Level/slot-zero LevelInfo");
        const auto& model=NativeTraceModel(info);auto* nativeModel=level->nativeLevelModel;
        const auto& collisionSource=NativeWorldCollisionSource(level);
        if(!nativeModel)throw std::runtime_error("Original world PointCheck native Model disappeared");
        struct ValidationFrame {std::size_t node;unsigned next;};
        struct Visit {std::int32_t node,parent;bool outside;};
        const auto width=std::max(3u*sizeof(ValidationFrame)+sizeof(std::uint8_t),3u*sizeof(Visit));
        QuestVr::NativeScratchScope workspace(nativeQueryScratchBytes_,QuestVr::DecalStateDetail::Add(2048u,
            QuestVr::DecalStateDetail::Multiply(model.nodes.size(),width)),[this](auto extra){ValidateStateBudget(extra);});
        const bool box=!model.nodes.empty()&&!(QuestVr::ModelPointOverlapDetail::ZeroOrUnordered(extent.x)&&
            QuestVr::ModelPointOverlapDetail::ZeroOrUnordered(extent.y)&&QuestVr::ModelPointOverlapDetail::ZeroOrUnordered(extent.z));
        if((fastTraceValidationWork_>=MaximumNativeModelValidationWork&&
            (!box||!collisionSource.HasNullHullValidation()))||fastTraceQueryWork_>=MaximumNativeModelQueryWork)
            throw std::runtime_error("Original world PointCheck shared Model work exhausted");
        QuestVr::ModelPointOverlapLimits limits;
        limits.validationSteps=std::min(limits.validationSteps,MaximumNativeModelValidationWork-fastTraceValidationWork_);
        limits.querySteps=std::min(limits.querySteps,MaximumNativeModelQueryWork-fastTraceQueryWork_);
        QuestVr::ModelPointOverlapStats stats;QuestVr::OriginalWorldLineReply reply;
        try {
            reply.clear=QuestVr::CheckOriginalModelPointOverlap(reply.hit,collisionSource,reinterpret_cast<std::uintptr_t>(nativeModel),
                0u,location,extent,0u,[]()->QuestVr::OriginalModelCoords {
                    throw std::runtime_error("Null-owner world PointCheck unexpectedly requested Owner.ToWorld");
                },limits,&stats);
            fastTraceValidationWork_+=stats.validationSteps;fastTraceQueryWork_+=stats.querySteps;
        }catch(...) {
            fastTraceValidationWork_=MaximumNativeModelValidationWork;fastTraceQueryWork_=MaximumNativeModelQueryWork;throw;
        }
        return reply;
    }
    std::int32_t FarSinglePoint(RuntimeObject* level,QuestVr::OriginalActorLineHit& caller,
        PortableModelVec3 location,PortableModelVec3 extent,std::uint32_t extra,RuntimeObject* info,std::int32_t checkActors) {
        if(!transaction_ || level!=RetailCollisionLevel())throw std::runtime_error("Original point service has no current-Level root");
        const auto id=reinterpret_cast<std::uintptr_t>(level);
        QuestVr::NativeScratchScope frame(nativeQueryScratchBytes_,4096u,[this](auto bytes){ValidateStateBudget(bytes);});
        QuestVr::OriginalSinglePointQueries query;query.work=[this]{LifecycleStep();};
        query.mark=[this,id]{return MoveHitMark(id);};query.pop=[this](auto token)noexcept{MoveHitPop(token);};
        query.packet=[this,id](auto handle){return MoveHitPacket(id,handle);};
        query.multiPoint=[this,level,id](auto point,auto size,auto flags,auto world,auto actors) {
            QuestVr::OriginalMultiPointQueries multi;multi.work=[this]{LifecycleStep();};
            multi.scratchBytes=&nativeQueryScratchBytes_;multi.validate=[this](auto bytes){ValidateStateBudget(bytes);};
            multi.hash=[this,level]{
                if(level!=RetailCollisionLevel())throw std::runtime_error("Original MultiPoint Level changed");
                return level->nativeRetailHash;
            };
            multi.actors=[](auto,auto,auto,auto)->QuestVr::OriginalLineHitArena {
                // Native267's actual FindSpot passes CheckActors0. A broader
                // live-hash query requires its genuine ActorPointCheck service.
                throw std::runtime_error("Original hash ActorPointCheck is not implemented");
            };
            multi.world=[this,level](auto worldId,auto owner,auto at,auto dimensions,auto nativeFlags) {
                if(owner || nativeFlags)throw std::runtime_error("Original world point changed null Owner/literal flags0");
                auto* worldInfo=ZoneActor(worldId,level);return FarWorldPoint(level,worldInfo,at,dimensions);
            };
            auto hits=QuestVr::CollectOriginalMultiPointHits(point,size,flags,world,actors,multi);
            return AdoptMoveHits(id,std::move(hits));
        };
        auto staged=caller;
        const auto clear=QuestVr::CheckOriginalSinglePoint(staged,location,extent,extra,reinterpret_cast<std::uintptr_t>(info),checkActors,query);
        ValidateStateBudget();caller=staged;return clear;
    }
    std::int32_t FarFindSpot(RuntimeObject* level,PortableModelVec3 extent,PortableModelVec3& location,
        std::int32_t checkActors,std::int32_t assumeFit) {
        QuestVr::OriginalLevelFindSpotQueries query;query.work=[this]{LifecycleStep();};
        query.freshLevelInfo=[this,level] {
            const auto identity=MoveLevelActor0(reinterpret_cast<std::uintptr_t>(level));MoveRequireLevelInfo(identity);return identity;
        };
        query.pointCheck=[this,level](auto& hit,auto at,auto size,auto flags,auto info,auto actors) {
            return FarSinglePoint(level,hit,at,size,flags,ZoneActor(info,level),actors);
        };
        query.lineCheck=[this,level](auto& hit,auto source,auto end,auto start,auto size,auto flags,auto extra) {
            auto* actor=source?ZoneActor(source,level):nullptr;
            return RetailSingleLineLevel(level,actor,hit,end,start,size,flags,extra).clear?1:0;
        };
        return QuestVr::FindOriginalLevelSpot(extent,location,checkActors,assumeFit,query);
    }
    void FarWriteOldLocation(RuntimeObject* actor,unsigned channel,float value,QuestVr::Vm::Execution& execution) {
        if(channel>=3u)throw std::runtime_error("Original FarMove OldLocation channel is invalid");
        const auto field=NativeCoordsProperty(actor,"Engine.Actor.OldLocation",Kind::Vector,2u);
        auto reference=std::make_shared<Reference>();reference->zero=Value::Float(0.f);
        reference->read=[this,actor,field,channel]{return Value::Float(Read(actor,field,0u).vector.at(channel));};
        reference->write=[this,actor,field,channel](const Value& value) {
            if(value.kind!=Kind::Float)throw std::runtime_error("Original FarMove OldLocation raw store is not Float");
            auto merged=Read(actor,field,0u);merged.vector.at(channel)=value.floating;
            auto normalized=TypedLiveValue(field,merged);Touch(actor);
            actor->scriptValues[InstanceStorageKey(actor->cls,field)][0u]=std::move(normalized);actor->committedScriptState=true;
        };
        execution.WriteReference(reference,Value::Float(value));ValidateStateBudget();
    }
    std::int32_t FarMoveLevelActor(RuntimeObject* level,RuntimeObject* actor,PortableModelVec3 location,
        std::int32_t test,std::int32_t noCheck,QuestVr::Vm::Execution& execution) {
        if(!transaction_ || nativeMoveDepth_>=64u)throw std::runtime_error("Original FarMove root/depth admission failed");
        const auto id=reinterpret_cast<std::uintptr_t>(level),source=reinterpret_cast<std::uintptr_t>(actor);
        static_cast<void>(ZoneLevel(id));static_cast<void>(ZoneActor(source,level));
        if(level!=RetailCollisionLevel())throw std::runtime_error("Original FarMove has a foreign Level dispatch");
        QuestVr::NativeScratchScope frame(nativeQueryScratchBytes_,8192u,[this](auto bytes){ValidateStateBudget(bytes);});
        ++nativeMoveDepth_;struct Depth {std::size_t& depth;~Depth()noexcept{--depth;}} depth{nativeMoveDepth_};
        QuestVr::OriginalLevelFarMoveQueries q;q.work=[this]{LifecycleStep();};
        q.eligibilityFlags=[this](auto a){auto* actor=RetailCollisionActor(a);
            return (NoiseField(actor,"Engine.Actor.bStatic",Kind::Bool,3u).boolean?1u:0u)|
                (NoiseField(actor,"Engine.Actor.bMovable",Kind::Bool,1u).boolean?0x80000u:0u);};
        q.editor=[](){return std::int32_t{};};
        q.collideWhenPlacing=[this](auto a){return NoiseField(RetailCollisionActor(a),"Engine.Actor.bCollideWhenPlacing",Kind::Bool,1u).boolean;};
        q.collisionFlags=[this](auto a){return MoveCollisionFlags(RetailCollisionActor(a));};
        q.standingCount=[this](auto a){return MoveStandingCount(RetailCollisionActor(a));};
        q.radius=[this](auto a){return NoiseField(RetailCollisionActor(a),"Engine.Actor.CollisionRadius",Kind::Float,0x23u).floating;};
        q.height=[this](auto a){return NoiseField(RetailCollisionActor(a),"Engine.Actor.CollisionHeight",Kind::Float,0x23u).floating;};
        q.levelActor0=[this](auto l){return MoveLevelActor0(l);};q.requireLevelInfo=[this](auto a){MoveRequireLevelInfo(a);};
        q.netMode=[this](auto a){return static_cast<std::uint8_t>(NoiseField(RetailCollisionActor(a),"Engine.LevelInfo.NetMode",Kind::Byte,0u,"Engine.LevelInfo.ENetMode").integer);};
        q.rotation=[this](auto a,unsigned c){return MoveRotation(a,c);};
        q.levelHash=[this](auto l){return ZoneLevel(l)->nativeRetailHash;};
        q.hashRemove=[this,level](auto hash,auto a){
            if(level->nativeRetailHash!=hash)throw std::runtime_error("Original FarMove Remove has foreign hash");
            TouchRetailCollision();RefreshRetailCollisionActor(RetailCollisionActor(a));persistentRetailCollision->Remove(hash,a,RetailCollisionQueries());};
        q.hashAdd=[this,level](auto hash,auto a){
            if(level->nativeRetailHash!=hash)throw std::runtime_error("Original FarMove Add has foreign hash");
            TouchRetailCollision();RefreshRetailCollisionActor(RetailCollisionActor(a));persistentRetailCollision->Add(hash,a,RetailCollisionQueries());};
        q.findSpot=[this](auto l,auto extent,auto& at,auto actors,auto assumeFit){return FarFindSpot(ZoneLevel(l),extent,at,actors,assumeFit);};
        q.encroachment=[this,&execution](auto l,auto a,auto at,auto rotation,auto touch){return MoveLevelEncroachment(l,a,at,rotation,touch,execution)?1:0;};
        q.actorCount=[this](auto l){static_cast<void>(ZoneLevel(l));
            if(persistentWorldActors.size()>65536u)throw std::runtime_error("Original FarMove live Actor array exceeds bound");
            return static_cast<std::int32_t>(persistentWorldActors.size());};
        q.actorAt=[this](auto l,std::int32_t index){static_cast<void>(ZoneLevel(l));
            if(index<0 || static_cast<std::size_t>(index)>=persistentWorldActors.size())throw std::runtime_error("Original FarMove Actor slot outside live array");
            return reinterpret_cast<std::uintptr_t>(persistentWorldActors[static_cast<std::size_t>(index)]);};
        q.base=[this,level](auto a){const auto value=RetailPredicatePointerRead(ZoneActor(a,level),"Engine.Actor.Base","Engine.Actor",0x22u);
            return value.text.empty()?std::uintptr_t{}:reinterpret_cast<std::uintptr_t>(Object(value.text));};
        q.setBase=[this,level,&execution](auto a,auto base,auto send){SetBase(ZoneActor(a,level),base?ZoneActor(base,level):nullptr,send!=0,execution);};
        q.justTeleported=[this,&execution](auto a){auto* actor=RetailCollisionActor(a);
            const auto field=NoiseProperty(actor,"Engine.Actor.bJustTeleported",Kind::Bool,2u);
            auto reference=std::make_shared<Reference>();reference->zero=Value::Bool(false);
            reference->read=[this,actor,field]{return Read(actor,field,0u);};
            reference->write=[this,actor,field](const Value& value){auto normalized=TypedLiveValue(field,value);Touch(actor);
                actor->scriptValues[InstanceStorageKey(actor->cls,field)][0u]=std::move(normalized);actor->committedScriptState=true;};
            execution.WriteReference(reference,Value::Bool(true));ValidateStateBudget();};
        q.writeLocation=[this,&execution](auto a,unsigned c,float v){MoveWriteLocation(RetailCollisionActor(a),c,v,execution);};
        q.writeOldLocation=[this,&execution](auto a,unsigned c,float v){FarWriteOldLocation(RetailCollisionActor(a),c,v,execution);};
        q.setZone=[this,&execution](auto l,auto a,auto test,auto force){
            QuestVr::SetOriginalActorZone(l,a,test,force,ActorZoneQueries(ZoneLevel(l),execution));};
        return QuestVr::MoveOriginalLevelFarActor(id,source,location,test,noCheck,q);
    }
    Evaluation FarMoveNative(RuntimeObject* actor,const std::vector<Evaluation>& arguments,
        const Function* supplied,QuestVr::Vm::Execution& execution) {
        const auto declaration=FunctionFor(Object("Engine.Actor.SetLocation"));
        const auto validate=[this](const Function& f) {
            if(f.path!="Engine.Actor.SetLocation" || f.nativeIndex!=267u || f.flags!=0x401u || f.variables.size()!=2u)
                throw std::runtime_error("SetLocation lacks its actual original native declaration");
            const auto& input=f.variables[0];const auto& result=f.variables[1];
            if(input.key!=f.path+".NewLocation" || input.name!="NewLocation" || input.flags!=0x80u || input.arrayDimension!=1u ||
                input.zero.kind!=Kind::Vector || result.key!=f.path+".ReturnValue" || result.name!="ReturnValue" ||
                result.flags!=0x580u || result.arrayDimension!=1u || result.zero.kind!=Kind::Bool)
                throw std::runtime_error("SetLocation input/result declaration differs from original");
            NativeCoordsVectorSchema(Object(input.key));
        };
        validate(*declaration);if(supplied){validate(*supplied);if(supplied->source!=declaration->source)
            throw std::runtime_error("SetLocation has a foreign source table");}
        if(arguments.size()!=1u)throw std::runtime_error("SetLocation requires one evaluated Vector argument");
        const auto location=arguments.front().Load();if(location.kind!=Kind::Vector)throw std::runtime_error("SetLocation argument is not Vector");
        static_cast<void>(RetailCollisionActor(reinterpret_cast<std::uintptr_t>(actor)));
        const auto moved=FarMoveLevelActor(actor->nativeXLevel,actor,{location.vector[0],location.vector[1],location.vector[2]},0,0,execution);
        return {Value::Bool(moved!=0),{}};
    }
