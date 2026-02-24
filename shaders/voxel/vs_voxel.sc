$input a_texcoord0, a_texcoord1
$output v_color0, v_normalAo

#include "bgfx_shader.sh"

uniform vec4 u_palette[256];

vec3 decodeNormal(int idx) {
    // 0:+X, 1:-X, 2:+Y, 3:-Y, 4:+Z, 5:-Z
    vec3 n = vec3(0.0, 0.0, 0.0);
    if (idx == 0) n = vec3( 1.0,  0.0,  0.0);
    else if (idx == 1) n = vec3(-1.0,  0.0,  0.0);
    else if (idx == 2) n = vec3( 0.0,  1.0,  0.0);
    else if (idx == 3) n = vec3( 0.0, -1.0,  0.0);
    else if (idx == 4) n = vec3( 0.0,  0.0,  1.0);
    else               n = vec3( 0.0,  0.0, -1.0);
    return n;
}

void main() {
    // Unpack position (chunk-local 0-32) from first 3 bytes
    vec3 pos = a_texcoord0.xyz;

    // Byte 3: normalIdx[2:0] | aoLevel[4:3]
    int packed = int(a_texcoord0.w);
    int normalIdx = packed & 7;
    int aoLevel = (packed >> 3) & 3;

    // Palette index from first 2 bytes of second attribute
    int palIdx = int(a_texcoord1.x) + int(a_texcoord1.y) * 256;

    gl_Position = mul(u_modelViewProj, vec4(pos, 1.0));
    v_color0 = u_palette[palIdx];
    v_normalAo = vec4(decodeNormal(normalIdx), float(aoLevel) / 3.0);
}
