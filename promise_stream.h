#pragma once
#include <deque>
#include <queue>
#include <memory>
#include <functional>
#include <utility>
#include <type_traits>
#include <mutex>
#include "promise.h"
#include "self_managing.h"

#if defined(DEBUG)
#define SAFE_PROMISE_STREAMS
#endif

namespace kaiu {

using namespace std;

enum class StreamAction {
	Continue,
	Discard,
	Stop
};

template <typename Result, typename Datum>
class PromiseStream;

class PromiseStreamStateBase : public self_managing {
public:
	/* Default constructor */
	PromiseStreamStateBase() = default;
	/* No copy/move constructor */
	PromiseStreamStateBase(const PromiseStreamStateBase&) = delete;
	PromiseStreamStateBase(PromiseStreamStateBase&&) = delete;
	/* Destructor */
#if defined(SAFE_PROMISE_STREAMS)
	~PromiseStreamStateBase() noexcept(false);
#endif
	/* Stop has been requested by consumer */
	bool is_stopping() const;
	StreamAction data_action() const;
protected:
	/* Data action (set by consumer, read by producer) */
	void set_action(ensure_locked, StreamAction);
	StreamAction get_action(ensure_locked) const;
	/*
	 * Called when on_data callback has been assigned - will make the object
	 * immortal
	 */
	void set_data_callback_assigned(ensure_locked);
	/* Get the state */
	enum class stream_state { pending, streaming1, streaming2, streaming3, completed };
	stream_state get_state(ensure_locked) const;
	/* Stream result type */
	enum class stream_result { pending, resolved, rejected, consumer_failed };
	using completer_func = function<void(ensure_locked)>;
	/* A→B: When stream has been written to */
	void set_stream_has_been_written_to(ensure_locked);
	/* B→C, A→E: When result has been bound */
	void set_stream_result(ensure_locked, stream_result, completer_func);
	/* C→D: When buffer is empty */
	void set_buffer_is_empty(ensure_locked);
	/* D→E: When consumer is not running */
	void set_consumer_is_running(ensure_locked, bool);
	bool get_consumer_is_running(ensure_locked) const;
	/*
	 * Note: When taking the last data from the
	 * buffer,
	 *   set_consumer_is_running(lock, true)
	 * must be called BEFORE
	 *   set_buffer_is_empty(lock)
	 */
private:
	/* Re-applies the current state, advances to next state if possible */
	void update_state(ensure_locked);
	/* Stream state */
	stream_state state{stream_state::pending};
	/* Validates state transitions */
	void set_state(ensure_locked, stream_state);
	/* Action requested by consumer */
	StreamAction action{StreamAction::Continue};
	/* Stream has been written to */
	bool stream_has_been_written_to{false};
	/* Buffer is empty */
	bool buffer_is_empty{true};
	/* Has an on_data callback been assigned (in derived class)? */
	bool data_callback_assigned{false};
	/* Is consumer running */
	bool consumer_is_running{false};
	/* Called on completion (bool stores if completer represents rejection) */
	completer_func completer{nullptr};
	stream_result result{stream_result::pending};
};

template <typename Result, typename Datum>
class PromiseStreamState : public PromiseStreamStateBase {
	static_assert(is_same<Datum, typename remove_cvr<Datum>::type>::value, "Datum type must not be const/volatile/reference-qualified");
	static_assert(is_same<Result, typename remove_cvr<Result>::type>::value, "Result type must not be const/volatile/reference-qualified");
	/* SFINAE check to select a stream() method based on consumer return type*/
	template <
		typename Consumer,
		template <typename, typename> class Test,
		typename ResultType,
		typename... Args>
	using stream_sel =
		typename enable_if<
			Test<
				typename result_of<Consumer(Args&&...)>::type,
				StreamAction
			>::value,
			Promise<ResultType>
		>::type;
public:
	PromiseStreamState() = default;
	PromiseStreamState(const PromiseStreamState&) = delete;
	PromiseStreamState(PromiseStreamState&&) = delete;
	virtual ~PromiseStreamState() = default;
	void forward_to(PromiseStream<Result, Datum> next);
	void forward_to(Promise<Result> next);
	/*** Used by producer ***/
	/* Write new data */
	template <typename... Args>
	void write(Args&&...);
	/* Resolve / reject */
	void resolve(Result result);
	void reject(exception_ptr error);
	void reject(const string& error);
	/* Stop has been requested */
	bool stop_requested() const;
	/*** Used by consumer ***/
	/* Bind callbacks that don't care about the data */
	Promise<Result> discard();
	Promise<Result> stop();
	/* Stateless consumer returning promise */
	template <typename = void, typename Consumer>
	stream_sel<Consumer, result_of_promise_is, Result, Datum>
		stream(Consumer consumer);
	/* Stateless consumer returning action */
	template <typename = void, typename Consumer>
	stream_sel<Consumer, result_of_not_promise_is, Result, Datum>
		stream(Consumer consumer);
	/* Stateful consumer returning promise */
	template <typename State, typename Consumer, typename... Args>
	stream_sel<Consumer, result_of_promise_is, pair<State, Result>, State&, Datum>
		stream(Consumer consumer, Args&&... args);
	/* Stateful consumer returning action */
	template <typename State, typename Consumer, typename... Args>
	stream_sel<Consumer, result_of_not_promise_is, pair<State, Result>, State&, Datum>
		stream(Consumer consumer, Args&&... args);
protected:
	/* Is data queued? */
	bool has_data(ensure_locked) const;
	/* Call the on_data callback */
	virtual void call_data_callback(ensure_locked, Datum);
	/* Bind resolve/reject dispatchers */
	using completer_func = typename PromiseStreamStateBase::completer_func;
	virtual completer_func resolve_completer(Result);
	virtual completer_func reject_completer(exception_ptr);
	/* Promise proxy for result of streaming operation */
	Promise<Result> proxy_promise;
private:
	Promise<Result> always(StreamAction);
	using stream_consumer = function<Promise<StreamAction>(Datum)>;
	template <typename State>
	using stateful_stream_consumer =
		function<Promise<StreamAction>(State&, Datum)>;
	Promise<Result> do_stream(stream_consumer consumer);
	template <typename State, typename... Args>
	Promise<pair<State, Result>> do_stateful_stream(
		stateful_stream_consumer<State> consumer,
		Args&&... args);
	using DataFunc = function<Promise<StreamAction>(ensure_locked, Datum)>;
	/* Buffer */
	queue<Datum> buffer{};
	template <typename... Args>
	void emplace_data(ensure_locked, Args&&...);
	/* Callbacks */
	DataFunc on_data{nullptr};
	void set_data_callback(DataFunc);
	/* Call consumer */
	void process_data(ensure_locked);
	/* If data is available, moves data into <out> */
	bool take_data(ensure_locked, Datum& out);
	/* Capture value and set resolve/reject completer */
	void do_resolve(ensure_locked, Result);
	void do_reject(ensure_locked, exception_ptr, bool consumer_failed);
};

template <typename T>
struct is_promise_stream {
private:
	template <typename U>
	static integral_constant<bool, U::is_promise_stream> check(int);
	template <typename>
	static std::false_type check(...);
public:
	static constexpr auto value = decltype(check<T>(0))::value;
};

template <typename Result, typename Datum>
class PromiseStream : public PromiseLike {
	static_assert(!is_void<Result>::value, "Void promise streams are not supported");
	static_assert(!is_promise<Result>::value, "Promise of promise is probably not intended");
	static_assert(!is_void<Datum>::value, "Void promise streams are not supported");
	static_assert(!is_promise<Datum>::value, "Promise of promise is probably not intended");
public:
	using result_type = Result;
	using datum_type = Datum;
	/* Promise stream */
	PromiseStream();
	/* Copy/move/cast constructors */
	PromiseStream(PromiseStream<Result, Datum>&&) = default;
	PromiseStream(const PromiseStream<Result, Datum>&) = default;
	/* Assignment */
	PromiseStream<Result, Datum>& operator =(PromiseStream<Result, Datum>&&) = default;
	PromiseStream<Result, Datum>& operator =(const PromiseStream<Result, Datum>&) = default;
	/* Access promise state (then/except/finally/resolve/reject) */
	PromiseStreamState<Result, Datum> *operator ->() const;
protected:
	PromiseStream(shared_ptr<PromiseStreamState<Result, Datum>> const stream);
private:
	shared_ptr<PromiseStreamState<Result, Datum>> stream;
};

namespace promise {

template <typename Result, typename Datum, typename... Args>
using StreamFactory = function<PromiseStream<Result, Datum>(Args...)>;

}

}

#ifndef promise_stream_tcc
#include "promise_stream.tcc"
#endif
