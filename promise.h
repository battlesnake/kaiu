#pragma once
#include <functional>
#include <atomic>
#include <memory>

namespace mark {

using namespace std;

template <typename Result>
class Promise_;

/*
 * Promises should always be passed around in this shared_ptr wrapper, to avoid
 * copying/moving of the larger promise structure (and whatever data may be
 * captured in its closures or result)
 */
template <typename Result>
class Promise : shared_ptr<Promise_<Result>> {
public:
	static Promise<Result> resolve(Result& value) {
		return promise(value);
	};
	static Promise<Result> reject(exception& error) {
		Promise<Result> promise;
		promise.reject(error);
		return promise;
	};
	template <typename... Args>
	static function<Promise<Result>(Args...)> wrap_function(function<Result(Args...)>& func) {
		return !func ? nullptr :
			[func] (Args&&... args) {
				try {
					return Promise::resolve(func(forward<Args>(args)...));
				} catch (exception& error) {
					return Promise::reject(error);
				}
		};
	};
	Promise() : shared_ptr<Promise_<Result>>(new Promise_<Result>) { };
	Promise(Result& value) : shared_ptr<Promise_<Result>>(new Promise_<Result>) {
		(*this)->resolve(value);
	};
};

/*
 * Internal class for promises (should always be wrapped in shared_ptr via
 * "Promise" class)
 *
 * THEN function can return:
 *   - void - next promise resolves with same value as this one.
 *   - new value of any type - next promise resolves with this new value.
 *   - throw exception - next promise is rejected.
 *
 * CATCH function can return:
 *   - void - next promise resolves with same value as this one.
 *   - new value of same type - next promise resolves with this new value.
 *   - throw exception - next promise is rejected.
 *
 * FINALLY function can return:
 *   - void - next promise is resolved/rejected with same value as this one.
 *   - throw exception - next promise is rejected.
 *
 * then(next, handler = nullptr, finally = nullptr):
 *   - if promise is resolved, next+finally are called.
 *   - if promise is rejected, handler+finally are called.
 *   - handler can re-throw.
 *   - if next or handler throws, finally is still called.
 *   - if next or finally throws, the next promise is rejected.  handler is NOT
 *     called in this case - the handler handles rejection of THIS promise only.
 *   - handler is called if THIS promise is rejected.
 *   - next promise is resolved if:
 *      - this promise is resolved and none of the callbacks throw.
 *      - this promise is rejected and has a handler (which provides the value).
 *   - next promise is rejected if:
 *      - this promise is resolved but a callback throws.
 *      - this promise is rejected and has no handler.
 *
 * except(handler, finally = nullptr):
 *   - shorthand for then(nullptr, handler, finally)
 *
 * finally(finally):
 *   - shorthand for then(nullptr, nullptr, finally)
 *
 * TODO:
 * Do we need the FINALLY construct?  Can we achieve this using RAII or
 * smart pointers somehow instead?
 */

template <typename Result>
class Promise_ {
public:
	using ForwardFunc = function<void(Result&)>&;
	template <typename NextResult = Result>
		using ThenFunc = function<Promise<NextResult>(Result&)>&;
	template <typename NextResult = Result>
		using ExceptFunc = function<Promise<NextResult>(exception&)>&;
	using FinallyFunc = function<void()>&;
	class is_promise : true_type {};
	Promise_(const Promise_<Result>&) = delete;
	Promise_<Result>& operator =(const Promise_<Result>&) = delete;
	Promise_() = default;
	/* Resolve */
	void resolve(Result& result);
	/* Reject */
	void reject(exception& error);
	void reject(string& error) { reject(exception(error)); };
	/* Then */
	Promise<Result> then(
		ForwardFunc& next,
		ExceptFunc<Result>& handler = nullptr,
		FinallyFunc& finally = nullptr) {
			return then(
				[next] (Result& result) {
					next(result);
					return Promise<Result>::resolve(result);
				},
				handler,
				finally);
		};
	Promise<Result> then(
		ThenFunc<Result>& next,
		ExceptFunc<Result>& handler = nullptr,
		FinallyFunc& finally = nullptr) {
			return then<Result>(next, handler, finally);
		};
	template <typename NextResult>
		Promise<NextResult> then(
			ThenFunc<NextResult>& next,
			ExceptFunc<NextResult>& handler = nullptr,
			FinallyFunc& finally = nullptr);
	/* Except */
	template <typename Handler>
		Promise<Result> except(Handler& handler)
			{ return then(nullptr, handler); };
	template <typename Handler, typename Finally>
		Promise<Result> except(Handler& handler, Finally& finally)
			{ return then(nullptr, handler, finally); };
	/* Finally */
	template <typename Finally>
		Promise<Result> finally(Finally& finally)
			{ return then(nullptr, nullptr, finally); };
private:
	atomic_flag state_lock = ATOMIC_FLAG_INIT;
	enum class promise_state { pending, rejected, resolved };
	promise_state state = promise_state::pending;
	bool assigned = false;
	unique_ptr<Result> resultPtr;
	unique_ptr<exception> errorPtr;
	function<void(Result&)> nextFunc;
	function<void(exception&)> exceptFunc;
	function<void()> finallyFunc;
	/* The state_lock should be acquired when these are called */
	void resolved();
	void rejected();
};

}

#include "promise.tpp"
