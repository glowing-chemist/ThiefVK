#include "ThiefVKEngine.hpp"

#include "ThiefVKDevice.hpp"
#include "ThiefVKInstance.hpp"
#include "ThiefVKModel.hpp"


ThiefVKEngine::ThiefVKEngine(GLFWwindow* window) :mWindow{window}, 
                                mInstance{ThiefVKInstance(mWindow)},
                                mDevice{ThiefVKDevice(mInstance.findSuitableDevices(
                                                                                   ThiefDeviceFeaturesFlags::Discrete
                                                                                   | ThiefDeviceFeaturesFlags::Geometry
																				   | ThiefDeviceFeaturesFlags::Compute)
                                                      , mInstance.getSurface(), mInstance.getWindow())} {}


ThiefVKEngine::~ThiefVKEngine() {
}



void ThiefVKEngine::Init() {
  mDevice.createRenderPasses();
  mDevice.createDeferedRenderTargetImageViews();
  mDevice.createFrameBuffers();
	mDevice.createCommandPools();
  mDevice.createSemaphores();
}


void ThiefVKEngine::addModelToScene(ThiefVKModel& model) {
  mModels.push_back(model);
}


void ThiefVKEngine::addLightToScene(glm::mat4& light) {
  mDevice.addSpotLight(light);
}


void ThiefVKEngine::renderScene() {
  mDevice.startFrame();

  for(auto& model : mModels) {
    mDevice.draw(model.getGeometry());
  }
  mModels.clear();

  mDevice.endFrame();
  mDevice.swap();
}