#pragma once
#include <functional>
#include <tuple>
#include <utility>
#include <type_traits>

namespace kaiu {

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
 * Wraps a function.  Enables currying and partial application.  Currying is
 * done by value via the "<<" operator, and partial application is done by
 * reference (when possible) via the "apply" method.  The functor can be invoked
 * via the "invoke" method or the "()" operator.
 */

template <typename Result, size_t Arity, typename Functor, typename... CurriedArgs>
struct CurriedFunction {
private:
	using ArgsTuple = tuple<CurriedArgs...>;
public:
	static const auto is_curried_function = true;
	using result_type = Result;
	using functor_type = Functor;
	static constexpr auto arity = Arity - sizeof...(CurriedArgs);
	CurriedFunction(Functor func);
	CurriedFunction(Functor func, const ArgsTuple& curried_args);
	/*
	 * Function operator ALWAYS invokes, to partially apply use the "apply"
	 * method or the curry "<<" operator.  A previous implementation permitted
	 * partial application using the function operator until sufficient
	 * arguments were available to invoke, but that could lead to difficult
	 * debugging scenarios involving 100+line template errors, so I separated
	 * "apply" and "invoke" into two different syntaxes.
	 *
	 * operator ()(args): same as .apply(args).invoke()
	 */
	template <typename... ExtraArgs>
	typename enable_if<(sizeof...(ExtraArgs) > 0), Result>::type
	operator () (ExtraArgs&&... extra_args) const;
	/* With no parameters, same as .invoke() */
	template <typename... ExtraArgs>
	typename enable_if<(sizeof...(ExtraArgs) == 0), Result>::type
	operator () (ExtraArgs&&... extra_args) const;
	/*
	 * Left-shift operator to curry
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
	 * Partial application, returns a new functor, with the extra arguments
	 * bound.  It captures by REFERENCE where possible.  To capture by value,
	 * either use static_cast, remove_reference, decay, or operator <<.
	 */
	template <typename... ExtraArgs>
	CurriedFunction<Result, Arity, Functor, CurriedArgs..., ExtraArgs...>
		apply (ExtraArgs&&... extra_args) const;
	/* Calls the function */
	template <size_t Arity_ = Arity>
	typename enable_if<(sizeof...(CurriedArgs) == Arity_), Result>::type
	invoke() const;
	/* Always returns false */
	bool operator == (nullptr_t) const;
	/* Always returns true */
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

/* Curry-wrap std::function */
template <typename Result, typename... Args>
const auto curry_wrap(function<Result(Args...)> functor)
{
	return Curried<Result, sizeof...(Args), decltype(functor)>(functor);
}

/* Curry-wrap plain function */
template <typename Result, typename... Args>
const auto curry_wrap(Result (&functor)(Args...))
{
	return Curried<Result, sizeof...(Args), decltype(functor)>(functor);
}

/* Curry-wrap functor */
template <typename Result, size_t Arity, typename Functor>
const auto curry_wrap(Functor functor)
{
	return Curried<Result, Arity, Functor>(functor);
}

/* Call a function using arguments stored in a tuple */
template <typename Result, typename Functor, typename Args>
Result invoke(Functor func, Args args);

namespace functional_chain {

/***
 * Bind operator
 *
 * This is a horrible idea, added only for fun.  Please DO NOT use it in
 * production!  Or change the operator to any left-to-right operator that isn't
 * the comma!
 *
 * Adds operator such that;
 *   "T t , U(T) u" ⇒ "u(t)"
 *
 * Giving us this syntax for chain operations:
 *   "T t , U(T) u , V(U) v" ⇒ "v(u(t))"
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
operator ,(From&& from, To to)
{
	return to(forward<From>(from));
}

/* Disable comma operator if we think you've made a mistake */
template <typename From, typename To,
	typename DFrom = typename decay<From>::type,
	typename DTo = typename decay<To>::type>
typename enable_if<
	is_curried_function<DTo>::value &&
	DTo::arity != 1,
		typename DTo::result_type>::type
operator ,(From&& from, To to)
{
	static_assert(DTo::arity == 1, "Functional bind cannot be implemented: Functor has arity != 1");
}

}

}

#ifndef functional_tcc
#include "functional.tcc"
#endif
