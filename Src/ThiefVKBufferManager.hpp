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
};

template<typename T>
class ThiefVKBufferManager {
public:

	ThiefVKBufferManager(ThiefVKDevice& Device, vk::BufferUsageFlags usage, int allignment = 1);

	void addBufferElements(const std::vector<T>& elements);

	std::pair<ThiefVKBuffer, ThiefVKBuffer> flushBufferUploads();
	std::vector<entryInfo> getBufferOffsets();

	bool bufferHasChanged() const;

private:
	std::pair<ThiefVKBuffer, ThiefVKBuffer> uploadBuffer(ThiefVKBuffer&);

	ThiefVKDevice& mDevice;

	vk::BufferUsageFlags mUsage;
	int mAllignment;

	std::vector<T> mBuffer;
	std::vector<entryInfo> mEntries;

	std::vector<T> mPreviousBuffer;
};


template<typename T>
class AllignedBuffer {
public:
	AllignedBuffer(std::vector<T>&, int allignment);
	char* data();

private:
	std::unique_ptr<char[]> mAllignedData;
};


#endif
