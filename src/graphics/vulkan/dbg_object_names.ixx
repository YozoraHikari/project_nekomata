export module projnekomata:graphics.vulkan.dbg_object_names;
import std;
import vulkan;
import projnekomata.corelib;
import :graphics.vulkan.context;

export namespace projnekomata::gfx::vkrhi {

template <typename Derived> class BuilderObjectNameMixin {
public:
    constexpr BuilderObjectNameMixin() = default;

    constexpr auto name(std::string_view name) noexcept -> Derived& {
        if constexpr (kVulkanDebugObjectNamesEnable)
            m_name = name;
        return static_cast<Derived&>(*this);
    }

protected:
    template <typename Handle> constexpr auto dbgApplyDebugName(const Handle& handle) const noexcept -> void {
        if constexpr (kVulkanDebugObjectNamesEnable) {
            if (m_name.empty()) return;
            VulkanContext::get().vkDevice().setDebugUtilsObjectNameEXT(handle, m_name);
        }
    }

private:
    std::string m_name = "";
};

}
