#include "ThiefVKSwapChain.hpp"
#include "ThiefVKInstance.hpp"
#include "ThiefVKDevice.hpp"

#include <vulkan/vulkan.hpp>
#include <GLFW/glfw3.h>

#include <iostream>

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

unsigned int ThiefVKSwapChain::getSwapChainImageWidth() const {
    return swapChainExtent.width;
}


unsigned int ThiefVKSwapChain::getSwapChainImageHeight() const {
    return swapChainExtent.height;
}


unsigned int ThiefVKSwapChain::getNumberOfSwapChainImages() const {
    return swapChainImages.size();
}


SwapChainSupportDetails ThiefVKSwapChain::querySwapchainSupport(vk::PhysicalDevice dev, vk::SurfaceKHR surface) {
    SwapChainSupportDetails details;

    details.capabilities = dev.getSurfaceCapabilitiesKHR(surface);
    details.formats = dev.getSurfaceFormatsKHR(surface);
    details.presentModes = dev.getSurfacePresentModesKHR(surface);

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

vk::Extent2D ThiefVKSwapChain::chooseSwapExtent(const vk::SurfaceCapabilitiesKHR&, GLFWwindow* window) {
    int width, height;
    glfwGetWindowSize(window, &width, &height);

    return vk::Extent2D{static_cast<uint32_t>(width), static_cast<uint32_t>(height)};
}

ThiefVKSwapChain::ThiefVKSwapChain(vk::Device& Device, vk::PhysicalDevice physDevice, vk::SurfaceKHR windowSurface, GLFWwindow* window) {
    SwapChainSupportDetails swapDetails = querySwapchainSupport(physDevice, windowSurface);
    vk::SurfaceFormatKHR swapFormat = chooseSurfaceFormat(swapDetails.formats);
    vk::PresentModeKHR presMode = choosePresentMode(swapDetails.presentModes);
    vk::Extent2D swapExtent = chooseSwapExtent(swapDetails.capabilities, window);

    uint32_t images = 0;
    if(swapDetails.capabilities.maxImageCount == 0) { // some intel GPUs return max image count of 0 so work around this here
        images = swapDetails.capabilities.minImageCount + 1;
    } else {
        images = swapDetails.capabilities.minImageCount + 1 > swapDetails.capabilities.maxImageCount ? swapDetails.capabilities.maxImageCount: swapDetails.capabilities.minImageCount + 1;
    }

    vk::SwapchainCreateInfoKHR info{}; // fill out the format and presentation mode info
    info.setSurface(windowSurface);
    info.setImageExtent(swapExtent);
    info.setPresentMode(presMode);
    info.setImageColorSpace(swapFormat.colorSpace);
    info.setImageFormat(swapFormat.format);
    info.setMinImageCount(images);
    info.setImageArrayLayers(1);
    info.setImageUsage(vk::ImageUsageFlagBits::eColorAttachment);

    const auto [graphicsQueueIndex, presentQueueIndex] = getGraphicsAndPresentQueue(windowSurface, physDevice); //select which queues the swap chain can be accessed by
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

    swapChain = Device.createSwapchainKHR(info);

    swapChainImages = Device.getSwapchainImagesKHR(swapChain);

    swapChainExtent = swapExtent;
    swapChainFormat = swapFormat.format;

    createSwapChainImageViews(Device);
}


void ThiefVKSwapChain::destroy(vk::Device& dev) {
    for(auto& imageView : swapChainImageViews) {
        dev.destroyImageView(imageView);
    }
    dev.destroySwapchainKHR(swapChain);
}


vk::Format ThiefVKSwapChain::getSwapChainImageFormat() const {
    return swapChainFormat;
}


void ThiefVKSwapChain::createSwapChainImageViews(vk::Device &Device) {
    swapChainImageViews.resize(swapChainImages.size());
    for(size_t i = 0; i < swapChainImages.size(); i++) {
        vk::ImageViewCreateInfo info{};
        info.setImage(swapChainImages[i]);
        info.setViewType(vk::ImageViewType::e2D);
        info.setFormat(swapChainFormat);

        info.setComponents(vk::ComponentMapping()); // set swizzle components to identity
        info.setSubresourceRange(vk::ImageSubresourceRange{vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1});

        swapChainImageViews[i] = Device.createImageView(info);
    }
    std::cerr << "created " << swapChainImageViews.size() << " image views" << std::endl;
}


const vk::ImageView& ThiefVKSwapChain::getImageView(const size_t index) const {
    return swapChainImageViews[index];
}

