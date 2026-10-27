// Included inside PortableActorVmHost. Native stored regions/selected events
// join the SAME caller VM journal. Not the read-only Region facade, a forced
// begun-world fixture, or a fake Move/SetBase/water-physics implementation.
    RuntimeObject* ZoneLevel(std::uintptr_t identity) {
        if(!identity||!persistentObjectLookup||!persistentObjectLookup->active)
            throw std::runtime_error("Original zone Level requires live UObject lookup authority");
        const auto* record=persistentObjectLookup->index.RecordForIdentity(identity);
        if(!record)throw std::runtime_error("Original zone Level identity is retired");
        auto* level=reinterpret_cast<RuntimeObject*>(identity);
        if(level->lookupControl!=persistentObjectLookup||level->objectIndex.Index()!=record->index||
            !level->nativeMapGeneration||level->nativeMapGeneration!=persistentMapGenerationCounter||
            level->sourcePath!=persistentMapSourcePath||
            LowerAscii(CastObjectIdentity(level->reflection.objectPath).classPath)!="engine.level")
            throw std::runtime_error("Original zone receiver is not the current loader-owned native Level");
        return level; // No slot0 validation is hidden in this identity gate.
    }
    RuntimeObject* ZoneActor(std::uintptr_t identity,RuntimeObject* level) {
        if(!identity||!persistentObjectLookup||!persistentObjectLookup->active)
            throw std::runtime_error("Original zone Actor requires live UObject lookup authority");
        const auto* record=persistentObjectLookup->index.RecordForIdentity(identity);
        if(!record)throw std::runtime_error("Original zone Actor identity is retired");
        auto* actor=reinterpret_cast<RuntimeObject*>(identity);
        if(actor->lookupControl!=persistentObjectLookup||actor->objectIndex.Index()!=record->index||actor->defaultOwner||
            !actor->cls||!IsDerivedFromPath(actor->cls,"Engine.Actor")||actor->nativeXLevel!=level)
            throw std::runtime_error("Original zone Actor is not in its supplied current native Level");
        return actor; // Pending deletion is NOT a new postcallback early exit.
    }
    Property ZoneRegionProperty(RuntimeObject* actor,QuestVr::OriginalActorRegionSlot slot) {
        using Slot=QuestVr::OriginalActorRegionSlot;
        const char* key{};const char* owner{};std::uint32_t flags{};
        switch(slot){
        case Slot::Main:key="Engine.Actor.Region";owner="Engine.Actor";flags=2u;break;
        case Slot::Foot:key="Engine.Pawn.FootRegion";owner="Engine.Pawn";break;
        case Slot::Head:key="Engine.Pawn.HeadRegion";owner="Engine.Pawn";break;
        default:throw std::runtime_error("Unknown original region slot");
        }
        const auto property=DescribeProperty(key);auto* field=Object(property.key);
        if(property.key!=key||!OwnsProperty(actor->cls,property)||property.zero.kind!=Kind::Struct||
            property.arrayDimension!=1u||property.flags!=flags||!field->property||field->property->type!="StructProperty"||
            field->property->baseField||field->outer!=Object(owner)||!field->property->referencedType||
            LowerAscii(Qualified(Table(field->sourcePath),Table(field->sourcePath).exports.at(field->exportIndex).ObjClass))!="core.structproperty")
            throw std::runtime_error("Original region lacks its exact declared scalar PointRegion field");
        auto* structure=Object(Qualified(Table(field->sourcePath),field->property->referencedType));
        if(structure!=Object("Engine.Actor.PointRegion")||structure->outer!=Object("Engine.Actor")||
            LowerAscii(Qualified(Table(structure->sourcePath),Table(structure->sourcePath).exports.at(structure->exportIndex).ObjClass))!="core.struct")
            throw std::runtime_error("Original region does not reference the actual Actor.PointRegion Struct");
        const auto& declaration=StructDescriptor(structure);const auto schema=StructSchema(structure);
        if(declaration.baseField||declaration.logicalSize||!declaration.rawBytes.empty()||!declaration.bytecode.empty()||
            schema->kind!=Kind::Struct||schema->fields.size()!=3u)
            throw std::runtime_error("Original PointRegion has an unsupported base/script/member layout");
        constexpr std::array<const char*,3u> names{"zone","ileaf","zonenumber"};
        constexpr std::array<const char*,3u> types{"ObjectProperty","IntProperty","ByteProperty"};
        constexpr std::array<Kind,3u> kinds{Kind::Object,Kind::Int,Kind::Byte};
        for(unsigned i=0u;i<3u;++i){
            const auto& member=schema->fields[i];auto* metadata=Object(member.key);
            if(LowerAscii(member.key)!=std::string("engine.actor.pointregion.")+names[i]||LowerAscii(member.name)!=names[i]||
                member.zero.kind!=kinds[i]||member.arrayDimension!=1u||member.nested||!metadata->property||
                metadata->property->type!=types[i]||metadata->property->baseField||metadata->property->flags||
                metadata->outer!=structure||
                LowerAscii(Qualified(Table(metadata->sourcePath),Table(metadata->sourcePath).exports.at(metadata->exportIndex).ObjClass))!=
                    std::string("core.")+LowerAscii(types[i]))
                throw std::runtime_error("Original PointRegion member schema is not Zone/iLeaf/ZoneNumber");
            if(i==0u){
                if(!metadata->property->referencedType||
                    LowerAscii(Qualified(Table(metadata->sourcePath),metadata->property->referencedType))!="engine.zoneinfo")
                    throw std::runtime_error("Original PointRegion.Zone has the wrong ZoneInfo constraint");
            }else if(metadata->property->referencedType)
                throw std::runtime_error("Original PointRegion scalar unexpectedly has a referenced type");
        }
        return property;
    }
    Value ZoneStoredRegion(RuntimeObject* actor,const Property& property) {
        const auto value=Read(actor,property,0u,false,false,nullptr,true);
        if(value.kind!=Kind::Struct||value.fields.size()!=3u||value.fields.at("zone").kind!=Kind::Object||
            value.fields.at("ileaf").kind!=Kind::Int||value.fields.at("zonenumber").kind!=Kind::Byte)
            throw std::runtime_error("Original stored region has an invalid live tuple");
        return value;
    }
    void ZoneWriteMember(RuntimeObject* actor,QuestVr::OriginalActorRegionSlot slot,const char* name,
        const Value& value,QuestVr::Vm::Execution& execution) {
        const auto property=ZoneRegionProperty(actor,slot);auto target=ReferenceFor(actor,property,0u,false);
        target->read=[this,actor,property]{return ZoneStoredRegion(actor,property);};
        auto updated=target->read();updated.fields.at(name)=value;
        execution.WriteReference(target,updated);
    }
    void ZoneSelectedEvent(RuntimeObject* receiver,QuestVr::OriginalActorZoneEvent selected,RuntimeObject* argument,
        QuestVr::Vm::Execution& execution) {
        using Event=QuestVr::OriginalActorZoneEvent;
        const char *owner{},*name{},*constraint{},*parameterName{};std::uint32_t flags{};
        switch(selected){
        case Event::Leaving:owner="Engine.ZoneInfo";name="ActorLeaving";constraint="Engine.Actor";parameterName="other";flags=0x802u;break;
        case Event::Entered:owner="Engine.ZoneInfo";name="ActorEntered";constraint="Engine.Actor";parameterName="other";flags=0x802u;break;
        case Event::Change:owner="Engine.Actor";name="ZoneChange";constraint="Engine.ZoneInfo";parameterName="newzone";flags=0x800u;break;
        case Event::FootChange:owner="Engine.Pawn";name="FootZoneChange";constraint="Engine.ZoneInfo";parameterName="newfootzone";flags=0x802u;break;
        case Event::HeadChange:owner="Engine.Pawn";name="HeadZoneChange";constraint="Engine.ZoneInfo";parameterName="newheadzone";flags=0x802u;break;
        default:throw std::runtime_error("Unknown original native zone event");
        }
        if(!IsDerivedFromPath(receiver->cls,owner)||!IsDerivedFromPath(argument->cls,constraint))
            throw std::runtime_error("Original zone event receiver/argument has an unsafe native type");
        const auto validate=[&](const Function& function,bool base){
            const auto expected=LowerAscii(std::string(owner)+'.'+name);
            if(base&&(LowerAscii(function.path)!=expected||function.nativeIndex||function.flags!=flags))
                throw std::runtime_error("Original zone callback base event declaration is invalid");
            const Property* parameter{};
            for(const auto& member:function.variables)if(member.flags&0x80u){
                if(parameter)throw std::runtime_error("Original zone event has extra parameter/result fields");
                parameter=&member;
            }
            if(!parameter)throw std::runtime_error("Original zone event has no native Object parameter");
            auto* metadata=Object(parameter->key);
            if((base&&LowerAscii(parameter->key)!=expected+'.'+parameterName)||parameter->zero.kind!=Kind::Object||
                parameter->arrayDimension!=1u||parameter->flags!=0x80u||!metadata->property||
                metadata->property->type!="ObjectProperty"||metadata->property->baseField||metadata->outer!=Object(function.path)||
                !metadata->property->referencedType||
                LowerAscii(Qualified(Table(metadata->sourcePath),metadata->property->referencedType))!=LowerAscii(constraint)||
                LowerAscii(Qualified(Table(metadata->sourcePath),Table(metadata->sourcePath).exports.at(metadata->exportIndex).ObjClass))!="core.objectproperty")
                throw std::runtime_error("Original zone callback native packet constraint is invalid");
        };
        validate(*FunctionFor(Object(std::string(owner)+'.'+name)),true);
        const auto method=Member(receiver,name);
        if(!method)throw std::runtime_error("Original zone FindFunctionChecked found no selected callback");
        validate(*FunctionFor(Object(method->path)),false);
        const auto scratch=QuestVr::DecalStateDetail::Add(1024u+2u*sizeof(Evaluation),
            QuestVr::DecalStateDetail::Multiply(argument->reflection.objectPath.size()+1u,4u));
        QuestVr::NativeScratchScope reservation(nativeQueryScratchBytes_,scratch,[this](std::size_t extra){ValidateStateBudget(extra);});
        static_cast<void>(execution.CallEvent(receiver->reflection.objectPath,name,true,
            {{Value::Text(Kind::Object,argument->reflection.objectPath),{}}}));
    }
    QuestVr::OriginalActorZoneQueries ActorZoneQueries(RuntimeObject* level,QuestVr::Vm::Execution& execution) {
        using Slot=QuestVr::OriginalActorRegionSlot;QuestVr::OriginalActorZoneQueries q;
        const auto actor=[this,level](std::uintptr_t a){return ZoneActor(a,level);};
        q.deleted=[this,actor](std::uintptr_t a){return NoiseField(actor(a),"Engine.Actor.bDeleteMe",Kind::Bool,2u).boolean;};
        q.pawn=[actor](std::uintptr_t a){return IsDerivedFromPath(actor(a)->cls,"Engine.Pawn");};
        q.levelActor0=[this](std::uintptr_t l){
            static_cast<void>(ZoneLevel(l));
            if(persistentWorldActors.empty())throw std::runtime_error("Original zone slot0 data is outside its safe native array");
            return reinterpret_cast<std::uintptr_t>(persistentWorldActors.front());
        };
        q.requireLevelInfo=[actor](std::uintptr_t a){
            if(!IsDerivedFromPath(actor(a)->cls,"Engine.LevelInfo"))throw std::runtime_error("Original zone slot0 is not LevelInfo");
        };
        q.levelActorCount=[this](std::uintptr_t l){
            static_cast<void>(ZoneLevel(l));
            if(persistentWorldActors.size()>65'536u)throw std::runtime_error("Original zone native Actor count exceeds its portable bound");
            return static_cast<std::int32_t>(persistentWorldActors.size());
        };
        q.location=[this,actor](std::uintptr_t a,unsigned channel){
            if(channel>=3u)throw std::runtime_error("Original zone Location channel is invalid");
            return NativeCoordsRead(actor(a),"Engine.Actor.Location",Kind::Vector,0x23u).vector[channel];
        };
        q.collisionHeight=[this,actor](std::uintptr_t a){return NoiseField(actor(a),"Engine.Actor.CollisionHeight",Kind::Float,0x23u).floating;};
        q.eyeHeight=[this,actor](std::uintptr_t a){return NoiseField(actor(a),"Engine.Pawn.EyeHeight",Kind::Float,0x20u).floating;};
        q.pointRegion=[this,actor](std::uintptr_t l,std::uintptr_t fallback,PortableModelVec3 point){
            auto* actual=ZoneLevel(l);auto* zone=actor(fallback);
            const auto& model=NativeLevelTraceModel(actual);
            const auto found=QuestVr::QueryOriginalModelPointRegion(model,point,std::min<std::size_t>(model.nodes.size(),65'536u),[this]{LifecycleStep();});
            if(found.zoneActorReference) {
                const auto path=ContextValuePath(Qualified(Table(actual->nativeLevelModel->sourcePath),found.zoneActorReference));
                if(!path.empty())zone=Object(path);
            }
            static_cast<void>(ZoneActor(reinterpret_cast<std::uintptr_t>(zone),actual));
            return QuestVr::OriginalActorRegion{reinterpret_cast<std::uintptr_t>(zone),found.leaf,found.zoneNumber};
        };
        q.regionZone=[this,actor](std::uintptr_t a,Slot slot){
            auto* current=actor(a);const auto value=ZoneStoredRegion(current,ZoneRegionProperty(current,slot)).fields.at("zone");
            return value.text.empty()?std::uintptr_t{}:reinterpret_cast<std::uintptr_t>(Object(value.text));
        };
        q.writeZone=[this,actor,&execution](std::uintptr_t a,Slot slot,std::uintptr_t z){
            auto* zone=z?actor(z):nullptr;
            ZoneWriteMember(actor(a),slot,"zone",Value::Text(Kind::Object,zone?zone->reflection.objectPath:std::string{}),execution);
        };
        q.writeLeaf=[this,actor,&execution](std::uintptr_t a,Slot slot,std::int32_t leaf){ZoneWriteMember(actor(a),slot,"ileaf",Value::Integer(leaf),execution);};
        q.writeNumber=[this,actor,&execution](std::uintptr_t a,Slot slot,std::uint8_t n){ZoneWriteMember(actor(a),slot,"zonenumber",Value::Byte(n),execution);};
        q.event=[this,actor,&execution](std::uintptr_t a,QuestVr::OriginalActorZoneEvent e,std::uintptr_t b){ZoneSelectedEvent(actor(a),e,actor(b),execution);};
        q.netMode=[this,actor](std::uintptr_t a){return static_cast<std::uint8_t>(NoiseField(actor(a),"Engine.LevelInfo.NetMode",Kind::Byte,0u,"Engine.LevelInfo.ENetMode").integer);};
        q.replicationInfo=[this,actor](std::uintptr_t a){auto* pri=NoiseObjectField(actor(a),"Engine.Pawn.PlayerReplicationInfo",0x20u,"Engine.PlayerReplicationInfo");
            return reinterpret_cast<std::uintptr_t>(pri);};
        q.writePlayerZone=[this,actor,&execution](std::uintptr_t a,std::uintptr_t z){
            auto* pri=actor(a);auto* zone=z?actor(z):nullptr;
            const auto property=RetailPredicatePointerField(pri,"Engine.PlayerReplicationInfo.PlayerZone","Engine.PlayerReplicationInfo","Engine.ZoneInfo",0x20u);
            execution.WriteReference(ReferenceFor(pri,property,0u,false),Value::Text(Kind::Object,zone?zone->reflection.objectPath:std::string{}));
        };
        q.work=[this]{LifecycleStep();};return q;
    }
    std::vector<std::uint8_t> ZoneClockSnapshot(RuntimeObject* actor) {
        if(!actor->animationClock)return {};
        std::size_t strings=actor->reflection.objectPath.size()+actor->cls->reflection.objectPath.size()+persistentMapPackageName.size()+3u;
        strings=QuestVr::DecalStateDetail::Add(strings,actor->animationClock->pose.main.sequence.size()+1u);
        for(const auto& blend:actor->animationClock->pose.blends)strings=QuestVr::DecalStateDetail::Add(strings,blend.sequence.size()+1u);
        const auto scratch=QuestVr::DecalStateDetail::Add(4096u,QuestVr::DecalStateDetail::Multiply(strings,8u));
        QuestVr::NativeScratchScope reservation(nativeQueryScratchBytes_,scratch,[this](std::size_t extra){ValidateStateBudget(extra);});
        QuestVr::ScriptSavedState state;state.mapName=persistentMapPackageName;
        QuestVr::ScriptSavedObject object;object.path=actor->reflection.objectPath;object.classPath=actor->cls->reflection.objectPath;object.clock=*actor->animationClock;
        state.objects.push_back(std::move(object));
        const QuestVr::ScriptStateLimits limits;
        QuestVr::ScriptStateDetail::Writer measured(limits,nullptr);measured.State(state);
        ReserveRetailDiagnosticOutput(measured.size());
        return QuestVr::EncodeScriptSavedState(state);
    }
    PortableRetailZoneSnapshot ZoneSnapshot(RuntimeObject* actor,std::size_t commandIndex) {
        using Slot=QuestVr::OriginalActorRegionSlot;
        const bool pawn=IsDerivedFromPath(actor->cls,"Engine.Pawn");
        std::array<Value,3u> values;std::size_t bytes=actor->reflection.objectPath.size()+1u;
        for(unsigned i=0u;i<(pawn?3u:1u);++i){values[i]=ZoneStoredRegion(actor,ZoneRegionProperty(actor,static_cast<Slot>(i)));
            bytes=QuestVr::DecalStateDetail::Add(bytes,values[i].fields.at("zone").text.size()+1u);}
        ReserveRetailDiagnosticOutput(bytes+16u*sizeof(void*));
        PortableRetailZoneSnapshot result;result.commandIndex=commandIndex;result.actorPath=actor->reflection.objectPath;result.pawn=pawn;
        for(unsigned i=0u;i<(pawn?3u:1u);++i){const auto& value=values[i];result.regions[i]={value.fields.at("zone").text,
            value.fields.at("ileaf").integer,static_cast<std::uint8_t>(value.fields.at("zonenumber").integer)};}
        return result;
    }
