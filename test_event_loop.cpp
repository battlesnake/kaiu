#include <atomic>
#include <thread>
#include <chrono>
#include <string>
#include "assertion.h"
#include "event_loop.h"

using namespace kaiu;
using namespace chrono_literals;

Assertions assert({
	{ nullptr, "Single-threaded event loop" },
	{ "SORDER", "All events fire and they fire in order" },
	{ nullptr, "Multi-threaded event loop" },
	{ "MALL", "All events fired" }
});

void test_single()
{
	EventFunc taskA, taskB1, taskB2, taskC;
	atomic<int> b_count{0};
	string order = "";
	taskA = [&] (EventLoop& loop) {
		order += "A";
		b_count = 2;
		loop.push(taskB1);
		loop.push(taskB2);
	};
	taskB1 = [&] (EventLoop& loop) {
		order += "B1";
		if (--b_count == 0) {
			loop.push(taskC);
		}
	};
	taskB2 = [&] (EventLoop& loop) {
		order += "B2";
		if (--b_count == 0) {
			loop.push(taskC);
		}
	};
	taskC = [&] (EventLoop& loop) {
		order += "C";
	};
	SynchronousEventLoop loop(taskA);
	assert.expect(order, "AB1B2C", "SORDER");
}

void test_multi()
{
	ParallelEventLoop loop({
		{ EventLoopPool::reactor, 1 },
		{ EventLoopPool::calculation, 2 },
		{ EventLoopPool::io_local, 10 }
	});
	EventFunc taskA, taskB1, taskB2, taskC, taskD, taskE;
	const int d_rep = 30;
	atomic<int> b_count{0};
	atomic<int> d_count{d_rep};
	/* Order check */
	mutex order_lock;
	string order = "";
	auto order_push = [&order, &order_lock] (const string name) {
		lock_guard<mutex> lock(order_lock);
		order += name;
	};
	/* Tasks */
	taskA = [&] (EventLoop& loop) {
		order_push("A");
		b_count = 2;
		loop.push(EventLoopPool::calculation, taskB1);
		loop.push(EventLoopPool::calculation, taskB2);
	};
	taskB1 = [&] (EventLoop& loop) {
		this_thread::sleep_for(20ms);
		order_push("B1");
		if (--b_count == 0) {
			loop.push(EventLoopPool::reactor, taskC);
		}
	};
	taskB2 = [&] (EventLoop& loop) {
		order_push("B2");
		if (--b_count == 0) {
			loop.push(EventLoopPool::reactor, taskC);
		}
	};
	taskC = [&] (EventLoop& loop) {
		order_push("C");
		for (int i = 0; i < d_rep; i++) {
			loop.push(EventLoopPool::io_local, taskD);
		}
	};
	taskD = [&] (EventLoop& loop) {
		order_push("D");
		if (--d_count == 0) {
			loop.push(EventLoopPool::reactor, taskE);
		}
	};
	taskE = [&] (EventLoop& loop) {
		this_thread::sleep_for(100ms);
		order_push("E");
	};
	loop.push(EventLoopPool::reactor, taskA);
	loop.join();
	assert.expect(order, "AB2B1C" + string(d_rep, 'D') + "E", "MALL");
}

int main(int argc, char *argv[])
try {
	test_single();
	test_multi();
	return assert.print(argc, argv);
} catch (...) {
	assert.print_error();
}
