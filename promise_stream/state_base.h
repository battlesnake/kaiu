#pragma once

namespace kaiu {

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

}
