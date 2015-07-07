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

class AbstractEventLoop;

using EventFunc = std::function<void(AbstractEventLoop&)>;
using Event = std::unique_ptr<EventFunc>;

enum EventLoopPool {
	reactor = 100,
	interaction = 200,
	service = 300,
	controller = 400,
	calculation = 500,
	io_local = 600,
	io_remote = 700,
	custom_base = 100000
};

/*
 * Base class for event loops
 */
class AbstractEventLoop {
public:
	AbstractEventLoop& operator =(const AbstractEventLoop &) = delete;
	AbstractEventLoop(const AbstractEventLoop &) = delete;
	virtual void push(const EventLoopPool pool, const EventFunc& event) = 0;
protected:
	AbstractEventLoop() = default;
	~AbstractEventLoop() = default;
	virtual Event next(const EventLoopPool pool) = 0;
};

/*
 * Non-threaded event loop which exits when no more events are left to process
 */
class EventLoop : public virtual AbstractEventLoop {
public:
	EventLoop& operator =(const EventLoop &) = delete;
	EventLoop(const EventLoop &) = delete;
	EventLoop(const EventLoopPool pool, const EventFunc& start);
	virtual void push(const EventLoopPool pool, const EventFunc& event) override;
protected:
	virtual Event next(const EventLoopPool pool) override;
private:
	std::queue<std::pair<EventLoopPool, Event>> queue;
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
	ParallelEventLoop(const std::unordered_map<EventLoopPool, size_t, std::hash<int>> pools);
	void process_exceptions(std::function<void(std::exception&)> handler);
	virtual void push(const EventLoopPool pool, const EventFunc& event) override;
	~ParallelEventLoop() throw();
protected:
	virtual Event next(const EventLoopPool pool) override;
private:
	std::atomic<int> threads_starting;
	std::condition_variable threads_started_cv;
	std::mutex threads_started_mutex;
	std::vector<std::thread> threads;
	std::unordered_map<EventLoopPool, ConcurrentQueue<Event>, std::hash<int>> queues;
	std::queue<std::exception> exceptions;
	std::mutex exceptions_mutex;
	void do_threaded_loop(const EventLoopPool pool);
	void thread_started();
};

}
