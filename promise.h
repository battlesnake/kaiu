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

template <typename T>
using remove_cvr = remove_reference<typename remove_cv<T>::type>;

class PromiseStateBase;

template <typename Result> class PromiseState;

class PromiseBase;

template <typename Result> class Promise;

/***
 * Test if a type represents a promise
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
 * Promise class
 *
 * The default (no-parameter) constructor for Result type and for NextResult
 * type must not throw - promise behaviour is undefined if this happens and
 * promise chains guarantees will most likely break in such conditions.
 */

class PromiseBase {
protected:
	void assign_weak_reference(const shared_ptr<PromiseStateBase> ref);
};

template <typename Result>
class Promise : public PromiseBase {
	static_assert(!is_void<Result>::value, "Void promises are no longer supported");
	static_assert(!is_promise<Result>::value, "Promise<Promise<T>> is invalid, use Promise<T> instead");
public:
	using DResult = typename remove_cvr<Result>::type;
	using RResult = DResult&;
	using XResult = DResult&&;
	static constexpr bool is_promise = true;
	using result_type = Result;
	/* Promise */
	Promise();
	/* Resolved promise */
	Promise(DResult&& result);
	Promise(DResult const& result);
	/* Rejected promise */
	Promise(const nullptr_t dummy, exception_ptr error);
	Promise(const nullptr_t dummy, const string& error);
	/* Copy/move/cast constructors */
	Promise(const Promise<DResult>&);
	Promise(const Promise<RResult>&);
	Promise(const Promise<XResult>&);
	Promise(Promise<Result>&&) = default;
	/* Assignment */
	Promise<Result>& operator =(Promise<Result>&&) = default;
	Promise<Result>& operator =(const Promise<Result>&) = default;
	/* Access promise state (then/except/finally/resolve/reject) */
	PromiseState<DResult> *operator ->() const;
private:
	friend class PromiseState<DResult>;
	friend class Promise<DResult>;
	friend class Promise<RResult>;
	friend class Promise<XResult>;
	Promise(PromiseState<DResult> * const promise);
	Promise(shared_ptr<PromiseState<DResult>> const promise);
	shared_ptr<PromiseState<DResult>> promise;
};

static_assert(is_promise<Promise<int>>::value, "is_promise failed (test 1)");
static_assert(!is_promise<int>::value, "is_promise failed (test 2)");

/***
 * Untyped promise state
 */

class PromiseStateBase {
public:
	/* Reject */
	void reject(exception_ptr error);
	void reject(const string& error);
	/* Default constructor */
	PromiseStateBase();
	/* No copy/move constructor */
	PromiseStateBase(PromiseStateBase const&) = delete;
	PromiseStateBase(PromiseStateBase&&) = delete;
	/* Construct a rejected promise */
	PromiseStateBase(const nullptr_t dummy, exception_ptr error);
	PromiseStateBase(const nullptr_t dummy, const string& error);
	/* Destructor */
	~PromiseStateBase() noexcept(false);
	/* Make terminator */
	void finish();
protected:
	/*
	 * All protected functions require a lock_guard to have been acquired on the
	 * state_lock, this is enforced via a reference parameter that the compiler
	 * should be able to happily optimize out.
	 * TODO: Remove the lock-passing sanity-check before release.
	 */
	using ensure_locked = lock_guard<mutex> const &;
	mutex state_lock;
	/*
	 * State transitions for a promise:
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
	exception_ptr& get_error(ensure_locked);
	/* Make this promise a terminator */
	void set_terminator(ensure_locked);
	/* Prevent destruction via self-reference.  Does not count locks/unlocks. */
	void set_locked(bool locked);
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
	/* Self-references, used to control lifetime of the state object */
	weak_ptr<PromiseStateBase> self_weak_reference;
	shared_ptr<PromiseStateBase> self_strong_reference;
	friend class PromiseBase;
};

/***
 * Typed promise state
 *
 * You will never use this class directly, instead use the Promise<T>, which
 * encapsulates a shareable PromiseState<T>.
 *
 * The PromiseState<T> is aggregated onto the Promise<T> via the member (->)
 * operator.
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
class PromiseState : public PromiseStateBase {
public:
	static_assert(
		is_same<Result, typename remove_cvr<Result>::type>::value,
		"Type parameter for promise internal state must not be cv-qualified or a reference");
	/* "then" and "except" returning new value to next promise */
	template <typename NextResult = Result>
		using NextFunc = function<NextResult(const Result&)>;
	template <typename NextResult = Result>
		using ExceptFunc = function<NextResult(exception_ptr&)>;
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
	/* Construct an immediately resolved */
	PromiseState(const Result& result);
	/* Resolve */
	void resolve(const Result& result);
	/* Reject */
	using PromiseStateBase::reject;
	/* Forwards the result of this promise to another promise */
	void forward_to(Promise<Result> next);
	/* Then (callbacks return immediate value) */
	template <typename Next>
	using ThenResult = typename result_of<Next(Result&&)>::type;
	template <
		typename Next,
		typename NextResult = ThenResult<Next>,
		typename Except = ExceptFunc<NextResult>,
		typename Finally = FinallyFunc,
		typename = typename enable_if<
			!is_promise<NextResult>::value &&
			!is_void<NextResult>::value
		>::type>
	Promise<NextResult> then(
			Next /* NextFunc<NextResult> */ next_func,
			Except /* ExceptFunc<NextResult> */ except_func = nullptr,
			Finally /* FinallyFunc */ finally_func = nullptr);
	/* Then (callbacks return promise) */
	template <
		typename Next,
		typename NextPromise = ThenResult<Next>,
		typename NextResult = typename NextPromise::result_type,
		typename Except = ExceptFunc<NextPromise>,
		typename Finally = FinallyFunc,
		typename = typename enable_if<
			is_promise<NextPromise>::value
		>::type>
	Promise<NextResult> then(
			Next /* NextFunc<Promise<NextResult>> */ next_func,
			Except /* ExceptFunc<Promise<NextResult>> */ except_func = nullptr,
			Finally /* FinallyFunc */ finally_func = nullptr);
	/* Then (end promise chain) */
	template <
		typename Next,
		typename NextResult = ThenResult<Next>,
		typename Except = ExceptVoidFunc,
		typename Finally = FinallyFunc,
		typename = typename enable_if<
			is_void<NextResult>::value
		>::type>
	void then(
		Next /* NextVoidFunc */ next_func,
		Except /* ExceptVoidFunc */ except_func = nullptr,
		Finally /* FinallyFunc */ finally_func = nullptr);
	/* Except */
	template <
		typename Except,
		typename NextPromise = typename result_of<Except(exception_ptr&)>::type,
		typename NextResult = typename NextPromise::result_type,
		typename = typename enable_if<
			is_promise<NextPromise>::value
		>::type>
	Promise<NextResult> except(
		Except /* ExceptFunc<Promise<NextResult>> */ except_func)
			{ return then<NextFunc<Promise<NextResult>>>(nullptr, except_func); };
	template <
		typename Except,
		typename NextResult = typename result_of<Except(exception_ptr)>::type,
		typename = typename enable_if<
			!is_promise<NextResult>::value &&
			!is_void<NextResult>::value
		>::type>
	Promise<NextResult> except(
		Except /* ExceptFunc<NextResult> */ except_func)
			{ return then<NextFunc<NextResult>>(nullptr, except_func); };
	/* Except (end promise chain) */
	template <
		typename Except,
		typename NextResult = typename result_of<Except(exception_ptr&)>::type,
		typename = typename enable_if<
			is_void<NextResult>::value
		>::type>
	void except(
		Except /* ExceptVoidFunc */ except_func)
			{ then<void>(nullptr, except_func); };
	/* Finally */
	template <typename Finally>
	Promise<Result> finally(
		Finally /* FinallyFunc */ finally_func)
			{ return then<NextFunc<Result>>(nullptr, nullptr, finally_func); };
protected:
	/* Get/set promise result */
	void set_result(ensure_locked, const Result& value);
	const Result& get_result(ensure_locked);
private:
	Result result;
	/* Helper functions to pass current value onwards if no 'next' callback */
	template <typename NextResult,
		typename = typename enable_if<is_same<Result, NextResult>::value>::type>
	static const NextResult& forward_result(const Result& result);
	template <typename NextResult, int dummy = 0,
		typename = typename enable_if<!is_same<Result, NextResult>::value>::type>
	static const NextResult& forward_result(const Result& result);
	template <typename NextResult>
	static Promise<NextResult> default_next(const Result& result);
	template <typename NextResult>
	static Promise<NextResult> default_except(exception_ptr& error);
	static void default_finally();
};

/*** Utils ***/
namespace promise {

/* Construct a resolved promise */

template <typename Result, typename DResult = typename remove_cvr<Result>::type>
Promise<DResult> resolved(Result&& result);

template <typename Result, typename DResult = typename remove_cvr<Result>::type>
Promise<DResult> resolved(const Result& result);

/* Construct a rejected promise */

template <typename Result, typename DResult = typename remove_cvr<Result>::type>
Promise<DResult> rejected(exception_ptr error);

template <typename Result, typename DResult = typename remove_cvr<Result>::type>
Promise<DResult> rejected(const string& error);

/*
 * Convert function to promise factory
 *
 * Result(Args...) => Promise<Result>(Args...)
 *
 * Takes function func and returns a new function which when invoked, returns a
 * promise and calls func.  The return value of func is used to resolve the
 * promise.  If func throws, then the exception is caught and the promise is
 * rejected.
 *
 * Since func returns synchronously, the promise factory will evaluate func
 * BEFORE returning.  This is not a magic way to make synchronous functions
 * asynchronous, it just makes them useable in promise chains.  For a magic way
 * to make synchronous functions asynchronous, use std::thread or
 * mark::promise::task.
 *
 * Returns nullptr iff func==nullptr
 */

template <typename Result, typename... Args>
using Factory = function<Promise<Result>(Args...)>;

nullptr_t factory(nullptr_t);

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
Promise<tuple<typename decay<Result>::type...>> combine(Promise<Result>&&... promise);

/*
 * Takes an iterable of homogenous promises and returns a single promise that
 * resolves to a vector containing the results of the given promises.
 *
 * Think of this as a map() operation over a homogenous ordered set of promises,
 * transforming the set into an vector containing the results of the promises.
 */

template <typename It, typename Result = typename remove_cvr<typename It::value_type::result_type>::type>
Promise<vector<Result>> combine(It first, It last, const size_t size);

template <typename It, typename Result = typename remove_cvr<typename It::value_type::result_type>::type>
Promise<vector<Result>> combine(It first, It last);

template <typename List, typename Result = typename remove_cvr<typename List::value_type::result_type>::type>
Promise<vector<Result>> combine(List promises);

}

}

#ifndef promise_tcc
#include "promise.tcc"
#endif
