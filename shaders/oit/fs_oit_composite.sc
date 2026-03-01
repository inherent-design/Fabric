$input v_texcoord0

#include "bgfx_shader.sh"

SAMPLER2D(s_oitAccum,    0);
SAMPLER2D(s_oitRevealage, 1);

void main() {
    vec4 accum = texture2D(s_oitAccum, v_texcoord0);
    float revealage = texture2D(s_oitRevealage, v_texcoord0).r;

    // Weighted blended OIT composite:
    // finalColor = (1 - revealage) * (accum.rgb / max(accum.a, 1e-5))
    //            + revealage * opaqueColor
    // Since we blend over the existing backbuffer (opaque), use alpha blending:
    //   src = vec4(accum.rgb / max(accum.a, 1e-5), 1.0 - revealage)
    //   blend: srcAlpha * src + (1 - srcAlpha) * dst
    vec3 averageColor = accum.rgb / max(accum.a, 1e-5);

    gl_FragColor = vec4(averageColor, 1.0 - revealage);
}
