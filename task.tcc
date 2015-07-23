#define task_tcc
#include "task.h"

namespace mark {

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
			if (reaction_pool == EventLoopPool::same) {
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

}
