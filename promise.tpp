#pragma once
#include <stdexcept>
#include "spinlock.h"
#include "promise.h"

namespace mark {

using namespace std;

template <typename Result>
void Promise_<Result>::resolve(Result& result)
{
	UserspaceSpinlock lock(state_lock);
	if (state != promise_state::pending) {
		throw logic_error("Attempted to resolve already completed promise");
	}
	resultPtr = move(result);
	resolved();
}

template <typename Result>
void Promise_<Result>::reject(exception& error)
{
	UserspaceSpinlock lock(state_lock);
	if (state != promise_state::pending) {
		throw logic_error("Attempted to reject already completed promise");
	}
	errorPtr = error;
	rejected();
}

template <typename Result>
void Promise_<Result>::resolved()
{
	/* Called from within state_lock spinlock */
	state = promise_state::resolved;
	if (assigned) {
		/* TODO */
	} else {
		/* TODO */
	}
}

template <typename Result>
void Promise_<Result>::rejected()
{
	/* Called from within state_lock spinlock */
	state = promise_state::rejected;
	if (assigned) {
		/* TODO */
	} else {
		/* TODO */
	}
}

template <typename Result>
template <typename NextResult>
Promise<NextResult> Promise_<Result>::then(
	Promise_<Result>::ThenFunc<NextResult>& next,
	Promise_<Result>::ExceptFunc<NextResult>& handler,
	Promise_<Result>::FinallyFunc& finally)
{
	UserspaceSpinlock lock(state_lock);
	if (assigned) {
		throw logic_error("Attempted to double-bind to promise");
	}
	assigned = true;
	Promise<NextResult> promise;
	if (next) {
		nextFunc = [this, next, promise] (Result& result) {
			Promise<NextResult> res;
			try {
				/*
				 * TODO:
				 * We need a way to see if a promise is wrapping an immediate
				 * value (or if it is just already resolved maybe) so we don't
				 * get a recursive loop of then(), given the promise-wrapping
				 * that is currently in use.
				 *
				 * Alternatively, maybe a better idea would be to ditch the
				 * promise wrapping?
				 *
				 * Otherwise, a way to bind one promise to another, so its
				 * events are forwarded (i.e. result promise obtained below
				 * forwards events directly to the promise that this method
				 * returns.
				 */
				res = next(result);
			} catch (exception& error) {
				promise.reject(error);
				return;
			}
			promise.resolve(res); /* wontwork, res is a promise! */
		};
	}
	if (handler) {
		exceptFunc = [this, handler, promise] (exception& error) {
			Promise<NextResult> res;
			try {
				/* TODO:
				 * See notes in previous comment
				 */
				res = handler(error);
			} catch (exception& newError) {
				promise.reject(newError);
				return;
			}
			promise.resolve(res); /* wontwork, res is a promise! */
		};
	}
	if (finally) {
		/* TODO:
		 * Implement the FINALLY logic, as specified in the header file spec
		 */
		finallyFunc = [this, finally, promise] () {
			try {
				finally();
			} catch (exception& newError) {
				promise.reject(newError);
				return;
			}
			switch (state) {
			case promise_state::rejected:
				promise.reject(*errorPtr);
				break;
			case promise_state::resolved:
				promise.resolve(*resultPtr);
				break;
			}
		};
	}
	switch (state) {
	case promise_state::rejected:
		rejected();
		break;
	case promise_state::resolved:
		resolved();
		break;
	}
	return promise;
}

}
