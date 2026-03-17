#pragma once

#include "fabric/render/BgfxHandle.hh"
#include "fabric/render/ViewLayout.hh"

#include <bgfx/bgfx.h>
#include <cstdint>

namespace fabric {

// HDR post-processing pipeline: brightness extraction, dual Kawase blur, ACES
// tonemapping. Scene renders into an RGBA16F offscreen framebuffer; the pipeline
// reads that framebuffer and composites the final LDR result to the backbuffer.
//
// View layout (base + offset):
//   baseViewId + 0 - brightness extraction
//   baseViewId + 1..4 - blur passes (downsample/upsample)
//   baseViewId + 5 - tonemapping composite to backbuffer
class PostProcess {
  public:
    PostProcess();
    ~PostProcess();

    PostProcess(const PostProcess&) = delete;
    PostProcess& operator=(const PostProcess&) = delete;

    // Create the HDR framebuffer, bloom chain, and shader programs.
    // Requires live bgfx context.
    void init(uint16_t width, uint16_t height);

    void shutdown();

    bool isValid() const;

    // Recreate framebuffers at new dimensions.
    void resize(uint16_t width, uint16_t height);

    // Brightness extraction threshold (default 1.0)
    void setThreshold(float threshold);
    float threshold() const;

    // Bloom intensity multiplier (default 0.5)
    void setIntensity(float intensity);
    float intensity() const;

    // Exposure value for tonemapping (default 1.0)
    void setExposure(float exposure);
    float exposure() const;

    // Returns the HDR framebuffer that the scene should render into.
    bgfx::FrameBufferHandle hdrFramebuffer() const;

    // Execute the post-process chain. Call after scene has been rendered
    // into hdrFramebuffer(). Writes final result to the default backbuffer.
    void render(uint8_t baseViewId = render::view::K_POST_BASE);

    // Set output framebuffer for the tonemap pass. BGFX_INVALID_HANDLE = backbuffer.
    void setOutputTarget(bgfx::FrameBufferHandle fb);

  private:
    static constexpr uint16_t K_BLUR_PASSES = 4;

    void initPrograms();
    void createFramebuffers(uint16_t width, uint16_t height);
    void destroyFramebuffers();

    // Shader programs
    BgfxHandle<bgfx::ProgramHandle> brightProgram_;
    BgfxHandle<bgfx::ProgramHandle> blurProgram_;
    BgfxHandle<bgfx::ProgramHandle> tonemapProgram_;

    // Uniforms
    BgfxHandle<bgfx::UniformHandle> uniformBloomParams_;   // threshold, soft knee
    BgfxHandle<bgfx::UniformHandle> uniformTexelSize_;     // 1/resolution
    BgfxHandle<bgfx::UniformHandle> uniformTonemapParams_; // bloom intensity, exposure

    // Texture samplers
    BgfxHandle<bgfx::UniformHandle> samplerHdrColor_;
    BgfxHandle<bgfx::UniformHandle> samplerBloomTex_;
    BgfxHandle<bgfx::UniformHandle> samplerInputTex_;

    // HDR scene framebuffer (RGBA16F + depth)
    BgfxHandle<bgfx::FrameBufferHandle> hdrFb_;

    // Bloom chain framebuffers (progressively halved resolution)
    BgfxHandle<bgfx::FrameBufferHandle> bloomFb_[K_BLUR_PASSES];

    uint16_t width_ = 0;
    uint16_t height_ = 0;
    float threshold_ = 1.0f;
    float intensity_ = 0.5f;
    float exposure_ = 1.0f;
    bool initialized_ = false;
    bgfx::FrameBufferHandle outputTarget_ = BGFX_INVALID_HANDLE;
};

} // namespace fabric
