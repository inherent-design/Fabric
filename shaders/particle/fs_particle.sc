$input v_texcoord0, v_color0

#include "bgfx_shader.sh"

void main() {
    // Radial distance from center of quad (UV 0..1, center at 0.5)
    vec2 uv = v_texcoord0 - vec2(0.5, 0.5);
    float dist = length(uv) * 2.0; // 0 at center, 1 at edge

    // Smooth radial falloff
    float alpha = 1.0 - smoothstep(0.0, 1.0, dist);

    gl_FragColor = vec4(v_color0.rgb, v_color0.a * alpha);
}
