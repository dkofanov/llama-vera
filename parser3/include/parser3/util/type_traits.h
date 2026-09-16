#pragma once

#include <cstddef>
#include <tuple>
#include <utility>

namespace vera::parser3 {

template <class Tuple>
struct ReversedTuple;

template <class... Elements>
struct ReversedTuple<std::tuple<Elements...>> {
  private:
    template <std::size_t... Indices>
    static auto Reverse(std::index_sequence<Indices...>)
        -> std::tuple<std::tuple_element_t<sizeof...(Elements) - 1 - Indices, std::tuple<Elements...>>...>;

  public:
    using Type = decltype(Reverse(std::make_index_sequence<sizeof...(Elements)>{}));
};

template <class Tuple>
using ReversedTupleT = typename ReversedTuple<Tuple>::Type;

template <class Type_>
struct TypeTag {
    using Type = Type_;
};

template <class Tuple, class Function, std::size_t... Indices>
constexpr void ForEachTupleType(Function &&function, std::index_sequence<Indices...>) {
    (function(TypeTag<std::tuple_element_t<Indices, Tuple>>{}), ...);
}

template <class Tuple, class Function>
constexpr void ForEachTupleType(Function &&function) {
    ForEachTupleType<Tuple>(std::forward<Function>(function), std::make_index_sequence<std::tuple_size_v<Tuple>>{});
}

} // namespace vera::parser3
