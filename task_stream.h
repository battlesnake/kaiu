#pragma once
#include "functional.h"
#include "event_loop.h"
#include "task.h"
#include "promise_stream.h"

namespace kaiu {

using namespace std;

/*
 * PromiseStream which calls callbacks (data/resolve/reject) in specified
 * threads in a thread pool
 */

template <typename Result, typename Datum>
class AsyncPromiseStreamState : public PromiseStreamState<Result, Datum> {
public:
	AsyncPromiseStreamState() = delete;
	AsyncPromiseStreamState(EventLoop& loop,
		const EventLoopPool stream_pool,
		const EventLoopPool react_pool = EventLoopPool::same);
protected:
	virtual void call_data_callback(Datum) override;
	using completer_func = typename PromiseStreamState<Result, Datum>::completer_func;
	virtual completer_func resolve_completer(Result) override;
	virtual completer_func reject_completer(exception_ptr) override;
private:
	EventLoop& loop;
	EventLoopPool stream_pool;
	EventLoopPool react_pool;
};

template <typename Result, typename Datum>
class AsyncPromiseStream : public PromiseStream<Result, Datum> {
public:
	AsyncPromiseStream() = delete;
	AsyncPromiseStream(EventLoop& loop,
		const EventLoopPool stream_pool,
		const EventLoopPool react_pool = EventLoopPool::same);
};

namespace promise {

/*** Promise-stream factories ***/

template <typename Result, typename Datum, typename... Args>
using UnboundTaskStream = Curried<PromiseStream<Result, Datum>, sizeof...(Args) + 1, StreamFactory<Result, Datum, EventLoop&, Args...>>;

template <typename Result, typename Datum, typename... Args>
using BoundTaskStream = Curried<PromiseStream<Result, Datum>, sizeof...(Args) + 1, StreamFactory<Result, Datum, EventLoop&, Args...>, EventLoop&>;

/* Parameter is a promise-stream factory, result is a streaming task */

template <typename Result, typename Datum, typename... Args>
UnboundTaskStream<Result, Datum, Args...> task_stream(
	StreamFactory<Result, Datum, Args...> factory,
	const EventLoopPool producer_pool,
	const EventLoopPool consumer_pool = EventLoopPool::same,
	const EventLoopPool reaction_pool = EventLoopPool::same);

template <typename Result, typename Datum, typename... Args>
UnboundTaskStream<Result, Datum, Args...> task_stream(
	PromiseStream<Result, Datum> (&factory)(Args...),
	const EventLoopPool producer_pool,
	const EventLoopPool consumer_pool = EventLoopPool::same,
	const EventLoopPool reaction_pool = EventLoopPool::same);

}

}

#ifndef task_stream_tcc
#include "task_stream.tcc"
#endif
