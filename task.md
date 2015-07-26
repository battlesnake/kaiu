Task
====

A promise factory that is launched asynchronously via an event loop.

	curry_promise_factory = task(promise_factory, action_pool, reaction_pool);

 * `promise_factory` is a promise factory (a function which returns promises).

 * `action_pool` is the thread pool to execute `promise_factory` in.

 * `reaction_pool` is the thread pool that callbacks attached to the promise should
be executed in.

`EventLoopPool::same` may be specified for either (or both) of the pools.

The result type is a promise factory of the same type as `promise_factory`, but
encapsulated in a curry wrapper (see
(functional)[https://github.com/battlesnake/kaiu/blob/master/functional.md]).
This allows easy currying, such as binding a task to an event loop.

Example usage
-------------

Let's make a function that does slow I/O, and blocks, returning a `vector<string>`:

	vector<string> read_lines_func(const string filename);

Now let's convert it to a promise factory.  This still blocks, but returns a
`Promise<vector<string>>`:

	auto read_lines_fac = promise::factory(read_lines_func);
	
Now we convert the promise factory to a task.  The task will do slow, blocking
I/O in a "io_local" worker thread, immediately returning a
`Promise<vector<string>>` without blocking the calling thread.  The callbacks on
the promise are called in the "reactor" thread pool once the operation is
complete:

	auto read_lines_task = promise::task(read_lines_fac,
		EventLoopPool::io_local, EventLoopPool::reactor);

We have to specify an event loop when we call the task:

	read_lines_task(loop, "/etc/passwd")
		->then([] (auto& lines) { cout << "I haz ur passwd" << endl; });

Curry the task so we don't have to specify the loop at each call site:

	auto read_lines = task.apply(loop);

Now our calls look a bit DRYer:

	read_lines("/etc/passwd")
		->then([] (auto& lines) { cout << "I haz ur passwd" << endl; });

Task monads
-----------

You can enable task monads by `#define`'ing `enable_task_monads` before you
`#include` `task.h`.  As with the functional monads, it is questionable whether
they are a good idea in production code.  There is a demonstration in the
unit-test for `functional` (test_functional.cpp).

	#include <iostream>
	#define enable_task_monads
	#include "task.h"

	using namespace kaiu;

	ParallelEventLoop loop{ {
		{ EventLoopPool::reactor, 1 },
		{ EventLoopPool::calculation, 1 }
	} };

	Promise<int> add_fac(int x, int y) {
		return promise::resolved(x + y);
	}

	Promise<int> square_fac(int x) {
		return promise::resolved(x * x);
	}

	Promise<void *> log_data(const string s, int x) {
		std::cout << x << " " << s << std::endl;
	}

	auto add = task(add_fac, EventLoopPool::calculation, EventLoopPool::reactor);
	auto square = task(square_fac, EventLoopPool::calculation, EventLoopPool::reactor);
	auto log = task(log_data, EventLoopPool::reactor);

	int main(int argc, char **argv)
	{
		/* Monads and currying: outputs "101 dalmations" */
		3 | add_fac << 7 | square_fac | add_fac << 1 | log_data << "dalmations";
		/* The above is the same as: */
		log_data("dalmations", add_fac(1, square_fac(add_fac(7, 3))));
	}
