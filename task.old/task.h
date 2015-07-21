#pragma once
#include <functional>
#include <type_traits>
#include "event_loop.h"
#include "promise.h"

namespace mark {

using namespace std;

class EventLoop;

/*
 * A Task<Result, Args...> defines an asynchronous process which transforms
 * Args... to Result in a specified thread-pool.  The callbacks executed upon
 * completion (or rejection) of the task can be executed immediately after in
 * the same thread, or can be executed in some other thread-pool.  Specification
 * of the process, action thread-pool, and reaction thread-pool is done by the
 * Task<Result, Args...> instance at construction.
 *
 * The function operator is used to execute the task on a given message loop
 * with a given set of arguments.  The make_factory method can be used to curry
 * the function operator, binding to a particular message loop.  This results in
 * a Promise<Result> factory which takes Args... as parameters.
 *
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
	Task(
		Result (&action)(EventLoop&, Args...),
		const EventLoopPool pool,
		const EventLoopPool react_in = EventLoopPool::invalid);
	/* Invoke task */
	Promise<Result> operator ()(EventLoop& loop, Args&&... args) const;
	/* Returns promise factory that encapsulates this task */
	template <typename... Binding>
	function<Promise<Result>(Args...)>
		make_factory(EventLoop& loop, Binding&&... binding) const;
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
