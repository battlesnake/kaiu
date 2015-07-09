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
PromiseInternal<Result>::PromiseInternal(Result&& result)
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
void PromiseInternal<Result>::resolve(Result&& result)
{
	ensure_is_still_pending();
	UserspaceSpinlock lock(state_lock);
	ensure_is_still_pending();
	this->result = move(result);
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
template <typename NextResult>
Promise<NextResult> PromiseInternal<Result>::then_p(
	const ThenFunc<Promise<NextResult>> next,
	const ExceptFunc<Promise<NextResult>> handler,
	const FinallyFunc finally)
{
	ensure_is_unbound();
	UserspaceSpinlock lock(state_lock);
	ensure_is_unbound();
	Promise<NextResult> promise;
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
				Promise<NextResult>(NextResult());
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
			nextResult = handler ? handler(error) :
				Promise<NextResult>(nullptr, error);
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
template <typename NextResult>
Promise<NextResult> PromiseInternal<Result>::then_i(
	const ThenFunc<NextResult> next,
	const ExceptFunc<NextResult> handler,
	const FinallyFunc finally)
{
	/*
	 * Used to be implemented by calls to then_p and wrapping returned values in
	 * promises, but that is stupidly inefficient
	 */
	ensure_is_unbound();
	UserspaceSpinlock lock(state_lock);
	ensure_is_unbound();
	Promise<NextResult> promise;
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
		try {
			if (next) {
				NextResult nextResult = next(result);
				if (call_finally()) {
					promise->resolve(nextResult);
				}
			} else {
				NextResult nextResult{};
				if (call_finally()) {
					promise->resolve(nextResult);
				}
			}
		} catch (exception) {
			if (call_finally()) {
				promise->reject(current_exception());
			}
			return;
		}
	};
	/* Branch for if promise is rejected */
	on_reject = [promise, handler, call_finally, this] {
		try {
			if (handler) {
				NextResult nextResult = handler(error);
				if (call_finally()) {
					promise->resolve(nextResult);
				}
			} else {
				if (call_finally()) {
					promise->reject(error);
				}
			}
		} catch (exception) {
			if (call_finally()) {
				promise->reject(current_exception());
			}
			return;
		}
	};
	assigned_callbacks();
	return promise;
}

template <typename Result>
void PromiseInternal<Result>::then(
		const ThenVoidFunc next,
		const ExceptVoidFunc handler,
		const FinallyFunc finally)
{
	/*
	 * Used to be implemented by calls to then_i with dummy return value but
	 * that seems inefficient
	 */
	ensure_is_unbound();
	UserspaceSpinlock lock(state_lock);
	ensure_is_unbound();
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
Promise<Result>::Promise(Result&& result) :
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
Promise<Result> resolved(Result&& result)
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
function<Promise<Result>(Args...)> make_factory(
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

/* Combine multiple promises into one promise */

template <typename NextResult>
struct _combine_iterator {
	Promise<NextResult>& nextPromise;
	atomic_flag& state_lock;
	bool& failed;
	size_t& remaining;
	NextResult& results;
	template <typename Result, const size_t index>
	void operator ()(Promise<Result>&& promise) {
		promise.then(
			[this]
			(Result& result) {
				bool done;
				{
					UserspaceSpinlock lock(state_lock);
					if (failed) {
						return;
					}
					get<index>(results) = result;
					remaining--;
					done = remaining == 0;
				}
				if (done) {
					nextPromise.resolve(results);
				}
			},
			[this] (exception_ptr error) {
				bool first_fail;
				{
					UserspaceSpinlock lock(state_lock);
					if (failed) {
						return;
					}
					first_fail = !failed;
					failed = true;
				}
				if (first_fail) {
					nextPromise.reject(error);
				}
			});
	};
};

template <typename... Result>
Promise<tuple<Result...>> combine(Promise<Result>&... promise)
{
	using NextResult = tuple<Result...>;
	/* Convert promise pack to tuple */
	auto promises = make_tuple<Result...>(promise...);
	/* Spinlock in case promises are completed in multiple threads */
	atomic_flag state_lock = ATOMIC_FLAG_INIT;
	/* Resulting promise */
	Promise<NextResult> nextPromise;
	/* Temporary storage of results */
	NextResult results;
	/* Number of incomplete promises */
	int remaining = tuple_size<NextResult>::value;
	/* Has a promise been rejected? */
	bool failed = false;
	/* Template metaprogramming "dynamically typed" functional fun */
	tuple_each_with_index(promises,
		_combine_iterator<NextResult>{
			nextPromise,
			state_lock,
			failed,
			remaining,
			results
		});
}

}

}
