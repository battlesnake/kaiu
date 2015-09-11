#pragma once

namespace kaiu {


template <typename T>
using remove_cvr = std::remove_reference<typename std::remove_cv<T>::type>;

/***
 * Test if a type represents a promise
 *
 * Example usage: enable_if<is_promise<T>::value>::type
 *
 * Could use is_base_of with PromiseBase (a since deleted base class), this way
 * seems more idiomatic.
 */

template <typename T>
struct is_promise : std::false_type { };

template <typename T>
struct is_promise<Promise<T>> : std::true_type { };

namespace detail {
	template <bool, typename T> struct result_of_promise_helper { };
	template <typename T> struct result_of_promise_helper<true, T>
		{ using type = typename T::result_type; };
	template <bool, typename T> struct result_of_not_promise_helper { };
	template <typename T> struct result_of_not_promise_helper<false, T>
		{ using type = T; };
}

/* result_of_promise<T>: is_promise<T> ? T::result_type : fail */
template <typename T>
using result_of_promise = detail::result_of_promise_helper<is_promise<T>::value, T>;

/* result_of_not_promise<T>: is_promise<T> ? fail : T */
template <typename T>
using result_of_not_promise = detail::result_of_not_promise_helper<is_promise<T>::value, T>;

/* is_promise<T> && R==T::result_type */
template <typename T, typename R>
struct result_of_promise_is {
private:
	template <typename U>
	static std::is_same<typename result_of_promise<U>::type, typename std::remove_cv<R>::type> check(int);
	template <typename>
	static std::false_type check(...);
public:
	static constexpr auto value = decltype(check<T>(0))::value;
};

/* !is_promise<T> && R==T */
template <typename T, typename R>
struct result_of_not_promise_is {
private:
	template <typename U>
	static std::is_same<typename result_of_not_promise<U>::type, typename std::remove_cv<R>::type> check(int);
	template <typename>
	static std::false_type check(...);
public:
	static constexpr auto value = decltype(check<T>(0))::value;
};

/* Is callback pack */
template <typename T>
struct is_callback_pack : std::false_type { };

template <typename Range, typename Domain>
struct is_callback_pack<promise::callback_pack<Range, Domain>> : std::true_type { };

/* Is callback pack terminal */
template <typename T>
struct is_terminal_callback_pack :
	std::integral_constant<bool, is_callback_pack<T>::value &&
		std::is_void<typename T::range_type>::value> { };

template <typename T>
using is_promise_like = std::is_base_of<PromiseLike, T>;

}
