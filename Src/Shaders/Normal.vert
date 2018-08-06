#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;
} ubo;

layout(location = 0) in vec3 fragPos;
layout(location = 1) in vec2 inText;
layout(location = 2) in vec3 inNorm;
layout(location = 3) in float albedo;

layout(location = 0) out vec3 Norms;

out gl_PerVertex {
    vec4 gl_Position;
};


void main() {
        gl_Position = ubo.proj * ubo.view * ubo.model * vec4(fragPos, 1.0);
        Norms = (ubo.proj * ubo.view * ubo.model * vec4(inNorm, 1.0)).xyz;
}
