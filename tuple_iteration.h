#pragma once
#include <tuple>
#include <utility>
#include <cstddef>

namespace kaiu {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"


namespace detail {

template <typename Tuple, typename Func, size_t... index>
void tuple_each_impl(Tuple const & tuple, Func func, std::index_sequence<index...>)
{
	auto res = { (func(std::get<index>(tuple)), nullptr)... };
}

template <typename Tuple, typename Func, size_t... index>
void tuple_each_with_index_impl(Tuple const & tuple, Func func, std::index_sequence<index...>)
{
	auto res = { (func.template operator()<const typename std::tuple_element<index, Tuple>::type&, index>(std::get<index>(tuple)), nullptr)... };
}

template <typename Tuple, typename Func, size_t... index>
auto tuple_map_impl(Tuple const & tuple, Func func, std::index_sequence<index...>)
	-> decltype(std::make_tuple(func(std::get<index>(tuple))...))
{
	return std::make_tuple(func(std::get<index>(tuple))...);
}

}

/*** Foreach ***/

template <typename Tuple, typename Func, typename Indices = std::make_index_sequence<std::tuple_size<Tuple>::value>>
void tuple_each(Tuple const & tuple, Func func)
{
	detail::tuple_each_impl(tuple, func, Indices());
}

template <typename Tuple, typename Func, typename Indices = std::make_index_sequence<std::tuple_size<Tuple>::value>>
void tuple_each_with_index(Tuple const & tuple, Func func)
{
	detail::tuple_each_with_index_impl(tuple, func, Indices());
}

/*** Map ***/

template <typename Tuple, typename Func, typename Indices = std::make_index_sequence<std::tuple_size<Tuple>::value>>
auto tuple_map(Tuple const & tuple, Func func)
	-> decltype(tuple_map_impl(tuple, func, Indices()))
{
	return detail::tuple_map_impl(tuple, func, Indices());
}

#pragma GCC diagnostic pop
}
