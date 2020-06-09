#pragma once
#include "CommonDefs.h"
#include <vector>

template <class T>
class RingBuffer
{
	std::vector<T> m_queue;
	size_t m_capacity;
	size_t m_start;
	size_t m_end;

	bool m_firstElementPushed;
public:
	RingBuffer(size_t capacity):
		m_capacity(capacity),
		m_queue(capacity, T())
	{
		m_start = 1;
		m_end = 0;
		m_firstElementPushed = false;
	}

	void push(T item)
	{
		m_firstElementPushed = true;
		m_end = (m_end + 1) % m_capacity;
		m_queue[m_end] = item;
		if (m_end == m_start)
			m_start = (m_start + 1) % m_capacity;
	}

	T pop()
	{
		if (empty())
			throw std::runtime_error("Trying to pop empty queue");

		T retVal = m_queue[m_start];
		m_start = (m_start + 1) % m_capacity;
		return retVal;
	}
	
	size_t size()
	{
		if (!m_firstElementPushed)
			return 0;
		else
			return (m_end >= m_start) ? 
				   (m_end - m_start + 1) : 
				   (m_capacity - (m_start - m_end - 1));
	}

	size_t empty()
	{
		return (size() == 0);
	}

	T front()
	{
		if (empty())
			throw std::runtime_error("Trying to access element when buffer is empty");

		return m_queue[m_start];
	}

	T back()
	{
		if (empty())
			throw std::runtime_error("Trying to access element when buffer is empty");

		return m_queue[m_end];
	}
};
