#define promise_tcc
#include <exception>
#include <stdexcept>
#include <mutex>
#include "tuple_iteration.h"
#include "self_locking.h"
#include "promise.h"

namespace kaiu {

using namespace std;

/*** PromiseState ***/

template <typename Result>
PromiseState<Result>::PromiseState(Result&& result) :
	PromiseStateBase()
{
	resolve(forward<Result>(result));
}

template <typename Result>
void PromiseState<Result>::set_result(ensure_locked lock, Result&& value)
{
	result = move(value);
}

template <typename Result>
Result& PromiseState<Result>::get_result(ensure_locked lock)
{
	return result;
}

template <typename Result>
void PromiseState<Result>::resolve(Result&& result)
{
	lock_guard<mutex> lock(state_lock);
	set_result(lock, forward<Result>(result));
	set_state(lock, promise_state::resolved);
}

template <typename Result>
void PromiseState<Result>::forward_to(Promise<Result> next)
{
	lock_guard<mutex> lock(state_lock);
	auto resolve = [next, this] (ensure_locked lock) {
		next->resolve(move(get_result(lock)));
	};
	auto reject = [next, this] (ensure_locked lock) {
		next->reject(get_error(lock));
	};
	set_callbacks(lock, resolve, reject);
}

template <typename Result>
template <typename Next, typename NextPromise, typename NextResult, typename Except, typename Finally, typename>
Promise<NextResult> PromiseState<Result>::then(
	Next next_func,
	Except except_func,
	Finally finally_func)
{
	lock_guard<mutex> lock(state_lock);
	NextPromise promise;
	NextFunc<NextPromise> next(next_func);
	ExceptFunc<NextPromise> handler(except_func);
	FinallyFunc finally(finally_func);
	if (next == nullptr) {
		next = PromiseState<Result>::default_next<NextResult>;
	}
	if (handler == nullptr) {
		handler = PromiseState<Result>::default_except<NextResult>;
	}
	if (finally == nullptr) {
		finally = PromiseState<Result>::default_finally;
	}
	/*
	 * Finally is called at the end of both branches (resolve and reject).
	 *
	 * Returns false iff the finally() callback threw.
	 */
	auto call_finally = [promise, finally] {
		try {
			finally();
		} catch (...) {
			promise->reject(current_exception());
			return false;
		}
		return true;
	};
	/* Branch for if promise is resolved */
	auto resolve = [promise, next, call_finally, this] (ensure_locked lock) {
		Promise<NextResult> nextResult;
		try {
			nextResult = next(get_result(lock));
		} catch (...) {
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
	auto reject = [promise, handler, call_finally, this] (ensure_locked lock) {
		Promise<NextResult> nextResult;
		try {
			nextResult = handler(get_error(lock));
		} catch (...) {
			if (call_finally()) {
				promise->reject(current_exception());
			}
			return;
		}
		if (call_finally()) {
			nextResult->forward_to(promise);
		}
	};
	set_callbacks(lock, resolve, reject);
	return promise;
}

template <typename Result>
template <typename Next, typename NextResult, typename Except, typename Finally, typename>
Promise<NextResult> PromiseState<Result>::then(
	const Next next_func,
	const Except except_func,
	const Finally finally_func)
{
	using namespace promise;
	return then(
		factory(NextFunc<NextResult>(next_func)),
		factory(ExceptFunc<NextResult>(except_func)),
		finally_func);
}

template <typename Result>
template <typename Next, typename NextResult, typename Except, typename Finally, typename>
void PromiseState<Result>::then(
	const Next next_func,
	const Except except_func,
	const Finally finally_func)
{
	NextFunc<nullptr_t> then_forward = [next_func] (Result& result) {
		if (NextVoidFunc(next_func) != nullptr) {
			next_func(result);
		}
		return (nullptr_t) nullptr;
	};
	ExceptFunc<nullptr_t> except_forward = [except_func] (exception_ptr error) {
		if (ExceptVoidFunc(except_func) != nullptr) {
			except_func(error);
		}
		return (nullptr_t) nullptr;
	};
	then(then_forward, except_forward, finally_func)
		->finish();
}

template <typename Result>
template <typename NextResult, typename>
NextResult&& PromiseState<Result>::forward_result(Result& result)
{
	return static_cast<Result&&>(result);
}

template <typename Result>
template <typename NextResult, int dummy, typename>
NextResult&& PromiseState<Result>::forward_result(Result& result)
{
	throw logic_error("If promise <A> is followed by promise <B>, but promise <A> has no 'next' callback, then promise <A> must produce exact same data-type as promise <B>.");
}

template <typename Result>
template <typename NextResult>
Promise<NextResult> PromiseState<Result>::default_next(Result& result)
{
	return Promise<NextResult>(forward_result<NextResult>(result));
}

template <typename Result>
template <typename NextResult>
Promise<NextResult> PromiseState<Result>::default_except(exception_ptr& error)
{
	rethrow_exception(error);
}

template <typename Result>
void PromiseState<Result>::default_finally()
{
}

/*** Promise ***/

/* Access promise */

template <typename Result>
auto Promise<Result>::operator ->() const -> PromiseState<DResult> *
{
	return promise.get();
}

/* One of these is always called by the other constructors */

template <typename Result>
Promise<Result>::Promise(shared_ptr<PromiseState<DResult>> const state) :
	promise(state)
{
}

/* Default constructor */

template <typename Result>
Promise<Result>::Promise() :
	Promise(make_self_locking<PromiseState<DResult>>())
{
}

/* Cast/copy constructors */

template <typename Result>
Promise<Result>::Promise(const Promise<DResult>& p) :
	Promise(p.promise)
{
}

template <typename Result>
Promise<Result>::Promise(const Promise<RResult>& p) :
	Promise(p.promise)
{
}

template <typename Result>
Promise<Result>::Promise(const Promise<XResult>& p) :
	Promise(p.promise)
{
}

/* Resolve constructor */

template <typename Result>
Promise<Result>::Promise(Result&& result) :
	Promise(make_self_locking<PromiseState<DResult>>(forward<Result>(result)))
{
}

/* Reject constructors */

template <typename Result>
Promise<Result>::Promise(const nullptr_t dummy, exception_ptr error) :
	Promise(make_self_locking<PromiseState<DResult>>(dummy, error))
{
}

template <typename Result>
Promise<Result>::Promise(const nullptr_t dummy, const string& error) :
	Promise(make_self_locking<PromiseState<DResult>>(dummy, error))
{
}

/*** Utils ***/
namespace promise {

/* Immediate promises */

template <typename Result, typename DResult>
Promise<DResult> resolved(Result&& result)
{
	return Promise<Result>(forward<Result>(result));
}

template <typename Result, typename DResult>
Promise<DResult> rejected(exception_ptr error)
{
	return Promise<Result>(nullptr, error);
}

template <typename Result, typename DResult>
Promise<DResult> rejected(const string& error)
{
	return Promise<Result>(nullptr, error);
}

/* Create promise factory using given function */

template <typename Result, typename... Args>
Factory<Result, Args...> factory(Result (*func)(Args...))
{
	return factory(function<Result(Args...)>(func));
}

template <typename Result, typename... Args>
Factory<Result, Args...> factory(function<Result(Args...)> func)
{
	Factory<Result, Args...> factory_function = [func] (Args&&... args) {
		try {
			return promise::resolved<Result>(func(forward<Args>(args)...));
		} catch (...) {
			return promise::rejected<Result>(current_exception());
		}
	};
	if (func == nullptr) {
		return nullptr;
	} else {
		return factory_function;
	}
}

/*** Combine multiple heterogenous promises into one promise ***/

/* State object, shared between callbacks on all promises */
template <typename NextResult>
struct HeterogenousCombineState {
	mutex state_lock;
	Promise<NextResult> nextPromise;
	NextResult results;
	size_t remaining = tuple_size<NextResult>::value;
	bool failed = false;
	template <typename Result, const size_t index>
	void next(Result& result) {
		lock_guard<mutex> lock(state_lock);
		if (failed) {
			return;
		}
		get<index>(results) = move(result);
	};
	void handler(exception_ptr& error) {
		{
			lock_guard<mutex> lock(state_lock);
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
			lock_guard<mutex> lock(state_lock);
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
Promise<tuple<typename decay<Result>::type...>> combine(Promise<Result>&&... promise)
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

/*** Combine multiple homogenous promises into one promise ***/

/* State object, shared between callbacks on all promises */
template <typename NextResult>
struct HomogenousCombineState {
	mutex state_lock;
	Promise<vector<NextResult>> nextPromise;
	vector<NextResult> results;
	size_t remaining;
	bool failed = false;
	HomogenousCombineState(const size_t count) :
		results(count), remaining(count)
			{ };
	using Result = NextResult;
	void next(const size_t index, Result& result) {
		lock_guard<mutex> lock(state_lock);
		if (failed) {
			return;
		}
		results[index] = move(result);
	};
	void handler(exception_ptr& error) {
		{
			lock_guard<mutex> lock(state_lock);
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
			lock_guard<mutex> lock(state_lock);
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
	void operator ()(Promise<Result> promise, const size_t index) {
		using Events = PromiseState<typename Promise<Result>::DResult>;
		using namespace placeholders;
		typename Events::NextVoidFunc next{bind(&State::next, state, index, _1)};
		typename Events::ExceptVoidFunc handler{bind(&State::handler, state, _1)};
		typename Events::FinallyFunc finally{bind(&State::finally, state)};
		promise->then(next, handler, finally);
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
	return combine<typename List::iterator>(
		promises.begin(),
		promises.end(),
		promises.size());
}

}

}
