#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 4) in vec3 lineColour;

layout(location = 0) out vec4 outColor;


void main() {
	// Map from [-1, 1] to [0, 1] so we don't lose precision
	outColor = vec4(lineColour, 1.0f);
}
