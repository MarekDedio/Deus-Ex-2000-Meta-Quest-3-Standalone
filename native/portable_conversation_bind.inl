// Included in PortableActorVmHost: all providers borrow this host's original
// VM execution, work ledger and outer transaction. No private child commit.
    static bool IsNativeConversationNode(const RuntimeObject* object) noexcept {
        return object && object->nativeConversation && object->nativeConversation->node;
    }
    RuntimeNativeConversationState& ConversationState(RuntimeObject* object) {
        Touch(object);
        if(!object->nativeConversation) {
            ValidateStateBudget(sizeof(RuntimeNativeConversationState));
            object->nativeConversation=std::make_unique<RuntimeNativeConversationState>();
        }
        return *object->nativeConversation;
    }
    Property ConversationField(RuntimeObject* object,const char* key,Kind kind,
        const char* type,const char* constraint,std::uint32_t flags) {
        const auto field=DescribeProperty(key);
        auto* metadata=Object(field.key);
        if(!object || !object->cls || field.zero.kind!=kind || field.arrayDimension!=1u || field.flags!=flags ||
            !OwnsProperty(object->cls,field) || !metadata->property || metadata->property->type!=type ||
            metadata->property->baseField || LowerAscii(metadata->reflection.objectPath)!=LowerAscii(key))
            throw std::runtime_error(std::string("Conversation native field lacks its exact original declaration: ")+key);
        if(constraint && LowerAscii(Qualified(Table(metadata->sourcePath),metadata->property->referencedType))!=LowerAscii(constraint))
            throw std::runtime_error("Conversation native field lacks its original object constraint");
        return field;
    }
    void RequireConversationNode(RuntimeObject* object,const bool allowRetired=false) {
        if(!IsNativeConversationNode(object) || (!allowRetired && object->nativeConversation->retired) ||
            object->runtimeBirth || object->nativeWindowObject || object->classDescriptor ||
            object->cls!=Object("ConSys.ConListItem") || !object->outer || !object->lookupName ||
            object->exportIndex!=std::numeric_limits<std::size_t>::max())
            throw std::runtime_error("Conversation link is not a live native ConListItem");
    }
    RuntimeObject* ConversationPointer(const Value& value) {
        if(value.kind!=Kind::Object) throw std::runtime_error("Conversation link is not an Object value");
        if(value.text.empty()) return nullptr;
        const auto live=persistentVmObjects.find(LowerAscii(value.text));
        if(live!=persistentVmObjects.end()) return Object(value.text);
        // Machine's ordinary write undo runs before Host::Rollback. These are
        // authenticated stable before-images, not discoverable dead UObjects.
        for(const auto& entry:retiredConversations_)
            if(entry.object.get()->reflection.objectPath==value.text) return entry.object.get();
        throw std::runtime_error("Conversation link refers to an unavailable UObject");
    }
    std::optional<Value> ReadNativeConversationValue(RuntimeObject* object,const Property& property,
        std::uint32_t index,bool defaults,bool immutable) {
        if(!object->nativeConversation || defaults || object->classDescriptor || object->defaultOwner) return {};
        const auto key=LowerAscii(property.key);
        if(IsNativeConversationNode(object)) {
            RequireConversationNode(object,true);
            if(index || property.arrayDimension!=1u || !OwnsProperty(object->cls,property))
                throw std::runtime_error("Conversation native read lacks its scalar declaring schema");
            if(key=="core.object.class") return Value::Text(Kind::Object,object->cls->reflection.objectPath);
            if(key=="core.object.name") return Value::Text(Kind::Name,object->lookupName->ToString());
            if(key=="core.object.outer") return Value::Text(Kind::Object,object->outer->reflection.objectPath);
            if(key=="core.object.objectflags") return Value::Integer(std::bit_cast<std::int32_t>(object->reflection.flags));
            if(key=="core.object.objectinternal") throw std::runtime_error("Native ConListItem pointer storage is not script-visible");
            if(key=="consys.conlistitem.con" || key=="consys.conlistitem.next") {
                const bool con=key=="consys.conlistitem.con";
                static_cast<void>(ConversationField(object,con?"ConSys.ConListItem.con":"ConSys.ConListItem.Next",
                    Kind::Object,"ObjectProperty",con?"ConSys.Conversation":"ConSys.ConListItem",0u));
                auto* value=con?object->nativeConversation->con:object->nativeConversation->next;
                return Value::Text(Kind::Object,value?value->reflection.objectPath:std::string{});
            }
        }
        if(immutable || !object->nativeConversation) return {};
        if(key=="engine.actor.conlistitems" && object->nativeConversation->headWritten) {
            if(index) throw std::runtime_error("Conversation Actor head is not a scalar");
            static_cast<void>(ConversationField(object,"Engine.Actor.ConListItems",Kind::Object,"ObjectProperty","Core.Object",0x2000u));
            auto* value=object->nativeConversation->head;
            return Value::Text(Kind::Object,value?value->reflection.objectPath:std::string{});
        }
        if(key=="consys.conversation.ownerrefcount" && object->nativeConversation->ownerRefCount) {
            if(index) throw std::runtime_error("Conversation reference count is not a scalar");
            static_cast<void>(ConversationField(object,"ConSys.Conversation.ownerRefCount",Kind::Int,"IntProperty",nullptr,0u));
            return Value::Integer(*object->nativeConversation->ownerRefCount);
        }
        return {};
    }
    bool WriteNativeConversationValue(RuntimeObject* object,const Property& property,
        std::uint32_t index,bool defaults,const Value& value) {
        if(defaults || object->classDescriptor || object->defaultOwner) return false;
        if(!IsNativeConversationNode(object) &&
            !SameInventoryIdentity(property.key,"Engine.Actor.ConListItems") &&
            !SameInventoryIdentity(property.key,"ConSys.Conversation.ownerRefCount") &&
            !SameInventoryIdentity(property.key,"ConSys.ConListItem.con") &&
            !SameInventoryIdentity(property.key,"ConSys.ConListItem.Next")) return false;
        const auto key=LowerAscii(property.key);
        if(key=="engine.actor.conlistitems" || key=="consys.conlistitem.con" || key=="consys.conlistitem.next") {
            const bool head=key=="engine.actor.conlistitems",con=key=="consys.conlistitem.con";
            static_cast<void>(ConversationField(object,head?"Engine.Actor.ConListItems":con?"ConSys.ConListItem.con":"ConSys.ConListItem.Next",
                Kind::Object,"ObjectProperty",head?"Core.Object":con?"ConSys.Conversation":"ConSys.ConListItem",head?0x2000u:0u));
            if(index) throw std::runtime_error("Native conversation link write is not scalar");
            if(!head) RequireConversationNode(object,true);
            auto* pointer=ConversationPointer(QuestVr::Vm::Coerce(value,property.zero));
            if(pointer && !ClassDerives(pointer->cls->reflection.objectPath,
                    head?"Core.Object":con?"ConSys.Conversation":"ConSys.ConListItem"))
                throw std::runtime_error("Native conversation pointer violates its original subtype");
            auto& state=ConversationState(object);
            if(head) {state.head=pointer;state.headWritten=true;}
            else if(con) state.con=pointer;
            else state.next=pointer;
            return true;
        }
        if(key=="consys.conversation.ownerrefcount") {
            static_cast<void>(ConversationField(object,"ConSys.Conversation.ownerRefCount",Kind::Int,"IntProperty",nullptr,0u));
            if(index) throw std::runtime_error("Native conversation count write is not scalar");
            ConversationState(object).ownerRefCount=QuestVr::Vm::Coerce(value,property.zero).integer;
            return true;
        }
        if(IsNativeConversationNode(object) && key.rfind("core.object.",0u)==0u) {
            if(key!="core.object.objectflags" || index || property.arrayDimension!=1u || property.zero.kind!=Kind::Int)
                throw std::runtime_error("Native ConListItem Class/Name/Outer identity is immutable");
            Touch(object);object->reflection.flags=std::bit_cast<std::uint32_t>(QuestVr::Vm::Coerce(value,property.zero).integer);
            return true;
        }
        return false;
    }
    void EnsureConversationIndexTransaction() {
        if(conversationIndices_) return;
        // Finite root operation policy, independent of original list matching.
        // All subsequent pool acquisitions (including nested Actor Spawn) join
        // this journal; no deleted index remains reserved by a GC tombstone.
        constexpr std::size_t entries=4096u;
        conversationIndices_.emplace(RuntimeObjectIndices().BeginPagedTransaction(entries,{this,
            [](void* owner,std::size_t bytes){static_cast<PortableActorVmHost*>(owner)->ValidateStateBudget(bytes);}}));
    }
    RuntimeObject* ConstructConversationNode(RuntimeObject* outer,std::uint32_t flags) {
        if(!transaction_ || !outer || flags || persistentConversationNodes.size()>=65536u)
            throw std::runtime_error("Native ConListItem construction requires original Outer/flags and bounded root ownership");
        auto* cls=Object("ConSys.ConListItem");
        if(!cls->classDescriptor || cls->classDescriptor->classFlags!=0x112u || !cls->classDescriptor->classWithinReference ||
            Object(Qualified(Table(cls->sourcePath),*cls->classDescriptor->classWithinReference))!=Object("Core.Object") ||
            !IsDerivedFromPath(outer->cls,"Core.Package"))
            throw std::runtime_error("ConListItem construction class/Within or actual package Outer is invalid");
        const auto className=CastClassIdentity(cls->reflection.objectPath).name;
        Touch(cls);
        const auto name=QuestVr::ObjectNames::MakeUnique(className,cls->uniqueObjectCounter,[&](std::string_view candidate) {
            LifecycleStep();return persistentVmObjects.count(LowerAscii(outer->reflection.objectPath+'.'+std::string(candidate)))!=0u;
        });
        const auto path=outer->reflection.objectPath+'.'+name;
        EnsureConversationIndexTransaction();
        conversationIndices_->PrepareNextEntryCapacity();
        // The journal is already retained here, so this admission proves the
        // combined journal + new object/default-copy peak, not separate peaks.
        ValidateStateBudget(sizeof(RuntimeObject)+sizeof(GCAllocation)+sizeof(RuntimeNativeConversationState)+
            2u*(sizeof(std::string)+6u*sizeof(void*)+path.size()+1u),cls);
        pendingBirthGarbage=true;
        provisionalConversations_.emplace_back(GC::Alloc<RuntimeObject>(PortableReflectionObject{
            path,className,outer->reflection.objectPath,{},0u,0},nullptr));
        auto* object=provisionalConversations_.back().get();
        object->nativeConversation=std::make_unique<RuntimeNativeConversationState>();
        object->nativeConversation->node=true; // Constructor con/Next are zero.
        object->cls=cls;object->outer=outer;object->lookupName=NameString(name);
        object->sourcePath=cls->sourcePath;object->exportIndex=std::numeric_limits<std::size_t>::max();
        object->birthVmKey=LowerAscii(path);object->frozenBirthDefaults=cls->classDefaultValues;
        persistentConversationNodes.push_back(object); // Weak rollback inventory, before aliases can throw.
        if(!persistentQualifiedObjects.emplace(path,object).second || !persistentVmObjects.emplace(object->birthVmKey,object).second)
            throw std::runtime_error("New ConListItem identity collides with a live UObject");
        PublishTransactionalLookupObject(object);
        return object;
    }
    void DestroyConversationNode(RuntimeObject* object) {
        RequireConversationNode(object);
        if(!transaction_) throw std::runtime_error("ConListItem deleting destructor requires an outer root");
        EnsureConversationIndexTransaction();Touch(object);
        // Allocate all undo/root storage before removing a single publication.
        retiredConversations_.emplace_back();auto& before=retiredConversations_.back();
        before.object.set(object);before.control=object->lookupControl;
        before.previous=object->lookupPrevious;before.next=object->lookupNext;
        before.serial=object->lookupPublicationSerial;before.publications=provisionalLookupObjects_.size();
        if(before.control) {
            before.lookup=before.control->index.Extract(object->objectIndex.Index(),reinterpret_cast<std::uintptr_t>(object));
            if(!before.lookup) throw std::runtime_error("Deleting ConListItem has no actual weak-index publication");
        }
        before.canonical=persistentQualifiedObjects.extract(object->reflection.objectPath);
        before.alias=persistentVmObjects.extract(object->birthVmKey);
        if(before.canonical.empty() || before.alias.empty() || before.canonical.mapped()!=object || before.alias.mapped()!=object)
            throw std::runtime_error("Deleting ConListItem lost its canonical aliases");
        RetireRuntimeLookupObject(object);
        before.unpublished=true;
        conversationIndices_->Retire(object->objectIndex); // Immediately reusable native slot, not GC-delayed release.
        object->nativeConversation->retired=true;
        object->nativeConversation->head=object->nativeConversation->con=object->nativeConversation->next=nullptr;
        pendingBirthGarbage=true;
    }
    void RollbackConversationLookup() noexcept {
        auto publications=provisionalLookupObjects_.size();
        const auto unpublish=[&](RuntimeObject* object) {
            RetireRuntimeLookupObject(object);
            // Remove every later provisional alias, not only conversation
            // nodes: restoring old aliases must never exceed the map's prior
            // peak size and trigger an allocating rehash during rollback.
            const auto canonical=persistentQualifiedObjects.find(object->reflection.objectPath);
            if(canonical!=persistentQualifiedObjects.end() && canonical->second==object) persistentQualifiedObjects.erase(canonical);
            const auto alias=persistentVmObjects.find(object->birthVmKey);
            if(alias!=persistentVmObjects.end() && alias->second==object) persistentVmObjects.erase(alias);
        };
        for(auto entry=retiredConversations_.rbegin();entry!=retiredConversations_.rend();++entry) {
            while(publications>entry->publications) unpublish(provisionalLookupObjects_[--publications]);
            auto* object=entry->object.get();
            if(!entry->canonical.empty() && !persistentQualifiedObjects.insert(std::move(entry->canonical)).inserted) std::terminate();
            if(!entry->alias.empty() && !persistentVmObjects.insert(std::move(entry->alias)).inserted) std::terminate();
            if(entry->lookup && (!entry->control || !entry->control->index.Restore(entry->lookup))) std::terminate();
            if(entry->unpublished) {
                object->lookupControl=entry->control;
                object->lookupPublicationSerial=entry->serial;object->lookupPrevious=entry->previous;object->lookupNext=entry->next;
                if(entry->serial) {
                    if(entry->previous) entry->previous->lookupNext=object;else persistentPublishedHead=object;
                    if(entry->next) entry->next->lookupPrevious=object;else persistentPublishedTail=object;
                }
            }
        }
        while(publications) unpublish(provisionalLookupObjects_[--publications]);
        provisionalLookupObjects_.clear();
    }
    void RollbackConversationObjects() noexcept {
        for(auto& root:provisionalConversations_) {
            auto* object=root.get();
            // Even allocation failures before weak publication entered the
            // common publication journal must remove their partial aliases.
            const auto canonical=persistentQualifiedObjects.find(object->reflection.objectPath);
            if(canonical!=persistentQualifiedObjects.end() && canonical->second==object) persistentQualifiedObjects.erase(canonical);
            const auto alias=persistentVmObjects.find(object->birthVmKey);
            if(alias!=persistentVmObjects.end() && alias->second==object) persistentVmObjects.erase(alias);
        }
        if(conversationIndices_) {conversationIndices_->Rollback();conversationIndices_.reset();}
        persistentConversationNodes.resize(beforeConversationNodes_);
        retiredConversations_.clear();provisionalConversations_.clear();
    }
    void CommitConversationObjects() noexcept {
        if(conversationIndices_) {conversationIndices_->Commit();conversationIndices_.reset();}
        persistentConversationNodes.erase(std::remove_if(persistentConversationNodes.begin(),persistentConversationNodes.end(),
            [](const auto* object){return object->nativeConversation->retired;}),persistentConversationNodes.end());
        retiredConversations_.clear();provisionalConversations_.clear();
    }
    void BindConversationList(RuntimeObject* list,RuntimeObject* actor,QuestVr::Vm::Execution& execution) {
        using Handle=std::uintptr_t;
        const auto pointer=[](Handle value){return reinterpret_cast<RuntimeObject*>(value);};
        const auto handle=[](RuntimeObject* value){return reinterpret_cast<Handle>(value);};
        const auto read=[&](Handle value,const char* key,Kind kind,const char* type,const char* constraint,std::uint32_t flags) {
            auto* object=pointer(value);const auto field=ConversationField(object,key,kind,type,constraint,flags);
            return Read(object,field,0u);
        };
        const auto link=[&](Handle value,const char* key,const char* constraint,std::uint32_t flags=0u) {
            const auto result=read(value,key,Kind::Object,"ObjectProperty",constraint,flags);
            return result.text.empty()?Handle{}:handle(Object(result.text));
        };
        const auto write=[&](Handle target,const char* key,const Value& value) {
            auto* object=pointer(target);execution.WriteReference(ReferenceFor(object,DescribeProperty(key),0u,false),value);
        };
        QuestVr::OriginalConversationBindQueries query;
        query.sourceHead=[&](Handle value){return link(value,"ConSys.ConversationList.conversations","ConSys.ConItem");};
        query.sourceNext=[&](Handle value){return link(value,"ConSys.ConItem.Next","ConSys.ConItem");};
        query.sourceConversation=[&](Handle value) {
            const auto result=link(value,"ConSys.ConItem.ConObject","ConSys.ConObject");
            if(result && !IsDerivedFromPath(pointer(result)->cls,"ConSys.Conversation"))
                throw std::runtime_error("Conversation source ConObject is not an actual Conversation");
            return result;
        };
        query.actorHead=[&](Handle value){return link(value,"Engine.Actor.ConListItems","Core.Object",0x2000u);};
        query.nodeNext=[&](Handle value){RequireConversationNode(pointer(value));return link(value,"ConSys.ConListItem.Next","ConSys.ConListItem");};
        query.nodeConversation=[&](Handle value){RequireConversationNode(pointer(value));return link(value,"ConSys.ConListItem.con","ConSys.Conversation");};
        query.conversationName=[&](Handle value){return read(value,"ConSys.Conversation.conName",Kind::Name,"NameProperty",nullptr,0u).text;};
        query.ownerName=[&](Handle value){return read(value,"ConSys.Conversation.conOwnerName",Kind::String,"StrProperty",nullptr,0x400000u).text;};
        query.bindName=[&](Handle value){return read(value,"Engine.Actor.BindName",Kind::String,"StrProperty",nullptr,0x400001u).text;};
        query.barkName=[&](Handle value){return read(value,"Engine.Actor.BarkBindName",Kind::String,"StrProperty",nullptr,0x400001u).text;};
        query.ownerRefCount=[&](Handle value){return read(value,"ConSys.Conversation.ownerRefCount",Kind::Int,"IntProperty",nullptr,0u).integer;};
        query.writeOwnerRefCount=[&](Handle target,std::int32_t value){write(target,"ConSys.Conversation.ownerRefCount",Value::Integer(value));};
        query.outer=[&](Handle value){auto* object=pointer(value);if(!object->outer)throw std::runtime_error("ConversationList has no actual Outer");return handle(object->outer);};
        query.createNode=[&](Handle outer,std::uint32_t flags){return handle(ConstructConversationNode(pointer(outer),flags));};
        query.destroyNode=[&](Handle value){DestroyConversationNode(pointer(value));};
        query.flags=[&](Handle value){RequireConversationNode(pointer(value));return pointer(value)->reflection.flags;};
        query.writeFlags=[&](Handle target,std::uint32_t flags){write(target,"Core.Object.ObjectFlags",Value::Integer(std::bit_cast<std::int32_t>(flags)));};
        const auto objectValue=[&](Handle value){return Value::Text(Kind::Object,value?pointer(value)->reflection.objectPath:std::string{});};
        query.writeNodeConversation=[&](Handle target,Handle value){write(target,"ConSys.ConListItem.con",objectValue(value));};
        query.writeNodeNext=[&](Handle target,Handle value){write(target,"ConSys.ConListItem.Next",objectValue(value));};
        query.writeActorHead=[&](Handle target,Handle value){write(target,"Engine.Actor.ConListItems",objectValue(value));};
        query.work=[&]{LifecycleStep();};
        static_cast<void>(QuestVr::BindOriginalConversations(handle(list),handle(actor),query));
    }
    Evaluation ConversationBindNative(std::uint16_t index,RuntimeObject* actor,const std::vector<Evaluation>& arguments,
        const Function* supplied,QuestVr::Vm::Execution& execution) {
        const char* path=index==2100u?"DeusEx.DeusExPlayer.ConBindEvents":index==2101u?
            "DeusEx.DeusExDecoration.ConBindEvents":"DeusEx.ScriptedPawn.ConBindEvents";
        const auto authored=FunctionFor(Object(path));
        const auto valid=[&](const Function& function) {
            if(LowerAscii(function.path)!=LowerAscii(path) || function.nativeIndex!=index || function.flags!=0x401u ||
                !function.variables.empty() || !function.bytecode.empty())
                throw std::runtime_error("ConBindEvents lacks its exact original void native declaration");
        };
        valid(*authored);if(supplied){valid(*supplied);if(supplied->source!=authored->source)throw std::runtime_error("ConBindEvents source changed");}
        auto* declaration=Object(path);
        if(!arguments.empty() || !declaration->outer || !IsDerivedFromPath(actor->cls,declaration->outer->reflection.objectPath))
            throw std::runtime_error("ConBindEvents has invalid arguments or receiver ancestry");
        using Handle=std::uintptr_t;
        const auto handle=[](RuntimeObject* object){return reinterpret_cast<Handle>(object);};
        const auto pointer=[](Handle value){return reinterpret_cast<RuntimeObject*>(value);};
        QuestVr::OriginalConversationMissionQueries query;
        query.xlevel=[&](Handle value) {auto* object=pointer(value);static_cast<void>(NativeLevelInfo(object));return handle(object->nativeXLevel);};
        query.actorCount=[&](Handle value) {
            if(pointer(value)!=actor->nativeXLevel)throw std::runtime_error("Conversation wrapper has a foreign native Level");
            return persistentWorldActors.size();
        };
        query.actorAt=[&](Handle level,std::size_t slot) {
            auto* candidate=persistentWorldActors.at(slot);
            if(!candidate || candidate->worldRemoved) return Handle{};
            if(candidate->nativeXLevel!=pointer(level))throw std::runtime_error("Conversation wrapper live slot belongs to another native Level");
            return handle(candidate);
        };
        query.isDeusExLevelInfo=[&](Handle value){return IsDerivedFromPath(pointer(value)->cls,"DeusEx.DeusExLevelInfo");};
        query.missingLevelInfoLog=[] {__android_log_print(ANDROID_LOG_WARN,"quest_main","DeusExLevelInfo object missing!  Unable to bind Conversations!");};
        query.missionNumber=[&](Handle value) {
            auto* object=pointer(value);return Read(object,ConversationField(object,"DeusEx.DeusExLevelInfo.MissionNumber",
                Kind::Int,"IntProperty",nullptr,1u),0u).integer;
        };
        query.conversationPackage=[&](Handle value) {
            auto* object=pointer(value);return Read(object,ConversationField(object,"DeusEx.DeusExLevelInfo.ConversationPackage",
                Kind::String,"StrProperty",nullptr,0x400001u),0u).text;
        };
        query.loadList=[&](const std::string& selected,std::uint32_t flags) {
            if(flags!=2u)throw std::runtime_error("Conversation list load has nonoriginal flags");
            const auto separator=selected.find('.');
            if(separator==std::string::npos)throw std::runtime_error("Conversation list load lacks a package");
            const auto package=LowerAscii(selected.substr(0u,separator));
            // The embedding preload phase owns these real source linkers. A
            // missing object in a loaded package is a genuine null load; an
            // unavailable package loader is NOT fabricated as successful None.
            const auto& sources=persistentRuntime->get()->sourceLinkers;
            if(std::none_of(sources.begin(),sources.end(),[&](const auto& source){return LowerAscii(source.name)==package;}))
                throw std::runtime_error("Conversation StaticLoadObject package is not supplied by the actual source loader");
            const auto found=persistentVmObjects.find(LowerAscii(selected));
            if(found==persistentVmObjects.end())return Handle{};
            if(!IsDerivedFromPath(found->second->cls,"ConSys.ConversationList"))return Handle{};
            return handle(found->second);
        };
        query.bind=[&](Handle list,Handle receiver){BindConversationList(pointer(list),pointer(receiver),execution);};
        query.work=[&]{LifecycleStep();};
        static_cast<void>(QuestVr::BindOriginalActorConversationMission(handle(actor),query));
        ValidateStateBudget();return {};
    }
