#define task_tcc
#include "task.h"

namespace kaiu {

namespace promise {

template <typename Result, typename... Args>
UnboundTask<Result, Args...> task(
	Factory<Result, Args...> factory,
	const EventLoopPool action_pool,
	const EventLoopPool reaction_pool)
{
	auto newFactory = [factory, action_pool, reaction_pool]
		(EventLoop& loop, Args... args) {
		Promise<Result> promise;
		auto action = [factory, promise, reaction_pool, args...]
			(EventLoop& loop) {
			if (reaction_pool == EventLoopPool::same || reaction_pool == ParallelEventLoop::current_pool()) {
				factory(args...)->forward_to(promise);
			} else {
				auto result = factory(args...);
				auto reaction = [result, promise] (auto& loop) {
					result->forward_to(promise);
				};
				loop.push(reaction_pool, reaction);
			}
		};
		loop.push(action_pool, action);
		return promise;
	};
	return Curry<Promise<Result>, sizeof...(Args) + 1, Factory<Result, EventLoop&, Args...>>(newFactory);
}

}

#ifdef enable_task_monads
/*** Task monad operators ***/

template <typename From, typename To,
	typename DFrom = typename decay<From>::type,
	typename DTo = typename decay<To>::type>
typename enable_if<
	is_curried_function<DFrom>::value &&
	is_curried_function<DTo>::value &&
	DFrom::arity == 0 &&
	DTo::arity == 1 &&
	is_promise<typename DFrom::result_type>::value &&
	is_promise<typename DTo::result_type>::value,
		typename DFrom::result_type>::type
operator | (From&& l, To&& r)
{
	return l()->then(forward<To>(r));
}

template <typename From, typename To,
	typename DFrom = typename decay<From>::type,
	typename DTo = typename decay<To>::type>
typename enable_if<
	is_promise<DFrom>::value &&
	is_curried_function<DTo>::value &&
	DTo::arity == 1 &&
	is_promise<typename DTo::result_type>::value,
		typename DTo::result_type>::type
operator | (From&& l, To&& r)
{
	return l->then(forward<To>(r));
}
#endif

}
