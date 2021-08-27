#include "CoreMinimal.h"
#include "Threading.h"

#define K3DPLATFORM_OS_WINDOWS 1
#define K3DPLATFORM_OS_WIN 1
#include <Windows.h>

namespace simd {
#if K3DPLATFORM_OS_WINDOWS
static SYSTEM_INFO sSysInfo = {};
#endif

uint32 GetCpuCoreNum() {
#if K3DPLATFORM_OS_WINDOWS
  if (sSysInfo.dwNumberOfProcessors == 0) {
    ::GetSystemInfo(&sSysInfo);
  }
  return sSysInfo.dwNumberOfProcessors;
#elif K3DPLATFORM_OS_UNIX
  return sysconf(_SC_NPROCESSORS_CONF);
#endif
}

struct MutexPrivate {
#if K3DPLATFORM_OS_WINDOWS
  CRITICAL_SECTION CS;
#else
  pthread_mutex_t mMutex;
#endif
  MutexPrivate() {
#if K3DPLATFORM_OS_WINDOWS
    InitializeCriticalSection(&CS);
#else
    pthread_mutex_init(&mMutex, NULL);
#endif
  }

  ~MutexPrivate() {
#if K3DPLATFORM_OS_WINDOWS
    DeleteCriticalSection(&CS);
#else
    pthread_mutex_destroy(&mMutex);
#endif
  }

  void Lock() {
#if K3DPLATFORM_OS_WINDOWS
    EnterCriticalSection(&CS);
#else
    pthread_mutex_lock(&mMutex);
#endif
  }
  void UnLock() {
#if K3DPLATFORM_OS_WINDOWS
    LeaveCriticalSection(&CS);
#else
    pthread_mutex_unlock(&mMutex);
#endif
  }
};

Mutex::Mutex() : m_Impl(new MutexPrivate) {}

Mutex::~Mutex() {
  delete m_Impl;
  m_Impl = nullptr;
}

void Mutex::Lock() { m_Impl->Lock(); }

void Mutex::UnLock() { m_Impl->UnLock(); }

struct ConditionVariablePrivate {
#if K3DPLATFORM_OS_WINDOWS
  CONDITION_VARIABLE CV;
#else
  pthread_cond_t mCond;
#endif
  ConditionVariablePrivate() {
#if K3DPLATFORM_OS_WINDOWS
    InitializeConditionVariable(&CV);
#else
    pthread_cond_init(&mCond, NULL);
#endif
  }
  ~ConditionVariablePrivate() {
#if K3DPLATFORM_OS_WINDOWS
#else
    pthread_cond_destroy(&mCond);
#endif
  }

  void Wait(MutexPrivate *mutex, uint32 time) {
#if K3DPLATFORM_OS_WINDOWS
    ::SleepConditionVariableCS(&CV, &(mutex->CS), time);
#else
    pthread_cond_wait(&mCond, &mutex->mMutex);
#endif
  }
  void Notify() {
#if K3DPLATFORM_OS_WINDOWS
    ::WakeConditionVariable(&CV);
#else
    pthread_cond_signal(&mCond);
#endif
  }
  void NotifyAll() {
#if K3DPLATFORM_OS_WINDOWS
    ::WakeAllConditionVariable(&CV);
#else
    pthread_cond_broadcast(&mCond);
#endif
  }
};

ConditionVariable::ConditionVariable() : m_Impl(new ConditionVariablePrivate) {}

ConditionVariable::~ConditionVariable() { delete m_Impl; }

void ConditionVariable::Wait(Mutex *mutex) {
  if (mutex != nullptr)
    m_Impl->Wait(mutex->m_Impl, 0xffffffff);
}

void ConditionVariable::Wait(Mutex *mutex, uint32 milliseconds) {
  if (mutex != nullptr)
    m_Impl->Wait(mutex->m_Impl, milliseconds);
}

void ConditionVariable::Notify() { m_Impl->Notify(); }

void ConditionVariable::NotifyAll() { m_Impl->NotifyAll(); }

#define DEFAULT_THREAD_STACK_SIZE 2048

__INTERNAL_THREAD_ROUTINE_RETURN Thread::RunOnThread(void *Thr) {
  Thread *SelfThr = static_cast<Thread *>(Thr);
  // Set ThreadName
  SetCurrentThreadName(SelfThr->GetName());
  // Run
  SelfThr->m_ThreadFunc(SelfThr->m_ThreadClosure);
  SelfThr->m_ThreadStatus = ThreadStatus::Finish;
  return 0;
}

Thread::Thread(std::string const &name, ThreadPriority priority, int32 CpuId)
    : m_ThreadName(name), m_ThreadPriority(priority), m_CoreId(CpuId),
      m_StackSize(DEFAULT_THREAD_STACK_SIZE),
      m_ThreadStatus(ThreadStatus::Ready), m_ThreadHandle(nullptr) {}

void Thread::SleepForMilliSeconds(uint32_t millisecond) { Sleep(millisecond); }

uint32_t Thread::GetId() {
#if K3DPLATFORM_OS_WINDOWS
  return ::GetCurrentThreadId();
#else
  return (uint64_t)pthread_self();
#endif
}

Thread::Thread() : Thread("", ThreadPriority::Normal) {}

Thread::~Thread() {
  if (m_ThreadHandle) {
#if defined(K3DPLATFORM_OS_WIN)
    // uint32 Tid = ::GetThreadId(m_ThreadHandle);
    // if (s_ThreadMap.find(Tid) != s_ThreadMap.end())
    //{
    //  s_ThreadMap.erase(Tid);
    //}
#endif
    if (m_ThreadClosure) {
      // already deleted by self
      m_ThreadClosure = nullptr;
    }
  }
}

void Thread::SetPriority(ThreadPriority prio) { m_ThreadPriority = prio; }

#if defined(K3DPLATFORM_OS_WIN) || defined(_WIN32)
const DWORD MS_VC_EXCEPTION = 0x406D1388;
#pragma pack(push, 8)
typedef struct tagTHREADNAME_INFO {
  DWORD dwType;     // Must be 0x1000.
  LPCSTR szName;    // Pointer to name (in user addr space).
  DWORD dwThreadID; // Thread ID (-1=caller thread).
  DWORD dwFlags;    // Reserved for future use, must be zero.
} THREADNAME_INFO;
#pragma pack(pop)
void SetThreadName(DWORD dwThreadID, const char *threadName) {
  THREADNAME_INFO info;
  info.dwType = 0x1000;
  info.szName = threadName;
  info.dwThreadID = dwThreadID;
  info.dwFlags = 0;
#pragma warning(push)
#pragma warning(disable : 6320 6322)
  __try {
    RaiseException(MS_VC_EXCEPTION, 0, sizeof(info) / sizeof(ULONG_PTR),
                   (ULONG_PTR *)&info);
  } __except (EXCEPTION_EXECUTE_HANDLER) {
  }
#pragma warning(pop)
}

#endif

void Thread::InternalStart(ThrRoutine Routine,
                           __internal::ThreadClosure *Closure) {
  m_ThreadFunc = Routine;
  m_ThreadClosure = Closure;
#if K3DPLATFORM_OS_WINDOWS
  if (nullptr == m_ThreadHandle) {
    DWORD threadId;
    m_ThreadHandle = ::CreateThread(
        nullptr, m_StackSize,
        reinterpret_cast<LPTHREAD_START_ROUTINE>(Thread::RunOnThread),
        reinterpret_cast<LPVOID>(this), 0, &threadId);
    if (m_CoreId >= 0) {
      ::SetThreadAffinityMask(m_ThreadHandle, (DWORD_PTR)(1) << m_CoreId);
    }
    {
      Mutex::AutoLock lock;
      DWORD tid = ::GetThreadId(m_ThreadHandle);
      // m_ThreadName.AppendSprintf(" #%d", tid);
      SetThreadName(threadId, m_ThreadName.c_str());
      // s_ThreadMap[tid] = this;
    }
  }
#else
  if (0 == (u_long)m_ThreadHandle) {
    typedef void *(threadfun)(void *);
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_create((pthread_t *)&m_ThreadHandle, nullptr, Thread::RunOnThread,
                   this);
#if K3DPLATFORM_OS_ANDROID
    pthread_setname_np((pthread_t)m_ThreadHandle, m_ThreadName.CStr());
#endif
    if (m_CoreId >= 0) {
#if K3DPLATFORM_OS_MAC || K3DPLATFORM_OS_IOS
      thread_affinity_policy ap;
      ap.affinity_tag = 1 << m_CoreId;
      thread_policy_set(pthread_mach_thread_np((pthread_t)m_ThreadHandle),
                        THREAD_AFFINITY_POLICY, (integer_t *)&ap,
                        THREAD_AFFINITY_POLICY_COUNT);
#else
      cpu_set_t mask;
      CPU_ZERO(&mask);
      CPU_SET(m_CoreId, &mask);
      // pthread_setaffinity_np((pthread_t)m_ThreadHandle, sizeof(mask), &mask);
#endif
    }
  }
#endif
}

void Thread::Start() {}

void Thread::Join() {
  if (m_ThreadHandle != nullptr) {
#if K3DPLATFORM_OS_WINDOWS
    ::WaitForSingleObject(m_ThreadHandle, INFINITE);
#else
    void *ret;
    pthread_join((pthread_t)m_ThreadHandle, &ret);
#endif
  }
}

void Thread::Terminate() {
  if (m_ThreadHandle != nullptr) {
#if K3DPLATFORM_OS_WIN
    ::TerminateThread(m_ThreadHandle, 0);
#endif
  }
}

ThreadStatus Thread::GetThreadStatus() { return m_ThreadStatus; }

std::string Thread::GetName() { return m_ThreadName; }

std::string Thread::GetCurrentThreadName() {
#if K3DPLATFORM_OS_WINDOWS
  //  uint32 tid = (uint32)::GetCurrentThreadId();
  return "Main";
#elif K3DPLATFORM_OS_ANDROID
  char name[32];
  prctl(PR_GET_NAME, (unsigned long)name);
  return name;
#else
  pthread_t tid = pthread_self();
  return "Anonymous Thread";
#endif
}

void Thread::SetCurrentThreadName(std::string const &name) {
#if K3DPLATFORM_OS_APPLE
  pthread_setname_np(name.CStr());
#endif
}

class EventPrivate {
public:
  EventPrivate(bool bManualRest) {
    event_ = ::CreateEventA(NULL, bManualRest, FALSE, NULL);
  }
  ~EventPrivate() { ::CloseHandle(event_); }

  void trigger() { ::SetEvent(event_); }
  void reset() { ::ResetEvent(event_); }
  void wait() {
    ::WaitForSingleObject(event_, // event handle
                          INFINITE);
  }

private:
  HANDLE event_;
};

Event::Event(bool manualReset) : impl_(nullptr) {
  impl_ = new EventPrivate(manualReset);
}
Event::~Event() { delete impl_; }
bool Event::isManualReset() const { return false; }
void Event::trigger() { impl_->trigger(); }
void Event::reset() { impl_->reset(); }
bool Event::wait(uint32 WaitTime) {
  impl_->wait();
  return false;
}
} // namespace simd