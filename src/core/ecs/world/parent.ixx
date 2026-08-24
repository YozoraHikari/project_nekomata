export module projnekomata:core.ecs.world.parent;
import :core.ecs.entity;

export namespace projnekomata {

class ParentComponent {
public:
    ecs::Entity m_parent;
};

class ChildrenComponent {
public:
    Vec<ecs::Entity> m_children;
};

}