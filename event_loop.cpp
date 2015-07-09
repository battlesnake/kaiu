#include "event_loop.h"

namespace mark {

using namespace std;

/*** AbstractEventLoop ***/

AbstractEventLoop::AbstractEventLoop(const EventLoopPool defaultPool)
{
	this->defaultPool = defaultPool;
}

/*** EventLoop ***/

EventLoop::EventLoop(const EventFunc& start) : AbstractEventLoop()
{
	push(start);
	do_loop();
}

void EventLoop::do_loop()
{
	while (events.size()) {
		auto event = next();
		(*event)(*this);
	}
}

void EventLoop::push(const EventLoopPool pool, const EventFunc& event)
{
	events.emplace(new EventFunc(event));
}

Event EventLoop::next(const EventLoopPool pool)
{
	auto event = move(events.front());
	events.pop();
	return event;
}

/*** ParallelEventLoop ***/

ParallelEventLoop::ParallelEventLoop(const unordered_map<EventLoopPool, size_t, EventLoopPoolHash> pools) : AbstractEventLoop()
{
	threads_starting = 1;
	for (const auto& pair : pools) {
		const auto pool_type = pair.first;
		const auto pool_size = pair.second;
		if (pool_size == 0) {
			throw invalid_argument("Thread count specified is zero.  Use EventLoop for non-threaded event loop.");
			return;
		}
		queues.emplace(piecewise_construct, forward_as_tuple(pool_type), forward_as_tuple());
		for (unsigned int i = 0; i < pool_size; i++) {
			threads_starting++;
			threads.emplace_back(bind(&ParallelEventLoop::do_threaded_loop, this, pool_type));
		}
	}
	/* Wait for all threads to start */
	thread_started();
}

void ParallelEventLoop::thread_started()
{
	unique_lock<mutex> lock(threads_started_mutex);
	if (--threads_starting == 0) {
		threads_started_cv.notify_all();
	} else {
		threads_started_cv.wait(lock, [this] { return threads_starting == 0; });
	}
}

void ParallelEventLoop::do_threaded_loop(const EventLoopPool pool)
{
	thread_started();
	Event event;
	while ((event = next(pool)) != nullptr) {
		try {
			(*event)(*this);
		} catch (exception& e) {
			lock_guard<mutex> lock(exceptions_mutex);
			exceptions.push(move(e));
		}
	}
}

void ParallelEventLoop::push(const EventLoopPool pool, const EventFunc& event)
{
	ConcurrentQueue<Event>& queue = queues.at(pool);
	queue.emplace(new EventFunc(event));
}

Event ParallelEventLoop::next(const EventLoopPool pool)
{
	ConcurrentQueue<Event>& queue = queues.at(pool);
	Event event;
	if (queue.pop(event)) {
		return event;
	} else {
		return nullptr;
	}
}

void ParallelEventLoop::process_exceptions(function<void(exception&)> handler)
{
	while (true) {
		exception e;
		{
			lock_guard<mutex> lock(exceptions_mutex);
			if (exceptions.size() == 0) {
				break;
			}
			e = move(exceptions.front());
			exceptions.pop();
		}
		handler(e);
	}
}

ParallelEventLoop::~ParallelEventLoop() throw()
{
	for (auto& pair : queues) {
		pair.second.stop();
	}
	for (auto& thread : threads) {
		thread.join();
	}
}

}

#ifdef test_event_loop
#include <stdio.h>
#include <stdlib.h>
#include <atomic>
#include <thread>
#include <chrono>
#include <random>

using namespace mark;
using namespace chrono_literals;

void test_single()
{
	printf("Testing single-threaded event loop\n");
	EventFunc taskA, taskB1, taskB2, taskC;
	atomic<int> b_count{0};
	taskA = [&] (AbstractEventLoop& loop) {
		printf(" * Event A\n");
		b_count = 2;
		loop.push(taskB1);
		loop.push(taskB2);
	};
	taskB1 = [&] (AbstractEventLoop& loop) {
		this_thread::sleep_for(200ms);
		printf(" * Event B1\n");
		if (--b_count == 0) {
			loop.push(taskC);
		}
	};
	taskB2 = [&] (AbstractEventLoop& loop) {
		this_thread::sleep_for(100ms);
		printf(" * Event B2\n");
		if (--b_count == 0) {
			loop.push(taskC);
		}
	};
	taskC = [&] (AbstractEventLoop& loop) {
		printf(" * Event C\n");
	};
	EventLoop loop(taskA);
	printf("\n");
}

void test_multi()
{
	printf("Testing multi-threaded event loop\n");
	ParallelEventLoop loop({
		{ EventLoopPool::reactor, 1 },
		{ EventLoopPool::calculation, 2 },
		{ EventLoopPool::io_local, 10 }
	});
	EventFunc taskA, taskB1, taskB2, taskC, taskD, taskE;
	const int d_rep = 30;
	atomic<int> b_count{0};
	atomic<int> d_idx{0}, d_count{d_rep};
	atomic<bool> done{false};
	mutex done_mutex;
	condition_variable done_cv;
	taskA = [&] (AbstractEventLoop& loop) {
		printf(" * Event A\n");
		b_count = 2;
		loop.push(EventLoopPool::calculation, taskB1);
		loop.push(EventLoopPool::calculation, taskB2);
	};
	taskB1 = [&] (AbstractEventLoop& loop) {
		this_thread::sleep_for(200ms);
		printf(" * Event B1\n");
		if (--b_count == 0) {
			loop.push(EventLoopPool::reactor, taskC);
		}
	};
	taskB2 = [&] (AbstractEventLoop& loop) {
		this_thread::sleep_for(100ms);
		printf(" * Event B2\n");
		if (--b_count == 0) {
			loop.push(EventLoopPool::reactor, taskC);
		}
	};
	taskC = [&] (AbstractEventLoop& loop) {
		printf(" * Event C\n");
		this_thread::sleep_for(100ms);
		printf(" * Event D:");
		for (int i = 0; i < d_rep; i++) {
			loop.push(EventLoopPool::io_local, taskD);
		}
	};
	taskD = [&] (AbstractEventLoop& loop) {
		int idx = ++d_idx;
		random_device rd;
		mt19937 gen(rd());
		uniform_int_distribution<> d(50, 150);
		this_thread::sleep_for(1ms * d(gen));
		printf(" %d", idx);
		if (--d_count == 0) {
			printf("\n");
			loop.push(EventLoopPool::reactor, taskE);
		}
	};
	taskE = [&] (AbstractEventLoop& loop) {
		printf(" * Event E\n");
		done = true;
		done_cv.notify_one();
	};
	loop.push(EventLoopPool::reactor, taskA);
	unique_lock<mutex> lock(done_mutex);
	done_cv.wait(lock, [&done] { return (bool) done; });
	printf("\n");
}

int main(int argc, char *argv[])
{
	test_single();
	test_multi();
	return 0;
}
#endif
