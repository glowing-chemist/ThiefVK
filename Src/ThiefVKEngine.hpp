#ifndef THIEFVKENGINE_HPP
#define THIEFVKENGINE_HPP

#include "ThiefVKInstance.hpp"
#include "ThiefVKDevice.hpp"
#include "ThiefVKMemoryManager.hpp"

class ThiefVKEngine {

public:
    ThiefVKEngine();
    ~ThiefVKEngine();

    void Init();

private:
    ThiefVKInstance Instance;
    ThiefVKDevice Device;
    ThiefVKMemoryManager MemoryManager;
};


#endif
