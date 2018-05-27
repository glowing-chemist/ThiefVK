#ifndef THIEFVKUNIFORMBUFFERMANAGER_HPP
#define THIEFVKUNIFORMBUFFERMANAGER_HPP

#include <vulkan/vulkan.hpp>
#include <glm/glm.hpp>

#include <vector>
#include <tuple>

#include "ThiefVKMemoryManager.hpp"

class ThiefVKDevice;

template<typename T>
class ThiefVKBufferManager {
public:

        ThiefVKBufferManager(ThiefVKDevice& Device, vk::BufferUsageFlags usage);
	void Destroy();

        std::pair<vk::Buffer&, uint32_t> addBufferElements(std::vector<T>& elements);

	std::vector<std::pair<vk::Buffer, Allocation>> flushBufferUploads();

private:
	void startNewBuffer();
	void uploadBuffer();

	ThiefVKDevice& mDevice;

        vk::BufferUsageFlags mUsage;

        std::vector<T> mUniformBuffer;
        size_t mMaxBufferSize;

	vk::Buffer mDeviceUniformBuffer;
	Allocation mUniformBufferMemory;

	std::vector<std::pair<vk::Buffer, Allocation>> mFilledBuffers;
};

#endif
