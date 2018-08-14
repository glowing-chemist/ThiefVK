#include "ThiefVKModel.hpp"
#include "ThiefVKVertex.hpp"

#include "tiny_obj_loader.h"

#include <unordered_map>
#include <fstream>
#include <iostream>
#include <iterator>
#include <vector>

ThiefVKModel::ThiefVKModel(const std::string& objectFileName, const std::string& textureFileName) {

	tinyobj::attrib_t attrib;
	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;

	tinyobj::LoadObj(&attrib, &shapes, &materials, nullptr, objectFileName.c_str());

	std::unordered_map<Vertex, uint32_t> uniqueVertices{};

	for (const auto& shape : shapes) {
	    for (const auto& index : shape.mesh.indices) {
	        Vertex vertex{};

	        vertex.pos = {
	            attrib.vertices[3 * index.vertex_index],
	            attrib.vertices[3 * index.vertex_index + 1],
	            attrib.vertices[3 * index.vertex_index + 2]
	        };

	        vertex.tex = {
	            attrib.texcoords[2 * index.texcoord_index],
	            1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
	        };

	        /*vertex.norm = {
	        	attrib.normals[3 * index.normal_index],
	        	attrib.normals[3 * index.normal_index + 1],
	        	attrib.normals[3 * index.normal_index] + 2,
	        };*/

	        // albedo isn't currently implemeted to load from object file.
	        // will need to extract it from materials at some point
	        vertex.albedo = 0.0f;

	        if (uniqueVertices.count(vertex) == 0) {
	            uniqueVertices[vertex] = static_cast<uint32_t>(mGeometry.verticies.size());
	            mGeometry.verticies.push_back(vertex);
	        }

	        mGeometry.indicies.push_back(uniqueVertices[vertex]);
	    }
	}

	mGeometry.texturePath = textureFileName;

#ifndef NDEBUG
	dumpBinaryVerticies("./chaletVerticies.bin");
	dumpBinaryIndicies("./chaletIndicies.bin");
#endif
}


ThiefVKModel::ThiefVKModel(const std::string& binaryVertexFilePath, const std::string& binaryIndexFilePath, const std::string& textureFilePath) {
	std::ifstream binaryVertexFile{binaryVertexFilePath, std::ios::binary};
	std::vector<char> binaryVertexData(std::istreambuf_iterator<char>(binaryVertexFile), std::istreambuf_iterator<char>{});
	mGeometry.verticies.resize(binaryVertexData.size() / sizeof(Vertex));
	std::memmove(mGeometry.verticies.data(), binaryVertexData.data(), binaryVertexData.size());

	std::ifstream binaryIndexile{binaryIndexFilePath, std::ios::binary};
	std::vector<char> binaryIndexData(std::istreambuf_iterator<char>(binaryIndexile), std::istreambuf_iterator<char>{});
	mGeometry.indicies.resize(binaryIndexData.size() / sizeof(uint32_t));
	std::memmove(mGeometry.indicies.data(), binaryIndexData.data(), binaryIndexData.size());

	mGeometry.texturePath = textureFilePath;
}


const geometry& ThiefVKModel::getGeometry() const {
	return mGeometry;
}


void ThiefVKModel::setObjectMatrix(const glm::mat4& object) {
	mGeometry.object = object;
}


void ThiefVKModel::setCameraMatrix(const glm::mat4& camera) {
	mGeometry.camera = camera;
}


void ThiefVKModel::setWorldMatrix(const glm::mat4& world) {
	mGeometry.world = world;
}

	
void ThiefVKModel::addFragmentShaderOverride(const std::string) {
	// currently unimplemented in the device
}
	

void ThiefVKModel::addVertexShaderOverride(const std::string) {
	// currently unimplemented in the device
}


void ThiefVKModel::dumpBinaryVerticies(const std::string& filePath) const {
	std::ofstream binaryFile{};
	binaryFile.open(filePath,  std::ofstream::binary);

	const char* vertexData = reinterpret_cast<const char*>(mGeometry.verticies.data());
	const size_t vertexSize = mGeometry.verticies.size() * sizeof(Vertex);
	for(unsigned int i = 0;i < vertexSize; ++i) {
		binaryFile << vertexData[i];
	}

	binaryFile.close();
}


void ThiefVKModel::dumpBinaryIndicies(const std::string& filePath) const {
	std::ofstream binaryFile{};
	binaryFile.open(filePath,  std::ofstream::binary);

	const char* indexData = reinterpret_cast<const char*>(mGeometry.indicies.data());
	const size_t indexSize = mGeometry.indicies.size() * sizeof(uint32_t);
	for(unsigned int i = 0;i < indexSize; ++i) {
		binaryFile << indexData[i];
	}
}