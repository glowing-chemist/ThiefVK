#include "ThiefVKEngine.hpp"

#include "ThiefVKDevice.hpp"
#include "ThiefVKInstance.hpp"


ThiefVKEngine::ThiefVKEngine() : Instance{ThiefVKInstance()},
                                 Device{ThiefVKDevice(Instance.findSuitableDevices(
                                                                                   ThiefDeviceFeaturesFlags::Discrete
                                                                                   | ThiefDeviceFeaturesFlags::Geometry )
                                                      , Instance.getSurface(), Instance.getWindow())} {}


ThiefVKEngine::~ThiefVKEngine() {
    MemoryManager.Destroy(); // to ensure we deallocat all our memory before we destroy the device and instance
}



void ThiefVKEngine::Init() {
#ifndef NDEBUG
    Instance.addDebugCallback();
#endif
    Device.createRenderPasses();

    auto [physDev, Dev] = Device.getDeviceHandles();
    ThiefVKMemoryManager memoryManager(physDev, Dev);
    MemoryManager = memoryManager;
}
