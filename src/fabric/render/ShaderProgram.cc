#include "fabric/render/ShaderProgram.hh"
#include "fabric/render/SpvOnly.hh"

namespace fabric::render {

bgfx::ProgramHandle createProgramFromEmbedded(const bgfx::EmbeddedShader* shaders, const char* vsName,
                                              const char* fsName) {
    auto type = bgfx::getRendererType();
    auto vs = bgfx::createEmbeddedShader(shaders, type, vsName);
    auto fs = bgfx::createEmbeddedShader(shaders, type, fsName);
    return bgfx::createProgram(vs, fs, true);
}

} // namespace fabric::render
