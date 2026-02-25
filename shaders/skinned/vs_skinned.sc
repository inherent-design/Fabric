$input a_position, a_normal, a_texcoord0, a_indices, a_weight
$output v_normal, v_texcoord0

#include "bgfx_shader.sh"

uniform mat4 u_jointMatrices[100];

void main()
{
	// GPU skinning: weighted sum of joint transforms
	mat4 skinMatrix =
		a_weight.x * u_jointMatrices[a_indices.x] +
		a_weight.y * u_jointMatrices[a_indices.y] +
		a_weight.z * u_jointMatrices[a_indices.z] +
		a_weight.w * u_jointMatrices[a_indices.w];

	vec4 skinnedPos = skinMatrix * vec4(a_position, 1.0);
	vec3 skinnedNormal = normalize(mat3(skinMatrix) * a_normal);

	gl_Position = mul(u_modelViewProj, skinnedPos);
	v_normal = skinnedNormal;
	v_texcoord0 = a_texcoord0;
}
