#ifndef THIEFVKDEVICE_HPP
#define THIEFVKDEVICE_HPP

// system includes
#include <vulkan/vulkan.hpp>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>


//local includes
#include "ThiefVKSwapChain.hpp"

// std library includes
#include <array>
#include <vector>
#include <string>
#include <tuple>

struct ThiefVKPipeLines {
    vk::Pipeline colourPipeline;
    vk::Pipeline normalsPipeline;
    vk::Pipeline lighPipeline;
    vk::Pipeline finalPipline;
};


struct ThiefVKImageTextutres {
    vk::Image colourImage;
    vk::ImageView colourImageView;
    vk::Framebuffer colourFrameBuffer;
    vk::DeviceMemory colourMemory;

    vk::Image depthImage;
    vk::ImageView depthImageView;
    vk::Framebuffer depthImageFrameBuffer;
    vk::DeviceMemory depthMemory;

    vk::Image normalsImage;
    vk::ImageView normalsImageView;
    vk::Framebuffer normalsImageFrameBuffer;
    vk::DeviceMemory normalsMemory;

    std::vector<vk::Image> lighImages;
    std::vector<vk::ImageView> lighImageViews;
    std::vector<vk::Framebuffer> lightImageFrameBufers;
    std::vector<vk::DeviceMemory> lightImageMemory;
};

struct ThiefVKRenderPasses{
    std::vector<vk::AttachmentDescription> attatchments;
    vk::SubpassDescription colourPass;
    vk::SubpassDescription depthPass;
    vk::SubpassDescription normalsPass;
    vk::SubpassDescription compositePass;

    vk::RenderPass RenderPass;
};

struct Vertex { // vertex struct representing vertex positions and texture coordinates
    glm::vec3 pos;
    glm::vec2 tex;

    static vk::VertexInputBindingDescription getBindingDesc();

    static std::array<vk::VertexInputAttributeDescription, 2> getAttribDesc();
};

struct modelInfo {
    size_t modelBeginOffset;
    std::string modelName;
};


struct spotLight {
    glm::vec3 pos;
    glm::mat4 view;
};

class ThiefVKDevice {

public:
    explicit ThiefVKDevice(std::pair<vk::PhysicalDevice, vk::Device>, vk::SurfaceKHR, GLFWwindow*);
    ~ThiefVKDevice();

    std::pair<vk::PhysicalDevice*, vk::Device*> getDeviceHandles();

    void addModelVerticies(std::vector<Vertex>&, std::string);
    void addPointLight(glm::vec3&);
    void addSpotLight(spotLight&);

    void drawAndPresent();

    void createDeferedRenderTargetImageViews();
    void createRenderPasses();
    void createGraphicsPipelines();
    void createFrameBuffers();
    void createCommandPool();
    void createVertexBuffer();
    void createCommandBuffers();
    void createSemaphores();

private:
    // private funcs

    // private variables
    vk::PhysicalDevice mPhysDev;
    vk::Device mDevice;

    vk::SurfaceKHR mWindowSurface;
    GLFWwindow* mWindow;

    vk::Queue mGraphicsQueue;
    vk::Queue mPresentQueue;

    ThiefVKSwapChain mSwapChain;

    ThiefVKRenderPasses mRenderPasses;

    vk::PipelineLayout pipeLineLayout;
    vk::PipelineLayout finalPipLineLayout;
    ThiefVKPipeLines PipeLines;

    vk::CommandPool graphicsCommandPool;
    std::vector<vk::CommandBuffer> currentFrameCommandBuffers;
    std::vector<vk::CommandBuffer> freeCommanBuffers;

    vk::Buffer vertexBuffer;
    vk::DeviceMemory vertexBufferMemory;

    ThiefVKImageTextutres deferedTextures;

    std::vector<Vertex> verticies;
    std::vector<modelInfo> vertexModelInfo;

    std::vector<spotLight> spotLights;
};

#endif
