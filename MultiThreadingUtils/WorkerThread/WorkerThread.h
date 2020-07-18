#pragma once
#include "ConsumerThread.h"
#include "CommonDefs.h"
typedef std::function<void()> Task;

class WorkerThread
{
	friend class ThreadPool;
	std::shared_ptr<FifoConsumerThread<Task>> m_consumer;
protected:
	

	WorkerThread(std::shared_ptr<std::vector<Task>> queue, stdMutex_SPtr mutex, ConditionVariable_SPtr cond)
	{
		m_consumer = std::shared_ptr<FifoConsumerThread<Task>>(new FifoConsumerThread<Task>(queue, mutex, [](Task task) {task();}, cond));
	}

public:
	WorkerThread(std::shared_ptr<std::vector<Task>> queue, stdMutex_SPtr mutex) :
		WorkerThread(queue, mutex, ConditionVariable_SPtr(new ConditionVariable))
	{}

	void push(Task task)
	{
		m_consumer->push(task);
	}

	void kill()
	{
		m_consumer->kill();
	}
};
DEFINE_PTR(WorkerThread)


class ThrottledWorkerThread
{
	typedef ThrottledConsumerThread<Task> ThrottledConsumerThread;
	DEFINE_PTR(ThrottledConsumerThread)

	ThrottledConsumerThread_SPtr m_consumer;
	
public:
	ThrottledWorkerThread(std::shared_ptr<std::vector<Task>> queue, duration unitTime, size_t numTransactions)
	{
		m_consumer = ThrottledConsumerThread_SPtr(new ThrottledConsumerThread(queue, [](Task task) {task(); }, unitTime, numTransactions));
	}


	void push(Task task)
	{
		m_consumer->push(task);
	}

	void kill()
	{
		m_consumer->kill();
	}
};

class Scheduler
{
	typedef TimedConsumerThread<Task> TimedConsumerThread;
	DEFINE_PTR(TimedConsumerThread)

	TimedConsumerThread_SPtr m_scheduler;

public:
	Scheduler(std::shared_ptr<std::vector<TimeAndItemAndCallback<Task>>> queue, stdMutex_SPtr mutex, ConditionVariable_SPtr cond)
	{
		m_scheduler = std::make_unique<TimedConsumerThread>(queue, mutex, [](Task task) {task(); }, cond);
	}

	void schedule(time_point scheduleTime, Task task, std::function<void(size_t)> callback)
	{
		m_scheduler->push(scheduleTime, task, callback);
	}

	void cancelTask(size_t taskId, std::function<void(bool)> callback)
	{
		m_scheduler->cancelItem(taskId, callback);
	}
};
DEFINE_PTR(Scheduler)
DEFINE_UNIQUE_PTR(Scheduler)
