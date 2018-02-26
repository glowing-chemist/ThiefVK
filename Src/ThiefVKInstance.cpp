// Local includes
#include "ThiefVKInstance.hpp"

// std library includes
#include <tuple>
#include <vector>
#include <algorithm>
#include <iostream>

// system includes
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.hpp>


ThiefVKInstance::ThiefVKInstance() {

    vk::ApplicationInfo appInfo{};
    appInfo.setPApplicationName("ThiefQuango");
    appInfo.setApiVersion(VK_MAKE_VERSION(0, 1, 0));
    appInfo.setPEngineName("ThiefVK");
    appInfo.setEngineVersion(VK_MAKE_VERSION(0, 1, 0));
    appInfo.setApiVersion(VK_API_VERSION_1_0);

    uint32_t numExtensions = 0;
    const char* const* requiredExtensions = glfwGetRequiredInstanceExtensions(&numExtensions);

    vk::InstanceCreateInfo instanceInfo{};
    instanceInfo.setEnabledExtensionCount(numExtensions);
    instanceInfo.setPpEnabledExtensionNames(requiredExtensions);
    instanceInfo.setPApplicationInfo(&appInfo);

    mInstance = vk::createInstance(instanceInfo);

}


std::pair<vk::PhysicalDevice, vk::Device> ThiefVKInstance::findSuitableDevices(int DeviceFeatureFlags) {
    bool GeometryWanted = DeviceFeatureFlags & ThiefDeviceFeaturesFlags::Geometry;
    bool TessWanted     = DeviceFeatureFlags & ThiefDeviceFeaturesFlags::Tessalation;
    bool DiscreteWanted = DeviceFeatureFlags & ThiefDeviceFeaturesFlags::Discrete;

    auto availableDevices = mInstance.enumeratePhysicalDevices();
    std::vector<int> deviceScores(availableDevices.size());

    for(int i = 0; i < availableDevices.size(); i++) {
        vk::PhysicalDeviceProperties properties = availableDevices[i].getProperties();
        vk::PhysicalDeviceFeatures   features   = availableDevices[i].getFeatures();

        std::cout << "Device Found: " << properties.deviceName << '\n';

        if(GeometryWanted && features.geometryShader) deviceScores[i] += 1;
        if(TessWanted && features.tessellationShader) deviceScores[i] += 1;
        if(DiscreteWanted && properties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu) deviceScores[i] += 1;
    }

    auto maxScoreeDeviceOffset = std::max_element(availableDevices.begin(), availableDevices.end());
    size_t physicalDeviceIndex = std::distance(availableDevices.begin(), maxScoreeDeviceOffset);
    vk::PhysicalDevice chosenDevice = availableDevices[physicalDeviceIndex];

}



int operator|(ThiefDeviceFeaturesFlags lhs, ThiefDeviceFeaturesFlags rhs) {
    return static_cast<int>(lhs) & static_cast<int>(rhs);
}


bool operator&(int flags, ThiefDeviceFeaturesFlags rhs) {
    return flags & static_cast<int>(rhs);
}
