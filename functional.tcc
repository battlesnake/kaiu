#define functional_tcc
#include "functional.h"

namespace kaiu {

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

template <typename Result, typename Functor, typename Args>
Result invoke(Functor func, Args args)
{
	return detail::invoke_with_tuple<Result, Functor, Args>
		(func, forward<Args>(args));
}

}
