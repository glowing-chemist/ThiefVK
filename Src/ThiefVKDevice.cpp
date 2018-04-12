#include "ThiefVKDevice.hpp"
#include "ThiefVKInstance.hpp"

#include <array>
#include <iostream>

// Vertex member functions
vk::VertexInputBindingDescription Vertex::getBindingDesc() {

    vk::VertexInputBindingDescription desc{};
    desc.setStride(sizeof(Vertex));
    desc.setBinding(0);
    desc.setInputRate(vk::VertexInputRate::eVertex);

    return desc;
}

std::array<vk::VertexInputAttributeDescription, 2> Vertex::getAttribDesc() {

    vk::VertexInputAttributeDescription atribDescPos{};
    atribDescPos.setBinding(0);
    atribDescPos.setLocation(0);
    atribDescPos.setFormat(vk::Format::eR32G32B32Sfloat);
    atribDescPos.setOffset(offsetof(Vertex, pos));

    vk::VertexInputAttributeDescription atribDescTex{};
    atribDescTex.setBinding(0);
    atribDescTex.setLocation(1);
    atribDescTex.setFormat(vk::Format::eR32G32Sfloat);
    atribDescTex.setOffset(offsetof(Vertex, tex));

    return {atribDescPos, atribDescTex};
}

// ThiefVKDeviceMemberFunctions

ThiefVKDevice::ThiefVKDevice(std::pair<vk::PhysicalDevice, vk::Device> Devices, vk::SurfaceKHR surface, GLFWwindow * window) :
    mPhysDev{std::get<0>(Devices)}, mDevice{std::get<1>(Devices)}, mWindowSurface{surface}, mWindow{window}, mSwapChain{mDevice, mPhysDev, surface, window}
{
    const QueueIndicies queueIndices = getAvailableQueues(mWindowSurface, mPhysDev);

    mGraphicsQueue = mDevice.getQueue(queueIndices.GraphicsQueueIndex, 0);
    mPresentQueue  = mDevice.getQueue(queueIndices.PresentQueueIndex, 0);
    mComputeQueue  = mDevice.getQueue(queueIndices.ComputeQueueIndex, 0);

   ThiefVKMemoryManager memoryManager(&mPhysDev, &mDevice);
   MemoryManager = memoryManager;
}


ThiefVKDevice::~ThiefVKDevice() {
    DestroyFrameBuffers();
    DestroyAllImageTextures();
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


std::pair<vk::Image, Allocation> ThiefVKDevice::createColourImage(const unsigned int width, const unsigned int height) {
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


std::pair<vk::Image, Allocation> ThiefVKDevice::createDepthImage(const unsigned int width, const unsigned int height) {
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


std::pair<vk::Image, Allocation> ThiefVKDevice::createNormalsImage(const unsigned int width, const unsigned int height) {
    vk::ImageCreateInfo imageInfo{};
    imageInfo.setExtent({width, height, 1});
    imageInfo.setFormat(vk::Format::eR8G8B8A8Sint);
    imageInfo.setInitialLayout(vk::ImageLayout::eUndefined);
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
    graphicsCommandPool = mDevice.createCommandPool(graphicsPoolInfo);

    vk::CommandPoolCreateInfo computePoolInfo{};
    computePoolInfo.setQueueFamilyIndex(queueIndicies.ComputeQueueIndex);
    computeCommandPool = mDevice.createCommandPool(computePoolInfo);
}


vk::CommandBuffer ThiefVKDevice::beginSingleUseGraphicsCommandBuffer() {
    vk::CommandBufferAllocateInfo allocInfo{};
    allocInfo.setCommandBufferCount(1);
    allocInfo.setLevel(vk::CommandBufferLevel::ePrimary); // this will probablybe used for image format transitions
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


void ThiefVKDevice::createCommandBuffers() {
	vk::CommandBufferAllocateInfo primaryAllocInfo{};
	primaryAllocInfo.setCommandPool(graphicsCommandPool);
	primaryAllocInfo.setLevel(vk::CommandBufferLevel::ePrimary);
	primaryAllocInfo.setCommandBufferCount(frameBuffers.size());

	freePrimaryCommanBuffers =  mDevice.allocateCommandBuffers(primaryAllocInfo);

	vk::CommandBufferAllocateInfo secondaryAllocInfo{};
	secondaryAllocInfo.setCommandPool(graphicsCommandPool);
	secondaryAllocInfo.setLevel(vk::CommandBufferLevel::eSecondary);
	secondaryAllocInfo.setCommandBufferCount(frameBuffers.size() * 4); // one for colour, depth, normals and light

	freeSecondaryCommanBuffers = mDevice.allocateCommandBuffers(secondaryAllocInfo);
}


void ThiefVKDevice::createSemaphores() {

}
