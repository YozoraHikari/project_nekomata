export module projnekomata:graphics.vulkan.vk_pipeline_graphics;
import std;
import vulkan;
import projnekomata.cs;
import :graphics.vulkan.vk_gpu_obrm;
import :graphics.vulkan.spv_shader_code;
import :graphics.vulkan.vk_pipeline_layout;
import :graphics.vulkan.context;
import :graphics.vulkan.shadercache;
import :graphics.vulkan.vk_spec_constants;

export namespace projnekomata::gfx::vkrhi {

class VulkanGraphicsPipelineBuilder;
class VulkanGraphicsPipeline {
public:
    VulkanGraphicsPipeline(std::nullptr_t);
    VulkanGraphicsPipeline(vk::raii::Pipeline&& vkPipeline);

    static auto builder() -> VulkanGraphicsPipelineBuilder;

    [[nodiscard]] auto vkPipeline() const -> const vk::raii::Pipeline& { return m_vkPipeline.vkHandle(); }

private:
    VulkanAsyncRaiiWrapper<vk::raii::Pipeline> m_vkPipeline = nullptr;
};

class VulkanGraphicsPipelineBuilder {
public:
    VulkanGraphicsPipelineBuilder();
    VulkanGraphicsPipelineBuilder(std::nullptr_t);

    VulkanGraphicsPipelineBuilder(const VulkanGraphicsPipelineBuilder&) = delete;
    VulkanGraphicsPipelineBuilder(VulkanGraphicsPipelineBuilder&&) = default;
    VulkanGraphicsPipelineBuilder& operator=(const VulkanGraphicsPipelineBuilder&) = delete;
    VulkanGraphicsPipelineBuilder& operator=(VulkanGraphicsPipelineBuilder&&) = default;

    [[nodiscard]] constexpr auto addShader(const SpirvShaderCode& shader, vk::ShaderStageFlagBits stage) noexcept -> VulkanGraphicsPipelineBuilder& {
        auto stageTuple = vk::StructureChain<vk::PipelineShaderStageCreateInfo, vk::ShaderModuleCreateInfo>{
            vk::PipelineShaderStageCreateInfo{}
                .setStage(stage)
                .setPName("main"),
            shader.shaderModuleCreateInfo()
        };
        m_shaderStages.emplace(stageTuple);
        return *this;
    }

    template <HasSpecializationInfo T>
    [[nodiscard]] constexpr auto addShader(const SpirvShaderCode& shader, vk::ShaderStageFlagBits stage, const T& specializationInfo) noexcept -> VulkanGraphicsPipelineBuilder& {
        m_specializationInfos.emplace(specializationInfo.specializationInfo());
        auto specInfoPtr = &m_specializationInfos.last();

        auto stageTuple = vk::StructureChain<vk::PipelineShaderStageCreateInfo, vk::ShaderModuleCreateInfo>{
            vk::PipelineShaderStageCreateInfo{}
                .setStage(stage)
                .setPName("main")
                .setPSpecializationInfo(specInfoPtr),
            shader.shaderModuleCreateInfo()
        };

        m_shaderStages.emplace(stageTuple);
        return *this;
    }

    [[nodiscard]] constexpr auto setPipelineLayout(const VulkanPipelineLayout& layout) noexcept -> VulkanGraphicsPipelineBuilder& {
        m_pipelineLayout = layout.vkPipelineLayout();
        return *this;
    }
    [[nodiscard]] constexpr auto setInputTopology(vk::PrimitiveTopology topology) noexcept -> VulkanGraphicsPipelineBuilder& {
        m_inputAssemblyState.topology = topology;
        return *this;
    }
    [[nodiscard]] constexpr auto setRastLineWidth(f32 lineWidth) noexcept -> VulkanGraphicsPipelineBuilder& {
        m_rasterizationState.lineWidth = lineWidth;
        return *this;
    }
    [[nodiscard]] constexpr auto setRastPolygonMode(vk::PolygonMode polygonMode) noexcept -> VulkanGraphicsPipelineBuilder& {
        m_rasterizationState.polygonMode = polygonMode;
        return *this;
    }
    [[nodiscard]] constexpr auto setRastCulling(vk::CullModeFlags cullModeFlags, vk::FrontFace frontFace) noexcept -> VulkanGraphicsPipelineBuilder& {
        m_rasterizationState.cullMode = cullModeFlags;
        m_rasterizationState.frontFace = frontFace;
        return *this;
    }
    [[nodiscard]] constexpr auto disableMultisampling() noexcept -> VulkanGraphicsPipelineBuilder& {
        m_multisampleState.sampleShadingEnable = false;
        m_multisampleState.rasterizationSamples = vk::SampleCountFlagBits::e1;
        m_multisampleState.minSampleShading = 1.0;
        m_multisampleState.alphaToCoverageEnable = false;
        m_multisampleState.alphaToOneEnable = false;   
        return *this;
    }
    [[nodiscard]] constexpr auto pushRenderingAttachment(vk::PipelineColorBlendAttachmentState blending, vk::Format attachmentFormat) noexcept -> VulkanGraphicsPipelineBuilder& {
        m_renderingColorAttachmentBlendStates.emplace(blending);
        m_renderingColorAttachmentFormats.emplace(attachmentFormat);
        return *this;
    }
    [[nodiscard]] constexpr auto pushInputAttachment(vk::Format attachmentFormat) noexcept -> VulkanGraphicsPipelineBuilder& {
        m_renderingColorAttachmentFormats.emplace(attachmentFormat);
        m_renderingColorAttachmentBlendStates.emplace(vk::PipelineColorBlendAttachmentState{}.setBlendEnable(false));
        return *this;
    }
    [[nodiscard]] constexpr auto pushUnusedAttachment(vk::Format attachmentFormat) noexcept -> VulkanGraphicsPipelineBuilder& {
        m_renderingColorAttachmentFormats.emplace(attachmentFormat);
        m_renderingColorAttachmentBlendStates.emplace(vk::PipelineColorBlendAttachmentState{}.setBlendEnable(false));
        return *this;
    }
    [[nodiscard]] constexpr auto disableDepthTest() noexcept -> VulkanGraphicsPipelineBuilder& {
        m_depthStencilState.depthTestEnable = false;
        m_depthStencilState.depthWriteEnable = false;
        m_depthStencilState.depthCompareOp = vk::CompareOp::eNever;
        m_depthStencilState.depthBoundsTestEnable = false;
        m_depthStencilState.stencilTestEnable = false;
        m_depthStencilState.minDepthBounds = 0.0;
        m_depthStencilState.maxDepthBounds = 1.0;
        return *this;
    }
    [[nodiscard]] constexpr auto enableDepthTest() noexcept -> VulkanGraphicsPipelineBuilder& {
        m_depthStencilState.depthTestEnable = true;
        m_depthStencilState.depthCompareOp = vk::CompareOp::eGreater;
        m_depthStencilState.depthBoundsTestEnable = false;
        m_depthStencilState.stencilTestEnable = false;
        m_depthStencilState.minDepthBounds = 0.0;
        m_depthStencilState.maxDepthBounds = 1.0;
        return *this;
    }
    [[nodiscard]] constexpr auto enableDepthWrite() noexcept -> VulkanGraphicsPipelineBuilder& {
        m_depthStencilState.depthWriteEnable = true;
        return *this;
    }
    [[nodiscard]] constexpr auto setDepthAttachmentFormat(vk::Format format) noexcept -> VulkanGraphicsPipelineBuilder& {
        m_renderingCreateInfo.depthAttachmentFormat = format;
        return *this;
    }
    [[nodiscard]] constexpr auto setMultiviewViewsMask(u32 mask) noexcept -> VulkanGraphicsPipelineBuilder& {
        m_renderingCreateInfo.viewMask = mask;
        return *this;
    }
    [[nodiscard]] constexpr auto setTessellationPatchControlPoints(u32 patchControlPoints) noexcept -> VulkanGraphicsPipelineBuilder& {
        m_tessellationState = Some(
            vk::PipelineTessellationStateCreateInfo{}
                .setPatchControlPoints(patchControlPoints)
        );
        return *this;
    }
    [[nodiscard]] constexpr auto setRenderingAttachmentLocations(Slice<const u32> locations) noexcept -> VulkanGraphicsPipelineBuilder& {
        m_renderingAttachmentLocationInfo = Some(
            vk::RenderingAttachmentLocationInfo{}
                .setColorAttachmentLocations(locations)
        );
        return *this;
    }
    [[nodiscard]] constexpr auto setRenderingInputAttachmentIndices(Slice<const u32> indices, const u32* depthIndex) noexcept -> VulkanGraphicsPipelineBuilder& {
        m_renderingInputAttachmentIndexInfo = Some(
            vk::RenderingInputAttachmentIndexInfo{}
                .setColorAttachmentInputIndices(indices)
                .setPDepthInputAttachmentIndex(depthIndex)
        );
        return *this;
    }

    [[nodiscard]] constexpr auto build() -> VulkanGraphicsPipeline {
        auto viewportState = vk::PipelineViewportStateCreateInfo{}
            .setViewportCount(1)
            .setScissorCount(1);

        auto vertexInputState = vk::PipelineVertexInputStateCreateInfo{};
        
        auto colorBlendState = vk::PipelineColorBlendStateCreateInfo{}
            .setLogicOpEnable(false)
            .setAttachments(m_renderingColorAttachmentBlendStates);
        
        auto dynamicStateList = std::array<vk::DynamicState, 2>{vk::DynamicState::eViewport, vk::DynamicState::eScissor};
        auto dynamicState = vk::PipelineDynamicStateCreateInfo{}
            .setDynamicStates(dynamicStateList);

        m_renderingCreateInfo = m_renderingCreateInfo.setColorAttachmentFormats(m_renderingColorAttachmentFormats);

        auto stagesRaw = m_shaderStages.iter()
            .map([](auto&& chain) -> vk::PipelineShaderStageCreateInfo { return chain.template get<vk::PipelineShaderStageCreateInfo>(); })
            .collect<Vec>();

        auto pipelineInfo = vk::GraphicsPipelineCreateInfo{}
            .setStages(stagesRaw)
            .setPVertexInputState(&vertexInputState)
            .setPInputAssemblyState(&m_inputAssemblyState)
            .setPViewportState(&viewportState)
            .setPRasterizationState(&m_rasterizationState)
            .setPMultisampleState(&m_multisampleState)
            .setPColorBlendState(&colorBlendState)
            .setPDepthStencilState(&m_depthStencilState)
            .setPDynamicState(&dynamicState)
            .setLayout(m_pipelineLayout);

        if (m_tessellationState.isSome()) {
            pipelineInfo.pTessellationState = &m_tessellationState.unwrap();
        }

        auto sc = vk::StructureChain{
            pipelineInfo,
            m_renderingCreateInfo,
            vk::RenderingAttachmentLocationInfo{},
            vk::RenderingInputAttachmentIndexInfo{},
            vk::PipelineBinaryInfoKHR{},
            vk::PipelineCreateFlags2CreateInfo{}
        };
        if (m_renderingAttachmentLocationInfo.isSome()) {
            sc.assign<vk::RenderingAttachmentLocationInfo>(m_renderingAttachmentLocationInfo.unwrap());
        } else {
            sc.unlink<vk::RenderingAttachmentLocationInfo>();
        }
        if (m_renderingInputAttachmentIndexInfo.isSome()) {
            sc.assign<vk::RenderingInputAttachmentIndexInfo>(m_renderingInputAttachmentIndexInfo.unwrap());
        } else {
            sc.unlink<vk::RenderingInputAttachmentIndexInfo>();
        }

        auto pipeline = VulkanContext::get().shaderCache()->createGraphicsPipeline(sc);

        return VulkanGraphicsPipeline(std::move(pipeline));
    }

private:
    vk::PipelineLayout m_pipelineLayout = nullptr;
    Option<vk::RenderingAttachmentLocationInfo> m_renderingAttachmentLocationInfo = None;
    Option<vk::RenderingInputAttachmentIndexInfo> m_renderingInputAttachmentIndexInfo = None;
    vk::PipelineInputAssemblyStateCreateInfo m_inputAssemblyState = {};
    vk::PipelineRasterizationStateCreateInfo m_rasterizationState = {};
    vk::PipelineMultisampleStateCreateInfo m_multisampleState = {};
    vk::PipelineDepthStencilStateCreateInfo m_depthStencilState = {};
    vk::PipelineRenderingCreateInfo m_renderingCreateInfo = {};
    Option<vk::PipelineTessellationStateCreateInfo> m_tessellationState = None;
    Vec<vk::PipelineColorBlendAttachmentState> m_renderingColorAttachmentBlendStates;
    Vec<vk::Format> m_renderingColorAttachmentFormats;
    Vec<vk::SpecializationInfo> m_specializationInfos = Vec<vk::SpecializationInfo>::withCapacity(5); // VS + TCS + TES + GS + FS

    Vec<vk::StructureChain<vk::PipelineShaderStageCreateInfo, vk::ShaderModuleCreateInfo>> m_shaderStages = Vec<vk::StructureChain<vk::PipelineShaderStageCreateInfo, vk::ShaderModuleCreateInfo>>::create();
};

}