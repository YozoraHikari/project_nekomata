module;
#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>
export module projnekomata:graphics.meshsystem.loader.mesh_loader;
import projnekomata.corelib;
import fmt;
import :graphics.meshsystem.handles;
import :core.fs.path_resolve;
import :core.ecs.entity;
import :core.ecs;
import :graphics.meshsystem.mesh_asset_storage;
import :core.vertex;
import :core.ecs.world.transform;
import :graphics.materialsystem.mat_manager;
import :core.ecs.world.renderable;
import :graphics.corematerial;
import :core.ecs.world.parent;

template <> struct fastgltf::ElementTraits<projnekomata::math::Vector2f> : fastgltf::ElementTraitsBase<projnekomata::math::Vector2f, AccessorType::Vec2, float> {};
template <> struct fastgltf::ElementTraits<projnekomata::math::Vector3f> : fastgltf::ElementTraitsBase<projnekomata::math::Vector3f, AccessorType::Vec3, float> {};
template <> struct fastgltf::ElementTraits<projnekomata::math::Vector4f> : fastgltf::ElementTraitsBase<projnekomata::math::Vector4f, AccessorType::Vec4, float> {};

export namespace projnekomata::gfx {

struct MeshLoadResult {
    Vec<MeshAsset> meshes = Vec<MeshAsset>::create();
    Vec<ecs::Entity> rootEntities = Vec<ecs::Entity>::create();
};

auto convertGltfSamplerParams(const fastgltf::Sampler& sampler) -> SamplerParams {
    auto result = SamplerParams::defaultValues();

    if (sampler.minFilter.has_value()) {
        switch (sampler.minFilter.value()) {
            case fastgltf::Filter::Nearest: result.setMinFilter(vk::Filter::eNearest); break;
            case fastgltf::Filter::NearestMipMapNearest: result.setMinFilter(vk::Filter::eNearest); result.setMipmapMode(vk::SamplerMipmapMode::eNearest); break;
            case fastgltf::Filter::NearestMipMapLinear: result.setMinFilter(vk::Filter::eNearest); result.setMipmapMode(vk::SamplerMipmapMode::eLinear); break;
            case fastgltf::Filter::Linear: result.setMinFilter(vk::Filter::eLinear); break;
            case fastgltf::Filter::LinearMipMapNearest: result.setMinFilter(vk::Filter::eLinear); result.setMipmapMode(vk::SamplerMipmapMode::eNearest); break;
            case fastgltf::Filter::LinearMipMapLinear: result.setMinFilter(vk::Filter::eLinear); result.setMipmapMode(vk::SamplerMipmapMode::eLinear); break;
        }
    }

    if (sampler.magFilter.has_value()) {
        switch (sampler.magFilter.value()) {
            case fastgltf::Filter::Nearest: result.setMagFilter(vk::Filter::eNearest); break;
            case fastgltf::Filter::NearestMipMapNearest: result.setMagFilter(vk::Filter::eNearest); result.setMipmapMode(vk::SamplerMipmapMode::eNearest); break;
            case fastgltf::Filter::NearestMipMapLinear: result.setMagFilter(vk::Filter::eNearest); result.setMipmapMode(vk::SamplerMipmapMode::eLinear); break;
            case fastgltf::Filter::Linear: result.setMagFilter(vk::Filter::eLinear); break;
            case fastgltf::Filter::LinearMipMapNearest: result.setMagFilter(vk::Filter::eLinear); result.setMipmapMode(vk::SamplerMipmapMode::eNearest); break;
            case fastgltf::Filter::LinearMipMapLinear: result.setMagFilter(vk::Filter::eLinear); result.setMipmapMode(vk::SamplerMipmapMode::eLinear); break;
        }
    }

    switch (sampler.wrapS) {
        case fastgltf::Wrap::Repeat: result.setAddressModeU(vk::SamplerAddressMode::eRepeat); break;
        case fastgltf::Wrap::ClampToEdge: result.setAddressModeU(vk::SamplerAddressMode::eClampToEdge); break;
        case fastgltf::Wrap::MirroredRepeat: result.setAddressModeU(vk::SamplerAddressMode::eMirroredRepeat); break;
    }
    switch (sampler.wrapT) {
        case fastgltf::Wrap::Repeat: result.setAddressModeV(vk::SamplerAddressMode::eRepeat); break;
        case fastgltf::Wrap::ClampToEdge: result.setAddressModeV(vk::SamplerAddressMode::eClampToEdge); break;
        case fastgltf::Wrap::MirroredRepeat: result.setAddressModeV(vk::SamplerAddressMode::eMirroredRepeat); break;
    }

    return result;
}

auto loadGltfTexture(const fastgltf::Asset& asset, const fastgltf::Image& image, SamplerParams& samplerParams, const std::filesystem::path& dir) -> Texture {
    return std::visit(fastgltf::visitor{
        [&](const fastgltf::sources::URI& filepath) {
            return TextureManager::get().loadKtx2TextureAsync(dir / filepath.uri.fspath(), samplerParams);
        },
        [&](const fastgltf::sources::Array& array) {
            auto data = Slice<const u8>(reinterpret_cast<const u8*>(array.bytes.data()), array.bytes.size());
            return TextureManager::get().laodKtx2TextureFromMemoryBlocking(std::string(image.name), data, samplerParams);
        },
        [&](const fastgltf::sources::BufferView& bufferView) {
            auto& srcBufferView = asset.bufferViews[bufferView.bufferViewIndex];
            auto& srcBuffer = asset.buffers[srcBufferView.bufferIndex];

            return std::visit(fastgltf::visitor{
                [&](const fastgltf::sources::Array& array) {
                    auto data = Slice<const u8>(reinterpret_cast<const u8*>(array.bytes.data() + srcBufferView.byteOffset), srcBufferView.byteLength);
                    return TextureManager::get().laodKtx2TextureFromMemoryBlocking(std::string(image.name), data, samplerParams);
                },
                [&](auto&) { panic("unsupported buffer view source for image '{}'", image.name); return Texture{}; }
            }, srcBuffer.data);
        },
        [&](auto&) { panic("unsupported image source for image '{}'", image.name); return Texture{}; }
    }, image.data);
}

auto importSceneFromGltf(ecs::World& dstWorld, const fs::Path& path, const MaterialShaderHandle& materialShader, Option<ecs::Entity> rootEntity) -> MeshLoadResult {
    log::info("loading gltf file: {}", path.string());

    auto resolvedPath = fs::PathResolver::resolve(path);

    fastgltf::Parser parser(fastgltf::Extensions::KHR_texture_basisu);
    auto databuf = fastgltf::GltfDataBuffer::FromPath(resolvedPath);
    auto easset = parser.loadGltf(databuf.get(), resolvedPath.parent_path(), fastgltf::Options::LoadExternalBuffers | fastgltf::Options::DecomposeNodeMatrices);

    if (easset.error() != fastgltf::Error::None) panic("failed to load gltf file '{}': {}", path.string(), fastgltf::getErrorMessage(easset.error()));
    auto asset = std::move(easset.get());

    auto result = MeshLoadResult{};
    auto nodeToEntity = Vec<ecs::Entity>::withCapacity(asset.nodes.size());
    auto indexToTexture = Vec<Texture>::withCapacity(asset.textures.size());
    auto materialIndexByMeshIndex = Vec<u32>::withCapacity(asset.meshes.size());
    auto indegrees = Vec<u32>::filledWith(asset.nodes.size(), 0_u32);

    // ---- Texture Import -------------------------------------------------------------------------------------------------------------------------------------

    for (auto& texture : asset.textures) {
        auto samplerParams = SamplerParams::defaultValues();
        if (texture.samplerIndex.has_value()) {
            auto& sampler = asset.samplers[texture.samplerIndex.value()];
            samplerParams = convertGltfSamplerParams(sampler);
        }

        if (!texture.basisuImageIndex.has_value()) {
            panic("gltf file '{}' texture '{}' has no image index", path.string(), texture.name);
        }

        auto& basisuImage = asset.images[texture.basisuImageIndex.value()];
        auto textureAsset = loadGltfTexture(asset, basisuImage, samplerParams, resolvedPath.parent_path());
        indexToTexture.emplace(textureAsset);
    }

    // ---- Mesh Import ----------------------------------------------------------------------------------------------------------------------------------------

    for (auto& mesh : asset.meshes) {
        for (auto& primitive : mesh.primitives) {
            auto pos = primitive.findAttribute("POSITION");
            auto norm = primitive.findAttribute("NORMAL");
            auto tangent = primitive.findAttribute("TANGENT");
            auto texcoord0 = primitive.findAttribute("TEXCOORD_0");

            if (pos == primitive.attributes.end()) continue;

            auto& posAccessor = asset.accessors[pos->accessorIndex];
            auto vertices = Vec<Vertex>::uninitialized(posAccessor.count);

            auto boundingSphereRadiusSq = 0.0_f32;
            fastgltf::iterateAccessorWithIndex<math::Vector3f>(asset, posAccessor, [&](auto val, auto idx) {
                auto yUpRHToYUpLH = Vector3f(val.x(), val.y(), -val.z());
                vertices[idx].position = yUpRHToYUpLH; boundingSphereRadiusSq = std::max(boundingSphereRadiusSq, val.lengthSquared());
            });

            bool hasNorm = false;
            if (norm != primitive.attributes.end()) {
                hasNorm = true;
                auto& normAccessor = asset.accessors[norm->accessorIndex];
                fastgltf::iterateAccessorWithIndex<math::Vector3f>(asset, normAccessor, [&](auto val, auto idx) {
                    auto yUpRHToYUpLH = Vector3f(val.x(), val.y(), -val.z());
                    vertices[idx].normal = yUpRHToYUpLH;
                });
            }

            bool hasTangent = false;
            if (tangent != primitive.attributes.end()) {
                hasTangent = true;
                auto& tangentAccessor = asset.accessors[tangent->accessorIndex];
                fastgltf::iterateAccessorWithIndex<math::Vector4f>(asset, tangentAccessor, [&](auto val, auto idx) {
                    auto yUpRHToYUpLH = Vector4f(val.x(), val.y(), -val.z(), -val.w());
                    vertices[idx].tangent = yUpRHToYUpLH;
                });
            }

            bool hasTexcoord0 = false;
            if (texcoord0 != primitive.attributes.end()) {
                hasTexcoord0 = true;
                auto& texcoordAccessor = asset.accessors[texcoord0->accessorIndex];
                fastgltf::iterateAccessorWithIndex<math::Vector2f>(asset, texcoordAccessor, [&](auto val, auto idx) { vertices[idx].texcoord = val; });
            }

            auto indices = Vec<u32>::withCapacity(vertices.len());
            if (primitive.indicesAccessor.has_value()) {
                auto& indicesAccessor = asset.accessors[primitive.indicesAccessor.value()];
                indices.resize(indicesAccessor.count, 0_u32);
                fastgltf::iterateAccessorWithIndex<u32>(asset, indicesAccessor, [&](auto val, auto idx) {
                    indices[idx] = val;
                });
            } else {
                indices.resize(vertices.len(), 0_u32);
                for (u32 i = 0; i < vertices.len(); i++) indices[i] = i;
            }

            if (!hasNorm) {
                log::warn("Mesh '{}' in file '{}' has no normals, will generate them", mesh.name, path.string());

                for (auto& vertex : vertices) vertex.normal = Vector3f(0.0f);

                for (auto i = 0_usize; i < indices.len(); i += 3) {
                    auto i0 = indices[i], i1 = indices[i + 1], i2 = indices[i + 2];
                    auto e10 = vertices[i1].position - vertices[i0].position;
                    auto e20 = vertices[i2].position - vertices[i0].position;
                    auto normal = e10.cross(e20);

                    vertices[i0].normal += normal;
                    vertices[i1].normal += normal;
                    vertices[i2].normal += normal;
                }

                for (auto& vertex : vertices) vertex.normal = vertex.normal.normalize();
            }

            if (!hasTangent) {
                log::warn("Mesh '{}' in file '{}' has no tangents!", mesh.name, path.string());
            }

            auto mesh = MeshAssetStorage::get().allocateMeshAsset();
            auto& lodlist = MeshAssetStorage::get().getLodList(mesh);
            lodlist.maxLodIndex = 0; // LODs currently unsupported
            lodlist.boundingSphereRadius = std::sqrt(boundingSphereRadiusSq);
            lodlist.lodHysteresisFactor = 1.0f;

            auto& lod = lodlist.lods[0];
            lod.screenSizeThreshold = 0.0f;
            MeshAssetStorage::get().perpareLodSpace(mesh, 0, vertices.len() * sizeof(Vertex), indices.len() * sizeof(u32), alignof(Vertex), 4);
            memcpy(lod.meshSuballocation.vertexBuffer.hostAddress, vertices.data(), vertices.len() * sizeof(Vertex));
            memcpy(lod.meshSuballocation.indexBuffer.hostAddress, indices.data(), indices.len() * sizeof(u32));

            lodlist.bestLodIndex.store(0, std::memory_order_release);
            result.meshes.emplace(mesh);

            if (primitive.materialIndex.has_value()) {
                materialIndexByMeshIndex.emplace(primitive.materialIndex.value());
            } else {
                materialIndexByMeshIndex.emplace(~0u);
            }
        }
    }

    // ---- Nodes Import ---------------------------------------------------------------------------------------------------------------------------------------

    for (auto i = 0_usize; i < asset.nodes.size(); i++) {
        auto& node = asset.nodes[i];
        auto entity = dstWorld.createEntity();
        nodeToEntity.emplace(entity);

        auto transform = std::get<fastgltf::TRS>(node.transform);
        // translation has negative Z, quaternions have negative X and Y components to transform from Y up RH to Y up LH
        auto translation = Vector3f(transform.translation.x(), transform.translation.y(), -transform.translation.z());
        auto rotation = Quaternion(-transform.rotation.x(), -transform.rotation.y(), transform.rotation.z(), transform.rotation.w());
        auto scale = Vector3f(transform.scale.x(), transform.scale.y(), transform.scale.z());
        dstWorld.emplace<LocalTransformComponent>(entity, translation, rotation, scale);
        dstWorld.emplace<WorldTransformComponent>(entity);

        if (node.meshIndex.has_value()) {
            auto materialIndex = materialIndexByMeshIndex[node.meshIndex.value()];
            auto material = CoreMaterialProps{};
            if (materialIndex != ~0u) {
                auto& materialAsset = asset.materials[materialIndex];
                if (materialAsset.normalTexture.has_value()) material.setNormalMap(indexToTexture[materialAsset.normalTexture.value().textureIndex]);
                if (materialAsset.pbrData.baseColorTexture.has_value()) {
                    material.setColor(indexToTexture[materialAsset.pbrData.baseColorTexture.value().textureIndex]);
                } else {
                    auto& fcol = materialAsset.pbrData.baseColorFactor;
                    auto col = Vector3f(fcol.x(), fcol.y(), fcol.z());
                    material.setColor(col);
                }
                if (materialAsset.emissiveTexture.has_value()) {
                    material.setEmissive(indexToTexture[materialAsset.emissiveTexture.value().textureIndex]);
                } else {
                    auto femissive = materialAsset.emissiveFactor * materialAsset.emissiveStrength;
                    auto emissive = Vector3f(femissive.x(), femissive.y(), femissive.z());
                    material.setEmissive(emissive);
                }
                if (materialAsset.pbrData.metallicRoughnessTexture.has_value()) {
                    log::warn("metallic and roughness textures not supported yet");
                }
                material.setMetallic(materialAsset.pbrData.metallicFactor);
                material.setRoughness(materialAsset.pbrData.roughnessFactor);
            } else {
                material.setColor(Vector3f(0.5f, 0.5f, 0.5f));
                material.setMetallic(0.0f);
                material.setRoughness(0.75f);
            }

            auto matl = Material::create<CoreMaterialProps>(materialShader, std::move(material));
            dstWorld.emplace<RenderableComponent>(entity, result.meshes[node.meshIndex.value()], matl);
        }

        result.rootEntities.emplace(entity);
    }

    for (auto i = 0_usize; i < asset.nodes.size(); i++) {
        auto& node = asset.nodes[i];
        if (node.children.empty()) continue;

        auto children = Vec<ecs::Entity>::withCapacity(node.children.size());

        for (auto child : node.children) {
            children.emplace(nodeToEntity[child]);
            dstWorld.emplace<ParentComponent>(nodeToEntity[child], nodeToEntity[i]);
            indegrees[child]++;
        }

        dstWorld.emplace<ChildrenComponent>(nodeToEntity[i], std::move(children));
    }

    if (rootEntity.isSome()) {
        auto rootEnt = rootEntity.unwrap();
        for (auto i = 0_usize; i < asset.nodes.size(); i++) {
            if (indegrees[i] != 0) continue;

            dstWorld.emplace<ParentComponent>(nodeToEntity[i], rootEnt);

            if (!dstWorld.components<ChildrenComponent>().containsEntity(rootEnt)) {
                dstWorld.emplace<ChildrenComponent>(rootEnt, Vec<ecs::Entity>::create());
            }
            dstWorld.components<ChildrenComponent>().get(rootEnt).m_children.emplace(nodeToEntity[i]);
        }
    }

    return result;
}

}
