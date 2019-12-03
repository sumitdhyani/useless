#pragma once
#include "ConsumerThread.h"
#include "CommonDefs.h"
typedef std::function<void()> Task;

class WorkerThread : public FifoConsumerThread<Task>
{
	friend class ThreadPool;
protected:
	virtual void processItem(Task task)
	{
		task();
	}

	WorkerThread(std::shared_ptr<std::vector<Task>> queue, stdMutexPtr mutex, ConditionVariablePtr cond) :
		FifoConsumerThread<Task>(queue, mutex, [](Task task) {task(); }, cond)
	{}

public:
	WorkerThread(std::shared_ptr<std::vector<Task>> queue, stdMutexPtr mutex) :
		WorkerThread(queue, mutex, ConditionVariablePtr(new ConditionVariable))
	{}

};
DEFINE_PTR(WorkerThread)

typedef std::pair<std::chrono::system_clock::time_point, Task> TimedTask;

typedef std::pair<std::chrono::system_clock::time_point, Task> TimeTaskPair;
class TimedTaskWorkerThread : public TimedConsumerThread<Task>
{
public:
	TimedTaskWorkerThread(std::shared_ptr<std::vector<TimeTaskPair>> queue, stdMutexPtr mutex, ConditionVariablePtr cond) :
		TimedConsumerThread<Task>(queue, mutex, [](Task task) {task();}, cond)
	{}
protected:

	virtual void processItem(Task task)
	{
		task();
	}
};
DEFINE_PTR(TimedTaskWorkerThread)
