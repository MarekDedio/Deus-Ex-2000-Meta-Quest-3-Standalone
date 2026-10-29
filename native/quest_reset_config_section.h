#pragma once

#include "quest_native_config.h"

#include <cstddef>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace QuestVr::ResetConfigSection {

// The selected-class section portion of Core ResetConfig10151AC0 and the
// explicit Editor GetSection1021B980 profile only. Class/default/instance
// reloads, cache ownership, dirty state and eventual file publication remain
// the enclosing runtime's responsibility. This helper owns no roots or cache.
struct Work {
    std::function<void(std::size_t)> step;
    std::function<void(std::size_t)> admit;
    std::function<void(std::string_view,std::string_view)> setString;
};
struct Result {
    bool sourcePresent{},sectionPresent{},keyPrefixStopped{};
    std::size_t pairs{},packetUnits{}; // Includes the final NUL when present.
};
inline constexpr std::size_t PacketCapacityUnits=32767u;

// admit receives an increasing ABSOLUTE additional workspace peak, excluding
// the already retained source document. The caller must retain that admitted
// peak in its enclosing scratch ledger THROUGH each nested setString call,
// then release it after Apply returns/throws. setString receives canonical
// UTF8/WTF8 CStrings borrowed only for that callback. Provider/work/admission
// exceptions propagate; prior callback effects are NOT rolled back here.
inline Result Apply(const NativeConfig::OrderedIni* source,std::string_view section,const Work& callbacks) {
    using namespace NativeConfig;
    if(!callbacks.step||!callbacks.admit||!callbacks.setString)
        Detail::Fail("ResetConfig section requires shared work, scratch and SetString providers");
    const auto fixedBytes=sizeof(Result)+sizeof(Detail::Operation)+2u*sizeof(NativeConfig::Work)+
        sizeof(std::vector<char16_t>)+2u*sizeof(std::string)+8u*sizeof(std::size_t);
    // Admit even callback adapters before constructing them. Borrow the
    // supplied functions rather than copying potentially allocating captures.
    callbacks.admit(fixedBytes);
    const Limits limits;
    const NativeConfig::Work shared{
        [&](std::size_t amount){callbacks.step(amount);},
        [&](std::size_t bytes){callbacks.admit(bytes);}};
    Detail::Operation operation{limits,shared,0u,fixedBytes};operation.Step();
    Result result;result.sourcePresent=source!=nullptr;
    if(!source)return result;

    std::size_t liveBytes=fixedBytes;
    const NativeConfig::Work query{
        [&](std::size_t amount){operation.Step(amount);},
        [&](std::size_t bytes){operation.Admit(Detail::Sum(liveBytes,bytes));}};
    const auto length=[&](std::string_view text) {
        std::size_t count{};bool ended{};
        Detail::Units(text,operation,[&](char16_t c) {
            if(c==u'\0')ended=true;
            if(!ended)++count;
        });
        return count;
    };
    std::size_t used{};
    result.sectionPresent=source->VisitSectionPairs(section,[&](std::string_view key,std::string_view value) {
        const auto keyUnits=length(key);
        // Original Editor1021BB08..1D tests only key length, before sprintf.
        // Exhaustion is a SUCCESSFUL prefix, including an empty prefix.
        if(Detail::Sum(Detail::Sum(used,keyUnits),1u)>=PacketCapacityUnits) {
            result.keyPrefixStopped=true;return false;
        }
        const auto valueUnits=length(value);
        const auto recordUnits=Detail::Sum(Detail::Sum(keyUnits,valueUnits),2u); // '=' and record NUL.
        // Retail omits this value/final-NUL bound. Refuse its unsafe overflow
        // rather than corrupt memory, truncate values or publish a partial reset.
        if(recordUnits>PacketCapacityUnits-1u-used)
            Detail::Fail("ResetConfig formatted section entry exceeds bounded packet");
        used+=recordUnits;++result.pairs;return true;
    },query);
    if(!result.sectionPresent)return result;

    result.packetUnits=used+1u;
    const auto packetBytes=Detail::Product(result.packetUnits,sizeof(char16_t));
    liveBytes=Detail::Sum(fixedBytes,packetBytes);operation.Admit(liveBytes);
    std::vector<char16_t> packet(result.packetUnits,u'\0');
    if(packet.capacity()!=result.packetUnits)
        Detail::Fail("ResetConfig packet allocation exceeded admitted capacity");
    std::size_t offset{},remaining=result.pairs;
    const auto append=[&](std::string_view text) {
        bool ended{};
        Detail::Units(text,operation,[&](char16_t c) {
            if(c==u'\0')ended=true;
            if(!ended) {
                if(offset==used)Detail::Fail("ResetConfig source changed during packet construction");
                packet[offset++]=c;
            }
        });
    };
    const auto present=source->VisitSectionPairs(section,[&](std::string_view key,std::string_view value) {
        if(!remaining)return false;
        append(key);
        if(offset==used)Detail::Fail("ResetConfig source changed during packet construction");
        packet[offset++]=u'=';append(value);
        if(offset==used)Detail::Fail("ResetConfig source changed during packet construction");
        packet[offset++]=u'\0';--remaining;return true;
    },query);
    if(!present||remaining||offset!=used)
        Detail::Fail("ResetConfig source changed during packet construction");

    // Core obtains the COMPLETE packet before the first cache mutation. It
    // saves each original record length, then splits only the first '='.
    for(std::size_t position=0u;position<used;) {
        std::size_t end=position,equals=used;
        while(end<used&&packet[end]!=u'\0') {
            operation.Step();
            if(equals==used&&packet[end]==u'=')equals=end;
            ++end;
        }
        if(end==used||equals==used)Detail::Fail("ResetConfig packet record is invalid");
        const std::u16string_view key(packet.data()+position,equals-position);
        const std::u16string_view value(packet.data()+equals+1u,end-equals-1u);
        // Encode uses geometrically growing strings. Twelve bytes per UTF16
        // unit bounds UTF8/WTF8 expansion AND overlapping old/new allocation
        // during growth of either string while its peer stays live. Headers
        // also cover SSO; a post-encode retained-capacity check alone would not.
        const auto encodedBytes=Detail::Sum(4u*sizeof(std::string),
            Detail::Product(Detail::Sum(key.size(),Detail::Sum(value.size(),2u)),12u));
        operation.Admit(Detail::Sum(liveBytes,encodedBytes));
        operation.Step(key.size()+value.size()+1u);
        const auto keyText=UnrealString::Encode(key),valueText=UnrealString::Encode(value);
        if(Detail::Sum(2u*sizeof(std::string),Detail::Sum(keyText.capacity()+1u,valueText.capacity()+1u))>encodedBytes)
            Detail::Fail("ResetConfig text conversion exceeded admitted capacity");
        callbacks.setString(keyText,valueText);position=end+1u;
    }
    return result;
}

} // namespace QuestVr::ResetConfigSection
