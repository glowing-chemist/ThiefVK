#include "ThiefVKMemoryManager.hpp"

#include <vulkan/vulkan.hpp>

#include <vector>
#include <list>
#include <iostream>
#include <algorithm>

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


#ifndef NDEBUG
    void ThiefVKMemoryManager::dumpPools() const {
        std::cout << "Device local pools: \n";
        for(auto& pool : deviceLocalPools) {
            for(auto& fragment : pool) {
                std::cout << "fragment size: " << fragment.size << '\n'
                          << "fragment offset: " << fragment.offset << '\n'
                          << "fragment marked for merge: " << fragment.canBeMerged << '\n'
                          << "fragment free: " << fragment.free << "\n\n\n";
            }
        }

        std::cout << "Host Mappable pools: \n";
        for(auto& pool : hostMappablePools) {
            for(auto& fragment : pool) {
                std::cout << "fragment size: " << fragment.size << '\n'
                          << "fragment offset: " << fragment.offset << '\n'
                          << "fragment marked for merge: " << fragment.canBeMerged << '\n'
                          << "fragment free: " << fragment.free << "\n\n\n";
            }
        }
    }
#endif


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

    std::list<PoolFragment> fragmentList(4);
    uint64_t offset = 0;
    for(auto& frag : fragmentList) {
        frag.free = true;
        frag.DeviceLocal = true;
        frag.size = 64 * 1000000;
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

    std::list<PoolFragment> fragmentList(4);
    uint64_t offset = 0;
    for(auto& frag : fragmentList) {
        frag.free = true;
        frag.DeviceLocal = true;
        frag.size = 64 * 1000000;
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
        // loop forwards through all the fragments and mark any free fragments ajasent to a free fragment as can be merged
#ifndef NDEBUG
        const auto freeFragmentsPre = std::count_if(pool.begin(), pool.end(), [](auto& fragment){ return fragment.free; });
        std::cout << "number of free fragments pre merge: " << freeFragmentsPre << '\n';
#endif
		bool fragmentFree = false;
        for(auto& fragment : pool) {
            
            if(fragmentFree && fragment.free) {
                fragment.canBeMerged = true;
            }

            if(fragment.free) {
                fragmentFree = true;
            } else { 
                fragmentFree = false;
            }
        }

        // Now loop backwards over the list and merge all fragments marked as can be merged
        uint32_t sizeToAdd = 0;
        bool fragmentMerged = false;
        std::list<PoolFragment>::reverse_iterator fragmentToRemove;
        bool needToRemoveFragment = false;
        for(auto fragment = pool.rbegin(); fragment != pool.rend(); ++fragment) {
            if(needToRemoveFragment) {
                pool.remove(*fragmentToRemove);
                needToRemoveFragment = false;
            }

            if(fragmentMerged) {
                fragment->size += sizeToAdd;
                sizeToAdd = 0;
                fragmentMerged = false;
            }

            if(fragment->canBeMerged) {
                sizeToAdd = fragment->size;
                fragmentToRemove = fragment;
                needToRemoveFragment = true;
                fragmentMerged = true;
            }
        }
#ifndef NDEBUG
        const auto freeFragmentsPost = std::count_if(pool.begin(), pool.end(), [](auto& fragment){ return fragment.free; });
        std::cout << "number of free fragments post merge: " << freeFragmentsPost << '\n';
#endif
    }
}


Allocation ThiefVKMemoryManager::AttemptToAllocate(uint64_t size, unsigned int allignment, bool hostMappable) {
    auto& memPools = hostMappable ? hostMappablePools : deviceLocalPools;

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
                fragIter->free = false;
                return alloc;
            }
        }
        ++poolNum;
    }
#ifndef NDEBUG
    std::cout << "ALLOCATION FAILED!!!!!!!! This should never reasonably happen \n"
              << "Alocation size: " << size / 1000000 << " MB \n";
#endif
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
        AllocateHostMappablePool();
    } else {
        AllocateDevicePool();
    }
    MergeFreePools();
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
