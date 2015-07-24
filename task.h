#pragma once
#include <functional>
#include "event_loop.h"
#include "promise.h"
#include "functional.h"

namespace mark {

using namespace std;

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

/* Parameter is a promise factory */
template <typename Result, typename... Args>
UnboundTask<Result, Args...> task(
	Factory<Result, Args...> factory,
	const EventLoopPool action_pool,
	const EventLoopPool reaction_pool = EventLoopPool::same);

}

}

#ifndef task_tcc
#include "task.tcc"
#endif