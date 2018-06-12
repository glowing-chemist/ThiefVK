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

#ifdef _MSC_VER 
	__debugbreak;
#else
    asm("int3");
#endif

    return VK_FALSE;

}

const QueueIndicies getAvailableQueues(vk::SurfaceKHR windowSurface, vk::PhysicalDevice& dev) {
    int graphics = -1;
    int present  = -1;
    int compute  = -1;

    std::vector<vk::QueueFamilyProperties> queueProperties = dev.getQueueFamilyProperties();
    for(uint32_t i = 0; i < queueProperties.size(); i++) {
        if(queueProperties[i].queueFlags & vk::QueueFlagBits::eGraphics) graphics = i;
        if(queueProperties[i].queueFlags & vk::QueueFlagBits::eCompute) compute = i;
        if(dev.getSurfaceSupportKHR(i, windowSurface)) present = i;
        if(graphics != -1 && present != -1 && compute != -1) return {graphics, present, compute};
    }
    return {graphics, present, compute};
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

    std::vector<const char*> requiredExtensionVector{requiredExtensions, requiredExtensions + numExtensions};

#ifndef NDEBUG
    requiredExtensionVector.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
#endif

    vk::InstanceCreateInfo instanceInfo{};
    instanceInfo.setEnabledExtensionCount(requiredExtensionVector.size());
    instanceInfo.setPpEnabledExtensionNames(requiredExtensionVector.data());
    instanceInfo.setPApplicationInfo(&appInfo);
#ifndef NDEBUG
    const auto availableLayers = vk::enumerateInstanceLayerProperties();
    bool validationLayersFound = false;
    const char* validationLayers = "VK_LAYER_LUNARG_standard_validation";

    for(auto& layer : availableLayers) {
        if(strcmp(layer.layerName, validationLayers) == 0) {
            validationLayersFound = true;
            break;
        }
    }
    if(!validationLayersFound) throw std::runtime_error{"Running in debug but Lunarg validation layers not found"};

    instanceInfo.setEnabledLayerCount(1);
    instanceInfo.setPpEnabledLayerNames(&validationLayers);
#endif

    mInstance = vk::createInstance(instanceInfo);

#ifndef NDEBUG // add the debug callback as soon as possible
    addDebugCallback();
#endif


    VkSurfaceKHR surface;
    glfwCreateWindowSurface(static_cast<VkInstance>(mInstance), mWindow, nullptr, &surface); // use glfw as is platform agnostic
    mWindowSurface = vk::SurfaceKHR(surface);
}


std::pair<vk::PhysicalDevice, vk::Device> ThiefVKInstance::findSuitableDevices(int DeviceFeatureFlags) {
    bool GeometryWanted = DeviceFeatureFlags & ThiefDeviceFeaturesFlags::Geometry;
    bool TessWanted     = DeviceFeatureFlags & ThiefDeviceFeaturesFlags::Tessalation;
    bool DiscreteWanted = DeviceFeatureFlags & ThiefDeviceFeaturesFlags::Discrete;
    bool ComputeWanted  = DeviceFeatureFlags & ThiefDeviceFeaturesFlags::Compute;

    auto availableDevices = mInstance.enumeratePhysicalDevices();
    std::vector<int> deviceScores(availableDevices.size());

    for(uint32_t i = 0; i < availableDevices.size(); i++) {
        const vk::PhysicalDeviceProperties properties = availableDevices[i].getProperties();
        const vk::PhysicalDeviceFeatures   features   = availableDevices[i].getFeatures();
        const QueueIndicies queueIndices = getAvailableQueues(mWindowSurface, availableDevices[i]);

        std::cout << "Device Found: " << properties.deviceName << '\n';

        if(GeometryWanted && features.geometryShader) deviceScores[i] += 1;
        if(TessWanted && features.tessellationShader) deviceScores[i] += 1;
        if(DiscreteWanted && properties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu) deviceScores[i] += 1;
        if(ComputeWanted && (queueIndices.ComputeQueueIndex != -1)) deviceScores[i] += 1; // check if a compute queue is available
    }

    auto maxScoreeDeviceOffset = std::max_element(deviceScores.begin(), deviceScores.end());
    size_t physicalDeviceIndex = std::distance(deviceScores.begin(), maxScoreeDeviceOffset);
    vk::PhysicalDevice physicalDevice = availableDevices[physicalDeviceIndex];

	std::cout << "Device selected: " << physicalDevice.getProperties().deviceName << '\n';

    const QueueIndicies queueIndices = getAvailableQueues(mWindowSurface, physicalDevice);

    float queuePriority = 1.0f;

    std::set<int> uniqueQueues{queueIndices.GraphicsQueueIndex, queueIndices.PresentQueueIndex, queueIndices.ComputeQueueIndex};
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
        auto call = static_cast<VkDebugReportCallbackEXT>(debugCallback);
        func(static_cast<VkInstance>(mInstance), &callbackCreateInfo, nullptr, &call);
    }

}

GLFWwindow* ThiefVKInstance::getWindow() const {
    return mWindow;
}


vk::SurfaceKHR ThiefVKInstance::getSurface() const {
    return mWindowSurface;
}


int operator|(ThiefDeviceFeaturesFlags lhs, ThiefDeviceFeaturesFlags rhs) {
    return static_cast<int>(lhs) | static_cast<int>(rhs);
}


int operator|(int lhs, ThiefDeviceFeaturesFlags rhs) {
	return lhs | static_cast<int>(rhs);
}


bool operator&(int flags, ThiefDeviceFeaturesFlags rhs) {
    return flags & static_cast<int>(rhs);
}
