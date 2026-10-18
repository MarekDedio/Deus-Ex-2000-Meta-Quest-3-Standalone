// Included inside PortableActorVmHost. The original receiver-local native
// predicates, not native720's unrelated current-Level GetPlayerPawn fallback.
    Property RetailPredicatePointerField(RuntimeObject* actor,const char* key,const char* ownerPath,
        const char* constraint,std::uint32_t flags) {
        const auto property=DescribeProperty(key);auto* field=Object(property.key);
        if(property.key!=key || !OwnsProperty(actor->cls,property) || property.zero.kind!=Kind::Object ||
            property.arrayDimension!=1u || property.flags!=flags || !field->property ||
            field->property->type!="ObjectProperty" || field->property->baseField ||
            field->outer!=Object(ownerPath) || !field->property->referencedType ||
            LowerAscii(Qualified(Table(field->sourcePath),field->property->referencedType))!=LowerAscii(constraint) ||
            LowerAscii(Qualified(Table(field->sourcePath),Table(field->sourcePath).exports.at(field->exportIndex).ObjClass))!="core.objectproperty")
            throw std::runtime_error("Original actor predicate lacks its exact declared native Object field");
        return property;
    }
    Value RetailPredicatePointerRead(RuntimeObject* actor,const char* key,const char* constraint,std::uint32_t flags) {
        const auto value=Read(actor,RetailPredicatePointerField(actor,key,"Engine.Actor",constraint,flags),0u);
        if(value.kind!=Kind::Object)throw std::runtime_error("Original actor predicate live pointer is not an Object");
        return value;
    }
    QuestVr::OriginalActorBlockingQueries RetailActorBlockingQueries() {
        using Class=QuestVr::OriginalBlockingClass;using Flag=QuestVr::OriginalBlockingFlag;
        QuestVr::OriginalActorBlockingQueries queries;
        queries.level=[this](std::uintptr_t identity) {
            LifecycleStep();auto* actor=RetailCollisionActor(identity);
            const auto value=RetailPredicatePointerRead(actor,"Engine.Actor.Level","Engine.LevelInfo",0x2u);
            return value.text.empty()?std::uintptr_t{}:reinterpret_cast<std::uintptr_t>(Object(value.text));
        };
        queries.hasBrush=[this](std::uintptr_t identity) {
            LifecycleStep();auto* actor=RetailCollisionActor(identity);
            // Presence only: never select GetPrimitive or dereference the Model.
            return !RetailPredicatePointerRead(actor,"Engine.Actor.Brush","Engine.Model",0x2au).text.empty();
        };
        queries.hasPlayer=[this](std::uintptr_t identity) {
            LifecycleStep();auto* actor=RetailCollisionActor(identity);
            const auto property=RetailPredicatePointerField(actor,"Engine.PlayerPawn.Player",
                "Engine.PlayerPawn","Engine.Player",0x2u);
            // Read's public view can hide a revoked native Player. This native
            // helper only tests the raw pointer; it reads no Player payload and
            // does not decide possession or validate a viewport/client here.
            if(actor->nativePlayerBinding)return actor->nativePlayer!=nullptr;
            const auto value=Read(actor,property,0u);
            if(value.kind!=Kind::Object)throw std::runtime_error("Original local Player pointer is not an Object");
            return !value.text.empty();
        };
        queries.isA=[this](std::uintptr_t identity,Class selected) {
            LifecycleStep();auto* actor=RetailCollisionActor(identity);
            const char* path{};
            switch(selected) {
            case Class::Brush:path="Engine.Brush";break;
            case Class::PlayerPawn:path="Engine.PlayerPawn";break;
            case Class::Projectile:path="Engine.Projectile";break;
            default:throw std::runtime_error("Unknown original actor blocking class selector");
            }
            auto* selectedClass=Object(path);
            if(!selectedClass->classDescriptor || selectedClass->defaultOwner)
                throw std::runtime_error("Original actor predicate class is not the actual UClass");
            std::array<RuntimeObject*,128u> visited{};std::size_t depth{};
            for(auto* cls=actor->cls;cls;cls=cls->base) {
                LifecycleStep();
                // Match before following/validating inactive Super links.
                if(cls==selectedClass)return true;
                if(depth==visited.size() || std::find(visited.begin(),visited.begin()+depth,cls)!=visited.begin()+depth)
                    throw std::runtime_error("Original actor predicate class chain cycles or exceeds its work bound");
                if(!cls->classDescriptor || cls->defaultOwner)
                    throw std::runtime_error("Original actor predicate class chain contains a non-UClass");
                visited[depth++]=cls;
            }
            return false;
        };
        queries.flag=[this](std::uintptr_t identity,Flag selected) {
            LifecycleStep();auto* actor=RetailCollisionActor(identity);
            const char* key{};
            switch(selected) {
            case Flag::CollideWorld:key="Engine.Actor.bCollideWorld";break;
            case Flag::BlockActors:key="Engine.Actor.bBlockActors";break;
            case Flag::BlockPlayers:key="Engine.Actor.bBlockPlayers";break;
            default:throw std::runtime_error("Unknown original actor blocking flag selector");
            }
            const auto property=DescribeProperty(key);auto* field=Object(property.key);
            if(property.key!=key || !OwnsProperty(actor->cls,property) || property.zero.kind!=Kind::Bool ||
                property.arrayDimension!=1u || property.flags!=0x21u || !field->property ||
                field->property->type!="BoolProperty" || field->property->baseField || field->property->referencedType ||
                field->outer!=Object("Engine.Actor") ||
                LowerAscii(Qualified(Table(field->sourcePath),Table(field->sourcePath).exports.at(field->exportIndex).ObjClass))!="core.boolproperty")
                throw std::runtime_error("Original actor predicate lacks its exact declared native Bool field");
            const auto value=Read(actor,property,0u);
            if(value.kind!=Kind::Bool)throw std::runtime_error("Original actor predicate live flag is not Bool");
            return value.boolean;
        };
        queries.base=[this](std::uintptr_t identity) {
            LifecycleStep();auto* actor=RetailCollisionActor(identity);
            const auto value=RetailPredicatePointerRead(actor,"Engine.Actor.Base","Engine.Actor",0x22u);
            return value.text.empty()?std::uintptr_t{}:reinterpret_cast<std::uintptr_t>(Object(value.text));
        };
        queries.work=[this]{LifecycleStep();};
        return queries;
    }
    bool RetailIsBlockedBy(RuntimeObject* source,RuntimeObject* candidate) {
        if(!transaction_)throw std::runtime_error("Original IsBlockedBy requires its outer VM root");
        return QuestVr::IsOriginalActorBlockedBy(reinterpret_cast<std::uintptr_t>(source),
            reinterpret_cast<std::uintptr_t>(candidate),RetailActorBlockingQueries());
    }
    bool RetailIsBasedOn(RuntimeObject* source,RuntimeObject* target) {
        if(!transaction_)throw std::runtime_error("Original IsBasedOn requires its outer VM root");
        return QuestVr::IsOriginalActorBasedOn(reinterpret_cast<std::uintptr_t>(source),
            reinterpret_cast<std::uintptr_t>(target),RetailActorBlockingQueries());
    }
