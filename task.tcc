#define task_tcc
#include "shared_functor.h"
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
			auto resolve = [promise, reaction_pool, &loop] (Result result) {
				auto proxy = [promise, result = std::move(result)] (EventLoop&) mutable {
					promise->resolve(std::move(result));
				};
				loop.push(reaction_pool, detail::make_shared_functor(proxy));
			};
			auto reject = [promise, reaction_pool, &loop] (std::exception_ptr error) {
				auto proxy = [promise, error] (EventLoop&) {
					promise->reject(error);
				};
				loop.push(reaction_pool, proxy);
			};
			factory(args...)
				->then(resolve, reject);
		};
		loop.push(action_pool, action);
		return promise;
	};
	return curry_wrap<Promise<Result>, sizeof...(Args) + 1, Factory<Result, EventLoop&, Args...>>(newFactory);
}

}

}
