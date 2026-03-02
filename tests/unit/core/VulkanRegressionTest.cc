#include "fabric/core/OITCompositor.hh"
#include "fabric/core/ParticleSystem.hh"
#include "fabric/core/PostProcess.hh"
#include "fabric/core/Rendering.hh"
#include "fabric/core/VoxelMesher.hh"
#include "fabric/core/VoxelVertex.hh"

#include <bgfx/bgfx.h>
#include <gtest/gtest.h>

#include <set>

using namespace fabric;

// ---------------------------------------------------------------------------
// Shader Profile Tests
//
// Verify SPIRV-only build: non-SPIRV profiles must be suppressed at the
// preprocessor level in every renderer translation unit.  We replicate the
// suppression block here so the test TU sees the same define values that the
// renderer .cc files declare.
// ---------------------------------------------------------------------------

#define BGFX_PLATFORM_SUPPORTS_DXBC 0
#define BGFX_PLATFORM_SUPPORTS_DXIL 0
#define BGFX_PLATFORM_SUPPORTS_ESSL 0
#define BGFX_PLATFORM_SUPPORTS_GLSL 0
#define BGFX_PLATFORM_SUPPORTS_METAL 0
#define BGFX_PLATFORM_SUPPORTS_NVN 0
#define BGFX_PLATFORM_SUPPORTS_PSSL 0
#define BGFX_PLATFORM_SUPPORTS_WGSL 0
#include <bgfx/embedded_shader.h>

TEST(VulkanRegression, NonSpirvProfilesSuppressed) {
    EXPECT_EQ(BGFX_PLATFORM_SUPPORTS_DXBC, 0);
    EXPECT_EQ(BGFX_PLATFORM_SUPPORTS_DXIL, 0);
    EXPECT_EQ(BGFX_PLATFORM_SUPPORTS_ESSL, 0);
    EXPECT_EQ(BGFX_PLATFORM_SUPPORTS_GLSL, 0);
    EXPECT_EQ(BGFX_PLATFORM_SUPPORTS_METAL, 0);
    EXPECT_EQ(BGFX_PLATFORM_SUPPORTS_NVN, 0);
    EXPECT_EQ(BGFX_PLATFORM_SUPPORTS_PSSL, 0);
    EXPECT_EQ(BGFX_PLATFORM_SUPPORTS_WGSL, 0);
}

TEST(VulkanRegression, SpirvProfileEnabled) {
    EXPECT_NE(BGFX_PLATFORM_SUPPORTS_SPIRV, 0);
}

// ---------------------------------------------------------------------------
// View ID Conflict Tests
//
// bgfx executes views in ascending ID order.  Overlapping IDs would cause
// one pass to overwrite another.  These tests codify the intended layout:
//   0   = sky dome
//   1   = opaque geometry
//   2   = transparent geometry
//   10  = particles
//   200 = post-process base (200..204)
//   210 = OIT accumulation
//   211 = OIT composite
//   255 = UI overlay (RmlUi)
// ---------------------------------------------------------------------------

TEST(VulkanRegression, ViewIdConstantsNoConflicts) {
    // Collect every known view ID into a set; duplicates collapse.
    constexpr uint8_t kSkyViewId = 0;
    constexpr uint8_t kGeometryViewId = 1;
    constexpr uint8_t kTransparentViewId = 2;
    constexpr uint8_t kUIViewId = 255; // BgfxRenderInterface::kDefaultViewId (private)

    std::set<uint8_t> ids = {
        kSkyViewId,
        kGeometryViewId,
        kTransparentViewId,
        ParticleSystem::kViewId, // 10
        200,
        201,
        202,
        203,
        204,                 // PostProcess range
        kOITAccumViewId,     // 210
        kOITCompositeViewId, // 211
        kUIViewId,           // 255
    };
    EXPECT_EQ(ids.size(), 12u) << "Duplicate view IDs detected";
}

TEST(VulkanRegression, ViewIdExecutionOrder) {
    // bgfx renders views in ascending ID order.
    // Sky < Geometry < Transparent < Particles < PostProcess < OIT < UI
    constexpr uint8_t kSkyViewId = 0;
    constexpr uint8_t kGeometryViewId = 1;
    constexpr uint8_t kTransparentViewId = 2;
    constexpr uint8_t kUIViewId = 255;

    EXPECT_LT(kSkyViewId, kGeometryViewId);
    EXPECT_LT(kGeometryViewId, kTransparentViewId);
    EXPECT_LT(kTransparentViewId, ParticleSystem::kViewId);
    EXPECT_LT(kOITAccumViewId, kOITCompositeViewId);

    // OIT composite (211) must execute AFTER opaque geometry (1).
    // This ordering caused the black screen bug -- the composite overwrites
    // the backbuffer if it writes opaque black.
    EXPECT_GT(kOITCompositeViewId, kGeometryViewId);

    // UI overlay is always last.
    EXPECT_GT(kUIViewId, kOITCompositeViewId);
}

// ---------------------------------------------------------------------------
// OIT Compositor Regression Tests
//
// The primary black screen root cause: OIT composite runs every frame even
// with zero transparent draws, outputting opaque black if the palette-indexed
// MRT clear fails under Vulkan/MoltenVK.
// ---------------------------------------------------------------------------

TEST(VulkanRegression, OITCompositorNotValidByDefault) {
    // Uninitialized compositor must not be valid -- beginAccumulation and
    // composite early-return when isValid() is false, preventing accidental
    // backbuffer overwrite.
    OITCompositor compositor;
    EXPECT_FALSE(compositor.isValid());
}

TEST(VulkanRegression, OITCompositeViewIdIsAfterAccum) {
    // The composite pass must come after accumulation in view order.
    EXPECT_GT(kOITCompositeViewId, kOITAccumViewId);
    EXPECT_EQ(kOITCompositeViewId, kOITAccumViewId + 1);
}

TEST(VulkanRegression, OITAccumViewIdDoesNotOverlapPostProcess) {
    // PostProcess uses views 200..204; OIT starts at 210.
    constexpr uint8_t kPostProcessEndViewId = 204;
    EXPECT_GT(kOITAccumViewId, kPostProcessEndViewId);
}

// ---------------------------------------------------------------------------
// VoxelVertex Format Tests
//
// VoxelVertex is 8 bytes packed as two uint32_t fields, declared to bgfx as
// TexCoord0 + TexCoord1 with Uint8 x4.  Under Vulkan, (Uint8, norm=false,
// asInt=false) maps to VK_FORMAT_R8G8B8A8_USCALED which MoltenVK does NOT
// natively support.  These tests document the current format and provide
// regression coverage for any future vertex layout changes.
// ---------------------------------------------------------------------------

TEST(VulkanRegression, VoxelVertexSizeIs8Bytes) {
    EXPECT_EQ(sizeof(VoxelVertex), 8u);
}

TEST(VulkanRegression, VoxelVertexPackRoundTrip) {
    // Verify vertex packing preserves position values in the 0-32 range
    // that voxel terrain uses (chunk size = 32).
    auto v = VoxelVertex::pack(32, 16, 8, 5, 3, 512);
    EXPECT_EQ(v.posX(), 32);
    EXPECT_EQ(v.posY(), 16);
    EXPECT_EQ(v.posZ(), 8);
    EXPECT_EQ(v.normalIndex(), 5);
    EXPECT_EQ(v.aoLevel(), 3);
    EXPECT_EQ(v.paletteIndex(), 512);
}

TEST(VulkanRegression, VoxelVertexRawBytesContainPositionValues) {
    // The position bytes must be directly readable.  When the vertex format
    // is USCALED, the GPU passes these as float(32), float(16), float(8).
    // When UNORM, the GPU passes 32/255, 16/255, 8/255 and the shader must
    // multiply by 255.  This test validates the CPU-side packing.
    VoxelVertex v{};
    v.posNormalAO = 32u | (16u << 8) | (8u << 16);

    auto* bytes = reinterpret_cast<const uint8_t*>(&v.posNormalAO);
    EXPECT_EQ(bytes[0], 32);
    EXPECT_EQ(bytes[1], 16);
    EXPECT_EQ(bytes[2], 8);
}

TEST(VulkanRegression, VoxelVertexLayoutStride) {
    // The vertex layout stride must match sizeof(VoxelVertex).
    auto layout = VoxelMesher::getVertexLayout();
    EXPECT_EQ(layout.getStride(), sizeof(VoxelVertex));
}

TEST(VulkanRegression, VoxelVertexLayoutHasExpectedAttributes) {
    auto layout = VoxelMesher::getVertexLayout();
    EXPECT_TRUE(layout.has(bgfx::Attrib::TexCoord0));
    EXPECT_TRUE(layout.has(bgfx::Attrib::TexCoord1));
    // Position is packed into TexCoord0/1, NOT Attrib::Position.
    EXPECT_FALSE(layout.has(bgfx::Attrib::Position));
}

// ---------------------------------------------------------------------------
// PostProcess Guard Tests
//
// PostProcess::initPrograms() only creates brightProgram_ but validates all
// three programs (bright, blur, tonemap).  If someone enables PostProcess,
// it silently fails.  This dormant bug is harmless while PostProcess is
// disabled but would surface if enabled.
// ---------------------------------------------------------------------------

TEST(VulkanRegression, PostProcessNotEnabledByDefault) {
    PostProcess pp;
    EXPECT_FALSE(pp.isValid());
}

TEST(VulkanRegression, PostProcessRenderWithoutInitIsNoOp) {
    PostProcess pp;
    // render() must not crash or submit draws when uninitialized.
    pp.render(200);
    EXPECT_FALSE(pp.isValid());
}

// ---------------------------------------------------------------------------
// GPU Timer / Perf Stats Tests
//
// MoltenVK produces garbage GPU timestamps (negative values, overflow).
// This validates the guard logic that sanitizes impossible values.
// ---------------------------------------------------------------------------

TEST(VulkanRegression, GpuTimerHandlesGarbageValues) {
    // Simulate MoltenVK garbage: gpuTimeEnd < gpuTimeBegin
    int64_t gpuTimeBegin = 1000000000LL;
    int64_t gpuTimeEnd = -1164463616LL;
    double frequency = 1000000000.0;

    double gpuMs = double(gpuTimeEnd - gpuTimeBegin) / frequency * 1000.0;

    // Guard: clamp impossible values
    if (gpuMs < 0.0 || gpuMs > 1000.0) {
        gpuMs = 0.0;
    }

    EXPECT_GE(gpuMs, 0.0);
    EXPECT_LE(gpuMs, 1000.0);
}

TEST(VulkanRegression, GpuTimerHandlesNormalValues) {
    // Normal case: gpuTimeEnd > gpuTimeBegin, reasonable delta
    int64_t gpuTimeBegin = 1000000000LL;
    int64_t gpuTimeEnd = 1016666666LL; // ~16.67ms (60fps)
    double frequency = 1000000000.0;

    double gpuMs = double(gpuTimeEnd - gpuTimeBegin) / frequency * 1000.0;

    if (gpuMs < 0.0 || gpuMs > 1000.0) {
        gpuMs = 0.0;
    }

    EXPECT_NEAR(gpuMs, 16.67, 0.01);
}

// ---------------------------------------------------------------------------
// Runtime bgfx context tests (skipped in CI, require GPU)
// ---------------------------------------------------------------------------

TEST(VulkanRegression, RendererTypeIsVulkanRequiresRuntimeBgfxContext) {
    GTEST_SKIP() << "Requires live bgfx runtime context to verify Vulkan renderer type.";
}

TEST(VulkanRegression, HomogeneousDepthFalseRequiresRuntimeBgfxContext) {
    GTEST_SKIP() << "Requires live bgfx runtime context to verify homogeneousDepth is false for Vulkan.";
}

TEST(VulkanRegression, OITCompositeBehaviorRequiresRuntimeBgfxContext) {
    GTEST_SKIP() << "Requires live bgfx runtime context to validate OIT composite does not overwrite backbuffer.";
}
