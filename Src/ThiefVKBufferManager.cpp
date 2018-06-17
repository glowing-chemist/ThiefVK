#include "ThiefVKBufferManager.hpp"

#include <vulkan/vulkan.hpp>

#include "ThiefVKDevice.hpp"
#include "ThiefVKVertex.hpp"

#include <iostream>


template<typename T>
ThiefVKBufferManager<T>::ThiefVKBufferManager(ThiefVKDevice &Device, vk::BufferUsageFlags usage) : mDevice{Device}, mUsage{usage} {}


template<typename T>
void ThiefVKBufferManager<T>::addBufferElements(const std::vector<T> &elements) {
	uint32_t offset = mBuffer.size();
	mOffsets.push_back(offset);

	mBuffer.insert(mBuffer.end(), elements.begin(), elements.end());

}


template<typename T>
std::pair<ThiefVKBuffer, ThiefVKBuffer> ThiefVKBufferManager<T>::uploadBuffer(vk::Buffer& buffer, Allocation alloc) {
	vk::Buffer stagingBuffer{nullptr};
	Allocation stagingBufferMemory{};

	if (mUsage & vk::BufferUsageFlagBits::eUniformBuffer) { // Take a synchronous buffer update path if we will be doing it frequently with little data.
		void* memory = mDevice.getMemoryManager()->MapAllocation(alloc);

		memcpy(memory, mBuffer.data(), mBuffer.size() * sizeof(T));

		mDevice.getMemoryManager()->UnMapAllocation(alloc);
	}
	else { // Else record the commands in to the flush buffer to copy the buffer to device local memory

		auto [Buffer, Memory] = mDevice.createBuffer(vk::BufferUsageFlagBits::eTransferSrc, mBuffer.size() * sizeof(T));
		stagingBuffer = Buffer;
		stagingBufferMemory = Memory;

		void* memory = mDevice.getMemoryManager()->MapAllocation(stagingBufferMemory);
		memcpy(memory, mBuffer.data(), mBuffer.size() * sizeof(T));
		mDevice.getMemoryManager()->UnMapAllocation(stagingBufferMemory);

		mDevice.copyBuffers(stagingBuffer, buffer, mBuffer.size() * sizeof(T));
	}

	mPreviousBuffer = mBuffer;
	mBuffer.clear();

	return {{buffer, alloc}, {stagingBuffer, stagingBufferMemory}};
}


template<typename T>
std::pair<ThiefVKBuffer, ThiefVKBuffer> ThiefVKBufferManager<T>::flushBufferUploads() {
	auto [buffer, alloc] = mDevice.createBuffer(vk::BufferUsageFlagBits::eTransferDst | mUsage, mBuffer.size() * sizeof(T));

	// Upload the current buffer
	return uploadBuffer(buffer, alloc);
}


template<typename T>
std::vector<uint32_t> ThiefVKBufferManager<T>::getBufferOffsets() {
	std::vector<uint32_t > offsets = mOffsets;
	mOffsets.clear();

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
