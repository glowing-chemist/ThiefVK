#include "ThiefVKDevice.hpp"
#include "ThiefVKInstance.hpp"

#include <array>

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


// basic struct to keep track of queuefamily indicies
struct QueueFamilyIndices { // Helper struct to represent QueFamilies
    int graphicsFamily = -1;
    int presentFamily = -1;

    bool isComplete() {
        return graphicsFamily >= 0 && presentFamily >= 0;
    }
};

vk::SurfaceFormatKHR chooseSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& formats) {
    if(formats.size() == 1 && formats[0].format == vk::Format::eUndefined) { // indicates any format can be used, so pick the best
        vk::SurfaceFormatKHR form;
        form.format = vk::Format::eB8G8R8A8Unorm;
        form.colorSpace = vk::ColorSpaceKHR::eSrgbNonlinear;
        return form;
    }
    return formats[0]; // if not pick the first format
}

SwapChainSupportDetails ThiefVKDevice::querySwapchainSupport(vk::PhysicalDevice dev) {
    SwapChainSupportDetails details;

    details.capabilities = dev.getSurfaceCapabilitiesKHR(mWindowSurface);
    details.formats = dev.getSurfaceFormatsKHR(mWindowSurface);
    details.presentModes = dev.getSurfacePresentModesKHR(mWindowSurface);

    return details;
}

vk::PresentModeKHR choosePresentMode(const std::vector<vk::PresentModeKHR>& presentModes) {
    for(auto& mode : presentModes) {
        if(mode == vk::PresentModeKHR::eMailbox) { // if availble return mailbox
            return mode;
        }
    }
    return vk::PresentModeKHR::eFifo; // else Efifo is garenteed to be present
}

vk::Extent2D ThiefVKDevice::chooseSwapExtent(const vk::SurfaceCapabilitiesKHR&) {
    int width, height;
    glfwGetWindowSize(mWindow, &width, &height);

    return vk::Extent2D{static_cast<uint32_t>(width), static_cast<uint32_t>(height)};
}

void ThiefVKDevice::createSwapChain() {
    SwapChainSupportDetails swapDetails = querySwapchainSupport(mPhysDev);
    vk::SurfaceFormatKHR swapFormat = chooseSurfaceFormat(swapDetails.formats);
    vk::PresentModeKHR presMode = choosePresentMode(swapDetails.presentModes);
    vk::Extent2D swapExtent = chooseSwapExtent(swapDetails.capabilities);

    uint32_t images = swapDetails.capabilities.minImageCount + 1 > swapDetails.capabilities.maxImageCount ? swapDetails.capabilities.maxImageCount: swapDetails.capabilities.minImageCount + 1;

    vk::SwapchainCreateInfoKHR info{}; // fill out the format and presentation mode info
    info.setSurface(mWindowSurface);
    info.setImageExtent(swapExtent);
    info.setPresentMode(presMode);
    info.setImageColorSpace(swapFormat.colorSpace);
    info.setImageFormat(swapFormat.format);
    info.setMinImageCount(images);
    info.setImageArrayLayers(1);
    info.setImageUsage(vk::ImageUsageFlagBits::eColorAttachment);

    const auto [graphicsQueueIndex, presentQueueIndex] = getGraphicsAndPresentQueue(mWindowSurface, mPhysDev); //select which queues the swap chain can be accessed by
    if(graphicsQueueIndex != presentQueueIndex) {
        uint32_t indices[2] = {static_cast<uint32_t>(graphicsQueueIndex), static_cast<uint32_t>(presentQueueIndex)};
        info.setImageSharingMode(vk::SharingMode::eConcurrent);
        info.setQueueFamilyIndexCount(2);
        info.setPQueueFamilyIndices(indices);
    } else {
        info.setImageSharingMode(vk::SharingMode::eExclusive); // graphics and present are the same queue
    }

    info.setPreTransform(swapDetails.capabilities.currentTransform);
    info.setCompositeAlpha(vk::CompositeAlphaFlagBitsKHR::eOpaque);
    info.setPresentMode(presMode);
    info.setClipped(true);

    VkSwapchainKHR sc;
    VkSwapchainCreateInfoKHR i = info;
    vkCreateSwapchainKHR(mDevice, &i, nullptr, &sc); // create swapChain
    swapChain = sc;

    uint32_t imageCount; // get references to the images in the swapChain
    vkGetSwapchainImagesKHR(mDevice, swapChain, &imageCount, nullptr);
    swapChainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(mDevice, swapChain, &imageCount, swapChainImages.data());

    swapChainExtent = swapExtent;
    swapChainFormat = swapFormat.format;
}
