#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 0) uniform UniformBufferObject {
    mat4 spotLights[10];
} ubo;

layout(binding = 1) uniform sampler2D colourTexture;
layout(binding = 2) uniform sampler2D depthTexture;
layout(binding = 3) uniform sampler2D normalstexture;
layout(binding = 4) uniform sampler2D aledoTexture;

layout (push_constant) uniform pushConstants {
	int numberOfLights;
} lights;

layout(location = 0) out vec4 frameBuffer;
layout(location = 1) in vec2 texCoords;

void main() {
	frameBuffer = texture(colourTexture, texCoords);
}
