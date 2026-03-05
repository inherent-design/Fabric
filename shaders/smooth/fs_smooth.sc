$input v_worldPos, v_normal, v_material

#include "bgfx_shader.sh"

uniform vec4 u_lightDir;      // xyz = normalized toward light
uniform vec4 u_viewPos;       // xyz = camera world position
uniform vec4 u_litColor;      // warm gold: (0.95, 0.85, 0.55, 1.0)
uniform vec4 u_shadowColor;   // cool purple-gray: (0.45, 0.35, 0.55, 1.0)
uniform vec4 u_rimParams;     // x = power (3.0), y = strength (0.15) - view-dependent, keep low
uniform vec4 u_oceanParams;   // x = power (16.0), y = strength (0.2) - view-dependent, keep low
uniform vec4 u_palette[256];  // material colors

void main() {
    vec3 N = normalize(v_normal);
    vec3 L = normalize(u_lightDir.xyz);
    vec3 V = normalize(u_viewPos.xyz - v_worldPos.xyz);

    // Material color from palette
    vec4 matData = v_material * 255.0;
    int palIdx = int(min(matData.x + matData.y * 256.0, 255.0));
    float ao = matData.z / 15.0;
    vec4 baseColor = u_palette[palIdx];

    // Layer 1: Diffuse Contrast (Journey style)
    // Squash vertical component to widen lit area on slopes
    vec3 Nc = N;
    Nc.y *= 0.3;
    Nc = normalize(Nc);
    float NdotL = saturate(4.0 * dot(Nc, L));
    vec3 diffuse = mix(u_shadowColor.rgb, u_litColor.rgb, NdotL);

    // Layer 2: Rim Lighting
    float rim = pow(1.0 - saturate(dot(N, V)), u_rimParams.x) * u_rimParams.y;

    // Layer 3: Ocean Specular (Blinn-Phong)
    vec3 H = normalize(L + V);
    float NdotH = saturate(dot(N, H));
    float ocean = pow(NdotH, u_oceanParams.x) * u_oceanParams.y;

    // Combine: base * diffuse + additive rim/specular, attenuated by AO
    vec3 color = baseColor.rgb * diffuse;
    color += max(rim, ocean);
    color *= 0.3 + 0.7 * ao;

    gl_FragColor = vec4(color, baseColor.a);
}
