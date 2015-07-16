#define concurrent_queue_tcc
#include "concurrent_queue.h"

namespace mark {

using namespace std;

template <typename T>
ConcurrentQueue<T>::ConcurrentQueue() 
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
	auto end_wait_condition = [this] {
		return is_nonblocking() || !events.empty();
	};
	unique_lock<mutex> lock(queue_mutex);
	if (!end_wait_condition()) {
		WaitGuard guard(forward<GuardParam>(guard_param)...);
		stop_waiting.wait(lock, end_wait_condition);
	}
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
	stop_waiting.notify_one();
}

template <typename T>
void ConcurrentQueue<T>::set_nonblocking(bool nonblocking)
{
	this->nonblocking = nonblocking;
	stop_waiting.notify_all();
}

template <typename T>
bool ConcurrentQueue<T>::is_nonblocking() const
{
	return nonblocking;
}

}
