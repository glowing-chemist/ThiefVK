#include "ThiefVKBufferManager.hpp"

#include <vulkan/vulkan.hpp>

#include "ThiefVKDevice.hpp"
#include "ThiefVKVertex.hpp"
#include "ThiefVKModel.hpp"

#include <iostream>
#include <numeric>

template<typename T>
ThiefVKBufferManager<T>::ThiefVKBufferManager(ThiefVKDevice &Device, vk::BufferUsageFlags usage, uint64_t allignment) : mDevice{Device}, mUsage{usage}, mAllignment{allignment}, mCurrentOffset{0ul} {}


template<typename T>
void ThiefVKBufferManager<T>::addBufferElements(const std::vector<T> &elements) {
	mRealAllignment = std::ceil(float(sizeof(T) * elements.size()) / float(mAllignment)) * mAllignment;

	uint32_t offset = mCurrentOffset;
	mEntries.push_back({offset, elements.size(), mRealAllignment});

	mCurrentOffset += mRealAllignment;

	mBuffer.insert(mBuffer.end(), elements.begin(), elements.end());

}


template<typename T>
std::pair<ThiefVKBuffer, ThiefVKBuffer> ThiefVKBufferManager<T>::uploadBuffer(ThiefVKBuffer& buffer, const uint64_t bufferSize) {
	ThiefVKBuffer stagingBuffer{};

	stagingBuffer = mDevice.createBuffer(vk::BufferUsageFlagBits::eTransferSrc, bufferSize);

	std::unique_ptr<char[]> allignedData(new char[bufferSize]);
	uint64_t bufferPos = 0;
	for(unsigned int i = 0; i < mEntries.size(); ++i) {
		std::memmove(allignedData.get() + mEntries[i].offset, &mBuffer[bufferPos], mEntries[i].numberOfEntries * sizeof(T));
		bufferPos += mEntries[i].numberOfEntries;
	}

	void* memory = mDevice.getMemoryManager()->MapAllocation(stagingBuffer.mBufferMemory);
	memcpy(memory, allignedData.get(), bufferSize);
	mDevice.getMemoryManager()->UnMapAllocation(stagingBuffer.mBufferMemory);

	mDevice.copyBuffers(stagingBuffer.mBuffer, buffer.mBuffer, bufferSize);

	mPreviousBuffer = mBuffer;
	mBuffer.clear();
	mEntries.clear();
	mCurrentOffset = 0;

	return {buffer, stagingBuffer};
}


template<typename T>
std::pair<ThiefVKBuffer, ThiefVKBuffer> ThiefVKBufferManager<T>::flushBufferUploads() {
	const uint64_t bufferSize = std::accumulate(mEntries.begin(), mEntries.end(), 0, [](const int& lhs, const entryInfo& rhs) { return lhs + rhs.entrySize; } );

	ThiefVKBuffer buffer = mDevice.createBuffer(vk::BufferUsageFlagBits::eTransferDst | mUsage, bufferSize);

	// Upload the current buffer
	return uploadBuffer(buffer, bufferSize);
}


template<typename T>
std::vector<entryInfo> ThiefVKBufferManager<T>::getBufferOffsets() {
	return mEntries;
}


template<typename T>
bool ThiefVKBufferManager<T>::bufferHasChanged() const {
	return mPreviousBuffer == mBuffer;
}

// we need to explicitly instansiate what buffer managers we will be using here

// For uniform constants.
template class ThiefVKBufferManager<glm::mat4>;

// For the vertex buffer
template class ThiefVKBufferManager<Vertex>;

// For the index buffer
template class ThiefVKBufferManager<uint32_t>;

// For the light buffer
template class ThiefVKBufferManager<ThiefVKLight>;
