Promise
=======

Inspired by Kris Kowal's Q library for JavaScript.

	run_task(arg1, arg2)
		->then(process_result)
		->then(save_result)
		->except(handle_error);

A promise abstracts an operation and provides access to the result of the
operation.

A promise can either be resolved or rejected.  Resolution is analogous to a
function returning a value, while rejection is analogous to a function throwing
an exception.

They can be used for non-blocking asynchronous flow control, for lazy
evaluation, and to guarantee finalization across scopes (i.e. where RAII
finalizers cannot be used).

The `Promise` class is actually a proxy to `PromiseState`, wrapping it in a
`shared_ptr`.  Hence, the `Promise` objects can be passed around, copied, moved,
destroyed as if they were just `shared_ptr`s.  The `Promise` wrapper just
provides some useful constructors, and automatically removes
const/volatile/reference qualifiers from the (possibly inferred) promise type.

The underlying `PromiseState` object is not moveable or copyable.  It should
really be an inner class of `Promise` (with single then+finish methods and the
rest as proxy methods on `Promise`), but that would make the header file
considerably less readable.

Examples
--------

Create a promise and resolve it to an `int` with value 42:

	promise::resolved(42)

The result type is inferred from the argument.

Create a promise and resolve it to a `string`, explicitly specifying the result
type:

	promise::resolved<string>("hello")

Create a promise that could resolve to `int` but reject it:

	promise::rejected<int>(logic_error("Oops"))

A promise can be rejected with an exception, or with a string value (that is
converted to an exception).

### Launch an asynchronous task and return a promise

	Promise<int> async_sqr(int x)
	{
		Promise<int> promise;
		thread t([x, promise] {
			promise.resolve(x * x);
		});
		t.detach();
		return promise;
	}

	Promise<int> async_inc(int x)
	{
		Promise<int> promise;
		thread t([x, promise] {
			promise.resolve(x + 1);
		});
		t.detach();
		return promise;
	}

### Consume a promise

	async_sqr(6)
		->then([] (int result) {
			assert(result == 36);
		});

### Chainable promises

	promise::resolved(3)
		->then(async_inc)
		->then(async_sqr)
		->then([] (int result) {
			assert(result, 16);
		});

### Branch on error

	promise::rejected<int>("Oops")
		->then(
			[] (int result) {
				/* Success handler, won't be called */
			},
			[] (exception_ptr error) {
				/* Error handler */
				try {
					rethrow_exception(error);
				} catch (const exception& err) {
					alert(err.what());
				}
			});

### Finalizers

	some_operation(params)
		->then(some_mapping)
		->then(some_other_operation)
		->then(save_result, handle_errors)
		->except(handle_error_when_save_failed)
		->finally(finalizer)
		->then(more_work)
		->finish(another_finalizer);

Details
-------

	then(next)
	then(next, handler)
	then(next, handler, finalizer)

	// Same as then(nullptr, handler)
	except(handler)

	// Same as then(nullptr, nullptr, finalizer)
	finally(finalizer)

	// Same as finally(finalizer)->finish()
	finish(finalizer)

	finish()

When a promise is resolved, `then` is called, followed by `finalizer`.  When a
promise is rejected,  `handler` is called, followed by `finalizer`.

The result of `then`, `except`, `finally` is a new promise.

If `next` throws, the resulting promise will be rejected with the thrown
exception.  If a promise is rejected, but has no `handler`, the promise returned
by `then` will be rejected with the original exception - so exceptions propagate
down promise chains until they are handled.  If `handler` throws, the resulting
promise is rejected with the caught exception.

`finalizer` will always be called, even if `next` or `handler` throws.  If
`finalizer` throws, the resulting promise will be rejected with the exception
thrown by `finalizer`, **even if** `next` or `handler` also threw.

`next` may return:

 * Value: if `next` returns type `T`, `then` returns a `Promise<T>`.

 * Promise: if `next` returns type `Promise<T>`, then also returns `Promise<T>`
   (albeit a different instance).  The promise returned by `next` will have its
   result (resolution/rejection) forwarded to the promise returned by `then`.

 * Void: Ends promise chain.

 * Throw: Promise returned by `then` is rejected.

For `Promise<X>::then(next)`, `next` has one of the following signatures
(references are optional):

	Y next(X& value)
	Promise<Y> next(X& value)
	void next(X& value)

`handler` may return any of the same types as `next`, resulting in the next
promise being resolved to that value.  `handler` may also throw or re-throw.  If
`handler` does not have a return value, it will end the promise chain.

For `Promise<X>::except(handler)`, `handler` has one of the following signatures

	Y handler(exception_ptr error)
	Promise<Y> handler(exception_ptr error)
	void handler(exception_ptr error)

`finalizer` takes no parameters and returns no value.

If using `then(next, handler [, finally])` then `next` and `handler` must have
the same return type, i.e. both have return type "promise" (`Promise<T>`) or both
have return type "value" (`T`), or both return void (terminating the promise
chain).

`finish` is used to explicitly terminate a promise chain, to prevent leaking
promises.  `finish(finalizer)` is a shorthand for `finally` followed by
`finish`.  If the last call on a promise chain is a `then` or an `except`, where
the result type is void, the promise chain is implicitly ended.

Promise factories
-----------------

A function which returns a promise is a promise factory.  A (non-lambda)
function which does not return a promise can be converted to a promise factory:

	int cube(int x) {
		return x * x * x;
	}

	auto cuber = promise::factory(cube);

	cuber(6)
		->then([] (int result) {
			assert(result == 216);
		});

	promise::resolved(2)
		->then(cuber)
		->then(cuber)
		->then([] (int result) {
			assert(result == 512);
		});

Combinators
-----------

Multiple promises can be combined into one promise, which will:

 * Resolve to a `tuple` or a `vector` of results if all promises resolved, OR

 * Reject immediately if any promise rejects, passing the error from the first
 promise to reject to the rejection handler.

The order of results from the combined promise matches the order that the
promises were passed to the combinator, regardless of the actual order that they
resolve in.

If a promise was already rejected when `combine` is called, then the error from
the first such promise in the list will be used to reject the combined promise.

### Heterogenous (static) combinator

Number of promises: statically known constant
Promise result types: mixed

	Promise<tuple<Result...>> combine(Promise<Result>...)

	combine(promise::resolved(2), promise::resolved<string>("hello"))
		->then([] (auto& result) {
			assert(get<0>(result) == 2);
			assert(get<1>(result) == "hello");
		});

### Homogenous combinator

Number of promises: variable
Promise result types: identical

	Promise<vector<Result>> combine(vector<Promise<Result>>)

	vector<Promise<int>> promises;

	promises.push_back(promise::resolved(1));
	promises.push_back(promise::resolved(2));
	promises.push_back(promise::rejected<int>("Oops"));
	promises.push_back(promise::resolved(3));
	promises.push_back(promise::rejected<int>("Whoopsie"));

	combine(promises)
		->then(
			[] (auto& result) {
				/* Won't reach here as some promises are rejected */
			},
			[] (exception_ptr error) {
				try {
					rethrow_exception(error);
				} catch (const exception& error) {
					assert(string(error.what()) == string("Oops"));
				}
			});

Gotchas
-------

### Non-copyable result, [] (const volatile auto & arg) { }

This doesn't work:

	promise::resolved(make_unique<int>(1))
		->then([] (auto ptr) {
		});

Recall that `auto` in the context of lambda parameters does not produce const,
volatile, or reference types - these qualifiers must be specified explicitly.

Typically, you will want to use `[] (auto& arg)` or `[] (const auto& arg)` in
order to avoid un-necessary copying.  When using `auto&`, it is safe to `move`
the value elsewhere if desired.

	promise::resolved(make_unique<int>(1))
		->then([] (auto& ptr) {
			the_ptr = move(ptr);
		});

### Leaks

Remember to terminate promise chains, either with a `then` or `except` that
returns void, or with a `finish`.
