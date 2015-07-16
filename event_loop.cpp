#include <stdexcept>
#include "event_loop.h"

namespace mark {

using namespace std;

thread_local EventLoopPool this_pool = EventLoopPool::unknown;

EventLoopPool current_pool()
{
	return this_pool;
}

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
	/*
	 * Include this thread in count of threads to initialize.  Prevents the
	 * thread_started function from returning prematurely if worker threads
	 * somehow start really quickly, by requiring them to wait for this thread
	 * to also call thread_started.
	 */
	threads_starting = 1;
	/* Iterate over requested thread pools, creating threads */
	for (const auto& pair : pools) {
		const auto pool_type = pair.first;
		const auto pool_size = pair.second;
		if (pool_size == 0) {
			throw invalid_argument("Thread count specified for a pool is zero.  Use SynchronousEventLoop for non-threaded event loop.");
			return;
		}
		queues.emplace(piecewise_construct, forward_as_tuple(pool_type), forward_as_tuple());
		for (int i = 0; i < pool_size; i++) {
			threads_starting++;
			threads.emplace_back(bind(&ParallelEventLoop::do_threaded_loop, this, pool_type));
		}
	}
	/* Mark this thread as started, Wait for all threads to start */
	thread_started();
}

void ParallelEventLoop::thread_started()
{
	/*
	 * Marks this thread as having started, waits until all others have started
	 * before returning
	 */
	unique_lock<mutex> lock(threads_started_mutex);
	if (--threads_starting == 0) {
		threads_started_cv.notify_all();
	} else {
		threads_started_cv.wait(lock, [this] { return threads_starting == 0; });
	}
}

void ParallelEventLoop::do_threaded_loop(const EventLoopPool pool)
{
	this_pool = pool;
	/*
	 * Mark this thread as "working".  This will be undone temporarily by any
	 * blocking wait operation on the event queue, via ConcurrentQueue<T>::pop.
	 */
	WorkTracker work_guard(*this, +1);
	thread_started();
	while (Event event = next(pool)) {
		try {
			(*event)(*this);
		} catch (exception& e) {
			lock_guard<mutex> lock(exceptions_mutex);
			exceptions.push(move(e));
			/*
			 * We need a better name for this condition_variable.
			 *
			 * Notify any ongoing join() operation that there is an exception to
			 * handle.
			 */
			threads_working_cv.notify_all();
		}
	}
}

void ParallelEventLoop::push(const EventLoopPool pool, const EventFunc& event)
{
	ConcurrentQueue<Event>& queue = queues.at(pool);
	queue.emplace(new EventFunc(event));
}

auto ParallelEventLoop::next(const EventLoopPool pool) -> Event
{
	ConcurrentQueue<Event>& queue = queues.at(pool);
	Event event;
	/*
	 * If pop blocks, the threads_working count will be decremented during the
	 * block.
	 */
	if (queue.pop<WorkTracker>(event, *this, -1)) {
		return event;
	} else {
		/* No events available and queue is in non-blocking mode */
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
		if (handler) {
			handler(e);
		}
	}
}

void ParallelEventLoop::join(function<void(exception&)> handler)
{
	if (current_pool() != EventLoopPool::unknown) {
		throw logic_error("join called from worker thread");
	}
	unique_lock<mutex> lock(threads_working_mutex);
	/*
	 * We need a better name for the condition variable.  It is also triggered
	 * when a worker thread has queued an exception, hence why this loop below
	 * actually works.
	 */
	threads_working_cv.wait(lock, [handler, this] {
		process_exceptions(handler);
		return threads_working == 0;
	});
}

void ParallelEventLoop::set_queue_nonblocking_mode(bool nonblocking)
{
	/*
	 * Event loops in worker threads will terminate if in nonblocking mode and
	 * there are no events to process.
	 */
	for (auto& pair : queues) {
		pair.second.set_nonblocking(nonblocking);
	}
}

ParallelEventLoop::~ParallelEventLoop()
{
	/* Wait for all workers to finish working */
	join(nullptr);
	/*
	 * Put queues into non-blocking mode so that they terminate when no events
	 * are left to be processed (there should be no events left since we just
	 * came back from a join().
	 */
	set_queue_nonblocking_mode(true);
	/* Wait for all workers to terminate */
	for (auto& thread : threads) {
		thread.join();
	}
}

/*** ParallelEventLoop::WorkTracker ***/

ParallelEventLoop::WorkTracker::
	WorkTracker(ParallelEventLoop& loop, const int delta) :
		loop(loop), delta(delta)
{
	loop.threads_working += delta;
	notify();
}

ParallelEventLoop::WorkTracker::~WorkTracker()
{
	loop.threads_working -= delta;
	notify();
}

void ParallelEventLoop::WorkTracker::notify() const
{
	loop.threads_working_cv.notify_all();
}

}
