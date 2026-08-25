export module projnekomata:graphics.materialsystem.mat_manager;
import std;
import projnekomata.corelib;
import :graphics.vulkan.vk_pipeline_layout;
import :core.containers.freelist_pool;
import :graphics.vulkan.vk_pipeline_graphics;

export namespace projnekomata::gfx {

enum class MaterialPassType {
    Deferred,
};

class MaterialShaderBuilder;

class MaterialShader {
public:
    MaterialShader(std::nullptr_t) {}
    MaterialShader(MaterialPassType passType, vkrhi::VulkanGraphicsPipeline&& pipeline, usize materialPropStructSize)
        : m_passType(passType), m_pipeline(std::move(pipeline)), m_materialPropStructSize(materialPropStructSize) {}

    [[nodiscard]] constexpr auto passType() const noexcept -> MaterialPassType { return m_passType; }
    [[nodiscard]] constexpr auto pipeline() const noexcept -> const vkrhi::VulkanGraphicsPipeline& { return m_pipeline; }
    [[nodiscard]] constexpr auto materialPropStructSize() const noexcept -> usize { return m_materialPropStructSize; }
    [[nodiscard]] constexpr static auto builder() noexcept -> MaterialShaderBuilder;

private:
    MaterialPassType m_passType       = MaterialPassType::Deferred;
    vkrhi::VulkanGraphicsPipeline m_pipeline = nullptr;
    usize m_materialPropStructSize = 0;
};

struct MaterialShaderHandle {
    u32 shaderIndex;

    auto operator->() -> MaterialShader*;
    auto operator->() const -> const MaterialShader*;
};

template <typename T> struct TypedMaterialPropertiesHandle {
    u32 propertiesIndex;

    auto operator->() -> T*;
    auto operator->() const -> const T*;
};

struct MaterialPropertiesHandle {
    u32 propertiesIndex;

    MaterialPropertiesHandle(u32 heapIndex) noexcept : propertiesIndex(heapIndex) {}
    template <typename T> MaterialPropertiesHandle(const TypedMaterialPropertiesHandle<T>& other) noexcept
        : propertiesIndex(other.propertiesIndex) {}

    template <typename T> auto asTyped() const -> TypedMaterialPropertiesHandle<T> { return TypedMaterialPropertiesHandle<T>{ propertiesIndex }; }
};

class MaterialManager {
public:
    MaterialManager(std::nullptr_t);
    MaterialManager(vkrhi::VulkanPipelineLayout&& pipelineLayout);

    static auto get() -> MaterialManager& { return *g_instance; }
    static auto create() -> Unique<MaterialManager>;

    [[nodiscard]] constexpr auto globalPipelineLayout() const noexcept -> const vkrhi::VulkanPipelineLayout& { return m_globalPipelineLayout; }

    [[nodiscard]] constexpr auto materialShaderHeap() noexcept -> VaSlotmap<MaterialShader, 1024>& { return m_materialShaders; }
    [[nodiscard]] constexpr auto materialShaderHeap() const noexcept -> const VaSlotmap<MaterialShader, 1024>& { return m_materialShaders; }
    [[nodiscard]] constexpr auto materialParamHeap(usize structSize) noexcept -> VaSlotmapNoType<4096>& { return *m_materialParamPoolsByStructSize[structSize]; }
    [[nodiscard]] constexpr auto materialParamHeap(usize structSize) const noexcept -> const VaSlotmapNoType<4096>& { return *m_materialParamPoolsByStructSize[structSize]; }
    [[nodiscard]] constexpr auto materialParamHeapsMap() noexcept -> HashMap<usize, Unique<VaSlotmapNoType<4096>>>& { return m_materialParamPoolsByStructSize; }

    constexpr auto ensureMaterialParamHeapBin(usize structSize) noexcept -> void {
        if (m_materialParamPoolsByStructSize.contains(structSize)) return;
        m_materialParamPoolsByStructSize.insert(structSize, VaSlotmapNoType<4096>::createUnique(structSize));
    }

private:
    static inline MaterialManager* g_instance = nullptr;
    vkrhi::VulkanPipelineLayout m_globalPipelineLayout = nullptr;

    VaSlotmap<MaterialShader, 1024> m_materialShaders = VaSlotmap<MaterialShader, 1024>::create();
    HashMap<usize, Unique<VaSlotmapNoType<4096>>> m_materialParamPoolsByStructSize = HashMap<usize, Unique<VaSlotmapNoType<4096>>>::create();

    friend class MaterialShaderBuilder;
};

auto MaterialShaderHandle::operator->() -> MaterialShader* { return &MaterialManager::get().materialShaderHeap()[shaderIndex]; }
auto MaterialShaderHandle::operator->() const -> const MaterialShader* { return &MaterialManager::get().materialShaderHeap()[shaderIndex]; }

template <typename T> auto TypedMaterialPropertiesHandle<T>::operator->() -> T* { return &MaterialManager::get().materialParamHeap(sizeof(T)).load<T>(propertiesIndex); }
template <typename T> auto TypedMaterialPropertiesHandle<T>::operator->() const -> const T* { return &MaterialManager::get().materialParamHeap(sizeof(T)).load<T>(propertiesIndex); }

class MaterialShaderBuilder {
public:
    [[nodiscard]] constexpr auto setPrerastVS(const vkrhi::SpirvShaderCode& shader) noexcept -> MaterialShaderBuilder& {
        auto& _ = m_vkGraphicsPipelineBuilder.addShader(shader, vk::ShaderStageFlagBits::eVertex);
        return *this;
    }

    [[nodiscard]] constexpr auto setFragmentShader(const vkrhi::SpirvShaderCode& shader) noexcept -> MaterialShaderBuilder& {
        auto& _ = m_vkGraphicsPipelineBuilder.addShader(shader, vk::ShaderStageFlagBits::eFragment);
        return *this;
    }

    [[nodiscard]] constexpr auto useInDeferredPass() noexcept -> MaterialShaderBuilder& {
        // static u32 inputLocationsAllUnused[5] = { vk::AttachmentUnused, vk::AttachmentUnused, vk::AttachmentUnused, vk::AttachmentUnused, vk::AttachmentUnused };
        // static u32 deferredGeomStageLocations[5] = { 0, 1, 2, 3, vk::AttachmentUnused };

        auto& _ = m_vkGraphicsPipelineBuilder
            // Albedo + Roughness
            .pushRenderingAttachment(
                vk::PipelineColorBlendAttachmentState{}
                     .setBlendEnable(false)
                     .setColorWriteMask(vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA),
                vk::Format::eR8G8B8A8Srgb
            )
            // Normals
            .pushRenderingAttachment(
                vk::PipelineColorBlendAttachmentState{}
                     .setBlendEnable(false)
                     .setColorWriteMask(vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA),
                vk::Format::eR16G16B16A16Snorm
            )
            // Metallic + AO
            .pushRenderingAttachment(
                vk::PipelineColorBlendAttachmentState{}
                     .setBlendEnable(false)
                     .setColorWriteMask(vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA),
                vk::Format::eR8G8Unorm
            )
            // Motion Vectors
            .pushRenderingAttachment(
                vk::PipelineColorBlendAttachmentState{}
                    .setBlendEnable(false)
                    .setColorWriteMask(vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA),
                vk::Format::eR16G16Sfloat
            );
            //.pushUnusedAttachment(vk::Format::eR8G8B8A8Srgb) // Color
            //.setRenderingAttachmentLocations(Slice<const u32>(deferredGeomStageLocations, 5))
            //.setRenderingInputAttachmentIndices(Slice<const u32>(inputLocationsAllUnused, 5), nullptr);
        return *this;
    }

    [[nodiscard]] constexpr auto setMaterialPropertyStructSize(usize size) noexcept -> MaterialShaderBuilder& {
        m_materialPropStructSize = size;
        return *this;
    }

    [[nodiscard]] constexpr auto buildIntoOwned() noexcept -> MaterialShader {
        return MaterialShader(MaterialPassType::Deferred, m_vkGraphicsPipelineBuilder.build(), m_materialPropStructSize);
    }

    [[nodiscard]] constexpr auto build() noexcept -> MaterialShaderHandle {
        auto shader = buildIntoOwned();
        return MaterialShaderHandle{ MaterialManager::get().m_materialShaders.emplace(std::move(shader)) };
    }

private:
    vkrhi::VulkanGraphicsPipelineBuilder m_vkGraphicsPipelineBuilder = vkrhi::VulkanGraphicsPipeline::builder();
    usize m_materialPropStructSize = 0;

    constexpr MaterialShaderBuilder() {
        auto& _ = m_vkGraphicsPipelineBuilder
            .setPipelineLayout(MaterialManager::get().globalPipelineLayout())
            .setInputTopology(vk::PrimitiveTopology::eTriangleList)
            .setRastPolygonMode(vk::PolygonMode::eFill)
            .setRastCulling(vk::CullModeFlagBits::eBack, vk::FrontFace::eCounterClockwise)
            .setRastLineWidth(1.0)
            .disableMultisampling()
            .enableDepthTest()
            .enableDepthWrite()
            .setDepthAttachmentFormat(vk::Format::eD32Sfloat);
    }

    friend class MaterialShader;
};
constexpr auto MaterialShader::builder() noexcept -> MaterialShaderBuilder { return MaterialShaderBuilder(); }

struct Material {
    MaterialShaderHandle shader;
    MaterialPropertiesHandle properties;

    template <typename PropType> static auto create(MaterialShaderHandle shader, PropType&& properties) -> Material {
        MaterialManager::get().ensureMaterialParamHeapBin(sizeof(PropType));
        return Material{
            .shader = shader,
            .properties = MaterialPropertiesHandle(MaterialManager::get().materialParamHeap(sizeof(PropType)).emplace<PropType>(std::forward<PropType>(properties)))
        };
    }
};

}
