#ifndef THIEFVKVERTEX_HPP
#define THIEFVKVERTEX_HPP

#include <vulkan/vulkan.hpp>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

struct Vertex { // vertex struct representing vertex positions and texture coordinates
    glm::vec3 pos;
    glm::vec2 tex;

    static vk::VertexInputBindingDescription getBindingDesc();

    static std::array<vk::VertexInputAttributeDescription, 2> getAttribDesc();
};

#endif
