#pragma once

#include <vector>
#include <mutex>

template <typename T>
class thread_safe_vector
{
public:
	thread_safe_vector() {}
	thread_safe_vector(const thread_safe_vector& other);

	void pushBack(T v);
	bool tryGetBack(T& v);
	bool empty() const;
	size_t size() const;

private:
	std::vector<T> vector;
	mutable std::mutex mutex;
};

template<typename T>
thread_safe_vector<T>::thread_safe_vector(const thread_safe_vector<T>& other)
{
	std::lock_guard<std::mutex> lock(other.mutex);
	vector = other.vector;
}

template<typename T>
void thread_safe_vector<T>::pushBack(T value)
{
	std::lock_guard<std::mutex> lock(mutex);
	vector.push_back(value);
}

template<typename T>
bool thread_safe_vector<T>::tryGetBack(T& v)
{
	std::lock_guard<std::mutex> lock(mutex);
	if (vector.empty())
	{
		return false;
	}

	v = vector.back();
	vector.pop_back();

	return true;
}

template<typename T>
bool thread_safe_vector<T>::empty() const
{
	std::lock_guard<std::mutex> lock(mutex);
	return vector.empty();
}

template<typename T>
size_t thread_safe_vector<T>::size() const
{
	std::lock_guard<std::mutex> lock(mutex);
	return vector.size();
}

