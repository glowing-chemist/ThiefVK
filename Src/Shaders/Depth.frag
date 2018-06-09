#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in float inDepth;


void main() {
    gl_FragDepth = inDepth;
}
