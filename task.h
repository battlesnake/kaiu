#pragma once
#include <functional>
#include <type_traits>
#include "event_loop.h"
#include "promise.h"

namespace mark {

using namespace std;

class EventLoop;

/*
 * A task is a promise factory.  When invoked via the function operator, the
 * task's action is submitted to the specified pool in the passed event loop.
 *
 * The action runs in the thread pool and may optionally invoke other tasks
 * (which could be run in other thread pools or on other event loops).  The
 * promise returned by the action is forwarded to the promise returned by the
 * task's function operator.
 *
 * If react_in was not specified or was 'invalid' then any callbacks attached
 * to the returned promise are executed in the same thread that the task was
 * executed in.
 *
 * If react_in was specified and was not 'invalid', callbacks on the returned
 * promise are executed in the react_in thread pool.
 *
 * make_factory can be used to create a promise factory that takes Args... as
 * parameters, launches the task on a pre-set event loop, and returns a promise.
 * Note that:
 *     make_factory(loop)(args...)
 * is functionally equivalent to:
 *     operator ()(loop, args...)
 */

template <typename Result, typename... Args>
class Task {
public:
	using Action = function<Promise<Result>(EventLoop& loop, Args...)>;
	/* Constructor */
	Task(
		const Action& action,
		const EventLoopPool pool,
		const EventLoopPool react_in = EventLoopPool::invalid);
	/* Invoke task */
	Promise<Result> operator ()(EventLoop& loop, Args&&... args) const;
	/* Returns promise factory that encapsulates this task */
	function<Promise<Result>(Args...)> make_factory(EventLoop& loop) const;
private:
	const EventLoopPool pool;
	const Action action;
	const EventLoopPool react_in;
};

namespace task {

template <typename Result, typename... Args>
function<Promise<Result>(Args...)> make_factory(
		EventLoop& loop,
		Result (&action)(EventLoop&, Args...),
		const EventLoopPool pool,
		const EventLoopPool react_in = EventLoopPool::invalid);

template <typename Result, typename... Args>
function<Promise<Result>(Args...)> make_factory(
		EventLoop& loop,
		const function<Result(EventLoop&, Args...)>& action,
		const EventLoopPool pool,
		const EventLoopPool react_in = EventLoopPool::invalid);

template <typename Result, typename... Args>
function<Promise<Result>(Args...)> make_factory(
		EventLoop& loop,
		Result (&action)(Args...),
		const EventLoopPool pool,
		const EventLoopPool react_in = EventLoopPool::invalid);

template <typename Result, typename... Args>
function<Promise<Result>(Args...)> make_factory(
		EventLoop& loop,
		const function<Result(Args...)>& action,
		const EventLoopPool pool,
		const EventLoopPool react_in = EventLoopPool::invalid);

}

}

#ifndef task_tcc
#include "task.tcc"
#endif
