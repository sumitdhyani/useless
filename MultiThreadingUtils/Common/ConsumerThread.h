#pragma once
#include <functional>
#include <queue>
#include <map>
#include <mutex>
#include <atomic>
#include <chrono>
#include <thread>
#include <memory>
#include <queue>
#include <unordered_map>
#include "ConditionVariable.h"
#include "RingBuffer.h""

template <class T>
class IConsumerThread
{
public:
	virtual void kill() = 0;
	virtual void push(T item) = 0;
	virtual ~IConsumerThread() {}
};


template <class T>
class ISuspendableConsumerThread : public IConsumerThread<T>
{
public:
	virtual void pause() = 0;
	virtual void resume() = 0;
	virtual ~ISuspendableConsumerThread() {}
};

class WorkerThread;
//Keep the template parameter as something assignable and copyable, otherwise results may be underministic
template <class T>
class FifoConsumerThread : public ISuspendableConsumerThread<T>
{
	friend class WorkerThread;
protected:
	typedef std::vector<T> ConsumerQueue;
	DEFINE_PTR(ConsumerQueue)

	//Visible to only subclasses, as owning this condition variable by the client code can be dangerous
	//in case the application programmer tries to do something fancy but is not aware of the dangerous edge cases
	//The most likely side effects can then be an infinitely waiting thread or unexpected spurious wakeups
	FifoConsumerThread(ConsumerQueue_SPtr queue, stdMutex_SPtr mutex, std::function<void(T)> predicate, ConditionVariable_SPtr cond )
		:m_queue(queue),
		m_mutex(mutex),
		m_predicate(predicate),
		m_cond(cond)
	{
		m_terminate = false;
		m_consumerBusy = false;
		m_paused = false;
		m_thread = stdThread(&FifoConsumerThread::run, this);
	}
private:
	ConsumerQueue_SPtr m_queue;
	stdMutex_SPtr m_mutex;
	ConditionVariable_SPtr m_cond;
	std::atomic<bool> m_terminate;
	bool m_consumerBusy;//Used to avoid unnecessary signalling of consumer if it is busy processing the queue, purely performance
	bool m_paused;//signifies whether is the thread is paused
	stdThread m_thread;
	std::function<void(T)> m_predicate;

	virtual void run()
	{
		while (!m_terminate)
		{
			ConsumerQueue local;

			{
				stdUniqueLock lock(*m_mutex);

				if (m_paused)
					m_cond->wait(lock);

				if (m_queue->empty())
				{
					m_consumerBusy = false;
					m_cond->wait(lock);
				}

				m_queue->swap(local);
				m_consumerBusy = true;
			}

			for(auto const& currentItem : local)
				m_predicate(currentItem);
		}
	}


public:

	FifoConsumerThread(ConsumerQueue_SPtr queue, stdMutex_SPtr mutex, std::function<void(T)> predicate)
		:FifoConsumerThread(queue, mutex, predicate, ConditionVariable_SPtr(new ConditionVariable))
	{
	}


	virtual void push(T item)
	{
		{
			stdUniqueLock lock(*m_mutex);
			m_queue->push_back(item);

			if (!(m_paused || m_consumerBusy))
			{
				lock.unlock();
				m_cond->notify_one();
			}
		}

	}

	virtual void pause()
	{
		stdUniqueLock lock(*m_mutex);

		if (m_paused)
		{
			lock.unlock();
			throw std::runtime_error("Thread already paused");
		}
		else
			m_paused = true;
	}


	
	virtual void resume()
	{
		stdUniqueLock lock(*m_mutex);

		if (!m_paused)
		{
			lock.unlock();
			throw std::runtime_error("Thread already running");
		}
		else
		{
			m_paused = false;
			lock.unlock();
			m_cond->notify_one();
		}
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

template<class T>
using TimeAndItemAndCallback = std::tuple<time_point, T, std::function<void(bool)>>;

template <class T>
class TimedConsumerThread : public IConsumerThread<TimeAndItemAndCallback<T>>
{
protected:
	typedef TimeAndItemAndCallback<T> TimeAndItemAndCallback;
	typedef std::vector<TimeAndItemAndCallback> ConsumerQueue;
	typedef std::unordered_map<size_t, T> ItemList;
	typedef std::pair<time_point, typename ItemList::iterator> ProcessingQueueItem;

	DEFINE_PTR(ConsumerQueue)

private:
	ConsumerQueue_SPtr m_itemQueue;
	stdMutex_SPtr m_mutex;
	ConditionVariable_SPtr m_cond;
	ItemList m_itemsById;

	struct customComp
	{
		bool operator()(ProcessingQueueItem t1, ProcessingQueueItem t2)
		{
			return std::greater<time_point>()(t1.first, t2.first);
		}
	};
	std::priority_queue < ProcessingQueueItem, std::vector<ProcessingQueueItem>, customComp > m_processingQueue;
	std::atomic<bool> m_terminate;
	stdThread m_thread;
	std::function<void(T)> m_predicate;

public:

	TimedConsumerThread(ConsumerQueue_SPtr queue, stdMutex_SPtr mutex, std::function<void(T)> predicate, ConditionVariable_SPtr cond)
		:m_itemQueue(queue),
		m_mutex(mutex),
		m_predicate(predicate),
		m_cond(cond)
	{
		m_terminate = false;
		m_thread = stdThread(&TimedConsumerThread::run, this);
	}


	TimedConsumerThread(ConsumerQueue_SPtr queue, stdMutex_SPtr mutex, std::function<void(T)> predicate)
		:TimedConsumerThread(queue, mutex, predicate, ConditionVariable_SPtr(new ConditionVariable))
	{
	}

	virtual void push(TimeAndItemAndCallback timeAndItemAndCallback)
	{
		{
			stdUniqueLock lock(*m_mutex);
			m_itemQueue->push_back(timeAndItemAndCallback);
		}

		m_cond->notify_one();
	}

	void push(time_point scheduleTime, T item, std::function<void(size_t)> callback)
	{
		push(TimeAndItemAndCallback(scheduleTime, item, callback));
	}

	virtual void run()
	{
		size_t itemID = 0;
		while (!m_terminate)
		{
			{
				ConsumerQueue local;

				{
					stdUniqueLock lock(*m_mutex);
					m_itemQueue->swap(local);
				}

				for (auto currentItem : local)
				{
					auto it = m_itemsById.insert({ itemID, std::get<1>(currentItem) });

					std::get<2>(currentItem)(itemID);
					m_processingQueue.push(ProcessingQueueItem(std::get<0>(currentItem), it.first));
					itemID++;
				}
			}

			//here we pick only the top element because we want to ensure first scheduled - first executed policy
			if (!m_processingQueue.empty() && (m_processingQueue.top().first <= now()))
			{
				m_predicate(m_processingQueue.top().second->second);
				m_itemsById.erase(m_processingQueue.top().second);
				m_processingQueue.pop();
			}

			if (!m_processingQueue.empty())
				m_cond->wait_until(m_processingQueue.top().first);
			else
				m_cond->wait();
		}
	}

	void cancelItem(size_t itemId, std::function<void(bool)> callback)
	{
		auto unschedulePendingItem = [this, callback, itemId]()
		{
			auto it = m_itemsById.find(itemId);
			bool retVal = true;
			if (it != m_itemsById.end())
				it->second = []() {};
			else
				retVal = false;

			callback(retVal);
		};

		push(now(), unschedulePendingItem, [](size_t) {});
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


template <class T>
class ThrottledConsumerThread : public IConsumerThread<T>
{
protected:
	typedef std::vector<T> ConsumerQueue;
	DEFINE_PTR(ConsumerQueue)


private:
	ConsumerQueue_SPtr m_queue;
	stdMutex m_mutex;
	ConditionVariable m_cond;
	std::atomic<bool> m_terminate;
	bool m_consumerBusy;//Used to avoid unnecessary signalling of consumer if it is busy processing the queue, purely performance
	stdThread m_thread;
	std::function<void(T)> m_predicate;
	duration m_unitTime;
	size_t m_numTransactions;
	RingBuffer<time_point> m_transactionLog;

	virtual void run()
	{
		while (!m_terminate)
		{
			ConsumerQueue local;

			{
				stdUniqueLock lock(m_mutex);

				if (m_queue->empty())
				{
					m_consumerBusy = false;
					m_cond.wait(lock);
				}

				m_queue->swap(local);
				m_consumerBusy = true;
			}

			for (auto const& currentItem : local)
			{
				if ((m_transactionLog.size() == m_numTransactions) &&
					((now() - m_transactionLog.front()) < m_unitTime)
				   )
					m_cond.wait_until(m_transactionLog.front() + m_unitTime);

				m_transactionLog.push(now());
				m_predicate(currentItem);
			}
		}
	}


public:

	ThrottledConsumerThread(ConsumerQueue_SPtr queue, std::function<void(T)> predicate, duration unitTime, size_t numTransactions)
		:m_queue(queue),
		m_predicate(predicate),
		m_unitTime(unitTime),
		m_numTransactions(numTransactions),
		m_transactionLog(numTransactions)
	{
		m_terminate = false;
		m_consumerBusy = false;
		m_thread = stdThread(&ThrottledConsumerThread::run, this);
	}

	virtual void push(T item)
	{
		{
			stdUniqueLock lock(m_mutex);
			m_queue->push_back(item);

			if (!m_consumerBusy)
			{
				lock.unlock();
				m_cond.notify_one();
			}
		}
	}

	virtual void kill()
	{
		stdUniqueLock lock(m_mutex);
		if (!m_terminate)
		{
			m_terminate = true;
			lock.unlock();
			m_cond.notify_one();
			m_thread.join();
		}
	}

	~ThrottledConsumerThread()
	{
		kill();
	}
};
