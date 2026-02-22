#include "fabric/core/Spatial.hh"
#include <cmath>

namespace fabric {

// Template instantiations for common types
template class Vector2<float, Space::World>;
template class Vector2<float, Space::Local>;
template class Vector3<float, Space::World>;
template class Vector3<float, Space::Local>;
template class Vector4<float, Space::World>;
template class Vector4<float, Space::Local>;
template class Matrix4x4<float>;
template class Transform<float>;

} // namespace fabric
