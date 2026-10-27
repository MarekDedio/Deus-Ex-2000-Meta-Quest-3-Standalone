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
        NativeWindowWrite(text,"Extension.TextWindow.Text",Value::Text(Kind::String,"A B"),execution);
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
        if(mode==PortableGcContextProbeMode::LateFailure)
            throw std::runtime_error("Native GC context diagnostic forced late failure");
        throw std::runtime_error("Native GC context diagnostic complete; rollback required");
    }
