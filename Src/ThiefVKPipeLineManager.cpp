#include "ThiefVKPipeLineManager.hpp"

#include <string>
#include <fstream>
#include <vector>
#include <array>


ThiefVKPipelineManager::ThiefVKPipelineManager(vk::Device dev)
    :dev{dev} {

    // crerate the descriptor set pools for uniform buffers and combined image samplers
    vk::DescriptorPoolSize uniformBufferDescPoolSize{};
    uniformBufferDescPoolSize.setType(vk::DescriptorType::eUniformBuffer);
    uniformBufferDescPoolSize.setDescriptorCount(5); // start with 5 we can allways allocate another pool if we later need more.

	vk::DescriptorPoolSize imageSamplerrDescPoolSize{};
	imageSamplerrDescPoolSize.setType(vk::DescriptorType::eCombinedImageSampler);
	imageSamplerrDescPoolSize.setDescriptorCount(5); // start with 5 we can allways allocate another pool if we later need more.

	std::array<vk::DescriptorPoolSize, 2> descPoolSizes{uniformBufferDescPoolSize, imageSamplerrDescPoolSize};

    vk::DescriptorPoolCreateInfo uniformBufferDescPoolInfo{};
    uniformBufferDescPoolInfo.setPoolSizeCount(descPoolSizes.size()); // two pools one for uniform buffers and one for combined image samplers
    uniformBufferDescPoolInfo.setPPoolSizes(descPoolSizes.data());
    uniformBufferDescPoolInfo.setMaxSets(100);

    DescPool = dev.createDescriptorPool(uniformBufferDescPoolInfo);


    // Load up the shader spir-v from disk and create shader modules.
    shaderModules[ShaderName::BasicTransformVertex] = createShaderModule("./Shaders/BasicTransVert.spv");
    shaderModules[ShaderName::BasicColourFragment]  = createShaderModule("./Shaders/BasicFragment.spv");
    shaderModules[ShaderName::DepthVertex]        = createShaderModule("./Shaders/DepthVertex.spv");
    shaderModules[ShaderName::DepthFragment]        = createShaderModule("./Shaders/DepthFragment.spv");
    shaderModules[ShaderName::LightVertex]        = createShaderModule("./Shaders/LightVertex.spv");
    shaderModules[ShaderName::LightFragment]        = createShaderModule("./Shaders/LightFragment.spv");
    shaderModules[ShaderName::NormalVertex]        = createShaderModule("./Shaders/NormalVertex.spv");
    shaderModules[ShaderName::NormalFragment]        = createShaderModule("./Shaders/NormalFragment.spv");
}


void ThiefVKPipelineManager::Destroy() {
    for(auto& shader : shaderModules) {
        dev.destroyShaderModule(shader.second);
    }
    for(auto& pipeLine : pipeLineCache) {
        dev.destroyPipelineLayout(pipeLine.second.mPipelineLayout);
        dev.destroyPipeline(pipeLine.second.mPipeLine);
    }
    dev.destroyDescriptorPool(DescPool);
}


vk::Pipeline ThiefVKPipelineManager::getPipeLine(ThiefVKPipelineDescription description) {
	return VK_NULL_HANDLE;
}


vk::ShaderModule ThiefVKPipelineManager::createShaderModule(std::string path) const {
    std::ifstream file{path, std::ios::binary};

    auto shaderSource = std::vector<char>(std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{});

    vk::ShaderModuleCreateInfo info{};
    info.setCodeSize(shaderSource.size());
    info.setPCode(reinterpret_cast<const uint32_t*>(shaderSource.data()));

    return dev.createShaderModule(info);
}


vk::PipelineLayout ThiefVKPipelineManager::createPipelineLayout(std::vector<vk::DescriptorSetLayout> descLayouts, ShaderName)  const {
    vk::PipelineLayoutCreateInfo pipelinelayoutinfo{};
    pipelinelayoutinfo.setPSetLayouts(descLayouts.data());
    pipelinelayoutinfo.setSetLayoutCount(descLayouts.size());

    return dev.createPipelineLayout(pipelinelayoutinfo);
}


std::vector<vk::DescriptorSetLayout> ThiefVKPipelineManager::createDescriptorSetLayout(ShaderName shader) const {
    std::vector<vk::DescriptorSetLayout> descSets{};

    vk::DescriptorSetLayoutBinding uboDescriptorLayout{};
    uboDescriptorLayout.setDescriptorCount(1);
    uboDescriptorLayout.setBinding(0);
    uboDescriptorLayout.setDescriptorType(vk::DescriptorType::eUniformBuffer);
    uboDescriptorLayout.setStageFlags(vk::ShaderStageFlagBits::eVertex); // set what stage it will be used in

    vk::DescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.setBindingCount(1);
    layoutInfo.setPBindings(&uboDescriptorLayout);

    descSets.push_back(dev.createDescriptorSetLayout(layoutInfo));

    if(shader == ShaderName::BasicColourFragment) {
        vk::DescriptorSetLayoutBinding imageSamplerDescriptorLayout{};
        imageSamplerDescriptorLayout.setDescriptorCount(1);
        imageSamplerDescriptorLayout.setBinding(1);
        imageSamplerDescriptorLayout.setDescriptorType(vk::DescriptorType::eCombinedImageSampler);
        imageSamplerDescriptorLayout.setStageFlags(vk::ShaderStageFlagBits::eFragment); // set what stage it will be used in

        vk::DescriptorSetLayoutCreateInfo samplerLayoutInfo{};
        samplerLayoutInfo.setBindingCount(1);
        samplerLayoutInfo.setPBindings(&imageSamplerDescriptorLayout);

        descSets.push_back(dev.createDescriptorSetLayout(samplerLayoutInfo));
    }

    return descSets;
}
