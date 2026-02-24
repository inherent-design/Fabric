$input v_color0, v_normalAo

#include "bgfx_shader.sh"

uniform vec4 u_lightDir; // xyz = normalized direction toward light

void main() {
    vec3 normal = normalize(v_normalAo.xyz);
    float ao = v_normalAo.w;

    // Directional lighting: ambient + diffuse
    float ndotl = max(dot(normal, u_lightDir.xyz), 0.0);
    float light = 0.3 + 0.7 * ndotl;

    // AO darkening
    light *= 0.3 + 0.7 * ao;

    vec4 color = v_color0;
    color.rgb *= light;
    gl_FragColor = color;
}
