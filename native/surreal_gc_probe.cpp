#include "surreal_gc_probe.h"

#include "GC/GC.h"

namespace {

class ProbeObject final : public GCObject {
public:
    explicit ProbeObject(std::size_t* destroyed) : destroyed_(destroyed) {}

    ProbeObject* child{};

protected:
    ~ProbeObject() override {
        if (destroyed_ != nullptr) ++*destroyed_;
    }

    GCAllocation* Mark(GCAllocation* marklist) override {
        return GC::MarkObject(marklist, child);
    }

private:
    std::size_t* destroyed_{};
};

}  // namespace

PortableGcProbeResult RunPortableGcProbe() {
    PortableGcProbeResult result;
    result.baselineObjects = GC::GetStats().numObjects;
    {
        GCRoot<ProbeObject> root(GC::Alloc<ProbeObject>(&result.destroyedObjects));
        root->child = GC::Alloc<ProbeObject>(&result.destroyedObjects);
        result.peakObjects = GC::GetStats().numObjects;
        GC::Collect();
        if (GC::GetStats().numObjects != result.baselineObjects + 2) return result;
    }
    GC::Collect();
    result.finalObjects = GC::GetStats().numObjects;
    result.passed = result.peakObjects == result.baselineObjects + 2 &&
        result.finalObjects == result.baselineObjects && result.destroyedObjects == 2;
    return result;
}
