export module projnekomata:graphics.vulkan.vk_spec_constants;
import std;
import vulkan;
import projnekomata.corelib;

export namespace projnekomata::gfx::vkrhi {


template <typename Derived> class SpecializationConstsReflect {
public:
    [[nodiscard]] constexpr auto specializationInfo() const -> vk::SpecializationInfo {
        vk::SpecializationInfo info{};
        info.mapEntryCount = static_cast<u32>(Derived::describe().len());
        info.pMapEntries = Derived::describe().data();
        info.dataSize = sizeof(Derived);
        info.pData = reinterpret_cast<const void*>(this);
        return info;
    }
};

template <typename T> concept HasSpecializationInfo = requires(T t) { { t.specializationInfo() } -> std::convertible_to<vk::SpecializationInfo>; };

}