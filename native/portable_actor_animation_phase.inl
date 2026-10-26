// Included inside PortableActorVmHost. This is only the ordinary retail MAIN
// animation phase, not complete Actor.Tick, a weapon-input bridge or a new VM
// root. The caller supplies its already captured Pawn/Role AnimEnd gate.
// Original ProcessEvent evidence: landing-state-20261010/
// original-landing-event-dispatch.log. All stores/callbacks use this Execution.
    Property OriginalAnimationField(RuntimeObject* actor,const char* owner,const char* name,
        Kind kind,std::uint32_t flags,const char* type) {
        const auto key=std::string(owner)+'.'+name;
        const auto property=DescribeProperty(key);auto* field=Object(property.key);
        if(LowerAscii(property.key)!=LowerAscii(key)||!OwnsProperty(actor->cls,property)||
            property.zero.kind!=kind||property.arrayDimension!=1u||property.flags!=flags||
            !field->property||field->property->type!=type||field->property->baseField||
            field->property->arrayDimension!=1||field->property->flags!=flags||
            field->outer!=Object(owner)||
            LowerAscii(Qualified(Table(field->sourcePath),Table(field->sourcePath).exports.at(field->exportIndex).ObjClass))!=
                std::string("core.")+LowerAscii(type))
            throw std::runtime_error("Original main animation lacks its actual scalar declaring field: "+key);
        if((kind==Kind::Float||kind==Kind::Bool||kind==Kind::Name||kind==Kind::Int)&&field->property->referencedType)
            throw std::runtime_error("Original main animation scalar field has an unexpected referenced subtype: "+key);
        return property;
    }
    Property OriginalAnimationFloatField(RuntimeObject* actor,QuestVr::OriginalActorAnimationFloat selected) {
        using F=QuestVr::OriginalActorAnimationFloat;
        const char* name{};std::uint32_t flags{};
        switch(selected) {
        case F::Frame:name="AnimFrame";flags=1u;break;
        case F::Rate:name="AnimRate";flags=1u;break;
        case F::TweenRate:name="TweenRate";break;
        case F::Last:name="AnimLast";break;
        case F::MinRate:name="AnimMinRate";flags=0x20u;break;
        default:throw std::runtime_error("Original main animation selector is not a scalar Float field");
        }
        auto property=OriginalAnimationField(actor,"Engine.Actor",name,Kind::Float,flags,"FloatProperty");
        if(Object(property.key)->property->referencedType)
            throw std::runtime_error("Original main animation Float has an unexpected referenced subtype");
        return property;
    }
    Property OriginalAnimationPlaneField(RuntimeObject* actor) {
        auto property=OriginalAnimationField(actor,"Engine.Actor","SimAnim",Kind::Struct,0x20u,"StructProperty");
        auto* structure=NativeCoordsStruct(Object(property.key),"core.object.plane");
        const auto schema=StructSchema(structure);const auto& declaration=StructDescriptor(structure);
        if(schema->kind!=Kind::Struct||schema->fields.size()!=4u)
            throw std::runtime_error("Original main animation SimAnim is not its canonical four-Float Plane");
        constexpr std::array<const char*,4u> names{"x","y","z","w"};auto child=declaration.children;
        for(std::size_t slot=0u;slot<names.size();++slot) {
            if(!child)throw std::runtime_error("Original main animation Plane field chain is truncated");
            auto* member=Object(Qualified(Table(structure->sourcePath),child));
            if(member->outer!=structure||!member->property||member->property->type!="FloatProperty"||
                member->property->arrayDimension!=1||member->property->baseField||member->property->referencedType||
                LowerAscii(member->reflection.objectPath)!=std::string("core.object.plane.")+names[slot]||
                LowerAscii(schema->fields[slot].key)!=LowerAscii(member->reflection.objectPath)||
                schema->fields[slot].arrayDimension!=1u||schema->fields[slot].zero.kind!=Kind::Float||
                schema->fields[slot].nested||
                LowerAscii(Qualified(Table(member->sourcePath),Table(member->sourcePath).exports.at(member->exportIndex).ObjClass))!="core.floatproperty")
                throw std::runtime_error("Original main animation Plane channel is not its original scalar Float");
            child=member->property->nextField;
        }
        if(child)throw std::runtime_error("Original main animation Plane field chain has extra members");
        return property;
    }
    Value OriginalAnimationRead(RuntimeObject* actor,const Property& property,Kind kind) {
        const auto value=Read(actor,property,0u);
        if(value.kind!=kind)throw std::runtime_error("Original main animation live field has the wrong storage kind");
        return value;
    }
    Value OriginalAnimationPlane(RuntimeObject* actor,const Property& property) {
        auto value=OriginalAnimationRead(actor,property,Kind::Struct);
        if(value.fields.size()!=4u)throw std::runtime_error("Original main animation live Plane has the wrong field count");
        for(const auto* name:{"x","y","z","w"}) {
            const auto found=value.fields.find(name);
            if(found==value.fields.end()||found->second.kind!=Kind::Float)
                throw std::runtime_error("Original main animation live Plane has an invalid Float channel");
        }
        return value;
    }
    // Native Tick writes individual native fields. ReferenceFor's generic script
    // callback refreshes the WHOLE optional clock, inspecting unrelated blend
    // fields. Do not use that path or reinstall an old pre-callback clock here.
    std::shared_ptr<Reference> OriginalAnimationReference(RuntimeObject* actor,const Property& property,
        std::optional<QuestVr::OriginalActorAnimationFloat> selected,const char* planeChannel=nullptr) {
        auto reference=std::make_shared<Reference>();
        reference->zero=planeChannel?Value::Float(0.0f):property.zero;
        reference->resultLayout=planeChannel?QuestVr::Vm::ResultLayout::Unknown:property.resultLayout;
        reference->dimension=1u;
        const std::string channel=planeChannel?planeChannel:std::string{};
        reference->read=[this,actor,property,channel] {
            static_cast<void>(MoveEventActor(actor));
            return channel.empty()?OriginalAnimationRead(actor,property,property.zero.kind):
                OriginalAnimationPlane(actor,property).fields.at(channel);
        };
        reference->write=[this,actor,property,selected,channel](const Value& incoming) {
            static_cast<void>(MoveEventActor(actor));
            if(incoming.kind!=(channel.empty()?property.zero.kind:Kind::Float))
                throw std::runtime_error("Original animation native store has the wrong storage kind");
            auto value=incoming;
            if(!channel.empty()) {
                if(value.kind!=Kind::Float)throw std::runtime_error("Original SimAnim channel store is not Float");
                auto plane=OriginalAnimationPlane(actor,property);plane.fields.at(channel)=value;value=std::move(plane);
            }
            auto normalized=TypedLiveValue(property,value);
            const auto key=InstanceStorageKey(actor->cls,property);
            Touch(actor);actor->scriptValues[key][0u]=std::move(normalized);actor->committedScriptState=true;
            if(actor->animationClock) {
                if(selected) {
                    using F=QuestVr::OriginalActorAnimationFloat;
                    switch(*selected) {
                    case F::Frame:actor->animationClock->pose.main.normalizedFrame=incoming.floating;break;
                    case F::Rate:actor->animationClock->main.rate=incoming.floating;break;
                    case F::TweenRate:actor->animationClock->main.tweenRate=incoming.floating;break;
                    case F::Last:actor->animationClock->main.last=incoming.floating;break;
                    case F::MinRate:actor->animationClock->main.minRate=incoming.floating;break;
                    default:break; // The clock has no main SimAnim mirror.
                    }
                }else if(LowerAscii(property.key)=="engine.actor.banimfinished")
                    actor->animationClock->main.finished=incoming.boolean;
            }
            ValidateStateBudget();
        };
        return reference;
    }
    void OriginalAnimationActorVirtuals(RuntimeObject* actor) {
        // These actual native vtables independently resolve both +10
        // ProcessEvent and +2C IsPendingKill to AActor's owned implementations.
        // A subclass's inherited CLASS bits do not prove its native boundary.
        // Engine Actor104268DC/Decoration104282F0/Inventory104286F8/
        // Projectile10428850/Pawn10426994/Weapon104287A4 use10302D74 and
        // 10302167. DeusExDecoration10027744/DeusExPlayer10027568/
        // ScriptedPawn10027624 use1001AF98/1001AF7A import thunks to the same
        // AActor methods. This list does not authorize other native classes.
        static constexpr const char* audited[]{"engine.actor","engine.decoration","engine.inventory",
            "engine.projectile","engine.pawn","engine.weapon","deusex.deusexdecoration",
            "deusex.deusexplayer","deusex.scriptedpawn"};
        std::array<RuntimeObject*,128u> visited{};std::size_t depth{};
        RuntimeObject* selected{};bool actualActor{};
        for(auto* cls=actor->cls;cls;cls=cls->base) {
            LifecycleStep();
            if(depth==visited.size()||std::find(visited.begin(),visited.begin()+depth,cls)!=visited.begin()+depth||
                !cls->classDescriptor||cls->reflection.metaClass!="Class"||!cls->classFieldLinksValid)
                throw std::runtime_error("Original animation native virtual class ancestry is malformed or cyclic");
            visited[depth++]=cls;
            const auto identity=CastClassIdentity(cls->reflection.objectPath);const auto path=LowerAscii(identity.path);
            actualActor=actualActor||path=="engine.actor";
            if(!selected&&(cls->reflection.flags&0x04000000u)!=0u) {
                if(std::find(std::begin(audited),std::end(audited),path)==std::end(audited))
                    throw std::runtime_error("Original animation nearest native ProcessEvent/IsPendingKill virtuals are not audited");
                selected=cls;
            }
        }
        if(!selected||!actualActor||CastObjectIdentity(actor->reflection.objectPath).classObject)
            throw std::runtime_error("Original animation virtual receiver is not an actual native Actor instance");
    }
    bool OriginalAnimationPendingKill(RuntimeObject* actor) {
        OriginalAnimationActorVirtuals(actor);
        const auto property=OriginalAnimationField(actor,"Engine.Actor","bDeleteMe",Kind::Bool,2u,"BoolProperty");
        return OriginalAnimationRead(actor,property,Kind::Bool).boolean;
    }
    std::uint32_t OriginalAnimationNativeLatent(RuntimeObject* actor) {
        // The raw loader FStateFrame contains the actual native UInt32 verbatim.
        // A modern Continue, null Code or portable finishAnimWaiting policy is
        // NOT evidence of its native latent slot. Only audited installed native
        // Sleep/WaitForLanding actions have represented nonzero equivalents.
        if(actor->stateObject&&actor->stateObject->frameOverride) {
            const auto& state=*actor->stateObject;
            if(!state.frame||!state.frame->control)
                throw std::runtime_error("Original main animation native latent action is unrepresented by this portable frame");
            if(state.frame->latent==QuestVr::StateLatent::Sleep)return 384u;
            if(state.frame->latent==QuestVr::StateLatent::WaitForLanding)return 528u;
            if(state.frame->latent==QuestVr::StateLatent::FinishAnim)return 385u;
            throw std::runtime_error("Original main animation modern native latent slot is unknown; Continue/Code presence cannot supply zero");
        }
        if(!actor->serializedStack)
            throw std::runtime_error("Original main animation reached a loop-ending branch without a represented native FStateFrame");
        return actor->serializedStack->latentAction;
    }
    QuestVr::OriginalActorAnimationPhaseStats AdvanceOriginalMainAnimation(RuntimeObject* actor,float elapsed,
        bool suppressAnimEnd,QuestVr::Vm::Execution& execution) {
        if(!transaction_)throw std::runtime_error("Original main animation requires its existing caller VM transaction");
        actor=MoveEventActor(actor);
        using F=QuestVr::OriginalActorAnimationFloat;
        using B=QuestVr::OriginalActorAnimationBool;
        // Each original phase executes at most four iterations. Retained mesh,
        // sequence and function handles are local and survive synchronous state
        // changes without reselecting the already chosen function identity.
        struct Selection {
            const PortableMeshAnimationSequence* sequence{};
            std::vector<const PortableMeshAnimationNotify*> originalOrder;
            bool ordered{};
        };
        std::array<RuntimeObject*,4u> meshes{};std::size_t meshCount{};
        std::array<Selection,4u> selections{};std::size_t selectionCount{};
        std::array<std::shared_ptr<const Function>,4u> functions{};std::size_t functionCount{};
        QuestVr::NativeScratchScope scratch(nativeQueryScratchBytes_,sizeof(meshes)+sizeof(selections)+sizeof(functions)+2048u,
            [this](std::size_t extra){ValidateStateBudget(extra);});
        const auto reserveScratch=[this](std::size_t bytes) {
            if(bytes>std::numeric_limits<std::size_t>::max()-nativeQueryScratchBytes_)
                throw std::runtime_error("Original animation temporary reservation overflow");
            ValidateStateBudget(bytes);nativeQueryScratchBytes_+=bytes;
            // The outer NativeScratchScope restores the entire lazy ledger.
        };
        const auto receiver=[this,actor](std::uintptr_t identity) {
            if(identity!=reinterpret_cast<std::uintptr_t>(actor))
                throw std::runtime_error("Original main animation changed its retained Actor identity");
            return MoveEventActor(actor);
        };
        const auto retainedMesh=[&](std::uintptr_t identity) {
            auto found=std::find_if(meshes.begin(),meshes.begin()+meshCount,[identity](auto* item) {
                return reinterpret_cast<std::uintptr_t>(item)==identity;
            });
            if(found==meshes.begin()+meshCount||Object((*found)->reflection.objectPath)!=*found)
                throw std::runtime_error("Original animation mesh identity is not a retained original asset");
            return *found;
        };
        const auto retainedSequence=[&](std::uintptr_t identity)->Selection& {
            for(std::size_t slot=0u;slot<selectionCount;++slot)
                if(reinterpret_cast<std::uintptr_t>(selections[slot].sequence)==identity)return selections[slot];
            throw std::runtime_error("Original animation sequence identity is not retained");
        };
        const auto find=[&](std::uintptr_t identity,const std::string& name,bool checked) {
            auto* current=receiver(identity);
            // FindFunctionChecked itself is GIsScriptable-gated; unchecked
            // FindFunction is not. Native passes the checked null through its
            // ProcessEvent gates without dereferencing it while disabled.
            if(checked&&!OriginalGameScriptsEnabled())return std::uintptr_t{};
            const auto context=CurrentDispatchContext(current,true);
            const auto path=context.exactSelected?
                QuestVr::ScriptDispatch::ResolveFunctionAtSelectedNode(DispatchGraph(),
                    current->cls->reflection.objectPath,context.selectedPath,name):
                QuestVr::ScriptDispatch::ResolveFunction(DispatchGraph(),
                    current->cls->reflection.objectPath,context.stateName,name);
            if(!path) {
                if(checked)throw std::runtime_error("Original animation FindFunctionChecked(AnimEnd) found no declaration");
                return std::uintptr_t{};
            }
            const auto method=FunctionIdentity(Object(*path));
            auto* declaration=Object(method->path);
            if(!declaration->script||method->source!=declaration->sourcePath||
                LowerAscii(Qualified(Table(declaration->sourcePath),Table(declaration->sourcePath).exports.at(declaration->exportIndex).ObjClass))!="core.function")
                throw std::runtime_error("Original animation selected function has no genuine authored UFunction identity");
            auto* owner=declaration->outer;if(owner&&owner->stateDescriptor)owner=owner->outer;
            if(!owner||!owner->classDescriptor||owner->defaultOwner||
                !IsDerivedFromPath(current->cls,owner->reflection.objectPath))
                throw std::runtime_error("Original animation selected function is not owned by actual receiver ancestry");
            if(functionCount==functions.size())throw std::runtime_error("Original animation retained function bound exceeded");
            reserveScratch(sizeof(Function)+method->path.capacity()+method->source.capacity()+2u);
            functions[functionCount++]=method;
            return reinterpret_cast<std::uintptr_t>(method.get());
        };
        QuestVr::OriginalActorAnimationPhaseQueries queries;
        queries.suppressAnimEnd=suppressAnimEnd;
        queries.work=[this]{LifecycleStep();};
        queries.sequence=[&](std::uintptr_t identity) {
            auto* current=receiver(identity);
            return OriginalAnimationRead(current,OriginalAnimationField(current,"Engine.Actor","AnimSequence",
                Kind::Name,0x21u,"NameProperty"),Kind::Name).text;
        };
        queries.readFloat=[&](std::uintptr_t identity,F selected) {
            auto* current=receiver(identity);
            if(selected==F::VelocityX||selected==F::VelocityY||selected==F::VelocityZ) {
                const auto property=NativeCoordsProperty(current,"Engine.Actor.Velocity",Kind::Vector,0x21u);
                const auto value=OriginalAnimationRead(current,property,Kind::Vector);
                return value.vector[selected==F::VelocityX?0u:selected==F::VelocityY?1u:2u];
            }
            if(selected==F::SimX||selected==F::SimY) {
                return OriginalAnimationPlane(current,OriginalAnimationPlaneField(current)).fields.at(selected==F::SimX?"x":"y").floating;
            }
            return OriginalAnimationRead(current,OriginalAnimationFloatField(current,selected),Kind::Float).floating;
        };
        queries.writeFloat=[&](std::uintptr_t identity,F selected,float value) {
            auto* current=receiver(identity);
            if(selected==F::SimX||selected==F::SimY) {
                const auto property=OriginalAnimationPlaneField(current);
                execution.WriteReference(OriginalAnimationReference(current,property,selected,selected==F::SimX?"x":"y"),Value::Float(value));
            }else {
                const auto property=OriginalAnimationFloatField(current,selected);
                execution.WriteReference(OriginalAnimationReference(current,property,selected),Value::Float(value));
            }
        };
        queries.readBool=[&](std::uintptr_t identity,B selected) {
            auto* current=receiver(identity);const bool notify=selected==B::Notify;
            if(!notify&&selected!=B::Loop)throw std::runtime_error("Unknown original animation Bool selector");
            return OriginalAnimationRead(current,OriginalAnimationField(current,"Engine.Actor",notify?"bAnimNotify":"bAnimLoop",
                Kind::Bool,notify?0x20u:0u,"BoolProperty"),Kind::Bool).boolean;
        };
        queries.writeFinished=[&](std::uintptr_t identity,bool value) {
            auto* current=receiver(identity);const auto property=OriginalAnimationField(current,"Engine.Actor","bAnimFinished",
                Kind::Bool,0u,"BoolProperty");
            execution.WriteReference(OriginalAnimationReference(current,property,{}),Value::Bool(value));
        };
        queries.mesh=[&](std::uintptr_t identity) {
            auto* current=receiver(identity);const auto property=OriginalAnimationField(current,"Engine.Actor","Mesh",
                Kind::Object,0x21u,"ObjectProperty");
            auto* field=Object(property.key);
            if(!field->property->referencedType||
                LowerAscii(Qualified(Table(field->sourcePath),field->property->referencedType))!="engine.mesh")
                throw std::runtime_error("Original animation Actor.Mesh lacks the genuine Mesh constraint");
            const auto value=OriginalAnimationRead(current,property,Kind::Object);
            ValidateObjectValue(value,"Engine.Mesh",false);
            if(value.text.empty()||LowerAscii(value.text)=="none")return std::uintptr_t{};
            auto* mesh=Object(value.text);
            auto found=std::find(meshes.begin(),meshes.begin()+meshCount,mesh);
            if(found==meshes.begin()+meshCount) {
                if(meshCount==meshes.size())throw std::runtime_error("Original animation retained mesh bound exceeded");
                meshes[meshCount++]=mesh;
            }
            if(!mesh->lodMesh)mesh->lodMesh=LoadCompleteRuntimeLodMesh(Table(mesh->sourcePath),mesh->exportIndex,mesh->sourcePath);
            return reinterpret_cast<std::uintptr_t>(mesh);
        };
        queries.meshSequence=[&](std::uintptr_t identity,const std::string& name) {
            auto* mesh=retainedMesh(identity);
            if(!mesh->lodMesh||!mesh->lodMesh->animation)return std::uintptr_t{};
            for(const auto& sequence:mesh->lodMesh->animation->sequences) {
                LifecycleStep();if(!QuestVr::MeshAnimationNamesEqual(sequence.name,name))continue;
                for(std::size_t slot=0u;slot<selectionCount;++slot)
                    if(selections[slot].sequence==&sequence)return reinterpret_cast<std::uintptr_t>(&sequence);
                if(selectionCount==selections.size())throw std::runtime_error("Original animation retained sequence bound exceeded");
                selections[selectionCount++].sequence=&sequence;return reinterpret_cast<std::uintptr_t>(&sequence);
            }
            return std::uintptr_t{};
        };
        queries.notifyCount=[&](std::uintptr_t identity) {
            auto& selection=retainedSequence(identity);const auto count=selection.sequence->notifies.size();
            if(count>queries.maxNotifySearches)throw std::runtime_error("Original animation notify count exceeds its phase bound");
            if(!selection.ordered) {
                reserveScratch(QuestVr::DecalStateDetail::Multiply(count,sizeof(const PortableMeshAnimationNotify*)));
                selection.originalOrder.resize(count,nullptr);
                for(const auto& notification:selection.sequence->notifies) {
                    LifecycleStep();const auto slot=static_cast<std::size_t>(notification.serializedIndex);
                    if(slot>=count||selection.originalOrder[slot])
                        throw std::runtime_error("Original animation notify array lacks unique original serialized ordinal ownership");
                    selection.originalOrder[slot]=&notification;
                }
                selection.ordered=true;
            }
            return count;
        };
        queries.notifyTime=[&](std::uintptr_t identity,std::size_t slot) {
            auto& selection=retainedSequence(identity);
            if(!selection.ordered||slot>=selection.originalOrder.size()||!selection.originalOrder[slot])
                throw std::runtime_error("Original animation notify time slot has no retained authored ordinal");
            return selection.originalOrder[slot]->time;
        };
        queries.notifyFunction=[&](std::uintptr_t identity,std::size_t slot) {
            auto& selection=retainedSequence(identity);
            if(!selection.ordered||slot>=selection.originalOrder.size()||!selection.originalOrder[slot])
                throw std::runtime_error("Original animation notify name slot has no retained authored ordinal");
            return selection.originalOrder[slot]->function;
        };
        queries.findFunction=[&](std::uintptr_t identity,const std::string& name){return find(identity,name,false);};
        queries.findAnimEnd=[&](std::uintptr_t identity){return find(identity,"AnimEnd",true);};
        queries.processEvent=[&](std::uintptr_t identity,std::uintptr_t selected) {
            auto* current=receiver(identity);std::shared_ptr<const Function> function;
            for(std::size_t slot=0u;slot<functionCount;++slot)
                if(reinterpret_cast<std::uintptr_t>(functions[slot].get())==selected)function=functions[slot];
            if(selected&&!function)throw std::runtime_error("Original animation ProcessEvent lost its retained selected UFunction identity");
            // Actor.ProcessEvent10369100 precedes Core.ProcessEvent1013F7D0:
            // Level.bBegunPlay, GIsScriptable, selected probe, virtual pending
            // kill, nativeIndex, then nativeIndex0/flags400 RPC dispatch.
            OriginalAnimationActorVirtuals(current);
            const auto levelField=OriginalAnimationField(current,"Engine.Actor","Level",Kind::Object,2u,"ObjectProperty");
            auto* levelMetadata=Object(levelField.key);
            if(!levelMetadata->property->referencedType||
                LowerAscii(Qualified(Table(levelMetadata->sourcePath),levelMetadata->property->referencedType))!="engine.levelinfo")
                throw std::runtime_error("Original animation Actor.Level lacks its original LevelInfo constraint");
            const auto levelValue=OriginalAnimationRead(current,levelField,Kind::Object);
            if(levelValue.text.empty())throw std::runtime_error("Original animation ProcessEvent has no actual Actor.Level");
            auto* level=Object(levelValue.text);ValidateObjectValue(levelValue,"Engine.LevelInfo",false);
            const auto begun=OriginalAnimationField(level,"Engine.LevelInfo","bBegunPlay",Kind::Bool,0u,"BoolProperty");
            if(!OriginalAnimationRead(level,begun,Kind::Bool).boolean)return;
            if(!OriginalGameScriptsEnabled())return;
            if(!function)throw std::runtime_error("Enabled original animation ProcessEvent has no selected UFunction");
            if(!CanCall(*function,current->reflection.objectPath))return;
            if(OriginalAnimationPendingKill(current))return;
            if(function->nativeIndex!=0u)return;
            if(function->flags&0x400u)
                throw std::runtime_error("Original animation ProcessEvent nativeIndex0 RPC virtual branch is not represented");
            const auto declaration=FunctionFor(Object(function->path));
            if(declaration->source!=function->source||declaration->nativeIndex!=function->nativeIndex||declaration->flags!=function->flags)
                throw std::runtime_error("Original animation ProcessEvent declaration changed retained identity");
            for(const auto& variable:declaration->variables)if(variable.flags&0x80u)
                throw std::runtime_error("Original animation null ProcessEvent packet cannot supply parameter/result fields");
            static_cast<void>(execution.CallFunction(*function,current->reflection.objectPath,{}));
        };
        queries.remoteRole=[&](std::uintptr_t identity) {
            auto* current=receiver(identity);const auto property=OriginalAnimationField(current,"Engine.Actor","RemoteRole",
                Kind::Byte,0x21u,"ByteProperty");
            auto* field=Object(property.key);
            if(!field->property->referencedType||
                LowerAscii(Qualified(Table(field->sourcePath),field->property->referencedType))!="engine.actor.enetrole")
                throw std::runtime_error("Original animation RemoteRole lacks its actual ENetRole declaration");
            return static_cast<std::uint8_t>(OriginalAnimationRead(current,property,Kind::Byte).integer);
        };
        queries.isWeapon=[this](std::uintptr_t identity){return RetailTouchClass(identity,"Engine.Weapon");};
        queries.nativeLatentAction=[&](std::uintptr_t identity){return OriginalAnimationNativeLatent(receiver(identity));};
        return QuestVr::AdvanceOriginalActorMainAnimation(reinterpret_cast<std::uintptr_t>(actor),elapsed,queries);
    }
