#define promise_tcc
#include <exception>
#include <stdexcept>
#include "spinlock.h"
#include "tuple_iteration.h"
#include "promise.h"

namespace mark {

using namespace std;

/*** PromiseInternal ***/

template <typename Result>
PromiseInternal<Result>::PromiseInternal(Result const& result)
{
	resolve(result);
}

template <typename Result>
void PromiseInternal<Result>::resolve(Result const& result)
{
	ensure_is_still_pending();
	UserspaceSpinlock lock(state_lock);
	ensure_is_still_pending();
	this->result = result;
	resolved();
}

template <typename Result>
void PromiseInternal<Result>::forward_to(Promise<Result> next)
{
	ensure_is_unbound();
	UserspaceSpinlock lock(state_lock);
	ensure_is_unbound();
	on_resolve = [next, this] { next->resolve(result); };
	on_reject = [next, this] { next->reject(error); };
	assigned_callbacks();
}

template <typename Result>
template <typename Then, typename NextPromise, typename NextResult, typename Except, typename Finally, typename>
Promise<NextResult> PromiseInternal<Result>::then(
	const Then then_func,
	const Except except_func,
	const Finally finally_func)
{
	ensure_is_unbound();
	UserspaceSpinlock lock(state_lock);
	ensure_is_unbound();
	NextPromise promise;
	ThenFunc<NextPromise> next(then_func);
	ExceptFunc<NextPromise> handler(except_func);
	FinallyFunc finally(finally_func);
	/*
	 * Finally is called at the end of both branches (resolve and reject).
	 *
	 * Returns false iff the finally() callback exists and it threw.
	 */
	auto call_finally = [promise, finally] {
		if (!finally) {
			return true;
		}
		try {
			finally();
		} catch (exception) {
			promise->reject(current_exception());
			return false;
		}
		return true;
	};
	/* Branch for if promise is resolved */
	on_resolve = [promise, next, call_finally, this] {
		Promise<NextResult> nextResult;
		try {
			nextResult = next ? next(result) :
				Promise<NextResult>(next_default_value<NextResult>(result));
		} catch (exception) {
			if (call_finally()) {
				promise->reject(current_exception());
			}
			return;
		}
		if (call_finally()) {
			nextResult->forward_to(promise);
		}
	};
	/* Branch for if promise is rejected */
	on_reject = [promise, handler, call_finally, this] {
		Promise<NextResult> nextResult;
		try {
			if (handler) {
				nextResult = handler(error);
			} else {
				rethrow_exception(error);
			}
		} catch (exception) {
			if (call_finally()) {
				promise->reject(current_exception());
			}
			return;
		}
		if (call_finally()) {
			nextResult->forward_to(promise);
		}
	};
	assigned_callbacks();
	return promise;
}

template <typename Result>
template <typename Then, typename NextResult, typename Except, typename Finally, typename>
Promise<NextResult> PromiseInternal<Result>::then(
	const Then then_func,
	const Except except_func,
	const Finally finally_func)
{
	/*
	 * Used to be implemented by calls to then_p and wrapping returned values in
	 * promises, but that is stupidly inefficient
	 */
	ensure_is_unbound();
	UserspaceSpinlock lock(state_lock);
	ensure_is_unbound();
	Promise<NextResult> promise;
	ThenFunc<NextResult> next(then_func);
	ExceptFunc<NextResult> handler(except_func);
	FinallyFunc finally(finally_func);
	/*
	 * Finally is called at the end of both branches (resolve and reject).
	 *
	 * Returns false iff the finally() callback exists and it threw.
	 */
	auto call_finally = [promise, finally] {
		if (!finally) {
			return true;
		}
		try {
			finally();
		} catch (exception) {
			promise->reject(current_exception());
			return false;
		}
		return true;
	};
	/* Branch for if promise is resolved */
	on_resolve = [promise, next, call_finally, this] {
		NextResult nextResult;
		try {
			nextResult = next ? next(result) :
				next_default_value<NextResult>(result);
		} catch (exception) {
			if (call_finally()) {
				promise->reject(current_exception());
			}
			return;
		}
		if (call_finally()) {
			promise->resolve(nextResult);
		}
	};
	/* Branch for if promise is rejected */
	on_reject = [promise, handler, call_finally, this] {
		NextResult nextResult;
		try {
			if (handler) {
				nextResult = handler(error);
			} else {
				rethrow_exception(error);
			}
		} catch (exception) {
			if (call_finally()) {
				promise->reject(current_exception());
			}
			return;
		}
		if (call_finally()) {
			promise->resolve(nextResult);
		}
	};
	assigned_callbacks();
	return promise;
}

template <typename Result>
template <typename Then, typename NextResult, typename Except, typename Finally, typename>
void PromiseInternal<Result>::then(
	const Then then_func,
	const Except except_func,
	const Finally finally_func)
{
	/*
	 * Used to be implemented by calls to then_i with dummy return value but
	 * that seems inefficient
	 */
	ensure_is_unbound();
	UserspaceSpinlock lock(state_lock);
	ensure_is_unbound();
	ThenFunc<NextResult> next(then_func);
	ExceptFunc<NextResult> handler(except_func);
	FinallyFunc finally(finally_func);
	/* Branch for if promise is resolved */
	on_resolve = [next, finally, this] {
		try {
			if (next) {
				next(result);
			}
		} catch (exception) {
			if (finally) {
				finally();
			}
			throw;
		}
		if (finally) {
			finally();
		}
	};
	/* Branch for if promise is rejected */
	on_reject = [handler, finally, this] {
		try {
			if (handler) {
				handler(error);
			}
		} catch (exception) {
			if (finally) {
				finally();
			}
			throw;
		}
		if (finally) {
			finally();
		}
	};
	assigned_callbacks();
}

template <typename Result>
template <typename NextResult>
NextResult PromiseInternal<Result>::next_default_value(const Result& value) const
{
	constexpr bool same = is_same<NextResult, Result>::value;
	if (!same) {
		throw new logic_error(
			"If promise <A> is followed by promise <B>, but promise <A> has no 'next' callback, then promise <A> must produce exact same data-type as promise <B>.");
	}
	/*
	 * Reinterpret cast will only run if dest and source types are identical,
	 * due to check above.  Although the check is performed at compile-time, we
	 * avoid compile-time type-errors with this ugly cast, and we throw the type
	 * error at run-time instead since we can't determine at compile-time
	 * whether value passing would be required - therefore we would generate
	 * invalid casts and compilation would fail even when we do not require
	 * value passing.
	 */
	return *reinterpret_cast<const NextResult*>(&value);
}

/*** Promise ***/

template <typename Result>
PromiseInternal<Result> *Promise<Result>::operator ->() const
{
	return promise.get();
}

template <typename Result>
Promise<Result>::Promise() :
	promise(new PromiseInternal<Result>())
{
}

template <typename Result>
Promise<Result>::Promise(Result const& result) :
	promise(new PromiseInternal<Result>(result))
{
}

template <typename Result>
Promise<Result>::Promise(const nullptr_t dummy, exception_ptr error) :
	promise(new PromiseInternal<Result>(dummy, error))
{
}

template <typename Result>
Promise<Result>::Promise(const nullptr_t dummy, const string& error) :
	promise(new PromiseInternal<Result>(dummy, error))
{
}

template <typename Result>
Promise<Result>::Promise(const Promise<Result>& source) :
	promise(source.promise)
{
}

template <typename Result>
Promise<Result>& Promise<Result>::operator =(const Promise<Result>& source)
{
	promise = source.promise;
	return *this;
}

template <typename Result>
Promise<Result>::Promise(Promise<Result>&& source) :
	promise(source.promise)
{

	source.promise = nullptr;
}

template <typename Result>
Promise<Result>& Promise<Result>::operator =(Promise<Result>&& source)
{
	promise = move(source.promise);
	return *this;
}

/*** Utils ***/
namespace promise {

/* Immediate promises */

template <typename Result>
Promise<Result> resolved(Result const& result)
{
	return Promise<Result>(result);
}

template <typename Result>
Promise<Result> rejected(exception_ptr error)
{
	return Promise<Result>(nullptr, error);
}

template <typename Result>
Promise<Result> rejected(const string& error)
{
	return Promise<Result>(nullptr, error);
}

/* Create promise factory using given function */

template <typename Result, typename... Args>
function<Promise<Result>(Args...)> factory(
	function<Result(Args...)>& func)
{
	return !func ? nullptr :
		[func] (Args&&... args) {
			try {
				return Promise<Result>::resolve(func(forward<Args>(args)...));
			} catch (exception *error) {
				return Promise<Result>::reject(error);
			}
		};
}

/*** Combine multiple promises into one promise ***/

/* State object, shared between callbacks on all promises */
template <typename NextResult>
struct HeterogenousCombineState {
	atomic_flag state_lock = ATOMIC_FLAG_INIT;
	Promise<NextResult> nextPromise;
	NextResult results;
	size_t remaining = tuple_size<NextResult>::value;
	bool failed = false;
	template <typename Result, const size_t index>
	void next(Result& result) {
		UserspaceSpinlock lock(state_lock);
		if (failed) {
			return;
		}
		get<index>(results) = result;
	};
	void handler(exception_ptr error) {
		{
			UserspaceSpinlock lock(state_lock);
			if (failed) {
				return;
			}
			failed = true;
		}
		nextPromise->reject(error);
	};
	void finally() {
		bool resolved;
		{
			UserspaceSpinlock lock(state_lock);
			remaining--;
			resolved = remaining == 0 && !failed;
		}
		if (resolved) {
			nextPromise->resolve(move(results));
		}
	};
};

/* Functor which binds a single promise to shared state */
template <typename NextResult>
struct HeterogenousCombineIterator {
	using State = HeterogenousCombineState<NextResult>;
	shared_ptr<State> state;
	template <typename PromiseType, const size_t index>
	void operator ()(PromiseType& promise) {
		using Result = typename decay<PromiseType>::type::result_type;
		promise->then(
			bind(&State::template next<Result, index>, state, placeholders::_1),
			bind(&State::handler, state, placeholders::_1),
			bind(&State::finally, state));
	};
};

template <typename... Result>
Promise<tuple<Result...>> combine(Promise<Result>&&... promise)
{
	using NextResult = tuple<Result...>;
	/* Convert promise pack to tuple */
	auto promises = make_tuple(forward<Promise<Result>>(promise)...);
	/* State */
	auto state = make_shared<HeterogenousCombineState<NextResult>>();
	/* Template metaprogramming "dynamically typed" functional fun */
	tuple_each_with_index(promises,
		HeterogenousCombineIterator<NextResult>{state});
	/* Return new promise */
	return state->nextPromise;
}

/* State object, shared between callbacks on all promises */
template <typename NextResult>
struct HomogenousCombineState {
	atomic_flag state_lock = ATOMIC_FLAG_INIT;
	Promise<vector<NextResult>> nextPromise;
	vector<NextResult> results;
	size_t remaining;
	bool failed = false;
	HomogenousCombineState(const size_t count) :
		results(count), remaining(count)
			{ };
	using Result = NextResult;
	void next(const size_t index, Result& result) {
		UserspaceSpinlock lock(state_lock);
		if (failed) {
			return;
		}
		results[index] = result;
	};
	void handler(exception_ptr error) {
		{
			UserspaceSpinlock lock(state_lock);
			if (failed) {
				return;
			}
			failed = true;
		}
		nextPromise->reject(error);
	};
	void finally() {
		bool resolved;
		{
			UserspaceSpinlock lock(state_lock);
			remaining--;
			resolved = remaining == 0 && !failed;
		}
		if (resolved) {
			nextPromise->resolve(move(results));
		}
	};
};

/* Binds a single promise to shared state */
template <typename NextResult>
struct HomogenousCombineIterator {
	using State = HomogenousCombineState<NextResult>;
	shared_ptr<State> state;
	using Result = NextResult;
	void operator ()(Promise<Result>& promise, const size_t index) {
		promise->then(
			bind(&State::next, state, index, placeholders::_1),
			bind(&State::handler, state, placeholders::_1),
			bind(&State::finally, state));
	};
};

template <typename It, typename Result>
Promise<vector<Result>> combine(It first, It last, const size_t size)
{
	auto state = make_shared<HomogenousCombineState<Result>>(size);
	size_t index = 0;
	for (It it = first; it != last; ++it, ++index) {
		HomogenousCombineIterator<Result>{state}(*it, index);
	}
	/* Return new promise */
	return state->nextPromise;
}

template <typename It, typename Result>
Promise<vector<Result>> combine(It first, It last)
{
	return combine<It, Result>(first, last, distance(first, last));
}

template <typename List, typename Result>
Promise<vector<Result>> combine(List promises)
{
	return combine<typename List::iterator, Result>(
		promises.begin(),
		promises.end(),
		promises.size());
}

}

}
