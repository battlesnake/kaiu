#include <stdexcept>
#include "promise.h"

namespace mark {

using namespace std;

/*** PromiseInternalBase ***/

PromiseInternalBase::PromiseInternalBase()
{
}

PromiseInternalBase::~PromiseInternalBase() noexcept(false)
{
	lock_guard<mutex> lock(state_lock);
	/* Not completed and not end of chain */
	if (callbacks_assigned && state != promise_state::completed) {
		on_resolve = nullptr;
		on_reject = nullptr;
		throw logic_error("Promise destructor called on uncompleted promise");
	}
}

PromiseInternalBase::PromiseInternalBase(const nullptr_t dummy, exception_ptr error) :
	error(error)
{
	lock_guard<mutex> lock(state_lock);
	rejected();
}

PromiseInternalBase::PromiseInternalBase(const nullptr_t dummy, const string& error) :
	error(make_exception_ptr(runtime_error(error)))
{
	lock_guard<mutex> lock(state_lock);
	rejected();
}

void PromiseInternalBase::ensure_is_still_pending() const
{
	if (state != promise_state::pending) {
		throw logic_error("Attempted to resolve/reject an already resolved/rejected promise");
	}
}

void PromiseInternalBase::assigned_callbacks()
{
	callbacks_assigned = true;
	if (state == promise_state::rejected) {
		rejected();
	} else if (state == promise_state::resolved) {
		resolved();
	}
}

void PromiseInternalBase::reject(exception_ptr error)
{
	ensure_is_still_pending();
	lock_guard<mutex> lock(state_lock);
	ensure_is_still_pending();
	this->error = error;
	rejected();
}

void PromiseInternalBase::reject(const string& error)
{
	reject(make_exception_ptr(runtime_error(error)));
}

void PromiseInternalBase::resolved()
{
	/* Only called from within state_lock spinlock */
	state = promise_state::resolved;
	if (callbacks_assigned) {
		on_resolve();
		completed_and_called();
	}
}

void PromiseInternalBase::rejected()
{
	/* Only called from within state_lock spinlock */
	state = promise_state::rejected;
	if (callbacks_assigned) {
		on_reject();
		completed_and_called();
	}
}

void PromiseInternalBase::completed_and_called()
{
	/* Only called from within state_lock spinlock */
	if (state == promise_state::pending) {
		throw logic_error("Attempted to mark uncompleted promise as complete");
	}
	state = promise_state::completed;
	/*
	 * The callbacks may hold references to this promise's container, via
	 * closure.  Let's break circular references so we don't memory leak...
	 */
	on_resolve = nullptr;
	on_reject = nullptr;
	error = exception_ptr{};
}

void PromiseInternalBase::ensure_is_unbound() const
{
	if (callbacks_assigned) {
		throw logic_error("Attempted to double-bind to promise");
	}
}

namespace promise {

Promise<nullptr_t> begin_chain()
{
	return Promise<nullptr_t>{nullptr};
}

}

}
