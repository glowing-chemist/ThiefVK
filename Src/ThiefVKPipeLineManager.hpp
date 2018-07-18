#ifndef THIEFVKPIPELINEMANAGER_HPP
#define THIEFVKPIPELINEMANAGER_HPP

#include "ThiefVKDescriptorManager.hpp"

#include <map> // used for a runtime pipeline cache
#include <string>

#include <vulkan/vulkan.hpp>

class ThiefVKDevice;

enum class ShaderName {
    BasicTransformVertex,
    BasicColourFragment,
    DepthVertex,
    DepthFragment,
    NormalVertex,
    NormalFragment,
    CompositeVertex,
    CompositeFragment
};

struct ThiefVKPipelineDescription {
    ShaderName vertexShader;
    ShaderName fragmentShader;

    vk::RenderPass renderPass; // render pass the pipeline wil be used with

    uint32_t renderTargetWidth; // -1 represents sizeof swap chain
    uint32_t renderTargetHeight;
    int32_t renderTargetOffsetX;
    int32_t renderTargetOffsetY;
};

bool operator<(const ThiefVKPipelineDescription&, const ThiefVKPipelineDescription&);


class ThiefVKPipelineManager {
public:
    ThiefVKPipelineManager(ThiefVKDevice& dev);
    void Destroy();

    vk::Pipeline getPipeLine(ThiefVKPipelineDescription);
	vk::DescriptorSetLayout getDescriptorSetLayout(const ShaderName) const;
private:

    // reference to device for creating shader modules and destroying pipelines
    ThiefVKDevice& dev;
	vk::DescriptorPool DescPool;

    vk::ShaderModule createShaderModule(std::string path) const;
    vk::PipelineLayout createPipelineLayout(vk::DescriptorSetLayout& descLayouts, ShaderName shader) const;

    std::map<ShaderName, vk::ShaderModule> shaderModules;

    struct PipeLine {
        vk::Pipeline mPipeLine;
        vk::PipelineLayout mPipelineLayout;
    };
    std::map<ThiefVKPipelineDescription, PipeLine> pipeLineCache;
};

#endif
