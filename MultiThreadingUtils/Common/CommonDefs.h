#pragma once
#include <memory>
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

typedef unsigned int UINT;
