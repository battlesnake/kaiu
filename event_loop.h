#pragma once
#include <functional>
#include <memory>
#include <queue>
#include <vector>
#include <thread>
#include <mutex>
#include <exception>
#include <stdexcept>
#include <unordered_map>
#include "concurrent_queue.h"
#include "starter_pistol.h"
#include "scoped_counter.h"

namespace kaiu {

using namespace std;

class EventLoop;

using EventFunc = function<void(EventLoop&)>;

enum class EventLoopPool : int {
	same = -2,
	unknown = -1,
	invalid = 0,
	reactor = 100,
	interaction = 200,
	service = 300,
	controller = 400,
	calculation = 500,
	io_local = 600,
	io_remote = 700
};

struct EventLoopPoolHash
{
	template <typename T> std::size_t operator()(T t) const
	{
		return static_cast<std::size_t>(t);
	}
};

/*
 * Base class for event loops
 */
class EventLoop {
public:
	EventLoop& operator =(const EventLoop&) = delete;
	EventLoop(const EventLoop&) = delete;
	/*
	 * Push an event into the queue
	 */
	virtual void push(const EventLoopPool pool, const EventFunc& event) = 0;
	void push(const EventFunc& event) { push(defaultPool, event); };
protected:
	using Event = unique_ptr<EventFunc>;
	EventLoop(const EventLoopPool defaultPool = EventLoopPool::reactor);
	virtual ~EventLoop() = default;
	virtual Event next(const EventLoopPool pool) = 0;
	Event next() { return next(defaultPool); };
private:
	EventLoopPool defaultPool{EventLoopPool::reactor};
};

/*
 * Non-threaded event loop which exits when no more events are left to process
 *
 * "pool" parameter ignored
 */
class SynchronousEventLoop : public virtual EventLoop {
public:
	SynchronousEventLoop& operator =(const EventLoop &) = delete;
	SynchronousEventLoop(const EventLoop&) = delete;
	SynchronousEventLoop(const EventFunc& start);
	virtual void push(const EventLoopPool pool, const EventFunc& event) override;
	void push(const EventFunc& event) { push(EventLoopPool::reactor, event); };
protected:
	virtual Event next(const EventLoopPool pool) override;
	Event next() { return next(EventLoopPool::reactor); };
private:
	queue<Event> events;
	void do_loop();
};

/*
 * Multi-threaded event loop which keeps running until terminate has been called
 * and all messages have been processed.
 *
 * Exceptions raised by events are stored in an array which can be processed by
 * the process_exceptions member function (e.g. periodically by the main
 * thread).
 */
class ParallelEventLoop : public virtual EventLoop {
public:
	ParallelEventLoop& operator =(const ParallelEventLoop &) = delete;
	ParallelEventLoop(const ParallelEventLoop &) = delete;
	ParallelEventLoop(const unordered_map<EventLoopPool, int, EventLoopPoolHash> pools);
	virtual ~ParallelEventLoop() override; 
	/* If handler is nullptr, the exceptions are discarded */
	void process_exceptions(function<void(exception_ptr&)> handler = nullptr);
	virtual void push(const EventLoopPool pool, const EventFunc& event) override;
	/*
	 * Returns when all threads are idle and no events are pending.
	 *
	 * Calls handler on all queued exceptions and on any that are queued during
	 * the wait.
	 */
	void join(function<void(exception_ptr&)> handler = nullptr);
	/*
	 * Get which pool the current thread is in.  Returns unknown if not a
	 * ParallelEventLoop-pooled thread, e.g. the application's main thread.
	 */
	static EventLoopPool current_pool();
protected:
	virtual Event next(const EventLoopPool pool) override;
private:
	/* Cause all threads to start at the same time */
	unique_ptr<StarterPistol> starter_pistol;
	/* Threads */
	vector<thread> threads;
	/* Event queues (one per pool) */
	unordered_map<EventLoopPool, ConcurrentQueue<Event>, EventLoopPoolHash> queues;
	/* Mutex shared by all queues */
	mutex queue_mutex;
	/* Exception queue */
	mutex exceptions_mutex;
	queue<exception_ptr> exceptions;
	/* Thread entry point */
	void do_threaded_loop(const EventLoopPool pool);
	/*
	 * Count of threads that are not idle (idle = waiting for events)
	 *
	 * Triggers a condition variable whenever the number if idle threads
	 * changes.  Is also triggered by this class when an exception is queued.
	 */
	ScopedCounter<int> threads_not_idle_counter;
};

}
