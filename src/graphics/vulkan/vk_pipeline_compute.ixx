export module projnekomata:graphics.vulkan.vk_pipeline_compute;
import std;
import vulkan;
import :graphics.vulkan.vk_gpu_obrm;
import :graphics.vulkan.spv_shader_code;
import :graphics.vulkan.vk_spec_constants;
import :graphics.vulkan.vk_pipeline_layout;


export namespace projnekomata::gfx::vkrhi {

class VulkanComputePipelineBuilder;
class VulkanComputePipeline {
public:
    VulkanComputePipeline(std::nullptr_t);
    VulkanComputePipeline(vk::raii::Pipeline&& vkPipeline);

    static auto builder() -> VulkanComputePipelineBuilder;

    [[nodiscard]] auto vkPipeline() const -> const vk::raii::Pipeline& { return m_vkPipeline.vkHandle(); }

private:
    VulkanAsyncRaiiWrapper<vk::raii::Pipeline> m_vkPipeline = nullptr;
};

class VulkanComputePipelineBuilder {
public:
    VulkanComputePipelineBuilder();
    VulkanComputePipelineBuilder(std::nullptr_t);

    [[nodiscard]] constexpr auto setPipelineLayout(const VulkanPipelineLayout& layout) noexcept -> VulkanComputePipelineBuilder& {
        m_pipelineLayout = layout.vkPipelineLayout();
        return *this;
    }
    [[nodiscard]] constexpr auto setShader(const SpirvShaderCode& shader) noexcept -> VulkanComputePipelineBuilder& {
        m_shaderStage = vk::StructureChain{
            vk::PipelineShaderStageCreateInfo{}
                .setStage(vk::ShaderStageFlagBits::eCompute)
                .setPName("main"),
            shader.shaderModuleCreateInfo()
        };
        return *this;
    }
    template <HasSpecializationInfo T>
    [[nodiscard]] constexpr auto setShader(const SpirvShaderCode& shader, const T& specializationInfo) noexcept -> VulkanComputePipelineBuilder& {
        m_specializationInfo = specializationInfo.specializationInfo();

        m_shaderStage = vk::StructureChain{
            vk::PipelineShaderStageCreateInfo{}
                .setStage(vk::ShaderStageFlagBits::eCompute)
                .setPName("main")
                .setPSpecializationInfo(&m_specializationInfo),
            shader.shaderModuleCreateInfo()
        };

        return *this;
    }

    [[nodiscard]] constexpr auto build() -> VulkanComputePipeline {
        auto pipelineInfo = vk::ComputePipelineCreateInfo{}
            .setStage(m_shaderStage.get<vk::PipelineShaderStageCreateInfo>())
            .setLayout(m_pipelineLayout);

        auto sc = vk::StructureChain{
            pipelineInfo,
            vk::PipelineCreateFlags2CreateInfo{}
        };

        if (VulkanContext::get().vkPhysicalDeviceProps().m_hasPipelineExecutableProperties) {
            sc.get<vk::PipelineCreateFlags2CreateInfo>().flags |= vk::PipelineCreateFlagBits2::eCaptureStatisticsKHR;
        }

        auto pipeline = vkCheckResult(VulkanContext::get().vkDevice().createComputePipeline(nullptr, sc.get<vk::ComputePipelineCreateInfo>()));

        if (VulkanContext::get().vkPhysicalDeviceProps().m_hasPipelineExecutableProperties) {
            auto pipelineObjInfo = vk::PipelineInfoKHR{}.setPipeline(pipeline);

            log::info(" ----- Pipeline Info -----");
            auto executables = Vec<vk::PipelineExecutablePropertiesKHR>::fromStdVector(vkCheckResult(VulkanContext::get().vkDevice().getPipelineExecutablePropertiesKHR(pipelineObjInfo)));

            for (auto [i, executable] : executables.iter().enumerate()) {
                log::info("  Executable {} \"{}\":", i, std::string(executable.name));
                log::info("    Description: {}", std::string(executable.description));
                log::info("    Stages: {}", vk::to_string(executable.stages));
                log::info("    Wave lane count: {}", executable.subgroupSize);
                log::info("    Statistics:");

                auto execInfo = vk::PipelineExecutableInfoKHR{}.setPipeline(pipeline).setExecutableIndex(i);
                auto execStats = Vec<vk::PipelineExecutableStatisticKHR>::fromStdVector(vkCheckResult(VulkanContext::get().vkDevice().getPipelineExecutableStatisticsKHR(execInfo)));

                log::info("       # Name                     Value                Description");
                for (auto [j, stat] : execStats.iter().enumerate()) {
                    switch (stat.format) {
                        case vk::PipelineExecutableStatisticFormatKHR::eUint64:
                            log::info("      {:>2} {:<24} {:<20} {}", j, std::string(stat.name), stat.value.u64, std::string(stat.description));
                            break;
                        case vk::PipelineExecutableStatisticFormatKHR::eInt64:
                            log::info("      {:>2} {:<24} {:<20} {}", j, std::string(stat.name), stat.value.i64, std::string(stat.description));
                            break;
                        case vk::PipelineExecutableStatisticFormatKHR::eFloat64:
                            log::info("      {:>2} {:<24} {:<20} {}", j, std::string(stat.name), stat.value.f64, std::string(stat.description));
                            break;
                        case vk::PipelineExecutableStatisticFormatKHR::eBool32:
                            log::info("      {:>2} {:<24} {:<20} {}", j, std::string(stat.name), stat.value.b32, std::string(stat.description));
                            break;
                    }
                }
            }
        }

        return VulkanComputePipeline(std::move(pipeline));
    }

private:
    vk::PipelineLayout m_pipelineLayout = nullptr;
    vk::SpecializationInfo m_specializationInfo = {};
    vk::StructureChain<vk::PipelineShaderStageCreateInfo, vk::ShaderModuleCreateInfo> m_shaderStage = {};
};

}
