#include "ThiefVKBufferManager.hpp"

#include <vulkan/vulkan.hpp>

#include "ThiefVKDevice.hpp"
#include "ThiefVKVertex.hpp"

#include <iostream>


template<typename T>
ThiefVKBufferManager<T>::ThiefVKBufferManager(ThiefVKDevice &Device, vk::BufferUsageFlags usage) : mDevice{Device}, mUsage{usage} {
	mMaxBufferSize = [this]() {
		switch(static_cast<VkBufferUsageFlags>(mUsage)) {
			case static_cast<VkBufferUsageFlags>(vk::BufferUsageFlagBits::eUniformBuffer):
				return sizeof(T) * 30;
			case static_cast<VkBufferUsageFlags>(vk::BufferUsageFlagBits::eVertexBuffer):
			case static_cast<VkBufferUsageFlags>(vk::BufferUsageFlagBits::eIndexBuffer):
				return sizeof(T) * 1000;
			default:
				throw std::runtime_error("unaccounted for buffer usage");
		}
		return 0ull;
	}();

	startNewBuffer();
}


template<typename T>
void ThiefVKBufferManager<T>::Destroy() {
	mDevice.destroyBuffer(mDeviceUniformBuffer, mUniformBufferMemory);

	for(auto& [buffer, alloc] : mFilledBuffers) {
		mDevice.destroyBuffer(buffer, alloc);
	}
}


template<typename T>
std::pair<vk::Buffer&, uint32_t> ThiefVKBufferManager<T>::addBufferElements(std::vector<T> &elements) {
	uint32_t offset = mUniformBuffer.size();

	mUniformBuffer.insert(mUniformBuffer.end(), elements.begin(), elements.end());

	if(mUniformBuffer.size() * sizeof(T) >= mMaxBufferSize) {
		// If we have filled up this buffer upload the values and start a new buffer
		uploadBuffer();

		mFilledBuffers.push_back( {mDeviceUniformBuffer, mUniformBufferMemory} );

		startNewBuffer();
	}

	return {mFilledBuffers.back().first, offset};
}


template<typename T>
void ThiefVKBufferManager<T>::startNewBuffer() {
	auto [buffer, alloc] = mDevice.createBuffer(mUsage, mMaxBufferSize);

	mDeviceUniformBuffer = buffer;
	mUniformBufferMemory = alloc;

	mUniformBuffer.clear();

#ifndef NDEBUG
	std::cout << "Allocated a uniform Buffer \n";
#endif
}


template<typename T>
void ThiefVKBufferManager<T>::uploadBuffer() {
	if (mUsage & vk::BufferUsageFlagBits::eUniformBuffer) { // Take a synchronous buffer update path if we will be doing it frequently with little data.
		void* memory = mDevice.getMemoryManager()->MapAllocation(mUniformBufferMemory);

		memcpy(memory, mUniformBuffer.data(), mUniformBuffer.size() * sizeof(T));

		mDevice.getMemoryManager()->UnMapAllocation(mUniformBufferMemory);
	}
	else { // Else record the commands in to the flush buffer to copy the buffer to device local memory
		auto [stagingBuffer, stagingAlloc] = mDevice.createBuffer(vk::BufferUsageFlagBits::eTransferSrc, mUniformBuffer.size() * sizeof(T));

		void* memory = mDevice.getMemoryManager()->MapAllocation(stagingAlloc);
		memcpy(memory, mUniformBuffer.data(), mUniformBuffer.size() * sizeof(T));
		mDevice.getMemoryManager()->UnMapAllocation(stagingAlloc);

		mDevice.copyBuffers(stagingBuffer, mDeviceUniformBuffer, mUniformBuffer.size() * sizeof(T));
	}
}


template<typename T>
std::vector<std::pair<vk::Buffer, Allocation>> ThiefVKBufferManager<T>::flushBufferUploads() {
	// Upload the current buffer
	uploadBuffer();

	// Add it to the list of filled buffers
	mFilledBuffers.push_back( {mDeviceUniformBuffer, mUniformBufferMemory} );

	// Create a new buffer for adding uniform constants to for next frame
	startNewBuffer();

	return mFilledBuffers;
}

// we need to explicitly instanciate what buffer managers we will be using here

// For uniform constants.
template class ThiefVKBufferManager<glm::mat4>;

// For the vertex buffer
template class ThiefVKBufferManager<Vertex>;
