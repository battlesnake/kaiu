#define promise_stream_tcc
#include <exception>
#include <stdexcept>
#include "shared_functor.h"
#include "promise_stream.h"

namespace kaiu {

using namespace std;

/*** PromiseStreamState ***/

/* Resolve / reject the stream */

template <typename Result, typename Datum>
void PromiseStreamState<Result, Datum>::resolve(Result&& result)
{
	lock_guard<mutex> lock(mx);
	do_resolve(lock, forward<Result>(result));
}

template <typename Result, typename Datum>
void PromiseStreamState<Result, Datum>::reject(exception_ptr error)
{
	lock_guard<mutex> lock(mx);
	do_reject(lock, error, false);
}

template <typename Result, typename Datum>
void PromiseStreamState<Result, Datum>::reject(const string& error)
{
	lock_guard<mutex> lock(mx);
	do_reject(lock, make_exception_ptr(runtime_error(error)), false);
}

template <typename Result, typename Datum>
template <typename... Args>
void PromiseStreamState<Result, Datum>::write(Args&&... args)
{
	{
		lock_guard<mutex> lock(mx);
		if (get_action(lock) == StreamAction::Continue) {
			emplace_data(lock, forward<Args>(args)...);
			set_written_to(lock);
		}
	}
	process_data();
}

template <typename Result, typename Datum>
template <typename... Args>
void PromiseStreamState<Result, Datum>::emplace_data(ensure_locked, Args&&... args)
{
	buffer.emplace(forward<Args>(args)...);
}

template <typename Result, typename Datum>
bool PromiseStreamState<Result, Datum>::has_data(ensure_locked) const
{
	return buffer.size() > 0;
}

template <typename Result, typename Datum>
void PromiseStreamState<Result, Datum>::set_data_callback(DataFunc data_callback)
{
	{
		lock_guard<mutex> lock(mx);
#if defined(DEBUG)
		if (on_data != nullptr) {
			throw logic_error("Callbacks are already assigned");
		}
		if (data_callback == nullptr) {
			throw logic_error("Attempted to bind null callback");
		}
#endif
		on_data = data_callback;
	}
	set_data_callback_assigned();
}

template <typename Result, typename Datum>
Promise<Result> PromiseStreamState<Result, Datum>::
	do_stream(stream_consumer consumer)
{
	auto data = [consumer] (Datum& datum) {
		try {
			return consumer(datum);
		} catch (...) {
			return promise::rejected<StreamAction>(current_exception());
		}
	};
	set_data_callback(data);
	return proxy_promise;
}

template <typename Result, typename Datum>
template <typename State, typename... Args>
Promise<pair<State, Result>> PromiseStreamState<Result, Datum>::
	do_stateful_stream(stateful_stream_consumer<State> consumer, Args&&... args)
{
	auto state = make_shared<State>(forward<Args>(args)...);
	auto consumer_proxy = [this, consumer, state] (Datum& datum) {
		return consumer(*state, datum);
	};
	auto next_proxy = [this, state] (Result& result) {
		return make_pair(move(*state), move(result));
	};
	return do_stream(consumer_proxy)
		->then(next_proxy);
}

template <typename Result, typename Datum>
Promise<Result> PromiseStreamState<Result, Datum>::
	always(ensure_locked lock, const StreamAction action)
{
	auto consumer = [action] (Datum& datum) {
		return promise::resolved(action);
	};
	return do_stream(lock, consumer);
}

template <typename Result, typename Datum>
void PromiseStreamState<Result, Datum>::forward_to(PromiseStream<Result, Datum> next)
{
	this
		->stream<void>([next] (Datum& datum) {
			next->write(move(datum));
			return next->data_action();
		})
		->forward_to(next);
}

template <typename Result, typename Datum>
void PromiseStreamState<Result, Datum>::forward_to(Promise<Result> next)
{
	this
		->discard()
		->forward_to(next);
}

template <typename Result, typename Datum>
void PromiseStreamState<Result, Datum>::process_data()
{
	Datum datum;
	{
		lock_guard<mutex> lock(mx);
		auto state = get_state(lock);
		if (state != stream_state::streaming1 && state != stream_state::streaming2) {
			return;
		}
		if (on_data == nullptr) {
			return;
		}
		if (is_consumer_running(lock)) {
			return;
		}
		if (get_action(lock) != StreamAction::Continue) {
			decltype(buffer)().swap(buffer);
		}
		if (!has_data(lock)) {
			update_state(lock);
			return;
		}
		datum = move(buffer.front());
		buffer.pop();
		set_consumer_running(lock, true);
	}
	/*
	 * Thread-safe, as we (within the previous lock) check whether on_data has
	 * been assigned yet, and it can not be re-assigned once set.  We also check
	 * and set consumer_running, so the data callback can not be called
	 * concurrently.
	 */
	call_data_callback(datum);
}

template <typename Result, typename Datum>
void PromiseStreamState<Result, Datum>::call_data_callback(Datum& datum) try {
	/* Assumes consumer_running has been set to true and mutex is not locked */
	/*
	 * Asynchronous on_data callbacks will obviously not trigger the catch block
	 * if they throw.  Synchronous on_data callbacks also will not, as the
	 * promise's "handler" eats the exception.
	 */
	on_data(datum)
		->then(
			[this] (const StreamAction action) {
				{
					lock_guard<mutex> lock(mx);
					set_action(lock, action);
					set_consumer_running(lock, false);
				}
				process_data();
			},
			[this] (exception_ptr error) {
				lock_guard<mutex> lock(mx);
				do_reject(lock, error, true);
				set_consumer_running(lock, false);
			});
} catch (...) {
	lock_guard<mutex> lock(mx);
	set_consumer_running(lock, false);
	/* Rethrowing is implicit since this is a function-try-catch block */
	throw;
}

template <typename Result, typename Datum>
void PromiseStreamState<Result, Datum>::do_resolve(ensure_locked lock, Result&& result)
{
	set_completer(lock, stream_result::resolved, resolve_completer(move(result)));
	update_state(lock);
}

template <typename Result, typename Datum>
void PromiseStreamState<Result, Datum>::do_reject(ensure_locked lock, exception_ptr error, bool consumer_failed)
{
	set_completer(lock, consumer_failed ? stream_result::consumer_failed : stream_result::rejected, reject_completer(error));
	set_action(lock, StreamAction::Stop);
	update_state(lock);
}

template <typename Result, typename Datum>
auto PromiseStreamState<Result, Datum>::resolve_completer(Result&& result)
	-> completer_func
{
	auto functor = [this, result = move(result)] (ensure_locked lock) mutable {
		proxy_promise->resolve(move(result));
	};
	return detail::shared_functor<decltype(functor)>(move(functor));
}

template <typename Result, typename Datum>
auto PromiseStreamState<Result, Datum>::reject_completer(exception_ptr error)
	-> completer_func
{
	return [this, error] (ensure_locked) {
		proxy_promise->reject(error);
	};
}

/* Stateless consumer returning promise */
template <typename Result, typename Datum>
template <typename, typename Consumer>
auto PromiseStreamState<Result, Datum>::stream(Consumer consumer)
	-> stream_sel<Consumer, result_of_promise_is, Result, Datum&>
{
	lock_guard<mutex> lock(mx);
	return do_stream(consumer);
}

/* Stateless consumer returning action */
template <typename Result, typename Datum>
template <typename, typename Consumer>
auto PromiseStreamState<Result, Datum>::stream(Consumer consumer)
	-> stream_sel<Consumer, result_of_not_promise_is, Result, Datum&>
{
	auto data_proxy = [consumer] (Datum& datum) {
		return promise::resolved<StreamAction>(consumer(datum));
	};
	return do_stream(data_proxy);
}

/* Stateful consumer returning promise */
template <typename Result, typename Datum>
template <typename State, typename Consumer, typename... Args>
auto PromiseStreamState<Result, Datum>::stream(Consumer consumer, Args&&... args)
	-> stream_sel<Consumer, result_of_promise_is, pair<State, Result>, State&, Datum&>
{
	return do_stateful_stream<State, Args...>(consumer, forward<Args>(args)...);
}

/* Stateful consumer returning action */
template <typename Result, typename Datum>
template <typename State, typename Consumer, typename... Args>
auto PromiseStreamState<Result, Datum>::stream(Consumer consumer, Args&&... args)
	-> stream_sel<Consumer, result_of_not_promise_is, pair<State, Result>, State&, Datum&>
{
	auto data_proxy = [consumer] (State& state, Datum& datum) {
		return promise::resolved<StreamAction>(consumer(state, datum));
	};
	return do_stateful_stream<State, Args...>(data_proxy, forward<Args>(args)...);
}

template <typename Result, typename Datum>
Promise<Result> PromiseStreamState<Result, Datum>::discard()
{
	return always(lock, StreamAction::Discard);
}

template <typename Result, typename Datum>
Promise<Result> PromiseStreamState<Result, Datum>::stop()
{
	lock_guard<mutex> lock(mx);
	return always(lock, StreamAction::Stop);
}

/*** PromiseStream ***/

/* Access stream */

template <typename Result, typename Datum>
auto PromiseStream<Result, Datum>::operator ->() const -> PromiseStreamState<DResult, Datum> *
{
	return stream.get();
}

/* Constructor for sharing state with another promise */

template <typename Result, typename Datum>
PromiseStream<Result, Datum>::PromiseStream(shared_ptr<PromiseStreamState<DResult, Datum>> const state) :
	stream(state)
{
}

/* Constructor for creating a new state */

template <typename Result, typename Datum>
PromiseStream<Result, Datum>::PromiseStream() :
	stream(make_shared<PromiseStreamState<DResult, Datum>>())
{
}

/* Cast/copy constructors */

template <typename Result, typename Datum>
PromiseStream<Result, Datum>::PromiseStream(const PromiseStream<DResult, Datum>& p) :
	PromiseStream(p.stream)
{
}

template <typename Result, typename Datum>
PromiseStream<Result, Datum>::PromiseStream(const PromiseStream<RResult, Datum>& p) :
	PromiseStream(p.stream)
{
}

template <typename Result, typename Datum>
PromiseStream<Result, Datum>::PromiseStream(const PromiseStream<XResult, Datum>& p) :
	PromiseStream(p.stream)
{
}

}
