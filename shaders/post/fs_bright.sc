$input v_texcoord0

#include "bgfx_shader.sh"

SAMPLER2D(s_hdrColor, 0);
uniform vec4 u_bloomParams; // x = threshold, y = soft knee

void main() {
    vec3 color = texture2D(s_hdrColor, v_texcoord0).rgb;
    float brightness = dot(color, vec3(0.2126, 0.7152, 0.0722));

    float threshold = u_bloomParams.x;
    float knee = u_bloomParams.y;

    // Soft thresholding: smooth transition around the threshold
    float soft = brightness - threshold + knee;
    soft = clamp(soft, 0.0, 2.0 * knee);
    soft = soft * soft / (4.0 * knee + 0.00001);

    float contribution = max(soft, brightness - threshold);
    contribution /= max(brightness, 0.00001);

    gl_FragColor = vec4(color * contribution, 1.0);
}
