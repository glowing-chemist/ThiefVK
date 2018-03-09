#include "ThiefVKEngine.hpp"

#include "ThiefVKDevice.hpp"
#include "ThiefVKInstance.hpp"


ThiefVKEngine::ThiefVKEngine() : Instance{ThiefVKInstance()},
                                 Device{ThiefVKDevice(Instance.findSuitableDevices(
                                                                                   ThiefDeviceFeaturesFlags::Discrete
                                                                                   | ThiefDeviceFeaturesFlags::Geometry )
                                                      , Instance.getSurface(), Instance.getWindow())} {}


void ThiefVKEngine::Init() {
#ifndef NDEBUG
    Instance.addDebugCallback();
#endif
    Device.createRenderPasses();
}
