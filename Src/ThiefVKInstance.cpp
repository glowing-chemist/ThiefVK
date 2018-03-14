// Local includes
#include "ThiefVKInstance.hpp"

// std library includes
#include <tuple>
#include <vector>
#include <set>
#include <algorithm>
#include <iostream>

// system includes
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan.h>

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallbackFunc(
    VkDebugReportFlagsEXT,
    VkDebugReportObjectTypeEXT,
    uint64_t,
    size_t,
    int32_t,
    const char*,
    const char* msg,
    void*) {

    std::cerr << "validation layer: " << msg << std::endl;

    return VK_FALSE;

}

std::pair<int, int> getGraphicsAndPresentQueue(vk::SurfaceKHR windowSurface, vk::PhysicalDevice& dev) {
    int graphics = -1;
    int present  = -1;

    std::vector<vk::QueueFamilyProperties> queueProperties = dev.getQueueFamilyProperties();
    for(uint32_t i = 0; i < queueProperties.size(); i++) {
        if(queueProperties[i].queueFlags & vk::QueueFlagBits::eGraphics) graphics = i;
        if(dev.getSurfaceSupportKHR(i, windowSurface)) present = i;
        if(graphics != -1 && present != -1) return {graphics, present};
    }
    return {graphics, present};
}


ThiefVKInstance::ThiefVKInstance() {

    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GL_FALSE); // only resize explicitly

    mWindow = glfwCreateWindow(500, 500, "Necromancer", nullptr, nullptr);

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
#ifndef NDEBUG
    const char* validationLayers = "VK_LAYER_LUNARG_standard_validation";
    instanceInfo.setEnabledLayerCount(1);
    instanceInfo.setPpEnabledLayerNames(&validationLayers);
#endif

    mInstance = vk::createInstance(instanceInfo);

    VkSurfaceKHR surface;
    glfwCreateWindowSurface(mInstance, mWindow, nullptr, &surface); // use glfw as is platform agnostic
    mWindowSurface = surface;
}


std::pair<vk::PhysicalDevice, vk::Device> ThiefVKInstance::findSuitableDevices(int DeviceFeatureFlags) {
    bool GeometryWanted = DeviceFeatureFlags & ThiefDeviceFeaturesFlags::Geometry;
    bool TessWanted     = DeviceFeatureFlags & ThiefDeviceFeaturesFlags::Tessalation;
    bool DiscreteWanted = DeviceFeatureFlags & ThiefDeviceFeaturesFlags::Discrete;

    auto availableDevices = mInstance.enumeratePhysicalDevices();
    std::vector<int> deviceScores(availableDevices.size());

    for(uint32_t i = 0; i < availableDevices.size(); i++) {
        vk::PhysicalDeviceProperties properties = availableDevices[i].getProperties();
        vk::PhysicalDeviceFeatures   features   = availableDevices[i].getFeatures();

        std::cout << "Device Found: " << properties.deviceName << '\n';

        if(GeometryWanted && features.geometryShader) deviceScores[i] += 1;
        if(TessWanted && features.tessellationShader) deviceScores[i] += 1;
        if(DiscreteWanted && properties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu) deviceScores[i] += 1;
    }

    auto maxScoreeDeviceOffset = std::max_element(availableDevices.begin(), availableDevices.end());
    size_t physicalDeviceIndex = std::distance(availableDevices.begin(), maxScoreeDeviceOffset);
    vk::PhysicalDevice physicalDevice = availableDevices[physicalDeviceIndex];

    const auto[graphics, present] = getGraphicsAndPresentQueue(mWindowSurface, physicalDevice);

    float queuePriority = 1.0f;

    std::set<int> uniqueQueues{graphics, present};
    std::vector<vk::DeviceQueueCreateInfo> queueInfo{};
    for(auto& queueIndex : uniqueQueues) {
        vk::DeviceQueueCreateInfo info{};
        info.setQueueCount(1);
        info.setQueueFamilyIndex(queueIndex);
        info.setPQueuePriorities(&queuePriority);
        queueInfo.push_back(info);
    }

    const char* deviceExtensions = VK_KHR_SWAPCHAIN_EXTENSION_NAME;

    vk::PhysicalDeviceFeatures physicalFeatures{};

    vk::DeviceCreateInfo deviceInfo{};
    deviceInfo.setEnabledExtensionCount(1);
    deviceInfo.setPpEnabledExtensionNames(&deviceExtensions);
    deviceInfo.setQueueCreateInfoCount(uniqueQueues.size());
    deviceInfo.setPQueueCreateInfos(queueInfo.data());
    deviceInfo.setPEnabledFeatures(&physicalFeatures);
#ifndef NDEBUG
    const char* validationLayers = "VK_LAYER_LUNARG_standard_validation";
    deviceInfo.setEnabledLayerCount(1);
    deviceInfo.setPpEnabledLayerNames(&validationLayers);
#endif

    vk::Device logicalDevice = physicalDevice.createDevice(deviceInfo);

    return {physicalDevice, logicalDevice};
}

ThiefVKInstance::~ThiefVKInstance() {
    mInstance.destroySurfaceKHR(mWindowSurface);
    mInstance.destroy();
    glfwDestroyWindow(mWindow);
    glfwTerminate();
}

void ThiefVKInstance::addDebugCallback() {
    VkDebugReportCallbackCreateInfoEXT callbackCreateInfo{};
    callbackCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CREATE_INFO_EXT;
    callbackCreateInfo.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT;
    callbackCreateInfo.pfnCallback = debugCallbackFunc;

    auto* func = (PFN_vkCreateDebugReportCallbackEXT) mInstance.getProcAddr("vkCreateDebugReportCallbackEXT");
    if(func != nullptr) {
        auto* call = static_cast<VkDebugReportCallbackEXT>(debugCallback);
        func(mInstance, &callbackCreateInfo, nullptr, &call);
    }

}

GLFWwindow* ThiefVKInstance::getWindow() const {
    return mWindow;
}


vk::SurfaceKHR ThiefVKInstance::getSurface() const {
    return mWindowSurface;
}


int operator|(ThiefDeviceFeaturesFlags lhs, ThiefDeviceFeaturesFlags rhs) {
    return static_cast<int>(lhs) & static_cast<int>(rhs);
}


bool operator&(int flags, ThiefDeviceFeaturesFlags rhs) {
    return flags & static_cast<int>(rhs);
}
