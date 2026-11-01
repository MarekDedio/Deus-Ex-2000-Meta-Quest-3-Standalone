// Included inside PortableActorVmHost. Both native action lookups share one
// root-owned cache of actual INPUT properties, never name/value mirrors.
    struct InputPropertyCacheEntry {
        GCRoot<RuntimeObject> cls;
        std::uint32_t index{};
        std::uint64_t generation{};
        std::vector<GCRoot<RuntimeObject>> fields;
    };
    std::vector<InputPropertyCacheEntry> inputPropertyCache_;
    RuntimeObject* lexicalCommandInput_{};
    const std::function<void()>* lexicalCommandCheck_{};

    std::size_t InputPropertyCacheBytes() const {
        using QuestVr::DecalStateDetail::Add;
        auto bytes=inputPropertyCache_.capacity()*sizeof(InputPropertyCacheEntry);
        for(const auto& entry:inputPropertyCache_) {
            bytes=Add(bytes,sizeof(GCRootNode));
            bytes=Add(bytes,entry.fields.capacity()*sizeof(GCRoot<RuntimeObject>));
            bytes=Add(bytes,entry.fields.size()*sizeof(GCRootNode));
        }
        return bytes;
    }
    void RequireOriginalCommandInput(RuntimeObject* input) {
        if(!transaction_)throw std::runtime_error("Original Input commands require their actual outer VM root");
        if(ActualNativeInputInstanceValid(input))return;
        // ONLY the real Init prefix lends this lexical authority. It neither
        // changes the public constructor authenticator nor permits publication.
        if(!input||input!=lexicalCommandInput_||!lexicalCommandCheck_)
            throw std::runtime_error("Original Input command owner has no constructor or actual Init-prefix lease");
        (*lexicalCommandCheck_)();
        const auto& state=*input->nativeInputInstance;
        if(state.initialized||state.values.viewport!=state.ownerViewport||
            std::none_of(beforeNativeInputInstances_.begin(),beforeNativeInputInstances_.end(),
                [&](const auto& entry){return entry.owner.get()==input&&entry.payload&&entry.payload.get()!=&state;}))
            throw std::runtime_error("Original Input command lost its actual COW Init-prefix authority");
    }
    RuntimeObject* InputCommandActor(RuntimeObject* input) {
        RequireOriginalCommandInput(input);
        auto* viewport=input->nativeInputInstance->values.viewport;
        if(!viewport||NativeSessionViewport(viewport->nativeViewportIdentity)!=viewport||viewport->nativeViewportInput!=input)
            throw std::runtime_error("Original Input action requires its actual native Init Viewport back-pointer");
        auto* actor=viewport->nativeViewportActor;
        if(actor) {
            RequireMapActor(actor);
            if(!actor->nativePlayerBinding||actor->nativePlayer!=viewport||actor->nativeXLevel!=CurrentClockLevel())
                throw std::runtime_error("Original Input action lost its actual Pawn/Viewport reciprocal binding");
        }
        return actor;
    }
    RuntimeObject* InputPropertyClass(RuntimeObject* cls) {
        if(!cls||!cls->classDescriptor||!cls->classFieldLinksValid||cls->reflection.metaClass!="Class"||
            !cls->objectIndex||cls->lookupControl!=persistentObjectLookup)
            throw std::runtime_error("Original Input lookup lacks actual linked Class/index authority");
        return cls;
    }
    RuntimeObject* InputLinkedField(RuntimeObject* owner,std::int32_t reference) {
        if(!reference)return nullptr;
        auto* field=Object(Qualified(Table(owner->sourcePath),reference));
        if(field->outer!=owner||(field->property&&field->linkedClassOwner!=owner))
            throw std::runtime_error("Original Input linked declaration ownership changed");
        return field;
    }
    RuntimeObject* InputNextField(RuntimeObject* field) {
        auto* owner=InputPropertyClass(field->outer);std::int32_t next{};
        if(field->property)next=field->property->nextField;
        else if(field->script)next=field->script->nextField;
        else if(const auto* metadata=StateMetadata(field))next=metadata->nextField;
        else {
            if(!field->commonFieldLinks)field->commonFieldLinks=LoadPortableFieldLinks(Table(field->sourcePath),field->exportIndex);
            next=field->commonFieldLinks->nextField;
        }
        return InputLinkedField(owner,next);
    }
    bool InputActualProperty(RuntimeObject* field) {
        const auto& table=Table(field->sourcePath);
        if(!field->cls||!field->inventoryClassReferenceValid||field->exportIndex>=table.exports.size()||
            Object(Qualified(table,table.exports[field->exportIndex].ObjClass))!=field->cls)
            throw std::runtime_error("Original Input field lacks its actual registered Class: "+field->reflection.objectPath);
        const bool property=IsDerivedFromPath(field->cls,"Core.Property");
        if(property&&!field->property)
            throw std::runtime_error("Original Input actual Property has no admitted declaration");
        return property;
    }
    template<class Visitor> void VisitInputProperties(RuntimeObject* supplied,Visitor&& visit) {
        for(auto* cls=InputPropertyClass(supplied);cls;cls=cls->base) {
            LifecycleStep();static_cast<void>(InputPropertyClass(cls));
            for(auto* field=InputLinkedField(cls,cls->classDescriptor->state.children);field;field=InputNextField(field)) {
                LifecycleStep();
                if(InputActualProperty(field)&&(field->property->flags&4u))visit(field);
            }
        }
    }
    const InputPropertyCacheEntry& InputProperties(RuntimeObject* cls) {
        static_cast<void>(InputPropertyClass(cls));
        for(const auto& entry:inputPropertyCache_) {
            LifecycleStep();
            if(entry.cls.get()==cls&&entry.index==cls->objectIndex.Index()&&entry.generation==persistentObjectLookup->generation)
                return entry;
        }
        std::size_t count{};
        VisitInputProperties(cls,[&](RuntimeObject*){count=QuestVr::DecalStateDetail::Add(count,1u);});
        // Admit the new Class root, all declaration roots and both old/new
        // vector allocations before any allocation. Clear on BOTH root exits.
        using QuestVr::DecalStateDetail::Add;using QuestVr::DecalStateDetail::Multiply;
        auto extra=Add(sizeof(InputPropertyCacheEntry)+sizeof(GCRootNode),
            Multiply(count,sizeof(GCRoot<RuntimeObject>)+sizeof(GCRootNode)));
        extra=Add(extra,Multiply(2u*(inputPropertyCache_.size()+1u),sizeof(InputPropertyCacheEntry)));
        QuestVr::NativeScratchScope building(nativeQueryScratchBytes_,extra,
            [this](std::size_t bytes){ValidateStateBudget(bytes);});
        InputPropertyCacheEntry entry{GCRoot<RuntimeObject>(cls),cls->objectIndex.Index(),persistentObjectLookup->generation,{}};
        entry.fields.reserve(count);
        if(entry.fields.capacity()!=count)
            throw std::runtime_error("Original Input cache library cannot retain its admitted declaration capacity");
        VisitInputProperties(cls,[&](RuntimeObject* field){
            if(entry.fields.size()==count)throw std::runtime_error("Original Input declaration count changed between cache passes");
            entry.fields.emplace_back(field);
        });
        if(entry.fields.size()!=count)throw std::runtime_error("Original Input declaration count changed between cache passes");
        inputPropertyCache_.reserve(inputPropertyCache_.size()+1u);
        inputPropertyCache_.push_back(std::move(entry));ValidateStateBudget();
        return inputPropertyCache_.back();
    }
    RuntimeObject* FindInputProperty(RuntimeObject* input,RuntimeObject* actor,std::u16string_view token,Kind kind) {
        RequireOriginalCommandInput(input);
        if(!input->nativeInputInstance->values.viewport||!actor)
            throw std::runtime_error("Original Input Find property requires its actual Viewport and supplied Actor");
        LifecycleStep();
        const auto name=NameString::FindCompareIndex(QuestVr::UnrealString::Encode(token)).value_or(0);
        if(!name)return nullptr; // BEFORE Actor.Class/cache, as FNAME_Find None.
        RequireMapActor(actor);const auto& entry=InputProperties(actor->cls);
        for(const auto& root:entry.fields) {
            LifecycleStep();auto* field=root.get();
            if(!InputActualProperty(field)||!(field->property->flags&4u)||!field->lookupName)
                throw std::runtime_error("Original Input cached declaration/name authority changed");
            if(field->lookupName->GetCompareIndex()!=name)continue;
            // A wrong-type same-name declaration is skipped, NOT a shadow.
            const auto* type=kind==Kind::Byte?"Core.ByteProperty":"Core.FloatProperty";
            if(!IsDerivedFromPath(field->cls,type))continue;
            const auto property=Describe(field);
            if(property.zero.kind!=kind||!property.arrayDimension||!OwnsProperty(actor->cls,property))
                throw std::runtime_error("Original Input selected property has no exact qualified Actor cell");
            return field;
        }
        return nullptr;
    }
    void ObserveInputPropertyCache(const std::string& classPath,PortableInputPropertyProbeResult& observed) {
        if(!transaction_)throw std::runtime_error("Original Input metadata query requires its actual root");
        auto* cls=Object(classPath);const auto& entry=InputProperties(cls);
        observed.properties=entry.fields.size();
        for(const auto& root:entry.fields) {
            LifecycleStep();auto* field=root.get();
            if(!InputActualProperty(field)||!(field->property->flags&4u)||!OwnsProperty(cls,Describe(field)))
                throw std::runtime_error("Actual cached Input declaration lost its qualified Class ownership");
            if(IsDerivedFromPath(field->cls,"Core.ByteProperty"))++observed.bytes;
            if(IsDerivedFromPath(field->cls,"Core.FloatProperty"))++observed.floats;
        }
        observed.completed=true;
    }
    Property InputActionCell(RuntimeObject* input,std::uintptr_t receiver,std::uintptr_t declaration,Kind kind) {
        RequireOriginalCommandInput(input);
        if(!receiver||!declaration)throw std::runtime_error("Original Input action has a partial selected cell");
        auto* actor=reinterpret_cast<RuntimeObject*>(receiver);auto* field=reinterpret_cast<RuntimeObject*>(declaration);
        RequireMapActor(actor);
        const auto& entry=InputProperties(actor->cls);
        if(std::none_of(entry.fields.begin(),entry.fields.end(),[&](const auto& root){return root.get()==field;})||
            !InputActualProperty(field)||!IsDerivedFromPath(field->cls,kind==Kind::Byte?"Core.ByteProperty":"Core.FloatProperty"))
            throw std::runtime_error("Original Input action selected a foreign/untyped declaration");
        const auto property=Describe(field);
        if(property.zero.kind!=kind||!(property.flags&4u)||!property.arrayDimension||!OwnsProperty(actor->cls,property))
            throw std::runtime_error("Original Input action selected a stale qualified cell");
        return property;
    }
    PortableInputQueryResult QueryOriginalInputActions(RuntimeObject* input,const std::string& command,
        QuestVr::Vm::Execution& execution,std::string* suppliedOutput) {
        RequireOriginalCommandInput(input);PortableInputQueryResult result;
        if(command.size()>3u*QuestVr::NativeInputKeyQueries::MaximumCommandUnits)
            throw std::runtime_error("Original Input action command exceeds admitted transport storage");
        QuestVr::NativeScratchScope capture(nativeQueryScratchBytes_,8u*(command.size()+1u)+1024u,
            [this](std::size_t bytes){ValidateStateBudget(bytes);});
        const auto units=QuestVr::UnrealString::Decode(command);
        if(units.size()>QuestVr::NativeInputKeyQueries::MaximumCommandUnits)
            throw std::runtime_error("Original Input action command exceeds admitted UTF16 storage");
        auto& output=suppliedOutput?*suppliedOutput:result.output;
        const auto previous=nativeQueryScratchBytes_;std::size_t frames{};
        struct Restore {std::size_t& ledger;std::size_t before;~Restore(){ledger=before;}} restore{nativeQueryScratchBytes_,previous};
        QuestVr::OriginalInputButtonQueries q;
        q.requireOuterRoot=[&]{RequireOriginalCommandInput(input);};
        q.work=[&]{++result.work;LifecycleStep();};
        q.enterScratch=[&](std::size_t bytes){const auto next=QuestVr::DecalStateDetail::Add(frames,bytes);
            nativeQueryScratchBytes_=previous;ValidateStateBudget(next);frames=next;nativeQueryScratchBytes_=previous+frames;};
        q.leaveScratch=[&](std::size_t bytes)noexcept{if(bytes>frames)std::terminate();frames-=bytes;nativeQueryScratchBytes_=previous+frames;};
        q.viewportActor=[&]{return reinterpret_cast<std::uintptr_t>(InputCommandActor(input));};
        q.findButton=[&](std::uintptr_t actor,std::u16string_view name){
            auto* field=FindInputProperty(input,reinterpret_cast<RuntimeObject*>(actor),name,Kind::Byte);
            return field?QuestVr::OriginalInputButtonCell{actor,reinterpret_cast<std::uintptr_t>(field)}:QuestVr::OriginalInputButtonCell{};};
        q.action=[&]{RequireOriginalCommandInput(input);return input->nativeInputInstance->action;};
        q.readByte=[&](QuestVr::OriginalInputButtonCell cell){const auto property=InputActionCell(input,cell.actor,cell.property,Kind::Byte);
            const auto value=Read(reinterpret_cast<RuntimeObject*>(cell.actor),property,0u);
            if(value.kind!=Kind::Byte||value.integer<0||value.integer>255)
                throw std::runtime_error("Original Input actual button cell is not an initialized Byte");
            return static_cast<std::uint8_t>(value.integer);};
        q.writeByte=[&](QuestVr::OriginalInputButtonCell cell,std::uint8_t value){const auto property=InputActionCell(input,cell.actor,cell.property,Kind::Byte);
            execution.WriteReference(ReferenceFor(reinterpret_cast<RuntimeObject*>(cell.actor),property,0u,false),Value::Byte(value));};
        q.log=[&](std::u16string_view text){RequireOriginalCommandInput(input);text=QuestVr::UnrealString::CString(text);
            if(text.size()>16384u||output.size()>49152u||text.size()>(49152u-output.size())/3u)
                throw std::runtime_error("Original Input action output exceeds admitted collector storage");
            ValidateStateBudget(6u*(output.size()+3u*text.size()+1u));output+=QuestVr::UnrealString::Encode(text);};
        q.axis=[&](std::u16string_view text,std::size_t cursor){
            QuestVr::OriginalInputAxisQueries axis;
            axis.requireOuterRoot=q.requireOuterRoot;axis.work=q.work;
            axis.enterScratch=q.enterScratch;axis.leaveScratch=q.leaveScratch;axis.viewportActor=q.viewportActor;
            axis.action=q.action;axis.logf=q.log;
            axis.delta=[&]{RequireOriginalCommandInput(input);return input->nativeInputInstance->delta;};
            axis.findAxis=[&](std::uintptr_t actor,std::u16string_view name){
                auto* field=FindInputProperty(input,reinterpret_cast<RuntimeObject*>(actor),name,Kind::Float);
                return field?QuestVr::OriginalInputAxisCell{actor,reinterpret_cast<std::uintptr_t>(field)}:QuestVr::OriginalInputAxisCell{};};
            axis.readFloat=[&](QuestVr::OriginalInputAxisCell cell){const auto property=InputActionCell(input,cell.actor,cell.property,Kind::Float);
                const auto value=Read(reinterpret_cast<RuntimeObject*>(cell.actor),property,0u);
                if(value.kind!=Kind::Float)throw std::runtime_error("Original Input actual axis cell is not an initialized Float");
                return value.floating;};
            axis.writeFloat=[&](QuestVr::OriginalInputAxisCell cell,float value){const auto property=InputActionCell(input,cell.actor,cell.property,Kind::Float);
                execution.WriteReference(ReferenceFor(reinterpret_cast<RuntimeObject*>(cell.actor),property,0u,false),Value::Float(value));};
            QuestVr::ExecuteOriginalInputAxisCommand(text,cursor,axis);
        };
        result.handled=QuestVr::ExecuteOriginalInputButtonCommands(units,q);RequireOriginalCommandInput(input);return result;
    }
    void ProbeOriginalInputActionCells(RuntimeObject* input,
        PortableWorldStartupProbeResult::InputResetPrefixObservation& observed,QuestVr::Vm::Execution& execution) {
        RequireOriginalCommandInput(input);auto* actor=InputCommandActor(input);
        if(!actor)throw std::runtime_error("Original Input action diagnostic requires its real spawned player");
        auto& protocol=*input->nativeInputInstance;
        auto* button=FindInputProperty(input,actor,u"bFire",Kind::Byte);
        auto* axis=FindInputProperty(input,actor,u"aForward",Kind::Float);
        if(!button||!axis)throw std::runtime_error("Original player lacks actual bFire/aForward INPUT declarations");
        const auto byte=Describe(button),floating=Describe(axis);
        if(Read(actor,byte,0u).integer||std::bit_cast<std::uint32_t>(Read(actor,floating,0u).floating))
            throw std::runtime_error("Original action diagnostic did not start after the real reset stores");
        const auto before=execution.WriteCount();
        const auto command=[&](const char* text,std::int32_t action,float delta=0.0f,const char* output="") {
            protocol.action=action;protocol.delta=delta;
            const auto result=QueryOriginalInput(input,text,execution);
            if(!result.handled||result.output!=output)
                throw std::runtime_error("Original action diagnostic lost actual handled/output semantics: "+std::string(text));
            ++observed.actionCommands;
        };
        const auto value=[&](std::uint8_t expected) {
            if(Read(actor,byte,0u).integer!=expected)
                throw std::runtime_error("Original Input command did not update the actual qualified player byte");
        };
        command("BUTTON bFire",3);value(0u);
        if(execution.WriteCount()!=before)throw std::runtime_error("Original BUTTON release redundantly wrote zero");
        command("BUTTON bFire",1);value(1u);
        command("BUTTON bFire",1);value(1u); // Original unconditional press store.
        command("PULSE bFire",3);value(1u);
        command("TOGGLE bFire",1);value(129u);
        command("TOGGLE bFire",1);value(1u);
        command("BUTTON bFire",3);value(0u);
        command("PULSE bFire",1);value(1u);
        command("BUTTON bFire",0);value(1u);
        command("BUTTON bFire",3);value(0u);
        command("BUTTON aForward",1,0.0f,"Bad Button command");
        command("BUTTON Health",1,0.0f,"Bad Button command");
        command("TOGGLE aForward",1,0.0f,"Bad Toggle command");
        command("AXIS bFire",4,1.0f,"Bad Axis command");
        command("BUTTON",1,0.0f,"Bad Button command");
        command("AXIS aForward SPEED=2",4,100.0f);
        if(Read(actor,floating,0u).floating!=2.0f)throw std::runtime_error("Original AXIS scale did not reach real player Float");
        command("AXIS aForward _SPEED=4",2,0.25f);
        if(Read(actor,floating,0u).floating!=3.0f)throw std::runtime_error("Original Hold axis did not accumulate actual Float");
        const auto noStore=execution.WriteCount();
        command("AXIS aForward SPEED=malformed",1,100.0f);
        if(execution.WriteCount()!=noStore||Read(actor,floating,0u).floating!=3.0f)
            throw std::runtime_error("Original unselected axis Action wrote its actual player field");
        // Restore the diagnostic axis through the same real command route.
        command("AXIS aForward SPEED=-3",2,1.0f);
        if(std::bit_cast<std::uint32_t>(Read(actor,floating,0u).floating))
            throw std::runtime_error("Original action diagnostic did not restore zero player axis");
        protocol.action=0;protocol.delta=0.0f;
        std::string output;const auto query=QueryOriginalInput(input,"KEYNAME 1",execution,&output);
        observed.actionKeyQuery=query.handled&&output=="LeftMouse";
        if(!observed.actionKeyQuery)throw std::runtime_error("Actual Init-prefix native key query lost supplied output/authority");
        observed.actionStores=execution.WriteCount()-before;
        observed.cachedInputProperties=InputProperties(actor->cls).fields.size();
        if(inputPropertyCache_.size()!=1u)throw std::runtime_error("Button/axis lookup did not share one actual Class property cache");
        observed.actionsCompleted=true;
    }
