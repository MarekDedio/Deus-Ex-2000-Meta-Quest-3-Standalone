// Included inside PortableActorVmHost after portable_actor_move_hits.inl.
// Actual current-Level SingleLine service, not the presentation-active public
// anchor wrapper, Actor.Trace, FindBase, support callbacks or a physics Tick.
    struct RetailSingleLineReply {
        bool clear{true},worldItemKnown{true};
        QuestVr::OriginalSingleLineStats stats;
    };
    bool RetailSingleLineFlag(RuntimeObject* actor,QuestVr::OriginalSingleLineFlag selected) {
        const char* key{};
        switch(selected) {
        case QuestVr::OriginalSingleLineFlag::ProjTarget:key="Engine.Actor.bProjTarget";break;
        case QuestVr::OriginalSingleLineFlag::BlockActors:key="Engine.Actor.bBlockActors";break;
        case QuestVr::OriginalSingleLineFlag::BlockPlayers:key="Engine.Actor.bBlockPlayers";break;
        default:throw std::runtime_error("Unknown original SingleLine Bool selector");
        }
        const auto property=DescribeProperty(key);auto* field=Object(property.key);
        if(property.key!=key||!OwnsProperty(actor->cls,property)||property.zero.kind!=Kind::Bool||
            property.arrayDimension!=1u||property.flags!=0x21u||!field->property||
            field->property->type!="BoolProperty"||field->property->baseField||field->property->referencedType||
            field->outer!=Object("Engine.Actor")||
            LowerAscii(Qualified(Table(field->sourcePath),Table(field->sourcePath).exports.at(field->exportIndex).ObjClass))!="core.boolproperty")
            throw std::runtime_error("Original SingleLine flag lacks its exact declared Actor Bool schema");
        const auto value=Read(actor,property,0u);
        if(value.kind!=Kind::Bool)throw std::runtime_error("Original SingleLine live flag is not Bool");
        return value.boolean;
    }
    RetailSingleLineReply RetailSingleLineLevel(RuntimeObject* level,RuntimeObject* source,
        QuestVr::OriginalActorLineHit& caller,PortableModelVec3 end,PortableModelVec3 start,
        PortableModelVec3 extent,std::uint8_t flags,std::uint8_t extraNodeFlags) {
        if(!transaction_||!level||level!=RetailCollisionLevel())
            throw std::runtime_error("Retail SingleLine requires the same actual native Level/VM root");
        const auto levelIdentity=reinterpret_cast<std::uintptr_t>(level);
        static_cast<void>(ZoneLevel(levelIdentity));
        if(source)static_cast<void>(ZoneActor(reinterpret_cast<std::uintptr_t>(source),level));
        // CheckActors==0 does not inspect/create a native hash or consume its
        // initialization RNG merely to own a temporary world-only hit list.
        if((flags&0x17u)&&level->nativeRetailHash)TouchRetailCollision();
        struct Staged {
            RetailSingleLineReply reply;
            QuestVr::OriginalActorLineHit packet;
        };
        const auto perform=[&]() -> Staged {
            QuestVr::NativeScratchScope frame(nativeQueryScratchBytes_,
                sizeof(QuestVr::OriginalSingleLineQueries)+sizeof(QuestVr::OriginalSingleLineResult)+sizeof(Staged)+1024u,
                [this](std::size_t extra){ValidateStateBudget(extra);});
            const auto mark=MoveHitMark(levelIdentity);
            struct MarkScope {
                PortableActorVmHost& host;std::uintptr_t token;
                ~MarkScope()noexcept{host.MoveHitPop(token);}
            } marked{*this,mark};
            Staged staged;std::memcpy(&staged.packet,&caller,sizeof(caller));
            bool worldItemKnown=true;std::uintptr_t world{};
            // Declared before the result: on failure the owning arena dies
            // first, then its nonallocating reservation. The returned vector
            // remains charged during every lazy filter/provider admission.
            std::optional<QuestVr::NativeScratchScope> incoming;
            QuestVr::OriginalSingleLineQueries query;
            query.levelActor0=[this,level,levelIdentity,&world] {
                static_cast<void>(ZoneLevel(levelIdentity));
                if(persistentWorldActors.empty())
                    throw std::runtime_error("Original SingleLine slot0 is outside the actual native Actor array");
                auto* actor=persistentWorldActors.front();world=reinterpret_cast<std::uintptr_t>(actor);
                if(actor)static_cast<void>(ZoneActor(world,level));
                return world; // The selector next authenticates actual LevelInfo ancestry.
            };
            query.multiLine=[this,level,&incoming,&worldItemKnown](PortableModelVec3 queryEnd,
                PortableModelVec3 queryStart,PortableModelVec3 queryExtent,std::int32_t checkActors,
                std::uintptr_t suppliedWorld,std::uint8_t extra) {
                if(incoming)throw std::runtime_error("Original SingleLine repeated its one owning collector call");
                auto* info=suppliedWorld?ZoneActor(suppliedWorld,level):nullptr;
                auto reply=RetailMultiLineLevel(level,queryEnd,queryStart,queryExtent,checkActors,info,extra);
                const auto bytes=QuestVr::DecalStateDetail::Multiply(reply.result.hits.records.capacity(),
                    sizeof(QuestVr::OriginalActorLineHit));
                incoming.emplace(nativeQueryScratchBytes_,bytes,
                    [this](std::size_t extraBytes){ValidateStateBudget(extraBytes);});
                worldItemKnown=reply.worldItemKnown;
                return std::move(reply.result.hits);
            };
            query.owner=[this,level](std::uintptr_t identity) {
                auto* actor=ZoneActor(identity,level);
                const auto value=RetailPredicatePointerRead(actor,"Engine.Actor.Owner","Engine.Actor",0x22u);
                if(value.text.empty())return std::uintptr_t{};
                auto* owner=Object(value.text);const auto result=reinterpret_cast<std::uintptr_t>(owner);
                static_cast<void>(ZoneActor(result,level));return result;
            };
            query.isA=[this](std::uintptr_t identity,QuestVr::OriginalSingleLineClass selected) {
                const char* path{};
                switch(selected) {
                case QuestVr::OriginalSingleLineClass::LevelInfo:path="Engine.LevelInfo";break;
                case QuestVr::OriginalSingleLineClass::Pawn:path="Engine.Pawn";break;
                case QuestVr::OriginalSingleLineClass::Mover:path="Engine.Mover";break;
                case QuestVr::OriginalSingleLineClass::ZoneInfo:path="Engine.ZoneInfo";break;
                default:throw std::runtime_error("Unknown original SingleLine native class selector");
                }
                return RetailTouchClass(identity,path); // Bounded live UClass pointers, not leaf names.
            };
            query.flag=[this,level](std::uintptr_t identity,QuestVr::OriginalSingleLineFlag selected) {
                return RetailSingleLineFlag(ZoneActor(identity,level),selected);
            };
            query.work=[this]{LifecycleStep();};
            auto selected=QuestVr::CheckOriginalSingleLine(staged.packet,reinterpret_cast<std::uintptr_t>(source),
                end,start,extent,flags,extraNodeFlags,query);
            staged.reply={selected.clear,selected.clear||staged.packet.actor!=world||worldItemKnown,selected.stats};
            // Exact accounting handoff: the next admission is AdoptMoveHits'
            // own incoming-capacity reservation. Its preceding identity checks
            // dispatch no providers or state-budget admission. No duplicate
            // vector allocation or invented second retained peak is charged.
            selected.hits.first=selected.selected;
            incoming.reset();
            const auto head=AdoptMoveHits(levelIdentity,std::move(selected.hits));
            if(selected.clear) {
                if(head)throw std::runtime_error("Original SingleLine miss retained a selected hit handle");
            }else {
                if(!head)throw std::runtime_error("Original SingleLine accepted hit lost its qualified owning handle");
                const auto packet=MoveHitPacket(levelIdentity,head);
                std::memcpy(&staged.packet,&packet,sizeof(packet));
            }
            ValidateStateBudget();
            return staged; // matching mark pops BEFORE returning to any future script callback.
        };
        const auto staged=[&] {
            if(retailCollisionRoot_) {
                if(!persistentRetailCollision)
                    throw std::runtime_error("Original SingleLine lost its enclosing native collision root");
                return persistentRetailCollision->WithRoot([&](QuestVr::OriginalCollisionHashWorld&){return perform();});
            }
            return perform();
        }();
        // Every throwing provider, adoption, lifetime check and root admission
        // has completed. A hit's copied Next is deliberately already expired
        // after Pop, like retail's temporary pointer: NEVER traverse it as a
        // capability after return. FindBase needs only the copied Actor word.
        if(staged.reply.clear){caller.actor=0u;caller.time=1.0f;}
        else std::memcpy(&caller,&staged.packet,sizeof(caller));
        return staged.reply;
    }
