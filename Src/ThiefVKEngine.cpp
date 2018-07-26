#include "ThiefVKEngine.hpp"

#include "ThiefVKDevice.hpp"
#include "ThiefVKInstance.hpp"

#include <glm/gtc/matrix_transform.hpp>

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

  for(int i = 0; i < 100; i++) {
	     Device.startFrame();

        auto model = glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
        auto view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
        auto proj = glm::perspective(glm::radians(45.0f), 500 / (float) 500, 0.1f, 10.0f);

        //proj[1][1] *= -1; // to map from gl to vulkan space

       geometry geom{std::vector<Vertex>{    Vertex{{-0.5f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
                          Vertex{{0.5f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}},
                          Vertex{{0.5f, 0.5f, 0.0f},  {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
                          Vertex{{0.5f, 0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}},
                          Vertex{{-0.5f, 0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
                          Vertex{{-0.5f, -0.5f, 0.0f},  {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},},
                          model,
                          view,
                          proj,
                          "./statue.jpg"};

      Device.draw(geom);


       Device.endFrame();
       Device.swap();
  }
}
