#ifndef THIEFVKUNIFORMBUFFERMANAGER_HPP
#define THIEFVKUNIFORMBUFFERMANAGER_HPP

#include <vulkan/vulkan.hpp>
#include <glm/glm.hpp>

#include <vector>
#include <tuple>

#include "ThiefVKMemoryManager.hpp"

class ThiefVKDevice;

struct entryInfo {
	uint32_t offset;
	uint64_t numberOfEntries;
};

template<typename T>
class ThiefVKBufferManager {
public:

	ThiefVKBufferManager(ThiefVKDevice& Device, vk::BufferUsageFlags usage);

	void addBufferElements(const std::vector<T>& elements);

	std::pair<ThiefVKBuffer, ThiefVKBuffer> flushBufferUploads();
	std::vector<entryInfo> getBufferOffsets();

	bool bufferHasChanged() const;

private:
	std::pair<ThiefVKBuffer, ThiefVKBuffer> uploadBuffer(ThiefVKBuffer&);

	ThiefVKDevice& mDevice;

	vk::BufferUsageFlags mUsage;

	std::vector<T> mBuffer;
	std::vector<entryInfo> mEntries;

	std::vector<T> mPreviousBuffer;
};

#endif
