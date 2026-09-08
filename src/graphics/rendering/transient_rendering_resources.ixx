export module projnekomata:graphics.rendering.transient_rendering_resources;
import vulkan;
import :graphics.vulkan.vk_image;
import :graphics.srt.shader_resource_table;
import :graphics.vulkan.vk_descriptor_set;
import :graphics.rendering.shared_rendering_resources;

export namespace projnekomata::gfx {

/// Transient rendering resources house all the data used in a single render step.
///
/// # Access Contingency
/// | CPU Read/Write | GPU Access Scope across Queue | GPU Visibility Scope across Queue |
/// |----------------|-------------------------------|-----------------------------------|
/// | No             | Exclusive                     | Shared                            |
///
class TransientRenderingResources {
public:
    TransientRenderingResources(std::nullptr_t);

    TransientRenderingResources(vk::Extent2D renderImageExtent, SharedRenderingResources& sharedResources);

    [[nodiscard]] vkrhi::VulkanImage& depthBuffer() { return m_depthBuffer; }
    [[nodiscard]] vkrhi::VulkanImage& albedoAndRoughnessBuffer() { return m_albedoAndRoughnessBuffer; }
    [[nodiscard]] vkrhi::VulkanImage& emissiveAndBloomBuffer() { return m_emissiveAndBloomBuffer; }
    [[nodiscard]] vkrhi::VulkanImageView& emissiveAndBloomBufferMipView(u32 mipIndex) { return m_emissiveAndBloomBufferMipViews[mipIndex]; }
    [[nodiscard]] vkrhi::VulkanImage& normalBuffer() { return m_normalBuffer; }
    [[nodiscard]] vkrhi::VulkanImage& metallicAndAoBuffer() { return m_metallicAndAoBuffer; }
    [[nodiscard]] vkrhi::VulkanImage& velocityBuffer() { return m_velocityBuffer; }
    [[nodiscard]] vkrhi::VulkanImage& hdrColorBuffer() { return m_hdrColorBuffer; }
    [[nodiscard]] vkrhi::VulkanImage& tonemappedColorBuffer() { return m_tonemappedColorBuffer; }
    [[nodiscard]] vkrhi::VulkanImageView& tonemappedColorBufferUnormView() { return m_tonemappedColorBufferUnormView; }
    [[nodiscard]] vkrhi::VulkanImage& smaaColorResolvedBuffer0() { return m_smaaColorResolvedBuffer0; }
    [[nodiscard]] vkrhi::VulkanImage& smaaColorResolvedBuffer1() { return m_smaaColorResolvedBuffer1; }
    [[nodiscard]] vkrhi::VulkanImageView& smaaColorResolvedBuffer0UnormView() { return m_smaaColorResolvedBuffer0UnormView; }
    [[nodiscard]] vkrhi::VulkanImageView& smaaColorResolvedBuffer1UnormView() { return m_smaaColorResolvedBuffer1UnormView; }
    [[nodiscard]] vkrhi::VulkanImage& finalImage() { return m_finalImage; }
    [[nodiscard]] vkrhi::VulkanImageView& finalImageUnormView() { return m_finalImageUnormView; }

    [[nodiscard]] vkrhi::VulkanImage& smaaEdgesImage() { return m_smaaEdgesImage; }
    [[nodiscard]] vkrhi::VulkanImage& smaaWeightsImage() { return m_smaaWeightsImage; }

    [[nodiscard]] vkrhi::VulkanImage& postSmaaImage() { return m_postSmaaImage; }
    [[nodiscard]] vkrhi::VulkanImageView& postSmaaImageUnormView() { return m_postSmaaImageUnormView; }

    [[nodiscard]] vkrhi::VulkanImage& overdrawCountersImage() { return m_overdrawCountersImage; }

    [[nodiscard]] auto depthBufferIndex()                       const -> SRTResourceIndex { return m_depthBufferIndex;                       }
    [[nodiscard]] auto albedoAndRoughnessBufferIndex()          const -> SRTResourceIndex { return m_albedoAndRoughnessBufferIndex;          }
    [[nodiscard]] auto normalBufferIndex()                      const -> SRTResourceIndex { return m_normalBufferIndex;                      }
    [[nodiscard]] auto metallicAndAoBufferIndex()               const -> SRTResourceIndex { return m_metallicAndAoBufferIndex;               }
    [[nodiscard]] auto velocityBufferIndex()                    const -> SRTResourceIndex { return m_velocityBufferIndex;                    }
    [[nodiscard]] auto hdrColorBufferIndex()                    const -> SRTResourceIndex { return m_hdrColorBufferIndex;                    }
    [[nodiscard]] auto tonemappedColorBufferIndex()             const -> SRTResourceIndex { return m_tonemappedColorBufferIndex;             }
    [[nodiscard]] auto tonemappedColorBufferUnormViewIndex()    const -> SRTResourceIndex { return m_tonemappedColorBufferUnormViewIndex;    }
    [[nodiscard]] auto smaaColorResolvedBuffer0UnormViewIndex() const -> SRTResourceIndex { return m_smaaColorResolvedBuffer0UnormViewIndex; }
    [[nodiscard]] auto smaaColorResolvedBuffer1UnormViewIndex() const -> SRTResourceIndex { return m_smaaColorResolvedBuffer1UnormViewIndex; }
    [[nodiscard]] auto smaaEdgesImageIndex()                    const -> SRTResourceIndex { return m_smaaEdgesImageIndex;                    }
    [[nodiscard]] auto smaaWeightsImageIndex()                  const -> SRTResourceIndex { return m_smaaWeightsImageIndex;                  }
    [[nodiscard]] auto postSmaaImageIndex()                     const -> SRTResourceIndex { return m_postSmaaImageIndex;                     }
    [[nodiscard]] auto postSmaaImageUnormViewIndex()            const -> SRTResourceIndex { return m_postSmaaImageUnormViewIndex;            }
    [[nodiscard]] auto overdrawCountersImageIndex()             const -> SRTResourceIndex { return m_overdrawCountersImageIndex;             }

    [[nodiscard]] auto emissiveAndBloomBufferSampledImageIndexForMip(u32 mipIndex) const -> SRTResourceIndex { return m_emissiveAndBloomBufferSampledImageMipIndices[mipIndex]; }
    [[nodiscard]] auto emissiveAndBloomBufferStorageImageIndexForMip(u32 mipIndex) const -> SRTResourceIndex { return m_emissiveAndBloomBufferStorageImageMipIndices[mipIndex]; }


    auto handleWindowSizeChange(vk::Extent2D newWindowSize) -> void;

private:
    // --------------------------------------------------------------------------------------------------------------------------------------------------------
    // Render Targets
    vkrhi::VulkanImage m_depthBuffer = nullptr;
    vkrhi::VulkanImage m_albedoAndRoughnessBuffer = nullptr;
    vkrhi::VulkanImage m_normalBuffer = nullptr;
    vkrhi::VulkanImage m_metallicAndAoBuffer = nullptr;
    vkrhi::VulkanImage m_velocityBuffer = nullptr;
    vkrhi::VulkanImage m_tonemappedColorBuffer = nullptr;
    vkrhi::VulkanImageView m_tonemappedColorBufferUnormView = nullptr;

    vkrhi::VulkanImage m_emissiveAndBloomBuffer = nullptr;
    Vec<vkrhi::VulkanImageView> m_emissiveAndBloomBufferMipViews = Vec<vkrhi::VulkanImageView>::create();
    Vec<SRTResourceIndex> m_emissiveAndBloomBufferSampledImageMipIndices = Vec<SRTResourceIndex>::create();
    Vec<SRTResourceIndex> m_emissiveAndBloomBufferStorageImageMipIndices = Vec<SRTResourceIndex>::create();

    vkrhi::VulkanImage m_hdrColorBuffer = nullptr;

    vkrhi::VulkanImage m_smaaColorResolvedBuffer0 = nullptr;
    vkrhi::VulkanImage m_smaaColorResolvedBuffer1 = nullptr;
    vkrhi::VulkanImageView m_smaaColorResolvedBuffer0UnormView = nullptr;
    vkrhi::VulkanImageView m_smaaColorResolvedBuffer1UnormView = nullptr;

    vkrhi::VulkanImage m_smaaEdgesImage = nullptr;
    vkrhi::VulkanImage m_smaaWeightsImage = nullptr;

    vkrhi::VulkanImage m_finalImage = nullptr;
    vkrhi::VulkanImageView m_finalImageUnormView = nullptr;

    SRTResourceIndex m_depthBufferIndex              = {};
    SRTResourceIndex m_albedoAndRoughnessBufferIndex = {};
    SRTResourceIndex m_normalBufferIndex             = {};
    SRTResourceIndex m_metallicAndAoBufferIndex      = {};
    SRTResourceIndex m_velocityBufferIndex           = {};
    SRTResourceIndex m_hdrColorBufferIndex           = {};
    SRTResourceIndex m_tonemappedColorBufferIndex              = {};
    SRTResourceIndex m_tonemappedColorBufferUnormViewIndex     = {};
    SRTResourceIndex m_smaaColorResolvedBuffer0UnormViewIndex = {};
    SRTResourceIndex m_smaaColorResolvedBuffer1UnormViewIndex = {};
    SRTResourceIndex m_smaaEdgesImageIndex           = {};
    SRTResourceIndex m_smaaWeightsImageIndex         = {};
    SRTResourceIndex m_postSmaaImageIndex          = {};
    SRTResourceIndex m_postSmaaImageUnormViewIndex = {};
    SRTResourceIndex m_overdrawCountersImageIndex = {};

    vkrhi::VulkanImage m_postSmaaImage = nullptr;
    vkrhi::VulkanImageView m_postSmaaImageUnormView = nullptr;

    vkrhi::VulkanImage m_overdrawCountersImage = nullptr;

    auto setupRenderingAttachments(vk::Extent2D renderImageExtent) -> void;
    auto zeroinitColorBuffers() -> void;

};

}
