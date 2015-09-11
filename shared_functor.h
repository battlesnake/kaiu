#pragma once
#include <functional>
#include <memory>
#include <type_traits>
#include <atomic>

#if defined(DEBUG)
#define SAFE_SHARED_FUNCTORS
#endif

namespace kaiu {


namespace detail {

/*
 * Allows us to capture a non-copyable in a lambda, then cast the lambda to
 * std::function.
 *
 * If DEBUG or SAFE_SHARED_FUNCTORS is defined it also enforces that the
 * callable can only be called once, and will throw an exception if it is called
 * subsequent times.
 */
template <typename F>
class shared_functor {
private:
	struct functor_state {
#if defined(SAFE_SHARED_FUNCTORS)
		std::atomic_flag called{ATOMIC_FLAG_INIT};
#endif
		F ptr;
		functor_state(F&& f) : ptr(std::move(f)) { }
	};
	std::shared_ptr<functor_state> state;
public:
	explicit shared_functor(F&& f) :
		state(std::make_shared<functor_state>(std::move(f)))
			{ }
	template <typename... Args>
	void operator () (Args&&... args)
	{
#if defined(SAFE_SHARED_FUNCTORS)
		if (state->called.test_and_set(std::memory_order_release)) {
			throw std::logic_error("Shared functor called more than once");
		}
#endif
		state->ptr(std::forward<Args>(args)...);
	}
};

template <typename F>
shared_functor<typename std::decay<F>::type> make_shared_functor(F&& functor)
{
	return shared_functor<typename std::decay<F>::type>(std::move(functor));
}

}

}
