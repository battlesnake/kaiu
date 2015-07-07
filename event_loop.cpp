#include "event_loop.h"

namespace mark {

EventLoop::EventLoop(const EventLoopPool pool, const EventFunc& start)
{
	push(pool, start);
	do_loop();
}

void EventLoop::do_loop()
{
	while (queue.size()) {
		auto event = next(EventLoopPool::reactor);
		(*event)(*this);
	}
}

void EventLoop::push(const EventLoopPool pool, const EventFunc& event)
{
	queue.emplace(std::make_pair(pool, Event(new EventFunc(event))));
}

Event EventLoop::next(const EventLoopPool pool)
{
	auto event = std::move(queue.front().second);
	queue.pop();
	return event;
}

ParallelEventLoop::ParallelEventLoop(const std::unordered_map<EventLoopPool, size_t, std::hash<int>> pools)
{
	threads_starting = 1;
	for (const auto& pair : pools) {
		const auto pool_type = pair.first;
		const auto pool_size = pair.second;
		if (pool_size == 0) {
			throw std::invalid_argument("Thread count specified is zero.  Use EventLoop for non-threaded event loop.");
			return;
		}
		queues.emplace(std::piecewise_construct, std::forward_as_tuple(pool_type), std::forward_as_tuple());
		for (unsigned int i = 0; i < pool_size; i++) {
			threads_starting++;
			threads.emplace_back(std::bind(&ParallelEventLoop::do_threaded_loop, this, pool_type));
		}
	}
	/* Wait for all threads to start */
	thread_started();
}

void ParallelEventLoop::thread_started()
{
	std::unique_lock<std::mutex> lock(threads_started_mutex);
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
		} catch (std::exception& e) {
			std::lock_guard<std::mutex> lock(exceptions_mutex);
			exceptions.push(std::move(e));
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

void ParallelEventLoop::process_exceptions(std::function<void(std::exception&)> handler)
{
	while (true) {
		std::exception e;
		{
			std::lock_guard<std::mutex> lock(exceptions_mutex);
			if (exceptions.size() == 0) {
				break;
			}
			e = std::move(exceptions.front());
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
using namespace std::chrono_literals;

void test_single()
{
	printf("Testing single-threaded event loop\n");
	EventFunc taskA, taskB1, taskB2, taskC;
	std::atomic<int> b_count;
	taskA = [&] (AbstractEventLoop& loop) {
		printf(" * Event A\n");
		b_count = 2;
		loop.push(EventLoopPool::reactor, taskB1);
		loop.push(EventLoopPool::reactor, taskB2);
	};
	taskB1 = [&] (AbstractEventLoop& loop) {
		std::this_thread::sleep_for(200ms);
		printf(" * Event B1\n");
		if (--b_count == 0) {
			loop.push(EventLoopPool::reactor, taskC);
		}
	};
	taskB2 = [&] (AbstractEventLoop& loop) {
		std::this_thread::sleep_for(100ms);
		printf(" * Event B2\n");
		if (--b_count == 0) {
			loop.push(EventLoopPool::reactor, taskC);
		}
	};
	taskC = [&] (AbstractEventLoop& loop) {
		printf(" * Event C\n");
	};
	EventLoop loop(EventLoopPool::reactor, taskA);
	printf("\n");
}

void test_multi()
{
	printf("Testing multi-threaded event loop\n");
	ParallelEventLoop loop({ { EventLoopPool::reactor, 8 } });
	EventFunc taskA, taskB1, taskB2, taskC, taskD, taskE;
	std::atomic<int> b_count;
	std::atomic<int> d_idx, d_count;
	const int d_rep = 30;
	std::mutex mutex;
	std::condition_variable cv;
	std::atomic<bool> done;
	taskA = [&] (AbstractEventLoop& loop) {
		printf(" * Event A\n");
		b_count = 2;
		loop.push(EventLoopPool::reactor, taskB1);
		loop.push(EventLoopPool::reactor, taskB2);
	};
	taskB1 = [&] (AbstractEventLoop& loop) {
		printf(" * Event B1\n");
		if (--b_count == 0) {
			loop.push(EventLoopPool::reactor, taskC);
		}
	};
	taskB2 = [&] (AbstractEventLoop& loop) {
		printf(" * Event B2\n");
		if (--b_count == 0) {
			loop.push(EventLoopPool::reactor, taskC);
		}
	};
	taskC = [&] (AbstractEventLoop& loop) {
		printf(" * Event C\n");
		d_count = d_rep;
		d_idx = 0;
		std::this_thread::sleep_for(100ms);
		printf(" * Event D\n");
		printf("    * ");
		for (int i = 0; i < d_rep; i++) {
			loop.push(EventLoopPool::reactor, taskD);
		}
	};
	taskD = [&] (AbstractEventLoop& loop) {
		int idx = ++d_idx;
		std::random_device rd;
		std::mt19937 gen(rd());
		std::uniform_int_distribution<> d(50, 150);
		std::this_thread::sleep_for(1ms * d(gen));
		printf("%d ", idx);
		if (--d_count == 0) {
			printf("\n");
			loop.push(EventLoopPool::reactor, taskE);
		}
	};
	taskE = [&] (AbstractEventLoop& loop) {
		printf(" * Event E\n");
		done = true;
		cv.notify_one();
	};
	done = false;
	loop.push(EventLoopPool::reactor, taskA);
	std::unique_lock<std::mutex> lock(mutex);
	cv.wait(lock, [&done] { return (bool) done; });
	printf("\n");
}

int main(int argc, char *argv[])
{
	test_single();
	test_multi();
	return 0;
}
#endif
