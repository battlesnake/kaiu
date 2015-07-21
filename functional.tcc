#define functional_tcc
#include "functional.h"

namespace mark {

using namespace std;

namespace detail {

template <typename Result, typename Functor, typename CurriedArgs, int ArgCount,
	bool Complete, int... Indices>
Result InvokeWithTuple<Result, Functor, CurriedArgs, ArgCount, Complete, Indices...>
	::invoke(Functor func, CurriedArgs&& t)
{
	constexpr auto numbers_count = sizeof...(Indices);
	return InvokeWithTuple<Result, Functor, CurriedArgs, ArgCount,
		ArgCount == 1 + numbers_count, Indices..., numbers_count>
			::invoke(func, forward<CurriedArgs>(t));
}

template <typename Result, typename Functor, typename CurriedArgs, int ArgCount, int... Indices>
Result InvokeWithTuple<Result, Functor, CurriedArgs, ArgCount, true, Indices...>
	::invoke(Functor func, CurriedArgs&& t)
{
	return func(get<Indices>(forward<CurriedArgs>(t))...);
}

template <typename Result, int Arity, typename Functor, typename... CurriedArgs>
CurriedFunction<Result, Arity, Functor, CurriedArgs...>
	::CurriedFunction(Functor func) :
		func(func)
{
}

template <typename Result, int Arity, typename Functor, typename... CurriedArgs>
CurriedFunction<Result, Arity, Functor, CurriedArgs...>
	::CurriedFunction(Functor func, tuple<CurriedArgs...> curried_args) :
		func(func), curried_args(curried_args)
{
}

template <typename Result, int Arity, typename Functor, typename... CurriedArgs>
template <typename... RemainingArgs>
typename
	enable_if<Arity != sizeof...(CurriedArgs) + sizeof...(RemainingArgs),
		CurriedFunction<Result, Arity, Functor, CurriedArgs..., RemainingArgs...>>::type
CurriedFunction<Result, Arity, Functor, CurriedArgs...>
	::operator () (RemainingArgs... remaining_args) const
{
	auto next_curried_args = tuple_cat(curried_args,
		make_tuple(forward<RemainingArgs>(remaining_args)...));
	auto incomplete_functor = CurriedFunction<Result, Arity, Functor,
		CurriedArgs..., RemainingArgs...>(func, next_curried_args);
	return incomplete_functor;
}

template <typename Result, int Arity, typename Functor, typename... CurriedArgs>
template <typename... RemainingArgs>
typename
	enable_if<Arity == sizeof...(CurriedArgs) + sizeof...(RemainingArgs),
		Result>::type
CurriedFunction<Result, Arity, Functor, CurriedArgs...>
	::operator () (RemainingArgs... remaining_args) const
{
	auto next_curried_args = tuple_cat(curried_args,
			make_tuple(remaining_args...));
	auto complete_functor = CurriedFunction<Result, Arity, Functor,
		CurriedArgs..., RemainingArgs...>(func, next_curried_args);
	return complete_functor();
}

template <typename Result, int Arity, typename Functor, typename... CurriedArgs>
Result CurriedFunction<Result, Arity, Functor, CurriedArgs...>
	::operator() () const
{
	return Invoke<Result>(func, curried_args);
}

template <typename Result, int Arity, typename Functor, typename... CurriedArgs>
CurriedFunction<Result, Arity, Functor, CurriedArgs...>
	::operator Result () const
{
	return operator() ();
}

}

template <typename Result, typename Functor, typename... CurriedArgs>
detail::CurriedFunction<Result, 0, Functor, CurriedArgs...>
	Curry(Functor func, CurriedArgs... curried_args)
{
	return detail::CurriedFunction<Result, 0, Functor, CurriedArgs...>(func, make_tuple(curried_args...));
}

template <typename Result, int Arity, typename Functor, typename... CurriedArgs>
detail::CurriedFunction<Result, Arity, Functor, CurriedArgs...>
	Curry(Functor func, CurriedArgs... curried_args)
{
	return detail::CurriedFunction<Result, Arity, Functor, CurriedArgs...>(func, make_tuple(curried_args...));
}

template <typename Result, typename Functor, typename Args>
Result Invoke(Functor func, Args args)
{
	constexpr auto total = tuple_size<typename decay<Args>::type>::value;
	return detail::InvokeWithTuple<Result, Functor, Args, total, total == 0>
		::invoke(func, forward<Args>(args));
}

}
