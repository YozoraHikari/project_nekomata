export module projnekomata:core.containers.sparse_set_storage;
import std;
import projnekomata.corelib;

export namespace projnekomata {

template<typename T> class SparseSetStorage {
public:
    const usize INVALID_INDEX = std::numeric_limits<usize>::max();

    auto allocate_one(const T& element) -> usize { return internal_allocate_one(element); }
    auto allocate_one(T&& element) -> usize { return internal_allocate_one(std::move(element)); }

    auto allocate_one_with_index(const T& element, usize index) -> usize { return internal_allocate_one_with_index(element, index); }
    auto allocate_one_with_index(T&& element, usize index) -> usize { return internal_allocate_one_with_index(std::move(element), index); }

    auto remove(usize index) -> Option<T> {
        if (index >= m_index_to_storage.len())
            return None;

        usize maybe_storage_index = m_index_to_storage[index];
        if (maybe_storage_index == INVALID_INDEX)
            return None;

        m_index_to_storage[index] = INVALID_INDEX;
        usize storage_index = maybe_storage_index;
        usize last_storage_index = m_storage.len() - 1;

        m_index_freelist.push(index);

        if (storage_index != last_storage_index) {
            std::swap(m_storage[storage_index], m_storage[last_storage_index]);
            std::swap(m_storage_to_index[storage_index], m_storage_to_index[last_storage_index]);

            usize moved_index = m_storage_to_index[storage_index];
            m_index_to_storage[moved_index] = storage_index;
        }

        m_storage_to_index.pop();

        auto removed = std::move(m_storage.last());
        m_storage.pop();

        return Some(std::move(removed));
    }

    auto get(usize index) -> T* {
        if (index >= m_index_to_storage.len()) {
            return nullptr;
        }

        auto storage_index = m_index_to_storage[index];
        if (storage_index == INVALID_INDEX) {
            return nullptr;
        }

        return &m_storage[storage_index];
    }

    auto get(usize index) const -> const T* {
        if (index >= m_index_to_storage.len())
            return nullptr;

        auto storage_index = m_index_to_storage[index];
        if (storage_index == INVALID_INDEX)
            return nullptr;

        return &m_storage[storage_index];
    }

    auto contains(usize index) const -> bool {
        return index < m_index_to_storage.len() && m_index_to_storage[index] != INVALID_INDEX;
    }

    auto len() const -> usize {
        return m_storage.len();
    }

    auto empty() const -> bool {
        return m_storage.isEmpty();
    }

    auto begin() { return m_storage.begin(); }
    auto end() { return m_storage.end(); }

    auto begin() const { return m_storage.begin(); }
    auto end() const { return m_storage.end(); }

    const Vec<T>& dense() const {
        return m_storage;
    }

    const Vec<usize>& dense_indices() const {
        return m_storage_to_index;
    }
private:
    template <typename U> auto internal_allocate_one(U&& element) -> usize {
        usize index;

        if (!m_index_freelist.isEmpty()) {
            index = m_index_freelist.last();
            m_index_freelist.pop();
        } else {
            index = m_index_to_storage.size();
            m_index_to_storage.push(INVALID_INDEX);
        }

        usize storage_index = m_storage.size();
        
        m_storage.emplace_back(std::forward<U>(element));
        m_storage_to_index.push(index);
        m_index_to_storage[index] = storage_index;

        return index;
    }

    template <typename U> auto internal_allocate_one_with_index(U&& element, usize index) -> void {
        usize storage_index = m_storage.len();

        if (index >= m_index_to_storage.len())
            m_index_to_storage.resize(index + 1, storage_index);

        if (m_index_to_storage[index] != INVALID_INDEX) {
            m_storage[m_index_to_storage[index]] = std::move(element);
            return;
        }

        m_storage.emplace(element);
        m_storage_to_index.emplace(index);
        m_index_to_storage[index] = storage_index;
    }

    Vec<T> m_storage;
    Vec<usize> m_storage_to_index;
    Vec<usize> m_index_to_storage;
    Vec<usize> m_index_freelist;
};

}