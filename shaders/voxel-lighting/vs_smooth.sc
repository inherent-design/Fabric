$input a_position, a_normal, a_texcoord0
$output v_worldPos, v_normal, v_material

#include "bgfx_shader.sh"

void main() {
    vec4 worldPos = mul(u_model[0], vec4(a_position, 1.0));
    gl_Position = mul(u_viewProj, worldPos);

    v_worldPos = worldPos;
    v_normal = normalize(mul(u_model[0], vec4(a_normal, 0.0)).xyz);
    v_material = a_texcoord0;
}
