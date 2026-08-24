module;
#include <string.h>
export module projnekomata:graphics.vulkan.shadercache.pipeline_binary_frontend;
import std;
import vulkan;
import projnekomata.corelib;
import :core.storage.sharded_hash_storage;
import :graphics.vulkan.context;
import :graphics.vulkan.shadercache.pipeline_sc_concept;

export namespace projnekomata::gfx::vkrhi {

constexpr u32 uint32Le(u32 x) {
    if constexpr (std::endian::native == std::endian::little) {
        return x;
    }
    return __builtin_bswap32(x);
}

class PipelineKeysToBinaryKeyObjectsSerde {
public:
    static auto serialize(Slice<const vk::PipelineBinaryKeyKHR> pipelineKeys, Vec<u8>& buffer) -> void {
        u32 keyCount = uint32Le(pipelineKeys.size());
        auto keyCountBytes = Slice<const u8>(reinterpret_cast<const u8*>(&keyCount), sizeof(keyCount));
        buffer.extend(keyCountBytes);


        for (const auto& pipelineKey : pipelineKeys) {
            u32 keySize = uint32Le(pipelineKey.keySize);
            auto keySizeBytes = Slice<const u8>(reinterpret_cast<const u8*>(&keySize), sizeof(keySize));
            auto keyBytes = Slice<const u8>(pipelineKey.key.data(), pipelineKey.keySize);

            buffer.extend(keySizeBytes);
            buffer.extend(keyBytes);
        }
    }

    static auto deserialize(Slice<const u8> buffer, Vec<vk::PipelineBinaryKeyKHR>& pipelineKeys) -> bool {
        if (buffer.len() < 4) {
            log::error("[Deserialization Error] Pipeline binary key objects buffer too small to read keyCount! (len = {})", buffer.len());
            return false;
        }
        u32 keyCount;
        memcpy(&keyCount, buffer.data(), sizeof(keyCount));
        keyCount = uint32Le(keyCount);

        usize cursor = 4;

        for (u32 i = 0; i < keyCount; i++) {
            if (buffer.size() < cursor + 4) {
                log::error("[Deserialization Error] Pipeline binary key objects buffer too small to read keySize! (i = {}, len = {})", i, buffer.len());
                return false;
            }
            u32 keySize;
            memcpy(&keySize, buffer.data() + cursor, sizeof(keySize));
            keySize = uint32Le(keySize);
            cursor += 4;
            if (buffer.size() < cursor + keySize) {
                log::error("[Deserialization Error] Pipeline binary key objects buffer too small to read key! (i = {}, len = {})", i, buffer.len());
                return false;
            }

            auto pipelineKey = vk::PipelineBinaryKeyKHR{};
            pipelineKey.keySize = keySize;
            memcpy(pipelineKey.key.data(), buffer.data() + cursor, keySize);
            cursor += keySize;
            pipelineKeys.emplace(pipelineKey);
        }

        return true;
    }
};

class ShaderCachePipelineBinaryFrontend {
public:
    ShaderCachePipelineBinaryFrontend(const std::filesystem::path& pipelineCacheDirectory, u32 storesNibblesPerLevel, u32 storesLevelCount)
        : m_pipelineCacheDirectory(pipelineCacheDirectory),
            m_pipelineKeysToBinaryKeyObjects(pipelineCacheDirectory / "pk", storesNibblesPerLevel, storesLevelCount),
            m_binaryKeysToBinaryObjects(pipelineCacheDirectory / "bin", storesNibblesPerLevel, storesLevelCount) {}

    void checkGlobalKeyAndInvalidateStale() {
        auto globalKey = vkCheckResult(VulkanContext::get().vkDevice().getPipelineKeyKHR());
        Slice<const u8> gpkKey = { globalKey.key.data(), globalKey.keySize };
        Vec<u8> byteBuffer;
        if (!getGlobalKeyFromStorage(byteBuffer)) {
            log::warn("Failed to load global pipeline key, will try to invalidate pipeline cache");
            m_pipelineKeysToBinaryKeyObjects.invalidate();
            m_binaryKeysToBinaryObjects.invalidate();
        } else {
            bool globalKeyMatches = gpkKey == byteBuffer.asSlice();
            if (!globalKeyMatches) {
                log::warn("Global pipeline key mismatch, invalidating pipeline cache.");
                m_pipelineKeysToBinaryKeyObjects.invalidate();
                m_binaryKeysToBinaryObjects.invalidate();
            }
        }
        if (!storeGlobalKeyToStorage(gpkKey)) {
            log::error("Failed to store global pipeline key!");
        }
    }

    template <typename... ScElements>
        requires PipelineBinaryCacheableGraphicsPipelineCreateStructChain<ScElements...>
    auto handleCreateGraphicsPipeline(vk::StructureChain<ScElements...>& chain) -> vk::raii::Pipeline {
        auto pipelineCreateInfo = vk::PipelineCreateInfoKHR{}
            .setPNext(&chain.template get<vk::GraphicsPipelineCreateInfo>());


        auto pipelineKey = vkCheckResult(VulkanContext::get().vkDevice().getPipelineKeyKHR(pipelineCreateInfo));
        Vec<vk::PipelineBinaryKeyKHR> binaryKeys;
        if (!getBinaryKeysForPipelineKey(pipelineKey, binaryKeys)) {
            return handleCreateGraphicsPipelineUncached(chain, pipelineKey);
        }
        Vec<vk::raii::PipelineBinaryKHR> binaries;
        if (!getBinariesForBinaryKeys(binaryKeys.asSlice(), binaries)) {
            return handleCreateGraphicsPipelineUncached(chain, pipelineKey);
        }

        auto binaryHandles = binaries.iter()
            .map([&](auto&& binary) { return *binary; })
            .template collect<Vec>();

        chain.template get<vk::PipelineBinaryInfoKHR>()
            .setPipelineBinaries(binaryHandles);

        return vkCheckResult(VulkanContext::get().vkDevice().createGraphicsPipeline(nullptr, chain.template get<vk::GraphicsPipelineCreateInfo>()));
    }

private:
    std::filesystem::path m_pipelineCacheDirectory;
    storage::ShardedHashStorage m_pipelineKeysToBinaryKeyObjects;
    storage::ShardedHashStorage m_binaryKeysToBinaryObjects;

    // ---------------------------------------------------------------------------------------------------------------------------------------------------------
    // Storage Helpers

    auto getBinaryKeysForPipelineKey(vk::PipelineBinaryKeyKHR pipelineKey, Vec<vk::PipelineBinaryKeyKHR>& buffer) -> bool {
        auto key = Slice<const u8>(pipelineKey.key.data(), pipelineKey.keySize);
        Vec<u8> byteBuffer;

        auto result = m_pipelineKeysToBinaryKeyObjects.load(key, byteBuffer);
        if (result.isErr()) {
            auto err = result.unwrapErr();
            switch (err) {
            case storage::HashStorageLoadError::FileOpenError: log::error("Failed to load pipeline key to binary key object: file open error"); break;
            case storage::HashStorageLoadError::FileReadError: log::error("Failed to load pipeline key to binary key object: file read error"); break;
            case storage::HashStorageLoadError::ObjectMissing: log::warn("Missed pipeline binary key cache"); break;
            }
            return false;
        }

        if (!PipelineKeysToBinaryKeyObjectsSerde::deserialize(byteBuffer.asSlice(), buffer)) {
            log::error("Failed to deserialize pipeline binary key objects");
            return false;
        }
        return true;
    }

    auto getBinariesForBinaryKeys(Slice<const vk::PipelineBinaryKeyKHR> binaryKeys, Vec<vk::raii::PipelineBinaryKHR>& buffer) -> bool {
        Vec<Vec<u8>> byteBuffers;
        for (const auto& binaryKey : binaryKeys) {
            auto key = Slice<const u8>(binaryKey.key.data(), binaryKey.keySize);
            Vec<u8> byte_buffer;
            auto result = m_binaryKeysToBinaryObjects.load(key, byte_buffer);
            if (result.isErr()) {
                auto err = result.unwrapErr();
                switch (err) {
                case storage::HashStorageLoadError::FileOpenError: log::error("Failed to load binary key to binary object: file open error"); break;
                case storage::HashStorageLoadError::FileReadError: log::error("Failed to load binary key to binary object: file read error"); break;
                case storage::HashStorageLoadError::ObjectMissing: log::warn("Missed binary cache"); break;
                }
                return false;
            }
            byteBuffers.emplace(std::move(byte_buffer));
        }

        auto binaryData = byteBuffers.iterMut()
            .map([&](auto&& byteBuffer) { return vk::PipelineBinaryDataKHR{}.setData<u8>(byteBuffer); })
            .collect<Vec>();

        auto binaryAndDataInfo = vk::PipelineBinaryKeysAndDataKHR{}
            .setPipelineBinaryKeys(binaryKeys)
            .setPipelineBinaryData(binaryData);

        auto binaryCreateInfo = vk::PipelineBinaryCreateInfoKHR{}
            .setPKeysAndDataInfo(&binaryAndDataInfo);

        buffer = Vec<vk::raii::PipelineBinaryKHR>::fromStdVector(vkCheckResult(VulkanContext::get().vkDevice().createPipelineBinariesKHR(binaryCreateInfo)));

        return true;
    }

    // ---------------------------------------------------------------------------------------------------------------------------------------------------------
    // GPK Key Utils

    auto getGlobalKeyFromStorage(Vec<u8>& buffer) -> bool {
        std::filesystem::path keyPath = m_pipelineCacheDirectory / "gpk";

        if (!std::filesystem::exists(keyPath)) {
            return false;
        }
        std::ifstream keyFile(keyPath, std::ios::binary | std::ios::ate);
        if (!keyFile) {
            log::error("Failed to open global pipeline key file!");
            return false;
        }
        buffer.resize(keyFile.tellg(), 0);
        keyFile.seekg(0);
        keyFile.read(reinterpret_cast<std::istream::char_type*>(buffer.data()), buffer.size());
        if (!keyFile) {
            log::error("Failed to read global pipeline key!");
            return false;
        }
        return true;
    }

    auto storeGlobalKeyToStorage(Slice<const u8> data) -> bool {
        std::filesystem::create_directories(m_pipelineCacheDirectory);
        std::filesystem::path keyPath = m_pipelineCacheDirectory / "gpk";
        std::ofstream keyFile(keyPath, std::ios::binary);
        if (!keyFile) {
            log::error("Failed to open global pipeline key file!");
            return false;
        }
        keyFile.write(reinterpret_cast<const std::ostream::char_type*>(data.data()), data.len());
        if (!keyFile) {
            log::error("Failed to write global pipeline key!");
            return false;
        }
        return true;
    }

    // ---------------------------------------------------------------------------------------------------------------------------------------------------------
    // Cache Miss Paths

    template <typename... ScElements>
        requires PipelineBinaryCacheableGraphicsPipelineCreateStructChain<ScElements...>
    auto handleCreateGraphicsPipelineUncached(vk::StructureChain<ScElements...>& chain, vk::PipelineBinaryKeyKHR pipelineKey) -> vk::raii::Pipeline {
        chain.template get<vk::PipelineCreateFlags2CreateInfo>().flags |= vk::PipelineCreateFlagBits2::eCaptureDataKHR;

        auto pipeline = vkCheckResult(VulkanContext::get().vkDevice().createGraphicsPipeline(nullptr, chain.template get<vk::GraphicsPipelineCreateInfo>()));

        // -----------------------------------------------------------------------------------------------------------------------------------------------------
        // Binary Extraction

        auto binaryCreateInfo = vk::PipelineBinaryCreateInfoKHR{}
            .setPipeline(pipeline);
        auto binaries = vkCheckResult(VulkanContext::get().vkDevice().createPipelineBinariesKHR(binaryCreateInfo));

        bool silentlyFailPipelineKeyWrite = false;
        Vec<vk::PipelineBinaryKeyKHR> binaryKeys;
        for (const auto& binary : binaries) {
            auto binaryDataInfo = vk::PipelineBinaryDataInfoKHR{}
                .setPipelineBinary(binary);

            auto [binaryDataKey, stbinaryData] = vkCheckResult(VulkanContext::get().vkDevice().getPipelineBinaryDataKHR(binaryDataInfo));
            binaryKeys.emplace(binaryDataKey);
            auto binaryData = Vec<u8>::fromStdVector(std::move(stbinaryData));

            auto binaryDataKeySpan = Slice<const u8>(binaryDataKey.key.data(), binaryDataKey.keySize);
            auto storeResult = m_binaryKeysToBinaryObjects.store(binaryDataKeySpan, binaryData.asSlice());

            if (storeResult.isErr()) {
                log::error("Failed to store pipeline binary object!");
                silentlyFailPipelineKeyWrite = true;
            }
        }

        auto release_info = vk::ReleaseCapturedPipelineDataInfoKHR{}
            .setPipeline(pipeline);

        if (silentlyFailPipelineKeyWrite) {
            log::warn("One of the pipeline binary objects failed to store, will not store the pipeline binary keys list.");
            VulkanContext::get().vkDevice().releaseCapturedPipelineDataKHR(release_info);
            return pipeline;
        }

        Vec<u8> byteBuffer;
        PipelineKeysToBinaryKeyObjectsSerde::serialize(binaryKeys.asSlice(), byteBuffer);
        auto pipelineKeySpan = Slice<const u8>(pipelineKey.key.data(), pipelineKey.keySize);
        auto storeResult = m_pipelineKeysToBinaryKeyObjects.store(pipelineKeySpan, byteBuffer.asSlice());

        if (storeResult.isErr()) {
            log::error("Failed to store pipeline binary keys object!");
        }
        VulkanContext::get().vkDevice().releaseCapturedPipelineDataKHR(release_info);
        return pipeline;
    }
};

}