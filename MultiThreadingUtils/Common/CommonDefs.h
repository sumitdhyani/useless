#pragma once
#include <memory>
#include <chrono>
#include <thread>
#include <mutex>
#include <condition_variable>
#define DEFINE_PTR(Type) typedef std::shared_ptr<Type> Type##_SPtr;
#define DEFINE_UNIQUE_PTR(Type) typedef std::unique_ptr<Type> Type##_UPtr;
typedef std::mutex stdMutex;
typedef std::condition_variable stdConnditionVariable;
typedef std::unique_lock<std::mutex> stdUniqueLock;
typedef std::thread stdThread;
typedef std::chrono::system_clock::time_point time_point;
typedef std::chrono::system_clock::duration duration;

DEFINE_PTR(stdMutex)
DEFINE_PTR(stdConnditionVariable)

time_point now();

typedef unsigned int UINT;
