set(PROJECT_NAME "ThiefVK")
project(${PROJECT_NAME})

set(SOURCE
    "Src/ThiefVKInstance.cpp"
	"Src/ThiefVKDevice.cpp"
	"Src/ThiefVKPipeLineManager.cpp"
	"Src/ThiefVKEngine.cpp"
    "Src/ThiefVKSwapChain.cpp"
    "Src/ThiefVKMemoryManager.cpp"
    "Src/ThiefVKVertex.cpp"
    "Src/ThiefVKBufferManager.cpp"
	"Src/ThiefVKDescriptorManager.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/../ThirdParty/stb_image/stb_image.cpp"	
	)

include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../ThirdParty/stb_image")

add_library(${PROJECT_NAME} ${SOURCE})

add_subdirectory("Src/Shaders")

add_definitions(-DGLM_FORCE_DEPTH_ZERO_TO_ONE)

if(WIN32)
	target_link_libraries(${PROJECT_NAME} "${CMAKE_CURRENT_SOURCE_DIR}/../ThirdParty/VulkanSDK1.0.65.0/Lib/vulkan-1.lib")
else()
	target_link_libraries(${PROJECT_NAME} vulkan)
endif(WIN32)
