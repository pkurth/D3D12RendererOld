#pragma once

#include <queue>
#include <mutex>

template <typename T>
class thread_safe_queue
{
public:
	thread_safe_queue() {}
	thread_safe_queue(const thread_safe_queue& other);

	void pushBack(T v);
	bool tryPop(T& v);
	bool empty() const;
	size_t size() const;

private:
	std::queue<T> queue;
	mutable std::mutex mutex;
};

template<typename T>
thread_safe_queue<T>::thread_safe_queue(const thread_safe_queue<T>& other)
{
	std::lock_guard<std::mutex> lock(other.mutex);
	queue = other.queue;
}

template<typename T>
void thread_safe_queue<T>::pushBack(T value)
{
	std::lock_guard<std::mutex> lock(mutex);
	queue.push(std::move(value));
}

template<typename T>
bool thread_safe_queue<T>::tryPop(T& v)
{
	std::lock_guard<std::mutex> lock(mutex);
	if (queue.empty())
	{
		return false;
	}

	v = queue.front();
	queue.pop();

	return true;
}

template<typename T>
bool thread_safe_queue<T>::empty() const
{
	std::lock_guard<std::mutex> lock(mutex);
	return queue.empty();
}

template<typename T>
size_t thread_safe_queue<T>::size() const
{
	std::lock_guard<std::mutex> lock(mutex);
	return queue.size();
}
