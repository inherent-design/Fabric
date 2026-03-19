$input a_texcoord0, a_texcoord1
$output v_worldPos, v_normal, v_material

#include "bgfx_shader.sh"

vec3 decodeAxisNormal(float packedByte) {
    float normalIdx = mod(packedByte, 8.0);
    if (normalIdx < 0.5) return vec3(1.0, 0.0, 0.0);
    if (normalIdx < 1.5) return vec3(-1.0, 0.0, 0.0);
    if (normalIdx < 2.5) return vec3(0.0, 1.0, 0.0);
    if (normalIdx < 3.5) return vec3(0.0, -1.0, 0.0);
    if (normalIdx < 4.5) return vec3(0.0, 0.0, 1.0);
    return vec3(0.0, 0.0, -1.0);
}

void main() {
    vec3 localPos = a_texcoord0.xyz * 255.0;
    float packedMeta = floor(a_texcoord0.w * 255.0 + 0.5);
    float ao = floor(packedMeta / 8.0);

    vec4 worldPos = mul(u_model[0], vec4(localPos, 1.0));
    gl_Position = mul(u_viewProj, worldPos);

    v_worldPos = worldPos;
    v_normal = normalize(mul(u_model[0], vec4(decodeAxisNormal(packedMeta), 0.0)).xyz);
    v_material = vec4(a_texcoord1.xy, ao / 255.0, 0.0);
}