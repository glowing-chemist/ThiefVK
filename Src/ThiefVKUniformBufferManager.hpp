#ifndef THIEFVKUNIFORMBUFFERMANAGER_HPP
#define THIEFVKUNIFORMBUFFERMANAGER_HPP

#include <vulkan/vulkan.hpp>
#include <glm/glm.hpp>

#include <vector>
#include <tuple>

#include "ThiefVKMemoryManager.hpp"

#define kUniformBufferSize sizeof(glm::mat4) * 30

class ThiefVKDevice;

class ThiefVKUnifromBufferManager {
public:

	ThiefVKUnifromBufferManager(ThiefVKDevice& Device);
	void Destroy();

	std::pair<vk::Buffer&, uint32_t> addBufferElements(std::vector<glm::mat4>& elements);

	std::vector<std::pair<vk::Buffer, Allocation>> flushBufferUploads();

private:
	void startNewBuffer();
	void uploadBuffer();

	ThiefVKDevice& mDevice;

	std::vector<glm::mat4> mUniformBuffer;

	vk::Buffer mDeviceUniformBuffer;
	Allocation mUniformBufferMemory;

	std::vector<std::pair<vk::Buffer, Allocation>> mFilledBuffers;
};

#endif
