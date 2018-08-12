#include "ThiefVKMemoryManager.hpp"

#include <vulkan/vulkan.hpp>

#include <vector>
#include <list>
#include <iostream>

bool operator==(const ThiefVKImage& lhs, const ThiefVKImage& rhs) {
	return lhs.mImage == rhs.mImage;
}


bool operator!=(const ThiefVKImage& lhs, const ThiefVKImage& rhs) {
	return !(lhs == rhs);
}


bool operator==(const PoolFragment& lhs, const PoolFragment& rhs) {
    return lhs.DeviceLocal == rhs.DeviceLocal
            && lhs.free == rhs.free
            && lhs.offset == rhs.offset
            && lhs.size == rhs.size;
}


ThiefVKMemoryManager::ThiefVKMemoryManager(vk::PhysicalDevice* physDev, vk::Device *Dev) : PhysDev{physDev}, Device{Dev} {
    findPoolIndicies();

    AllocateDevicePool();
    AllocateHostMappablePool();
}


void ThiefVKMemoryManager::Destroy() {
    FreeDevicePools();
    FreeHostMappablePools();
}


void ThiefVKMemoryManager::findPoolIndicies() {
    vk::PhysicalDeviceMemoryProperties memProps = PhysDev->getMemoryProperties();

    bool poolFound = false;
    for(uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
        if(memProps.memoryTypes[i].propertyFlags & vk::MemoryPropertyFlagBits::eDeviceLocal
           && !(memProps.memoryTypes[i].propertyFlags & vk::MemoryPropertyFlagBits::eHostCoherent))
        {
            mDeviceLocalPoolIndex = i; // just find the find pool that is device local
            poolFound = true;
            break;
        }
    }

    if(!poolFound) {
        for(uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
            if(memProps.memoryTypes[i].propertyFlags & vk::MemoryPropertyFlagBits::eDeviceLocal)
            {
                std::cerr << "having to use host coherint memory, probably a integrated GPU  \n";
                mDeviceLocalPoolIndex = i; // just find the first pool that is device local
                break;
            }
        }
    }

    for(uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
        if((memProps.memoryTypes[i].propertyFlags & vk::MemoryPropertyFlagBits::eHostCoherent)
           && (memProps.memoryTypes[i]).propertyFlags & vk::MemoryPropertyFlagBits::eHostVisible)
        {
            mHostMappablePoolIndex = i; // just find the find pool that is device local
            break;
        }
    }

    
}


void ThiefVKMemoryManager::AllocateDevicePool() {

    vk::MemoryAllocateInfo allocInfo{256 * 1000000, static_cast<uint32_t>(mDeviceLocalPoolIndex)};

    deviceMemoryBackers.push_back(Device->allocateMemory(allocInfo));

    std::list<PoolFragment> fragmentList(16);
    uint64_t offset = 0;
    for(auto& frag : fragmentList) {
        frag.free = true;
        frag.DeviceLocal = true;
        frag.size = 16 * 1000000;
        frag.offset = offset;

        offset += frag.size;
    }

    deviceLocalPools.push_back(fragmentList);

#ifndef NDEBUG
    std::cerr << "Allocated a memory pool \n";
#endif
}


void ThiefVKMemoryManager::AllocateHostMappablePool() {

    vk::MemoryAllocateInfo allocInfo{256 * 1000000, static_cast<uint32_t>(mHostMappablePoolIndex)};

    hostMappableMemoryBackers.push_back(Device->allocateMemory(allocInfo));

    std::list<PoolFragment> fragmentList(16);
    uint64_t offset = 0;
    for(auto& frag : fragmentList) {
        frag.free = true;
        frag.DeviceLocal = true;
        frag.size = 16 * 1000000;
        frag.offset = offset;

        offset += frag.size;
    }

    hostMappablePools.push_back(fragmentList);

#ifndef NDEBUG
    std::cerr << "Allocated a host mappable memory pool \n";
#endif
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
    MergePool(deviceLocalPools);
    MergePool(hostMappablePools);
}


void ThiefVKMemoryManager::MergePool(std::vector<std::list<PoolFragment> > &pools) { // this is expensive on a a list so only call when really needed
    for(auto& pool : pools) {
		for(auto fragIter = pool.begin(); fragIter != --pool.end() || fragIter != pool.end(); ++fragIter) {
            PoolFragment frag = *fragIter;
            auto fragIterClone = fragIter;
            PoolFragment nextFrag = *(++fragIterClone);
            if(frag.free && nextFrag.free) {
                frag.size += nextFrag.size;
                pool.remove(nextFrag);
            }
        }
    }
}


Allocation ThiefVKMemoryManager::AttemptToAllocate(uint64_t size, unsigned int allignment, bool hostMappable) {
    auto& memPools = hostMappable ? deviceLocalPools : hostMappablePools;

    uint32_t poolNum = 0;
    for(auto& pool : memPools) {
        for(auto fragIter = pool.begin(); fragIter != pool.end(); ++fragIter) {
            PoolFragment frag = *fragIter;
            if(frag.free && frag.size >= size) {
                unsigned int allignedoffset = 0;
                while((frag.offset  + allignedoffset) % allignment != 0) {
                    ++allignedoffset;
                    if(size + allignedoffset > frag.size) continue; // make sure there is enought size to be alligned
                }
                Allocation alloc;
                alloc.offset = frag.offset + allignedoffset;
                alloc.hostMappable = hostMappable;
                alloc.deviceLocal = true;
                alloc.pool = poolNum;
                alloc.size = size;

                if(size + allignedoffset < (frag.size / 2)) { // we'd be wasting more than half the fragment, so split it up
                    PoolFragment fragToInsert;
                    fragToInsert.DeviceLocal = true;
                    fragToInsert.free = true;
                    fragToInsert.offset = frag.offset + size + allignedoffset;
                    fragToInsert.size = frag.size - size - allignedoffset;

                    fragIter->size = size + allignedoffset;
                    pool.insert(fragIter, fragToInsert);
                }

                return alloc;
            }
        }
        ++poolNum;
    }
    Allocation alloc;
    alloc.size = 0; // signify that the allocation failed
    return alloc;
}


Allocation ThiefVKMemoryManager::Allocate(uint64_t size, unsigned allignment,  bool hostMappable) {

    Allocation alloc = AttemptToAllocate(size, allignment, hostMappable);
    if(alloc.size != 0) return alloc;

    MergeFreePools(); // if we failed to allocate, perform a defrag of the pools and try again.
    alloc = AttemptToAllocate(size, allignment, hostMappable);
    if(alloc.size != 0) return alloc;

    if(hostMappable) {
        AllocateDevicePool();
    } else {
        AllocateHostMappablePool();
    }
    return AttemptToAllocate(size, allignment, hostMappable); // should succeed or we are out of memory :(
}


void ThiefVKMemoryManager::Free(Allocation alloc) {
    auto& pools = alloc.hostMappable ? hostMappablePools : deviceLocalPools ;
    auto& pool  = pools[alloc.pool];

    for(auto& frag : pool) {
        if(alloc.offset == frag.offset) frag.free = true;
    }
}


void ThiefVKMemoryManager::BindBuffer(vk::Buffer &buffer, Allocation alloc) {
    std::vector<vk::DeviceMemory> pools = alloc.hostMappable ? hostMappableMemoryBackers : deviceMemoryBackers ;

    Device->bindBufferMemory(buffer, pools[alloc.pool], alloc.offset);
}


void ThiefVKMemoryManager::BindImage(vk::Image &image, Allocation alloc) {
    std::vector<vk::DeviceMemory> pools = alloc.hostMappable ? hostMappableMemoryBackers : deviceMemoryBackers ;

    Device->bindImageMemory(image, pools[alloc.pool], alloc.offset);
}


void* ThiefVKMemoryManager::MapAllocation(Allocation alloc) {
	std::vector<vk::DeviceMemory> pools = alloc.hostMappable ? hostMappableMemoryBackers : deviceMemoryBackers;

	return Device->mapMemory(pools[alloc.pool], alloc.offset, alloc.size);
}


void ThiefVKMemoryManager::UnMapAllocation(Allocation alloc) {
	std::vector<vk::DeviceMemory> pools = alloc.hostMappable ? hostMappableMemoryBackers : deviceMemoryBackers;

	Device->unmapMemory(pools[alloc.pool]);
}
