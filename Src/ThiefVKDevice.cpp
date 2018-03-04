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
    mDevice.destroy();
}


void ThiefVKDevice::createRenderPasses() {

    // Specify all attachments used in all subrenderpasses
    vk::AttachmentDescription colourPassAttachment{}; // also used for the shadow mapping
    colourPassAttachment.setFormat(vk::Format::eR8G8B8A8Srgb);
    colourPassAttachment.setLoadOp(vk::AttachmentLoadOp::eDontCare); // we are going to overwrite all pixles
    colourPassAttachment.setStoreOp(vk::AttachmentStoreOp::eDontCare);
    colourPassAttachment.setInitialLayout(vk::ImageLayout::eColorAttachmentOptimal);
    colourPassAttachment.setFinalLayout(vk::ImageLayout::eColorAttachmentOptimal);

    vk::AttachmentDescription depthPassAttachment{};
    depthPassAttachment.setFormat(vk::Format::eR32Sfloat); // store in each pixel a 32bit depth value
    depthPassAttachment.setStencilLoadOp(vk::AttachmentLoadOp::eDontCare);
    depthPassAttachment.setStencilStoreOp(vk::AttachmentStoreOp::eDontCare);
    depthPassAttachment.setInitialLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal); // wriet in a subpass then read in a subsequent one
    depthPassAttachment.setFinalLayout(vk::ImageLayout::eDepthStencilReadOnlyOptimal);

    vk::AttachmentDescription normalsPassAttachment{};
    normalsPassAttachment.setFormat(vk::Format::eR8G8B8Snorm);
    normalsPassAttachment.setLoadOp(vk::AttachmentLoadOp::eDontCare);
    normalsPassAttachment.setStoreOp(vk::AttachmentStoreOp::eDontCare);
    normalsPassAttachment.setInitialLayout(vk::ImageLayout::eColorAttachmentOptimal);
    normalsPassAttachment.setFinalLayout(vk::ImageLayout::eColorAttachmentOptimal); // these will be used in the subsqeuent light renderpass

    vk::AttachmentDescription swapChainImageAttachment{};
    swapChainImageAttachment.setFormat(mSwapChain.getSwapChainImageFormat());
    swapChainImageAttachment.setLoadOp(vk::AttachmentLoadOp::eClear);
    swapChainImageAttachment.setStoreOp(vk::AttachmentStoreOp::eStore);
    swapChainImageAttachment.setInitialLayout(vk::ImageLayout::eColorAttachmentOptimal);
    swapChainImageAttachment.setFinalLayout(vk::ImageLayout::ePresentSrcKHR);

    // declare all subpasses
    // we will have a subpass for depth, normals, colour.
    // as these will remain constant regardless of the number of lights
    // all written attachments will be used in a subsequent ligth renderPass


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
