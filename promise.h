#pragma once
#include <functional>
#include <atomic>
#include <memory>
#include <cstddef>
#include <tuple>
#include <vector>
#include <type_traits>
#include "spinlock.h"

namespace mark {

/*
 * Promise, inspired by the JavaScript Q library.
 *
 * A promise object encapsulates a value that may not exist yet.
 *
 * When the value has been acquired, then the promise is "resolved" and a "next"
 * callback is called with the value.
 *
 * If the value cannot be resolved, then it is rejected with an exception (or a
 * string that is mapped to an exception) and the "except" callback is called
 * instead.
 *
 * A promise is "completed" when it has either been "rejected" or "resolved".
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
 *       calls either next or handler
 *       (calls next iff promise is resolved)
 *       (calls handler iff promise is rejected)
 *   - promise.except(handler).then(next)
 *       may call handler, may call next
 *       (calls handler iff promise is rejected)
 *       (calls next iff (promise is resolved or handler does not throw))
 *   - promise.then(next).except(handler)
 *       may call next, may call handler
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
 *   - if next/handler/finally throws, the next promise is rejected.  handler is
 *     NOT called when next/finally throws, as handler operates on this promise,
 *     not the next one.
 *   - next promise is resolved iff either:
 *      - this promise is resolved and none of the callbacks throw.
 *      - this promise is rejected and has a handler which doesn't throw.
 *   - next promise is rejected iff either:
 *      - this promise is resolved but a callback throws.
 *      - this promise is rejected and either has no handler or the handler
 *        throws.
 *   - if next promise is rejected, exception thrown by finally takes priority
 *     over exception thrown by next/except.
 *
 * except(handler, finally = nullptr):
 *   - shorthand for then(nullptr, handler, finally)
 *
 * finally(finally):
 *   - shorthand for then(nullptr, nullptr, finally)
 */

using namespace std;

/*
 * PromiseInternal is internal representation of a promise
 */

class PromiseInternalBase;

template <typename Result> class PromiseInternal;

/*
 * Promise wraps the PromiseInternal in a smart pointer for efficient copying
 */

class PromiseBase;

template <typename Result> class Promise;

/*
 * Test if a type represents a promise wrapper
 *
 * Example usage: enable_if<is_promise<T>::value>::type
 */

template <typename T>
using is_promise = is_base_of<PromiseBase, T>;

class PromiseBase { };

/*
 * Promises should always be passed around in this wrapper, to avoid
 * copying/moving of the larger promise structure (and whatever data may be
 * captured in its closures or result).  The internal promise object has no move
 * or copy constructors, intentionally.
 *
 * The default (no-parameter) constructor for Result type and for NextResult
 * type must not throw - promise behaviour is undefined if this happens and
 * promise chains will most likely break in such conditions.
 */

template <typename Result>
class Promise : PromiseBase {
	static_assert(!is_void<Result>::value, "Void promises are no longer supported");
public:
	using result_type = Result;
	/* Promise */
	Promise();
	/* Resolved promise */
	Promise(Result const& result);
	/* Rejected promise */
	Promise(const nullptr_t dummy, exception_ptr error);
	Promise(const nullptr_t dummy, const string& error);
	/* Default copy/move constructors */
	Promise(const Promise<Result>&);
	Promise<Result>& operator =(const Promise<Result>&);
	Promise(Promise<Result>&&);
	Promise<Result>& operator =(Promise<Result>&&);
	/*
	 * Get wrapped promise (for accessing methods e.g.
	 * then/except/finally/resolve/reject)
	 */
	PromiseInternal<Result> *operator ->() const;
private:
	shared_ptr<PromiseInternal<Result>> promise{nullptr};
};

/***
 * Non-templated base class for promises (reduce code bloat)
 */

class PromiseInternalBase {
public:
	/* Reject */
	void reject(exception_ptr error);
	void reject(const string& error);
	/* Default constructor */
	PromiseInternalBase();
	/* No copy/move constructor */
	PromiseInternalBase(PromiseInternalBase const&) = delete;
	PromiseInternalBase(PromiseInternalBase&&) = delete;
	/* Destructor throws logic_error on uncompleted promise */
	~PromiseInternalBase();
	/* Construct a rejected promise */
	PromiseInternalBase(const nullptr_t dummy, exception_ptr error);
	PromiseInternalBase(const nullptr_t dummy, const string& error);
protected:
	/*
	 * One of these is called when the promise is resolved/rejected if callbacks
	 * are already bound.  If the promise does not already have callbacks bound
	 * when it is resolved/rejected, then the result/error is stored, and the
	 * appropriate one of these functions is called as soon as callbacks are
	 * bound to the promise.
	 */
	function<void()> on_resolve{nullptr};
	function<void()> on_reject{nullptr};
	/*
	 * We release the callbacks after using them, so this variable is used to
	 * track whether we have bound callbacks at all, so that re-binding cannot
	 * happen after unbinding (and completion).
	 */
	bool callbacks_assigned{false};
	/*
	 * State of the promise:
	 *
	 *  no result  |  has result    |  relevant callback(s)
	 *  or error   |  value/error   |  have been called
	 *             |                |
	 *
	 *              --> resolved -->
	 *  pending -->|                |--> completed
	 *              --> rejected -->
	 *
	 */
	enum class promise_state { pending, rejected, resolved, completed };
	promise_state state{promise_state::pending};
	/*
	 * The state_lock must be acquired before these are called.  If handlers
	 * have been assigned, then these functions call the handlers, otherwise
	 * they do nothing.
	 */
	void resolved();
	void rejected();
	/* Called when callbacks have been assigned */
	void assigned_callbacks();
	/* Called on resolve/reject after callbacks */
	void completed_and_called();
	/*
	 * Used for fast userspace mutex (spinlock).
	 *
	 * Locked when:
	 *   - binding callbacks
	 *   - storing result/error
	 *   - calling callbacks
	 *
	 * Locking this should never take long in a well-formed program, as the only
	 * operation which holds this promise for a long time is calling callbacks.
	 * Calling callbacks can only happen when the other two locking operations
	 * have already been called, and they can only each be called once per
	 * instance.  This is why I chose a userspace spinlock over std::mutex.
	 */
	atomic_flag state_lock{ATOMIC_FLAG_INIT};
	/* Sanity checks */
	void ensure_is_still_pending() const;
	void ensure_is_unbound() const;
	/* Stores caught error */
	exception_ptr error{};
};

/***
 * Internal class for promises
 *
 * You will never use this class directly, instead use the Promise<T> wrapper,
 * which encapsulates a PromiseInternal<T>.
 *
 * The PromiseInternal<T> is aggregated onto the Promise<T> via the member (->)
 * operator, e.g.
 *
 * Promise<string> get_username(int user_id)
 * {
 *     Promise<string> promise;
 *
 *     run_query(
 *         "SELECT `username` FROM `user` WHERE `user`=" + to_string(user_id),
 *         "username",
 *         [promise] (string& result) {
 *             promise->resolve(result);
 *         },
 *         [promise] (string& error) {
 *             promise->reject(result);
 *         });
 *
 *     return promise;
 * }
 *
 * get_user_id(session)
 *     ->then<string>(get_username)
 *     ->then<bool>(send_string(connection))
 *     ->except([] (auto error) {
 *         ( handle error )
 *     });
 *
 */

template <typename Result>
class PromiseInternal : public PromiseInternalBase {
	static_assert(!is_void<Result>::value, "Void promises are no longer supported");
public:
	/* "then" and "except" ending promise chain */
	using ThenVoidFunc = function<void(Result&)>;
	using ExceptVoidFunc = function<void(exception_ptr)>;
	/* "then" and "except" returning new value to next promise */
	template <typename NextResult = Result>
		using ThenFunc = function<NextResult(Result&)>;
	template <typename NextResult = Result>
		using ExceptFunc = function<NextResult(exception_ptr)>;
	/* "finally" doesn't return any value */
	using FinallyFunc = function<void()>;
	/* Default constructor */
	PromiseInternal() = default;
	using PromiseInternalBase::PromiseInternalBase;
	/* No copy/move constructors */
	PromiseInternal(PromiseInternal<Result>&&) = delete;
	PromiseInternal(const PromiseInternal<Result>&) = delete;
	/* Construct an immediately resolved (copy/move result) */
	PromiseInternal(Result const& result);
	/* Resolve (copy/move result) */
	void resolve(Result const& result);
	using PromiseInternalBase::reject;
	/* Forwards the result of this promise to another promise */
	void forward_to(Promise<Result> next);
	/* Then (callbacks return promise) */
	template <typename NextResult>
	Promise<NextResult> then_p(
			const ThenFunc<Promise<NextResult>> next,
			const ExceptFunc<Promise<NextResult>> handler = nullptr,
			const FinallyFunc finally = nullptr);
	/* Then (callbacks return immediate value) */
	template <typename NextResult>
	Promise<NextResult> then_i(
			const ThenFunc<NextResult> next,
			const ExceptFunc<NextResult> handler = nullptr,
			const FinallyFunc finally = nullptr);
	/* "then_?" function aliases, overloaded as "then" */
	template <typename NextResult,
		typename = typename enable_if<is_promise<NextResult>::value>::type>
	Promise<typename NextResult::result_type> then(
			const ThenFunc<Promise<typename NextResult::result_type>> next,
			const ExceptFunc<Promise<typename NextResult::result_type>> handler = nullptr,
			const FinallyFunc finally = nullptr)
		{ return then_p<typename NextResult::result_type>(next, handler, finally); }
	template <typename NextResult,
		typename = typename enable_if<!is_promise<NextResult>::value>::type>
	Promise<NextResult> then(
			const ThenFunc<NextResult> next,
			const ExceptFunc<NextResult> handler = nullptr,
			const FinallyFunc finally = nullptr)
		{ return then_i<NextResult>(next, handler, finally); }
	/* Except */
	template <typename T>
		Promise<T> except(const ExceptFunc<T> handler)
			{ return then<T>(nullptr, handler); };
	/* Finally */
	template <typename T = Result>
		Promise<T> finally(const FinallyFunc finally)
			{ return then<T>(nullptr, nullptr, finally); };
	/* Then (end promise chain) */
	void then(
		const ThenVoidFunc next,
		const ExceptVoidFunc handler = nullptr,
		const FinallyFunc finally = nullptr);
	/* Except (end promise chain) */
	void except(const ExceptVoidFunc handler);
	/* Finally (end promise chain) */
	void finally(const FinallyFunc finally);
private:
	/*
	 * Stores the result so that we can avoid race conditions between binding
	 * callbacks to the promise and resolving/rejecting the promise.
	 */
	Result result{};
	/* Pass current value onwards if no 'next' callback */
	template <typename NextResult>
		NextResult next_default_value(const Result& value) const;
};

/*** Utils ***/
namespace promise {

/* Construct a resolved promise */

Promise<nullptr_t> begin_chain();

template <typename Result>
Promise<Result> resolved(Result const& result);

/* Construct a rejected promise */

template <typename Result>
Promise<Result> rejected(exception_ptr error);

template <typename Result>
Promise<Result> rejected(const string& error);

/*
 * Convert function to promise factory
 *
 * Takes function func and returns a new function which when invoked, returns a
 * promise and calls func.  The return value of func is used to resolve the
 * promise.  If func throws, then the exception is caught and the promise is
 * rejected.
 *
 * Returns nullptr iff func==nullptr
 */
template <typename Result, typename... Args>
function<Promise<Result>(Args...)> make_factory(
	function<Result(Args...)>& func);

/*** Combinators ***/

/*
 * Returns a Promise<tuple> that resolves to a tuple of results when all
 * given promises have resolved, or which rejects if any of them reject
 * (without waiting for the others to complete).
 *
 * Think of this as a map() operation over a possible heterogenous ordered set
 * of promises, which transforms the set into a new ordered set containing the
 * results of the promises.
 */
template <typename... Result>
Promise<tuple<Result...>> combine(Promise<Result>&&... promise);

/*
 * Takes an iterable of homogenous promises and returns a single promise that
 * resolves to a vector containing the results of the given promises.
 *
 * Think of this as a map() operation over a heterogenous ordered set of
 * promises, transforming the set into an ordered set of results.
 */

template <typename It, typename Result = typename It::value_type::result_type>
Promise<vector<Result>> combine(It first, It last, const size_t size);

template <typename It, typename Result = typename It::value_type::result_type>
Promise<vector<Result>> combine(It first, It last);

template <typename List, typename Result = typename List::value_type::result_type>
Promise<vector<Result>> combine(List promises);

}

}

#ifndef promise_tcc
#include "promise.tcc"
#endif
