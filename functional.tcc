#define functional_tcc
#include "functional.h"

namespace mark {

using namespace std;

namespace detail {

template <typename Result, typename Functor, typename Args>
Result invoke_with_tuple(Functor func, Args tuple)
{
	constexpr auto ArgCount = tuple_size<Args>::value;
	return invoke_shuffle_args<Result, Functor, Args, ArgCount>
		(func, make_index_sequence<ArgCount>(), forward<Args>(tuple));
}

template <typename Result, typename Functor, typename Args, size_t ArgCount, size_t... Indices>
Result invoke_shuffle_args(Functor func, index_sequence<Indices...> indices, Args tuple)
{
	return func(get<Indices>(forward<Args>(tuple))...);
}

/*** CurriedFunction ***/

template <typename Result, size_t Arity, typename Functor, typename... CurriedArgs>
CurriedFunction<Result, Arity, Functor, CurriedArgs...>
	::CurriedFunction(Functor func) :
		func(func)
{
}

template <typename Result, size_t Arity, typename Functor, typename... CurriedArgs>
CurriedFunction<Result, Arity, Functor, CurriedArgs...>
	::CurriedFunction(Functor func, const tuple<CurriedArgs...>& curried_args) :
		func(func),
		curried_args(curried_args)
{
}

template <typename Result, size_t Arity, typename Functor, typename... CurriedArgs>
template <typename... ExtraArgs>
CurriedFunction<Result, Arity, Functor, CurriedArgs..., ExtraArgs...>
	CurriedFunction<Result, Arity, Functor, CurriedArgs...>
		::apply (ExtraArgs&&... extra_args) const
{
	/* See body of operator() for why we aren't using enable_of */
	constexpr auto NumArgs = sizeof...(CurriedArgs) + sizeof...(ExtraArgs);
	static_assert(NumArgs <= Arity,
		"Cannot curry function: too many arguments specified");
	return CurriedFunction<Result, Arity, Functor,
		CurriedArgs..., ExtraArgs...>(func,
			tuple_cat(
				curried_args,
				forward_as_tuple(extra_args...)));
}

template <typename Result, size_t Arity, typename Functor, typename... CurriedArgs>
template <typename Arg>
CurriedFunction<Result, Arity, Functor, CurriedArgs..., Arg&>
	CurriedFunction<Result, Arity, Functor, CurriedArgs...>
		::operator << (reference_wrapper<Arg> ref) const
{
	return apply(ref.get());
}

template <typename Result, size_t Arity, typename Functor, typename... CurriedArgs>
template <typename Arg>
CurriedFunction<Result, Arity, Functor, CurriedArgs..., typename decay<Arg>::type>
	CurriedFunction<Result, Arity, Functor, CurriedArgs...>
		::operator << (Arg&& arg) const
{
	return apply(static_cast<typename decay<Arg>::type>(arg));
}

template <typename Result, size_t Arity, typename Functor, typename... CurriedArgs>
template <typename... ExtraArgs>
typename enable_if<(sizeof...(ExtraArgs) > 0), Result>::type
CurriedFunction<Result, Arity, Functor, CurriedArgs...>
	::operator () (ExtraArgs&&... extra_args) const
{
	statically_check_args_count_for_invoke<sizeof...(CurriedArgs) + sizeof...(ExtraArgs)>();
	return apply(forward<ExtraArgs>(extra_args)...)
		.invoke();
}

template <typename Result, size_t Arity, typename Functor, typename... CurriedArgs>
template <typename... ExtraArgs>
typename enable_if<(sizeof...(ExtraArgs) == 0), Result>::type
CurriedFunction<Result, Arity, Functor, CurriedArgs...>
	::operator () (ExtraArgs&&... extra_args) const
{
	statically_check_args_count_for_invoke<sizeof...(CurriedArgs) + sizeof...(ExtraArgs)>();
	return invoke();
}

template <typename Result, size_t Arity, typename Functor, typename... CurriedArgs>
template <size_t Arity_>
typename enable_if<(sizeof...(CurriedArgs) == Arity_), Result>::type
CurriedFunction<Result, Arity, Functor, CurriedArgs...>
	::invoke () const
{
	statically_check_args_count_for_invoke<sizeof...(CurriedArgs)>();
	return invoke_with_tuple<Result, Functor, ArgsTuple>(func, curried_args);
}

template <typename Result, size_t Arity, typename Functor, typename... CurriedArgs>
template <size_t NumArgs>
void CurriedFunction<Result, Arity, Functor, CurriedArgs...>
	::statically_check_args_count_for_invoke() const
{
	/*
	 * Fail with static_assert instead of enable_if, this way we get a useful
	 * error message instead of pages of template soup
	 *
	 * We can use static_assert instead of enable_if since:
	 *  a) we're not overloading the function operator anymore.
	 *  b) this is a template function so will only be instantiated if called.
	 */
	static_assert(NumArgs >= Arity, "Cannot invoke curried function, not enough arguments curried/passed to it at time of invocation");
	static_assert(NumArgs <= Arity, "Cannot invoke curried function, too many arguments curried/passed to it at time of invocation");
}

template <typename Result, size_t Arity, typename Functor, typename... CurriedArgs>
bool CurriedFunction<Result, Arity, Functor, CurriedArgs...>
	::operator == (nullptr_t) const
{
	return false;
}

template <typename Result, size_t Arity, typename Functor, typename... CurriedArgs>
bool CurriedFunction<Result, Arity, Functor, CurriedArgs...>
	::operator != (nullptr_t) const
{
	return !operator ==(nullptr);
}

} /* end namespace detail */

/*** Curry */

template <typename Result, typename... Args, typename... CurriedArgs>
Curried<Result, sizeof...(Args), function<Result(Args...)>, CurriedArgs...>
	Curry(function<Result(Args...)> func, CurriedArgs... curried_args)
{
	return Curry<Result, sizeof...(Args), function<Result(Args...)>, CurriedArgs...>(func, curried_args...);
}

template <typename Result, size_t Arity, typename Functor, typename... CurriedArgs>
Curried<Result, Arity, Functor, CurriedArgs...>
	Curry(Functor func, CurriedArgs... curried_args)
{
	return detail::CurriedFunction<Result, Arity, Functor, CurriedArgs...>(func, forward_as_tuple(curried_args...));
}

template <typename Result, typename Functor, typename Args>
Result Invoke(Functor func, Args args)
{
	return detail::invoke_with_tuple<Result, Functor, Args>
		(func, forward<Args>(args));
}

#ifdef enable_monads
template <typename From, typename To,
	typename DFrom = typename decay<From>::type,
	typename DTo = typename decay<To>::type>
typename enable_if<
	is_curried_function<DTo>::value &&
	DTo::arity == 1,
		typename DTo::result_type>::type
	operator ,(From&& from, To to)
{
	return to(forward<From>(from));
}
#endif

}
