#define task_tcc
#include <functional>
#include "task.h"

namespace mark {

using namespace std;

template <typename Result, typename... Args>
Task<Result, Args...>::Task(
	const Action& action,
	const EventLoopPool pool,
	const EventLoopPool react_in) :
	pool(pool), action(action), react_in(react_in)
{
}

template <typename Result, typename... Args>
Promise<Result>Task<Result, Args...>::
	operator ()(EventLoop& loop, Args&&... args) const
{
	if (action == nullptr) {
		throw bad_function_call();
	}
	auto functor = bind(
		action,
		placeholders::_1,
		forward<Args>(args)...
	);
	Promise<Result> promise;
	auto job = [functor, promise, react_in=react_in] (EventLoop& loop) {
		if (react_in == EventLoopPool::invalid) {
			functor(loop)->forward_to(promise);
		} else {
			auto result = functor(loop);
			loop.push(react_in, [result, promise] (auto& loop) {
				result->forward_to(promise);
			});
		}
	};
	loop.push(pool, job);
	return promise;
}

template <typename Result, typename... Args>
function<Promise<Result>(Args...)> Task<Result, Args...>::
	make_factory(EventLoop& loop) const
{
	return [this_task=*this, &loop] (Args&&... args) {
		return this_task(loop, forward<Args>(args)...);
	};
}

namespace task {

template <typename Result, typename... Args>
function<Promise<Result>(Args...)> make_factory(
		EventLoop& loop,
		Result (&action)(EventLoop&, Args...),
		const EventLoopPool pool,
		const EventLoopPool react_in)
{
	const auto wrappedAction = function<Result(EventLoop&, Args...)>(action);
	return Task<Result, Args...>(wrappedAction, pool, react_in).make_factory(loop);
}

template <typename Result, typename... Args>
function<Promise<Result>(Args...)> make_factory(
		EventLoop& loop,
		const function<Result(EventLoop&, Args...)>& action,
		const EventLoopPool pool,
		const EventLoopPool react_in)
{
	return Task<Result, Args...>(action, pool, react_in).make_factory(loop);
}

template <typename Result, typename... Args>
function<Promise<Result>(Args...)> make_factory(
		EventLoop& loop,
		Result (&action)(Args...),
		const EventLoopPool pool,
		const EventLoopPool react_in)
{
	const auto wrappedAction = [action=action] (EventLoop& loop, Args&&... args) {
		return action(forward<Args>(args)...);
	};
	return Task<Result, Args...>(wrappedAction, pool, react_in).make_factory(loop);
}

template <typename Result, typename... Args>
function<Promise<Result>(Args...)> make_factory(
		EventLoop& loop,
		const function<Result(Args...)>& action,
		const EventLoopPool pool,
		const EventLoopPool react_in)
{
	const auto wrappedAction = [action=action] (EventLoop& loop, Args&&... args) {
		return action(forward<Args>(args)...);
	};
	return Task<Result, Args...>(wrappedAction, pool, react_in).make_factory(loop);
}

}

}
