#include "ThiefVKEngine.hpp"

#include "ThiefVKDevice.hpp"
#include "ThiefVKInstance.hpp"


ThiefVKEngine::ThiefVKEngine() : Instance{ThiefVKInstance()},
                                 Device{ThiefVKDevice(Instance.findSuitableDevices(
                                                                                   ThiefDeviceFeaturesFlags::Discrete
                                                                                   | ThiefDeviceFeaturesFlags::Geometry
																				   | ThiefDeviceFeaturesFlags::Compute)
                                                      , Instance.getSurface(), Instance.getWindow())} {}


ThiefVKEngine::~ThiefVKEngine() {
}



void ThiefVKEngine::Init() {
#ifndef NDEBUG
    Instance.addDebugCallback();
#endif
    Device.createRenderPasses();
    Device.createDeferedRenderTargetImageViews();
    Device.createFrameBuffers();
	Device.createCommandPools();
	Device.createCommandBuffers();
}
