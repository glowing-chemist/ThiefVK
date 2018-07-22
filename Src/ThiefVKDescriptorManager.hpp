#ifndef THIEFVKDESCRIPTORMANAGER_HPP
#define THIEFVKDESCRIPTORMANAGER_HPP

#include <vulkan/vulkan.hpp>

#include <vector>
#include <map>
#include <variant>

class ThiefVKDevice;
class ThiefVKDescriptorManager;

struct ThiefVKDescriptor {
	vk::DescriptorType mDescType;
	vk::ShaderStageFlagBits mShaderStage;
	uint32_t mBinding;
};

struct ThiefVKDescriptorDescription {
	ThiefVKDescriptor mDescriptor;
	std::variant<vk::ImageView*, vk::Buffer*> mResource;
};
bool operator<(const ThiefVKDescriptorDescription&, const ThiefVKDescriptorDescription&);


using ThiefVKDescriptorSetDescription = std::vector<ThiefVKDescriptorDescription>;


class ThiefVKDescriptorSet {
public:
	friend ThiefVKDescriptorManager;

	ThiefVKDescriptorSet() = default;
	ThiefVKDescriptorSet(const vk::DescriptorSet& descSet, const ThiefVKDescriptorSetDescription& desc, std::vector<vk::Sampler>& samplers) : 
		mDescSet{ descSet }, 
		mDesc{ desc }, 
		mSamplers{ samplers } {}

	vk::DescriptorSet& getHandle() { return mDescSet; }

private:
	vk::DescriptorSet mDescSet;
	ThiefVKDescriptorSetDescription mDesc;
	std::vector<vk::Sampler> mSamplers;
};


class ThiefVKDescriptorManager {
public:
	ThiefVKDescriptorManager(ThiefVKDevice&);
	void Destroy();

	ThiefVKDescriptorSet getDescriptorSet(const ThiefVKDescriptorSetDescription&);
	vk::DescriptorSetLayout getDescriptorSetLayout(const ThiefVKDescriptorSetDescription&);

	void destroyDescriptorSet(const ThiefVKDescriptorSet&);
private:

	ThiefVKDescriptorSet createDescriptorSet(const ThiefVKDescriptorSetDescription&);
	vk::DescriptorSetLayout createDescriptorSetLayout(const ThiefVKDescriptorSetDescription&);
	void writeDescriptorSet(ThiefVKDescriptorSet&);
	vk::DescriptorPool allocateNewPool();
	std::vector<vk::DescriptorSetLayoutBinding> extractLayoutBindings(const ThiefVKDescriptorSetDescription&) const ;

	ThiefVKDevice& mDev;

	std::vector<vk::DescriptorPool> mPools;

	std::map<ThiefVKDescriptorSetDescription, std::pair<vk::DescriptorSetLayout, std::vector<ThiefVKDescriptorSet>>> mFreeCache;
};


#endif
