$input v_texcoord0

#include "bgfx_shader.sh"

SAMPLER2D(s_sceneTex, 0);
uniform vec4 u_params;        // x=strength(D), y=half_tan_fov, z=fill_zoom, w=enabled
uniform vec4 u_viewportSize;  // x=width, y=height, z=1/width, w=1/height
uniform vec4 u_paniniExtra;   // x=vertical_comp(S), y=aspect, z/w=reserved

// Panini inverse: screen xy -> view ray
vec3 paniniInverse(vec2 projected_xy, float d, float s) {
    float dp1 = d + 1.0;
    float k = (projected_xy.x * projected_xy.x) / (dp1 * dp1);
    float disc = k * k * d * d - (k + 1.0) * (k * d * d - 1.0);
    if (disc <= 0.0) {
        return normalize(vec3(projected_xy, 1.0));
    }
    float c_lon = (-k * d + sqrt(disc)) / (k + 1.0);
    float S = dp1 / (d + c_lon);
    vec2 ang = vec2(atan2(projected_xy.y, S), atan2(projected_xy.x, S * c_lon));
    float cos_lat = cos(ang.x);
    vec3 ray = vec3(cos_lat * sin(ang.y), sin(ang.x), cos_lat * cos(ang.y));
    if (ray.z > 0.0) {
        float q = ray.x / ray.z;
        q = (q * q) / ((d + 1.0) * (d + 1.0));
        ray.y *= mix(1.0, 1.0 / sqrt(1.0 + q), s);
    }
    return ray;
}

void main() {
    float D = u_params.x;
    float halfTan = u_params.y;
    float zoom = u_params.z;
    float enabled = u_params.w;

    if (enabled < 0.5 || D < 0.001) {
        gl_FragColor = texture2D(s_sceneTex, v_texcoord0);
        return;
    }

    float aspect = u_paniniExtra.y;
    float S = u_paniniExtra.x;
    vec2 maxRectXY = vec2(halfTan * aspect, halfTan);

    vec2 screenXY = (v_texcoord0 * 2.0 - 1.0) / max(zoom, 0.001);
    vec2 projectedXY = screenXY * maxRectXY;

    vec3 ray = paniniInverse(projectedXY, D, S);

    if (ray.z > 0.0) {
        vec2 sourceUV = (ray.xy / ray.z) / maxRectXY * 0.5 + 0.5;

        if (all(greaterThanEqual(sourceUV, vec2(0.0, 0.0))) &&
            all(lessThanEqual(sourceUV, vec2(1.0, 1.0)))) {
            vec2 dx = dFdx(sourceUV);
            vec2 dy = dFdy(sourceUV);
            gl_FragColor = texture2DGrad(s_sceneTex, sourceUV, dx, dy);
        } else {
            gl_FragColor = vec4(0.0, 1.0, 0.0, 1.0);
        }
    } else {
        gl_FragColor = vec4(1.0, 1.0, 0.0, 1.0);
    }
}
