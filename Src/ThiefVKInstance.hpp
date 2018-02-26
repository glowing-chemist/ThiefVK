#ifndef THIEFVKCONTEXT_HPP
#define THIEFVKCONTEXT_HPP

#include <vulkan/vulkan.hpp> // use vulkan hpp for erganomics

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
    vk::Instance mInstance;
};


#endif
