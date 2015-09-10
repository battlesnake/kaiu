#pragma once

namespace kaiu {

template <typename T>
struct is_promise_stream {
private:
	template <typename U>
	static integral_constant<bool, U::is_promise_stream> check(int);
	template <typename>
	static std::false_type check(...);
public:
	static constexpr auto value = decltype(check<T>(0))::value;
};

}
