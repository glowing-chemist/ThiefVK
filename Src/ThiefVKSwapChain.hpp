#ifndef THIEFVKSWAPCHAIN_HPP
#define THIEFVKSWAPCHAIN_HPP

#include <vulkan/vulkan.hpp>
#include <GLFW/glfw3.h>

struct ThiefVKRenderPasses;

// swapChain setup functions
struct SwapChainSupportDetails { // represent swapchain capabilities
    vk::SurfaceCapabilitiesKHR capabilities;
    std::vector<vk::SurfaceFormatKHR> formats;
    std::vector<vk::PresentModeKHR> presentModes;
};


class ThiefVKSwapChain {
public:
    ThiefVKSwapChain(vk::Device& Device, vk::PhysicalDevice physDevice, vk::SurfaceKHR windowSurface, GLFWwindow* window);

    void destroy(vk::Device&);

    vk::Format getSwapChainImageFormat() const;

    void createSwpaChainFrameBuffers(vk::Device&, ThiefVKRenderPasses& renderPass, uint32_t spotLights);

    unsigned int getSwapChainImageWidth() const;
    unsigned int getSwapChainImageHeight() const;

    unsigned int getNumberOfSwapChainImages() const;

private:
    SwapChainSupportDetails querySwapchainSupport(vk::PhysicalDevice, vk::SurfaceKHR);
    vk::Extent2D chooseSwapExtent(const vk::SurfaceCapabilitiesKHR&, GLFWwindow*);

    void createSwapChainImageViews(vk::Device&);

    vk::SwapchainKHR swapChain;
    std::vector<vk::Image> swapChainImages;
    std::vector<vk::ImageView> swapChainImageViews;
    vk::Extent2D swapChainExtent;
    vk::Format swapChainFormat;
    std::vector<vk::Framebuffer> swapChainFrameBuffers;
};

#endif
