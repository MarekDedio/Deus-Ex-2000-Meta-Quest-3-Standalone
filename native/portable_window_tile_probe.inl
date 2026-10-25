// Included inside PortableActorVmHost. Real native base Root/Tile/Window
// constructors and query wrappers, with explicit journaled fixture settings.
// This is ALWAYS rolled back, not a configured HUD or published game session.
    Value ProbeNativeWindowTileQuery(const std::string& playerPath,PortableWindowTileProbeMode mode,
        PortableWindowTileProbeResult& result,QuestVr::Vm::Execution& execution) {
        switch(mode) {
        case PortableWindowTileProbeMode::RoundTrip:case PortableWindowTileProbeMode::LateFailure:
        case PortableWindowTileProbeMode::BadGraph:case PortableWindowTileProbeMode::DeadChild:break;
        default:throw std::runtime_error("Unknown native Tile diagnostic mode");
        }
        auto* pawn=Object(playerPath);RequireMapActor(pawn);
        if(!IsDerivedFromPath(pawn->cls,"Extension.PlayerPawnExt") || pawn->nativeRootWindow)
            throw std::runtime_error("Native Tile diagnostic requires an uninitialized actual player");
        constexpr std::size_t snapshotBytes=128u<<10u;
        ValidateStateBudget(snapshotBytes);nativeActorOrderObservationBytes_+=snapshotBytes;
        QuestVr::AuthoredStructLimits bounds;bounds.retainedBytes=snapshotBytes;
        QuestVr::AuthoredStructDetail::Budget retained{bounds};
        result.stages.reserve(8u);
        retained.Retain(sizeof(result)+result.stages.capacity()*sizeof(PortableWindowTileProbeStage));
        auto* root=ConstructNativeRootWindow(pawn,DefaultClass(Object("Extension.RootWindow")),execution);
        const auto init=Member(root,"InitWindow");
        if(!init)throw std::runtime_error("Native Tile fixture lacks original base Root InitWindow");
        static_cast<void>(execution.CallFunction(*init,root->reflection.objectPath,{}));
        const auto newChild=NewChildWindowDeclaration(nullptr);
        const auto child=[&](RuntimeObject* parent,const char* classPath) {
            const auto born=execution.CallFunction(*newChild,parent->reflection.objectPath,
                {{Value::Text(Kind::Object,classPath),{}},{Value::Bool(false),{}}});
            if(born.kind!=Kind::Object || born.text.empty())
                throw std::runtime_error("Native Tile fixture NewChild returned None");
            auto* value=Object(born.text);RequireNativeWindow(value);return value;
        };
        auto* tile=child(root,"Extension.TileWindow");
        std::array<RuntimeObject*,3u> children{};
        for(auto*& value:children)value=child(tile,"Extension.Window");
        result.rootPath=root->reflection.objectPath;result.tilePath=tile->reflection.objectPath;
        retained.Retain(result.rootPath.capacity()+result.tilePath.capacity()+2u);
        for(std::size_t i=0u;i<children.size();++i) {
            result.childPaths[i]=children[i]->reflection.objectPath;
            retained.Retain(result.childPaths[i].capacity()+1u);
        }
        const auto windowFloat=[&](RuntimeObject* object,const char* leaf,float value) {
            NativeWindowWrite(object,("Extension.Window."+std::string(leaf)).c_str(),Value::Float(value),execution);
        };
        const auto windowBool=[&](RuntimeObject* object,const char* leaf,bool value) {
            NativeWindowWrite(object,("Extension.Window."+std::string(leaf)).c_str(),Value::Bool(value),execution);
        };
        const auto tileFloat=[&](const char* leaf,float value) {
            NativeWindowWrite(tile,("Extension.TileWindow."+std::string(leaf)).c_str(),Value::Float(value),execution);
        };
        const auto tileBool=[&](const char* leaf,bool value) {
            NativeWindowWrite(tile,("Extension.TileWindow."+std::string(leaf)).c_str(),Value::Bool(value),execution);
        };
        const auto tileByte=[&](const char* leaf,std::uint8_t value) {
            NativeWindowWrite(tile,("Extension.TileWindow."+std::string(leaf)).c_str(),Value::Byte(value),execution);
        };
        // Root and Tile stay hidden. Children's LOCAL visibility is the native
        // sizing input, deliberately independent of effective root visibility.
        for(auto* object:{tile,children[0u],children[1u],children[2u]}) {
            windowFloat(object,"hardcodedWidth",-1.0f);windowFloat(object,"hardcodedHeight",-1.0f);
            windowBool(object,"bIsVisible",false);windowBool(object,"bNeedsQuery",true);
        }
        tileByte("orientation",0u);tileByte("hDirection",0u);tileByte("vDirection",0u);
        tileByte("hChildAlign",0u);tileByte("vChildAlign",0u);
        tileFloat("hMargin",2.0f);tileFloat("vMargin",3.0f);
        tileFloat("minorSpacing",1.0f);tileFloat("majorSpacing",2.0f);
        tileBool("bWrap",true);tileBool("bFillParent",false);
        tileBool("bEqualWidth",false);tileBool("bEqualHeight",false);
        for(std::size_t i=0u;i<children.size();++i) {
            auto* object=children[i];
            windowFloat(object,"Width",i==0u?10.75f:i==1u?20.5f:99.0f);
            windowFloat(object,"Height",i==0u?4.25f:i==1u?8.75f:88.0f);
            // Hidden initial held geometry must be genuinely overwritten,
            // including the otherwise untouched position fields, in the query.
            windowFloat(object,"holdX",71.0f);windowFloat(object,"holdY",72.0f);
            windowFloat(object,"holdWidth",73.0f);windowFloat(object,"holdHeight",74.0f);
        }
        const auto capture=[&](const char* name,std::uint16_t index,float width,float height) {
            if(result.stages.size()>=8u)throw std::runtime_error("Native Tile fixture snapshot count exceeded");
            if(!tile->nativeWindowState || !tile->nativeWindowState->tileRowsInitialized)
                throw std::runtime_error("Native Tile fixture lost initialized row storage");
            PortableWindowTileProbeStage stage;stage.name=name;stage.nativeIndex=index;
            stage.width=width;stage.height=height;stage.writes=execution.WriteCount();
            retained.Retain(stage.name.capacity()+1u);
            NativeWindowQueryOperations parent{*this,tile,execution};
            stage.lastSpecifiedWidth=parent.ReadFloat("lastSpecifiedWidth");
            stage.lastSpecifiedHeight=parent.ReadFloat("lastSpecifiedHeight");
            stage.lastQueryWidth=parent.ReadFloat("lastQueryWidth");
            stage.lastQueryHeight=parent.ReadFloat("lastQueryHeight");
            stage.lastWidthSpecified=parent.ReadBool("bLastWidthSpecified");
            stage.lastHeightSpecified=parent.ReadBool("bLastHeightSpecified");
            stage.needsQuery=parent.ReadBool("bNeedsQuery");
            const auto& rows=tile->nativeWindowState->tileRows;stage.rowCount=rows.size();
            if(rows.size()>stage.rows.size())throw std::runtime_error("Native Tile fixture row count exceeded");
            for(std::size_t i=0u;i<rows.size();++i)
                stage.rows[i]={rows[i].crossSize,rows[i].mainExtent,rows[i].childCount};
            for(std::size_t i=0u;i<children.size();++i) {
                NativeWindowQueryOperations current{*this,children[i],execution};auto& view=stage.children[i];
                view.holdX=current.ReadFloat("holdX");view.holdY=current.ReadFloat("holdY");
                view.holdWidth=current.ReadFloat("holdWidth");view.holdHeight=current.ReadFloat("holdHeight");
                view.lastQueryWidth=current.ReadFloat("lastQueryWidth");
                view.lastQueryHeight=current.ReadFloat("lastQueryHeight");
                view.visible=current.ReadBool("bIsVisible");view.needsQuery=current.ReadBool("bNeedsQuery");
            }
            result.stages.push_back(std::move(stage));
        };
        const auto fullQuery=[&](const char* name) {
            // OUT aliases survive in the transaction journal, so storage must
            // outlive this call. Actual1465 writes width before height.
            auto width=std::make_shared<float>(-101.0f),height=std::make_shared<float>(-102.0f);
            const auto output=[](const std::shared_ptr<float>& storage) {
                auto reference=std::make_shared<Reference>();reference->zero=Value::Float(0.0f);
                reference->read=[storage] {return Value::Float(*storage);};
                reference->write=[storage](const Value& value) {
                    if(value.kind!=Kind::Float || !std::isfinite(value.floating))
                        throw std::runtime_error("Native Tile fixture received a nonfinite/non-Float OUT value");
                    *storage=value.floating;
                };
                return reference;
            };
            static_cast<void>(execution.CallFunction(*WindowNativeDeclaration(1465u,nullptr),tile->reflection.objectPath,
                {{Value::Float(*width),output(width)},{Value::Float(*height),output(height)}}));
            capture(name,1465u,*width,*height);
        };
        const auto heightQuery=[&](float width,const char* name) {
            const auto value=execution.CallFunction(*WindowNativeDeclaration(1464u,nullptr),tile->reflection.objectPath,
                {{Value::Float(width),{}}});
            if(value.kind!=Kind::Float || !std::isfinite(value.floating))
                throw std::runtime_error("Native Tile fixture received no finite preferred height");
            capture(name,1464u,width,value.floating);
        };
        const auto placement=[&](float width,float height,const char* name) {
            NativeWindowTileSize(tile,true,width,true,height,execution);
            capture(name,0u,width,height);
        };
        fullQuery("all-hidden");
        windowBool(children[0u],"bIsVisible",true);windowBool(children[1u],"bIsVisible",true);
        windowBool(tile,"bNeedsQuery",true);fullQuery("horizontal");
        if(mode==PortableWindowTileProbeMode::BadGraph) {
            WindowLinkWrite(children[2u],"prevSibling",children[2u],execution);
            windowBool(tile,"bNeedsQuery",true);fullQuery("bad-graph-accepted");
            throw std::runtime_error("Native Tile fixture incorrectly accepted a cyclic child graph");
        }
        if(mode==PortableWindowTileProbeMode::DeadChild) {
            windowBool(children[1u],"bBeingDestroyed",true);
            windowBool(tile,"bNeedsQuery",true);fullQuery("dead-child-accepted");
            throw std::runtime_error("Native Tile fixture incorrectly accepted a destroying child");
        }
        fullQuery("horizontal-cache");
        // Focused native scalar-tail control: Window.Init's parent callbacks
        // can already have populated rows before Tile.Init writes defaults.
        // This direct helper call does NOT replay Window.Init or claim an
        // authored ChildAdded/DescendantAdded callback acceptance test.
        if(!tile->nativeWindowState || tile->nativeWindowState->tileRows.size()!=1u)
            throw std::runtime_error("Native Tile scalar-tail control lacks its populated row");
        const auto populatedRow=tile->nativeWindowState->tileRows.front();
        const auto retainedRow=[&] {
            if(!tile->nativeWindowState || !tile->nativeWindowState->tileRowsInitialized ||
                tile->nativeWindowState->tileRows.size()!=1u ||
                tile->nativeWindowState->tileRows.front()!=populatedRow)
                throw std::runtime_error("Native Tile scalar tail or setter discarded populated row storage");
        };
        InitializeChildTileWindow(tile,execution);retainedRow();
        // Restore every explicit fixture Tile field after the real scalar
        // defaults. All stores and the subsequent setter share this root.
        tileByte("orientation",0u);tileByte("hDirection",0u);tileByte("vDirection",0u);
        tileByte("hChildAlign",0u);tileByte("vChildAlign",0u);
        tileFloat("hMargin",2.0f);tileFloat("vMargin",3.0f);
        tileFloat("minorSpacing",1.0f);tileFloat("majorSpacing",2.0f);
        tileBool("bWrap",true);tileBool("bFillParent",false);
        tileBool("bEqualWidth",false);tileBool("bEqualHeight",false);
        static_cast<void>(execution.CallFunction(*WindowNativeDeclaration(1535u,nullptr),tile->reflection.objectPath,
            {{Value::Float(2.0f),{}},{Value::Float(3.0f),{}}}));
        retainedRow();
        heightQuery(20.0f,"wrapped-height");
        placement(20.0f,30.0f,"wrapped-placement");
        tileByte("orientation",1u);tileBool("bWrap",false);tileBool("bFillParent",true);
        tileByte("hChildAlign",1u);windowBool(tile,"bNeedsQuery",true);
        heightQuery(15.0f,"vertical-fill");
        tileByte("hDirection",1u);tileByte("vDirection",1u);
        placement(15.0f,30.0f,"vertical-reversed-placement");
        for(auto* object:children)windowBool(object,"bIsVisible",false);
        windowBool(tile,"bNeedsQuery",true);fullQuery("replacement-empty");
        if(mode==PortableWindowTileProbeMode::LateFailure)
            throw std::runtime_error("Native Tile diagnostic late failure after valid layout");
        throw std::runtime_error("Native Tile diagnostic complete; rollback required");
    }
