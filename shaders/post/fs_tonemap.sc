$input v_texcoord0

#include "bgfx_shader.sh"

SAMPLER2D(s_hdrColor, 0);
SAMPLER2D(s_bloomTex, 1);
uniform vec4 u_tonemapParams; // x = bloom intensity, y = exposure

// Interleaved gradient noise for dithering (Lagarde 2014)
float interleavedGradientNoise(vec2 screenPos) {
    vec3 magic = vec3(0.06711056, 0.00583715, 52.9829189);
    return fract(magic.z * fract(dot(screenPos, magic.xy)));
}

// ACES filmic tonemapping (Narkowicz 2015 fit)
vec3 acesFilmic(vec3 x) {
    float a = 2.51;
    float b = 0.03;
    float c = 2.43;
    float d = 0.59;
    float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

void main() {
    vec3 hdr = texture2D(s_hdrColor, v_texcoord0).rgb;
    vec3 bloom = texture2D(s_bloomTex, v_texcoord0).rgb;

    float bloomIntensity = u_tonemapParams.x;
    float exposure = u_tonemapParams.y;

    // Composite bloom into HDR color
    vec3 combined = hdr + bloom * bloomIntensity;

    // Apply exposure
    combined *= exposure;

    // ACES filmic tonemapping
    vec3 ldr = acesFilmic(combined);

    // Linear to sRGB gamma (approximate)
    ldr = pow(ldr, vec3_splat(1.0 / 2.2));

    // Apply dithering to break up banding from 8-bit quantization
    float dither = interleavedGradientNoise(gl_FragCoord.xy) - 0.5;
    vec3 dithered = ldr + dither / 255.0;

    gl_FragColor = vec4(dithered, 1.0);
}
