#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in float inDepth;

layout(location = 1) out float outDepth;

void main() {
    outDepth = inDepth;
}
