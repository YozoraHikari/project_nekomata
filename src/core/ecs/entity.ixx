export module projnekomata:core.ecs.entity;
import std;
import projnekomata.corelib;

export namespace projnekomata::ecs {

class Entity {
public:
    static constexpr u64 G_INDEX_MASK      = 0x00000000ffffffff;
    static constexpr u64 G_GENERATION_MASK = 0xffffffff00000000;
    static constexpr u32 G_GENERATION_SHIFT_LEFT = 32;

    Entity() = default;
    Entity(u64 id) : m_id(id) {}

    static auto create(u32 generation, u32 index) {
        return Entity((static_cast<u64>(generation) << G_GENERATION_SHIFT_LEFT) | static_cast<u64>(index));
    }

    static auto null() {
        return Entity(0);   
    }

    [[nodiscard]] auto index() const -> u32 {
        return static_cast<u32>(m_id & G_INDEX_MASK);
    }

    [[nodiscard]] auto generation() const -> u32 {
        return static_cast<u32>((m_id & G_GENERATION_MASK) >> G_GENERATION_SHIFT_LEFT);
    }

    auto operator==(const Entity& other) const -> bool { return m_id == other.m_id; }
    auto operator!=(const Entity& other) const -> bool { return m_id != other.m_id; }
    [[nodiscard]] auto valid() const -> bool { return m_id != 0; }

    u64 m_id = 0;
};

struct EntityHash {
    static u64 hash(Entity e) {
        return std::hash<u64>{}(e.m_id);
    }
};

}