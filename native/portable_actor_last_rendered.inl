// Actual Engine.Actor.LastRendered723; no current render-publication write or
// synthesized visibility/default is introduced by this read-only native query.
    Evaluation OriginalLastRenderedNative(RuntimeObject* actor,const std::vector<Evaluation>& arguments,
        const Function* declaration) {
        if(!transaction_)throw std::runtime_error("Original LastRendered lacks its caller root");
        if(!arguments.empty())throw std::runtime_error("Original LastRendered takes no input parameters");
        // Numeric native bytecode carries no UFunction pointer, while a named
        // native call can carry only a lazy identity. Authenticate the actual
        // authored declaration in both cases, rather than rejecting either
        // original call form or accepting an unauthenticated result schema.
        const auto authored=FunctionFor(Object("Engine.Actor.LastRendered"));
        const auto validate=[&](const Function& function) {
        if(LowerAscii(function.path)!="engine.actor.lastrendered"||function.nativeIndex!=723u||
            function.flags!=0x401u||function.variables.size()!=1u)
            throw std::runtime_error("Original LastRendered lacks its actual native723 declaration");
        auto* method=Object(function.path);const auto& result=function.variables.front();auto* field=Object(result.key);
        if(!method->script||method->outer!=Object("Engine.Actor")||method->sourcePath!=function.source||
            method->script->nativeIndex!=723u||method->script->functionFlags!=0x401u||
            LowerAscii(Qualified(Table(method->sourcePath),Table(method->sourcePath).exports.at(method->exportIndex).ObjClass))!="core.function"||
            result.key!="Engine.Actor.LastRendered.ReturnValue"||result.zero.kind!=Kind::Float||result.arrayDimension!=1u||result.flags!=0x580u||
            field->outer!=method||!field->property||field->property->type!="FloatProperty"||field->property->arrayDimension!=1||
            field->property->flags!=0x580u||field->property->baseField||field->property->referencedType||
            LowerAscii(Qualified(Table(field->sourcePath),Table(field->sourcePath).exports.at(field->exportIndex).ObjClass))!="core.floatproperty")
            throw std::runtime_error("Original LastRendered has a different actual Float result packet");
        };
        validate(*authored);
        if(declaration) {
            if(declaration->path!=authored->path||declaration->source!=authored->source||
                declaration->nativeIndex!=authored->nativeIndex||declaration->flags!=authored->flags)
                throw std::runtime_error("Original LastRendered has a foreign callable identity");
            if(!declaration->variables.empty())validate(*declaration);
        }
        actor=LifecycleActor(reinterpret_cast<std::uintptr_t>(actor));
        const auto source=reinterpret_cast<std::uintptr_t>(actor);
        const auto require=[&](auto identity) {
            if(identity!=source)throw std::runtime_error("Original LastRendered changed its retained Actor");
            return LifecycleActor(identity);
        };
        QuestVr::OriginalActorLastRenderedQueries q;q.work=[this]{LifecycleStep();};
        q.nativeLevel=[&](auto a){return reinterpret_cast<std::uintptr_t>(require(a)->nativeXLevel);};
        q.lastRenderTime=[&](auto a) {
            auto* r=require(a);static_cast<void>(OriginalAnimationField(r,"Engine.Actor","LastRenderTime",Kind::Float,0u,"FloatProperty"));
            if(!r->lastRenderTime)throw std::runtime_error("Original LastRendered has no initialized native LastRenderTime");
            return *r->lastRenderTime;
        };
        q.nativeTime=[&](auto identity) {
            if(identity!=reinterpret_cast<std::uintptr_t>(actor->nativeXLevel))
                throw std::runtime_error("Original LastRendered changed its captured native Level");
            auto* level=ZoneLevel(identity);
            if(!level->nativeTimeSeconds)throw std::runtime_error("Original LastRendered has no native ULevel Double clock");
            return *level->nativeTimeSeconds;
        };
        return {Value::Float(QuestVr::QueryOriginalActorLastRendered(source,q).returned),{}};
    }
