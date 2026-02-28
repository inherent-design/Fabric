$input v_texcoord0

#include "bgfx_shader.sh"

SAMPLER2D(s_inputTex, 0);
uniform vec4 u_texelSize; // xy = 1.0/resolution, z = blur direction (unused), w = iteration

void main() {
    vec2 texel = u_texelSize.xy;

    // Dual Kawase downsample/upsample blur kernel
    // Sample pattern: center + 4 diagonal samples offset by half-texel
    vec3 sum = texture2D(s_inputTex, v_texcoord0).rgb * 4.0;
    sum += texture2D(s_inputTex, v_texcoord0 + texel * vec2( 1.0,  1.0)).rgb;
    sum += texture2D(s_inputTex, v_texcoord0 + texel * vec2(-1.0,  1.0)).rgb;
    sum += texture2D(s_inputTex, v_texcoord0 + texel * vec2( 1.0, -1.0)).rgb;
    sum += texture2D(s_inputTex, v_texcoord0 + texel * vec2(-1.0, -1.0)).rgb;
    sum /= 8.0;

    gl_FragColor = vec4(sum, 1.0);
}
