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
protected:
	typedef std::vector<T> ConsumerQueue;
	DEFINE_PTR(ConsumerQueue)

	//Visible to only subclasses, as owning this condition variable by the client code can be dangerous
	//in case the application programmer tries to do something fancy but is not aware of the dangerous edge cases
	//The most likely side effects can then be an infinitely waiting thread or unexpected spurious wakeups
	FifoConsumerThread(ConsumerQueuePtr queue, stdMutexPtr mutex, std::function<void(T)> predicate, ConditionVariablePtr cond )
		:m_queue(queue),
		m_mutex(mutex),
		m_predicate(predicate),
		m_cond(cond)
	{
		m_terminate = false;
		m_thread = stdThread(&FifoConsumerThread::run, this);
	}
private:
	ConsumerQueuePtr m_queue;
	stdMutexPtr m_mutex;
	ConditionVariablePtr m_cond;
	std::atomic<bool> m_terminate;
	stdThread m_thread;
	std::function<void(T)> m_predicate;

	virtual void run()
	{
		while (!m_terminate)
		{
			ConsumerQueue local;

			{
				stdUniqueLock lock(*m_mutex);
				m_queue->swap(local);
			}

			for(auto const& currentItem : local)
				m_predicate(currentItem);

			m_cond->wait();
		}
	}


public:

	FifoConsumerThread(ConsumerQueuePtr queue, stdMutexPtr mutex, std::function<void(T)> predicate)
		:FifoConsumerThread(queue, mutex, predicate, ConditionVariablePtr(new ConditionVariable))
	{
	}


	virtual void push(T item)
	{
		{
			stdUniqueLock lock(*m_mutex);
			m_queue->push_back(item);
		}

		m_cond->notify_one();
	}

	virtual void kill()
	{
		stdUniqueLock lock(*m_mutex);
		if (!m_terminate)
		{
			m_terminate = true;
			lock.unlock();
			//To avoid a spurious wakeup during kill process, to ensure we are able to wakeup the
			//Thread owned by this object, we need to notify_all on this condition variable as this condition variable
			//is shared by all the threads in case they belong to a threadpool
			//Downside is that all threads will be woken up and contention
			//is likely to happen between rest of the peer threads in the pool
			//In case this is a standalone thread, i.e. a thread which is not a part of the threadpool, no performance
			//impact will be there
			//This will certainly involve spurious wakeups but the thing is we know when these spuirious wakeups will occur and the 
			//impact and are ok with it
			m_cond->notify_all();
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
protected:
	typedef std::pair<std::chrono::system_clock::time_point, T> TimeItemPair;
	typedef std::vector<TimeItemPair> ConsumerQueue;
	DEFINE_PTR(ConsumerQueue)

	TimedConsumerThread(ConsumerQueuePtr queue, stdMutexPtr mutex, std::function<void(T)> predicate, ConditionVariablePtr cond)
		:m_itemQueue(queue),
		m_mutex(mutex),
		m_predicate(predicate),
		m_cond(cond)
	{
		m_terminate = false;
		m_thread = stdThread(&TimedConsumerThread::run, this);
	}

private:
	ConsumerQueuePtr m_itemQueue;
	stdMutexPtr m_mutex;
	ConditionVariablePtr m_cond;
	std::map<std::chrono::system_clock::time_point, std::vector<T>> m_processingQueue;
	std::atomic<bool> m_terminate;
	stdThread m_thread;
	std::function<void(T)> m_predicate;

public:

	TimedConsumerThread(ConsumerQueuePtr queue, stdMutexPtr mutex, std::function<void(T)> predicate)
		:TimedConsumerThread(queue, mutex, predicate, ConditionVariablePtr(new ConditionVariable))
	{
	}

	virtual void push(TimeItemPair timeItemPair)
	{
		{
			stdUniqueLock lock(*m_mutex);
			m_itemQueue->push_back(timeItemPair);
		}

		m_cond->notify_one();
	}

	virtual void run()
	{
		while (!m_terminate)
		{
			{
				ConsumerQueue local;

				{
					stdUniqueLock lock(*m_mutex);
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
						m_predicate(item);

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
		stdUniqueLock lock(*m_mutex);
		if (!m_terminate)
		{
			m_terminate = true;
			lock.unlock();//Ugly but necessary
			m_cond->notify_all();
			m_thread.join();
		}
	}

	~TimedConsumerThread()
	{
		kill();
	}
};
