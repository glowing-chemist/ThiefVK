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

	void addBufferElements(const std::vector<T>& elements);

	std::pair<ThiefVKBuffer, ThiefVKBuffer> flushBufferUploads();
	std::vector<uint32_t> getBufferOffsets();

	bool bufferHasChanged() const;

private:
	std::pair<ThiefVKBuffer, ThiefVKBuffer> uploadBuffer(vk::Buffer& buffer, Allocation alloc);

	ThiefVKDevice& mDevice;

	vk::BufferUsageFlags mUsage;

	std::vector<T> mBuffer;
	std::vector<uint32_t> mOffsets;

	std::vector<T> mPreviousBuffer;
};

#endif
