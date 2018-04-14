#ifndef THIEFVKPIPELINEMANAGER_HPP
#define THIEFVKPIPELINEMANAGER_HPP

#include <map> // used for a runtime pipeline cache

#include <vulkan/vulkan.hpp>

enum class ShaderName {
    BasicTransformVertex,
    BasicColourFragment,
    DepthVertex,
    DepthFragment,
    LightVertex,
    LightFragment,
    NormalVertex,
    NormalFragment
};

struct ThiefVKPipelineDescription {
    ShaderName vertexShader;
    ShaderName fragmentShader;

    int renderTargetWidth; // -1 represents sizeof swap chain
    int renderTargetHeight;
    int renderTargetOffset;
};

class ThiefVKPipelineManager {
public:
    ThiefVKPipelineManager();
    void Destroy(vk::Device);

    vk::Pipeline getPipeLine(ThiefVKPipelineDescription);
private:

    std::map<ThiefVKPipelineDescription, vk::Pipeline> pipeLineCache;
};

#endif
