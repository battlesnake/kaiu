#pragma once
#include <queue>
#include <atomic>
#include <mutex>
#include <condition_variable>

namespace mark {

using namespace std;

template <typename T>
class ConcurrentQueue {
public:
	ConcurrentQueue(const ConcurrentQueue&) = delete;
	ConcurrentQueue& operator=(const ConcurrentQueue&) = delete;
	ConcurrentQueue() = delete;
	ConcurrentQueue(mutex& queue_mutex, bool nowaiting = false);
	/* Append event to end of queue */
	void push(const T& item);
	void push(const T&& item);
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
	/* Set/unset no-waiting mode */
	void set_nowaiting(bool value = true);
	bool is_nowaiting() const;
	/* Test if queue is empty */
	bool isEmpty();
	bool isEmpty(const unique_lock<mutex>& lock) const;
private:
	atomic<bool> nowaiting{false};
	condition_variable unblock;
	queue<T> events;
	mutex& queue_mutex;
	void notify();
};

}

#ifndef concurrent_queue_tcc
#include "concurrent_queue.tcc"
#endif
