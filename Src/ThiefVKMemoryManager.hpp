#ifndef THIEFVKMEMORYMANAGER_HPP
#define THIEFVKMEMORYMANAGER_HPP

#include <vulkan/vulkan.hpp>

#include <vector>
#include <list>

struct PoolFragment {
    friend class ThiefVKMemoryManager; // to allow this to be an opaque handle that only the memory manager can use
    friend bool operator==(const PoolFragment&, const PoolFragment&);
private:
    uint64_t offset;
    uint64_t size;
    bool DeviceLocal;
    bool free;
};

struct Allocation {
    friend class ThiefVKMemoryManager;
private:
    uint64_t size;
    uint64_t offset;
    uint32_t pool; // for if we end up allocating more than one pool
    bool deviceLocal;
    bool hostMappable;
};

// This class will be used for keeping track of GPU allocations for buffers and
// images. this will be done my maintaing2 pools of gpu memory, one device local
// and the other host mappable. allocations will be handles a opaque types that
// the caller will keep track of.
class ThiefVKMemoryManager {

public:
    ThiefVKMemoryManager() = default; // constructor that doens't allocate pools
    explicit ThiefVKMemoryManager(vk::PhysicalDevice* , vk::Device* ); // one that does
    ~ThiefVKMemoryManager();

    Allocation Allocate(uint64_t size, bool deviceLocal);
    void       Free(Allocation alloc);

    void       MapImage(vk::Image& image, Allocation alloc);
    void       MapBuffer(vk::Buffer& buffer, Allocation alloc);

private:
    void MergeFreePools();
    void MergePool(std::vector<std::list<PoolFragment>>& pools);

    Allocation AttemptToAllocate(uint64_t size, bool deviceLocal);

    void AllocateDevicePool();
    void AllocateHostMappablePool();

    void FreeDevicePools();
    void FreeHostMappablePools();

    std::vector<vk::DeviceMemory> deviceMemoryBackers;
    std::vector<vk::DeviceMemory> hostMappableMemoryBackers;
    std::vector<std::list<PoolFragment>>   deviceLocalPools;
    std::vector<std::list<PoolFragment>>   hostMappablePools;

    vk::PhysicalDevice* PhysDev; // handles so we can allocate more memory withou having
    vk::Device*         Device;  // to call out to the main device instance
};

#endif
