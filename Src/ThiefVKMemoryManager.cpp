#include "ThiefVKMemoryManager.hpp"

#include <vulkan/vulkan.hpp>

#include <vector>
#include <list>
#include <iostream>

ThiefVKMemoryManager::ThiefVKMemoryManager(vk::PhysicalDevice* physDev, vk::Device *Dev) : PhysDev{physDev}, Device{Dev} {
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

    if(deviceLocalPoolIndex == -1) {
        for(uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
            if(memProps.memoryTypes[i].propertyFlags & vk::MemoryPropertyFlagBits::eDeviceLocal)
            {
                std::cerr << "having to use host coherint memory, probably a integrated GPU  \n";
                deviceLocalPoolIndex = i; // just find the find pool that is device local
                break;
            }
        }
    }

    if(deviceLocalPoolIndex == -1) std::cerr << "Unable to find Device Local Memory \n";

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

    hostMappableMemoryBackers.push_back(Device->allocateMemory(allocInfo));

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
    for(auto& frags : deviceMemoryBackers) {
        Device->freeMemory(frags);
    }
}


void ThiefVKMemoryManager::FreeHostMappablePools() {
    for(auto& frags : hostMappableMemoryBackers) { // we assume that all has been unmapped
        Device->freeMemory(frags);
    }
}


void ThiefVKMemoryManager::MergeFreePools() {
    MergeFreePools();
    MergeFreeHostPools();
}


void ThiefVKMemoryManager::MergeFreeDeiveLocalPools() {

}


void ThiefVKMemoryManager::MergeFreeHostPools() {

}


Allocation ThiefVKMemoryManager::AttemptToAllocate(uint64_t size, bool deviceLocal) {
    auto memPool = deviceLocal ? deviceLocalPools : hostMappablePools;

    uint32_t pool = 0;
    for(auto& pools : memPool) {
        for(auto& frag : pools) {
            if(frag.free && frag.size >= size) {
                Allocation alloc;
                alloc.offset = frag.offset;
                alloc.mappedToHost = false;
                alloc.deviceLocal = true;
                alloc.pool = pool;
                alloc.size = size;

                return alloc;
            }
        }
        ++pool;
    }
    Allocation alloc;
    alloc.size = 0; // signify that the allocation failed
    return alloc;
}


Allocation ThiefVKMemoryManager::Allocate(uint64_t size, bool deviceLocal) {

    Allocation alloc = AttemptToAllocate(size, deviceLocal);
    if(alloc.size != 0) return alloc;
    MergeFreePools(); // if we failed to allocat, perform a defrag of the pools and try again.
    alloc = AttemptToAllocate(size, deviceLocal);
    if(alloc.size != 0) return alloc;
    if(deviceLocal) {
        AllocateDevicePool();
    } else {
        AllocateHostMappablePool();
    }
    return AttemptToAllocate(size, deviceLocal); // should succedd or we are out of memory :(
}


void ThiefVKMemoryManager::Free(Allocation alloc) {

}
