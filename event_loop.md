Event loop
==========

Synchronous
-----------

`SynchronousEventLoop` takes a job via its constructor and runs the job.  If
that job pushes any more jobs into the loop, they are then executed in order.

The constructor returns when there are no jobs remaining in the loop.

Tasks are executed in the current thread, this loop has no parallelism and is
not even thread-safe.  It is provided primarily for unit-testing jobs.

	auto finish = [] (EventLoop& loop) {
		cout << endl;
	};

	auto start = [] (EventLoop& loop) {
		/* Spawn 10 jobs */
		for (int i = 1; i <= 10; i++) {
			loop.push([i] (EventLoop& loop) {
				cout << i << " ";
			});
		}
		/* Jobs are executed in order - add the "finish" job to the end */
		loop.push(finish);
	};

	/* Construct loop with initial job - constructor returns when no jobs are left */
	SynchronousEventLoop loop(start);

Output:

	1 2 3 4 5 6 7 8 9 10

ParallelEventLoop
-----------------

This event loop creates one or more thread pools.  Jobs are submitted to the
loop along with an extra parameter specifying which thread pool to execute the
job in.

Jobs are not executed in the current thread (the thread which constructed the
event loop), but any exceptions thrown by jobs are queued for handling by the
current thread.

The destructor for `ParallelEventLoop` calls `join()` which blocks until all
worker threads are idle and all job queues are empty.

### Constructor

	ParallelEventLoop loop({
		{ EventLoopPool::reactor, 1 },
		{ EventLoopPool::calculation, 6 },
		{ EventLoopPool::io_local, 20 },
		{ EventLoopPool::io_remote, 100 }
	});

The constructor takes an `unordered_map` where keys are thread-pool types and
values are the number of threads which that pool should contain.

### Pools

Possible values for the pool type are defined in `event_loop.h`: `reactor`,
`interaction`, `service`, `controller`, `calculation`, `io_local`,
`io_remote`.

The event loop does no special treatment for different thread pools; the names
are provided purely for convenience.  

There are some other values also: `same`, `unknown`, `invalid`.  You can find out
which thread pool you're currently executing in by calling
`ParallelEventLoop::current_pool()`.  It will return `unknown` if called from a
thread that isn't in a `ParallelEventLoop`'s pool.  `invalid` is currently not
in use.  When pushing a task to the loop, one can specify `same` as the pool to
execute the task in, and the value of `current_pool()` will be used to determine
which pool to use.

### Submitting jobs

To submit jobs, use the `push` function:

	loop.push(job, pool)

For example:

	loop.push([] (auto& loop) { cout << "Hello" << endl; }, EventLoopPool::interaction);

### Waiting for jobs to finish

	loop.join();

Will block until all threads are idle and all job queues are empty.

Takes an optional parameter which is a callback function that gets called for
every exception that is caught in the worker threads (unhandled exceptions
thrown by jobs).  See the "handling exceptions" section below for an example.

### Handling exceptions

	process_exceptions([] (exception_ptr error) {
		try {
			rethrow_exception(error);
		} catch (const exception& ex) {
			cerr << ex.what() << endl;
		}
	});

The exeption-handler callback can also be used with the `join` method.
