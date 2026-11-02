// Actual native Root.Process binding. Physical Quest mapping and InputExt's
// current-binding route remain separate; no provider is fabricated here.
    PortableRootInputProbeResult* rootInputObservation_{};
    void RequireRootInputOwner(RuntimeObject* root) {
        if(!transaction_)throw std::runtime_error("Original Root input requires its actual outer VM root");
        RequireKeyboardWindow(root);
        if(!root->nativeRootWindowObject||!IsDerivedFromPath(root->cls,"Extension.RootWindow")||
            NativeWindowRoot(root)!=root||!persistentWindowDomain||!persistentWindowDomain->get()||
            std::find(persistentWindowDomain->get()->roots.begin(),persistentWindowDomain->get()->roots.end(),root)==
                persistentWindowDomain->get()->roots.end())
            throw std::runtime_error("Original Root input lost its actual constructed terminal Root owner");
    }
    Property RootInputKeyMap(RuntimeObject* root) {
        RequireRootInputOwner(root);const auto property=DescribeProperty("Extension.RootWindow.keyDownMap");
        auto* field=Object(property.key);
        if(property.zero.kind!=Kind::Byte||property.arrayDimension!=255u||property.flags||
            !OwnsProperty(root->cls,property)||!field->property||field->property->type!="ByteProperty"||
            field->property->referencedType||field->outer!=Object("Extension.RootWindow"))
            throw std::runtime_error("Original Root input requires the actual Byte255 keyDownMap declaration");
        return property;
    }
    std::uint8_t RootInputKey(RuntimeObject* root,std::int32_t key) {
        if(key<0||key>=255)throw std::runtime_error("Original Root key exceeds its actual255-key storage");
        const auto value=Read(root,RootInputKeyMap(root),static_cast<std::uint32_t>(key));
        if(value.kind!=Kind::Byte||value.integer<0||value.integer>255)
            throw std::runtime_error("Original Root actual keyDownMap cell is not Byte");
        return static_cast<std::uint8_t>(value.integer);
    }
    RuntimeObject* RootInputParentPawn(RuntimeObject* root) {
        RequireRootInputOwner(root);
        const auto value=KeyboardWindowScalar(root,"Extension.RootWindow.parentPawn",Kind::Object,
            "Extension.RootWindow","ObjectProperty","Extension.PlayerPawnExt");
        if(value.text.empty())return nullptr;
        auto* pawn=Object(value.text);RequireMapActor(pawn);
        if(!IsDerivedFromPath(pawn->cls,"Extension.PlayerPawnExt"))
            throw std::runtime_error("Original Root parentPawn is not its actual declared player type");
        return pawn;
    }
    void ClearRootParentButton(RuntimeObject* root,const char* path,QuestVr::Vm::Execution& execution) {
        // Reread parentPawn separately for Fire and AltFire as retail does.
        auto* pawn=RootInputParentPawn(root);
        if(!pawn)throw std::runtime_error("Original Root parentPawn disappeared before its selected button store");
        const auto property=DescribeProperty(path);auto* field=Object(property.key);
        if(property.zero.kind!=Kind::Byte||property.arrayDimension!=1u||property.flags!=4u||
            !OwnsProperty(pawn->cls,property)||!field->property||field->property->type!="ByteProperty"||
            field->property->referencedType||field->outer!=Object("Engine.Pawn"))
            throw std::runtime_error("Original Root Fire clear lacks its actual Pawn INPUT Byte declaration");
        execution.WriteReference(ReferenceFor(pawn,property,0u,false),Value::Byte(0u));
    }
    bool ProcessNativeRootInput(RuntimeObject* root,std::int32_t key,std::int32_t action,float delta,
        QuestVr::Vm::Execution& execution) {
        RequireRootInputOwner(root);
        QuestVr::NativeScratchScope control(nativeQueryScratchBytes_,sizeof(QuestVr::OriginalRootInputQueries)+256u,
            [this](std::size_t bytes){ValidateStateBudget(bytes);});
        const auto previous=nativeQueryScratchBytes_;std::size_t frames{};
        struct Restore {std::size_t& ledger;std::size_t before;~Restore(){ledger=before;}} restore{nativeQueryScratchBytes_,previous};
        QuestVr::OriginalRootInputQueries q;
        q.work=[this]{LifecycleStep();};
        q.enterFrame=[&](std::size_t bytes){const auto next=QuestVr::DecalStateDetail::Add(frames,bytes);
            nativeQueryScratchBytes_=previous;ValidateStateBudget(next);frames=next;nativeQueryScratchBytes_=previous+frames;};
        q.leaveFrame=[&](std::size_t bytes)noexcept{if(bytes>frames)std::terminate();frames-=bytes;nativeQueryScratchBytes_=previous+frames;};
        q.keyDown=[&](std::int32_t selected){return RootInputKey(root,selected);};
        q.writeKeyDown=[&](std::int32_t selected,std::uint8_t value){
            if(selected<0||selected>=255)throw std::runtime_error("Original Root selected key store exceeds its declared array");
            execution.WriteReference(ReferenceFor(root,RootInputKeyMap(root),static_cast<std::uint32_t>(selected),false),Value::Byte(value));};
        q.mouseReferences=[&]{return KeyboardWindowScalar(root,"Extension.RootWindow.handleMouseRef",Kind::Int,
            "Extension.RootWindow","IntProperty","",0u).integer;};
        q.keyboardReferences=[&]{return KeyboardWindowScalar(root,"Extension.RootWindow.handleKeyboardRef",Kind::Int,
            "Extension.RootWindow","IntProperty","",0u).integer;};
        q.hasParentPawn=[&]{return RootInputParentPawn(root)!=nullptr;};
        q.clearParentFire=[&]{ClearRootParentButton(root,"Engine.Pawn.bFire",execution);};
        q.clearParentAltFire=[&]{ClearRootParentButton(root,"Engine.Pawn.bAltFire",execution);};
        q.handleVirtualKeyboard=[&](std::int32_t selected,std::int32_t state,bool duplicate){
            static_cast<void>(HandleNativeRootVirtualKeyboard(root,selected,state,duplicate,execution));};
        q.handleButtons=[&](std::int32_t,std::int32_t){
            if(rootInputObservation_) {
                auto* pawn=RootInputParentPawn(root);
                rootInputObservation_->mouseFireCleared=pawn&&Read(pawn,DescribeProperty("Engine.Pawn.bFire"),0u).integer==0&&
                    Read(pawn,DescribeProperty("Engine.Pawn.bAltFire"),0u).integer==0;
            }
            throw std::runtime_error("Original Root mouse-button handler is not prepared");
        };
        return QuestVr::ProcessOriginalRootInput(key,action,delta,q);
    }
    Value ProbeNativeRootInput(const std::string& levelInfoPath,PortableRootInputProbeMode mode,
        PortableRootInputProbeResult& observed,QuestVr::Vm::Execution& execution) {
        using Mode=PortableRootInputProbeMode;
        switch(mode){case Mode::RoundTrip:case Mode::LateFailure:case Mode::Navigation:case Mode::MouseGrab:case Mode::InvalidKey:
            case Mode::FocusChild:case Mode::DestroyedRoot:break;
            default:throw std::runtime_error("Unknown original Root input diagnostic mode");}
        auto* outer=Object(levelInfoPath);RequireMapActor(outer);
        if(persistentWorldActors.empty()||persistentWorldActors.front()!=outer||NativeLevelInfo(outer)!=outer||
            QuestVr::Vm::ToBool(Read(outer,DescribeProperty("Engine.LevelInfo.bBegunPlay"),0u)))
            throw std::runtime_error("Original Root input diagnostic requires the dormant slot-zero Training LevelInfo");
        constexpr std::size_t observationBytes=65536u;
        ValidateStateBudget(observationBytes);nativeActorOrderObservationBytes_+=observationBytes;
        RuntimeObject* start{};
        for(auto* candidate:persistentWorldActors){LifecycleStep();if(!candidate||!candidate->active||candidate->worldRemoved)continue;
            RequireMapActor(candidate);if(IsDerivedFromPath(candidate->cls,"Engine.PlayerStart")){start=candidate;break;}}
        if(!start)throw std::runtime_error("Original Root input diagnostic lacks the actual Training PlayerStart");
        const auto spawn=Member(outer,"Engine.Actor.Spawn");
        if(!spawn)throw std::runtime_error("Original Root input diagnostic lacks original Actor.Spawn");
        const auto born=execution.CallFunction(*spawn,outer->reflection.objectPath,{
            {Value::Text(Kind::Object,"DeusEx.JCDentonMale"),{}},{Value::Text(Kind::Object,{}),{}},{Value::Text(Kind::Name,{}),{}},
            {Read(start,DescribeProperty("Engine.Actor.Location"),0u),{}},{Read(start,DescribeProperty("Engine.Actor.Rotation"),0u),{}}});
        if(born.kind!=Kind::Object||born.text.empty())throw std::runtime_error("Actual Root input player Spawn returned None");
        auto* pawn=Object(born.text);RequireMapActor(pawn);
        if(!pawn->runtimeBirth||pawn->cls!=Object("DeusEx.JCDentonMale")||!pawn->nativePlayerFlagBaseBound||pawn->nativeRootWindow)
            throw std::runtime_error("Original Root input player lacks actual constructor/FlagBase authority");
        observed.pawnPath=pawn->reflection.objectPath;
        const auto init=Member(pawn,"Extension.PlayerPawnExt.InitRootWindow");
        if(!init)throw std::runtime_error("Original Root input player lacks original InitRootWindow");
        static_cast<void>(execution.CallFunction(*init,pawn->reflection.objectPath,{}));
        auto* root=pawn->nativeRootWindow;RequireRootInputOwner(root);observed.rootPath=root->reflection.objectPath;
        if(RootInputParentPawn(root)!=pawn)throw std::runtime_error("Original Root input constructed a foreign parentPawn binding");
        if(rootInputObservation_)throw std::runtime_error("Original Root input observation cannot nest");
        rootInputObservation_=&observed;
        struct Clear {PortableRootInputProbeResult*& pointer;~Clear(){pointer=nullptr;}} clear{rootInputObservation_};
        observed.writesBeforeInput=execution.WriteCount();
        if(mode==Mode::FocusChild){auto* child=WindowLink(root,"firstChild");
            if(!child||NativeWindowRoot(child)!=root)throw std::runtime_error("Focused Root diagnostic lacks its actual configured child");
            // Explicit diagnostic FocusWindow state; NOT a fake implementation
            // or acceptance claim for the still-separate SetFocusWindow service.
            NativeWindowWrite(root,"Extension.RootWindow.FocusWindow",Value::Text(Kind::Object,child->reflection.objectPath),execution);}
        const auto event=[&](std::int32_t key,std::int32_t action){const auto result=ProcessNativeRootInput(root,key,action,0.01f,execution);
            ++observed.events;return result;};
        if(mode==Mode::InvalidKey)static_cast<void>(event(255,1));
        // Explicit diagnostic grab states in ACTUAL original fields; not a
        // claim that the real menu automatically grabbed focus/input here.
        NativeWindowWrite(root,"Extension.RootWindow.handleKeyboardRef",Value::Integer(0),execution);
        NativeWindowWrite(root,"Extension.RootWindow.handleMouseRef",Value::Integer(0),execution);
        const bool press=event(65,1);observed.keyStates[0]=RootInputKey(root,65);
        const bool release=event(65,3);observed.keyStates[1]=RootInputKey(root,65);
        observed.declinedWithoutGrab=!press&&!release;
        NativeWindowWrite(root,"Extension.RootWindow.handleKeyboardRef",Value::Integer(1),execution);
        if(mode==Mode::Navigation)static_cast<void>(event(37,1)); // Real selected native1442 must not become None.
        if(mode==Mode::MouseGrab){
            NativeWindowWrite(root,"Extension.RootWindow.handleMouseRef",Value::Integer(1),execution);
            execution.WriteReference(ReferenceFor(pawn,DescribeProperty("Engine.Pawn.bFire"),0u,false),Value::Byte(1u));
            execution.WriteReference(ReferenceFor(pawn,DescribeProperty("Engine.Pawn.bAltFire"),0u,false),Value::Byte(255u));
            static_cast<void>(event(1,1));
        }
        if(mode==Mode::DestroyedRoot)
            NativeWindowWrite(root,"Extension.Window.bBeingDestroyed",Value::Bool(true),execution);
        const bool consumedPress=event(65,1);observed.keyStates[2]=RootInputKey(root,65);
        const bool consumedDuplicate=event(65,1);
        const bool consumedRelease=event(65,3);observed.keyStates[3]=RootInputKey(root,65);
        const auto raw=observed.rawCallbacks,virtuals=observed.virtualCallbacks;
        const bool duplicateRelease=event(65,3);
        observed.duplicateReleaseSkipped=duplicateRelease&&observed.rawCallbacks==raw&&observed.virtualCallbacks==virtuals;
        observed.consumedWithGrab=consumedPress&&consumedDuplicate&&consumedRelease;
        if(!event(65,2))throw std::runtime_error("Actual Root keyboard grab failed to consume Hold");
        if(observed.keyStates!=std::array<std::uint8_t,4u>{1u,0u,1u,0u}||!observed.declinedWithoutGrab||
            !observed.consumedWithGrab||!observed.duplicateReleaseSkipped||!observed.rawCallbacks||!observed.virtualCallbacks)
            throw std::runtime_error("Original Root input actual key/grab/event observations differ");
        observed.completed=true;
        if(mode==Mode::LateFailure)throw std::runtime_error("Original Root input late failure after actual callbacks");
        throw std::runtime_error("Original Root input diagnostic complete; rollback required");
    }
