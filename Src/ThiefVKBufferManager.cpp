#include "ThiefVKBufferManager.hpp"

#include <vulkan/vulkan.hpp>

#include "ThiefVKDevice.hpp"
#include "ThiefVKVertex.hpp"

#include <iostream>


template<typename T>
ThiefVKBufferManager<T>::ThiefVKBufferManager(ThiefVKDevice &Device, vk::BufferUsageFlags usage, int allignment) : mDevice{Device}, mUsage{usage}, 
	mAllignment{int(std::ceil(float(sizeof(T)) / float(allignment))) * allignment} {}


template<typename T>
void ThiefVKBufferManager<T>::addBufferElements(const std::vector<T> &elements) {
	uint32_t offset = mBuffer.size() * mAllignment;
	mEntries.push_back({offset, elements.size()});

	mBuffer.insert(mBuffer.end(), elements.begin(), elements.end());

}


template<typename T>
std::pair<ThiefVKBuffer, ThiefVKBuffer> ThiefVKBufferManager<T>::uploadBuffer(ThiefVKBuffer& buffer) {
	ThiefVKBuffer stagingBuffer{};

	stagingBuffer = mDevice.createBuffer(vk::BufferUsageFlagBits::eTransferSrc, mBuffer.size() * mAllignment);

	void* memory = mDevice.getMemoryManager()->MapAllocation(stagingBuffer.mBufferMemory);
	memcpy(memory, AllignedBuffer(mBuffer, mAllignment).data(), mBuffer.size() * mAllignment);
	mDevice.getMemoryManager()->UnMapAllocation(stagingBuffer.mBufferMemory);

	mDevice.copyBuffers(stagingBuffer.mBuffer, buffer.mBuffer, mBuffer.size() * mAllignment);

	mPreviousBuffer = mBuffer;
	mBuffer.clear();

	return {buffer, stagingBuffer};
}


template<typename T>
std::pair<ThiefVKBuffer, ThiefVKBuffer> ThiefVKBufferManager<T>::flushBufferUploads() {
	ThiefVKBuffer buffer = mDevice.createBuffer(vk::BufferUsageFlagBits::eTransferDst | mUsage, mBuffer.size() * sizeof(T));

	// Upload the current buffer
	return uploadBuffer(buffer);
}


template<typename T>
std::vector<entryInfo> ThiefVKBufferManager<T>::getBufferOffsets() {
	std::vector<entryInfo > offsets = mEntries;
	mEntries.clear();

	return offsets;
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


// Alligned buffer helper class
template<typename T>
AllignedBuffer<T>::AllignedBuffer(std::vector<T>& buffer, int allignment) {
	char* memory = new char[allignment * buffer.size()];
	for(unsigned int i = 0; i < buffer.size(); ++i) {
		std::memmove(memory + (allignment * i), &buffer[i], sizeof(T));
	}

	mAllignedData.reset(memory);
}
	

template<typename T>
char* AllignedBuffer<T>::data() {
	return mAllignedData.get();
}
