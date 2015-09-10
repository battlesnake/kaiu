#pragma once

namespace kaiu {

template <typename Result, typename Datum>
class PromiseStreamState : public PromiseStreamStateBase {
	static_assert(is_same<Datum, typename remove_cvr<Datum>::type>::value, "Datum type must not be const/volatile/reference-qualified");
	static_assert(is_same<Result, typename remove_cvr<Result>::type>::value, "Result type must not be const/volatile/reference-qualified");
	/* SFINAE check to select a stream() method based on consumer return type*/
	template <
		typename Consumer,
		template <typename, typename> class Test,
		typename Expect,
		typename ResultType,
		typename... Args>
	using stream_sel =
		typename enable_if<
			Test<
				typename result_of<Consumer(Args&&...)>::type,
				Expect
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
	stream_sel<Consumer, result_of_promise_is, StreamAction, Result, Datum>
		stream(Consumer consumer);
	/* Stateless consumer returning action */
	template <typename = void, typename Consumer>
	stream_sel<Consumer, result_of_not_promise_is, StreamAction, Result, Datum>
		stream(Consumer consumer);
	/* Stateless consumer returning void */
	template <typename = void, typename Consumer>
	stream_sel<Consumer, result_of_not_promise_is, void, Result, Datum>
		stream(Consumer consumer);
	/* Stateful consumer returning promise */
	template <typename State, typename Consumer, typename... Args>
	stream_sel<Consumer, result_of_promise_is, StreamAction, pair<State, Result>, State&, Datum>
		stream(Consumer consumer, Args&&... args);
	/* Stateful consumer returning action */
	template <typename State, typename Consumer, typename... Args>
	stream_sel<Consumer, result_of_not_promise_is, StreamAction, pair<State, Result>, State&, Datum>
		stream(Consumer consumer, Args&&... args);
	/* Stateful consumer returning void */
	template <typename State, typename Consumer, typename... Args>
	stream_sel<Consumer, result_of_not_promise_is, void, pair<State, Result>, State&, Datum>
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

}
