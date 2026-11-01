// Included inside PortableActorVmHost. Extension1004C980 first finds the actual
// terminal Root, then checks the Key byte and reads that Root's live keyDownMap.
    Evaluation WindowKeyDownNative(RuntimeObject* window,const std::vector<Evaluation>& arguments,
        const Function* declaration) {
        static_cast<void>(WindowNativeDeclaration(1470u,declaration));RequireNativeWindow(window);
        if(arguments.size()!=1u || arguments.front().reference ||
            arguments.front().disposition!=QuestVr::Vm::ResultDisposition::Written ||
            arguments.front().value.kind!=Kind::Byte || arguments.front().value.integer<0 ||
            arguments.front().value.integer>255)
            throw std::runtime_error("IsKeyDown requires its single copied initialized EInputKey Byte");
        auto* root=NativeWindowRoot(window); // Original GetRoot runs even for Key255.
        const auto key=arguments.front().value.integer;
        if(!root || key<0 || key>=255) return {Value::Bool(false),{}};
        const auto property=DescribeProperty("Extension.RootWindow.keyDownMap");
        auto* metadata=Object(property.key);
        if(property.zero.kind!=Kind::Byte || property.arrayDimension!=255u || property.flags!=0u ||
            !OwnsProperty(root->cls,property) || metadata->outer!=Object("Extension.RootWindow") ||
            !metadata->property || metadata->property->type!="ByteProperty" || metadata->property->referencedType!=0)
            throw std::runtime_error("IsKeyDown lacks the actual Root keyDownMap Byte255 declaration");
        const auto value=Read(root,property,static_cast<std::uint32_t>(key));
        if(value.kind!=Kind::Byte) throw std::runtime_error("IsKeyDown Root key state is not Byte");
        return {Value::Bool(value.integer!=0),{}};
    }
    Value ProbeNativeWindowKeyState(const std::string& levelInfoPath,PortableWindowKeyStateProbeMode mode,
        PortableWindowKeyStateProbeResult& result,QuestVr::Vm::Execution& execution) {
        using Mode=PortableWindowKeyStateProbeMode;
        switch(mode) {
        case Mode::RoundTrip:case Mode::LateFailure:case Mode::Rootless:case Mode::WrongReceiver:
        case Mode::WrongArgument:case Mode::MalformedByte:case Mode::MissingArgument:case Mode::WrongDeclaration:
        case Mode::ForeignEnumField:case Mode::ParentCycle:case Mode::InvalidTerminalCast:break;
        default:throw std::runtime_error("Unknown native Window key-state diagnostic mode");
        }
        auto* outer=Object(levelInfoPath);RequireMapActor(outer);
        if(persistentWorldActors.empty() || persistentWorldActors.front()!=outer ||
            NativeLevelInfo(outer)!=outer ||
            QuestVr::Vm::ToBool(Read(outer,DescribeProperty("Engine.LevelInfo.bBegunPlay"),0u)))
            throw std::runtime_error("Window key-state diagnostic requires the actual dormant slot-zero LevelInfo");
        constexpr std::size_t observationBytes=16384u;
        ValidateStateBudget(observationBytes);nativeActorOrderObservationBytes_+=observationBytes;
        RuntimeObject* start{};
        for(std::size_t slot=0u;;++slot) {
            LifecycleStep();
            if(slot>=persistentWorldActors.size())break;
            auto* candidate=persistentWorldActors[slot];
            if(!candidate || !candidate->active || candidate->worldRemoved)continue;
            RequireMapActor(candidate);
            if(IsDerivedFromPath(candidate->cls,"Engine.PlayerStart")) {start=candidate;break;}
        }
        if(!start)throw std::runtime_error("Window key-state diagnostic has no actual first Training PlayerStart");
        const auto spawn=Member(outer,"Engine.Actor.Spawn");
        if(!spawn)throw std::runtime_error("Window key-state diagnostic lacks original native Actor.Spawn");
        // Real native construction, including PlayerPawnExt's FlagBase, stays
        // INSIDE this same rollback root. A committed native FlagBase cannot
        // be silently dropped by the still-unimplemented checkpoint archive.
        const auto born=execution.CallFunction(*spawn,outer->reflection.objectPath,{
            {Value::Text(Kind::Object,"DeusEx.JCDentonMale"),{}},{Value::Text(Kind::Object,{}),{}},
            {Value::Text(Kind::Name,{}),{}},
            {Read(start,DescribeProperty("Engine.Actor.Location"),0u),{}},
            {Read(start,DescribeProperty("Engine.Actor.Rotation"),0u),{}}});
        if(born.kind!=Kind::Object || born.text.empty())
            throw std::runtime_error("Window key-state actual dormant JCDenton Spawn returned None");
        auto* pawn=Object(born.text);RequireMapActor(pawn);
        if(pawn->cls!=Object("DeusEx.JCDentonMale") || !pawn->runtimeBirth || pawn->nativeRootWindow ||
            !pawn->nativePlayerFlagBaseBound || !pawn->nativePlayerFlagBase)
            throw std::runtime_error("Window key-state Spawn lost actual player/native FlagBase constructor authority");
        result.playerPath=pawn->reflection.objectPath;
        const auto method=WindowNativeDeclaration(1470u,nullptr);
        const auto query=[&](RuntimeObject* receiver,std::uint8_t key) {
            const auto value=execution.CallFunction(*method,receiver->reflection.objectPath,{{Value::Byte(key),{}}});
            if(value.kind!=Kind::Bool)throw std::runtime_error("Window key-state query did not return Bool");
            ++result.queries;return value.boolean;
        };
        if(mode==Mode::Rootless || mode==Mode::InvalidTerminalCast) {
            // Genuine, isolated original base Window constructors. They do not
            // fabricate a Root or replace the configured-root path below.
            auto* parent=ConstructWindowObject(DefaultClass(Object("Extension.Window")),pawn,execution);
            InitializeChildNativeWindowStorage(parent,"extension.window");InitializeBaseWindow(parent,nullptr,execution);
            const auto init=Member(parent,"InitWindow");
            if(!init)throw std::runtime_error("Rootless key-state fixture has no original InitWindow");
            static_cast<void>(execution.CallFunction(*init,parent->reflection.objectPath,{}));
            const auto newChild=NewChildWindowDeclaration(nullptr);
            const auto born=execution.CallFunction(*newChild,parent->reflection.objectPath,
                {{Value::Text(Kind::Object,"Extension.Window"),{}},{Value::Bool(false),{}}});
            if(born.kind!=Kind::Object || born.text.empty())throw std::runtime_error("Rootless key-state child is None");
            auto* child=Object(born.text);result.rootPath=parent->reflection.objectPath;result.childPath=child->reflection.objectPath;
            if(mode==Mode::InvalidTerminalCast) {
                NativeWindowWrite(parent,"Extension.Window.windowType",Value::Byte(3u),execution);
                static_cast<void>(query(child,255u)); // Cast must refuse BEFORE key255 short-circuit.
            }
            for(const auto key:{std::uint8_t{0u},std::uint8_t{16u},std::uint8_t{254u},std::uint8_t{255u}})
                if(query(parent,key) || query(child,key))throw std::runtime_error("Rootless IsKeyDown fabricated a Root");
            result.key255False=true;result.completed=true;
            throw std::runtime_error("Window key-state rootless diagnostic complete; rollback required");
        }
        const auto init=Member(pawn,"Extension.PlayerPawnExt.InitRootWindow");
        if(!init)throw std::runtime_error("Window key-state fixture lacks original InitRootWindow");
        static_cast<void>(execution.CallFunction(*init,pawn->reflection.objectPath,{}));
        auto* root=pawn->nativeRootWindow;
        if(!root || !root->nativeRootWindowObject || NativeWindowRoot(root)!=root)
            throw std::runtime_error("Window key-state fixture did not construct the configured actual Root");
        auto* child=WindowLink(root,"firstChild");
        if(!child || NativeWindowRoot(child)!=root)throw std::runtime_error("Configured Root lacks an actual native child");
        result.rootPath=root->reflection.objectPath;result.childPath=child->reflection.objectPath;
        result.writesBeforeQueries=execution.WriteCount();
        if(mode==Mode::WrongReceiver)static_cast<void>(query(pawn,16u));
        if(mode==Mode::WrongArgument)
            static_cast<void>(execution.CallFunction(*method,child->reflection.objectPath,{{Value::Integer(16),{}}}));
        if(mode==Mode::MalformedByte) {
            auto malformed=Value::Byte(0u);malformed.integer=256;
            static_cast<void>(execution.CallFunction(*method,child->reflection.objectPath,{{malformed,{}}}));
        }
        if(mode==Mode::MissingArgument)static_cast<void>(execution.CallFunction(*method,child->reflection.objectPath,{}));
        if(mode==Mode::WrongDeclaration || mode==Mode::ForeignEnumField) {
            Function malformed=*method;
            if(mode==Mode::WrongDeclaration)malformed.flags^=0x1u;
            else malformed.variables.front().key="Extension.Window.SetBackgroundStyle.NewStyle";
            // Direct declaration-authentication control. The foreign field is
            // an actual Byte parameter with EDrawStyle, not an EInputKey alias.
            static_cast<void>(WindowKeyDownNative(child,{{Value::Byte(16u),{}}},&malformed));
        }
        if(mode==Mode::ParentCycle) {
            WindowLinkWrite(root,"winParent",child,execution);
            static_cast<void>(query(child,255u)); // Still walks ancestry before range check.
        }
        const auto property=DescribeProperty("Extension.RootWindow.keyDownMap");
        for(std::uint32_t key=0u;key<255u;++key) {
            const auto byte=static_cast<std::uint8_t>(key);
            const bool a=query(root,byte),b=query(child,byte);
            if(a!=b || a)throw std::runtime_error("Constructed Root keyDownMap was not cleared or child chose a different Root");
            result.cleared[key]=a;
        }
        for(std::uint32_t key=0u;key<255u;++key) {
            const auto value=key%3u==0u?0u:key%3u==1u?1u:255u;
            execution.WriteReference(ReferenceFor(root,property,key,false),Value::Byte(static_cast<std::uint8_t>(value)));
            const bool a=query(root,static_cast<std::uint8_t>(key)),b=query(child,static_cast<std::uint8_t>(key));
            if(a!=b || a!=(value!=0u))throw std::runtime_error("IsKeyDown did not read the actual live Root byte");
            result.written[key]=a;
        }
        constexpr std::array<std::uint8_t,7u> keys{0u,1u,16u,17u,18u,128u,254u};
        for(std::size_t slot=0u;slot<keys.size();++slot) {
            const auto key=keys[slot];const bool expected=!result.written[key];
            execution.WriteReference(ReferenceFor(root,property,key,false),Value::Byte(expected?2u:0u));
            const bool a=query(root,key),b=query(child,key);
            if(a!=b || a!=expected)throw std::runtime_error("IsKeyDown returned a stale/cached Root key byte");
            result.changed[slot]=a;
        }
        const auto unwritten=std::make_shared<Reference>();unwritten->zero=Value::Byte(255u);
        unwritten->read=[]()->Value {throw std::runtime_error("IsKeyDown reloaded an unwritten stale alias");};
        for(const auto disposition:{QuestVr::Vm::ResultDisposition::NoWrite,QuestVr::Vm::ResultDisposition::ZeroFill}) {
            const auto value=execution.CallFunction(*method,child->reflection.objectPath,
                {{Value::Byte(255u),unwritten,disposition,1u}});
            if(value.kind!=Kind::Bool || value.boolean!=result.changed[0u])
                throw std::runtime_error("IsKeyDown did not consume its original initialized Byte0 temporary");
            ++result.queries;
        }
        const auto omitted=execution.CallFunction(*method,child->reflection.objectPath,{{Value{}, {}}});
        if(omitted.kind!=Kind::Bool || omitted.boolean!=result.changed[0u])
            throw std::runtime_error("IsKeyDown explicit Nothing did not preserve its initialized Byte0 temporary");
        ++result.queries;
        std::size_t reads{};const auto live=std::make_shared<Reference>();live->zero=Value::Byte(0u);
        live->read=[&reads] {return Value::Byte(++reads==1u?1u:0u);};
        const auto copied=execution.CallFunction(*method,child->reflection.objectPath,{{Value::Byte(0u),live}});
        if(copied.kind!=Kind::Bool || copied.boolean!=result.changed[1u] || reads!=1u)
            throw std::runtime_error("IsKeyDown did not snapshot its actual Byte input exactly once");
        ++result.queries;
        result.key255False=!query(root,255u)&&!query(child,255u);
        if(!result.key255False)throw std::runtime_error("IsKeyDown read outside255 Root key slots");
        // The actual traversal uses terminal ancestry, not the nearest Root or
        // cached pawn.RootWindow. Type<3 legitimately returns None even though
        // the terminal object's runtime class is RootWindow.
        NativeWindowWrite(root,"Extension.Window.windowType",Value::Byte(2u),execution);
        const bool noRoot=!query(child,0u);
        NativeWindowWrite(root,"Extension.Window.windowType",Value::Byte(3u),execution);
        result.terminalNotNearest=noRoot && query(child,0u)==result.changed[0u];
        if(!result.terminalNotNearest)throw std::runtime_error("IsKeyDown ignored live terminal Root type");
        result.completed=true;
        if(mode==Mode::LateFailure)throw std::runtime_error("Window key-state late failure after actual Root queries");
        throw std::runtime_error("Window key-state diagnostic complete; rollback required");
    }
