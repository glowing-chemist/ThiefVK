#ifndef THIEFVKDEVICE_HPP
#define THIEFVKDEVICE_HPP

#include <vulkan/vulkan.hpp>

class ThiefVKDevice {

public:
    explicit ThiefVKDevice(vk::PhysicalDevice, vk::Device);

private:
    vk::PhysicalDevice mPhysDev;
    vk::Device mDevice;

    vk::Queue mGraphicsQueue;
    vk::Queue mPresentQueue;
};

#endif
