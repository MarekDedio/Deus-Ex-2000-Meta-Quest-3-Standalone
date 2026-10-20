#include "Precomp.h"
#include "surreal_portable_package_tables.h"
#include "portable_model_geometry.h"
#include "quest_conversation_bind.h"
#include "quest_native_work.h"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

// Static original-data audit ONLY. No UObject/runtime/VM initialization, native
// DLL loading, transient list construction or presentation dialogue index.
namespace {
namespace fs=std::filesystem;
void Require(bool value,const std::string& why) {if(!value)throw std::runtime_error(why);}
std::string Lower(std::string value) {
    for(auto& c:value)if(c>='A'&&c<='Z')c=static_cast<char>(c-'A'+'a');
    return value;
}
struct Source {PortablePackageTables table;std::string name;std::map<std::string,std::size_t> exports;};
struct Ref {Source* source{};std::size_t index{};};
struct Value {std::string text,origin;std::uint8_t tag{};std::int32_t integer{};};
struct Audit {
    std::map<std::string,Source> sources;
    std::map<std::string,PortableClassDescriptor> classes;
    std::map<std::string,PortablePropertyDescriptor> fields;
    std::map<std::string,std::set<std::int32_t>> classFields;
    std::map<std::string,PortablePropertyStream> properties;
    explicit Audit(const fs::path& root) {
        for(const auto* name:{"Core","Engine","Extension","ConSys","DeusEx","DeusExConversations","DeusExConText","00_Training"}) {
            const bool map=std::string_view(name)=="00_Training";
            const auto path=root/(map?"Maps":"System")/(std::string(name)+(map?".dx":".u"));
            Require(fs::is_regular_file(path)&&fs::file_size(path)<512u*1024u*1024u,"Missing/oversized allowed source: "+path.string());
            Source source{LoadPortablePackageTables(path.string()),name,{}};
            Require(source.table.version==68u&&source.table.licenseeMode==0u,"Unexpected original package layout: "+path.string());
            for(std::size_t i=0u;i<source.table.exports.size();++i)
                Require(source.exports.emplace(Lower(GetPortableObjectPath(source.table,static_cast<std::int32_t>(i+1u))),i).second,
                    "Duplicate qualified source export");
            std::cout<<"SOURCE "<<name<<" exports="<<source.table.exports.size()<<" path="<<path.string()<<'\n';
            sources.emplace(Lower(name),std::move(source));
        }
    }
    std::string Path(Ref ref) const {
        const auto value=GetPortableObjectPath(ref.source->table,static_cast<std::int32_t>(ref.index+1u));
        return Lower(value)==Lower(ref.source->name)?value:ref.source->name+'.'+value;
    }
    std::string Qualified(Source& source,std::int32_t reference) const {
        if(!reference)return {};
        Require(reference<0?static_cast<std::size_t>(-static_cast<std::int64_t>(reference))<=source.table.imports.size():
            static_cast<std::size_t>(reference)<=source.table.exports.size(),"Source reference is outside its table");
        const auto path=GetPortableObjectPath(source.table,reference);
        if(reference<0||Lower(path)==Lower(source.name))return path;
        return source.name+'.'+path;
    }
    Ref Find(const std::string& path) {
        const auto dot=path.find('.');Require(dot!=std::string::npos,"Qualified source path required: "+path);
        const auto source=sources.find(Lower(path.substr(0u,dot)));
        Require(source!=sources.end(),"Source outside explicit static allowlist: "+path);
        const auto found=source->second.exports.find(Lower(path.substr(dot+1u)));
        Require(found!=source->second.exports.end(),"Original export not found: "+path);
        return {&source->second,found->second};
    }
    std::string Class(Ref ref) const {
        const auto reference=ref.source->table.exports.at(ref.index).ObjClass;
        return reference?Qualified(*ref.source,reference):"Core.Class";
    }
    const PortableClassDescriptor& Descriptor(const std::string& path) {
        auto found=classes.find(Lower(path));
        if(found==classes.end()) {
            const auto ref=Find(path);Require(!ref.source->table.exports[ref.index].ObjClass,"Expected actual serialized UClass: "+path);
            found=classes.emplace(Lower(path),LoadPortableClassDescriptor(ref.source->table,ref.index)).first;
        }
        return found->second;
    }
    std::vector<std::string> Ancestry(std::string path) {
        std::vector<std::string> result;std::set<std::string> visited;
        while(!path.empty()) {
            Require(result.size()<128u&&visited.insert(Lower(path)).second,"Invalid static class ancestry: "+path);
            const auto ref=Find(path);static_cast<void>(Descriptor(path));result.push_back(Path(ref));
            path=Qualified(*ref.source,ref.source->table.exports[ref.index].ObjBase);
        }
        return result;
    }
    bool IsA(Ref ref,const std::string& type) {
        const auto ancestry=Ancestry(Class(ref));
        return std::any_of(ancestry.begin(),ancestry.end(),[&](const auto& path){return Lower(path)==Lower(type);});
    }
    const std::vector<PortableTaggedProperty>& Properties(Ref ref) {
        const auto path=Lower(Path(ref));auto found=properties.find(path);
        if(found==properties.end())found=properties.emplace(path,LoadPortableExportProperties(ref.source->table,ref.index)).first;
        return found->second.properties;
    }
    Value Decode(Ref owner,const PortableTaggedProperty& property,const std::string& origin) {
        Value result;result.origin=origin;result.tag=property.type;
        if(property.type==13u||property.type==7u)result.text=DecodePortableVmStringProperty(property);
        else if(property.type==5u||property.type==8u)
            result.text=Qualified(*owner.source,DecodePortableObjectReference(property));
        else if(property.type==6u)result.text=DecodePortableNameProperty(owner.source->table,property);
        else if(property.type==2u) {
            Require(property.value.size()==4u,"Invalid authored Int value");std::memcpy(&result.integer,property.value.data(),4u);
        }else throw std::runtime_error("Unsupported active static audit value tag at "+origin);
        Require(result.text.size()<=8192u&&result.text.find('\0')==std::string::npos,"Unbounded static audit text");
        return result;
    }
    PortablePropertyDescriptor Field(const std::string& path) {
        if(const auto found=fields.find(Lower(path));found!=fields.end())return found->second;
        const auto ref=Find(path);const auto property=LoadPortablePropertyDescriptor(ref.source->table,ref.index);
        Require(!property.baseField&&property.arrayDimension==1u,"Expected scalar direct binding/list property: "+path);
        Require(Lower(Class(ref))==Lower("Core."+property.type),"Property descriptor does not match actual UClass");
        const auto ownerPath=Qualified(*ref.source,ref.source->table.exports.at(ref.index).ObjOuter);
        const auto owner=Find(ownerPath);
        if(!classFields.contains(Lower(ownerPath))) {
            std::set<std::int32_t> members;auto at=Descriptor(ownerPath).state.children;
            while(at) {
                Require(at>0&&static_cast<std::size_t>(at)<=owner.source->table.exports.size()&&members.size()<65'536u&&members.insert(at).second,
                    "Invalid actual UClass.Children/Next chain");
                Require(owner.source->table.exports.at(static_cast<std::size_t>(at-1)).ObjOuter==static_cast<std::int32_t>(owner.index+1u),
                    "Actual declaration chain contains foreign member");
                const auto index=static_cast<std::size_t>(at-1);
                const auto kind=Lower(Class({owner.source,index}));
                if(kind=="core.function")at=LoadPortableFunctionScript(owner.source->table,index).nextField;
                else if(kind=="core.state")at=LoadPortableStateDescriptor(owner.source->table,index).nextField;
                else if(kind.ends_with("property"))at=LoadPortablePropertyDescriptor(owner.source->table,index).nextField;
                else at=LoadPortableFieldLinks(owner.source->table,index).nextField;
            }
            classFields.emplace(Lower(ownerPath),std::move(members));
        }
        Require(owner.source==ref.source&&classFields.at(Lower(ownerPath)).contains(static_cast<std::int32_t>(ref.index+1u)),
            "Property is not a genuine declaring UClass child");
        fields.emplace(Lower(path),property);
        return property;
    }
    Value Read(Ref actor,const std::string& declaration,bool classDefaults=false) {
        const auto property=Field(declaration);const auto leaf=declaration.substr(declaration.find_last_of('.')+1u);
        const auto tagged=[&](Ref owner,const std::vector<PortableTaggedProperty>& values,const std::string& origin)->std::optional<Value> {
            const PortableTaggedProperty* selected{};
            for(const auto& value:values)if(Lower(value.name.ToString())==Lower(leaf)&&value.arrayIndex==0u) {
                Require(!selected,"Duplicate active authored tag: "+origin+'.'+leaf);selected=&value;
            }
            return selected?std::optional<Value>(Decode(owner,*selected,origin)):std::nullopt;
        };
        const auto chain=Ancestry(classDefaults?Path(actor):Class(actor));
        const auto owner=declaration.substr(0u,declaration.find_last_of('.'));
        Require(std::any_of(chain.begin(),chain.end(),[&](const auto& path){return Lower(path)==Lower(owner);}),
            "Static field declaration is outside object ancestry: "+Path(actor)+" / "+declaration);
        if(!classDefaults)if(const auto value=tagged(actor,Properties(actor),"actor:"+Path(actor)))return *value;
        for(const auto& path:chain) {
            const auto ref=Find(path);
            if(const auto value=tagged(ref,Descriptor(path).defaults,"default:"+path))return *value;
        }
        Value result;result.origin="declared-zero:"+declaration;
        if(property.type=="StrProperty")result.tag=13u;
        else if(property.type=="ObjectProperty")result.tag=5u;
        else if(property.type=="IntProperty")result.tag=2u;
        else if(property.type=="NameProperty"){result.tag=6u;result.text="None";}
        else throw std::runtime_error("No zero policy for active static declaration: "+declaration);
        return result;
    }
    void PrintField(const std::string& path) {
        const auto ref=Find(path);const auto property=Field(path);
        std::cout<<"FIELD "<<path<<" type="<<property.type<<" dim="<<property.arrayDimension<<" flags=0x"<<std::hex<<property.flags
            <<" rf=0x"<<static_cast<std::uint32_t>(ref.source->table.exports[ref.index].ObjFlags)<<std::dec
            <<" outer="<<Qualified(*ref.source,ref.source->table.exports[ref.index].ObjOuter)
            <<" constraint="<<Qualified(*ref.source,property.referencedType)<<'\n';
    }
    void PrintClass(const std::string& path) {
        std::cout<<"ANCESTRY "<<path;
        for(const auto& parent:Ancestry(path))std::cout<<" -> "<<parent;
        std::cout<<'\n';const auto ref=Find(path);const auto& cls=Descriptor(path);
        std::cout<<"CLASS "<<path<<" classFlags=0x"<<std::hex<<cls.classFlags<<" rf=0x"
            <<static_cast<std::uint32_t>(ref.source->table.exports[ref.index].ObjFlags)<<std::dec
            <<" within="<<(cls.classWithinReference?Qualified(*ref.source,*cls.classWithinReference):"<not serialized>")<<'\n';
    }
    void PrintFunction(const std::string& path) {
        const auto ref=Find(path);Require(Lower(Class(ref))=="core.function","Expected actual native Function: "+path);
        const auto function=LoadPortableFunctionScript(ref.source->table,ref.index);
        std::cout<<"FUNCTION "<<path<<" native="<<function.nativeIndex<<" flags=0x"<<std::hex<<function.functionFlags<<std::dec
            <<" logical="<<function.logicalSize<<" base="<<Qualified(*ref.source,function.baseField)
            <<" children="<<Qualified(*ref.source,function.children)<<'\n';
    }
    std::vector<std::pair<std::string,Ref>> Walk(Ref owner,const std::string& field,const std::string& objectType) {
        std::vector<std::pair<std::string,Ref>> result;std::set<std::string> visited;
        auto item=Read(owner,field).text;
        while(!item.empty()) {
            Require(result.size()<65'536u&&visited.insert(Lower(item)).second,"Malformed authored ConItem.Next chain");
            const auto ref=Find(item);Require(IsA(ref,"ConSys.ConItem"),"Authored list link is not ConSys.ConItem");
            const auto target=Read(ref,"ConSys.ConItem.ConObject").text;
            Require(!target.empty(),"Authored list link has no ConObject");
            const auto object=Find(target);Require(IsA(object,objectType),"Authored ConObject violates expected list target type");
            result.emplace_back(Path(ref),object);item=Read(ref,"ConSys.ConItem.Next").text;
        }
        return result;
    }
    void Protocol(Ref actor,bool defaults,Ref list,const std::vector<std::string>& predicted) {
        using Id=std::uintptr_t;
        static_assert(sizeof(Id)>=8u);
        std::vector<Ref> references;std::map<std::string,Id> ids;
        const auto identify=[&](const std::string& path) {
            if(path.empty())return Id{};
            if(const auto found=ids.find(Lower(path));found!=ids.end())return found->second;
            const auto ref=Find(path);references.push_back(ref);const auto id=static_cast<Id>(references.size());
            ids.emplace(Lower(path),id);return id;
        };
        const auto ref=[&](Id id){Require(id&&id<=references.size(),"Mock provider received non-source identity");return references.at(id-1u);};
        const auto actorId=identify(Path(actor)),listId=identify(Path(list));
        Require(Read(actor,"Engine.Actor.ConListItems",defaults).text.empty(),"Static protocol requires genuinely empty authored head");
        Require(list.source->table.exports.at(list.index).ObjOuter==0,
            "Static protocol's package-Outer witness is not top-level");
        const Id outer=0x100000000ull;
        struct Node {Id con{},next{};std::uint32_t flags{};bool live{true};};
        std::map<Id,Node> nodes;std::map<Id,std::int32_t> counts,initialCounts;
        Id head{},next=outer+1u;
        QuestVr::NativeWork::Budget work;
        QuestVr::OriginalConversationBindQueries q;
        q.work=[&]{work.Step();};
        q.sourceHead=[&](Id id){return identify(Read(ref(id),"ConSys.ConversationList.conversations").text);};
        q.sourceNext=[&](Id id){return identify(Read(ref(id),"ConSys.ConItem.Next").text);};
        q.sourceConversation=[&](Id id){return identify(Read(ref(id),"ConSys.ConItem.ConObject").text);};
        q.actorHead=[&](Id id){Require(id==actorId,"Static protocol lost Actor receiver");return head;};
        const auto live=[&](Id id)->Node& {auto& node=nodes.at(id);Require(node.live,"Static protocol read deleted mock node");return node;};
        q.nodeNext=[&](Id id){return live(id).next;};q.nodeConversation=[&](Id id){return live(id).con;};
        q.outer=[&](Id id){Require(id==listId,"Static protocol used Actor Outer");return outer;};
        q.conversationName=[&](Id id){return Read(ref(id),"ConSys.Conversation.conName").text;};
        q.ownerName=[&](Id id){return Read(ref(id),"ConSys.Conversation.conOwnerName").text;};
        q.bindName=[&](Id id){Require(id==actorId,"Wrong static BindName receiver");return Read(actor,"Engine.Actor.BindName",defaults).text;};
        q.barkName=[&](Id id){Require(id==actorId,"Wrong static BarkBindName receiver");return Read(actor,"Engine.Actor.BarkBindName",defaults).text;};
        q.ownerRefCount=[&](Id id){
            if(!counts.contains(id)) {
                const auto value=Read(ref(id),"ConSys.Conversation.ownerRefCount");Require(value.tag==2u,"Actual count is not Int");
                counts.emplace(id,value.integer);initialCounts.emplace(id,value.integer);
            }return counts.at(id);
        };
        q.writeOwnerRefCount=[&](Id id,std::int32_t value){counts.at(id)=value;};
        q.destroyNode=[&](Id id){live(id).live=false;};
        q.createNode=[&](Id parent,std::uint32_t flags){
            Require(parent==outer&&flags==0u,"Actual-source protocol requested wrong allocation envelope");
            const auto id=next++;nodes.emplace(id,Node{});return id; // MOCK, never a UObject allocation.
        };
        q.flags=[&](Id id){return live(id).flags;};q.writeFlags=[&](Id id,std::uint32_t flags){live(id).flags=flags;};
        q.writeNodeConversation=[&](Id id,Id con){live(id).con=con;};q.writeNodeNext=[&](Id id,Id value){live(id).next=value;};
        q.writeActorHead=[&](Id id,Id value){Require(id==actorId,"Static head store lost Actor");head=value;};
        const auto check=[&] {
            std::vector<std::string> actual;std::map<Id,std::size_t> frequencies;
            for(auto at=head;at;at=live(at).next) {
                Require(actual.size()<65'536u&&live(at).flags==4u,"Static binding output flags/cycle differs");
                const auto con=live(at).con;actual.push_back(Path(ref(con)));++frequencies[con];
            }
            auto expected=predicted;std::reverse(expected.begin(),expected.end());
            Require(actual==expected,"Actual-source coordinator differs from independent accepted/reversed list");
            for(const auto& [id,count]:counts)Require(std::bit_cast<std::uint32_t>(count)==
                std::bit_cast<std::uint32_t>(initialCounts.at(id))+frequencies[id],"Actual-source rebind reference balance differs");
        };
        const auto first=QuestVr::BindOriginalConversations(listId,actorId,q);check();const auto firstEnd=next;
        const auto second=QuestVr::BindOriginalConversations(listId,actorId,q);check();
        for(const auto& [id,node]:nodes)Require(node.live==(id>=firstEnd),"Actual-source rebind reused or retained purged mock node");
        Require(first.accepted==predicted.size()&&second.accepted==predicted.size()&&second.purged==first.created&&
            work.Read().used==first.steps+second.steps,"Actual-source coordinator lost repeated count/shared work");
        std::cout<<"ACTUAL_SOURCE_MOCK_PROTOCOL "<<Path(actor)<<" visits="<<first.visited<<" accepted="<<first.accepted
            <<" rebindPurged="<<second.purged<<" sharedWork="<<work.Read().used<<" sourceOuterPackage="<<list.source->name
            <<"; actual static fields, MOCK nodes/count stores/deletion, NOT runtime allocation/GC/save/startup\n";
    }
    void Run() {
        for(const auto* path:{"ConSys.ConObject","ConSys.ConListItem","ConSys.ConItem","ConSys.ConversationList",
            "ConSys.ConversationMissionList","ConSys.Conversation","DeusEx.Plant2","DeusEx.DeusExDecoration",
            "DeusEx.ScriptedPawn","DeusEx.DeusExPlayer"})PrintClass(path);
        for(const auto* path:{"Engine.Actor.BindName","Engine.Actor.BarkBindName","Engine.Actor.ConListItems",
            "ConSys.ConListItem.con","ConSys.ConListItem.Next","ConSys.ConItem.ConObject","ConSys.ConItem.Next",
            "ConSys.ConversationList.conversations","ConSys.ConversationList.missionNumber","ConSys.ConversationList.missionDescription",
            "ConSys.ConversationMissionList.missions","ConSys.Conversation.conOwnerName","ConSys.Conversation.conName",
            "ConSys.Conversation.ownerRefCount","DeusEx.DeusExLevelInfo.MissionNumber",
            "DeusEx.DeusExLevelInfo.ConversationPackage"})PrintField(path);
        for(const auto* path:{"DeusEx.DeusExPlayer.ConBindEvents","DeusEx.DeusExDecoration.ConBindEvents","DeusEx.ScriptedPawn.ConBindEvents"})PrintFunction(path);
        auto& map=sources.at("00_training");std::vector<Ref> actors;std::optional<Ref> missionInfo;
        for(const auto reference:ReadPortableLevel68ActorOrder(map.table)) {
            if(!reference)continue;
            Require(reference>0&&static_cast<std::size_t>(reference)<=map.table.exports.size(),"Invalid actual first-Training native Actor slot");
            const Ref actor{&map,static_cast<std::size_t>(reference-1)};actors.push_back(actor);
            if(IsA(actor,"DeusEx.DeusExLevelInfo")) {
                if(!missionInfo)missionInfo=actor; // Original chooses FIRST, no bDeleteMe filter.
            }
        }
        Require(missionInfo.has_value(),"First Training lacks its actual mission-info actor");
        const auto mission=Read(*missionInfo,"DeusEx.DeusExLevelInfo.MissionNumber");Require(mission.tag==2u,"MissionNumber is not Int");
        std::cout<<"MAP_ONLY 00_Training actualActors="<<actors.size()<<" info="<<Path(*missionInfo)
            <<" mission="<<mission.integer<<" origin="<<mission.origin<<'\n';
        const auto convPackage=Read(*missionInfo,"DeusEx.DeusExLevelInfo.ConversationPackage");
        Require(convPackage.tag==13u&&mission.integer>=0,"Actual first Training does not select a nonnegative mission/package");
        auto prefix=QuestVr::UnrealString::Decode(convPackage.text);
        prefix.resize(QuestVr::UnrealString::CString(prefix).size());
        if(QuestVr::UnrealString::EqualInsensitive(prefix,u"DeusExConversations"))prefix=u"DeusExCon";
        auto number=std::to_string(mission.integer);if(number.size()<2u)number.insert(number.begin(),'0');
        const auto requested=QuestVr::UnrealString::Encode(prefix)+"Text.ConList_Mission"+number;
        const auto selected=Find(requested);Require(IsA(selected,"ConSys.ConversationList"),"Direct requested list is not original ConversationList");
        std::cout<<"DIRECT_MISSION_PATH package="<<std::quoted(convPackage.text)<<" origin="<<convPackage.origin
            <<" requested="<<requested<<" selected="<<Path(selected)<<" loadFlags=2; static lookup only, not StaticLoadObject execution\n";
        const auto conversations=Walk(selected,"ConSys.ConversationList.conversations","ConSys.Conversation");
        std::map<std::string,std::vector<std::string>> owners;
        std::cout<<"SELECTED_LIST "<<Path(selected)<<" authoredConversations="<<conversations.size()<<'\n';
        for(std::size_t i=0u;i<conversations.size();++i) {
            const auto& [link,conversation]=conversations[i];const auto owner=Read(conversation,"ConSys.Conversation.conOwnerName");
            const auto name=Read(conversation,"ConSys.Conversation.conName");
            std::cout<<"CONVERSATION_LINK "<<i<<" link="<<link<<" con="<<Path(conversation)
                <<" owner="<<std::quoted(owner.text)<<" name="<<std::quoted(name.text)<<'\n';
            owners[Lower(owner.text)].push_back(Path(conversation));
        }
        const auto printBinding=[&](Ref object,bool defaults) {
            const auto bind=Read(object,"Engine.Actor.BindName",defaults),bark=Read(object,"Engine.Actor.BarkBindName",defaults),
                list=Read(object,"Engine.Actor.ConListItems",defaults);
            Require(bind.tag==13u&&bark.tag==13u&&list.tag==5u,"Binding fields lost actual Str/Str/Object value types");
            std::cout<<(defaults?"CLASS_BINDING ":"ACTOR_BINDING ")<<Path(object)<<" class="<<(defaults?Path(object):Class(object))
                <<" BindName="<<std::quoted(bind.text)<<" from="<<bind.origin<<" BarkBindName="<<std::quoted(bark.text)
                <<" from="<<bark.origin<<" ConListItems="<<std::quoted(list.text)<<" from="<<list.origin<<'\n';
            for(const auto& [label,value]:std::vector<std::pair<std::string,std::string>>{{"BindName",bind.text},{"BarkBindName",bark.text}}) {
                const auto matches=owners.find(Lower(value));
                std::cout<<"OWNER_MATCH_INPUT "<<Path(object)<<' '<<label<<" matches="<<(matches==owners.end()?0u:matches->second.size());
                if(matches!=owners.end())for(const auto& path:matches->second)std::cout<<' '<<path;
                std::cout<<'\n';
            }
            std::vector<std::string> accepted;
            for(const auto& entry:conversations) {
                const auto owner=Read(entry.second,"ConSys.Conversation.conOwnerName").text;
                const auto conName=Read(entry.second,"ConSys.Conversation.conName").text;
                const auto ownerUnits=QuestVr::UnrealString::Decode(owner),barkUnits=QuestVr::UnrealString::Decode(bark.text),
                    bindUnits=QuestVr::UnrealString::Decode(bind.text),nameUnits=QuestVr::UnrealString::Decode(conName);
                const bool isBark=QuestVr::UnrealString::CString(nameUnits).find(u"_Bark")!=std::u16string_view::npos;
                const bool matches=isBark?
                    (QuestVr::UnrealString::EqualInsensitive(ownerUnits,barkUnits)||
                        (QuestVr::UnrealString::CString(barkUnits).empty()&&QuestVr::UnrealString::EqualInsensitive(ownerUnits,bindUnits))):
                    QuestVr::UnrealString::EqualInsensitive(ownerUnits,bindUnits);
                if(matches)accepted.push_back(Path(entry.second));
            }
            std::cout<<"ORIGINAL_BIND_PREDICTION "<<Path(object)<<" accepted="<<accepted.size()<<" reversedHeadOrder";
            for(auto i=accepted.rbegin();i!=accepted.rend();++i)std::cout<<' '<<*i;
            std::cout<<"; static prediction, NOT allocated/runtime binding\n";
            Protocol(object,defaults,selected,accepted);
        };
        for(const auto* cls:{"DeusEx.Plant2","DeusEx.DeusExDecoration","DeusEx.JaimeReyes","DeusEx.Doctor","DeusEx.DeusExPlayer"})printBinding(Find(cls),true);
        const std::set<std::string> representatives{"plant2","jaimereyes0","jaimereyes1","jaimereyes2","doctor0","doctor1",
            "repairbot0","medicalbot0","weaponcrowbar1","cratebreakablemedgeneral1","playerstart1"};
        std::size_t shown{};
        for(const auto actor:actors) {
            const auto bind=Read(actor,"Engine.Actor.BindName"),bark=Read(actor,"Engine.Actor.BarkBindName");
            const auto leaf=GetPortableObjectPath(map.table,static_cast<std::int32_t>(actor.index+1u));
            if(!bind.text.empty()||!bark.text.empty()||representatives.contains(Lower(leaf))) {printBinding(actor,false);++shown;}
        }
        std::cout<<"STATIC_AUDIT actorsPrinted="<<shown<<"; actual first-map inputs/direct native-format list selection only. NOT execution of "
            "StaticLoadObject/ConBindEvents, list construction, save/rollback/GC proof or successful startup.\n";
    }
};
} // namespace
int main(int argc,char** argv) {
    try {
        Require(argc==2,"Usage: conversation_binding_inspect OWNED_GAME_ROOT (ONLY Maps/00_Training.dx)");
        Audit audit(fs::canonical(argv[1]));audit.Run();return 0;
    }catch(const std::exception& error){std::cerr<<"STATIC_AUDIT_FAIL "<<error.what()<<'\n';return 1;}
}
