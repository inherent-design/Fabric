#pragma once

#include <bgfx/bgfx.h>
#include <cstdint>

namespace recurse {

/// 32-byte smooth-terrain vertex used by smooth meshers and CPU-side staging.
///
/// Greedy is the current production near-chunk path and typically packs down
/// to VoxelVertex before upload. This format remains the higher-fidelity
/// interchange for optional smooth meshers and comparison paths.
struct SmoothVoxelVertex {
    float px, py, pz;  // 12 bytes: chunk-local sub-voxel position
    float nx, ny, nz;  // 12 bytes: analytic normal from density gradient
    uint32_t material; // 4 bytes: packed material payload + AO + flags (see helpers below)
    uint32_t padding;  // 4 bytes: alignment to 32 bytes

    static constexpr uint8_t K_SHADER_DEFAULT_AO = 3;

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

    /// Pack a raw material payload plus AO and flags into the material field.
    /// Mesh extractors use this for intermediate material IDs before the
    /// render path repacks them to shader palette indices.
    static uint32_t packMaterial(uint16_t materialId, uint8_t ao = 255, uint8_t flags = 0) {
        return static_cast<uint32_t>(materialId) | (static_cast<uint32_t>(ao) << 16) |
               (static_cast<uint32_t>(flags) << 24);
    }

    /// Pack a smooth-terrain shader payload.
    /// The low 16 bits are a palette index into u_palette, and AO must be in
    /// the 0..15 range consumed by fs_smooth.sc.
    static uint32_t packShaderMaterial(uint16_t paletteIndex, uint8_t ao = K_SHADER_DEFAULT_AO, uint8_t flags = 0) {
        return packMaterial(paletteIndex, ao, flags);
    }

    uint16_t getMaterialId() const { return static_cast<uint16_t>(material & 0xFFFF); }
    uint8_t getAO() const { return static_cast<uint8_t>((material >> 16) & 0xFF); }
    uint8_t getFlags() const { return static_cast<uint8_t>((material >> 24) & 0xFF); }
};
static_assert(sizeof(SmoothVoxelVertex) == 32, "SmoothVoxelVertex must be 32 bytes");

} // namespace recurse
