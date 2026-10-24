// Commercial-data-free authored v68 packages. This is the actual portable
// runtime/VM/save path, not a host stub or a manually constructed codec schema.
namespace StructArrayGenerated {
using Value=QuestVr::Vm::Value;
using Kind=QuestVr::Vm::Kind;
using Evaluation=QuestVr::Vm::Evaluation;
struct Code {Bytes raw;std::size_t logical{};};
void Append(Bytes& b,const Bytes& tail) {b.insert(b.end(),tail.begin(),tail.end());}
Code Token(std::uint8_t op) {return {{op},1u};}
Code Ref(std::uint8_t op,std::int32_t ref) {Code c{{op},5u};Index(c.raw,ref);return c;}
Code Join(std::initializer_list<Code> parts) {Code c;for(const auto& p:parts){Append(c.raw,p.raw);c.logical+=p.logical;}return c;}
Code Int(std::int32_t n) {Code c{{0x1du},5u};U32(c.raw,static_cast<std::uint32_t>(n));return c;}
Code Return(Code value=Token(0x0bu)) {return Join({Token(0x04u),std::move(value)});}
Code Assign(Code left,Code right) {return Join({Token(0x0fu),std::move(left),std::move(right)});}
Code Member(std::int32_t field,Code source) {return Join({Ref(0x36u,field),std::move(source)});}
Code Element(Code index,Code source) {return Join({Token(0x1au),std::move(index),std::move(source)});}
Code Call(std::int32_t function,std::initializer_list<Code> arguments) {
    Code c=Ref(0x1cu,function);for(const auto& argument:arguments){Append(c.raw,argument.raw);c.logical+=argument.logical;}
    return Join({c,Token(0x16u)});
}
struct Package {
    std::string stem;
    std::vector<std::string> names{"None"};
    std::vector<ImportTableEntry> imports;
    std::vector<ExportTableEntry> exports;
    std::vector<std::function<Bytes(std::int32_t,std::int32_t)>> builders;
    std::map<std::int32_t,std::vector<std::int32_t>> children;
    std::int32_t Name(const std::string& name) {
        const auto it=std::find(names.begin(),names.end(),name);
        if(it!=names.end())return static_cast<std::int32_t>(it-names.begin());
        names.push_back(name);return static_cast<std::int32_t>(names.size()-1u);
    }
    std::int32_t Import(const std::string& name,std::int32_t outer,const std::string& type="Class") {
        imports.push_back({Name("Core"),Name(type),outer,Name(name)});return -static_cast<std::int32_t>(imports.size());
    }
    std::int32_t ImportPackage(const std::string& name) {return Import(name,0,"Package");}
    std::int32_t Export(const std::string& name,std::int32_t cls=0,std::int32_t base=0,std::int32_t outer=0,bool field=false) {
        exports.push_back({cls,base,outer,Name(name),ObjectFlags{},0,-1});builders.emplace_back();
        const auto ref=static_cast<std::int32_t>(exports.size());if(field)children[outer].push_back(ref);return ref;
    }
    void Body(std::int32_t ref,Bytes body) {
        builders.at(static_cast<std::size_t>(ref-1))=[body=std::move(body)](std::int32_t,std::int32_t){return body;};
    }
    void Tag(Bytes& b,const std::string& name,std::uint8_t type,const Bytes& data,
        const std::string& structure={},std::uint32_t index=0u) {
        Require(data.size()<=255u&&index<128u,"Generated struct tag exceeds independent small-fixture bound");
        Index(b,Name(name));b.push_back(static_cast<std::uint8_t>(type|0x50u|(index?0x80u:0u)));
        if(type==10u)Index(b,Name(structure));
        b.push_back(static_cast<std::uint8_t>(data.size()));if(index)b.push_back(static_cast<std::uint8_t>(index));Append(b,data);
    }
    void ClassBody(std::int32_t ref) {
        builders.at(static_cast<std::size_t>(ref-1))=[this,ref](std::int32_t next,std::int32_t first) {
            const auto& e=exports.at(static_cast<std::size_t>(ref-1));Bytes b;Index(b,e.ObjBase);Index(b,next);Index(b,0);Index(b,first);Index(b,e.ObjName);
            U32(b,0);U32(b,0);U32(b,0);U64(b,~std::uint64_t{});U64(b,~std::uint64_t{});U16(b,0xffffu);U32(b,0);U32(b,0);
            b.resize(b.size()+16u,0u);for(unsigned i=0;i<4u;++i)Index(b,0);Index(b,0);return b;
        };
    }
    void StructBody(std::int32_t ref) {
        builders.at(static_cast<std::size_t>(ref-1))=[this,ref](std::int32_t next,std::int32_t first) {
            Bytes b{0u};Index(b,0);Index(b,next);Index(b,0);Index(b,first);Index(b,exports.at(static_cast<std::size_t>(ref-1)).ObjName);
            U32(b,0);U32(b,0);U32(b,0);return b;
        };
    }
    std::int32_t Property(const std::string& name,const std::string& type,std::int32_t owner,
        std::int32_t core,std::int32_t target=0,std::uint32_t flags=0u,std::uint32_t dim=1u) {
        const auto ref=Export(name,Import(type,core),0,owner,true);
        builders.at(static_cast<std::size_t>(ref-1))=[type,target,flags,dim](std::int32_t next,std::int32_t) {
            Bytes b{0u};Index(b,0);Index(b,next);U32(b,dim);U32(b,flags);Index(b,0);if(flags&0x20u)U16(b,0);
            if(type=="StructProperty"||type=="ByteProperty"||type=="ObjectProperty")Index(b,target);
            return b;
        };return ref;
    }
    std::int32_t Function(const std::string& name,std::int32_t owner,std::int32_t core) {return Export(name,Import("Function",core),0,owner,true);}
    void FunctionBody(std::int32_t ref,Code code) {
        builders.at(static_cast<std::size_t>(ref-1))=[this,ref,code=std::move(code)](std::int32_t next,std::int32_t first) {
            Bytes b{0u};Index(b,0);Index(b,next);Index(b,0);Index(b,first);Index(b,exports.at(static_cast<std::size_t>(ref-1)).ObjName);
            U32(b,0);U32(b,0);U32(b,static_cast<std::uint32_t>(code.logical));Append(b,code.raw);U16(b,0);b.push_back(0u);U32(b,2u);return b;
        };
    }
    void ActorBody(std::int32_t ref,Bytes tags={}) {Index(tags,0);Body(ref,std::move(tags));}
    Bytes Serialize() {
        std::map<std::int32_t,std::int32_t> next;
        for(const auto& [owner,list]:children){static_cast<void>(owner);for(std::size_t i=1u;i<list.size();++i)next[list[i-1u]]=list[i];}
        Bytes b;U32(b,0x9e2a83c1u);U16(b,68u);U16(b,0);b.resize(56u,0u);std::vector<Bytes> bodies;std::vector<std::int32_t> offsets;
        for(std::size_t i=0u;i<exports.size();++i){const auto ref=static_cast<std::int32_t>(i+1u);const auto it=children.find(ref);
            auto body=builders[i]?builders[i](next[ref],it==children.end()||it->second.empty()?0:it->second.front()):Bytes{};
            offsets.push_back(static_cast<std::int32_t>(b.size()));Append(b,body);bodies.push_back(std::move(body));}
        ReplaceU32(b,12u,static_cast<std::uint32_t>(names.size()));ReplaceU32(b,16u,static_cast<std::uint32_t>(b.size()));
        for(const auto& name:names){Index(b,static_cast<std::int32_t>(name.size()+1u));b.insert(b.end(),name.begin(),name.end());b.push_back(0u);U32(b,0);}
        ReplaceU32(b,20u,static_cast<std::uint32_t>(exports.size()));ReplaceU32(b,24u,static_cast<std::uint32_t>(b.size()));
        for(std::size_t i=0u;i<exports.size();++i){const auto& e=exports[i];Index(b,e.ObjClass);Index(b,e.ObjBase);U32(b,static_cast<std::uint32_t>(e.ObjOuter));Index(b,e.ObjName);U32(b,static_cast<std::uint32_t>(e.ObjFlags));Index(b,static_cast<std::int32_t>(bodies[i].size()));if(!bodies[i].empty())Index(b,offsets[i]);}
        ReplaceU32(b,28u,static_cast<std::uint32_t>(imports.size()));ReplaceU32(b,32u,static_cast<std::uint32_t>(b.size()));
        for(const auto& e:imports){Index(b,e.ClassPackage);Index(b,e.ClassName);U32(b,static_cast<std::uint32_t>(e.ObjOuter));Index(b,e.ObjName);}return b;
    }
};
struct RuntimeFixture {
    std::filesystem::path parent,directory;std::map<std::filesystem::path,Bytes> sources;
    RuntimeFixture() {
        parent=std::filesystem::canonical(std::filesystem::temp_directory_path());
        const auto stamp=std::chrono::high_resolution_clock::now().time_since_epoch().count();
        for(unsigned i=0u;i<20u;++i){const auto candidate=parent/("deusex-struct-array-runtime-"+std::to_string(stamp)+'-'+std::to_string(i));
            if(std::filesystem::create_directory(candidate)){directory=std::filesystem::canonical(candidate);break;}}
        Require(!directory.empty()&&directory.parent_path()==parent,"Generated struct-array fixture escaped temporary parent");
        std::filesystem::create_directory(directory/"System");std::filesystem::create_directory(directory/"Maps");
    }
    ~RuntimeFixture() {
        ShutdownPortableRuntime();std::error_code error;const auto actual=std::filesystem::weakly_canonical(directory,error);
        if(!error&&!directory.empty()&&actual==directory&&actual.parent_path()==parent&&actual.filename().string().rfind("deusex-struct-array-runtime-",0u)==0u)
            std::filesystem::remove_all(actual,error);
    }
    void Write(const std::filesystem::path& path,const Bytes& bytes) const {
        std::ofstream file(path,std::ios::binary|std::ios::trunc);Require(static_cast<bool>(file),"Cannot create generated struct-array fixture/save");
        if(!bytes.empty())file.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));
        Require(static_cast<bool>(file),"Cannot write generated struct-array fixture/save");
    }
    Bytes Read(const std::filesystem::path& path) const {
        const auto size=std::filesystem::file_size(path);Require(size<=16u*1024u*1024u,"Struct-array fixture/save exceeds independent read bound");
        Bytes bytes(static_cast<std::size_t>(size));std::ifstream file(path,std::ios::binary);Require(static_cast<bool>(file),"Cannot read struct-array fixture/save");
        if(!bytes.empty())file.read(reinterpret_cast<char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));
        Require(static_cast<bool>(file),"Truncated struct-array fixture/save");return bytes;
    }
    PortablePackageTables Write(Package& package,bool map=false) {
        const auto path=directory/(map?"Maps":"System")/(package.stem+(map?".dx":".u"));
        const auto bytes=package.Serialize();Write(path,bytes);sources.emplace(path,bytes);return LoadPortablePackageTables(path.string());
    }
    Bytes Snapshot(const std::string& label) const {
        const auto path=directory/(label+".sav");Require(SavePortableRuntimeState(path.string()),"Cannot snapshot complete generated struct-array runtime");return Read(path);
    }
    void UnchangedSources() const {for(const auto& [path,bytes]:sources)Require(Read(path)==bytes,"Struct-array execution modified authored source package");}
};
struct Tables {std::vector<PortablePackageTables> scripts;PortablePackageTables map;};
Tables Build(RuntimeFixture& fixture) {
    Package core;core.stem="Core";core.ImportPackage("Core");const auto object=core.Export("Object"),meta=core.Export("Class",0,object);
    core.ClassBody(object);core.ClassBody(meta);const auto ct=fixture.Write(core);
    Package engine;engine.stem="Engine";const auto ec=engine.ImportPackage("Core"),eo=engine.Import("Object",ec),actor=engine.Export("Actor",0,eo);
    engine.exports.at(static_cast<std::size_t>(actor-1)).ObjFlags=static_cast<ObjectFlags>(0x04000000u);
    engine.ClassBody(actor);const auto et=fixture.Write(engine);
    Package classes;classes.stem="StructArrayClasses";
    const auto cc=classes.ImportPackage("Core"),ce=classes.ImportPackage("Engine"),ca=classes.Import("Actor",ce);
    const auto allowed=classes.Export("Allowed",0,ca),derived=classes.Export("DerivedAllowed",0,allowed),probe=classes.Export("Probe",0,ca);
    const auto child=classes.Export("ChildRecord",classes.Import("Struct",cc),0,probe);
    const auto childLabel=classes.Property("Label","NameProperty",child,cc),childCount=classes.Property("Count","IntProperty",child,cc);
    const auto record=classes.Export("Record",classes.Import("Struct",cc),0,probe);
    const auto names=classes.Property("Names","NameProperty",record,cc,0,0u,3u);
    const auto counts=classes.Property("Counts","IntProperty",record,cc,0,0u,2u);
    const auto links=classes.Property("Links","ObjectProperty",record,cc,allowed,0u,2u);
    const auto children=classes.Property("Children","StructProperty",record,cc,child,0u,2u);
    const auto sentinel=classes.Property("Sentinel","IntProperty",record,cc);
    const auto records=classes.Property("Records","StructProperty",probe,cc,record,0u,2u);
    const auto recordAt=[&](Code index){return Element(std::move(index),Ref(0x01u,records));};
    const auto fieldAt=[&](std::int32_t member,Code outer,Code inner){return Element(std::move(inner),Member(member,recordAt(std::move(outer))));};
    const auto makeAccess=[&](const char* name,std::int32_t member,const char* type,std::int32_t target,bool setter) {
        const auto fn=classes.Function(name,probe,cc);
        const auto outer=classes.Property("OuterIndex","IntProperty",fn,cc,0,0x80u),inner=classes.Property("Slot","IntProperty",fn,cc,0,0x80u);
        const auto data=classes.Property(setter?"Input":"ReturnValue",type,fn,cc,target,setter?0x80u:0x480u);
        auto address=fieldAt(member,Ref(0x00u,outer),Ref(0x00u,inner));
        classes.FunctionBody(fn,setter?Join({Assign(address,Ref(0x00u,data)),Return()}):Return(address));
    };
    makeAccess("SetName",names,"NameProperty",0,true);makeAccess("GetName",names,"NameProperty",0,false);
    makeAccess("SetCount",counts,"IntProperty",0,true);makeAccess("GetCount",counts,"IntProperty",0,false);
    // The function accepts Actor; the nested destination independently imposes Allowed.
    makeAccess("SetLink",links,"ObjectProperty",ca,true);makeAccess("GetLink",links,"ObjectProperty",ca,false);
    const auto setChild=classes.Function("SetChildCount",probe,cc);
    const auto childOuter=classes.Property("OuterIndex","IntProperty",setChild,cc,0,0x80u),childSlot=classes.Property("Slot","IntProperty",setChild,cc,0,0x80u);
    const auto childInput=classes.Property("Input","IntProperty",setChild,cc,0,0x80u);
    classes.FunctionBody(setChild,Join({Assign(Member(childCount,fieldAt(children,Ref(0x00u,childOuter),Ref(0x00u,childSlot))),Ref(0x00u,childInput)),Return()}));
    const auto getChild=classes.Function("GetChildLabel",probe,cc);
    const auto labelOuter=classes.Property("OuterIndex","IntProperty",getChild,cc,0,0x80u),labelSlot=classes.Property("Slot","IntProperty",getChild,cc,0,0x80u);
    classes.Property("ReturnValue","NameProperty",getChild,cc,0,0x480u);
    classes.FunctionBody(getChild,Return(Member(childLabel,fieldAt(children,Ref(0x00u,labelOuter),Ref(0x00u,labelSlot)))));
    const auto setRecord=classes.Function("SetRecord",probe,cc),setIndex=classes.Property("OuterIndex","IntProperty",setRecord,cc,0,0x80u);
    const auto inputRecord=classes.Property("Input","StructProperty",setRecord,cc,record,0x80u);
    classes.FunctionBody(setRecord,Join({Assign(recordAt(Ref(0x00u,setIndex)),Ref(0x00u,inputRecord)),Return()}));
    const auto copy=classes.Function("CopyRecord",probe,cc),from=classes.Property("Source","IntProperty",copy,cc,0,0x80u),to=classes.Property("Destination","IntProperty",copy,cc,0,0x80u);
    classes.FunctionBody(copy,Join({Assign(recordAt(Ref(0x00u,to)),recordAt(Ref(0x00u,from))),Return()}));
    const auto outHelper=classes.Function("WriteOutCount",probe,cc),out=classes.Property("Output","IntProperty",outHelper,cc,0,0x180u);
    classes.FunctionBody(outHelper,Join({Assign(Ref(0x00u,out),Int(1234)),Return()}));
    const auto outCaller=classes.Function("WriteNestedOut",probe,cc);
    classes.FunctionBody(outCaller,Join({Call(outHelper,{fieldAt(counts,Int(1),Int(1))}),Return()}));
    const Code unknown{{0x63u,0xe7u,0x16u},3u}; // Deliberate unsupported native999 after real member writes.
    const auto mutateCode=Join({Assign(fieldAt(names,Int(0),Int(1)),Ref(0x21u,classes.Name("Changed"))),
        Assign(fieldAt(counts,Int(0),Int(1)),Int(77)),Assign(Member(childCount,fieldAt(children,Int(0),Int(1))),Int(88))});
    for(const auto* name:{"Mutate","MutateThenFail"}) {
        const auto fn=classes.Function(name,probe,cc);
        classes.FunctionBody(fn,std::string(name)=="Mutate"?Join({mutateCode,Return()}):Join({mutateCode,unknown,Return()}));
    }
    static_cast<void>(sentinel);classes.ClassBody(allowed);classes.ClassBody(derived);classes.ClassBody(probe);
    classes.StructBody(child);classes.StructBody(record);const auto st=fixture.Write(classes);
    Package map;map.stem="StructArrayGenerated";const auto mc=map.ImportPackage("StructArrayClasses"),me=map.ImportPackage("Engine");
    const auto driver=map.Export("Driver",map.Import("Probe",mc)),first=map.Export("Allowed0",map.Import("Allowed",mc));
    const auto second=map.Export("Allowed1",map.Import("DerivedAllowed",mc)),wrong=map.Export("Wrong",map.Import("Actor",me));
    Bytes tags;
    for(std::uint32_t slot=0u;slot<2u;++slot) {
        Bytes raw;for(const auto* label:slot?std::array<const char*,3u>{"Delta","Epsilon","Zeta"}:std::array<const char*,3u>{"Alpha","Beta","Gamma"})Index(raw,map.Name(label));
        U32(raw,slot?30u:10u);U32(raw,slot?40u:20u);Index(raw,slot?second:first);Index(raw,slot?first:second);
        Index(raw,map.Name(slot?"RightA":"LeftA"));U32(raw,slot?300u:100u);
        Index(raw,map.Name(slot?"RightB":"LeftB"));U32(raw,slot?400u:200u);U32(raw,slot?222u:111u);
        map.Tag(tags,"Records",10u,raw,"Record",slot);
    }
    map.ActorBody(driver,tags);map.ActorBody(first);map.ActorBody(second);map.ActorBody(wrong);
    return {{ct,et,st},fixture.Write(map,true)};
}
std::string Path(const std::string& leaf) {return "StructArrayGenerated."+leaf;}
Value Array(std::initializer_list<Value> values) {Value result;result.kind=Kind::Struct;std::size_t slot{};for(const auto& value:values)result.fields.emplace(std::to_string(slot++),value);return result;}
Value Child(const char* label,std::int32_t count) {Value result;result.kind=Kind::Struct;result.fields={{"label",Value::Text(Kind::Name,label)},{"count",Value::Integer(count)}};return result;}
Value Expected(std::uint32_t slot) {
    Value result;result.kind=Kind::Struct;
    result.fields={{"names",slot?Array({Value::Text(Kind::Name,"Delta"),Value::Text(Kind::Name,"Epsilon"),Value::Text(Kind::Name,"Zeta")}):Array({Value::Text(Kind::Name,"Alpha"),Value::Text(Kind::Name,"Beta"),Value::Text(Kind::Name,"Gamma")})},
        {"counts",Array({Value::Integer(slot?30:10),Value::Integer(slot?40:20)})},
        {"links",Array({Value::Text(Kind::Object,Path(slot?"Allowed1":"Allowed0")),Value::Text(Kind::Object,Path(slot?"Allowed0":"Allowed1"))})},
        {"children",slot?Array({Child("RightA",300),Child("RightB",400)}):Array({Child("LeftA",100),Child("LeftB",200)})},
        {"sentinel",Value::Integer(slot?222:111)}};return result;
}
QuestVr::Vm::Result Execute(const char* name,const std::vector<Evaluation>& arguments={},const QuestVr::Vm::Limits& limits={}) {
    return ExecutePortableActorFunction(Path("Driver"),name,arguments,limits);
}
QuestVr::Vm::Result MustCall(const char* name,const std::vector<Evaluation>& arguments={}) {
    const auto result=Execute(name,arguments);
    Require(result.passed()&&result.committed,std::string("Generated struct-array ")+name+" failed: "+result.error+" at "+result.function+':'+std::to_string(result.offset));return result;
}
Evaluation Number(std::int32_t n) {return {Value::Integer(n),{}};}
Evaluation Text(Kind kind,const std::string& value) {return {Value::Text(kind,value),{}};}
using Records=std::array<Value,2u>;
void Same(const Records& expected,const std::string& reason) {
    const auto actual=ReadPortableActorScriptPropertySlots(Path("Driver"),"Records",0u,2u);
    Require(actual.size()==expected.size(),reason+": outer fixed property array changed dimension");
    for(std::size_t slot=0u;slot<expected.size();++slot)Require(QuestVr::Vm::Equal(actual[slot],expected[slot]),reason+": Records["+std::to_string(slot)+']');
}
std::uint32_t Word(const Bytes& bytes,std::size_t offset) {
    Require(offset<=bytes.size()&&bytes.size()-offset>=4u,"Generated struct-array checkpoint word is truncated");
    std::uint32_t value{};for(unsigned i=0u;i<4u;++i)value|=std::uint32_t(bytes[offset+i])<<(i*8u);return value;
}
std::size_t ScriptOffset(const Bytes& bytes) {
    Require(Word(bytes,0u)==0x53515844u&&Word(bytes,4u)>=4u&&Word(bytes,4u)<=13u,"Struct-array checkpoint lacks bounded script envelope");
    std::size_t cursor=8u;
    const auto skip=[&](std::size_t count){Require(cursor<=bytes.size()&&count<=bytes.size()-cursor,"Struct-array checkpoint prefix is truncated");cursor+=count;};
    const auto word=[&](){const auto value=Word(bytes,cursor);cursor+=4u;return value;};
    const auto text=[&](){const auto size=word();Require(size<=1048576u,"Struct-array prefix text exceeds bound");skip(size);};
    const auto list=[&](){const auto count=word();Require(count<=100000u,"Struct-array prefix list exceeds bound");for(std::uint32_t i=0u;i<count;++i)text();};
    list();list();list();skip(4u);const auto damaged=word();Require(damaged<=100000u,"Struct-array damage prefix exceeds bound");
    for(std::uint32_t i=0u;i<damaged;++i){text();skip(4u);}skip(8u);list();list();list();list();
    const auto length=Word(bytes,cursor);Require(length>0u&&cursor+4u<=bytes.size()&&length==bytes.size()-cursor-4u,"Struct-array script trailer does not cover exact tail");return cursor;
}
QuestVr::ScriptSavedState Saved(const Bytes& checkpoint) {
    const auto offset=ScriptOffset(checkpoint);return QuestVr::DecodeScriptSavedState({checkpoint.begin()+static_cast<std::ptrdiff_t>(offset+4u),checkpoint.end()});
}
Bytes ReplaceScript(const Bytes& checkpoint,const QuestVr::ScriptSavedState& state) {
    const auto offset=ScriptOffset(checkpoint);const auto blob=QuestVr::EncodeScriptSavedState(state);
    Bytes bytes(checkpoint.begin(),checkpoint.begin()+static_cast<std::ptrdiff_t>(offset+4u));ReplaceU32(bytes,offset,static_cast<std::uint32_t>(blob.size()));Append(bytes,blob);return bytes;
}
Value& SavedRecord(QuestVr::ScriptSavedState& state,std::uint32_t index) {
    const auto object=std::find_if(state.objects.begin(),state.objects.end(),[](const auto& value){return value.path==Path("Driver");});
    Require(object!=state.objects.end(),"Struct-array saved overlay omitted its actual actor");
    const auto property=std::find_if(object->properties.begin(),object->properties.end(),[&](const auto& value){return value.key=="StructArrayClasses.Probe.Records"&&value.index==index;});
    Require(property!=object->properties.end(),"Struct-array saved overlay omitted its qualified fixed outer slot");return property->value;
}
void Run() {
    const auto beforeChecks=checks,beforeRejections=rejections;RuntimeFixture fixture;const auto tables=Build(fixture);
    const auto initialized=InitializePortableRuntime(tables.scripts);Require(initialized.passed,"Generated struct-array runtime initialization failed");
    const auto loaded=LoadPortableRuntimeMap(tables.map);Require(loaded.passed,"Generated struct-array map load failed");
    Records expected{Expected(0u),Expected(1u)};Same(expected,"Authored member-array bytes decoded in the wrong member/slot order");
    const auto untouched=fixture.Snapshot("Initial");const auto untouchedPath=fixture.directory/"Initial.sav";
    for(std::uint32_t outer=0u;outer<2u;++outer) {
        for(std::uint32_t slot=0u;slot<3u;++slot)Require(QuestVr::Vm::Equal(MustCall("GetName",{Number(outer),Number(slot)}).value,expected[outer].fields.at("names").fields.at(std::to_string(slot))),"Real 0x36/ArrayElement Name read lost slot identity");
        for(std::uint32_t slot=0u;slot<2u;++slot) {
            Require(QuestVr::Vm::Equal(MustCall("GetCount",{Number(outer),Number(slot)}).value,expected[outer].fields.at("counts").fields.at(std::to_string(slot))),"Real Int member-array read lost slot identity");
            Require(QuestVr::Vm::Equal(MustCall("GetLink",{Number(outer),Number(slot)}).value,expected[outer].fields.at("links").fields.at(std::to_string(slot))),"Real constrained Object member-array read lost slot identity");
            Require(QuestVr::Vm::Equal(MustCall("GetChildLabel",{Number(outer),Number(slot)}).value,expected[outer].fields.at("children").fields.at(std::to_string(slot)).fields.at("label")),"Nested Struct member-array read lost child identity");
        }
    }
    Require(fixture.Snapshot("ReadOnly")==untouched,"Pure member-array reads manufactured an overlay/save change");
    const auto measured=MustCall("Mutate");Require(measured.writes>=3u,"Generated member-array mutation did not execute all three real writes");
    Require(LoadPortableRuntimeState(untouchedPath.string()),"Cannot reset generated member-array write controls");
    for(std::size_t prefix=0u;prefix<measured.writes;++prefix) {
        const auto baseline=fixture.Snapshot("WriteCutBefore");const auto revision=GetPortableRuntimeWorldRevision();GC::Collect();const auto gc=GC::GetStats();
        QuestVr::Vm::Limits limits;limits.writes=prefix;const auto cut=Execute("Mutate",{},limits);
        Require(!cut.passed()&&!cut.committed&&cut.status==QuestVr::Vm::Status::Budget&&cut.writes==prefix,"Nested member-array update bypassed the shared partial-write limit");
        Same(expected,"Write-prefix failure retained a nested member mutation");GC::Collect();const auto after=GC::GetStats();
        Require(fixture.Snapshot("WriteCutAfter")==baseline&&GetPortableRuntimeWorldRevision()==revision&&after.numObjects==gc.numObjects&&after.memoryUsage==gc.memoryUsage,"Write-prefix failure changed complete save/revision/GC state");++rejections;
    }
    {
        const auto baseline=fixture.Snapshot("NativeFailureBefore");const auto revision=GetPortableRuntimeWorldRevision();
        const auto failed=Execute("MutateThenFail");Require(!failed.passed()&&!failed.committed&&failed.writes>=3u&&failed.error.find("999")!=std::string::npos,"Native failure did not happen after real nested member-array writes");
        Same(expected,"Late native failure retained a nested member mutation");Require(fixture.Snapshot("NativeFailureAfter")==baseline&&GetPortableRuntimeWorldRevision()==revision,"Late native failure changed complete save or revision");++rejections;
    }
    MustCall("SetName",{Number(1),Number(2),Text(Kind::Name,"Selected")});expected[1].fields.at("names").fields.at("2")=Value::Text(Kind::Name,"Selected");Same(expected,"Name slot write overwrote a sibling/outer record");
    MustCall("SetCount",{Number(0),Number(1),Number(-73)});expected[0].fields.at("counts").fields.at("1")=Value::Integer(-73);Same(expected,"Int slot write overwrote a sibling/outer record");
    MustCall("SetLink",{Number(0),Number(1),Text(Kind::Object,Path("Allowed0"))});expected[0].fields.at("links").fields.at("1")=Value::Text(Kind::Object,Path("Allowed0"));Same(expected,"Object slot write overwrote a sibling/outer record");
    MustCall("SetChildCount",{Number(1),Number(0),Number(-99)});expected[1].fields.at("children").fields.at("0").fields.at("count")=Value::Integer(-99);Same(expected,"Nested Struct-array child write overwrote siblings");
    MustCall("WriteNestedOut");expected[1].fields.at("counts").fields.at("1")=Value::Integer(1234);Same(expected,"OUT binding did not copy back exactly its nested fixed-array element");
    for(const auto& invalid:std::array<std::string,3u>{Path("Wrong"),"StructArrayClasses.Allowed",Path("Missing")}) {
        const auto baseline=fixture.Snapshot("BadObjectBefore");const auto revision=GetPortableRuntimeWorldRevision();
        const auto failed=Execute("SetLink",{Number(0),Number(0),Text(Kind::Object,invalid)});
        Require(!failed.passed()&&!failed.committed,"Nested Object member-array destination accepted unrelated/class/missing identity");Same(expected,"Object constraint refusal changed records");
        Require(fixture.Snapshot("BadObjectAfter")==baseline&&GetPortableRuntimeWorldRevision()==revision,"Object constraint refusal changed complete save/revision");++rejections;
    }
    MustCall("CopyRecord",{Number(1),Number(0)});expected[0]=expected[1];Same(expected,"Whole-struct assignment lost nested member-array data");
    MustCall("SetName",{Number(1),Number(0),Text(Kind::Name,"AfterCopy")});expected[1].fields.at("names").fields.at("0")=Value::Text(Kind::Name,"AfterCopy");Same(expected,"Whole-struct copy retained an alias instead of an independent value");
    MustCall("SetRecord",{Number(0),{Expected(0u),{}}});expected[0]=Expected(0u);Same(expected,"Whole typed Struct input flattened member arrays");
    const auto checkpoint=fixture.Snapshot("AllSlots");const auto checkpointPath=fixture.directory/"AllSlots.sav";
    auto state=Saved(checkpoint);for(std::uint32_t i=0u;i<2u;++i)Require(QuestVr::Vm::Equal(SavedRecord(state,i),expected[i]),"Saved struct-array slot differs from live typed record");
    MustCall("Mutate");Require(LoadPortableRuntimeState(checkpointPath.string()),"Cannot restore every nested member-array slot");Same(expected,"Checkpoint restore retained stale nested member-array values");
    Require(fixture.Snapshot("Restored")==checkpoint,"Nested fixed member arrays changed existing save envelope or canonical roundtrip");
    const auto reject=[&](const std::function<void(Value&)>& modify,const char* reason) {
        auto invalid=state;modify(SavedRecord(invalid,0u));const auto path=fixture.directory/"Invalid.sav";fixture.Write(path,ReplaceScript(checkpoint,invalid));
        const auto revision=GetPortableRuntimeWorldRevision();Require(!ValidatePortableRuntimeState(path.string())&&!LoadPortableRuntimeState(path.string()),std::string("Malformed nested member-array checkpoint accepted: ")+reason);
        Same(expected,std::string("Malformed checkpoint changed live slots: ")+reason);Require(fixture.Snapshot("Refused")==checkpoint&&GetPortableRuntimeWorldRevision()==revision,std::string("Malformed checkpoint changed complete save/revision: ")+reason);++rejections;
    };
    reject([](Value& v){v.fields.at("names").fields.erase("2");},"missing Name slot");
    reject([](Value& v){v.fields.at("counts").fields.emplace("2",Value::Integer(0));},"extra Int slot");
    reject([](Value& v){auto& a=v.fields.at("names").fields;const auto value=a.at("1");a.erase("1");a.emplace("01",value);},"noncanonical decimal slot name");
    reject([](Value& v){v.fields.at("names").fields.at("1")=Value::Integer(1);},"wrong Name element type");
    reject([](Value& v){v.fields.at("counts").fields.at("1")=Value::Bool(true);},"wrong Int element type");
    reject([](Value& v){v.fields.at("links").fields.at("1")=Value::Text(Kind::String,Path("Allowed0"));},"wrong Object element kind");
    reject([](Value& v){v.fields.at("links").fields.at("1")=Value::Text(Kind::Object,Path("Wrong"));},"Object element outside declared ancestry");
    reject([](Value& v){v.fields.at("links").fields.at("1")=Value::Text(Kind::Object,"StructArrayClasses.Allowed");},"Object element holding a class");
    reject([](Value& v){v.fields.at("links").fields.at("1")=Value::Text(Kind::Object,Path("Missing"));},"Object element holding unavailable identity");
    reject([](Value& v){v.fields.at("children").fields.at("0").fields.erase("label");},"Struct element missing a scalar member");
    reject([](Value& v){v.fields.at("children").fields.erase("1");},"missing Struct slot");
    reject([](Value& v){v.fields.at("children").fields.at("1").fields.at("count")=Value::Bool(false);},"wrong nested Struct scalar kind");
    reject([](Value& v){v.fields.at("names")=Value::Text(Kind::Name,"Alpha");},"scalar replacing fixed-array envelope");
    // The same schema must protect whole live assignments, not just save load.
    {
        auto invalid=expected[0];invalid.fields.at("links").fields.at("1")=Value::Text(Kind::Object,Path("Wrong"));
        const auto revision=GetPortableRuntimeWorldRevision();const auto failed=Execute("SetRecord",{Number(0),{invalid,{}}});
        Require(!failed.passed()&&!failed.committed,"Whole live Struct assignment bypassed an inner array's Object constraint");Same(expected,"Whole live invalid record assignment leaked");
        Require(fixture.Snapshot("BadWhole")==checkpoint&&GetPortableRuntimeWorldRevision()==revision,"Whole live invalid record assignment changed complete save/revision");++rejections;
    }
    fixture.UnchangedSources();
    std::cout<<"PASS generated production nested fixed member arrays: "<<checks-beforeChecks<<" checks, "<<rejections-beforeRejections
        <<" refusal controls; Name[3]/Int[2]/Object<Allowed>[2]/Struct[2] inside Records[2], actual 0x36/ArrayElement/Let/OUT/copy, rollback and existing checkpoint schema.\n"
        <<"SCOPE: referenced targets are genuine authored map Actors, already rooted by the map; this does not prove a sole nested edge retains a runtime-born Object through GC or replacement-birth checkpoint restore.\n";
}
} // namespace StructArrayGenerated
