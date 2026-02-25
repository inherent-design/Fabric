$input v_normal, v_texcoord0

#include "bgfx_shader.sh"

void main()
{
	// Basic directional light (sun direction pointing down-left-forward)
	vec3 lightDir = normalize(vec3(0.5, 1.0, 0.3));
	vec3 normal = normalize(v_normal);

	float ndotl = max(dot(normal, lightDir), 0.0);
	float ambient = 0.2;
	float diffuse = ndotl * 0.8;

	vec3 color = vec3(0.8, 0.8, 0.8) * (ambient + diffuse);
	gl_FragColor = vec4(color, 1.0);
}
