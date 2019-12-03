#pragma once
#include <memory>
#define DEFINE_PTR(Type) typedef std::shared_ptr<Type> Type##Ptr;
typedef std::mutex stdMutex;
typedef std::condition_variable stdConnditionVariable;
typedef std::unique_lock<std::mutex> stdUniqueLock;
typedef std::thread stdThread;

DEFINE_PTR(stdMutex)
DEFINE_PTR(stdConnditionVariable)

typedef unsigned int UINT;
