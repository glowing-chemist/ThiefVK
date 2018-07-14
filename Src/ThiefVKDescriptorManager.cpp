#include "ThiefVKDescriptorManager.hpp"
#include "ThiefVKDevice.hpp"

#include <iostream>

bool operator<(const ThiefVKDescriptorDescription& lhs, const ThiefVKDescriptorDescription& rhs) {
	return static_cast<int>(lhs.mDescriptor.mDescType) < static_cast<int>(rhs.mDescriptor.mDescType) &&
		lhs.mDescriptor.mBinding < rhs.mDescriptor.mBinding;
}


ThiefVKDescriptorManager::ThiefVKDescriptorManager(ThiefVKDevice& device) : mDev{ device } {
	// allocate the initial pool.
	mPools.push_back(allocateNewPool());
}


void ThiefVKDescriptorManager::Destroy() {
	for (auto& pool : mPools) {
		mDev.getLogicalDevice()->destroyDescriptorPool(pool);
	}
}


ThiefVKDescriptorSet ThiefVKDescriptorManager::getDescriptorSet(const ThiefVKDescriptorSetDescription& description) {
	vk::DescriptorSet set{};
	
	if (!mFreeCache[description].second.empty()) {
		set = mFreeCache[description].second.back();
		mFreeCache[description].second.pop_back();
	} else {
		set = createDescriptorSet(description);
	}
	ThiefVKDescriptorSet descSet = { set, description };

	writeDescriptorSet(descSet);
	
	return descSet;
}


vk::DescriptorSet ThiefVKDescriptorManager::createDescriptorSet(const ThiefVKDescriptorSetDescription& description) {
	vk::DescriptorSetLayout layout;

	if (mFreeCache[description].first != vk::DescriptorSetLayout(nullptr)) {
		layout = mFreeCache[description].first;
	} 
	else {
		layout = createDescriptorSetLayout(description);
		mFreeCache[description].first = layout;
	}
	for (;;) {
		for (auto& pool : mPools) {
			vk::DescriptorSetAllocateInfo allocInfo{};
			allocInfo.setDescriptorPool(pool);
			allocInfo.setDescriptorSetCount(1);
			allocInfo.setPSetLayouts(&layout);

			try {
				auto descriptorSet = mDev.getLogicalDevice()->allocateDescriptorSets(allocInfo);

				return descriptorSet[0];
			}
			catch (...) {
				std::cerr << "pool exhausted trying next descriptor pool \n";
			}
		}
		vk::DescriptorPool newPool = allocateNewPool();
		mPools.insert(mPools.begin(), newPool);
	}
}


vk::DescriptorSetLayout ThiefVKDescriptorManager::getDescriptorSetLayout(const ThiefVKDescriptorSetDescription& description) {
	if (const auto layout = mFreeCache[description].first; layout != vk::DescriptorSetLayout{ nullptr }) return layout;

	else {
		vk::DescriptorSetLayout createdLayout = createDescriptorSetLayout(description);
		mFreeCache[description].first = createdLayout;

		return createdLayout;
	}
}


vk::DescriptorSetLayout ThiefVKDescriptorManager::createDescriptorSetLayout(const ThiefVKDescriptorSetDescription& description) {
	std::vector<vk::DescriptorSetLayoutBinding> layoutBindings = extractLayoutBindings(description);

	vk::DescriptorSetLayoutCreateInfo info{};
	info.setPBindings(layoutBindings.data());
	info.setBindingCount(layoutBindings.size());

	return mDev.getLogicalDevice()->createDescriptorSetLayout(info);
}


void ThiefVKDescriptorManager::destroyDescriptorSet(const ThiefVKDescriptorSet& descSet) {

}


void ThiefVKDescriptorManager::writeDescriptorSet(ThiefVKDescriptorSet& descSet) {

}


vk::DescriptorPool ThiefVKDescriptorManager::allocateNewPool() {
	// crerate the descriptor set pools for uniform buffers and combined image samplers
	vk::DescriptorPoolSize uniformBufferDescPoolSize{};
	uniformBufferDescPoolSize.setType(vk::DescriptorType::eUniformBuffer);
	uniformBufferDescPoolSize.setDescriptorCount(15); // start with 5 we can allways allocate another pool if we later need more.

	vk::DescriptorPoolSize imageSamplerrDescPoolSize{};
	imageSamplerrDescPoolSize.setType(vk::DescriptorType::eCombinedImageSampler);
	imageSamplerrDescPoolSize.setDescriptorCount(15); // start with 5 we can allways allocate another pool if we later need more.

	std::array<vk::DescriptorPoolSize, 2> descPoolSizes{ uniformBufferDescPoolSize, imageSamplerrDescPoolSize };

	vk::DescriptorPoolCreateInfo uniformBufferDescPoolInfo{};
	uniformBufferDescPoolInfo.setPoolSizeCount(descPoolSizes.size()); // two pools one for uniform buffers and one for combined image samplers
	uniformBufferDescPoolInfo.setPPoolSizes(descPoolSizes.data());
	uniformBufferDescPoolInfo.setMaxSets(30);

	return mDev.getLogicalDevice()->createDescriptorPool(uniformBufferDescPoolInfo);
}


std::vector<vk::DescriptorSetLayoutBinding> ThiefVKDescriptorManager::extractLayoutBindings(const ThiefVKDescriptorSetDescription& description) {
	std::vector<vk::DescriptorSetLayoutBinding> bindings(description.size());
	
	for (const auto& ThiefDescription : description) {
		vk::DescriptorSetLayoutBinding binding{};
		binding.setDescriptorCount(1);
		binding.setDescriptorType(ThiefDescription.mDescriptor.mDescType);
		binding.setStageFlags(ThiefDescription.mDescriptor.mShaderStage);

		bindings.push_back(binding);
	}

	return bindings;
}