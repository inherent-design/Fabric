$input a_position
$output v_dir

#include "bgfx_shader.sh"

void main() {
    // Fullscreen triangle: clip-space position with z=1 (far plane)
    gl_Position = vec4(a_position.xy, 1.0, 1.0);

    // Unproject clip-space to world-space to get view ray direction.
    // The inverse VP matrix reconstructs the world position at the far plane.
    vec4 wpos = mul(u_invViewProj, vec4(a_position.xy, 1.0, 1.0));
    vec3 worldPos = wpos.xyz / wpos.w;

    // Camera world position from inverse view matrix
    vec3 eye = mul(u_invView, vec4(0.0, 0.0, 0.0, 1.0)).xyz;

    v_dir = worldPos - eye;
}
