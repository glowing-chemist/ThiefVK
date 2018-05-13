#include "ThiefVKUniformBufferManager.hpp"

#include "ThiefVKDevice.hpp"

#include <iostream>

ThiefVKUnifromBufferManager::ThiefVKUnifromBufferManager(ThiefVKDevice &Device) : mDevice{Device} {
	startNewBuffer();
}


void ThiefVKUnifromBufferManager::Destroy() {
	mDevice.destroyBuffer(mDeviceUniformBuffer, mUniformBufferMemory);

	for(auto& [buffer, alloc] : mFilledBuffers) {
		mDevice.destroyBuffer(buffer, alloc);
	}
}


std::pair<vk::Buffer&, uint32_t> ThiefVKUnifromBufferManager::addBufferElements(std::vector<glm::mat4> &elements) {
	uint32_t offset = mUniformBuffer.size();

	mUniformBuffer.insert(mUniformBuffer.end(), elements.begin(), elements.end());

	if(mUniformBuffer.size() * sizeof(glm::mat4) >= kUniformBufferSize) {
		// If we have filled up this buffer upload the values and start a new buffer
		uploadBuffer();

		mFilledBuffers.push_back( {mDeviceUniformBuffer, mUniformBufferMemory} );

		startNewBuffer();
	}

	return {mFilledBuffers.back().first, offset};
}


void ThiefVKUnifromBufferManager::startNewBuffer() {
	auto [buffer, alloc] = mDevice.createBuffer(vk::BufferUsageFlagBits::eUniformBuffer, kUniformBufferSize);

	mDeviceUniformBuffer = buffer;
	mUniformBufferMemory = alloc;

	mUniformBuffer.clear();

#ifndef NDEBUG
	std::cout << "Allocated a uniform Buffer \n";
#endif
}


void ThiefVKUnifromBufferManager::uploadBuffer() {
	void* memory = mDevice.getMemoryManager()->MapAllocation(mUniformBufferMemory);

	memcpy(memory, mUniformBuffer.data(), mUniformBuffer.size() * sizeof(glm::mat4));

	mDevice.getMemoryManager()->UnMapAllocation(mUniformBufferMemory);
}


std::vector<std::pair<vk::Buffer, Allocation>> ThiefVKUnifromBufferManager::flushBufferUploads() {
	// Upload the current buffer
	uploadBuffer();

	// Add it to the list of filled buffers
	mFilledBuffers.push_back( {mDeviceUniformBuffer, mUniformBufferMemory} );

	// Create a new buffer for adding uniform constants to for next frame
	startNewBuffer();

	return mFilledBuffers;
}
