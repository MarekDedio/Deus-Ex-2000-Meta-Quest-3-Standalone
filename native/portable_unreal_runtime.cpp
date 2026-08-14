#include "portable_unreal_runtime.h"

#include "GC/GC.h"

#include <memory>
#include <cstring>
#include <stdexcept>
#include <unordered_map>
#include <utility>

namespace {

class RuntimeObject : public GCObject {
public:
    RuntimeObject(
        PortableReflectionObject reflection,
        std::size_t* destroyed)
        : reflection(std::move(reflection)), destroyed_(destroyed) {}

    PortableReflectionObject reflection;
    std::vector<RuntimeObject*> references;
    RuntimeObject* outer{};
    RuntimeObject* base{};
    RuntimeObject* cls{};
    std::unique_ptr<PortableScriptBody> script;
    std::unique_ptr<PortablePropertyDescriptor> property;
    std::vector<PortableTaggedProperty> instanceProperties;

protected:
    ~RuntimeObject() override {
        if (destroyed_ != nullptr) ++*destroyed_;
    }

    GCAllocation* Mark(GCAllocation* marklist) override {
        for (RuntimeObject* reference : references) {
            marklist = GC::MarkObject(marklist, reference);
        }
        return marklist;
    }

private:
    std::size_t* destroyed_{};
};

class RuntimePackage final : public GCObject {
public:
    explicit RuntimePackage(std::size_t* destroyedPackage)
        : destroyedPackage_(destroyedPackage) {}

    std::vector<RuntimeObject*> exports;

protected:
    ~RuntimePackage() override {
        if (destroyedPackage_ != nullptr) ++*destroyedPackage_;
    }

    GCAllocation* Mark(GCAllocation* marklist) override {
        for (RuntimeObject* object : exports) {
            marklist = GC::MarkObject(marklist, object);
        }
        return marklist;
    }

private:
    std::size_t* destroyedPackage_{};
};

std::unique_ptr<GCRoot<RuntimePackage>> persistentRuntime;
std::unordered_map<std::string, RuntimeObject*> persistentQualifiedObjects;

class RuntimeBytecodeReader {
public:
    explicit RuntimeBytecodeReader(const std::vector<std::uint8_t>& bytes)
        : bytes_(bytes) {}

    std::uint8_t Byte() {
        Require(1);
        return bytes_[position_++];
    }
    std::uint32_t Dword() {
        const std::uint32_t a = Byte();
        const std::uint32_t b = Byte();
        const std::uint32_t c = Byte();
        const std::uint32_t d = Byte();
        return a | (b << 8u) | (c << 16u) | (d << 24u);
    }
    std::string AsciiZ() {
        std::string result;
        while (true) {
            const char value = static_cast<char>(Byte());
            if (value == '\0') return result;
            result.push_back(value);
        }
    }

private:
    void Require(std::size_t count) const {
        if (count > bytes_.size() - position_) {
            throw std::runtime_error("Portable VM read past function bytecode");
        }
    }
    const std::vector<std::uint8_t>& bytes_;
    std::size_t position_{};
};

PortableVmValue EvaluateConstant(RuntimeBytecodeReader& reader) {
    PortableVmValue result;
    switch (reader.Byte()) {
        case 0x0b: // Nothing
            return result;
        case 0x1d: // IntConst
            result.type = PortableVmValueType::Integer;
            result.integer = static_cast<std::int32_t>(reader.Dword());
            return result;
        case 0x1e: { // FloatConst
            const std::uint32_t bits = reader.Dword();
            result.type = PortableVmValueType::Float;
            std::memcpy(&result.floating, &bits, sizeof(bits));
            return result;
        }
        case 0x1f: // StringConst
            result.type = PortableVmValueType::String;
            result.string = reader.AsciiZ();
            return result;
        case 0x20: // ObjectConst
            result.type = PortableVmValueType::ObjectReference;
            result.integer = static_cast<std::int32_t>(reader.Dword());
            return result;
        case 0x21: // NameConst
            result.type = PortableVmValueType::NameReference;
            result.integer = static_cast<std::int32_t>(reader.Dword());
            return result;
        case 0x24: // ByteConst
            result.type = PortableVmValueType::Integer;
            result.integer = reader.Byte();
            return result;
        case 0x25: // IntZero
            result.type = PortableVmValueType::Integer;
            return result;
        case 0x26: // IntOne
            result.type = PortableVmValueType::Integer;
            result.integer = 1;
            return result;
        case 0x27: // True
            result.type = PortableVmValueType::Boolean;
            result.boolean = true;
            return result;
        case 0x28: // False
            result.type = PortableVmValueType::Boolean;
            return result;
        case 0x2a: // NoObject
            result.type = PortableVmValueType::ObjectReference;
            return result;
        default:
            throw std::runtime_error("Portable VM constant evaluator encountered unsupported token");
    }
}

RuntimeObject* ResolveLocal(
    std::int32_t reference,
    const std::vector<RuntimeObject*>& exports,
    PortableRuntimeSummary& summary) {
    if (reference > 0 && static_cast<std::size_t>(reference) <= exports.size()) {
        ++summary.resolvedLinks;
        return exports[static_cast<std::size_t>(reference - 1)];
    }
    if (reference < 0) ++summary.unresolvedExternalLinks;
    return nullptr;
}

std::string PackageStem(const std::string& path) {
    const std::size_t slash = path.find_last_of("/\\");
    const std::size_t begin = slash == std::string::npos ? 0 : slash + 1;
    const std::size_t dot = path.find_last_of('.');
    const std::size_t end = dot == std::string::npos || dot < begin ? path.size() : dot;
    return path.substr(begin, end - begin);
}

void PopulateRuntime(
    RuntimePackage* runtime,
    const PortablePackageTables& package,
    const PortableReflectionGraph& graph,
    PortableRuntimeSummary& summary,
    std::size_t* destroyedObjects) {
    runtime->exports.reserve(graph.objects.size());
    for (const PortableReflectionObject& reflection : graph.objects) {
        runtime->exports.push_back(
            GC::Alloc<RuntimeObject>(reflection, destroyedObjects));
    }

    for (std::size_t index = 0; index < runtime->exports.size(); ++index) {
        RuntimeObject* object = runtime->exports[index];
        const ExportTableEntry& entry = package.exports[index];
        if (RuntimeObject* outer = ResolveLocal(entry.ObjOuter, runtime->exports, summary)) {
            object->outer = outer;
            object->references.push_back(outer);
        }
        if (RuntimeObject* base = ResolveLocal(entry.ObjBase, runtime->exports, summary)) {
            object->base = base;
            object->references.push_back(base);
        }
        if (RuntimeObject* cls = ResolveLocal(entry.ObjClass, runtime->exports, summary)) {
            object->cls = cls;
            object->references.push_back(cls);
        }

        if (object->reflection.metaClass == "Function") {
            object->script = std::make_unique<PortableScriptBody>(
                LoadPortableFunctionScript(package, index));
            summary.normalizedBytecodeBytes += object->script->bytecode.size();
            ++summary.functions;
        } else if (object->reflection.metaClass.size() >= 8 &&
            object->reflection.metaClass.compare(
                object->reflection.metaClass.size() - 8, 8, "Property") == 0) {
            object->property = std::make_unique<PortablePropertyDescriptor>(
                LoadPortablePropertyDescriptor(package, index));
            if (RuntimeObject* type = ResolveLocal(
                    object->property->referencedType, runtime->exports, summary)) {
                object->references.push_back(type);
            }
            if (RuntimeObject* type = ResolveLocal(
                    object->property->secondaryType, runtime->exports, summary)) {
                object->references.push_back(type);
            }
            ++summary.properties;
        }
        if (object->reflection.metaClass == "Class") ++summary.classes;
    }
    summary.objects = runtime->exports.size();
    summary.peakGcObjects = GC::GetStats().numObjects;
}

}  // namespace

PortableRuntimeSummary BuildAndVerifyPortableRuntime(
    const PortablePackageTables& package) {
    PortableRuntimeSummary summary;
    const std::size_t baseline = GC::GetStats().numObjects;
    std::size_t destroyedPackage{};
    const PortableReflectionGraph graph = BuildPortableReflectionGraph(package);
    {
        GCRoot<RuntimePackage> runtime(GC::Alloc<RuntimePackage>(&destroyedPackage));
        PopulateRuntime(
            runtime.get(), package, graph, summary, &summary.destroyedObjects);
        GC::Collect();
        if (GC::GetStats().numObjects != baseline + summary.objects + 1) return summary;
    }
    GC::Collect();
    summary.passed = summary.objects == package.exports.size() &&
        summary.classes == graph.classCount &&
        summary.functions == graph.functionCount &&
        summary.properties == graph.propertyCount &&
        summary.normalizedBytecodeBytes != 0 &&
        summary.destroyedObjects == summary.objects &&
        destroyedPackage == 1 && GC::GetStats().numObjects == baseline;
    return summary;
}

PortableRuntimeSummary InitializePortableRuntime(
    const PortablePackageTables& package) {
    ShutdownPortableRuntime();
    PortableRuntimeSummary summary;
    const std::size_t baseline = GC::GetStats().numObjects;
    const PortableReflectionGraph graph = BuildPortableReflectionGraph(package);
    persistentRuntime = std::make_unique<GCRoot<RuntimePackage>>(
        GC::Alloc<RuntimePackage>(nullptr));
    PopulateRuntime(persistentRuntime->get(), package, graph, summary, nullptr);
    GC::Collect();
    summary.passed = summary.objects == package.exports.size() &&
        summary.classes == graph.classCount &&
        summary.functions == graph.functionCount &&
        summary.properties == graph.propertyCount &&
        summary.normalizedBytecodeBytes != 0 &&
        GC::GetStats().numObjects == baseline + summary.objects + 1;
    return summary;
}

PortableRuntimeSummary InitializePortableRuntime(
    const std::vector<PortablePackageTables>& packages) {
    ShutdownPortableRuntime();
    PortableRuntimeSummary summary;
    const std::size_t baseline = GC::GetStats().numObjects;
    persistentRuntime = std::make_unique<GCRoot<RuntimePackage>>(
        GC::Alloc<RuntimePackage>(nullptr));

    struct PackageSlice {
        const PortablePackageTables* package{};
        PortableReflectionGraph graph;
        std::string name;
        std::size_t first{};
    };
    std::vector<PackageSlice> slices;
    persistentQualifiedObjects.clear();
    for (const PortablePackageTables& package : packages) {
        PackageSlice slice;
        slice.package = &package;
        slice.graph = BuildPortableReflectionGraph(package);
        slice.name = PackageStem(package.sourcePath);
        slice.first = persistentRuntime->get()->exports.size();
        for (PortableReflectionObject& reflection : slice.graph.objects) {
            reflection.objectPath = slice.name + "." + reflection.objectPath;
            RuntimeObject* object = GC::Alloc<RuntimeObject>(reflection, nullptr);
            persistentRuntime->get()->exports.push_back(object);
            persistentQualifiedObjects[reflection.objectPath] = object;
        }
        slices.push_back(std::move(slice));
    }

    const auto resolve = [&](const PackageSlice& slice, std::int32_t reference) {
        if (reference == 0) return static_cast<RuntimeObject*>(nullptr);
        std::string path;
        if (reference > 0) {
            path = slice.name + "." +
                GetPortableObjectPath(*slice.package, reference);
        } else {
            path = GetPortableObjectPath(*slice.package, reference);
        }
        const auto found = persistentQualifiedObjects.find(path);
        if (found != persistentQualifiedObjects.end()) {
            ++summary.resolvedLinks;
            return found->second;
        }
        ++summary.unresolvedExternalLinks;
        return static_cast<RuntimeObject*>(nullptr);
    };

    for (const PackageSlice& slice : slices) {
        for (std::size_t localIndex = 0;
             localIndex < slice.package->exports.size();
             ++localIndex) {
            const std::size_t globalIndex = slice.first + localIndex;
            RuntimeObject* object = persistentRuntime->get()->exports[globalIndex];
            const ExportTableEntry& entry = slice.package->exports[localIndex];
            object->outer = resolve(slice, entry.ObjOuter);
            object->base = resolve(slice, entry.ObjBase);
            object->cls = resolve(slice, entry.ObjClass);
            if (object->outer) object->references.push_back(object->outer);
            if (object->base) object->references.push_back(object->base);
            if (object->cls) object->references.push_back(object->cls);
            if (object->reflection.metaClass == "Function") {
                object->script = std::make_unique<PortableScriptBody>(
                    LoadPortableFunctionScript(*slice.package, localIndex));
                summary.normalizedBytecodeBytes += object->script->bytecode.size();
                ++summary.functions;
            } else if (object->reflection.metaClass.size() >= 8 &&
                object->reflection.metaClass.compare(
                    object->reflection.metaClass.size() - 8, 8, "Property") == 0) {
                object->property = std::make_unique<PortablePropertyDescriptor>(
                    LoadPortablePropertyDescriptor(*slice.package, localIndex));
                for (const std::int32_t reference :
                     {object->property->referencedType,
                      object->property->secondaryType}) {
                    if (RuntimeObject* target = resolve(slice, reference)) {
                        object->references.push_back(target);
                    }
                }
                ++summary.properties;
            }
            if (object->reflection.metaClass == "Class") ++summary.classes;
        }
    }
    summary.objects = persistentRuntime->get()->exports.size();
    summary.peakGcObjects = GC::GetStats().numObjects;
    GC::Collect();
    summary.passed = !packages.empty() && summary.objects != 0 &&
        summary.classes != 0 && summary.functions != 0 &&
        summary.normalizedBytecodeBytes != 0 &&
        GC::GetStats().numObjects == baseline + summary.objects + 1;
    return summary;
}

void ShutdownPortableRuntime() {
    persistentRuntime.reset();
    persistentQualifiedObjects.clear();
    GC::Collect();
}

PortableVmValue ExecutePortableFunction(const std::string& objectPath) {
    if (!persistentRuntime || !persistentRuntime->get()) {
        throw std::runtime_error("Portable VM has no initialized runtime");
    }
    RuntimeObject* function = nullptr;
    for (RuntimeObject* object : persistentRuntime->get()->exports) {
        if ((object->reflection.objectPath == objectPath ||
             (object->reflection.objectPath.size() > objectPath.size() &&
              object->reflection.objectPath.compare(
                  object->reflection.objectPath.size() - objectPath.size(),
                  objectPath.size(), objectPath) == 0 &&
              object->reflection.objectPath[
                  object->reflection.objectPath.size() - objectPath.size() - 1] == '.')) &&
            object->script) {
            function = object;
            break;
        }
    }
    if (function == nullptr) {
        throw std::runtime_error("Portable VM function was not found: " + objectPath);
    }
    RuntimeBytecodeReader reader(function->script->bytecode);
    if (reader.Byte() != 0x04u) {
        throw std::runtime_error("Portable VM function does not begin with Return");
    }
    return EvaluateConstant(reader);
}

PortableMapRuntimeSummary LoadPortableRuntimeMap(
    const PortablePackageTables& package) {
    if (!persistentRuntime || !persistentRuntime->get()) {
        throw std::runtime_error("Cannot load a map without an initialized runtime");
    }
    PortableMapRuntimeSummary summary;
    const std::string packageName = PackageStem(package.sourcePath);
    const PortableReflectionGraph graph = BuildPortableReflectionGraph(package);
    const std::size_t first = persistentRuntime->get()->exports.size();
    persistentRuntime->get()->exports.reserve(first + graph.objects.size());
    for (PortableReflectionObject reflection : graph.objects) {
        reflection.objectPath = packageName + "." + reflection.objectPath;
        RuntimeObject* object = GC::Alloc<RuntimeObject>(reflection, nullptr);
        persistentRuntime->get()->exports.push_back(object);
        persistentQualifiedObjects[reflection.objectPath] = object;
    }

    const auto resolve = [&](std::int32_t reference) {
        if (reference == 0) return static_cast<RuntimeObject*>(nullptr);
        std::string path;
        if (reference > 0) {
            path = packageName + "." + GetPortableObjectPath(package, reference);
        } else {
            path = GetPortableObjectPath(package, reference);
        }
        const auto found = persistentQualifiedObjects.find(path);
        return found == persistentQualifiedObjects.end() ? nullptr : found->second;
    };
    const auto isActorClass = [](RuntimeObject* cls) {
        for (RuntimeObject* current = cls; current != nullptr; current = current->base) {
            if (current->reflection.objectPath == "Engine.Actor") return true;
        }
        return false;
    };

    for (std::size_t localIndex = 0; localIndex < package.exports.size(); ++localIndex) {
        RuntimeObject* object = persistentRuntime->get()->exports[first + localIndex];
        const ExportTableEntry& entry = package.exports[localIndex];
        object->outer = resolve(entry.ObjOuter);
        object->base = resolve(entry.ObjBase);
        object->cls = resolve(entry.ObjClass);
        if (object->outer) object->references.push_back(object->outer);
        if (object->base) object->references.push_back(object->base);
        if (object->cls) {
            object->references.push_back(object->cls);
            ++summary.resolvedClasses;
        } else if (entry.ObjClass != 0) {
            ++summary.unresolvedClasses;
        }
        if (isActorClass(object->cls) && entry.ObjSize > 0) {
            const PortablePropertyStream properties =
                LoadPortableExportProperties(package, localIndex);
            object->instanceProperties = properties.properties;
            summary.actorProperties += object->instanceProperties.size();
            ++summary.actors;
        }
    }
    summary.exports = graph.objects.size();
    GC::Collect();
    summary.passed = summary.exports == package.exports.size() &&
        summary.actors != 0 && summary.actorProperties != 0 &&
        summary.resolvedClasses != 0;
    return summary;
}
