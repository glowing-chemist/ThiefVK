#ifndef THIEFVKVERTEX_HPP
#define THIEFVKVERTEX_HPP

#include <vulkan/vulkan.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

struct Vertex { // vertex struct representing vertex positions and texture coordinates
	glm::vec3 pos;
	glm::vec3 norm;
	glm::vec2 tex;
	float	  albedo;

    static vk::VertexInputBindingDescription getBindingDesc();

	static std::array<vk::VertexInputAttributeDescription, 4> getAttribDesc();
};


bool operator==(const Vertex& lhs, const Vertex& rhs);

bool operator<(const Vertex& lhs, const Vertex& rhs);

namespace std {
    template<> struct hash<Vertex> {
        size_t operator()(Vertex const& vertex) const {
            return ((hash<glm::vec3>()(vertex.pos) ^
                   (hash<glm::vec3>()(vertex.norm) << 1)) >> 1) ^
                   (hash<glm::vec2>()(vertex.tex) << 1);
        }
    };
}

#endif
