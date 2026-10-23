// Private same-root player-action diagnostic. The target is an ACTUAL damage
// birth, not a spawned grant or a substitute for controller reach/occlusion.
    std::shared_ptr<const Function> StartupLootInputFunction(RuntimeObject* player) {
        const auto identity=Member(player,"ParseRightClick");
        if(!identity)throw std::runtime_error("Startup loot player has no selected ParseRightClick");
        const auto function=FunctionFor(Object(identity->path));auto* node=Object(function->path);
        if(function->path!="DeusEx.DeusExPlayer.ParseRightClick" || function->nativeIndex || function->flags!=0x2c2u ||
            !node->script || node->script->nativeIndex || node->script->functionFlags!=function->flags ||
            node->outer!=Object("DeusEx.DeusExPlayer") || function->source!=node->sourcePath ||
            LowerAscii(Qualified(Table(node->sourcePath),Table(node->sourcePath).exports.at(node->exportIndex).ObjClass))!="core.function")
            throw std::runtime_error("Startup loot player action lacks its actual original ParseRightClick declaration");
        for(const auto& variable:function->variables)if(variable.flags&0x80u)
            throw std::runtime_error("Original ParseRightClick must have no parameters or result");
        return function;
    }
    Property StartupLootInputProperty(RuntimeObject* player,const char* key,Kind kind,
        std::uint32_t flags,std::uint32_t dimension,const char* constraint="") {
        const auto property=DescribeProperty(key);auto* node=Object(property.key);
        const char* metaclass=kind==Kind::Object?"core.objectproperty":kind==Kind::Byte?"core.byteproperty":"core.intproperty";
        if(property.key!=key || !OwnsProperty(player->cls,property) || property.zero.kind!=kind ||
            property.flags!=flags || property.arrayDimension!=dimension || !node->property || node->property->baseField ||
            node->outer!=Object("DeusEx.DeusExPlayer") ||
            LowerAscii(Qualified(Table(node->sourcePath),Table(node->sourcePath).exports.at(node->exportIndex).ObjClass))!=metaclass ||
            (*constraint?LowerAscii(Qualified(Table(node->sourcePath),node->property->referencedType))!=LowerAscii(constraint):
                node->property->referencedType!=0))
            throw std::runtime_error(std::string("Startup player loot field lacks its actual declaring schema: ")+key);
        return property;
    }
    PortableStartupLootGrid StartupLootInputGrid(RuntimeObject* player) {
        // Exact original fixed Byte grid. It is not a renderer inventory list
        // or a manually packed replacement for PlaceItemInSlot/SetInvSlots.
        const auto field=StartupLootInputProperty(player,"DeusEx.DeusExPlayer.invSlots",Kind::Byte,0x10000u,30u);
        PortableStartupLootGrid result;
        const auto integer=[&](const char* key) {
            const auto value=Read(player,StartupLootInputProperty(player,key,Kind::Int,0u,1u),0u);
            if(value.kind!=Kind::Int)throw std::runtime_error("Original player inventory dimension is not Int");
            return value.integer;
        };
        result.rows=integer("DeusEx.DeusExPlayer.maxInvRows");result.columns=integer("DeusEx.DeusExPlayer.maxInvCols");
        if(result.rows<=0 || result.columns<=0 ||
            std::int64_t{result.rows}*result.columns>static_cast<std::int64_t>(field.arrayDimension))
            throw std::runtime_error("Original inventory dimensions exceed the bounded fixed grid");
        result.cells.reserve(field.arrayDimension);
        for(std::uint32_t slot=0u;slot<field.arrayDimension;++slot) {
            LifecycleStep();const auto value=Read(player,field,slot);
            if(value.kind!=Kind::Byte || value.integer<0 || value.integer>255)
                throw std::runtime_error("Original player inventory grid contains non-Byte storage");
            result.cells.push_back(static_cast<std::uint8_t>(value.integer));
        }
        return result;
    }
