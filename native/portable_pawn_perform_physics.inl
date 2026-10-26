// Actual APawn outer method. Shared movement services are reused only at
// their authentic call sites; Actor's outer/rotation algorithms are not.
    std::uintptr_t OriginalPawnPlayerCast(RuntimeObject* actor) {
        actor=LifecycleActor(reinterpret_cast<std::uintptr_t>(actor));auto* target=Object("Engine.PlayerPawn");
        if(!target->classDescriptor||target->defaultOwner||!target->classFieldLinksValid||target->reflection.metaClass!="Class"||
            !(target->reflection.flags&0x04000000u)||target->base!=Object("Engine.Pawn"))
            throw std::runtime_error("Original Pawn Tick lost its actual native PlayerPawn cast target");
        std::array<RuntimeObject*,128u> visited{};std::size_t depth{};
        for(auto* cls=actor->cls;cls;cls=cls->base) {
            LifecycleStep();
            if(depth==visited.size()||std::find(visited.begin(),visited.begin()+depth,cls)!=visited.begin()+depth||
                !cls->classDescriptor||cls->defaultOwner||!cls->classFieldLinksValid)
                throw std::runtime_error("Original Pawn Tick PlayerPawn ancestry is malformed or cyclic");
            visited[depth++]=cls;
            if(cls==target)return reinterpret_cast<std::uintptr_t>(actor);
        }
        return 0u;
    }
    QuestVr::OriginalPawnPerformPhysicsStats RetailPerformPawnPhysics(RuntimeObject* actor,float elapsed,
        QuestVr::Vm::Execution& execution) {
        if(!transaction_)throw std::runtime_error("Original Pawn performPhysics lacks its caller root");
        const auto source=reinterpret_cast<std::uintptr_t>(actor);actor=LifecycleActor(source);
        const auto& native=OriginalActorTickNativeClass(actor)->reflection.objectPath;
        if(native!="DeusEx.ScriptedPawn"&&native!="Engine.Pawn")
            throw std::runtime_error("Original Pawn performPhysics virtual identity is not audited");
        auto* level=ZoneLevel(reinterpret_cast<std::uintptr_t>(actor->nativeXLevel));
        const auto require=[&](auto identity) {
            if(identity!=source)throw std::runtime_error("Original Pawn performPhysics changed its retained source");
            auto* r=LifecycleActor(identity);
            if(r->nativeXLevel!=level)throw std::runtime_error("Original Pawn performPhysics changed its native Level");
            return r;
        };
        const auto scalar=[&](RuntimeObject* r,const char* name,std::uint32_t flags) {
            return OriginalAnimationField(r,"Engine.Pawn",name,Kind::Float,flags,"FloatProperty");
        };
        QuestVr::OriginalPawnPerformPhysicsQueries q;q.work=[this]{LifecycleStep();};
        q.velocity=[&](auto a,unsigned axis) {
            auto* r=require(a);if(axis>=3u)throw std::runtime_error("Original Pawn entry Velocity axis is out of bounds");
            return ReadActorPhysicsVector(r,ActorPhysicsVectorProperty(r,"Engine.Actor.Velocity")).vector[axis];
        };
        q.physics=[&](auto a){auto* r=require(a);return static_cast<std::uint8_t>(OriginalAnimationRead(r,ActorPhysicsProperty(r),Kind::Byte).integer);};
        q.falling=[&](auto a,float delta,std::int32_t iterations) {
            auto* r=require(a);if(iterations)throw std::runtime_error("Original Pawn falling dispatch changed iterations0");
            static_cast<void>(RetailStepActorFalling(r,delta,execution));
        };
        q.otherPhysicsBlock=[&](auto a,QuestVr::OriginalPawnPhysicsDispatch mode,float) {
            static_cast<void>(require(a));
            throw std::runtime_error("Original Pawn performPhysics mode "+std::to_string(static_cast<unsigned>(mode))+" is not implemented");
        };
        q.isPlayerPawnClass=[&](auto a){return OriginalPawnPlayerCast(require(a))!=0u;};
        q.rotation=[&](auto a,unsigned axis){return PhysicsRotatorChannel(require(a),"Engine.Actor.Rotation",0x23u,axis);};
        q.desiredRotation=[&](auto a,unsigned axis){return PhysicsRotatorChannel(require(a),"Engine.Actor.DesiredRotation",0x21u,axis);};
        q.rotationRateRoll=[&](auto a){return PhysicsRotatorChannel(require(a),"Engine.Actor.RotationRate",0x21u,2u);};
        q.physicsRotation=[&](auto a,float delta,std::array<float,3u> oldVelocity) {
            static_cast<void>(RetailPawnPhysicsRotation(require(a),delta,oldVelocity,execution));
        };
        q.moveTimer=[&](auto a){auto* r=require(a);return OriginalAnimationRead(r,scalar(r,"MoveTimer",0u),Kind::Float).floating;};
        q.averagePhysicsTime=[&](auto a){auto* r=require(a);return OriginalAnimationRead(r,scalar(r,"AvgPhysicsTime",2u),Kind::Float).floating;};
        q.writeMoveTimer=[&](auto a,float value){auto* r=require(a);WriteFallingScalar(r,scalar(r,"MoveTimer",0u),Value::Float(value),execution);};
        q.writeAveragePhysicsTime=[&](auto a,float value){auto* r=require(a);WriteFallingScalar(r,scalar(r,"AvgPhysicsTime",2u),Value::Float(value),execution);};
        q.pendingTouch=[&](auto identity) {
            auto* r=LifecycleActor(identity);static_cast<void>(PendingTouchField(r));
            if(!r->nativePendingTouchBound)throw std::runtime_error("Original Pawn PendingTouch lacks initialized typed ownership");
            return reinterpret_cast<std::uintptr_t>(r->nativePendingTouch);
        };
        q.postTouch=[&](auto retained,auto other) {
            static_cast<void>(require(other));
            OriginalNativeActorCallback(LifecycleActor(retained),"PostTouch",Value::Text(Kind::Object,actor->reflection.objectPath),execution);
        };
        q.writePendingTouch=[&](auto owner,auto next) {
            WriteNativePendingTouch(LifecycleActor(owner),next?LifecycleActor(next):nullptr,execution);
        };
        return QuestVr::PerformOriginalPawnPhysics(source,elapsed,q);
    }
