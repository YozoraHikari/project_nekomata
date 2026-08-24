module projnekomata;
import std;
import projnekomata.corelib;
import :core.storage.sharded_hash_storage;

namespace projnekomata::storage {

ShardedHashStorage::ShardedHashStorage(const std::filesystem::path& directory, u32 nibblesPerLevel, u32 levelCount)
    : m_directory(directory), m_nibblesPerLevel(nibblesPerLevel), m_levelCount(levelCount) {}

auto ShardedHashStorage::store(Slice<const u8> hash, Slice<const u8> data) -> Result<std::monostate, HashStorageWriteError> {
    auto fullpath = buildPathFromHash(hash);

    std::error_code ec;
    std::filesystem::create_directories(fullpath.parent_path(), ec);

    if (ec) {
        log::crit("Failed to create directories for hash storage: {}", ec.message());
        return Err(HashStorageWriteError::FileOpenError);
    }

    std::ofstream file(fullpath, std::ios::binary | std::ios::trunc);
    if (!file) return Err(HashStorageWriteError::FileOpenError);

    file.write(reinterpret_cast<const char*>(data.data()), data.len());
    if (!file) return Err(HashStorageWriteError::FileWriteError);

    return Ok(std::monostate{});
}

auto ShardedHashStorage::load(Slice<const u8> hash, Vec<u8>& dst) -> Result<std::monostate, HashStorageLoadError> {
    auto fullpath = buildPathFromHash(hash);
    if (!std::filesystem::exists(fullpath))
        return Err(HashStorageLoadError::ObjectMissing);

    auto file = std::ifstream(fullpath, std::ios::binary | std::ios::ate);
    if (!file) return Err(HashStorageLoadError::FileOpenError);
    dst.resize(file.tellg(), 0);
    file.seekg(0);

    file.read(reinterpret_cast<char*>(dst.data()), dst.len());
    if (!file) return Err(HashStorageLoadError::FileReadError);

    return Ok(std::monostate{});
}
auto ShardedHashStorage::invalidate() -> void {
    std::filesystem::remove_all(m_directory);
}

auto ShardedHashStorage::buildPathFromHash(Slice<const u8> hash) const -> std::filesystem::path {
    static constexpr char kHexLut[] = "0123456789abcdef";

    std::string hashHexStr(hash.len() * 2, '\0');
    for (usize i = 0; i < hash.len(); i++) {
        hashHexStr[i * 2    ] = kHexLut[hash[i] >> 4];
        hashHexStr[i * 2 + 1] = kHexLut[hash[i] & 0xf];
    }

    std::filesystem::path fullpath = m_directory;

    for (u32 i = 0; i < m_levelCount; i++) {
        fullpath /= hashHexStr.substr(i * m_nibblesPerLevel, m_nibblesPerLevel);
    }

    return fullpath / hashHexStr;
}

} // namespace projnekomata::storage