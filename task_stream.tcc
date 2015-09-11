#define task_stream_tcc
#include "promise_stream.h"
#include "task_stream.h"

namespace kaiu {


/*** AsyncPromiseStreamState ***/

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
void AsyncPromiseStreamState<Result, Datum>::call_data_callback(Datum datum)
{
	auto functor = [this, datum = std::move(datum)] () mutable {
		PromiseStreamState<Result, Datum>::call_data_callback(std::move(datum));
	};
	auto wrapper = detail::make_shared_functor(std::move(functor));
	loop.push(stream_pool, wrapper);
}

template <typename Result, typename Datum>
auto AsyncPromiseStreamState<Result, Datum>::resolve_completer(Result result)
	-> completer_func
{
	auto functor = PromiseStreamState<Result, Datum>::bind_resolve(std::move(result));
	return [this, functor = std::move(functor)] {
		loop.push(react_pool, functor);
	};
}

template <typename Result, typename Datum>
auto AsyncPromiseStreamState<Result, Datum>::reject_completer(std::exception_ptr error)
	-> completer_func
{
	auto functor = PromiseStreamState<Result, Datum>::bind_reject(error);
	return [this, functor = std::move(functor)] {
		loop.push(react_pool, functor);
	};
}

/*** AsyncPromiseStream ***/

template <typename Result, typename Datum>
AsyncPromiseStream<Result, Datum>::AsyncPromiseStream(EventLoop& loop,
	const EventLoopPool stream_pool,
	const EventLoopPool react_pool) :
		PromiseStream<Result, Datum>(
			std::make_shared<AsyncPromiseStreamState<Result, Datum>>(loop, stream_pool, react_pool))
{
}

/*** Utils ***/

namespace promise {

template <typename Result, typename Datum, typename... Args>
UnboundTaskStream<Result, Datum, Args...> task_stream(
	StreamFactory<Result, Datum, Args...> factory,
	const EventLoopPool producer_pool,
	const EventLoopPool consumer_pool,
	const EventLoopPool reaction_pool)
{
	auto newFactory = [factory, producer_pool, consumer_pool, reaction_pool]
		(EventLoop& loop, Args&&... args)
	{
		PromiseStream<Result, Datum> stream;
		/* Consumer */
		auto consumer = [stream, consumer_pool, &loop]
			(Datum datum) -> Promise<StreamAction>
		{
			Promise<StreamAction> consumer_action;
			auto proxy = [stream, consumer_action, datum = std::move(datum)]
				(EventLoop&) mutable -> void
			{
				try {
					stream->write(std::move(datum));
				} catch (...) {
					consumer_action->reject(std::current_exception());
					return;
				}
				consumer_action->resolve(stream->data_action());
			};
			loop.push(consumer_pool, detail::make_shared_functor(proxy));
			return consumer_action;
		};
		/* Producer */
		auto producer = [stream, producer_pool, &loop,
			factory, args..., consumer, reaction_pool]
			(EventLoop&) mutable -> void
		{
			auto resolve = [stream, reaction_pool, &loop] (Result result) -> void
			{
				auto proxy = [stream, result = std::move(result)]
					(EventLoop&) mutable -> void
				{
					stream->resolve(std::move(result));
				};
				loop.push(reaction_pool, detail::make_shared_functor(proxy));
			};
			auto reject = [stream, reaction_pool, &loop] (std::exception_ptr error) -> void
			{
				auto proxy = [stream, error] (EventLoop&) -> void
				{
					stream->reject(error);
				};
				loop.push(reaction_pool, proxy);
			};
			factory(std::forward<Args>(args)...)
				->stream(consumer)
				->then(resolve, reject);
		};
		/* Push production task */
		loop.push(producer_pool, detail::make_shared_functor(producer));
		return stream;
	};
	return curry_wrap<PromiseStream<Result, Datum>, sizeof...(Args) + 1, StreamFactory<Result, Datum, EventLoop&, Args...>>(newFactory);
}

template <typename Result, typename Datum, typename... Args>
UnboundTaskStream<Result, Datum, Args...> task_stream(
	PromiseStream<Result, Datum> (&factory)(Args...),
	const EventLoopPool producer_pool,
	const EventLoopPool consumer_pool,
	const EventLoopPool reaction_pool)
{
	return task_stream(
		StreamFactory<Result, Datum, Args...>{factory},
		producer_pool, consumer_pool, reaction_pool);
}

}

}
