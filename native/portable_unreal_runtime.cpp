#include "portable_unreal_runtime.h"

#include "GC/GC.h"

#include <memory>
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
    std::unique_ptr<PortableScriptBody> script;
    std::unique_ptr<PortablePropertyDescriptor> property;

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
            object->references.push_back(outer);
        }
        if (RuntimeObject* base = ResolveLocal(entry.ObjBase, runtime->exports, summary)) {
            object->references.push_back(base);
        }
        if (RuntimeObject* cls = ResolveLocal(entry.ObjClass, runtime->exports, summary)) {
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

void ShutdownPortableRuntime() {
    persistentRuntime.reset();
    GC::Collect();
}
