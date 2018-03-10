#ifndef THIEFVKENGINE_HPP
#define THIEFVKENGINE_HPP

#include "ThiefVKInstance.hpp"
#include "ThiefVKDevice.hpp"

class ThiefVKEngine {

public:
    ThiefVKEngine();
    ~ThiefVKEngine() = default;

    void Init();

private:
    ThiefVKInstance Instance;
    ThiefVKDevice Device;
};


#endif
