#ifndef THIEFVKDESCRIPTORMANAGER_HPP
#define THIEFVKDESCRIPTORMANAGER_HPP

#include "ThiefVKPipelineManager.hpp"

#include <vulkan/vulkan.hpp>

#include <vector>
#include <map>
#include <variant>

class ThiefVKDevice;
class ThiefVKDescriptorManager;

struct ThiefVKDescriptor {
	vk::DescriptorType mDescType;
	vk::ShaderStageFlagBits mShaderStage;
	uint32_t mbinding;
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

	ThiefVKDescriptorSet(const vk::DescriptorSet& descSet, const ThiefVKDescriptorSetDescription& desc) : mDescSet{ descSet }, mDesc{ desc } {}
	vk::DescriptorSet& getHandle() { return mDescSet; }

private:
	vk::DescriptorSet mDescSet;
	ThiefVKDescriptorSetDescription mDesc;
};


class ThiefVKDescriptorManager {
public:
	ThiefVKDescriptorManager(ThiefVKDevice&);
	~ThiefVKDescriptorManager();

	ThiefVKDescriptorSet getDescriptorSet(const ThiefVKDescriptorSetDescription&);
	vk::DescriptorSetLayout getDescriptorSetLayout(const ThiefVKDescriptorSetDescription&);

	void destroyDescriptorSet(const ThiefVKDescriptorSet&);
private:

	vk::DescriptorSet createDescriptorSet(const ThiefVKDescriptorSetDescription&);
	vk::DescriptorSetLayout createDescriptorSetLayout(const ThiefVKDescriptorSetDescription&);
	void writeDescriptorSet(ThiefVKDescriptorSet&);
	vk::DescriptorPool allocateNewPool();
	std::vector<vk::DescriptorSetLayoutBinding> extractLayoutBindings(const ThiefVKDescriptorSetDescription&);

	ThiefVKDevice& mDev;

	std::vector<vk::DescriptorPool> mPools;

	std::map<ThiefVKDescriptorSetDescription, std::pair<vk::DescriptorSetLayout, std::vector<vk::DescriptorSet>>> mFreeCache;
};


#endif