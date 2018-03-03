#include "ThiefVKDevice.hpp"

#include <array>

vk::VertexInputBindingDescription Vertex::getBindingDesc() {

    vk::VertexInputBindingDescription desc{};
    desc.setStride(sizeof(Vertex));
    desc.setBinding(0);
    desc.setInputRate(vk::VertexInputRate::eVertex);

    return desc;
}

std::array<vk::VertexInputAttributeDescription, 5> Vertex::getAttribDesc() {

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

    vk::VertexInputAttributeDescription atribDesctrans1{};
    atribDesctrans1.setBinding(0);
    atribDesctrans1.setLocation(0);
    atribDesctrans1.setFormat(vk::Format::eR32G32B32Sfloat);
    atribDesctrans1.setOffset(offsetof(Vertex, transCom1));

    vk::VertexInputAttributeDescription atribDesctrans2{};
    atribDesctrans2.setBinding(0);
    atribDesctrans2.setLocation(0);
    atribDesctrans2.setFormat(vk::Format::eR32G32B32Sfloat);
    atribDesctrans2.setOffset(offsetof(Vertex, transCom2));

    vk::VertexInputAttributeDescription atribDesctrans3{};
    atribDesctrans3.setBinding(0);
    atribDesctrans3.setLocation(0);
    atribDesctrans3.setFormat(vk::Format::eR32G32B32Sfloat);
    atribDesctrans3.setOffset(offsetof(Vertex, transCom1));

    return {atribDescPos, atribDescTex, atribDesctrans1, atribDesctrans2, atribDesctrans3};
}
