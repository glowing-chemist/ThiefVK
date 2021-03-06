#include "ThiefVKDevice.hpp"
#include "ThiefVKInstance.hpp"
#include "ThiefVKDescriptorManager.hpp"
#include "ThiefVKModel.hpp"

#include "stb_image.h"

#include <array>
#include <set>
#include <iostream>
#include <limits>

#define DEBUG_SHOW_NORMALS 0

// ThiefVKDeviceMemberFunctions

ThiefVKDevice::ThiefVKDevice(std::pair<vk::PhysicalDevice, vk::Device> Devices, vk::SurfaceKHR surface, GLFWwindow * window) :
    mPhysDev{std::get<0>(Devices)}, 
	mDevice{std::get<1>(Devices)},
    mLimits{mPhysDev.getProperties().limits}, 
    finishedSubmissionID{0},
    currentFrameBufferIndex{0},
	pipelineManager{*this},
	MemoryManager{&mPhysDev, &mDevice},
	mUniformBufferManager{*this, vk::BufferUsageFlagBits::eUniformBuffer, mLimits.minUniformBufferOffsetAlignment},
	mVertexBufferManager{*this, vk::BufferUsageFlagBits::eVertexBuffer},
    mIndexBufferManager{*this, vk::BufferUsageFlagBits::eIndexBuffer},
    mSpotLightBufferManager{*this, vk::BufferUsageFlagBits::eUniformBuffer},
	DescriptorManager{*this},
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

    // unique buffers, this is needed as multiple frameResources structs can have handles to the same 
    // device buffers. This avoids double freeing any of them.
    std::set<ThiefVKBuffer> uniqueBuffers;
    for(auto& resource : frameResources) {
        destroyPerFrameResources(resource);
        uniqueBuffers.insert(resource.vertexBuffer);
        uniqueBuffers.insert(resource.indexBuffer);
        uniqueBuffers.insert(resource.uniformBuffer);
        uniqueBuffers.insert(resource.spotLightBuffer);
    }

    for(auto buffer : uniqueBuffers) {
        DestroyBufferInternal(buffer);
    }

    for(auto& [path, texture] : mTextureCache) {
        destroyImage(texture);
    }

    for(auto& [submissionID, buffer] : mPendingFreeBuffers) {
        DestroyBufferInternal(buffer);
    }

    DestroyFrameBuffers();
    DestroyAllImageTextures();
    pipelineManager.Destroy();
    MemoryManager.Destroy();
	DescriptorManager.Destroy();
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
    
    currentSubmissionID++;
    DestroyPendingBuffers();

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
        frameResources[currentFrameBufferIndex].primaryCmdBuffer        = primaryCmdBuffers[0];
        frameResources[currentFrameBufferIndex].flushCommandBuffer      = primaryCmdBuffers[1];
        frameResources[currentFrameBufferIndex].colourCmdBuffer         = secondaryCmdBuffers[0];
        frameResources[currentFrameBufferIndex].albedoCmdBuffer         = secondaryCmdBuffers[1];
        frameResources[currentFrameBufferIndex].normalsCmdBuffer        = secondaryCmdBuffers[2];
        frameResources[currentFrameBufferIndex].compositeCmdBuffer      = secondaryCmdBuffers[3];

    } else { // Otherwise just reset them
        finishedSubmissionID++;

        auto& resources = frameResources[currentFrameBufferIndex];

        resources.primaryCmdBuffer.reset(vk::CommandBufferResetFlags());
        resources.flushCommandBuffer.reset(vk::CommandBufferResetFlags());
        resources.colourCmdBuffer.reset(vk::CommandBufferResetFlags());
        resources.albedoCmdBuffer.reset(vk::CommandBufferResetFlags());
        resources.normalsCmdBuffer.reset(vk::CommandBufferResetFlags());

        for(auto& descSet : resources.DescSets) {
            DescriptorManager.destroyDescriptorSet(descSet);
        }
        resources.DescSets.clear();

        for(auto& stagingBuffer : resources.stagingBuffers)  {
            destroyBuffer(stagingBuffer);
        }
        resources.stagingBuffers.clear();

        for(auto& imageView : resources.textureImageViews) {
            mDevice.destroyImageView(imageView);
        }   
        resources.textureImageViews.clear();

        resources.textureImages.clear();
    }

    vk::CommandBufferBeginInfo beginInfo{};
    frameResources[currentFrameBufferIndex].flushCommandBuffer.begin(beginInfo);
}


void ThiefVKDevice::endFrame() {
    auto& resources = frameResources[currentFrameBufferIndex];

    // we defered the buffer destruction to here to we can avoid reuploading the buffer each 
    // frame if it hasn't changed.
    if(mVertexBufferManager.bufferHasChanged()) {
        destroyBuffer(resources.vertexBuffer);
        destroyBuffer(resources.indexBuffer);
    }
    if(mUniformBufferManager.bufferHasChanged()) {
        destroyBuffer(resources.uniformBuffer);
    }
    if(mSpotLightBufferManager.bufferHasChanged()) {
        destroyBuffer(resources.spotLightBuffer);
    }

    const std::vector<entryInfo> vertexBufferOffsets = mVertexBufferManager.getBufferOffsets();
    auto [vertexBuffer, vertexStagingBuffer] = mVertexBufferManager.flushBufferUploads();

    const std::vector<entryInfo> indexBufferOffsets = mIndexBufferManager.getBufferOffsets();
    auto [indexBuffer, indexStagingBuffer]          = mIndexBufferManager.flushBufferUploads();

    const std::vector<entryInfo> uniformBufferOffsets = mUniformBufferManager.getBufferOffsets();
    auto [uniformBuffer, uniformStagingBuffer] = mUniformBufferManager.flushBufferUploads();

    const std::vector<entryInfo> spotLIghtOffsets = mSpotLightBufferManager.getBufferOffsets();
    auto [spotLightBuffer, spotLightStagingBuffer] = mSpotLightBufferManager.flushBufferUploads();

    resources.stagingBuffers.push_back(vertexStagingBuffer);
    resources.vertexBuffer = vertexBuffer;
    resources.stagingBuffers.push_back(uniformStagingBuffer);
    resources.uniformBuffer = uniformBuffer;
    resources.stagingBuffers.push_back(spotLightStagingBuffer);
    resources.spotLightBuffer = spotLightBuffer;
    resources.stagingBuffers.push_back(indexStagingBuffer);
    resources.indexBuffer = indexBuffer;

    // Get all of the descriptor sets needed for this frame.

	//Colour potentially needs one desc set per draw call as could bind a different texture per model
	std::vector<ThiefVKDescriptorSet> colourDescriptorSets{};
	colourDescriptorSets.reserve(vertexBufferOffsets.size()); // only allocate once.
	for(uint32_t i = 0; i < vertexBufferOffsets.size(); ++i) {
		const ThiefVKDescriptorSetDescription basicColourDesc = getDescriptorSetDescription("Colour.frag.spv", i);
		colourDescriptorSets.push_back(DescriptorManager.getDescriptorSet(basicColourDesc));
	}

    ThiefVKDescriptorSetDescription albedoDesc = getDescriptorSetDescription("Albedo.frag.spv");
    ThiefVKDescriptorSet albedoDescriptor = DescriptorManager.getDescriptorSet(albedoDesc);
#if DEBUG_SHOW_NORMALS
    ThiefVKDescriptorSetDescription normalsDesc = getDescriptorSetDescription("NormalDebug.frag.spv");
#else
    ThiefVKDescriptorSetDescription normalsDesc = getDescriptorSetDescription("Normal.frag.spv");
#endif
    ThiefVKDescriptorSet normalsDescriptor = DescriptorManager.getDescriptorSet(normalsDesc);

    ThiefVKDescriptorSetDescription compositeDesc = getDescriptorSetDescription("Composite.frag.spv");
    ThiefVKDescriptorSet compositeDescriptor = DescriptorManager.getDescriptorSet(compositeDesc);

	frameResources[currentFrameBufferIndex].DescSets.insert(frameResources[currentFrameBufferIndex].DescSets.end(), colourDescriptorSets.begin(), colourDescriptorSets.end());
    frameResources[currentFrameBufferIndex].DescSets.push_back(albedoDescriptor);
    frameResources[currentFrameBufferIndex].DescSets.push_back(normalsDescriptor);
    frameResources[currentFrameBufferIndex].DescSets.push_back(compositeDescriptor);
	
    startFrameInternal();

	for (uint32_t i = 0; i < vertexBufferOffsets.size(); ++i) {
		const vk::DeviceSize bufferOffset   = vertexBufferOffsets[i].offset;
        const vk::DeviceSize indexOffset    = indexBufferOffsets[i].offset;
        const vk::DeviceSize uniformOffset  = uniformBufferOffsets[i].offset;
		
		resources.colourCmdBuffer.bindVertexBuffers(0, 1, &resources.vertexBuffer.mBuffer, &bufferOffset);
        resources.colourCmdBuffer.bindIndexBuffer(resources.indexBuffer.mBuffer, indexOffset, vk::IndexType::eUint32);
		resources.colourCmdBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipelineManager.getPipelineLayout("Colour.frag.spv"), 0, colourDescriptorSets[i].getHandle(), uniformOffset);
		resources.colourCmdBuffer.drawIndexed(indexBufferOffsets[i].numberOfEntries, 1, 0, 0, 0);

		resources.normalsCmdBuffer.bindVertexBuffers(0, 1, &resources.vertexBuffer.mBuffer, &bufferOffset);
        resources.normalsCmdBuffer.bindIndexBuffer(resources.indexBuffer.mBuffer, indexOffset, vk::IndexType::eUint32);
#if DEBUG_SHOW_NORMALS
        resources.normalsCmdBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipelineManager.getPipelineLayout("NormalDebug.frag.spv"), 0, normalsDescriptor.getHandle(), uniformOffset);
#else
        resources.normalsCmdBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipelineManager.getPipelineLayout("Normal.frag.spv"), 0, normalsDescriptor.getHandle(), uniformOffset);
#endif
        resources.normalsCmdBuffer.drawIndexed(indexBufferOffsets[i].numberOfEntries, 1, 0, 0, 0);

        resources.albedoCmdBuffer.bindVertexBuffers(0, 1, &resources.vertexBuffer.mBuffer, &bufferOffset);
        resources.albedoCmdBuffer.bindIndexBuffer(resources.indexBuffer.mBuffer, indexOffset, vk::IndexType::eUint32);
        resources.albedoCmdBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipelineManager.getPipelineLayout("Albedo.frag.spv"), 0, albedoDescriptor.getHandle(), uniformOffset );
        resources.albedoCmdBuffer.drawIndexed(indexBufferOffsets[i].numberOfEntries, 1, 0, 0, 0);
	}

    glm::vec4 pushConstants[5];
    pushConstants[0] = glm::vec4(spotLIghtOffsets[0].numberOfEntries);
    glm::mat4 currentView = getCurrentView();
    pushConstants[1] = currentView[0];
    pushConstants[2] = currentView[1];
    pushConstants[3] = currentView[2];
    pushConstants[4] = currentView[3];
    resources.compositeCmdBuffer.pushConstants(pipelineManager.getPipelineLayout("Composite.frag.spv"), vk::ShaderStageFlagBits::eFragment, 0, sizeof(glm::vec4) * 5, &pushConstants);
    resources.compositeCmdBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipelineManager.getPipelineLayout("Composite.frag.spv"), 0, compositeDescriptor.getHandle(), {} );
    resources.compositeCmdBuffer.draw(3,1,0,0);

    endFrameInternal();
}


void ThiefVKDevice::startFrameInternal() {

	frameResources[currentFrameBufferIndex].submissionID				= currentSubmissionID; // set the minimum we need to start recording command buffers.

    vk::CommandBufferBeginInfo primaryBeginInfo{};
    frameResources[currentFrameBufferIndex].primaryCmdBuffer.begin(primaryBeginInfo);

	// start the render pass so that we can begin recording in to the command buffers
	vk::RenderPassBeginInfo renderPassBegin{};
	renderPassBegin.framebuffer = frameBuffers[currentFrameBufferIndex];
	renderPassBegin.renderPass = mRenderPasses.RenderPass;
	vk::ClearValue colour[5]  = {vk::ClearValue(std::array<float, 4>{0.0f, 0.0f, 0.0f, 1.0f}) 
                                ,vk::ClearValue(std::array<float, 4>{1.0f, 1.0f, 1.0f, 1.0f}) 
                                ,vk::ClearValue(std::array<float, 4>{0.0f, 0.0f, 0.0f, 1.0f}) 
                                ,vk::ClearValue(std::array<float, 4>{0.0f, 0.0f, 0.0f, 1.0f})
                                ,vk::ClearValue(std::array<float, 4>{0.0f, 0.0f, 0.0f, 1.0f})};
    renderPassBegin.setClearValueCount(5);
	renderPassBegin.setPClearValues(colour);
	renderPassBegin.setRenderArea(vk::Rect2D{ { 0, 0 },
		{ static_cast<uint32_t>(mSwapChain.getSwapChainImageHeight()), static_cast<uint32_t>(mSwapChain.getSwapChainImageWidth()) } });

	// Begin the render pass
	frameResources[currentFrameBufferIndex].primaryCmdBuffer.beginRenderPass(renderPassBegin, vk::SubpassContents::eSecondaryCommandBuffers);

	startRecordingColourCmdBuffer();
	startRecordingAlbedoCmdBuffer();
	startRecordingNormalsCmdBuffer();
    startRecordingCompositeCmdBuffer();
}


// Just gather all the state we need here, then call Start RenderScene.
void ThiefVKDevice::draw(const geometry& geom) {
    mVertexBufferManager.addBufferElements(geom.verticies);
    mUniformBufferManager.addBufferElements({geom.object, geom.camera, geom.world});
    mIndexBufferManager.addBufferElements(geom.indicies);

    auto image = createTexture(geom.texturePath); 
    frameResources[currentFrameBufferIndex].textureImages.push_back(image);

    // create an image View as well.
    vk::ImageViewCreateInfo viewInfo{};
    viewInfo.setImage(image.mImage);
    viewInfo.setFormat(vk::Format::eR8G8B8A8Unorm);
    viewInfo.setViewType(vk::ImageViewType::e2D);
    viewInfo.setSubresourceRange({vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1});

    frameResources[currentFrameBufferIndex].textureImageViews.push_back(mDevice.createImageView(viewInfo));
}


void ThiefVKDevice::addSpotLights(std::vector<ThiefVKLight>& lights) {
    mSpotLightBufferManager.addBufferElements(lights);
}


void ThiefVKDevice::endFrameInternal() {
	perFrameResources& resources = frameResources[currentFrameBufferIndex];
	vk::CommandBuffer& primaryCmdBuffer = resources.primaryCmdBuffer;

	// end recording in to these as we're
	resources.colourCmdBuffer.end();
	resources.albedoCmdBuffer.end();
	resources.normalsCmdBuffer.end();
    resources.compositeCmdBuffer.end();

	// Execute the secondary cmd buffers in the primary
	primaryCmdBuffer.executeCommands(1, &resources.colourCmdBuffer);
	primaryCmdBuffer.nextSubpass(vk::SubpassContents::eSecondaryCommandBuffers);
	primaryCmdBuffer.executeCommands(1, &resources.normalsCmdBuffer);
    primaryCmdBuffer.nextSubpass(vk::SubpassContents::eSecondaryCommandBuffers);
    primaryCmdBuffer.executeCommands(1, &resources.albedoCmdBuffer);
	primaryCmdBuffer.nextSubpass(vk::SubpassContents::eSecondaryCommandBuffers);
    primaryCmdBuffer.executeCommands(1, &resources.compositeCmdBuffer);

	primaryCmdBuffer.endRenderPass();
	primaryCmdBuffer.end();

    transitionImageLayout(deferedTextures[currentFrameBufferIndex].colourImage, vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal);
    transitionImageLayout(deferedTextures[currentFrameBufferIndex].depthImage, vk::ImageLayout::eUndefined, vk::ImageLayout::eDepthStencilAttachmentOptimal);
    transitionImageLayout(deferedTextures[currentFrameBufferIndex].normalsImage, vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal);
    transitionImageLayout(deferedTextures[currentFrameBufferIndex].albedoImage, vk::ImageLayout::eUndefined, vk::ImageLayout::eColorAttachmentOptimal);
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
												  static_cast<uint32_t>(usage) & static_cast<uint32_t>(vk::BufferUsageFlagBits::eTransferSrc));

	MemoryManager.BindBuffer(buffer, bufferMem);

    return {buffer, bufferMem};
}


void ThiefVKDevice::destroyBuffer(ThiefVKBuffer& buffer) {
    mPendingFreeBuffers.push_back({currentSubmissionID, buffer});
}


void ThiefVKDevice::DestroyBufferInternal(ThiefVKBuffer& buffer) {
    if(buffer.mBuffer == vk::Buffer(nullptr)) {
        return;
    }

	MemoryManager.Free(buffer.mBufferMemory);

	mDevice.destroyBuffer(buffer.mBuffer);
    buffer.mBuffer = vk::Buffer(nullptr);
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
        DestroyImageView(images.albedoImageView);
        DestroyImage(images.albedoImage, images.albedoImageMemory);
    }
}


ThiefVKImage ThiefVKDevice::createTexture(const std::string& path) {
	if (auto image = mTextureCache[path]; image != ThiefVKImage{}) return image;  

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

	mTextureCache[path] = textureImage;

    return textureImage;
}


vk::Fence ThiefVKDevice::createFence() {
    vk::FenceCreateInfo info{};
    return mDevice.createFence(info);
}


void ThiefVKDevice::destroyFence(vk::Fence& fence) {
    mDevice.destroyFence(fence);
}


vk::Sampler ThiefVKDevice::createSampler() {
    vk::SamplerCreateInfo info{};
    info.setMagFilter(vk::Filter::eLinear);
    info.setMinFilter(vk::Filter::eLinear);
    info.setAddressModeU(vk::SamplerAddressMode::eClampToEdge);
    info.setAddressModeV(vk::SamplerAddressMode::eClampToEdge);
    info.setAddressModeW (vk::SamplerAddressMode::eClampToEdge);
    info.setAnisotropyEnable(true);
    info.setMaxAnisotropy(8);
    info.setBorderColor(vk::BorderColor::eIntOpaqueBlack);
    info.setCompareEnable(false);
    info.setCompareOp(vk::CompareOp::eAlways);
    info.setMipmapMode(vk::SamplerMipmapMode::eLinear);
    info.setMipLodBias(0.0f);
    info.setMinLod(0.0f);
    info.setMaxLod(0.0f);

    return mDevice.createSampler(info);
}


void ThiefVKDevice::destroySampler(vk::Sampler& theSampler) {
    mDevice.destroySampler(theSampler);
}


void ThiefVKDevice::createDeferedRenderTargetImageViews() {
    ThiefVKImageTextutres Result{};

    for(unsigned int swapImageCount = 0; swapImageCount < mSwapChain.getNumberOfSwapChainImages(); ++swapImageCount) {
        auto [colourImage, colourMemory]   = createImage(vk::Format::eR8G8B8A8Srgb, 
                                                         vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eInputAttachment | vk::ImageUsageFlagBits::eSampled, 
                                                         mSwapChain.getSwapChainImageWidth(), mSwapChain.getSwapChainImageHeight());

        auto [depthImage , depthMemory]    = createImage(vk::Format::eD32Sfloat,
                                                         vk::ImageUsageFlagBits::eDepthStencilAttachment | vk::ImageUsageFlagBits::eInputAttachment | vk::ImageUsageFlagBits::eSampled,
                                                         mSwapChain.getSwapChainImageWidth(), mSwapChain.getSwapChainImageHeight());

        auto [normalsImage, normalsMemory] = createImage(vk::Format::eR8G8B8A8Srgb,
                                                         vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eInputAttachment | vk::ImageUsageFlagBits::eSampled,
                                                         mSwapChain.getSwapChainImageWidth(), mSwapChain.getSwapChainImageHeight());

        auto [albedoImage, albedoMemory] =  createImage(vk::Format::eR8G8B8A8Srgb,
                                                         vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eInputAttachment | vk::ImageUsageFlagBits::eSampled,
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
        normalsViewInfo.setFormat(vk::Format::eR8G8B8A8Srgb);
        normalsViewInfo.setComponents(vk::ComponentMapping());
        normalsViewInfo.setSubresourceRange(vk::ImageSubresourceRange{vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1});

        vk::ImageViewCreateInfo albedoViewInfo{};
        albedoViewInfo.setImage(albedoImage);
        albedoViewInfo.setViewType(vk::ImageViewType::e2D);
        albedoViewInfo.setFormat(vk::Format::eR8G8B8A8Srgb);
        albedoViewInfo.setComponents(vk::ComponentMapping());
        albedoViewInfo.setSubresourceRange(vk::ImageSubresourceRange{vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1});


        vk::ImageView colourImageView  = mDevice.createImageView(colourViewInfo);
        vk::ImageView depthImageView   = mDevice.createImageView(depthViewInfo);
        vk::ImageView normalsImageView = mDevice.createImageView(normalsViewInfo);
        vk::ImageView albedoImageView  = mDevice.createImageView(albedoViewInfo);

        Result.colourImage          = colourImage;
        Result.colourImageView      = colourImageView;
        Result.colourImageMemory    = colourMemory;

        Result.depthImage           = depthImage;
        Result.depthImageView       = depthImageView;
        Result.depthImageMemory     = depthMemory;

        Result.normalsImage         = normalsImage;
        Result.normalsImageView     = normalsImageView;
        Result.normalsImageMemory   = normalsMemory;

        Result.albedoImage          = albedoImage;
        Result.albedoImageView      = albedoImageView;
        Result.albedoImageMemory         = albedoMemory;

        deferedTextures.push_back(Result);
    }
}


void ThiefVKDevice::createRenderPasses() {

    // Specify all attachments used in all subrenderpasses
    vk::AttachmentDescription colourPassAttachment{}; 
    colourPassAttachment.setFormat(vk::Format::eR8G8B8A8Srgb);
    colourPassAttachment.setLoadOp(vk::AttachmentLoadOp::eClear); // we are going to overwrite all pixles
    colourPassAttachment.setStoreOp(vk::AttachmentStoreOp::eDontCare);
    colourPassAttachment.setStencilLoadOp(vk::AttachmentLoadOp::eDontCare);
    colourPassAttachment.setStencilStoreOp(vk::AttachmentStoreOp::eDontCare);
    colourPassAttachment.setInitialLayout(vk::ImageLayout::eUndefined);
    colourPassAttachment.setFinalLayout(vk::ImageLayout::eColorAttachmentOptimal);

    vk::AttachmentDescription depthPassAttachment{};
    depthPassAttachment.setFormat(vk::Format::eD32Sfloat); // store in each pixel a 32bit depth value
    depthPassAttachment.setLoadOp(vk::AttachmentLoadOp::eClear); // we are going to overwrite all pixles
    depthPassAttachment.setStoreOp(vk::AttachmentStoreOp::eDontCare);
    depthPassAttachment.setStencilLoadOp(vk::AttachmentLoadOp::eDontCare);
    depthPassAttachment.setStencilStoreOp(vk::AttachmentStoreOp::eDontCare);
    depthPassAttachment.setInitialLayout(vk::ImageLayout::eUndefined); // write in a subpass then read in a subsequent one
    depthPassAttachment.setFinalLayout(vk::ImageLayout::eDepthStencilAttachmentOptimal);

    vk::AttachmentDescription normalsPassAttachment{};
    normalsPassAttachment.setFormat(vk::Format::eR8G8B8A8Srgb);
    normalsPassAttachment.setLoadOp(vk::AttachmentLoadOp::eClear);
    normalsPassAttachment.setStoreOp(vk::AttachmentStoreOp::eDontCare);
    normalsPassAttachment.setStencilLoadOp(vk::AttachmentLoadOp::eDontCare);
    normalsPassAttachment.setStencilStoreOp(vk::AttachmentStoreOp::eDontCare);
    normalsPassAttachment.setInitialLayout(vk::ImageLayout::eUndefined);
    normalsPassAttachment.setFinalLayout(vk::ImageLayout::eColorAttachmentOptimal); // these will be used in the subsqeuent light renderpass

    vk::AttachmentDescription albedoPassAttachment{};
    albedoPassAttachment.setFormat(vk::Format::eR8G8B8A8Srgb);
    albedoPassAttachment.setLoadOp(vk::AttachmentLoadOp::eClear);
    albedoPassAttachment.setStoreOp(vk::AttachmentStoreOp::eDontCare);
    albedoPassAttachment.setStencilLoadOp(vk::AttachmentLoadOp::eDontCare);
    albedoPassAttachment.setStencilStoreOp(vk::AttachmentStoreOp::eDontCare);
    albedoPassAttachment.setInitialLayout(vk::ImageLayout::eUndefined);
    albedoPassAttachment.setFinalLayout(vk::ImageLayout::eColorAttachmentOptimal); // these will be used in the subsqeuent light renderpass

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
    std::array<vk::AttachmentReference, 4> colourAttatchmentRefs{vk::AttachmentReference{0, vk::ImageLayout::eColorAttachmentOptimal}, 
                                                                vk::AttachmentReference{2, vk::ImageLayout::eColorAttachmentOptimal}, 
                                                                vk::AttachmentReference{3, vk::ImageLayout::eColorAttachmentOptimal},
                                                                vk::AttachmentReference{4, vk::ImageLayout::eColorAttachmentOptimal}};

    vk::AttachmentReference depthRef = {1, vk::ImageLayout::eDepthStencilAttachmentOptimal };

    vk::SubpassDescription colourPassDesc{};
    colourPassDesc.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics);
    colourPassDesc.setColorAttachmentCount(1);
    colourPassDesc.setPColorAttachments(&colourAttatchmentRefs[0]);
    colourPassDesc.setPDepthStencilAttachment(&depthRef);

    vk::SubpassDescription normalsPassDesc{};
    normalsPassDesc.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics);
    normalsPassDesc.setColorAttachmentCount(1);
    normalsPassDesc.setPColorAttachments(&colourAttatchmentRefs[1]);
    normalsPassDesc.setPDepthStencilAttachment(&depthRef);

    vk::SubpassDescription albedoPassDesc{};
    albedoPassDesc.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics);
    albedoPassDesc.setColorAttachmentCount(1);
    albedoPassDesc.setPColorAttachments(&colourAttatchmentRefs[2]);
    albedoPassDesc.setPDepthStencilAttachment(&depthRef);

    std::array<vk::AttachmentReference, 4> inputAttachments{vk::AttachmentReference{0, vk::ImageLayout::eShaderReadOnlyOptimal},
                                                            vk::AttachmentReference{1, vk::ImageLayout::eShaderReadOnlyOptimal},
                                                            vk::AttachmentReference{2, vk::ImageLayout::eShaderReadOnlyOptimal},
                                                            vk::AttachmentReference{3, vk::ImageLayout::eShaderReadOnlyOptimal}};

    vk::AttachmentReference swapChainAttachment{4, vk::ImageLayout::eColorAttachmentOptimal};

    vk::SubpassDescription compositPassDesc{};
    compositPassDesc.setPipelineBindPoint(vk::PipelineBindPoint::eGraphics);
    compositPassDesc.setInputAttachmentCount(inputAttachments.size());
    compositPassDesc.setPInputAttachments(inputAttachments.data());
    compositPassDesc.setColorAttachmentCount(1);
    compositPassDesc.setPColorAttachments(&swapChainAttachment);

    mRenderPasses.colourPass    = colourPassDesc;
    mRenderPasses.normalsPass   = normalsPassDesc;
    mRenderPasses.albedoPass    = albedoPassDesc;


    std::vector<vk::AttachmentDescription> allAttachments{colourPassAttachment, depthPassAttachment, normalsPassAttachment, albedoPassAttachment, swapChainImageAttachment};

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

    vk::SubpassDependency normalsToCompositeDepen{};
    normalsToCompositeDepen.setSrcSubpass(1);
    normalsToCompositeDepen.setDstSubpass(2);
    normalsToCompositeDepen.setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);
    normalsToCompositeDepen.setDstStageMask(vk::PipelineStageFlagBits::eFragmentShader);
    normalsToCompositeDepen.setSrcAccessMask(vk::AccessFlagBits::eColorAttachmentWrite);
    normalsToCompositeDepen.setDstAccessMask(vk::AccessFlagBits::eShaderRead);
    normalsToCompositeDepen.setDependencyFlags(vk::DependencyFlagBits::eByRegion);

    vk::SubpassDependency albedoToCompositeDepen{};
    albedoToCompositeDepen.setSrcSubpass(2);
    albedoToCompositeDepen.setDstSubpass(3);
    albedoToCompositeDepen.setSrcStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);
    albedoToCompositeDepen.setDstStageMask(vk::PipelineStageFlagBits::eFragmentShader);
    albedoToCompositeDepen.setSrcAccessMask(vk::AccessFlagBits::eColorAttachmentWrite);
    albedoToCompositeDepen.setDstAccessMask(vk::AccessFlagBits::eShaderRead);
    albedoToCompositeDepen.setDependencyFlags(vk::DependencyFlagBits::eByRegion);

    std::array<vk::SubpassDescription, 4> allSubpasses{colourPassDesc, normalsPassDesc, albedoPassDesc, compositPassDesc};
    std::array<vk::SubpassDependency, 4>  allSubpassDependancies{implicitFirstDepen, colourToCompositeDepen, albedoToCompositeDepen, normalsToCompositeDepen};

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

        std::array<vk::ImageView, 5> frameBufferAttatchments{deferedTextures[i].colourImageView
                                                          , deferedTextures[i].depthImageView
                                                          , deferedTextures[i].normalsImageView
                                                          , deferedTextures[i].albedoImageView
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
    imageSamplerrDescPoolSize.setDescriptorCount(40); // start with 5 we can allways allocate another pool if we later need more.

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
	pipelineDesc.vertexShaderName	 = "BasicTransform.vert.spv";
	pipelineDesc.fragmentShaderName	 = "Colour.frag.spv";
	pipelineDesc.renderPass			 = mRenderPasses.RenderPass;
    pipelineDesc.subpassIndex        = 0;
	pipelineDesc.renderTargetOffsetX = 0;
	pipelineDesc.renderTargetOffsetY = 0;
	pipelineDesc.renderTargetHeight  = mSwapChain.getSwapChainImageHeight();
	pipelineDesc.renderTargetWidth   = mSwapChain.getSwapChainImageWidth();
    pipelineDesc.useDepthTest        = true;
    pipelineDesc.useBackFaceCulling  = true;

	colourCmdBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipelineManager.getPipeLine(pipelineDesc));

	return colourCmdBuffer;
}


vk::CommandBuffer& ThiefVKDevice::startRecordingAlbedoCmdBuffer() {
	vk::CommandBuffer& depthCmdBuffer = frameResources[currentFrameBufferIndex].albedoCmdBuffer;

    vk::CommandBufferInheritanceInfo inheritanceInfo{};
    inheritanceInfo.setRenderPass(mRenderPasses.RenderPass);
    inheritanceInfo.setSubpass(2);
    inheritanceInfo.setFramebuffer(frameBuffers[currentFrameBufferIndex]);

	vk::CommandBufferBeginInfo beginInfo{};
    beginInfo.setPInheritanceInfo(&inheritanceInfo);
	beginInfo.setFlags(vk::CommandBufferUsageFlagBits::eRenderPassContinue);

	// start recording commands in to the buffer
	depthCmdBuffer.begin(beginInfo);

	ThiefVKPipelineDescription pipelineDesc{};
	pipelineDesc.vertexShaderName    = "Albedo.vert.spv";
	pipelineDesc.fragmentShaderName	 = "Albedo.frag.spv";
	pipelineDesc.renderPass			 = mRenderPasses.RenderPass;
    pipelineDesc.subpassIndex        = 2;
	pipelineDesc.renderTargetOffsetX = 0;
	pipelineDesc.renderTargetOffsetY = 0;
	pipelineDesc.renderTargetHeight  = mSwapChain.getSwapChainImageHeight();
	pipelineDesc.renderTargetWidth   = mSwapChain.getSwapChainImageWidth();
    pipelineDesc.useDepthTest        = true;
    pipelineDesc.useBackFaceCulling  = true;

	depthCmdBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipelineManager.getPipeLine(pipelineDesc));


	return depthCmdBuffer;
}


vk::CommandBuffer& ThiefVKDevice::startRecordingNormalsCmdBuffer() {
	vk::CommandBuffer& normalsCmdBuffer = frameResources[currentFrameBufferIndex].normalsCmdBuffer;

    vk::CommandBufferInheritanceInfo inheritanceInfo{};
    inheritanceInfo.setRenderPass(mRenderPasses.RenderPass);
    inheritanceInfo.setSubpass(1);
    inheritanceInfo.setFramebuffer(frameBuffers[currentFrameBufferIndex]);

	vk::CommandBufferBeginInfo beginInfo{};
    beginInfo.setPInheritanceInfo(&inheritanceInfo);
	beginInfo.setFlags(vk::CommandBufferUsageFlagBits::eRenderPassContinue);

	// start recording commands in to the buffer
	normalsCmdBuffer.begin(beginInfo);

	ThiefVKPipelineDescription pipelineDesc{};
#if DEBUG_SHOW_NORMALS
	pipelineDesc.vertexShaderName    = "NormalDebug.vert.spv";
    pipelineDesc.geometryShaderName  = "NormalDebug.geom.spv";
	pipelineDesc.fragmentShaderName	 = "NormalDebug.frag.spv";
#else
    pipelineDesc.vertexShaderName    = "Normal.vert.spv";
    pipelineDesc.fragmentShaderName  = "Normal.frag.spv";
#endif
    pipelineDesc.renderPass			 = mRenderPasses.RenderPass;
    pipelineDesc.subpassIndex        = 1;
	pipelineDesc.renderTargetOffsetX = 0;
	pipelineDesc.renderTargetOffsetY = 0;
	pipelineDesc.renderTargetHeight  = mSwapChain.getSwapChainImageHeight();
	pipelineDesc.renderTargetWidth   = mSwapChain.getSwapChainImageWidth();
    pipelineDesc.useDepthTest        = true;
    pipelineDesc.useBackFaceCulling  = true;

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
    pipelineDesc.vertexShaderName    = "Composite.vert.spv";
    pipelineDesc.fragmentShaderName  = "Composite.frag.spv";
    pipelineDesc.renderPass          = mRenderPasses.RenderPass;
    pipelineDesc.subpassIndex        = 3;
    pipelineDesc.renderTargetOffsetX = 0;
    pipelineDesc.renderTargetOffsetY = 0;
    pipelineDesc.renderTargetHeight  = mSwapChain.getSwapChainImageHeight();
    pipelineDesc.renderTargetWidth   = mSwapChain.getSwapChainImageWidth();
    pipelineDesc.useDepthTest        = false;
    pipelineDesc.useBackFaceCulling  = false;

    compositeCmdBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipelineManager.getPipeLine(pipelineDesc));

    return compositeCmdBuffer;
}


void ThiefVKDevice::destroyPerFrameResources(perFrameResources& resources) {
    mDevice.destroyFence(resources.frameFinished);
    mDevice.destroySemaphore(resources.swapChainImageAvailable);
    mDevice.destroySemaphore(resources.imageRendered);

	for (auto& descSet : resources.DescSets) {
		DescriptorManager.destroyDescriptorSet(descSet);
	}
    resources.DescSets.clear();

    for(auto& buffer : resources.stagingBuffers)  {
        destroyBuffer(buffer);;
    }
    resources.stagingBuffers.clear();

    for(auto& imageView : resources.textureImageViews) {
        mDevice.destroyImageView(imageView);
    }
    resources.textureImageViews.clear();
}


void ThiefVKDevice::DestroyPendingBuffers() {
    for(unsigned int i = 0; i < mPendingFreeBuffers.size(); ++i) {
        auto& [submissionID, buffer] = mPendingFreeBuffers.back();
        if(submissionID <= finishedSubmissionID) {
            DestroyBufferInternal(buffer);
            mPendingFreeBuffers.pop_back();
        } else {
            break;
        }
    }
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


ThiefVKDescriptorSetDescription ThiefVKDevice::getDescriptorSetDescription(const std::string shader,  const uint32_t colourImageViewIndex) {
    ThiefVKDescriptorSetDescription descSets{};

    ThiefVKDescriptorDescription uboDescriptorLayout{};
    uboDescriptorLayout.mDescriptor.mBinding = 0;
    uboDescriptorLayout.mDescriptor.mDescType = shader.find("Composite") != std::string::npos ? vk::DescriptorType::eUniformBuffer : vk::DescriptorType::eUniformBufferDynamic;
    uboDescriptorLayout.mDescriptor.mShaderStage  = shader.find("Composite") != std::string::npos ? vk::ShaderStageFlagBits::eFragment : vk::ShaderStageFlagBits::eVertex;
    uboDescriptorLayout.mResource = shader.find("Composite") != std::string::npos ? &frameResources[currentFrameBufferIndex].spotLightBuffer.mBuffer : &frameResources[currentFrameBufferIndex].uniformBuffer.mBuffer;

    descSets.push_back(uboDescriptorLayout);

    if(shader.find("Colour") != std::string::npos) {
        ThiefVKDescriptorDescription imageSamplerDescriptorLayout{};
        imageSamplerDescriptorLayout.mDescriptor.mBinding = 1;
        imageSamplerDescriptorLayout.mDescriptor.mDescType = vk::DescriptorType::eCombinedImageSampler;
        imageSamplerDescriptorLayout.mDescriptor.mShaderStage = vk::ShaderStageFlagBits::eFragment;
		imageSamplerDescriptorLayout.mResource = &frameResources[currentFrameBufferIndex].textureImageViews[colourImageViewIndex];

        descSets.push_back(imageSamplerDescriptorLayout);
    } else if(shader.find("Composite") != std::string::npos) {
        for(unsigned int i = 1; i < 5; ++i) {
            ThiefVKDescriptorDescription imageSamplerDescriptorLayout{};
            imageSamplerDescriptorLayout.mDescriptor.mBinding = i;
            imageSamplerDescriptorLayout.mDescriptor.mDescType = vk::DescriptorType::eCombinedImageSampler;
            imageSamplerDescriptorLayout.mDescriptor.mShaderStage = vk::ShaderStageFlagBits::eFragment;
            imageSamplerDescriptorLayout.mResource = [this, i]() -> vk::ImageView*{
                switch(i) {
                    case 1:
                        return &this->deferedTextures[currentFrameBufferIndex].colourImageView;
                    case 2:
                        return &this->deferedTextures[currentFrameBufferIndex].depthImageView;
                    case 3:
                        return &this->deferedTextures[currentFrameBufferIndex].normalsImageView;
                    case 4:
                        return &this->deferedTextures[currentFrameBufferIndex].albedoImageView;
                }
                return nullptr;
            }();

            descSets.push_back(imageSamplerDescriptorLayout);
        }

    }

    return descSets;
}


void ThiefVKDevice::setCurrentView(glm::mat4 viewMatrix) {
    mCurrentView = viewMatrix;
}


glm::mat4 ThiefVKDevice::getCurrentView() const {
    return mCurrentView;
} 
