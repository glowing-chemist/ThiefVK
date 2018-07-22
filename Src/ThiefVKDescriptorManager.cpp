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
	for(auto& [key, descriptorSet] : mFreeCache) {
		for(auto& descriptor : descriptorSet.second) {
			for(auto& sampler : descriptor.mSamplers) {
				mDev.destroySampler(sampler);
			}
		}
	}
	for (auto& pool : mPools) {
		mDev.getLogicalDevice()->destroyDescriptorPool(pool);
	}
}


ThiefVKDescriptorSet ThiefVKDescriptorManager::getDescriptorSet(const ThiefVKDescriptorSetDescription& description) {
	ThiefVKDescriptorSet set{};
	
	if (!mFreeCache[description].second.empty()) {
		set = mFreeCache[description].second.back();
		mFreeCache[description].second.pop_back();
	} else {
		set = createDescriptorSet(description);
	}

	writeDescriptorSet(set);
	
	return set;
}


ThiefVKDescriptorSet ThiefVKDescriptorManager::createDescriptorSet(const ThiefVKDescriptorSetDescription& description) {
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

				std::vector<vk::Sampler> samplers{};
				for(const auto& desc : description) {
					if(desc.mDescriptor.mDescType == vk::DescriptorType::eCombinedImageSampler)
						samplers.push_back(mDev.createSampler());
				}

				return {descriptorSet[0], description, samplers};
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
	mFreeCache[descSet.mDesc].second.push_back(descSet);
}


void ThiefVKDescriptorManager::writeDescriptorSet(ThiefVKDescriptorSet& descSet) {
	std::vector<vk::WriteDescriptorSet> descSetWrites{};
	uint32_t samplerCount = 0;

	for (const auto& description : descSet.mDesc) {
		vk::WriteDescriptorSet descWrite{};
		descWrite.setDstBinding(1);
		descWrite.setDescriptorCount(1);
		descWrite.setDescriptorType(description.mDescriptor.mDescType);
		descWrite.setDstSet(descSet.mDescSet);

		vk::DescriptorImageInfo imageInfo{};

		vk::DescriptorBufferInfo bufferInfo{};


		switch (description.mResource.index()) {
			case 0:
				imageInfo.setSampler(descSet.mSamplers[samplerCount]);
				imageInfo.setImageView(*std::get<vk::ImageView*>(description.mResource));
				imageInfo.setImageLayout(vk::ImageLayout::eShaderReadOnlyOptimal);
				descWrite.setPImageInfo(&imageInfo);
				++samplerCount;
				break;
			case 1:
				bufferInfo.setBuffer(*std::get<vk::Buffer*>(description.mResource));
				bufferInfo.setOffset(0);
				bufferInfo.setRange(VK_WHOLE_SIZE);
				descWrite.setPBufferInfo(&bufferInfo);
				break;
			default:
				break;
		}

		descSetWrites.push_back(descWrite);
	}

	mDev.getLogicalDevice()->updateDescriptorSets(descSetWrites, {});
}


vk::DescriptorPool ThiefVKDescriptorManager::allocateNewPool() {
	return mDev.createDescriptorPool();
}


std::vector<vk::DescriptorSetLayoutBinding> ThiefVKDescriptorManager::extractLayoutBindings(const ThiefVKDescriptorSetDescription& description) const {
	std::vector<vk::DescriptorSetLayoutBinding> bindings{};
	
	for (const auto& ThiefDescription : description) {
		vk::DescriptorSetLayoutBinding binding{};
		binding.setBinding(ThiefDescription.mDescriptor.mBinding);
		binding.setDescriptorCount(1);
		binding.setDescriptorType(ThiefDescription.mDescriptor.mDescType);
		binding.setStageFlags(ThiefDescription.mDescriptor.mShaderStage);

		bindings.push_back(binding);
	}

	return bindings;
}