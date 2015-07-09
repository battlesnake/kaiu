#pragma once
#include <queue>
#include <atomic>
#include <mutex>
#include <condition_variable>

namespace mark {

template <class T>
class ConcurrentQueue {
public:
	ConcurrentQueue(const ConcurrentQueue&) = delete;
	ConcurrentQueue& operator=(const ConcurrentQueue&) = delete;
	ConcurrentQueue();
	void push(const T& item);
	void push(const T&& item);
	template <class... Args> void emplace(Args&&... args);
	bool pop(T& out);
	void stop();
	bool is_stopping() const;
private:
	std::atomic<bool> stopping{false};
	std::mutex mutex;
	std::condition_variable cv;
	std::queue<T> queue;
	void notify();
};

}

#include "concurrent_queue.tpp"
