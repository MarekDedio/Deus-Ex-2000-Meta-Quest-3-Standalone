#pragma once

#include <cstddef>
#include <functional>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace QuestVr {
// Extension100379A5..10037A25. Both the lexical scan and FName constructor
// consume a UTF16 CString, not the entire owning FString after an embedded NUL.
// The actual error call is GError.Logf, NOT a warning sink. A returning error
// provider proceeds with the UNCHANGED CString; it never truncates at the bad
// unit. The embedding must explicitly supply that provider (or refuse there).
struct OriginalStringToNameQueries {
    std::function<void(std::size_t)> work;
    std::function<void(char16_t,std::u16string_view)> error;
};
template<class Intern>
auto RunOriginalStringToName(std::u16string_view input,
    const OriginalStringToNameQueries& queries,Intern&& intern) {
    std::size_t end{};
    for(;end<input.size()&&input[end]!=u'\0';++end)
        if(queries.work)queries.work(1u);
    input=input.substr(0u,end);
    for(const auto unit:input) {
        if(queries.work)queries.work(1u);
        if((unit>=u'a'&&unit<=u'z')||(unit>=u'A'&&unit<=u'Z')||
            (unit>=u'0'&&unit<=u'9')||unit==u'_'||unit==u'-')continue;
        if(!queries.error)throw std::runtime_error("StringToName has no actual GError provider");
        queries.error(unit,input);
        break;
    }
    // Original10037A18 explicitly uses EFindName1 (find-or-add), including
    // the empty/None case. There is no63-character truncation or repair.
    return std::forward<Intern>(intern)(input);
}
} // namespace QuestVr
