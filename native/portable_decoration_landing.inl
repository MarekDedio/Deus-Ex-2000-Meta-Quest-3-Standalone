// Actual native fields, collision queries and selected script event on the
// SAME caller root. Not a guessed floor snap or an independently committed Tick.
    Property LandingScalarProperty(RuntimeObject* actor,const char* key,const char* owner,
        Kind kind,std::uint32_t flags) {
        const auto property=DescribeProperty(key);auto* field=Object(property.key);
        const char* type=kind==Kind::Bool?"BoolProperty":"IntProperty";
        if(property.key!=key || !OwnsProperty(actor->cls,property) || property.zero.kind!=kind ||
            property.flags!=flags || property.arrayDimension!=1u || !field->property ||
            field->property->type!=type || field->property->baseField || field->property->referencedType ||
            field->outer!=Object(owner) ||
            LowerAscii(Qualified(Table(field->sourcePath),Table(field->sourcePath).exports.at(field->exportIndex).ObjClass))!=
                std::string("core.")+LowerAscii(type))
            throw std::runtime_error("Original landing scalar lacks its actual declared schema");
        return property;
    }
    Property LandingZoneVelocityProperty(RuntimeObject* zone) {
        if(!RetailTouchClass(reinterpret_cast<std::uintptr_t>(zone),"Engine.ZoneInfo"))
            throw std::runtime_error("Original landing zone is not actual ZoneInfo");
        const auto field=NativeCoordsProperty(zone,"Engine.ZoneInfo.ZoneVelocity",Kind::Vector,0x21u);
        auto* node=Object(field.key);
        if(node->outer!=Object("Engine.ZoneInfo") || !node->property || node->property->baseField ||
            LowerAscii(Qualified(Table(node->sourcePath),Table(node->sourcePath).exports.at(node->exportIndex).ObjClass))!="core.structproperty")
            throw std::runtime_error("Original landing ZoneVelocity lacks its declaring native field");
        return field;
    }
    Value DecodeLandingZoneVelocity(const PortableTaggedProperty& tag,const Property& field) {
        if(field.key!="Engine.ZoneInfo.ZoneVelocity" || tag.type!=10u || tag.arrayIndex || tag.value.size()!=12u)
            throw std::runtime_error("Original landing raw ZoneVelocity has a different tag layout");
        QuestVr::ValidateAuthoredStructTag(*StructPropertySchema(field.key),tag.structName.ToString());
        auto value=Value::Vector({});
        for(std::size_t c=0u;c<3u;++c) {
            const auto p=c*4u;const std::uint32_t bits=std::uint32_t(tag.value[p])|
                (std::uint32_t(tag.value[p+1u])<<8u)|(std::uint32_t(tag.value[p+2u])<<16u)|
                (std::uint32_t(tag.value[p+3u])<<24u);
            std::memcpy(&value.vector[c],&bits,sizeof(bits));
        }
        return value;
    }
    void LandingEventDeclaration(const Function& function,bool base) {
        auto* node=Object(function.path);
        if(!node->script || function.nativeIndex || (function.flags&0x400u) ||
            function.source!=node->sourcePath || function.flags!=node->script->functionFlags || node->script->nativeIndex ||
            LowerAscii(Qualified(Table(node->sourcePath),Table(node->sourcePath).exports.at(node->exportIndex).ObjClass))!="core.function" ||
            (base && (function.path!="Engine.Actor.Landed" || node->outer!=Object("Engine.Actor") ||
                function.flags!=0x800u || function.variables.size()!=1u)))
            throw std::runtime_error("Original landing event lost its actual script declaration");
        const Property* parameter{};
        for(const auto& field:function.variables)if(field.flags&0x80u) {
            LifecycleStep();
            if(parameter)throw std::runtime_error("Original Landed has extra input/result fields");
            parameter=&field;
        }
        if(!parameter || parameter->zero.kind!=Kind::Vector || parameter->arrayDimension!=1u ||
            parameter->flags!=0x80u || parameter->resultLayout!=QuestVr::Vm::ResultLayout::CoreVector12)
            throw std::runtime_error("Original Landed lacks its required Vector packet");
        auto* field=Object(parameter->key);
        if(field->outer!=node || !field->property || field->property->type!="StructProperty" || field->property->baseField ||
            LowerAscii(Qualified(Table(field->sourcePath),field->property->referencedType))!="core.object.vector" ||
            LowerAscii(Qualified(Table(field->sourcePath),Table(field->sourcePath).exports.at(field->exportIndex).ObjClass))!="core.structproperty")
            throw std::runtime_error("Original Landed Vector lacks its actual declaring metadata");
        NativeCoordsVectorSchema(field); // Locals and non-event overrides are legitimate.
    }
    QuestVr::OriginalDecorationLandingStats RetailProcessDecorationLanded(RuntimeObject* actor,
        PortableModelVec3 normal,RuntimeObject* floor,QuestVr::Vm::Execution& execution) {
        if(!transaction_)throw std::runtime_error("Original landing requires its caller VM root");
        actor=RetailCollisionActor(reinterpret_cast<std::uintptr_t>(actor));
        auto* level=ZoneLevel(reinterpret_cast<std::uintptr_t>(actor->nativeXLevel));
        if(floor)floor=ZoneActor(reinterpret_cast<std::uintptr_t>(floor),level);
        QuestVr::NativeScratchScope scratch(nativeQueryScratchBytes_,16u<<10u,
            [this](auto bytes){ValidateStateBudget(bytes);});
        const auto source=reinterpret_cast<std::uintptr_t>(actor);
        const auto originalFloor=reinterpret_cast<std::uintptr_t>(floor);
        const auto requireSource=[&](std::uintptr_t identity) {
            if(identity!=source)throw std::runtime_error("Original landing source identity changed");
            return ZoneActor(identity,level);
        };
        QuestVr::OriginalDecorationLandingQueries q;
        q.work=[this]{LifecycleStep();};
        q.pawnFlag=[&](auto a){return Read(requireSource(a),LandingScalarProperty(actor,
            "Engine.Actor.bIsPawn","Engine.Actor",Kind::Bool,0x2u),0u).boolean;};
        q.zone=[&](auto a) {
            auto* current=requireSource(a);
            const auto value=ZoneStoredRegion(current,ZoneRegionProperty(current,QuestVr::OriginalActorRegionSlot::Main));
            const auto& path=value.fields.at("zone").text;
            return path.empty()?std::uintptr_t{}:reinterpret_cast<std::uintptr_t>(ZoneActor(
                reinterpret_cast<std::uintptr_t>(Object(path)),level));
        };
        q.zoneBounceVelocity=[&](auto z) {
            auto* zone=ZoneActor(z,level);
            return Read(zone,LandingScalarProperty(zone,"Engine.ZoneInfo.bBounceVelocity",
                "Engine.ZoneInfo",Kind::Bool,0x1u),0u).boolean;
        };
        q.zoneVelocity=[&](auto z,unsigned channel) {
            if(channel>=3u)throw std::runtime_error("Original landing zone channel is out of bounds");
            auto* zone=ZoneActor(z,level);const auto field=LandingZoneVelocityProperty(zone);
            const auto value=Read(zone,field,0u,false,false,nullptr,false,false,true);
            if(value.kind!=Kind::Vector || !value.text.empty() || !value.fields.empty() || !value.intArray.empty())
                throw std::runtime_error("Original landing raw zone Vector is not canonical");
            return value.vector[channel];
        };
        q.isPawnClass=[&](auto a){requireSource(a);return RetailTouchClass(a,"Engine.Pawn");};
        q.isDecorationClass=[&](auto a){requireSource(a);return RetailTouchClass(a,"Engine.Decoration");};
        q.isCarcassClass=[&](auto a){requireSource(a);return RetailTouchClass(a,"Engine.Carcass");};
        q.level=[&](auto a){requireSource(a);return reinterpret_cast<std::uintptr_t>(level);};
        q.location=[&](auto a,unsigned c){requireSource(a);return MoveLocation(a,c);};
        q.radius=[&](auto a){return RetailFindBaseDimension(requireSource(a),"Engine.Actor.CollisionRadius");};
        q.height=[&](auto a){return RetailFindBaseDimension(requireSource(a),"Engine.Actor.CollisionHeight");};
        const auto counter=[&] {return LandingScalarProperty(actor,"Engine.Decoration.numLandings",
            "Engine.Decoration",Kind::Int,0x2u);};
        q.numLandings=[&](auto a){return Read(requireSource(a),counter(),0u).integer;};
        q.writeNumLandings=[&](auto a,std::int32_t next) {
            requireSource(a);const auto field=counter();ReserveActorPhysicsReferences(field,1u);
            auto reference=std::make_shared<Reference>();reference->zero=Value::Integer(0);
            reference->read=[this,actor,field]{return Read(actor,field,0u);};
            reference->write=[this,actor,field](const Value& value) {
                if(value.kind!=Kind::Int)throw std::runtime_error("Original landing counter journal is not INT");
                Touch(actor);actor->scriptValues[InstanceStorageKey(actor->cls,field)][0u]=value;
                actor->committedScriptState=true; // Native store does NOT reconstruct animation.
            };
            execution.WriteReference(reference,Value::Integer(next));
        };
        q.velocity=[&](auto a,unsigned c) {
            requireSource(a);if(c>=3u)throw std::runtime_error("Original landing Velocity channel is out of bounds");
            return ReadActorPhysicsVector(actor,ActorPhysicsVectorProperty(actor,"Engine.Actor.Velocity")).vector[c];
        };
        std::optional<Property> velocityWrites;
        q.writeVelocityWord=[&](auto a,unsigned c,std::uint32_t bits) {
            requireSource(a);
            if(!velocityWrites) {
                velocityWrites=ActorPhysicsVectorProperty(actor,"Engine.Actor.Velocity");
                ReserveActorPhysicsReferences(*velocityWrites,3u); // BEFORE intermediate raw words exist.
            }
            WriteActorPhysicsVectorWord(actor,*velocityWrites,c,bits,execution);
        };
        q.singleLineCheck=[&](auto l,QuestVr::OriginalActorLineHit& hit,auto a,const auto& end,
            const auto& start,std::uint32_t flags,const auto& extent,std::int32_t extra) {
            requireSource(a);
            if(l!=reinterpret_cast<std::uintptr_t>(level) || (flags!=0x37u && flags!=0x17u) || extra)
                throw std::runtime_error("Original landing changed its native Level/query packet");
            return RetailSingleLineLevel(level,actor,hit,end,start,extent,static_cast<std::uint8_t>(flags),0u).clear?1:0;
        };
        q.landed=[&](auto a,const auto& supplied) {
            requireSource(a);LandingEventDeclaration(*FunctionFor(Object("Engine.Actor.Landed")),true);
            const auto selected=Member(actor,"Landed");
            if(!selected)throw std::runtime_error("Original landing FindFunctionChecked found no selected Landed");
            LandingEventDeclaration(*FunctionFor(Object(selected->path)),false);
            static_cast<void>(execution.CallEvent(actor->reflection.objectPath,"Landed",true,
                {{Value::Vector({supplied.x,supplied.y,supplied.z}),{}}}));
        };
        q.physics=[&](auto a){return static_cast<std::uint8_t>(Read(requireSource(a),ActorPhysicsProperty(actor),0u).integer);};
        q.setPhysics=[&](auto a,std::uint8_t mode,auto supplied) {
            requireSource(a);
            if(mode || supplied!=originalFloor)throw std::runtime_error("Original landing changed its mode0/supplied floor");
            static_cast<void>(ActorSetPhysicsNative(actor,{{Value::Byte(0u),{}},
                {Value::Text(Kind::Object,floor?floor->reflection.objectPath:std::string{}),{}}},nullptr,execution));
        };
        const auto result=QuestVr::ProcessOriginalDecorationLanded(source,normal,originalFloor,q);
        ValidateStateBudget();return result;
    }
