#pragma once

#include <bgfx/bgfx.h>
#include <cstdint>

namespace recurse {

/// 32-byte vertex for smooth isosurface rendering (SnapMC / Surface Nets).
/// Replaces 8-byte VoxelVertex for VP0+ smooth terrain pipeline.
struct SmoothVoxelVertex {
    float px, py, pz;  // 12 bytes: chunk-local sub-voxel position
    float nx, ny, nz;  // 12 bytes: analytic normal from density gradient
    uint32_t material; // 4 bytes: packed materialId(u16) + AO(u8) + flags(u8)
    uint32_t padding;  // 4 bytes: alignment to 32 bytes

    /// Get the bgfx vertex layout descriptor.
    static const bgfx::VertexLayout& getVertexLayout() {
        static bgfx::VertexLayout s_layout;
        static bool s_initialized = false;
        if (!s_initialized) {
            s_layout.begin()
                .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
                .add(bgfx::Attrib::Normal, 3, bgfx::AttribType::Float)
                .add(bgfx::Attrib::TexCoord0, 4, bgfx::AttribType::Uint8, true) // material packed
                .add(bgfx::Attrib::TexCoord1, 4, bgfx::AttribType::Uint8, true) // padding/reserved
                .end();
            s_initialized = true;
        }
        return s_layout;
    }

    /// Pack materialId + AO + flags into the material field.
    static uint32_t packMaterial(uint16_t materialId, uint8_t ao = 255, uint8_t flags = 0) {
        return static_cast<uint32_t>(materialId) | (static_cast<uint32_t>(ao) << 16) |
               (static_cast<uint32_t>(flags) << 24);
    }

    uint16_t getMaterialId() const { return static_cast<uint16_t>(material & 0xFFFF); }
    uint8_t getAO() const { return static_cast<uint8_t>((material >> 16) & 0xFF); }
    uint8_t getFlags() const { return static_cast<uint8_t>((material >> 24) & 0xFF); }
};
static_assert(sizeof(SmoothVoxelVertex) == 32, "SmoothVoxelVertex must be 32 bytes");

} // namespace recurse
