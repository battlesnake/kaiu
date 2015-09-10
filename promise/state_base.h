#pragma once

namespace kaiu {

/***
 * Untyped promise state
 */

class PromiseStateBase : public self_managing {
public:
	/* Reject */
	void reject(exception_ptr error);
	void reject(const string& error);
	/* Default constructor */
	PromiseStateBase() = default;
	/* No copy/move constructor */
	PromiseStateBase(PromiseStateBase const&) = delete;
	PromiseStateBase(PromiseStateBase&&) = delete;
	PromiseStateBase operator =(PromiseStateBase const&) = delete;
	PromiseStateBase operator =(PromiseStateBase&&) = delete;
#if defined(SAFE_PROMISES)
	/* Destructor */
	~PromiseStateBase() noexcept(false);
#endif
	/* Make terminator */
	void finish();
protected:
	enum class promise_state { pending, rejected, resolved, completed };
	/* Validates that the above state transitions are being followed */
	void set_state(ensure_locked, const promise_state next_state);
	/* Re-applies the current state, advances to next state if possible */
	void update_state(ensure_locked);
	/*
	 * Set the resolve/reject callbacks.  If the promise has been
	 * resolved/rejected, the appropriate callback will be called immediately.
	 */
	void set_callbacks(ensure_locked, function<void(ensure_locked)> resolve, function<void(ensure_locked)> reject);
	/* Get/set rejection result */
	void set_error(ensure_locked, exception_ptr error);
	exception_ptr get_error(ensure_locked) const;
	/* Make this promise a terminator */
	void set_terminator(ensure_locked);
private:
	promise_state state{promise_state::pending};
	exception_ptr error{};
	/*
	 * We release the callbacks after using them, so this variable is used to
	 * track whether we have bound callbacks at all, so that re-binding cannot
	 * happen after unbinding (and completion).
	 */
	bool callbacks_assigned{false};
	function<void(ensure_locked)> on_resolve{nullptr};
	function<void(ensure_locked)> on_reject{nullptr};
};

}
