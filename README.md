Kaiu
====

N59E25

Promises, parallel event-loops, segregated thread pools, functional fun, ...

All contents of this library are located within the namespace `kaiu`.

### Single-threaded reactor

A traditional ``event-loop'' à la Win32 / Node.js.

### Thread multi-pooling parallel reactor

A concurrent event-loop, consisting of multiple pools of threads.

Different pools may be used for different types of tasks, e.g. `$NUMBER_OF_CPUS` threads in a pool for computational tasks, lots of threads in pool for local I/O tasks, lots more threads in pool for remote I/O tasks.

This enables concurrency to be finely-tuned to balance contention on different resource groups (e.g. CPUs, network I/O, disk I/O).

### Promises

Inspired by the JS `q` library, these promises are really just monads for representing the result of an operation.

### Promise streams

These are basically instances of generators which generate promises then return a promise.

A promise stream produces a sequence of `datum` promises (of the same type), enabling asynchronous streaming of data from a producer (and then into a consumer).

After the last item has been produced, a finally `result` promise is produced, representing the result of the entire streaming operation.

### Promise factories

A promise factory creates a promise, given some parameters.

It's the basic building block of an asynchronous program.

Each operation is wrapped in a promise factory, then the resulting promises can be chained to compose a more complex operation --- which can then also be published as a promise factory.

It's basically the asynchronous equivalent of a subroutine.

### Promise stream factories

This is really just an asynchronous generator, using a promise stream to provide its values, then the final `result` promise to provide the return value of the operation.

### DSL / operator overloads

Operator overloads are provided to create a DSL (domain-specific language) for composing asynchronous pipelines of tasks and streams from task factories and promise stream factories.


Documentation
-------------

The header files are commented.  Some components have their own README:

 * (Promise)[https://github.com/battlesnake/kaiu/blob/master/promise.md]

 * (Functional)[https://github.com/battlesnake/kaiu/blob/master/functional.md]

 * (Event loop)[https://github.com/battlesnake/kaiu/blob/master/event_loop.md]

 * (Task)[https://github.com/battlesnake/kaiu/blob/master/task.md]

 * (Promise stream)[https://github.com/battlesnake/kaiu/blob/master/promise_stream.md]

 * (Task stream)[https://github.com/battlesnake/kaiu/blob/master/task_stream.md]

 * (Assertion)[https://github.com/battlesnake/kaiu/blob/master/assertion.md]

 * (Tuple iteration)[https://github.com/battlesnake/kaiu/blob/master/tuple_iteration.md]

Building / running tests
------------------------

Requires g++, developed using version 5.2.0.

To build and run the tests:

	make tests

To build in release-mode (optimizations enabled, warnings disabled):

	make tests mode=release

To run a particular test, use the runner:

	./run_test [commands]

For a description of the commands, run:

	./run_test help:

For example, to build and run the promise test a leak check with valgrind:

	./run_test mem: promise
