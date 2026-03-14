#include "fabric/ui/BgfxRenderInterface.hh"
#include "fabric/log/Log.hh"
#include "fabric/utils/Profiler.hh"

#include "stb_image.h"

// Vulkan-only: suppress all non-SPIR-V shader profiles so
// BGFX_EMBEDDED_SHADER only references *_spv symbol arrays.
#define BGFX_PLATFORM_SUPPORTS_DXBC 0
#define BGFX_PLATFORM_SUPPORTS_DXIL 0
#define BGFX_PLATFORM_SUPPORTS_ESSL 0
#define BGFX_PLATFORM_SUPPORTS_GLSL 0
#define BGFX_PLATFORM_SUPPORTS_METAL 0
#define BGFX_PLATFORM_SUPPORTS_NVN 0
#define BGFX_PLATFORM_SUPPORTS_PSSL 0
#define BGFX_PLATFORM_SUPPORTS_WGSL 0
#include <bgfx/embedded_shader.h>

#include <bx/math.h>

// Compiled SPIR-V shader bytecode generated at build time from .sc sources.
#include "spv/fs_rmlui.sc.bin.h"
#include "spv/vs_rmlui.sc.bin.h"

static const bgfx::EmbeddedShader s_embeddedShaders[] = {BGFX_EMBEDDED_SHADER(vs_rmlui), BGFX_EMBEDDED_SHADER(fs_rmlui),
                                                         BGFX_EMBEDDED_SHADER_END()};

namespace fabric {

namespace {

// Premultiplied alpha: SRC=ONE, DST=INV_SRC_ALPHA
constexpr uint64_t K_RENDER_STATE = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_MSAA |
                                    BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_ONE, BGFX_STATE_BLEND_INV_SRC_ALPHA);

} // namespace

BgfxRenderInterface::BgfxRenderInterface() {
    bx::mtxIdentity(transform_);

    layout_.begin()
        .add(bgfx::Attrib::Position, 2, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
        .add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
        .end();
}

BgfxRenderInterface::~BgfxRenderInterface() = default;

void BgfxRenderInterface::init() {
    FABRIC_ZONE_SCOPED;

    bgfx::RendererType::Enum type = bgfx::getRendererType();
    program_.reset(bgfx::createProgram(bgfx::createEmbeddedShader(s_embeddedShaders, type, "vs_rmlui"),
                                       bgfx::createEmbeddedShader(s_embeddedShaders, type, "fs_rmlui"), true));

    texUniform_.reset(bgfx::createUniform("s_tex", bgfx::UniformType::Sampler));

    // 1x1 white texture for untextured geometry
    uint32_t white = 0xFFFFFFFF;
    whiteTexture_.reset(bgfx::createTexture2D(1, 1, false, 1, bgfx::TextureFormat::RGBA8,
                                              BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP,
                                              bgfx::copy(&white, sizeof(white))));

    FABRIC_LOG_INFO("RmlUi bgfx render interface initialized (view {})", viewId_);
}

void BgfxRenderInterface::shutdown() {
    FABRIC_ZONE_SCOPED;

    geometries_.clear();
    textures_.destroyAll();

    whiteTexture_.reset();
    texUniform_.reset();
    program_.reset();

    FABRIC_LOG_INFO("RmlUi bgfx render interface shut down");
}

void BgfxRenderInterface::beginFrame(uint16_t width, uint16_t height) {
    FABRIC_ZONE_SCOPED;

    float ortho[16];
    const bgfx::Caps* caps = bgfx::getCaps();
    bx::mtxOrtho(ortho, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 0.0f, 1000.0f, 0.0f,
                 caps->homogeneousDepth);

    bgfx::setViewTransform(viewId_, nullptr, ortho);
    bgfx::setViewRect(viewId_, 0, 0, width, height);
    bgfx::setViewMode(viewId_, bgfx::ViewMode::Sequential);
    bgfx::setViewClear(viewId_, BGFX_CLEAR_NONE);
    bgfx::touch(viewId_);
}

// -- Geometry --

Rml::CompiledGeometryHandle BgfxRenderInterface::CompileGeometry(Rml::Span<const Rml::Vertex> vertices,
                                                                 Rml::Span<const int> indices) {
    FABRIC_ZONE_SCOPED;

    CompiledGeom geom;
    geom.vbh.reset(bgfx::createVertexBuffer(
        bgfx::copy(vertices.data(), static_cast<uint32_t>(vertices.size() * sizeof(Rml::Vertex))), layout_));
    geom.ibh.reset(bgfx::createIndexBuffer(
        bgfx::copy(indices.data(), static_cast<uint32_t>(indices.size() * sizeof(int))), BGFX_BUFFER_INDEX32));
    geom.indexCount = static_cast<uint32_t>(indices.size());

    auto handle = nextGeomHandle_++;
    geometries_[handle] = std::move(geom);
    return static_cast<Rml::CompiledGeometryHandle>(handle);
}

void BgfxRenderInterface::RenderGeometry(Rml::CompiledGeometryHandle geometry, Rml::Vector2f translation,
                                         Rml::TextureHandle texture) {
    FABRIC_ZONE_SCOPED;

    auto it = geometries_.find(static_cast<uintptr_t>(geometry));
    if (it == geometries_.end())
        return;

    const auto& geom = it->second;

    // Build model matrix: combine stored transform with per-call translation
    float model[16];
    if (hasTransform_) {
        // Start with the stored CSS transform, then apply translation
        float translate[16];
        bx::mtxTranslate(translate, translation.x, translation.y, 0.0f);
        bx::mtxMul(model, transform_, translate);
    } else {
        bx::mtxTranslate(model, translation.x, translation.y, 0.0f);
    }
    bgfx::setTransform(model);

    bgfx::setVertexBuffer(0, geom.vbh.get());
    bgfx::setIndexBuffer(geom.ibh.get());

    // Bind texture: use white placeholder if no texture provided
    bgfx::TextureHandle tex = whiteTexture_.get();
    if (texture) {
        auto texHandle = textures_.get(static_cast<uintptr_t>(texture));
        if (bgfx::isValid(texHandle)) {
            tex = texHandle;
        }
    }
    bgfx::setTexture(0, texUniform_.get(), tex);

    // Per-draw-call scissor
    if (scissorEnabled_) {
        bgfx::setScissor(static_cast<uint16_t>(scissorRect_.Left()), static_cast<uint16_t>(scissorRect_.Top()),
                         static_cast<uint16_t>(scissorRect_.Width()), static_cast<uint16_t>(scissorRect_.Height()));
    }

    bgfx::setState(K_RENDER_STATE);
    bgfx::submit(viewId_, program_.get());
}

void BgfxRenderInterface::ReleaseGeometry(Rml::CompiledGeometryHandle geometry) {
    geometries_.erase(static_cast<uintptr_t>(geometry));
}

// -- Textures --

Rml::TextureHandle BgfxRenderInterface::LoadTexture(Rml::Vector2i& dimensions, const Rml::String& source) {
    FABRIC_ZONE_SCOPED;

    int w = 0, h = 0, channels = 0;
    unsigned char* data = stbi_load(source.c_str(), &w, &h, &channels, 4);
    if (!data) {
        FABRIC_LOG_WARN("LoadTexture failed: {}", source);
        return Rml::TextureHandle(0);
    }

    dimensions.x = w;
    dimensions.y = h;

    uint32_t size = static_cast<uint32_t>(w * h * 4);
    bgfx::TextureHandle tex =
        bgfx::createTexture2D(static_cast<uint16_t>(w), static_cast<uint16_t>(h), false, 1, bgfx::TextureFormat::RGBA8,
                              BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP, bgfx::copy(data, size));

    stbi_image_free(data);

    auto handle = nextTexHandle_++;
    textures_.emplace(handle, tex);
    return static_cast<Rml::TextureHandle>(handle);
}

Rml::TextureHandle BgfxRenderInterface::GenerateTexture(Rml::Span<const Rml::byte> source, Rml::Vector2i dimensions) {
    FABRIC_ZONE_SCOPED;

    bgfx::TextureHandle tex = bgfx::createTexture2D(
        static_cast<uint16_t>(dimensions.x), static_cast<uint16_t>(dimensions.y), false, 1, bgfx::TextureFormat::RGBA8,
        BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP, bgfx::copy(source.data(), static_cast<uint32_t>(source.size())));

    auto handle = nextTexHandle_++;
    textures_.emplace(handle, tex);
    return static_cast<Rml::TextureHandle>(handle);
}

void BgfxRenderInterface::ReleaseTexture(Rml::TextureHandle texture) {
    textures_.erase(static_cast<uintptr_t>(texture));
}

// -- Scissor --

void BgfxRenderInterface::EnableScissorRegion(bool enable) {
    scissorEnabled_ = enable;
}

void BgfxRenderInterface::SetScissorRegion(Rml::Rectanglei region) {
    scissorRect_ = region;
}

// -- Transform --

void BgfxRenderInterface::SetTransform(const Rml::Matrix4f* transform) {
    if (transform) {
        // RmlUi Matrix4f is column-major, same as bgfx
        hasTransform_ = true;
        const auto* data = transform->data();
        for (int i = 0; i < 16; ++i) {
            transform_[i] = data[i];
        }
    } else {
        hasTransform_ = false;
        bx::mtxIdentity(transform_);
    }
}

} // namespace fabric
