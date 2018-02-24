#ifndef THIEFVKCONTEXT_HPP
#define THIEFVKCONTEXT_HPP

#include <vulkan/vulkan.hpp> // use vulkan hpp for erganomics

#include <tuple>

enum class ThiefDeviceFeaturesFlags {
    Present,
    Geometry,
    Tessalation,
    Discrete
};


class ThiefVKInstance {
public:
    ThiefVKInstance();

    std::pair<vk::PhysicalDevice, vk::Device> findSuitableDevices(int DeviceFeatureFlags);

private:
    vk::Instance mInstance;
};


#endif
