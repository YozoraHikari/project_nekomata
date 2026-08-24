export module projnekomata:core.ui.nodes.image;
import projnekomata.corelib;
import :core.ui.nodes.node;

export namespace projnekomata::ui {

enum class UIImageFitMode {
    ScaleToFit,
    UniformScaleToFit,
    ForceImageSize,
};

class UIImage : public UINode {
public:
    explicit UIImage(Texture texture, math::Vector2f texcoordStart, math::Vector2f texcoordEnd)
        : texture(std::move(texture)), texcoordStart(std::move(texcoordStart)), texcoordEnd(std::move(texcoordEnd)) {}

    constexpr static auto create(Texture texture, math::Vector2f texcoordStart, math::Vector2f texcoordEnd) -> Unique<UIImage> {
        return Unique<UIImage>::create(std::move(texture), texcoordStart, texcoordEnd);
    }

    auto scanForUnrasterizedGlyphs(Vec<FontRasterBatch>& dstFontRasterBatches, gfx::DynamicBitmapFontAtlas& fontAtlas) -> void override {}

    Texture texture;
    math::Vector2f texcoordStart;
    math::Vector2f texcoordEnd;

    UIImageFitMode fitMode = UIImageFitMode::UniformScaleToFit;

    auto buildDrawCmds(BuildCtx& ctx, InheritanceCtx inheritanceCtx, math::Aabb2f bounds) -> math::Vector2f override {
        if (!visible) return math::Vector2f(0.0f);

        auto size = computeIntrinsicImageSize(bounds.extents());

        ctx.drawCmds.emplace(UiTextureDrawCmd{
            .ssBegin       = bounds.min(),
            .ssEnd         = bounds.min() + size,
            .texcoordBegin = texcoordStart,
            .texcoordEnd   = texcoordEnd,
            .texture       = texture
        });
        return bounds.extents();
    }

protected:
    auto measureSelf(gfx::DynamicBitmapFontAtlas& fontAtlas, MeasureConstraints constraints) -> math::Vector2f override {
        return computeIntrinsicImageSize(constraints.maxSize);
    }

private:
    auto computeIntrinsicImageSize(math::Vector2f maxSize) -> math::Vector2f {
        switch (fitMode) {
            case UIImageFitMode::ScaleToFit: return maxSize;
            case UIImageFitMode::UniformScaleToFit: {
                auto& im = gfx::TextureManager::get().getTextureResources(texture).image();

                // TODO: rework TextureManager to always have valid VkImages in the texture resources
                if (im.vkImage() == nullptr) return maxSize;
                auto ext = im.extent();

                auto sx = maxSize.x() / ext.width;
                auto sy = maxSize.y() / ext.height;
                auto scale = std::min(sx, sy);
                return math::Vector2f(ext.width * scale, ext.height * scale);
            }
            case UIImageFitMode::ForceImageSize: {
                auto& im = gfx::TextureManager::get().getTextureResources(texture).image();

                // TODO: rework TextureManager to always have valid VkImages in the texture resources
                if (im.vkImage() == nullptr) return maxSize;
                auto ext = im.extent();
                return math::Vector2f(ext.width, ext.height);
            }
        }
    }
};

}