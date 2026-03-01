$input a_position
$output v_texcoord0

#include "bgfx_shader.sh"

void main() {
    gl_Position = vec4(a_position.xy, 0.0, 1.0);

    // Map clip-space [-1,1] to UV [0,1], flip Y for texture sampling
    v_texcoord0 = a_position.xy * 0.5 + 0.5;
#if !BGFX_SHADER_LANGUAGE_GLSL
    v_texcoord0.y = 1.0 - v_texcoord0.y;
#endif
}
