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

class AbstractEventLoop;

template <typename Result, typename... Args>
class Task;

using EventFunc = function<void(AbstractEventLoop&)>;
using Event = unique_ptr<EventFunc>;

enum class EventLoopPool : int {
	reactor = 100,
	interaction = 200,
	service = 300,
	controller = 400,
	calculation = 500,
	io_local = 600,
	io_remote = 700,
	custom_base = 100000
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
class AbstractEventLoop {
public:
	AbstractEventLoop& operator =(const AbstractEventLoop&) = delete;
	AbstractEventLoop(const AbstractEventLoop&) = delete;
	virtual void push(const EventLoopPool pool, const EventFunc& event) = 0;
	void push(const EventFunc& event) { push(defaultPool, event); };
	template <typename... Args> void push(Task<Args...> task, Args... args) {
		task(*this, args...);
	};
	template <typename T> AbstractEventLoop& operator <<(T arg) {
		push(arg);
		return *this;
	};
protected:
	AbstractEventLoop(const EventLoopPool defaultPool = EventLoopPool::reactor);
	~AbstractEventLoop() = default;
	virtual Event next(const EventLoopPool pool) = 0;
	Event next() { return next(defaultPool); };
private:
	EventLoopPool defaultPool;
};

/*
 * Non-threaded event loop which exits when no more events are left to process
 */
class EventLoop : public virtual AbstractEventLoop {
public:
	EventLoop& operator =(const EventLoop &) = delete;
	EventLoop(const EventLoop&) = delete;
	EventLoop(const EventFunc& start);
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
class ParallelEventLoop : public virtual AbstractEventLoop {
public:
	ParallelEventLoop& operator =(const ParallelEventLoop &) = delete;
	ParallelEventLoop(const ParallelEventLoop &) = delete;
	ParallelEventLoop(const unordered_map<EventLoopPool, size_t, EventLoopPoolHash> pools);
	~ParallelEventLoop() throw();
	void process_exceptions(function<void(exception&)> handler);
	virtual void push(const EventLoopPool pool, const EventFunc& event) override;
protected:
	virtual Event next(const EventLoopPool pool) override;
private:
	atomic<int> threads_starting{0};
	condition_variable threads_started_cv;
	mutex threads_started_mutex;
	vector<thread> threads;
	unordered_map<EventLoopPool, ConcurrentQueue<Event>, EventLoopPoolHash> queues;
	queue<exception> exceptions;
	mutex exceptions_mutex;
	void do_threaded_loop(const EventLoopPool pool);
	void thread_started();
};

}

#include "task.h"
