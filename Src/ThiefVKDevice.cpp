#include "ThiefVKDevice.hpp"
#include "ThiefVKInstance.hpp"

#include <array>
#include <iostream>

// Vertex member functions
vk::VertexInputBindingDescription Vertex::getBindingDesc() {

    vk::VertexInputBindingDescription desc{};
    desc.setStride(sizeof(Vertex));
    desc.setBinding(0);
    desc.setInputRate(vk::VertexInputRate::eVertex);

    return desc;
}

std::array<vk::VertexInputAttributeDescription, 2> Vertex::getAttribDesc() {

    vk::VertexInputAttributeDescription atribDescPos{};
    atribDescPos.setBinding(0);
    atribDescPos.setLocation(0);
    atribDescPos.setFormat(vk::Format::eR32G32B32Sfloat);
    atribDescPos.setOffset(offsetof(Vertex, pos));

    vk::VertexInputAttributeDescription atribDescTex{};
    atribDescTex.setBinding(0);
    atribDescTex.setLocation(1);
    atribDescTex.setFormat(vk::Format::eR32G32Sfloat);
    atribDescTex.setOffset(offsetof(Vertex, tex));

    return {atribDescPos, atribDescTex};
}

// ThiefVKDeviceMemberFunctions

ThiefVKDevice::ThiefVKDevice(vk::PhysicalDevice physDev, vk::Device Dev, vk::SurfaceKHR surface, GLFWwindow * window) :
    mPhysDev{physDev}, mDevice{Dev}, mWindowSurface{surface}, mWindow{window}, mSwapChain{Dev, physDev, surface, window} {}


ThiefVKDevice::~ThiefVKDevice() {
    mSwapChain.destroy(mDevice);
    mDevice.destroyRenderPass(mRenderPasses.RenderPass);
    mDevice.destroy();
}


void ThiefVKDevice::createRenderPasses() {

    // Specify all attachments used in all subrenderpasses
    vk::AttachmentDescription colourPassAttachment{}; // also used for the shadow mapping
    colourPassAttachment.setFormat(vk::Format::eR8G8B8A8Srgb);
    colourPassAttachment.setLoadOp(vk::AttachmentLoadOp::eDontCare); // we are going to overwrite all pixles
    colourPassAttachment.setStoreOp(vk::AttachmentStoreOp::eDontCare);
    colourPassAttachment.setStencilLoadOp(vk::AttachmentLoadOp::eDontCare);
    colourPassAttachment.setStencilStoreOp(vk::AttachmentStoreOp::eDontCare);
    colourPassAttachment.setInitialLayout(vk::ImageLayout::eColorAttachmentOptimal);
    colourPassAttachment.setFinalLayout(vk::ImageLayout::eColorAttachmentOptimal);

    vk::AttachmentReference coloursubPassReference{};
    coloursubPassReference.setAttachment(0);
    coloursubPassReference.setLayout(vk::ImageLayout::eGeneral); // as these will be read and written to


    vk::AttachmentDescription depthPassAttachment{};
    depthPassAttachment.setFormat(vk::Format::eR32Sfloat); // store in each pixel a 32bit depth value
    depthPassAttachment.setStencilLoadOp(vk::AttachmentLoadOp::eDontCare);
    depthPassAttachment.setStencilStoreOp(vk::AttachmentStoreOp::eDontCare);
    depthPassAttachment.setStencilLoadOp(vk::AttachmentLoadOp::eDontCare);
    depthPassAttachment.setStencilStoreOp(vk::AttachmentStoreOp::eDontCare);
    depthPassAttachment.setInitialLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal); // wriet in a subpass then read in a subsequent one
    depthPassAttachment.setFinalLayout(vk::ImageLayout::eDepthStencilReadOnlyOptimal);

    vk::AttachmentReference depthsubPassReference{};
    depthsubPassReference.setAttachment(1);
    depthsubPassReference.setLayout(vk::ImageLayout::eGeneral);


    vk::AttachmentDescription normalsPassAttachment{};
    normalsPassAttachment.setFormat(vk::Format::eR8G8B8Snorm);
    normalsPassAttachment.setLoadOp(vk::AttachmentLoadOp::eDontCare);
    normalsPassAttachment.setStoreOp(vk::AttachmentStoreOp::eDontCare);
    normalsPassAttachment.setStencilLoadOp(vk::AttachmentLoadOp::eDontCare);
    normalsPassAttachment.setStencilStoreOp(vk::AttachmentStoreOp::eDontCare);
    normalsPassAttachment.setInitialLayout(vk::ImageLayout::eColorAttachmentOptimal);
    normalsPassAttachment.setFinalLayout(vk::ImageLayout::eColorAttachmentOptimal); // these will be used in the subsqeuent light renderpass

    vk::AttachmentReference normalssubPassReference{};
    normalssubPassReference.setAttachment(2);
    normalssubPassReference.setLayout(vk::ImageLayout::eGeneral);

    // specify the subpass descriptions
    vk::SubpassDescription colourPassDesc{};
    colourPassDesc.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics);
    colourPassDesc.setColorAttachmentCount(1);
    colourPassDesc.setPColorAttachments(&coloursubPassReference);

    vk::SubpassDescription depthPassDesc{};
    depthPassDesc.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics);
    depthPassDesc.setPDepthStencilAttachment(&depthsubPassReference);

    vk::SubpassDescription normalsPassDesc{};
    normalsPassDesc.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics);
    normalsPassDesc.setColorAttachmentCount(1);
    normalsPassDesc.setPColorAttachments(&normalssubPassReference);

    mRenderPasses.colourPass    = colourPassDesc;
    mRenderPasses.depthPass     = depthPassDesc;
    mRenderPasses.normalsPass   = normalsPassDesc;


    // create the light and composite subPasses
    vk::AttachmentDescription swapChainImageAttachment{};
    swapChainImageAttachment.setFormat(mSwapChain.getSwapChainImageFormat());
    swapChainImageAttachment.setLoadOp(vk::AttachmentLoadOp::eClear);
    swapChainImageAttachment.setStoreOp(vk::AttachmentStoreOp::eStore);
    swapChainImageAttachment.setStencilLoadOp(vk::AttachmentLoadOp::eDontCare);
    swapChainImageAttachment.setStencilStoreOp(vk::AttachmentStoreOp::eDontCare);
    swapChainImageAttachment.setInitialLayout(vk::ImageLayout::eColorAttachmentOptimal);
    swapChainImageAttachment.setFinalLayout(vk::ImageLayout::ePresentSrcKHR);

    vk::AttachmentReference colourCompositPassReference{};
    colourCompositPassReference.setAttachment(3);
    colourCompositPassReference.setLayout(vk::ImageLayout::eColorAttachmentOptimal);

    // calculate and add the light attachment desc
    std::vector<vk::AttachmentReference> spotLightattachmentRefs{coloursubPassReference, depthsubPassReference, normalssubPassReference};
    for(int i = 0; i < spotLights.size(); ++i)
    {
        vk::AttachmentReference lightRef{};
        lightRef.setAttachment(i + 4);
        lightRef.setLayout(vk::ImageLayout::eGeneral);

        spotLightattachmentRefs.push_back(lightRef);
    }

    vk::SubpassDescription compositPassDesc{};
    compositPassDesc.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics);
    compositPassDesc.setInputAttachmentCount(spotLightattachmentRefs.size());
    compositPassDesc.setPInputAttachments(spotLightattachmentRefs.data());
    compositPassDesc.setColorAttachmentCount(1);
    compositPassDesc.setPColorAttachments(&colourCompositPassReference);


    std::vector<vk::AttachmentReference> lightAttachments{};
    for(int i = 0; i < spotLights.size(); ++i) {
        lightAttachments.push_back(coloursubPassReference);
    }

    vk::SubpassDescription lightSubpass{};
    lightSubpass.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics);
    lightSubpass.setColorAttachmentCount(lightAttachments.size());
    lightSubpass.setPColorAttachments(lightAttachments.data());


    std::vector<vk::AttachmentDescription> allAttachments{colourPassAttachment, depthPassAttachment, normalsPassAttachment, swapChainImageAttachment};
    for(int i = 0; i < spotLights.size(); ++i ) { // only support spot lights currently
        allAttachments.push_back(colourPassAttachment);
    }

    // Subpass dependancies
    // The only dependancies are between all the passes and the final pass
    // as only the final passes uses resourecs from the otheres

    vk::SubpassDependency implicitFirstDepen{};
    implicitFirstDepen.setSrcSubpass(VK_SUBPASS_EXTERNAL);
    implicitFirstDepen.setDstSubpass(0);
    implicitFirstDepen.setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);
    implicitFirstDepen.setDstStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);
    implicitFirstDepen.setDstAccessMask(vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite);

    vk::SubpassDependency colourToCompositeDepen{};
    colourToCompositeDepen.setSrcSubpass(0);
    colourToCompositeDepen.setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);
    colourToCompositeDepen.setDstSubpass(4);
    colourToCompositeDepen.setDstStageMask(vk::PipelineStageFlagBits::eFragmentShader);
    colourToCompositeDepen.setSrcAccessMask(vk::AccessFlagBits::eColorAttachmentWrite);
    colourToCompositeDepen.setDstAccessMask(vk::AccessFlagBits::eShaderRead);

    vk::SubpassDependency depthToCompositeDepen{};
    depthToCompositeDepen.setSrcSubpass(1);
    depthToCompositeDepen.setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);
    depthToCompositeDepen.setDstSubpass(4);
    depthToCompositeDepen.setDstStageMask(vk::PipelineStageFlagBits::eFragmentShader);
    depthToCompositeDepen.setSrcAccessMask(vk::AccessFlagBits::eColorAttachmentWrite);
    depthToCompositeDepen.setDstAccessMask(vk::AccessFlagBits::eShaderRead);

    vk::SubpassDependency normalsToCompositeDepen{};
    normalsToCompositeDepen.setSrcSubpass(2);
    normalsToCompositeDepen.setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);
    normalsToCompositeDepen.setDstSubpass(4);
    normalsToCompositeDepen.setDstStageMask(vk::PipelineStageFlagBits::eFragmentShader);
    normalsToCompositeDepen.setSrcAccessMask(vk::AccessFlagBits::eColorAttachmentWrite);
    normalsToCompositeDepen.setDstAccessMask(vk::AccessFlagBits::eShaderRead);

    vk::SubpassDependency lightToCompositeDepen{};
    lightToCompositeDepen.setSrcSubpass(3);
    lightToCompositeDepen.setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);
    lightToCompositeDepen.setDstSubpass(4);
    lightToCompositeDepen.setDstStageMask(vk::PipelineStageFlagBits::eFragmentShader);
    lightToCompositeDepen.setSrcAccessMask(vk::AccessFlagBits::eColorAttachmentWrite);
    lightToCompositeDepen.setDstAccessMask(vk::AccessFlagBits::eShaderRead);

    std::vector<vk::SubpassDescription> allSubpasses{colourPassDesc, depthPassDesc, normalsPassDesc, lightSubpass, compositPassDesc};
    std::vector<vk::SubpassDependency>  allSubpassDependancies{implicitFirstDepen, colourToCompositeDepen, depthToCompositeDepen, normalsToCompositeDepen, lightToCompositeDepen};

    vk::RenderPassCreateInfo renderPassInfo{};
    renderPassInfo.setAttachmentCount(allAttachments.size());
    renderPassInfo.setPAttachments(allAttachments.data());
    renderPassInfo.setSubpassCount(allSubpasses.size());
    renderPassInfo.setPSubpasses(allSubpasses.data());
    renderPassInfo.setDependencyCount(allSubpassDependancies.size());
    renderPassInfo.setPDependencies(allSubpassDependancies.data());

    mRenderPasses.RenderPass = mDevice.createRenderPass(renderPassInfo);
}

void ThiefVKDevice::createFrameBuffers() {

}


void ThiefVKDevice::createCommandPool() {
    auto [graphicsQueueIndex, presentQueueIndex] = getGraphicsAndPresentQueue(mWindowSurface, mPhysDev);

    vk::CommandPoolCreateInfo poolInfo{};
    poolInfo.setQueueFamilyIndex(graphicsQueueIndex);

    graphicsCommandPool = mDevice.createCommandPool(poolInfo);
}


void ThiefVKDevice::createVertexBuffer() {

}


void ThiefVKDevice::createCommandBuffers() {

}


void ThiefVKDevice::createSemaphores() {

}
