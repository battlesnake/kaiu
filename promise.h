#pragma once
#include <functional>
#include <atomic>
#include <mutex>
#include <memory>
#include <cstddef>
#include <tuple>
#include <vector>
#include <type_traits>

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

/***
 * Test if a type represents a promise wrapper
 *
 * Example usage: enable_if<is_promise<T>::value>::type
 *
 * Could use is_base_of with PromiseBase, this way seems more idiomatic.
 */

template <typename T>
struct is_promise {
private:
	template <typename U>
	static integral_constant<bool, U::is_promise> check(int);
	template <typename>
	static std::false_type check(...);
public:
	static constexpr auto value = decltype(check<T>(0))::value;
};

/***
 * Promises should always be passed around in this wrapper, to avoid
 * copying/moving of the larger promise structure (and whatever data may be
 * captured in its closures or result).  The internal promise object has no move
 * or copy constructors, intentionally.
 *
 * The default (no-parameter) constructor for Result type and for NextResult
 * type must not throw - promise behaviour is undefined if this happens and
 * promise chains will most likely break in such conditions.
 */

class PromiseBase {
};

template <typename Result>
class Promise : public PromiseBase {
	static_assert(!is_void<Result>::value, "Void promises are no longer supported");
public:
	static constexpr bool is_promise = true;
	using result_type = Result;
	/* Promise */
	Promise();
	/* Resolved promise */
	Promise(Result const& result);
	template <typename T>
	static Promise<Result> resolved(T result);
	/* Rejected promise */
	Promise(const nullptr_t dummy, exception_ptr error);
	Promise(const nullptr_t dummy, const string& error);
	template <typename T>
	static Promise<Result> rejected(T error);
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
	Promise(PromiseInternal<Result> * const promise);
	shared_ptr<PromiseInternal<Result>> promise;
};

static_assert(is_promise<Promise<int>>::value, "is_promise failed (test 1)");
static_assert(!is_promise<int>::value, "is_promise failed (test 2)");

/***
 * Non-templated base class for promises (handles everything except resolution)
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
	/* Construct a rejected promise */
	PromiseInternalBase(const nullptr_t dummy, exception_ptr error);
	PromiseInternalBase(const nullptr_t dummy, const string& error);
	/* Destructor */
	~PromiseInternalBase() noexcept(false);
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
	 * Locked when:
	 *   - binding callbacks
	 *   - storing result/error
	 *   - calling callbacks
	 */
	mutex state_lock;
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
 *     ->then(get_username)
 *     ->then(send_string(connection))
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
	/* Then (callbacks return immediate value) */
	template <
		typename Then,
		typename NextResult = typename result_of<Then(Result&)>::type,
		typename Except = ExceptFunc<NextResult>,
		typename Finally = FinallyFunc,
		typename = typename enable_if<
			!is_promise<NextResult>::value &&
			!is_void<NextResult>::value
		>::type>
	Promise<NextResult> then(
			const Then /* ThenFunc<NextResult> */ then_func,
			const Except /* ExceptFunc<NextResult> */ except_func = nullptr,
			const Finally /* FinallyFunc */ finally_func = nullptr);
	/* Then (callbacks return promise) */
	template <
		typename Then,
		typename NextPromise = typename result_of<Then(Result&)>::type,
		typename NextResult = typename NextPromise::result_type,
		typename Except = ExceptFunc<NextPromise>,
		typename Finally = FinallyFunc,
		typename = typename enable_if<
			is_promise<NextPromise>::value
		>::type>
	Promise<NextResult> then(
			const Then /* ThenFunc<Promise<NextResult>> */ then_func,
			const Except /* ExceptFunc<Promise<NextResult>> */ except_func = nullptr,
			const Finally /* FinallyFunc */ finally_func = nullptr);
	/* Then (end promise chain) */
	template <
		typename Then,
		typename NextResult = typename result_of<Then(Result&)>::type,
		typename Except = ExceptVoidFunc,
		typename Finally = FinallyFunc,
		typename = typename enable_if<
			is_void<NextResult>::value
		>::type>
	void then(
		const Then /* ThenVoidFunc */ then_func,
		const Except /* ExceptVoidFunc */ except_func = nullptr,
		const Finally /* FinallyFunc */ finally_func = nullptr);
	/* Except */
	template <
		typename Except,
		typename NextPromise = typename result_of<Except(exception_ptr)>::type,
		typename NextResult = typename NextPromise::result_type,
		typename = typename enable_if<
			is_promise<NextPromise>::value
		>::type>
	Promise<NextResult> except(
		const Except /* ExceptFunc<Promise<NextResult>> */ except_func)
			{ return then<ThenFunc<Promise<NextResult>>>(nullptr, except_func); };
	template <
		typename Except,
		typename NextResult = typename result_of<Except(exception_ptr)>::type,
		typename = typename enable_if<
			!is_promise<NextResult>::value &&
			!is_void<NextResult>::value
		>::type>
	Promise<NextResult> except(
		const Except /* ExceptFunc<NextResult> */ except_func)
			{ return then<ThenFunc<NextResult>>(nullptr, except_func); };
	/* Except (end promise chain) */
	template <
		typename Except,
		typename NextResult = typename result_of<Except(exception_ptr)>::type,
		typename = typename enable_if<
			is_void<NextResult>::value
		>::type>
	void except(
		const Except /* ExceptVoidFunc */ except_func)
			{ then<void>(nullptr, except_func); };
	/* Finally */
	template <typename Finally>
	Promise<Result> finally(
		const Finally /* FinallyFunc */ finally_func)
			{ return then<ThenFunc<Result>>(nullptr, nullptr, finally_func); };
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
 * Since func returns synchronously, the promise factory will evaluate func
 * BEFORE returning.  This is not a magic way to make synchronous functions
 * asynchronous, it just makes useable in promise chains.  For a magic way to
 * make synchronous functions asynchronous, use std::thread or
 * mark::promise::task.
 *
 * Returns nullptr iff func==nullptr
 */

template <typename Result, typename... Args>
using Factory = function<Promise<Result>(Args...)>;

template <typename Result, typename... Args>
Factory<Result, Args...> factory(Result (*func)(Args...));

template <typename Result, typename... Args>
Factory<Result, Args...> factory(function<Result(Args...)> func);

/*** Combinators ***/

/*
 * Returns a Promise<tuple> that resolves to a tuple of results when all
 * given promises have resolved, or which rejects if any of them reject
 * (without waiting for the others to complete).
 *
 * Think of this as a map() operation over a possibly heterogenous ordered set
 * of promises, which transforms the set into a tuple containing the results of
 * the promises.
 */
template <typename... Result>
Promise<tuple<Result...>> combine(Promise<Result>&&... promise);

/*
 * Takes an iterable of homogenous promises and returns a single promise that
 * resolves to a vector containing the results of the given promises.
 *
 * Think of this as a map() operation over a homogenous ordered set of promises,
 * transforming the set into an vector containing the results of the promises.
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
