#include "ThiefVKEngine.hpp"

#include "ThiefVKDevice.hpp"
#include "ThiefVKInstance.hpp"
#include "ThiefVKModel.hpp"

#include <glm/gtc/matrix_transform.hpp>

ThiefVKEngine::ThiefVKEngine() :mWindow{glfwCreateWindow(500, 500, "Necromancer", nullptr, nullptr)}, 
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

  auto view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
  auto proj = glm::perspective(glm::radians(45.0f), 500 / (float) 500, 0.1f, 10.0f);

  //proj[1][1] *= -1; // to map from gl to vulkan space

  ThiefVKModel chaletModel("./chalet.obj", "./chalet.jpg");
  chaletModel.setCameraMatrix(view);
  chaletModel.setWorldMatrix(proj);

  for(int i = 0; i < 200; i++) {
    auto model = glm::rotate(glm::mat4(1.0f),  (i / 100.0f) * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));

    chaletModel.setObjectMatrix(model);

    mDevice.startFrame();


    mDevice.draw(chaletModel.getGeometry());
    mDevice.addSpotLight(proj);

    mDevice.endFrame();
    mDevice.swap();
  }
}
