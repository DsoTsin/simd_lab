#pragma once

#include "CoreMinimal.h"

#include <atomic>

namespace simd {
class ITask {
public:
  ITask() : finished_(false), event_(new Event(false)) {}
  virtual ~ITask() { delete event_; }

  virtual void run() {}

  void wait() { event_->wait(); }

  bool isComplete() const { return finished_.load(); }

private:
  void finish() {
    event_->trigger();
    finished_.store(true);
  }

  friend class TaskThread;

  std::atomic_bool finished_;
  Event *event_;
};

class TaskThread {
public:
  TaskThread();
  ~TaskThread();

  void launch();

  void enqueue(ITask *task);

private:
  Thread *thread_;
  volatile int stopped_;
  moodycamel::ConcurrentQueue<ITask *> taskQueue_;
};

} // namespace simd