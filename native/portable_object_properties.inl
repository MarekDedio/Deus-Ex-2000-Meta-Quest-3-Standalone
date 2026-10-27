// Included inside PortableActorVmHost. Immutable native ConstructorLink and
// selected dynamic-payload destruction. No generic Let, clocks or provenance
// mutation. Every owning value store borrows the caller's VM write journal.
    std::size_t DestroyRetainedValue(const Value& value) {
        QuestVr::AuthoredStructLimits limits;QuestVr::AuthoredStructDetail::Budget budget{limits};
        QuestVr::AuthoredStructDetail::RetainedValue(value,budget);
        const auto arrays=[&](const auto& self,const Value& current)->void {
            budget.Retain(current.intArray.ContainerBytes());
            budget.Retain(QuestVr::DecalStateDetail::Multiply(current.intArray.capacity(),sizeof(std::int32_t)));
            for(const auto& [name,member]:current.fields){static_cast<void>(name);self(self,member);}
        };
        arrays(arrays,value);return budget.retained;
    }
    void PrepareConstructorLinks(RuntimeObject* owner,std::array<RuntimeObject*,128u>& chain,std::size_t depth=0u) {
        if(!owner)throw std::runtime_error("ConstructorLink requires an actual Struct/Class");
        if(!owner->classDescriptor&&!owner->nativeEngineClass&&!owner->nativeMetadataClass&&
            owner->reflection.metaClass!="Struct")
            throw std::runtime_error("ConstructorLink receiver is not an actual Struct/Class");
        if(owner->nativeConstructorLinksPrepared)return;
        if(depth==chain.size()||std::find(chain.begin(),chain.begin()+depth,owner)!=chain.begin()+depth)
            throw std::runtime_error("ConstructorLink superclass chain cycles or exceeds its bound");
        chain[depth]=owner;LifecycleStep();
        RuntimeObject* super{};std::int32_t child{};
        if(owner->classDescriptor) {
            if(!owner->classFieldLinksValid)throw std::runtime_error("ConstructorLink Class Children are not authenticated");
            const auto& state=owner->classDescriptor->state;child=state.children;
            super=state.baseField?Object(Qualified(Table(owner->sourcePath),state.baseField)):nullptr;
            if(super!=owner->base)throw std::runtime_error("ConstructorLink Class Super disagrees with its actual identity");
        }else {
            if(owner->nativeEngineClass||owner->nativeMetadataClass)
                throw std::runtime_error("ConstructorLink native class has no prepared field authority");
            const auto& descriptor=StructDescriptor(owner);child=descriptor.children;
            super=descriptor.baseField?Object(Qualified(Table(owner->sourcePath),descriptor.baseField)):nullptr;
        }
        if(super)PrepareConstructorLinks(super,chain,depth+1u);
        // Charge actual owning capacity and reserve peaks, not an all-fields
        // allocation envelope. This scratch remains visible to nested checks.
        const auto previous=nativeQueryScratchBytes_;
        struct Restore {std::size_t& value;std::size_t before;~Restore(){value=before;}} restore{nativeQueryScratchBytes_,previous};
        std::vector<RuntimeObject*> own;
        auto* source=owner;std::size_t visited{};
        while(child) {
            LifecycleStep();
            if(++visited>8192u)throw std::runtime_error("ConstructorLink Children cycle/count exceeds its bound");
            auto* field=Object(Qualified(Table(source->sourcePath),child));
            if(field->outer!=owner)throw std::runtime_error("ConstructorLink field belongs to another declaring Struct");
            if(field->property) {
                const auto& descriptor=*field->property;
                const auto& table=Table(field->sourcePath);
                if(descriptor.baseField||LowerAscii(Qualified(table,table.exports.at(field->exportIndex).ObjClass))!=
                    LowerAscii("Core."+descriptor.type))
                    throw std::runtime_error("ConstructorLink property lacks its actual source Class/field authority");
                if(descriptor.flags&0x00400000u) {
                    if(own.size()==own.capacity()) {
                        const auto old=own.capacity(),next=std::max<std::size_t>(8u,old*2u);
                        ValidateStateBudget(next*sizeof(RuntimeObject*));own.reserve(next);
                        nativeQueryScratchBytes_+=(own.capacity()-old)*sizeof(RuntimeObject*);
                        ValidateStateBudget();
                    }
                    own.push_back(field);
                }
                child=descriptor.nextField;
            }else if(field->script)child=field->script->nextField;
            else if(const auto* state=StateMetadata(field))child=state->nextField;
            else {
                if(!field->commonFieldLinks)
                    field->commonFieldLinks=LoadPortableFieldLinks(Table(field->sourcePath),field->exportIndex);
                child=field->commonFieldLinks->nextField;
            }
            source=field;
        }
        auto* tail=super?super->nativePropertyHeads[2u]:nullptr;
        for(auto it=own.rbegin();it!=own.rend();++it) {
            if((*it)->nativeConstructorLinksPrepared&&(*it)->nativePropertyNext[2u]!=tail)
                throw std::runtime_error("ConstructorLink shared inherited successor conflicts");
            tail=*it;
        }
        // Nonallocating publication of the complete immutable source graph.
        auto* next=super?super->nativePropertyHeads[2u]:nullptr;
        for(auto it=own.rbegin();it!=own.rend();++it) {
            (*it)->nativePropertyNext[2u]=next;(*it)->nativeConstructorLinksPrepared=true;next=*it;
        }
        owner->nativePropertyHeads[2u]=next;owner->nativeConstructorLinksPrepared=true;
    }
    RuntimeObject* ConstructorHead(RuntimeObject* owner) {
        std::array<RuntimeObject*,128u> chain{};PrepareConstructorLinks(owner,chain);
        return owner->nativePropertyHeads[2u];
    }
    void RequireDestroyProperty(RuntimeObject* field,const char* type,std::size_t registration) {
        if(!field||!field->property||field->property->type!=type||field->property->baseField||
            !(field->property->flags&0x00400000u)||!field->outer||
            registration>=QuestVr::NativeMetadataClasses.size())
            throw std::runtime_error("DestroyValue has no authenticated constructor Property");
        const auto& profile=QuestVr::NativeMetadataClasses[registration];auto* cls=Object(std::string(profile.path));
        const auto& table=Table(field->sourcePath);
        if(field->cls!=cls||cls->nativeMetadataClass!=&profile||cls->base!=Object("Core.Property")||
            cls->nativeClassWithin!=Object("Core.Field")||!cls->objectIndex||
            LowerAscii(Qualified(table,table.exports.at(field->exportIndex).ObjClass))!=LowerAscii(std::string(profile.path)))
            throw std::runtime_error("DestroyValue selection lacks its actual registered native Property Class");
    }
    struct DestroyValueAddress {
        RuntimeObject* root{};std::uint32_t slot{};
        std::array<RuntimeObject*,32u> members{};
        std::array<std::uint32_t,32u> memberSlots{};std::size_t depth{};
    };
    DestroyValueAddress DestroyElement(DestroyValueAddress address,std::uint32_t index) {
        if(address.depth) {
            auto* field=address.members[address.depth-1u];
            if(!field||!field->property||field->property->arrayDimension<1||
                index>=static_cast<std::uint32_t>(field->property->arrayDimension)||
                address.memberSlots[address.depth-1u]>
                    static_cast<std::uint32_t>(field->property->arrayDimension)-1u-index)
                throw std::runtime_error("DestroyValue nested fixed-array index is outside its actual field");
            address.memberSlots[address.depth-1u]+=index;
        }else {
            if(index>std::numeric_limits<std::uint32_t>::max()-address.slot)
                throw std::runtime_error("DestroyValue root array index overflows");
            address.slot+=index;
        }
        return address;
    }
    void DestroyPayloadStore(RuntimeObject* object,const DestroyValueAddress& address,Kind kind,
        QuestVr::Vm::Execution& execution) {
        const auto root=std::make_shared<const Property>(Describe(address.root));
        if(address.slot>=root->arrayDimension||!OwnsProperty(object->cls,*root))
            throw std::runtime_error("DestroyValue data address is outside its original root field");
        auto value=Read(object,*root,address.slot);auto* selected=&value;
        const auto referenceBytes=QuestVr::DecalStateDetail::Add(2u*DestroyRetainedValue(root->zero),
            sizeof(Reference)+sizeof(Property)+256u+root->key.capacity()+root->name.capacity());
        ValidateStateBudget(referenceBytes);
        nativeDestroyReferenceBytes_=QuestVr::DecalStateDetail::Add(nativeDestroyReferenceBytes_,referenceBytes);
        const auto localBytes=QuestVr::DecalStateDetail::Multiply(DestroyRetainedValue(value),3u);
        QuestVr::NativeScratchScope scratch(nativeQueryScratchBytes_,localBytes,
            [this](std::size_t extra){ValidateStateBudget(extra);});
        for(std::size_t i=0u;i<address.depth;++i) {
            if(selected->kind!=Kind::Struct)throw std::runtime_error("DestroyValue member data is not its original Struct");
            const auto& name=address.members[i]->reflection.objectPath;
            const auto found=selected->fields.find(LowerAscii(name.substr(name.find_last_of('.')+1u)));
            if(found==selected->fields.end())throw std::runtime_error("DestroyValue member data is absent");
            selected=&found->second;
            const auto dimension=address.members[i]->property->arrayDimension;
            if(dimension<1||address.memberSlots[i]>=static_cast<std::uint32_t>(dimension))
                throw std::runtime_error("DestroyValue member element lacks its actual dimension");
            if(dimension>1) {
                if(selected->kind!=Kind::Struct||selected->fields.size()!=static_cast<std::size_t>(dimension))
                    throw std::runtime_error("DestroyValue nested fixed-array envelope has the wrong shape");
                const auto element=selected->fields.find(std::to_string(address.memberSlots[i]));
                if(element==selected->fields.end())throw std::runtime_error("DestroyValue nested fixed-array slot is absent");
                selected=&element->second;
            }
        }
        if(selected->kind!=kind)throw std::runtime_error("DestroyValue owning payload has the wrong native type");
        *selected=kind==Kind::String?Value::Text(Kind::String,{}):Value::IntArray({});
        auto reference=std::make_shared<Reference>();reference->zero=root->zero;
        reference->read=[this,object,root,slot=address.slot]{return Read(object,*root,slot);};
        reference->write=[this,object,root,slot=address.slot](const Value& next) {
            auto typed=TypedLiveValue(*root,next);Touch(object);
            object->scriptValues[InstanceStorageKey(object->cls,*root)][slot]=std::move(typed);
            object->committedScriptState=true;
        };
        execution.WriteReference(reference,value);ValidateStateBudget();
    }
    void DestroyPropertyValue(RuntimeObject* object,RuntimeObject* field,DestroyValueAddress address,
        QuestVr::Vm::Execution& execution) {
        LifecycleStep();
        if(!field||!field->property)throw std::runtime_error("DestroyValue reached a non-Property");
        const auto& descriptor=*field->property;
        if(descriptor.type=="StrProperty") {
            RequireDestroyProperty(field,"StrProperty",9u);
            for(std::int32_t index=0;index<descriptor.arrayDimension;++index) {
                LifecycleStep();DestroyPayloadStore(object,DestroyElement(address,static_cast<std::uint32_t>(index)),Kind::String,execution);
            }
        }else if(descriptor.type=="StructProperty") {
            RequireDestroyProperty(field,"StructProperty",11u);
            if(!descriptor.referencedType)throw std::runtime_error("DestroyValue StructProperty has no actual Struct");
            auto* structure=Object(Qualified(Table(field->sourcePath),descriptor.referencedType));
            for(auto* inner=ConstructorHead(structure);inner;inner=inner->nativePropertyNext[2u]) {
                LifecycleStep();
                // Original selected Struct.DestroyValue: INNER FIELD outer,
                // ascending outer ArrayDim ELEMENT inner. Not element-first.
                for(std::int32_t index=0;index<descriptor.arrayDimension;++index) {
                    auto element=DestroyElement(address,static_cast<std::uint32_t>(index));
                    if(element.depth==element.members.size())throw std::runtime_error("DestroyValue Struct nesting exceeds its bound");
                    element.memberSlots[element.depth]=0u;element.members[element.depth++]=inner;
                    DestroyPropertyValue(object,inner,element,execution);
                }
            }
        }else if(descriptor.type=="ArrayProperty") {
            RequireDestroyProperty(field,"ArrayProperty",10u);
            auto* inner=Object(Qualified(Table(field->sourcePath),descriptor.referencedType));
            if(!inner->property||inner->outer!=field||inner->property->type!="IntProperty"||
                inner->cls!=Object("Core.IntProperty")||(inner->property->flags&0x00400000u))
                throw std::runtime_error("DestroyValue dynamic Array inner teardown is not implemented");
            // Native UArrayProperty destroys one Data/Num/Max header, not a
            // fixed outer ArrayDim loop. Int elements own no dynamic payload.
            DestroyPayloadStore(object,address,Kind::IntArray,execution);
        }else throw std::runtime_error("DestroyValue selected native Property implementation is not implemented");
    }
    void ExitObjectProperties(RuntimeObject* object,RuntimeObject* cls,QuestVr::Vm::Execution& execution) {
        if(!transaction_||!object||object->cls!=cls||Object(object->reflection.objectPath)!=object)
            throw std::runtime_error("ExitProperties lost the real UObject/Class/caller root");
        for(auto* field=ConstructorHead(cls);field;field=field->nativePropertyNext[2u]) {
            if(!field->property||!field->nativeConstructorLinksPrepared)
                throw std::runtime_error("ExitProperties reached an unprepared native ConstructorLink");
            DestroyValueAddress address;address.root=field;DestroyPropertyValue(object,field,address,execution);
        }
    }
