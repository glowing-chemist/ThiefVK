#ifndef THIEFVKDEVICE_HPP
#define THIEFVKDEVICE_HPP

// system includes
#include <vulkan/vulkan.hpp>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>


//local includes
#include "ThiefVKSwapChain.hpp"
#include "ThiefVKMemoryManager.hpp"

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
    Allocation colourImageMemory;

    vk::Image depthImage;
    vk::ImageView depthImageView;
    Allocation depthImageMemory;

    vk::Image normalsImage;
    vk::ImageView normalsImageView;
    Allocation normalsImageMemory;

    std::vector<vk::Image> lighImages;
    std::vector<vk::ImageView> lighImageViews;
    std::vector<Allocation> lightImageMemory;
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
    void addSpotLight(spotLight&);

    std::pair<vk::Image, Allocation> createColourImage(const unsigned int width, const unsigned int height);
    std::pair<vk::Image, Allocation> createDepthImage(const unsigned int width, const unsigned int height);
    std::pair<vk::Image, Allocation> createNormalsImage(const unsigned int width, const unsigned int height);

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
    void DestroyAllImageTextures();
    void DestroyImageView(vk::ImageView& view);
    void DestroyImage(vk::Image&, Allocation);

    void DestroyFrameBuffers();

    // private variables
    vk::PhysicalDevice mPhysDev;
    vk::Device mDevice;

    ThiefVKMemoryManager MemoryManager;

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

    std::vector<ThiefVKImageTextutres> deferedTextures; // have one per frameBuffer/swapChain images
    std::vector<vk::Framebuffer> frameBuffers;

    std::vector<Vertex> verticies;
    std::vector<modelInfo> vertexModelInfo;

    std::vector<spotLight> spotLights;
};

#endif
