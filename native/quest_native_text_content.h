#pragma once

#include "quest_unreal_string.h"

#include <cstdint>
#include <string>
#include <string_view>

namespace QuestVr::NativeTextContent {

//10050710: only lower-case |n is converted. Any other bar consumes the
// following code unit as a pair, so ||n stays ||n; a trailing bar is retained.
// The caller owns/admitted this buffer. Output never exceeds the CString input.
template<class Step>
void ConvertScriptString(std::u16string& text,Step&& step) {
    const auto length=UnrealString::CString(text).size();std::size_t input{},output{};
    while(input<length) {
        step();const auto character=text[input++];
        if(character==u'|'&&input<length) {
            step();const auto next=text[input++];
            if(next==u'n')text[output++]=u'\n';
            else {text[output++]=u'|';text[output++]=next;}
        } else text[output++]=character;
    }
    text.resize(output);
}

//1004F310: the first |& wins, including an empty/unsupported key that clears
// the accelerator. Non-matching bar pairs skip their second code unit.
template<class Step>
std::int32_t AcceleratorKey(std::u16string_view text,Step&& step) {
    text=UnrealString::CString(text);
    for(std::size_t index=0u;index<text.size();) {
        step();const auto character=text[index++];
        if(character!=u'|'||index==text.size())continue;
        step();const auto next=text[index++];
        if(next!=u'&')continue;
        step();const auto key=index==text.size()?char16_t{}:text[index];
        return key<0xffu?static_cast<std::int32_t>(key):0;
    }
    return 0;
}

// XTextWindow10045D50. Operations owns real storage and same-root undo.
// Do not rewrite case-only differences, normalize markup again, omit the
// always-written accelerator key or perform a local rollback on callbacks.
template<class Operations>
void SetText(Operations& operations,std::u16string_view incoming) {
    incoming=UnrealString::CString(incoming);
    operations.Step();const auto previous=UnrealString::CString(operations.ReadText());
    bool equal=previous.size()==incoming.size();
    if(equal)for(std::size_t index=0u;index<incoming.size();++index) {
        operations.Step();
        if(UnrealString::FoldAscii(previous[index])!=UnrealString::FoldAscii(incoming[index])) {equal=false;break;}
    }
    if(equal)return;
    operations.Step();operations.WriteText(incoming);
    operations.Step();const auto accelerator=operations.TextIsAccelerator();
    if(accelerator) {
        const auto key=AcceleratorKey(incoming,[&]{operations.Step();});
        operations.Step();const auto previousKey=operations.ReadAcceleratorKey();
        operations.Step();operations.WriteAcceleratorKey(key); // Always, even equal.
        if(previousKey!=key) {operations.Step();operations.DirtyAccelerators();}
    }
    operations.Step();operations.AskForReconfigure();
}

} // namespace QuestVr::NativeTextContent
