#include <algorithm>
#include <cstdlib>
#include <exception>
#include <stdexcept>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <list>
#include "assertion.h"
#include "promise.h"
#include "promise_stream.h"
#include "task_stream.h"

using namespace std;
using namespace kaiu;

Assertions assert({
	{ nullptr, "Concurrency" },
	{ "SYNCHRO", "Synchronization logic" }
});

void synchronization_test()
{
	const long count = 500;
	const long nmax = 100;
	const long expect = (nmax * nmax + nmax) / 2;
	atomic<bool> failed{false};
	for (int i = 0; i < count && !failed; i++) {
		ParallelEventLoop loop{ {
			{ EventLoopPool::reactor, 1 },
			{ EventLoopPool::calculation, 1 },
			{ EventLoopPool::io_remote, 1 }
		} };
		mutex consumer_mx, producer_mx;
		unique_lock<mutex> consumer_lock(consumer_mx);
		unique_lock<mutex> producer_lock(producer_mx);
		/* Mimic async read from some remote source */
		auto get_remote_data = [&producer_mx, &loop] {
			PromiseStream<long, long> stream;
			auto producer_m = [stream, &producer_mx] (EventLoop&) {
				/* Lock to block until this thread is triggered */
				lock_guard<mutex> lock(producer_mx);
				/* Write a load of messages to the stream */
				for (long n = 1; n <= nmax; n++) {
					stream->write(n);
				}
				/* Resolve the stream promise */
				stream->resolve(+expect);
			};
			/* Asynchronously write to the stream */
			loop.push(EventLoopPool::same, producer_m);
			/*
			 * Return promise stream object
			 *
			 * if we wrote to the stream synchronously, then the stream would
			 * contain all the data and also the result - streaming can't start
			 * until the promise stream has been returned!  In such a case, why
			 * not just use a promise instead?
			 */
			return stream;
		};
		auto get_remote_data_task = promise::task_stream(
			promise::StreamFactory<long, long>{get_remote_data},
			EventLoopPool::io_remote,
			EventLoopPool::calculation,
			EventLoopPool::reactor);
		auto stream = get_remote_data_task(loop);
		/*
		 * Wrapped in async task so we can test with events bound before/after
		 * streaming started/resolved
		 */
		auto consumer_m = [stream, &consumer_mx, &failed] {
			/* Read data from stream and accumulate sum */
			auto consumer = [&failed] (long& accum, const long value) {
				accum += value;
				return failed ? StreamAction::Stop : StreamAction::Continue;
			};
			/* Verify that calculated sum is correct */
			auto verify = [&failed] (const pair<long, long>& res) {
				const long& result = res.first;
				const long& expect = res.second;
				if (expect != result) {
					assert.fail("SYNCHRO");
					failed = true;
				}
			};
			/* Lock to block until this thread is triggered */
			lock_guard<mutex> lock(consumer_mx);
			stream
				->stream<long>(consumer)
				->then(verify);
		};
		thread consumer_t(consumer_m);
		this_thread::sleep_for(30us);
		/* Alternate order of binding/producing */
		if (i % 2) {
			consumer_lock.unlock();
			this_thread::sleep_for(30us);
			producer_lock.unlock();
		} else {
			producer_lock.unlock();
			this_thread::sleep_for(30us);
			consumer_lock.unlock();
		}
		/* Shouldn't be necessary, destructors should do this anyway */
		consumer_t.join();
		loop.join();
	}
	assert.try_pass("SYNCHRO");
}

void concurrency_test()
{
	synchronization_test();
}

int main(int argc, char *argv[])
try {
	concurrency_test();
	return assert.print(argc, argv);
} catch (...) {
	assert.print_error();
}
