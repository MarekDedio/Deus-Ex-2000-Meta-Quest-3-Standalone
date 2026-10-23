// Original3970/setPhysics, using the caller's real support services/VM root.
    std::shared_ptr<const Function> ActorSetPhysicsDeclaration(const Function* supplied) {
        const auto authored=FunctionFor(Object("Engine.Actor.SetPhysics"));
        const auto validate=[&](const Function& function) {
            auto* node=Object(function.path);
            if(function.path!="Engine.Actor.SetPhysics" || function.nativeIndex!=3970u ||
                function.flags!=0x401u || function.variables.size()!=2u || !node->script ||
                function.source!=node->sourcePath || node->outer!=Object("Engine.Actor") ||
                function.nativeIndex!=node->script->nativeIndex || function.flags!=node->script->functionFlags ||
                LowerAscii(Qualified(Table(node->sourcePath),Table(node->sourcePath).exports.at(node->exportIndex).ObjClass))!="core.function")
                throw std::runtime_error("SetPhysics is not its original native declaration");
            for(std::size_t slot=0u;slot<2u;++slot) {
                const auto& field=function.variables[slot];auto* metadata=Object(field.key);
                if(field.key!=(slot==0u?"Engine.Actor.SetPhysics.newPhysics":"Engine.Actor.SetPhysics.newFloor") ||
                    field.zero.kind!=(slot==0u?Kind::Byte:Kind::Object) || field.arrayDimension!=1u ||
                    field.flags!=(slot==0u?0x80u:0x90u) || metadata->outer!=Object(function.path) ||
                    !metadata->property || metadata->property->type!=(slot==0u?"ByteProperty":"ObjectProperty") ||
                    metadata->property->baseField || LowerAscii(Qualified(Table(metadata->sourcePath),
                    metadata->property->referencedType))!=(slot==0u?"engine.actor.ephysics":"engine.actor") ||
                    LowerAscii(Qualified(Table(metadata->sourcePath),Table(metadata->sourcePath).exports.at(metadata->exportIndex).ObjClass))!=
                        (slot==0u?"core.byteproperty":"core.objectproperty"))
                    throw std::runtime_error("SetPhysics lacks its original required enum Byte/optional Actor packet");
            }
        };
        validate(*authored);
        if(supplied) {
            if(supplied->path!=authored->path || supplied->source!=authored->source ||
                supplied->nativeIndex!=authored->nativeIndex || supplied->flags!=authored->flags)
                throw std::runtime_error("SetPhysics has a foreign callable identity");
            if(!supplied->variables.empty())validate(*supplied);
        }
        return authored;
    }
    Property ActorPhysicsProperty(RuntimeObject* actor) {
        RequireMapActor(actor);
        const auto property=NoiseProperty(actor,"Engine.Actor.Physics",Kind::Byte,0x23u,"Engine.Actor.EPhysics");
        auto* field=Object(property.key);
        if(field->property->baseField || field->outer!=Object("Engine.Actor") ||
            LowerAscii(Qualified(Table(field->sourcePath),Table(field->sourcePath).exports.at(field->exportIndex).ObjClass))!="core.byteproperty")
            throw std::runtime_error("Original Physics field lacks its actual Actor/Core.ByteProperty declaration");
        return property;
    }
    Property ActorPhysicsVectorProperty(RuntimeObject* actor,const char* key) {
        RequireMapActor(actor);
        const bool velocity=std::string_view(key)=="Engine.Actor.Velocity";
        if(!velocity && std::string_view(key)!="Engine.Actor.Acceleration")
            throw std::runtime_error("Raw physics Vector capability is limited to original Velocity/Acceleration");
        return NativeCoordsProperty(actor,key,Kind::Vector,velocity?0x21u:0x20u);
    }
    Value DecodeActorPhysicsVector(const PortableTaggedProperty& tag,const Property& field) {
        // Only canonical Actor V/A schema can select this raw native read.
        // General VM and checkpoint Vector finite validation stays unchanged.
        if(tag.type!=10u || tag.arrayIndex || tag.value.size()!=12u)
            throw std::runtime_error("Raw physics Vector has a different original tag layout");
        QuestVr::ValidateAuthoredStructTag(*StructPropertySchema(field.key),tag.structName.ToString());
        auto value=Value::Vector({});
        for(std::size_t channel=0u;channel<3u;++channel) {
            const auto at=channel*4u;const std::uint32_t bits=std::uint32_t(tag.value[at])|
                (std::uint32_t(tag.value[at+1u])<<8u)|(std::uint32_t(tag.value[at+2u])<<16u)|
                (std::uint32_t(tag.value[at+3u])<<24u);
            std::memcpy(&value.vector[channel],&bits,sizeof(bits));
        }
        return value;
    }
    Value ReadActorPhysicsVector(RuntimeObject* actor,const Property& field) {
        const auto value=Read(actor,field,0u,false,false,nullptr,false,true);
        if(value.kind!=Kind::Vector || !value.text.empty() || !value.fields.empty() || !value.intArray.empty())
            throw std::runtime_error("Raw physics Vector storage is not canonical");
        return value;
    }
    void ReserveActorPhysicsReferences(const Property& property,std::size_t copies) {
        // Both closures and their Property copies survive to OUTER VM undo.
        const auto payload=property.key.capacity()+property.name.capacity()+property.zero.text.capacity()+3u;
        const auto per=sizeof(Reference)+2u*(sizeof(Property)+4u*sizeof(void*)+payload)+256u;
        const auto bytes=QuestVr::DecalStateDetail::Multiply(per,copies);
        ValidateStateBudget(bytes);nativePhysicsReferenceBytes_=QuestVr::DecalStateDetail::Add(nativePhysicsReferenceBytes_,bytes);
    }
    void WriteActorPhysicsByte(RuntimeObject* actor,const Property& property,std::uint8_t value,
        QuestVr::Vm::Execution& execution) {
        ReserveActorPhysicsReferences(property,1u);
        auto reference=std::make_shared<Reference>();reference->zero=Value::Byte(0u);
        reference->read=[this,actor,property]{return Read(actor,property,0u);};
        reference->write=[this,actor,property](const Value& next) {
            if(next.kind!=Kind::Byte || next.integer<0 || next.integer>255)
                throw std::runtime_error("Original Physics store requires a raw Byte");
            Touch(actor);actor->scriptValues[InstanceStorageKey(actor->cls,property)][0u]=next;
            actor->committedScriptState=true; // No animation-clock reconstruction.
        };
        execution.WriteReference(reference,Value::Byte(value));
    }
    void WriteActorPhysicsVectorWord(RuntimeObject* actor,const Property& field,unsigned channel,
        std::uint32_t bits,QuestVr::Vm::Execution& execution) {
        if(channel>=3u)throw std::runtime_error("Original physics Vector word is out of bounds");
        // An INT DWORD proxy journals inactive IEEE bits without converting
        // them to arithmetic Float/Vector operands or rebuilding animation.
        auto reference=std::make_shared<Reference>();reference->zero=Value::Integer(0);
        reference->read=[this,actor,field,channel] {
            const auto value=ReadActorPhysicsVector(actor,field);
            return Value::Integer(std::bit_cast<std::int32_t>(value.vector[channel]));
        };
        reference->write=[this,actor,field,channel](const Value& next) {
            if(next.kind!=Kind::Int)throw std::runtime_error("Original physics raw word journal is not INT");
            auto merged=ReadActorPhysicsVector(actor,field);const auto word=std::bit_cast<std::uint32_t>(next.integer);
            std::memcpy(&merged.vector[channel],&word,sizeof(word));Touch(actor);
            actor->scriptValues[InstanceStorageKey(actor->cls,field)][0u]=std::move(merged);
            actor->committedScriptState=true;
        };
        execution.WriteReference(reference,Value::Integer(std::bit_cast<std::int32_t>(bits)));
    }
    Evaluation ActorSetPhysicsNative(RuntimeObject* actor,const std::vector<Evaluation>& arguments,
        const Function* declaration,QuestVr::Vm::Execution& execution) {
        if(!transaction_)throw std::runtime_error("SetPhysics requires its actual caller root");
        const auto authored=ActorSetPhysicsDeclaration(declaration);const auto physicsField=ActorPhysicsProperty(actor);
        if(arguments.empty() || arguments.size()>2u)throw std::runtime_error("SetPhysics requires its original one/two input packet");
        auto mode=arguments[0u].Load();if(mode.kind==Kind::Nothing)mode=Value::Byte(0u);
        if(mode.kind!=Kind::Byte && mode.kind!=Kind::Int && mode.kind!=Kind::Float)
            throw std::runtime_error("SetPhysics requires a numeric physics Byte");
        mode=TypedLiveValue(authored->variables[0u],mode);
        auto floorValue=arguments.size()==2u?arguments[1u].Load():Value::Text(Kind::Object,{});
        if(floorValue.kind==Kind::Nothing)floorValue=Value::Text(Kind::Object,{});
        if(floorValue.kind!=Kind::Object)throw std::runtime_error("SetPhysics floor is not Actor/None");
        floorValue=TypedLiveValue(authored->variables[1u],floorValue);
        auto* floor=floorValue.text.empty()?nullptr:Object(floorValue.text);if(floor)RequireMapActor(floor);
        QuestVr::NativeScratchScope scratch(nativeQueryScratchBytes_,8192u,[this](auto bytes){ValidateStateBudget(bytes);});
        QuestVr::OriginalActorPhysicsQueries q;std::optional<std::array<Property,2u>> vectors;
        const auto source=reinterpret_cast<std::uintptr_t>(actor);
        const auto requireSource=[&](std::uintptr_t identity){if(identity!=source)throw std::runtime_error("SetPhysics source identity changed");};
        q.work=[this]{LifecycleStep();};
        q.physics=[&](auto identity){requireSource(identity);const auto value=Read(actor,physicsField,0u);
            if(value.kind!=Kind::Byte)throw std::runtime_error("Original Physics storage is not Byte");
            return static_cast<std::uint8_t>(value.integer);};
        q.writePhysics=[&](auto identity,std::uint8_t value){requireSource(identity);WriteActorPhysicsByte(actor,physicsField,value,execution);};
        q.base=[&](auto identity){requireSource(identity);const auto value=RetailPredicatePointerRead(actor,"Engine.Actor.Base","Engine.Actor",0x22u);
            return value.text.empty()?std::uintptr_t{}:reinterpret_cast<std::uintptr_t>(Object(value.text));};
        q.setSupportBase=[&](auto identity,auto target){requireSource(identity);
            if(target!=reinterpret_cast<std::uintptr_t>(floor))throw std::runtime_error("SetPhysics support floor changed");
            static_cast<void>(RetailSetSupportBase(actor,floor,execution));};
        q.findBase=[&](auto identity){requireSource(identity);static_cast<void>(RetailFindBase(actor,execution));};
        q.setBase=[&](auto identity,auto target,std::int32_t send){requireSource(identity);
            if(target || send!=1)throw std::runtime_error("SetPhysics detach changed None/true");
            SetBase(actor,nullptr,true,execution);};
        q.writeVectorWord=[&](auto identity,QuestVr::OriginalPhysicsVector which,unsigned channel,std::uint32_t bits) {
            requireSource(identity);
            if(!vectors) {
                vectors.emplace(std::array<Property,2u>{ActorPhysicsVectorProperty(actor,"Engine.Actor.Velocity"),
                    ActorPhysicsVectorProperty(actor,"Engine.Actor.Acceleration")});
                // Admit all six proxies BEFORE intermediate native raw words
                // exist. There are no callbacks between these six stores.
                ReserveActorPhysicsReferences((*vectors)[0u],3u);ReserveActorPhysicsReferences((*vectors)[1u],3u);
            }
            WriteActorPhysicsVectorWord(actor,(*vectors)[which==QuestVr::OriginalPhysicsVector::Velocity?0u:1u],channel,bits,execution);
        };
        static_cast<void>(QuestVr::SetOriginalActorPhysics(source,static_cast<std::uint8_t>(mode.integer),
            reinterpret_cast<std::uintptr_t>(floor),q));
        ValidateStateBudget();return {};
    }
