#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 Norm;

layout(location = 0) out ivec4 outColor;


void main() {
    outColor = ivec4(Norm, 0);
}
