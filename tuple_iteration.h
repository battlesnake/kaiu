#pragma once
#include <tuple>
#include <utility>
#include <cstddef>

namespace kaiu {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"

using namespace std;

namespace detail {

template <typename Tuple, typename Func, size_t... index>
void tuple_each_impl(Tuple const & tuple, Func func, index_sequence<index...>)
{
	auto res = { (func(get<index>(tuple)), nullptr)... };
}

template <typename Tuple, typename Func, size_t... index>
void tuple_each_with_index_impl(Tuple const & tuple, Func func, index_sequence<index...>)
{
	auto res = { (func.template operator()<const typename tuple_element<index, Tuple>::type&, index>(get<index>(tuple)), nullptr)... };
}

template <typename Tuple, typename Func, size_t... index>
auto tuple_map_impl(Tuple const & tuple, Func func, index_sequence<index...>)
	-> decltype(make_tuple(func(get<index>(tuple))...))
{
	return make_tuple(func(get<index>(tuple))...);
}

}

/*** Foreach ***/

template <typename Tuple, typename Func, typename Indices = make_index_sequence<tuple_size<Tuple>::value>>
void tuple_each(Tuple const & tuple, Func func)
{
	detail::tuple_each_impl(tuple, func, Indices());
}

template <typename Tuple, typename Func, typename Indices = make_index_sequence<tuple_size<Tuple>::value>>
void tuple_each_with_index(Tuple const & tuple, Func func)
{
	detail::tuple_each_with_index_impl(tuple, func, Indices());
}

/*** Map ***/

template <typename Tuple, typename Func, typename Indices = make_index_sequence<tuple_size<Tuple>::value>>
auto tuple_map(Tuple const & tuple, Func func)
	-> decltype(tuple_map_impl(tuple, func, Indices()))
{
	return detail::tuple_map_impl(tuple, func, Indices());
}

#pragma GCC diagnostic pop
}
