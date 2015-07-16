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

namespace mark {

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
 * Get which pool the current thread is in.  Returns unknown if not a
 * ParallelEventLoop-pooled thread, e.g. the main thread.
 */
EventLoopPool current_pool();

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
	/*
	 * Usage: loop.push(task)
	 * Where exists:
	 *   Promise<Result> Task::operator ()(EventLoop& loop, Args...)
	 */
	template <typename Task, typename... Args>
		decltype(Task::operator ()(Args{}...)) push(Task task, Args&&... args)
			{ return task(*this, forward<Args>(args)...); };
	/*
	 * Submit events / tasks using << operator, as used with streams.
	 */
	template <typename T> EventLoop& operator <<(T arg) {
		push(arg);
		return *this;
	};
protected:
	using Event = unique_ptr<EventFunc>;
	EventLoop(const EventLoopPool defaultPool = EventLoopPool::reactor);
	~EventLoop() = default;
	virtual Event next(const EventLoopPool pool) = 0;
	Event next() { return next(defaultPool); };
private:
	EventLoopPool defaultPool;
};

/*
 * Non-threaded event loop which exits when no more events are left to process
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
	~ParallelEventLoop();
	/* If handler is nullptr, the exceptions are discarded */
	void process_exceptions(function<void(exception&)> handler = nullptr);
	virtual void push(const EventLoopPool pool, const EventFunc& event) override;
	/*
	 * Returns when all threads are idle and no events are pending.
	 *
	 * Calls handler on all queued exceptions and on any that are queued during
	 * the wait.
	 */
	void join(function<void(exception&)> handler = nullptr);
protected:
	virtual Event next(const EventLoopPool pool) override;
private:
	/* Track starting threads */
	atomic<int> threads_starting{0};
	condition_variable threads_started_cv;
	mutex threads_started_mutex;
	void thread_started();
	/* Threads */
	vector<thread> threads;
	/* Event queues (one per pool) */
	unordered_map<EventLoopPool, ConcurrentQueue<Event>, EventLoopPoolHash> queues;
	void set_queue_nonblocking_mode(bool nonblocking);
	/* Exception queue */
	queue<exception> exceptions;
	mutex exceptions_mutex;
	/* Thread entry point */
	void do_threaded_loop(const EventLoopPool pool);
	/* Count of threads that are doing stuff */
	atomic<int> threads_working{0};
	condition_variable threads_working_cv;
	mutex threads_working_mutex;
	/*
	 * Used to track number of threads that are busy, is passed as a WaitGuard
	 * to ConcurrentQueue::pop to track idling.  All hail RAII.
	 */
	class WorkTracker {
	public:
		WorkTracker() = delete;
		WorkTracker(const WorkTracker&) = delete;
		WorkTracker(WorkTracker&&) = delete;
		WorkTracker(ParallelEventLoop& loop, const int delta);
		~WorkTracker();
	private:
		ParallelEventLoop& loop;
		const int delta;
		void notify() const;
	};
};

}
