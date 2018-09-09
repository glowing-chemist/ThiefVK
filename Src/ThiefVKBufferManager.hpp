#ifndef THIEFVKUNIFORMBUFFERMANAGER_HPP
#define THIEFVKUNIFORMBUFFERMANAGER_HPP

#include <vulkan/vulkan.hpp>
#include <glm/glm.hpp>

#include <vector>
#include <tuple>
#include <memory>

#include "ThiefVKMemoryManager.hpp"

class ThiefVKDevice;

struct entryInfo {
	uint32_t offset;
	uint64_t numberOfEntries;
	uint64_t entrySize;
};

template<typename T>
class ThiefVKBufferManager {
public:

	ThiefVKBufferManager(ThiefVKDevice& Device, vk::BufferUsageFlags usage, uint64_t allignment = 1);

	void addBufferElements(const std::vector<T>& elements);

	std::pair<ThiefVKBuffer, ThiefVKBuffer> flushBufferUploads();
	std::vector<entryInfo> getBufferOffsets();

	bool bufferHasChanged() const;

private:
	std::pair<ThiefVKBuffer, ThiefVKBuffer> uploadBuffer(ThiefVKBuffer&, const uint64_t);

	ThiefVKDevice& mDevice;

	vk::BufferUsageFlags mUsage;
	uint64_t mAllignment;
	uint64_t mRealAllignment;

	std::vector<T> mBuffer;
	std::vector<entryInfo> mEntries;
	uint64_t mCurrentOffset;

	std::vector<T> mPreviousBuffer;
	ThiefVKBuffer mPreviousDeviceBuffer;
};

#endif
