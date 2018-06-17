#include "ThiefVKVertex.hpp"

// Vertex member functions
vk::VertexInputBindingDescription Vertex::getBindingDesc() {

    vk::VertexInputBindingDescription desc{};
    desc.setStride(sizeof(Vertex));
    desc.setBinding(0);
    desc.setInputRate(vk::VertexInputRate::eVertex);

    return desc;
}

std::array<vk::VertexInputAttributeDescription, 3> Vertex::getAttribDesc() {

    vk::VertexInputAttributeDescription atribDescPos{};
    atribDescPos.setBinding(0);
    atribDescPos.setLocation(0);
    atribDescPos.setFormat(vk::Format::eR32G32B32Sfloat);
    atribDescPos.setOffset(offsetof(Vertex, pos));


    vk::VertexInputAttributeDescription atribDescTex{};
    atribDescTex.setBinding(0);
	atribDescTex.setLocation(1);
    atribDescTex.setFormat(vk::Format::eR32G32Sfloat);
    atribDescTex.setOffset(offsetof(Vertex, tex));

	vk::VertexInputAttributeDescription atribDescNormal{};
	atribDescNormal.setBinding(0);
	atribDescNormal.setLocation(2);
	atribDescNormal.setFormat(vk::Format::eR32G32B32Sfloat);
	atribDescNormal.setOffset(offsetof(Vertex, norm));

	return {atribDescPos, atribDescTex, atribDescNormal};
}


bool operator==(Vertex& lhs, Vertex& rhs) {
    return lhs.pos == rhs.pos && lhs.norm == rhs.norm && lhs.tex == rhs.tex;
}