#pragma once

#include <bgfx/bgfx.h>
#include <cstdint>

namespace fabric {

// Weighted blended order-independent transparency compositor.
// Implements the McGuire & Bavoil 2013 technique using two render targets:
//   - Accumulation buffer (RGBA16F): premultiplied color * weight, alpha * weight
//   - Revealage buffer (R8): product of (1 - alpha)
//
// Requires MRT support and RGBA16F texture format. Falls back gracefully
// (isValid() returns false) when hardware does not support the required features.
//
// View layout (caller-provided base view IDs):
//   accumViewId     = transparent geometry rendered with depth-weight blending
//   compositeViewId = fullscreen quad compositing OIT result over opaque
class OITCompositor {
  public:
    OITCompositor();
    ~OITCompositor();

    OITCompositor(const OITCompositor&) = delete;
    OITCompositor& operator=(const OITCompositor&) = delete;

    // Create framebuffers, load shaders, build fullscreen quad.
    // Returns false if hardware lacks MRT or RGBA16F support.
    bool init(uint16_t width, uint16_t height);

    void shutdown();

    bool isValid() const;

    // Set render state for the accumulation pass.
    // Configures the view with the OIT framebuffer and blend state.
    void beginAccumulation(uint8_t viewId, const float* viewMtx, const float* projMtx, uint16_t width, uint16_t height);

    // Composite the OIT result over the current backbuffer (or active framebuffer).
    // Uses alpha blending: src.a * src + (1 - src.a) * dst
    void composite(uint8_t viewId, uint16_t width, uint16_t height);

    // Set the surface color and alpha for the next transparent draw.
    void setColor(float r, float g, float b, float a);

    // Access the accumulation program for external draw call submission.
    bgfx::ProgramHandle accumProgram() const;

    // Access the OIT framebuffer for external view setup.
    bgfx::FrameBufferHandle framebuffer() const;

    // Access the color uniform handle for external submission.
    bgfx::UniformHandle colorUniform() const;

    uint8_t accumViewId() const;
    uint8_t compositeViewId() const;

  private:
    // Shader programs
    bgfx::ProgramHandle accumProgram_;
    bgfx::ProgramHandle compositeProgram_;

    // Fullscreen triangle vertex buffer (for composite pass)
    bgfx::VertexBufferHandle vbh_;

    // Uniforms
    bgfx::UniformHandle uniformOitColor_;

    // Texture samplers (composite pass)
    bgfx::UniformHandle samplerAccum_;
    bgfx::UniformHandle samplerRevealage_;

    // OIT framebuffer: attachment 0 = RGBA16F (accumulation), attachment 1 = R8 (revealage)
    bgfx::FrameBufferHandle oitFb_;

    // Stored view IDs from most recent begin/composite calls
    uint8_t accumViewId_ = 0;
    uint8_t compositeViewId_ = 0;

    uint16_t width_ = 0;
    uint16_t height_ = 0;
    float color_[4] = {1.0f, 1.0f, 1.0f, 0.5f};
    bool initialized_ = false;

    void initPrograms();
    void createFramebuffers(uint16_t width, uint16_t height);
    void destroyFramebuffers();
};

} // namespace fabric
