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

std::array<vk::VertexInputAttributeDescription, 5> Vertex::getAttribDesc() {

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

    vk::VertexInputAttributeDescription atribDesctrans1{};
    atribDesctrans1.setBinding(0);
    atribDesctrans1.setLocation(0);
    atribDesctrans1.setFormat(vk::Format::eR32G32B32Sfloat);
    atribDesctrans1.setOffset(offsetof(Vertex, transCom1));

    vk::VertexInputAttributeDescription atribDesctrans2{};
    atribDesctrans2.setBinding(0);
    atribDesctrans2.setLocation(0);
    atribDesctrans2.setFormat(vk::Format::eR32G32B32Sfloat);
    atribDesctrans2.setOffset(offsetof(Vertex, transCom2));

    vk::VertexInputAttributeDescription atribDesctrans3{};
    atribDesctrans3.setBinding(0);
    atribDesctrans3.setLocation(0);
    atribDesctrans3.setFormat(vk::Format::eR32G32B32Sfloat);
    atribDesctrans3.setOffset(offsetof(Vertex, transCom1));

    return {atribDescPos, atribDescTex, atribDesctrans1, atribDesctrans2, atribDesctrans3};
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

    // TODO specify the subpass dependancies
    std::vector<vk::AttachmentDescription> allAttachments{colourPassAttachment, depthPassAttachment, normalsPassAttachment, swapChainImageAttachment};
    for(int i = 0; i < spotLights.size(); ++i ) { // only support spot lights currently
        allAttachments.push_back(colourPassAttachment);
    }

    std::vector<vk::SubpassDescription> allSubpasses{colourPassDesc, depthPassDesc, normalsPassDesc, compositPassDesc};

    vk::RenderPassCreateInfo renderPassInfo{};
    renderPassInfo.setAttachmentCount(allAttachments.size());
    renderPassInfo.setPAttachments(allAttachments.data());
    renderPassInfo.setSubpassCount(allSubpasses.size());
    renderPassInfo.setPSubpasses(allSubpasses.data());

    mRenderPasses.RenderPass = mDevice.createRenderPass(renderPassInfo);
}

void ThiefVKDevice::createFrameBuffers() {

}


void ThiefVKDevice::createCommandPool() {

}


void ThiefVKDevice::createVertexBuffer() {

}


void ThiefVKDevice::createCommandBuffers() {

}


void ThiefVKDevice::createSemaphores() {

}
