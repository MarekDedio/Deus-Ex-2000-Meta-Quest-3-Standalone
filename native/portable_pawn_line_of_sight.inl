// Included inside PortableActorVmHost. The original internal C++ LOS service
// reached by PickTarget531: actual Pawn target, checkSight0, ignoreDistancefalse.
// Not a selected native514 call or a replacement collision/presentation world.
    struct PawnLineOfSightOperations {
        PortableActorVmHost& host;RuntimeObject* source;RuntimeObject* target;
        QuestVr::Vm::Execution& execution;
        struct Flags {bool losFlag{},player{};};
        bool TargetIsEnemy() {
            return host.NoiseObjectField(source,"Engine.Pawn.Enemy",0u,"Engine.Pawn")==target;
        }
        PortableModelVec3 SourceLocation() {return host.NoiseLocation(source);}
        PortableModelVec3 TargetLocation() {return host.NoiseLocation(target);}
        bool IsPlayer() {return host.NoisePlayer(source);}
        std::uint8_t Visibility() {
            const auto value=host.NoiseField(target,"Engine.Pawn.Visibility",Kind::Byte,1u).integer;
            if(value<0 || value>255)throw std::runtime_error("Original Pawn LOS Visibility is not a declared Byte");
            return static_cast<std::uint8_t>(value);
        }
        float BaseEyeHeight() {
            return host.NoiseField(source,"Engine.Pawn.BaseEyeHeight",Kind::Float,1u).floating;
        }
        float CollisionHeight() {
            return host.NoiseField(target,"Engine.Actor.CollisionHeight",Kind::Float,0x23u).floating;
        }
        float CollisionRadius() {
            return host.NoiseField(target,"Engine.Actor.CollisionRadius",Kind::Float,0x23u).floating;
        }
        bool SideFlag() {
            return host.NoiseField(source,"Engine.Pawn.bLOSflag",Kind::Bool,2u).boolean;
        }
        Flags DistantFlags() {
            const auto los=SideFlag();const auto player=IsPlayer();return {los,player};
        }
        std::uint32_t Draw15() {
            host.LifecycleStep();
            if(!persistentScriptRandomSeed)persistentScriptRandomSeed=QuestVr::ScriptRandom::InitialSeed;
            return QuestVr::ScriptRandom::Draw15(*persistentScriptRandomSeed);
        }
        bool FastLine(const PortableModelVec3& end,const PortableModelVec3& start) {
            host.LifecycleStep();
            // Uses native source.XLevel.Model, existing shared validation/query
            // budgets and the actual current Level generation. No Actor hash,
            // Trace277 or external/public execution is introduced here.
            return host.NativeFastLine(source,end,start);
        }
        void WritePosition(const char* key,const PortableModelVec3& point) {
            const auto property=host.NoiseProperty(source,key,Kind::Vector,0u,"Core.Object.Vector");
            // A raw host reference, not a guarded native OUT. Journal/count
            // once through the same Execution; a caught failure poisons it.
            execution.WriteReference(host.ReferenceFor(source,property,0u,false),
                Value::Vector({point.x,point.y,point.z}));
        }
        void WriteLastSeeingPos(const PortableModelVec3& point) {
            WritePosition("Engine.Pawn.LastSeeingPos",point);
        }
        void WriteLastSeenPosFromCurrentEnemy() {
            // Retail rereads the source's actual Enemy, not the candidate or
            // a prequery captured position. BSP queries have no script callback.
            auto* enemy=host.NoiseObjectField(source,"Engine.Pawn.Enemy",0u,"Engine.Pawn");
            if(!enemy)throw std::runtime_error("Original Pawn LOS enemy-history store has no current Enemy");
            host.RequireMapActor(enemy);
            if(enemy->nativeXLevel!=source->nativeXLevel)
                throw std::runtime_error("Original Pawn LOS current Enemy leaves its actual native Level");
            WritePosition("Engine.Pawn.LastSeenPos",host.NoiseLocation(enemy));
        }
    };
    bool PawnPickTargetLineOfSight(RuntimeObject* source,RuntimeObject* candidate,QuestVr::Vm::Execution& execution) {
        LifecycleStep();RequireMapActor(source);RequireMapActor(candidate);
        if(!IsDerivedFromPath(source->cls,"Engine.Pawn") || !IsDerivedFromPath(candidate->cls,"Engine.Pawn") ||
            source->nativeXLevel!=candidate->nativeXLevel || NativeLevelInfo(source)!=NativeLevelInfo(candidate))
            throw std::runtime_error("Original PickTarget LOS requires actual Pawns in the same native Level");
        QuestVr::NativeScratchScope scratch(nativeQueryScratchBytes_,sizeof(PawnLineOfSightOperations)+512u,
            [this](auto extra){ValidateStateBudget(extra);});
        PawnLineOfSightOperations operations{*this,source,candidate,execution};
        return QuestVr::CheckOriginalPawnLineOfSightToPawn(operations);
    }
