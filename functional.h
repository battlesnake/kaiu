#pragma once
#include <functional>
#include <tuple>

namespace mark {

using namespace std;

namespace detail {

/*
 * InvokeWithTuple<Result, Functor, CurriedArgs...>(func, tuple<CurriedArgs...>)
 *
 * Calls a function, passing parameters from tuple
 */

template <typename Result, typename Functor, typename CurriedArgs, int ArgCount,
	bool Complete, int... Indices>
class InvokeWithTuple {
public:
	static Result invoke(Functor func, CurriedArgs&& t);
};

template <typename Result, typename Functor, typename CurriedArgs, int ArgCount, int... Indices>
struct InvokeWithTuple<Result, Functor, CurriedArgs, ArgCount, true, Indices...> {
public:
	static Result invoke(Functor func, CurriedArgs&& t);
};

/*
 * CurriedFunction<Result, Arity, Functor, CurriedArgs...>(func, [args...])
 *
 * Wraps a function and optionally binds arguments to it.
 *
 * The function operator can be used to specify remaining arguments, returning
 * either another CurriedFunction (if not all arguments are bound) or the result
 * of the invoked function if all arguments are bound.
 */

template <typename Result, int Arity, typename Functor, typename... CurriedArgs>
struct CurriedFunction {
public:
	CurriedFunction(Functor func);
	CurriedFunction(Functor func, tuple<CurriedArgs...> curried_args);
	/*
	 * If stored curried_args and passed curried_args don't cover parameter
	 * list, create a new functor bound to the new arguments and return it
	 */
	template <typename... RemainingArgs>
	typename
		enable_if<Arity != sizeof...(CurriedArgs) + sizeof...(RemainingArgs),
			CurriedFunction<Result, Arity, Functor, CurriedArgs..., RemainingArgs...>>::type
	operator () (RemainingArgs... remaining_args) const;
	/*
	 * If stored curried_args and passed curried_args covers the parameter list,
	 * create a new functor that has complete parameter list and invoke it,
	 * returning Result
	 */
	template <typename... RemainingArgs>
	typename
		enable_if<Arity == sizeof...(CurriedArgs) + sizeof...(RemainingArgs),
			Result>::type
	operator () (RemainingArgs... remaining_args) const;
	/* Function operator with no curried_args: Call and return result */
	Result operator() () const;
	/* Cast to Result: Call functor with curried_args & cast result  */
	operator Result () const;
private:
	Functor func;
	tuple<CurriedArgs...> curried_args;
};

}

/* Curry function with no parameters */
template <typename Result, typename Functor, typename... CurriedArgs>
detail::CurriedFunction<Result, 0, Functor, CurriedArgs...>
	Curry(Functor func, CurriedArgs... curried_args);

/* Curry function with Arity parameters */
template <typename Result, int Arity, typename Functor, typename... CurriedArgs>
detail::CurriedFunction<Result, Arity, Functor, CurriedArgs...>
	Curry(Functor func, CurriedArgs... curried_args);

/* Call a function using arguments stored in a tuple */
template <typename Result, typename Functor, typename Args>
Result Invoke(Functor func, Args args);

}

#ifndef functional_tcc
#include "functional.tcc"
#endif
