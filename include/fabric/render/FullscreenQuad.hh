#pragma once

#include <bgfx/bgfx.h>

namespace fabric::render {

/// Returns a shared vertex buffer containing a single oversized triangle
/// that covers the entire clip-space viewport: (-1,-1,0), (3,-1,0), (-1,3,0).
/// Position-only float3 layout. Lazy-initialized on first call.
bgfx::VertexBufferHandle fullscreenTriangleVB();

/// Destroy the shared fullscreen triangle VB. Call before bgfx::shutdown().
void destroyFullscreenTriangleVB();

} // namespace fabric::render
