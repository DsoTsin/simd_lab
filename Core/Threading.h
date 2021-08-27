#pragma once
#include <string>
namespace simd {
uint32 GetCpuCoreNum();

class EventPrivate;
class Event {
public:
  Event(bool manualReset);
  ~Event();
  bool isManualReset() const;
  void trigger();
  void reset();
  bool wait(uint32 WaitTime = 0);

private:
  EventPrivate *impl_;
};

struct MutexPrivate;
class Mutex {
public:
  Mutex();
  ~Mutex();

  void Lock();
  void UnLock();
  friend class ConditionVariable;

  struct AutoLock {
    AutoLock() {
      m_Mutex = new Mutex;
      m_Mutex->Lock();
    }

    explicit AutoLock(Mutex *mutex, bool lostOwnerShip = false)
        : m_OnwerShipGot(lostOwnerShip), m_Mutex(mutex) {}

    ~AutoLock() {
      m_Mutex->UnLock();
      if (m_OnwerShipGot) {
        delete m_Mutex;
        m_Mutex = nullptr;
      }
    }

  private:
    bool m_OnwerShipGot;
    Mutex *m_Mutex;
  };

private:
  MutexPrivate *m_Impl;
};

struct ConditionVariablePrivate;
class ConditionVariable {
public:
  ConditionVariable();
  ~ConditionVariable();

  void Wait(Mutex *mutex);
  void Wait(Mutex *mutex, uint32 milliseconds);
  void Notify();
  void NotifyAll();

  ConditionVariable(const ConditionVariable &) = delete;
  ConditionVariable(const ConditionVariable &&) = delete;

protected:
  ConditionVariablePrivate *m_Impl;
};

enum class ThreadPriority { Low, Normal, High, RealTime };
enum class ThreadStatus { Ready, Running, Finish };
#define __INTERNAL_THREAD_ROUTINE_RETURN void *
namespace __internal {
struct ThreadClosure {
  void *operator new(size_t Size) { return ::malloc(Size); }
  void operator delete(void *Ptr) { ::free(Ptr); }
};

template <class F> struct ThreadClosure0 : ThreadClosure {
  F Function;
  static __INTERNAL_THREAD_ROUTINE_RETURN StartRoutine(void *C) {
    ThreadClosure0 *Self = static_cast<ThreadClosure0 *>(C);
    Self->Function();
    delete Self;
    return 0;
  }

  ThreadClosure0(const F &f) : Function(f) {}
};

template <class F, class X> struct ThreadClosure1 : ThreadClosure {
  F Function;
  X Arg1;

  static __INTERNAL_THREAD_ROUTINE_RETURN StartRoutine(void *C) {
    ThreadClosure1 *Self = static_cast<ThreadClosure1 *>(C);
    Self->Function(Self->Arg1);
    delete Self;
    return 0;
  }

  ThreadClosure1(const F &f, const X &x) : Function(f), Arg1(x) {}
};
} // namespace __internal
class Thread {
public:
  // static functions
  static void SleepForMilliSeconds(uint32 millisecond);
  // static void Yield();
  static uint32 GetId();

public:
  typedef void *Handle;

  Thread();

  template <class F>
  explicit Thread(F f, std::string const &Name,
                  ThreadPriority Priority = ThreadPriority::Normal,
                  int32 CpuId = -1)
      : m_ThreadName(Name), m_ThreadPriority(Priority), m_CoreId(CpuId),
        m_StackSize(2048), m_ThreadStatus(ThreadStatus::Ready),
        m_ThreadHandle(nullptr) {
    typedef __internal::ThreadClosure0<F> closure_t;
    InternalStart(closure_t::StartRoutine, new closure_t(f));
  }

  explicit Thread(std::string const &name,
                  ThreadPriority priority = ThreadPriority::Normal,
                  int32 CpuId = -1);

  virtual ~Thread();

  void SetPriority(ThreadPriority prio);

  void Start();

  void Join();
  void Terminate();

  ThreadStatus GetThreadStatus();
  std::string GetName();

public:
  static std::string GetCurrentThreadName();
  static void SetCurrentThreadName(std::string const &name);

private:
  typedef __INTERNAL_THREAD_ROUTINE_RETURN (*ThrRoutine)(void *);
  void InternalStart(ThrRoutine Routine, __internal::ThreadClosure *Closure);

  static __INTERNAL_THREAD_ROUTINE_RETURN RunOnThread(void *);

  std::string m_ThreadName;
  ThreadPriority m_ThreadPriority;
  int32 m_CoreId;
  uint32 m_StackSize;
  ThreadStatus m_ThreadStatus;
  Handle m_ThreadHandle;
  ThrRoutine m_ThreadFunc;
  __internal::ThreadClosure *m_ThreadClosure;
};

} // namespace simd