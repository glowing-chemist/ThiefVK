#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in float Albedo;

layout(location = 0) out float outColor;


void main() {
    outColor = Albedo;
}
