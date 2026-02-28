$input a_texcoord0, a_texcoord1, a_texcoord2
$output v_worldPos, v_normal, v_flow

#include "bgfx_shader.sh"

vec3 decodeNormal(float idx) {
    // 0:+X, 1:-X, 2:+Y, 3:-Y, 4:+Z, 5:-Z
    vec3 n = vec3(0.0, 0.0, 0.0);
    if (idx < 0.5) n = vec3( 1.0,  0.0,  0.0);
    else if (idx < 1.5) n = vec3(-1.0,  0.0,  0.0);
    else if (idx < 2.5) n = vec3( 0.0,  1.0,  0.0);
    else if (idx < 3.5) n = vec3( 0.0, -1.0,  0.0);
    else if (idx < 4.5) n = vec3( 0.0,  0.0,  1.0);
    else               n = vec3( 0.0,  0.0, -1.0);
    return n;
}

void main() {
    // Unpack position from first 3 bytes of posNormalAO (a_texcoord0)
    vec3 pos = a_texcoord0.xyz;

    // Byte 3: normalIdx[2:0] | aoLevel[4:3]
    float bits = a_texcoord0.w;
    float normalIdx = floor(mod(bits, 8.0));

    // Flow direction from a_texcoord2 (int8 encoded as uint8, remap to -1..1)
    // Values are stored as uint8 (0-255), reinterpret: 0-127 = 0..1, 128-255 = -1..0
    vec2 flowRaw = a_texcoord2.xy;
    vec2 flow;
    flow.x = flowRaw.x < 128.0 ? flowRaw.x / 127.0 : (flowRaw.x - 256.0) / 127.0;
    flow.y = flowRaw.y < 128.0 ? flowRaw.y / 127.0 : (flowRaw.y - 256.0) / 127.0;

    // World position via model transform
    vec4 worldPos = mul(u_model[0], vec4(pos, 1.0));
    v_worldPos = worldPos.xyz;
    v_normal = decodeNormal(normalIdx);
    v_flow = flow;

    gl_Position = mul(u_modelViewProj, vec4(pos, 1.0));
}
