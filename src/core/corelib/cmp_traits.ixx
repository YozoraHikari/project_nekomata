export module projnekomata.corelib:cmp_traits;
import std;

template <typename A, typename B> concept Eq = std::equality_comparable_with<A, B> && std::equality_comparable_with<B, A>;