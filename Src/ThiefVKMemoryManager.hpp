#ifndef THIEFVKMEMORYMANAGER_HPP
#define THIEFVKMEMORYMANAGER_HPP

#include <vulkan/vulkan.hpp>

#include <vector>

struct PoolFramganet {
    friend class ThiefVKMemoryManager; // to allow this to be an opaque handle that only the memory manager can use
private:
    PoolFramganet* prev;
    PoolFramganet* next;
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
    PoolFramganet* allocatedPool;
    bool DeviceLocal;
};

// This class will be used for keeping track of GPU allocations for buffers and
// images. this will be done my maintaing2 pools of gpu memory, one device local
// and the other host mappable. allocations will be handles a opaque types that
// the caller will keep track of.
class ThiefMemoryManager {

public:
    ThiefMemoryManager() = default; // constructor that doens't allocate pools
    explicit ThiefMemoryManager(vk::PhysicalDevice, vk::Device); // one that does
    ~ThiefMemoryManager();

    Allocation Allocate(uint64_t size, bool deviceLocal);
    void       Free(Allocation alloc);

private:
    void MergeFreePools();
    void MergeFreeDeiveLocalPools();
    void MergeFreeHostPools();

    std::vector<vk::DeviceMemory> deviceMemoryBackers;
    std::vector<PoolFramganet*>   deviceLocalPools;
    std::vector<PoolFramganet*>   hostLocalPool;

    vk::PhysicalDevice& PhysDev; // handles so we can allocate more memory withou having
    vk::Device&         Device;  // to call out to the main device instance
};

#endif
