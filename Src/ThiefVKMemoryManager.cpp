#include "ThiefVKMemoryManager.hpp"

#include <vulkan/vulkan.hpp>

#include <vector>
#include <list>
#include <iostream>

ThiefVKMemoryManager::ThiefVKMemoryManager(vk::PhysicalDevice& physDev, vk::Device& Dev) : PhysDev{&physDev}, Device{&Dev} {
    AllocateDevicePool();
    AllocateHostMappablePool();
}


ThiefVKMemoryManager::~ThiefVKMemoryManager() {
    FreeDevicePools();
    FreeHostMappablePools();
}


void ThiefVKMemoryManager::AllocateDevicePool() {
    vk::PhysicalDeviceMemoryProperties memProps = PhysDev->getMemoryProperties();

    int deviceLocalPoolIndex = -1;
    for(uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
        if(memProps.memoryTypes[i].propertyFlags & vk::MemoryPropertyFlagBits::eDeviceLocal
           && !(memProps.memoryTypes[i].propertyFlags & vk::MemoryPropertyFlagBits::eHostCoherent))
        {
            deviceLocalPoolIndex = i; // just find the find pool that is device local
            break;
        }
    }

    if(deviceLocalPoolIndex == -1) std::cerr << "Unable to find non host Coheriant Device Local Memory \n";

    vk::MemoryAllocateInfo allocInfo{256 * 1000000, static_cast<uint32_t>(deviceLocalPoolIndex)};

    deviceMemoryBackers.push_back(Device->allocateMemory(allocInfo));

    std::list<PoolFramganet> fragmentList(16);
    uint64_t offset = 0;
    for(auto& frag : fragmentList) {
        frag.free = true;
        frag.DeviceLocal = true;
        frag.size = 16 * 1000000;
        frag.offset = offset;

        offset += frag.size;
    }

    deviceLocalPools.push_back(fragmentList);
}


void ThiefVKMemoryManager::AllocateHostMappablePool() {
    vk::PhysicalDeviceMemoryProperties memProps = PhysDev->getMemoryProperties();

    int deviceLocalPoolIndex = -1;
    for(uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
        if((memProps.memoryTypes[i].propertyFlags & vk::MemoryPropertyFlagBits::eDeviceLocal)
           && (memProps.memoryTypes[i].propertyFlags & vk::MemoryPropertyFlagBits::eHostCoherent)
           && (memProps.memoryTypes[i]).propertyFlags & vk::MemoryPropertyFlagBits::eHostVisible)
        {
            deviceLocalPoolIndex = i; // just find the find pool that is device local
            break;
        }
    }

    if(deviceLocalPoolIndex == -1) std::cerr << "Unable to find host visible Device Local Memory \n";

    vk::MemoryAllocateInfo allocInfo{256 * 1000000, static_cast<uint32_t>(deviceLocalPoolIndex)};

    deviceMemoryBackers.push_back(Device->allocateMemory(allocInfo));

    std::list<PoolFramganet> fragmentList(16);
    uint64_t offset = 0;
    for(auto& frag : fragmentList) {
        frag.free = true;
        frag.DeviceLocal = true;
        frag.size = 16 * 1000000;
        frag.offset = offset;

        offset += frag.size;
    }

    hostMappablePools.push_back(fragmentList);
}


void ThiefVKMemoryManager::FreeDevicePools() {

}


void ThiefVKMemoryManager::FreeHostMappablePools() {

}
