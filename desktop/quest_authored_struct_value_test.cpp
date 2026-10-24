#include "quest_authored_struct_value.h"

#include <functional>
#include <iostream>

namespace {
using namespace QuestVr;
using Vm::Kind;
using Bytes=std::vector<std::uint8_t>;
std::size_t checks{},rejections{};
void Require(bool condition,const std::string& message) {
    if (!condition) throw std::runtime_error(message);
    ++checks;
}
void Reject(const std::function<void()>& operation,const std::string& message) {
    bool rejected{};
    try { operation(); } catch (const std::runtime_error&) { rejected=true; }
    Require(rejected,message); ++rejections;
}
Vm::Value Scalar(Kind kind) { Vm::Value result; result.kind=kind; return result; }
AuthoredStructField Field(std::string name,Kind kind,const std::string& owner="Fixture.Record") {
    AuthoredStructField result; result.key=owner+'.'+name; result.name=std::move(name);
    result.zero=Scalar(kind); return result;
}
AuthoredStructSchema Schema(std::vector<AuthoredStructField> fields,Kind kind=Kind::Struct,
                           const std::string& path="Fixture.Record") {
    AuthoredStructSchema result; result.path=path; result.name=path.substr(path.find_last_of('.')+1u);
    result.kind=kind; result.fields=std::move(fields); return result;
}
void U32(Bytes& bytes,std::uint32_t value) {
    for (unsigned i=0u; i<4u; ++i) bytes.push_back(static_cast<std::uint8_t>(value>>(i*8u)));
}
void I32(Bytes& bytes,std::int32_t value) {
    std::uint32_t bits{}; std::memcpy(&bits,&value,sizeof(value)); U32(bytes,bits);
}
void F32(Bytes& bytes,float value) {
    std::uint32_t bits{}; std::memcpy(&bits,&value,sizeof(value)); U32(bytes,bits);
}
void Index(Bytes& bytes,std::int32_t value) {
    const bool negative=value<0;
    std::uint64_t magnitude=negative ? static_cast<std::uint64_t>(-static_cast<std::int64_t>(value)) :
        static_cast<std::uint64_t>(value);
    auto first=static_cast<std::uint8_t>((negative ? 0x80u : 0u)|(magnitude&0x3fu));
    magnitude>>=6u; if (magnitude!=0u) first|=0x40u; bytes.push_back(first);
    while (magnitude!=0u) {
        auto next=static_cast<std::uint8_t>(magnitude&0x7fu); magnitude>>=7u;
        if (magnitude!=0u) next|=0x80u;
        bytes.push_back(next);
    }
}
void String(Bytes& bytes,const std::string& value,bool version64=true,bool terminate=true) {
    if (version64) Index(bytes,static_cast<std::int32_t>(value.size()+(terminate ? 1u : 0u)));
    bytes.insert(bytes.end(),value.begin(),value.end()); if (terminate) bytes.push_back(0u);
}
bool SameFloat(float a,float b) { return std::memcmp(&a,&b,sizeof(a))==0; }

void InventoryMembers() {
    auto inventory=Field("Inventory",Kind::Object,"Fixture.InventoryItem");
    inventory.classReference=true; inventory.referenceClassPath="Engine.Inventory";
    auto count=Field("Count",Kind::Int,"Fixture.InventoryItem");
    // Synthetic, original-style compact class reference plus signed count. Its
    // order is supplied by metadata here, never inferred from a C++ struct ABI.
    const auto schema=Schema({inventory,count},Kind::Struct,"Fixture.InventoryItem");
    const auto zero=MakeAuthoredStructZero(schema);
    Require(zero.kind==Kind::Struct && zero.fields.size()==2u && zero.fields.at("inventory").kind==Kind::Object &&
        zero.fields.at("inventory").text.empty() && zero.fields.at("count").integer==0,"Inventory zero shape");
    std::size_t callbackCount{}; const std::string valueSource="ActualValuePackage";
    AuthoredStructResolvers resolver;
    resolver.resolveObject=[&](std::int32_t index,const AuthoredStructField& field) {
        ++callbackCount;
        Require(field.classReference && field.referenceClassPath=="Engine.Inventory" && field.key==inventory.key,
            "Class constraints lost by decoder");
        if (index==-3) return valueSource+".ImportedInventoryClass";
        if (index==7) return valueSource+".ExportedInventoryClass";
        throw std::runtime_error("Unresolved or wrong-type source-package object");
    };
    for (const auto reference : {0,-3,7}) {
        Bytes bytes; Index(bytes,reference); I32(bytes,-1);
        const auto decoded=DecodeAuthoredStructValue(schema,bytes,69,resolver);
        Require(decoded.fields.at("count").integer==-1,"Signed Count sentinel was lost");
        const auto expected=reference==0 ? std::string{} : reference<0 ? valueSource+".ImportedInventoryClass" :
            valueSource+".ExportedInventoryClass";
        Require(decoded.fields.at("inventory").text==expected,"Source-package null/import/export reference changed");
        for (std::size_t n=0u; n<bytes.size(); ++n) {
            const Bytes truncated(bytes.begin(),bytes.begin()+static_cast<std::ptrdiff_t>(n));
            Reject([&] { DecodeAuthoredStructValue(schema,truncated,69,resolver); },"Truncated inventory value accepted");
        }
        auto trailing=bytes; trailing.push_back(0u);
        Reject([&] { DecodeAuthoredStructValue(schema,trailing,69,resolver); },"Trailing inventory bytes accepted");
    }
    Require(callbackCount>0u,"Class source callback was bypassed");
    Bytes wrong; Index(wrong,8); I32(wrong,3);
    Reject([&] { DecodeAuthoredStructValue(schema,wrong,69,resolver); },"Wrong-type reference callback failure swallowed");
    resolver.resolveObject=[](std::int32_t,const AuthoredStructField&) { return std::string{}; };
    Reject([&] { DecodeAuthoredStructValue(schema,wrong,69,resolver); },"Unresolved non-null reference accepted");
    auto reordered=Schema({count,inventory},Kind::Struct,"Fixture.InventoryItem");
    Bytes reversed; I32(reversed,-29); Index(reversed,0);
    const auto decoded=DecodeAuthoredStructValue(reordered,reversed,69);
    Require(decoded.fields.at("count").integer==-29 && decoded.fields.at("inventory").text.empty(),
        "Metadata wire order was replaced by alphabetic/map order");
}

void CompactReferences() {
    const auto schema=Schema({Field("Object",Kind::Object)});
    AuthoredStructResolvers resolver; std::int32_t observed{};
    resolver.resolveObject=[&](std::int32_t reference,const AuthoredStructField&) {
        observed=reference; return "ValuePackage.Reference";
    };
    for (const auto reference : {1,-1,63,-63,64,-64,8191,-8191,8192,-8192,1'048'575,-1'048'575,
         std::numeric_limits<std::int32_t>::max(),std::numeric_limits<std::int32_t>::min()}) {
        Bytes bytes; Index(bytes,reference); DecodeAuthoredStructValue(schema,bytes,69,resolver);
        Require(observed==reference,"Signed compact reference failed int32 boundary roundtrip");
    }
    for (const Bytes& malformed : {Bytes{0x40},Bytes{0x40,0x80,0x80,0x80,0x10},
         Bytes{0xc0,0x80,0x80,0x80,0x20},Bytes{0x40,0x80,0x80,0x80,0x80}})
        Reject([&] { DecodeAuthoredStructValue(schema,malformed,69,resolver); },"Malformed compact reference accepted");
    // The pin does not require shortest-form compact indices. Preserve its
    // signed-zero and nonminimal encodings without accepting overflow.
    Require(DecodeAuthoredStructValue(schema,Bytes{0x80},69).fields.at("object").text.empty(),"Signed zero ref changed");
    Require(DecodeAuthoredStructValue(schema,Bytes{0x40,0x00},69).fields.at("object").text.empty(),"Nonminimal zero ref changed");
    Reject([&] { DecodeAuthoredStructValue(schema,Bytes{1},69); },"Non-null reference without resolver accepted");

    const auto names=Schema({Field("Name",Kind::Name)});
    resolver.resolveName=[](std::int32_t index) {
        if (index==0) return std::string("None");
        if (index==81) return std::string("AuthoredState");
        throw std::runtime_error("Name is not in the value-source table");
    };
    Bytes none; Index(none,0); const auto nameZero=MakeAuthoredStructZero(names);
    Require(nameZero.fields.at("name").text=="None" &&
        DecodeAuthoredStructValue(names,none,69,resolver).fields.at("name").text=="None","None name semantics changed");
    Bytes state; Index(state,81);
    Require(DecodeAuthoredStructValue(names,state,69,resolver).fields.at("name").text=="AuthoredState","Name table source callback bypassed");
    for (const auto badIndex : {-1,82,std::numeric_limits<std::int32_t>::min()}) {
        Bytes bad; Index(bad,badIndex);
        Reject([&] { DecodeAuthoredStructValue(names,bad,69,resolver); },"Invalid name index accepted");
    }
    Reject([&] { DecodeAuthoredStructValue(names,none,69); },"Missing name resolver accepted");
    resolver.resolveName=[](std::int32_t) { return std::string("Authored tag: \xe9"); };
    Require(DecodeAuthoredStructValue(names,none,69,resolver).fields.at("name").text=="Authored tag: \xe9",
        "Authored name-table text was restricted to ASCII path identifiers");
    resolver.resolveName=[](std::int32_t) { return std::string("Bad\0Name",8u); };
    Reject([&] { DecodeAuthoredStructValue(names,none,69,resolver); },"NUL in name resolver output accepted");
}

void LoaderAuthorizedNone() {
    auto target=Field("Target",Kind::Object);target.classReference=true;
    target.referenceClassPath="Engine.Inventory";
    const auto schema=Schema({target,Field("Count",Kind::Int)});
    const auto payload=[](const std::int32_t reference) {
        Bytes bytes;Index(bytes,reference);I32(bytes,-73);return bytes;
    };
    std::size_t calls{};std::int32_t observed{};
    AuthoredStructResolvers resolver;
    resolver.resolveObjectOrNone=[&](const std::int32_t reference,const AuthoredStructField& field)
        ->std::optional<std::string> {
        ++calls;observed=reference;
        Require(field.key==target.key && field.classReference &&
            field.referenceClassPath=="Engine.Inventory","Loader-aware resolver lost authored object constraints");
        if(reference==1)return "ValuePackage.AdmittedExport";
        if(reference==-1)return "ValuePackage.AdmittedImport";
        if(reference==2 || reference==-2)return std::nullopt; // Source-proved exclusion, not failure.
        throw std::runtime_error("Missing or wrongly typed source reference");
    };
    for(const auto reference:{1,-1,2,-2}) {
        const auto bytes=payload(reference);const auto value=DecodeAuthoredStructValue(schema,bytes,69,resolver);
        const auto expected=reference==1?std::string("ValuePackage.AdmittedExport"):
            reference==-1?std::string("ValuePackage.AdmittedImport"):std::string{};
        Require(observed==reference && value.fields.at("target").kind==Kind::Object &&
            value.fields.at("target").text==expected && value.fields.at("count").integer==-73,
            "Loader-aware signed import/export path/None changed the following member boundary");
        for(std::size_t length=0u;length<bytes.size();++length) {
            const Bytes truncated(bytes.begin(),bytes.begin()+static_cast<std::ptrdiff_t>(length));
            Reject([&]{DecodeAuthoredStructValue(schema,truncated,69,resolver);},
                "Loader-authorized None bypassed truncated member rejection");
        }
        auto trailing=bytes;trailing.push_back(0u);
        Reject([&]{DecodeAuthoredStructValue(schema,trailing,69,resolver);},
            "Loader-aware resolver bypassed trailing byte rejection");
    }
    const auto beforeZero=calls;
    for(const Bytes& index:std::vector<Bytes>{{0u},{0x80u},{0x40u,0u}}) {
        auto bytes=index;I32(bytes,-73);const auto value=DecodeAuthoredStructValue(schema,bytes,69,resolver);
        Require(value.fields.at("target").text.empty() && value.fields.at("count").integer==-73 && calls==beforeZero,
            "Serialized zero called the loader resolver or lost its None/member boundary");
    }
    for(const auto reference:{3,-3})
        Reject([&]{DecodeAuthoredStructValue(schema,payload(reference),69,resolver);},
            "Loader-aware resolver failure was silently converted to None");
    resolver.resolveObjectOrNone=[](std::int32_t,const AuthoredStructField&)->std::optional<std::string> {
        return std::string{};
    };
    for(const auto reference:{1,-1})
        Reject([&]{DecodeAuthoredStructValue(schema,payload(reference),69,resolver);},
            "Engaged empty optional object identity was mistaken for authorized None");
    resolver.resolveObjectOrNone=[](std::int32_t,const AuthoredStructField&)->std::optional<std::string> {
        return std::nullopt;
    };
    resolver.resolveObject=[](std::int32_t,const AuthoredStructField&) {return std::string("ValuePackage.Object");};
    for(const auto reference:{0,1,-1})
        Reject([&]{DecodeAuthoredStructValue(schema,payload(reference),69,resolver);},
            "Ambiguous historical and loader-aware resolvers were accepted");
    resolver={};
    for(const auto reference:{1,-1})
        Reject([&]{DecodeAuthoredStructValue(schema,payload(reference),69,resolver);},
            "Missing resolver became implicit loader-authorized None");
    resolver.resolveObject=[](std::int32_t,const AuthoredStructField&) {return std::string{};};
    for(const auto reference:{1,-1})
        Reject([&]{DecodeAuthoredStructValue(schema,payload(reference),69,resolver);},
            "Historical empty-string resolver stopped rejecting unresolved source identities");

    resolver={};const auto largePath=std::string(1000u,'x');
    resolver.resolveObjectOrNone=[&](std::int32_t,const AuthoredStructField&)->std::optional<std::string> {return largePath;};
    AuthoredStructLimits limits;limits.identityBytes=128u;
    Reject([&]{DecodeAuthoredStructValue(schema,payload(1),69,resolver,limits);},
        "Loader-aware nonempty object identity bypassed its unchanged identity limit");
    limits={};AuthoredStructDetail::Budget measured{limits};std::vector<const AuthoredStructSchema*> stack;
    AuthoredStructDetail::Validate(schema,measured,0u,stack);
    limits.retainedBytes=measured.retained+largePath.size()-1u;
    Reject([&]{DecodeAuthoredStructValue(schema,payload(-1),69,resolver,limits);},
        "Loader-aware optional object identity bypassed retained text accounting");
    limits.retainedBytes=measured.retained+largePath.size();
    Require(DecodeAuthoredStructValue(schema,payload(-1),69,resolver,limits).fields.at("target").text==largePath,
        "Exact retained loader-aware object identity bound was rejected");
    resolver.resolveObjectOrNone=[](std::int32_t,const AuthoredStructField&)->std::optional<std::string> {return std::nullopt;};
    limits.retainedBytes=measured.retained;
    Require(DecodeAuthoredStructValue(schema,payload(2),69,resolver,limits).fields.at("target").text.empty(),
        "Authorized None charged a fabricated nonempty object identity");
    limits.inputBytes=payload(2).size()-1u;
    Reject([&]{DecodeAuthoredStructValue(schema,payload(2),69,resolver,limits);},
        "Authorized None raised the unchanged input byte limit");
    limits={};limits.valueNodes=2u;
    Reject([&]{DecodeAuthoredStructValue(schema,payload(-2),69,resolver,limits);},
        "Authorized None raised the unchanged structural node limit");
}

void ScalarsAndNested() {
    auto vector=std::make_shared<AuthoredStructSchema>(Schema({Field("Z",Kind::Float),Field("X",Kind::Float),
        Field("Y",Kind::Float)},Kind::Vector,"Core.Object.Vector"));
    auto rotator=std::make_shared<AuthoredStructSchema>(Schema({Field("Yaw",Kind::Int),Field("Roll",Kind::Int),
        Field("Pitch",Kind::Int)},Kind::Rotator,"Core.Object.Rotator"));
    auto location=Field("Location",Kind::Vector); location.nested=vector;
    auto rotation=Field("Rotation",Kind::Rotator); rotation.nested=rotator;
    auto child=std::make_shared<AuthoredStructSchema>(Schema({Field("Flag",Kind::Bool),Field("Label",Kind::Name)}));
    auto nested=Field("Child",Kind::Struct); nested.nested=child;
    const auto schema=Schema({Field("Byte",Kind::Byte),Field("Int",Kind::Int),Field("Float",Kind::Float),
        Field("Bool",Kind::Bool),location,rotation,nested,Field("String",Kind::String)});
    Bytes bytes{255u}; I32(bytes,std::numeric_limits<std::int32_t>::min()); F32(bytes,-0.0f); bytes.push_back(1u);
    F32(bytes,30.5f); F32(bytes,-0.0f); F32(bytes,-42.25f);
    I32(bytes,65536); I32(bytes,-65536); I32(bytes,-17); bytes.push_back(2u); Index(bytes,0);
    String(bytes,"Legacy: \xe9");
    AuthoredStructResolvers resolver;
    resolver.resolveName=[](std::int32_t index) {
        if (index==0) return std::string("None");
        throw std::runtime_error("Missing synthetic name");
    };
    const auto value=DecodeAuthoredStructValue(schema,bytes,69,resolver);
    Require(value.fields.at("byte").integer==255 && value.fields.at("int").integer==std::numeric_limits<std::int32_t>::min(),
        "Scalar integer wire width/sign changed");
    Require(SameFloat(value.fields.at("float").floating,-0.0f) && value.fields.at("bool").boolean,"Float negative zero or bool1 changed");
    Require(SameFloat(value.fields.at("location").vector[0],-0.0f) && value.fields.at("location").vector[1]==-42.25f &&
        value.fields.at("location").vector[2]==30.5f,"Specialized Vector ignored authored member order/names");
    Require(value.fields.at("rotation").rotation==std::array<std::int32_t,3>{-17,65536,-65536},
        "Specialized Rotator ignored authored member order/names");
    Require(!value.fields.at("child").fields.at("flag").boolean && value.fields.at("child").fields.at("label").text=="None",
        "Nested scalar schema or bool byte==1 contract changed");
    Require(value.fields.at("string").text=="Legacy: \xe9","Original code-page bytes were silently converted");
    const auto zero=MakeAuthoredStructZero(schema);
    Require(zero.fields.at("location").kind==Kind::Vector && zero.fields.at("location").vector==std::array<float,3>{} &&
        zero.fields.at("rotation").rotation==std::array<std::int32_t,3>{} &&
        zero.fields.at("child").fields.at("label").text=="None","Nested/specialized typed zero invalid");
    for (std::size_t n=0u; n<bytes.size(); ++n) {
        const Bytes truncated(bytes.begin(),bytes.begin()+static_cast<std::ptrdiff_t>(n));
        Reject([&] { DecodeAuthoredStructValue(schema,truncated,69,resolver); },"Truncated mixed/nested value accepted");
    }
    const auto bools=Schema({Field("Flag",Kind::Bool)});
    for (unsigned i=0u; i<=255u; ++i)
        Require(DecodeAuthoredStructValue(bools,Bytes{static_cast<std::uint8_t>(i)},69).fields.at("flag").boolean==(i==1u),
            "Bool struct member differs from pinned byte==1 semantics");
    const auto floats=Schema({Field("Float",Kind::Float)});
    for (const auto bits : {0x7f800000u,0xff800000u,0x7fc00000u}) {
        Bytes nonfinite; U32(nonfinite,bits);
        Reject([&] { DecodeAuthoredStructValue(floats,nonfinite,69); },"Nonfinite float accepted");
    }
}

void FixedArrayMembers() {
    auto names=Field("Names",Kind::Name);names.arrayDimension=3u;
    auto numbers=Field("Numbers",Kind::Int);numbers.arrayDimension=12u;
    auto objects=Field("Objects",Kind::Object);objects.arrayDimension=3u;
    objects.referenceClassPath="Engine.Actor";
    auto childNumbers=Field("Values",Kind::Int,"Fixture.Child");childNumbers.arrayDimension=2u;
    auto child=std::make_shared<AuthoredStructSchema>(Schema(
        {Field("Label",Kind::Name,"Fixture.Child"),childNumbers},Kind::Struct,"Fixture.Child"));
    auto children=Field("Children",Kind::Struct);children.nested=child;children.arrayDimension=2u;
    // Deliberately not map/name order. Twelve slots distinguish ascending wire
    // indexing from the envelope map's canonical lexicographic key ordering.
    const auto schema=Schema({numbers,names,objects,children,Field("After",Kind::Int)});
    Bytes bytes;
    for(std::int32_t index=0;index<12;++index)I32(bytes,100*index+7);
    for(const auto index:{1,2,0})Index(bytes,index);
    for(const auto reference:{-3,0,7})Index(bytes,reference);
    Index(bytes,4);I32(bytes,1000);I32(bytes,1001);
    Index(bytes,5);I32(bytes,2000);I32(bytes,2001);I32(bytes,-91);
    const std::string source="ActualValuePackage";
    std::vector<std::int32_t> nameReads,objectReads;
    AuthoredStructResolvers resolver;
    resolver.resolveName=[&](std::int32_t index) {
        nameReads.push_back(index);
        if(index==0)return std::string("None");
        if(index<0||index>5)throw std::runtime_error("Unresolved source-package name");
        return source+".Name"+std::to_string(index);
    };
    resolver.resolveObject=[&](std::int32_t reference,const AuthoredStructField& field) {
        Require(field.key==objects.key&&field.arrayDimension==3u&&!field.classReference&&
            field.referenceClassPath=="Engine.Actor","Fixed-array resolver lost its original element declaration");
        objectReads.push_back(reference);
        if(reference!=-3&&reference!=7)throw std::runtime_error("Unresolved source-package object");
        return source+(reference<0?".ImportedActor":".ExportedActor");
    };
    const auto decoded=DecodeAuthoredStructValue(schema,bytes,69,resolver);
    Require(nameReads==std::vector<std::int32_t>{1,2,0,4,5}&&objectReads==std::vector<std::int32_t>{-3,7},
        "ArrayDim member stream changed field-major/ascending slot order or called resolver for Object None");
    for(std::size_t index=0u;index<12u;++index) {
        const auto& number=decoded.fields.at("numbers").fields.at(std::to_string(index));
        Require(number.kind==Kind::Int&&number.integer==static_cast<std::int32_t>(100u*index+7u),
            "Fixed INT array consumed a length prefix or followed map order instead of actual wire slots");
    }
    Require(decoded.fields.at("names").kind==Kind::Struct&&decoded.fields.at("names").fields.size()==3u&&
        decoded.fields.at("names").fields.at("0").text==source+".Name1"&&
        decoded.fields.at("names").fields.at("1").text==source+".Name2"&&
        decoded.fields.at("names").fields.at("2").text=="None","Name ArrayDim slots were flattened, dropped or fabricated");
    const auto& decodedObjects=decoded.fields.at("objects").fields;
    Require(decodedObjects.at("0").text==source+".ImportedActor"&&decodedObjects.at("1").text.empty()&&
        decodedObjects.at("2").text==source+".ExportedActor","Fixed Object array lost signed reference/source resolver semantics");
    const auto& decodedChildren=decoded.fields.at("children").fields;
    Require(decodedChildren.at("0").fields.at("label").text==source+".Name4"&&
        decodedChildren.at("0").fields.at("values").fields.at("1").integer==1001&&
        decodedChildren.at("1").fields.at("label").text==source+".Name5"&&
        decodedChildren.at("1").fields.at("values").fields.at("0").integer==2000&&
        decoded.fields.at("after").integer==-91,"Nested Struct arrays lost repeated substreams or following member boundary");
    auto zero=MakeAuthoredStructZero(schema);
    for(const auto* field:{&names,&numbers,&objects,&children}) {
        const auto& envelope=zero.fields.at(AuthoredStructDetail::Fold(field->name));
        Require(envelope.kind==Kind::Struct&&envelope.fields.size()==field->arrayDimension,
            "Fixed-array typed zero did not preserve exact authored dimension");
        Require(Vm::Equal(envelope,AuthoredStructDetail::FieldZero(*field)),
            "Validated per-field zero helper disagrees with complete struct construction");
        for(std::size_t index=0u;index<field->arrayDimension;++index)
            Require(envelope.fields.count(std::to_string(index))==1u,"Fixed-array envelope omitted a canonical decimal slot");
    }
    Require(zero.fields.at("names").fields.at("2").text=="None"&&zero.fields.at("objects").fields.at("1").text.empty()&&
        zero.fields.at("after").kind==Kind::Int&&zero.fields.at("after").fields.empty(),
        "Array slots did not receive element typed zero or ArrayDim1 gained an envelope");
    Require(Vm::Equal(Vm::Coerce(decoded,zero),decoded),
        "Whole struct coercion/comparison did not preserve all fixed-array slots");
    auto malformed=decoded;malformed.fields.at("names").fields.erase("1");
    Reject([&]{Vm::Coerce(malformed,zero);},"Missing fixed-array slot passed existing recursive value shape validation");
    malformed=decoded;malformed.fields.at("names").fields.emplace("01",malformed.fields.at("names").fields.at("1"));
    malformed.fields.at("names").fields.erase("1");
    Reject([&]{Vm::Coerce(malformed,zero);},"Noncanonical fixed-array slot spelling passed recursive shape validation");
    malformed=decoded;malformed.fields.at("names").fields.at("1")=Vm::Value::Integer(7);
    Reject([&]{Vm::Coerce(malformed,zero);},"Wrong fixed-array element kind passed recursive shape validation");
    zero.fields.at("children").fields.at("0").fields.at("values").fields.at("1").integer=88;
    Require(zero.fields.at("children").fields.at("1").fields.at("values").fields.at("1").integer==0,
        "Repeated nested array zeros share mutable element storage");
    for(std::size_t length=0u;length<bytes.size();++length) {
        const Bytes truncated(bytes.begin(),bytes.begin()+static_cast<std::ptrdiff_t>(length));
        Reject([&]{DecodeAuthoredStructValue(schema,truncated,69,resolver);},"Truncated ArrayDim member stream was accepted");
    }
    auto trailing=bytes;trailing.push_back(0u);
    Reject([&]{DecodeAuthoredStructValue(schema,trailing,69,resolver);},"Trailing bytes after complete fixed-array member stream were accepted");
    resolver.resolveObject={};
    resolver.resolveObjectOrNone=[&](std::int32_t reference,const AuthoredStructField& field)->std::optional<std::string> {
        Require(field.key==objects.key&&field.arrayDimension==3u,"Optional array resolver lost real declaration identity");
        if(reference==-3)return std::nullopt;
        if(reference==7)return source+".ExportedActor";
        throw std::runtime_error("Unresolved optional source-package object");
    };
    const auto excluded=DecodeAuthoredStructValue(schema,bytes,69,resolver);
    Require(excluded.fields.at("objects").fields.at("0").text.empty()&&
        excluded.fields.at("objects").fields.at("2").text==source+".ExportedActor"&&
        excluded.fields.at("after").integer==-91,"Loader-authorized array None shifted later slots or field boundary");
}

void FixedArrayBudgets() {
    auto names=Field("Names",Kind::Name);names.arrayDimension=3u;
    const auto schema=Schema({names});
    AuthoredStructLimits limits;
    AuthoredStructDetail::Budget measured{limits};std::vector<const AuthoredStructSchema*> stack;
    AuthoredStructDetail::Validate(schema,measured,0u,stack);
    Require(measured.nodes==5u,"Fixed-array schema did not preaccount root, envelope and every element node");
    std::size_t calls{};AuthoredStructResolvers resolver;
    resolver.resolveName=[&](std::int32_t){++calls;return std::string(1000u,'x');};
    Bytes bytes{1u,1u,1u};
    limits.valueNodes=4u;
    Reject([&]{DecodeAuthoredStructValue(schema,bytes,69,resolver,limits);},"Fixed-array zero/read preallocation ignored repeated element nodes");
    Require(calls==0u,"Invalid structural budget reached array input callbacks before admission");
    limits.valueNodes=5u;limits.depth=2u;
    Reject([&]{MakeAuthoredStructZero(schema,limits);},"Fixed-array envelope level was absent from depth accounting");
    limits.depth=3u;
    Require(MakeAuthoredStructZero(schema,limits).fields.at("names").fields.at("2").text=="None",
        "Exact fixed-array node/depth boundary was rejected");
    limits={};limits.retainedBytes=measured.retained-1u;
    Reject([&]{MakeAuthoredStructZero(schema,limits);},"Fixed-array preallocation ignored envelope/slot retained storage");
    limits.retainedBytes=measured.retained;
    const auto zero=MakeAuthoredStructZero(schema,limits);
    Require(zero.fields.at("names").fields.size()==3u,"Exact fixed-array structural retained boundary was rejected");
    limits.retainedBytes=measured.retained+2999u;
    Reject([&]{DecodeAuthoredStructValue(schema,bytes,69,resolver,limits);},"Fixed-array resolved strings did not share one cumulative retained allowance");
    limits.retainedBytes=measured.retained+3000u;
    Require(DecodeAuthoredStructValue(schema,bytes,69,resolver,limits).fields.at("names").fields.at("2").text.size()==1000u,
        "Exact repeated resolved-string retained boundary was rejected");
    auto invalid=schema;invalid.fields.front().arrayDimension=0u;
    Reject([&]{MakeAuthoredStructZero(invalid);},"Fixed-array zero dimension was accepted");
    invalid.fields.front().arrayDimension=std::numeric_limits<std::size_t>::max();
    Reject([&]{MakeAuthoredStructZero(invalid);},"Huge fixed dimension was multiplied or allocated before admission");
    invalid.fields.front().arrayDimension=3u;invalid.fields.front().zero.kind=Kind::IntArray;
    Reject([&]{MakeAuthoredStructZero(invalid);},"Fixed envelope implicitly enabled dynamic INT-array member support");
    auto shortKey=Field("C",Kind::Int,"X");shortKey.key="B";shortKey.arrayDimension=12u;
    limits={};limits.identityBytes=1u;
    const auto keys=Schema({shortKey},Kind::Struct,"A");
    Reject([&]{MakeAuthoredStructZero(keys,limits);},"Two-digit canonical slot key bypassed identity admission");
    for(const auto specialized:{Kind::Vector,Kind::Rotator}) {
        auto first=Field(specialized==Kind::Vector?"X":"Pitch",specialized==Kind::Vector?Kind::Float:Kind::Int);
        first.arrayDimension=2u;
        const auto invalidSpecial=Schema({first,Field(specialized==Kind::Vector?"Y":"Yaw",first.zero.kind),
            Field(specialized==Kind::Vector?"Z":"Roll",first.zero.kind)},specialized);
        Reject([&]{MakeAuthoredStructZero(invalidSpecial);},"Specialized Vector/Rotator admitted a nonscalar channel envelope");
    }
    auto values=Field("Values",Kind::Int,"Fixture.Child");values.arrayDimension=2u;
    auto child=std::make_shared<AuthoredStructSchema>(Schema({values},Kind::Struct,"Fixture.Child"));
    auto repeated=Field("Children",Kind::Struct);repeated.nested=child;repeated.arrayDimension=2u;
    const auto nested=Schema({repeated});
    limits={};AuthoredStructDetail::Budget nestedSize{limits};stack.clear();
    AuthoredStructDetail::Validate(nested,nestedSize,0u,stack);
    Require(nestedSize.nodes==10u,"Repeated nested schemas were charged once instead of for every constructed occurrence");
    limits.valueNodes=9u;
    Reject([&]{MakeAuthoredStructZero(nested,limits);},"Nested fixed-array aggregate node multiplication was skipped");
    limits.valueNodes=10u;limits.depth=4u;
    Reject([&]{MakeAuthoredStructZero(nested,limits);},"Nested fixed-array envelope depth was skipped");
    limits.depth=5u;
    Require(MakeAuthoredStructZero(nested,limits).fields.at("children").fields.at("1").fields.at("values").fields.size()==2u,
        "Exact nested fixed-array node/depth boundary was rejected");
    auto cycle=std::make_shared<AuthoredStructSchema>();cycle->path="Fixture.Cycle";cycle->name="Cycle";
    auto self=Field("Self",Kind::Struct,"Fixture.Cycle");self.nested=cycle;self.arrayDimension=2u;cycle->fields.push_back(self);
    Reject([&]{MakeAuthoredStructZero(*cycle);},"Fixed-array envelope concealed a recursive nested schema");
    cycle->fields.clear();
    limits={};AuthoredStructDetail::Budget retained{limits};AuthoredStructDetail::RetainedValue(zero,retained);
    Require(retained.nodes==5u,"Batched retained-value walk omitted fixed-array envelope slots");
}

void Strings() {
    const auto schema=Schema({Field("Text",Kind::String),Field("After",Kind::Int)});
    Bytes bytes; String(bytes,"Cafe: \xe9"); I32(bytes,-3);
    AuthoredStructResolvers resolver; std::string observed;
    resolver.decodeString=[&](std::string_view raw) {
        observed=std::string(raw); return std::string("Cafe: \xc3\xa9");
    };
    const auto value=DecodeAuthoredStructValue(schema,bytes,69,resolver);
    Require(observed=="Cafe: \xe9" && value.fields.at("text").text=="Cafe: \xc3\xa9" &&
        value.fields.at("after").integer==-3,"Explicit source code-page callback or member boundary failed");
    Bytes legacy; String(legacy,"Old",false); I32(legacy,10);
    Require(DecodeAuthoredStructValue(schema,legacy,63).fields.at("text").text=="Old","Pre64 ASCII-Z member failed");
    Bytes noTerminator; String(noTerminator,"ABC",true,false); I32(noTerminator,55);
    Require(DecodeAuthoredStructValue(schema,noTerminator,69).fields.at("text").text=="ABC" &&
        DecodeAuthoredStructValue(schema,noTerminator,69).fields.at("after").integer==55,
        "Pinned appended-terminator string semantics changed");
    Bytes embedded; Index(embedded,5); embedded.insert(embedded.end(),{'A',0,'B','C',0}); I32(embedded,-50);
    Require(DecodeAuthoredStructValue(schema,embedded,69).fields.at("text").text=="A" &&
        DecodeAuthoredStructValue(schema,embedded,69).fields.at("after").integer==-50,
        "Pinned C-string prefix or full length consumption changed");
    Bytes empty; Index(empty,0); I32(empty,90);
    Require(DecodeAuthoredStructValue(schema,empty,69).fields.at("text").text.empty(),"Zero-length package string changed");
    Bytes negative; Index(negative,-2); negative.insert(negative.end(),{0u,0u,0u,0u}); I32(negative,0);
    Reject([&] { DecodeAuthoredStructValue(schema,negative,69); },"Unsupported negative Unicode string size accepted");
    const auto only=Schema({Field("Text",Kind::String)});
    AuthoredStructLimits limits; limits.stringBytes=3u;
    Bytes exact; String(exact,"ABC");
    Require(DecodeAuthoredStructValue(only,exact,69,{},limits).fields.at("text").text=="ABC","Boundary-sized terminated string rejected");
    Bytes longText; String(longText,"ABCD");
    Reject([&] { DecodeAuthoredStructValue(only,longText,69,{},limits); },"Encoded string limit ignored");
    Reject([&] { DecodeAuthoredStructValue(only,Bytes{'A','B','C','D',0u},63,{},limits); },"ASCII-Z string limit ignored");
    Bytes missing{'A','B','C'};
    Reject([&] { DecodeAuthoredStructValue(only,missing,63); },"Unterminated ASCII-Z string accepted");
    resolver.decodeString=[](std::string_view) { return std::string("Oversized"); };
    Reject([&] { DecodeAuthoredStructValue(only,exact,69,resolver,limits); },"Decoded string expansion limit ignored");
    resolver.decodeString=[](std::string_view) { return std::string("A\0B",3u); };
    Reject([&] { DecodeAuthoredStructValue(only,exact,69,resolver); },"Decoded text NUL accepted");
}

void InvalidSchemasAndBudgets() {
    const auto scalar=Schema({Field("A",Kind::Int)});
    Bytes four; I32(four,17);
    Reject([&] { DecodeAuthoredStructValue(scalar,nullptr,4u,69); },"Null nonempty input accepted");
    auto invalid=scalar; invalid.fields.push_back(Field("a",Kind::Int,"Different.Declaration"));
    Reject([&] { MakeAuthoredStructZero(invalid); },"Canonical duplicate member names accepted");
    invalid=scalar; auto other=Field("B",Kind::Int); other.key="fixture.record.a"; invalid.fields.push_back(other);
    Reject([&] { MakeAuthoredStructZero(invalid); },"Canonical duplicate qualified identities accepted");
    invalid=scalar; invalid.path.clear();
    Reject([&] { MakeAuthoredStructZero(invalid); },"Empty schema path accepted");
    invalid=scalar; invalid.name.clear();
    Reject([&] { MakeAuthoredStructZero(invalid); },"Empty declaration export Name accepted");
    invalid=scalar; invalid.fields.front().name="bad name";
    Reject([&] { MakeAuthoredStructZero(invalid); },"Invalid schema identity accepted");
    AuthoredStructLimits shortIdentity; shortIdentity.identityBytes=3u;
    Reject([&] { MakeAuthoredStructZero(scalar,shortIdentity); },"Independent metadata identity budget ignored");
    invalid=scalar; invalid.fields.front().arrayDimension=2u;
    Reject([&] { DecodeAuthoredStructValue(invalid,four,69); },"Truncated nested fixed array was silently accepted");
    invalid.fields.front().arrayDimension=0u;
    Reject([&] { MakeAuthoredStructZero(invalid); },"Zero array dimension accepted");
    invalid=scalar; invalid.fields.front().zero.kind=Kind::Nothing;
    Reject([&] { MakeAuthoredStructZero(invalid); },"Unsupported dynamic-array/member kind accepted");
    invalid=scalar; invalid.fields.front().zero.kind=Kind::Struct;
    Reject([&] { MakeAuthoredStructZero(invalid); },"Missing nested schema accepted");
    invalid=scalar; invalid.fields.front().nested=std::make_shared<AuthoredStructSchema>(scalar);
    Reject([&] { MakeAuthoredStructZero(invalid); },"Scalar with nested schema accepted");
    invalid=scalar; invalid.fields.front().classReference=true;
    Reject([&] { MakeAuthoredStructZero(invalid); },"Class reference constraint on integer accepted");
    invalid=scalar; invalid.fields.front().referenceClassPath="Engine.Actor";
    Reject([&] { MakeAuthoredStructZero(invalid); },"Object type constraint on integer accepted");
    invalid=scalar; invalid.fields.clear();
    Reject([&] { MakeAuthoredStructZero(invalid); },"Empty struct schema accepted");
    invalid=scalar; invalid.kind=Kind::Int;
    Reject([&] { MakeAuthoredStructZero(invalid); },"Scalar top-level schema accepted");
    auto specialized=Schema({Field("X",Kind::Float),Field("Y",Kind::Float),Field("Other",Kind::Float)},Kind::Vector);
    Reject([&] { MakeAuthoredStructZero(specialized); },"Invalid specialized vector field name accepted");
    specialized.fields[2].name="Z"; specialized.fields[2].zero.kind=Kind::Int;
    Reject([&] { MakeAuthoredStructZero(specialized); },"Invalid specialized vector field kind accepted");
    specialized.fields.pop_back();
    Reject([&] { MakeAuthoredStructZero(specialized); },"Incomplete specialized vector accepted");
    auto cycle=std::make_shared<AuthoredStructSchema>(); cycle->path="Fixture.Cycle"; cycle->name="Cycle";
    auto recursive=Field("Child",Kind::Struct); recursive.nested=cycle; cycle->fields.push_back(recursive);
    Reject([&] { MakeAuthoredStructZero(*cycle); },"Cyclic nested metadata accepted");
    cycle->fields.clear(); // Break test fixture's shared_ptr cycle.

    AuthoredStructLimits limits; limits.inputBytes=3u;
    Reject([&] { DecodeAuthoredStructValue(scalar,four,69,{},limits); },"Input-byte budget ignored");
    limits={}; limits.valueNodes=1u;
    Reject([&] { MakeAuthoredStructZero(scalar,limits); },"Value-node budget ignored");
    limits.valueNodes=2u;
    Require(MakeAuthoredStructZero(scalar,limits).fields.at("a").integer==0,"Exact node budget rejected");
    limits={}; limits.depth=1u;
    Reject([&] { MakeAuthoredStructZero(scalar,limits); },"Value-depth budget ignored");
    limits.depth=2u;
    Require(DecodeAuthoredStructValue(scalar,four,69,{},limits).fields.at("a").integer==17,"Exact depth budget rejected");
    limits={}; limits.retainedBytes=sizeof(Vm::Value);
    Reject([&] { MakeAuthoredStructZero(scalar,limits); },"Retained structural byte budget ignored");
    const auto text=Schema({Field("Text",Kind::String)}); Bytes encoded; String(encoded,std::string(1000u,'x'));
    limits={}; AuthoredStructDetail::Budget measured{limits};
    // The public decode bound includes structure plus callback-expanded strings.
    std::vector<const AuthoredStructSchema*> stack;
    AuthoredStructDetail::Validate(text,measured,0u,stack);
    limits.retainedBytes=measured.retained+999u;
    Reject([&] { DecodeAuthoredStructValue(text,encoded,69,{},limits); },"Aggregate decoded-text budget ignored");
    limits.retainedBytes=measured.retained+1000u;
    Require(DecodeAuthoredStructValue(text,encoded,69,{},limits).fields.at("text").text.size()==1000u,
        "Exact retained text bound rejected");

    // Both identity strings and reference constraints consume retained budget.
    limits={}; AuthoredStructDetail::Budget small{limits}; stack.clear();
    AuthoredStructDetail::Validate(scalar,small,0u,stack);
    auto longPath=scalar; longPath.path+=std::string(1000u,'x');
    limits.retainedBytes=small.retained+999u;
    Reject([&] { MakeAuthoredStructZero(longPath,limits); },"Schema path storage was not charged");
    longPath=scalar; longPath.name+=std::string(1000u,'x');
    Reject([&] { MakeAuthoredStructZero(longPath,limits); },"Declaration Name storage was not charged");
    auto object=Schema({Field("Object",Kind::Object)});
    limits={}; AuthoredStructDetail::Budget objectSize{limits}; stack.clear();
    AuthoredStructDetail::Validate(object,objectSize,0u,stack);
    object.fields.front().referenceClassPath=std::string(1000u,'x');
    limits.retainedBytes=objectSize.retained+999u;
    Reject([&] { MakeAuthoredStructZero(object,limits); },"Reference constraint storage was not charged");
    limits.retainedBytes=objectSize.retained+1000u;
    Require(MakeAuthoredStructZero(object,limits).fields.at("object").text.empty(),
        "Exact reference constraint storage bound rejected");
}

void DeclarationTagNames() {
    auto schema=Schema({Field("A",Kind::Int)},Kind::Struct,"Fixture.Container.TestStruct");
    // Same independent Name/FriendlyName pair as the descriptor fixture.
    const std::string friendlyName="Friendly";
    ValidateAuthoredStructTag(schema,"TestStruct");
    ValidateAuthoredStructTag(schema,"tEsTsTrUcT");
    Require(schema.name!=friendlyName,"Tag control did not distinguish independent names");
    Reject([&] { ValidateAuthoredStructTag(schema,friendlyName); },"FriendlyName accepted as export Name tag");
    Reject([&] { ValidateAuthoredStructTag(schema,schema.path); },"Qualified path accepted as export Name tag");
    Reject([&] { ValidateAuthoredStructTag(schema,""); },"Empty struct tag accepted");
    // Do not derive Name by splitting a display path: a literal name may itself
    // contain a dot; the runtime fills it directly from the export name table.
    schema.name="Literal.Name";
    ValidateAuthoredStructTag(schema,"literal.name");
    Reject([&] { ValidateAuthoredStructTag(schema,"Name"); },"Path-derived leaf accepted as literal Name");
}

void RetainedReadBudgets() {
    AuthoredStructLimits limits;
    auto value=Vm::Value::Text(Kind::String,std::string(1000u,'x'));
    AuthoredStructDetail::Budget measured{limits};
    AuthoredStructDetail::RetainedValue(value,measured);
    limits.retainedBytes=2u*measured.retained;
    AuthoredStructDetail::Budget aggregate{limits};
    AuthoredStructDetail::RetainedValue(value,aggregate);
    AuthoredStructDetail::RetainedValue(value,aggregate);
    Require(aggregate.nodes==2u,"Read-slot cumulative value nodes were lost");
    Reject([&] {AuthoredStructDetail::RetainedValue(value,aggregate);},"Read-slot retained budget reset per value");
    limits={};limits.valueNodes=2u;
    AuthoredStructDetail::Budget nodes{limits};
    AuthoredStructDetail::RetainedValue(value,nodes);AuthoredStructDetail::RetainedValue(value,nodes);
    Reject([&] {AuthoredStructDetail::RetainedValue(value,nodes);},"Read-slot aggregate node budget ignored");
    Vm::Value nested;nested.kind=Kind::Struct;nested.fields.emplace("child",value);
    limits={};limits.depth=1u;AuthoredStructDetail::Budget depth{limits};
    Reject([&] {AuthoredStructDetail::RetainedValue(nested,depth);},"Read-slot nested depth budget ignored");
}
} // namespace

int main() {
    try {
        InventoryMembers(); CompactReferences(); LoaderAuthorizedNone(); ScalarsAndNested(); FixedArrayMembers(); FixedArrayBudgets();
        Strings(); InvalidSchemasAndBudgets();
        DeclarationTagNames();
        RetainedReadBudgets();
        std::cout << "Authored struct value tests passed: " << checks << " checks, " << rejections << " rejections\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Authored struct value test failed after " << checks << " checks: " << error.what() << '\n';
        return 1;
    }
}
