#include <stdexcept>
#include "promise.h"

namespace kaiu {

using namespace std;

/*** Monads ***/

namespace detail {

	function<void()> combine_finalizers(const function<void()> f1, const function<void()> f2)
	{
		if (f1 == nullptr) {
			return f2;
		} else if (f2 == nullptr) {
			return f1;
		} else {
			return [f1, f2] () {
				try {
					f1();
				} catch (...) {
					f2();
					throw;
				}
				f2();
			};
		}
	}

}

/*** PromiseStateBase ***/

#if defined(DEBUG)
PromiseStateBase::~PromiseStateBase() noexcept(false)
{
	/*
	 * Don't throw if stack is being unwound, it'll prevent catch blocks from
	 * being run and will generally ruin your debugging experience.
	 */
	if (std::uncaught_exception()) {
		return;
	}
	if (callbacks_assigned && state != promise_state::completed) {
		/* Bound but not completed */
		throw logic_error("Promise destructor called on bound but uncompleted promise");
	}
}
#endif

void PromiseStateBase::reject(exception_ptr error)
{
	auto lock = get_lock();
	set_error(lock, error);
	set_state(lock, promise_state::rejected);
}

void PromiseStateBase::reject(const string& error)
{
	reject(make_exception_ptr(runtime_error(error)));
}

void PromiseStateBase::set_callbacks(ensure_locked lock, function<void(ensure_locked)> resolve, function<void(ensure_locked)> reject)
{
#if defined(DEBUG)
	if (callbacks_assigned) {
		throw logic_error("Attempted to double-bind to promise");
	}
	if (!resolve || !reject) {
		throw logic_error("Attempted to bind null callback");
	}
#endif
	on_resolve = resolve;
	on_reject = reject;
	callbacks_assigned = true;
	update_state(lock);
}

void PromiseStateBase::set_state(ensure_locked lock, const promise_state next_state)
{
#if defined(DEBUG)
	/* Validate transition */
	switch (next_state) {
	case promise_state::pending:
		throw logic_error("Cannot explicitly mark a promise as pending");
	case promise_state::rejected:
	case promise_state::resolved:
		if (state == promise_state::pending) {
			make_immortal(lock);
			break;
		}
		if (state == next_state) {
			break;
		}
		throw logic_error("Cannot resolve/reject promise: it is already resolved/rejected");
	case promise_state::completed:
		if (state == promise_state::rejected || state == promise_state::resolved) {
			break;
		}
		throw logic_error("Cannot mark promise as completed: promise has not been resolved/rejected");
	default:
		throw logic_error("Invalid state");
	}
#endif
	/* Apply transition */
	state = next_state;
	update_state(lock);
}

void PromiseStateBase::update_state(ensure_locked lock)
{
	/*
	 * After unlock_self(), this instance may be destroyed.  Hence, we must not
	 * do anything that accesses *this after calling unlock_self().
	 * Consequently, any method which calls update_state(...) or set_state()
	 * must not access *this after that call.
	 */
	switch (state) {
	case promise_state::pending:
		break;
	case promise_state::rejected:
		if (callbacks_assigned) {
			auto callback = on_reject;
			callback(lock);
			set_state(lock, promise_state::completed);
		}
		break;
	case promise_state::resolved:
		if (callbacks_assigned) {
			auto callback = on_resolve;
			callback(lock);
			set_state(lock, promise_state::completed);
		}
		break;
	case promise_state::completed:
		on_resolve = nullptr;
		on_reject = nullptr;
		make_mortal(lock);
		break;
	}
}

void PromiseStateBase::set_error(ensure_locked lock, exception_ptr error)
{
	this->error = error;
}

exception_ptr PromiseStateBase::get_error(ensure_locked) const
{
	return error;
}

void PromiseStateBase::set_terminator(ensure_locked lock)
{
	set_callbacks(lock,
		[] (ensure_locked) {},
		[this] (ensure_locked) {
			if (error) {
				rethrow_exception(error);
			}
		});
}

void PromiseStateBase::finish()
{
	auto lock = get_lock();
	set_terminator(lock);
}

/*** Utils ***/
namespace promise {

/*
 * Promise factory creator - for when the parameter is statically known to be
 * nullptr
 */

nullptr_t factory(nullptr_t)
{
	return nullptr;
}

}

}
