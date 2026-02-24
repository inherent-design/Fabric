#pragma once

#include <RmlUi/Core/RenderInterface.h>

#include <bgfx/bgfx.h>

#include <unordered_map>

namespace fabric {

class BgfxRenderInterface : public Rml::RenderInterface {
  public:
    BgfxRenderInterface();
    ~BgfxRenderInterface() override;

    // Call after bgfx::init() to create GPU resources (shaders, white texture)
    void init();

    // Call before bgfx::shutdown() to release GPU resources
    void shutdown();

    // Call once per frame before Rml::Context::Render() to set up the view
    void beginFrame(uint16_t width, uint16_t height);

    // -- RenderInterface required methods --

    Rml::CompiledGeometryHandle CompileGeometry(Rml::Span<const Rml::Vertex> vertices,
                                                Rml::Span<const int> indices) override;

    void RenderGeometry(Rml::CompiledGeometryHandle geometry, Rml::Vector2f translation,
                        Rml::TextureHandle texture) override;

    void ReleaseGeometry(Rml::CompiledGeometryHandle geometry) override;

    Rml::TextureHandle LoadTexture(Rml::Vector2i& dimensions, const Rml::String& source) override;

    Rml::TextureHandle GenerateTexture(Rml::Span<const Rml::byte> source, Rml::Vector2i dimensions) override;

    void ReleaseTexture(Rml::TextureHandle texture) override;

    void EnableScissorRegion(bool enable) override;
    void SetScissorRegion(Rml::Rectanglei region) override;

    // -- Optional methods --

    void SetTransform(const Rml::Matrix4f* transform) override;

    // -- Accessors for testing --

    bgfx::ViewId viewId() const { return viewId_; }
    const bgfx::VertexLayout& vertexLayout() const { return layout_; }
    bool isScissorEnabled() const { return scissorEnabled_; }
    Rml::Rectanglei scissorRegion() const { return scissorRect_; }

  private:
    struct CompiledGeom {
        bgfx::VertexBufferHandle vbh;
        bgfx::IndexBufferHandle ibh;
        uint32_t indexCount;
    };

    static constexpr bgfx::ViewId kDefaultViewId = 255;

    bgfx::ViewId viewId_ = kDefaultViewId;
    bgfx::VertexLayout layout_;
    bgfx::ProgramHandle program_ = BGFX_INVALID_HANDLE;
    bgfx::UniformHandle texUniform_ = BGFX_INVALID_HANDLE;
    bgfx::TextureHandle whiteTexture_ = BGFX_INVALID_HANDLE;

    bool scissorEnabled_ = false;
    Rml::Rectanglei scissorRect_ = {};

    bool hasTransform_ = false;
    float transform_[16] = {};

    uintptr_t nextGeomHandle_ = 1;
    uintptr_t nextTexHandle_ = 1;
    std::unordered_map<uintptr_t, CompiledGeom> geometries_;
    std::unordered_map<uintptr_t, bgfx::TextureHandle> textures_;
};

} // namespace fabric
