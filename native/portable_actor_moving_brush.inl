// Original Physics9 services, borrowing the caller's real Execution/root.
    Property MovingBrushScalarField(RuntimeObject* actor,const char* key,const char* owner,Kind kind,
        std::uint32_t flags,const char* constraint="") {
        const auto field=NoiseProperty(actor,key,kind,flags,constraint);auto* node=Object(field.key);
        const auto type=kind==Kind::Bool?"boolproperty":kind==Kind::Float?"floatproperty":"byteproperty";
        if(node->outer!=Object(owner)||!node->property||node->property->baseField||
            (!*constraint&&node->property->referencedType)||
            LowerAscii(Qualified(Table(node->sourcePath),Table(node->sourcePath).exports.at(node->exportIndex).ObjClass))!=
                std::string("core.")+type)
            throw std::runtime_error("Original MovingBrush scalar lacks its exact declaring native field");
        return field;
    }
    Property MovingBrushLocationField(RuntimeObject* actor) {
        RequireMapActor(actor);const auto field=NativeCoordsProperty(actor,"Engine.Actor.Location",Kind::Vector,0x23u);
        auto* node=Object(field.key);
        if(node->outer!=Object("Engine.Actor")||!node->property||node->property->baseField)
            throw std::runtime_error("Original MovingBrush Location lacks its declaring Actor field");
        return field;
    }
    Property MovingBrushStructField(RuntimeObject* actor,const char* key,Kind kind,unsigned dimension) {
        RequireMapActor(actor);const auto field=DescribeProperty(key);auto* node=Object(field.key);
        const auto canonical=NativeCoordsProperty(actor,kind==Kind::Vector?"Engine.Actor.Location":"Engine.Actor.Rotation",kind,0x23u);
        auto* canonicalNode=Object(canonical.key);
        if(!OwnsProperty(actor->cls,field)||field.key!=key||field.zero.kind!=kind||field.arrayDimension!=dimension||
            field.flags||node->outer!=Object("Engine.Mover")||!node->property||node->property->baseField||
            node->property->type!="StructProperty"||
            LowerAscii(Qualified(Table(node->sourcePath),node->property->referencedType))!=
                LowerAscii(Qualified(Table(canonicalNode->sourcePath),canonicalNode->property->referencedType))||
            LowerAscii(Qualified(Table(node->sourcePath),Table(node->sourcePath).exports.at(node->exportIndex).ObjClass))!="core.structproperty")
            throw std::runtime_error("Original MovingBrush pose lacks its exact declaring native struct/array");
        return field;
    }
    Property MovingBrushVectorField(RuntimeObject* actor,const char* key) {
        const auto name=std::string_view(key);
        if(name=="Engine.Actor.Location")return MovingBrushLocationField(actor);
        if(name!="Engine.Mover.KeyPos"&&name!="Engine.Mover.BasePos"&&name!="Engine.Mover.OldPos")
            throw std::runtime_error("Original MovingBrush raw Vector capability is outside its native fields");
        return MovingBrushStructField(actor,key,Kind::Vector,name=="Engine.Mover.KeyPos"?8u:1u);
    }
    Value DecodeMovingBrushVector(const PortableTaggedProperty& tag,const Property& field,unsigned index) {
        if(tag.type!=10u||tag.arrayIndex!=index||index>=field.arrayDimension||tag.value.size()!=12u)
            throw std::runtime_error("Original MovingBrush raw Vector tag has a different native array layout");
        QuestVr::ValidateAuthoredStructTag(*StructPropertySchema(field.key),tag.structName.ToString());
        auto value=Value::Vector({});
        for(unsigned c=0u;c<3u;++c) {
            const auto at=c*4u;const auto bits=std::uint32_t(tag.value[at])|(std::uint32_t(tag.value[at+1u])<<8u)|
                (std::uint32_t(tag.value[at+2u])<<16u)|(std::uint32_t(tag.value[at+3u])<<24u);
            std::memcpy(&value.vector[c],&bits,sizeof(bits));
        }
        return value;
    }
    float MovingBrushVectorChannel(RuntimeObject* actor,const char* key,unsigned index,unsigned channel) {
        const auto field=MovingBrushVectorField(actor,key);
        if(channel>=3u||index>=field.arrayDimension)throw std::runtime_error("Original MovingBrush Vector channel/index is out of bounds");
        const auto value=Read(actor,field,index,false,false,nullptr,false,false,false,false,true);
        if(value.kind!=Kind::Vector)throw std::runtime_error("Original MovingBrush raw Vector changed kind");
        return value.vector[channel];
    }
    std::int32_t MovingBrushRotatorChannel(RuntimeObject* actor,const char* key,unsigned index,unsigned channel) {
        const auto field=MovingBrushStructField(actor,key,Kind::Rotator,std::string_view(key)=="Engine.Mover.KeyRot"?8u:1u);
        if(channel>=3u||index>=field.arrayDimension)throw std::runtime_error("Original MovingBrush Rotator channel/index is out of bounds");
        const auto value=Read(actor,field,index);
        if(value.kind!=Kind::Rotator)throw std::runtime_error("Original MovingBrush Rotator changed kind");
        return value.rotation[channel];
    }
    QuestVr::OriginalMoverInterpolationStats RetailInterpolateMover(RuntimeObject* actor,float deltaSeconds,
        QuestVr::Vm::Execution& execution) {
        if(!transaction_)throw std::runtime_error("Original MovingBrush interpolation requires its caller root");
        const auto source=reinterpret_cast<std::uintptr_t>(actor);LifecycleActor(source);
        const auto require=[&](auto identity) {
            if(identity!=source)throw std::runtime_error("Original MovingBrush interpolation source identity changed");
            return LifecycleActor(identity);
        };
        const auto scalar=[&](auto a,const char* key,const char* owner,Kind kind,std::uint32_t flags,const char* constraint="") {
            auto* receiver=require(a);return Read(receiver,MovingBrushScalarField(receiver,key,owner,kind,flags,constraint),0u);
        };
        QuestVr::OriginalMoverInterpolationQueries q;q.work=[this]{LifecycleStep();};
        q.isMoverClass=[&](auto a) {
            auto* receiver=require(a);auto* target=Object("Engine.Mover");
            if(!target->classDescriptor||target->defaultOwner)
                throw std::runtime_error("Original MovingBrush cast lacks its actual Mover UClass");
            std::array<RuntimeObject*,128u> visited{};std::size_t depth{};bool result{};
            // This native Class/Super cast has no Level/Engine/Cylinder read.
            // Do not borrow RetailTouchClass's stronger collision capability.
            for(auto* cls=receiver->cls;cls;cls=cls->base) {
                LifecycleStep();if(cls==target){result=true;break;}
                if(depth==visited.size()||std::find(visited.begin(),visited.begin()+depth,cls)!=visited.begin()+depth||
                    !cls->classDescriptor||cls->defaultOwner)
                    throw std::runtime_error("Original MovingBrush cast ancestry is unsafe or exceeds its bound");
                visited[depth++]=cls;
            }
            if(activeTickStats_)activeTickStats_->lastMovingBrushClass=result;
            return result;
        };
        q.keyNum=[&](auto a){const auto key=static_cast<std::uint8_t>(scalar(a,"Engine.Mover.KeyNum","Engine.Mover",Kind::Byte,1u).integer);
            if(activeTickStats_)activeTickStats_->lastMovingBrushKey=std::min<unsigned>(key,8u);return key;};
        q.interpolating=[&](auto a){const auto result=scalar(a,"Engine.Actor.bInterpolating","Engine.Actor",Kind::Bool,0u).boolean;
            if(activeTickStats_)activeTickStats_->lastMovingBrushInterpolating=result;return result;};
        q.physAlpha=[&](auto a){return scalar(a,"Engine.Actor.PhysAlpha","Engine.Actor",Kind::Float,0u).floating;};
        q.physRate=[&](auto a){return scalar(a,"Engine.Actor.PhysRate","Engine.Actor",Kind::Float,0u).floating;};
        q.glideType=[&](auto a){return static_cast<std::uint8_t>(scalar(a,"Engine.Mover.MoverGlideType","Engine.Mover",Kind::Byte,1u,
            "Engine.Mover.EMoverGlideType").integer);};
        q.baseRot=[&](auto a,unsigned c){return MovingBrushRotatorChannel(require(a),"Engine.Mover.BaseRot",0u,c);};
        q.oldRot=[&](auto a,unsigned c){return MovingBrushRotatorChannel(require(a),"Engine.Mover.OldRot",0u,c);};
        q.keyRot=[&](auto a,unsigned index,unsigned c) {
            auto* receiver=require(a);
            if(index==8u)return std::bit_cast<std::int32_t>(MovingBrushVectorChannel(receiver,"Engine.Mover.BasePos",0u,c));
            return MovingBrushRotatorChannel(receiver,"Engine.Mover.KeyRot",index,c);
        };
        q.keyPos=[&](auto a,unsigned index,unsigned c) {
            auto* receiver=require(a);
            if(index==8u)return std::bit_cast<float>(MovingBrushRotatorChannel(receiver,"Engine.Mover.KeyRot",0u,c));
            return MovingBrushVectorChannel(receiver,"Engine.Mover.KeyPos",index,c);
        };
        q.basePos=[&](auto a,unsigned c){return MovingBrushVectorChannel(require(a),"Engine.Mover.BasePos",0u,c);};
        q.oldPos=[&](auto a,unsigned c){return MovingBrushVectorChannel(require(a),"Engine.Mover.OldPos",0u,c);};
        q.location=[&](auto a,unsigned c){return MovingBrushVectorChannel(require(a),"Engine.Actor.Location",0u,c);};
        q.level=[&](auto a){return reinterpret_cast<std::uintptr_t>(require(a)->nativeXLevel);};
        q.moveActor=[&](auto level,auto a,auto translation,auto rotation,auto& hit,auto options) {
            require(a);if(activeTickStats_)++activeTickStats_->movingBrushMoves;
            return MoveLevelActor(level,a,translation,rotation,hit,options,execution)?1:0;
        };
        q.writePhysAlpha=[&](auto a,float value) {
            auto* receiver=require(a);WriteFallingScalar(receiver,
                MovingBrushScalarField(receiver,"Engine.Actor.PhysAlpha","Engine.Actor",Kind::Float,0u),Value::Float(value),execution);
        };
        q.writeInterpolating=[&](auto a,bool value) {
            auto* receiver=require(a);WriteFallingScalar(receiver,
                MovingBrushScalarField(receiver,"Engine.Actor.bInterpolating","Engine.Actor",Kind::Bool,0u),Value::Bool(value),execution);
        };
        q.interpolateEnd=[&](auto a){if(activeTickStats_)++activeTickStats_->movingBrushEnds;
            LifecycleEvent(require(a),LifecycleEventKind::InterpolateEnd,nullptr,execution);};
        return QuestVr::InterpolateOriginalMover(source,deltaSeconds,q);
    }
    void RetailPerformMovingBrush(RuntimeObject* actor,float deltaSeconds,QuestVr::Vm::Execution& execution) {
        if(!transaction_)throw std::runtime_error("Original MovingBrush dispatch requires its caller root");
        const auto source=reinterpret_cast<std::uintptr_t>(actor);LifecycleActor(source);
        if(activeTickStats_) {
            const auto& name=actor->reflection.objectPath;
            if(name.size()>1024u)throw std::runtime_error("Original MovingBrush diagnostic actor identity exceeds its bound");
            const auto bytes=2u*(name.size()+1u);ValidateStateBudget(bytes);
            nativeActorOrderObservationBytes_=QuestVr::DecalStateDetail::Add(nativeActorOrderObservationBytes_,bytes);
            activeTickStats_->lastMovingBrushActor=name;++activeTickStats_->movingBrushCalls;
            activeTickStats_->lastMovingBrushClass.reset();activeTickStats_->lastMovingBrushInterpolating.reset();
            activeTickStats_->lastMovingBrushKey.reset();
        }
        const auto require=[&](auto identity) {
            if(identity!=source)throw std::runtime_error("Original MovingBrush dispatch source identity changed");
            return LifecycleActor(identity);
        };
        QuestVr::OriginalActorMovingBrushQueries q;q.work=[this]{LifecycleStep();};
        q.location=[&](auto a,unsigned c){return MovingBrushVectorChannel(require(a),"Engine.Actor.Location",0u,c);};
        q.oldLocation=[&](auto a,unsigned c){auto* receiver=require(a);
            return ReadFallingOldLocation(receiver,FallingOldLocationProperty(receiver)).vector.at(c);};
        q.writeOldLocation=[&](auto a,unsigned c,float value) {
            auto* receiver=require(a);const auto field=FallingOldLocationProperty(receiver);
            if(c==0u)ReserveActorPhysicsReferences(field,3u);
            WriteFallingVectorWord(receiver,field,c,std::bit_cast<std::uint32_t>(value),execution);
        };
        q.inner=[&](auto a,float dt){static_cast<void>(RetailInterpolateMover(require(a),dt,execution));};
        q.writeVelocity=[&](auto a,unsigned c,float value) {
            auto* receiver=require(a);const auto field=ActorPhysicsVectorProperty(receiver,"Engine.Actor.Velocity");
            if(c==0u)ReserveActorPhysicsReferences(field,3u);
            WriteActorPhysicsVectorWord(receiver,field,c,std::bit_cast<std::uint32_t>(value),execution);
        };
        static_cast<void>(QuestVr::PerformOriginalActorMovingBrush(source,deltaSeconds,q));
    }
