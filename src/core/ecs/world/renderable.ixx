export module projnekomata:core.ecs.world.renderable;
import :graphics.texturesystem.texture_manager;
import :graphics.meshsystem.mesh_asset_storage;
import :graphics.materialsystem.mat_manager;

export namespace projnekomata {

struct RenderableComponent {
    MeshAsset meshAsset;
    gfx::Material material;

    RenderableComponent() = default;
    RenderableComponent(MeshAsset meshAsset, gfx::Material material) : meshAsset(meshAsset), material(material) {}
};

}
