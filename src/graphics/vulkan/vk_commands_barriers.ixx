export module projnekomata:graphics.vulkan.vk_commands_barriers;
import std;
import projnekomata.corelib;
import vulkan;
import :graphics.vulkan.vk_image_trait;
import :graphics.vulkan.vk_commands;

export namespace projnekomata::gfx::vkrhi {

class VulkanPipelineBarriers {
public:
    static auto builder() -> VulkanPipelineBarriers {
        return VulkanPipelineBarriers();
    }

    /**
     * Inserts an image memory barrier to transition the image from one layout to another
     * and synchronize access between GPU pipeline stages.
     *
     * Note that this function automatically considers the full subresource range of the image and ignores any queue
     * family transitions.
     *
     * @param image The VulkanImage object representing the target image.
     * @param srcLayout The current layout of the image before the transition.
     * @param srcStage The pipeline stage where image access begins in the source state.
     * @param srcAccess The access flags specifying how the image is being accessed in the source state.
     * @param dstLayout The desired layout of the image after the transition.
     * @param dstStage The pipeline stage where image access will occur in the destination state.
     * @param dstAccess The access flags specifying how the image will be accessed in the destination state.
     */

    template <CVulkanImage TVulkanImage>
    auto insertImageMemoryBarrier(TVulkanImage& image, vk::ImageLayout srcLayout, vk::PipelineStageFlags2 srcStage, vk::AccessFlags2 srcAccess, vk::ImageLayout dstLayout, vk::PipelineStageFlags2 dstStage, vk::AccessFlags2 dstAccess) -> VulkanPipelineBarriers& {
        auto barrier = vk::ImageMemoryBarrier2{}
            .setOldLayout(srcLayout)
            .setSrcStageMask(srcStage)
            .setSrcAccessMask(srcAccess)
            .setSrcQueueFamilyIndex(vk::QueueFamilyIgnored)
            .setNewLayout(dstLayout)
            .setDstStageMask(dstStage)
            .setDstAccessMask(dstAccess)
            .setDstQueueFamilyIndex(vk::QueueFamilyIgnored)
            .setImage(image.vkImage())
            .setSubresourceRange(image.subresourceRangeFull());
        m_imageMemoryBarriers.emplace(barrier);
        return *this;
    }

    auto byRegion() -> VulkanPipelineBarriers& {
        m_dependencyFlags |= vk::DependencyFlagBits::eByRegion;
        return *this;
    }

    auto flush(VulkanCommandBuffer& cb) -> void {
        auto dependency_info = vk::DependencyInfo{}
            .setImageMemoryBarriers(m_imageMemoryBarriers)
            .setBufferMemoryBarriers(m_bufferMemoryBarriers)
            .setMemoryBarriers(m_memoryBarriers);
        cb.vkCommandBuffer().pipelineBarrier2(dependency_info);
    }

private:

    Vec<vk::BufferMemoryBarrier2> m_bufferMemoryBarriers;
    Vec<vk::ImageMemoryBarrier2> m_imageMemoryBarriers;
    Vec<vk::MemoryBarrier2> m_memoryBarriers;
    vk::DependencyFlags m_dependencyFlags = {};
};

}
