#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec2 texCoord;

layout(location = 0) out vec4 outColor;

uniform sampler2D texture1;


void main() {
    outColor = texture(texture1, texCoord);
}