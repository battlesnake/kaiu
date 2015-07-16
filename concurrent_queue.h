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
	ConcurrentQueue();
	/* Append event to end of queue */
	void push(const T& item);
	void push(const T&& item);
	template <typename... Args>
	void emplace(Args&&... args);
	/* Remove event from front of queue.  Wait if no events are in the queue. */
	bool pop(T& out);
	/*
	 * WaitGuard is instantiated when we start waiting for events, and destroyed
	 * when we stop waiting.  It is not instantiated if an event is already
	 * available, as no wait is required.  It is not instantiated if the queue
	 * is in non-blocking mode.
	 */
	template <typename WaitGuard, typename... GuardParam>
	bool pop(T& out, GuardParam&&... guard_param);
	/*
	 * In non-blocking mode, pop() does not do blocking waits.  Instead, it
	 * returns false immediately if there are no messages in the queue.  This is
	 * contrary to the usual blocking mode where pop() always returns true,
	 * waiting if necessary for a message to be added to the queue.
	 *
	 * The WaitGuard is never instantiated in non-blocking mode.
	 */
	void set_nonblocking(bool nonblocking = true);
	bool is_nonblocking() const;
private:
	atomic<bool> nonblocking{false};
	condition_variable stop_waiting;
	queue<T> events;
	mutex queue_mutex;
	void notify();
};

}

#ifndef concurrent_queue_tcc
#include "concurrent_queue.tcc"
#endif
