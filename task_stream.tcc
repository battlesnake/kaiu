#define task_stream_tcc
#include "promise_stream.h"
#include "task_stream.h"

namespace kaiu {

using namespace std;

template <typename Result, typename Datum>
AsyncPromiseStreamState<Result, Datum>::AsyncPromiseStreamState(
	EventLoop& loop,
	const EventLoopPool stream_pool,
	const EventLoopPool react_pool) :
		loop(loop), stream_pool(stream_pool), react_pool(react_pool),
		PromiseStreamState<Result, Datum>()
{
}

template <typename Result, typename Datum>
void AsyncPromiseStreamState<Result, Datum>::call_data_callback(Datum& datum)
{
	auto functor = [this, datum = move(datum)] () mutable {
		PromiseStreamState<Result, Datum>::call_data_callback(datum);
	};
	auto wrapper = detail::shared_functor<decltype(functor)>(functor);
	loop.push(stream_pool, wrapper);
}

template <typename Result, typename Datum>
function<void()> AsyncPromiseStreamState<Result, Datum>::bind_resolve(
	Result&& result)
{
	auto functor = PromiseStreamState<Result, Datum>::bind_resolve(move(result));
	return [this, functor] {
		loop.push(react_pool, functor);
	};
}

template <typename Result, typename Datum>
function<void()> AsyncPromiseStreamState<Result, Datum>::bind_reject(
	exception_ptr error)
{
	auto functor = PromiseStreamState<Result, Datum>::bind_reject(error);
	return [this, functor] {
		loop.push(react_pool, functor);
	};
}

template <typename Result, typename Datum>
AsyncPromiseStream<Result, Datum>::AsyncPromiseStream(EventLoop& loop,
	const EventLoopPool stream_pool,
	const EventLoopPool react_pool) :
		PromiseStream<Result, Datum>(
			make_shared<AsyncPromiseStreamState<Result, Datum>>(loop, stream_pool, react_pool))
{
}

/*** Utils ***/

namespace promise {

template <typename Result, typename Datum, typename... Args>
UnboundTaskStream<Result, Datum, Args...> task_stream(
	StreamFactory<Result, Datum, Args...> factory,
	const EventLoopPool action_pool,
	const EventLoopPool stream_pool,
	const EventLoopPool reaction_pool)
{
	auto newFactory = [factory, action_pool, stream_pool, reaction_pool]
		(EventLoop& loop, Args... args) {
		PromiseStream<Result, Datum> stream;
		auto action = [factory, stream, stream_pool, reaction_pool, args...]
			(EventLoop& loop) {
			auto resolve = [stream, reaction_pool, &loop] (Result& result) {
				auto proxy = [stream, result = move(result)] (EventLoop&) mutable {
					stream->resolve(move(result));
				};
				loop.push(reaction_pool, detail::make_shared_functor(proxy));
			};
			auto reject = [stream, reaction_pool, &loop] (exception_ptr error) {
				auto proxy = [stream, error] (EventLoop&) {
					stream->reject(error);
				};
				loop.push(reaction_pool, proxy);
			};
			auto data = [stream, stream_pool, &loop, resolve, reject] (Datum& datum) {
				auto proxy = [stream, datum = move(datum)] (EventLoop&) mutable {
					stream->write(move(datum));
				};
				loop.push(stream_pool, detail::make_shared_functor(proxy));
			};
			factory(args...)
				->template stream<void>(data)
				->then(resolve, reject);
		};
		loop.push(action_pool, action);
		return stream;
	};
	return Curry<Promise<Result>, sizeof...(Args) + 1, Factory<Result, EventLoop&, Args...>>(newFactory);
}

template <typename Result, typename Datum, typename... Args>
UnboundTaskStream<Result, Datum, Args...> task_stream(
	PromiseStream<Result, Datum> (&factory)(Args...),
	const EventLoopPool action_pool,
	const EventLoopPool stream_pool,
	const EventLoopPool reaction_pool)
{
	return task_stream(
		StreamFactory<Result, Datum, Args...>{factory},
		action_pool, stream_pool, reaction_pool);
}

}

}
