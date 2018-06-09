#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

layout(location = 0) in vec3 fragPos;
layout(location = 2) in vec3 normal;
layout(location = 1) in vec2 inText;

layout(location = 0) out float depth;

out gl_PerVertex {
    vec4 gl_Position;
};


void main() {
        vec4 cameraPosition = ubo.view * ubo.model * vec4(fragPos, 1.0);
        depth = -cameraPosition.y; // get the depth from camera space
        gl_Position = ubo.proj * cameraPosition;
}
