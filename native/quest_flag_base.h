#pragma once
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <string_view>

namespace QuestVr::FlagBase {
// Core10149875..101498A8 constructs this UNREFLECTED polynomial table.
// Extension10024E60 hashes UTF16 bytes low/high using that table and a right
// shift. Do not replace this combination with ordinary reflected CRC32.
inline constexpr auto CrcTable=[] {
    std::array<std::uint32_t,256u> table{};
    for(std::uint32_t i=0u;i<256u;++i){auto value=i<<24u;
        for(unsigned bit=0u;bit<8u;++bit)value=(value<<1u)^((value&0x80000000u)?0x04c11db7u:0u);
        table[i]=value;
    }
    return table;
}();
inline std::uint32_t Hash(std::u16string_view text,const std::function<void(std::size_t)>& work={}) {
    std::uint32_t value{};
    for(auto unit:text){if(work)work(1u);if(!unit)break;
        if(unit>=u'a'&&unit<=u'z')unit=static_cast<char16_t>(unit-(u'a'-u'A'));
        for(unsigned shift:{0u,8u})value=CrcTable[(value^(static_cast<std::uint32_t>(unit)>>shift))&255u]^(value>>8u);
    }
    return value;
}
struct Node {std::uintptr_t identity{},next{};int name{};std::int32_t hash{};std::uint8_t type{};};
struct Queries {
    std::function<void(std::size_t)> work;
    std::function<std::uintptr_t(std::size_t)> bucket;
    std::function<Node(std::uintptr_t)> node;
};
struct Found {std::uintptr_t node{},previous{};std::size_t bucket{},visited{};std::uint32_t hash{};};
inline Found Find(int name,std::u16string_view canonical,std::uint8_t type,const Queries& query) {
    if(!query.bucket||!query.node)throw std::runtime_error("FlagBase lookup requires actual native storage providers");
    Found result;result.hash=Hash(canonical,query.work);result.bucket=result.hash&63u;
    auto current=query.bucket(result.bucket);
    // Bounded traversal policy, NOT64 total flags. A bad/cyclic graph refuses
    // under the caller's same root work cap without allocating a visited set.
    for(;current;){if(query.work)query.work(1u);
        if(++result.visited>65536u)throw std::runtime_error("FlagBase chain exceeds bounded native traversal");
        const auto node=query.node(current);
        if(node.identity!=current)throw std::runtime_error("FlagBase node provider changed actual identity");
        if(node.hash>std::bit_cast<std::int32_t>(result.hash))break;
        if(node.hash==std::bit_cast<std::int32_t>(result.hash)&&node.name==name){
            if(node.type>type)break;
            if(node.type==type){result.node=current;break;}
        }
        result.previous=current;current=node.next;
    }
    return result;
}
} // namespace QuestVr::FlagBase
