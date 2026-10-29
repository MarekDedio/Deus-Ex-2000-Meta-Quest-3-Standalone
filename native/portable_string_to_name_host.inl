// Included inside PortableActorVmHost. No native DLL is loaded or executed.
    QuestVr::NativeScratchScope ReserveStringToNameDiagnosticScratch(std::size_t bytes) {
        return QuestVr::NativeScratchScope(nativeQueryScratchBytes_,bytes,
            [this](std::size_t amount){ValidateStateBudget(amount);});
    }
    std::shared_ptr<const Function> OriginalStringToNameDeclaration(const Function* supplied) {
        constexpr const char* path="Extension.ExtensionObject.StringToName";
        auto* method=Object(path);const auto authored=FunctionFor(method);
        const auto validate=[&](const Function& function) {
            if(function.path!=path||function.source!=method->sourcePath||function.flags!=0x401u||
                function.nativeIndex!=1024u||function.variables.size()!=2u||!method->script||
                method->outer!=Object("Extension.ExtensionObject")||method->script->baseField||
                method->script->nativeIndex!=1024u||method->script->functionFlags!=0x401u||
                method->script->logicalSize!=5u||method->script->bytecode.size()!=5u||
                function.bytecode!=method->script->bytecode||
                LowerAscii(Qualified(Table(method->sourcePath),Table(method->sourcePath).exports.at(method->exportIndex).ObjClass))!="core.function")
                throw std::runtime_error("StringToName lacks its original Extension native1024 UFunction");
            RuntimeBytecodeReader code(function.bytecode);
            for(std::size_t slot=0u;slot<2u;++slot) {
                const auto& parameter=function.variables[slot];auto* field=Object(parameter.key);
                const std::string name=slot==0u?"str":"ReturnValue";
                const auto flags=slot==0u?0x400880u:0x580u;
                if(parameter.key!=std::string(path)+'.'+name||parameter.name!=name||parameter.flags!=flags||
                    parameter.arrayDimension!=1u||parameter.zero.kind!=(slot==0u?Kind::String:Kind::Name)||
                    !field->property||field->outer!=method||field->property->flags!=flags||
                    field->property->type!=(slot==0u?"StrProperty":"NameProperty")||
                    (slot==0u&&(code.Byte()!=0x29u||
                        Qualified(Table(method->sourcePath),static_cast<std::int32_t>(code.Dword()))!=parameter.key)))
                    throw std::runtime_error("StringToName differs from its original NativeParm/result packet");
            }
        };
        validate(*authored);
        if(supplied) {
            validate(*supplied);
            if(supplied->source!=authored->source)
                throw std::runtime_error("StringToName supplied a foreign original declaration");
        }
        return authored;
    }
    Evaluation OriginalStringToNameNative(RuntimeObject* receiver,const std::vector<Evaluation>& arguments,
        const Function* supplied,QuestVr::Vm::Execution& execution) {
        static_cast<void>(execution); // Same active root; this native has no receiver writes/callbacks.
        if(!transaction_)throw std::runtime_error("StringToName requires its existing root execution");
        QuestVr::NativeScratchScope frame(nativeQueryScratchBytes_,4096u,
            [this](std::size_t bytes){ValidateStateBudget(bytes);});
        static_cast<void>(OriginalStringToNameDeclaration(supplied));
        if(!receiver||!receiver->cls||Object(receiver->reflection.objectPath)!=receiver||
            !IsDerivedFromPath(receiver->cls,"Extension.ExtensionObject"))
            throw std::runtime_error("StringToName receiver is not an actual ExtensionObject");
        if(receiver->defaultOwner&&(receiver->defaultOwner!=receiver->cls||receiver->cls->defaultObject!=receiver))
            throw std::runtime_error("StringToName default receiver lost its actual owning Class");
        if(arguments.size()!=1u)throw std::runtime_error("StringToName requires its original single String expression");
        // VM native dispatch owns initialized argument copies. Reject forged
        // reference/no-write packets rather than perform a hidden read callback.
        if(arguments[0u].reference||arguments[0u].disposition!=QuestVr::Vm::ResultDisposition::Written||
            arguments[0u].value.kind!=Kind::String)
            throw std::runtime_error("StringToName requires a copied initialized String expression");
        const auto& source=arguments[0u].value.text;
        using QuestVr::DecalStateDetail::Add;using QuestVr::DecalStateDetail::Multiply;
        QuestVr::NativeScratchScope transport(nativeQueryScratchBytes_,Add(4096u,Multiply(Add(source.size(),1u),12u)),
            [this](std::size_t bytes){ValidateStateBudget(bytes);});
        nativeWork_.Step(source.size());
        const auto decoded=QuestVr::UnrealString::Decode(source);
        QuestVr::OriginalStringToNameQueries queries;
        queries.work=[this](std::size_t amount){nativeWork_.Step(amount);};
        queries.error=[](char16_t unit,std::u16string_view) {
            // Concrete packed-launcher GError behavior is not established.
            // Explicitly refuse at that real provider, before any interning;
            // do not turn GError into a warning or fabricate a valid name.
            throw std::runtime_error("StringToName original GError provider required for invalid UTF16 unit "+
                std::to_string(static_cast<unsigned>(unit)));
        };
        return QuestVr::RunOriginalStringToName(decoded,queries,[&](std::u16string_view spelling) {
            const auto previous=nativeQueryScratchBytes_;
            struct Restore {std::size_t& value;std::size_t before;~Restore(){value=before;}} restore{nativeQueryScratchBytes_,previous};
            NameStringArenaStage arena(NameStringArenaStage::Work{
                [this](std::size_t units){nativeWork_.Step(units);},
                [this,previous](std::size_t bytes) {
                    nativeQueryScratchBytes_=previous;ValidateStateBudget(bytes);
                    nativeQueryScratchBytes_=QuestVr::DecalStateDetail::Add(previous,bytes);
                }
            },2u,NameStringArenaStage::Mode::IncrementalOriginalName);
            // Lexically valid input is ASCII, so its UTF16 CString length is
            // exactly its transport-byte prefix length, including embeddedNUL.
            const auto name=arena.AddOriginalName(std::string_view(source).substr(0u,spelling.size()));
            ValidateStateBudget(Add(sizeof(Evaluation),Multiply(Add(spelling.size(),1u),4u)));
            Evaluation result{Value::Text(Kind::Name,arena.Spelling(name)),{}};
            arena.Validate();
            const auto growth=arena.CommittedGrowthBytes();
            const auto nextCredit=Add(runtimeNameArenaCredit,growth);
            ValidateStateBudget(Add(growth,Add(sizeof(result),result.value.text.capacity()+1u)));
            // No allocation, work callback or throwing validation after this
            // process-lifetime publication. Never keep a detached name stage
            // alive through an arbitrary script callback/another interner.
            arena.Commit();runtimeNameArenaCredit=nextCredit;
            return result;
        });
    }
