export module projnekomata:graphics.fontsystem.font_face;
import projnekomata.cs;

export namespace projnekomata {

struct FontFace {
    u32 handleIndex;

    bool operator==(const FontFace& other) const {
        return handleIndex == other.handleIndex;
    }

    constexpr auto clone() const -> FontFace {
        return FontFace { handleIndex };
    }
};

}