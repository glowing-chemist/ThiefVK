#ifndef THIEFVKCONTEXT_HPP
#define THIEFVKCONTEXT_HPP

#include <vulkan/vulkan.h>
#include <vulkan/vulkan.hpp> // use vulkan hpp for erganomics
#include <GLFW/glfw3.h>

#include <tuple>

enum class ThiefDeviceFeaturesFlags {
    Geometry = 1,
    Tessalation = 2,
    Discrete = 4
};

int operator|(ThiefDeviceFeaturesFlags, ThiefDeviceFeaturesFlags);
bool operator&(int, ThiefDeviceFeaturesFlags);


class ThiefVKInstance {
public:
    ThiefVKInstance();

    std::pair<vk::PhysicalDevice, vk::Device> findSuitableDevices(int DeviceFeatureFlags);

private:
    std::pair<int, int> getGraphicsAndPresentQueue(vk::PhysicalDevice& dev);

    vk::Instance mInstance;
    VkSurfaceKHR mWindowSurface;
    GLFWwindow* mWindow;
};


#endif
