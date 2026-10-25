#pragma once

#include "quest_native_config_import.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace QuestVr::NativeConfigImport {
enum class StructImportWarning {ExpectedOpen,ExpectedArrayClose,ExpectedEquals,UnclosedQuote,
    TooManyClose,TooManyOpen,ExpectedSeparator};

// Complete normal control flow of Core.UStructProperty.ImportText,
// 101698B0..10169BEB. Ops supplies actual Struct/Property/FName identities,
// live Children/Next/Super traversal, the real Port virtual, typed destination
// writes and shared work/recursion/output budgets. This creates no metadata,
// UObject, FName or transactional authority. Import failure keeps prior writes;
// only the caller's genuine transaction may roll back the whole operation.
template<class Ops>
Result Struct(std::u16string_view input,std::uint32_t flags,Ops& operations) {
    input=Detail::Input(input);std::size_t cursor{};
    const auto at=[&]{return Detail::At(input,cursor);};
    const auto fail=[&](StructImportWarning warning)->Result {operations.Warning(warning);return {};};
    operations.Step();
    if(at()!=u'(')return fail(StructImportWarning::ExpectedOpen);
    ++cursor;
    for(;;) {
        operations.Step();if(at()==u')')return {true,cursor+1u};
        std::u16string spelling;
        while(spelling.size()<63u&&at()!=0&&at()!=u'='&&at()!=u'[') {
            operations.Step();spelling.push_back(at());++cursor;
        }
        // Retail does NOT skip '[' before its digit loop at10169937. Every
        // attempted inner array subscript therefore reaches the original
        // bad-']' warning. Do not silently repair that behavior. The enclosing
        // native Aliases[40] property is indexed separately by LoadConfig.
        if(at()==u'[')return fail(StructImportWarning::ExpectedArrayClose);
        if(at()!=u'=')return fail(StructImportWarning::ExpectedEquals);
        ++cursor;const auto name=operations.FindName(spelling);bool imported{};
        if(name!=typename Ops::Name{}) {
            auto structure=operations.Structure();std::size_t depth{},fields{};
            while(structure!=typename Ops::StructureType{}) {
                operations.Step();if(++depth>128u)Detail::Fail("struct Super traversal exceeds bound");
                auto field=operations.Child(structure);
                while(field!=typename Ops::Field{}) {
                    operations.Step();if(++fields>8192u)Detail::Fail("struct field traversal exceeds bound");
                    if(operations.IsProperty(field)&&operations.FieldName(field)==name) {
                        const auto dimension=operations.Dimension(field),element=operations.ElementSize(field);
                        const auto product=static_cast<std::uint32_t>(dimension)*static_cast<std::uint32_t>(element);
                        if(dimension>0&&product!=0u&&operations.Port(field)) {
                            const auto remaining=input.substr(cursor);
                            const auto result=operations.Import(field,0u,remaining,flags|2u);
                            if(!result.returned)return {};
                            if(result.nextUnit>remaining.size())Detail::Fail("struct member cursor exceeds CString storage");
                            cursor+=result.nextUnit;imported=true;
                        }
                    }
                    field=operations.Next(field);
                }
                structure=operations.Super(structure);
            }
        }
        if(!imported) {
            std::int32_t nesting{};
            for(;;) {
                operations.Step();const auto value=at();
                if(!value||value==u'\r'||value==u'\n'||(nesting<=0&&(value==u')'||value==u',')))break;
                if(value==u'"') {
                    // Original10169B1E tests the CURRENT quote immediately;
                    // it then advances one unit, without a normal quoted scan.
                    // Preserve its observable parentheses/separator behavior.
                    ++cursor;continue;
                }
                if(value==u'(') {
                    if(nesting==128)Detail::Fail("unknown struct value nesting exceeds bound");
                    ++nesting;
                }else if(value==u')') {
                    --nesting;if(nesting<0)return fail(StructImportWarning::TooManyClose);
                }
                ++cursor;
            }
            if(nesting>0)return fail(StructImportWarning::TooManyOpen);
        }
        if(at()==u','){++cursor;continue;}
        if(at()==u')')continue;
        return fail(StructImportWarning::ExpectedSeparator);
    }
}
} // namespace QuestVr::NativeConfigImport
