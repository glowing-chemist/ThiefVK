#ifndef THIEFVKMODEL_HPP
#define THIEFVKMODEL_HPP

#include <string>
#include <glm/glm.hpp>

#include "ThiefVKVertex.hpp"

struct geometry {
	std::vector<Vertex> verticies;
    std::vector<uint32_t> indicies;

	glm::mat4 object; // These will be pushed to a uniform buffer
	glm::mat4 camera;
	glm::mat4 world;

	std::string texturePath;
};

class ThiefVKModel {

public:
	ThiefVKModel(const std::string& objectFilePath, const std::string& textureFilePath);
	const geometry& getGeometry() const;

	void setObjectMatrix(const glm::mat4&);
	void setCameraMatrix(const glm::mat4&);
	void setWorldMatrix(const glm::mat4&);

	void addFragmentShaderOverride(const std::string shaderName);
	void addVertexShaderOverride(const std::string shaderName);

private:
	geometry mGeometry;
};


#endif