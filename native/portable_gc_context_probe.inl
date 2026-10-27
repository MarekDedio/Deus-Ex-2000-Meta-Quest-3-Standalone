// Included inside PortableActorVmHost. Explicit original base-Root fixture;
// authentic native ingress in one always-rolled-back caller Execution.
    bool NativeGcProbeSameValue(const Value& left,const Value& right) {
        LifecycleStep();
        if(left.kind!=right.kind)return false;
        if(left.kind==Kind::Float)
            return std::bit_cast<std::uint32_t>(left.floating)==std::bit_cast<std::uint32_t>(right.floating);
        if(left.kind==Kind::Struct) {
            if(left.fields.size()!=right.fields.size())return false;
            auto a=left.fields.begin(),b=right.fields.begin();
            for(;a!=left.fields.end();++a,++b)
                if(a->first!=b->first || !NativeGcProbeSameValue(a->second,b->second))return false;
            return true;
        }
        return QuestVr::Vm::Equal(left,right);
    }
    Value ProbeNativeGcContext(const std::string& playerPath,const PortableGcContextProbeMode mode,
        PortableGcContextProbeResult& result,QuestVr::Vm::Execution& execution) {
        switch(mode) {
        case PortableGcContextProbeMode::RoundTrip:case PortableGcContextProbeMode::NoneCopy:
        case PortableGcContextProbeMode::WrongReceiver:case PortableGcContextProbeMode::WrongArgument:
        case PortableGcContextProbeMode::MissingArgument:case PortableGcContextProbeMode::CyclicGraph:
        case PortableGcContextProbeMode::NoWriteCopy:case PortableGcContextProbeMode::LateFailure:break;
        default:throw std::runtime_error("Unknown native GC context diagnostic mode");
        }
        auto* pawn=Object(playerPath);RequireMapActor(pawn);
        if(!IsDerivedFromPath(pawn->cls,"Extension.PlayerPawnExt") || pawn->nativeRootWindow)
            throw std::runtime_error("Native GC diagnostic requires an actual uninitialized player");
        // Fixed peak reservation covers the bounded context/parameter/value
        // observations below. It is not a raised live-state or snapshot cap.
        constexpr std::size_t snapshotBytes=64u<<10u;
        ValidateStateBudget(snapshotBytes);nativeActorOrderObservationBytes_+=snapshotBytes;
        auto* root=ConstructNativeRootWindow(pawn,DefaultClass(Object("Extension.RootWindow")),execution);
        const auto init=Member(root,"InitWindow");
        if(!init)throw std::runtime_error("Native GC fixture base Root has no original InitWindow");
        static_cast<void>(execution.CallFunction(*init,root->reflection.objectPath,{}));
        const auto context=[&](const char* key) {
            const auto value=Read(root,DescribeProperty(key),0u);
            if(value.kind!=Kind::Object || value.text.empty())
                throw std::runtime_error("Native GC fixture lost a constructed Root context");
            auto* object=Object(value.text);ValidateNativeGcContextFields(object);return object;
        };
        auto* drawing=context("Extension.Window.wGc");auto* general=context("Extension.Window.gGc");
        result.rootPath=root->reflection.objectPath;result.drawingPath=drawing->reflection.objectPath;
        result.generalPath=general->reflection.objectPath;
        if(root==drawing || root==general || drawing==general)
            throw std::runtime_error("Native GC fixture does not own distinct original identities");
        const auto invoke=[&](std::uint16_t index,RuntimeObject* receiver,const std::vector<Evaluation>& arguments={}) {
            const auto function=NativeGcDeclaration(index,nullptr);
            return execution.CallFunction(*function,receiver->reflection.objectPath,arguments);
        };
        const auto objectArgument=[](RuntimeObject* object) {
            return Evaluation{Value::Text(Kind::Object,object?object->reflection.objectPath:std::string{}),{}};
        };
        const auto expect=[&](bool condition,const char* message) {
            LifecycleStep();if(!condition)throw std::runtime_error(message);
        };
        const auto readCount=[&](RuntimeObject* object) {
            const auto value=NativeGcRead(object,"gcCount");
            expect(value.kind==Kind::Int,"Native GC diagnostic count lost its Int kind");return value.integer;
        };
        const auto readFreeFlag=[&](RuntimeObject* object) {
            const auto value=NativeGcRead(object,"bFree");
            expect(value.kind==Kind::Bool,"Native GC diagnostic free flag lost its Bool kind");return value.boolean;
        };
        const auto protocol=[&](RuntimeObject* object) {
            std::array<Value,5u> values;std::size_t slot{};
            for(const auto* name:{"bFree","gcCount","gcStack","gcFree","gcOwner"})values[slot++]=NativeGcRead(object,name);
            return values;
        };
        const auto sameProtocol=[&](RuntimeObject* object,const std::array<Value,5u>& expected) {
            const auto actual=protocol(object);
            for(std::size_t slot=0u;slot<expected.size();++slot)
                expect(NativeGcProbeSameValue(actual[slot],expected[slot]),"CopyGC changed destination protocol fields");
        };
        if(mode==PortableGcContextProbeMode::NoneCopy)
            static_cast<void>(invoke(1270u,general,{objectArgument(nullptr)}));
        else if(mode==PortableGcContextProbeMode::WrongReceiver)
            static_cast<void>(invoke(1271u,root));
        else if(mode==PortableGcContextProbeMode::WrongArgument)
            static_cast<void>(invoke(1270u,general,{{Value::Integer(1),{}}}));
        else if(mode==PortableGcContextProbeMode::MissingArgument)
            static_cast<void>(invoke(1270u,general));
        else if(mode==PortableGcContextProbeMode::NoWriteCopy) {
            auto input=objectArgument(drawing);input.disposition=QuestVr::Vm::ResultDisposition::NoWrite;
            static_cast<void>(invoke(1270u,general,{std::move(input)}));
        } else if(mode==PortableGcContextProbeMode::CyclicGraph) {
            static_cast<void>(invoke(1271u,general));
            auto* node=NativeGcLink(general,"gcStack");
            expect(node!=nullptr,"Native GC cycle control did not create a saved context");
            // Explicit invalid fixture, followed immediately by real ingress.
            NativeGcWrite(node,"gcStack",Value::Text(Kind::Object,node->reflection.objectPath),execution);
            static_cast<void>(invoke(1272u,general));
        }
        if(mode!=PortableGcContextProbeMode::RoundTrip && mode!=PortableGcContextProbeMode::LateFailure)
            throw std::runtime_error("Native GC context diagnostic guard unexpectedly accepted invalid input");

        // Deliberately inconsistent raw context data: no public style/color
        // setters can be used to construct or normalize this CopyGC fixture.
        const std::array<std::uint32_t,7u> fixtureWords{
            0x80000000u,0x3f800000u,0xbf800000u,0x7f800000u,0xff800000u,0x7fc01234u,0x7f801234u};
        std::array<Value,27u> expected;
        std::size_t fieldIndex{};
        for(const auto* name:QuestVr::NativeGcContext::ContextFields) {
            auto value=NativeGcRead(drawing,name);
            if(value.kind==Kind::Object) {
                const std::string_view field=name;
                value=Value::Text(Kind::Object,field=="normalFont"||field=="boldFont"?"Extension.TechMedium":
                    field=="underlineTexture"?"Extension.Solid":std::string{});
            } else if(value.kind==Kind::Struct) {
                std::size_t channel{};
                for(auto& [key,member]:value.fields) {
                    static_cast<void>(key);
                    if(member.kind==Kind::Float)member=Value::Float(std::bit_cast<float>(fixtureWords[(fieldIndex+channel)%fixtureWords.size()]));
                    else if(member.kind==Kind::Byte)member=Value::Byte(static_cast<std::uint8_t>((fieldIndex*31u+channel*67u)&255u));
                    else throw std::runtime_error("Native GC context fixture found an unexpected struct member kind");
                    ++channel;
                }
            } else if(value.kind==Kind::Float)value=Value::Float(std::bit_cast<float>(fixtureWords[fieldIndex%fixtureWords.size()]));
            else if(value.kind==Kind::Int)value=Value::Integer(-123456789+static_cast<std::int32_t>(fieldIndex));
            else if(value.kind==Kind::Byte)value=Value::Byte(static_cast<std::uint8_t>(255u-fieldIndex));
            else if(value.kind==Kind::Bool)value=Value::Bool(fieldIndex%2u!=0u);
            else throw std::runtime_error("Native GC context fixture found an unexpected field kind");
            NativeGcWrite(drawing,name,value,execution);expected[fieldIndex++]=std::move(value);
        }
        const auto verifyContext=[&](RuntimeObject* object) {
            std::size_t slot{};
            for(const auto* name:QuestVr::NativeGcContext::ContextFields)
                expect(NativeGcProbeSameValue(NativeGcRead(object,name),expected[slot++]),
                    "Native CopyGC changed a raw semantic context value");
        };
        // Protocol differs on purpose and is not part of a context copy. These
        // explicit source fields are reset before ordinary stack integration.
        NativeGcWrite(drawing,"gcCount",Value::Integer(37),execution);
        NativeGcWrite(drawing,"gcOwner",Value::Text(Kind::Object,general->reflection.objectPath),execution);
        NativeGcWrite(drawing,"bFree",Value::Bool(true),execution);
        NativeGcWrite(general,"gcCount",Value::Integer(-91),execution);
        const auto drawingProtocol=protocol(drawing),generalProtocol=protocol(general);
        const auto copy=[&](RuntimeObject* destination,RuntimeObject* from) {
            const auto before=execution.WriteCount();
            static_cast<void>(invoke(1270u,destination,{objectArgument(from)}));
            expect(execution.WriteCount()==before+27u,"Native CopyGC skipped a semantic field store");
            verifyContext(destination);
        };
        copy(general,drawing);verifyContext(drawing);sameProtocol(general,generalProtocol);sameProtocol(drawing,drawingProtocol);
        ++result.contextCases;
        copy(general,general);sameProtocol(general,generalProtocol);++result.contextCases;

        const std::array<std::uint32_t,15u> rawWords{
            0u,0x80000000u,1u,0x80000001u,0x007fffffu,0x00800000u,0x3f800000u,0xbf800000u,
            0x7f7fffffu,0xff7fffffu,0x7f800000u,0xff800000u,0x7fc01234u,0x7f801234u,0xff801234u};
        for(const auto bits:rawWords) {
            for(const auto index:{1u,12u,14u,18u,19u,20u}) {
                auto& value=expected[index];
                if(value.kind==Kind::Float)value=Value::Float(std::bit_cast<float>(bits));
                else for(auto& [key,member]:value.fields) {
                    static_cast<void>(key);expect(member.kind==Kind::Float,"Raw GC copy fixture lost a Float member");
                    member=Value::Float(std::bit_cast<float>(bits));
                }
                NativeGcWrite(drawing,QuestVr::NativeGcContext::ContextFields[index],value,execution);
            }
            copy(general,drawing);verifyContext(drawing);sameProtocol(general,generalProtocol);++result.rawCases;
        }
        for(std::uint32_t byte=0u;byte<256u;++byte) {
            for(const auto index:{2u,21u,22u}) {
                expected[index]=Value::Byte(static_cast<std::uint8_t>((byte+index*73u)&255u));
                NativeGcWrite(drawing,QuestVr::NativeGcContext::ContextFields[index],expected[index],execution);
            }
            for(const auto index:{11u,13u}) {
                std::size_t channel{};
                for(auto& [key,member]:expected[index].fields) {
                    static_cast<void>(key);expect(member.kind==Kind::Byte,"GC Color fixture lost a Byte channel");
                    member=Value::Byte(static_cast<std::uint8_t>((byte+index*11u+channel++*67u)&255u));
                }
                NativeGcWrite(drawing,QuestVr::NativeGcContext::ContextFields[index],expected[index],execution);
            }
            copy(general,drawing);verifyContext(drawing);++result.contextCases;
        }
        for(const auto pattern:{0u,255u,85u,170u}) {
            std::size_t bit{};
            for(const auto index:{3u,4u,5u,6u,7u,8u,23u,24u}) {
                expected[index]=Value::Bool(((pattern>>bit++)&1u)!=0u);
                NativeGcWrite(drawing,QuestVr::NativeGcContext::ContextFields[index],expected[index],execution);
            }
            copy(general,drawing);verifyContext(drawing);++result.contextCases;
        }
        const auto verifyReference=[&](RuntimeObject* object,const char* name) {
            const auto value=NativeGcRead(object,name);
            expect(value.kind==Kind::Object,"GC reference-edge diagnostic lost its Object kind");
            const auto found=object->nativeWindowPropertyRoots.find(LowerAscii("Extension.GC."+std::string(name)));
            if(value.text.empty()) {
                expect(found==object->nativeWindowPropertyRoots.end(),"Native GC retained an allocated empty reference edge");
                ++result.referenceCases;return;
            }
            expect(found!=object->nativeWindowPropertyRoots.end(),"GC native property has no journaled reference-root slot");
            const auto slot=found->second.find(0u);
            expect(slot!=found->second.end(),"GC native property has no scalar reference-root entry");
            expect(slot->second.size()==(value.text.empty()?0u:1u) &&
                (value.text.empty()||slot->second.front()==Object(value.text)),"GC context value and real markable UObject edge disagree");
            ++result.referenceCases;
        };
        for(auto* object:{general,drawing})for(const auto* name:{"Canvas","normalFont","boldFont","underlineTexture"})
            verifyReference(object,name);
        const auto resetProtocol=[&](RuntimeObject* object) {
            NativeGcWrite(object,"gcCount",Value::Integer(1),execution);
            for(const auto* name:{"gcOwner","gcStack","gcFree"})NativeGcWrite(object,name,Value::Text(Kind::Object,{}),execution);
            NativeGcWrite(object,"bFree",Value::Bool(false),execution);
        };
        resetProtocol(drawing);resetProtocol(general);
        const auto initialObjects=persistentWindowDomain->get()->objects.size();
        const auto beforeFresh=execution.WriteCount();const auto fresh=invoke(1271u,general);
        result.freshPushWrites=execution.WriteCount()-beforeFresh;
        auto* first=NativeGcLink(general,"gcStack");
        expect(fresh.kind==Kind::Int&&fresh.integer==1&&first&&first->outer==general&&
            readCount(general)==2&&readCount(first)==1&&!readFreeFlag(first)&&NativeGcLink(first,"gcOwner")==general&&
            persistentWindowDomain->get()->objects.size()==initialObjects+1u,"Fresh native PushGC identity/count/ownership differs");
        result.firstSavedPath=first->reflection.objectPath;verifyContext(first);++result.stackCases;
        for(const auto* name:{"normalFont","boldFont","underlineTexture","gcOwner"})verifyReference(first,name);
        NativeGcWrite(general,"textVSpacing",Value::Float(123.25f),execution);
        const auto nested=invoke(1271u,general);auto* second=NativeGcLink(general,"gcStack");
        expect(nested.kind==Kind::Int&&nested.integer==2&&second&&second!=first&&second->outer==general&&
            NativeGcLink(second,"gcStack")==first&&readCount(second)==2&&readCount(general)==3,
            "Nested native PushGC did not save a distinct context");
        result.secondSavedPath=second->reflection.objectPath;result.nestedObjects=persistentWindowDomain->get()->objects.size();
        expect(result.nestedObjects==initialObjects+2u,"Native nested PushGC object count differs");++result.stackCases;
        static_cast<void>(invoke(1272u,general,{{Value::Integer(1),{}}}));
        expect(readCount(general)==1&&!NativeGcLink(general,"gcStack")&&NativeGcLink(general,"gcFree")==first&&
            NativeGcLink(first,"gcStack")==second&&readCount(first)==2&&readCount(second)==1&&
            readFreeFlag(first)&&readFreeFlag(second),"Nested native PopGC threshold/free-list restoration differs");
        verifyContext(general);++result.stackCases;
        const auto beforeReuse=execution.WriteCount();const auto reused=invoke(1271u,general);
        result.reusedPushWrites=execution.WriteCount()-beforeReuse;
        expect(reused.kind==Kind::Int&&reused.integer==1&&NativeGcLink(general,"gcStack")==first&&
            NativeGcLink(general,"gcFree")==second&&!readFreeFlag(first)&&result.reusedPushWrites==35u&&
            result.freshPushWrites>result.reusedPushWrites&&persistentWindowDomain->get()->objects.size()==result.nestedObjects,
            "Native PushGC did not reuse the real free context or changed its exact35 stores");++result.stackCases;
        static_cast<void>(invoke(1272u,general));verifyContext(general);
        expect(readCount(general)==1&&!NativeGcLink(general,"gcStack")&&NativeGcLink(general,"gcFree")==first,
            "Omitted native PopGC did not use its authentic-1 sentinel");++result.stackCases;
        static_cast<void>(invoke(1271u,general));
        static_cast<void>(invoke(1272u,general,{{Value::Integer(-1),{}}}));
        expect(readCount(general)==1&&!NativeGcLink(general,"gcStack"),"Explicit-1 PopGC differed from omitted argument");++result.stackCases;
        static_cast<void>(invoke(1271u,general));
        Evaluation noPop{Value::Integer(99),{}};noPop.disposition=QuestVr::Vm::ResultDisposition::NoWrite;
        static_cast<void>(invoke(1272u,general,{noPop}));
        expect(readCount(general)==1&&!NativeGcLink(general,"gcStack"),"NoWrite PopGC ignored original initialized-1 temporary");++result.stackCases;
        auto before=execution.WriteCount();const auto ownedCount=readCount(first);const auto owned=invoke(1271u,first);
        static_cast<void>(invoke(1272u,first,{{Value::Integer(0),{}}}));
        expect(owned.kind==Kind::Int&&owned.integer==ownedCount&&execution.WriteCount()==before,
            "Owner-bound native GC push/pop forwarded to its owner or stored fields");++result.stackCases;
        before=execution.WriteCount();static_cast<void>(invoke(1272u,general,{{Value::Integer(1),{}}}));
        static_cast<void>(invoke(1272u,general));
        expect(execution.WriteCount()==before&&readCount(general)==1,"Native GC empty/current threshold pop was not a no-op");++result.stackCases;

        NativeGcWrite(general,"textVSpacing",Value::Float(991.0f),execution);
        const auto acquired=invoke(1485u,root);
        expect(acquired.kind==Kind::Object&&acquired.text==general->reflection.objectPath&&readCount(general)==2,
            "Window.GetGC returned a saved node/default context rather than its actual general GC");
        verifyContext(general);static_cast<void>(invoke(1486u,root,{objectArgument(general)}));
        expect(readCount(general)==1&&NativeGcProbeSameValue(NativeGcRead(general,"textVSpacing"),Value::Float(991.0f)),
            "Window.ReleaseGC did not restore the exact acquired context");++result.windowCases;
        before=execution.WriteCount();static_cast<void>(invoke(1486u,root,{objectArgument(nullptr)}));
        Evaluation noRelease=objectArgument(general);noRelease.disposition=QuestVr::Vm::ResultDisposition::NoWrite;
        static_cast<void>(invoke(1486u,root,{noRelease}));
        expect(execution.WriteCount()==before,"None/no-result native Window.ReleaseGC was not a no-op");++result.windowCases;
        NativeWindowWrite(root,"Extension.Window.gGc",Value::Text(Kind::Object,{}),execution);
        before=execution.WriteCount();const auto absent=invoke(1485u,root);
        expect(absent.kind==Kind::Object&&absent.text.empty()&&execution.WriteCount()==before,
            "Window.GetGC with actual general None fabricated a context or stack mutation");++result.windowCases;
        NativeWindowWrite(root,"Extension.Window.gGc",Value::Text(Kind::Object,general->reflection.objectPath),execution);
        before=execution.WriteCount();static_cast<void>(invoke(1486u,root,{objectArgument(first)}));
        expect(execution.WriteCount()==before&&readCount(general)==1,
            "Window.ReleaseGC owner-bound argument incorrectly popped the window's general GC");++result.windowCases;
        ValidateNativeGcLists(general);ValidateNativeGcLists(drawing);
        for(auto* object:{general,first,second})for(const auto* name:{"gcStack","gcFree","gcOwner"})verifyReference(object,name);
        expect(persistentWindowDomain->get()->objects.size()==result.nestedObjects,
            "Native context reuse or Window acquisition created an additional GC identity");
        // Actual1483 ingress: Window equality is authoritative, including an
        // inconsistent drawing GC. No general-context or layout repair.
        const auto translucent=WindowNativeDeclaration(1483u,nullptr);
        const auto setTranslucent=[&](const std::vector<Evaluation>& arguments) {
            return execution.CallFunction(*translucent,root->reflection.objectPath,arguments);
        };
        const auto windowTextBefore=Read(root,DescribeProperty("Extension.Window.bTextTranslucent"),0u);
        std::array<Value,27u> drawBefore,generalBefore;std::size_t contextIndex{};
        for(const auto* name:QuestVr::NativeGcContext::ContextFields) {
            drawBefore[contextIndex]=NativeGcRead(drawing,name);
            generalBefore[contextIndex++]=NativeGcRead(general,name);
        }
        const auto guarded=[&](const auto& call) {
            const auto writes=execution.WriteCount();bool rejected{};
            try {call();} catch(const std::exception&) {rejected=true;}
            expect(rejected&&execution.WriteCount()==writes,
                "Text-translucency precondition guard accepted invalid ingress or wrote a prefix");
            ++result.windowCases;
        };
        // Direct precondition controls are NOT claimed as independent failed
        // VM roots; the existing whole-root budget cuts below cover rollback.
        guarded([&]{static_cast<void>(WindowTextTranslucencyNative(pawn,{},translucent.get(),execution));});
        guarded([&]{static_cast<void>(WindowTextTranslucencyNative(root,{{Value::Integer(1),{}}},translucent.get(),execution));});
        guarded([&]{static_cast<void>(WindowTextTranslucencyNative(root,
            {{Value::Bool(true),{}},{Value::Bool(false),{}}},translucent.get(),execution));});
        auto wrongTranslucent=*translucent;wrongTranslucent.flags=0u;
        guarded([&]{static_cast<void>(WindowTextTranslucencyNative(root,{},&wrongTranslucent,execution));});
        for(const bool present:{false,true})for(const bool old:{false,true})
            for(const bool requested:{false,true})for(const bool oldContext:{false,true}) {
            NativeWindowWrite(root,"Extension.Window.wGc",Value::Text(Kind::Object,
                present?drawing->reflection.objectPath:std::string{}),execution);
            NativeWindowWrite(root,"Extension.Window.bTextTranslucent",Value::Bool(old),execution);
            NativeGcWrite(drawing,"bTextTranslucent",Value::Bool(oldContext),execution);
            NativeGcWrite(drawing,"textPolyFlags",Value::Integer(0x5a5a5a5a),execution);
            const auto writes=execution.WriteCount();
            static_cast<void>(setTranslucent({{Value::Bool(requested),{}}}));
            const bool changed=old!=requested,changedContext=changed&&present;
            expect(execution.WriteCount()-writes==(changed?(present?3u:1u):0u),
                "Text translucency lost its equal/None/three-store protocol");
            expect(Read(root,DescribeProperty("Extension.Window.bTextTranslucent"),0u).boolean==requested,
                "Text translucency did not retain the actual Window bit");
            std::size_t slot{};
            for(const auto* name:QuestVr::NativeGcContext::ContextFields) {
                const std::string_view field=name;
                const auto expectedDrawing=field=="bTextTranslucent"?Value::Bool(changedContext?requested:oldContext):
                    field=="textPolyFlags"?Value::Integer(changedContext?(requested?0x804:0x802):0x5a5a5a5a):drawBefore[slot];
                expect(NativeGcProbeSameValue(NativeGcRead(drawing,name),expectedDrawing),
                    "Text translucency changed an unrelated drawing field or repaired equal input");
                expect(NativeGcProbeSameValue(NativeGcRead(general,name),generalBefore[slot++]),
                    "Text translucency changed the independent general GC");
            }
            ++result.windowCases;
            before=execution.WriteCount();static_cast<void>(setTranslucent({{Value::Bool(requested),{}}}));
            expect(execution.WriteCount()==before,"Repeated text translucency was not a true no-op");++result.windowCases;
        }
        NativeWindowWrite(root,"Extension.Window.wGc",Value::Text(Kind::Object,drawing->reflection.objectPath),execution);
        for(const unsigned omitted:{0u,1u,2u}) {
            NativeWindowWrite(root,"Extension.Window.bTextTranslucent",Value::Bool(false),execution);
            before=execution.WriteCount();
            if(!omitted)static_cast<void>(setTranslucent({}));
            else {
                Evaluation argument{omitted==1u?Value{}:Value::Bool(false),{}};
                if(omitted==2u)argument.disposition=QuestVr::Vm::ResultDisposition::NoWrite;
                static_cast<void>(setTranslucent({argument}));
            }
            expect(execution.WriteCount()==before+3u&&
                Read(root,DescribeProperty("Extension.Window.bTextTranslucent"),0u).boolean&&
                NativeGcRead(drawing,"bTextTranslucent").boolean&&NativeGcRead(drawing,"textPolyFlags").integer==0x804,
                "Omitted/Nothing/NoWrite text translucency lost its native INT1 seed");++result.windowCases;
        }
        NativeWindowWrite(root,"Extension.Window.bTextTranslucent",windowTextBefore,execution);
        contextIndex=0u;
        for(const auto* name:QuestVr::NativeGcContext::ContextFields) {
            if(std::string_view(name)=="bTextTranslucent"||std::string_view(name)=="textPolyFlags")
                NativeGcWrite(drawing,name,drawBefore[contextIndex],execution);
            ++contextIndex;
        }
        // Nonempty real-font integration, not only the startup log's empty
        // text branch. Set the explicit base fixture's actual inherited Font,
        // construct original TextWindow and call genuine query wrappers.
        const auto fontSetter=WindowNativeDeclaration(1477u,nullptr);
        static_cast<void>(execution.CallFunction(*fontSetter,root->reflection.objectPath,
            {{Value::Text(Kind::Object,"Extension.TechMedium"),{}}}));
        const auto newChild=NewChildWindowDeclaration(nullptr);
        const auto child=execution.CallFunction(*newChild,root->reflection.objectPath,
            {{Value::Text(Kind::Object,"Extension.TextWindow"),{}},{Value::Bool(false),{}}});
        expect(child.kind==Kind::Object&&!child.text.empty(),"Native real-font TextWindow construction returned None");
        auto* text=Object(child.text);RequireNativeTextWindowSetters(text);
        const auto margins=TextWindowSetterDeclaration(1557u,nullptr);
        static_cast<void>(execution.CallFunction(*margins,text->reflection.objectPath,
            {{Value::Float(0.0f),{}},{Value::Float(0.0f),{}}}));
        // Genuine Show1413, not an unconditional flag store. Root remains
        // actually hidden; effective-visible/focus/layout is a separate service.
        const auto show=WindowNativeDeclaration(1413u,nullptr);
        const auto setVisible=[&](const std::vector<Evaluation>& inputs) {
            return execution.CallFunction(*show,text->reflection.objectPath,inputs);
        };
        guarded([&]{static_cast<void>(WindowShowNative(pawn,{},show.get(),execution));});
        guarded([&]{static_cast<void>(WindowShowNative(text,{{Value::Integer(1),{}}},show.get(),execution));});
        guarded([&]{static_cast<void>(WindowShowNative(text,{{Value::Bool(true),{}},{Value::Bool(false),{}}},show.get(),execution));});
        auto wrongShow=*show;wrongShow.variables.front().flags=0x80u;
        guarded([&]{static_cast<void>(WindowShowNative(text,{},&wrongShow,execution));});
        for(const bool previous:{false,true})for(const bool requested:{false,true}) {
            NativeWindowWrite(text,"Extension.Window.bIsVisible",Value::Bool(previous),execution);
            for(const auto* name:{"bNeedsQuery","bNeedsReconfigure"})
                NativeWindowWrite(root,("Extension.Window."+std::string(name)).c_str(),Value::Bool(false),execution);
            before=execution.WriteCount();static_cast<void>(setVisible({{Value::Bool(requested),{}}}));
            const auto changed=previous!=requested;
            expect(execution.WriteCount()-before==(changed?(requested?1u:3u):0u)&&
                Read(text,DescribeProperty("Extension.Window.bIsVisible"),0u).boolean==requested,
                "Original Show changed its equal/hidden-parent show/hide store protocol");
            for(const auto* name:{"bNeedsQuery","bNeedsReconfigure"})
                expect(Read(root,DescribeProperty("Extension.Window."+std::string(name)),0u).boolean==(changed&&!requested),
                    "Original Show skipped or invented the actual modal hide-only reconfiguration");
            ++result.windowCases;
            before=execution.WriteCount();static_cast<void>(setVisible({{Value::Bool(requested),{}}}));
            expect(execution.WriteCount()==before,"Repeated Show notified its parent or wrote unchanged visibility");++result.windowCases;
        }
        for(const unsigned mode:{0u,1u,2u}) {
            NativeWindowWrite(text,"Extension.Window.bIsVisible",Value::Bool(false),execution);
            before=execution.WriteCount();
            if(mode==0u)static_cast<void>(setVisible({}));
            else {
                Evaluation input{mode==1u?Value{}:Value::Bool(false),{}};
                if(mode==2u)input.disposition=QuestVr::Vm::ResultDisposition::NoWrite;
                static_cast<void>(setVisible({input}));
            }
            expect(execution.WriteCount()==before+1u&&Read(text,DescribeProperty("Extension.Window.bIsVisible"),0u).boolean,
                "Show omission/Nothing/NoWrite lost its genuine INT1 default");++result.windowCases;
        }
        // A canonical equality must not even inspect a malformed parent edge.
        // Restore the explicit fixture immediately, before any changed request.
        WindowLinkWrite(text,"winParent",text,execution);before=execution.WriteCount();
        static_cast<void>(setVisible({{Value::Bool(true),{}}}));
        expect(execution.WriteCount()==before,"Equal Show inspected/notified an unrelated cyclic parent fixture");++result.windowCases;
        WindowLinkWrite(text,"winParent",root,execution);
        before=execution.WriteCount();static_cast<void>(execution.CallFunction(*show,root->reflection.objectPath,{{Value::Bool(false),{}}}));
        expect(execution.WriteCount()==before,"Equal unparented Show fabricated a visible-root service");++result.windowCases;
        const auto content=NativeTextContentDeclaration(nullptr);
        const auto setText=[&](const std::string& input) {
            return execution.CallFunction(*content,text->reflection.objectPath,{{Value::Text(Kind::String,input),{}}});
        };
        static_cast<void>(setText("A B"));
        const auto query=[&](std::uint16_t index,float constraint) {
            const auto function=WindowNativeDeclaration(index,nullptr);
            return execution.CallFunction(*function,text->reflection.objectPath,{{Value::Float(constraint),{}}});
        };
        const auto preferredWidth=query(1463u,-1.0f);
        expect(!nativeFontMetrics_.empty(),"Native nonempty query bypassed its cold actual Font metrics provider");
        const auto& metrics=NativeFontMetricsFor(Object("Extension.TechMedium"));
        const auto a=QuestVr::LookupNativeFontGlyph(metrics,u'A'),space=QuestVr::LookupNativeFontGlyph(metrics,u' '),
            b=QuestVr::LookupNativeFontGlyph(metrics,u'B');
        expect(preferredWidth.kind==Kind::Float&&preferredWidth.floating==static_cast<float>(a.width+space.width+b.width),
            "Nonempty native preferred width did not use actual Font glyph advances");++result.textCases;
        const auto preferredHeight=query(1464u,1024.0f);
        expect(preferredHeight.kind==Kind::Float&&preferredHeight.floating==
            static_cast<float>(std::max({a.height,space.height,b.height})+1),
            "Nonempty native preferred height did not use actual glyph heights and GC spacing");++result.textCases;
        const auto lines=TextWindowSetterDeclaration(1558u,nullptr);
        static_cast<void>(execution.CallFunction(*lines,text->reflection.objectPath,
            {{Value::Integer(2),{}},{Value::Integer(2),{}}}));
        const auto boundedHeight=query(1464u,1024.0f);
        expect(boundedHeight.kind==Kind::Float&&boundedHeight.floating==static_cast<float>(2*(space.height+1)),
            "Constrained wrapped native text did not use real space-glyph line bounds");++result.textCases;
        expect(!nativeFontMetrics_.empty(),"Native text query did not retain its admitted current Font metrics");
        const auto contentGuard=[&](const auto& call) {
            const auto writes=execution.WriteCount();bool refused{};
            try{call();}catch(const std::exception&){refused=true;}
            expect(refused&&execution.WriteCount()==writes,"Text content guard accepted invalid ingress or wrote a prefix");
        };
        contentGuard([&]{static_cast<void>(NativeTextContentMethod(root,{},content.get(),execution));});
        contentGuard([&]{static_cast<void>(NativeTextContentMethod(text,{},content.get(),execution));});
        contentGuard([&]{static_cast<void>(NativeTextContentMethod(text,{{Value::Integer(1),{}}},content.get(),execution));});
        auto wrongContent=*content;wrongContent.variables.front().flags=0x80u;
        contentGuard([&]{static_cast<void>(NativeTextContentMethod(text,
            {{Value::Text(Kind::String,"bad"),{}}},&wrongContent,execution));});
        struct ContentCase {const char* input;const char* expected;std::int32_t key;};
        for(const auto& item:std::array<ContentCase,13u>{
            ContentCase{"Alpha|nBeta","Alpha\nBeta",0},ContentCase{"ALPHA|nBETA","Alpha\nBeta",0},
            ContentCase{"one |&K two |&Z","one |&K two |&Z",75},ContentCase{"one |&K two |&Z","one |&K two |&Z",75},
            ContentCase{"none","none",0},ContentCase{"||&A","||&A",0},ContentCase{"||n","||n",0},
            ContentCase{"|N","|N",0},ContentCase{"|&\xc3\xbe","|&\xc3\xbe",254},
            ContentCase{"|&\xc3\xbf","|&\xc3\xbf",0},ContentCase{"\xce\xa9|n\xe4\xb8\xad","\xce\xa9\n\xe4\xb8\xad",0},
            ContentCase{"ends|","ends|",0},ContentCase{"","",0}}) {
            const auto previousText=Read(text,DescribeProperty("Extension.TextWindow.Text"),0u);
            const auto currentKey=Read(text,DescribeProperty("Extension.Window.acceleratorKey"),0u);
            const auto dirty=Read(root,DescribeProperty("Extension.ModalWindow.bDirtyAccelerators"),0u);
            before=execution.WriteCount();static_cast<void>(setText(item.input));
            const auto actual=Read(text,DescribeProperty("Extension.TextWindow.Text"),0u);
            const bool changed=actual.text!=previousText.text;
            expect(actual.kind==Kind::String&&actual.text==item.expected&&
                Read(text,DescribeProperty("Extension.Window.acceleratorKey"),0u).integer==item.key,
                "Authentic SetText changed string conversion/case retention/first accelerator key");
            const auto expectedWrites=changed?(currentKey.integer!=item.key?5u:4u):0u;
            expect(execution.WriteCount()-before==expectedWrites,
                "Authentic SetText changed text/key/dirty/layout store sequence");
            if(!changed)expect(NativeGcProbeSameValue(Read(root,DescribeProperty("Extension.ModalWindow.bDirtyAccelerators"),0u),dirty),
                "Equal SetText dirtied the actual modal");
            ++result.textCases;
        }
        NativeWindowWrite(text,"Extension.TextWindow.bTextIsAccelerator",Value::Bool(false),execution);
        NativeWindowWrite(text,"Extension.Window.acceleratorKey",Value::Integer(91),execution);
        before=execution.WriteCount();static_cast<void>(setText("|&Q"));
        expect(execution.WriteCount()==before+3u&&Read(text,DescribeProperty("Extension.Window.acceleratorKey"),0u).integer==91,
            "Disabled native text accelerator rewrote its key or omitted genuine layout");++result.textCases;
        // A NoWrite argument Steps into the wrapper's genuine empty FString.
        Evaluation noText{Value::Text(Kind::String,"must not survive"),{}};
        noText.disposition=QuestVr::Vm::ResultDisposition::NoWrite;
        static_cast<void>(execution.CallFunction(*content,text->reflection.objectPath,{noText}));
        expect(Read(text,DescribeProperty("Extension.TextWindow.Text"),0u).text.empty(),
            "NoWrite SetText retained a stale String instead of its empty native temporary");++result.textCases;
        const auto reconfigureDeclaration=WindowNativeDeclaration(1460u,nullptr);
        const auto reconfigureVisible=Read(text,DescribeProperty("Extension.Window.bIsVisible"),0u);
        for(const bool visible:{false,true})for(unsigned flags=0u;flags<4u;++flags) {
            NativeWindowWrite(text,"Extension.Window.bIsVisible",Value::Bool(visible),execution);
            NativeWindowWrite(text,"Extension.Window.bNeedsReconfigure",Value::Bool((flags&1u)!=0u),execution);
            NativeWindowWrite(text,"Extension.Window.bNeedsQuery",Value::Bool((flags&2u)!=0u),execution);
            const auto parentReconfigure=Read(root,DescribeProperty("Extension.Window.bNeedsReconfigure"),0u);
            const auto parentQuery=Read(root,DescribeProperty("Extension.Window.bNeedsQuery"),0u);
            before=execution.WriteCount();
            static_cast<void>(execution.CallFunction(*reconfigureDeclaration,text->reflection.objectPath,{}));
            expect(execution.WriteCount()==before+2u&&NativeWindowBool(text,"bNeedsReconfigure")&&NativeWindowBool(text,"bNeedsQuery"),
                "Native1460 skipped repeated dirty stores or entered layout below its hidden ancestor");
            expect(QuestVr::Vm::Equal(Read(root,DescribeProperty("Extension.Window.bNeedsReconfigure"),0u),parentReconfigure)&&
                QuestVr::Vm::Equal(Read(root,DescribeProperty("Extension.Window.bNeedsQuery"),0u),parentQuery),
                "Hidden native1460 dirtied its parent or invoked an unrelated layout callback");++result.windowCases;
        }
        NativeWindowWrite(text,"Extension.Window.bIsVisible",reconfigureVisible,execution);
        contentGuard([&]{static_cast<void>(WindowAskParentForReconfigureNative(drawing,{},reconfigureDeclaration.get(),execution));});
        contentGuard([&]{static_cast<void>(WindowAskParentForReconfigureNative(text,{{Value::Bool(false),{}}},
            reconfigureDeclaration.get(),execution));});
        auto forgedReconfigure=*reconfigureDeclaration;forgedReconfigure.flags^=0x10u;
        contentGuard([&]{static_cast<void>(WindowAskParentForReconfigureNative(text,{},&forgedReconfigure,execution));});
        // Original GC/font/string native packets, through the actual caller
        // Execution. Output aliases below are real reflected Float storage.
        auto* measured=Object(Read(text,DescribeProperty("Extension.Window.wGc"),0u).text);
        ValidateNativeGcContextFields(measured);
        const auto textNative=[&](std::uint16_t index,RuntimeObject* receiver,const std::vector<Evaluation>& inputs={}) {
            const auto function=NativeGcTextDeclaration(index,nullptr);
            return execution.CallFunction(*function,receiver->reflection.objectPath,inputs);
        };
        for(const unsigned form:{0u,1u,2u,3u}) {
            Evaluation font{Value::Text(Kind::Object,form==2u?std::string{}:"Extension.TechMedium"),{}};
            if(form==3u)font.disposition=QuestVr::Vm::ResultDisposition::NoWrite;
            before=execution.WriteCount();static_cast<void>(textNative(1242u,measured,{font}));
            const auto expected=form>=2u?std::string{}:"Extension.TechMedium";
            expect(execution.WriteCount()==before+2u&&NativeGcRead(measured,"normalFont").text==expected&&
                NativeGcRead(measured,"boldFont").text==expected,"GC.SetFont omitted duplicate/None stores or lost its Object seed");
            ++result.textCases;
        }
        NativeGcWrite(measured,"textVSpacing",Value::Float(1.0f),execution);
        for(const bool spacing:{false,true}) {
            const auto height=textNative(1281u,measured,{{Value::Bool(spacing),{}}});
            expect(height.kind==Kind::Float&&height.floating==(spacing?1.0f:0.0f),
                "GC.GetFontHeight fabricated glyph metrics for actual None fonts");++result.textCases;
        }
        const auto hMargin=DescribeProperty("Extension.TextWindow.hMargin"),vMargin=DescribeProperty("Extension.TextWindow.vMargin");
        const auto spacingProperty=DescribeProperty("Extension.GC.textVSpacing");
        auto xOutput=ReferenceFor(text,hMargin,0u,false),yOutput=ReferenceFor(text,vMargin,0u,false);
        auto sharedOutput=ReferenceFor(measured,spacingProperty,0u,false);
        before=execution.WriteCount();
        static_cast<void>(textNative(1280u,measured,{{Value::Float(1024.0f),{}},{Value::Float(0.0f),xOutput},
            {Value::Float(0.0f),yOutput},{Value::Text(Kind::String,"A B"),{}}}));
        expect(execution.WriteCount()==before+2u&&Read(text,hMargin,0u).floating==0.0f&&Read(text,vMargin,0u).floating==1.0f,
            "GC.GetTextExtent invented a fallback font or lost genuine no-font line spacing");++result.textCases;
        static_cast<void>(textNative(1242u,measured,{{Value::Text(Kind::Object,"Extension.TechMedium"),{}}}));
        for(const unsigned form:{0u,1u,2u,3u}) {
            std::vector<Evaluation> inputs;
            if(form<2u)inputs.push_back({Value::Bool(form==1u),{}});
            if(form==3u) {Evaluation input{Value::Bool(true),{}};input.disposition=QuestVr::Vm::ResultDisposition::NoWrite;inputs.push_back(input);}
            before=execution.WriteCount();const auto height=textNative(1281u,measured,inputs);
            expect(height.kind==Kind::Float&&height.floating==static_cast<float>(space.height+(form==1u?1:0))&&
                execution.WriteCount()==before,"GC.GetFontHeight changed its optional-false/space metric protocol");++result.textCases;
        }
        struct ConversionCase {const char* input;const char* expected;};
        for(const auto& item:std::array<ConversionCase,6u>{ConversionCase{"A|nB","A\nB"},ConversionCase{"||n","||n"},
            ConversionCase{"|N","|N"},ConversionCase{"tail|","tail|"},
            ConversionCase{"\xce\xa9|n\xf0\x9f\x98\x80","\xce\xa9\n\xf0\x9f\x98\x80"},ConversionCase{"",""}}) {
            before=execution.WriteCount();const auto converted=textNative(1469u,text,{{Value::Text(Kind::String,item.input),{}}});
            expect(converted.kind==Kind::String&&converted.text==item.expected&&execution.WriteCount()==before,
                "Native1469 conversion lost CString/bar/UTF16 ownership or mutated Window state");++result.textCases;
        }
        NativeWindowWrite(text,"Extension.TextWindow.Text",Value::Text(Kind::String,"One|nTwo"),execution);
        const auto privateText=DescribeProperty("Extension.TextWindow.Text");
        before=execution.WriteCount();const auto firstConversion=textNative(1469u,text,{{{},ReferenceFor(text,privateText,0u,false)}});
        static_cast<void>(textNative(1469u,text,{{Value::Text(Kind::String,"other"),{}}}));
        expect(firstConversion.text=="One\nTwo"&&Read(text,privateText,0u).text=="One|nTwo"&&execution.WriteCount()==before,
            "Native1469 returned mutable global storage or changed its caller input alias");++result.textCases;
        Evaluation noString{Value::Text(Kind::String,"stale"),{}};noString.disposition=QuestVr::Vm::ResultDisposition::NoWrite;
        const auto emptyConversion=textNative(1469u,text,{noString});
        expect(emptyConversion.kind==Kind::String&&emptyConversion.text.empty(),"Native1469 lost its initialized empty String");++result.textCases;
        const auto expectedX=static_cast<float>(a.width+space.width+b.width),expectedY=static_cast<float>(std::max({a.height,space.height,b.height})+1);
        for(const unsigned form:{0u,1u,2u,3u,4u}) {
            NativeGcWrite(measured,"textVSpacing",Value::Float(1.0f),execution);
            NativeWindowWrite(text,hMargin.key.c_str(),Value::Float(-11.0f),execution);
            NativeWindowWrite(text,vMargin.key.c_str(),Value::Float(-22.0f),execution);
            const auto x=form==1u?sharedOutput:form==2u||form==4u?nullptr:xOutput;
            const auto y=form==1u?sharedOutput:form==3u||form==4u?nullptr:yOutput;
            before=execution.WriteCount();static_cast<void>(textNative(1280u,measured,{{Value::Float(1024.0f),{}},
                {Value::Float(-91.0f),x},{Value::Float(-92.0f),y},{Value::Text(Kind::String,"A B"),{}}}));
            expect(execution.WriteCount()-before==static_cast<std::size_t>(!!x)+static_cast<std::size_t>(!!y),
                "GC text outputs were double-journaled or wrote discarded native temporaries");
            if(form==1u)expect(NativeGcRead(measured,"textVSpacing").floating==expectedY,
                "Aliased native GC outputs lost Y's last store or reread spacing after X");
            else {
                expect(Read(text,hMargin,0u).floating==(x?expectedX:-11.0f)&&Read(text,vMargin,0u).floating==(y?expectedY:-22.0f),
                    "GC.GetTextExtent lost real Font metrics, output aliases or discarded-result independence");
            }
            ++result.textCases;
        }
        NativeGcWrite(measured,"textVSpacing",Value::Float(1.0f),execution);
        static_cast<void>(textNative(1280u,measured,{{Value::Float(100.0f),{}},{Value::Float(1.0f),xOutput},
            {Value::Float(2.0f),yOutput},noString}));
        expect(Read(text,hMargin,0u).floating==0.0f&&Read(text,vMargin,0u).floating==0.0f,
            "GC.GetTextExtent read stale input after the empty FString seed");++result.textCases;
        for(const auto index:{1242u,1280u,1281u,1469u}) {
            const auto declaration=NativeGcTextDeclaration(static_cast<std::uint16_t>(index),nullptr);
            contentGuard([&]{static_cast<void>(NativeGcTextMethod(static_cast<std::uint16_t>(index),
                index==1469u?measured:root,{},declaration.get(),execution));});
            const auto receiver=index==1469u?text:measured;
            std::vector<Evaluation> valid;
            if(index==1242u)valid={{Value::Text(Kind::Object,"Extension.TechMedium"),{}}};
            else if(index==1281u)valid={{Value::Bool(false),{}}};
            else if(index==1469u)valid={{Value::Text(Kind::String,"A|nB"),{}}};
            else valid={{Value::Float(1024.0f),{}},{Value::Float(0.0f),xOutput},
                {Value::Float(0.0f),yOutput},{Value::Text(Kind::String,"A B"),{}}};
            auto forged=*declaration;forged.variables.front().flags^=0x10u;
            contentGuard([&]{static_cast<void>(NativeGcTextMethod(static_cast<std::uint16_t>(index),
                receiver,valid,&forged,execution));});
            auto wrongKind=valid;wrongKind.front()={Value::Integer(7),{}};
            contentGuard([&]{static_cast<void>(NativeGcTextMethod(static_cast<std::uint16_t>(index),
                receiver,wrongKind,declaration.get(),execution));});
            auto noWrite=valid;noWrite.front().disposition=QuestVr::Vm::ResultDisposition::NoWrite;
            contentGuard([&]{static_cast<void>(NativeGcTextMethod(static_cast<std::uint16_t>(index),
                receiver,noWrite,declaration.get(),execution));});
            auto excess=valid;excess.push_back({Value::Integer(0),{}});
            contentGuard([&]{static_cast<void>(NativeGcTextMethod(static_cast<std::uint16_t>(index),
                receiver,excess,declaration.get(),execution));});
            if(index!=1281u)contentGuard([&]{static_cast<void>(NativeGcTextMethod(static_cast<std::uint16_t>(index),
                receiver,{},declaration.get(),execution));});
        }
        const auto fontDeclaration=NativeGcTextDeclaration(1242u,nullptr);
        contentGuard([&]{static_cast<void>(NativeGcTextMethod(1242u,measured,
            {{Value::Text(Kind::Object,"Extension.Solid"),{}}},fontDeclaration.get(),execution));});
        const auto extentDeclaration=NativeGcTextDeclaration(1280u,nullptr);
        for(const auto slot:{1u,2u}) {
            std::vector<Evaluation> wrongOutput{{Value::Float(10.0f),{}},{Value::Float(0.0f),xOutput},
                {Value::Float(0.0f),yOutput},{Value::Text(Kind::String,"A"),{}}};
            wrongOutput[slot]={Value::Integer(0),{}};
            contentGuard([&]{static_cast<void>(NativeGcTextMethod(1280u,measured,wrongOutput,extentDeclaration.get(),execution));});
            wrongOutput[slot]={Value::Float(0.0f),ReferenceFor(text,privateText,0u,false)};
            contentGuard([&]{static_cast<void>(NativeGcTextMethod(1280u,measured,wrongOutput,extentDeclaration.get(),execution));});
        }
        if(mode==PortableGcContextProbeMode::LateFailure)
            throw std::runtime_error("Native GC context diagnostic forced late failure");
        throw std::runtime_error("Native GC context diagnostic complete; rollback required");
    }
