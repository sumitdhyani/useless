#pragma once
#include "WorkerThread.h"

class IThreadPool
{
public:
	virtual void push(Task task) = 0;
	virtual void kill() = 0;
	virtual ~IThreadPool() {}
};

class ThreadPool
{
	std::vector<std::shared_ptr<WorkerThread>> m_workers;
	std::shared_ptr<std::vector<Task>> m_queue;
	std::shared_ptr<std::mutex> m_mutex;
	std::shared_ptr<ConditionVariable> m_cond;
public:
	ThreadPool(int numThreads, std::shared_ptr<std::vector<Task>> queue, std::shared_ptr<std::mutex> mutex, std::shared_ptr<ConditionVariable> cond)
		:m_queue(queue),
		m_mutex(mutex),
		m_cond(cond)
	{
		for (int i = 0; i < numThreads; i++)
		{
			m_workers.push_back(std::shared_ptr<WorkerThread>(new WorkerThread(queue, mutex, cond)));
		}
	}

	virtual void push(Task task)
	{
		{
			std::unique_lock<std::mutex> lock(*m_mutex);
			m_queue->push_back(task);
		}

		m_cond->notify_one();
	}

	virtual void kill()
	{
		for (auto& worker : m_workers)
			worker->kill();
	}

	~ThreadPool()
	{
		kill();
	}
};

