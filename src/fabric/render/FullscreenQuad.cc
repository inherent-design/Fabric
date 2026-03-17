#include "fabric/render/FullscreenQuad.hh"

namespace fabric::render {

bgfx::VertexBufferHandle fullscreenTriangleVB() {
    static bgfx::VertexBufferHandle s_vb = BGFX_INVALID_HANDLE;
    if (!bgfx::isValid(s_vb)) {
        static const float vertices[] = {
            -1.0f, -1.0f, 0.0f, 3.0f, -1.0f, 0.0f, -1.0f, 3.0f, 0.0f,
        };
        bgfx::VertexLayout layout;
        layout.begin().add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float).end();
        s_vb = bgfx::createVertexBuffer(bgfx::makeRef(vertices, sizeof(vertices)), layout);
    }
    return s_vb;
}

} // namespace fabric::render
