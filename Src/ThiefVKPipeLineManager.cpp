#include "ThiefVKPipeLineManager.hpp"
#include "ThiefVKVertex.hpp"
#include "ThiefVKDescriptorManager.hpp"
#include "ThiefVKDevice.hpp"

#include <string>
#include <fstream>
#include <vector>
#include <limits>
#include <array>


ThiefVKPipelineManager::ThiefVKPipelineManager(ThiefVKDevice& dev)
    :dev{dev} {

    // Load up the shader spir-v from disk and create shader modules.
    shaderModules[ShaderName::BasicTransformVertex] = createShaderModule("./Shaders/BasicTransform.vert.spv");
    shaderModules[ShaderName::BasicColourFragment]  = createShaderModule("./Shaders/Colour.frag.spv");
    shaderModules[ShaderName::AlbedoVertex]        = createShaderModule("./Shaders/Albedo.vert.spv");
    shaderModules[ShaderName::AlbedoFragment]        = createShaderModule("./Shaders/Albedo.frag.spv");
    shaderModules[ShaderName::NormalVertex]        = createShaderModule("./Shaders/Normal.vert.spv");
    shaderModules[ShaderName::NormalFragment]        = createShaderModule("./Shaders/Normal.frag.spv");
    shaderModules[ShaderName::CompositeVertex]      = createShaderModule("./Shaders/Composite.vert.spv");
    shaderModules[ShaderName::CompositeFragment]    = createShaderModule("./Shaders/Composite.frag.spv");
}


void ThiefVKPipelineManager::Destroy() {
    for(auto& shader : shaderModules) {
        dev.getLogicalDevice()->destroyShaderModule(shader.second);
    }
    for(auto& pipeLine : pipeLineCache) {
		dev.getLogicalDevice()->destroyPipelineLayout(pipeLine.second.mPipelineLayout);
		dev.getLogicalDevice()->destroyPipeline(pipeLine.second.mPipeLine);
    }
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

    vk::PipelineVertexInputStateCreateInfo compositeVertexInputInfo{};
    compositeVertexInputInfo.setVertexAttributeDescriptionCount(0);
    compositeVertexInputInfo.setVertexBindingDescriptionCount(0);

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
    if(description.useBackFaceCulling){
        rastInfo.setCullMode(vk::CullModeFlagBits::eBack); // cull fragments from the back
        rastInfo.setFrontFace(vk::FrontFace::eClockwise);
    } else {
        rastInfo.setCullMode(vk::CullModeFlagBits::eNone);
    }
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

	vk::DescriptorSetLayout descSetLayouts = getDescriptorSetLayout(description.fragmentShader);
    vk::PipelineLayout pipelineLayout = createPipelineLayout(descSetLayouts, description.fragmentShader);

    const uint32_t subpassIndex = [&description]() {
        switch (description.vertexShader) {
            case ShaderName::BasicTransformVertex:
                return 0u;
            case ShaderName::NormalVertex:
                return  1u;
            case ShaderName::AlbedoVertex:
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
    } else {
        pipeLineCreateInfo.setPVertexInputState(&compositeVertexInputInfo);
    }
    pipeLineCreateInfo.setPInputAssemblyState(&inputAssemblyInfo);

    pipeLineCreateInfo.setPViewportState(&viewPortInfo);
    pipeLineCreateInfo.setPRasterizationState(&rastInfo);
    pipeLineCreateInfo.setPMultisampleState(&multiSampInfo);
    pipeLineCreateInfo.setPColorBlendState(&blendStateInfo);

    pipeLineCreateInfo.setLayout(pipelineLayout);
    pipeLineCreateInfo.setRenderPass(description.renderPass);
    pipeLineCreateInfo.setSubpass(subpassIndex); // index for subpass that this pipeline will be used in

    vk::PipelineDepthStencilStateCreateInfo depthStencilInfo{};
    depthStencilInfo.setDepthTestEnable(description.useDepthTest);
    depthStencilInfo.setDepthWriteEnable(true);
    depthStencilInfo.setDepthCompareOp(vk::CompareOp::eLessOrEqual);
    depthStencilInfo.setDepthBoundsTestEnable(false);
    pipeLineCreateInfo.setPDepthStencilState(&depthStencilInfo);

    vk::Pipeline pipeline = dev.getLogicalDevice()->createGraphicsPipeline(nullptr, pipeLineCreateInfo);

    PipeLine piplineInfo{pipeline, pipelineLayout};
    pipeLineCache[description] = piplineInfo;

    return pipeline;
}


vk::ShaderModule ThiefVKPipelineManager::createShaderModule(std::string path) const {
    std::ifstream file{path, std::ios::binary};

    auto shaderSource = std::vector<char>(std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{});

    vk::ShaderModuleCreateInfo info{};
    info.setCodeSize(shaderSource.size());
    info.setPCode(reinterpret_cast<const uint32_t*>(shaderSource.data()));

    return dev.getLogicalDevice()->createShaderModule(info);
}


vk::PipelineLayout ThiefVKPipelineManager::createPipelineLayout(vk::DescriptorSetLayout& descLayouts, ShaderName name)  const {
    vk::PipelineLayoutCreateInfo pipelinelayoutinfo{};
    pipelinelayoutinfo.setPSetLayouts(&descLayouts);
    pipelinelayoutinfo.setSetLayoutCount(1);

    vk::PushConstantRange range{};
    if(name == ShaderName::CompositeFragment) {
        range.setStageFlags(vk::ShaderStageFlagBits::eFragment);
        range.setOffset(0);
        range.setSize(sizeof(uint32_t));

        pipelinelayoutinfo.setPushConstantRangeCount(1);
        pipelinelayoutinfo.setPPushConstantRanges(&range);
    }

    return dev.getLogicalDevice()->createPipelineLayout(pipelinelayoutinfo);
}


vk::DescriptorSetLayout ThiefVKPipelineManager::getDescriptorSetLayout(const ShaderName shader) const {
	const ThiefVKDescriptorSetDescription descSetDesc = dev.getDescriptorSetDescription(shader);

	return dev.getDescriptorManager()->getDescriptorSetLayout(descSetDesc);
}


// This is a bit hacky, will fix at some point
vk::PipelineLayout ThiefVKPipelineManager::getPipelineLayout(const ShaderName shader) const {
    for(const auto& [key, pipeline] : pipeLineCache) {
        if( key.fragmentShader == shader) return pipeline.mPipelineLayout;
    }

    return vk::PipelineLayout{nullptr};
}


bool operator<(const ThiefVKPipelineDescription& lhs, const ThiefVKPipelineDescription& rhs) {
    return static_cast<int>(lhs.fragmentShader) < static_cast<int>(rhs.fragmentShader);
}
