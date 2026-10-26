// Original AScriptedPawn::Tick and its two native decay helpers, on the
// enclosing Level frame's SAME Execution. This does not admit APawn physics
// or the still-unimplemented Pawn continuation of qualified AActor::Tick.
    Property ScriptedPawnFloatField(RuntimeObject* actor,QuestVr::OriginalScriptedPawnTickFloat field) {
        using F=QuestVr::OriginalScriptedPawnTickFloat;
        const char* name{};const char* owner="DeusEx.ScriptedPawn";std::uint32_t flags{};
        switch(field) {
        case F::PrePivotTime:name="PrePivotTime";break;
        case F::LastRenderTime:name="LastRenderTime";owner="Engine.Actor";break;
        case F::DistanceFromPlayer:name="DistanceFromPlayer";owner="Engine.Actor";break;
        case F::AlarmTimer:name="AlarmTimer";break;
        case F::WeaponTimer:name="WeaponTimer";break;
        case F::FireTimer:name="FireTimer";break;
        case F::SpecialTimer:name="SpecialTimer";break;
        case F::ReloadTimer:name="ReloadTimer";break;
        case F::AvoidWallTimer:name="AvoidWallTimer";break;
        case F::AvoidBumpTimer:name="AvoidBumpTimer";break;
        case F::ObstacleTimer:name="ObstacleTimer";break;
        case F::CloakEMPTimer:name="CloakEMPTimer";break;
        case F::TakeHitTimer:name="TakeHitTimer";break;
        case F::CarcassCheckTimer:name="CarcassCheckTimer";break;
        case F::PotentialEnemyTimer:name="PotentialEnemyTimer";break;
        case F::BeamCheckTimer:name="BeamCheckTimer";break;
        case F::FutzTimer:name="FutzTimer";break;
        case F::PlayerAgitationTimer:name="PlayerAgitationTimer";break;
        case F::DistressTimer:name="DistressTimer";break;
        case F::FearSustainTime:name="FearSustainTime";break;
        case F::MoveTimer:name="MoveTimer";owner="Engine.Pawn";break;
        case F::BurnTimer:name="burnTimer";owner="Engine.Pawn";flags=0x20u;break;
        case F::BurnPeriod:name="BurnPeriod";break;
        case F::BleedRate:name="BleedRate";break;
        case F::DropCounter:name="DropCounter";break;
        case F::ClotPeriod:name="ClotPeriod";flags=1u;break;
        default:throw std::runtime_error("Original ScriptedPawn Tick has an unknown Float selector");
        }
        return OriginalAnimationField(actor,owner,name,Kind::Float,flags,"FloatProperty");
    }
    Property ScriptedPawnFlagField(RuntimeObject* actor,QuestVr::OriginalScriptedPawnTickFlag field) {
        using F=QuestVr::OriginalScriptedPawnTickFlag;
        const char* name{};const char* owner="DeusEx.ScriptedPawn";std::uint32_t flags{};
        switch(field) {
        case F::SimulatedPawn:name="bSimulatedPawn";owner="Engine.Actor";flags=2u;break;
        case F::Disappear:name="bDisappear";break;
        case F::TickVisibleOnly:name="bTickVisibleOnly";flags=1u;break;
        case F::HasCloak:name="bHasCloak";break;
        case F::AdvancedTactics:name="bAdvancedTactics";owner="Engine.Pawn";break;
        case F::ClearedObstacle:name="bClearedObstacle";break;
        case F::OnFire:name="bOnFire";owner="Engine.Pawn";flags=0x20u;break;
        case F::CanBleed:name="bCanBleed";break;
        default:throw std::runtime_error("Original ScriptedPawn Tick has an unknown Bool selector");
        }
        return OriginalAnimationField(actor,owner,name,Kind::Bool,flags,"BoolProperty");
    }
    Property ScriptedPawnByteField(RuntimeObject* actor,QuestVr::OriginalScriptedPawnTickByte field) {
        using F=QuestVr::OriginalScriptedPawnTickByte;
        const char* name{};const char* owner="Engine.Actor";const char* constraint="Engine.Actor.ENetRole";
        std::uint32_t flags{};
        switch(field) {
        case F::Role:name="Role";flags=0x20u;break;
        case F::RemoteRole:name="RemoteRole";flags=0x21u;break;
        case F::Physics:return ActorPhysicsProperty(actor);
        case F::TurnDirection:name="TurnDirection";owner="DeusEx.ScriptedPawn";constraint="DeusEx.ScriptedPawn.ETurning";break;
        case F::NextDirection:name="NextDirection";owner="DeusEx.ScriptedPawn";constraint="DeusEx.ScriptedPawn.ETurning";break;
        default:throw std::runtime_error("Original ScriptedPawn Tick has an unknown Byte selector");
        }
        const auto property=OriginalAnimationField(actor,owner,name,Kind::Byte,flags,"ByteProperty");
        auto* node=Object(property.key);
        if(!node->property->referencedType||
            LowerAscii(Qualified(Table(node->sourcePath),node->property->referencedType))!=LowerAscii(constraint))
            throw std::runtime_error("Original ScriptedPawn Tick Byte lost its exact Enum constraint");
        return property;
    }
    Property ScriptedPawnVectorField(RuntimeObject* actor,const char* key) {
        const auto name=std::string_view(key);std::uint32_t flags{};const char* owner{};
        if(name=="Engine.Actor.PrePivot"||name=="Engine.Actor.Acceleration") {flags=0x20u;owner="Engine.Actor";}
        else if(name=="Engine.Actor.Velocity") {flags=0x21u;owner="Engine.Actor";}
        else if(name=="DeusEx.ScriptedPawn.DesiredPrePivot")owner="DeusEx.ScriptedPawn";
        else throw std::runtime_error("Original ScriptedPawn raw Vector authority is outside its audited fields");
        RequireMapActor(actor);
        const auto property=NativeCoordsProperty(actor,key,Kind::Vector,flags);auto* node=Object(property.key);
        if(node->outer!=Object(owner)||!node->property||node->property->baseField||
            node->property->type!="StructProperty"||node->property->arrayDimension!=1||node->property->flags!=flags||
            LowerAscii(Qualified(Table(node->sourcePath),Table(node->sourcePath).exports.at(node->exportIndex).ObjClass))!="core.structproperty")
            throw std::runtime_error("Original ScriptedPawn Vector lost its actual declaring schema");
        return property;
    }
    Value ReadScriptedPawnVector(RuntimeObject* actor,const Property& field) {
        auto value=Read(actor,field,0u,false,false,nullptr,false,false,false,false,false,true);
        if(value.kind!=Kind::Vector||!value.text.empty()||!value.fields.empty()||!value.intArray.empty())
            throw std::runtime_error("Original ScriptedPawn raw Vector is not canonical storage");
        return value;
    }
    void WriteScriptedPawnVector(RuntimeObject* actor,const Property& field,unsigned channel,float value,
        QuestVr::Vm::Execution& execution) {
        if(channel>=3u)throw std::runtime_error("Original ScriptedPawn Vector channel is out of bounds");
        ReserveActorPhysicsReferences(field,1u);
        auto reference=std::make_shared<Reference>();reference->zero=Value::Integer(0);
        reference->read=[this,actor,field,channel] {
            return Value::Integer(std::bit_cast<std::int32_t>(ReadScriptedPawnVector(actor,field).vector[channel]));
        };
        reference->write=[this,actor,field,channel](const Value& incoming) {
            if(incoming.kind!=Kind::Int)throw std::runtime_error("Original ScriptedPawn Vector journal lost its raw DWORD");
            auto merged=ReadScriptedPawnVector(actor,field);const auto bits=std::bit_cast<std::uint32_t>(incoming.integer);
            std::memcpy(&merged.vector[channel],&bits,sizeof(bits));Touch(actor);
            actor->scriptValues[InstanceStorageKey(actor->cls,field)][0u]=std::move(merged);
            actor->committedScriptState=true;
        };
        execution.WriteReference(reference,Value::Integer(std::bit_cast<std::int32_t>(value)));
    }
    Property ScriptedPawnAllianceField(RuntimeObject* actor) {
        const auto field=DescribeProperty("DeusEx.ScriptedPawn.AlliancesEx");auto* node=Object(field.key);
        if(field.key!="DeusEx.ScriptedPawn.AlliancesEx"||!OwnsProperty(actor->cls,field)||field.zero.kind!=Kind::Struct||
            field.arrayDimension!=16u||field.flags||node->outer!=Object("DeusEx.ScriptedPawn")||!node->property||
            node->property->type!="StructProperty"||node->property->baseField||node->property->arrayDimension!=16||
            node->property->flags||!node->property->referencedType||
            LowerAscii(Qualified(Table(node->sourcePath),Table(node->sourcePath).exports.at(node->exportIndex).ObjClass))!="core.structproperty")
            throw std::runtime_error("Original ScriptedPawn decay lacks its actual AlliancesEx[16] declaration");
        auto* structure=Object(Qualified(Table(node->sourcePath),node->property->referencedType));
        if(structure->reflection.objectPath!="DeusEx.ScriptedPawn.AllianceInfoEx"||structure->outer!=Object("DeusEx.ScriptedPawn"))
            throw std::runtime_error("Original ScriptedPawn AlliancesEx lost its actual nested AllianceInfoEx");
        const auto& descriptor=StructDescriptor(structure);const auto schema=StructSchema(structure);
        if(descriptor.baseField||descriptor.logicalSize||!descriptor.rawBytes.empty()||!descriptor.bytecode.empty()||
            schema->kind!=Kind::Struct||schema->fields.size()!=4u)
            throw std::runtime_error("Original ScriptedPawn AllianceInfoEx is not its four-member scalar struct");
        constexpr std::array<const char*,4u> names{"AllianceName","AllianceLevel","AgitationLevel","bPermanent"};
        constexpr std::array<const char*,4u> types{"NameProperty","FloatProperty","FloatProperty","BoolProperty"};
        constexpr std::array kinds{Kind::Name,Kind::Float,Kind::Float,Kind::Bool};auto child=descriptor.children;
        for(std::size_t index=0u;index<names.size();++index) {
            LifecycleStep();if(!child)throw std::runtime_error("Original ScriptedPawn AllianceInfoEx child chain is truncated");
            auto* member=Object(Qualified(Table(structure->sourcePath),child));const auto& declared=schema->fields[index];
            const auto key=structure->reflection.objectPath+'.'+names[index];
            if(member->reflection.objectPath!=key||member->outer!=structure||!member->property||
                member->property->type!=types[index]||member->property->arrayDimension!=1||member->property->flags||
                member->property->baseField||member->property->referencedType||declared.key!=key||
                declared.arrayDimension!=1u||declared.zero.kind!=kinds[index]||declared.nested||
                LowerAscii(Qualified(Table(member->sourcePath),Table(member->sourcePath).exports.at(member->exportIndex).ObjClass))!=
                    std::string("core.")+LowerAscii(types[index]))
                throw std::runtime_error("Original ScriptedPawn AllianceInfoEx member lost its exact declaring schema");
            child=member->property->nextField;
        }
        if(child)throw std::runtime_error("Original ScriptedPawn AllianceInfoEx has extra native members");
        return field;
    }
    Value ReadScriptedPawnAlliance(RuntimeObject* actor,std::size_t index,const char* name,Kind kind) {
        if(index>=16u)throw std::runtime_error("Original ScriptedPawn alliance index is out of bounds");
        const auto value=Read(actor,ScriptedPawnAllianceField(actor),static_cast<std::uint32_t>(index));
        constexpr std::array<const char*,4u> names{"alliancename","alliancelevel","agitationlevel","bpermanent"};
        constexpr std::array kinds{Kind::Name,Kind::Float,Kind::Float,Kind::Bool};
        if(value.kind!=Kind::Struct||value.fields.size()!=4u)
            throw std::runtime_error("Original ScriptedPawn alliance has a malformed reached scalar member");
        for(std::size_t member=0u;member<names.size();++member) {
            const auto found=value.fields.find(names[member]);
            if(found==value.fields.end()||found->second.kind!=kinds[member])
                throw std::runtime_error("Original ScriptedPawn alliance has a malformed canonical scalar member");
        }
        const auto found=value.fields.find(name);
        if(found==value.fields.end()||found->second.kind!=kind)
            throw std::runtime_error("Original ScriptedPawn alliance selected an unknown member");
        return found->second;
    }
    void WriteScriptedPawnAlliance(RuntimeObject* actor,std::size_t index,float value,QuestVr::Vm::Execution& execution) {
        if(index>=16u)throw std::runtime_error("Original ScriptedPawn alliance store index is out of bounds");
        const auto field=ScriptedPawnAllianceField(actor);
        // The journal retains two copies of the four-member schema and its
        // canonical zero. Budget these, not just a Float-sized facade.
        constexpr std::size_t reserve=8192u;ValidateStateBudget(reserve);
        nativePhysicsReferenceBytes_=QuestVr::DecalStateDetail::Add(nativePhysicsReferenceBytes_,reserve);
        auto reference=std::make_shared<Reference>();reference->zero=Value::Float(0.0f);
        reference->read=[this,actor,index]{return ReadScriptedPawnAlliance(actor,index,"agitationlevel",Kind::Float);};
        reference->write=[this,actor,field,index](const Value& incoming) {
            if(incoming.kind!=Kind::Float)throw std::runtime_error("Original ScriptedPawn agitation store changed its Float type");
            auto merged=Read(actor,field,static_cast<std::uint32_t>(index));
            if(merged.kind!=Kind::Struct||merged.fields.size()!=4u||!merged.fields.contains("agitationlevel")||
                merged.fields.at("agitationlevel").kind!=Kind::Float)
                throw std::runtime_error("Original ScriptedPawn agitation store lost its current array element");
            merged.fields.at("agitationlevel")=incoming;auto normalized=TypedLiveValue(field,merged);Touch(actor);
            actor->scriptValues[InstanceStorageKey(actor->cls,field)][static_cast<std::uint32_t>(index)]=std::move(normalized);
            actor->committedScriptState=true;ValidateStateBudget();
        };
        execution.WriteReference(reference,Value::Float(value));
    }
    void UpdateOriginalScriptedPawnDecay(RuntimeObject* actor,float delta,bool fear,QuestVr::Vm::Execution& execution) {
        const auto source=reinterpret_cast<std::uintptr_t>(actor);
        const auto require=[&](auto identity) {
            if(identity!=source)throw std::runtime_error("Original ScriptedPawn decay changed its retained Actor");
            return LifecycleActor(identity);
        };
        using F=QuestVr::OriginalScriptedPawnDecayField;
        const auto scalar=[&](RuntimeObject* receiver,F field) {
            const char* name{};
            switch(field) {
            case F::AgitationCheckTimer:name="AgitationCheckTimer";break;
            case F::AgitationTimer:name="AgitationTimer";break;
            case F::AgitationDecayRate:name="AgitationDecayRate";break;
            case F::FearTimer:name="FearTimer";break;
            case F::FearDecayRate:name="FearDecayRate";break;
            case F::FearLevel:name="FearLevel";break;
            default:throw std::runtime_error("Original ScriptedPawn decay has an unknown Float selector");
            }
            return OriginalAnimationField(receiver,"DeusEx.ScriptedPawn",name,Kind::Float,0u,"FloatProperty");
        };
        const auto changed=[&](RuntimeObject* receiver) {
            return OriginalAnimationField(receiver,"DeusEx.ScriptedPawn","bAlliancesChanged",Kind::Bool,0u,"BoolProperty");
        };
        QuestVr::OriginalScriptedPawnDecayQueries q;q.work=[this]{LifecycleStep();};
        q.readFloat=[&](auto a,F f){auto* receiver=require(a);return OriginalAnimationRead(receiver,scalar(receiver,f),Kind::Float).floating;};
        q.writeFloat=[&](auto a,F f,float value){auto* receiver=require(a);WriteFallingScalar(receiver,scalar(receiver,f),Value::Float(value),execution);};
        q.alliancesChanged=[&](auto a){auto* receiver=require(a);return OriginalAnimationRead(receiver,changed(receiver),Kind::Bool).boolean;};
        q.writeAlliancesChanged=[&](auto a,bool value){auto* receiver=require(a);WriteFallingScalar(receiver,changed(receiver),Value::Bool(value),execution);};
        q.allianceNamed=[&](auto a,std::size_t i) {
            const auto name=ReadScriptedPawnAlliance(require(a),i,"alliancename",Kind::Name).text;
            return !name.empty()&&LowerAscii(name)!="none";
        };
        q.alliancePermanent=[&](auto a,std::size_t i){return ReadScriptedPawnAlliance(require(a),i,"bpermanent",Kind::Bool).boolean;};
        q.allianceAgitation=[&](auto a,std::size_t i){return ReadScriptedPawnAlliance(require(a),i,"agitationlevel",Kind::Float).floating;};
        q.writeAllianceAgitation=[&](auto a,std::size_t i,float value){WriteScriptedPawnAlliance(require(a),i,value,execution);};
        if(fear)static_cast<void>(QuestVr::UpdateOriginalScriptedPawnFear(source,delta,q));
        else static_cast<void>(QuestVr::UpdateOriginalScriptedPawnAgitation(source,delta,q));
    }
    void OriginalScriptedPawnCallback(RuntimeObject* actor,const char* name,std::optional<bool> input,
        QuestVr::Vm::Execution& execution) {
        OriginalNativeActorCallback(actor,name,input?std::optional<Value>{Value::Bool(*input)}:std::nullopt,execution);
    }
    std::int32_t AdvanceOriginalScriptedPawnTick(RuntimeObject* actor,float delta,std::int32_t tickType,
        QuestVr::Vm::Execution& execution) {
        const auto source=reinterpret_cast<std::uintptr_t>(actor);
        const auto require=[&](auto identity) {
            if(identity!=source)throw std::runtime_error("Original ScriptedPawn Tick changed its retained Actor");
            auto* receiver=LifecycleActor(identity);
            if(receiver->nativeXLevel!=activeTickLevel_||!activeTickStats_)
                throw std::runtime_error("Original ScriptedPawn Tick lost its enclosing native Level phase");
            return receiver;
        };
        using F=QuestVr::OriginalScriptedPawnTickFloat;using B=QuestVr::OriginalScriptedPawnTickFlag;
        using Y=QuestVr::OriginalScriptedPawnTickByte;using V=QuestVr::OriginalScriptedPawnTickVector;
        using I=QuestVr::OriginalScriptedPawnTickInt;using O=QuestVr::OriginalScriptedPawnTickObject;
        const auto vector=[&](RuntimeObject* receiver,V f) {
            const char* key{};
            switch(f) {
            case V::PrePivot:key="Engine.Actor.PrePivot";break;
            case V::DesiredPrePivot:key="DeusEx.ScriptedPawn.DesiredPrePivot";break;
            case V::Acceleration:key="Engine.Actor.Acceleration";break;
            case V::Velocity:key="Engine.Actor.Velocity";break;
            default:throw std::runtime_error("Original ScriptedPawn Tick has an unknown Vector selector");
            }
            return ScriptedPawnVectorField(receiver,key);
        };
        const auto object=[&](RuntimeObject* receiver,O f) {
            const bool weapon=f==O::Weapon;
            if(!weapon&&f!=O::ActorAvoiding)throw std::runtime_error("Original ScriptedPawn Tick has an unknown Object selector");
            const auto field=OriginalAnimationField(receiver,weapon?"Engine.Pawn":"DeusEx.ScriptedPawn",weapon?"Weapon":"ActorAvoiding",
                Kind::Object,weapon?0x10020u:0u,"ObjectProperty");auto* node=Object(field.key);
            if(!node->property->referencedType||LowerAscii(Qualified(Table(node->sourcePath),node->property->referencedType))!=
                (weapon?"engine.weapon":"engine.actor"))throw std::runtime_error("Original ScriptedPawn Object lost its exact Class constraint");
            return field;
        };
        const auto nativeClass=[&](const char* key,const char* base) {
            auto* cls=Object(key);
            if(!cls->classDescriptor||cls->defaultOwner||!(cls->reflection.flags&0x04000000u)||cls->base!=Object(base))
                throw std::runtime_error("Original ScriptedPawn Tick lost its actual native Class cast target");
            return reinterpret_cast<std::uintptr_t>(cls);
        };
        QuestVr::OriginalScriptedPawnTickQueries q;q.work=[this]{LifecycleStep();};
        q.readFloat=[&](auto a,F f) {
            auto* r=require(a);const auto field=ScriptedPawnFloatField(r,f);
            if(f==F::LastRenderTime) {
                if(!r->lastRenderTime)throw std::runtime_error("Original ScriptedPawn Tick has no initialized native Actor.LastRenderTime");
                return *r->lastRenderTime;
            }
            return OriginalAnimationRead(r,field,Kind::Float).floating;
        };
        q.writeFloat=[&](auto a,F f,float value){auto* r=require(a);WriteFallingScalar(r,ScriptedPawnFloatField(r,f),Value::Float(value),execution);};
        q.readFlag=[&](auto a,B f){auto* r=require(a);return OriginalAnimationRead(r,ScriptedPawnFlagField(r,f),Kind::Bool).boolean;};
        q.writeFlag=[&](auto a,B f,bool value){auto* r=require(a);WriteFallingScalar(r,ScriptedPawnFlagField(r,f),Value::Bool(value),execution);};
        q.readByte=[&](auto a,Y f){auto* r=require(a);return static_cast<std::uint8_t>(OriginalAnimationRead(r,ScriptedPawnByteField(r,f),Kind::Byte).integer);};
        q.writeByte=[&](auto a,Y f,std::uint8_t value){auto* r=require(a);WriteFallingScalar(r,ScriptedPawnByteField(r,f),Value::Byte(value),execution);};
        q.readVectorComponent=[&](auto a,V f,unsigned c) {
            auto* r=require(a);if(c>=3u)throw std::runtime_error("Original ScriptedPawn Vector read channel is out of bounds");
            return ReadScriptedPawnVector(r,vector(r,f)).vector[c];
        };
        q.writeVectorComponent=[&](auto a,V f,unsigned c,float value){auto* r=require(a);WriteScriptedPawnVector(r,vector(r,f),c,value,execution);};
        q.readInt=[&](auto a,I f) {
            auto* r=require(a);const bool health=f==I::Health;
            if(!health&&f!=I::CloakThreshold)throw std::runtime_error("Original ScriptedPawn Tick has an unknown Int selector");
            return OriginalAnimationRead(r,OriginalAnimationField(r,health?"Engine.Pawn":"DeusEx.ScriptedPawn",health?"Health":"CloakThreshold",
                Kind::Int,health?0x10021u:0u,"IntProperty"),Kind::Int).integer;
        };
        q.readObject=[&](auto a,O f) {
            auto* r=require(a);const auto field=object(r,f);const auto value=OriginalAnimationRead(r,field,Kind::Object);
            ValidateObjectValue(value,f==O::Weapon?"Engine.Weapon":"Engine.Actor",false);
            return value.text.empty()?std::uintptr_t{}:reinterpret_cast<std::uintptr_t>(Object(value.text));
        };
        q.clearObject=[&](auto a,O f){auto* r=require(a);WriteFallingScalar(r,object(r,f),Value::Text(Kind::Object,{}),execution);};
        q.clearPotentialEnemyAlliance=[&](auto a) {
            auto* r=require(a);WriteFallingScalar(r,OriginalAnimationField(r,"DeusEx.ScriptedPawn","PotentialEnemyAlliance",Kind::Name,0u,"NameProperty"),
                Value::Text(Kind::Name,"None"),execution);
        };
        q.nativeTime=[&](auto a) {
            auto* r=require(a);if(!r->nativeXLevel||!r->nativeXLevel->nativeTimeSeconds)
                throw std::runtime_error("Original ScriptedPawn Tick has no native ULevel Double clock");
            return *r->nativeXLevel->nativeTimeSeconds;
        };
        q.actorClass=[&](auto a){return reinterpret_cast<std::uintptr_t>(require(a)->cls);};
        q.superClass=[&](auto identity) {
            auto* cls=reinterpret_cast<RuntimeObject*>(identity);
            if(!cls||!cls->classDescriptor||cls->defaultOwner||!cls->classFieldLinksValid)
                throw std::runtime_error("Original ScriptedPawn Tick Class/Super metadata is malformed");
            return reinterpret_cast<std::uintptr_t>(cls->base);
        };
        q.nativePlayerPawnClass=[&]{return nativeClass("Engine.PlayerPawn","Engine.Pawn");};
        q.nativeDeusExPlayerClass=[&]{return nativeClass("DeusEx.DeusExPlayer","Extension.PlayerPawnExt");};
        q.readPlayerPawnField5D4=[&](auto a)->std::uint32_t {
            static_cast<void>(require(a));throw std::runtime_error("Original ScriptedPawn compatibility PlayerPawn field5D4 is not represented");
        };
        q.inStasis=[&](auto a){return OriginalActorStasis(require(a)).inStasis;};
        q.destroyActor=[&](auto a,bool force){static_cast<void>(DestroyNativeActor(activeTickLevel_,require(a),force?1:0,execution));};
        q.updateAgitation=[&](auto a,float dt){UpdateOriginalScriptedPawnDecay(require(a),dt,false,execution);};
        q.updateFear=[&](auto a,float dt){UpdateOriginalScriptedPawnDecay(require(a),dt,true,execution);};
        q.enableCloak=[&](auto a,bool enabled){OriginalScriptedPawnCallback(require(a),"EnableCloak",enabled,execution);};
        q.extinguishFire=[&](auto a){OriginalScriptedPawnCallback(require(a),"ExtinguishFire",{},execution);};
        q.spurtBlood=[&](auto a){OriginalScriptedPawnCallback(require(a),"SpurtBlood",{},execution);};
        q.appSqrt=[](double value){return QuestVr::NativeScalar::Sqrt(value);};
        q.baseTick=[&](auto a,float dt,std::uint32_t type){return AdvanceOriginalActorBaseTick(require(a),dt,static_cast<std::int32_t>(type),execution);};
        return QuestVr::TickOriginalScriptedPawn(source,delta,static_cast<std::uint32_t>(tickType),q).returned;
    }
