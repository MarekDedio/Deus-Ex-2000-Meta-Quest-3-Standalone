#pragma once

#include "portable_model_geometry.h"
#include <cstddef>

namespace QuestVr::ModelNodeAccessDetail {
// Serialized callers keep their existing full-domain validation: EVERY raw
// node is active, including unreachable components. These are not public
// already-validated/proof flags or a live-view factory.
inline void Check(const PortableModelGeometry&) noexcept {}
inline bool Active(const PortableModelGeometry&,std::size_t) noexcept {return true;}

// Shared INTERNAL kernels can consume a runtime-private proxy. Its producer
// authenticates current ownership/root lease and every indexed read; only its
// genuine free dynamic reserve is inactive. No runtime authority is created by
// these dispatch helpers, and public raw/immutable APIs remain concrete types.
template<class Model> inline void Check(const Model& model) {model.nodes.check();}
template<class Model> inline bool Active(const Model& model,std::size_t index) {return model.nodes.active(index);}
} // namespace QuestVr::ModelNodeAccessDetail
