module;
#include <stddef.h>
export module projnekomata:core.vertex;
import :core.math;
import vulkan;

export namespace projnekomata {
struct Vertex {
    math::Vector3f normal;
    math::Vector4f tangent;
    math::Vector2f texcoord;
};

const auto kVertexBindingsPositionOnly = vk::VertexInputBindingDescription{}
    .setBinding(0)
    .setInputRate(vk::VertexInputRate::eVertex)
    .setStride(sizeof(math::Vector3f));

const Slice<const vk::VertexInputBindingDescription> kVertexBindingsAll = StaticSlice<const vk::VertexInputBindingDescription>::inst<
    vk::VertexInputBindingDescription{}
        .setBinding(0)
        .setInputRate(vk::VertexInputRate::eVertex)
        .setStride(sizeof(math::Vector3f)),
    vk::VertexInputBindingDescription{}
        .setBinding(1)
        .setInputRate(vk::VertexInputRate::eVertex)
        .setStride(sizeof(Vertex))
>();

const auto kVertexAttributesPositionOnly = vk::VertexInputAttributeDescription{}
    .setBinding(0)
    .setFormat(vk::Format::eR32G32B32Sfloat)
    .setLocation(0)
    .setOffset(0);

const Slice<const vk::VertexInputAttributeDescription> kVertexAttributesAll = StaticSlice<const vk::VertexInputAttributeDescription>::inst<
    vk::VertexInputAttributeDescription{}
        .setBinding(0)
        .setFormat(vk::Format::eR32G32B32Sfloat)
        .setLocation(0)
        .setOffset(0),
    vk::VertexInputAttributeDescription{}
        .setBinding(1)
        .setFormat(vk::Format::eR32G32B32Sfloat)
        .setLocation(1)
        .setOffset(offsetof(Vertex, normal)),
    vk::VertexInputAttributeDescription{}
        .setBinding(1)
        .setFormat(vk::Format::eR32G32B32A32Sfloat)
        .setLocation(2)
        .setOffset(offsetof(Vertex, tangent)),
    vk::VertexInputAttributeDescription{}
        .setBinding(1)
        .setFormat(vk::Format::eR32G32Sfloat)
        .setLocation(3)
        .setOffset(offsetof(Vertex, texcoord))
>();

}
