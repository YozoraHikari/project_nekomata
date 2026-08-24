module;
#include <cstdlib>
#include <malloc.h>
export module projnekomata.cs:vec;
import std;
import :iterators;
import :mem;
import :nonnull_ptr;
import :primitives;
import :slice;
import :type_traits;

// -------------------------------------------------------------------------------------------------------------------------------------------------------------

export template <typename T> class Vec {
public:
    static constexpr bool kUsesTriviallyRelocatableFastpath = TTriviallyRelocatableValue<T>;
    static constexpr bool kNeedsFinalizer = !std::is_trivially_destructible_v<T>;

    // TODO: remove later
    Vec() = default;
    constexpr ~Vec() { destroyFinalize(); free(m_data); }

    constexpr Vec(Vec&& other) noexcept : m_data(other.m_data), m_len(other.m_len), m_capacity(other.m_capacity) {
        other.m_data = nullptr;
        other.m_len = 0;
        other.m_capacity = 0;
    }
    constexpr Vec& operator=(Vec&& other) noexcept {
        if (this == &other) return *this;
        destroyFinalize();
        Mem::free(m_data);

        m_data = other.m_data;
        m_len = other.m_len;
        m_capacity = other.m_capacity;
        other.m_data = nullptr;
        other.m_len = 0;
        other.m_capacity = 0;
        return *this;
    }

    constexpr auto clone() noexcept -> Vec {
        auto other = Vec::withCapacity(m_len);
        other.extend(asSlice());
        return other;
    }

    constexpr static auto create() noexcept -> Vec { return Vec(nullptr, 0, 0); }
    constexpr static auto create(std::initializer_list<T> list) -> Vec {
        auto vec = Vec::withCapacity(list.size());
        for (auto& elem : list) vec.emplaceUnchecked(elem);
        return vec;
    }
    constexpr static auto filledWith(usize len, const T& val) -> Vec {
        auto vec = Vec::create();
        vec.setAlloc(len);
        for (usize i = 0; i < len; i++) vec.emplaceUnchecked(val);
        return vec;
    }
    template <typename V>
    constexpr static auto filledWith(usize len, V&& val) -> Vec {
        auto vec = Vec::create();
        vec.setAlloc(len);
        for (usize i = 0; i < len; i++) vec.emplaceUnchecked(std::forward<V>(val));
        return vec;
    }
    constexpr static auto withCapacity(usize capacity) -> Vec {
        auto vec = Vec::create();
        vec.setAlloc(capacity);

        return vec;
    }
    constexpr static auto uninitialized(usize len) -> Vec {
        auto vec = Vec::create();
        vec.setAlloc(len);
        vec.m_len = len;
        return vec;
    }

    constexpr static auto fromStdVector(std::vector<T>&& vec) -> Vec {
        Vec dst = Vec::withCapacity(vec.size());
        if constexpr (!std::is_same_v<T, bool>) {
            if constexpr (kUsesTriviallyRelocatableFastpath) {
                if (!vec.empty()) std::memcpy(dst.m_data, vec.data(), vec.size() * sizeof(T));
                dst.m_len = vec.size();
            } else {
                for (auto& elem : vec) dst.emplaceUnchecked(std::move(elem));
            }
        } else {
            /*                      std::vector<bool>
            ⠀⠀⢀⠀⣠⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
            ⢀⠀⣿⡂⢹⡇⠀⠀⣰⠄⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
            ⢸⡇⢸⣇⢸⣇⠀⢀⣿⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢾⠀⠀⣯⡀⡆⠀⠀
            ⢸⣷⢸⣇⣸⣇⠀⣾⠏⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⣀⣀⣀⣠⣀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢳⣂⠀⣿⡄⢸⡀⣤
            ⢠⣿⣿⣿⣿⣿⣿⠇⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⣾⣿⣿⣊⡝⠛⠙⠂⠄⠠⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠈⢿⣦⣼⣷⣼⣁⠼
            ⢸⣿⣿⣿⣿⣿⣿⣀⢀⣀⣀⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣰⣿⣿⣿⣿⡻⣥⢋⡔⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠻⣿⣂⣜⣿⡟⢿⣿⣿⣄
            ⠈⣿⣿⣿⣿⣿⣿⣿⠿⠋⠉⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣿⣿⣿⣿⣿⣷⢯⣿⣾⡔⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠙⢪⣷⣿⢿⣿⣿
            ⠀⣿⣿⣟⢿⠿⠋⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢻⣿⡟⠛⠉⡉⢸⡉⠁⢀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢢⣽⣗⣿⠇
            ⠀⣿⣿⣿⡏⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠺⣿⡇⣤⡤⢔⡿⣇⠀⢦⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⣿⣿⣯⠀
            ⠘⡟⣛⠋⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠈⡇⣿⣿⠗⡲⠏⠟⠿⠀⠈⠓⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⣿⠍⠁⠁⠀
            ⠃⡜⡠⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠘⣼⣿⡟⢡⡿⠿⠷⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢸⣟⠒⠂⠂
            ⠐⢐⠁⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢻⠸⣡⢶⣿⣟⡃⠀⠘⠀⠀⢀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣼⡇⠀⡀⠀
            ⢠⡏⠀⠃⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⡰⢨⠣⠉⠉⠋⠉⠀⠀⠀⠀⢈⠀⡂⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠠⡿⠀⠀⠀⠀
            ⢺⡇⢸⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⣽⡿⢛⢭⠏⣢⠍⠈⠖⠀⠀⠒⣶⢦⡁⠂⠀⠀⠀⠀⠀⠯⠤⣤⣴⢶⣍⠝⣯⣦⡀⠀⠀⠀⠀⠀⠀⠀⠀⢌⣿⠱⠀⠀⠀⠀⠀
            ⣯⣯⠸⣀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⡠⠄⠀⠈⠀⠁⠀⠀⠀⠀⠀⠀⠀⠂⠀⠀⠏⠈⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠧⠍⠶⠤⠈⣆⠀⠀⠀⠀⠀⠀⠀⣷⡻⠀⣼⠀⠀⠀
            ⣯⣨⡀⢀⡠⠤⣐⠤⣀⣰⠔⠊⠁⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠁⠑⠐⠐⠢⠺⠥⡾⠉⡠⠀⠀⠀
            ⠋⠙⠈⠉⠉⠁⠈⠈⠀⠁⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
            ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
            ⠓⠂⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀
            ⠀⠀⠇⣣⡁⢶⣠⢀⣀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⢶⠀⡶⣲⠀⣆⡒⣰⠒⢦⢰⠀⢰⡆⣴⠐⣶⠒⣐⣒⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⣠⣴⣺⣿⣿⣿⠛
            ⠀⠀⠑⢌⠻⣗⣔⠉⡅⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠞⠚⠃⠻⠴⠃⠦⠝⠘⠤⠎⠸⠤⠘⠧⠞⠀⠛⠀⠰⠤⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣼⡟⣾⣿⣿⣿⠃⠀
            ⠀⠀⠀⠀⠉⠢⠁⠀⠀⠀⠀⢀⣤⣤⣤⣄⠀⠀⢠⣤⠀⠀⣤⣄⠀⠀⠀⣤⣤⠀⢠⣤⣤⣤⣤⣤⡄⢠⣤⣄⠀⠀⠀⠀⣤⣤⡄⠀⠀⠀⢠⣤⡄⠀⠀⠀⢘⡮⡝⣿⣿⡿⢆⠁⠀
            ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣰⣿⠏⠉⠉⢿⣷⠀⢸⣿⠀⠠⣿⣿⣧⡀⠀⣿⣿⠀⢸⣿⡏⠉⠉⠉⠁⢼⣿⣿⡄⠀⠀⢸⡿⣿⡇⠀⠀⢀⣿⢻⣷⠀⠀⠀⠞⡜⣹⣿⣿⡙⢆⠀⠀
            ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣿⣿⠀⠀⠀⠀⠀⠀⢸⣿⠀⠐⣿⡯⢻⣷⡀⣿⣿⠀⢸⣿⣷⣶⣶⡆⠀⢺⣿⠹⣿⡀⢠⣿⠃⣿⡇⠀⠀⣾⡟⠀⢿⣧⠀⠀⠀⠠⢽⣿⣯⡙⠀⠀⠀
            ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢿⣿⡀⠀⠀⣠⣤⠀⢸⣿⠀⢈⣿⡧⠀⠹⣿⣿⣿⠀⢸⣿⡇⠀⠀⠀⠀⢸⣿⡄⢻⣧⣾⡏⢠⣿⡇⠀⣼⣿⣷⣶⣾⣿⣇⠀⠀⠀⠘⣿⢣⠜⠁⠀⠀
            ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠈⢿⣿⣶⣾⣿⠏⠀⢸⣿⠀⠀⣿⡷⠀⠀⠹⣿⣿⠀⢸⣿⣿⣿⣿⣿⡆⢸⣿⡆⠀⢿⡿⠀⢰⣿⡇⢀⣿⡏⠀⠀⠀⢹⣿⡀⠀⠀⠀⠀⠈⡆⠀⠀⠀
            ⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠈⠉⠉⠀⠀⠀⠈⠉⠀⠀⠉⠁⠀⠀⠀⠉⠉⠀⠈⠉⠉⠈⠉⠉⠁⠈⠉⠀⠀⠈⠁⠀⠀⠉⠁⠈⠉⠀⠀⠀⠀⠈⠉⠁⠐⡀⠀⠀⠀⠀⠀⠀⠀
            */
            dst.m_len = vec.size();
            for (usize i = 0; i < dst.m_len; i++) {
                dst[i] = static_cast<bool>(vec[i]);
            }
        }
        return dst;
    }

    // ---- Access ---------------------------------------------------------------------------------------------------------------------------------------------

    constexpr auto len() const noexcept -> usize { return m_len; }
    constexpr auto size() const noexcept -> usize { return m_len; }
    constexpr auto capacity() const noexcept -> usize { return m_capacity; }

    constexpr auto data() noexcept -> T* { return m_data; }
    constexpr auto begin() noexcept -> T* { return m_data; }
    constexpr auto end() noexcept -> T* { return m_data + m_len; }

    constexpr auto data() const noexcept -> const T* { return m_data; }
    constexpr auto begin() const noexcept -> const T* { return m_data; }
    constexpr auto end() const noexcept -> const T* { return m_data + m_len; }

    constexpr T& operator[](usize index) noexcept {
#ifndef NDEBUG
        if (index >= m_len) {
            panic("attempted to access index {} but len is {}", index, m_len);
        }
#endif
        return m_data[index];
    }
    constexpr const T& operator[](usize index) const noexcept {
#ifndef NDEBUG
        if (index >= m_len) {
            panic("attempted to access index {} but len is {}", index, m_len);
        }
#endif
        return m_data[index];
    }

    constexpr Option<std::reference_wrapper<T>> at(usize index) noexcept {
        if (index >= m_len) return None;
        return Some(std::ref(m_data[index]));
    }
    constexpr Option<std::reference_wrapper<const T>> at(usize index) const noexcept {
        if (index >= m_len) return None;
        return Some(std::cref(m_data[index]));
    }

    constexpr T& first() noexcept { return m_data[0]; }
    constexpr const T& first() const noexcept { return m_data[0]; }
    constexpr T& last() noexcept { return m_data[m_len - 1]; }
    constexpr const T& last() const noexcept { return m_data[m_len - 1]; }

    constexpr auto isEmpty() const noexcept -> bool { return m_len == 0; }
    template <typename U> constexpr auto contains(const U& val) const noexcept -> bool { return std::find(m_data, m_data + m_len, val) != m_data + m_len; }

    // ---- Reserve / Resize -----------------------------------------------------------------------------------------------------------------------------------

    constexpr auto reserve(usize newCapacity) {
        if (newCapacity <= m_capacity) return;
        usize autoNewCap = m_capacity + m_capacity / 2;
        reallocate(std::max(newCapacity, autoNewCap));
    }

    constexpr auto reserveExact(usize newCapacity) {
        if (newCapacity <= m_capacity) return;
        reallocate(newCapacity);
    }

    constexpr auto shrinkToFit() {
        if (m_capacity == m_len) return;
        reallocate(m_len);
    }

    template <typename U>
    constexpr auto resize(usize newLen, U&& val) {
        if (newLen > m_capacity) reserve(newLen);
        resizeCommon(newLen, std::forward<U>(val));
    }

    template <typename U>
    constexpr auto resizeExact(usize newLen, U&& val) {
        if (newLen > m_capacity) reserveExact(newLen);
        resizeCommon(newLen, std::forward<U>(val));
    }

    constexpr auto truncate(usize newLen) {
        if (newLen >= m_len) return;
        if constexpr (kNeedsFinalizer) {
            for (usize i = newLen; i < m_len; i++) m_data[i].~T();
        }
        m_len = newLen;
    }

    // ---- Mutation -------------------------------------------------------------------------------------------------------------------------------------------

    constexpr auto push(const T& val) { emplace(val); }
    constexpr auto push(T&& val) { emplace(std::move(val)); }

    template <typename... Args>
    constexpr auto emplace(Args&&... args) -> T& {
        if (m_len == m_capacity) reserve(m_capacity + 1);
        auto index = m_data + m_len;
        new (index) T(std::forward<Args>(args)...);
        m_len++;
        return *index;
    }

    template <typename... Args>
    constexpr auto emplaceUnchecked(Args&&... args) -> T& {
        auto index = m_data + m_len;
        new (index) T(std::forward<Args>(args)...);
        m_len++;
        return *index;
    }

    constexpr auto pop() {
        if (m_len == 0) return;
        m_len--;
        if constexpr (kNeedsFinalizer) m_data[m_len].~T();
    }

    constexpr auto pop2() -> Option<T> {
        if (m_len == 0) return None;
        m_len--;
        auto elem = std::move(m_data[m_len]);
        if constexpr (kNeedsFinalizer) m_data[m_len].~T();
        return Some(std::move(elem));
    }

    constexpr auto clear() {
        if (m_len == 0) return;
        destroyFinalize();
        m_len = 0;
    }

    constexpr auto sort() -> void {
        if (m_len == 0) return;
        std::sort(m_data, m_data + m_len);
    }

    /// Removes consecutive elements in the vector.
    ///
    /// If the vector is sorted, then this removes all duplicates.
    constexpr auto dedup() -> void {
        if (m_len == 0) return;
        auto it = std::unique(m_data, m_data + m_len);
        m_len = std::distance(m_data, it);
    }

    template <typename P> constexpr auto retain(P pred) requires requires(T& d) { { pred(d) } -> std::convertible_to<bool>; }
    {
        usize newLen = 0;
        for (usize src = 0; src < m_len; src++) {
            if (pred(m_data[src])) {
                if (newLen != src) {
                    if constexpr (kUsesTriviallyRelocatableFastpath) {
                        m_data[newLen] = std::move(m_data[src]);
                    } else {
                        new (m_data + newLen) T(std::move(m_data[src]));
                        if constexpr (kNeedsFinalizer) m_data[src].~T();
                    }
                }
                newLen++;
            } else {
                if constexpr (kNeedsFinalizer) m_data[src].~T();
            }
        }

        m_len = newLen;
    }

    // ---- Insertion ------------------------------------------------------------------------------------------------------------------------------------------

    // todo: insert
    // todo: splice
    constexpr auto extend(Slice<const T> other) {
        reserve(m_len + other.len());

        if constexpr (std::is_trivially_copyable_v<T>) {
            std::memcpy(m_data + m_len, other.data(), other.len() * sizeof(T));
            m_len += other.len();
        } else {
            for (auto& elem : other) emplace(std::move(elem));
        }
    }

    constexpr auto copyFrom(Slice<const T> other) {
        reserve(other.len());
        m_len = 0;
        if constexpr (std::is_trivially_copyable_v<T>) {
            std::memcpy(m_data, other.data(), other.len() * sizeof(T));
            m_len = other.len();
        } else {
            for (auto& elem : other) emplace(elem);
        }
    }

    // ---- Slice Conversion -----------------------------------------------------------------------------------------------------------------------------------

    constexpr auto asSlice() const -> Slice<const T> { return Slice<const T>(m_data, m_len); }
    constexpr auto asSliceMut() -> Slice<T> { return Slice<T>(m_data, m_len); }

    // ---- Iterators ------------------------------------------------------------------------------------------------------------------------------------------

    constexpr auto iter() const -> SliceIter<const T> { return asSlice().iter(); }
    constexpr auto iterMut() -> SliceIter<T> { return asSliceMut().iter(); }

    constexpr auto iterRev() const -> ReverseSliceIter<const T> { return asSlice().iterRev(); }
    constexpr auto iterRevMut() -> ReverseSliceIter<T> { return asSliceMut().iterRev(); }

private:
    constexpr Vec(T* data, usize len, usize capacity) noexcept : m_data(data), m_len(len), m_capacity(capacity) {}

    constexpr auto destroyFinalize() {
        if constexpr (kNeedsFinalizer) {
            for (usize i = 0; i < m_len; i++) m_data[i].~T();
        }
    }

    template <typename V>
    constexpr auto resizeCommon(usize newLen, V&& val) {
        if (newLen > m_len) {
            for (usize i = m_len; i < newLen; i++) new (m_data + i) T(std::forward<V>(val));
        } else {
            if constexpr (kNeedsFinalizer) {
                for (usize i = newLen; i < m_len; i++) m_data[i].~T();
            }
        }
        m_len = newLen;
    }

    constexpr auto setAlloc(usize capacity) {
        Mem::free(m_data);
        m_data = Mem::allocChecked<T>(capacity);
        m_capacity = capacity;
    }

    constexpr auto reallocate(usize newCapacity) {
        if constexpr (kUsesTriviallyRelocatableFastpath) {
            auto ptr = Mem::reallocChecked(m_data, newCapacity);
            m_data = ptr;
        } else {
            auto ptr = Mem::allocChecked<T>(newCapacity);
            for (usize i = 0; i < m_len; i++) {
                new (ptr + i) T(std::move(m_data[i]));
                if constexpr (kNeedsFinalizer) m_data[i].~T();
            }
            Mem::free(m_data);
            m_data = ptr;
        }
        m_capacity = newCapacity;
    }

    T* m_data        = nullptr;
    usize m_len      = 0_usize;
    usize m_capacity = 0_usize;
};

export extern template class Vec<u8>;
export extern template class Vec<u16>;
export extern template class Vec<u32>;
export extern template class Vec<u64>;
export extern template class Vec<i8>;
export extern template class Vec<i16>;
export extern template class Vec<i32>;
export extern template class Vec<i64>;
export extern template class Vec<f32>;
export extern template class Vec<f64>;
export extern template class Vec<bool>;