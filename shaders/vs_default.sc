$input a_position, a_normal, a_tangent, a_texcoord0, a_texcoord1, a_indices, a_weight, i_data0, i_data1, i_data2, i_data3
$output v_worldPos, v_normal, v_tangent, v_bitangent, v_texcoord0, v_texcoord1

#include <bgfx_shader.sh>

// Skinning uniforms
uniform mat4 u_BoneMatrices[128];
uniform vec4 u_IsSkinned;

void main()
{
	vec3 localPos = a_position;
	vec3 localNormal = a_normal;
	vec3 localTangent = a_tangent.xyz;

	bool isSkinned = u_IsSkinned.x > 0.5;

	if (isSkinned)
	{
		float4 skinnedPos = float4(0.0, 0.0, 0.0, 0.0);
		float3 skinnedNormal = float3(0.0, 0.0, 0.0);
		float3 skinnedTangent = float3(0.0, 0.0, 0.0);

		// Bone 0
		float4x4 bone0 = u_BoneMatrices[int(a_indices.x)];
		float w0 = a_weight.x;
		skinnedPos += mul(bone0, float4(localPos, 1.0)) * w0;
		skinnedNormal += mul(bone0, float4(localNormal, 0.0)).xyz * w0;
		skinnedTangent += mul(bone0, float4(localTangent, 0.0)).xyz * w0;

		// Bone 1
		float4x4 bone1 = u_BoneMatrices[int(a_indices.y)];
		float w1 = a_weight.y;
		skinnedPos += mul(bone1, float4(localPos, 1.0)) * w1;
		skinnedNormal += mul(bone1, float4(localNormal, 0.0)).xyz * w1;
		skinnedTangent += mul(bone1, float4(localTangent, 0.0)).xyz * w1;

		// Bone 2
		float4x4 bone2 = u_BoneMatrices[int(a_indices.z)];
		float w2 = a_weight.z;
		skinnedPos += mul(bone2, float4(localPos, 1.0)) * w2;
		skinnedNormal += mul(bone2, float4(localNormal, 0.0)).xyz * w2;
		skinnedTangent += mul(bone2, float4(localTangent, 0.0)).xyz * w2;

		// Bone 3
		float4x4 bone3 = u_BoneMatrices[int(a_indices.w)];
		float w3 = a_weight.w;
		skinnedPos += mul(bone3, float4(localPos, 1.0)) * w3;
		skinnedNormal += mul(bone3, float4(localNormal, 0.0)).xyz * w3;
		skinnedTangent += mul(bone3, float4(localTangent, 0.0)).xyz * w3;

		// Assign back
		localPos = skinnedPos.xyz;
		localNormal = normalize(skinnedNormal);
		localTangent = normalize(skinnedTangent);
	}

    // Build model matrix from instance data
    mat4 model = mtxFromCols(i_data0, i_data1, i_data2, i_data3);
    
    // Transform to world space
    vec4 worldPos = mul(model, vec4(localPos, 1.0));
    gl_Position = mul(u_viewProj, worldPos);
    v_worldPos = worldPos.xyz;
    
    // Transform normal to world space
    mat3 normalMatrix = mat3(model[0].xyz, model[1].xyz, model[2].xyz);
	//mat3 normalMatrix = transpose(inverse(mat3(model[0].xyz, model[1].xyz, model[2].xyz))); // Todo: This might be correct, but inverse() needs to be implemented
    v_normal = normalize(mul(normalMatrix, localNormal));
    v_tangent = normalize(mul(normalMatrix, localTangent));
    v_bitangent = cross(v_normal, v_tangent) * a_tangent.w;
    
    v_texcoord0 = a_texcoord0;
    v_texcoord1 = a_texcoord1;
}