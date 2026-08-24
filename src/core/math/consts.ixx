export module projnekomata:core.math.consts;
import std;
import projnekomata.corelib;

export namespace projnekomata::math::consts {

template <typename T> constexpr T epsilonValue() noexcept {
    if constexpr (std::is_same_v<T, float>) return 1e-6f;
    if constexpr (std::is_same_v<T, double>) return 1e-12f;
    return std::numeric_limits<T>::epsilon();
}

constexpr float EPSILON = 1e-6f;
constexpr float PI = std::bit_cast<float>(0x40490fdbu);

}