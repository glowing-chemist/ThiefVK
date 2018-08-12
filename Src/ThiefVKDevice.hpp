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
#include "ThiefVKBufferManager.hpp"
#include "ThiefVKDescriptorManager.hpp"
#include "ThiefVKVertex.hpp"

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

    vk::Image albedoImage;
    vk::ImageView albedoImageView;
    Allocation albedoImageMemory;
};

struct ThiefVKRenderPasses{
    std::vector<vk::AttachmentDescription> attatchments;
    vk::SubpassDescription colourPass;
    vk::SubpassDescription normalsPass;
    vk::SubpassDescription albedoPass;
    vk::SubpassDescription compositePass;

    vk::RenderPass RenderPass;
};


struct perFrameResources {
	vk::Fence frameFinished;

    size_t submissionID;
	vk::CommandBuffer flushCommandBuffer;

	vk::Semaphore swapChainImageAvailable;
	vk::Semaphore imageRendered;

    std::vector<ThiefVKBuffer> stagingBuffers;
    std::vector<ThiefVKImage> textureImages;
    std::vector<vk::ImageView> textureImageViews;

	vk::CommandBuffer primaryCmdBuffer;
	vk::CommandBuffer colourCmdBuffer;
	vk::CommandBuffer albedoCmdBuffer;
	vk::CommandBuffer normalsCmdBuffer;
    vk::CommandBuffer compositeCmdBuffer;

    ThiefVKBuffer vertexBuffer;
    ThiefVKBuffer indexBuffer;
    ThiefVKBuffer uniformBuffer;
    ThiefVKBuffer spotLightBuffer; 

    std::vector<ThiefVKDescriptorSet> DescSets;
};

struct geometry;

class ThiefVKDevice {
public:
    explicit ThiefVKDevice(std::pair<vk::PhysicalDevice, vk::Device>, vk::SurfaceKHR, GLFWwindow*);
    ~ThiefVKDevice();

    std::pair<vk::PhysicalDevice*, vk::Device*> getDeviceHandles();
	vk::PhysicalDevice* getPhysicalDevice() { return &mPhysDev;  }
	vk::Device* getLogicalDevice() { return &mDevice; }

    void addSpotLight(glm::mat4&);

	void transitionImageLayout(vk::Image& image, vk::ImageLayout oldLayout, vk::ImageLayout newLayout);
	void CopybufferToImage(vk::Buffer& srcBuffer, vk::Image& dstImage, uint32_t width, uint32_t height);
	void copyBuffers(vk::Buffer& SrcBuffer, vk::Buffer& DstBuffer, vk::DeviceSize size);

	ThiefVKMemoryManager*	getMemoryManager() { return &MemoryManager; }
	ThiefVKDescriptorManager* getDescriptorManager() { return &DescriptorManager;  }

	void startFrame();
	void draw(const geometry& geom);
	void endFrame();
	void swap();

    ThiefVKImage createImage(vk::Format format, vk::ImageUsageFlags usage, const uint32_t width, const uint32_t height);
    void destroyImage(ThiefVKImage& image);

	ThiefVKBuffer createBuffer(const vk::BufferUsageFlags usage, const uint32_t size);
	void destroyBuffer(ThiefVKBuffer& buffer);

    ThiefVKImage createTexture(const std::string&);

    vk::Fence createFence();
    void destroyFence(vk::Fence&);

    vk::Sampler createSampler();
    void destroySampler(vk::Sampler&);

    vk::DescriptorPool createDescriptorPool();
    void destroyDescriptorPool(vk::DescriptorPool&);

    void createDeferedRenderTargetImageViews();
    void createRenderPasses();
    void createFrameBuffers();
    void createCommandPools();
    void createDescriptorPools();
    void createSemaphores();

    ThiefVKDescriptorSetDescription getDescriptorSetDescription(const ShaderName);

private:
    // private funcs
    void DestroyAllImageTextures();
    void DestroyImageView(vk::ImageView& view);
    void DestroyImage(vk::Image&, Allocation);

    void DestroyFrameBuffers();

    vk::CommandBuffer beginSingleUseGraphicsCommandBuffer();
    void              endSingleUseGraphicsCommandBuffer(vk::CommandBuffer);

	vk::CommandBuffer&  startRecordingColourCmdBuffer();
	vk::CommandBuffer&  startRecordingAlbedoCmdBuffer();
	vk::CommandBuffer&  startRecordingNormalsCmdBuffer();
	vk::CommandBuffer&  startRecordingCompositeCmdBuffer();

    void renderFrame();
    void startFrameInternal();
    void endFrameInternal();

    void destroyPerFrameResources(perFrameResources&);

    // private variables
    vk::PhysicalDevice mPhysDev;
    vk::Device mDevice;
    size_t finishedSubmissionID; // to keep track of resources such as staging buffers and command buffers that are no
                                 // longer needed and can be freed.
    size_t currentFrameBufferIndex;

    ThiefVKPipelineManager pipelineManager;

    ThiefVKMemoryManager MemoryManager;

	ThiefVKBufferManager<glm::mat4> mUniformBufferManager;
	ThiefVKBufferManager<Vertex>	mVertexBufferManager;
    ThiefVKBufferManager<uint32_t>  mIndexBufferManager;
    ThiefVKBufferManager<glm::mat4> mSpotLightBufferManager;

	ThiefVKDescriptorManager DescriptorManager;

	std::map<std::string, ThiefVKImage> mTextureCache;

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

    std::vector<perFrameResources> frameResources;

    std::vector<ThiefVKImageTextutres> deferedTextures; // have one per frameBuffer/swapChain images
    std::vector<vk::Framebuffer> frameBuffers;

    std::vector<glm::mat4> spotLights;
};

#endif
