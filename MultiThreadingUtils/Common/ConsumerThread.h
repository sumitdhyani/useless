#pragma once
#include <functional>
#include <queue>
#include <map>
#include <mutex>
#include <atomic>
#include <chrono>
#include <thread>
#include <memory>
#include "ConditionVariable.h"

template <class T>
class IConsumerThread
{
public:
	virtual void kill() = 0;
	virtual void push(T item) = 0;
	virtual ~IConsumerThread() {}
};


//Keep the template parameter as something assignable and copyable, otherwise results may be underministic
template <class T>
class FifoConsumerThread : public IConsumerThread<T>
{
	std::shared_ptr<std::vector<T>> m_queue;
	std::shared_ptr<std::mutex> m_mutex;
	std::shared_ptr<ConditionVariable> m_cond;
	std::atomic<bool> m_terminate;
	std::thread m_thread;

	virtual void run()
	{
		while (!m_terminate)
		{
			std::vector<T> local;

			{
				std::unique_lock<std::mutex> lock(*m_mutex);
				m_queue->swap(local);
			}

			for(auto const& currentItem : local)
				processItem(currentItem);

			m_cond->wait();
		}
	}

protected:
	virtual void processItem(T item) = 0;
public:

	FifoConsumerThread(std::shared_ptr<std::vector<T>> queue, std::shared_ptr<std::mutex> mutex, std::shared_ptr<ConditionVariable> cond)
		:m_queue(queue),
		 m_mutex(mutex),
		 m_cond(cond)
	{
		m_terminate = false;
		m_thread = std::thread(&FifoConsumerThread::run, this);
	}

	virtual void push(T item)
	{
		{
			std::unique_lock<std::mutex> lock(*m_mutex);
			m_queue->push_back(item);
		}

		m_cond->notify_one();
	}

	virtual void kill()
	{
		std::unique_lock<std::mutex> lock(*m_mutex);
		if (!m_terminate)
		{
			m_terminate = true;
			lock.unlock();
			m_cond->notify_one();
			m_thread.join();
		}
	}

	~FifoConsumerThread()
	{
		kill();
	}
};



template <class T>
class TimedConsumerThread : public IConsumerThread<std::pair<std::chrono::system_clock::time_point, T>>
{
	typedef std::pair<std::chrono::system_clock::time_point, T> TimeItemPair;
	std::shared_ptr<std::vector<TimeItemPair>> m_itemQueue;
	std::shared_ptr<std::mutex> m_mutex;
	std::shared_ptr<ConditionVariable> m_cond;
	std::map<std::chrono::system_clock::time_point, std::vector<T>> m_processingQueue;
	std::atomic<bool> m_terminate;
	std::thread m_thread;

protected:
	virtual void processItem(T item) = 0;

public:

	TimedConsumerThread(std::shared_ptr<std::vector<TimeItemPair>> queue, std::shared_ptr<std::mutex> mutex, std::shared_ptr<ConditionVariable> cond)
		:m_itemQueue(queue),
		m_mutex(mutex),
		m_cond(cond)
	{
		m_terminate = false;
		m_thread = std::thread(&TimedConsumerThread::run, this);
	}

	virtual void push(TimeItemPair timeItemPair)
	{
		{
			std::unique_lock<std::mutex> lock(*m_mutex);
			m_itemQueue->push_back(timeItemPair);
		}

		m_cond->notify_one();
	}

	virtual void run()
	{
		while (!m_terminate)
		{
			{
				std::vector<TimeItemPair> local;

				{
					std::unique_lock<std::mutex> lock(*m_mutex);
					m_itemQueue->swap(local);
				}

				for (auto currentItem : local)
					m_processingQueue[currentItem.first].push_back(currentItem.second);
			}

			{
				auto it = m_processingQueue.begin();
				while (it != m_processingQueue.end() && (it->first <= std::chrono::system_clock::now()))
				{
					auto const& processingQueue = it->second;
					for (auto const& item : processingQueue)
						processItem(item);

					it++;
				}

				m_processingQueue.erase(m_processingQueue.begin(), it);
			}

			m_processingQueue.empty()? m_cond->wait() : 
									   m_cond->wait_until(m_processingQueue.begin()->first);
		}
	}

	virtual void kill()
	{
		std::unique_lock<std::mutex> lock(*m_mutex);
		if (!m_terminate)
		{
			m_terminate = true;
			lock.unlock();//Ugly but necessary
			m_cond->notify_one();
			m_thread.join();
		}
	}

	~TimedConsumerThread()
	{
		kill();
	}
};
