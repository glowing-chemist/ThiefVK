set(PROJECT_NAME "ThiefVK")
project(${PROJECT_NAME})

set(SOURCE
        "Src/ThiefVKInstance.cpp"
	"Src/ThiefVKDevice.cpp"
	"Src/ThiefVKPipeLineManager.cpp"
	"Src/ThiefVKEngine.cpp"
	)



add_library(${PROJECT_NAME} ${SOURCE})
target_link_libraries(${PROJECT_NAME} vulkan glfw)
