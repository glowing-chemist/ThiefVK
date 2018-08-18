#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in flat vec3 Norm;

layout(location = 0) out vec4 outColor;


void main() {
	// Map from [-1, 1] to [0, 1] so we don't lose precision
	vec3 mappedNormals = (normalize(Norm) + 1.0) / 2.0; 
	outColor = vec4(mappedNormals, 1);
}
