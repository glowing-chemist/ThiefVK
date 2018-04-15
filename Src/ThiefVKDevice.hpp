#ifndef THIEFVKDEVICE_HPP
#define THIEFVKDEVICE_HPP

// system includes
#include <vulkan/vulkan.hpp>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>


//local includes
#include "ThiefVKSwapChain.hpp"
#include "ThiefVKMemoryManager.hpp"
#include "ThiefVKPipeLineManager.hpp"

// std library includes
#include <array>
#include <vector>
#include <string>
#include <tuple>


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
    void createCommandPools();
    void createVertexBuffer();
    void createCommandBuffers();
    void createSemaphores();

private:
    // private funcs
    void DestroyAllImageTextures();
    void DestroyImageView(vk::ImageView& view);
    void DestroyImage(vk::Image&, Allocation);

    void DestroyFrameBuffers();

    vk::CommandBuffer beginSingleUseGraphicsCommandBuffer();
    void              endSingleUseGraphicsCommandBuffer(vk::CommandBuffer);

    void transitionImageLayout(vk::Image image, vk::Format format, vk::ImageLayout oldLayout, vk::ImageLayout newLayout);
    void CopybufferToImage(vk::Buffer srcBuffer, vk::Image dstImage, uint32_t width, uint32_t height);
    void copyBuffers(vk::Buffer SrcBuffer, vk::Buffer DstBuffer, vk::DeviceSize size);

    // private variables
    vk::PhysicalDevice mPhysDev;
    vk::Device mDevice;

    ThiefVKPipelineManager pipelineManager;

    ThiefVKMemoryManager MemoryManager;

    vk::SurfaceKHR mWindowSurface;
    GLFWwindow* mWindow;

    vk::Queue mGraphicsQueue;
    vk::Queue mPresentQueue;
    vk::Queue mComputeQueue;

    ThiefVKSwapChain mSwapChain;

    ThiefVKRenderPasses mRenderPasses;

    vk::CommandPool computeCommandPool;
    std::vector<vk::CommandBuffer> currentComputeCommandBuffers;
    std::vector<vk::CommandBuffer> freeComputeCommanBuffers;

    vk::CommandPool graphicsCommandPool;
    std::vector<vk::CommandBuffer> primaryCommandBuffers;
	std::vector<vk::CommandBuffer> secondaryCommandBuffers;
    std::vector<vk::CommandBuffer> freePrimaryCommanBuffers;
	std::vector<vk::CommandBuffer> freeSecondaryCommanBuffers;

    vk::CommandBuffer flushCommandBuffer;

    vk::Buffer vertexBuffer;
    Allocation vertexBufferMemory;

    std::vector<ThiefVKImageTextutres> deferedTextures; // have one per frameBuffer/swapChain images
    std::vector<vk::Framebuffer> frameBuffers;

    std::vector<Vertex> verticies;
    std::vector<modelInfo> vertexModelInfo;

    std::vector<spotLight> spotLights;
};

#endif
