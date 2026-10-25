// Included inside PortableActorVmHost. An isolated real native Window fixture,
// not a substitute for the configured DeusEx HUD or a published window graph.
    Value ProbeNativeWindowAxisQuery(const std::string& playerPath,PortableWindowAxisProbeMode mode,
        PortableWindowAxisProbeResult& result,QuestVr::Vm::Execution& execution) {
        switch(mode) {
        case PortableWindowAxisProbeMode::RoundTrip:case PortableWindowAxisProbeMode::LateFailure:
        case PortableWindowAxisProbeMode::WrongReceiver:case PortableWindowAxisProbeMode::WrongArgument:
        case PortableWindowAxisProbeMode::MissingArgument:break;
        default:throw std::runtime_error("Unknown native preferred-axis diagnostic mode");
        }
        auto* pawn=Object(playerPath);RequireMapActor(pawn);
        if(!IsDerivedFromPath(pawn->cls,"Extension.PlayerPawnExt") || pawn->nativeRootWindow)
            throw std::runtime_error("Native preferred-axis diagnostic requires an uninitialized actual player");
        constexpr std::size_t snapshotBytes=128u<<10u;
        ValidateStateBudget(snapshotBytes);nativeActorOrderObservationBytes_+=snapshotBytes;
        QuestVr::AuthoredStructLimits bounds;bounds.retainedBytes=snapshotBytes;
        QuestVr::AuthoredStructDetail::Budget retained{bounds};
        result.stages.reserve(16u);
        retained.Retain(sizeof(result)+result.stages.capacity()*sizeof(PortableWindowAxisProbeStage));
        auto* root=ConstructNativeRootWindow(pawn,DefaultClass(Object("Extension.RootWindow")),execution);
        const auto rootInit=Member(root,"InitWindow");
        if(!rootInit)throw std::runtime_error("Native preferred-axis fixture lacks the original base Root InitWindow");
        static_cast<void>(execution.CallFunction(*rootInit,root->reflection.objectPath,{}));
        const auto newChild=NewChildWindowDeclaration(nullptr);
        const auto born=execution.CallFunction(*newChild,root->reflection.objectPath,
            {{Value::Text(Kind::Object,"Extension.Window"),{}},{Value::Bool(false),{}}});
        if(born.kind!=Kind::Object || born.text.empty())
            throw std::runtime_error("Native preferred-axis fixture NewChild returned None");
        auto* window=Object(born.text);RequireNativeWindow(window);
        for(const auto& [target,object]:{std::pair{&result.rootPath,root},std::pair{&result.windowPath,window}}) {
            *target=object->reflection.objectPath;retained.Retain(target->capacity()+1u);
        }
        // Explicit fixture constraints, all journaled. These are not changes to
        // authored class defaults, guessed layout results or skipped callbacks.
        const auto setFloat=[&](const char* field,float value) {
            NativeWindowWrite(window,("Extension.Window."+std::string(field)).c_str(),Value::Float(value),execution);
        };
        setFloat("Width",37.0f);setFloat("Height",53.0f);
        setFloat("hardcodedWidth",-1.0f);setFloat("hardcodedHeight",-1.0f);
        NativeWindowWrite(window,"Extension.Window.bNeedsQuery",Value::Bool(true),execution);
        const auto query=[&](RuntimeObject* receiver,std::uint16_t index,std::vector<Evaluation> arguments,
            const char* name,float input) {
            const auto method=WindowNativeDeclaration(index,nullptr);
            const auto value=execution.CallFunction(*method,receiver->reflection.objectPath,arguments);
            if(value.kind!=Kind::Float || !std::isfinite(value.floating))
                throw std::runtime_error("Native preferred-axis fixture received no finite written Float result");
            if(result.stages.size()>=16u)throw std::runtime_error("Native preferred-axis snapshot count exceeded");
            PortableWindowAxisProbeStage stage;stage.name=name;stage.nativeIndex=index;
            stage.input=input;stage.result=value.floating;stage.writes=execution.WriteCount();
            retained.Retain(stage.name.capacity()+1u);
            NativeWindowQueryOperations operations{*this,window,execution};
            stage.lastSpecifiedWidth=operations.ReadFloat("lastSpecifiedWidth");
            stage.lastSpecifiedHeight=operations.ReadFloat("lastSpecifiedHeight");
            stage.lastQueryWidth=operations.ReadFloat("lastQueryWidth");
            stage.lastQueryHeight=operations.ReadFloat("lastQueryHeight");
            stage.lastWidthSpecified=operations.ReadBool("bLastWidthSpecified");
            stage.lastHeightSpecified=operations.ReadBool("bLastHeightSpecified");
            stage.needsQuery=operations.ReadBool("bNeedsQuery");
            result.stages.push_back(std::move(stage));
        };
        const auto input=[&](std::uint16_t index,float value,const char* name) {
            query(window,index,{{Value::Float(value),{}}},name,value);
        };
        if(mode==PortableWindowAxisProbeMode::WrongReceiver)
            query(pawn,1463u,{{Value::Float(1.0f),{}}},"wrong-receiver",1.0f);
        else if(mode==PortableWindowAxisProbeMode::WrongArgument)
            query(window,1463u,{{Value::Integer(1),{}}},"wrong-argument",1.0f);
        else if(mode==PortableWindowAxisProbeMode::MissingArgument)
            query(window,1464u,{},"missing-argument",0.0f);
        else {
            input(1463u,21.0f,"width");input(1463u,21.0f,"width-cache");
            input(1464u,31.0f,"height");input(1464u,31.0f,"height-cache");
            input(1463u,-7.0f,"width-negative");input(1464u,-9.0f,"height-negative");
            input(1463u,-0.0f,"width-minuszero");input(1463u,0.0f,"width-pluszero-cache");
            input(1464u,-0.0f,"height-minuszero");input(1464u,0.0f,"height-pluszero-cache");
            // Real public declaration-entry defaults: an unwritten attached
            // result must use the wrapper's +0 seed without loading its alias.
            auto stale=std::make_shared<Reference>();stale->zero=Value::Float(0.0f);
            stale->read=[]()->Value {throw std::runtime_error("Preferred-axis fixture loaded an unwritten stale alias");};
            query(window,1463u,{{Value::Float(999.0f),stale,QuestVr::Vm::ResultDisposition::NoWrite}},
                "width-nowrite",0.0f);
            query(window,1464u,{{Value::Float(999.0f),stale,QuestVr::Vm::ResultDisposition::ZeroFill,4u}},
                "height-zerofill",0.0f);
            setFloat("hardcodedWidth",44.0f);setFloat("hardcodedHeight",55.0f);
            input(1463u,-17.0f,"width-promoted");input(1464u,-19.0f,"height-promoted");
            if(mode==PortableWindowAxisProbeMode::LateFailure)
                throw std::runtime_error("Native preferred-axis diagnostic late failure after valid queries");
        }
        throw std::runtime_error("Native preferred-axis diagnostic complete; rollback required");
    }
