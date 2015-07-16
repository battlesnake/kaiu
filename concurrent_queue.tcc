#define concurrent_queue_tcc
#include <stdexcept>
#include "concurrent_queue.h"

namespace mark {

using namespace std;

template <typename T>
ConcurrentQueue<T>::ConcurrentQueue(mutex& queue_mutex, bool nowaiting) 
	: queue_mutex(queue_mutex), nowaiting(nowaiting)
{
}

template <typename T>
void ConcurrentQueue<T>::push(const T& item)
{
	unique_lock<mutex> lock(queue_mutex);
	events.push(item);
	lock.unlock();
	notify();
}

template <typename T>
void ConcurrentQueue<T>::push(const T&& item)
{
	unique_lock<mutex> lock(queue_mutex);
	events.push(move(item));
	lock.unlock();
	notify();
}

template <typename T>
template <typename... Args> void ConcurrentQueue<T>::emplace(Args&&... args)
{
	unique_lock<mutex> lock(queue_mutex);
	events.emplace(forward<Args...>(args...));
	lock.unlock();
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
	if (!end_wait_condition()) {
		/* Externally supplied wait callback guard */
		WaitGuard guard(forward<GuardParam>(guard_param)...);
		/*
		 * Unlocks queue, re-locks it when calling end_wait_condition and upon
		 * return
		 */
		unblock.wait(lock, end_wait_condition);
	}
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
bool ConcurrentQueue<T>::isEmpty()
{
	lock_guard<mutex> lock(queue_mutex);
	return events.empty();
}

template <typename T>
bool ConcurrentQueue<T>::isEmpty(const unique_lock<mutex>& lock) const
{
	if (!lock.owns_lock()) {
		throw logic_error("Mutex must be owned before calling isEmpty");
	}
	if (lock.mutex() != &queue_mutex) {
		throw logic_error("Wrong mutex in lock");
	}
	return events.empty();
}

}
