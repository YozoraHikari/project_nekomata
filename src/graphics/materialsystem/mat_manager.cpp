module projnekomata;
import :graphics.materialsystem.mat_manager;

namespace projnekomata::gfx {

MaterialManager::MaterialManager(std::nullptr_t) {}

MaterialManager::MaterialManager(vkrhi::VulkanPipelineLayout&& pipelineLayout)
    : m_globalPipelineLayout(std::move(pipelineLayout)) {}

auto MaterialManager::create() -> Unique<MaterialManager> {
    debug_assert(g_instance == nullptr, "MaterialManager already exists");
    auto globalPipelineLayout = vkrhi::VulkanPipelineLayout::builder()
        .addPushConstantRange(0, 128, vk::ShaderStageFlagBits::eAll)
        .addDescriptorSetLayout(TextureManager::get().shaderResourceTable().descriptorSetLayout())
        .build();

    auto manager = Unique<MaterialManager>::create(std::move(globalPipelineLayout));
    g_instance = manager.ptr();
    return manager;
}

}
