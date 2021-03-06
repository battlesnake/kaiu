#define promise_tcc
#include <stdexcept>
#include "tuple_iteration.h"
#include "promise.h"

namespace kaiu {


/*** callback_pack ***/

namespace promise {

template <typename Range, typename Domain>
template <typename NextRange>
auto callback_pack<Range, Domain>::
	bind(const callback_pack<NextRange, Range> after) const
{
	return callback_pack<NextRange, Domain>(
		[after, self=*this] (Domain d) -> Promise<NextRange> {
			return self(std::move(d))->then(after);
		},
		[after, self=*this] (std::exception_ptr error) -> Promise<NextRange> {
			return self.reject(error)->then(after);
		}
	);
}

template <typename Range, typename Domain>
Promise<Range> callback_pack<Range, Domain>::call(const Promise<Domain> d) const
{
	return d->then(*this);
}

template <typename Range, typename Domain>
Promise<Range> callback_pack<Range, Domain>::operator () (const Promise<Domain> d) const
{
	return call(d);
}

template <typename Range, typename Domain>
Promise<Range> callback_pack<Range, Domain>::operator () (Domain d) const
{
	return call(promise::resolved<Domain>(std::move(d)));
}

template <typename Range, typename Domain>
Promise<Range> callback_pack<Range, Domain>::reject(std::exception_ptr error) const
{
	return call(promise::rejected<Domain>(error));
}

template <typename Range, typename Domain>
callback_pack<Range, Domain>::callback_pack(const Next next, const Handler handler, const Finalizer finalizer) :
	next(next), handler(handler), finalizer(finalizer)
{
}

}

/*** PromiseState ***/

template <typename Result>
void PromiseState<Result>::set_result(ensure_locked lock, Result&& value)
{
	result = std::move(value);
}

template <typename Result>
Result PromiseState<Result>::get_result(ensure_locked lock)
{
	return std::move(result);
}

template <typename Result>
void PromiseState<Result>::resolve(Result result)
{
	auto lock = get_lock();
	set_result(lock, std::move(result));
	set_state(lock, promise_state::resolved);
}

template <typename Result>
template <typename NextPromise>
void PromiseState<Result>::forward_to(NextPromise next)
{
	auto lock = get_lock();
	auto resolve = [next, this] (ensure_locked lock) {
		next->resolve(std::move(get_result(lock)));
	};
	auto reject = [next, this] (ensure_locked lock) {
		next->reject(get_error(lock));
	};
	set_callbacks(lock, resolve, reject);
}

template <typename Result>
template <typename Range>
Promise<Range> PromiseState<Result>::then(
	const promise::callback_pack<Range, Result> pack)
{
	return then(pack.next, pack.handler, pack.finalizer);
}

template <typename Result>
template <typename Next, typename NextPromise, typename NextResult, typename Except, typename Finally, typename>
Promise<NextResult> PromiseState<Result>::then(
	Next next_func,
	Except except_func,
	Finally finally_func)
{
	auto lock = get_lock();
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
			promise->reject(std::current_exception());
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
				promise->reject(std::current_exception());
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
				promise->reject(std::current_exception());
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
	return then(
		promise::factory(NextFunc<NextResult>(next_func)),
		promise::factory(ExceptFunc<NextResult>(except_func)),
		finally_func);
}

template <typename Result>
template <typename Next, typename NextResult, typename Except, typename Finally, typename>
void PromiseState<Result>::then(
	const Next next_func,
	const Except except_func,
	const Finally finally_func)
{
	NextFunc<nullptr_t> then_forward = [next_func] (Result result) {
		if (NextVoidFunc(next_func) != nullptr) {
			next_func(std::move(result));
		}
		return (nullptr_t) nullptr;
	};
	ExceptFunc<nullptr_t> except_forward = [except_func] (std::exception_ptr error) {
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
NextResult PromiseState<Result>::forward_result(Result result)
{
	return result;
}

template <typename Result>
template <typename NextResult, int dummy, typename>
NextResult PromiseState<Result>::forward_result(Result result)
{
	throw std::logic_error("If promise <A> is followed by promise <B>, but promise <A> has no 'next' callback, then promise <A> must produce exact same data-type as promise <B>.");
}

template <typename Result>
template <typename NextResult>
Promise<NextResult> PromiseState<Result>::default_next(Result result)
{
	return promise::resolved<NextResult>(forward_result<NextResult>(std::move(result)));
}

template <typename Result>
template <typename NextResult>
Promise<NextResult> PromiseState<Result>::default_except(std::exception_ptr error)
{
	std::rethrow_exception(error);
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

/* Constructor for sharing state with another promise */

template <typename Result>
Promise<Result>::Promise(std::shared_ptr<PromiseState<DResult>> const state) :
	promise(state)
{
}

/* Constructor for creating a new state */

template <typename Result>
Promise<Result>::Promise() :
	promise(std::make_shared<PromiseState<DResult>>())
{
}

/* Cast/copy constructors */

template <typename Result>
Promise<Result>::Promise(const Promise<DResult>& p) :
	Promise(p.promise)
{
}

template <typename Result>
Promise<Result>::Promise(const Promise<DResult&>& p) :
	Promise(p.promise)
{
}

template <typename Result>
Promise<Result>::Promise(const Promise<DResult&&>& p) :
	Promise(p.promise)
{
}

/*** Utils ***/
namespace promise {

/* Immediate promises */

template <typename Result, typename DResult>
Promise<DResult> resolved(Result result)
{
	Promise<DResult> promise;
	promise->resolve(std::move(result));
	return promise;
}

template <typename Result, typename DResult>
Promise<DResult> rejected(std::exception_ptr error)
{
	Promise<DResult> promise;
	promise->reject(error);
	return promise;
}

template <typename Result, typename DResult>
Promise<DResult> rejected(const std::string& error)
{
	Promise<DResult> promise;
	promise->reject(error);
	return promise;
}

/* Create promise factory using given function */

template <typename Result, typename... Args>
Factory<Result, Args...> factory(Result (*func)(Args...))
{
	return factory(std::function<Result(Args...)>(func));
}

template <typename Result, typename... Args>
Factory<Result, Args...> factory(std::function<Result(Args...)> func)
{
	if (func == nullptr) {
		return nullptr;
	}
	Factory<Result, Args...> factory_function = [func] (Args&&... args) {
		try {
			return promise::resolved<Result>(func(std::forward<Args>(args)...));
		} catch (...) {
			return promise::rejected<Result>(std::current_exception());
		}
	};
	return factory_function;
}

/*** Combine multiple heterogenous promises into one promise ***/

/* State object, shared between callbacks on all promises */
template <typename NextResult>
struct HeterogenousCombineState {
	std::mutex state_lock;
	Promise<NextResult> nextPromise;
	NextResult results;
	size_t remaining = std::tuple_size<NextResult>::value;
	bool failed = false;
	template <typename Result, const size_t index>
	void next(Result result)
	{
		std::lock_guard<std::mutex> lock(state_lock);
		if (failed) {
			return;
		}
		std::get<index>(results) = std::move(result);
	}
	void handler(std::exception_ptr error)
	{
		{
			std::lock_guard<std::mutex> lock(state_lock);
			if (failed) {
				return;
			}
			failed = true;
		}
		nextPromise->reject(error);
	}
	void finally()
	{
		bool resolved;
		{
			std::lock_guard<std::mutex> lock(state_lock);
			remaining--;
			resolved = remaining == 0 && !failed;
		}
		if (resolved) {
			nextPromise->resolve(std::move(results));
		}
	}
};

/* Functor which binds a single promise to shared state */
template <typename NextResult>
struct HeterogenousCombineIterator {
	using State = HeterogenousCombineState<NextResult>;
	std::shared_ptr<State> state;
	template <typename PromiseType, const size_t index>
	void operator ()(PromiseType promise)
	{
		using Result = typename std::decay<PromiseType>::type::result_type;
		const auto next = [state = state] (Result result)
			{ return state->template next<Result, index>(std::move(result)); };
		const auto handler = [state = state] (std::exception_ptr error)
			{ return state->handler(error); };
		const auto finalizer = [state = state] ()
			{ state->finally(); };
		promise->then(next, handler, finalizer);
	}
};

template <typename... Result>
Promise<std::tuple<typename std::decay<Result>::type...>> combine(Promise<Result>&&... promise)
{
	using NextResult = std::tuple<Result...>;
	/* Convert promise pack to tuple */
	auto promises = std::make_tuple(std::forward<Promise<Result>>(promise)...);
	/* State */
	auto state = std::make_shared<HeterogenousCombineState<NextResult>>();
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
	std::mutex state_lock;
	Promise<std::vector<NextResult>> nextPromise;
	std::vector<NextResult> results;
	size_t remaining;
	bool failed = false;
	HomogenousCombineState(const size_t count) :
		results(count), remaining(count)
			{ };
	using Result = NextResult;
	void next(const size_t index, Result result)
	{
		std::lock_guard<std::mutex> lock(state_lock);
		if (failed) {
			return;
		}
		results[index] = std::move(result);
	}
	void handler(std::exception_ptr error)
	{
		{
			std::lock_guard<std::mutex> lock(state_lock);
			if (failed) {
				return;
			}
			failed = true;
		}
		nextPromise->reject(error);
	}
	void finalizer()
	{
		bool resolved;
		{
			std::lock_guard<std::mutex> lock(state_lock);
			remaining--;
			resolved = remaining == 0 && !failed;
		}
		if (resolved) {
			nextPromise->resolve(std::move(results));
		}
	}
};

/* Binds a single promise to shared state */
template <typename NextResult>
struct HomogenousCombineIterator {
	using State = HomogenousCombineState<NextResult>;
	std::shared_ptr<State> state;
	using Result = NextResult;
	void operator ()(Promise<Result> promise, const size_t index)
	{
		const auto next = [state = state, index] (Result result)
			{ return state->next(index, std::move(result)); };
		const auto handler = [state = state] (std::exception_ptr error)
			{ return state->handler(error); };
		const auto finalizer = [state = state] ()
			{ return state->finalizer(); };
		promise->then(next, handler, finalizer);
	}
};

template <typename It, typename Result>
Promise<std::vector<Result>> combine(It first, It last, const size_t size)
{
	auto state = std::make_shared<HomogenousCombineState<Result>>(size);
	size_t index = 0;
	for (It it = first; it != last; ++it, ++index) {
		HomogenousCombineIterator<Result>{state}(*it, index);
	}
	/* Return new promise */
	return state->nextPromise;
}

template <typename It, typename Result>
Promise<std::vector<Result>> combine(It first, It last)
{
	return combine<It, Result>(first, last, distance(first, last));
}

template <typename List, typename Result>
Promise<std::vector<Result>> combine(List promises)
{
	return combine<typename List::iterator>(
		promises.begin(),
		promises.end(),
		promises.size());
}

}

}
