#include "fabric/ui/BgfxRenderInterface.hh"
#include "fabric/ui/BgfxSystemInterface.hh"

#include <gtest/gtest.h>

#include <RmlUi/Core/Types.h>

#include <thread>

// Non-GPU tests for RmlUi backend interfaces.
// GPU-dependent methods (CompileGeometry, RenderGeometry, etc.) require
// a bgfx context and are verified via manual testing.

// -- BgfxSystemInterface --

TEST(BgfxSystemInterfaceTest, ElapsedTimeMonotonic) {
    fabric::BgfxSystemInterface sys;
    double t0 = sys.GetElapsedTime();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    double t1 = sys.GetElapsedTime();
    EXPECT_GT(t1, t0);
}

TEST(BgfxSystemInterfaceTest, ElapsedTimeNonNegative) {
    fabric::BgfxSystemInterface sys;
    EXPECT_GE(sys.GetElapsedTime(), 0.0);
}

TEST(BgfxSystemInterfaceTest, LogMessageReturnsTrue) {
    fabric::BgfxSystemInterface sys;
    EXPECT_TRUE(sys.LogMessage(Rml::Log::LT_INFO, "test message"));
    EXPECT_TRUE(sys.LogMessage(Rml::Log::LT_ERROR, "error message"));
    EXPECT_TRUE(sys.LogMessage(Rml::Log::LT_WARNING, "warning message"));
    EXPECT_TRUE(sys.LogMessage(Rml::Log::LT_DEBUG, "debug message"));
}

// -- BgfxRenderInterface (non-GPU) --

TEST(BgfxRenderInterfaceTest, VertexLayoutStride) {
    fabric::BgfxRenderInterface renderer;
    // RmlUi vertex: Position2F(8) + Color0_4U8(4) + TexCoord0_2F(8) = 20
    EXPECT_EQ(renderer.vertexLayout().getStride(), 20);
}

TEST(BgfxRenderInterfaceTest, DefaultViewId) {
    fabric::BgfxRenderInterface renderer;
    EXPECT_EQ(renderer.viewId(), 255);
}

TEST(BgfxRenderInterfaceTest, ScissorDefaultDisabled) {
    fabric::BgfxRenderInterface renderer;
    EXPECT_FALSE(renderer.isScissorEnabled());
}

TEST(BgfxRenderInterfaceTest, ScissorEnableDisable) {
    fabric::BgfxRenderInterface renderer;
    renderer.EnableScissorRegion(true);
    EXPECT_TRUE(renderer.isScissorEnabled());
    renderer.EnableScissorRegion(false);
    EXPECT_FALSE(renderer.isScissorEnabled());
}

TEST(BgfxRenderInterfaceTest, ScissorRegionStored) {
    fabric::BgfxRenderInterface renderer;
    auto region = Rml::Rectanglei::FromPositionSize({10, 20}, {100, 200});
    renderer.SetScissorRegion(region);
    auto stored = renderer.scissorRegion();
    EXPECT_EQ(stored.Left(), 10);
    EXPECT_EQ(stored.Top(), 20);
    EXPECT_EQ(stored.Width(), 100);
    EXPECT_EQ(stored.Height(), 200);
}

TEST(BgfxRenderInterfaceTest, SetTransformNullResetsState) {
    fabric::BgfxRenderInterface renderer;
    renderer.SetTransform(nullptr);
    // No crash, state is reset (verified indirectly via render)
}

TEST(BgfxRenderInterfaceTest, LoadTextureNonexistentFileReturnsZero) {
    fabric::BgfxRenderInterface renderer;
    Rml::Vector2i dimensions{0, 0};
    auto handle = renderer.LoadTexture(dimensions, "/nonexistent/path/to/image.png");
    EXPECT_EQ(handle, Rml::TextureHandle(0));
}
