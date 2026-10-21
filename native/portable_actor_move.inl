// Included inside PortableActorVmHost after move pose/hit/event providers.
// Complete recursive native movement on the retained actual Level and SAME
// Execution/hash root. No direct-transform/unobstructed/zero-only substitute.
    std::size_t nativeMoveDepth_{};
    std::vector<RuntimeObject*> nativeDurableMoved_;
    // A REAL CPU-owned constructor VALUE, not the original mutable GMath
    // address or a fabricated integer token. Profile domain is documented in
    // ORIGINAL-SINE-PROFILE; UnitCoords words are Core10144130..10144386.
    const QuestVr::OriginalModelCoords nativeMoveUnitCoords_=QuestVr::IdentityOriginalModelCoords();
    void MarkNativeDurableMove(RuntimeObject* actor) {
        if(std::find(nativeDurableMoved_.begin(),nativeDurableMoved_.end(),actor)!=nativeDurableMoved_.end())return;
        if(nativeDurableMoved_.size()>=4096u)throw std::runtime_error("Native movement durable actor bound exceeded");
        if(nativeDurableMoved_.size()==nativeDurableMoved_.capacity()){
            const auto next=std::max<std::size_t>(8u,nativeDurableMoved_.size()*2u);
            ValidateStateBudget(QuestVr::DecalStateDetail::Multiply(next,sizeof(RuntimeObject*)));
            nativeDurableMoved_.reserve(next);
        }
        nativeDurableMoved_.push_back(actor);
    }
    void SynchronizeNativeMoveMirror() {
        if(nativeActorPoses_.Depth()||nativeMoveDepth_)
            throw std::runtime_error("Native movement cannot publish through an active temporary pose/frame");
        if(nativeDurableMoved_.empty())return;
        TouchCollision();
        for(auto* actor:nativeDurableMoved_){
            LifecycleStep();
            // Derived older presentation/placement cache ONLY. Never rebuild,
            // normalize or repair the original retail hash/ColLocation here.
            if(actor->worldRemoved)persistentActorCollision.Remove(reinterpret_cast<std::uintptr_t>(actor));
            else RegisterCollisionActor(actor,persistentActorCollision);
        }
        ValidateStateBudget();
    }
    std::uint8_t MoveCollisionFlags(RuntimeObject* actor) {
        std::uint8_t flags{};
        if(NoiseField(actor,"Engine.Actor.bCollideActors",Kind::Bool,0x23u).boolean)flags|=1u;
        if(NoiseField(actor,"Engine.Actor.bCollideWorld",Kind::Bool,0x21u).boolean)flags|=2u;
        if(NoiseField(actor,"Engine.Actor.bBlockActors",Kind::Bool,0x21u).boolean)flags|=4u;
        if(NoiseField(actor,"Engine.Actor.bBlockPlayers",Kind::Bool,0x21u).boolean)flags|=8u;
        return flags; // One CPU owner: no callback runs between native Bool reads.
    }
    std::uint8_t MoveStandingCount(RuntimeObject* actor) {
        return static_cast<std::uint8_t>(NoiseField(actor,"Engine.Actor.StandingCount",Kind::Byte,2u).integer);
    }
    float MoveLocation(std::uintptr_t source,unsigned channel) {
        if(channel>=3u)throw std::runtime_error("Original native movement Location channel is invalid");
        return NativeCoordsRead(RetailCollisionActor(source),"Engine.Actor.Location",Kind::Vector,0x23u).vector[channel];
    }
    std::int32_t MoveRotation(std::uintptr_t source,unsigned channel,bool view=false) {
        if(channel>=3u)throw std::runtime_error("Original native movement Rotation channel is invalid");
        return NativeCoordsRead(RetailCollisionActor(source),view?"Engine.Pawn.ViewRotation":"Engine.Actor.Rotation",
            Kind::Rotator,view?0x20u:0x23u).rotation[channel];
    }
    std::uintptr_t MoveLevelActor0(std::uintptr_t level) {
        static_cast<void>(ZoneLevel(level));
        if(persistentWorldActors.empty())throw std::runtime_error("Original native movement slot0 lies outside actual Actor array");
        return reinterpret_cast<std::uintptr_t>(persistentWorldActors.front());
    }
    void MoveRequireLevelInfo(std::uintptr_t actor) {
        if(!RetailTouchClass(actor,"Engine.LevelInfo"))throw std::runtime_error("Original native movement slot0 is not LevelInfo");
    }
    bool MoveOverlaps(std::uintptr_t source,std::uintptr_t peer) {
        return QuestVr::IsOriginalActorOverlapping(source,peer,RetailActorOverlapQueries());
    }
    void MoveCarryChildren(std::uintptr_t level,std::uintptr_t source,PortableModelVec3 delta,
        std::array<std::int32_t,3u> rotation,std::int32_t test,QuestVr::Vm::Execution& execution) {
        QuestVr::OriginalActorChildCarryQueries q;
        q.standingCount=[this](std::uintptr_t a){return MoveStandingCount(RetailCollisionActor(a));};
        q.actorCount=[this](std::uintptr_t l){
            static_cast<void>(ZoneLevel(l));
            if(persistentWorldActors.size()>65536u)throw std::runtime_error("Native child carry actor array bound exceeded");
            return static_cast<std::int32_t>(persistentWorldActors.size());
        };
        q.actorSlot=[this](std::uintptr_t l,std::int32_t slot){
            static_cast<void>(ZoneLevel(l));
            if(slot<0||static_cast<std::size_t>(slot)>=persistentWorldActors.size())throw std::runtime_error("Native child carry slot is outside live array");
            return reinterpret_cast<std::uintptr_t>(persistentWorldActors[static_cast<std::size_t>(slot)]);
        };
        q.base=[this](std::uintptr_t a){
            const auto value=RetailPredicatePointerRead(RetailCollisionActor(a),"Engine.Actor.Base","Engine.Actor",0x22u);
            return value.text.empty()?std::uintptr_t{}:reinterpret_cast<std::uintptr_t>(Object(value.text));
        };
        q.rotation=[this](std::uintptr_t a,unsigned c){return MoveRotation(a,c);};
        q.location=[this](std::uintptr_t a,unsigned c){return MoveLocation(a,c);};
        q.mathRoot=[this]{return reinterpret_cast<std::uintptr_t>(&nativeMoveUnitCoords_);};
        q.unitCoords=[this](std::uintptr_t math){
            if(math!=reinterpret_cast<std::uintptr_t>(&nativeMoveUnitCoords_))throw std::runtime_error("Native child carry math VALUE authority changed");
            return nativeMoveUnitCoords_;
        };
        q.moveActor=[this,&execution](QuestVr::OriginalActorChildMove move,QuestVr::OriginalActorLineHit& hit){
            return MoveLevelActor(move.level,move.actor,move.delta,move.rotation,hit,move.options,execution)?1:0;
        };
        q.pawn=[this](std::uintptr_t a){return RetailTouchClass(a,"Engine.Pawn");};
        q.viewRotation=[this](std::uintptr_t a,unsigned c){return MoveRotation(a,c,true);};
        q.writeViewRotation=[this,&execution](std::uintptr_t a,unsigned c,std::int32_t v){MoveWriteRotation(RetailCollisionActor(a),c,v,execution,true);};
        q.work=[this]{LifecycleStep();};
        static_cast<void>(QuestVr::CarryOriginalActorChildren(level,source,delta,rotation,test,QuestVr::OriginalSineProfile::Table(),q));
    }
    std::int32_t MoveActorSmooth(std::uintptr_t source,PortableModelVec3 delta,QuestVr::Vm::Execution& execution) {
        QuestVr::OriginalActorSmoothQueries q;
        q.rotationChannel=[this](std::uintptr_t a,unsigned c){return MoveRotation(a,c);};
        q.xLevel=[this](std::uintptr_t a){return reinterpret_cast<std::uintptr_t>(RetailCollisionActor(a)->nativeXLevel);};
        q.moveActor=[this,&execution](QuestVr::OriginalActorSmoothMove move,QuestVr::OriginalActorLineHit& hit){
            // Current loader-owned ULevel has the audited native dispatch; no
            // reflected Actor.Level is substituted for either sampled pointer.
            static_cast<void>(ZoneLevel(move.dispatchLevel));
            return MoveLevelActor(move.level,move.actor,move.delta,move.rotation,hit,move.options,execution)?1:0;
        };
        q.hitWall=[this,&execution](std::uintptr_t a,PortableModelVec3 normal,std::uintptr_t wall){
            MoveHitWall(RetailCollisionActor(a),normal,wall?RetailCollisionActor(wall):nullptr,execution);
        };
        q.work=[this]{LifecycleStep();};return QuestVr::MoveOriginalActorSmooth(source,delta,q);
    }
    bool MoveLevelEncroachment(std::uintptr_t level,std::uintptr_t source,PortableModelVec3 proposed,
        std::array<std::int32_t,3u> rotation,std::int32_t touch,QuestVr::Vm::Execution& execution) {
        // Admission precedes mark/hash access in the original. Authenticating
        // a native Actor/Level binding must not eagerly demand an Engine or
        // Cylinder when the complete mask0D/moving-brush gate returns false.
        // Sources here are trusted same-host calls; actual query/hit providers
        // below retain their stronger live lookup/hash identity admission.
        auto* admitted=reinterpret_cast<RuntimeObject*>(source);
        if(!admitted || !level) throw std::runtime_error("Original encroachment has null native arguments");
        RequireMapActor(admitted);
        if(Object(admitted->reflection.objectPath)!=admitted ||
            reinterpret_cast<std::uintptr_t>(admitted->nativeXLevel)!=level)
            throw std::runtime_error("Original encroachment source belongs to another native Level");
        const auto admission=[&](std::uintptr_t identity) {
            if(identity!=source) throw std::runtime_error("Original encroachment admission changed its retained source");
            return admitted;
        };
        QuestVr::OriginalLevelEncroachmentQueries q;
        std::optional<QuestVr::NativeActorPoseStack::LocationScope> temporary;
        q.admissionFlags=[this,&admission](std::uintptr_t a){return MoveCollisionFlags(admission(a));};
        q.movingBrush=[this,&admission,source](std::uintptr_t a){
            return RetailMovingBrush(a==source?admission(a):RetailCollisionActor(a));
        };
        q.levelHash=[this](std::uintptr_t l){return ZoneLevel(l)->nativeRetailHash;};
        q.mark=[this,level]{return MoveHitMark(level);};q.popMark=[this](std::uintptr_t m)noexcept{MoveHitPop(m);};
        q.hashEncroachment=[this](std::uintptr_t hash,std::uintptr_t a,PortableModelVec3 p,std::array<std::int32_t,3u> r,std::uint32_t flags){
            if(flags)throw std::runtime_error("Original higher encroachment changed its literal final0");
            return MoveEncroachmentHits(hash,a,p,r);
        };
        q.hitActor=[this,level](std::uintptr_t h){return MoveHitActor(level,h);};q.hitNext=[this,level](std::uintptr_t h){return MoveHitNext(level,h);};
        q.levelActor0=[this](std::uintptr_t l){return MoveLevelActor0(l);};q.requireLevelInfo=[this](std::uintptr_t a){MoveRequireLevelInfo(a);};
        q.blockedBy=[this](std::uintptr_t a,std::uintptr_t b){return RetailIsBlockedBy(RetailCollisionActor(a),RetailCollisionActor(b));};
        q.locationChannel=[this](std::uintptr_t a,unsigned c){return MoveLocation(a,c);};
        q.writeLocationChannel=[this,source,&temporary](std::uintptr_t a,unsigned c,std::uint32_t word){
            if(a!=source||c>=3u)throw std::runtime_error("Reverse push changed its retained source/raw channel");
            if(!temporary)temporary.emplace(nativeActorPoses_,a,NativeCoordsRead(RetailCollisionActor(a),"Engine.Actor.Location",Kind::Vector,0x23u).vector);
            nativeActorPoses_.WriteLocation(nativeActorPoses_.CaptureLocationWrite(a),c,std::bit_cast<float>(word));
        };
        q.restoreLocation=[&temporary](std::uintptr_t,std::array<std::uint32_t,3u>)noexcept{
            // All writes targeted this nearest mutable layer. Releasing it
            // exposes the saved raw source Location but never restores Rotation,
            // repairs hash/ColLocation, or undoes candidate reverse movement.
            temporary.reset();
        };
        q.moveSmooth=[this,&execution](std::uintptr_t a,PortableModelVec3 d){return MoveActorSmooth(a,d,execution);};
        q.encroachingOn=[this,&execution](std::uintptr_t a,std::uintptr_t b){return MoveEncroachingOn(RetailCollisionActor(a),b?RetailCollisionActor(b):nullptr,execution);};
        q.encroachedBy=[this,&execution](std::uintptr_t a,std::uintptr_t b){MoveEncroachedBy(RetailCollisionActor(a),b?RetailCollisionActor(b):nullptr,execution);};
        q.touchSlot=[this](std::uintptr_t a,unsigned i){return RetailTouchReadSlot(a,i);};
        q.overlaps=[this](std::uintptr_t a,std::uintptr_t b){return MoveOverlaps(a,b);};
        q.endTouch=[this,&execution](std::uintptr_t a,std::uintptr_t b,std::int32_t suppress){QuestVr::EndOriginalActorTouch(a,b,suppress,RetailActorTouchQueries(execution));};
        q.beginTouch=[this,&execution](std::uintptr_t a,std::uintptr_t b){QuestVr::BeginOriginalActorTouch(a,b,RetailActorTouchQueries(execution));};
        q.work=[this]{LifecycleStep();};
        return QuestVr::CheckOriginalLevelEncroachment(level,source,proposed,rotation,touch,q);
    }
    bool MoveLevelActor(std::uintptr_t level,std::uintptr_t source,PortableModelVec3 delta,
        std::array<std::int32_t,3u> rotation,QuestVr::OriginalActorLineHit& hit,
        std::array<std::int32_t,4u> options,QuestVr::Vm::Execution& execution) {
        if(!transaction_||nativeMoveDepth_>=64u)throw std::runtime_error("Native movement root/depth admission failed");
        auto* actual=ZoneLevel(level);static_cast<void>(ZoneActor(source,actual));
        if(actual!=RetailCollisionLevel())throw std::runtime_error("Native movement lost its original current-Level dispatch");
        QuestVr::NativeScratchScope frame(nativeQueryScratchBytes_,16u<<10u,[this](std::size_t extra){ValidateStateBudget(extra);});
        ++nativeMoveDepth_;struct Depth{std::size_t& value;~Depth()noexcept{--value;}} depth{nativeMoveDepth_};
        QuestVr::OriginalLevelMoveActorQueries q;
        q.eligibilityFlags=[this](std::uintptr_t a){auto* actor=RetailCollisionActor(a);
            const auto fixed=NoiseField(actor,"Engine.Actor.bStatic",Kind::Bool,3u).boolean;
            const auto movable=NoiseField(actor,"Engine.Actor.bMovable",Kind::Bool,1u).boolean;
            return (fixed?1u:0u)|(movable?0x80000u:0u);};
        q.editor=[](){return std::int32_t{};}; // This host is standalone game mode, never an editor executor.
        q.staticFlag=[this](std::uintptr_t a){return RetailCollisionFlag(RetailCollisionActor(a),"Engine.Actor.bStatic");};
        q.hasBrush=[this](std::uintptr_t a){return !RetailPredicatePointerRead(RetailCollisionActor(a),"Engine.Actor.Brush","Engine.Model",0x2au).text.empty();};
        q.isA=[this](std::uintptr_t a,QuestVr::OriginalMoveActorClass c){
            const char* name=c==QuestVr::OriginalMoveActorClass::Brush?"Engine.Brush":c==QuestVr::OriginalMoveActorClass::Pawn?"Engine.Pawn":"Engine.Decoration";
            return RetailTouchClass(a,name);};
        q.collisionFlags=[this](std::uintptr_t a){return MoveCollisionFlags(RetailCollisionActor(a));};
        q.standingCount=[this](std::uintptr_t a){return MoveStandingCount(RetailCollisionActor(a));};
        q.rotation=[this](std::uintptr_t a,unsigned c){return MoveRotation(a,c);};
        q.location=[this](std::uintptr_t a,unsigned c){return MoveLocation(a,c);};
        q.writeRotation=[this,&execution](std::uintptr_t a,unsigned c,std::int32_t v){MoveWriteRotation(RetailCollisionActor(a),c,v,execution);};
        q.writeLocation=[this,&execution](std::uintptr_t a,unsigned c,float v){MoveWriteLocation(RetailCollisionActor(a),c,v,execution);};
        q.radius=[this](std::uintptr_t a){return NoiseField(RetailCollisionActor(a),"Engine.Actor.CollisionRadius",Kind::Float,0x23u).floating;};
        q.height=[this](std::uintptr_t a){return NoiseField(RetailCollisionActor(a),"Engine.Actor.CollisionHeight",Kind::Float,0x23u).floating;};
        q.levelActor0=[this](std::uintptr_t l){return MoveLevelActor0(l);};q.requireLevelInfo=[this](std::uintptr_t a){MoveRequireLevelInfo(a);};
        q.mark=[this,level]{return MoveHitMark(level);};q.popMark=[this](std::uintptr_t m)noexcept{MoveHitPop(m);};
        q.multiLine=[this,level](QuestVr::OriginalMoveActorSweep sweep){return MoveMultiLineHits(level,sweep);};
        q.hitActor=[this,level](std::uintptr_t h){return MoveHitActor(level,h);};q.hitNext=[this,level](std::uintptr_t h){return MoveHitNext(level,h);};
        q.hitTime=[this,level](std::uintptr_t h){return MoveHitTime(level,h);};q.hitPacket=[this,level](std::uintptr_t h){return MoveHitPacket(level,h);};
        q.basedOn=[this](std::uintptr_t a,std::uintptr_t b){return RetailIsBasedOn(RetailCollisionActor(a),b?RetailCollisionActor(b):nullptr);};
        q.blockedBy=[this](std::uintptr_t a,std::uintptr_t b){return RetailIsBlockedBy(RetailCollisionActor(a),RetailCollisionActor(b));};
        q.overlaps=[this](std::uintptr_t a,std::uintptr_t b){return MoveOverlaps(a,b);};
        q.carryChildren=[this,&execution](std::uintptr_t l,std::uintptr_t a,PortableModelVec3 d,std::array<std::int32_t,3u> r,std::int32_t t){MoveCarryChildren(l,a,d,r,t,execution);};
        q.encroachment=[this,&execution](std::uintptr_t l,std::uintptr_t a,PortableModelVec3 p,std::array<std::int32_t,3u> r,std::int32_t t){return MoveLevelEncroachment(l,a,p,r,t,execution)?1:0;};
        q.levelHash=[this](std::uintptr_t l){return ZoneLevel(l)->nativeRetailHash;};
        q.hashRemove=[this,actual](std::uintptr_t hash,std::uintptr_t a){
            if(actual->nativeRetailHash!=hash)throw std::runtime_error("Native Move Remove has a foreign actual hash");
            TouchRetailCollision();auto* actor=RetailCollisionActor(a);RefreshRetailCollisionActor(actor);
            persistentRetailCollision->Remove(hash,a,RetailCollisionQueries());};
        q.hashAdd=[this,actual](std::uintptr_t hash,std::uintptr_t a){
            if(actual->nativeRetailHash!=hash)throw std::runtime_error("Native Move Add has a foreign actual hash");
            TouchRetailCollision();auto* actor=RetailCollisionActor(a);RefreshRetailCollisionActor(actor);
            persistentRetailCollision->Add(hash,a,RetailCollisionQueries());};
        q.bumpWall=[this,&execution](std::uintptr_t a,PortableModelVec3 l,PortableModelVec3 n){MoveBumpWall(RetailCollisionActor(a),l,n,execution);};
        q.bump=[this,&execution](std::uintptr_t a,std::uintptr_t b){MoveBump(RetailCollisionActor(a),b?RetailCollisionActor(b):nullptr,execution);};
        q.touching=[this](std::uintptr_t a,unsigned i){return RetailTouchReadSlot(a,i);};
        q.beginTouch=[this,&execution](std::uintptr_t a,std::uintptr_t b){QuestVr::BeginOriginalActorTouch(a,b,RetailActorTouchQueries(execution));};
        q.endTouch=[this,&execution](std::uintptr_t a,std::uintptr_t b,std::int32_t suppress){QuestVr::EndOriginalActorTouch(a,b,suppress,RetailActorTouchQueries(execution));};
        q.setZone=[this,&execution](std::uintptr_t l,std::uintptr_t a,std::int32_t test,std::int32_t force){
            auto* levelObject=ZoneLevel(l);QuestVr::SetOriginalActorZone(l,a,test,force,ActorZoneQueries(levelObject,execution));};
        q.work=[this]{LifecycleStep();};
        return QuestVr::MoveOriginalLevelActor(level,source,delta,rotation,hit,options,q);
    }
    Evaluation MoveNative(std::uint16_t index,RuntimeObject* actor,const std::vector<Evaluation>& arguments,
        const Function* supplied,QuestVr::Vm::Execution& execution) {
        if(index!=266u&&index!=299u)throw std::runtime_error("Unknown original movement wrapper");
        const std::string path=index==266u?"Engine.Actor.Move":"Engine.Actor.SetRotation";
        const auto declaration=FunctionFor(Object(path));
        const auto validate=[&](const Function& fn){
            if(fn.path!=path||fn.nativeIndex!=index||fn.flags!=0x401u||fn.variables.size()!=2u)
                throw std::runtime_error("Original movement wrapper declaration is not its actual UFunction");
            const auto& input=fn.variables[0];const auto& result=fn.variables[1];
            const char* name=index==266u?"Delta":"NewRotation";
            if(input.key!=path+'.'+name||input.name!=name||input.flags!=0x80u||input.arrayDimension!=1u||
                input.zero.kind!=(index==266u?Kind::Vector:Kind::Rotator)||result.key!=path+".ReturnValue"||
                result.name!="ReturnValue"||result.flags!=0x580u||result.arrayDimension!=1u||result.zero.kind!=Kind::Bool)
                throw std::runtime_error("Original Move/SetRotation input/result scalar packet differs");
            auto* field=Object(input.key);
            if(index==266u)NativeCoordsVectorSchema(field);else NativeCoordsRotatorSchema(field);
        };
        validate(*declaration);if(supplied)validate(*supplied);
        if(arguments.size()!=1u)throw std::runtime_error("Original Move/SetRotation requires one evaluated argument");
        const auto input=arguments[0].Load();PortableModelVec3 delta{};std::array<std::int32_t,3u> rotation;
        if(index==266u){
            if(input.kind!=Kind::Vector)throw std::runtime_error("Original Move Delta is not Vector");
            delta={input.vector[0u],input.vector[1u],input.vector[2u]};
            // Native266 samples Rotation AFTER evaluating/finishing arguments.
            rotation=NativeCoordsRead(actor,"Engine.Actor.Rotation",Kind::Rotator,0x23u).rotation;
        }else{
            if(input.kind!=Kind::Rotator)throw std::runtime_error("Original SetRotation NewRotation is not Rotator");
            rotation=input.rotation;
        }
        static_cast<void>(RetailCollisionActor(reinterpret_cast<std::uintptr_t>(actor)));
        QuestVr::OriginalActorLineHit hit;
        const auto moved=MoveLevelActor(reinterpret_cast<std::uintptr_t>(actor->nativeXLevel),
            reinterpret_cast<std::uintptr_t>(actor),delta,rotation,hit,{},execution);
        return {Value::Bool(moved),{}};
    }
