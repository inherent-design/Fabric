#pragma once

#include "fabric/render/SpvOnly.hh"
#include <bgfx/bgfx.h>

namespace fabric::render {

/// Creates a bgfx program from embedded SPIR-V shaders.
/// Looks up vertex and fragment shaders by name in the provided array,
/// creates shader objects, and links them into a program.
bgfx::ProgramHandle createProgramFromEmbedded(const bgfx::EmbeddedShader* shaders, const char* vsName,
                                              const char* fsName);

} // namespace fabric::render
