#include "quest_input_source.h"
#include <iostream>
#include <thread>
#include <limits>

namespace {
std::size_t checks{},refusals{};
void Require(bool value,const char* why){++checks;if(!value)throw std::runtime_error(why);}
template<class Callback> void Refuse(Callback callback){bool refused{};try{callback();}catch(const std::exception&){refused=true;}Require(refused,"Expected input-source refusal");++refusals;}
}
int main() {
    try {
        using namespace QuestVr;auto lifetime=std::make_shared<NativeClientLifetime>();
        Refuse([&]{QuestInputSource bad({},1u,2u);});Refuse([&]{QuestInputSource bad(lifetime,0u,2u);});
        QuestInputSource source(lifetime,1u,2u);QuestInputCursor cursor;
        Require(source.Read().count==0u&&source.Stage(cursor).status==QuestInputReadStatus::Empty,"Missing samples fabricated input");
        QuestInputObservation sample;sample.displayTime=1;sample.deltaSeconds=1.f/72.f;
        sample.focused=sample.headValid=sample.headTracked=sample.mounted=true;
        sample.hands[1u].tracked=true;sample.hands[1u].trigger=.8f;sample.buttons=1u;
        source.Publish(sample);
        Require(source.Stage(cursor).status==QuestInputReadStatus::NeedsReset,"Uninitialized cursor silently consumed hardware");
        const auto reset=source.Stage(cursor,true);
        Require(reset.reset&&reset.count==1u&&reset.after.sequence==1u&&reset.rows[0].observation.hands[1u].trigger==.8f,"Reset baseline lost actual controls");
        Require(source.Stage(cursor,true).after.sequence==reset.after.sequence&&cursor.sequence==0u,"Staging mutated cursor");
        cursor=reset.after;
        Require(source.Stage(cursor).status==QuestInputReadStatus::Empty,"Same sample consumed twice");
        sample.displayTime=2;sample.buttons=0;sample.hands[1u].trigger=0;source.Publish(sample);
        sample.displayTime=3;sample.focused=false;sample.headValid=false;sample.headTracked=false;sample.hands[1u].tracked=false;source.Publish(sample);
        const auto pending=source.Stage(cursor);
        Require(pending.count==2u&&pending.rows[0].sequence==2u&&pending.rows[1].sequence==3u&&!pending.rows[1].observation.focused&&
            !pending.rows[1].observation.hands[1u].tracked,"Release/tracking-loss history was dropped");
        const auto retry=source.Stage(cursor);
        Require(retry.before.sequence==pending.before.sequence&&retry.after.sequence==pending.after.sequence&&retry.count==pending.count,"Rollback staging cannot retry same hardware history");
        cursor=pending.after;
        const auto before=source.Read();
        for(int which=0;which<8;++which) {
            auto invalid=sample;
            if(which==0)invalid.displayTime=std::numeric_limits<double>::quiet_NaN();
            if(which==1)invalid.deltaSeconds=-1;
            if(which==2)invalid.buttons=128;
            if(which==3)invalid.headTracked=true;
            if(which==4)invalid.hands[0].stickX=2;
            if(which==5)invalid.hands[0].trigger=-.1f;
            if(which==6){invalid.headValid=true;invalid.headPose.orientation={0,0,0,0};}
            if(which==7)invalid.displayTime=0;
            Refuse([&]{source.Publish(invalid);});Require(source.Read().latest==before.latest,"Rejected sample advanced sequence");
        }
        Require(source.Stage(cursor).status==QuestInputReadStatus::NeedsReset&&source.Stage(cursor,true).status==QuestInputReadStatus::NeedsReset,
            "Rejected hardware frame exposed a stale held-action baseline");
        sample.displayTime+=1;source.Publish(sample);const auto faultReset=source.Stage(cursor,true);
        Require(faultReset.status==QuestInputReadStatus::Ready&&faultReset.after.rejections==8u,
            "New valid state did not support an explicit post-fault reset");
        for(std::size_t index=0;index<QuestInputSource::Capacity+1u;++index){sample.displayTime+=1;source.Publish(sample);}
        Require(source.Stage(cursor).status==QuestInputReadStatus::NeedsReset,"Overflow silently dropped events");
        const auto latest=source.Read();const auto recovered=source.Stage(cursor,true);
        Require(recovered.count==1u&&recovered.after.sequence==latest.latest&&latest.count==QuestInputSource::Capacity&&latest.overwritten>0u,
            "Explicit reset did not recover bounded newest baseline");
        QuestInputSource other(lifetime,1u,2u);Refuse([&]{static_cast<void>(other.Stage(recovered.after,true));});
        Refuse([&]{static_cast<void>(source.Stage({latest.source,latest.latest+1u}));});
        // Real concurrent producer/consumer, fixed storage. Overrun must be
        // explicit; each copied row must be coherent despite producer wrap.
        auto concurrent=std::make_shared<QuestInputSource>(lifetime,1u,3u);std::atomic<bool> done{},failed{};
        std::jthread producer([&]{try{QuestInputObservation value;for(std::uint64_t n=1;n<=20000u;++n){value.displayTime=double(n);value.buttons=static_cast<std::uint8_t>(n%128u);concurrent->Publish(value);}}catch(...){failed=true;}done=true;});
        QuestInputCursor concurrentCursor;std::size_t observations{};
        while(!done.load()||concurrentCursor.sequence<concurrent->Read().latest) {
            auto batch=concurrent->Stage(concurrentCursor);
            if(batch.status==QuestInputReadStatus::NeedsReset)batch=concurrent->Stage(concurrentCursor,true);
            for(std::size_t n=0;n<batch.count;++n){Require(batch.rows[n].observation.displayTime==double(batch.rows[n].sequence)&&
                batch.rows[n].observation.buttons==batch.rows[n].sequence%128u,"Torn concurrent observation");++observations;}
            concurrentCursor=batch.after;
        }
        producer.join();Require(!failed&&observations>0u&&concurrentCursor.sequence==20000u,"Concurrent source did not finish coherently");
        lifetime->Revoke();Refuse([&]{source.Publish(sample);});Refuse([&]{static_cast<void>(source.Read());});
        Refuse([&]{static_cast<void>(source.Stage(cursor));});Require(!source.Matches(lifetime,1u,2u),"Revoked source retained authority");
        std::cout<<"PASS Quest input observation source: "<<checks<<" checks / "<<refusals<<" refusals; bounded coherent history, explicit reset/overflow, rollback-safe staging, tracking/focus observations, concurrency and revoke. NOT original input dispatch or headset acceptance.\n";
        return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
