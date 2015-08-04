#pragma once
#include "functional.h"
#include "event_loop.h"
#include "promise_stream.h"

namespace kaiu {

using namespace std;

/*
 * PromiseStream which calls callbacks (data/resolve/reject) in specified
 * threads in a thread pool
 *
 * This class is the reason that PromiseStreamState has a vtable, and if this
 * class doesn't look to have much value then I may remove it and revert
 * PromiseStreamState back to having no vtable.  Performance difference will be
 * negligible, the motivation is just for keeping things simple and minimal.
 */

template <typename Result, typename Datum>
class AsyncPromiseStreamState : public PromiseStreamState<Result, Datum> {
public:
	AsyncPromiseStreamState() = delete;
	AsyncPromiseStreamState(EventLoop& loop,
		const EventLoopPool stream_pool,
		const EventLoopPool react_pool = EventLoopPool::same);
	virtual ~AsyncPromiseStreamState() = default;
protected:
	virtual void call_data_callback(Datum&) override;
	virtual function<void()> bind_resolve(Result&& result) override;
	virtual function<void()> bind_reject(exception_ptr error) override;
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
	const EventLoopPool action_pool,
	const EventLoopPool stream_pool,
	const EventLoopPool reaction_pool = EventLoopPool::same);

template <typename Result, typename Datum, typename... Args>
UnboundTaskStream<Result, Datum, Args...> task_stream(
	PromiseStream<Result, Datum> (&factory)(Args...),
	const EventLoopPool action_pool,
	const EventLoopPool stream_pool,
	const EventLoopPool reaction_pool = EventLoopPool::same);

}

}

#ifndef task_stream_tcc
#include "task_stream.tcc"
#endif
