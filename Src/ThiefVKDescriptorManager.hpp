#ifndef THIEFVKDESCRIPTORMANAGER_HPP
#define THIEFVKDESCRIPTORMANAGER_HPP

#include "ThiefVKPipelineManager.hpp"

#include <vulkan/vulkan.hpp>

#include <vector>
#include <map>
#include <variant>

class ThiefVKDevice;

struct ThiefVKDescriptor {
	vk::DescriptorType mDescType;
	uint32_t mbinding;
};

struct ThiefVKDescriptorDescription {
	ThiefVKDescriptor mDescriptor;
	std::variant<vk::ImageView*, vk::Buffer*> mResource;
};

using ThiefVKDescriptorSetDescription = std::vector<ThiefVKDescriptorDescription>;



class ThiefVKDescriptorManager {
public:
	ThiefVKDescriptorManager(ThiefVKDevice&);
	~ThiefVKDescriptorManager();

	vk::DescriptorSet getDescriptorSet(const ThiefVKDescriptorSetDescription&);
private:

	std::map<ThiefVKDescriptorSetDescription, std::pair<vk::DescriptorSetLayout, std::vector<vk::DescriptorSet>>> mFreeCache;
	std::map<ThiefVKDescriptorSetDescription, std::pair<vk::DescriptorSetLayout, std::vector<vk::DescriptorSet>>> mUsedCache;
};


#endif