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
};

struct ThiefVKRenderPasses{
    std::vector<vk::AttachmentDescription> attatchments;
    vk::SubpassDescription colourPass;
    vk::SubpassDescription depthPass;
    vk::SubpassDescription normalsPass;
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

	vk::CommandBuffer primaryCmdBuffer;
	vk::CommandBuffer colourCmdBuffer;
	vk::CommandBuffer depthCmdBuffer;
	vk::CommandBuffer normalsCmdBuffer;

    ThiefVKBuffer vertexBuffer;

    ThiefVKBuffer indexBuffer;

    ThiefVKBuffer subpassUniformBuffer;
    ThiefVKBuffer compositeUniformBuffer;
    std::vector<vk::Sampler> samplers;
    std::vector<vk::Sampler> usedSamplers;
    std::array<vk::DescriptorSet, 2> subpassDescriptorSets;
};


struct geometry {
	std::vector<Vertex> verticies;

	glm::mat4 object; // These will be pushed to a uniform buffer
	glm::mat4 camera;
	glm::mat4 world;

	std::string texturePath;
};

struct SceneInfo {
	std::vector<Vertex> vertexData;

	std::vector<geometry> geometries;
};

class ThiefVKDevice {
public:
    explicit ThiefVKDevice(std::pair<vk::PhysicalDevice, vk::Device>, vk::SurfaceKHR, GLFWwindow*);
    ~ThiefVKDevice();

    std::pair<vk::PhysicalDevice*, vk::Device*> getDeviceHandles();

	void drawScene(SceneInfo&);

    void addSpotLight(glm::mat4&);

	void transitionImageLayout(vk::Image& image, vk::ImageLayout oldLayout, vk::ImageLayout newLayout);
	void CopybufferToImage(vk::Buffer& srcBuffer, vk::Image& dstImage, uint32_t width, uint32_t height);
	void copyBuffers(vk::Buffer& SrcBuffer, vk::Buffer& DstBuffer, vk::DeviceSize size);

	ThiefVKMemoryManager*	getMemoryManager() { return &MemoryManager; }

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

    void createDeferedRenderTargetImageViews();
    void createRenderPasses();
    void createFrameBuffers();
    void createCommandPools();
    void createDescriptorPools();
    void createSemaphores();
    std::vector<vk::DescriptorSet> createDescriptorSets();

private:
    // private funcs
    void DestroyAllImageTextures();
    void DestroyImageView(vk::ImageView& view);
    void DestroyImage(vk::Image&, Allocation);

    void DestroyFrameBuffers();

    vk::CommandBuffer beginSingleUseGraphicsCommandBuffer();
    void              endSingleUseGraphicsCommandBuffer(vk::CommandBuffer);

	vk::CommandBuffer&  startRecordingColourCmdBuffer();
	vk::CommandBuffer&  startRecordingDepthCmdBuffer();
	vk::CommandBuffer&  startRecordingNormalsCmdBuffer();
	vk::CommandBuffer&  compositeCmdBufferBindPipeline();

    void renderFrame();

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

    vk::DescriptorPool mDescPool ;

    std::vector<ThiefVKImageTextutres> deferedTextures; // have one per frameBuffer/swapChain images
    std::vector<vk::Framebuffer> frameBuffers;

    std::vector<glm::mat4> spotLights;

    std::vector<ThiefVKImage> mPreFrameTextures;
    std::vector<ThiefVKBuffer> mPreFrameStagingBuffers;
};

#endif
