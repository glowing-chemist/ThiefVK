#ifndef THIEFVKPIPELINEMANAGER_HPP
#define THIEFVKPIPELINEMANAGER_HPP

#include "ThiefVKDescriptorManager.hpp"

#include <map> // used for a runtime pipeline cache
#include <string>

#include <vulkan/vulkan.hpp>

class ThiefVKDevice;

struct ThiefVKPipelineDescription {
    std::string vertexShaderName;
    std::string geometryShaderName;
    std::string fragmentShaderName;

    vk::RenderPass renderPass; // render pass the pipeline wil be used with
    uint32_t subpassIndex;

    uint32_t renderTargetWidth; // -1 represents sizeof swap chain
    uint32_t renderTargetHeight;
    int32_t renderTargetOffsetX;
    int32_t renderTargetOffsetY;
    bool    useDepthTest;
    bool    useBackFaceCulling;
};

bool operator<(const ThiefVKPipelineDescription&, const ThiefVKPipelineDescription&);


class ThiefVKPipelineManager {
public:
    ThiefVKPipelineManager(ThiefVKDevice& dev);
    void Destroy();

    vk::Pipeline getPipeLine(ThiefVKPipelineDescription);
	vk::DescriptorSetLayout getDescriptorSetLayout(const std::string&) const;
    vk::PipelineLayout getPipelineLayout(const std::string&) const;
private:

    // reference to device for creating shader modules and destroying pipelines
    ThiefVKDevice& dev;
	vk::DescriptorPool DescPool;

    vk::ShaderModule createShaderModule(std::string& path) const;
    vk::PipelineLayout createPipelineLayout(vk::DescriptorSetLayout& descLayouts, std::string& shader) const;

    std::map<std::string, vk::ShaderModule> shaderModules;

    struct PipeLine {
        vk::Pipeline mPipeLine;
        vk::PipelineLayout mPipelineLayout;
    };
    std::map<ThiefVKPipelineDescription, PipeLine> pipeLineCache;
};

#endif
