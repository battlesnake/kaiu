#pragma once
#include <queue>
#include <atomic>
#include <mutex>
#include <condition_variable>

namespace kaiu {


template <typename T>
class ConcurrentQueue {
public:
	ConcurrentQueue(const ConcurrentQueue&) = delete;
	ConcurrentQueue& operator=(const ConcurrentQueue&) = delete;
	ConcurrentQueue() = delete;
	explicit ConcurrentQueue(bool nowaiting = false);
	/* Append event to end of queue */
	void push(const T& item);
	void push(T&& item);
	template <typename... Args>
	void emplace(Args&&... args);
	/*
	 * Remove event from front of queue
	 *
	 * If there are no elements in the queue then either:
	 *   wait until there are if we're not in no-waiting mode
	 *   return false without waiting if we are in no-waiting mode
	 */
	bool pop(T& out);
	/*
	 * WaitGuard is instantiated when we start waiting for events, and destroyed
	 * when we stop waiting.  It is not instantiated if events are in the queue,
	 * as no wait is required.  It is not instantiated if the queue is in
	 * no-waiting mode.
	 */
	template <typename WaitGuard, typename... GuardParam>
	bool pop(T& out, GuardParam&&... guard_param);
	template <typename = void>
	bool pop(T& out) { return pop<int>(out, 0); }
	/* Set/unset no-waiting mode */
	void set_nowaiting(bool value = true);
	bool is_nowaiting() const;
	/* Test if queue is empty */
	bool isEmpty(bool is_locked = false) const;
	/* Mutex is exposed for ParallelEventLoop::join */
	mutable std::mutex queue_mutex;
private:
	std::queue<T> events;
	std::condition_variable unblock;
	std::atomic<bool> nowaiting{false};
	void notify();
};

}

#ifndef concurrent_queue_tcc
#include "concurrent_queue.tcc"
#endif
