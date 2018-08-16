#ifndef THIEFVKENGINE_HPP
#define THIEFVKENGINE_HPP

#include "ThiefVKInstance.hpp"
#include "ThiefVKDevice.hpp"
#include "ThiefVKMemoryManager.hpp"
#include "ThiefVKModel.hpp"

class ThiefVKEngine {

public:
    ThiefVKEngine(GLFWwindow*);
    ~ThiefVKEngine();

    void Init();

    void addModelToScene(ThiefVKModel&);
    void addLightToScene(glm::mat4&);
    void renderScene();

private:
	GLFWwindow* mWindow;
    ThiefVKInstance mInstance;
    ThiefVKDevice mDevice;
    std::vector<ThiefVKModel> mModels;
};


#endif
