module projnekomata;
import :graphics.fontsystem.dynamic_font_atlas;

namespace projnekomata::gfx {

AtlasShelfPacker::AtlasShelfPacker(std::nullptr_t) {}
AtlasShelfPacker::AtlasShelfPacker(i32 width, i32 height)
    : m_atlasWidth(width), m_atlasHeight(height) {}

Option<math::Vector2i> AtlasShelfPacker::pack(i32 width, i32 height) {
    // Try best fit first.
    Shelf* bestShelf = nullptr;
    i32 bestInefficiency = std::numeric_limits<i32>::max();

    for (auto& shelf : m_shelves) {
        i32 inefficiency = shelf.height - height;
        if (inefficiency >= 0                     // the glyph must fit in the shelf...
         && shelf.writerX + width <= m_atlasWidth // ... and fit in the atlas horizontally...
         && inefficiency < bestInefficiency       // ... and produce the least wasted space
            ) {
            bestShelf = &shelf;
            bestInefficiency = inefficiency;
        }
    }

    if (bestShelf) {
        math::Vector2i pos(bestShelf->writerX, bestShelf->writerY);
        bestShelf->writerX += width;
        return Some(pos);
    }

    // No shelves fit the glyphs, so try creating a new one.
    i32 newShelfY = m_shelves.isEmpty() ? 0 : m_shelves.last().writerY + m_shelves.last().height;
    if (newShelfY + height <= m_atlasHeight) {
        m_shelves.emplace(height, width, newShelfY);
        return Some(math::Vector2i(0, newShelfY));
    }

    // Atlas is full.
    return None;
}

} // namespace projnekomata