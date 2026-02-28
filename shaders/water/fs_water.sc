$input v_worldPos, v_normal, v_flow

#include "bgfx_shader.sh"

uniform vec4 u_waterColor; // xyz = base color, w = alpha
uniform vec4 u_time;       // x = elapsed seconds
uniform vec4 u_lightDir;   // xyz = normalized direction toward light

void main() {
    vec3 normal = normalize(v_normal);

    // Generate world-space UV from XZ plane
    vec2 uv = v_worldPos.xz * 0.25;

    // Scroll UV by flow direction over time
    float t = u_time.x;
    uv += v_flow * t * 0.3;

    // Procedural ripple noise (two overlapping sin waves at different frequencies)
    float ripple1 = sin(uv.x * 6.2832 + t * 1.5) * sin(uv.y * 6.2832 + t * 1.2);
    float ripple2 = sin(uv.x * 12.5664 + t * 2.3 + 0.7) * sin(uv.y * 12.5664 + t * 1.8 + 1.1);
    float ripple = ripple1 * 0.5 + ripple2 * 0.25;

    // Perturb normal slightly by ripple for specular variation
    vec3 perturbedNormal = normalize(normal + vec3(ripple * 0.15, 0.0, ripple * 0.15));

    // Directional lighting: ambient + diffuse
    float ndotl = max(dot(perturbedNormal, u_lightDir.xyz), 0.0);
    float light = 0.35 + 0.65 * ndotl;

    // Fresnel approximation: view angle dependent blend
    // Approximate view direction from fragment position
    vec3 viewDir = normalize(mul(u_invView, vec4(0.0, 0.0, 0.0, 1.0)).xyz - v_worldPos);
    float fresnel = pow(1.0 - max(dot(viewDir, perturbedNormal), 0.0), 3.0);

    // Sky color for Fresnel reflection tint
    vec3 skyColor = vec3(0.6, 0.75, 0.9);
    vec3 baseColor = u_waterColor.xyz;

    // Mix water color with sky based on Fresnel
    vec3 color = mix(baseColor, skyColor, fresnel * 0.5);

    // Apply lighting and ripple brightness variation
    color *= light;
    color += ripple * 0.04; // subtle surface caustic shimmer

    // Alpha from uniform, slightly modulated by ripple
    float alpha = u_waterColor.w + ripple * 0.03;
    alpha = clamp(alpha, 0.0, 1.0);

    gl_FragColor = vec4(color, alpha);
}
