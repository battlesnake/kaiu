#pragma once
#include <functional>
#include <type_traits>
#include "event_loop.h"
#include "promise.h"
#include "functional.h"

namespace kaiu {


namespace promise {

/*** Task factories ***/

/*
 * Convert a function or a promise factory into a task factory
 *
 * A task is a promise factory, with extra behaviour:
 *
 * When invoked, a task performs its work in a specified thread pool, then
 * resolves/rejects its promise either in the same thread, or in some specified
 * thread pool.
 *
 * For example, we could take synchronous disk I/O functions and convert them to
 * tasks such that when called, they execute their I/O operation(s) in an I/O
 * worker thread, then execute their resolve/reject callback in a reactor
 * thread.
 *
 * Likewise, if we have 100+ promise factories that encapsulate database
 * queries, and which all call the same promise factory to execute their query,
 * we could convert that to a task, causing all queries to be executed in a
 * network I/O worker thread, and then to execute their callbacks in threads
 * from the calculation pool.
 *
 * The complexity of submitting an action to a thread pool, and deciding which
 * thread pool the reaction should occur in is completely hidden when using
 * tasks.  Simply convert your function or promise factory to a task and the
 * code that calls it should not need altering at all (if it is thread-safe).
 *
 * You can bind (curry) an argument to a promise factory, returning a new
 * promise factory like so:
 *
 *   int logger_func(const string level, const string message);
 *
 *   auto logger_unbound = task(logger_func, action_pool, reaction_pool);
 *   logger_unbound(some_event_loop, "debug", "some debug output");
 *   ...
 *   auto logger = logger_unbound << loop;
 *   logger("info", "some information");
 *   ...
 *   auto error_logger = logger << "error";
 *   error_logger("error_output");
 *   ...
 *   auto ping_logger = logger << "info" << "ping!";
 *   ping_logger();
 */

template <typename Result, typename... Args>
using UnboundTask = Curried<Promise<Result>, sizeof...(Args) + 1, Factory<Result, EventLoop&, Args...>>;

template <typename Result, typename... Args>
using BoundTask = Curried<Promise<Result>, sizeof...(Args) + 1, Factory<Result, EventLoop&, Args...>, EventLoop&>;

/* Parameter is a promise factory, result is a task */

template <typename Result, typename... Args>
UnboundTask<Result, Args...> task(
	Factory<Result, Args...> factory,
	const EventLoopPool action_pool,
	const EventLoopPool reaction_pool = EventLoopPool::same);

/* Parameter is a function, result is a task */

template <typename Result, typename... Args>
UnboundTask<Result, Args...> dispatchable(
	std::function<Result(Args...)> func,
	const EventLoopPool action_pool,
	const EventLoopPool reaction_pool = EventLoopPool::same)
		{ return task(factory(func), action_pool, reaction_pool); }

template <typename Result, typename... Args>
UnboundTask<Result, Args...> dispatchable(
	Result (&func)(Args...),
	const EventLoopPool action_pool,
	const EventLoopPool reaction_pool = EventLoopPool::same)
		{ return task(factory(func), action_pool, reaction_pool); }

/* Parameter is a function, it is task-wrapped and immediately executed */

template <typename Result, typename... Args>
Promise<Result> dispatch(
	std::function<Result(Args...)> func,
	const EventLoopPool action_pool,
	const EventLoopPool reaction_pool,
	EventLoop& loop,
	Args&&... args)
		{ return task(factory(func), action_pool, reaction_pool)
			(loop, std::forward<Args>(args)...); }

template <typename Result, typename... Args>
Promise<Result> dispatch(
	Result (&func)(Args...),
	const EventLoopPool action_pool,
	const EventLoopPool reaction_pool,
	EventLoop& loop,
	Args&&... args)
		{ return task(factory(func), action_pool, reaction_pool)
			(loop, std::forward<Args>(args)...); }

}

namespace task_monad {

/***
 * Task monad operators
 *
 *
 * Task1&& t | Task2&& u, ...
 *
 * Requires:
 *  - t, u are curried promise factories
 *  - t has arity 0
 *  - u has arity 1
 *
 *
 * Promise&& t | Task&& u, ...
 *
 * Requires:
 *  - u is Promise
 *  - u is a curried promise factory
 *  - u has arity 1
 */

template <typename From, typename To,
	typename DFrom = typename std::decay<From>::type,
	typename DTo = typename std::decay<To>::type>
typename std::enable_if<
	is_curried_function<DFrom>::value &&
	is_curried_function<DTo>::value &&
	DFrom::arity == 0 &&
	DTo::arity == 1 &&
	is_promise<typename DFrom::result_type>::value &&
	is_promise<typename DTo::result_type>::value,
		typename DFrom::result_type>::type
operator | (From&& l, To&& r)
{
	return l()->then(std::forward<To>(r));
}

template <typename From, typename To,
	typename DFrom = typename std::decay<From>::type,
	typename DTo = typename std::decay<To>::type>
typename std::enable_if<
	is_promise<DFrom>::value &&
	is_curried_function<DTo>::value &&
	DTo::arity == 1 &&
	is_promise<typename DTo::result_type>::value,
		typename DTo::result_type>::type
operator | (From&& l, To&& r)
{
	return l->then(std::forward<To>(r));
}

}

}

#ifndef task_tcc
#include "task.tcc"
#endif
