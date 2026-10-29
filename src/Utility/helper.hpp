#pragma once
template <typename Element_t, typename... Args, std::size_t... I>
auto make_array(std::index_sequence<I...>, Args&&... args) {
    return std::array<Element_t, sizeof...(I)>{
        (static_cast<void>(I), Element_t{std::forward<Args>(args)...})...};
}
template <typename Element_t, typename F, std::size_t... I>
auto make_array_l(std::index_sequence<I...>, F&& f) {
    return std::array<Element_t, sizeof...(I)>{
        f(std::integral_constant<std::size_t, I>{})...};
}
