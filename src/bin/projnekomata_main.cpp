import std;
import fmt;
import vulkan;
import projnekomata;
#include <cstdlib>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

using namespace projnekomata::math;
using namespace projnekomata::core::input;
using Vertex = projnekomata::Vertex;

auto menuButton(const std::string& text, projnekomata::FontFace fontFace, const projnekomata::ui::ElementStyle& elementStyle, const projnekomata::ui::ElementStyle& buttonStyle, std::function<void(Vector2f)> onClick) -> Unique<projnekomata::ui::UIBox> {
    return projnekomata::ui::UIBox::builder()
        .child(
            projnekomata::ui::UIInteractive::builder()
                .child(
                    projnekomata::ui::UIPanel::builder()
                        .child(
                            projnekomata::ui::UIBox::builder()
                                .child(
                                    projnekomata::ui::UIText::builder(text, 16.0f, fontFace)
                                        .style(elementStyle)
                                        .build()
                                )
                                .positionX(20.0f)
                                .anchorPreset(projnekomata::ui::AnchorPreset::MiddleLeft)
                                .build()
                        )
                        .style(buttonStyle)
                        .build()
                )
                .onClick(std::move(onClick))
                .capturesClicks(true)
                .capturesHover(true)
                .build()
        )
        .extentPercentX(100.0f)
        .extentY(50.0f)
        .build();
}


class CameraScript : public projnekomata::ecs::ScriptBase {
public:
    CameraScript(projnekomata::FontFace face) : m_fontFace(face) {}

    void onCreate() override {
        m_workingWorld->get<projnekomata::LocalTransformComponent>(m_workingEntity)
            .m_transform3d = Transform3D::identity();
        m_workingWorld->get<projnekomata::LocalTransformComponent>(m_workingEntity)
            .m_transform3d.m_position = { 0.0f, 0.0f, 0.0f };

        auto& ts = projnekomata::gfx::TextureManager::get();

        auto samplerSettings = projnekomata::gfx::SamplerParams::defaultValues()
            .setAnisotropy(16.0f);

        projnekomata::Texture ts1 = ts.loadKtx2TextureAsync("//assets:/ui_test.ktx2", samplerSettings);
        projnekomata::Texture ts2 = ts.loadKtx2TextureAsync("//assets:/ui_test2.ktx2", samplerSettings);
        projnekomata::Texture ts3 = ts.loadKtx2TextureAsync("//assets:/ui_test3.ktx2", samplerSettings);
        projnekomata::Texture ts4 = ts.loadKtx2TextureAsync("//assets:/ui_test4.ktx2", samplerSettings);
        projnekomata::Texture ts5 = ts.loadKtx2TextureAsync("//assets:/ui_test5.ktx2", samplerSettings);

        auto posText = projnekomata::ui::UIText::create("hai :3", 18.0f, m_fontFace);

        m_text = posText.ptr();
        //projnekomata::ui::UiSystem::get().getRoot().addChild(std::move(posText));

        // ---- Escape Overlay Memes ---------------------------------------------------------------------------------------------------------------------------

        auto escapeOverlayMeme1 = projnekomata::ui::UIBox::builder()
            .child(
                projnekomata::ui::UIImage::create(ts1, Vector2f(0.0f), Vector2f(1.0f))
            )
            .position({800.0f, 50.0f})
            .extent({250.0f, 250.0f})
            .build();

        auto escapeOverlayMeme2 = projnekomata::ui::UIBox::builder()
            .child(
                projnekomata::ui::UIImage::create(ts2, Vector2f(0.0f), Vector2f(1.0f))
            )
            .position({800.0f, 310.0f})
            .extent({320.0f, 320.0f})
            .build();

        auto escapeOverlayMeme3 = projnekomata::ui::UIBox::builder()
            .child(
                projnekomata::ui::UIImage::create(ts3, Vector2f(0.0f), Vector2f(1.0f))
            )
            .position({800.0f, 640.0f})
            .extent({250.0f, 275.0f})
            .build();

        auto escapeOverlayMeme4 = projnekomata::ui::UIBox::builder()
            .child(
                projnekomata::ui::UIImage::create(ts4, Vector2f(0.0f), Vector2f(1.0f))
            )
            .position({1200.0f, 200.0f})
            .extent({400.0f, 350.0f})
            .build();

        auto escapeOverlayMeme5 = projnekomata::ui::UIBox::builder()
            .child(
                projnekomata::ui::UIImage::create(ts5, Vector2f(0.0f), Vector2f(1.0f))
            )
            .position({1200.0f, 600.0f})
            .extent({400.0f, 380.0f})
            .build();

        // ---- Escape Overlay Menu ----------------------------------------------------------------------------------------------------------------------------


        auto buttonStyle = projnekomata::ui::ElementStyle::builder()
            .color(projnekomata::Color::fromRgba32Float(.204f, .204f, .204f, 0.95f))
            .colorHovered(projnekomata::Color::fromRgba32Float(.254f, .254f, .254f, 0.95f))
            .colorPressed(projnekomata::Color::fromRgba32Float(.154f, .154f, .154f, 0.95f))
            .build();

        auto buttonTextStyle = projnekomata::ui::ElementStyle::builder()
            .color(projnekomata::Color::fromRgba32Float(1.0f, 1.0f, 1.0f, 1.0f))
            .colorHovered(projnekomata::Color::fromRgb8Uint(255, 115, 0))
            .hoverStateInheritsParent(true)
            .build();

        auto overlayStyle = projnekomata::ui::ElementStyle::builder()
            .color(projnekomata::Color::fromRgba32Float(.0f, .0f, .0f, 0.25f))
            .build();

        auto menuPanelStyle = projnekomata::ui::ElementStyle::builder()
            .color(projnekomata::Color::fromRgba32Float(.0f, .0f, .0f, 0.85f))
            .build();

        auto escapeOverlay = projnekomata::ui::UIPanel::builder()
            .child(
                projnekomata::ui::UICanvas::builder()
                    .addChild(std::move(escapeOverlayMeme1))
                    .addChild(std::move(escapeOverlayMeme2))
                    .addChild(std::move(escapeOverlayMeme3))
                    .addChild(std::move(escapeOverlayMeme4))
                    .addChild(std::move(escapeOverlayMeme5))
                    .addChild(
                        projnekomata::ui::UIBox::builder()
                            .child(
                                projnekomata::ui::UIPanel::builder()
                                    .child(
                                        projnekomata::ui::UICanvas::builder()
                                            .addChild(
                                                projnekomata::ui::UIBox::builder()
                                                    .child(
                                                        projnekomata::ui::UIText::create("Project Nekomata", 18.0f, m_fontFace)
                                                    )
                                                    .position({20.0f, 190.0f})
                                                    .build()
                                            )
                                            .addChild(
                                                projnekomata::ui::UIBox::builder()
                                                    .child(
                                                        projnekomata::ui::UIStack::builder()
                                                            .addChild(menuButton(
                                                                "Continue",
                                                                m_fontFace,
                                                                buttonTextStyle,
                                                                buttonStyle,
                                                                [this](Vector2f) {
                                                                    Input::get().setMouseMode(MouseMode::Captured);
                                                                     m_handleInput = true;
                                                                     m_escOverlay->visible = false;
                                                                }
                                                            ))
                                                            .addChild(menuButton(
                                                                "Test Logger Messages",
                                                                m_fontFace,
                                                                buttonTextStyle,
                                                                buttonStyle,
                                                                [this](Vector2f) {
                                                                    projnekomata::log::trace("Test Trace");
                                                                    projnekomata::log::info("Test Info");
                                                                    projnekomata::log::warn("Test Warning");
                                                                    projnekomata::log::error("Test Error");
                                                                    projnekomata::log::crit("Test Critical");
                                                                }
                                                            ))
                                                            .addChild(menuButton(
                                                                "Panic",
                                                                m_fontFace,
                                                                buttonTextStyle,
                                                                buttonStyle,
                                                                [this](Vector2f) {
                                                                    panic("Test Panic");
                                                                }
                                                            ))
                                                            .spacing(10.0f)
                                                            .direction(projnekomata::ui::StackDirection::TopToBottom)
                                                            .build()

                                                    )
                                                    .positionY(300.0f)
                                                    .extentPercentX(100.0f)
                                                    .extentY(400.0f)
                                                    .build()
                                            )
                                            .build()
                                    )
                                    .style(menuPanelStyle)
                                    .build()
                            )
                            .position({250.0f, 0.0f})
                            .extentX(500.0f)
                            .extentPercentY(100.0f)
                            .build()
                    )
                    .addChild(
                        projnekomata::ui::UIBox::builder()
                            .child(
                                projnekomata::ui::UITextInput::create("", Some(std::string("Type a command...")), 16.0f, m_fontFace, [](auto text) {
                                    projnekomata::cmdRun(text);
                                })
                            )
                            .anchorPreset(projnekomata::ui::AnchorPreset::BottomLeft)
                            .extentPercentX(100.0f)
                            .extentY(30.0f)
                            .build()
                    )
                    .build()
            )
            .style(overlayStyle)
            .visible(false)
            .build();

        m_escOverlay = escapeOverlay.ptr();
        projnekomata::ui::UiSystem::get().getRoot().addChild(std::move(escapeOverlay));
    }

    void onDestroy() override {}

    void onUpdate(float dt) override {
        if (m_handleInput) {
            auto mousedelta = Input::get().mouseDelta();
            m_rotationYaw += mousedelta.x() * 0.1f;
            m_rotationPitch -= mousedelta.y() * 0.1f;

            m_rotationYaw = std::fmod(m_rotationYaw, 360.0f);
            if (m_rotationYaw < 0.0f) m_rotationYaw += 360.0f;
            m_rotationPitch = std::clamp(m_rotationPitch, -90.0f, 90.0f);


            auto yawQuat = Quaternion::fromAxisAngle(Vector3f(0.0f, 1.0f, 0.0f), degreesToRadians(m_rotationYaw));
            auto pitchQuat = Quaternion::fromAxisAngle(Vector3f(1.0f, 0.0f, 0.0f), degreesToRadians(m_rotationPitch));

            m_workingWorld->get<projnekomata::LocalTransformComponent>(m_workingEntity).m_transform3d.m_rotation = yawQuat * pitchQuat;
        }

        float forwardVel = 0.0f;
        float sidewaysVel = 0.0f;
        float upVel = 0.0f;

        if (m_handleInput) {
            if (Input::get().isKeyDown(Key::W)) forwardVel -= 1.0f;
            if (Input::get().isKeyDown(Key::S)) forwardVel += 1.0f;
            if (Input::get().isKeyDown(Key::A)) sidewaysVel += 1.0f;
            if (Input::get().isKeyDown(Key::D)) sidewaysVel -= 1.0f;
            if (Input::get().isKeyDown(Key::Space)) upVel += 1.0f;
            if (Input::get().isKeyDown(Key::C)) upVel -= 1.0f;
        }
        auto dp = Vector3f(sidewaysVel, upVel, forwardVel);

        if (dp != Vector3f(0.0f)) {
            auto factor = 5.0f;
            if (Input::get().isKeyDown(Key::LShift)) factor = 250.0f;

            auto rotation = m_workingWorld->get<projnekomata::LocalTransformComponent>(m_workingEntity).m_transform3d.m_rotation;
            auto delta = dp.normalize() * dt * factor;
            delta = rotation.rotateVector3f(delta);

            m_workingWorld->get<projnekomata::LocalTransformComponent>(m_workingEntity).m_transform3d.m_position += delta;
        }

        if (Input::get().isKeyPressed(Key::Escape)) {
            Input::get().setMouseMode(MouseMode::Normal);
            m_handleInput = false;
            m_escOverlay->visible = true;
        }

        auto camPos = m_workingWorld->get<projnekomata::LocalTransformComponent>(m_workingEntity).m_transform3d.m_position;
        //acquireInto<projnekomata::ui::UiText>(m_text->element).text = "";
    }

    bool m_handleInput = true;
    float m_rotationPitch = 0.0f;
    float m_rotationYaw = 0.0f;

    projnekomata::FontFace m_fontFace;
    projnekomata::ui::UINode* m_text = nullptr;
    projnekomata::ui::UINode* m_escOverlay = nullptr;
};

class SpinningCubesScript : public projnekomata::ecs::ScriptBase {
public:
    SpinningCubesScript() = default;

    auto onCreate() -> void override {}
    auto onUpdate(float dt) -> void override {
        auto rotation = Quaternion::fromAxisAngle(Vector3f(0.0f, 1.0f, 0.0f), dt * -4.5f);

        auto& transform = m_workingWorld->get<projnekomata::LocalTransformComponent>(m_workingEntity);
        transform.m_transform3d.m_rotation = rotation * transform.m_transform3d.m_rotation;
    }
};

std::pair<Vec<Vertex>, Vec<u32>> generateSphere(u32 latSegments, u32 lonSegments, float radius) {
    Vec<Vertex> vertices;
    Vec<u32> indices;

    for (u32 lat = 0; lat <= latSegments; lat++) {
        float theta = static_cast<float>(lat) / static_cast<float>(latSegments) * consts::PI;
        float sinTheta = std::sin(theta);
        float cosTheta = std::cos(theta);

        for (u32 lon = 0; lon <= lonSegments; lon++) {
            float phi = static_cast<float>(lon) / static_cast<float>(lonSegments) * consts::PI * 2;
            float sinPhi = std::sin(phi);
            float cosPhi = std::cos(phi);

            float x = cosPhi * sinTheta;
            float y = cosTheta;
            float z = sinPhi * sinTheta;

            Vector2f texcoord = Vector2f(static_cast<float>(lon) / static_cast<float>(lonSegments), static_cast<float>(lat) / static_cast<float>(latSegments));
            Vector3f normal = Vector3f(x, y, z);
            Vector3f tangent = Vector3f(-sinPhi, 0.0f, cosPhi).normalize();
            Vector4f handedTangent = Vector4f(tangent.x(), tangent.y(), tangent.z(), 1.0f);

            vertices.emplace(Vector3f(x * radius, y * radius, z * radius), normal, handedTangent, texcoord, Vector4f(1.0f, 0.0f, 0.0f, 1.0f));
        }
    }

    for (u32 lat = 0; lat < latSegments; lat++) {
        for (u32 lon = 0; lon < lonSegments; lon++) {
            u32 first = (lat * (lonSegments + 1)) + lon;
            u32 second = first + lonSegments + 1;
            indices.emplace(first);
            indices.emplace(second);
            indices.emplace(first + 1);

            indices.emplace(first + 1);
            indices.emplace(second);
            indices.emplace(second + 1);
        }
    }

    return {std::move(vertices), std::move(indices)};
}


void onGameInit(Unique<projnekomata::ecs::World>& world) {
    auto& ts = projnekomata::gfx::TextureManager::get();

    auto matShaderCode = projnekomata::gfx::vkrhi::SpirvShaderCode::loadFromFile("//spirv:/mainrender_geom.spv").unwrap();
    auto mainMaterialShader = projnekomata::gfx::MaterialShader::builder()
        .setPrerastVS(matShaderCode)
        .setFragmentShader(matShaderCode)
        .useInDeferredPass()
        .setMaterialPropertyStructSize(sizeof(projnekomata::CoreMaterialProps))
        .build();

    auto samplerSettings = projnekomata::gfx::SamplerParams::defaultValues()
        .setAnisotropy(16.0f);

    projnekomata::Texture ts1 = ts.loadKtx2TextureAsync("//assets:/abstractart.ktx2", samplerSettings);
    projnekomata::Texture ts2 = ts.loadKtx2TextureAsync("//assets:/fihcalling.ktx2", samplerSettings);
    projnekomata::Texture ts3 = ts.loadKtx2TextureAsync("//assets:/ui_test.ktx2", samplerSettings);
    projnekomata::Texture ts4 = ts.loadKtx2TextureAsync("//assets:/ui_test2.ktx2", samplerSettings);
    projnekomata::Texture ts5 = ts.loadKtx2TextureAsync("//assets:/ui_test3.ktx2", samplerSettings);
    projnekomata::Texture ts6 = ts.loadKtx2TextureAsync("//assets:/ui_test4.ktx2", samplerSettings);
    projnekomata::Texture ts7 = ts.loadKtx2TextureAsync("//assets:/ui_test5.ktx2", samplerSettings);
    auto fnt = projnekomata::FontManager::get().loadFont("//assets:/Inconsolata-VariableFont_wdth,wght.ttf");

    auto& mas = projnekomata::gfx::MeshAssetStorage::get();
    auto mesh = mas.allocateMeshAsset();
    mas.getLodList(mesh).maxLodIndex = 3; // set all LOD levels used
    // initially bestLodIndex = ~0 => no LODs are ready (rendering thread will skip rendering).

    // LOD 3
    auto [l3verts, l3inds] = generateSphere(10, 10, 1.0f);
    mas.perpareLodSpace(mesh, 3, l3verts.size() * sizeof(Vertex), l3inds.size() * sizeof(u32), alignof(Vertex), 4);
    memcpy(mas.getLodList(mesh).lods[3].meshSuballocation.vertexBuffer.hostAddress, l3verts.data(), l3verts.size() * sizeof(Vertex));
    memcpy(mas.getLodList(mesh).lods[3].meshSuballocation.indexBuffer.hostAddress, l3inds.data(), l3inds.size() * sizeof(u32));
    mas.getLodList(mesh).bestLodIndex.store(3, std::memory_order_release);
    mas.getLodList(mesh).lods[3].screenSizeThreshold = 8.0f;

    // LOD 2
    auto [l2verts, l2inds] = generateSphere(16, 16, 1.0f);
    mas.perpareLodSpace(mesh, 2, l2verts.size() * sizeof(Vertex), l2inds.size() * sizeof(u32), alignof(Vertex), 4);
    memcpy(mas.getLodList(mesh).lods[2].meshSuballocation.vertexBuffer.hostAddress, l2verts.data(), l2verts.size() * sizeof(Vertex));
    memcpy(mas.getLodList(mesh).lods[2].meshSuballocation.indexBuffer.hostAddress, l2inds.data(), l2inds.size() * sizeof(u32));
    mas.getLodList(mesh).bestLodIndex.store(2, std::memory_order_release);
    mas.getLodList(mesh).lods[2].screenSizeThreshold = 20.0f;

    // LOD 1
    auto [l1verts, l1inds] = generateSphere(30, 30, 1.0f);
    mas.perpareLodSpace(mesh, 1, l1verts.size() * sizeof(Vertex), l1inds.size() * sizeof(u32), alignof(Vertex), 4);
    memcpy(mas.getLodList(mesh).lods[1].meshSuballocation.vertexBuffer.hostAddress, l1verts.data(), l1verts.size() * sizeof(Vertex));
    memcpy(mas.getLodList(mesh).lods[1].meshSuballocation.indexBuffer.hostAddress, l1inds.data(), l1inds.size() * sizeof(u32));
    mas.getLodList(mesh).bestLodIndex.store(1, std::memory_order_release);
    mas.getLodList(mesh).lods[1].screenSizeThreshold = 80.0f;

    // LOD 0
    auto [l0verts, l0inds] = generateSphere(100, 100, 1.0f);
    mas.perpareLodSpace(mesh, 0, l0verts.size() * sizeof(Vertex), l0inds.size() * sizeof(u32), alignof(Vertex), 4);
    memcpy(mas.getLodList(mesh).lods[0].meshSuballocation.vertexBuffer.hostAddress, l0verts.data(), l0verts.size() * sizeof(Vertex));
    memcpy(mas.getLodList(mesh).lods[0].meshSuballocation.indexBuffer.hostAddress, l0inds.data(), l0inds.size() * sizeof(u32));
    mas.getLodList(mesh).bestLodIndex.store(0, std::memory_order_release);
    mas.getLodList(mesh).lods[0].screenSizeThreshold = 200.0f;

    std::random_device rd;
    std::mt19937 gen(rd());

    std::uniform_real_distribution<float> colorDist(0.02f, 1.0f);
    std::uniform_real_distribution<float> roughnessDist(0.0f, 1.0f);
    std::uniform_int_distribution<usize> texIndexDist(0, 7);
    //for (usize i = 0; i < 1990; i++) {
    //    auto ent = world->createEntity();
    //    world->emplace<projnekomata::Transform>(ent);
    //    world->emplace<projnekomata::Renderable>(ent, mesh, ts1);
    //    world->addScript<MovingScript>(ent, 0.0f, radiusDist(gen), thetaSpeedDist(gen), phiSpeedDist(gen), thetaDist(gen), phiDist(gen), rotationConstDist(gen), rotationConstDist(gen));
    //    if (i % 100 == 0) {
    //        world->emplace<projnekomata::PointLight>(ent, Vector3f{lightradianceDist(gen), 0.0f, lightradianceDist(gen)});
    //    }
    //}

    // for demo

    usize inOneDim = 11;
    float spacing = 2.5f;
    for (usize x = 0; x < inOneDim; x++) {
        for (usize y = 0; y < inOneDim; y++) {
                auto scale = Vector3f(1.0f);
                auto rotation = Quaternion(0.0f, 0.0f, 0.0f, 1.0f);
                auto translation = Vector3f(x * spacing, 0.0f, y * spacing);
                auto transform = projnekomata::LocalTransformComponent(translation, rotation, scale);
                auto matprops = projnekomata::CoreMaterialProps()
                    .setRoughness(roughnessDist(gen))
                    .setMetallic(roughnessDist(gen));

                auto texindex = texIndexDist(gen);

                switch (texindex) {
                    case 0: matprops.setColor(ts1); break;
                    case 1: matprops.setColor(ts2); break;
                    case 2: matprops.setColor(ts3); break;
                    case 3: matprops.setColor(ts4); break;
                    case 4: matprops.setColor(ts5); break;
                    case 5: matprops.setColor(ts6); break;
                    case 6: matprops.setColor(ts7); break;
                    case 7: matprops.setColor(Vector3f(colorDist(gen), colorDist(gen), colorDist(gen))); break;
                }

                auto matl = projnekomata::gfx::Material::create<projnekomata::CoreMaterialProps>(mainMaterialShader, std::move(matprops));

                auto ent = world->createEntity();
                world->emplace<projnekomata::LocalTransformComponent>(ent, std::move(transform));
                world->emplace<projnekomata::WorldTransformComponent>(ent);
                world->emplace<projnekomata::RenderableComponent>(ent, mesh, matl);
//                world->addScript<MovingScript>(ent, 0.0f, radiusDist(gen), thetaSpeedDist(gen), phiSpeedDist(gen), thetaDist(gen), phiDist(gen), rotationConstDist(gen), rotationConstDist(gen));
        }
    }

    auto scale = Vector3f(1.0f);
    auto rotation = Quaternion(0.0f, 0.0f, 0.0f, 1.0f);
    auto translation = Vector3f(10.0f, 40.0f, 10.0f);
    auto transform = projnekomata::LocalTransformComponent(translation, rotation, scale);

    auto lightEnt = world->createEntity();

    world->emplace<projnekomata::PointlightComponent>(lightEnt, Vector3f{10000.0f, 10000.0f, 10000.0f});
    world->emplace<projnekomata::LocalTransformComponent>(lightEnt, std::move(transform));
    world->emplace<projnekomata::WorldTransformComponent>(lightEnt);

    auto cameraEnt = world->createEntity();
    world->emplace<projnekomata::CameraComponent>(cameraEnt, projnekomata::CameraComponent{0.01f, 10000.0f, 90.0f, true});
    world->emplace<projnekomata::LocalTransformComponent>(cameraEnt);
    world->emplace<projnekomata::WorldTransformComponent>(cameraEnt);
    world->addScript<CameraScript>(cameraEnt, fnt);
    Input::get().setMouseMode(MouseMode::Captured);

    auto parentOfTheFuckedCubes = world->createEntity();
    world->emplace<projnekomata::LocalTransformComponent>(parentOfTheFuckedCubes, Vector3f(20.0f, 15.0f, 6.0f), Quaternion::identity(), Vector3f(1.0f, 1.0f, 1.0f));
    world->emplace<projnekomata::WorldTransformComponent>(parentOfTheFuckedCubes);
    world->addScript<SpinningCubesScript>(parentOfTheFuckedCubes);

    auto stuff = projnekomata::gfx::importSceneFromGltf(*world, "//assets:/deccer-cubes-main/deccer_cubes_merged_textured_uastc.gltf", mainMaterialShader, Some(parentOfTheFuckedCubes));

    auto parentOfTheMoreFuckedCubes = world->createEntity();
    world->emplace<projnekomata::LocalTransformComponent>(parentOfTheMoreFuckedCubes, Vector3f(-30.0f, 15.0f, 6.0f), Quaternion::fromEulerAngles(consts::PI / -2.0f, 0.0f, 0.0f), Vector3f(1.0f, 1.0f, 1.0f));
    world->emplace<projnekomata::WorldTransformComponent>(parentOfTheMoreFuckedCubes);

    auto stuff2 = projnekomata::gfx::importSceneFromGltf(*world, "//assets:/deccer-cubes-main/deccer_cubes_textured_complex_uastc.gltf", mainMaterialShader, Some(parentOfTheMoreFuckedCubes));

    auto lightAttachedToCubes = world->createEntity();
    // world->emplace<projnekomata::PointlightComponent>(lightAttachedToCubes, Vector3f{100.0f, 100.0f, 100.0f});
    world->emplace<projnekomata::LocalTransformComponent>(lightAttachedToCubes, Vector3f(10.0f, -4.0f, 6.0f), Quaternion::identity(), Vector3f(1.0f, 1.0f, 1.0f));
    world->emplace<projnekomata::WorldTransformComponent>(lightAttachedToCubes);

    world->get<projnekomata::ChildrenComponent>(parentOfTheMoreFuckedCubes).m_children.emplace(lightAttachedToCubes);
}

int main(int argc, char* argv[]) {
    projnekomata::log::info("haii :3");

    projnekomata::entry(onGameInit);

    return 0;
}

