$input a_position, i_data0, i_data1, i_data2
$output v_texcoord0, v_color0

#include "bgfx_shader.sh"

void main() {
    // Instance data:
    //   i_data0.xyz = world position, i_data0.w = size
    //   i_data1     = color (rgba)
    //   i_data2.x   = rotation (radians), i_data2.y = age ratio (0..1)

    vec3 center = i_data0.xyz;
    float size  = i_data0.w;
    vec4 color  = i_data1;
    float rot   = i_data2.x;

    // Camera right and up from inverse view matrix (billboard orientation)
    vec3 camRight = vec3(u_invView[0][0], u_invView[0][1], u_invView[0][2]);
    vec3 camUp    = vec3(u_invView[1][0], u_invView[1][1], u_invView[1][2]);

    // Apply rotation around the view direction
    float cr = cos(rot);
    float sr = sin(rot);
    vec3 rotRight = camRight * cr + camUp * sr;
    vec3 rotUp    = -camRight * sr + camUp * cr;

    // Expand quad corner: a_position.xy is in [-0.5, 0.5]
    vec3 worldPos = center
                  + rotRight * a_position.x * size
                  + rotUp    * a_position.y * size;

    gl_Position = mul(u_viewProj, vec4(worldPos, 1.0));

    // Pass UV and color to fragment shader
    v_texcoord0 = a_position.xy + vec2(0.5, 0.5);
    v_color0    = color;
}
