#include "ThiefVKDevice.hpp"
#include "ThiefVKInstance.hpp"

#include <array>
#include <iostream>
#include <limits>


// ThiefVKDeviceMemberFunctions

ThiefVKDevice::ThiefVKDevice(std::pair<vk::PhysicalDevice, vk::Device> Devices, vk::SurfaceKHR surface, GLFWwindow * window) :
    mPhysDev{std::get<0>(Devices)}, 
	mDevice{std::get<1>(Devices)}, 
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
    DestroyFrameBuffers();
    DestroyAllImageTextures();
    pipelineManager.Destroy();
    MemoryManager.Destroy();
	mUniformBufferManager.Destroy();
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
	vk::SemaphoreCreateInfo semInfo{}; // will be set once the swapchain image is available
	vk::Semaphore swapChainImageAvailable = mDevice.createSemaphore(semInfo);

	currentFrameBufferIndex = mSwapChain.getNextImageIndex(mDevice, swapChainImageAvailable);

	vk::CommandBuffer flushBuffer = beginSingleUseGraphicsCommandBuffer();

	if(frameResources[currentFrameBufferIndex].frameFinished != vk::Fence(nullptr)) {
		mDevice.waitForFences(frameResources[currentFrameBufferIndex].frameFinished, true, std::numeric_limits<uint64_t>::max());
		mDevice.resetFences(1, &frameResources[currentFrameBufferIndex].frameFinished);
	} else {
		vk::FenceCreateInfo fenceInfo{};

		frameResources[currentFrameBufferIndex].frameFinished = mDevice.createFence(fenceInfo);
	}

	perFrameResources frameResource{};

	if(frameResources[currentFrameBufferIndex].primaryCmdBuffer == vk::CommandBuffer(nullptr)) {
		// Only allocate thecommand buffers if this will be there first use.
		vk::CommandBufferAllocateInfo primaryCmdBufferAllocInfo;
		primaryCmdBufferAllocInfo.setLevel(vk::CommandBufferLevel::ePrimary);
		primaryCmdBufferAllocInfo.setCommandPool(graphicsCommandPool);
		primaryCmdBufferAllocInfo.setCommandBufferCount(1);

		vk::CommandBuffer primaryCmdBuffer = mDevice.allocateCommandBuffers(primaryCmdBufferAllocInfo)[0];

		vk::CommandBufferAllocateInfo secondaryCmdBufferAllocInfo;
		secondaryCmdBufferAllocInfo.setLevel(vk::CommandBufferLevel::eSecondary);
		secondaryCmdBufferAllocInfo.setCommandPool(graphicsCommandPool);
		secondaryCmdBufferAllocInfo.setCommandBufferCount(4);

		std::vector<vk::CommandBuffer> secondaryCmdBuffers = mDevice.allocateCommandBuffers(secondaryCmdBufferAllocInfo);

		// Set the initial cmd Buffers.
		frameResource.primaryCmdBuffer			= primaryCmdBuffer;
		frameResource.colourCmdBuffer			= secondaryCmdBuffers[0];
		frameResource.depthCmdBuffer			= secondaryCmdBuffers[1];
		frameResource.normalsCmdBuffer			= secondaryCmdBuffers[2];
		frameResource.shadowCmdBuffer			= secondaryCmdBuffers[3];
	} else { // Otherwise just reset them
		frameResources[currentFrameBufferIndex].primaryCmdBuffer.reset(vk::CommandBufferResetFlags());
		frameResources[currentFrameBufferIndex].colourCmdBuffer.reset(vk::CommandBufferResetFlags());
		frameResources[currentFrameBufferIndex].depthCmdBuffer.reset(vk::CommandBufferResetFlags());
		frameResources[currentFrameBufferIndex].normalsCmdBuffer.reset(vk::CommandBufferResetFlags());
		frameResources[currentFrameBufferIndex].shadowCmdBuffer.reset(vk::CommandBufferResetFlags());
	}

	frameResource.submissionID				= finishedSubmissionID; // set the minimum we need to start recording command buffers.
	frameResource.swapChainImageAvailable	= swapChainImageAvailable;
	frameResource.flushCommandBuffer		= flushBuffer;

	frameResources[currentFrameBufferIndex] = frameResource;

	// start the render pass so that we can begin recording in to the command buffers
	vk::RenderPassBeginInfo renderPassBegin{};
	renderPassBegin.framebuffer = frameBuffers[currentFrameBufferIndex];
	renderPassBegin.renderPass = mRenderPasses.RenderPass;
	vk::ClearValue colour(0.0f);
	renderPassBegin.setPClearValues(&colour);
	renderPassBegin.setRenderArea(vk::Rect2D{ { 0, 0 },
		{ static_cast<uint32_t>(mSwapChain.getSwapChainImageHeight()), static_cast<uint32_t>(mSwapChain.getSwapChainImageWidth()) } });

	// Begin the render pass
	frameResource.primaryCmdBuffer.beginRenderPass(renderPassBegin, vk::SubpassContents::eSecondaryCommandBuffers);

	startRecordingColourCmdBuffer();
	startRecordingDepthCmdBuffer();
	startRecordingNormalsCmdBuffer();
}


void ThiefVKDevice::draw(geometry& geom) {

}


void ThiefVKDevice::endFrame() {
	finishedSubmissionID++;

	perFrameResources& resources = frameResources[currentFrameBufferIndex];
	vk::CommandBuffer& primaryCmdBuffer = resources.primaryCmdBuffer;

	vk::CommandBufferBeginInfo primaryBeginInfo{};
	primaryCmdBuffer.begin(primaryBeginInfo);

	// end recording in to these as we're
	resources.flushCommandBuffer.end();
	resources.colourCmdBuffer.end();
	resources.depthCmdBuffer.end();
	resources.normalsCmdBuffer.end();
	resources.shadowCmdBuffer.end();

	// Execute the secondary cmd buffers in the primary
	primaryCmdBuffer.nextSubpass(vk::SubpassContents::eSecondaryCommandBuffers);
	primaryCmdBuffer.executeCommands(1, &resources.colourCmdBuffer);
	primaryCmdBuffer.nextSubpass(vk::SubpassContents::eSecondaryCommandBuffers);
	primaryCmdBuffer.executeCommands(1, &resources.depthCmdBuffer);
	primaryCmdBuffer.nextSubpass(vk::SubpassContents::eSecondaryCommandBuffers);
	primaryCmdBuffer.executeCommands(1, &resources.normalsCmdBuffer);
	primaryCmdBuffer.nextSubpass(vk::SubpassContents::eSecondaryCommandBuffers);
	primaryCmdBuffer.executeCommands(1, &resources.shadowCmdBuffer);
	primaryCmdBuffer.nextSubpass(vk::SubpassContents::eInline);

	// Record the compositing operations.
	// TODO

	primaryCmdBuffer.endRenderPass();
	primaryCmdBuffer.end();

	// Submit everything for this frame
	std::array<vk::CommandBuffer, 2> cmdBuffers{resources.flushCommandBuffer, resources.primaryCmdBuffer};

	vk::SubmitInfo submitInfo{};
	submitInfo.setCommandBufferCount(cmdBuffers.size());
	submitInfo.setPCommandBuffers(cmdBuffers.data());
	submitInfo.setWaitSemaphoreCount(1);
	submitInfo.setPWaitSemaphores(&resources.swapChainImageAvailable);
	auto const waitStage = vk::PipelineStageFlags(vk::PipelineStageFlagBits::eTransfer);
	submitInfo.setPWaitDstStageMask(&waitStage);

	mGraphicsQueue.submit(submitInfo, resources.frameFinished);
}


std::pair<vk::Image, Allocation> ThiefVKDevice::createColourImage(const uint32_t width, const uint32_t height) {
    vk::ImageCreateInfo imageInfo{};
    imageInfo.setExtent({width, height, 1});
    imageInfo.setFormat(vk::Format::eR8G8B8A8Srgb);
    imageInfo.setInitialLayout(vk::ImageLayout::eUndefined);
    imageInfo.setImageType(vk::ImageType::e2D);
    imageInfo.setMipLevels(1);
    imageInfo.setArrayLayers(1);
    imageInfo.setTiling(vk::ImageTiling::eOptimal);
    imageInfo.setUsage(vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eInputAttachment);

    vk::Image image = mDevice.createImage(imageInfo);
    vk::MemoryRequirements imageMemRequirments = mDevice.getImageMemoryRequirements(image);

    Allocation imageMemory = MemoryManager.Allocate(width * height, imageMemRequirments.alignment, false); // we don't need to be able to map the image

    return {image, imageMemory};
}


std::pair<vk::Image, Allocation> ThiefVKDevice::createDepthImage(const uint32_t width, const uint32_t height) {
    vk::ImageCreateInfo imageInfo{};
    imageInfo.setExtent({width, height, 1});
    imageInfo.setFormat(vk::Format::eD32Sfloat);
    imageInfo.setInitialLayout(vk::ImageLayout::eUndefined);
    imageInfo.setImageType(vk::ImageType::e2D);
    imageInfo.setMipLevels(1);
    imageInfo.setArrayLayers(1);
    imageInfo.setTiling(vk::ImageTiling::eOptimal);
    imageInfo.setUsage(vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eInputAttachment);

    vk::Image image = mDevice.createImage(imageInfo);
    vk::MemoryRequirements imageMemRequirments = mDevice.getImageMemoryRequirements(image);

    Allocation imageMemory = MemoryManager.Allocate(width * height, imageMemRequirments.alignment,  false); // we don't need to be able to map the image

    return {image, imageMemory};
}


std::pair<vk::Image, Allocation> ThiefVKDevice::createNormalsImage(const uint32_t width, const uint32_t height) {
    vk::ImageCreateInfo imageInfo{};
    imageInfo.setExtent({width, height, 1});
    imageInfo.setFormat(vk::Format::eR8G8B8A8Sint);
    imageInfo.setInitialLayout(vk::ImageLayout::eUndefined);
	imageInfo.setSharingMode(vk::SharingMode::eExclusive);
    imageInfo.setImageType(vk::ImageType::e2D);
    imageInfo.setMipLevels(1);
    imageInfo.setArrayLayers(1);
    imageInfo.setTiling(vk::ImageTiling::eOptimal);
    imageInfo.setUsage(vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eInputAttachment);

    vk::Image image = mDevice.createImage(imageInfo);
    vk::MemoryRequirements imageMemRequirments = mDevice.getImageMemoryRequirements(image);

    Allocation imageMemory = MemoryManager.Allocate(width * height, imageMemRequirments.alignment,  false); // we don't need to be able to map the image

    return {image, imageMemory};
}


std::pair<vk::Buffer, Allocation> ThiefVKDevice::createBuffer(const vk::BufferUsageFlags usage, const uint32_t size) {
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


void ThiefVKDevice::destroyBuffer(vk::Buffer& buffer, Allocation alloc) {
	MemoryManager.Free(alloc);

	mDevice.destroyBuffer(buffer);
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

        for(unsigned int i = 0; i < images.lighImages.size(); ++i) {
            DestroyImageView(images.lighImageViews[i]);
            DestroyImage(images.lighImages[i], images.lightImageMemory[i]);
        }
    }
}


void ThiefVKDevice::createDeferedRenderTargetImageViews() {
    ThiefVKImageTextutres Result{};

    for(unsigned int swapImageCount = 0; swapImageCount < mSwapChain.getNumberOfSwapChainImages(); ++swapImageCount) {
        auto [colourImage, colourMemory]   = createColourImage(mSwapChain.getSwapChainImageWidth(), mSwapChain.getSwapChainImageHeight());
        auto [depthImage , depthMemory]    = createDepthImage(mSwapChain.getSwapChainImageWidth(), mSwapChain.getSwapChainImageHeight());
        auto [normalsImage, normalsMemory] = createNormalsImage(mSwapChain.getSwapChainImageWidth(), mSwapChain.getSwapChainImageHeight());

        // Bind the memory to the images
        MemoryManager.BindImage(colourImage, colourMemory);
        MemoryManager.BindImage(depthImage, depthMemory);
        MemoryManager.BindImage(normalsImage, normalsMemory);

        std::vector<vk::Image> lightImages(spotLights.size());
        std::vector<Allocation> lightImageMemory(spotLights.size());

        for(unsigned int i = 0; i < spotLights.size(); ++i) {
            auto [lightImage, lightMemory] = createColourImage(mSwapChain.getSwapChainImageWidth(), mSwapChain.getSwapChainImageHeight());
            lightImages.push_back(lightImage);
            lightImageMemory.push_back(lightMemory);

            MemoryManager.BindImage(lightImage, lightMemory);
        }
                
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

        std::vector<vk::ImageViewCreateInfo> lighViewCreateInfo(spotLights.size());
        for(unsigned int i = 0; i < spotLights.size(); ++i) {
            vk::ImageViewCreateInfo lightViewInfo{};
            lightViewInfo.setImage(colourImage);
            lightViewInfo.setViewType(vk::ImageViewType::e2D);
            lightViewInfo.setFormat(vk::Format::eR8G8B8A8Srgb);
            lightViewInfo.setComponents(vk::ComponentMapping());
            lightViewInfo.setSubresourceRange(vk::ImageSubresourceRange{vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1});

            lighViewCreateInfo.push_back(lightViewInfo);
        }

        vk::ImageView colourImageView  = mDevice.createImageView(colourViewInfo);
        vk::ImageView depthImageView   = mDevice.createImageView(depthViewInfo);
        vk::ImageView normalsImageView = mDevice.createImageView(normalsViewInfo);

        std::vector<vk::ImageView> lightsImageViews(spotLights.size());
        for(unsigned int i = 0; i < spotLights.size(); ++i) {
            lightsImageViews[i] = mDevice.createImageView(lighViewCreateInfo[i]);
        }

        Result.colourImage          = colourImage;
        Result.colourImageView      = colourImageView;
        Result.colourImageMemory    = colourMemory;

        Result.depthImage           = depthImage;
        Result.depthImageView       = depthImageView;
        Result.depthImageMemory     = depthMemory;

        Result.normalsImage         = normalsImage;
        Result.normalsImageView     = normalsImageView;
        Result.normalsImageMemory   = normalsMemory;

        Result.lighImages           = lightImages;
        Result.lighImageViews       = lightsImageViews;
        Result.lightImageMemory     = lightImageMemory;

        deferedTextures.push_back(Result);
    }
}


void ThiefVKDevice::createRenderPasses() {

    // Specify all attachments used in all subrenderpasses
    vk::AttachmentDescription colourPassAttachment{}; // also used for the shadow mapping
    colourPassAttachment.setFormat(vk::Format::eR8G8B8A8Srgb);
    colourPassAttachment.setLoadOp(vk::AttachmentLoadOp::eDontCare); // we are going to overwrite all pixles
    colourPassAttachment.setStoreOp(vk::AttachmentStoreOp::eDontCare);
    colourPassAttachment.setStencilLoadOp(vk::AttachmentLoadOp::eDontCare);
    colourPassAttachment.setStencilStoreOp(vk::AttachmentStoreOp::eDontCare);
    colourPassAttachment.setInitialLayout(vk::ImageLayout::eColorAttachmentOptimal);
    colourPassAttachment.setFinalLayout(vk::ImageLayout::eColorAttachmentOptimal);

    vk::AttachmentReference coloursubPassReference{};
    coloursubPassReference.setAttachment(0);
    coloursubPassReference.setLayout(vk::ImageLayout::eGeneral); // as these will be read and written to


    vk::AttachmentDescription depthPassAttachment{};
    depthPassAttachment.setFormat(vk::Format::eD32Sfloat); // store in each pixel a 32bit depth value
    depthPassAttachment.setStencilLoadOp(vk::AttachmentLoadOp::eDontCare);
    depthPassAttachment.setStencilStoreOp(vk::AttachmentStoreOp::eDontCare);
    depthPassAttachment.setStencilLoadOp(vk::AttachmentLoadOp::eDontCare);
    depthPassAttachment.setStencilStoreOp(vk::AttachmentStoreOp::eDontCare);
    depthPassAttachment.setInitialLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal); // wriet in a subpass then read in a subsequent one
    depthPassAttachment.setFinalLayout(vk::ImageLayout::eDepthStencilReadOnlyOptimal);

    vk::AttachmentReference depthsubPassReference{};
    depthsubPassReference.setAttachment(1);
    depthsubPassReference.setLayout(vk::ImageLayout::eGeneral);


    vk::AttachmentDescription normalsPassAttachment{};
    normalsPassAttachment.setFormat(vk::Format::eR8G8B8A8Sint);
    normalsPassAttachment.setLoadOp(vk::AttachmentLoadOp::eDontCare);
    normalsPassAttachment.setStoreOp(vk::AttachmentStoreOp::eDontCare);
    normalsPassAttachment.setStencilLoadOp(vk::AttachmentLoadOp::eDontCare);
    normalsPassAttachment.setStencilStoreOp(vk::AttachmentStoreOp::eDontCare);
    normalsPassAttachment.setInitialLayout(vk::ImageLayout::eColorAttachmentOptimal);
    normalsPassAttachment.setFinalLayout(vk::ImageLayout::eColorAttachmentOptimal); // these will be used in the subsqeuent light renderpass

    vk::AttachmentReference normalssubPassReference{};
    normalssubPassReference.setAttachment(2);
    normalssubPassReference.setLayout(vk::ImageLayout::eGeneral);

    vk::AttachmentReference swapChainAttatchmentreference{};
    swapChainAttatchmentreference.setAttachment(3);
    swapChainAttatchmentreference.setLayout(vk::ImageLayout::eColorAttachmentOptimal);

    // specify the subpass descriptions
    vk::SubpassDescription colourPassDesc{};
    colourPassDesc.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics);
    colourPassDesc.setColorAttachmentCount(1);
    colourPassDesc.setPColorAttachments(&coloursubPassReference);

    vk::SubpassDescription depthPassDesc{};
    depthPassDesc.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics);
    depthPassDesc.setPDepthStencilAttachment(&depthsubPassReference);

    vk::SubpassDescription normalsPassDesc{};
    normalsPassDesc.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics);
    normalsPassDesc.setColorAttachmentCount(1);
    normalsPassDesc.setPColorAttachments(&normalssubPassReference);

    mRenderPasses.colourPass    = colourPassDesc;
    mRenderPasses.depthPass     = depthPassDesc;
    mRenderPasses.normalsPass   = normalsPassDesc;


    // create the light and composite subPasses
    vk::AttachmentDescription swapChainImageAttachment{};
    swapChainImageAttachment.setFormat(mSwapChain.getSwapChainImageFormat());
    swapChainImageAttachment.setLoadOp(vk::AttachmentLoadOp::eClear);
    swapChainImageAttachment.setStoreOp(vk::AttachmentStoreOp::eStore);
    swapChainImageAttachment.setStencilLoadOp(vk::AttachmentLoadOp::eDontCare);
    swapChainImageAttachment.setStencilStoreOp(vk::AttachmentStoreOp::eDontCare);
    swapChainImageAttachment.setInitialLayout(vk::ImageLayout::eColorAttachmentOptimal);
    swapChainImageAttachment.setFinalLayout(vk::ImageLayout::ePresentSrcKHR);

    vk::AttachmentReference colourCompositPassReference{};
    colourCompositPassReference.setAttachment(3);
    colourCompositPassReference.setLayout(vk::ImageLayout::eColorAttachmentOptimal);

    // calculate and add the light attachment desc
    std::vector<vk::AttachmentReference> spotLightattachmentRefs{coloursubPassReference, depthsubPassReference, normalssubPassReference};
    for(uint32_t i = 0; i < spotLights.size(); ++i)
    {
        vk::AttachmentReference lightRef{};
        lightRef.setAttachment(i + 4);
        lightRef.setLayout(vk::ImageLayout::eGeneral);

        spotLightattachmentRefs.push_back(lightRef);
    }

    vk::SubpassDescription compositPassDesc{};
    compositPassDesc.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics);
    compositPassDesc.setInputAttachmentCount(spotLightattachmentRefs.size());
    compositPassDesc.setPInputAttachments(spotLightattachmentRefs.data());
    compositPassDesc.setColorAttachmentCount(1);
    compositPassDesc.setPColorAttachments(&swapChainAttatchmentreference);


    std::vector<vk::AttachmentReference> lightAttachments{};
    for(uint32_t i = 0; i < spotLights.size(); ++i) {
        lightAttachments.push_back(coloursubPassReference);
    }

    vk::SubpassDescription lightSubpass{};
    lightSubpass.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics);
    lightSubpass.setColorAttachmentCount(lightAttachments.size());
    lightSubpass.setPColorAttachments(lightAttachments.data());


    std::vector<vk::AttachmentDescription> allAttachments{colourPassAttachment, depthPassAttachment, normalsPassAttachment, swapChainImageAttachment};
    for(uint32_t i = 0; i < spotLights.size(); ++i ) { // only support spot lights currently
        allAttachments.push_back(colourPassAttachment);
    }

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
    colourToCompositeDepen.setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);
    colourToCompositeDepen.setDstSubpass(4);
    colourToCompositeDepen.setDstStageMask(vk::PipelineStageFlagBits::eFragmentShader);
    colourToCompositeDepen.setSrcAccessMask(vk::AccessFlagBits::eColorAttachmentWrite);
    colourToCompositeDepen.setDstAccessMask(vk::AccessFlagBits::eShaderRead);

    vk::SubpassDependency depthToCompositeDepen{};
    depthToCompositeDepen.setSrcSubpass(1);
    depthToCompositeDepen.setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);
    depthToCompositeDepen.setDstSubpass(4);
    depthToCompositeDepen.setDstStageMask(vk::PipelineStageFlagBits::eFragmentShader);
    depthToCompositeDepen.setSrcAccessMask(vk::AccessFlagBits::eColorAttachmentWrite);
    depthToCompositeDepen.setDstAccessMask(vk::AccessFlagBits::eShaderRead);

    vk::SubpassDependency normalsToCompositeDepen{};
    normalsToCompositeDepen.setSrcSubpass(2);
    normalsToCompositeDepen.setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);
    normalsToCompositeDepen.setDstSubpass(4);
    normalsToCompositeDepen.setDstStageMask(vk::PipelineStageFlagBits::eFragmentShader);
    normalsToCompositeDepen.setSrcAccessMask(vk::AccessFlagBits::eColorAttachmentWrite);
    normalsToCompositeDepen.setDstAccessMask(vk::AccessFlagBits::eShaderRead);

    vk::SubpassDependency lightToCompositeDepen{};
    lightToCompositeDepen.setSrcSubpass(3);
    lightToCompositeDepen.setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);
    lightToCompositeDepen.setDstSubpass(4);
    lightToCompositeDepen.setDstStageMask(vk::PipelineStageFlagBits::eFragmentShader);
    lightToCompositeDepen.setSrcAccessMask(vk::AccessFlagBits::eColorAttachmentWrite);
    lightToCompositeDepen.setDstAccessMask(vk::AccessFlagBits::eShaderRead);

    std::vector<vk::SubpassDescription> allSubpasses{colourPassDesc, depthPassDesc, normalsPassDesc, lightSubpass, compositPassDesc};
    std::vector<vk::SubpassDependency>  allSubpassDependancies{implicitFirstDepen, colourToCompositeDepen, depthToCompositeDepen, normalsToCompositeDepen, lightToCompositeDepen};

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

        std::vector<vk::ImageView> frameBufferAttatchments{deferedTextures[i].colourImageView
                                                          , deferedTextures[i].depthImageView
                                                          , deferedTextures[i].normalsImageView
                                                          , swapChainImage };

        for(const auto& lightImageViews : deferedTextures[i].lighImageViews) {
            frameBufferAttatchments.push_back(lightImageViews);
        }

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


void ThiefVKDevice::createVertexBuffer() {

}


void ThiefVKDevice::transitionImageLayout(vk::Image image, vk::Format format, vk::ImageLayout oldLayout, vk::ImageLayout newLayout) {
    vk::ImageMemoryBarrier memBarrier{};
    memBarrier.setOldLayout(oldLayout);
    memBarrier.setNewLayout(newLayout);

    // we aren't transfering queue ownership
    memBarrier.setSrcQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED);
    memBarrier.setDstQueueFamilyIndex(VK_QUEUE_FAMILY_IGNORED);

    memBarrier.setImage(image);
    memBarrier.setSubresourceRange({vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1});

    vk::PipelineStageFlags sourceStage;
    vk::PipelineStageFlags destinationStage;

    if (oldLayout == vk::ImageLayout::eUndefined && newLayout == vk::ImageLayout::eTransferDstOptimal) {
        memBarrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;

        sourceStage = vk::PipelineStageFlagBits::eTopOfPipe;
        destinationStage = vk::PipelineStageFlagBits::eTransfer;
    } else if (oldLayout == vk::ImageLayout::eTransferDstOptimal && newLayout == vk::ImageLayout::eShaderReadOnlyOptimal) {
        memBarrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
        memBarrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

        sourceStage = vk::PipelineStageFlagBits::eTransfer;
        destinationStage = vk::PipelineStageFlagBits::eFragmentShader;
    }

    frameResources[currentFrameBufferIndex].flushCommandBuffer.pipelineBarrier(sourceStage, destinationStage, vk::DependencyFlagBits::eByRegion, 0, nullptr, 0, nullptr, 1, &memBarrier);
}


void ThiefVKDevice::copyBuffers(vk::Buffer SrcBuffer, vk::Buffer DstBuffer, vk::DeviceSize size) {
    vk::BufferCopy copyInfo{};
    copyInfo.setSize(size);

	frameResources[currentFrameBufferIndex].flushCommandBuffer.copyBuffer(SrcBuffer, DstBuffer, copyInfo); // record these commands in to the flush buffer that will get submitted before any draw calls are made
}


void ThiefVKDevice::CopybufferToImage(vk::Buffer srcBuffer, vk::Image dstImage, uint32_t width, uint32_t height) {
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

	vk::CommandBufferBeginInfo beginInfo{};
	beginInfo.setFlags(vk::CommandBufferUsageFlagBits::eRenderPassContinue);

	// start recording commands in to the buffer
	colourCmdBuffer.begin(beginInfo);

	ThiefVKPipelineDescription pipelineDesc{};
	pipelineDesc.fragmentShader		 = ShaderName::BasicTransformVertex;
	pipelineDesc.vertexShader		 = ShaderName::BasicColourFragment;
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

	vk::CommandBufferBeginInfo beginInfo{};
	beginInfo.setFlags(vk::CommandBufferUsageFlagBits::eRenderPassContinue);

	// start recording commands in to the buffer
	depthCmdBuffer.begin(beginInfo);

	depthCmdBuffer.bindVertexBuffers(0, frameResources[currentFrameBufferIndex].vertexBuffer, {0});

	ThiefVKPipelineDescription pipelineDesc{};
	pipelineDesc.fragmentShader		 = ShaderName::DepthVertex;
	pipelineDesc.vertexShader		 = ShaderName::DepthFragment;
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

	vk::CommandBufferBeginInfo beginInfo{};
	beginInfo.setFlags(vk::CommandBufferUsageFlagBits::eRenderPassContinue);

	// start recording commands in to the buffer
	normalsCmdBuffer.begin(beginInfo);

	normalsCmdBuffer.bindVertexBuffers(0, frameResources[currentFrameBufferIndex].vertexBuffer, {0});

	ThiefVKPipelineDescription pipelineDesc{};
	pipelineDesc.fragmentShader		 = ShaderName::NormalVertex;
	pipelineDesc.vertexShader		 = ShaderName::NormalFragment;
	pipelineDesc.renderPass			 = mRenderPasses.RenderPass;
	pipelineDesc.renderTargetOffsetX = 0;
	pipelineDesc.renderTargetOffsetY = 0;
	pipelineDesc.renderTargetHeight  = mSwapChain.getSwapChainImageHeight();
	pipelineDesc.renderTargetWidth   = mSwapChain.getSwapChainImageWidth();

	normalsCmdBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipelineManager.getPipeLine(pipelineDesc));


	return normalsCmdBuffer;
}


void ThiefVKDevice::createSemaphores() {

}
