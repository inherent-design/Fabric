$input v_dir

#include "bgfx_shader.sh"

uniform vec4 u_sunDirection; // xyz = normalized direction toward sun
uniform vec4 u_skyParams;    // x = turbidity

// Perez luminance distribution function (Preetham et al. 1999)
// Models the angular distribution of sky radiance.
// theta: zenith angle of sample direction
// gamma: angle between sample direction and sun
float perez(float cosTheta, float gamma, float cosGamma,
            float A, float B, float C, float D, float E) {
    return (1.0 + A * exp(B / max(cosTheta, 0.01)))
         * (1.0 + C * exp(D * gamma) + E * cosGamma * cosGamma);
}

void main() {
    vec3 dir = normalize(v_dir);
    vec3 sunDir = normalize(u_sunDirection.xyz);
    float T = u_skyParams.x;

    float cosTheta = max(dir.y, 0.001);
    float cosThetaS = max(sunDir.y, 0.001);
    float cosGamma = clamp(dot(dir, sunDir), -1.0, 1.0);
    float gamma = acos(cosGamma);
    float thetaS = acos(cosThetaS);

    // Preetham luminance (Y) distribution coefficients
    float A = 0.1787 * T - 1.4630;
    float B = -0.3554 * T + 0.4275;
    float C = -0.0227 * T + 5.3251;
    float D = 0.1206 * T - 2.5771;
    float E = -0.0670 * T + 0.3703;

    float Yp = perez(cosTheta, gamma, cosGamma, A, B, C, D, E)
             / perez(1.0, thetaS, cosThetaS, A, B, C, D, E);

    // Zenith luminance from turbidity and sun angle
    float chi = (4.0 / 9.0 - T / 120.0) * (3.14159265 - 2.0 * thetaS);
    float Yz = (4.0453 * T - 4.9710) * tan(chi) - 0.2155 * T + 2.4192;
    float Y = max(Yz * Yp, 0.0);

    // Sky chromaticity gradient: deep blue at zenith, warm white at horizon
    float horizonBlend = pow(max(1.0 - dir.y, 0.0), 3.0);
    vec3 zenithColor = vec3(0.15, 0.25, 0.65);
    vec3 horizonColor = vec3(0.70, 0.65, 0.60);
    vec3 skyColor = mix(zenithColor, horizonColor, horizonBlend);

    // Modulate by Perez luminance distribution
    skyColor *= Y * 0.05;

    // Sun disc (angular radius ~0.26 degrees)
    float sunDisc = smoothstep(0.9996, 0.9998, cosGamma);
    skyColor += vec3(1.5, 1.3, 0.9) * sunDisc;

    // Mie scattering glow around sun
    float glow = pow(max(cosGamma, 0.0), 32.0);
    skyColor += vec3(0.4, 0.3, 0.1) * glow;

    // Below-horizon fade
    float horizonFade = smoothstep(-0.02, 0.0, dir.y);
    skyColor *= horizonFade;

    // Reinhard tone mapping
    skyColor = skyColor / (skyColor + vec3_splat(1.0));

    gl_FragColor = vec4(skyColor, 1.0);
}
