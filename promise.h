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

#if defined(DEBUG)
#define SAFE_PROMISES
#endif

namespace kaiu {

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
