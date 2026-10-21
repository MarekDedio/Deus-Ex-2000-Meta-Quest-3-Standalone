// Included inside PortableActorVmHost after actual property teardown helpers.
    std::uint32_t DestroyObjectFlags(RuntimeObject* object) {
        const auto field=DescribeProperty("Core.Object.ObjectFlags");auto* metadata=Object(field.key);
        if(field.zero.kind!=Kind::Int||field.arrayDimension!=1u||field.flags!=0x1002u||
            !OwnsProperty(object->cls,field)||metadata->outer!=Object("Core.Object")||
            !metadata->property||metadata->property->type!="IntProperty")
            throw std::runtime_error("ConditionalDestroy lacks original UObject ObjectFlags authority");
        const auto value=Read(object,field,0u);
        if(value.kind!=Kind::Int)throw std::runtime_error("ConditionalDestroy ObjectFlags is not an Int");
        return std::bit_cast<std::uint32_t>(value.integer);
    }
    void DestroyWriteObjectFlags(RuntimeObject* object,std::uint32_t flags,QuestVr::Vm::Execution& execution) {
        static_cast<void>(DestroyObjectFlags(object));const auto field=DescribeProperty("Core.Object.ObjectFlags");
        auto reference=std::make_shared<Reference>();reference->zero=field.zero;
        reference->read=[this,object,field]{return Read(object,field,0u);};
        reference->write=[this,object,field](const Value& value) {
            if(value.kind!=Kind::Int)throw std::runtime_error("ConditionalDestroy ObjectFlags store is not an Int");
            Touch(object);object->reflection.flags=std::bit_cast<std::uint32_t>(value.integer);
            object->scriptValues[InstanceStorageKey(object->cls,field)][0u]=value;
            object->committedScriptState=true;
        };
        execution.WriteReference(reference,Value::Integer(std::bit_cast<std::int32_t>(flags)));ValidateStateBudget();
    }
    std::int32_t ConditionalDestroyObject(RuntimeObject* object,QuestVr::Vm::Execution& execution) {
        LifecycleActor(reinterpret_cast<std::uintptr_t>(object));
        const auto resolve=[this](std::uintptr_t identity){return LifecycleActor(identity);};
        QuestVr::OriginalConditionalDestroyQueries q;
        q.objectIndex=[resolve](std::uintptr_t identity) {
            const auto* current=resolve(identity);
            return current->objectIndex?static_cast<std::int32_t>(current->objectIndex.Index()):-1;
        };
        q.objectFlags=[this,resolve](std::uintptr_t identity){return DestroyObjectFlags(resolve(identity));};
        q.writeObjectFlags=[this,resolve,&execution](std::uintptr_t identity,std::uint32_t flags) {
            DestroyWriteObjectFlags(resolve(identity),flags,execution);
        };
        q.nativeDestroy=[this,resolve,&execution](std::uintptr_t identity) {
            auto* current=resolve(identity);bool selected{};
            // Authenticate the nearest native superclass, not any convenient
            // leaf/path or a universal Actor override bypass. These exact
            // finite vtable profiles are audited in ORIGINAL-DESTROY-ACTOR.
            std::array<RuntimeObject*,128u> seen{};std::size_t count{};
            for(auto* cls=current->cls;cls;cls=cls->base) {
                LifecycleStep();
                if(count==seen.size()||std::find(seen.begin(),seen.begin()+count,cls)!=seen.begin()+count)
                    throw std::runtime_error("ConditionalDestroy native selection hierarchy is unsafe");
                seen[count++]=cls;
                if(!cls->classDescriptor)throw std::runtime_error("ConditionalDestroy native Actor Class has no original descriptor");
                // RF_Native belongs to the actual UClass export header.
                // ClassFlags80 is RuntimeStatic, NOT native implementation.
                if(!AnyFlags(Table(cls->sourcePath).exports.at(cls->exportIndex).ObjFlags,ObjectFlags::Native))continue;
                const auto name=LowerAscii(cls->reflection.objectPath);
                selected=name=="engine.actor"||name=="engine.keypoint"||name=="engine.decoration"||
                    name=="engine.inventory"||name=="engine.pawn"||name=="engine.brush"||
                    name=="deusex.deusexdecoration"||name=="deusex.scriptedpawn";
                if(!selected)throw std::runtime_error("ConditionalDestroy selected native Actor override is not implemented");
                break;
            }
            if(!selected)throw std::runtime_error("ConditionalDestroy has no authenticated native Actor Destroy selection");
            QuestVr::OriginalActorDestroyQueries actor;
            actor.auxiliary=[this,resolve](std::uintptr_t receiver) {
                auto* source=resolve(receiver);
                const auto field=RetailPredicatePointerField(source,"Engine.Actor.RenderInterface",
                    "Engine.Actor","Engine.RenderIterator",0x2000u);
                static_cast<void>(field);
                if(!source->nativeRenderInterfaceBound)
                    throw std::runtime_error("Actor.Destroy native RenderInterface storage is not bound");
                auto* auxiliary=source->nativeRenderInterface;
                if(auxiliary&&(Object(auxiliary->reflection.objectPath)!=auxiliary||
                    !IsDerivedFromPath(auxiliary->cls,"Engine.RenderIterator")))
                    throw std::runtime_error("Actor.Destroy native RenderInterface has a different live identity/Class");
                return reinterpret_cast<std::uintptr_t>(auxiliary);
            };
            actor.conditionalDestroy=[](std::uintptr_t)->std::int32_t {
                throw std::runtime_error("Actor.Destroy nonnull RenderIterator native teardown is not implemented");
            };
            actor.clearAuxiliary=[](std::uintptr_t) {
                throw std::runtime_error("Actor.Destroy auxiliary clear requires completed RenderIterator teardown");
            };
            actor.baseObjectDestroy=[this,resolve,&execution](std::uintptr_t receiver) {
                QuestVr::OriginalObjectDestroyQueries base;
                base.objectFlags=[this,resolve](std::uintptr_t target){return DestroyObjectFlags(resolve(target));};
                base.writeObjectFlags=[this,resolve,&execution](std::uintptr_t target,std::uint32_t flags) {
                    DestroyWriteObjectFlags(resolve(target),flags,execution);
                };
                base.actualClass=[resolve](std::uintptr_t target){return reinterpret_cast<std::uintptr_t>(resolve(target)->cls);};
                base.exitProperties=[this,resolve,&execution](std::uintptr_t target,std::uintptr_t cls) {
                    ExitObjectProperties(resolve(target),reinterpret_cast<RuntimeObject*>(cls),execution);
                };
                base.setLinker=[this,resolve](std::uintptr_t target,std::uintptr_t linker,std::int32_t index) {
                    SetNativeObjectLinker(resolve(target),linker,index);
                };
                base.outer=[resolve](std::uintptr_t target){return reinterpret_cast<std::uintptr_t>(resolve(target)->outer);};
                base.objectIndex=[this](std::uintptr_t target) {
                    auto* outer=reinterpret_cast<RuntimeObject*>(target);
                    if(!outer||Object(outer->reflection.objectPath)!=outer||!outer->objectIndex||
                        outer->objectIndex.Index()>static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max()))
                        throw std::runtime_error("UObject.Destroy cached Outer has no actual native object index");
                    return static_cast<std::int32_t>(outer->objectIndex.Index());
                };
                base.writeLinkerIndex=[this,resolve](std::uintptr_t target,std::int32_t index) {
                    NativeLinkerIndexStore(resolve(target),index);
                };
                base.work=[this]{LifecycleStep();};
                QuestVr::DestroyOriginalObject(receiver,base);
            };
            actor.work=[this]{LifecycleStep();};QuestVr::DestroyOriginalActor(identity,actor);
        };
        q.fullName=[](std::uintptr_t)->std::string {
            throw std::runtime_error("ConditionalDestroy missing-Super diagnostic native GetFullName is not implemented");
        };
        q.logMissingDebugDestroy=[](const std::string&) {
            throw std::runtime_error("ConditionalDestroy native diagnostic device is not implemented");
        };
        q.work=[this]{LifecycleStep();};return QuestVr::ConditionalDestroyOriginalObject(reinterpret_cast<std::uintptr_t>(object),q);
    }
