export module projnekomata:core.ecs.component_pool;
import std;
import projnekomata.cs;
import :core.ecs.entity;
import :core.ecs.world.camera;

export namespace projnekomata::ecs {

constexpr u32 INVALID_INDEX = 0xffffffff;

template <typename T> struct ComponentSetSnapshot {
    Vec<T> m_storage              = Vec<T>::create();
    Vec<u32> m_sparseToStorage    = Vec<u32>::create();
    Vec<Entity> m_storageToEntity = Vec<Entity>::create();

    [[nodiscard]] bool containsEntity(Entity ent) const {
        u32 idx = ent.index();
        return idx < m_sparseToStorage.len() && m_sparseToStorage[idx] != INVALID_INDEX;
    }

    T& get(Entity ent) {
        debug_assert(containsEntity(ent), "attempted to get component from entity that does not have that component");
        return m_storage[m_sparseToStorage[ent.index()]];
    }
};

class IComponentSet {
public:
    virtual ~IComponentSet() = default;

    [[nodiscard]] virtual bool containsEntity(Entity ent) const = 0;
    virtual void removeForEntity(Entity ent) = 0;
    [[nodiscard]] virtual usize size() const = 0;
};

template <typename T> class ComponentSet : public IComponentSet {
public:
    ComponentSet() = default;

    void resizeSparseToFit(u32 index) {
        if (index >= m_sparseToStorage.len())
            m_sparseToStorage.resize(index + 1, INVALID_INDEX);
    }

    template <typename... Args> T& emplace(Entity e, Args&&... args) {
        u32 index = e.index();
        resizeSparseToFit(index);

        if (m_sparseToStorage[index] != INVALID_INDEX)
            panic("attempted to add component to entity that already has that component");

        m_sparseToStorage[index] = static_cast<u32>(m_storage.len());
        m_storage.emplace(std::forward<Args>(args)...);

        m_storageToEntity.emplace(e);
        return m_storage.last();
    }

    void removeForEntity(Entity ent) override {
        if (!containsEntity(ent)) return;

        u32 storageIndex = m_sparseToStorage[ent.index()];
        u32 lastStorageIndex = static_cast<u32>(m_storage.len()) - 1;
        
        if (storageIndex != lastStorageIndex) {
            // NOTE: the component is not the last component in the dense vector, move the last element to this component
            m_storage[storageIndex] = std::move(m_storage[lastStorageIndex]);
            m_storageToEntity[storageIndex] = m_storageToEntity[lastStorageIndex];

            // NOTE: for the entity whose component was moved to correctly index it again
            m_sparseToStorage[m_storageToEntity[storageIndex].index()] = storageIndex;
        }

        m_storage.pop();
        m_storageToEntity.pop();
        m_sparseToStorage[ent.index()] = INVALID_INDEX;
    }

    [[nodiscard]] bool containsEntity(Entity ent) const override {
        u32 idx = ent.index();
        return idx < m_sparseToStorage.len() && m_sparseToStorage[idx] != INVALID_INDEX;
    }

    T& get(Entity ent) {
        if (!containsEntity(ent)) panic("attempted to get component from entity that does not have that component");
        return m_storage[m_sparseToStorage[ent.index()]];
    }

    [[nodiscard]] const T& get(Entity ent) const {
        if (!containsEntity(ent)) panic("attempted to get component from entity that does not have that component");
        return m_storage[m_sparseToStorage[ent.index()]];
    }

    T* tryGet(Entity ent) {
        if (!containsEntity(ent)) return nullptr;
        return &m_storage[m_sparseToStorage[ent.index()]];
    }

    auto storage() -> Vec<T>& { return m_storage; }
    auto sparseToStorage() -> Vec<u32>& { return m_sparseToStorage; }
    auto storageToEntity() -> Vec<Entity>& { return m_storageToEntity; }

    [[nodiscard]] usize size() const override {
        return m_storage.len();
    }

    void copyTo(ComponentSetSnapshot<T>& buf) const requires (std::is_copy_constructible_v<T>) {
        buf.m_storage.copyFrom(m_storage.asSlice());
        buf.m_sparseToStorage.copyFrom(m_sparseToStorage.asSlice());
        buf.m_storageToEntity.copyFrom(m_storageToEntity.asSlice());
    }

private:
    Vec<T>      m_storage         = Vec<T>::create();
    Vec<u32>    m_sparseToStorage = Vec<u32>::create();
    Vec<Entity> m_storageToEntity = Vec<Entity>::create();
};

}