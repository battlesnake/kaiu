#pragma once
#include <functional>
#include <tuple>
#include <utility>

namespace mark {

using namespace std;

namespace detail {

/*
 * InvokeWithTuple<Result, Functor, CurriedArgs...>(func, tuple<CurriedArgs...>)
 *
 * Calls a function, passing parameters from tuple
 */

template <typename Result, typename Functor, typename Args>
Result invoke_with_tuple(Functor func, Args tuple);

template <typename Result, typename Functor, typename Args, size_t ArgCount,
	size_t... Indices>
Result invoke_shuffle_args(Functor func, index_sequence<Indices...> indices, Args tuple);

/*
 * CurriedFunction<Result, Arity, Functor, CurriedArgs...>(func, [args...])
 *
 * Wraps a function and optionally binds arguments to it.
 *
 * The function operator can be used to specify remaining arguments, returning
 * either another CurriedFunction (if not all arguments are bound) or the result
 * of the invoked function if all arguments are bound.
 */

template <typename Result, size_t Arity, typename Functor, typename... CurriedArgs>
struct CurriedFunction {
private:
	using ArgsTuple = tuple<CurriedArgs...>;
public:
	CurriedFunction(Functor func);
	CurriedFunction(Functor func, const ArgsTuple& curried_args);
	/*
	 * Function operator ALWAYS invokes, to curry use the "apply" method or the
	 * "<<" operator.  A previous implementation curried using the function
	 * operator until sufficient arguments were available to invoke, but that
	 * could lead to difficult debugging scenarios involving 100+line template
	 * errors, so I separated "apply" and "invoke" into two different syntaxes.
	 *
	 * With parameters, same as .apply(extra_args).invoke()
	 */
	template <typename... ExtraArgs>
	typename enable_if<(sizeof...(ExtraArgs) > 0), Result>::type
	operator () (ExtraArgs&&... extra_args) const;
	/* With no parameters, same as .invoke() */
	template <typename... ExtraArgs>
	typename enable_if<(sizeof...(ExtraArgs) == 0), Result>::type
	operator () (ExtraArgs&&... extra_args) const;
	/*
	 * Left-shift operator to curry arguments one at a time (only curries, never
	 * invokes - unlike the function operator which invokes as soon as it can)
	 *
	 * Same as .apply(arg);
	 */
	template <typename Arg>
	CurriedFunction<Result, Arity, Functor, CurriedArgs..., Arg>
		operator << (Arg&& arg) const;
	/* Returns a new functor, curried with the extra arguments */
	template <typename... ExtraArgs>
	CurriedFunction<Result, Arity, Functor, CurriedArgs..., ExtraArgs...>
		apply (ExtraArgs&&... extra_args) const;
	/* Calls the function */
	template <size_t Arity_ = Arity>
	typename enable_if<(sizeof...(CurriedArgs) == Arity_), Result>::type
	invoke() const;
private:
	template <size_t NumArgs>
	void statically_check_args_count_for_invoke() const;
	Functor func;
	ArgsTuple curried_args;
};

}

template <typename Result, size_t Arity, typename Functor, typename... CurriedArgs>
using Curried = detail::CurriedFunction<Result, Arity, Functor, CurriedArgs...>;

/* Curry std::function with auto parameters */
template <typename Result, typename... Args, typename... CurriedArgs>
Curried<Result, sizeof...(Args), function<Result(Args...)>, CurriedArgs...>
	Curry(function<Result(Args...)> func, CurriedArgs... curried_args);

/* Curry function with Arity parameters */
template <typename Result, size_t Arity, typename Functor, typename... CurriedArgs>
Curried<Result, Arity, Functor, CurriedArgs...>
	Curry(Functor func, CurriedArgs... curried_args);

/* Call a function using arguments stored in a tuple */
template <typename Result, typename Functor, typename Args>
Result Invoke(Functor func, Args args);

}

#ifndef functional_tcc
#include "functional.tcc"
#endif
