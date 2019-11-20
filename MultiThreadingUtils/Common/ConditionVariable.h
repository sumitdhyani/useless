#pragma once
#include <mutex>
#include <condition_variable>
#include <functional>

class ConditionVariable
{
	std::condition_variable m_cond;
	std::mutex m_mutex;
	bool m_signalled;

public:
	ConditionVariable()
	{
		m_signalled = false;
	}


	~ConditionVariable()
	{
	}

	//Whenever in Critical section and have the instance of unique lock, use the versions of wait() with 'lock'
	//as waiting in a critical section without releasing the lock will be the last thing we want to do

	void wait()
	{
		std::unique_lock<std::mutex> lock(m_mutex);
		m_cond.wait(lock, std::bind(&ConditionVariable::signalled, this));
		m_signalled = false;
	}

	void wait(std::unique_lock<std::mutex>& applicationLock)
	{
		applicationLock.unlock();
		wait();
		applicationLock.lock();
	}

	void wait_until(std::chrono::system_clock::time_point time)
	{
		std::unique_lock<std::mutex> lock(m_mutex);
		m_cond.wait_until(lock, time, std::bind(&ConditionVariable::signalled, this));
		m_signalled = false;
	}

	void wait_until(std::chrono::system_clock::time_point time, std::unique_lock<std::mutex>& applicationLock)
	{
		applicationLock.unlock();
		wait_until(time);
		applicationLock.lock();
	}


	void wait_for(std::chrono::system_clock::duration duration)
	{
		std::unique_lock<std::mutex> lock(m_mutex);
		m_cond.wait_for(lock, duration, std::bind(&ConditionVariable::signalled, this));
		m_signalled = false;
	}

	void wait_for(std::chrono::system_clock::duration duration, std::unique_lock<std::mutex>& applicationLock)
	{
		applicationLock.unlock();
		wait_for(duration);
		applicationLock.lock();
	}

	void notify_one()
	{
		{
			std::unique_lock<std::mutex> lock(m_mutex);
			m_signalled = true;
		}
		m_cond.notify_one();
	}

	void notify_all()
	{
		{
			std::unique_lock<std::mutex> lock(m_mutex);
			m_signalled = true;
		}
		m_cond.notify_all();
	}

	bool signalled()
	{
		return m_signalled;
	}
};
