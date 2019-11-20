#pragma once
#include "ConsumerThread.h"
typedef std::function<void()> Task;

class WorkerThread : public FifoConsumerThread<Task>
{

public:
	WorkerThread(std::shared_ptr<std::vector<Task>> queue, std::shared_ptr<std::mutex> mutex, std::shared_ptr<ConditionVariable> cond) :
		FifoConsumerThread<Task>(queue, mutex, cond)
	{}
protected:

	virtual void processItem(Task task)
	{
		task();
	}
};


typedef std::pair<std::chrono::system_clock::time_point, Task> TimedTask;

typedef std::pair<std::chrono::system_clock::time_point, Task> TimeTaskPair;
class TimedTaskWorkerThread : public TimedConsumerThread<Task>
{
public:
	TimedTaskWorkerThread(std::shared_ptr<std::vector<TimeTaskPair>> queue, std::shared_ptr<std::mutex> mutex, std::shared_ptr<ConditionVariable> cond) :
		TimedConsumerThread<Task>(queue, mutex, cond)
	{}
protected:

	virtual void processItem(Task task)
	{
		task();
	}
};
