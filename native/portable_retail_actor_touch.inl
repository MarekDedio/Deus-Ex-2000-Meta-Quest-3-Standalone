// Included inside PortableActorVmHost: real qualified Touching slots, original
// event dispatch and the SAME caller VM journal. Never the old touchEventSent
// registry, a render collider, or a forced begun/possessed-world fixture.
    Property RetailTouchingField(RuntimeObject* actor) {
        const auto property=DescribeProperty("Engine.Actor.Touching");auto* field=Object(property.key);
        if(property.key!="Engine.Actor.Touching" || !OwnsProperty(actor->cls,property) ||
            property.zero.kind!=Kind::Object || property.arrayDimension!=4u || property.flags!=0x2u ||
            !field->property || field->property->type!="ObjectProperty" || field->property->baseField ||
            field->outer!=Object("Engine.Actor") || !field->property->referencedType ||
            LowerAscii(Qualified(Table(field->sourcePath),field->property->referencedType))!="engine.actor" ||
            LowerAscii(Qualified(Table(field->sourcePath),Table(field->sourcePath).exports.at(field->exportIndex).ObjClass))!="core.objectproperty")
            throw std::runtime_error("Original Touching lacks its exact declared Actor Object[4] schema");
        return property;
    }
    std::uintptr_t RetailTouchReadSlot(std::uintptr_t identity,unsigned slot) {
        if(slot>=4u)throw std::runtime_error("Original Touching slot is outside its four-element native array");
        auto* actor=RetailCollisionActor(identity);
        const auto value=Read(actor,RetailTouchingField(actor),slot);
        if(value.kind!=Kind::Object)throw std::runtime_error("Original live Touching slot is not an Object");
        if(value.text.empty()||LowerAscii(value.text)=="none")return 0u;
        auto* other=Object(value.text);const auto peer=reinterpret_cast<std::uintptr_t>(other);
        static_cast<void>(RetailCollisionActor(peer));return peer;
    }
    bool RetailTouchClass(std::uintptr_t identity,const char* path) {
        auto* actor=RetailCollisionActor(identity);auto* target=Object(path);
        if(!target->classDescriptor||target->defaultOwner)
            throw std::runtime_error("Original touching class selector is not the actual UClass");
        std::array<RuntimeObject*,128u> visited{};std::size_t depth{};
        for(auto* cls=actor->cls;cls;cls=cls->base) {
            LifecycleStep();if(cls==target)return true;
            if(depth==visited.size()||std::find(visited.begin(),visited.begin()+depth,cls)!=visited.begin()+depth||
                !cls->classDescriptor||cls->defaultOwner)
                throw std::runtime_error("Original touching class chain is unsafe or exceeds its bound");
            visited[depth++]=cls;
        }
        return false;
    }
    std::uint8_t RetailTouchRole(std::uintptr_t identity) {
        auto* actor=RetailCollisionActor(identity);const auto property=DescribeProperty("Engine.Actor.Role");
        auto* field=Object(property.key);
        if(property.key!="Engine.Actor.Role"||!OwnsProperty(actor->cls,property)||property.zero.kind!=Kind::Byte||
            property.arrayDimension!=1u||property.flags!=0x20u||!field->property||
            field->property->type!="ByteProperty"||field->property->baseField||field->outer!=Object("Engine.Actor")||
            !field->property->referencedType||
            LowerAscii(Qualified(Table(field->sourcePath),field->property->referencedType))!="engine.actor.enetrole"||
            LowerAscii(Qualified(Table(field->sourcePath),Table(field->sourcePath).exports.at(field->exportIndex).ObjClass))!="core.byteproperty")
            throw std::runtime_error("Original Role lacks its exact declared Actor ENetRole Byte schema");
        const auto value=Read(actor,property,0u);
        if(value.kind!=Kind::Byte)throw std::runtime_error("Original live Role is not a Byte");
        return static_cast<std::uint8_t>(value.integer); // Raw byte, not an enum-range clamp.
    }
    void RetailTouchEvent(RuntimeObject* actor,RuntimeObject* other,QuestVr::OriginalActorTouchEvent selected,
        QuestVr::Vm::Execution& execution) {
        const char* name{};
        switch(selected) {
        case QuestVr::OriginalActorTouchEvent::Touch:name="Touch";break;
        case QuestVr::OriginalActorTouchEvent::UnTouch:name="UnTouch";break;
        default:throw std::runtime_error("Unknown original native touching event");
        }
        const auto validate=[&](const Function& function,bool base) {
            const auto expected=std::string("engine.actor.")+LowerAscii(name);
            if(base&&(LowerAscii(function.path)!=expected||function.nativeIndex!=0u||
                function.flags!=0x800u||function.variables.size()!=1u))
                throw std::runtime_error("Original touching callback has a different event declaration");
            // Derived/state overrides may have locals and different parameter
            // spelling. The native packet is one Actor, not a one-local rule.
            const Property* argument{};
            for(const auto& member:function.variables)if(member.flags&0x80u) {
                if(argument)throw std::runtime_error("Original touching override has extra parameter/result fields");
                argument=&member;
            }
            if(!argument)throw std::runtime_error("Original touching callback has no Actor parameter");
            const auto& parameter=*argument;auto* field=Object(parameter.key);
            if((base&&LowerAscii(parameter.key)!=expected+".other")||parameter.zero.kind!=Kind::Object||
                parameter.arrayDimension!=1u||parameter.flags!=0x80u||!field->property||field->outer!=Object(function.path)||
                field->property->type!="ObjectProperty"||field->property->baseField||
                !field->property->referencedType||
                LowerAscii(Qualified(Table(field->sourcePath),field->property->referencedType))!="engine.actor"||
                LowerAscii(Qualified(Table(field->sourcePath),Table(field->sourcePath).exports.at(field->exportIndex).ObjClass))!="core.objectproperty")
                throw std::runtime_error("Original touching callback lacks its original Actor Other packet");
        };
        validate(*FunctionFor(Object(std::string("Engine.Actor.")+name)),true);
        // Retail FindFunctionChecked happens BEFORE ProcessEvent's begun/probe/
        // deletion suppression. Authenticate the real selected override first,
        // even when the dormant world's event body will not execute.
        const auto identity=Member(actor,name);
        if(!identity)throw std::runtime_error("Original touching callback lookup found no function");
        validate(*FunctionFor(Object(identity->path)),false);
        const auto scratch=QuestVr::DecalStateDetail::Add(1024u+2u*sizeof(Evaluation),
            QuestVr::DecalStateDetail::Multiply(other->reflection.objectPath.size()+1u,4u));
        QuestVr::NativeScratchScope reservation(nativeQueryScratchBytes_,scratch,
            [this](std::size_t extra){ValidateStateBudget(extra);});
        static_cast<void>(execution.CallEvent(actor->reflection.objectPath,name,true,
            {{Value::Text(Kind::Object,other->reflection.objectPath),{}}}));
    }
    QuestVr::OriginalActorTouchQueries RetailActorTouchQueries(QuestVr::Vm::Execution& execution) {
        QuestVr::OriginalActorTouchQueries queries;
        queries.readSlot=[this](std::uintptr_t identity,unsigned slot){return RetailTouchReadSlot(identity,slot);};
        queries.writeSlot=[this,&execution](std::uintptr_t identity,unsigned slot,std::uintptr_t peer) {
            if(slot>=4u)throw std::runtime_error("Original native Touching write has invalid slot");
            auto* actor=RetailCollisionActor(identity);
            auto* other=peer?RetailCollisionActor(peer):nullptr;
            const auto property=RetailTouchingField(actor);
            execution.WriteReference(ReferenceFor(actor,property,slot,false),
                Value::Text(Kind::Object,other?other->reflection.objectPath:std::string{}));
        };
        queries.requireValid=[this](std::uintptr_t identity){static_cast<void>(RetailCollisionActor(identity));};
        queries.role=[this](std::uintptr_t identity){return RetailTouchRole(identity);};
        queries.pawn=[this](std::uintptr_t identity){return RetailTouchClass(identity,"Engine.Pawn");};
        queries.isPlayer=[this](std::uintptr_t identity) {
            auto* actor=RetailCollisionActor(identity);
            return NoiseField(actor,"Engine.Pawn.bIsPlayer",Kind::Bool,0x20u).boolean;
        };
        queries.event=[this,&execution](std::uintptr_t source,QuestVr::OriginalActorTouchEvent event,std::uintptr_t peer) {
            RetailTouchEvent(RetailCollisionActor(source),RetailCollisionActor(peer),event,execution);
        };
        queries.work=[this]{LifecycleStep();};return queries;
    }
    QuestVr::OriginalActorOverlapQueries RetailActorOverlapQueries() {
        QuestVr::OriginalActorOverlapQueries queries;
        queries.isBrush=[this](std::uintptr_t identity) {
            auto* actor=RetailCollisionActor(identity);
            return !RetailPredicatePointerRead(actor,"Engine.Actor.Brush","Engine.Model",0x2au).text.empty()&&
                RetailTouchClass(identity,"Engine.Brush");
        };
        queries.level=[this](std::uintptr_t identity) {
            auto* actor=RetailCollisionActor(identity);
            const auto value=RetailPredicatePointerRead(actor,"Engine.Actor.Level","Engine.LevelInfo",0x2u);
            return value.text.empty()?std::uintptr_t{}:reinterpret_cast<std::uintptr_t>(Object(value.text));
        };
        queries.location=[this](std::uintptr_t identity,unsigned channel) {
            if(channel>=3u)throw std::runtime_error("Original overlap location channel is invalid");
            return NativeCoordsRead(RetailCollisionActor(identity),"Engine.Actor.Location",Kind::Vector,0x23u).vector[channel];
        };
        const auto dimension=[this](std::uintptr_t identity,const char* key) {
            auto* actor=RetailCollisionActor(identity);
            const auto value=Read(actor,RetailCollisionSetterField(actor,key),0u);
            if(value.kind!=Kind::Float)throw std::runtime_error("Original overlap dimension is not its native Float");
            return value.floating; // Signed sums are squared; no nonnegative Cylinder admission here.
        };
        queries.radius=[dimension](std::uintptr_t identity){return dimension(identity,"Engine.Actor.CollisionRadius");};
        queries.height=[dimension](std::uintptr_t identity){return dimension(identity,"Engine.Actor.CollisionHeight");};
        queries.work=[this]{LifecycleStep();};return queries;
    }
    PortableRetailTouchSnapshot RetailTouchSnapshot(RuntimeObject* source,RuntimeObject* other,
        std::size_t commandIndex,bool sideAccepted) {
        std::array<std::uintptr_t,8u> pointers{};
        for(unsigned i=0u;i<4u;++i) {
            pointers[i]=RetailTouchReadSlot(reinterpret_cast<std::uintptr_t>(source),i);
            pointers[4u+i]=RetailTouchReadSlot(reinterpret_cast<std::uintptr_t>(other),i);
        }
        std::size_t bytes{};
        for(const auto identity:pointers)if(identity)bytes=QuestVr::DecalStateDetail::Add(bytes,
            reinterpret_cast<RuntimeObject*>(identity)->reflection.objectPath.size()+1u+4u*sizeof(void*));
        ReserveRetailDiagnosticOutput(bytes); // Admit all detached strings BEFORE copying.
        PortableRetailTouchSnapshot result;result.commandIndex=commandIndex;result.sideAccepted=sideAccepted;
        for(unsigned i=0u;i<8u;++i)if(pointers[i]) {
            auto& path=i<4u?result.source[i]:result.other[i-4u];
            path=reinterpret_cast<RuntimeObject*>(pointers[i])->reflection.objectPath;
        }
        return result;
    }
