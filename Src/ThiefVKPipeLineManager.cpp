#include "ThiefVKPipeLineManager.hpp"
#include "ThiefVKVertex.hpp"

#include <string>
#include <fstream>
#include <vector>
#include <limits>
#include <array>


ThiefVKPipelineManager::ThiefVKPipelineManager(vk::Device& dev)
    :dev{dev} {

    // crerate the descriptor set pools for uniform buffers and combined image samplers
    vk::DescriptorPoolSize uniformBufferDescPoolSize{};
    uniformBufferDescPoolSize.setType(vk::DescriptorType::eUniformBuffer);
    uniformBufferDescPoolSize.setDescriptorCount(15); // start with 5 we can allways allocate another pool if we later need more.

	vk::DescriptorPoolSize imageSamplerrDescPoolSize{};
	imageSamplerrDescPoolSize.setType(vk::DescriptorType::eCombinedImageSampler);
	imageSamplerrDescPoolSize.setDescriptorCount(15); // start with 5 we can allways allocate another pool if we later need more.

	std::array<vk::DescriptorPoolSize, 2> descPoolSizes{uniformBufferDescPoolSize, imageSamplerrDescPoolSize};

    vk::DescriptorPoolCreateInfo uniformBufferDescPoolInfo{};
    uniformBufferDescPoolInfo.setPoolSizeCount(descPoolSizes.size()); // two pools one for uniform buffers and one for combined image samplers
    uniformBufferDescPoolInfo.setPPoolSizes(descPoolSizes.data());
    uniformBufferDescPoolInfo.setMaxSets(30);

    DescPool = dev.createDescriptorPool(uniformBufferDescPoolInfo);


    // Load up the shader spir-v from disk and create shader modules.
    shaderModules[ShaderName::BasicTransformVertex] = createShaderModule("./Shaders/BasicTransVert.spv");
    shaderModules[ShaderName::BasicColourFragment]  = createShaderModule("./Shaders/BasicFragment.spv");
    shaderModules[ShaderName::DepthVertex]        = createShaderModule("./Shaders/DepthVertex.spv");
    shaderModules[ShaderName::DepthFragment]        = createShaderModule("./Shaders/DepthFragment.spv");
    shaderModules[ShaderName::NormalVertex]        = createShaderModule("./Shaders/NormalVertex.spv");
    shaderModules[ShaderName::NormalFragment]        = createShaderModule("./Shaders/NormalFragment.spv");
    shaderModules[ShaderName::CompositeVertex]      = createShaderModule("./shaders/CompositeVertex.spv");
    shaderModules[ShaderName::CompositeFragment]    = createShaderModule("./shaders/CompositeFragment.spv");
}


void ThiefVKPipelineManager::Destroy() {
    for(auto& shader : shaderModules) {
        dev.destroyShaderModule(shader.second);
    }
    for(auto& pipeLine : pipeLineCache) {
        dev.destroyPipelineLayout(pipeLine.second.mPipelineLayout);
        dev.destroyDescriptorSetLayout(pipeLine.second.mDescLayout);
        dev.destroyPipeline(pipeLine.second.mPipeLine);
    }
    dev.destroyDescriptorPool(DescPool);
}


vk::Pipeline ThiefVKPipelineManager::getPipeLine(ThiefVKPipelineDescription description) {

    if(pipeLineCache[description].mPipeLine != vk::Pipeline{nullptr}) return pipeLineCache[description].mPipeLine;

    vk::PipelineShaderStageCreateInfo vertexStage{};
    vertexStage.setStage(vk::ShaderStageFlagBits::eVertex);
    vertexStage.setPName("main"); //entry point of the shader
    vertexStage.setModule(shaderModules[description.vertexShader]);

    vk::PipelineShaderStageCreateInfo fragStage{};
    fragStage.setStage(vk::ShaderStageFlagBits::eFragment);
    fragStage.setPName("main");
    fragStage.setModule(shaderModules[description.fragmentShader]);

    vk::PipelineShaderStageCreateInfo shaderStages[2] = {vertexStage, fragStage};

    auto bindingDesc = Vertex::getBindingDesc();
    auto attribDesc  = Vertex::getAttribDesc();

    vk::PipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.setVertexAttributeDescriptionCount(attribDesc.size());
    vertexInputInfo.setPVertexAttributeDescriptions(attribDesc.data());
    vertexInputInfo.setVertexBindingDescriptionCount(1);
    vertexInputInfo.setPVertexBindingDescriptions(&bindingDesc);

    vk::PipelineInputAssemblyStateCreateInfo inputAssemblyInfo{};
    inputAssemblyInfo.setTopology(vk::PrimitiveTopology::eTriangleList);
    inputAssemblyInfo.setPrimitiveRestartEnable(false);

    vk::Extent2D extent{description.renderTargetHeight, description.renderTargetWidth};

    vk::Viewport viewPort{0.0f, 0.0f, static_cast<float>(extent.width), static_cast<float>(extent.height), 0.0f, 1.0f}; // render to all of the framebuffer

    vk::Offset2D offset{description.renderTargetOffsetX, description.renderTargetOffsetY};

    vk::Rect2D scissor{offset, extent};

    vk::PipelineViewportStateCreateInfo viewPortInfo{};
    viewPortInfo.setPScissors(&scissor);
    viewPortInfo.setPViewports(&viewPort);
    viewPortInfo.setScissorCount(1);
    viewPortInfo.setViewportCount(1);

    vk::PipelineRasterizationStateCreateInfo rastInfo{};
    rastInfo.setRasterizerDiscardEnable(false);
    rastInfo.setDepthBiasClamp(false);
    rastInfo.setPolygonMode(vk::PolygonMode::eFill); // output filled in fragments
    rastInfo.setLineWidth(1.0f);
    rastInfo.setCullMode(vk::CullModeFlagBits::eBack); // cull fragments from the back
    rastInfo.setFrontFace(vk::FrontFace::eCounterClockwise);
    rastInfo.setDepthBiasEnable(false);

    vk::PipelineMultisampleStateCreateInfo multiSampInfo{};
    multiSampInfo.setSampleShadingEnable(false);
    multiSampInfo.setRasterizationSamples(vk::SampleCountFlagBits::e1);

    vk::PipelineColorBlendAttachmentState colorAttachState{};
    colorAttachState.setColorWriteMask(vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |  vk::ColorComponentFlagBits::eB |  vk::ColorComponentFlagBits::eA); // write to all color components
    colorAttachState.setBlendEnable(false);

    vk::PipelineColorBlendStateCreateInfo blendStateInfo{};
    blendStateInfo.setLogicOpEnable(false);
    blendStateInfo.setAttachmentCount(1);
    blendStateInfo.setPAttachments(&colorAttachState);

    vk::DescriptorSetLayout descSetLayouts = createDescriptorSetLayout(description.fragmentShader);
    vk::PipelineLayout pipelineLayout = createPipelineLayout(descSetLayouts, description.vertexShader);

    const uint32_t subpassIndex = [&description]() {
        switch (description.vertexShader) {
            case ShaderName::BasicTransformVertex:
                return 0u;
            case ShaderName::DepthVertex:
                return 1u;
            case ShaderName::NormalVertex:
                return  2u;
            case ShaderName::CompositeVertex:
                return 3u;
            default:
                return std::numeric_limits<uint32_t>::max();
        }
    }();

    vk::GraphicsPipelineCreateInfo pipeLineCreateInfo{};
    pipeLineCreateInfo.setStageCount(2); // vertex and fragment
    pipeLineCreateInfo.setPStages(shaderStages);

    if(description.fragmentShader != ShaderName::CompositeFragment) { // The composite pass uses a hardcoded full screen triangle so doesn't need a vertex buffer.
        pipeLineCreateInfo.setPVertexInputState(&vertexInputInfo);
        pipeLineCreateInfo.setPInputAssemblyState(&inputAssemblyInfo);
    }

    pipeLineCreateInfo.setPViewportState(&viewPortInfo);
    pipeLineCreateInfo.setPRasterizationState(&rastInfo);
    pipeLineCreateInfo.setPMultisampleState(&multiSampInfo);
    pipeLineCreateInfo.setPColorBlendState(&blendStateInfo);

    pipeLineCreateInfo.setLayout(pipelineLayout);
    pipeLineCreateInfo.setRenderPass(description.renderPass);
    pipeLineCreateInfo.setSubpass(subpassIndex); // index for subpass that this pipeline will be used in

    if(description.vertexShader == ShaderName::DepthVertex) { // we need to specify ou rdepth stencil state
        vk::PipelineDepthStencilStateCreateInfo depthStencilInfo{};
        depthStencilInfo.setDepthTestEnable(true);
        depthStencilInfo.setDepthWriteEnable(true);
        depthStencilInfo.setDepthCompareOp(vk::CompareOp::eLess);
        depthStencilInfo.setDepthBoundsTestEnable(false);

        pipeLineCreateInfo.setPDepthStencilState(&depthStencilInfo);
    }

    vk::Pipeline pipeline = dev.createGraphicsPipeline(nullptr, pipeLineCreateInfo);

    PipeLine piplineInfo{pipeline, pipelineLayout, descSetLayouts};
    pipeLineCache[description] = piplineInfo;

    return pipeline;
}


vk::DescriptorSetLayout ThiefVKPipelineManager::getPipelineLayout(ThiefVKPipelineDescription description) {
    if(pipeLineCache[description].mPipeLine != vk::Pipeline{nullptr}) return pipeLineCache[description].mDescLayout;

    return nullptr;
}


vk::ShaderModule ThiefVKPipelineManager::createShaderModule(std::string path) const {
    std::ifstream file{path, std::ios::binary};

    auto shaderSource = std::vector<char>(std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{});

    vk::ShaderModuleCreateInfo info{};
    info.setCodeSize(shaderSource.size());
    info.setPCode(reinterpret_cast<const uint32_t*>(shaderSource.data()));

    return dev.createShaderModule(info);
}


vk::PipelineLayout ThiefVKPipelineManager::createPipelineLayout(vk::DescriptorSetLayout& descLayouts, ShaderName)  const {
    vk::PipelineLayoutCreateInfo pipelinelayoutinfo{};
    pipelinelayoutinfo.setPSetLayouts(&descLayouts);
    pipelinelayoutinfo.setSetLayoutCount(1);

    return dev.createPipelineLayout(pipelinelayoutinfo);
}


vk::DescriptorSetLayout ThiefVKPipelineManager::createDescriptorSetLayout(ShaderName shader) const {
    std::vector<vk::DescriptorSetLayoutBinding> descSets{};

    vk::DescriptorSetLayoutBinding uboDescriptorLayout{};
    uboDescriptorLayout.setDescriptorCount(1);
    uboDescriptorLayout.setBinding(0);
    uboDescriptorLayout.setDescriptorType(vk::DescriptorType::eUniformBuffer);
    uboDescriptorLayout.setStageFlags(vk::ShaderStageFlagBits::eVertex); 

    descSets.push_back(uboDescriptorLayout);

    if(shader == ShaderName::BasicColourFragment) {
        vk::DescriptorSetLayoutBinding imageSamplerDescriptorLayout{};
        imageSamplerDescriptorLayout.setDescriptorCount(1);
        imageSamplerDescriptorLayout.setBinding(1);
        imageSamplerDescriptorLayout.setDescriptorType(vk::DescriptorType::eCombinedImageSampler);
        imageSamplerDescriptorLayout.setStageFlags(vk::ShaderStageFlagBits::eFragment);

        descSets.push_back(imageSamplerDescriptorLayout);
    } else if(shader == ShaderName::CompositeFragment) {
        for(unsigned int i = 1; i < 4; ++i) {
            vk::DescriptorSetLayoutBinding imageSamplerDescriptorLayout{};
            imageSamplerDescriptorLayout.setDescriptorCount(1);
            imageSamplerDescriptorLayout.setBinding(i);
            imageSamplerDescriptorLayout.setDescriptorType(vk::DescriptorType::eCombinedImageSampler);
            imageSamplerDescriptorLayout.setStageFlags(vk::ShaderStageFlagBits::eFragment); 

            descSets.push_back(imageSamplerDescriptorLayout);
        }

    }

    vk::DescriptorSetLayoutCreateInfo LayoutInfo{};
    LayoutInfo.setBindingCount(descSets.size());
    LayoutInfo.setPBindings(descSets.data());

    return dev.createDescriptorSetLayout(LayoutInfo);
}


bool operator<(const ThiefVKPipelineDescription& lhs, const ThiefVKPipelineDescription& rhs) {
    return static_cast<int>(lhs.fragmentShader) < static_cast<int>(rhs.fragmentShader);
}
