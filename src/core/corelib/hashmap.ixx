module;
#include <cstdlib>
#include <xxhash.h>
#include <string.h>
export module projnekomata.corelib:hashmap;
import :assertions;
import :simd;
import :mem;
import :iterators;
import :option;
import :primitives;

// -------------------------------------------------------------------------------------------------------------------------------------------------------------

export template <typename K, typename V, typename H> class HashMap;
export template <typename K, typename V, typename H> class HashMapKeysIter;
export template <typename K, typename V, typename H> class HashMapValuesIter;
export template <typename K, typename V, typename H> class HashMapIter;

template <typename T> constexpr auto avalancheHash(T v) -> u64 {
    u64 x = v;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9_u64;
    x = (x ^ (x >> 27)) * 0x94d049bb133111eb_u64;
    return x ^ (x >> 31);
}

template <typename K> struct Hash;

template<> struct Hash<u8>          { static u64 hash(u8 v) { return avalancheHash(v); } };
template<> struct Hash<i8>          { static u64 hash(i8 v) { return avalancheHash(v); } };
template<> struct Hash<u16>         { static u64 hash(u16 v) { return avalancheHash(v); } };
template<> struct Hash<i16>         { static u64 hash(i16 v) { return avalancheHash(v); } };
template<> struct Hash<u32>         { static u64 hash(u32 v) { return avalancheHash(v); } };
template<> struct Hash<i32>         { static u64 hash(i32 v) { return avalancheHash(v); } };
template<> struct Hash<u64>         { static u64 hash(u64 v) { return avalancheHash(v); } };
template<> struct Hash<i64>         { static u64 hash(i64 v) { return avalancheHash(v); } };
template<> struct Hash<f32>         { static u64 hash(f32 v) { return avalancheHash(v); } };
template<> struct Hash<f64>         { static u64 hash(f64 v) { return avalancheHash(v); } };
template<> struct Hash<bool>        { static u64 hash(bool v) { return avalancheHash(v); } };
template<> struct Hash<std::string> { static u64 hash(std::string_view v) { return XXH3_64bits(v.data(), v.size()); } };
template<> struct Hash<std::type_index> { static u64 hash(std::type_index v) { return avalancheHash(v.hash_code()); } };

export template <typename K, typename V, typename H = Hash<K>> class HashMap {
public:
    ~HashMap() {
        freeAll();
    }

    constexpr HashMap(const HashMap&) = delete;
    constexpr HashMap& operator=(const HashMap&) = delete;
    constexpr HashMap(HashMap&& other) noexcept : m_ctrls(other.m_ctrls), m_entries(other.m_entries), m_dibs(other.m_dibs), m_len(other.m_len), m_capacity(other.m_capacity) {
        other.m_ctrls = nullptr;
        other.m_entries = nullptr;
        other.m_dibs = nullptr;
        other.m_len = 0;
        other.m_capacity = 0;
    }
    constexpr HashMap& operator=(HashMap&& other) noexcept {
        if (this == &other) return *this;
        freeAll();
        m_ctrls = other.m_ctrls;
        m_entries = other.m_entries;
        m_dibs = other.m_dibs;
        m_len = other.m_len;
        m_capacity = other.m_capacity;
        other.m_ctrls = nullptr;
        other.m_entries = nullptr;
        other.m_dibs = nullptr;
        other.m_len = 0;
        other.m_capacity = 0;
        return *this;
    }

    static auto create() -> HashMap {
        auto hashmap = HashMap();
        hashmap.alloc(kInitialCapacity);
        return hashmap;
    }

    static auto create(std::initializer_list<std::pair<K, V>> init) -> HashMap {
        auto hashmap = HashMap();
        hashmap.alloc(init.size());
        for (auto& [key, value] : init) {
            hashmap.insert(key, value);
        }
        return hashmap;
    }

    static auto withCapacity(usize cap) -> HashMap {
        auto hashmap = HashMap();
        hashmap.alloc(cap);
        return hashmap;
    }

    // ---------------------------------------------------------------------------------------------------------------------------------------------------------

    constexpr auto insert(const K& key, const V& value) -> V& {
        return insertInner(K(key), V(value));
    }

    constexpr auto insert(const K& key, V&& value) -> V& {
        return insertInner(K(key), std::forward<V>(value));
    }

    constexpr auto insert(K&& key, V&& value) -> V& {
        return insertInner(std::forward<K>(key), std::forward<V>(value));
    }


    // ---------------------------------------------------------------------------------------------------------------------------------------------------------

    constexpr auto get(const K& key) -> Option<std::reference_wrapper<V>> {
        u64 hash = H::hash(key);
        u8 h2 = computeH2(hash);
        usize index = hashHomeIndex(hash);

        u8x16 needle = u8x16::broadcast(h2);
        u8x16 empty = u8x16::broadcast(kCtrlSentinelEmpty);

        for (usize step = 0; step < m_capacity; step += 16) {
            usize base = step + index;

            u8x16 group = u8x16::loadUnaligned(m_ctrls + base);
            u32 matches = group.cmplaneEq(needle).movemask();

            while (matches) {
                u32 bit = __builtin_ctz(matches);
                usize slot = (base + bit) & (m_capacity - 1);
                if (m_entries[slot].key == key) return Some(std::ref(m_entries[slot].value));
                matches &= (matches - 1);
            }

            if (group.cmplaneEq(empty).any()) return None;
        }
        return None;
    }

    constexpr auto get(const K& key) const -> Option<std::reference_wrapper<const V>> {
        u64 hash = H::hash(key);
        u8 h2 = computeH2(hash);
        usize index = hashHomeIndex(hash);

        u8x16 needle = u8x16::broadcast(h2);
        u8x16 empty = u8x16::broadcast(kCtrlSentinelEmpty);

        for (usize step = 0; step < m_capacity; step += 16) {
            usize base = step + index;

            u8x16 group = u8x16::loadUnaligned(m_ctrls + base);
            u32 matches = group.cmplaneEq(needle).movemask();

            while (matches) {
                u32 bit = __builtin_ctz(matches);
                usize slot = (base + bit) & (m_capacity - 1);
                if (m_entries[slot].key == key) return Some(std::cref(m_entries[slot].value));
                matches &= (matches - 1);
            }

            if (group.cmplaneEq(empty).any()) return None;
        }
        return None;
    }

    constexpr auto operator[](const K& key) -> V& {
        auto ref = get(key);
        debug_assert(ref.isSome(), "HashMap: key not found");
        return ref.unwrap();
    }

    constexpr auto operator[](const K& key) const -> const V& {
        auto ref = get(key);
        debug_assert(ref.isSome(), "HashMap: key not found");
        return ref.unwrap();
    }

    constexpr auto contains(const K& key) -> bool {
        return get(key).isSome();
    }


    // ---------------------------------------------------------------------------------------------------------------------------------------------------------

    constexpr auto remove(const K& key) -> Option<V> {
        u64 hash = H::hash(key);
        u8 h2 = computeH2(hash);
        usize index = hashHomeIndex(hash);

        u8x16 needle = u8x16::broadcast(h2);
        u8x16 empty = u8x16::broadcast(kCtrlSentinelEmpty);

        for (usize step = 0; step < m_capacity; step += 16) {
            usize base = step + index;

            u8x16 group = u8x16::loadUnaligned(m_ctrls + base);
            u32 matches = group.cmplaneEq(needle).movemask();

            while (matches) {
                u32 bit = __builtin_ctz(matches);
                usize slot = (base + static_cast<usize>(bit)) & (m_capacity - 1);
                if (m_entries[slot].key == key) return removeAt(slot);

                matches &= (matches - 1);
            }

            if (group.cmplaneEq(empty).any()) return None;
        }
        return None;
    }

    // ---------------------------------------------------------------------------------------------------------------------------------------------------------

    constexpr auto keys() -> HashMapKeysIter<K, V, H> {
        return HashMapKeysIter<K, V, H>(this);
    }

    constexpr auto values() -> HashMapValuesIter<K, V, H> {
        return HashMapValuesIter<K, V, H>(this);
    }

    constexpr auto iter() -> HashMapIter<K, V, H> {
        return HashMapIter<K, V, H>(this);
    }

    // ---------------------------------------------------------------------------------------------------------------------------------------------------------

    constexpr auto len() const -> usize { return m_len; }
    constexpr auto capacity() const -> usize { return m_capacity; }
    constexpr auto isEmpty() const -> bool { return m_len == 0; }

    constexpr auto clear() -> void {
        destroyEntries();
        memset(m_ctrls, kCtrlSentinelEmpty, m_capacity + 16);
        memset(m_dibs, 0, m_capacity * sizeof(u32));
        m_len = 0;
    }

private:
    HashMap() = default;
    friend class HashMapKeysIter<K, V, H>;
    friend class HashMapValuesIter<K, V, H>;
    friend class HashMapIter<K, V, H>;

    static constexpr auto kKeyNeedsFinalizer = !std::is_trivially_destructible_v<K>;
    static constexpr auto kValNeedsFinalizer = !std::is_trivially_destructible_v<V>;
    static constexpr auto kMustFinalize = kKeyNeedsFinalizer || kValNeedsFinalizer;

    static constexpr auto kInitialCapacity = 32_usize;
    static constexpr auto kLoadFactor      = 0.75_f32;

    struct Entry {
        K key;
        V value;
    };

    u8*    m_ctrls    = nullptr;
    Entry* m_entries  = nullptr;
    u32*   m_dibs     = nullptr;
    u32    m_len      = 0;
    u32    m_capacity = 0;

    static constexpr auto kCtrlSentinelEmpty   = 0x80_u8;

    static constexpr auto computeH2(u64 hash) -> u8 { return static_cast<u8>(hash & 0x7f); }

    constexpr auto hashHomeIndex(u64 hash) const -> usize { return static_cast<usize>(hash >> 7) & (m_capacity - 1); }
    constexpr auto computeNextIndex(usize index) const -> usize { return (index + 1) & (m_capacity - 1); }

    // ---------------------------------------------------------------------------------------------------------------------------------------------------------

    constexpr auto alloc(usize cap) -> void {
        m_capacity = cap;
        m_ctrls = Mem::allocAlignedChecked<u8>(cap + 16, 16);
        m_entries = Mem::allocChecked<Entry>(cap);
        m_dibs = Mem::allocChecked<u32>(cap);

        memset(m_ctrls, kCtrlSentinelEmpty, cap + 16);
        memset(m_dibs, 0, cap * sizeof(u32));
    }

    constexpr auto freeAll() -> void {
        if (!m_ctrls) return;
        destroyEntries();
        Mem::freeAligned(m_ctrls);
        Mem::free(m_entries);
        Mem::free(m_dibs);
        m_ctrls = nullptr;
        m_entries = nullptr;
        m_dibs = nullptr;
    }

    // ---------------------------------------------------------------------------------------------------------------------------------------------------------

    constexpr auto removeAt(usize index) -> Option<V> {
        auto result = Some(std::move(m_entries[index].value));

        if constexpr (kKeyNeedsFinalizer) m_entries[index].key.~K();
        if constexpr (kValNeedsFinalizer) m_entries[index].value.~V();
        m_ctrls[index] = kCtrlSentinelEmpty;
        mirrorPaddingEntries(index);
        m_dibs[index] = 0;

        m_len--;

        while (true) {
            auto nextIndex = computeNextIndex(index);
            if (m_ctrls[nextIndex] == kCtrlSentinelEmpty || m_dibs[nextIndex] == 0) break;

            new (&m_entries[index].key) K(std::move(m_entries[nextIndex].key));
            new (&m_entries[index].value) V(std::move(m_entries[nextIndex].value));
            m_ctrls[index] = m_ctrls[nextIndex];
            mirrorPaddingEntries(index);
            m_dibs[index] = m_dibs[nextIndex] - 1;

            if constexpr (kKeyNeedsFinalizer) m_entries[nextIndex].key.~K();
            if constexpr (kValNeedsFinalizer) m_entries[nextIndex].value.~V();
            m_ctrls[nextIndex] = kCtrlSentinelEmpty;
            mirrorPaddingEntries(nextIndex);
            m_dibs[nextIndex] = 0;
            index = nextIndex;
        }

        return result;
    }


    constexpr auto insertInner(K&& key, V&& value) -> V& {
        maybeRehash();

        u64 hash = H::hash(key);
        u8 h2 = computeH2(hash);
        usize index = hashHomeIndex(hash);

        auto wkey = std::move(key);
        auto wvalue = std::move(value);
        auto wh2 = h2;
        auto wdib = 0_u32;

        V* resultptr = nullptr;

        while (true) {
            u8 c = m_ctrls[index];

            // Path 1 - Find an empty slot
            if (c == kCtrlSentinelEmpty) {
                new (&m_entries[index].key) K(std::move(wkey));
                new (&m_entries[index].value) V(std::move(wvalue));
                m_ctrls[index] = wh2;
                mirrorPaddingEntries(index);
                m_dibs[index] = wdib;
                m_len++;
                return resultptr ? *resultptr : m_entries[index].value;
            }

            // Path 2 - Same h2 and a possible key match
            if (c == wh2 && m_entries[index].key == wkey) {
                if constexpr (kValNeedsFinalizer) m_entries[index].value.~V();
                new (&m_entries[index].value) V(std::move(wvalue));
                return resultptr ? *resultptr : m_entries[index].value;
            }

            // Path 3 - Robin Hood steal
            if (wdib > m_dibs[index]) {
                auto tkey = std::move(m_entries[index].key);
                auto tvalue = std::move(m_entries[index].value);
                auto th2 = m_ctrls[index];
                auto tdib = m_dibs[index];

                if constexpr (kKeyNeedsFinalizer) m_entries[index].key.~K();
                if constexpr (kValNeedsFinalizer) m_entries[index].value.~V();

                new (&m_entries[index].key) K(std::move(wkey));
                new (&m_entries[index].value) V(std::move(wvalue));
                m_ctrls[index] = wh2;
                mirrorPaddingEntries(index);
                m_dibs[index] = wdib;

                if (!resultptr) resultptr = &m_entries[index].value;

                wkey = std::move(tkey);
                wvalue = std::move(tvalue);
                wh2 = th2;
                wdib = tdib;
            }

            wdib++;
            index = computeNextIndex(index);
        }
    }

    // ---------------------------------------------------------------------------------------------------------------------------------------------------------

    constexpr auto maybeRehash() -> void {
        if (m_len + 1 > static_cast<usize>(m_capacity * kLoadFactor)) rehash(m_capacity * 2);
    }

    constexpr auto rehash(usize newCap) -> void {
        auto oldCtrls = m_ctrls;
        auto oldEntries = m_entries;
        auto oldDibs = m_dibs;
        auto oldCap = m_capacity;

        alloc(newCap);
        m_len = 0;

        for (usize i = 0; i < oldCap; i++) {
            if (oldCtrls[i] != kCtrlSentinelEmpty) {
                insert(std::move(oldEntries[i].key), std::move(oldEntries[i].value));
            }
        }

        Mem::freeAligned(oldCtrls);
        Mem::free(oldEntries);
        Mem::free(oldDibs);
    }

    // ---------------------------------------------------------------------------------------------------------------------------------------------------------

    constexpr auto mirrorPaddingEntries(usize index) -> void {
        if (index < 16)
            m_ctrls[m_capacity + index] = m_ctrls[index];
    }

    constexpr auto destroyEntries() -> void {
        if constexpr (kMustFinalize) {
            for (usize i = 0; i < m_capacity; i++) {
                if (m_ctrls[i] != kCtrlSentinelEmpty) {
                    if constexpr (kKeyNeedsFinalizer) m_entries[i].key.~K();
                    if constexpr (kValNeedsFinalizer) m_entries[i].value.~V();
                }
            }
        }
    }
};


export template <typename K, typename V, typename H> class HashMapKeysIter : public IteratorBase<HashMapKeysIter<K, V, H>> {
public:
    using Item = IteratorInternalNonNullPtr<const K>;
    constexpr HashMapKeysIter(HashMap<K, V, H>* hashmap) : m_hashmap(hashmap) { skipEmpty(); }

    constexpr auto next() -> Option<Item> {
        if (m_index >= m_hashmap->m_capacity) return None;
        auto i = m_index;
        m_index++;
        skipEmpty();
        return Some(IteratorInternalNonNullPtr(NonNullPtr<const K>(&m_hashmap->m_entries[i].key)));
    }

private:
    HashMap<K, V, H>* m_hashmap = nullptr;
    usize m_index = 0;

    constexpr auto skipEmpty() -> void {
        while (m_index < m_hashmap->m_capacity && m_hashmap->m_ctrls[m_index] == HashMap<K, V, H>::kCtrlSentinelEmpty) m_index++;
    }
};

export template <typename K, typename V, typename H> class HashMapValuesIter : public IteratorBase<HashMapValuesIter<K, V, H>> {
public:
    using Item = IteratorInternalNonNullPtr<V>;
    constexpr HashMapValuesIter(HashMap<K, V, H>* hashmap) : m_hashmap(hashmap) { skipEmpty(); }

    constexpr auto next() -> Option<Item> {
        if (m_index >= m_hashmap->m_capacity) return None;
        auto i = m_index;
        m_index++;
        skipEmpty();
        return Some(IteratorInternalNonNullPtr(NonNullPtr<V>(&m_hashmap->m_entries[i].value)));
    }

private:
    HashMap<K, V, H>* m_hashmap = nullptr;
    usize m_index = 0;

    constexpr auto skipEmpty() -> void {
        while (m_index < m_hashmap->m_capacity && m_hashmap->m_ctrls[m_index] == HashMap<K, V, H>::kCtrlSentinelEmpty) m_index++;
    }
};

export template <typename K, typename V, typename H> class HashMapIter : public IteratorBase<HashMapIter<K, V, H>> {
public:
    using Item = KeyValue<IteratorInternalNonNullPtr<const K>, IteratorInternalNonNullPtr<V>>;
    constexpr HashMapIter(HashMap<K, V, H>* hashmap) : m_hashmap(hashmap) { skipEmpty(); }

    constexpr auto next() -> Option<Item> {
        if (m_index >= m_hashmap->m_capacity) return None;
        auto i = m_index;
        m_index++;
        skipEmpty();
        return Some(Item(IteratorInternalNonNullPtr(NonNullPtr<const K>(&m_hashmap->m_entries[i].key)), IteratorInternalNonNullPtr(NonNullPtr<V>(&m_hashmap->m_entries[i].value))));
    }

private:
    HashMap<K, V, H>* m_hashmap = nullptr;
    usize m_index = 0;

    constexpr auto skipEmpty() -> void {
        while (m_index < m_hashmap->m_capacity && m_hashmap->m_ctrls[m_index] == HashMap<K, V, H>::kCtrlSentinelEmpty) m_index++;
    }
};