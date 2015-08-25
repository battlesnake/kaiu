#include <stdexcept>
#include "promise.h"

namespace kaiu {

using namespace std;

/*** PromiseStateBase ***/

PromiseStateBase::PromiseStateBase()
{
}

#if defined(DEBUG)
PromiseStateBase::~PromiseStateBase() noexcept(false)
{
	lock_guard<mutex> lock(state_lock);
	/* Not completed and not end of chain */
	if (callbacks_assigned && state != promise_state::completed) {
		on_resolve = nullptr;
		on_reject = nullptr;
		throw logic_error("Promise destructor called on bound but uncompleted promise");
	}
}
#endif

PromiseStateBase::PromiseStateBase(const nullptr_t dummy, exception_ptr error)
{
	reject(error);
}

PromiseStateBase::PromiseStateBase(const nullptr_t dummy, const string& error)
{
	reject(error);
}

void PromiseStateBase::reject(exception_ptr error)
{
	lock_guard<mutex> lock(state_lock);
	set_error(lock, error);
	set_state(lock, promise_state::rejected);
}

void PromiseStateBase::reject(const string& error)
{
	reject(make_exception_ptr(runtime_error(error)));
}

void PromiseStateBase::set_callbacks(ensure_locked lock, function<void(ensure_locked)> resolve, function<void(ensure_locked)> reject)
{
	if (callbacks_assigned) {
		throw logic_error("Attempted to double-bind to promise");
	}
	if (!resolve || !reject) {
		throw logic_error("Attempted to bind null callback");
	}
	on_resolve = resolve;
	on_reject = reject;
	callbacks_assigned = true;
	update_state(lock);
}

void PromiseStateBase::set_state(ensure_locked lock, const promise_state next_state)
{
	/* Validate transition */
	switch (next_state) {
	case promise_state::pending:
		throw logic_error("Cannot explicitly mark a promise as pending");
	case promise_state::rejected:
	case promise_state::resolved:
		if (state == promise_state::pending || state == next_state) {
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
	/* Apply transition */
	state = next_state;	
	update_state(lock);
}

void PromiseStateBase::update_state(ensure_locked lock)
{
	switch (state) {
	case promise_state::pending:
		break;
	case promise_state::rejected:
		set_locked(true);
		if (callbacks_assigned) {
			on_reject(lock);
			set_state(lock, promise_state::completed);
		}
		break;
	case promise_state::resolved:
		set_locked(true);
		if (callbacks_assigned) {
			on_resolve(lock);
			set_state(lock, promise_state::completed);
		}
		break;
	case promise_state::completed:
		/*
		 * The callbacks should hold references to this promise's container, via
		 * closures.  Let's break circular references so we don't memory leak...
		 */
		on_resolve = nullptr;
		on_reject = nullptr;
		set_locked(false);
		break;
	}
}

void PromiseStateBase::set_error(ensure_locked lock, exception_ptr error)
{
	this->error = error;
}

exception_ptr& PromiseStateBase::get_error(ensure_locked)
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
	update_state(lock);
}

void PromiseStateBase::finish()
{
	lock_guard<mutex> lock(state_lock);
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
