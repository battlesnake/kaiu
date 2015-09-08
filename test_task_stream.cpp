#include <exception>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <stdio.h>
#include <queue>
#include <atomic>
#include "assertion.h"
#include "promise.h"
#include "promise_stream.h"
#include "task_stream.h"

using namespace std;
using namespace std::chrono;
using namespace std::literals;
using namespace kaiu;

Assertions assert({
	{ nullptr, "Concurrency" },
	{ "SYNCHRO", "Deadlock test" },
	{ "RSYNCHRO", "Random deadlock test" }
});

void synchronization_test()
{
	/*
	 * TODO: All possible orders that PromiseStream events/transitions can
	 * occur in, then loop it 10000 times to be more certain.
	 */
	assert.skip("SYNCHRO");
}

void random_synchronization_test()
{
	const long count = 50000;
	printf("Attempting to trigger deadlock:\n");
	printf(" * If counter freezes, test has failed\n");
	printf(" * Tests n<1000 do not log state chars\n\n");
	/* Logging state */
	static constexpr int len = 8;
	class state_t {
	public:
		state_t(bool enabled) : enabled(enabled) { }
		void write(const char c) { if (enabled) { ch[state_i++] = c; modified_ = true; } }
		void print() { modified_ = false; printf("%s", ch); }
		bool modified() const { return modified_; }
		bool done() const { return state_i == len; }
	private:
		const bool enabled;
		char ch[len+1] = {32,32,32,32,32,32,32,32,0};
		atomic<int> state_i{0};
		atomic<bool> modified_{true};
	};
	atomic<bool> failed{false};
	for (int i = 1; i <= count && !failed; i++) {
		const bool log_state = i > 1000;
		const long nmax = (i % 2) ? 1000 : 1;
		const long expect = (nmax * nmax + nmax) / 2;
		ParallelEventLoop loop{ {
			{ EventLoopPool::reactor, 1 },
			{ EventLoopPool::calculation, 1 },
			{ EventLoopPool::io_remote, 1 }
		} };
		mutex consumer_mx, producer_mx;
		unique_lock<mutex> consumer_lock(consumer_mx);
		unique_lock<mutex> producer_lock(producer_mx);
		state_t state(log_state);
		printf("\r  Run %d of %ld... ", i, count);
		state.print();
		/* Mimic async read from some remote source */
		auto get_remote_data = [&producer_mx, &loop, nmax, expect, &state] () -> PromiseStream<long, long> {
			PromiseStream<long, long> stream;
			auto producer_m = [stream, &producer_mx, nmax, expect, &state] (EventLoop&) {
				/* Lock to block until this thread is triggered */
				lock_guard<mutex> lock(producer_mx);
				state.write('S');
				state.write('W');
				/* Write a load of messages to the stream */
				for (long n = 1; n <= nmax; n++) {
					stream->write(n);
				}
				state.write('W');
				/* Resolve the stream promise */
				stream->resolve(expect);
				state.write('R');
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
		auto consumer_m = [stream, &consumer_mx, &failed, expect, nmax, &state] {
			/* Read data from stream and accumulate sum */
			auto consumer = [&failed, &state, nmax] (long& accum, const long value) {
				if (accum == 0) {
					state.write('w');
				}
				if (value == nmax) {
					state.write('w');
				}
				accum += value;
				return failed ? StreamAction::Stop : StreamAction::Continue;
			};
			/* Verify that calculated sum is correct */
			auto verify = [&failed, expect, &state] (const pair<long, long> res) {
				const long& result = res.first;
				const long& expect = res.second;
				if (expect != result) {
					assert.fail("RSYNCHRO", "Incorrect result");
					failed = true;
				}
				state.write('r');
			};
			auto handler = [&failed] (auto e) {
				assert.fail("RSYNCHRO", "Consumer chain failed");
				failed = true;
			};
			/* Lock to block until this thread is triggered */
			lock_guard<mutex> lock(consumer_mx);
			state.write('s');
			stream
				->stream<long>(consumer)
				->then(verify, handler);
		};
		thread consumer_t(consumer_m);
		this_thread::sleep_for(10us);
		/* Alternate order of binding/producing */
		const char order = i / 7 % 4;
		const auto delay = duration<size_t, nano>(rand() % 1000000 + 50);
		switch (order) {
		case 0:
			consumer_lock.unlock();
			this_thread::sleep_for(delay);
			producer_lock.unlock();
			break;
		case 1:
			producer_lock.unlock();
			this_thread::sleep_for(delay);
			consumer_lock.unlock();
			break;
		case 2:
			producer_lock.unlock();
			consumer_lock.unlock();
			break;
		case 3:
			consumer_lock.unlock();
			producer_lock.unlock();
			break;
		}
		/* Monitor threads */
		do {
			if (!log_state) break;
			this_thread::sleep_for(1us);
			if (state.modified()) {
				printf("\r  Run %d of %ld... ", i, count);
				state.print();
				fflush(stdout);
			}
		} while (!state.done() && !failed);
		consumer_t.join();
	}
	printf("\r  Run %ld of %ld - DONE        \n\n", count, count);
	assert.try_pass("RSYNCHRO");
}

void concurrency_test()
{
	synchronization_test();
	//random_synchronization_test();
	assert.skip("RSYNCHRO");
}

int main(int argc, char *argv[])
try {
	concurrency_test();
	return assert.print(argc, argv);
} catch (...) {
	assert.print_error();
}
