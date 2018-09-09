#version 450
#extension GL_ARB_separate_shader_objects : enable

struct Light {
	vec4 mPosition;
	vec4 mDirection;
	vec4 mColourAndAngle;
};

layout(binding = 0) uniform UniformBufferObject {
    Light spotLights[10];
} ubo;

layout(binding = 1) uniform sampler2D colourTexture;
layout(binding = 2) uniform sampler2D depthTexture;
layout(binding = 3) uniform sampler2D normalstexture;
layout(binding = 4) uniform sampler2D albedoTexture;

layout (push_constant) uniform pushConstants {
	vec4 LightAndInvView[5];
} push_constants;

layout(location = 0) out vec4 frameBuffer;
layout(location = 1) in vec2 texCoords;


void main()
{             
    // retrieve data from G-buffer
    vec3 FragPos = (texture(albedoTexture, texCoords).xyz * 2.0f) - 1.0f;
    vec3 Normal = (texture(normalstexture, texCoords).xyz * 2.0f) - 1.0f;
    vec3 Albedo = vec3(texture(albedoTexture, texCoords).w);
    
    // then calculate lighting as usual
    vec3 lighting = Albedo * 0.1; // hard-coded ambient component
    vec3 viewDir = normalize(push_constants.LightAndInvView[4].xyz - FragPos);
    for(int i = 0; i < push_constants.LightAndInvView[0].x; ++i)
    {
        // diffuse
        vec3 lightDir = normalize(ubo.spotLights[i].mPosition.xyz  - FragPos);
        vec3 diffuse = max(dot(Normal, lightDir), 0.0) * Albedo * ubo.spotLights[i].mColourAndAngle.xyz;
        lighting += diffuse;
    }
    
    frameBuffer = vec4(lighting, 1.0);
} 