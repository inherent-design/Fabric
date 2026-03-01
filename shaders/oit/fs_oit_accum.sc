$input v_texcoord0

#include "bgfx_shader.sh"

uniform vec4 u_oitColor; // xyz = color, w = alpha

void main() {
    vec4 color = u_oitColor;
    float alpha = color.a;

    // Linearize depth to [0,1]
    float z = v_texcoord0.x / v_texcoord0.y;
    z = z * 0.5 + 0.5; // map from [-1,1] to [0,1]

    // McGuire & Bavoil 2013 weight function:
    // w(z, alpha) = alpha * max(1e-2, 3e3 * (1 - z)^3)
    float oneMinusZ = 1.0 - z;
    float w = alpha * max(1e-2, 3e3 * oneMinusZ * oneMinusZ * oneMinusZ);

    // MRT output 0: accumulation (premultiplied color * weight, alpha * weight)
    gl_FragData[0] = vec4(color.rgb * alpha * w, alpha * w);

    // MRT output 1: revealage (1 - alpha), stored in red channel
    gl_FragData[1] = vec4(1.0 - alpha, 0.0, 0.0, 1.0);
}
