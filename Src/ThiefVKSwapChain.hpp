#ifndef THIEFVKSWAPCHAIN_HPP
#define THIEFVKSWAPCHAIN_HPP

#include <vulkan/vulkan.hpp>
#include <GLFW/glfw3.h>

// swapChain setup functions
struct SwapChainSupportDetails { // represent swapchain capabilities
    vk::SurfaceCapabilitiesKHR capabilities;
    std::vector<vk::SurfaceFormatKHR> formats;
    std::vector<vk::PresentModeKHR> presentModes;
};


class ThiefVKSwapChain {
public:
    ThiefVKSwapChain(vk::Device Device, vk::PhysicalDevice physDevice, vk::SurfaceKHR windowSurface, GLFWwindow* window);
    ~ThiefVKSwapChain();

private:
    SwapChainSupportDetails querySwapchainSupport(vk::PhysicalDevice, vk::SurfaceKHR);
    vk::Extent2D chooseSwapExtent(const vk::SurfaceCapabilitiesKHR&, GLFWwindow*);

    void createSwapChainImageViews(vk::Device);
    void createSwpaChainFrameBuffers(vk::Device, vk::RenderPass renderPass);

    vk::SwapchainKHR swapChain;
    std::vector<VkImage> swapChainImages;
    std::vector<VkImageView> swapChainImageViews;
    vk::Extent2D swapChainExtent;
    vk::Format swapChainFormat;
    std::vector<vk::Framebuffer> swapChainFrameBuffers;
};

#endif
