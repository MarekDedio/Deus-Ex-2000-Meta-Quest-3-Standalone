// Included inside PortableActorVmHost. Typed same-root MAIN+BLEND channels;
// no world Tick, state/timer/physics suffix or Pawn look/network preprocessing.
    Property OriginalBlendField(RuntimeObject* actor,const char* name,Kind kind,std::uint32_t flags,const char* type) {
        const auto key=std::string("Engine.Actor.")+name;const auto field=DescribeProperty(key);auto* node=Object(field.key);
        if(field.key!=key||!OwnsProperty(actor->cls,field)||field.zero.kind!=kind||field.arrayDimension!=4u||
            field.flags!=flags||!node->property||node->property->type!=type||node->property->baseField||
            node->property->arrayDimension!=4||node->property->flags!=flags||node->outer!=Object("Engine.Actor")||
            LowerAscii(Qualified(Table(node->sourcePath),Table(node->sourcePath).exports.at(node->exportIndex).ObjClass))!=
                std::string("core.")+LowerAscii(type))
            throw std::runtime_error("Original blend lacks its actual four-slot Actor field: "+key);
        if((kind==Kind::Float||kind==Kind::Name)&&node->property->referencedType)
            throw std::runtime_error("Original blend scalar has an unexpected referenced subtype: "+key);
        return field;
    }
    Property OriginalBlendFloatField(RuntimeObject* actor,QuestVr::OriginalActorBlendFloat selected) {
        using F=QuestVr::OriginalActorBlendFloat;const char* name{};std::uint32_t flags{};
        switch(selected) {
        case F::Frame:name="BlendAnimFrame";break;
        case F::Rate:name="BlendAnimRate";break;
        case F::TweenRate:name="BlendTweenRate";break;
        case F::Last:name="BlendAnimLast";break;
        case F::MinRate:name="BlendAnimMinRate";flags=0x20u;break;
        default:throw std::runtime_error("Original blend selector is not a scalar Float array");
        }
        return OriginalBlendField(actor,name,Kind::Float,flags,"FloatProperty");
    }
    Property OriginalBlendPlaneField(RuntimeObject* actor) {
        const auto field=OriginalBlendField(actor,"SimBlendAnim",Kind::Struct,0x20u,"StructProperty");
        auto* node=Object(field.key);
        if(!node->property->referencedType)throw std::runtime_error("Original SimBlendAnim lacks its actual Plane constraint");
        auto* structure=Object(Qualified(Table(node->sourcePath),node->property->referencedType));
        if(LowerAscii(structure->reflection.objectPath)!="core.object.plane"||structure->outer!=Object("Core.Object")||
            LowerAscii(Qualified(Table(structure->sourcePath),Table(structure->sourcePath).exports.at(structure->exportIndex).ObjClass))!="core.struct")
            throw std::runtime_error("Original SimBlendAnim constraint is not the actual Core.Object.Plane");
        const auto schema=StructSchema(structure);const auto& declaration=StructDescriptor(structure);
        if(declaration.baseField||declaration.logicalSize||!declaration.rawBytes.empty()||!declaration.bytecode.empty()||
            schema->kind!=Kind::Struct||schema->fields.size()!=4u)
            throw std::runtime_error("Original SimBlendAnim is not its canonical four-Float Plane");
        constexpr std::array<const char*,4u> names{"x","y","z","w"};auto child=declaration.children;
        for(std::size_t slot=0u;slot<names.size();++slot) {
            if(!child)throw std::runtime_error("Original SimBlendAnim Plane chain is truncated");
            auto* member=Object(Qualified(Table(structure->sourcePath),child));
            if(member->outer!=structure||!member->property||member->property->type!="FloatProperty"||
                member->property->arrayDimension!=1||member->property->baseField||member->property->referencedType||
                LowerAscii(member->reflection.objectPath)!=std::string("core.object.plane.")+names[slot]||
                LowerAscii(schema->fields[slot].key)!=LowerAscii(member->reflection.objectPath)||
                schema->fields[slot].arrayDimension!=1u||schema->fields[slot].zero.kind!=Kind::Float||schema->fields[slot].nested||
                LowerAscii(Qualified(Table(member->sourcePath),Table(member->sourcePath).exports.at(member->exportIndex).ObjClass))!="core.floatproperty")
                throw std::runtime_error("Original SimBlendAnim Plane lacks a declared scalar Float channel");
            child=member->property->nextField;
        }
        if(child)throw std::runtime_error("Original SimBlendAnim Plane has extra members");
        return field;
    }
    Value OriginalBlendRead(RuntimeObject* actor,const Property& property,unsigned slot) {
        if(slot>=4u)throw std::runtime_error("Original blend slot is outside its four native channels");
        const auto value=Read(actor,property,slot);
        if(value.kind!=property.zero.kind)throw std::runtime_error("Original blend live field has changed kind");
        if(value.kind==Kind::Struct) {
            if(value.fields.size()!=4u)throw std::runtime_error("Original SimBlendAnim live Plane changed size");
            for(const auto* name:{"x","y","z","w"}) {
                const auto found=value.fields.find(name);
                if(found==value.fields.end()||found->second.kind!=Kind::Float)
                    throw std::runtime_error("Original SimBlendAnim live Plane changed channel kind");
            }
        }
        return value;
    }
    std::shared_ptr<Reference> OriginalBlendReference(RuntimeObject* actor,const Property& field,unsigned slot,
        QuestVr::OriginalActorBlendFloat selected) {
        using F=QuestVr::OriginalActorBlendFloat;
        if(slot>=4u)throw std::runtime_error("Original blend write exceeds four channels");
        const bool plane=selected==F::SimX||selected==F::SimY;
        auto reference=std::make_shared<Reference>();reference->zero=Value::Float(0.0f);reference->dimension=1u;
        reference->read=[this,actor,field,slot,plane,selected] {
            MoveEventActor(actor);const auto value=OriginalBlendRead(actor,field,slot);
            return plane?value.fields.at(selected==F::SimX?"x":"y"):value;
        };
        reference->write=[this,actor,field,slot,plane,selected](const Value& incoming) {
            MoveEventActor(actor);
            if(incoming.kind!=Kind::Float)throw std::runtime_error("Original blend native store changed Float kind");
            auto value=incoming;
            if(plane) {value=OriginalBlendRead(actor,field,slot);value.fields.at(selected==F::SimX?"x":"y")=incoming;}
            auto normalized=TypedLiveValue(field,value);
            Touch(actor);actor->scriptValues[InstanceStorageKey(actor->cls,field)][slot]=std::move(normalized);
            actor->committedScriptState=true;
            if(actor->animationClock) {
                auto& clock=*actor->animationClock;
                switch(selected) {
                case F::Frame:clock.pose.blends[slot].normalizedFrame=incoming.floating;break;
                case F::Rate:clock.blends[slot].rate=incoming.floating;break;
                case F::TweenRate:clock.blends[slot].tweenRate=incoming.floating;break;
                case F::Last:clock.blends[slot].last=incoming.floating;break;
                case F::MinRate:clock.blends[slot].minRate=incoming.floating;break;
                case F::SimX:clock.blends[slot].simulated[0u]=incoming.floating;break;
                case F::SimY:clock.blends[slot].simulated[1u]=incoming.floating;break;
                }
            }
            ValidateStateBudget(); // Never rebuild unrelated channels/history.
        };
        return reference;
    }
    QuestVr::OriginalActorBlendPhaseStats AdvanceOriginalBlendAnimation(RuntimeObject* actor,
        const QuestVr::OriginalActorAnimationPhaseStats& main,QuestVr::Vm::Execution& execution) {
        if(!transaction_)throw std::runtime_error("Original blend requires its existing caller transaction");
        actor=MoveEventActor(actor);const auto source=reinterpret_cast<std::uintptr_t>(actor);
        const auto receiver=[&](std::uintptr_t identity) {
            if(identity!=source)throw std::runtime_error("Original blend changed its retained Actor");
            return MoveEventActor(actor);
        };
        using F=QuestVr::OriginalActorBlendFloat;
        QuestVr::OriginalActorBlendPhaseQueries q;q.work=[this]{LifecycleStep();};
        q.mainSequence=[&](auto a) {
            auto* current=receiver(a);
            return OriginalAnimationRead(current,OriginalAnimationField(current,"Engine.Actor","AnimSequence",Kind::Name,0x21u,"NameProperty"),Kind::Name).text;
        };
        q.readMainFloat=[&](auto a,auto selected) {
            auto* current=receiver(a);return OriginalAnimationRead(current,OriginalAnimationFloatField(current,selected),Kind::Float).floating;
        };
        q.sequence=[&](auto a,unsigned slot) {
            auto* current=receiver(a);return OriginalBlendRead(current,
                OriginalBlendField(current,"BlendAnimSequence",Kind::Name,0x20u,"NameProperty"),slot).text;
        };
        q.readFloat=[&](auto a,unsigned slot,F selected) {
            auto* current=receiver(a);const bool plane=selected==F::SimX||selected==F::SimY;
            const auto value=OriginalBlendRead(current,plane?OriginalBlendPlaneField(current):OriginalBlendFloatField(current,selected),slot);
            return plane?value.fields.at(selected==F::SimX?"x":"y").floating:value.floating;
        };
        q.writeFloat=[&](auto a,unsigned slot,F selected,float value) {
            auto* current=receiver(a);const bool plane=selected==F::SimX||selected==F::SimY;
            const auto field=plane?OriginalBlendPlaneField(current):OriginalBlendFloatField(current,selected);
            ReserveActorPhysicsReferences(field,1u);
            execution.WriteReference(OriginalBlendReference(current,field,slot,selected),Value::Float(value));
        };
        q.velocity=[&](auto a,unsigned axis) {
            if(axis>=3u)throw std::runtime_error("Original blend Velocity channel exceeds actual XYZ fields");
            auto* current=receiver(a);
            return OriginalAnimationRead(current,NativeCoordsProperty(current,"Engine.Actor.Velocity",Kind::Vector,0x21u),Kind::Vector).vector[axis];
        };
        q.remoteRole=[&](auto a) {
            auto* current=receiver(a);const auto field=OriginalAnimationField(current,"Engine.Actor","RemoteRole",
                Kind::Byte,0x21u,"ByteProperty");auto* metadata=Object(field.key);
            if(!metadata->property->referencedType||
                LowerAscii(Qualified(Table(metadata->sourcePath),metadata->property->referencedType))!="engine.actor.enetrole")
                throw std::runtime_error("Original blend RemoteRole lacks its actual ENetRole declaration");
            return static_cast<std::uint8_t>(OriginalAnimationRead(current,field,Kind::Byte).integer);
        };
        q.isWeapon=[&](auto a){receiver(a);return RetailTouchClass(a,"Engine.Weapon");};
        return QuestVr::AdvanceOriginalActorBlendAnimation(source,main,q);
    }
    Value AdvanceOriginalAnimationChannels(const std::string& path,float elapsed,PortableActorAnimationStepResult& result,
        QuestVr::Vm::Execution& execution) {
        auto* actor=MoveEventActor(Object(path));
        const bool suppress=OriginalAnimationRead(actor,OriginalAnimationField(actor,"Engine.Actor","bIsPawn",Kind::Bool,2u,"BoolProperty"),Kind::Bool).boolean&&
            IsDerivedFromPath(actor->cls,"Engine.Pawn")&&OriginalAnimationRead(actor,
                OriginalAnimationField(actor,"Engine.Actor","Role",Kind::Byte,0x20u,"ByteProperty"),Kind::Byte).integer==2;
        const auto main=AdvanceOriginalMainAnimation(actor,elapsed,suppress,execution);
        result.mainIterations=main.iterations;result.mainRemainingSeconds=main.remainingSeconds;
        const auto blends=AdvanceOriginalBlendAnimation(actor,main,execution);
        result.blendIterations=blends.iterations;result.blendWrites=blends.floatWrites;
        result.remainingSeconds=blends.remainingSeconds;result.completed=true;
        return Value{};
    }
