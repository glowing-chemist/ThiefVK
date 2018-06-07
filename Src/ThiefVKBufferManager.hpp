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

	void addBufferElements(std::vector<T>& elements);

	std::pair<ThiefVKBuffer, ThiefVKBuffer> flushBufferUploads();
	std::vector<uint32_t> getBufferOffsets() const;

private:
	std::pair<ThiefVKBuffer, ThiefVKBuffer> uploadBuffer(vk::Buffer& buffer, Allocation alloc);

	ThiefVKDevice& mDevice;

	vk::BufferUsageFlags mUsage;

	std::vector<T> mUniformBuffer;
	std::vector<uint32_t> mOffsets;
};

#endif
