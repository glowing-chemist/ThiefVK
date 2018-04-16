#ifndef THIEFVKPIPELINEMANAGER_HPP
#define THIEFVKPIPELINEMANAGER_HPP

#include <map> // used for a runtime pipeline cache
#include <string>

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

    vk::RenderPass renderPass; // render pass the pipeline wil be used with

    int renderTargetWidth; // -1 represents sizeof swap chain
    int renderTargetHeight;
    int renderTargetOffset;
};


class ThiefVKPipelineManager {
public:
    ThiefVKPipelineManager(vk::Device& dev);
    void Destroy();

    vk::Pipeline getPipeLine(ThiefVKPipelineDescription);
private:

    // reference to device for creating shader modules and destroying pipelines
    vk::Device& dev;
	vk::DescriptorPool DescPool;

    vk::ShaderModule createShaderModule(std::string path) const;
    vk::PipelineLayout createPipelineLayout(std::vector<vk::DescriptorSetLayout> descLayouts, ShaderName shader) const;
    std::vector<vk::DescriptorSetLayout> createDescriptorSetLayout(ShaderName shader) const;

    std::map<ShaderName, vk::ShaderModule> shaderModules;

    struct PipeLine {
        vk::Pipeline mPipeLine;
        vk::PipelineLayout mPipelineLayout;
        vk::DescriptorSetLayout mDescLayout;
    };
    std::map<ThiefVKPipelineDescription, PipeLine> pipeLineCache;
};

#endif
