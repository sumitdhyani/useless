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

typedef std::pair<std::chrono::system_clock::time_point, Task> TimedTask;

typedef std::pair<std::chrono::system_clock::time_point, Task> TimeTaskPair;
class TimedTaskWorkerThread : public TimedConsumerThread<Task>
{
public:
	TimedTaskWorkerThread(std::shared_ptr<std::vector<TimeTaskPair>> queue, stdMutex_SPtr mutex, ConditionVariable_SPtr cond) :
		TimedConsumerThread<Task>(queue, mutex, [](Task task) {task();}, cond)
	{}
protected:

	virtual void processItem(Task task)
	{
		task();
	}
};
DEFINE_PTR(TimedTaskWorkerThread)
DEFINE_UNIQUE_PTR(TimedTaskWorkerThread)
