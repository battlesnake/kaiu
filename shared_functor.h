#pragma once
#include <functional>
#include <memory>
#include <type_traits>

namespace kaiu {

using namespace std;

namespace detail {

/*
 * Allows us to capture a non-copyable in a lambda, then cast the lambda to
 * std::function
 */
template <typename F>
class shared_functor {
public:
	explicit shared_functor(F&& f) : ptr(make_shared<F>(move(f))) { }
	template <typename... Args>
	void operator () (Args&&... args) { (*ptr)(forward<Args>(args)...); }
private:
	shared_ptr<F> ptr;
};

template <typename F>
auto make_shared_functor(F&& functor)
{
	return shared_functor<typename decay<F>::type>(move(functor));
}

}

}
