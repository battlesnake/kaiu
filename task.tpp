#pragma once
#include "task.h"

namespace mark {

using namespace std;

template <typename ResultPromise, typename... Args>
Task<ResultPromise, Args...>::Task(const EventLoopPool pool, Target& func)
	: pool(pool), func(func)
{
}

template <typename ResultPromise, typename... Args>
	template <typename>
		void Task<ResultPromise, Args...>::operator ()(
			AbstractEventLoop& loop, Args&&... args)
{
	auto functor = bind(
		forward<Target>(func),
		placeholders::_1,
		forward<Args>(args)...
	);
	loop.push(pool, functor);
}

template <typename ResultPromise, typename... Args>
	template <typename>
		ResultPromise Task<ResultPromise, Args...>::operator ()(
			AbstractEventLoop& loop, Args&&... args)
{
	auto functor = bind(
		forward<Target>(func),
		placeholders::_1,
		forward<Args>(args)...
	);
	ResultPromise promise;
	auto action = [functor, promise] (AbstractEventLoop& loop) {
		try {
			auto result = functor(loop);
			loop.push(EventLoopPool::reactor,
				[promise, result] (AbstractEventLoop& loop) {
					promise.resolve(result);
				});
		} catch (exception& error) {
			loop.push(EventLoopPool::reactor,
				[promise, error] (AbstractEventLoop& loop) {
					promise.reject(error);
				});
		}
	};
	loop.push(pool, action);
	return promise;
}

template class Task<void, int, long, bool>;

}
