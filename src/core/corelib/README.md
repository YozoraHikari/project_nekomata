# Project Nekomata CoreLib

This is a set of random containers and some convenience features I made when I was bored.
They bring a more Rust-style approach to C++ to solve some sanity problems with its standard library, such as bad
ergonomics/syntax, low performance, and LSP breakage (fuck you `std::ranges`).
They are also thin by-design to make them cheap in terms of compile time.

In this project it's preferable to use these containers/algorithms over the standard library ones (e.g. `Vec` instead of
`std::vector`, `HashMap` instead of `std::unordered_map`) for the reasons stated above.

Note: These containers might have some bugs and edge cases... but I still think they're pretty good.

## Examples

Consider the task of finding all the even numbers in a vector, squaring them, and then summing the squares.
In plain C++ without `std::ranges`:
```cpp
std::vector<int> v = {1, 2, 3, 4, 5};
int accum = 0;
for (const int& x : v) {
    if (x % 2 == 0) {
        accum += x * x;
    }
}
```

With C++ with `std::ranges`:
```cpp
std::vector<int> v = {1, 2, 3, 4, 5};

int sum = std::ranges::fold_left(
    v | std::views::filter([](int x) { return x % 2 == 0; })
        | std::views::transform([](int x) { return x * x; }),
    0,
    std::plus{}
);
```

With CoreLib:
```cpp
auto v = Vec<i32>::create({1, 2, 3, 4, 5});
auto sum = v.iter()
    .filter([](i32 x) { return x % 2 == 0; })
    .map([](i32 x) { return x * x; })
    .sum();
```

