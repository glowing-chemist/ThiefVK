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
  Device.createRenderPasses();
  Device.createDeferedRenderTargetImageViews();
  Device.createFrameBuffers();
	Device.createCommandPools();
  Device.createSemaphores();
  Device.createDescriptorSets();

  for(int i = 0; i < 100; i++) {
	     Device.startFrame();
       Device.endFrame();
       Device.swap();
  }
}
