Promise
=======

Inspired by Unlambda, Haskell, and Kris Kowal's Q library for JavaScript.

	run_task(arg1, arg2)
		->then(process_result)
		->then(save_result)
		->except(handle_error);

A promise encapsulates the result of an operation that may or may not have
completed yet, or which may not have even started yet.  Promises facilitate
control flow in non-blocking asynchronous code by replacing nested callbacks
with a list of functions.  They give the comfortable appearance of sequential,
blocking code.  They can also be used as an easy way to provide lazy evaluation.

A promise can either be resolved or rejected.  Resolution is analogous to a
function returning a value, while rejection is analogous to a function throwing
an exception.

By wrapping the operations of your application/library/class in promises from
day #1, you can move parts of the program to other processes, or to other
machines.  By using promise-returning functions to operate on objects,
converting local objects to remote objects requires no changes to the code that
uses those objects, as the interface to access remote objects via promises are no
different to those for local objects.  Likewise, converting synchronous functions
to asynchronous also then requires no change to the associated interfaces.

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
			this_thread::sleep_for(100ms);
			promise.resolve(x * x);
		});
		t.detach();
		return promise;
	}

	Promise<int> async_inc(int x)
	{
		Promise<int> promise;
		thread t([x, promise] {
			this_thread::sleep_for(100ms);
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
				} catch (i_want_to_handle_this_exception) {
					/*
					 * Handle the error and return some value to resolve the
					 * promise
					 */
					return promise::resolved(new_value);
				}
				/*
				 * If we didn't catch the exception, it will result in another
				 * rejected promise.  Errors propagate down promise chains until
				 * they are handled.
				 */
			});

### Finalizers

	some_operation(params)
		->then(some_mapping)
		->then(some_other_operation)
		/*
		 * If error occurred, 'handle_errors' is called, else 'save_result'.
		 * If 'save_result' throws, handle_errors will NOT be called, but the
		 * next handler in the promise chain will be.
		 */
		->then(save_result, handle_errors)
		/*
		 * If 'save_result' or 'handle_errors' threw, this handler will be
		 * called
		 */
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

Flow:

	        ╭─success───▶next(result)─────▶╮
	Operation                              ├───▶finalizer()───▶
	        ╰─failed────▶handler(error)───▶╯


When a promise is resolved, `then` is called, followed by `finalizer`.  When a
promise is rejected,  `handler` is called, followed by `finalizer`.

The result of `then`, `except`, `finally` is a new promise.

If `next` throws, the resulting promise will be rejected with the thrown
exception.  If a promise is rejected, but has no `handler`, the promise returned
by `then` will be rejected with the original exception - so exceptions propagate
down promise chains (skipping "next" handlers) until the exceptions are handled.
If `handler` throws, the resulting promise is rejected with the new exception.

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

	Y next(X value)
	Promise<Y> next(X value)
	void next(X value)

`handler` may return any of the same types as `next`, resulting in the next
promise being resolved to that value.  `handler` may also throw or re-throw.  If
`handler` does not have a return value, it will end the promise chain.

For `Promise<X>::except(handler)`, `handler` has one of the following signatures

	Y handler(exception_ptr error)
	Promise<Y> handler(exception_ptr error)
	void handler(exception_ptr error)

`finalizer` takes no parameters and returns no value.

	void finalizer()

If using `then(next, handler [, finally])` then `next` and `handler` must have
the same return type, i.e. both have return type "promise" (`Promise<T>`) or both
have return type "value" (`T`), or both return void (terminating the promise
chain).  If one returns a different type to the other, you will get drowned in
compiler babble.

`finish` is used to explicitly terminate a promise chain, to prevent leaking
promises.  `finish(finalizer)` is a shorthand for `finally` followed by
`finish`.  If the last call on a promise chain is a `then` or an `except`, where
the result type is void, the promise chain is implicitly ended.

Promise factories
-----------------

A function which returns a promise is a promise factory.  A (non-lambda)
function which does not return a promise can be easily converted to a promise
factory:

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

For lambdas, you will need to be more explicit:

	promise::Factory<int, int> cube = [] (int x) { return x * x * x; };

The template arguments are:

	template <typename Result, typename... Args>

For a factory matching:

	(Args...) -> Promise<Result>

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
		->then([] (auto result) {
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
			[] (auto result) {
				/* Won't reach here as some promises are rejected */
			},
			[] (exception_ptr error) {
				try {
					rethrow_exception(error);
				} catch (const exception& error) {
					assert(string(error.what()) == string("Oops"));
				}
			});

Monads / bind
-------------

Promises can be used as monads:

	using kaiu::promise::monads;

Given:

	X :: T → Promise<U>
	Y :: U → Promise<V>
	Z :: V → Promise<W>

The monad chain:

	auto chain = X/E >>= Y >>= Z/E/F;
	chain(initial)->finish();

is the same as:

	promise::resolved(initial)->then(X, E)->then(Y)->then(Z, E, F)->finish()

Gotchas
-------

### Flow control

The following are not the same flow graph:

	A: p->then(next, handler);
	B: p->then(next)->except(handler);
	C: p->except(handler)->then(next);

`A` will call:

  1.   *either* `next` or `handler` depending on the result of the promise.

`B` will call:

  1. `next` if the promise resolved

  2.  `handler` if either:

    * the promise rejected

	* `next` threw

	* `next` returned a rejected promise

`C` will call:

  1. `handler` if the promise rejected

  2. `next` if either:

    * the promise resolved

	* the the promise rejected, but `handler` did not throw and `handler`
	  returned a value or a promise which resolved

### Leaks

Remember to terminate promise chains, either with a `then` or `except` that
returns void, or with a `finish`, to prevent leaks.

### Pointers

Never resolve a promise with a raw pointer, unless you can totally guarantee
that the pointer will remain valid until the promise has been consumed.  If you
can guarantee that, then your application is tightly coupled and promises
probably won't help you much.

Instead use a unique_ptr, so that ownership of the pointer is transparently
transferred from the promise producer to the promise consumer, and which will
then guarantee that the memory will be released when the unique_ptr leaves the
consumer's scope.

Grizzly details
---------------

### What is called when?

The following are functionally equivalent (next/handler can be nullptr):
  - promise.then(next, handler, finalizer)
  - promise.then(next, handler).finally(finalizer)
    - calls either next or handler, always calls finalizer
    - calls next iff promise is resolved
    - calls handler iff promise is rejected
    - calls finalizer always

The following are NOT functionally equivalent:
  - promise.then(next, handler)
    - calls EITHER next or handler
    - calls next iff promise is resolved
    - calls handler iff promise is rejected
  - promise.except(handler).then(next)
    - MIGHT call handler AND MIGHT call next
    - calls handler iff promise is rejected
    - calls next iff (promise is resolved or handler resolves/returns value)
  - promise.then(next).except(handler)
    - MIGHT call next AND MIGHT call Handler
    - calls next iff promise is resolved
    - calls handler iff (promise is rejected or next rejects/throws)

THEN function can return:
  - void - next promise resolves with same value as this one.
  - new value of any type - next promise resolves with this new value.
  - promise - resolution/rejection is forwarded to the returned promise.
  - throw exception - next promise is rejected with this exception unless
    FINALLY function throws.

EXCEPT function can return:
  - new value of same type - next promise resolves with this new value.
  - promise - resolution/rejection is forwarded to the returned promise.
  - throw exception - next promise is rejected with this exception unless
    FINALLY function throws.

FINALLY function can return:
  - void - next promise is resolved/rejected with same value as this one.
  - throw exception - next promise is rejected with THIS exception even if
    THEN function or EXCEPT function also threw.

next_promise = this_promise->then(next, handler = nullptr, finally = nullptr):
  - next is called iff this promise is resolved.
  - handler is called iff this promise is rejected.
  - finally is always called, even if next/handler throws.
  - iff promise is resolved, next+finally are called.
  - iff promise is rejected, handler+finally are called.
  - with no handler, rejected promise propagates by rejection of the next
    promise.
  - handler can re-throw or return a rejected promise, causing rejection of the
    next promise.
  - handler can return a value, causing resolution of the next promise (unless finally throws).
  - if next/handler/finally throws, the next promise is rejected.  handler is
    NOT called when next/finally throws, as handler operates on this promise,
    not the next one.
  - next promise is resolved iff either:
     - this promise is resolved and none of the callbacks throw.
     - this promise is rejected and has a handler which returns a resolution.
  - next promise is rejected iff either:
     - a callback throws/rejects.
     - this promise is rejected and either has no handler or the handler
       throws/rejects.
  - when next promise is rejected, exception thrown by finally takes priority
    over exception thrown by next/except (regarding exception used to reject
    next promise).

except(handler, finally = nullptr):
  - shorthand for then(nullptr, handler, finally)

finally(finally):
  - shorthand for then(nullptr, nullptr, finally)

### Internal state transitions


Promise states:

	   ┌───────────┬──────────┬──────────┬──────────┐
	   │  Name of  │  Has a   │  Has an  │ Callback │
	   │   state   │  result  │  error   │  called  │
	   ├———————————┼——————————┼——————————┼——————————┤
	 A │pending    │ no       │ no       │ (no)     │
	 B │resolved   │ yes      │ (no)     │ no       │
	 C │rejected   │ (no)     │ yes      │ no       │
	 D │completed  │ *        │ *        │ yes      │
	   └───────────┴──────────┴──────────┴──────────┘
	
	      * = don't care (any value)
	  (val) = value is implicit, enforced due to value of some other field

State transition graph:

	      ┌──▶ B ──┐
	  A ──┤        ├──▶ D
	      └──▶ C ──┘

State descriptions/conditions:

 * A. pending  
   initial state, nothing done  

 * B: resolved  
   promise represents a successful operation, a result value has been assigned  

 * C: rejected  
   promise represents a failed operation, an error has been assigned  

 * D: completed  
   promise has been resolved/rejected, then the appropriate callback has
   been called  


