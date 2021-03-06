#ifndef THIEFVKCONTEXT_HPP
#define THIEFVKCONTEXT_HPP

#include <vulkan/vulkan.h>
#include <vulkan/vulkan.hpp> // use vulkan hpp for erganomics
#include <GLFW/glfw3.h>

#include <tuple>

enum class ThiefDeviceFeaturesFlags {
    Geometry = 1,
    Tessalation = 2,
    Discrete = 4,
    Compute = 8
};

int operator|(ThiefDeviceFeaturesFlags, ThiefDeviceFeaturesFlags);
int operator|(int, ThiefDeviceFeaturesFlags);
bool operator&(int, ThiefDeviceFeaturesFlags);

struct QueueIndicies {
    int GraphicsQueueIndex;
    int PresentQueueIndex;
    int ComputeQueueIndex;
};


const QueueIndicies getAvailableQueues(vk::SurfaceKHR windowSurface, vk::PhysicalDevice& dev);

class ThiefVKInstance {
public:
    ThiefVKInstance(GLFWwindow*);
    ~ThiefVKInstance();

    void addDebugCallback();

    std::pair<vk::PhysicalDevice, vk::Device> findSuitableDevices(int DeviceFeatureFlags = 0);
    GLFWwindow* getWindow() const;
    vk::SurfaceKHR getSurface() const;

private:

    vk::Instance mInstance;
    vk::DebugReportCallbackEXT debugCallback;
    vk::SurfaceKHR mWindowSurface;
    GLFWwindow* mWindow;
};


#endif
