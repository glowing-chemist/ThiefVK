#version 450
#extension GL_ARB_separate_shader_objects : enable

struct Light {
	vec4 mPosition;
	vec4 mDirection;
	vec4 mColourAndAngle;
};

layout(binding = 0) uniform UniformBufferObject {
    Light spotLights[10];
} ubo;

layout(binding = 1) uniform sampler2D colourTexture;
layout(binding = 2) uniform sampler2D depthTexture;
layout(binding = 3) uniform sampler2D normalstexture;
layout(binding = 4) uniform sampler2D aledoTexture;

layout (push_constant) uniform pushConstants {
	vec4 LightAndInvView[5];
} push_constants;

layout(location = 0) out vec4 frameBuffer;
layout(location = 1) in vec2 texCoords;

vec4 calculatePosition() {
	float depth = 1.0f - texture(depthTexture, texCoords).x;
	if(depth == 0.0f) return vec4(0.0f);
	mat4 inverseView = mat4(push_constants.LightAndInvView[1], push_constants.LightAndInvView[2],
							push_constants.LightAndInvView[3], push_constants.LightAndInvView[4]);

	vec4 position = inverseView * vec4((texCoords.x * 2.0f) + 1, ((1.0f - texCoords.y) * 2.0f) + 1.0f, depth, 1.0f);

	return position / position.w;
}

void main() {
	frameBuffer = texture(colourTexture, texCoords);
	vec3 normals = vec4((texture(normalstexture, texCoords) * 2.0) - 1.0).xyz; // remap the normals to [-1, 1]
	vec4 fragPos = calculatePosition();

	for(int i = 0; i < push_constants.LightAndInvView[0].x; ++i) {
		vec3 lightVector = normalize(ubo.spotLights[i].mPosition.xyz - fragPos.xyz);
		float diffuseTerm = max(0.0, dot(normals, lightVector));
		frameBuffer += vec4(diffuseTerm + 0.1f);

		if(diffuseTerm > 0.0) {
			vec3 cameraPos = push_constants.LightAndInvView[4].xyz;
			vec3 halfVector = normalize(cameraPos + lightVector);
			float shinniness = texture(aledoTexture, texCoords).x * 100.0f;
			vec3 specularTerm = pow(dot(halfVector, normals), shinniness) * ubo.spotLights[i].mColourAndAngle.xyz;

			frameBuffer += vec4(specularTerm, 1.0f);
		}
	}  

}
