#pragma once
#include <functional>
#include <exception>
#include <cstddef>
#include <mutex>
#include <memory>
#include <type_traits>
#include <vector>
#include <tuple>
#include "self_managing.h"

#include "promise/fwd.h"
#include "promise/traits.h"
#include "promise/factories.h"
#include "promise/combiners.h"
#include "promise/callback_pack.h"
#include "promise/monad.h"

namespace kaiu {

/*
 * Promise, inspired by the JavaScript Q library, Unlambda, and Haskell.
 *
 * A promise encapsulates the result of an operation that may not hav completed
 * yet, and may not even have been started yet.  They're great for non-blocking
 * asynchronous programming (e.g. the NodeJS reactor) and can also be used to
 * provide lazy evaluation.
 *
 * When the value has been acquired, then the promise is "resolved" and a "next"
 * callback is called with the value.
 *
 * If the value cannot be resolved, then it is "rejected" with an exception (or
 * a string that is mapped to an exception) and the "except" callback is called
 * instead.
 *
 * Resolution is the equivalent of a function returning a value, and rejection
 * is the equivalent of a function throwing or returning an error.
 *
 * A promise is "completed" after it has either been "rejected" or "resolved"
 * and the relevant callbacks have been called.
 *
 * The following are functionally equivalent (next/handler can be nullptr):
 *   - promise.then(next, handler, finalizer)
 *   - promise.then(next, handler).finally(finalizer)
 *       calls either next or handler, always calls finalizer
 *       (calls next iff promise is resolved)
 *       (calls handler iff promise is rejected)
 *       (calls finalizer always)
 *
 * For readability, I recommend avoiding the first syntax.
 *
 * The following are NOT functionally equivalent:
 *   - promise.then(next, handler)
 *       calls EITHER next or handler
 *       (calls next iff promise is resolved)
 *       (calls handler iff promise is rejected)
 *   - promise.except(handler).then(next)
 *       MIGHT call handler AND MIGHT call next
 *       (calls handler iff promise is rejected)
 *       (calls next iff (promise is resolved or handler does not throw))
 *   - promise.then(next).except(handler)
 *       MIGHT call next AND MIGHT call Handler
 *       (calls next iff promise is resolved)
 *       (calls handler iff (promise is rejected or next throws))
 *
 * THEN function can return:
 *   - void - next promise resolves with same value as this one.
 *   - new value of any type - next promise resolves with this new value.
 *   - promise - resolution/rejection is forwarded to the returned promise.
 *   - throw exception - next promise is rejected with this exception unless
 *     FINALLY function throws.
 *
 * EXCEPT function can return:
 *   - new value of same type - next promise resolves with this new value.
 *   - promise - resolution/rejection is forwarded to the returned promise.
 *   - throw exception - next promise is rejected with this exception unless
 *     FINALLY function throws.
 *
 * FINALLY function can return:
 *   - void - next promise is resolved/rejected with same value as this one.
 *   - throw exception - next promise is rejected with THIS exception even if
 *     THEN function or EXCEPT function also threw.
 *
 * then(next, handler = nullptr, finally = nullptr):
 *   - next is called iff this promise is resolved.
 *   - handler is called iff this promise is rejected.
 *   - finally is always called, even if next/handler throws.
 *   - iff promise is resolved, next+finally are called.
 *   - iff promise is rejected, handler+finally are called.
 *   - with no handler, rejected promise propagates by rejection of the next
 *     promise.
 *   - handler can re-throw, causing rejection of the next promise.
 *   - handler can return a value, causing resolution of the next promise.
 *   - if next/handler/finally throws, the next promise is rejected.  handler is
 *     NOT called when next/finally throws, as handler operates on this promise,
 *     not the next one.
 *   - next promise is resolved iff either:
 *      - this promise is resolved and none of the callbacks throw.
 *      - this promise is rejected and has a handler which returns a resolution.
 *   - next promise is rejected iff either:
 *      - this promise is resolved but a callback throws.
 *      - this promise is rejected and either has no handler or the handler
 *        throws.
 *   - if next promise is rejected, exception thrown by finally takes priority
 *     over exception thrown by next/except (regarding exception used to reject
 *     next promise).
 *
 * except(handler, finally = nullptr):
 *   - shorthand for then(nullptr, handler, finally)
 *
 * finally(finally):
 *   - shorthand for then(nullptr, nullptr, finally)
 */

using namespace std;

/***
 * Promise class
 *
 * The default (no-parameter) constructor for Result type and for NextResult
 * type must not throw - promise chains guarantees will break.
 */

template <typename Result>
class Promise : public PromiseLike {
public:
	using DResult = typename remove_cvr<Result>::type;
	using result_type = DResult;
	/* Promise */
	Promise();
	/* Copy/move/cast constructors */
	Promise(const Promise<DResult>&);
	Promise(const Promise<DResult&>&);
	Promise(const Promise<DResult&&>&);
	Promise(Promise<Result>&&) = default;
	/* Assignment */
	Promise<Result>& operator =(Promise<Result>&&) = default;
	Promise<Result>& operator =(const Promise<Result>&) = default;
	/* Access promise state (then/except/finally/resolve/reject) */
	PromiseState<DResult> *operator ->() const;
private:
	friend class Promise<DResult>;
	friend class Promise<DResult&>;
	friend class Promise<DResult&&>;
	static_assert(!is_void<DResult>::value, "Void promises are no longer supported");
	static_assert(!is_same<DResult, exception_ptr>::value, "Promise result type cannot be exception_ptr");
	static_assert(!is_promise<DResult>::value, "Promise<Promise<T>> is invalid, use Promise<T>/forward_to instead");
	Promise(shared_ptr<PromiseState<DResult>> const promise);
	shared_ptr<PromiseState<DResult>> promise;
};

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
#if defined(DEBUG)
	/* Destructor */
	~PromiseStateBase() noexcept(false);
#endif
	/* Make terminator */
	void finish();
protected:
	/*
	 * Promise states:
	 *
	 *    ┌───────────┬──────────┬──────────┬──────────┐
	 *    │  Name of  │  Has a   │  Has an  │ Callback │
	 *    │   state   │  result  │  error   │  called  │
	 *    ├———————————┼——————————┼——————————┼——————————┤
	 *  A │pending    │ no       │ no       │ (no)     │
	 *  B │resolved   │ yes      │ (no)     │ no       │
	 *  C │rejected   │ (no)     │ yes      │ no       │
	 *  D │completed  │ *        │ *        │ yes      │
	 *    └───────────┴──────────┴──────────┴──────────┘
	 *
	 *       * = don't care (any value)
	 *   (val) = value is implicit, enforced due to value of some other field
	 *
	 * State transition graph:
	 *
	 *       ┌──▶ B ──┐
	 *   A ──┤        ├──▶ D
	 *       └──▶ C ──┘
	 *
	 * State descriptions/conditions:
	 *
	 *   A: pending
	 *     initial state, nothing done
	 *
	 *   B: resolved
	 *     promise represents a successful operation, a result value has been
	 *     assigned
	 *
	 *   C: rejected
	 *     promise represents a failed operation, an error has been assigned
	 *
	 *   D: completed
	 *     promise has been resolved/rejected, then the appropriate callback has
	 *     been called
	 *
	 */
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

/***
 * Typed promise state
 *
 * You will never use this class directly, instead use the Promise<T>, which
 * encapsulates a shareable PromiseState<T>.
 *
 * The PromiseState<T> is aggregated onto the Promise<T> via the member (->)
 * operator.
 */

template <typename Result>
class PromiseState : public PromiseStateBase {
public:
	static_assert(
		is_same<Result, typename remove_cvr<Result>::type>::value,
		"Type parameter for promise internal state must not be cv-qualified or a reference");
	/* "then" and "except" returning new value or next promise */
	template <typename NextResult = Result>
		using NextFunc = function<NextResult(Result)>;
	template <typename NextResult = Result>
		using ExceptFunc = function<NextResult(exception_ptr)>;
	/* "then" and "except" ending promise chain */
	using NextVoidFunc = NextFunc<void>;
	using ExceptVoidFunc = ExceptFunc<void>;
	/* "finally" doesn't return any value */
	using FinallyFunc = function<void()>;
	/* Default constructor */
	PromiseState() = default;
	using PromiseStateBase::PromiseStateBase;
	/* No copy/move constructors */
	PromiseState(PromiseState<Result>&&) = delete;
	PromiseState(const PromiseState<Result>&) = delete;
	/* Resolve */
	void resolve(Result result);
	/* Reject */
	using PromiseStateBase::reject;
	/* Forwards the result of this promise to another promise */
	template <typename NextPromise>
	void forward_to(NextPromise next);
	/* Bind a callback pack */
	template <typename Range>
	Promise<Range> then(const promise::callback_pack<Range, Result>);
	/* Then (callbacks return immediate value) */
	template <typename Next>
	using ThenResult = typename result_of<Next(Result)>::type;
	template <
		typename Next,
		typename NextResult = ThenResult<Next>,
		typename Except = ExceptFunc<NextResult>,
		typename Finally = FinallyFunc,
		typename = typename enable_if<
			!is_promise<NextResult>::value &&
			!is_void<NextResult>::value &&
			!is_callback_pack<Next>::value
		>::type>
	Promise<NextResult> then(
			Next next_func,
			Except except_func = nullptr,
			Finally finally_func = nullptr);
	/* Then (callbacks return promise) */
	template <
		typename Next,
		typename NextPromise = ThenResult<Next>,
		typename NextResult = typename NextPromise::result_type,
		typename Except = ExceptFunc<NextPromise>,
		typename Finally = FinallyFunc,
		typename = typename enable_if<
			is_promise<NextPromise>::value &&
			!is_callback_pack<Next>::value
		>::type>
	Promise<NextResult> then(
			Next next_func,
			Except except_func = nullptr,
			Finally finally_func = nullptr);
	/* Then (end promise chain) */
	template <
		typename Next,
		typename NextResult = ThenResult<Next>,
		typename Except = ExceptVoidFunc,
		typename Finally = FinallyFunc,
		typename = typename enable_if<
			is_void<NextResult>::value &&
			!is_callback_pack<Next>::value
		>::type>
	void then(
		Next next_func,
		Except except_func = nullptr,
		Finally finally_func = nullptr);
	/* Except */
	template <
		typename Except,
		typename NextPromise = typename result_of<Except(exception_ptr)>::type,
		typename NextResult = typename NextPromise::result_type,
		typename = typename enable_if<
			is_promise<NextPromise>::value
		>::type>
	Promise<NextResult> except(
		Except except_func)
			{ return then<NextFunc<Promise<NextResult>>>(nullptr, except_func); }
	template <
		typename Except,
		typename NextResult = typename result_of<Except(exception_ptr)>::type,
		typename = typename enable_if<
			!is_promise<NextResult>::value &&
			!is_void<NextResult>::value
		>::type>
	Promise<NextResult> except(
		Except except_func)
			{ return then<NextFunc<NextResult>>(nullptr, except_func); }
	/* Except (end promise chain) */
	template <
		typename Except,
		typename NextResult = typename result_of<Except(exception_ptr)>::type,
		typename = typename enable_if<
			is_void<NextResult>::value
		>::type>
	void except(
		Except except_func)
			{ then<NextVoidFunc>(nullptr, except_func); }
	/* Finally */
	template <typename Finally>
	Promise<Result> finally(
		Finally finally_func)
			{ return then<NextFunc<Result>>(nullptr, nullptr, finally_func); }
	/* Make terminator with finalizer */
	template <typename Finally>
	void finish(Finally finally_func)
		{ finally(finally_func)->finish(); }
	using PromiseStateBase::finish;
protected:
	/* Get/set promise result */
	void set_result(ensure_locked, Result&& value);
	Result get_result(ensure_locked);
private:
	Result result;
	/* Helper functions to pass current value onwards if no 'next' callback */
	template <typename NextResult,
		typename = typename enable_if<is_same<Result, NextResult>::value>::type>
	static NextResult forward_result(Result result);
	template <typename NextResult, int dummy = 0,
		typename = typename enable_if<!is_same<Result, NextResult>::value>::type>
	static NextResult forward_result(Result result);
	template <typename NextResult>
	static Promise<NextResult> default_next(Result result);
	template <typename NextResult>
	static Promise<NextResult> default_except(exception_ptr error);
	static void default_finally();
};

static_assert(is_promise<Promise<int>>::value, "Promise traits test #1 failed");
static_assert(!is_promise<int>::value, "Promise traits test #2 failed");
static_assert(result_of_promise_is<Promise<int>, int>::value, "Promise traits test #3 failed");
static_assert(!result_of_promise_is<Promise<int>, long>::value, "Promise traits test #4 failed");
static_assert(!result_of_promise_is<int, int>::value, "Promise traits test #5 failed");
static_assert(result_of_not_promise_is<int, int>::value, "Promise traits test #6 failed");
static_assert(!result_of_not_promise_is<int, long>::value, "Promise traits test #7 failed");
static_assert(!result_of_not_promise_is<Promise<int>, int>::value, "Promise traits test #8 failed");

}

#ifndef promise_tcc
#include "promise.tcc"
#endif
