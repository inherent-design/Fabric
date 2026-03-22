#include "fabric/render/FullscreenQuad.hh"

namespace fabric::render {

static bgfx::VertexBufferHandle s_fullscreenVB = BGFX_INVALID_HANDLE;

bgfx::VertexBufferHandle fullscreenTriangleVB() {
    if (!bgfx::isValid(s_fullscreenVB)) {
        static const float vertices[] = {
            -1.0f, -1.0f, 0.0f, 3.0f, -1.0f, 0.0f, -1.0f, 3.0f, 0.0f,
        };
        bgfx::VertexLayout layout;
        layout.begin().add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float).end();
        s_fullscreenVB = bgfx::createVertexBuffer(bgfx::makeRef(vertices, sizeof(vertices)), layout);
    }
    return s_fullscreenVB;
}

void destroyFullscreenTriangleVB() {
    if (bgfx::isValid(s_fullscreenVB)) {
        bgfx::destroy(s_fullscreenVB);
        s_fullscreenVB = BGFX_INVALID_HANDLE;
    }
}

} // namespace fabric::render
