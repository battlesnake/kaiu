#include "promise_stream.h"

namespace kaiu {

using namespace std;

#if defined(SAFE_PROMISE_STREAMS)
PromiseStreamStateBase::~PromiseStreamStateBase() noexcept(false)
{
	if (data_callback_assigned) {
		if (state != stream_state::completed) {
			throw logic_error("Promise stream destructor called on bound but uncompleted promise stream");
		}
	} else {
		/* Not completed and not end of chain (should be a "warning" really) */
		if (state != stream_state::pending && state != stream_state::completed) {
			throw logic_error("Unterminted promise chain (forgot ->discard?)");
		}
	}
}
#endif

void PromiseStreamStateBase::set_state(ensure_locked lock, stream_state next_state)
{
#if defined(SAFE_PROMISE_STREAMS)
	/* Validate transition */
	switch (next_state) {
	case stream_state::pending:
		throw logic_error("Invalid state transition");
	case stream_state::streaming1:
		if (state == stream_state::pending) {
			break;
		}
		throw logic_error("Invalid state transition");
	case stream_state::streaming2:
		if (state == stream_state::streaming1) {
			break;
		}
		throw logic_error("Invalid state transition");
	case stream_state::streaming3:
		if (state == stream_state::streaming2) {
			break;
		}
		throw logic_error("Invalid state transition");
	case stream_state::completed:
		if (state == stream_state::pending || state == stream_state::streaming3) {
			break;
		}
		throw logic_error("Invalid state transition");
	default:
		throw logic_error("Invalid state");
	}
#endif
	/* Apply transition */
	state = next_state;
	update_state(lock);
}

void PromiseStreamStateBase::update_state(ensure_locked lock)
{
	switch (state) {
	case stream_state::pending:
		if (stream_has_been_written_to) {
			set_state(lock, stream_state::streaming1);
		} else if (result != stream_result::pending) {
			set_state(lock, stream_state::completed);
		}
		break;
	case stream_state::streaming1:
		if (result != stream_result::pending) {
			set_state(lock, stream_state::streaming2);
		}
		break;
	case stream_state::streaming2:
		if (buffer_is_empty) {
			set_state(lock, stream_state::streaming3);
		}
		break;
	case stream_state::streaming3:
		if (!consumer_is_running) {
			set_state(lock, stream_state::completed);
		}
		break;
	case stream_state::completed:
		if (result != stream_result::pending) {
			decltype(completer) callback{nullptr};
			swap(callback, completer);
			callback(lock);
			make_mortal(lock);
		}
		break;
	}
}

auto PromiseStreamStateBase::get_state(ensure_locked) const -> stream_state
{
	return state;
}

void PromiseStreamStateBase::set_stream_has_been_written_to(ensure_locked lock)
{
#if defined(SAFE_PROMISE_STREAMS)
	if (result == stream_result::resolved || result == stream_result::rejected) {
		throw logic_error("Data written to stream after it has been completed");
	}
#endif
	stream_has_been_written_to = true;
	buffer_is_empty = false;
	update_state(lock);
}

void PromiseStreamStateBase::set_stream_result(ensure_locked lock, stream_result result_, function<void(ensure_locked)> completer_)
{
	if (result == stream_result::consumer_failed) {
		/*
		 * If consumer throws, that rejection overrides any result set by the
		 * producer
		 */
		return;
	} else if (result != stream_result::pending) {
		/*
		 * Throw on multiple resolutions by producer
		 */
		throw logic_error("Attempted to resolve promise stream multiple times");
	}
	completer = completer_;
	result = result_;
	update_state(lock);
}

void PromiseStreamStateBase::set_buffer_is_empty(ensure_locked lock)
{
	if (buffer_is_empty) {
		return;
	}
	/* Unset by set_stream_has_been_written_to */
	buffer_is_empty = true;
	update_state(lock);
}

void PromiseStreamStateBase::set_consumer_is_running(ensure_locked lock,
	bool value)
{
#if defined(SAFE_PROMISE_STREAMS)
	if (consumer_is_running == value) {
		throw logic_error("set_consumer_is_running: concurrent consumers detected");
	}
#endif
	consumer_is_running = value;
	update_state(lock);
}

bool PromiseStreamStateBase::get_consumer_is_running(ensure_locked) const
{
	return consumer_is_running;
}

void PromiseStreamStateBase::set_action(ensure_locked, StreamAction action)
{
	this->action = action;
}

StreamAction PromiseStreamStateBase::get_action(ensure_locked) const
{
	return action;
}

void PromiseStreamStateBase::set_data_callback_assigned(ensure_locked lock)
{
#if defined(SAFE_PROMISE_STREAMS)
	if (data_callback_assigned) {
		throw logic_error("Data callback bound multiple times");
	}
#endif
	data_callback_assigned = true;
	if (get_state(lock) != stream_state::completed) {
		make_immortal(lock);
	}
}

/* Public functions */

bool PromiseStreamStateBase::is_stopping() const
{
	auto lock = get_lock();
	return get_action(lock) == StreamAction::Stop;
}

StreamAction PromiseStreamStateBase::data_action() const
{
	auto lock = get_lock();
	return get_action(lock);
}

}
