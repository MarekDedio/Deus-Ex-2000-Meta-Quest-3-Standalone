#pragma once
#include <cmath>
#include <stdexcept>

namespace QuestVr {
// ConfigureChild1004ec50 geometry/dispatch phase. The runtime owns the lock,
// scoped global depth and transactional unlock/deferred-destruction refusal.
// Local visibility alone gates geometry; a hidden ancestor does not do so.
template<class Operations>
void ConfigureNativeWindowGeometry(Operations& operations,float x,float y,float width,float height) {
    for(float value:{x,y,width,height}) if(!std::isfinite(value))
        throw std::runtime_error("ConfigureChild requires finite copied Float inputs");
    operations.WriteBool("bConfigured",true);
    if(!operations.ReadBool("bIsVisible")) return;
    const bool changed=operations.ReadFloat("Width")!=width || operations.ReadFloat("Height")!=height ||
        operations.ReadBool("bNeedsReconfigure");
    operations.WriteBool("bNeedsReconfigure",false);
    // These are CURRENT, not requested geometry. Even numerically equal zero
    // signs are overwritten. No clamping and no bNeedsQuery update belong here.
    operations.WriteFloat("X",x);operations.WriteFloat("Y",y);
    operations.WriteFloat("Width",width);operations.WriteFloat("Height",height);
    if(changed) operations.ChangeConfiguration();
    if(operations.ShouldClip()) {operations.ClipTree();operations.ResortSelection();}
}
} // namespace QuestVr
