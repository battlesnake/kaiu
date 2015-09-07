#pragma once
#include <functional>
#include <atomic>
#include <mutex>
#include <memory>
#include <cstddef>
#include <tuple>
#include <vector>
#include <type_traits>
#include "self_managing.h"

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

template <typename T>
using remove_cvr = remove_reference<typename remove_cv<T>::type>;

class PromiseStateBase;

template <typename Result> class PromiseState;

template <typename Result> class Promise;

/***
 * Test if a type represents a promise
 *
 * Example usage: enable_if<is_promise<T>::value>::type
 *
 * Could use is_base_of with PromiseBase (a since deleted base class), this way
 * seems more idiomatic.
 */

template <typename T>
struct is_promise {
	static constexpr auto value = false;
};

template <typename T>
struct is_promise<Promise<T>> {
	static constexpr auto value = true;
};

namespace detail {
	template <bool, typename T> struct result_of_promise_helper { };
	template <typename T> struct result_of_promise_helper<true, T>
		{ using type = typename T::result_type; };
	template <bool, typename T> struct result_of_not_promise_helper { };
	template <typename T> struct result_of_not_promise_helper<false, T>
		{ using type = T; };
}

/* result_of_promise<T>: is_promise<T> ? T::result_type : fail */
template <typename T>
using result_of_promise = detail::result_of_promise_helper<is_promise<T>::value, T>;

/* result_of_not_promise<T>: is_promise<T> ? fail : T */
template <typename T>
using result_of_not_promise = detail::result_of_not_promise_helper<is_promise<T>::value, T>;

/* is_promise<T> && R==T::result_type */
template <typename T, typename R>
struct result_of_promise_is {
private:
	template <typename U>
	static is_same<typename result_of_promise<U>::type, typename remove_cv<R>::type> check(int);
	template <typename>
	static std::false_type check(...);
public:
	static constexpr auto value = decltype(check<T>(0))::value;
};

/* !is_promise<T> && R==T */
template <typename T, typename R>
struct result_of_not_promise_is {
private:
	template <typename U>
	static is_same<typename result_of_not_promise<U>::type, typename remove_cv<R>::type> check(int);
	template <typename>
	static std::false_type check(...);
public:
	static constexpr auto value = decltype(check<T>(0))::value;
};

namespace promise {

template <typename Result, typename DResult = typename remove_cvr<Result>::type>
Promise<DResult> resolved(Result result);

template <typename Result, typename DResult = typename remove_cvr<Result>::type>
Promise<DResult> rejected(exception_ptr error);

/*
 * Can be used to pack callbacks for a promise, for use in monad syntax
 */
template <typename Range, typename Domain>
class callback_pack {
public:
	using range_type = Range;
	using domain_type = Domain;
	static constexpr bool is_terminal = is_void<Range>::value;
	using Next = typename conditional<
		is_terminal,
		function<void(Domain)>,
		function<Promise<Range>(Domain)>>::type;
	using Handler = typename conditional<
		is_terminal,
		function<void(exception_ptr)>,
		function<Promise<Range>(exception_ptr)>>::type;
	using Finalizer = function<void()>;
	/* Pack callbacks */
	explicit callback_pack(const Next next, const Handler handler = nullptr, const Finalizer finalizer = nullptr);
	/* Bind operator, for chaining callback packs */
	template <typename NextRange>
	auto bind(callback_pack<NextRange, Range> after) const;
	/* Bind callbacks to promise */
	Promise<Range> operator () (const Promise<Domain> d) const
		{ return call(d); }
	Promise<Range> operator () (Domain d) const
		{ return call(promise::resolved<Domain>(move(d))); }
	const Next next;
	const Handler handler;
	const Finalizer finalizer;
private:
	Promise<Range> call (const Promise<Domain> d) const
		{ return d->then(*this); }
	Promise<Range> rejected(exception_ptr error) const
		{ return call(promise::rejected<Domain>(error)); }
};

}

template <typename T>
struct is_callback_pack : false_type { };
template <typename Range, typename Domain>
struct is_callback_pack<promise::callback_pack<Range, Domain>> : true_type { };
template <typename T>
struct is_terminal_callback_pack :
	integral_constant<bool, is_callback_pack<T>::value &&
		is_void<typename T::range_type>::value> { };

/*
 * Result type of a promise cannot derive from (or be) PromiseLike.
 *
 * PromiseLike should support ->resolve ->reject ->forward_to:
 *   void resolve(T)
 *   void reject(exception_ptr)
 *   void reject(const string&)
 *   void forward_to(PromiseLike)
 */
class PromiseLike {
};

template <typename T>
using is_promise_like = is_base_of<PromiseLike, T>;

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

static_assert(is_promise<Promise<int>>::value, "Promise traits test #1 failed");
static_assert(!is_promise<int>::value, "Promise traits test #2 failed");
static_assert(result_of_promise_is<Promise<int>, int>::value, "Promise traits test #3 failed");
static_assert(!result_of_promise_is<Promise<int>, long>::value, "Promise traits test #4 failed");
static_assert(!result_of_promise_is<int, int>::value, "Promise traits test #5 failed");
static_assert(result_of_not_promise_is<int, int>::value, "Promise traits test #6 failed");
static_assert(!result_of_not_promise_is<int, long>::value, "Promise traits test #7 failed");
static_assert(!result_of_not_promise_is<Promise<int>, int>::value, "Promise traits test #8 failed");

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
	Promise<Range> then(const promise::callback_pack<Range, Result>&);
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
			!is_void<NextResult>::value
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
			is_promise<NextPromise>::value
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
			is_void<NextResult>::value
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

/*** Utils ***/
namespace promise {

/* Construct a resolved promise */

template <typename Result, typename DResult = typename remove_cvr<Result>::type>
Promise<DResult> resolved(Result result);

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
 * kaiu::promise::task.
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
 *
 * Using std::vector for result rather than std::array, as the latter does not
 * support move semantics.
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

#include "promise_monad.h"
