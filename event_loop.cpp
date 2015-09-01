#include <stdexcept>
#include <algorithm>
#include "lock_many.h"
#include "event_loop.h"

namespace kaiu {

using namespace std;

thread_local EventLoopPool this_pool = EventLoopPool::unknown;

/*** EventLoop ***/

EventLoop::EventLoop(const EventLoopPool defaultPool)
{
	this->defaultPool = defaultPool;
}

/*** SynchronousEventLoop ***/

SynchronousEventLoop::SynchronousEventLoop(const EventFunc& start) : EventLoop()
{
	push(start);
	do_loop();
}

void SynchronousEventLoop::do_loop()
{
	while (events.size()) {
		auto event = next();
		(*event)(*this);
	}
}

void SynchronousEventLoop::push(const EventLoopPool pool, const EventFunc& event)
{
	events.emplace(new EventFunc(event));
}

auto SynchronousEventLoop::next(const EventLoopPool pool) -> Event
{
	auto event = move(events.front());
	events.pop();
	return event;
}

/*** ParallelEventLoop ***/

ParallelEventLoop::ParallelEventLoop(const unordered_map<EventLoopPool, int, EventLoopPoolHash> pools) : EventLoop()
{
	/* Count how many threads we need (including this thread) */
	int total_threads = 0;
	for (const auto& pair : pools) {
		const auto pool_size = pair.second;
		if (pool_size <= 0) {
			throw invalid_argument("Thread count specified for a pool is zero or negative.  Use SynchronousEventLoop for non-threaded event loop.");
			return;
		}
		total_threads += pool_size;
	}
	/* Include this thread in count of threads to initialize */
	starter_pistol.reset(total_threads + 1);
	/* Iterate over requested thread pools, creating threads */
	for (const auto& pair : pools) {
		const auto pool_type = pair.first;
		const auto pool_size = pair.second;
		queues.emplace(piecewise_construct, forward_as_tuple(pool_type), forward_as_tuple(false));
		for (int i = 0; i < pool_size; i++) {
			threads.emplace_back(bind(&ParallelEventLoop::do_threaded_loop, this, pool_type));
		}
	}
	/* Mark this thread as started, Wait for all threads to start */
	starter_pistol.ready();
}

void ParallelEventLoop::do_threaded_loop(const EventLoopPool pool)
{
	this_pool = pool;
	/*
	 * Mark this thread as "working".  This will be undone temporarily by any
	 * blocking wait operation on the event queue, via ConcurrentQueue<T>::pop.
	 */
	auto not_idle = threads_not_idle_counter.delta(+1);
	starter_pistol.ready();
	while (Event event = next(pool)) {
		try {
			(*event)(*this);
		} catch (...) {
			/* Store exception (uses exceptions_mutex) */
			exceptions.push(current_exception());
			/* Notify any ongoing join() that there is an exception to handle */
			threads_not_idle_counter.notify();
		}
	}
}

void ParallelEventLoop::push(const EventLoopPool pool, const EventFunc& event)
{
	/* const-param antipattern */
	auto _pool = pool == EventLoopPool::same ? current_pool() : pool;
	if (int(_pool) <= 0) {
		throw invalid_argument("Invalid thread pool");
	}
	ConcurrentQueue<Event>& queue = queues.at(_pool);
	queue.emplace(new EventFunc(event));
}

void ParallelEventLoop::process_exceptions(function<void(exception_ptr)> handler)
{
	exception_ptr ptr;
	while (exceptions.pop(ptr)) {
		if (handler) {
			handler(ptr);
		}
	}
}

auto ParallelEventLoop::next(const EventLoopPool pool) -> Event
{
	ConcurrentQueue<Event>& queue = queues.at(pool);
	Event event;
	/*
	 * If pop waits, this thread is considered idle during the wait.
	 *
	 * queue_mutex is always acquired during this call, and may be
	 * released/reacquired several times if a wait occurs.
	 *
	 * threads_not_idle_counter mutex will be acquired at the start and at the
	 * end of a wait, always while queue_mutex is acquired.
	 */
	if (queue.pop<ScopedCounter<int>::Guard>(event, threads_not_idle_counter, -1)) {
		return event;
	} else {
		/* No events available and queue is in non-blocking mode */
		return nullptr;
	}
}

void ParallelEventLoop::join(function<void(exception_ptr)> handler)
{
	/* Proxy iterator for iterating over queue mutexes */
	using src_it = typename decltype(queues)::const_iterator;
	class queue_mutex_iterator {
	public:
		queue_mutex_iterator(src_it it) : it(it) { }
		bool operator !=(const queue_mutex_iterator& b) { return it != b.it; }
		void operator ++() { ++it; }
		mutex& operator *() { return it->second.queue_mutex; }
		mutex& operator ->() { return it->second.queue_mutex; }
	private:
		src_it it;
	};
	/* Sanity check */
	if (current_pool() != EventLoopPool::unknown) {
		throw logic_error("join called from worker thread");
	}
	/* Loop until all queues are empty and all threads are idle */
	do {
		/* Handle pending exceptions */
		process_exceptions(handler);
		/* Wait until all threads are idle */
		threads_not_idle_counter.waitForZero();
		/* Lock all queues */
		lock_many lock(
			queue_mutex_iterator(queues.cbegin()),
			queue_mutex_iterator(queues.cend()));
		bool all_queues_are_empty =
			all_of(queues.cbegin(), queues.cend(),
				[] (auto& pair) { return pair.second.isEmpty(true); });
		/* If all queues are empty and all threads are idle, break */
		if (all_queues_are_empty && threads_not_idle_counter.isZero()) {
			break;
		}
	} while (true);
	/* Handle pending exceptions */
	process_exceptions(handler);
}

ParallelEventLoop::~ParallelEventLoop()
{
	/* Wait for all workers to finish working */
	join(nullptr);
	/*
	 * Put queues into no-waiting mode so that they terminate when no events
	 * are left to be processed (there should be no events left since we just
	 * came back from a join().  Setting nowaiting will wake any threads that
	 * are waiting for events.
	 */
	for (auto& pair : queues) {
		auto& queue = pair.second;
		queue.set_nowaiting(true);
	}
	/* Wait for all workers to terminate */
	for (auto& thread : threads) {
		thread.join();
	}
}

EventLoopPool ParallelEventLoop::current_pool()
{
	return this_pool;
}

}
