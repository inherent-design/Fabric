#pragma once

#include <bgfx/bgfx.h>
#include <cstdint>

namespace recurse {

// Packed 8-byte voxel vertex for GPU bandwidth efficiency.
// posNormalAO: px[7:0] | py[15:8] | pz[23:16] | normalIdx[26:24] | ao[28:27] | pad[31:29]
// appearance:  essenceIdx[15:0] | reserved[31:16]
struct VoxelVertex {
    uint32_t posNormalAO;
    uint32_t appearance;

    /// Get the bgfx vertex layout descriptor.
    static const bgfx::VertexLayout& getVertexLayout() {
        static bgfx::VertexLayout s_layout;
        static bool s_initialized = false;
        if (!s_initialized) {
            s_layout.begin()
                .add(bgfx::Attrib::TexCoord0, 4, bgfx::AttribType::Uint8, true) // posNormalAO
                .add(bgfx::Attrib::TexCoord1, 4, bgfx::AttribType::Uint8, true) // appearance
                .end();
            s_initialized = true;
        }
        return s_layout;
    }

    static VoxelVertex pack(uint8_t px, uint8_t py, uint8_t pz, uint8_t normalIdx, uint8_t ao, uint16_t essenceIdx) {
        VoxelVertex v;
        v.posNormalAO = static_cast<uint32_t>(px) | (static_cast<uint32_t>(py) << 8) |
                        (static_cast<uint32_t>(pz) << 16) | (static_cast<uint32_t>(normalIdx & 0x7) << 24) |
                        (static_cast<uint32_t>(ao & 0x3) << 27);
        v.appearance = static_cast<uint32_t>(essenceIdx);
        return v;
    }

    uint8_t posX() const { return static_cast<uint8_t>(posNormalAO & 0xFF); }
    uint8_t posY() const { return static_cast<uint8_t>((posNormalAO >> 8) & 0xFF); }
    uint8_t posZ() const { return static_cast<uint8_t>((posNormalAO >> 16) & 0xFF); }
    uint8_t normalIndex() const { return static_cast<uint8_t>((posNormalAO >> 24) & 0x7); }
    uint8_t aoLevel() const { return static_cast<uint8_t>((posNormalAO >> 27) & 0x3); }
    uint16_t essenceIndex() const { return static_cast<uint16_t>(appearance & 0xFFFF); }
};

static_assert(sizeof(VoxelVertex) == 8, "VoxelVertex must be 8 bytes");

} // namespace recurse
