#define promise_stream_tcc
#include <exception>
#include <stdexcept>
#include <thread>
#include "shared_functor.h"
#include "promise_stream.h"

namespace kaiu {

using namespace std;

/*** PromiseStreamState ***/

/* Resolve / reject the stream */

template <typename Result, typename Datum>
void PromiseStreamState<Result, Datum>::resolve(Result result)
{
	auto lock = get_lock();
	do_resolve(lock, move(result));
}

template <typename Result, typename Datum>
void PromiseStreamState<Result, Datum>::reject(exception_ptr error)
{
	auto lock = get_lock();
	do_reject(lock, error, false);
}

template <typename Result, typename Datum>
void PromiseStreamState<Result, Datum>::reject(const string& error)
{
	auto lock = get_lock();
	do_reject(lock, make_exception_ptr(runtime_error(error)), false);
}

template <typename Result, typename Datum>
template <typename... Args>
void PromiseStreamState<Result, Datum>::write(Args&&... args)
{
	auto lock = get_lock();
	if (get_action(lock) == StreamAction::Continue) {
		emplace_data(lock, forward<Args>(args)...);
		set_stream_has_been_written_to(lock);
	}
	process_data(lock);
}

template <typename Result, typename Datum>
void PromiseStreamState<Result, Datum>::set_data_callback(DataFunc data_callback)
{
	auto lock = get_lock();
#if defined(SAFE_PROMISE_STREAMS)
	if (on_data != nullptr) {
		throw logic_error("Callbacks are already assigned");
	}
	if (data_callback == nullptr) {
		throw logic_error("Attempted to bind null callback");
	}
#endif
	on_data = data_callback;
	set_data_callback_assigned(lock);
	process_data(lock);
}

template <typename Result, typename Datum>
Promise<Result> PromiseStreamState<Result, Datum>::
	do_stream(stream_consumer consumer)
{
	auto data = [consumer] (ensure_locked, Datum datum) {
		try {
			return consumer(move(datum));
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
	auto consumer_proxy = [this, consumer, state] (Datum datum) {
		return consumer(*state, move(datum));
	};
	auto next_proxy = [this, state] (Result result) {
		return make_pair(move(*state), move(result));
	};
	return do_stream(consumer_proxy)
		->then(next_proxy);
}

template <typename Result, typename Datum>
Promise<Result> PromiseStreamState<Result, Datum>::always(StreamAction action)
{
	auto consumer = [action] (Datum datum) {
		return promise::resolved(action);
	};
	return do_stream(consumer);
}

template <typename Result, typename Datum>
Promise<Result> PromiseStreamState<Result, Datum>::discard()
{
	return always(StreamAction::Discard);
}

template <typename Result, typename Datum>
Promise<Result> PromiseStreamState<Result, Datum>::stop()
{
	return always(StreamAction::Stop);
}

template <typename Result, typename Datum>
void PromiseStreamState<Result, Datum>::forward_to(PromiseStream<Result, Datum> next)
{
	this
		->stream<void>([next] (Datum datum) {
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
void PromiseStreamState<Result, Datum>::call_data_callback(ensure_locked lock, Datum datum) try {
	/*
	 * Asynchronous on_data callbacks will obviously not trigger the catch block
	 * if they throw.  Synchronous on_data callbacks also will not, as the
	 * promise's "handler" eats the exception.
	 *
	 * The callback passed via stream(...) methods is wrapped in a try/catch
	 * block so in theory, the catch block in this function should never be
	 * triggered by exceptions from user code, with the possible exception of
	 * the move constructor for type Datum.
	 */
	/* Set that consumer is running */
	set_consumer_is_running(lock, true);
	/*
	 * We need to be able to detect whether the callbacks are run synchronously
	 * or not.  If run asynchronously, they could be in the same thread as the
	 * caller, or a different one.  Hence we need to combine two variables:
	 *  * "current thread id"
	 *  * the async_check boolean
	 *
	 * async_check detects asynchronous calling when the callbacks run in the
	 * calling thread, but may not always work if the callbacks are called in
	 * another thread.  "current thread id" fixes that case.
	 */
	auto async_check = make_shared<atomic<bool>>(false);
	const auto caller_id = this_thread::get_id();
	const auto is_async = [=] {
		return *async_check || this_thread::get_id() != caller_id;
	};
	using lock_type = typename std::decay<decltype(lock)>::type;
	/* Run consumer */
	on_data(lock, move_if_noexcept(datum))
		->then(
			[this, is_async, lck=&lock] (const StreamAction action) {
				/* Get lock if we're running asynchronously */
				const bool async = is_async();
				lock_type new_lock;
				if (async) {
					new_lock = get_lock();
				}
				lock_type& lock = async ? new_lock : *lck;
				/* Lock acquired */
				set_action(lock, action);
				set_consumer_is_running(lock, false);
				/* Will release the lock */
				process_data(lock);
			},
			[this, is_async, lck=&lock] (exception_ptr error) {
				/* Get lock if we're running asynchronously */
				const bool async = is_async();
				lock_type new_lock;
				if (async) {
					new_lock = get_lock();
				}
				lock_type& lock = async ? new_lock : *lck;
				/* Lock acquired */
				do_reject(lock, error, true);
				set_consumer_is_running(lock, false);
			});
	*async_check = true;
} catch (...) {
	set_consumer_is_running(lock, false);
	/* Rethrowing is implicit since this is a function-try-catch block */
	throw;
}

template <typename Result, typename Datum>
void PromiseStreamState<Result, Datum>::process_data(ensure_locked lock)
{
	Datum datum;
	if (!take_data(lock, datum)) {
		return;
	}
	/*
	 * Thread-safe, as we (within the previous lock) check whether on_data has
	 * been assigned yet, and it can not be re-assigned once set.  We also check
	 * and set consumer_running, so the data callback can not be called
	 * concurrently.
	 *
	 * Will release the lock.
	 */
	call_data_callback(lock, move(datum));
}

template <typename Result, typename Datum>
bool PromiseStreamState<Result, Datum>::take_data(ensure_locked lock, Datum& out)
{
	auto state = get_state(lock);
	/* Wrong state */
	if (state != stream_state::streaming1 && state != stream_state::streaming2) {
		return false;
	}
	/* No consumer assigned yet */
	if (on_data == nullptr) {
		return false;
	}
	/* Don't run the consumer multiple time simultaneously */
	if (get_consumer_is_running(lock)) {
		return false;
	}
	/*
	 * If consumer has finished, empty the buffer (and ignore subsequent
	 * writes, see "write" method)
	 */
	if (get_action(lock) != StreamAction::Continue) {
		decltype(buffer)().swap(buffer);
	}
	if (buffer.empty()) {
		/*
		 * When removing the last data from the buffer, this call must occur
		 * AFTER set_consumer_is_running(lock, true) in order for state
		 * transitions to work correctly.  Since we do not call
		 * set_buffer_is_empty on removing the last data from the buffer,
		 * but instead we call it after the last data has been processed,
		 * this is not a problem for us.
		 */
		set_buffer_is_empty(lock);
		return false;
	}
	out = move(buffer.front());
	buffer.pop();
	/* Return success */
	return true;
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
void PromiseStreamState<Result, Datum>::do_resolve(ensure_locked lock, Result result)
{
	set_stream_result(lock, stream_result::resolved, resolve_completer(move(result)));
}

template <typename Result, typename Datum>
void PromiseStreamState<Result, Datum>::do_reject(ensure_locked lock, exception_ptr error, bool consumer_failed)
{
	set_action(lock, StreamAction::Stop);
	set_stream_result(lock, consumer_failed ? stream_result::consumer_failed : stream_result::rejected, reject_completer(error));
}

template <typename Result, typename Datum>
auto PromiseStreamState<Result, Datum>::resolve_completer(Result result)
	-> completer_func
{
	auto functor = [this, result = move(result)] (ensure_locked lock) mutable {
		proxy_promise->resolve(move(result));
	};
	return detail::make_shared_functor(move(functor));
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
	-> stream_sel<Consumer, result_of_promise_is, Result, Datum>
{
	return do_stream(consumer);
}

/* Stateless consumer returning action */
template <typename Result, typename Datum>
template <typename, typename Consumer>
auto PromiseStreamState<Result, Datum>::stream(Consumer consumer)
	-> stream_sel<Consumer, result_of_not_promise_is, Result, Datum>
{
	auto data_proxy = [consumer] (Datum datum) {
		return promise::resolved<StreamAction>(consumer(move(datum)));
	};
	return do_stream(data_proxy);
}

/* Stateful consumer returning promise */
template <typename Result, typename Datum>
template <typename State, typename Consumer, typename... Args>
auto PromiseStreamState<Result, Datum>::stream(Consumer consumer, Args&&... args)
	-> stream_sel<Consumer, result_of_promise_is, pair<State, Result>, State&, Datum>
{
	return do_stateful_stream<State, Args...>(consumer, forward<Args>(args)...);
}

/* Stateful consumer returning action */
template <typename Result, typename Datum>
template <typename State, typename Consumer, typename... Args>
auto PromiseStreamState<Result, Datum>::stream(Consumer consumer, Args&&... args)
	-> stream_sel<Consumer, result_of_not_promise_is, pair<State, Result>, State&, Datum>
{
	auto data_proxy = [consumer] (State& state, Datum datum) {
		return promise::resolved<StreamAction>(consumer(state, move(datum)));
	};
	return do_stateful_stream<State, Args...>(data_proxy, forward<Args>(args)...);
}

/*** PromiseStream ***/

/* Access stream */

template <typename Result, typename Datum>
auto PromiseStream<Result, Datum>::operator ->() const -> PromiseStreamState<Result, Datum> *
{
	return stream.get();
}

/* Constructor for sharing state with another promise */

template <typename Result, typename Datum>
PromiseStream<Result, Datum>::PromiseStream(shared_ptr<PromiseStreamState<Result, Datum>> const state) :
	stream(state)
{
}

/* Constructor for creating a new state */

template <typename Result, typename Datum>
PromiseStream<Result, Datum>::PromiseStream() :
	stream(make_shared<PromiseStreamState<Result, Datum>>())
{
}

}
