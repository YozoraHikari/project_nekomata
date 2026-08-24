export module projnekomata:core.overloaded;
import std;
import projnekomata.corelib;

export namespace projnekomata {

template <class... Arms>
struct overloaded : Arms... {
    using Arms::operator()...;
};

template <class Variant, class... Arms>
constexpr auto matchOlf(Variant&& v, Arms&&... arms) -> decltype(auto) {
    return std::visit(
        overloaded{ std::forward<Arms>(arms)... },
        std::forward<Variant>(v)
    );
}

template <class Variant, class... Arms>
constexpr auto match(Variant&& v, Arms&&... arms) -> decltype(auto) {
    return v.match(overloaded{ std::forward<Arms>(arms)... });
}

} // namespace projnekomata