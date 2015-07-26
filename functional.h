#pragma once
#include <functional>
#include <tuple>
#include <utility>
#include <type_traits>

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
	static const auto is_curried_function = true;
	using result_type = Result;
	static constexpr auto arity = Arity - sizeof...(CurriedArgs);
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
	 * Similar as .apply(arg), but captures by VALUE instead of by reference,
	 * unless std::reference_wrapper is used.
	 */
	template <typename Arg>
	CurriedFunction<Result, Arity, Functor, CurriedArgs..., Arg&>
		operator << (reference_wrapper<Arg> ref) const;
	template <typename Arg>
	CurriedFunction<Result, Arity, Functor, CurriedArgs..., typename decay<Arg>::type>
		operator << (Arg&& arg) const;
	/*
	 * Returns a new functor, curried with the extra arguments.
	 * It captures by REFERENCE where possible.  Either use static_cast or use
	 * operator<< to capture by value.
	 */
	template <typename... ExtraArgs>
	CurriedFunction<Result, Arity, Functor, CurriedArgs..., ExtraArgs...>
		apply (ExtraArgs&&... extra_args) const;
	/* Calls the function */
	template <size_t Arity_ = Arity>
	typename enable_if<(sizeof...(CurriedArgs) == Arity_), Result>::type
	invoke() const;
	/* Null-pointer comparison */
	bool operator == (nullptr_t) const;
	bool operator != (nullptr_t) const;
private:
	template <size_t NumArgs>
	void statically_check_args_count_for_invoke() const;
	Functor func;
	ArgsTuple curried_args;
};

}

template <typename T>
struct is_curried_function {
private:
	template <typename U>
	static integral_constant<bool, U::is_curried_function> check(int);
	template <typename>
	static std::false_type check(...);
public:
	static constexpr auto value = decltype(check<T>(0))::value;
};

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

#ifdef enable_monads
/***
 * Monad operator
 *
 * Adds operator such that;
 *   "T&& t , U(T) u" ⇒ "u(t)"
 *
 * Giving us this syntax for chain operations:
 *   "T&& t , U(T) u , V(U) v" ⇒ "v(u(t))"     
 *
 * Given the requirements for left side <t> and right-side <u>:
 *  - <u> is a curry-wrapped function
 *  - <u> takes one parameter (after any currying)
 *
 * You MUST compile with '-Wunused-value', in order to detect when the operator
 * has not been chosen by the compiler (since a no-op default will silently be
 * used in its place, triggering an unused-value warning)
 */
template <typename From, typename To,
	typename DFrom = typename decay<From>::type,
	typename DTo = typename decay<To>::type>
typename enable_if<
	is_curried_function<DTo>::value &&
	DTo::arity == 1,
		typename DTo::result_type>::type
	operator ,(From&& from, To to);
#endif

}

#ifndef functional_tcc
#include "functional.tcc"
#endif
