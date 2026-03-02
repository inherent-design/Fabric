#pragma once

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
    void render(uint8_t baseViewId = 200);

  private:
    static constexpr uint16_t kBlurPasses = 4;

    void initPrograms();
    void createFramebuffers(uint16_t width, uint16_t height);
    void destroyFramebuffers();

    // Shader programs
    bgfx::ProgramHandle brightProgram_;
    bgfx::ProgramHandle blurProgram_;
    bgfx::ProgramHandle tonemapProgram_;

    // Fullscreen triangle vertex buffer (shared across all passes)
    bgfx::VertexBufferHandle vbh_;

    // Uniforms
    bgfx::UniformHandle uniformBloomParams_;   // threshold, soft knee
    bgfx::UniformHandle uniformTexelSize_;     // 1/resolution
    bgfx::UniformHandle uniformTonemapParams_; // bloom intensity, exposure

    // Texture samplers
    bgfx::UniformHandle samplerHdrColor_;
    bgfx::UniformHandle samplerBloomTex_;
    bgfx::UniformHandle samplerInputTex_;

    // HDR scene framebuffer (RGBA16F + depth)
    bgfx::FrameBufferHandle hdrFb_;

    // Bloom chain framebuffers (progressively halved resolution)
    bgfx::FrameBufferHandle bloomFb_[kBlurPasses];

    uint16_t width_ = 0;
    uint16_t height_ = 0;
    float threshold_ = 1.0f;
    float intensity_ = 0.5f;
    float exposure_ = 1.0f;
    bool initialized_ = false;
};

} // namespace fabric
