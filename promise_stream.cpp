#include "promise_stream.h"

namespace kaiu {

using namespace std;

#if defined(DEBUG)
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

auto PromiseStreamStateBase::get_state(ensure_locked) const -> stream_state
{
	return state;
}

void PromiseStreamStateBase::set_state(ensure_locked lock, const stream_state next_state)
{
#if defined(DEBUG)
	/* Validate transition */
	switch (next_state) {
	case stream_state::pending:
		throw logic_error("Invalid state transition");
	case stream_state::streaming1:
		if (state == stream_state::pending || state == next_state) {
			break;
		}
		throw logic_error("Invalid state transition");
	case stream_state::streaming2:
		if (state == stream_state::streaming1 || state == next_state) {
			break;
		}
		throw logic_error("Invalid state transition");
	case stream_state::streaming3:
		if (state == stream_state::streaming2 || state == next_state) {
			break;
		}
		throw logic_error("Invalid state transition");
	case stream_state::completed:
		if (state == stream_state::pending || state == stream_state::streaming3) {
			if (has_completer(lock)) {
				break;
			}
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
		if (is_written_to(lock)) {
			set_state(lock, stream_state::streaming1);
		} else if (has_completer(lock)) {
			set_state(lock, stream_state::completed);
		}
		break;
	case stream_state::streaming1:
		if (has_completer(lock)) {
			set_state(lock, stream_state::streaming2);
		}
		break;
	case stream_state::streaming2:
		if (!has_data(lock)) {
			set_state(lock, stream_state::streaming3);
		}
		break;
	case stream_state::streaming3:
		if (!is_consumer_running(lock)) {
			set_state(lock, stream_state::completed);
		}
		break;
	case stream_state::completed:
		if (has_completer(lock)) {
			decltype(completer) callback{nullptr};
			swap(callback, completer);
			lock_self();
			callback(lock);
			unlock_self();
		}
		break;
	}
}

void PromiseStreamStateBase::set_written_to(ensure_locked lock)
{
	auto state = get_state(lock);
	if (state != stream_state::pending && state != stream_state::streaming1) {
		/*
		 * TODO: If consumer throws, causing rejection of the stream, we should
		 * set_action(Stop) (if we don't already) and allow more writes (albeit
		 * ignored).
		 */
		throw logic_error("Data written to stream after it has been completed");
	}
	written_to = true;
	update_state(lock);
}

bool PromiseStreamStateBase::is_written_to(ensure_locked) const
{
	return written_to;
}

void PromiseStreamStateBase::set_completer(ensure_locked lock, stream_result result_, function<void(ensure_locked)> completer_)
{
	/*
	 * If consumer throws, that rejection overrides any result set by the
	 * producer
	 */
	if (completer && result == stream_result::consumer_failed) {
		return;
	}
	/* Throw on multiple resolutions by producer */
	if (completer && result_ != stream_result::consumer_failed) {
		throw logic_error("Attempted to resolve promise stream multiple times");
	}
	completer = completer_;
	result = result_;
	update_state(lock);
}

bool PromiseStreamStateBase::has_completer(ensure_locked) const
{
	return completer != nullptr;
}

void PromiseStreamStateBase::set_consumer_running(ensure_locked lock,
	const bool value)
{
	consumer_is_running = value;
	update_state(lock);
}

bool PromiseStreamStateBase::is_consumer_running(ensure_locked) const
{
	return consumer_is_running;
}

void PromiseStreamStateBase::set_action(ensure_locked, const StreamAction action)
{
	this->action = action;
}

StreamAction PromiseStreamStateBase::get_action(ensure_locked) const
{
	return action;
}

void PromiseStreamStateBase::set_data_callback_assigned()
{
	{
		lock_guard<mutex> lock(mx);
		if (data_callback_assigned) {
			throw logic_error("Data callback bound multiple times");
		}
		data_callback_assigned = true;
		update_state(lock);
	}
	process_data();
}

void PromiseStreamStateBase::lock_self()
{
	if (!self_reference) {
		self_reference = shared_from_this();
	}
}

void PromiseStreamStateBase::unlock_self()
{
	self_reference = nullptr;
}

/* Public functions */

bool PromiseStreamStateBase::is_stopping() const
{
	return data_action() == StreamAction::Stop;
}

StreamAction PromiseStreamStateBase::data_action() const
{
	lock_guard<mutex> lock(mx);
	return get_action(lock);
}

}
