#define concurrent_queue_tcc
#include <stdexcept>
#include "concurrent_queue.h"

namespace kaiu {

using namespace std;

template <typename T>
ConcurrentQueue<T>::ConcurrentQueue(bool nowaiting)
	: nowaiting(nowaiting)
{
}

template <typename T>
void ConcurrentQueue<T>::push(const T& item)
{
	lock_guard<mutex> lock(queue_mutex);
	events.push(item);
	notify();
}

template <typename T>
void ConcurrentQueue<T>::push(T&& item)
{
	lock_guard<mutex> lock(queue_mutex);
	events.push(move(item));
	notify();
}

template <typename T>
template <typename... Args> void ConcurrentQueue<T>::emplace(Args&&... args)
{
	lock_guard<mutex> lock(queue_mutex);
	events.emplace(forward<Args...>(args...));
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
	unique_lock<mutex> lock(queue_mutex);
	/* Queue is always locked when this is called */
	auto end_wait_condition = [this] {
		return is_nowaiting() || !events.empty();
	};
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
	if (!end_wait_condition()) {
		/* Externally supplied wait callback guard */
		WaitGuard guard(forward<GuardParam>(guard_param)...);
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
	out = move(events.front());
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
		lock_guard<mutex> lock(queue_mutex);
		return events.empty();
	}
}

}
