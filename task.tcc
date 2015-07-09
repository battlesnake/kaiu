#pragma once
#include "task.h"

namespace mark {

using namespace std;

template <typename Result, typename... Args>
Task<Result, Args...>::Task(const EventLoopPool pool, Action& action)
	: pool(pool), action(action)
{
}

template <typename Result, typename... Args>
Task<Result, Args...>::Task(const EventLoopPool pool, Action&& action)
	: pool(pool), action(move(action))
{
}

template <typename Result, typename... Args>
auto Task<Result, Args...>::
	operator ()(AbstractEventLoop& loop, Args&&... args) -> ResultPromise
{
	auto functor = bind(
		forward<Action>(action),
		placeholders::_1,
		forward<Args>(args)...
	);
	ResultPromise promise;
	auto action = [functor, promise] (AbstractEventLoop& loop) {
		functor(loop)->forward_to(promise);
	};
	loop.push(pool, action);
	return promise;
}

template class Task<bool, int, long, bool>;

}
