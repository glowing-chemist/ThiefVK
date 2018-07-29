#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in flat vec3 Norm;

layout(location = 0) out vec4 outColor;


void main() {
    outColor = ivec4(Norm, 1);
}
