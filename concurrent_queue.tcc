#define concurrent_queue_tcc
#include <stdexcept>
#include "concurrent_queue.h"

namespace kaiu {


template <typename T>
ConcurrentQueue<T>::ConcurrentQueue(bool nowaiting)
	: nowaiting(nowaiting)
{
}

template <typename T>
void ConcurrentQueue<T>::push(const T& item)
{
	std::lock_guard<std::mutex> lock(queue_mutex);
	events.push(item);
	notify();
}

template <typename T>
void ConcurrentQueue<T>::push(T&& item)
{
	std::lock_guard<std::mutex> lock(queue_mutex);
	events.push(std::move(item));
	notify();
}

template <typename T>
template <typename... Args> void ConcurrentQueue<T>::emplace(Args&&... args)
{
	std::lock_guard<std::mutex> lock(queue_mutex);
	events.emplace(std::forward<Args...>(args...));
	notify();
}

template <typename T>
bool ConcurrentQueue<T>::pop(T& out)
{
	return pop<bool>(out, false);
}

template <typename T>
template <typename WaitGuard, typename... GuardParam>
bool ConcurrentQueue<T>::pop(T& out, GuardParam&&... guard_param)
{
	/* Lock the queue */
	std::unique_lock<std::mutex> lock(queue_mutex);
	/* Queue is always locked when this is called */
	auto end_wait_condition = [this] {
		return is_nowaiting() || !events.empty();
	};
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
	if (!end_wait_condition()) {
		/* Externally supplied wait callback guard */
		WaitGuard guard(std::forward<GuardParam>(guard_param)...);
		/*
		 * Unlocks queue, re-locks it when calling end_wait_condition and upon
		 * return
		 */
		unblock.wait(lock, end_wait_condition);
	}
#pragma GCC diagnostic pop
	/* Queue is locked at this point whether or not we waited */
	if (events.empty()) {
		return false;
	}
	out = std::move(events.front());
	events.pop();
	return true;
}

template <typename T>
void ConcurrentQueue<T>::notify()
{
	unblock.notify_one();
}

template <typename T>
void ConcurrentQueue<T>::set_nowaiting(bool value)
{
	nowaiting = value;
	unblock.notify_all();
}

template <typename T>
bool ConcurrentQueue<T>::is_nowaiting() const
{
	return nowaiting;
}

template <typename T>
bool ConcurrentQueue<T>::isEmpty(bool is_locked) const
{
	if (is_locked) {
		return events.empty();
	} else {
		std::lock_guard<std::mutex> lock(queue_mutex);
		return events.empty();
	}
}

}
