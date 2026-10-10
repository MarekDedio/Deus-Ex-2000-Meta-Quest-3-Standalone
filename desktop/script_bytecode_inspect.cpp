#include "Precomp.h"
#include "surreal_portable_package_tables.h"

#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <set>
#include <stdexcept>

namespace {
struct Reader {
    const std::vector<std::uint8_t>& bytes;
    std::size_t position{};
    std::uint8_t Byte() {
        if (position >= bytes.size()) throw std::runtime_error("Truncated inspected bytecode");
        return bytes[position++];
    }
    std::uint16_t Word() {
        const auto a = Byte(); return static_cast<std::uint16_t>(a | (Byte() << 8u));
    }
    std::uint32_t Dword() {
        const auto a = Word(); return a | (static_cast<std::uint32_t>(Word()) << 16u);
    }
    std::int32_t Index() {
        auto byte = Byte(); const bool negative = (byte & 0x80u) != 0u;
        std::uint32_t value = byte & 63u; unsigned shift = 6u;
        bool more = (byte & 0x40u) != 0u;
        while (more) {
            if (shift >= 32u) throw std::runtime_error("Invalid inspected compact index");
            byte = Byte(); value |= static_cast<std::uint32_t>(byte & 127u) << shift;
            more = (byte & 0x80u) != 0u; shift += 7u;
        }
        return negative ? -static_cast<std::int32_t>(value) : static_cast<std::int32_t>(value);
    }
    float Float() {
        const auto bits = Dword(); float value{}; std::memcpy(&value, &bits, sizeof(value)); return value;
    }
};
std::string TokenName(const unsigned token) {
    static const std::map<unsigned, const char*> labels{
        {0x00,"LocalVariable"},{0x01,"InstanceVariable"},{0x02,"DefaultVariable"},
        {0x04,"Return"},{0x05,"Switch"},{0x06,"Jump"},{0x07,"JumpIfNot"},{0x08,"Stop"},
        {0x09,"Assert"},{0x0a,"Case"},{0x0b,"Nothing"},{0x0c,"LabelTable"},
        {0x0d,"GotoLabel"},{0x0e,"EatString"},{0x0f,"Let"},{0x10,"DynArrayElement"},
        {0x11,"New"},{0x12,"ClassContext"},{0x13,"MetaCast"},{0x14,"LetBool"},
        {0x15,"Unknown15"},{0x16,"EndFunctionParms"},{0x17,"Self"},{0x18,"Skip"},
        {0x19,"Context"},{0x1a,"ArrayElement"},{0x1b,"VirtualFunction"},
        {0x1c,"FinalFunction"},{0x1d,"IntConst"},{0x1e,"FloatConst"},
        {0x1f,"StringConst"},{0x20,"ObjectConst"},{0x21,"NameConst"},
        {0x22,"RotationConst"},{0x23,"VectorConst"},{0x24,"ByteConst"},{0x25,"IntZero"},
        {0x26,"IntOne"},{0x27,"True"},{0x28,"False"},{0x29,"NativeParm"},
        {0x2a,"NoObject"},{0x2b,"Unknown2b"},{0x2c,"IntConstByte"},{0x2d,"BoolVariable"},
        {0x2e,"DynamicCast"},{0x2f,"Iterator"},{0x30,"IteratorPop"},{0x31,"IteratorNext"},
        {0x32,"StructCmpEq"},{0x33,"StructCmpNe"},{0x34,"UnicodeStringConst"},
        {0x36,"StructMember"},{0x38,"GlobalFunction"},{0x39,"RotatorToVector"},
        {0x3a,"ByteToInt"},{0x3b,"ByteToBool"},{0x3c,"ByteToFloat"},
        {0x3d,"IntToByte"},{0x3e,"IntToBool"},{0x3f,"IntToFloat"},{0x40,"BoolToByte"},
        {0x41,"BoolToInt"},{0x42,"BoolToFloat"},{0x43,"FloatToByte"},
        {0x44,"FloatToInt"},{0x45,"FloatToBool"},{0x46,"Unknown46"},
        {0x47,"ObjectToBool"},{0x48,"NameToBool"},{0x49,"StringToByte"},
        {0x4a,"StringToInt"},{0x4b,"StringToBool"},{0x4c,"StringToFloat"},
        {0x4d,"StringToVector"},{0x4e,"StringToRotator"},{0x4f,"VectorToBool"},
        {0x50,"VectorToRotator"},{0x51,"RotatorToBool"},{0x52,"ByteToString"},
        {0x53,"IntToString"},{0x54,"BoolToString"},{0x55,"FloatToString"},
        {0x56,"ObjectToString"},{0x57,"NameToString"},{0x58,"VectorToString"},
        {0x59,"RotatorToString"},{0x5a,"StringToName"}};
    const auto found = labels.find(token);
    return found == labels.end() ? "Token" : found->second;
}
struct Inspector {
    const PortablePackageTables& package;
    const std::map<unsigned,std::string>& natives;
    Reader reader;
    std::set<unsigned> tokens, nativeIndices;
    std::string Name(const std::int32_t index) const {
        if (index < 0 || static_cast<std::size_t>(index) >= package.names.size())
            throw std::runtime_error("Inspected name index outside package");
        return package.names[static_cast<std::size_t>(index)].Name.ToString();
    }
    std::uint8_t Expr(const unsigned depth = 0u) {
        if (depth > 64u) throw std::runtime_error("Inspected expression exceeds depth bound");
        const auto offset = reader.position; const auto token = reader.Byte(); tokens.insert(token);
        const auto line = [&](const std::string& text) {
            std::cout << std::setw(5) << offset << ' ' << std::string(depth*2u,' ') << text << '\n';
        };
        const auto child = [&]() { return Expr(depth+1u); };
        const auto object = [&]() {
            const auto reference = static_cast<std::int32_t>(reader.Dword());
            return std::to_string(reference) + "=" + GetPortableObjectPath(package,reference);
        };
        const auto children = [&]() { while (child() != 0x16u) {} };
        if (token >= 0x60u) {
            const auto native = token >= 0x70u ? static_cast<unsigned>(token) :
                ((token-0x60u)<<8u) + reader.Byte();
            nativeIndices.insert(native); const auto found = natives.find(native);
            line("Native " + std::to_string(native) + " " +
                (found == natives.end() ? "(unresolved)" : found->second)); children();
        } else if (token == 0x1bu || token == 0x38u) {
            line(TokenName(token) + " '" + Name(static_cast<std::int32_t>(reader.Dword())) + "'"); children();
        } else if (token == 0x1cu) {
            line(TokenName(token) + " " + object()); children();
        } else {
            switch (token) {
                case 0x00: case 0x01: case 0x02: case 0x20: case 0x29:
                    line(TokenName(token) + " " + object()); break;
                case 0x21: line("NameConst '"+Name(static_cast<std::int32_t>(reader.Dword()))+"'"); break;
                case 0x1d: line("IntConst "+std::to_string(static_cast<std::int32_t>(reader.Dword()))); break;
                case 0x1e: line("FloatConst "+std::to_string(reader.Float())); break;
                case 0x24: case 0x2c: line(TokenName(token)+" "+std::to_string(reader.Byte())); break;
                case 0x1f: {
                    std::string value; char c{};
                    while ((c=static_cast<char>(reader.Byte())) != '\0') value += c;
                    line("StringConst "+value); break;
                }
                case 0x22: case 0x23: {
                    const auto a = reader.Dword(), b = reader.Dword(), c = reader.Dword();
                    line(TokenName(token)+" bits="+std::to_string(a)+','+std::to_string(b)+','+std::to_string(c)); break;
                }
                case 0x06: line("Jump ->"+std::to_string(reader.Word())); break;
                case 0x07: case 0x09: case 0x18:
                    line(TokenName(token)+" address/line="+std::to_string(reader.Word())); child(); break;
                case 0x0a: {
                    const auto next = reader.Word(); line("Case ->"+std::to_string(next));
                    if (next != 0xffffu) child();
                    break;
                }
                case 0x05: case 0x2b:
                    line(TokenName(token)+" size="+std::to_string(reader.Byte())); child(); break;
                case 0x12: case 0x19: {
                    line(TokenName(token)); child();
                    const auto skip = reader.Word(); const auto zero = reader.Byte();
                    std::cout << std::string((depth+1u)*2u,' ') << "nullSkip=" << skip << " zeroSize=" << unsigned(zero) << '\n';
                    child(); break;
                }
                case 0x13: case 0x2e: case 0x36:
                    line(TokenName(token)+" "+object()); child(); break;
                case 0x04: case 0x0d: case 0x0e: case 0x2d:
                    line(TokenName(token)); child(); break;
                case 0x0f: case 0x10: case 0x14: case 0x1a:
                    line(TokenName(token)); child(); child(); break;
                case 0x11: line("New"); child(); child(); child(); child(); break;
                case 0x32: case 0x33: line(TokenName(token)+" "+object()); child(); child(); break;
                case 0x2f: line("Iterator"); child(); line("iteratorEnd="+std::to_string(reader.Word())); break;
                case 0x34: line("UnicodeStringConst"); while (reader.Word() != 0u) {} break;
                case 0x0c:
                    line("LabelTable"); while (true) {
                        const auto name = Name(static_cast<std::int32_t>(reader.Dword()));
                        const auto target = reader.Dword(); std::cout << " label " << name << " ->" << target << '\n';
                        if (name == "None") break;
                    } break;
                case 0x08: case 0x0b: case 0x15: case 0x16: case 0x17: case 0x25:
                case 0x26: case 0x27: case 0x28: case 0x2a: case 0x30: case 0x31:
                    line(TokenName(token)); break;
                default:
                    if (token >= 0x39u && token <= 0x5fu) { line(TokenName(token)+" conversion="+std::to_string(token)); child(); }
                    else throw std::runtime_error("Unsupported inspector token "+std::to_string(token));
            }
        }
        return token;
    }
};
void Parameters(const PortablePackageTables& package, const std::size_t index) {
    const auto& entry = package.exports[index];
    if (entry.ObjSize <= 0 || entry.ObjSize > (1 << 20))
        throw std::runtime_error("Inspected struct payload exceeds byte bound");
    std::ifstream file(package.sourcePath,std::ios::binary);
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(entry.ObjSize));
    file.seekg(entry.ObjOffset); file.read(reinterpret_cast<char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));
    if (!file) throw std::runtime_error("Could not read inspected struct payload");
    Reader header{bytes,LoadPortableExportProperties(package,index).bytesConsumed};
    const auto base = header.Index(), next = header.Index(), source = header.Index();
    auto child = header.Index();
    std::cout << "header base=" << GetPortableObjectPath(package,base) << " next=" << GetPortableObjectPath(package,next) <<
        " source=" << GetPortableObjectPath(package,source) << " children=" << GetPortableObjectPath(package,child) << '\n';
    std::set<std::int32_t> visited;
    while (child != 0) {
        if (child < 0 || !visited.insert(child).second || visited.size() > 4096u ||
            static_cast<std::size_t>(child) > package.exports.size())
            throw std::runtime_error("Invalid inspected struct child chain");
        const auto childIndex = static_cast<std::size_t>(child - 1);
        if (package.exports[childIndex].ObjOuter != static_cast<std::int32_t>(index + 1u))
            throw std::runtime_error("Inspected struct child belongs to a different owner");
        if (package.exports[childIndex].ObjSize <= 0 || package.exports[childIndex].ObjSize > (1 << 20))
            throw std::runtime_error("Inspected struct child payload exceeds byte bound");
        auto childClass = GetPortableObjectPath(package, package.exports[childIndex].ObjClass);
        childClass = childClass.substr(childClass.find_last_of('.') + 1u);
        if (!childClass.ends_with("Property")) {
            std::cout << "child " << GetPortableObjectPath(package, child) << " type=" << childClass << '\n';
            if (childClass == "Function") child = LoadPortableFunctionScript(package, childIndex).nextField;
            else if (childClass == "State") child = LoadPortableStateDescriptor(package, childIndex).nextField;
            else child = LoadPortableFieldLinks(package, childIndex).nextField;
            continue;
        }
        const auto descriptor = LoadPortablePropertyDescriptor(package,childIndex);
        std::cout << "child " << descriptor.objectPath << " type=" << descriptor.type << " dim=" << descriptor.arrayDimension <<
            " flags=0x" << std::hex << descriptor.flags << std::dec <<
            " refType=" << GetPortableObjectPath(package,descriptor.referencedType) << '\n';
        child = descriptor.nextField;
    }
}
} // namespace

int main(int argc, char** argv) {
    try {
        if (argc < 3) throw std::runtime_error("Usage: script_bytecode_inspect GAME_ROOT PACKAGE.FunctionOrStatePath [...]");
        const std::filesystem::path root(argv[1]);
        std::map<std::string,PortablePackageTables> packages;
        std::map<unsigned,std::string> natives;
        const auto load = [&](const std::string& name) -> const PortablePackageTables& {
            auto found = packages.find(name);
            if (found == packages.end()) found = packages.emplace(name,
                LoadPortablePackageTables((root/"System"/(name+".u")).string())).first;
            return found->second;
        };
        for (const auto* name : {"Core","Engine"}) {
            const auto& package = load(name);
            for (std::size_t index=0u; index<package.exports.size(); ++index) {
                const auto cls = GetPortableObjectPath(package,package.exports[index].ObjClass);
                if (cls != "Core.Function" && cls != "Function") continue;
                const auto script = LoadPortableFunctionScript(package,index);
                if ((script.functionFlags&0x400u) != 0u && script.nativeIndex != 0u)
                    natives.emplace(script.nativeIndex,std::string(name)+'.'+script.objectPath);
            }
        }
        for (int arg=2; arg<argc; ++arg) {
            const std::string requested(argv[arg]); const auto dot=requested.find('.');
            if (dot == std::string::npos) throw std::runtime_error("Script path lacks package");
            const auto& package=load(requested.substr(0u,dot));
            const auto index=FindPortableExport(package,requested.substr(dot+1u));
            auto scriptClass = GetPortableObjectPath(package, package.exports[index].ObjClass);
            scriptClass = scriptClass.substr(scriptClass.find_last_of('.') + 1u);
            PortableScriptBody script;
            if (scriptClass == "State") {
                const auto state = LoadPortableStateDescriptor(package, index);
                script.objectPath = state.objectPath; script.logicalSize = state.logicalSize;
                script.bytecode = state.bytecode;
                std::cout << "\nSTATE " << requested << " logicalBytes=" << script.bytecode.size() <<
                    " flags=0x" << std::hex << state.stateFlags << std::dec <<
                    " labelTableOffset=" << state.labelTableOffset << '\n';
            } else {
                script = LoadPortableFunctionScript(package, index);
                std::cout << "\nFUNCTION " << requested << " logicalBytes=" << script.bytecode.size() <<
                    " native=" << script.nativeIndex << " flags=0x" << std::hex << script.functionFlags << std::dec << '\n';
            }
            Parameters(package,index);
            Inspector inspector{package,natives,{script.bytecode,0u},{},{}};
            while (inspector.reader.position<script.bytecode.size()) inspector.Expr();
            std::cout << "TOKENS";
            for (const auto token:inspector.tokens) std::cout << " 0x" << std::hex << token << std::dec;
            std::cout << "\nNATIVES";
            for (const auto native:inspector.nativeIndices) std::cout << ' ' << native;
            std::cout << '\n';
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Script inspection failed: " << error.what() << '\n'; return 1;
    }
}
