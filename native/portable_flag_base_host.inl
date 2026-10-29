    Property FlagBaseField(RuntimeObject* receiver,const char* owner,const char* name,Kind kind,
        const char* type,std::uint32_t flags,std::size_t dimension=1u,const char* constraint=nullptr) {
        const auto key=std::string(owner)+'.'+name;const auto property=DescribeProperty(key);auto* field=Object(key);
        if(!receiver||!receiver->cls||!OwnsProperty(receiver->cls,property)||property.zero.kind!=kind||
            property.flags!=flags||property.arrayDimension!=dimension||!field->property||field->outer!=Object(owner)||
            field->property->type!=type||field->property->baseField||field->property->flags!=flags||
            static_cast<std::size_t>(field->property->arrayDimension)!=dimension||
            (constraint?Qualified(Table(field->sourcePath),field->property->referencedType)!=constraint:
                field->property->referencedType!=0))
            throw std::runtime_error("FlagBase field lost its original typed schema: "+key);
        return property;
    }
    void AuthenticateFlagBaseClass(RuntimeObject* cls) {
        auto* parent=Object("Extension.ExtensionObject");
        const auto valid=[&](RuntimeObject* type,RuntimeObject* base) {
            if(!type->classDescriptor||!type->defaultObject||type->defaultObject->defaultOwner!=type||
                type->defaultObject->cls!=type||type->base!=base||type->classDescriptor->classFlags!=0x112u||
                !type->classDescriptor->classWithinReference||
                Qualified(Table(type->sourcePath),*type->classDescriptor->classWithinReference)!="Core.Object"||
                !type->classDescriptor->configName||type->classDescriptor->configName->ToString()!="System"||
                !type->classDescriptor->defaults.empty()||!type->classDefaultValues.empty()||
                type->classDescriptor->state.logicalSize||!type->classDescriptor->stateBytecode.empty())
                throw std::runtime_error("FlagBase default-copy ancestry differs from original native zero-extension profile");
        };
        if(cls!=Object("Extension.FlagBase"))throw std::runtime_error("FlagBase constructor requires its actual exact Class");
        valid(parent,Object("Core.Object"));valid(cls,parent);
        static_cast<void>(FlagBaseField(cls->defaultObject,"Extension.FlagBase","hashTable",Kind::Object,"ObjectProperty",0x10000u,64u,"Extension.Flag"));
        static_cast<void>(FlagBaseField(cls->defaultObject,"Extension.FlagBase","defaultFlagExpiration",Kind::Int,"IntProperty",0x10000u));
        if(cls->classDescriptor->state.children!=static_cast<std::int32_t>(Object("Extension.FlagBase.DeleteAllFlags")->exportIndex+1u))
            throw std::runtime_error("FlagBase changed original class child-chain identity");
    }
    void AuthenticatePlayerFlagBaseDefault() {
        // Engine103B7F74 registers PlayerPawn native size890. Extension10037B45
        // registers PlayerPawnExt99C; its FlagBase pointer at890 is therefore
        // wholly inside Core InitProperties' zero-extended native tail.
        constexpr std::size_t superBytes=0x890u,nativeBytes=0x99cu,fieldOffset=0x890u;
        static_assert(fieldOffset>=superBytes&&fieldOffset+4u<=nativeBytes);
        auto* cls=Object("Extension.PlayerPawnExt");
        if(!cls->classDescriptor||cls->base!=Object("Engine.PlayerPawn")||cls->classDescriptor->classFlags!=0x136u||
            !cls->defaultObject||cls->defaultObject->defaultOwner!=cls||cls->defaultObject->cls!=cls||
            !cls->classDescriptor->defaults.empty()||cls->classDescriptor->state.logicalSize||
            !cls->classDescriptor->stateBytecode.empty()||!cls->classDescriptor->classWithinReference||
            Qualified(Table(cls->sourcePath),*cls->classDescriptor->classWithinReference)!="Core.Object")
            throw std::runtime_error("PlayerPawnExt.FlagBase lost native zero-tail default-copy authority");
        static_cast<void>(FlagBaseField(cls->defaultObject,"Extension.PlayerPawnExt","FlagBase",Kind::Object,"ObjectProperty",0x10002u,1u,"Extension.FlagBase"));
    }
    void RequireFlagBase(RuntimeObject* object) {
        if(!object||!object->nativeFlagBase||!object->nativeFlagBase->defaultCopyVerified||
            !object->nativeFlagBase->constructorCompleted||object->defaultOwner||object->runtimeBirth||
            object->nativeWindowObject||object->cls!=Object("Extension.FlagBase")||!object->outer||
            !object->lookupName||!object->objectIndex||object->reflection.flags||
            object->worldActorIndex!=-1||object->exportIndex!=std::numeric_limits<std::size_t>::max()||
            Object(object->reflection.objectPath)!=object)
            throw std::runtime_error("FlagBase query requires an actual constructed native UObject");
    }
    PortableFlagBaseSnapshot FlagBaseSnapshot(RuntimeObject* object) {
        RequireFlagBase(object);const auto& state=*object->nativeFlagBase;
        return {object->reflection.objectPath,object->cls->reflection.objectPath,object->outer->reflection.objectPath,
            object->objectIndex.Index(),object->reflection.flags,state.defaultExpiration,
            static_cast<std::size_t>(std::count_if(state.buckets.begin(),state.buckets.end(),[](auto* p){return p!=nullptr;})),
            state.constructorCompleted,state.defaultCopyVerified};
    }
    RuntimeObject* ConstructFlagBase(RuntimeObject* outer) {
        if(!transaction_||!outer||!outer->cls||outer->defaultOwner||outer->classDescriptor||
            Object(outer->reflection.objectPath)!=outer||persistentFlagBases.size()>=4096u)
            throw std::runtime_error("FlagBase construction requires a bounded actual UObject Outer and active root");
        auto* cls=Object("Extension.FlagBase");auto* defaults=cls->defaultObject;
        nativeWork_.Step(64u);
        if(!defaults||!defaults->nativeFlagBase||!defaults->nativeFlagBase->defaultCopyVerified||
            defaults->nativeFlagBase->constructorCompleted||defaults->nativeFlagBase->defaultExpiration||
            std::any_of(defaults->nativeFlagBase->buckets.begin(),defaults->nativeFlagBase->buckets.end(),[](auto* p){return p!=nullptr;}))
            throw std::runtime_error("FlagBase constructor requires its explicitly prepared native Class/default copy");
        QuestVr::NativeScratchScope frame(nativeQueryScratchBytes_,16384u+16u*(outer->reflection.objectPath.size()+1u),
            [this](std::size_t bytes){ValidateStateBudget(bytes);});
        Touch(cls);const auto name=QuestVr::ObjectNames::MakeUnique("FlagBase",cls->uniqueObjectCounter,[&](std::string_view candidate){
            LifecycleStep();return persistentVmObjects.count(LowerAscii(outer->reflection.objectPath+'.'+std::string(candidate)))!=0u;});
        const auto path=outer->reflection.objectPath+'.'+name;
        EnsureConversationIndexTransaction();conversationIndices_->PrepareNextEntryCapacity();
        const auto capacity=std::max<std::size_t>(1u,persistentFlagBases.size()+1u);
        ValidateStateBudget(sizeof(RuntimeObject)+sizeof(GCAllocation)+sizeof(RuntimeNativeFlagBaseState)+
            16u*(path.size()+cls->sourcePath.size()+1u)+capacity*(sizeof(RuntimeObject*)+sizeof(GCRoot<RuntimeObject>))+
            sizeof(GCRootNode),cls);
        persistentFlagBases.reserve(capacity);provisionalFlagBases_.reserve(provisionalFlagBases_.size()+1u);
        pendingBirthGarbage=true;
        provisionalFlagBases_.emplace_back(GC::Alloc<RuntimeObject>(PortableReflectionObject{
            path,"FlagBase",outer->reflection.objectPath,{},0u,0},nullptr));
        auto* object=provisionalFlagBases_.back().get();object->cls=cls;object->outer=outer;
        object->sourcePath=cls->sourcePath;object->exportIndex=std::numeric_limits<std::size_t>::max();
        object->birthVmKey=LowerAscii(path);object->lookupName=NameString(name);
        object->nativeLinkerBound=true;
        // Actual prepared CDO native payload, not zero guessed from a missing
        // serialized property. Constructor only rewrites expiration at+128.
        object->nativeFlagBase=std::make_unique<RuntimeNativeFlagBaseState>(*defaults->nativeFlagBase);
        object->nativeFlagBase->defaultExpiration=0;object->nativeFlagBase->constructorCompleted=true;
        persistentFlagBases.push_back(object);
        if(!persistentQualifiedObjects.emplace(path,object).second||!persistentVmObjects.emplace(object->birthVmKey,object).second)
            throw std::runtime_error("FlagBase identity collided with an actual UObject");
        PublishTransactionalLookupObject(object);ValidateStateBudget();return object;
    }
    void ConstructPlayerFlagBase(RuntimeObject* pawn) {
        if(!transaction_||!pawn||!pawn->runtimeBirth||!IsDerivedFromPath(pawn->cls,"Extension.PlayerPawnExt")||pawn->nativePlayerFlagBaseBound)
            throw std::runtime_error("PlayerPawnExt native constructor lacks its fresh actual Actor/default-copy authority");
        AuthenticatePlayerFlagBaseDefault();
        const auto field=FlagBaseField(pawn,"Extension.PlayerPawnExt","FlagBase",Kind::Object,"ObjectProperty",0x10002u,1u,"Extension.FlagBase");
        // Original constructor preserves a nonnull copied field. Never silently
        // replace a custom class default/template pointer with a new object.
        const auto copied=Read(pawn,field,0u,true);
        RuntimeObject* selected=copied.text.empty()?nullptr:Object(copied.text);
        if(selected)RequireFlagBase(selected);else selected=ConstructFlagBase(pawn);
        Touch(pawn);pawn->nativePlayerFlagBase=selected;pawn->nativePlayerFlagBaseBound=true;
        if(worldStartupResult_) {
            using Row=PortableWorldStartupProbeResult::FlagBaseObservation;
            auto& rows=worldStartupResult_->flagBases;
            if(rows.size()>=4096u)throw std::runtime_error("FlagBase constructor observations exceed root policy");
            if(rows.size()==rows.capacity()) {
                const auto old=rows.capacity(),requested=std::max<std::size_t>(1u,old*2u);
                ValidateStateBudget(requested*sizeof(Row));rows.reserve(requested);
                nativeActorOrderObservationBytes_+=(rows.capacity()-old)*sizeof(Row);
            }
            ValidateStateBudget(4u*(selected->reflection.objectPath.size()+pawn->reflection.objectPath.size()+2u));
            rows.push_back({selected->reflection.objectPath,pawn->reflection.objectPath,
                selected->objectIndex.Index(),selected->reflection.flags,selected->nativeFlagBase->defaultCopyVerified,
                selected->nativeFlagBase->constructorCompleted});
            nativeActorOrderObservationBytes_+=rows.back().path.capacity()+rows.back().pawnPath.capacity()+2u;
        }
    }
    std::optional<Value> ReadNativeFlagBaseValue(RuntimeObject* object,const Property& property,std::uint32_t index,bool defaults,bool immutable) {
        const auto key=LowerAscii(property.key);
        if(!defaults&&!object->defaultOwner&&key=="extension.playerpawnext.flagbase") {
            static_cast<void>(FlagBaseField(object,"Extension.PlayerPawnExt","FlagBase",Kind::Object,"ObjectProperty",0x10002u,1u,"Extension.FlagBase"));
            if(index||immutable||!object->nativePlayerFlagBaseBound)
                throw std::runtime_error("PlayerPawnExt.FlagBase requires its original constructor-owned native service");
            if(object->nativePlayerFlagBase)RequireFlagBase(object->nativePlayerFlagBase);
            return Value::Text(Kind::Object,object->nativePlayerFlagBase?object->nativePlayerFlagBase->reflection.objectPath:std::string{});
        }
        if(!object->nativeFlagBase)return {};
        if(object->defaultOwner) {
            if(object->cls!=object->defaultOwner||object->cls->defaultObject!=object||!object->nativeFlagBase->defaultCopyVerified)
                throw std::runtime_error("FlagBase CDO lost actual class ownership");
        }else {if(defaults||immutable)throw std::runtime_error("FlagBase immutable/authored read needs its actual native archive");RequireFlagBase(object);}
        if(key.rfind("core.object.",0u)==0u&&!object->defaultOwner) {
            if(index||property.arrayDimension!=1u||!OwnsProperty(object->cls,property))throw std::runtime_error("FlagBase header read is not an actual scalar");
            if(key=="core.object.class")return Value::Text(Kind::Object,object->cls->reflection.objectPath);
            if(key=="core.object.name")return Value::Text(Kind::Name,object->lookupName->ToString());
            if(key=="core.object.outer")return Value::Text(Kind::Object,object->outer->reflection.objectPath);
            if(key=="core.object.objectflags")return Value::Integer(0);
            throw std::runtime_error("FlagBase raw ObjectInternal is not script-visible");
        }
        if(key=="extension.flagbase.hashtable") {
            static_cast<void>(FlagBaseField(object,"Extension.FlagBase","hashTable",Kind::Object,"ObjectProperty",0x10000u,64u,"Extension.Flag"));
            if(index>=64u)throw std::runtime_error("FlagBase bucket index exceeds actual array dimension");
            const auto* head=object->nativeFlagBase->buckets[index];
            return Value::Text(Kind::Object,head?head->reflection.objectPath:std::string{});
        }
        if(key=="extension.flagbase.defaultflagexpiration") {
            static_cast<void>(FlagBaseField(object,"Extension.FlagBase","defaultFlagExpiration",Kind::Int,"IntProperty",0x10000u));
            if(index)throw std::runtime_error("FlagBase default expiration is scalar");
            return Value::Integer(object->nativeFlagBase->defaultExpiration);
        }
        return {};
    }
    Evaluation OriginalCheckFlagNative(RuntimeObject* receiver,const std::vector<Evaluation>& arguments,
        const Function* supplied,QuestVr::Vm::Execution&) {
        RequireFlagBase(receiver);if(!transaction_)throw std::runtime_error("CheckFlag requires its same original root");
        const std::string path="Extension.FlagBase.CheckFlag";auto* method=Object(path);const auto authored=FunctionFor(method);
        const auto validate=[&](const Function& f) {
            if(f.path!=path||f.source!=method->sourcePath||f.nativeIndex!=1120u||f.flags!=0x401u||f.variables.size()!=3u||
                !method->script||method->outer!=Object("Extension.FlagBase")||method->script->baseField||
                method->script->logicalSize!=10u||method->script->bytecode.size()!=10u||f.bytecode!=method->script->bytecode)
                throw std::runtime_error("CheckFlag differs from its original native1120 packet");
            RuntimeBytecodeReader code(f.bytecode);
            for(std::size_t slot=0u;slot<3u;++slot) {
                const auto& p=f.variables[slot];const auto name=slot==0u?"FlagName":slot==1u?"flagType":"ReturnValue";
                const auto kind=slot==0u?Kind::Name:slot==1u?Kind::Byte:Kind::Bool;
                auto* field=Object(p.key);
                if(p.key!=path+'.'+name||p.flags!=(slot==2u?0x580u:0x80u)||p.zero.kind!=kind||p.arrayDimension!=1u||
                    !field->property||field->outer!=method||field->property->baseField||field->property->flags!=p.flags||
                    field->property->type!=(slot==0u?"NameProperty":slot==1u?"ByteProperty":"BoolProperty")||
                    (slot==1u&&Qualified(Table(field->sourcePath),field->property->referencedType)!="Extension.ExtensionObject.EFlagType")||
                    (slot<2u&&(code.Byte()!=0x29u||Qualified(Table(method->sourcePath),static_cast<std::int32_t>(code.Dword()))!=p.key)))
                    throw std::runtime_error("CheckFlag native parameter/result shape changed");
            }
        };
        validate(*authored);if(supplied)validate(*supplied);
        if(arguments.size()!=2u||arguments[0].reference||arguments[1].reference||
            arguments[0].disposition!=QuestVr::Vm::ResultDisposition::Written||arguments[1].disposition!=QuestVr::Vm::ResultDisposition::Written||
            arguments[0].value.kind!=Kind::Name||arguments[1].value.kind!=Kind::Byte)
            throw std::runtime_error("CheckFlag requires its two copied Name/Byte inputs");
        const auto& text=arguments[0].value.text;
        QuestVr::NativeScratchScope scratch(nativeQueryScratchBytes_,8192u+16u*(text.size()+1u),
            [this](std::size_t bytes){ValidateStateBudget(bytes);});
        nativeWork_.Step(text.size()+1u);
        const auto found=NameString::FindCompareIndex(text);
        if(!found)throw std::runtime_error("CheckFlag Name has no actual FName identity");
        const auto before=nativeQueryScratchBytes_;
        struct Restore {std::size_t& ledger;std::size_t before;~Restore(){ledger=before;}} restore{nativeQueryScratchBytes_,before};
        NameStringArenaStage nameStage({[this](std::size_t n){nativeWork_.Step(n);},
            [this,before](std::size_t n){nativeQueryScratchBytes_=before;ValidateStateBudget(n);nativeQueryScratchBytes_=QuestVr::DecalStateDetail::Add(before,n);}},
            2u,NameStringArenaStage::Mode::IncrementalOriginalName);
        const auto name=nameStage.AddOriginalName(text);const auto canonical=QuestVr::UnrealString::Decode(nameStage.Spelling(name));
        if(name.GetCompareIndex()!=*found)throw std::runtime_error("CheckFlag changed actual name identity");
        const auto result=QuestVr::FlagBase::Find(*found,canonical,static_cast<std::uint8_t>(arguments[1].value.integer),{
            [this](std::size_t n){nativeWork_.Step(n);},
            [receiver](std::size_t bucket){return reinterpret_cast<std::uintptr_t>(receiver->nativeFlagBase->buckets.at(bucket));},
            [](std::uintptr_t)->QuestVr::FlagBase::Node {
                // No Flag node constructor is admitted yet. A nonempty graph
                // is an explicit missing service, never an assumed lookup miss.
                throw std::runtime_error("CheckFlag nonempty node requires original typed Flag/FlagInt construction");
            }});
        return {Value::Bool(result.node!=0u),{}};
    }
    std::shared_ptr<const Function> FlagBaseCheckDeclaration() {return FunctionFor(Object("Extension.FlagBase.CheckFlag"));}
