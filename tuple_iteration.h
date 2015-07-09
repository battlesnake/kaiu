#pragma once
#include <tuple>
#include <utility>
#include <cstddef>

namespace mark {

using namespace std;

/*** Foreach ***/

template <typename Tuple, typename Func, size_t... index>
void tuple_each_impl(Tuple const & tuple, Func func, index_sequence<index...>)
{
	auto res = { (func(get<index>(tuple)), nullptr)... };
}

template <typename Tuple, typename Func, typename Indices = make_index_sequence<tuple_size<Tuple>::value>>
void tuple_each(Tuple const & tuple, Func func)
{
	tuple_each_impl(tuple, func, Indices());
}

template <typename Tuple, typename Func, size_t... index>
void tuple_each_with_index_impl(Tuple const & tuple, Func func, index_sequence<index...>)
{
	auto res = { (func.template operator()<const typename tuple_element<index, Tuple>::type&, index>(get<index>(tuple)), nullptr)... };
}

template <typename Tuple, typename Func, typename Indices = make_index_sequence<tuple_size<Tuple>::value>>
void tuple_each_with_index(Tuple const & tuple, Func func)
{
	tuple_each_with_index_impl(tuple, func, Indices());
}

/*** Map ***/

template <typename Tuple, typename Func, size_t... index>
auto tuple_map_impl(Tuple const & tuple, Func func, index_sequence<index...>)
	-> decltype(make_tuple(func(get<index>(tuple))...))
{
	return make_tuple(func(get<index>(tuple))...);
}

template <typename Tuple, typename Func, typename Indices = make_index_sequence<tuple_size<Tuple>::value>>
auto tuple_map(Tuple const & tuple, Func func)
	-> decltype(tuple_map_impl(tuple, func, Indices()))
{
	return tuple_map_impl(tuple, func, Indices());
}

#ifdef test_tuple_iteration
#include <iostream>

struct functor_each {
	template <typename T>
	void operator ()(T&& t) { cout << t << endl; }
	template <typename T, size_t index>
	void operator ()(T&& t) { cout << index << ":" << t << endl; }
};
struct functor_map {
	template <typename T>
	T operator ()(T&& t) { return t + t; }
	template <typename T, size_t index>
	T operator ()(T&& t) { return t + t; }
};

void test() {
	tuple<int, bool, string> t;
	tuple_each(t, functor_each());
	tuple_each_with_index(t, functor_each());
	t = tuple_map(t, functor_map());
	tuple_each_with_index(t, functor_each());
	t = tuple_map_with_index(t, functor_map());
	tuple_each_with_index(t, functor_each());
}
#endif

}
