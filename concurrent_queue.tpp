#include "concurrent_queue.h"

namespace mark {

template <class T>
ConcurrentQueue<T>::ConcurrentQueue() 
{
	stopping = false;
}

template <class T>
void ConcurrentQueue<T>::push(const T& item)
{
	std::unique_lock<std::mutex> lock(mutex);
	queue.push(item);
	lock.unlock();
	notify();
}

template <class T>
void ConcurrentQueue<T>::push(const T&& item)
{
	std::unique_lock<std::mutex> lock(mutex);
	queue.push(std::move(item));
		lock.unlock();
		notify();
	}

	template <class T>
	template <class... Args> void ConcurrentQueue<T>::emplace(Args&&... args)
	{
		std::unique_lock<std::mutex> lock(mutex);
		queue.emplace(std::forward<Args...>(args...));
		lock.unlock();
		notify();
	}

	template <class T>
	bool ConcurrentQueue<T>::pop(T& out)
	{
		std::unique_lock<std::mutex> lock(mutex);
		cv.wait(lock, [this] { return stopping || !queue.empty(); });
		if (queue.empty()) {
			return false;
		}
		out = std::move(queue.front());
		queue.pop();
		return true;
	}

	template <class T>
	void ConcurrentQueue<T>::notify()
	{
		cv.notify_one();
	}

	template <class T>
	void ConcurrentQueue<T>::stop()
	{
		stopping = true;
	cv.notify_all();
}

template <class T>
bool ConcurrentQueue<T>::is_stopping() const
{
	return stopping;
}

}
