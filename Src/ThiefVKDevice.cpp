#include "ThiefVKDevice.hpp"
#include "ThiefVKInstance.hpp"

#include "stb_image.h"

#include <array>
#include <iostream>
#include <limits>


// ThiefVKDeviceMemberFunctions

ThiefVKDevice::ThiefVKDevice(std::pair<vk::PhysicalDevice, vk::Device> Devices, vk::SurfaceKHR surface, GLFWwindow * window) :
    mPhysDev{std::get<0>(Devices)}, 
	mDevice{std::get<1>(Devices)}, 
    currentFrameBufferIndex{0},
	pipelineManager{mDevice},
	MemoryManager{&mPhysDev, &mDevice},
	mUniformBufferManager{*this, vk::BufferUsageFlagBits::eUniformBuffer},
	mVertexBufferManager{*this, vk::BufferUsageFlagBits::eVertexBuffer},
	mWindowSurface{surface}, 
	mWindow{window}, 
	mSwapChain{mDevice, mPhysDev, surface, window}
{
    const QueueIndicies queueIndices = getAvailableQueues(mWindowSurface, mPhysDev);

    mGraphicsQueue = mDevice.getQueue(queueIndices.GraphicsQueueIndex, 0);
    mPresentQueue  = mDevice.getQueue(queueIndices.PresentQueueIndex, 0);
    mComputeQueue  = mDevice.getQueue(queueIndices.ComputeQueueIndex, 0);
}


ThiefVKDevice::~ThiefVKDevice() {
    mDevice.waitIdle();

    for(auto& resource : frameResources) {
        destroyPerFrameResources(resource);
    }
    DestroyFrameBuffers();
    DestroyAllImageTextures();
    pipelineManager.Destroy();
    MemoryManager.Destroy();
    mSwapChain.destroy(mDevice);
    mDevice.destroyRenderPass(mRenderPasses.RenderPass);
    mDevice.destroyCommandPool(graphicsCommandPool);
    mDevice.destroyCommandPool(computeCommandPool);
    mDevice.destroy();
}


std::pair<vk::PhysicalDevice*, vk::Device*> ThiefVKDevice::getDeviceHandles()  {
        return std::pair<vk::PhysicalDevice*, vk::Device*>(&mPhysDev, &mDevice);
}


void ThiefVKDevice::startFrame() {
    currentFrameBufferIndex = mSwapChain.getNextImageIndex(mDevice, frameResources[currentFrameBufferIndex].swapChainImageAvailable);
}


void ThiefVKDevice::endFrame() {
    startFrameInternal();

    auto& resources = frameResources[currentFrameBufferIndex];
    resources.compositeCmdBuffer.draw(3,1,0,0);

    endFrameInternal();
}


void ThiefVKDevice::startFrameInternal() {
	mDevice.waitForFences(frameResources[currentFrameBufferIndex].frameFinished, true, std::numeric_limits<uint64_t>::max());
	mDevice.resetFences(1, &frameResources[currentFrameBufferIndex].frameFinished);

	if(frameResources[currentFrameBufferIndex].primaryCmdBuffer == vk::CommandBuffer(nullptr)) {
		// Only allocate the command buffers if this will be there first use.
		vk::CommandBufferAllocateInfo primaryCmdBufferAllocInfo;
		primaryCmdBufferAllocInfo.setLevel(vk::CommandBufferLevel::ePrimary);
		primaryCmdBufferAllocInfo.setCommandPool(graphicsCommandPool);
		primaryCmdBufferAllocInfo.setCommandBufferCount(2);

		std::vector<vk::CommandBuffer> primaryCmdBuffers = mDevice.allocateCommandBuffers(primaryCmdBufferAllocInfo);

		vk::CommandBufferAllocateInfo secondaryCmdBufferAllocInfo;
		secondaryCmdBufferAllocInfo.setLevel(vk::CommandBufferLevel::eSecondary);
		secondaryCmdBufferAllocInfo.setCommandPool(graphicsCommandPool);
		secondaryCmdBufferAllocInfo.setCommandBufferCount(4);

		std::vector<vk::CommandBuffer> secondaryCmdBuffers = mDevice.allocateCommandBuffers(secondaryCmdBufferAllocInfo);

		// Set the initial cmd Buffers.
		frameResources[currentFrameBufferIndex].primaryCmdBuffer			= primaryCmdBuffers[0];
        frameResources[currentFrameBufferIndex].flushCommandBuffer       = primaryCmdBuffers[1];
		frameResources[currentFrameBufferIndex].colourCmdBuffer			= secondaryCmdBuffers[0];
		frameResources[currentFrameBufferIndex].depthCmdBuffer			= secondaryCmdBuffers[1];
		frameResources[currentFrameBufferIndex].normalsCmdBuffer			= secondaryCmdBuffers[2];
        frameResources[currentFrameBufferIndex].compositeCmdBuffer            = secondaryCmdBuffers[3];

        // create the descriptor Pool
        frameResources[currentFrameBufferIndex].descPool = createDescriptorPool();

	} else { // Otherwise just reset them
		frameResources[currentFrameBufferIndex].primaryCmdBuffer.reset(vk::CommandBufferResetFlags());
        frameResources[currentFrameBufferIndex].flushCommandBuffer.reset(vk::CommandBufferResetFlags());
		frameResources[currentFrameBufferIndex].colourCmdBuffer.reset(vk::CommandBufferResetFlags());
		frameResources[currentFrameBufferIndex].depthCmdBuffer.reset(vk::CommandBufferResetFlags());
		frameResources[currentFrameBufferIndex].normalsCmdBuffer.reset(vk::CommandBufferResetFlags());

        mDevice.resetDescriptorPool(frameResources[currentFrameBufferIndex].descPool, vk::DescriptorPoolResetFlags());
	}

	frameResources[currentFrameBufferIndex].submissionID				= finishedSubmissionID; // set the minimum we need to start recording command buffers.

    vk::CommandBufferBeginInfo primaryBeginInfo{};
    frameResources[currentFrameBufferIndex].primaryCmdBuffer.begin(primaryBeginInfo);

	// start the render pass so that we can begin recording in to the command buffers
	vk::RenderPassBeginInfo renderPassBegin{};
	renderPassBegin.framebuffer = frameBuffers[currentFrameBufferIndex];
	renderPassBegin.renderPass = mRenderPasses.RenderPass;
	vk::ClearValue colour[4]  = {vk::ClearValue{0.0f}, vk::ClearValue{0.0f}, vk::ClearValue{0.0f}, vk::ClearValue{0.0f}};
    renderPassBegin.setClearValueCount(4);
	renderPassBegin.setPClearValues(colour);
	renderPassBegin.setRenderArea(vk::Rect2D{ { 0, 0 },
		{ static_cast<uint32_t>(mSwapChain.getSwapChainImageHeight()), static_cast<uint32_t>(mSwapChain.getSwapChainImageWidth()) } });

	// Begin the render pass
	frameResources[currentFrameBufferIndex].primaryCmdBuffer.beginRenderPass(renderPassBegin, vk::SubpassContents::eSecondaryCommandBuffers);

	startRecordingColourCmdBuffer();
	startRecordingDepthCmdBuffer();
	startRecordingNormalsCmdBuffer();
    startRecordingCompositeCmdBuffer();

    vk::CommandBufferBeginInfo beginInfo{};
    frameResources[currentFrameBufferIndex].flushCommandBuffer.begin(beginInfo);
}


// Just gather all the state we need here, then call Start RenderScene.
void ThiefVKDevice::draw(const geometry& geom) {
    mVertexBufferManager.addBufferElements(geom.verticies);
    mUniformBufferManager.addBufferElements({geom.object, geom.camera, geom.world});

    auto image = createTexture(geom.texturePath); 
    frameResources[currentFrameBufferIndex].textureImages.push_back(image);
}


void ThiefVKDevice::endFrameInternal() {
	finishedSubmissionID++;

	perFrameResources& resources = frameResources[currentFrameBufferIndex];
	vk::CommandBuffer& primaryCmdBuffer = resources.primaryCmdBuffer;

	// end recording in to these as we're
	resources.colourCmdBuffer.end();
	resources.depthCmdBuffer.end();
	resources.normalsCmdBuffer.end();
    resources.compositeCmdBuffer.end();

	// Execute the secondary cmd buffers in the primary
	primaryCmdBuffer.executeCommands(1, &resources.colourCmdBuffer);
	primaryCmdBuffer.nextSubpass(vk::SubpassContents::eSecondaryCommandBuffers);
	primaryCmdBuffer.executeCommands(1, &resources.depthCmdBuffer);
	primaryCmdBuffer.nextSubpass(vk::SubpassContents::eSecondaryCommandBuffers);
	primaryCmdBuffer.executeCommands(1, &resources.normalsCmdBuffer);
	primaryCmdBuffer.nextSubpass(vk::SubpassContents::eSecondaryCommandBuffers);
    primaryCmdBuffer.executeCommands(1, &resources.compositeCmdBuffer);

	primaryCmdBuffer.endRenderPass();
	primaryCmdBuffer.end();

    transitionImageLayout(deferedTextures[currentFrameBufferIndex].colourImage, vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal);
    transitionImageLayout(deferedTextures[currentFrameBufferIndex].depthImage, vk::ImageLayout::eUndefined, vk::ImageLayout::eDepthStencilAttachmentOptimal);
    transitionImageLayout(deferedTextures[currentFrameBufferIndex].normalsImage, vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal);
    transitionImageLayout(mSwapChain.getImage(currentFrameBufferIndex), vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal);

    resources.flushCommandBuffer.end();

	// Submit everything for this frame
	std::array<vk::CommandBuffer, 2> cmdBuffers{resources.flushCommandBuffer, resources.primaryCmdBuffer};

	vk::SubmitInfo submitInfo{};
	submitInfo.setCommandBufferCount(cmdBuffers.size());
	submitInfo.setPCommandBuffers(cmdBuffers.data());
	submitInfo.setWaitSemaphoreCount(1);
	submitInfo.setPWaitSemaphores(&resources.swapChainImageAvailable);
    submitInfo.setPSignalSemaphores(&resources.imageRendered);
    submitInfo.setSignalSemaphoreCount(1);
	auto const waitStage = vk::PipelineStageFlags(vk::PipelineStageFlagBits::eTransfer);
	submitInfo.setPWaitDstStageMask(&waitStage);

	mGraphicsQueue.submit(submitInfo, resources.frameFinished);
}


void ThiefVKDevice::swap() {
	mSwapChain.present(mPresentQueue, frameResources[currentFrameBufferIndex].imageRendered);

    currentFrameBufferIndex = (mSwapChain.getCurrentImageIndex() + 1) % mSwapChain.getNumberOfSwapChainImages();
}


ThiefVKImage ThiefVKDevice::createImage(vk::Format format, vk::ImageUsageFlags usage, const uint32_t width, const uint32_t height) {
    vk::ImageCreateInfo imageInfo{};
    imageInfo.setExtent({width, height, 1});
    imageInfo.setFormat(format);
    imageInfo.setInitialLayout(vk::ImageLayout::eUndefined);
    imageInfo.setImageType(vk::ImageType::e2D);
    imageInfo.setMipLevels(1);
    imageInfo.setArrayLayers(1);
    imageInfo.setTiling(vk::ImageTiling::eOptimal);
    imageInfo.setUsage(usage);

    vk::Image image = mDevice.createImage(imageInfo);
    vk::MemoryRequirements imageMemRequirments = mDevice.getImageMemoryRequirements(image);

    Allocation imageMemory = MemoryManager.Allocate(imageMemRequirments.size, imageMemRequirments.alignment, false); // we don't need to be able to map the image
    MemoryManager.BindImage(image, imageMemory);

    return {image, imageMemory};
}


void ThiefVKDevice::destroyImage(ThiefVKImage& image) {
    MemoryManager.Free(image.mImageMemory);

    mDevice.destroyImage(image.mImage);
}


ThiefVKBuffer ThiefVKDevice::createBuffer(const vk::BufferUsageFlags usage, const uint32_t size) {
	vk::BufferCreateInfo bufferInfo{};
	bufferInfo.setSize(size);
	bufferInfo.setUsage(usage);
	bufferInfo.setSharingMode(vk::SharingMode::eExclusive);

	vk::Buffer buffer = mDevice.createBuffer(bufferInfo);
	vk::MemoryRequirements bufferMemReqs = mDevice.getBufferMemoryRequirements(buffer);

	Allocation bufferMem = MemoryManager.Allocate(size, bufferMemReqs.alignment,
												  static_cast<uint32_t>(usage) & static_cast<uint32_t>(vk::BufferUsageFlagBits::eUniformBuffer) ||
												  static_cast<uint32_t>(usage) & static_cast<uint32_t>(vk::BufferUsageFlagBits::eTransferSrc));

	MemoryManager.BindBuffer(buffer, bufferMem);

	return {buffer, bufferMem};
}


void ThiefVKDevice::destroyBuffer(ThiefVKBuffer& buffer) {
	MemoryManager.Free(buffer.mBufferMemory);

	mDevice.destroyBuffer(buffer.mBuffer);
}


void ThiefVKDevice::DestroyImageView(vk::ImageView& view) {
    mDevice.destroyImageView(view);
}


void ThiefVKDevice::DestroyImage(vk::Image& image, Allocation imageMem) {
    mDevice.destroyImage(image);
    MemoryManager.Free(imageMem);
}


void ThiefVKDevice::DestroyAllImageTextures() {
    for(auto& images : deferedTextures) {
        DestroyImageView(images.colourImageView);
        DestroyImage(images.colourImage, images.colourImageMemory);
        DestroyImageView(images.depthImageView);
        DestroyImage(images.depthImage, images.depthImageMemory);
        DestroyImageView(images.normalsImageView);
        DestroyImage(images.normalsImage, images.normalsImageMemory);
    }
}


ThiefVKImage ThiefVKDevice::createTexture(const std::string& path) {
        int texWidth, texHeight, texChannels;
        stbi_uc* pixels = stbi_load(path.data(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
        vk::DeviceSize imageSize = texWidth * texHeight * 4;

        ThiefVKBuffer stagingBuffer = createBuffer(vk::BufferUsageFlagBits::eTransferSrc, imageSize);

        void* data = MemoryManager.MapAllocation(stagingBuffer.mBufferMemory);
        memcpy(data, pixels, imageSize);
        MemoryManager.UnMapAllocation(stagingBuffer.mBufferMemory);

        stbi_image_free(pixels);

        ThiefVKImage textureImage = createImage(vk::Format::eR8G8B8A8Unorm
                                                ,vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled, 
                                                texWidth, texHeight);

        transitionImageLayout(textureImage.mImage, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);
        CopybufferToImage(stagingBuffer.mBuffer, textureImage.mImage, texWidth, texHeight);

        transitionImageLayout(textureImage.mImage, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal); // we will sample from it next so transition the layout

        frameResources[currentFrameBufferIndex].stagingBuffers.push_back(stagingBuffer);

        return textureImage;
}


vk::Fence ThiefVKDevice::createFence() {
    vk::FenceCreateInfo info{};
    return mDevice.createFence(info);
}


void ThiefVKDevice::destroyFence(vk::Fence& fence) {
    mDevice.destroyFence(fence);
}


void ThiefVKDevice::createDeferedRenderTargetImageViews() {
    ThiefVKImageTextutres Result{};

    for(unsigned int swapImageCount = 0; swapImageCount < mSwapChain.getNumberOfSwapChainImages(); ++swapImageCount) {
        auto [colourImage, colourMemory]   = createImage(vk::Format::eR8G8B8A8Srgb, 
                                                         vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eInputAttachment, 
                                                         mSwapChain.getSwapChainImageWidth(), mSwapChain.getSwapChainImageHeight());

        auto [depthImage , depthMemory]    = createImage(vk::Format::eD32Sfloat,
                                                         vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eInputAttachment,
                                                         mSwapChain.getSwapChainImageWidth(), mSwapChain.getSwapChainImageHeight());

        auto [normalsImage, normalsMemory] = createImage(vk::Format::eR8G8B8A8Sint,
                                                         vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eInputAttachment,
                                                         mSwapChain.getSwapChainImageWidth(), mSwapChain.getSwapChainImageHeight());


                
        vk::ImageViewCreateInfo colourViewInfo{};
        colourViewInfo.setImage(colourImage);
        colourViewInfo.setViewType(vk::ImageViewType::e2D);
        colourViewInfo.setFormat(vk::Format::eR8G8B8A8Srgb);
        colourViewInfo.setComponents(vk::ComponentMapping()); // set swizzle components to identity
        colourViewInfo.setSubresourceRange(vk::ImageSubresourceRange{vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1});

        vk::ImageViewCreateInfo depthViewInfo{};
        depthViewInfo.setImage(depthImage);
        depthViewInfo.setViewType(vk::ImageViewType::e2D);
        depthViewInfo.setFormat(vk::Format::eD32Sfloat);
        depthViewInfo.setComponents(vk::ComponentMapping());
        depthViewInfo.setSubresourceRange(vk::ImageSubresourceRange{vk::ImageAspectFlagBits::eDepth, 0, 1, 0, 1});

        vk::ImageViewCreateInfo normalsViewInfo{};
        normalsViewInfo.setImage(normalsImage);
        normalsViewInfo.setViewType(vk::ImageViewType::e2D);
        normalsViewInfo.setFormat(vk::Format::eR8G8B8A8Sint);
        normalsViewInfo.setComponents(vk::ComponentMapping());
        normalsViewInfo.setSubresourceRange(vk::ImageSubresourceRange{vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1});


        vk::ImageView colourImageView  = mDevice.createImageView(colourViewInfo);
        vk::ImageView depthImageView   = mDevice.createImageView(depthViewInfo);
        vk::ImageView normalsImageView = mDevice.createImageView(normalsViewInfo);

        Result.colourImage          = colourImage;
        Result.colourImageView      = colourImageView;
        Result.colourImageMemory    = colourMemory;

        Result.depthImage           = depthImage;
        Result.depthImageView       = depthImageView;
        Result.depthImageMemory     = depthMemory;

        Result.normalsImage         = normalsImage;
        Result.normalsImageView     = normalsImageView;
        Result.normalsImageMemory   = normalsMemory;

        deferedTextures.push_back(Result);
    }
}


void ThiefVKDevice::createRenderPasses() {

    // Specify all attachments used in all subrenderpasses
    vk::AttachmentDescription colourPassAttachment{}; 
    colourPassAttachment.setFormat(vk::Format::eR8G8B8A8Srgb);
    colourPassAttachment.setLoadOp(vk::AttachmentLoadOp::eDontCare); // we are going to overwrite all pixles
    colourPassAttachment.setStoreOp(vk::AttachmentStoreOp::eDontCare);
    colourPassAttachment.setStencilLoadOp(vk::AttachmentLoadOp::eDontCare);
    colourPassAttachment.setStencilStoreOp(vk::AttachmentStoreOp::eDontCare);
    colourPassAttachment.setInitialLayout(vk::ImageLayout::eUndefined);
    colourPassAttachment.setFinalLayout(vk::ImageLayout::eColorAttachmentOptimal);

    vk::AttachmentDescription depthPassAttachment{};
    depthPassAttachment.setFormat(vk::Format::eD32Sfloat); // store in each pixel a 32bit depth value
    depthPassAttachment.setLoadOp(vk::AttachmentLoadOp::eDontCare); // we are going to overwrite all pixles
    depthPassAttachment.setStoreOp(vk::AttachmentStoreOp::eDontCare);
    depthPassAttachment.setStencilLoadOp(vk::AttachmentLoadOp::eDontCare);
    depthPassAttachment.setStencilStoreOp(vk::AttachmentStoreOp::eDontCare);
    depthPassAttachment.setStencilLoadOp(vk::AttachmentLoadOp::eDontCare);
    depthPassAttachment.setStencilStoreOp(vk::AttachmentStoreOp::eDontCare);
    depthPassAttachment.setInitialLayout(vk::ImageLayout::eUndefined); // write in a subpass then read in a subsequent one
    depthPassAttachment.setFinalLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);

    vk::AttachmentDescription normalsPassAttachment{};
    normalsPassAttachment.setFormat(vk::Format::eR8G8B8A8Sint);
    normalsPassAttachment.setLoadOp(vk::AttachmentLoadOp::eDontCare);
    normalsPassAttachment.setStoreOp(vk::AttachmentStoreOp::eDontCare);
    normalsPassAttachment.setStencilLoadOp(vk::AttachmentLoadOp::eDontCare);
    normalsPassAttachment.setStencilStoreOp(vk::AttachmentStoreOp::eDontCare);
    normalsPassAttachment.setInitialLayout(vk::ImageLayout::eUndefined);
    normalsPassAttachment.setFinalLayout(vk::ImageLayout::eColorAttachmentOptimal); // these will be used in the subsqeuent light renderpass

    // composite subPasses
    vk::AttachmentDescription swapChainImageAttachment{};
    swapChainImageAttachment.setFormat(mSwapChain.getSwapChainImageFormat());
    swapChainImageAttachment.setLoadOp(vk::AttachmentLoadOp::eClear);
    swapChainImageAttachment.setStoreOp(vk::AttachmentStoreOp::eStore);
    swapChainImageAttachment.setStencilLoadOp(vk::AttachmentLoadOp::eDontCare);
    swapChainImageAttachment.setStencilStoreOp(vk::AttachmentStoreOp::eDontCare);
    swapChainImageAttachment.setInitialLayout(vk::ImageLayout::eColorAttachmentOptimal);
    swapChainImageAttachment.setFinalLayout(vk::ImageLayout::ePresentSrcKHR);



    // specify the subpass descriptions
    std::array<vk::AttachmentReference, 3> colourAttatchmentRefs{vk::AttachmentReference{0, vk::ImageLayout::eColorAttachmentOptimal}, 
                                                                vk::AttachmentReference{2, vk::ImageLayout::eColorAttachmentOptimal}, 
                                                                vk::AttachmentReference{3, vk::ImageLayout::eColorAttachmentOptimal}};

    vk::AttachmentReference depthRef = {1, vk::ImageLayout::eDepthStencilReadOnlyOptimal};

    vk::SubpassDescription colourPassDesc{};
    colourPassDesc.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics);
    colourPassDesc.setColorAttachmentCount(1);
    colourPassDesc.setPColorAttachments(&colourAttatchmentRefs[0]);

    vk::SubpassDescription depthPassDesc{};
    depthPassDesc.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics);
    depthPassDesc.setPDepthStencilAttachment(&depthRef);

    vk::SubpassDescription normalsPassDesc{};
    normalsPassDesc.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics);
    normalsPassDesc.setColorAttachmentCount(1);
    normalsPassDesc.setPColorAttachments(&colourAttatchmentRefs[1]);

    std::array<vk::AttachmentReference, 2> inputAttachments{vk::AttachmentReference{0, vk::ImageLayout::eShaderReadOnlyOptimal}, 
                                                            vk::AttachmentReference{2, vk::ImageLayout::eShaderReadOnlyOptimal}};

    vk::AttachmentReference inputDepthAttatchment{1, vk::ImageLayout::eDepthStencilReadOnlyOptimal};
    vk::AttachmentReference swapChainAttachment{3, vk::ImageLayout::eColorAttachmentOptimal};

    vk::SubpassDescription compositPassDesc{};
    compositPassDesc.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics);
    compositPassDesc.setInputAttachmentCount(inputAttachments.size());
    compositPassDesc.setPInputAttachments(inputAttachments.data());
    compositPassDesc.setPDepthStencilAttachment(&inputDepthAttatchment);
    compositPassDesc.setColorAttachmentCount(1);
    compositPassDesc.setPColorAttachments(&swapChainAttachment);

    mRenderPasses.colourPass    = colourPassDesc;
    mRenderPasses.depthPass     = depthPassDesc;
    mRenderPasses.normalsPass   = normalsPassDesc;


    std::vector<vk::AttachmentDescription> allAttachments{colourPassAttachment, depthPassAttachment, normalsPassAttachment, swapChainImageAttachment};

    mRenderPasses.attatchments = allAttachments;

    // Subpass dependancies
    // The only dependancies are between all the passes and the final pass
    // as only the final passes uses resourecs from the otheres

    vk::SubpassDependency implicitFirstDepen{};
    implicitFirstDepen.setSrcSubpass(VK_SUBPASS_EXTERNAL);
    implicitFirstDepen.setDstSubpass(0);
    implicitFirstDepen.setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);
    implicitFirstDepen.setDstStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);
    implicitFirstDepen.setDstAccessMask(vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite);

    vk::SubpassDependency colourToCompositeDepen{};
    colourToCompositeDepen.setSrcSubpass(0);
    colourToCompositeDepen.setDstSubpass(1);
    colourToCompositeDepen.setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);
    colourToCompositeDepen.setDstStageMask(vk::PipelineStageFlagBits::eFragmentShader);
    colourToCompositeDepen.setSrcAccessMask(vk::AccessFlagBits::eColorAttachmentWrite);
    colourToCompositeDepen.setDstAccessMask(vk::AccessFlagBits::eShaderRead);
    colourToCompositeDepen.setDependencyFlags(vk::DependencyFlagBits::eByRegion);

    vk::SubpassDependency depthToCompositeDepen{};
    depthToCompositeDepen.setSrcSubpass(1);
    depthToCompositeDepen.setDstSubpass(2);
    depthToCompositeDepen.setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);
    depthToCompositeDepen.setDstStageMask(vk::PipelineStageFlagBits::eFragmentShader);
    depthToCompositeDepen.setSrcAccessMask(vk::AccessFlagBits::eColorAttachmentWrite);
    depthToCompositeDepen.setDstAccessMask(vk::AccessFlagBits::eShaderRead);
    depthToCompositeDepen.setDependencyFlags(vk::DependencyFlagBits::eByRegion);

    vk::SubpassDependency normalsToCompositeDepen{};
    normalsToCompositeDepen.setSrcSubpass(2);
    normalsToCompositeDepen.setDstSubpass(3);
    normalsToCompositeDepen.setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);
    normalsToCompositeDepen.setDstStageMask(vk::PipelineStageFlagBits::eFragmentShader);
    normalsToCompositeDepen.setSrcAccessMask(vk::AccessFlagBits::eColorAttachmentWrite);
    normalsToCompositeDepen.setDstAccessMask(vk::AccessFlagBits::eShaderRead);
    normalsToCompositeDepen.setDependencyFlags(vk::DependencyFlagBits::eByRegion);

    std::array<vk::SubpassDescription, 4> allSubpasses{colourPassDesc, depthPassDesc, normalsPassDesc, compositPassDesc};
    std::array<vk::SubpassDependency, 4>  allSubpassDependancies{implicitFirstDepen, colourToCompositeDepen, depthToCompositeDepen, normalsToCompositeDepen};

    vk::RenderPassCreateInfo renderPassInfo{};
    renderPassInfo.setAttachmentCount(allAttachments.size());
    renderPassInfo.setPAttachments(allAttachments.data());
    renderPassInfo.setSubpassCount(allSubpasses.size());
    renderPassInfo.setPSubpasses(allSubpasses.data());
    renderPassInfo.setDependencyCount(allSubpassDependancies.size());
    renderPassInfo.setPDependencies(allSubpassDependancies.data());

    mRenderPasses.RenderPass = mDevice.createRenderPass(renderPassInfo);
}


void ThiefVKDevice::createFrameBuffers() {
    frameBuffers.resize(mSwapChain.getNumberOfSwapChainImages());

    for(uint32_t i = 0; i < mSwapChain.getNumberOfSwapChainImages(); i++) {
        const vk::ImageView& swapChainImage = mSwapChain.getImageView(i);

        std::array<vk::ImageView, 4> frameBufferAttatchments{deferedTextures[i].colourImageView
                                                          , deferedTextures[i].depthImageView
                                                          , deferedTextures[i].normalsImageView
                                                          , swapChainImage };


        vk::FramebufferCreateInfo frameBufferInfo{}; // we need to allocate all our images before trying to fix this
        frameBufferInfo.setRenderPass(mRenderPasses.RenderPass);
        frameBufferInfo.setAttachmentCount(frameBufferAttatchments.size());
        frameBufferInfo.setPAttachments(frameBufferAttatchments.data());
        frameBufferInfo.setWidth(mSwapChain.getSwapChainImageWidth());
        frameBufferInfo.setHeight(mSwapChain.getSwapChainImageHeight());
        frameBufferInfo.setLayers(1);

        frameBuffers[i] = mDevice.createFramebuffer(frameBufferInfo);
    }
    std::cerr << "created " << frameBuffers.size() << " frame buffers \n";

	// Resize the per frame resource vector here so when we go to use it it will be valid to index in to it
	frameResources.resize(frameBuffers.size());
}


void ThiefVKDevice::DestroyFrameBuffers() {
    for(auto& frameBuffer : frameBuffers) {
        mDevice.destroyFramebuffer(frameBuffer);
    }
}


void ThiefVKDevice::createCommandPools() {
    const QueueIndicies queueIndicies = getAvailableQueues(mWindowSurface, mPhysDev);

    vk::CommandPoolCreateInfo graphicsPoolInfo{};
    graphicsPoolInfo.setQueueFamilyIndex(queueIndicies.GraphicsQueueIndex);
	graphicsPoolInfo.setFlags(vk::CommandPoolCreateFlagBits::eResetCommandBuffer); // We want to reuse cmd buffers for each frame.
    graphicsCommandPool = mDevice.createCommandPool(graphicsPoolInfo);

    vk::CommandPoolCreateInfo computePoolInfo{};
    computePoolInfo.setQueueFamilyIndex(queueIndicies.ComputeQueueIndex);
    computeCommandPool = mDevice.createCommandPool(computePoolInfo);
}


vk::DescriptorPool ThiefVKDevice::createDescriptorPool() {
    // crerate the descriptor set pools for uniform buffers and combined image samplers
    vk::DescriptorPoolSize uniformBufferDescPoolSize{};
    uniformBufferDescPoolSize.setType(vk::DescriptorType::eUniformBuffer);
    uniformBufferDescPoolSize.setDescriptorCount(15); // start with 5 we can allways allocate another pool if we later need more.

    vk::DescriptorPoolSize imageSamplerrDescPoolSize{};
    imageSamplerrDescPoolSize.setType(vk::DescriptorType::eCombinedImageSampler);
    imageSamplerrDescPoolSize.setDescriptorCount(15); // start with 5 we can allways allocate another pool if we later need more.

    std::array<vk::DescriptorPoolSize, 2> descPoolSizes{uniformBufferDescPoolSize, imageSamplerrDescPoolSize};

    vk::DescriptorPoolCreateInfo uniformBufferDescPoolInfo{};
    uniformBufferDescPoolInfo.setPoolSizeCount(descPoolSizes.size()); // two pools one for uniform buffers and one for combined image samplers
    uniformBufferDescPoolInfo.setPPoolSizes(descPoolSizes.data());
    uniformBufferDescPoolInfo.setMaxSets(15);

    return mDevice.createDescriptorPool(uniformBufferDescPoolInfo);
}


void ThiefVKDevice::destroyDescriptorPool(vk::DescriptorPool& pool) {
    mDevice.destroyDescriptorPool(pool);
}


vk::CommandBuffer ThiefVKDevice::beginSingleUseGraphicsCommandBuffer() {
    vk::CommandBufferAllocateInfo allocInfo{};
    allocInfo.setCommandBufferCount(1);
	allocInfo.setLevel(vk::CommandBufferLevel::ePrimary); // this will probably be used for image format transitions
    allocInfo.setCommandPool(graphicsCommandPool);

    vk::CommandBuffer cmdBuffer = mDevice.allocateCommandBuffers(allocInfo)[0]; // we know we're only allocating one

    vk::CommandBufferBeginInfo beginInfo{};
    beginInfo.setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit); // just use once

    cmdBuffer.begin(beginInfo);

    return cmdBuffer;
}


void ThiefVKDevice::endSingleUseGraphicsCommandBuffer(vk::CommandBuffer cmdBuffer) {
	cmdBuffer.end();

	vk::SubmitInfo submitInfo{};
	submitInfo.setCommandBufferCount(1);
	submitInfo.setPCommandBuffers(&cmdBuffer);
	
	mGraphicsQueue.submit(submitInfo, nullptr);
	mGraphicsQueue.waitIdle();

	mDevice.freeCommandBuffers(graphicsCommandPool, cmdBuffer);
}


void ThiefVKDevice::transitionImageLayout(vk::Image& image, vk::ImageLayout oldLayout, vk::ImageLayout newLayout) {
    vk::ImageMemoryBarrier memBarrier{};
    memBarrier.setOldLayout(oldLayout);
    memBarrier.setNewLayout(newLayout);

    // we aren't transfering queue ownership
    memBarrier.setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED);
    memBarrier.setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED);

    memBarrier.setImage(image);

    if(newLayout == vk::ImageLayout::eDepthStencilAttachmentOptimal) {
        memBarrier.setSubresourceRange({vk::ImageAspectFlagBits::eDepth, 0, 1, 0, 1});
    } else {
        memBarrier.setSubresourceRange({vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1});
    }

    vk::PipelineStageFlags sourceStage = vk::PipelineStageFlagBits::eTopOfPipe;
    vk::PipelineStageFlags destinationStage = vk::PipelineStageFlagBits::eColorAttachmentOutput;

    frameResources[currentFrameBufferIndex].flushCommandBuffer.pipelineBarrier(sourceStage, destinationStage, vk::DependencyFlagBits::eByRegion, 0, nullptr, 0, nullptr, 1, &memBarrier);
}


void ThiefVKDevice::copyBuffers(vk::Buffer& SrcBuffer, vk::Buffer& DstBuffer, vk::DeviceSize size) {
    vk::BufferCopy copyInfo{};
    copyInfo.setSize(size);

	frameResources[currentFrameBufferIndex].flushCommandBuffer.copyBuffer(SrcBuffer, DstBuffer, copyInfo); // record these commands in to the flush buffer that will get submitted before any draw calls are made
}


void ThiefVKDevice::CopybufferToImage(vk::Buffer& srcBuffer, vk::Image& dstImage, uint32_t width, uint32_t height) {
    vk::BufferImageCopy copyInfo{};
    copyInfo.setBufferOffset(0);
    copyInfo.setBufferImageHeight(0);
    copyInfo.setBufferRowLength(0);

    copyInfo.setImageSubresource({vk::ImageAspectFlagBits::eColor, 0, 0, 1});

    copyInfo.setImageOffset({0, 0, 0}); // copy to the image starting at the start (0, 0, 0)
    copyInfo.setImageExtent({width, height, 1});

	frameResources[currentFrameBufferIndex].flushCommandBuffer.copyBufferToImage(srcBuffer, dstImage, vk::ImageLayout::eTransferDstOptimal, copyInfo);
}


vk::CommandBuffer& ThiefVKDevice::startRecordingColourCmdBuffer() {
	vk::CommandBuffer& colourCmdBuffer = frameResources[currentFrameBufferIndex].colourCmdBuffer;

    vk::CommandBufferInheritanceInfo inheritanceInfo{};
    inheritanceInfo.setRenderPass(mRenderPasses.RenderPass);
    inheritanceInfo.setSubpass(0);
    inheritanceInfo.setFramebuffer(frameBuffers[currentFrameBufferIndex]);

	vk::CommandBufferBeginInfo beginInfo{};
    beginInfo.setPInheritanceInfo(&inheritanceInfo);
	beginInfo.setFlags(vk::CommandBufferUsageFlagBits::eRenderPassContinue);

	// start recording commands in to the buffer
	colourCmdBuffer.begin(beginInfo);

	ThiefVKPipelineDescription pipelineDesc{};
	pipelineDesc.vertexShader		 = ShaderName::BasicTransformVertex;
	pipelineDesc.fragmentShader		 = ShaderName::BasicColourFragment;
	pipelineDesc.renderPass			 = mRenderPasses.RenderPass;
	pipelineDesc.renderTargetOffsetX = 0;
	pipelineDesc.renderTargetOffsetY = 0;
	pipelineDesc.renderTargetHeight  = mSwapChain.getSwapChainImageHeight();
	pipelineDesc.renderTargetWidth   = mSwapChain.getSwapChainImageWidth();

	colourCmdBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipelineManager.getPipeLine(pipelineDesc));

	return colourCmdBuffer;
}


vk::CommandBuffer& ThiefVKDevice::startRecordingDepthCmdBuffer() {
	vk::CommandBuffer& depthCmdBuffer = frameResources[currentFrameBufferIndex].depthCmdBuffer;

    vk::CommandBufferInheritanceInfo inheritanceInfo{};
    inheritanceInfo.setRenderPass(mRenderPasses.RenderPass);
    inheritanceInfo.setSubpass(1);
    inheritanceInfo.setFramebuffer(frameBuffers[currentFrameBufferIndex]);

	vk::CommandBufferBeginInfo beginInfo{};
    beginInfo.setPInheritanceInfo(&inheritanceInfo);
	beginInfo.setFlags(vk::CommandBufferUsageFlagBits::eRenderPassContinue);

	// start recording commands in to the buffer
	depthCmdBuffer.begin(beginInfo);

	ThiefVKPipelineDescription pipelineDesc{};
	pipelineDesc.vertexShader		 = ShaderName::DepthVertex;
	pipelineDesc.fragmentShader		 = ShaderName::DepthFragment;
	pipelineDesc.renderPass			 = mRenderPasses.RenderPass;
	pipelineDesc.renderTargetOffsetX = 0;
	pipelineDesc.renderTargetOffsetY = 0;
	pipelineDesc.renderTargetHeight  = mSwapChain.getSwapChainImageHeight();
	pipelineDesc.renderTargetWidth   = mSwapChain.getSwapChainImageWidth();

	depthCmdBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipelineManager.getPipeLine(pipelineDesc));


	return depthCmdBuffer;
}


vk::CommandBuffer& ThiefVKDevice::startRecordingNormalsCmdBuffer() {
	vk::CommandBuffer& normalsCmdBuffer = frameResources[currentFrameBufferIndex].normalsCmdBuffer;

    vk::CommandBufferInheritanceInfo inheritanceInfo{};
    inheritanceInfo.setRenderPass(mRenderPasses.RenderPass);
    inheritanceInfo.setSubpass(2);
    inheritanceInfo.setFramebuffer(frameBuffers[currentFrameBufferIndex]);

	vk::CommandBufferBeginInfo beginInfo{};
    beginInfo.setPInheritanceInfo(&inheritanceInfo);
	beginInfo.setFlags(vk::CommandBufferUsageFlagBits::eRenderPassContinue);

	// start recording commands in to the buffer
	normalsCmdBuffer.begin(beginInfo);

	ThiefVKPipelineDescription pipelineDesc{};
	pipelineDesc.vertexShader		 = ShaderName::NormalVertex;
	pipelineDesc.fragmentShader		 = ShaderName::NormalFragment;
	pipelineDesc.renderPass			 = mRenderPasses.RenderPass;
	pipelineDesc.renderTargetOffsetX = 0;
	pipelineDesc.renderTargetOffsetY = 0;
	pipelineDesc.renderTargetHeight  = mSwapChain.getSwapChainImageHeight();
	pipelineDesc.renderTargetWidth   = mSwapChain.getSwapChainImageWidth();

	normalsCmdBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipelineManager.getPipeLine(pipelineDesc));


	return normalsCmdBuffer;
}

vk::CommandBuffer& ThiefVKDevice::startRecordingCompositeCmdBuffer() {
    vk::CommandBuffer& compositeCmdBuffer = frameResources[currentFrameBufferIndex].compositeCmdBuffer;

    vk::CommandBufferInheritanceInfo inheritanceInfo{};
    inheritanceInfo.setRenderPass(mRenderPasses.RenderPass);
    inheritanceInfo.setSubpass(3);
    inheritanceInfo.setFramebuffer(frameBuffers[currentFrameBufferIndex]);

    vk::CommandBufferBeginInfo beginInfo{};
    beginInfo.setPInheritanceInfo(&inheritanceInfo);
    beginInfo.setFlags(vk::CommandBufferUsageFlagBits::eRenderPassContinue);

    compositeCmdBuffer.begin(beginInfo);

    ThiefVKPipelineDescription pipelineDesc{};
    pipelineDesc.vertexShader        = ShaderName::CompositeVertex;
    pipelineDesc.fragmentShader      = ShaderName::CompositeFragment;
    pipelineDesc.renderPass          = mRenderPasses.RenderPass;
    pipelineDesc.renderTargetOffsetX = 0;
    pipelineDesc.renderTargetOffsetY = 0;
    pipelineDesc.renderTargetHeight  = mSwapChain.getSwapChainImageHeight();
    pipelineDesc.renderTargetWidth   = mSwapChain.getSwapChainImageWidth();

    compositeCmdBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipelineManager.getPipeLine(pipelineDesc));

    return compositeCmdBuffer;
}


void ThiefVKDevice::destroyPerFrameResources(perFrameResources& resources) {
    mDevice.destroyFence(resources.frameFinished);
    mDevice.destroySemaphore(resources.swapChainImageAvailable);
    mDevice.destroySemaphore(resources.imageRendered);

    for(auto& buffer : resources.stagingBuffers)  {
        destroyBuffer(buffer);
    }
    for(auto& image : resources.textureImages) {
        destroyImage(image);
    }
    destroyBuffer(resources.vertexBuffer);
    destroyBuffer(resources.indexBuffer);
    destroyDescriptorPool(resources.descPool);
}


void ThiefVKDevice::createSemaphores() {
    for(auto& resources : frameResources) {
        vk::SemaphoreCreateInfo semInfo{};

        vk::FenceCreateInfo fenceInfo{};
        fenceInfo.setFlags(vk::FenceCreateFlagBits::eSignaled);

        resources.frameFinished = mDevice.createFence(fenceInfo);
        resources.swapChainImageAvailable = mDevice.createSemaphore(semInfo);
        resources.imageRendered = mDevice.createSemaphore(semInfo);
    }
}


std::vector<vk::DescriptorSet> ThiefVKDevice::allocateDescriptorSets(vk::DescriptorPool& pool) {
        vk::DescriptorSetAllocateInfo info{};
        info.setDescriptorPool(pool);
        info.setDescriptorSetCount(3);

        std::array<vk::DescriptorSetLayout, 3> descLayouts{pipelineManager.getDescriptorSetLayout(ShaderName::NormalFragment),
                                                               pipelineManager.getDescriptorSetLayout(ShaderName::BasicColourFragment),
                                                               pipelineManager.getDescriptorSetLayout(ShaderName::CompositeFragment)};
        info.setPSetLayouts(descLayouts.data());

        return mDevice.allocateDescriptorSets(info);

}