export module projnekomata:core.ecs;
import std;
import projnekomata.corelib;
export import :core.ecs.component_pool;
export import :core.ecs.entity;
export import :core.ecs.script_base;

export namespace projnekomata::ecs {

class World {
public:
    World() = default;
    auto createEntity() -> Entity {
        u32 index;
        u32 generation;

        if (!m_entityIndexFreelist.empty()) {
            index = m_entityIndexFreelist.front();
            m_entityIndexFreelist.pop();
            // NOTE: destroying an entity already incremented that generation
            generation = m_generations[index];
        } else {
            index = static_cast<u32>(m_generations.size());
            m_generations.emplace(1);
            generation = 1;
        }

        Entity ent = Entity::create(generation, index);
        m_aliveEntities.emplace(ent);
        return ent;
    }

    void destroyEntity(Entity ent) {
        if (!isEntityValid(ent))
            return;

        for (auto& cset : m_components.values())
            cset->removeForEntity(ent);

        removeAllScripts(ent);

        u32 index = ent.index();
        m_generations[index]++;
        m_entityIndexFreelist.push(index);

        m_aliveEntities.retain([&](const auto& other) -> bool { return other != ent; });
    }

    bool isEntityValid(Entity ent) const {
        u32 idx = ent.index();
        return idx < m_generations.size() && m_generations[idx] == ent.generation();
    }

    const Vec<Entity>& entities() const { return m_aliveEntities; }
    usize entityCount() const { return m_aliveEntities.size(); }

    template <typename T, typename... Args> T& emplace(Entity e, Args&&... args) {
        debug_assert(isEntityValid(e), "attempted to add component to entity that does not exist");
        return components<T>().emplace(e, std::forward<Args>(args)...);
    }

    template <typename T> void remove(Entity e) { components<T>().remove(e); }

    template <typename T> bool has(Entity e) {
        auto vect = m_components.get(std::type_index(typeid(T)));
        if (vect.isNone()) return false;
        return vect.unwrap().get()->containsEntity(e);
    }

    template <typename T> T& get(Entity e) { return components<T>().get(e); }

    template <typename T> const T& get(Entity e) const { return components<T>().get(e); }

    template <typename T> T* tryGet(Entity e) {
        auto vect = m_components.get(std::type_index(typeid(T)));
        if (vect.isNone()) return nullptr;
        return static_cast<ComponentSet<T>*>(vect.unwrap().get())->try_get(e);
    }

    template <typename T> ComponentSet<T>& components() {
        auto key = std::type_index(typeid(T));
        auto vect = m_components.get(key);
        if (vect.isNone()) {
            auto set = Unique<ComponentSet<T>>::create();
            auto& p = m_components.insert(std::move(key), Unique<IComponentSet>::upcast(std::move(set)));
            return *static_cast<ComponentSet<T>*>(p.ptr());
        }
        return *static_cast<ComponentSet<T>*>(vect.unwrap().get().ptr());
    }

    // Attach a script of the given type to the given entity.
    // The given script must derive from ScriptBase.
    template <typename ScriptType, typename... Args> ScriptType& addScript(Entity e, Args&&... args) {
        static_assert(std::is_base_of_v<ScriptBase, ScriptType>, "S must derive from ScriptBase");
        debug_assert(isEntityValid(e), "attempted to add script to entity that does not exist");

        auto s = Unique<ScriptType>::create(std::forward<Args>(args)...);
        s->m_workingEntity = e;
        s->m_workingWorld = this;

        ScriptType* raw = s.ptr();
        if (!m_scripts.contains(e)) m_scripts.insert(e, Vec<Unique<ScriptBase>>::create());
        m_scripts[e].emplace(Unique<ScriptBase>::upcast(std::move(s)));
        raw->onCreate();
        return *raw;
    }

    // Remove the given script type from the given entity.
    template <typename ScriptType> void removeScript(Entity ent) {
        auto vec = m_scripts.get(ent);
        if (vec.isNone()) return;

        auto& cont = vec.unwrap().get();
        cont.retain([](const auto& s) -> bool { return dynamic_cast<ScriptType*>(s.ptr()) == nullptr; });
    }

    void removeAllScripts(Entity ent) { m_scripts.remove(ent); }

    // Get first script of given type of given entity. Returns nullptr if absent.
    template <typename ScriptType> ScriptType* getScript(Entity ent) {
        auto vec = m_scripts.get(ent);
        if (vec.isNone()) return nullptr;

        for (auto& s : vec.unwrap().get())
            if (auto* cast = dynamic_cast<ScriptType*>(s.ptr()))
                return cast;
        return nullptr;
    }

    void scriptsUpdate(float dt) {
        for (auto& vec : m_scripts.values())
            for (auto& s : vec)
                s->onUpdate(dt);
    }

private:
    Vec<u32> m_generations;
    std::queue<u32> m_entityIndexFreelist;
    Vec<Entity> m_aliveEntities;

    HashMap<std::type_index, Unique<IComponentSet>> m_components   = HashMap<std::type_index, Unique<IComponentSet>>::create();


    HashMap<Entity, Vec<Unique<ScriptBase>>, EntityHash> m_scripts = HashMap<Entity, Vec<Unique<ScriptBase>>, EntityHash>::create();
};

} // namespace projnekomata::ecs